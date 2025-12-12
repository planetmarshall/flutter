// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/gles/compute_pass_gles.h"

#include "impeller/renderer/backend/gles/compute_pipeline_gles.h"
#include "reactor_gles.h"

namespace {

using namespace impeller;

bool EncodeCommandsInReactor(
    const ReactorGLES& reactor,
    const std::vector<ComputePassGLES::ComputeCommand>& commands) {
  const auto& gl = reactor.GetProcTable();

  // This is modeled on what RenderPassGLES does,
  // however it is expected that there will only ever be
  // one command
  for (const auto& command : commands) {
#ifdef IMPELLER_DEBUG
    fml::ScopedCleanupClosure debug_marker([&gl]() { gl.PopDebugGroup(); });

    if (!command.label.empty()) {
      gl.PushDebugGroup(command.label);
    } else {
      debug_marker.Release();
    }
#endif

    const auto& pipeline = ComputePipelineGLES::Cast(*command.pipeline);

    if (!pipeline.BindProgram()) {
      return false;
    }
  }

  return true;
}
}  // namespace

namespace impeller {

ComputePassGLES::ComputePassGLES(std::shared_ptr<const Context> context,
                                 std::shared_ptr<ReactorGLES> reactor)
    : ComputePass(std::move(context)), reactor_(reactor) {}

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
  pending_.label = label;
}

// |ComputePass|
void ComputePassGLES::SetPipeline(
    const std::shared_ptr<Pipeline<ComputePipelineDescriptor>>& pipeline) {
  pending_.pipeline = pipeline;
}

// |ComputePass|
fml::Status ComputePassGLES::Compute(const ISize& grid_size) {
  pending_.grid_size = grid_size;
  commands_.push_back(std::move(pending_));
  pending_ = ComputeCommand{};

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
void ComputePassGLES::AddBufferMemoryBarrier() {}

// |ComputePass|
void ComputePassGLES::AddTextureMemoryBarrier() {}

// |ComputePass|
bool ComputePassGLES::OnEncodeCommands(const Context& context) const {
  return reactor_->AddOperation(
      [pass = shared_from_this()](const auto& reactor) {
        auto result = EncodeCommandsInReactor(reactor, pass->commands_);
        FML_DCHECK(result)
            << "Must be able to encode GL commands without error.";
      },
      /*defer=*/true);
}

}  // namespace impeller
