// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

#include "impeller/entity/contents/filters/recursive_blur_filter_contents.h"
#include "impeller/entity/contents/filters/gaussian_blur_filter_contents.h"

#include <cmath>

#include "flutter/fml/make_copyable.h"
#include "impeller/entity/contents/clip_contents.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/entity.h"
#include "impeller/entity/texture_fill.frag.h"
#include "impeller/entity/texture_fill.vert.h"
#include "impeller/geometry/color.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/vertex_buffer_builder.h"

namespace impeller {

using RecursiveBlurVertexShader = RecursiveBlurPipeline::VertexShader;
using RecursiveBlurFragmentShader = RecursiveBlurPipeline::FragmentShader;

namespace {
constexpr Scalar kMaxSigma = 500.0f;

SamplerDescriptor MakeSamplerDescriptor(MinMagFilter filter,
                                        SamplerAddressMode address_mode) {
  SamplerDescriptor sampler_desc;
  sampler_desc.min_filter = filter;
  sampler_desc.mag_filter = filter;
  sampler_desc.width_address_mode = address_mode;
  sampler_desc.height_address_mode = address_mode;
  return sampler_desc;
}

void SetTileMode(SamplerDescriptor* descriptor,
                 const ContentContext& renderer,
                 Entity::TileMode tile_mode) {
  switch (tile_mode) {
    case Entity::TileMode::kDecal:
      if (renderer.GetDeviceCapabilities().SupportsDecalSamplerAddressMode()) {
        descriptor->width_address_mode = SamplerAddressMode::kDecal;
        descriptor->height_address_mode = SamplerAddressMode::kDecal;
      }
      break;
    case Entity::TileMode::kClamp:
      descriptor->width_address_mode = SamplerAddressMode::kClampToEdge;
      descriptor->height_address_mode = SamplerAddressMode::kClampToEdge;
      break;
    case Entity::TileMode::kMirror:
      descriptor->width_address_mode = SamplerAddressMode::kMirror;
      descriptor->height_address_mode = SamplerAddressMode::kMirror;
      break;
    case Entity::TileMode::kRepeat:
      descriptor->width_address_mode = SamplerAddressMode::kRepeat;
      descriptor->height_address_mode = SamplerAddressMode::kRepeat;
      break;
  }
}

Vector2 Clamp(Vector2 vec2, Scalar min, Scalar max) {
  return Vector2(std::clamp(vec2.x, /*lo=*/min, /*hi=*/max),
                 std::clamp(vec2.y, /*lo=*/min, /*hi=*/max));
}

Vector2 ExtractScale(const Matrix& matrix) {
  Vector2 entity_scale_x = matrix * Vector2(1.0, 0.0);
  Vector2 entity_scale_y = matrix * Vector2(0.0, 1.0);
  return Vector2(entity_scale_x.GetLength(), entity_scale_y.GetLength());
}

struct BlurInfo {
  /// The scalar that is used to get from source space to unrotated local space.
  Vector2 source_space_scalar;
  /// The translation that is used to get from  source space to unrotated local
  /// space.
  Vector2 source_space_offset;
  /// Sigma when considering an entity's scale and the effect transform.
  Vector2 scaled_sigma;
  /// Blur radius in source pixels based on scaled_sigma.
  Vector2 blur_radius;
  /// The halo padding in source space.
  Vector2 padding;
  /// Padding in unrotated local space.
  Vector2 local_padding;
};

/// Calculates sigma derivatives necessary for rendering or calculating
/// coverage.
BlurInfo CalculateBlurInfo(const Entity& entity,
                           const Matrix& effect_transform,
                           Vector2 sigma) {
  // Source space here is scaled by the entity's transform. This is a
  // requirement for text to be rendered correctly. You can think of this as
  // "scaled source space" or "un-rotated local space". The entity's rotation is
  // applied to the result of the blur as part of the result's transform.
  const Vector2 source_space_scalar =
      ExtractScale(entity.GetTransform().Basis());
  const Vector2 source_space_offset =
      Vector2(entity.GetTransform().m[12], entity.GetTransform().m[13]);

  Vector2 scaled_sigma =
      (effect_transform.Basis() * Matrix::MakeScale(source_space_scalar) *  //
       Vector2(GaussianBlurFilterContents::ScaleSigma(sigma.x),
               GaussianBlurFilterContents::ScaleSigma(sigma.y)))
          .Abs();
  scaled_sigma = Clamp(scaled_sigma, 0, kMaxSigma);
  Vector2 blur_radius =
      Vector2(GaussianBlurFilterContents::CalculateBlurRadius(scaled_sigma.x),
              GaussianBlurFilterContents::CalculateBlurRadius(scaled_sigma.y));
  Vector2 padding(ceil(blur_radius.x), ceil(blur_radius.y));
  Vector2 local_padding =
      (Matrix::MakeScale(source_space_scalar) * padding).Abs();
  return {
      .source_space_scalar = source_space_scalar,
      .source_space_offset = source_space_offset,
      .scaled_sigma = scaled_sigma,
      .blur_radius = blur_radius,
      .padding = padding,
      .local_padding = local_padding,
  };
}

/// Perform FilterInput::GetSnapshot with safety checks.
std::optional<Snapshot> GetSnapshot(const std::shared_ptr<FilterInput>& input,
                                    const ContentContext& renderer,
                                    const Entity& entity,
                                    const std::optional<Rect>& coverage_hint) {
  std::optional<Snapshot> input_snapshot =
      input->GetSnapshot("RecursiveBlur", renderer, entity,
                         /*coverage_limit=*/coverage_hint);
  if (!input_snapshot.has_value()) {
    return std::nullopt;
  }

  return input_snapshot;
}

/// Returns `rect` relative to `reference`, where Rect::MakeXYWH(0,0,1,1) will
/// be returned when `rect` == `reference`.
Rect MakeReferenceUVs(const Rect& reference, const Rect& rect) {
  Rect result = Rect::MakeOriginSize(rect.GetOrigin() - reference.GetOrigin(),
                                     rect.GetSize());
  return result.Scale(1.0f / Vector2(reference.GetSize()));
}

Quad CalculateSnapshotUVs(
    const Snapshot& input_snapshot,
    const std::optional<Rect>& source_expanded_coverage_hint) {
  std::optional<Rect> input_snapshot_coverage = input_snapshot.GetCoverage();
  Quad blur_uvs = {Point(0, 0), Point(1, 0), Point(0, 1), Point(1, 1)};
  FML_DCHECK(input_snapshot.transform.IsTranslationScaleOnly());
  if (source_expanded_coverage_hint.has_value() &&
      input_snapshot_coverage.has_value()) {
    // Only process the uvs where the blur is happening, not the whole texture.
    std::optional<Rect> uvs =
        MakeReferenceUVs(input_snapshot_coverage.value(),
                         source_expanded_coverage_hint.value())
            .Intersection(Rect::MakeSize(Size(1, 1)));
    FML_DCHECK(uvs.has_value());
    if (uvs.has_value()) {
      blur_uvs[0] = uvs->GetLeftTop();
      blur_uvs[1] = uvs->GetRightTop();
      blur_uvs[2] = uvs->GetLeftBottom();
      blur_uvs[3] = uvs->GetRightBottom();
    }
  }
  return blur_uvs;
}

struct HaloPassArgs {
  /// The output size of the down-sampling pass.
  ISize subpass_size;
  /// The UVs that will be used for drawing to the down-sampling pass.
  /// This effectively is chopping out a region of the input.
  Quad uvs;
  /// Transforms from unrotated local space to position the output from the
  /// down-sample pass.
  /// This can differ if we request a coverage hint but it is rejected, as is
  /// the case with backdrop filters.
  Matrix transform;
};

/// Calculates info required for the down-sampling pass.
HaloPassArgs CalculateHaloPassArgs(
    Vector2 padding,
    const Snapshot& input_snapshot,
    const std::optional<Rect>& source_expanded_coverage_hint,
    const std::shared_ptr<FilterInput>& input,
    const Entity& snapshot_entity) {
  // TODO(jonahwilliams): If desired_scalar is 1.0 and we fully acquired the
  // gutter from the expanded_coverage_hint, we can skip the downsample pass.
  // pass.
  // TODO(gaaclarke): The padding could be removed if we know it's not needed or
  //   resized to account for the expanded_clip_coverage. There doesn't appear
  //   to be the math to make those calculations though. The following
  //   optimization works, but causes a shimmer as a result of
  //   https://github.com/flutter/flutter/issues/140193 so it isn't applied.
  //
  //   !input_snapshot->GetCoverage()->Expand(-local_padding)
  //     .Contains(coverage_hint.value()))

  std::optional<Rect> snapshot_coverage = input_snapshot.GetCoverage();
  if (input_snapshot.transform.Equals(snapshot_entity.GetTransform()) &&
      source_expanded_coverage_hint.has_value() &&
      snapshot_coverage.has_value() &&
      snapshot_coverage->Contains(source_expanded_coverage_hint.value())) {
    // If the snapshot's transform is the identity transform and we have
    // coverage hint that fits inside of the snapshots coverage that means the
    // coverage hint was ignored so we will trim out the area we are interested
    // in the down-sample pass. This usually means we have a backdrop image
    // filter.
    //
    // The region we cut out will be aligned with the down-sample divisor to
    // avoid pixel alignment problems that create shimmering.
    ISize source_size = ISize(source_expanded_coverage_hint->GetSize());

    Quad uvs =
        CalculateSnapshotUVs(input_snapshot, source_expanded_coverage_hint);
    return {.subpass_size = source_size,
            .uvs = uvs,
            .transform = Matrix::MakeTranslation(
                {source_expanded_coverage_hint->GetX(),
                 source_expanded_coverage_hint->GetY(), 0})};
  } else {
    //////////////////////////////////////////////////////////////////////////////
    auto input_snapshot_size = input_snapshot.texture->GetSize();
    Rect source_rect = Rect::MakeSize(input_snapshot_size);
    Rect source_rect_padded = source_rect.Expand(padding);

    Quad uvs = RecursiveBlurFilterContents::CalculateUVs(
        input, snapshot_entity, source_rect_padded, input_snapshot_size);
    return {
        .subpass_size = ISize(source_rect_padded.GetSize()),
        .uvs = uvs,
        .transform =
            input_snapshot.transform * Matrix::MakeTranslation(-padding),
    };
  }
}

/// Makes a subpass that will render the scaled down input and add the
/// transparent gutter required for the blur halo.
fml::StatusOr<RenderTarget> MakeHaloSubpass(
    const ContentContext& renderer,
    const std::shared_ptr<CommandBuffer>& command_buffer,
    const std::shared_ptr<Texture>& input_texture,
    const SamplerDescriptor& sampler_descriptor,
    const HaloPassArgs& pass_args,
    Entity::TileMode tile_mode) {
  using VS = TextureFillVertexShader;

  ContentContext::SubpassCallback subpass_callback =
      [&](const ContentContext& renderer, RenderPass& pass) {
        HostBuffer& data_host_buffer = renderer.GetTransientsDataBuffer();

        pass.SetCommandLabel("Recursive blur halo");
        auto pipeline_options = OptionsFromPass(pass);
        pipeline_options.primitive_type = PrimitiveType::kTriangleStrip;
        pass.SetPipeline(renderer.GetTexturePipeline(pipeline_options));

        TextureFillVertexShader::FrameInfo frame_info;
        frame_info.mvp = Matrix::MakeOrthographic(ISize(1, 1));
        frame_info.texture_sampler_y_coord_scale =
            input_texture->GetYCoordScale();

        TextureFillFragmentShader::FragInfo frag_info;
        frag_info.alpha = 1.0;

        const Quad& uvs = pass_args.uvs;
        std::array<VS::PerVertexData, 4> vertices = {
            VS::PerVertexData{Point(0, 0), uvs[0]},
            VS::PerVertexData{Point(1, 0), uvs[1]},
            VS::PerVertexData{Point(0, 1), uvs[2]},
            VS::PerVertexData{Point(1, 1), uvs[3]},
        };
        pass.SetVertexBuffer(CreateVertexBuffer(vertices, data_host_buffer));

        SamplerDescriptor linear_sampler_descriptor = sampler_descriptor;
        SetTileMode(&linear_sampler_descriptor, renderer, tile_mode);
        linear_sampler_descriptor.mag_filter = MinMagFilter::kLinear;
        linear_sampler_descriptor.min_filter = MinMagFilter::kLinear;
        TextureFillVertexShader::BindFrameInfo(
            pass, data_host_buffer.EmplaceUniform(frame_info));
        TextureFillFragmentShader::BindFragInfo(
            pass, data_host_buffer.EmplaceUniform(frag_info));
        TextureFillFragmentShader::BindTextureSampler(
            pass, input_texture,
            renderer.GetContext()->GetSamplerLibrary()->GetSampler(
                linear_sampler_descriptor));

        return pass.Draw().ok();
      };
  return renderer.MakeSubpass("Gaussian Blur Filter", pass_args.subpass_size,
                              command_buffer, subpass_callback,
                              /*msaa_enabled=*/false,
                              /*depth_stencil_enabled=*/false);
}

fml::StatusOr<RenderTarget> MakeBlurSubpass(
    const ContentContext& renderer,
    const std::shared_ptr<CommandBuffer>& command_buffer,
    const std::shared_ptr<Texture>& src_texture,
    const SamplerDescriptor& sampler_descriptor,
    Scalar pixel_size,
    Scalar sigma,
    int step,
    RecursiveBlurFilterContents::Direction direction,
    RecursiveBlurFilterContents::Orientation orientation,
    std::optional<RenderTarget> destination_target) {
  using VS = RecursiveBlurVertexShader;

  // TODO(gaaclarke): This blurs the whole image, but because we know the clip
  //                  region we could focus on just blurring that.
  ISize subpass_size = src_texture->GetSize();
  ContentContext::SubpassCallback subpass_callback =
      [&](const ContentContext& renderer, RenderPass& pass) {
        RecursiveBlurVertexShader::FrameInfo frame_info;
        frame_info.mvp = Matrix::MakeOrthographic(ISize(1, 1)),
        frame_info.texture_sampler_y_coord_scale =
            src_texture->GetYCoordScale();

        HostBuffer& data_host_buffer = renderer.GetTransientsDataBuffer();

        ContentContextOptions options = OptionsFromPass(pass);
        options.primitive_type = PrimitiveType::kTriangleStrip;
        pass.SetPipeline(renderer.GetRecursiveBlurPipeline(options));

        const auto [region, offset] =
            RecursiveBlurFilterContents::CalculateUpdateRegion(
                step, pixel_size, orientation, direction);
        std::array vertices = {
            VS::PerVertexData{region[0], region[0]},
            VS::PerVertexData{region[1], region[1]},
            VS::PerVertexData{region[2], region[2]},
            VS::PerVertexData{region[3], region[3]},
        };
        pass.SetVertexBuffer(CreateVertexBuffer(vertices, data_host_buffer));

        SamplerDescriptor linear_sampler_descriptor = sampler_descriptor;
        linear_sampler_descriptor.mag_filter = MinMagFilter::kLinear;
        linear_sampler_descriptor.min_filter = MinMagFilter::kLinear;
        RecursiveBlurFragmentShader::BindTextureSampler(
            pass, src_texture,
            renderer.GetContext()->GetSamplerLibrary()->GetSampler(
                linear_sampler_descriptor));
        RecursiveBlurVertexShader::BindFrameInfo(
            pass, data_host_buffer.EmplaceUniform(frame_info));
        RecursiveBlurFragmentShader::BindParameters(
            pass, data_host_buffer.EmplaceUniform(
                      RecursiveBlurFilterContents::CalculateParameters(
                          sigma, pixel_size)));
        RecursiveBlurFragmentShader::BindOffset(
            pass, data_host_buffer.EmplaceUniform(
                      RecursiveBlurFragmentShader::Offset{.x = offset}));
        return pass.Draw().ok();
      };
  if (destination_target.has_value()) {
    return renderer.MakeSubpass("Recursive Blur Filter",
                                destination_target.value(), command_buffer,
                                subpass_callback);
  } else {
    return renderer.MakeSubpass(
        "Recursive Blur Filter", subpass_size, command_buffer, subpass_callback,
        /*msaa_enabled=*/false, /*depth_stencil_enabled=*/false);
  }
}

Entity ApplyClippedBlurStyle(Entity::ClipOperation clip_operation,
                             const Entity& entity,
                             const std::shared_ptr<FilterInput>& input,
                             const Snapshot& input_snapshot,
                             Entity blur_entity,
                             const Geometry* geometry) {
  Matrix entity_transform = entity.GetTransform();
  Matrix blur_transform = blur_entity.GetTransform();

  auto renderer =
      fml::MakeCopyable([blur_entity = blur_entity.Clone(), clip_operation,
                         entity_transform, blur_transform, geometry](
                            const ContentContext& renderer,
                            const Entity& entity, RenderPass& pass) mutable {
        Entity clipper;
        clipper.SetClipDepth(entity.GetClipDepth());
        clipper.SetTransform(entity.GetTransform() * entity_transform);

        auto geom_result = geometry->GetPositionBuffer(renderer, clipper, pass);

        ClipContents clip_contents(geometry->GetCoverage(clipper.GetTransform())
                                       .value_or(Rect::MakeLTRB(0, 0, 0, 0)),
                                   /*is_axis_aligned_rect=*/false);
        clip_contents.SetClipOperation(clip_operation);
        clip_contents.SetGeometry(std::move(geom_result));

        if (!clip_contents.Render(renderer, pass, entity.GetClipDepth())) {
          return false;
        }
        blur_entity.SetClipDepth(entity.GetClipDepth());
        blur_entity.SetTransform(entity.GetTransform() * blur_transform);

        return blur_entity.Render(renderer, pass);
      });
  auto coverage =
      fml::MakeCopyable([blur_entity = std::move(blur_entity),
                         blur_transform](const Entity& entity) mutable {
        blur_entity.SetTransform(entity.GetTransform() * blur_transform);
        return blur_entity.GetCoverage();
      });
  Entity result;
  result.SetContents(Contents::MakeAnonymous(renderer, coverage));
  return result;
}

Entity ApplyBlurStyle(FilterContents::BlurStyle blur_style,
                      const Entity& entity,
                      const std::shared_ptr<FilterInput>& input,
                      const Snapshot& input_snapshot,
                      Entity blur_entity,
                      const Geometry* geometry,
                      Vector2 source_space_scalar,
                      Vector2 source_space_offset) {
  switch (blur_style) {
    case FilterContents::BlurStyle::kNormal:
      return blur_entity;
    case FilterContents::BlurStyle::kInner:
      return ApplyClippedBlurStyle(Entity::ClipOperation::kIntersect, entity,
                                   input, input_snapshot,
                                   std::move(blur_entity), geometry);
      break;
    case FilterContents::BlurStyle::kOuter:
      return ApplyClippedBlurStyle(Entity::ClipOperation::kDifference, entity,
                                   input, input_snapshot,
                                   std::move(blur_entity), geometry);
    case FilterContents::BlurStyle::kSolid: {
      Entity snapshot_entity =
          Entity::FromSnapshot(input_snapshot, entity.GetBlendMode());
      Entity result;
      Matrix blurred_transform = blur_entity.GetTransform();
      Matrix snapshot_transform =
          entity.GetTransform() *  //
          Matrix::MakeScale(1.f / source_space_scalar) *
          Matrix::MakeTranslation(-1 * source_space_offset) *
          input_snapshot.transform;
      result.SetContents(Contents::MakeAnonymous(
          fml::MakeCopyable([blur_entity = blur_entity.Clone(),
                             blurred_transform, snapshot_transform,
                             snapshot_entity = std::move(snapshot_entity)](
                                const ContentContext& renderer,
                                const Entity& entity,
                                RenderPass& pass) mutable {
            snapshot_entity.SetTransform(entity.GetTransform() *
                                         snapshot_transform);
            snapshot_entity.SetClipDepth(entity.GetClipDepth());
            if (!snapshot_entity.Render(renderer, pass)) {
              return false;
            }
            blur_entity.SetClipDepth(entity.GetClipDepth());
            blur_entity.SetTransform(entity.GetTransform() * blurred_transform);
            return blur_entity.Render(renderer, pass);
          }),
          fml::MakeCopyable([blur_entity = blur_entity.Clone(),
                             blurred_transform](const Entity& entity) mutable {
            blur_entity.SetTransform(entity.GetTransform() * blurred_transform);
            return blur_entity.GetCoverage();
          })));
      return result;
    }
  }
}
}  // namespace

RecursiveBlurFilterContents::RecursiveBlurFilterContents(
    Scalar sigma_x,
    Scalar sigma_y,
    Entity::TileMode tile_mode,
    BlurStyle mask_blur_style,
    const Geometry* mask_geometry)
    : sigma_(sigma_x, sigma_y),
      tile_mode_(tile_mode),
      mask_blur_style_(mask_blur_style),
      mask_geometry_(mask_geometry) {
  // This is supposed to be enforced at a higher level.
  FML_DCHECK(mask_blur_style == BlurStyle::kNormal || mask_geometry);
}

std::optional<Rect> RecursiveBlurFilterContents::GetFilterSourceCoverage(
    const Matrix& effect_transform,
    const Rect& output_limit) const {
  Vector2 scaled_sigma = {ScaleSigma(sigma_.x), ScaleSigma(sigma_.y)};
  Vector2 blur_radius = {CalculateBlurRadius(scaled_sigma.x),
                         CalculateBlurRadius(scaled_sigma.y)};
  Vector3 blur_radii =
      effect_transform.Basis() * Vector3{blur_radius.x, blur_radius.y, 0.0};
  return output_limit.Expand(Point(blur_radii.x, blur_radii.y));
}

std::optional<Rect> RecursiveBlurFilterContents::GetFilterCoverage(
    const FilterInput::Vector& inputs,
    const Entity& entity,
    const Matrix& effect_transform) const {
  if (inputs.empty()) {
    return {};
  }
  std::optional<Rect> input_coverage = inputs[0]->GetCoverage(entity);
  if (!input_coverage.has_value()) {
    return {};
  }

  BlurInfo blur_info = CalculateBlurInfo(entity, effect_transform, sigma_);
  return input_coverage.value().Expand(
      Point(blur_info.local_padding.x, blur_info.local_padding.y));
}

// A brief overview how this works:
// 1) Snapshot the filter input.
// 2) Perform downsample pass. This also inserts the gutter around the input
//    snapshot since the blur can render outside the bounds of the snapshot.
// 3) Perform 1D horizontal blur pass.
// 4) Perform 1D vertical blur pass.
// 5) Apply the blur style to the blur result. This may just mask the output or
//    draw the original snapshot over the result.
std::optional<Entity> RecursiveBlurFilterContents::RenderFilter(
    const FilterInput::Vector& inputs,
    const ContentContext& renderer,
    const Entity& entity,
    const Matrix& effect_transform,
    const Rect& coverage,
    const std::optional<Rect>& coverage_hint) const {
  if (inputs.empty()) {
    return std::nullopt;
  }

  BlurInfo blur_info = CalculateBlurInfo(entity, effect_transform, sigma_);

  // Apply as much of the desired padding as possible from the source. This may
  // be ignored so must be accounted for in the downsample pass by adding a
  // transparent gutter.
  std::optional<Rect> expanded_coverage_hint;
  if (coverage_hint.has_value()) {
    expanded_coverage_hint = coverage_hint->Expand(blur_info.local_padding);
  }

  Entity snapshot_entity = entity.Clone();
  snapshot_entity.SetTransform(
      Matrix::MakeTranslation(blur_info.source_space_offset) *
      Matrix::MakeScale(blur_info.source_space_scalar));

  std::optional<Rect> source_expanded_coverage_hint;
  if (expanded_coverage_hint.has_value()) {
    source_expanded_coverage_hint = expanded_coverage_hint->TransformBounds(
        Matrix::MakeTranslation(blur_info.source_space_offset) *
        Matrix::MakeScale(blur_info.source_space_scalar) *
        entity.GetTransform().Invert());
  }

  std::optional<Snapshot> input_snapshot = GetSnapshot(
      inputs[0], renderer, snapshot_entity, source_expanded_coverage_hint);
  if (!input_snapshot.has_value()) {
    return std::nullopt;
  }

  if (blur_info.scaled_sigma.x < kEhCloseEnough &&
      blur_info.scaled_sigma.y < kEhCloseEnough) {
    Entity result =
        Entity::FromSnapshot(input_snapshot.value(),
                             entity.GetBlendMode());  // No blur to render.
    result.SetTransform(
        entity.GetTransform() *
        Matrix::MakeScale(1.f / blur_info.source_space_scalar) *
        Matrix::MakeTranslation(-1 * blur_info.source_space_offset) *
        input_snapshot->transform);
    return result;
  }

  std::shared_ptr<CommandBuffer> command_buffer =
      renderer.GetContext()->CreateCommandBuffer();
  if (!command_buffer) {
    return std::nullopt;
  }

  auto halo_pass_args = CalculateHaloPassArgs(
      blur_info.padding, input_snapshot.value(), source_expanded_coverage_hint,
      inputs[0], snapshot_entity);

  fml::StatusOr<RenderTarget> halo_pass = MakeHaloSubpass(
      renderer, command_buffer, input_snapshot->texture,
      input_snapshot->sampler_descriptor, halo_pass_args, tile_mode_);

  if (!halo_pass.ok()) {
    return std::nullopt;
  }
  const auto size = halo_pass.value().GetRenderTargetTexture()->GetSize();
  Vector2 pixel_size = 1.0 / Vector2(size);
  auto dest_pass = std::optional<RenderTarget>(std::nullopt);
  auto src_pass = std::optional<RenderTarget>(halo_pass.value());
  for (int i = 0; i < size.width; ++i) {
    auto result = MakeBlurSubpass(
        renderer, command_buffer, src_pass->GetRenderTargetTexture(),
        input_snapshot->sampler_descriptor, pixel_size.x,
        blur_info.scaled_sigma.x, i, Direction::Causal, Orientation::Horizontal,
        dest_pass);

    if (!result.ok()) {
      return std::nullopt;
    }
    dest_pass = result.value();
    std::swap(src_pass, dest_pass);
  }
  dest_pass = src_pass.value();

  if (!renderer.GetContext()->EnqueueCommandBuffer(std::move(command_buffer))) {
    return std::nullopt;
  }

  // The ping-pong approach requires that each render pass output has the same
  // size.

  SamplerDescriptor sampler_desc = MakeSamplerDescriptor(
      MinMagFilter::kLinear, SamplerAddressMode::kClampToEdge);

  Entity blur_output_entity = Entity::FromSnapshot(
      Snapshot{.texture = dest_pass.value().GetRenderTargetTexture(),
               .transform =
                   entity.GetTransform() *                                   //
                   Matrix::MakeScale(1.f / blur_info.source_space_scalar) *  //
                   Matrix::MakeTranslation(-1 * blur_info.source_space_offset) *
                   halo_pass_args.transform,
               .sampler_descriptor = sampler_desc,
               .opacity = input_snapshot->opacity,
               .needs_rasterization_for_runtime_effects = true},
      entity.GetBlendMode());

  return ApplyBlurStyle(mask_blur_style_, entity, inputs[0],
                        input_snapshot.value(), std::move(blur_output_entity),
                        mask_geometry_, blur_info.source_space_scalar,
                        blur_info.source_space_offset);
}

Scalar RecursiveBlurFilterContents::CalculateBlurRadius(Scalar sigma) {
  return static_cast<Radius>(Sigma(sigma)).radius;
}

Quad RecursiveBlurFilterContents::CalculateUVs(
    const std::shared_ptr<FilterInput>& filter_input,
    const Entity& entity,
    const Rect& source_rect,
    const ISize& texture_size) {
  Matrix input_transform = filter_input->GetLocalTransform(entity);
  Quad coverage_quad = source_rect.GetTransformedPoints(input_transform);

  Matrix uv_transform = Matrix::MakeScale(
      {1.0f / texture_size.width, 1.0f / texture_size.height, 1.0f});
  return uv_transform.Transform(coverage_quad);
}

// This function was calculated by observing Skia's behavior. Its blur at 500
// seemed to be 0.15.  Since we clamp at 500 I solved the quadratic equation
// that puts the minima there and a f(0)=1.
Scalar RecursiveBlurFilterContents::ScaleSigma(Scalar sigma) {
  // Limit the kernel size to 1000x1000 pixels, like Skia does.
  Scalar clamped = std::min(sigma, kMaxSigma);
  constexpr Scalar a = 3.4e-06;
  constexpr Scalar b = -3.4e-3;
  constexpr Scalar c = 1.f;
  Scalar scalar = c + b * clamped + a * clamped * clamped;
  return clamped * scalar;
}

Scalar RecursiveBlurFilterContents::CalculateQFromSigma(Scalar sigma) {
  // [Young & van Vliet (1995)], eq 11.b
  if (sigma < 2.5) {
    return 3.97156 - 4.14554 * std::sqrt(1 - 0.26891 * sigma);
  }
  return 0.98711 * sigma - 0.96330;
}

RecursiveBlurFragmentShader::Parameters
RecursiveBlurFilterContents::CalculateParameters(Scalar sigma,
                                                 Scalar pixel_size) {
  const auto q = CalculateQFromSigma(sigma);

  // [Young & van Vliet (1995)], eqs 8c and 10
  const auto q2 = q * q;
  const auto q3 = q2 * q;
  const auto b0 = 1.57825 + (2.44413 * q) + (1.4281 * q2) + (0.422205 * q3);
  const auto b1 = (2.44413 * q) + (2.85619 * q2) + (1.26661 * q3);
  const auto b2 = -((1.4281 * q2) + (1.26661 * q3));
  const auto b3 = 0.422205 * q3;
  const auto B_norm = 1 - (b1 + b2 + b3) / b0;
  return {.B = static_cast<float>(B_norm),
          .data = {
              Vector4(-pixel_size, 0, static_cast<float>(b1 / b0), 0),
              Vector4(-pixel_size * 2, 0, static_cast<float>(b2 / b0), 0),
              Vector4(-pixel_size * 3, 0, static_cast<float>(b3 / b0), 0),
          }};
}

std::pair<Quad, Scalar> RecursiveBlurFilterContents::CalculateUpdateRegion(
    int index,
    Scalar pixel_size,
    Orientation orientation,
    Direction direction) {
  const auto x_lim = pixel_size * static_cast<Scalar>(index + 1);
  const auto epsilon = pixel_size * 1.0e-3F;
  return {{Point(0, 0), Point(x_lim, 0), Point(0, 1), Point(x_lim, 1)},
          static_cast<Scalar>(index) * pixel_size - epsilon};
}
}  // namespace impeller
