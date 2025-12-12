// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "compute_pass_gles.h"
#include "flutter/testing/testing.h"  // IWYU pragma: keep
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "impeller/renderer/backend/gles/command_buffer_gles.h"
#include "impeller/renderer/backend/gles/compute_pass_gles.h"
#include "impeller/renderer/backend/gles/context_gles.h"
#include "impeller/renderer/backend/gles/proc_table_gles.h"
#include "impeller/renderer/backend/gles/reactor_gles.h"
#include "impeller/renderer/backend/gles/test/mock_gles.h"
#include "impeller/renderer/context.h"

namespace impeller {
namespace testing {
using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::TestWithParam;

class ComputePassGLESTest : public ::testing::Test {
  class FakeWorker final : public ReactorGLES::Worker {
   public:
    FakeWorker() = default;

    // |ReactorGLES::Worker|
    bool CanReactorReactOnCurrentThreadNow(
        const ReactorGLES& reactor) const override {
      return true;
    }
  };

 public:
  std::shared_ptr<ComputePass> CreateComputePass() const {
    const auto command_buffer =
        std::static_pointer_cast<Context>(context)->CreateCommandBuffer();
    return command_buffer->CreateComputePass();
  }

 protected:
  ComputePassGLESTest()
      : gles(MockGLES::Init({}, "OpenGL ES 3.1")),
        worker_(std::make_shared<FakeWorker>()) {
    auto dummy_gl_procs = std::make_unique<ProcTableGLES>(kMockResolverGLES);
    auto dummy_shader_library = std::vector<std::shared_ptr<fml::Mapping>>{};
    context = ContextGLES::Create({}, std::move(dummy_gl_procs),
                                  dummy_shader_library, false);
    context->AddReactorWorker(worker_);
    reactor = context->GetReactor();
  }

  std::shared_ptr<MockGLES> gles;
  std::shared_ptr<ContextGLES> context;
  std::shared_ptr<ReactorGLES> reactor;

 private:
  std::shared_ptr<FakeWorker> worker_;
};

TEST_F(ComputePassGLESTest, PassIsValidWithValidReactor) {
  auto compute_pass = CreateComputePass();
  ASSERT_TRUE(compute_pass->IsValid());
}

TEST_F(ComputePassGLESTest, CanEncodeCommands) {
  auto compute_pass = CreateComputePass();
  // setup expectations here...
  ASSERT_TRUE(compute_pass->EncodeCommands());
  ASSERT_TRUE(reactor->React());
}

}  // namespace testing

}  // namespace impeller
