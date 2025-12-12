// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "compute_pass_gles.h"
#include "flutter/testing/testing.h"  // IWYU pragma: keep
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "impeller/renderer/backend/gles/compute_pass_gles.h"
#include "impeller/renderer/backend/gles/command_buffer_gles.h"
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
  class TestReactorGLES : public ReactorGLES {
  public:
    TestReactorGLES()
        : ReactorGLES(std::make_unique<ProcTableGLES>(kMockResolverGLES)) {}

    ~TestReactorGLES() = default;
  };
public:
  std::shared_ptr<ComputePass> CreateComputePass() const {
    const auto command_buffer =
        std::static_pointer_cast<Context>(context)->CreateCommandBuffer();
    return command_buffer->CreateComputePass();
  }

protected:
  ComputePassGLESTest() :
    gles(MockGLES::Init()),
      reactor(std::make_shared<TestReactorGLES>()) {
    auto dummy_gl_procs = std::make_unique<ProcTableGLES>(kMockResolverGLES);
    auto dummy_shader_library = std::vector<std::shared_ptr<fml::Mapping>>{};
    context = ContextGLES::Create({}, std::move(dummy_gl_procs), dummy_shader_library, false);
  }

  std::shared_ptr<MockGLES> gles;
  std::shared_ptr<ReactorGLES> reactor;
  std::shared_ptr<ContextGLES> context;
};

class MockWorker final : public ReactorGLES::Worker {
public:
  MockWorker() = default;

  // |ReactorGLES::Worker|
  bool CanReactorReactOnCurrentThreadNow(
      const ReactorGLES& reactor) const override {
    return true;
  }
};

TEST_F(ComputePassGLESTest, PassIsValidWithValidReactor) {
  auto compute_pass = CreateComputePass();
  ASSERT_TRUE(compute_pass->IsValid());
}
} // testing

} // impeller
