// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "flutter/testing/testing.h"  // IWYU pragma: keep
#include "gtest/gtest.h"
#include "impeller/renderer/backend/gles/context_gles.h"
#include "impeller/renderer/backend/gles/proc_table_gles.h"
#include "impeller/renderer/backend/gles/test/mock_gles.h"
#include "impeller/renderer/context.h"

namespace impeller::testing {

namespace {
std::shared_ptr<ContextGLES> CreateFakeGLESContext(std::string_view version) {
  MockGLES::Init(std::nullopt, version.data());
  auto dummy_gl_procs = std::make_unique<ProcTableGLES>(kMockResolverGLES);
  auto dummy_shader_library = std::vector<std::shared_ptr<fml::Mapping>>{};
  return ContextGLES::Create({}, std::move(dummy_gl_procs),
                             dummy_shader_library, false);
}
}  // namespace

TEST(CommandBufferGLESTest, CanCreateComputePassIfSupported) {
  const auto context = CreateFakeGLESContext("OpenGL ES 3.1");
  const auto command_buffer =
      std::static_pointer_cast<Context>(context)->CreateCommandBuffer();
  ASSERT_TRUE(command_buffer->CreateComputePass());
}

TEST(CommandBufferGLESTest, ComputePassIsNotSupportedBeforeES310) {
  const auto context = CreateFakeGLESContext("OpenGL ES 3.0");
  const auto command_buffer =
      std::static_pointer_cast<Context>(context)->CreateCommandBuffer();
  ASSERT_FALSE(command_buffer->CreateComputePass());
}
}  // namespace impeller::testing
