// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_GLES_COMPUTE_PIPELINE_GLES_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_GLES_COMPUTE_PIPELINE_GLES_H_

#include <memory>

#include "impeller/base/backend_cast.h"
#include "impeller/renderer/pipeline.h"

namespace impeller {

class PipelineLibraryGLES;
class ReactorGLES;
class UniqueHandleGLES;
class HandleGLES;

class ComputePipelineGLES final
    : public Pipeline<ComputePipelineDescriptor>,
      public BackendCast<ComputePipelineGLES,
                         Pipeline<ComputePipelineDescriptor>> {
 public:
  ~ComputePipelineGLES() override;

  [[nodiscard]]
  const HandleGLES& GetProgramHandle() const;

  [[nodiscard]]
  const std::shared_ptr<UniqueHandleGLES> GetSharedHandle() const;

  [[nodiscard]]
  bool BindProgram() const;

  [[nodiscard]]
  bool UnbindProgram() const;

 private:
  ComputePipelineGLES(std::shared_ptr<ReactorGLES> reactor,
                      std::weak_ptr<PipelineLibrary> library,
                      const ComputePipelineDescriptor& desc,
                      std::shared_ptr<UniqueHandleGLES> handle);
  std::shared_ptr<ReactorGLES> reactor_;
  std::shared_ptr<UniqueHandleGLES> handle_;
  bool is_valid_;
  friend PipelineLibraryGLES;
  bool IsValid() const override;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_GLES_COMPUTE_PIPELINE_GLES_H_
