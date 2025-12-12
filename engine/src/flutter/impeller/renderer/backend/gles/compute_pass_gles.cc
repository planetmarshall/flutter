// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/gles/compute_pass_gles.h"
#include "impeller/renderer/backend/gles/compute_pipeline_gles.h"

namespace impeller {

ComputePassGLES::ComputePassGLES(std::shared_ptr<const Context> context,
  std::shared_ptr<ReactorGLES> reactor)
    : ComputePass(std::move(context)) {
}

ComputePassGLES::~ComputePassGLES() = default;

bool ComputePassGLES::IsValid() const {
  return true;
}

void ComputePassGLES::OnSetLabel(const std::string& label) {
  if (label.empty()) {
    return;
  }
  label_ = label;
}

// |RenderPass|
void ComputePassGLES::SetCommandLabel(std::string_view label) {
#ifdef IMPELLER_DEBUG
#endif  // IMPELLER_DEBUG
}

// |ComputePass|
void ComputePassGLES::SetPipeline(
    const std::shared_ptr<Pipeline<ComputePipelineDescriptor>>& pipeline) {
}

// |ComputePass|
fml::Status ComputePassGLES::Compute(const ISize& grid_size) {

#ifdef IMPELLER_DEBUG
#endif  // IMPELLER_DEBUG

  return fml::Status();
}

// |ResourceBinder|
bool ComputePassGLES::BindResource(ShaderStage stage,
                                 DescriptorType type,
                                 const ShaderUniformSlot& slot,
                                 const ShaderMetadata* metadata,
                                 BufferView view) {
  return BindResource(slot.binding, type, view);
}

// |ResourceBinder|
bool ComputePassGLES::BindResource(ShaderStage stage,
                                 DescriptorType type,
                                 const SampledImageSlot& slot,
                                 const ShaderMetadata* metadata,
                                 std::shared_ptr<const Texture> texture,
                                 raw_ptr<const Sampler> sampler) {
  return true;
}

bool ComputePassGLES::BindResource(size_t binding,
                                 DescriptorType type,
                                 BufferView view) {
  return true;
}

// |ComputePass|
void ComputePassGLES::AddBufferMemoryBarrier() {
}

// |ComputePass|
void ComputePassGLES::AddTextureMemoryBarrier() {
}

// |ComputePass|
bool ComputePassGLES::EncodeCommands() const {

  return true;
}

}  // namespace impeller
