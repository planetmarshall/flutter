// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/testing/testing.h"
#include "fml/status_or.h"
#include "gmock/gmock.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/contents/filters/recursive_blur_filter_contents.h"
#include "impeller/entity/contents/texture_contents.h"
#include "impeller/entity/entity_playground.h"
#include "impeller/geometry/color.h"
#include "impeller/geometry/geometry_asserts.h"
#include "impeller/renderer/testing/mocks.h"

#if FML_OS_MACOSX
#define IMPELLER_RAND arc4random
#else
#define IMPELLER_RAND rand
#endif

namespace impeller {
using impeller::RecursiveBlurFilterContents;
namespace testing {
using ShaderParameters = RecursiveBlurPipeline::FragmentShader::Parameters;
class RecursiveBlurFilterContentsTest : public EntityPlayground {
 public:
  /// Create a texture that has been cleared to transparent black.
  std::shared_ptr<Texture> MakeTexture(ISize size) {
    std::shared_ptr<CommandBuffer> command_buffer =
        GetContentContext()->GetContext()->CreateCommandBuffer();
    if (!command_buffer) {
      return nullptr;
    }

    auto render_target = GetContentContext()->MakeSubpass(
        "Clear Subpass", size, command_buffer,
        [](const ContentContext&, RenderPass&) { return true; });

    if (!GetContentContext()
             ->GetContext()
             ->GetCommandQueue()
             ->Submit(/*buffers=*/{command_buffer})
             .ok()) {
      return nullptr;
    }

    if (render_target.ok()) {
      return render_target.value().GetRenderTargetTexture();
    }
    return nullptr;
  }
};

INSTANTIATE_PLAYGROUND_SUITE(RecursiveBlurFilterContentsTest);

TEST_P(RecursiveBlurFilterContentsTest, TextureContentsWithEffectTransform) {}

TEST(RecursiveBlurFilterContentsTest, CalculateQFromSigma) {
  // example given in the paper for sigma = 6.04
  ASSERT_NEAR(5.0, RecursiveBlurFilterContents::CalculateQFromSigma(6.04),
              0.05);
}

TEST(RecursiveBlurFilterContentsTest, CalculateBlurCoefficients) {
  // example given in the paper for q = 5.0
  const auto expected_params = ShaderParameters{.B = 0.01543,
                                                .data = {
                                                    Vector4(0, 0, 2.36565, 0),
                                                    Vector4(0, 0, -1.89709, 0),
                                                    Vector4(0, 0, 0.51601, 0),
                                                }};
  const auto params = RecursiveBlurFilterContents::CalculateParameters(6.04, 1);
  ASSERT_NEAR(expected_params.B, params.B, 0.001);
  constexpr auto epsilon = 5.0e-4;
  ASSERT_NEAR(expected_params.data[0].z, params.data[0].z, epsilon);
  ASSERT_NEAR(expected_params.data[1].z, params.data[1].z, epsilon);
  ASSERT_NEAR(expected_params.data[2].z, params.data[2].z, epsilon);
}

TEST(RecursiveBlurFilterContentsTest, CalculateBlurOffsets) {
  // example given in the paper for q = 5.0
  const auto expected_params = ShaderParameters{.B = 0.01543,
                                                .data = {
                                                    Vector4(-1, 0, 0, 0),
                                                    Vector4(-2, 0, 0, 0),
                                                    Vector4(-3, 0, 0, 0),
                                                }};
  const auto params = RecursiveBlurFilterContents::CalculateParameters(6.04, 1);
  constexpr auto epsilon = 5.0e-4;
  ASSERT_NEAR(expected_params.data[0].x, params.data[0].x, epsilon);
  ASSERT_NEAR(expected_params.data[0].x, params.data[0].x, epsilon);

  ASSERT_NEAR(expected_params.data[1].x, params.data[1].x, epsilon);
  ASSERT_NEAR(expected_params.data[1].x, params.data[1].x, epsilon);

  ASSERT_NEAR(expected_params.data[2].x, params.data[2].x, epsilon);
  ASSERT_NEAR(expected_params.data[2].x, params.data[2].x, epsilon);
}

TEST(RecursiveBlurFilterContentsTest, BoundsForHorizontalCausalPassAreExclusive) {
            const auto region1 = RecursiveBlurFilterContents::CalculateDestinationBounds(
              0,
              1,
              RecursiveBlurFilterContents::Orientation::Horizontal,
              RecursiveBlurFilterContents::Direction::Causal
              );

  ASSERT_LT(-4, region1.x0);
  ASSERT_GT(-3, region1.x0);
  ASSERT_LT(0, region1.x1);
  ASSERT_GT(1, region1.x1);
}
}  // namespace testing
}  // namespace impeller
