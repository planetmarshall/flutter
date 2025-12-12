// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/gles/compute_pipeline_gles.h"
#include "impeller/renderer/backend/gles/unique_handle_gles.h"

namespace impeller {
ComputePipelineGLES::ComputePipelineGLES(
    std::shared_ptr<ReactorGLES> reactor,
    std::weak_ptr<PipelineLibrary> library,
    const ComputePipelineDescriptor& desc,
    std::shared_ptr<UniqueHandleGLES> handle)
    : Pipeline(std::move(library), desc),
      reactor_(std::move(reactor)),
      handle_(std::move(handle)),
      is_valid_(handle_->IsValid()) {
  if (!is_valid_) {
    reactor_->SetDebugLabel(handle_->Get(), GetDescriptor().GetLabel());
  }
}

ComputePipelineGLES::~ComputePipelineGLES() = default;

const HandleGLES& ComputePipelineGLES::GetProgramHandle() const {
  return handle_->Get();
}

const std::shared_ptr<UniqueHandleGLES> ComputePipelineGLES::GetSharedHandle()
    const {
  return handle_;
}

bool ComputePipelineGLES::IsValid() const {
  return true;
}

bool ComputePipelineGLES::BindProgram() const {
  if (!handle_->IsValid()) {
    return false;
  }
  auto handle = reactor_->GetGLHandle(handle_->Get());
  if (!handle.has_value()) {
    return false;
  }
  reactor_->GetProcTable().UseProgram(handle.value());
  return true;
}

bool ComputePipelineGLES::UnbindProgram() const {
  if (reactor_) {
    reactor_->GetProcTable().UseProgram(0u);
  }
  return true;
}
}  // namespace impeller
