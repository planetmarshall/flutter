// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_GLES_COMPUTE_PASS_GLES_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_GLES_COMPUTE_PASS_GLES_H_

#include "impeller/renderer/backend/gles/reactor_gles.h"
#include "impeller/renderer/compute_pass.h"
#include "impeller/renderer/compute_pipeline_descriptor.h"
#include "impeller/renderer/pipeline.h"

namespace impeller {

class ReactorGLES;

class ComputePassGLES final
    : public ComputePass,
      public std::enable_shared_from_this<ComputePassGLES> {
 public:
  struct ComputeCommand {
    std::shared_ptr<Pipeline<ComputePipelineDescriptor>> pipeline;
    std::string label;
    ISize grid_size;
  };

  // |ComputePass|
  ~ComputePassGLES() override;

 private:
  friend class CommandBufferGLES;

  std::shared_ptr<ReactorGLES> reactor_;
  std::string label_;
  std::vector<ComputeCommand> commands_;
  ComputeCommand pending_;

  ComputePassGLES(std::shared_ptr<const Context> context,
                  std::shared_ptr<ReactorGLES> reactor);

  // |ComputePass|
  bool IsValid() const override;

  // |ComputePass|
  void OnSetLabel(const std::string& label) override;

  // |ComputePass|
  bool OnEncodeCommands(const Context& context) const override;

  // |ComputePass|
  void SetCommandLabel(std::string_view label) override;

  // |ComputePass|
  void SetPipeline(const std::shared_ptr<Pipeline<ComputePipelineDescriptor>>&
                       pipeline) override;

  // |ComputePass|
  void AddBufferMemoryBarrier() override;

  // |ComputePass|
  void AddTextureMemoryBarrier() override;

  // |ComputePass|
  fml::Status Compute(const ISize& grid_size) override;

  // |ResourceBinder|
  bool BindResource(ShaderStage stage,
                    DescriptorType type,
                    const ShaderUniformSlot& slot,
                    const ShaderMetadata* metadata,
                    BufferView view) override;

  // |ResourceBinder|
  bool BindResource(ShaderStage stage,
                    DescriptorType type,
                    const SampledImageSlot& slot,
                    const ShaderMetadata* metadata,
                    std::shared_ptr<const Texture> texture,
                    raw_ptr<const Sampler> sampler) override;

  bool BindResource(size_t binding, DescriptorType type, BufferView view);
};

}  // namespace impeller
#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_GLES_COMPUTE_PASS_GLES_H_
