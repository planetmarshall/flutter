// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/testing/testing.h"
#include "fml/status_or.h"
#include "gmock/gmock.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/contents/filters/gaussian_blur_filter_contents.h"
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
namespace testing {

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

TEST_P(RecursiveBlurFilterContentsTest, TextureContentsWithEffectTransform) {
}

TEST(RecursiveBlurFilterContentsTest, CalculateSigmaForBlurRadius) {
}

TEST(RecursiveBlurFilterContentsTest, Coefficients) {
}

}  // namespace testing
}  // namespace impeller
