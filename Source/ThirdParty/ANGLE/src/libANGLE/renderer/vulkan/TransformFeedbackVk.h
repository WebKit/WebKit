//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TransformFeedbackVk.h:
//    Defines the class interface for TransformFeedbackVk, implementing TransformFeedbackImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_TRANSFORMFEEDBACKVK_H_
#define LIBANGLE_RENDERER_VULKAN_TRANSFORMFEEDBACKVK_H_

#include "libANGLE/renderer/TransformFeedbackImpl.h"

namespace rx
{

class TransformFeedbackVk : public TransformFeedbackImpl
{
  public:
    TransformFeedbackVk(const gl::TransformFeedbackState &state);
    ~TransformFeedbackVk() override;

    void begin(GLenum primitiveMode) override;
    void end() override;
    void pause() override;
    void resume() override;

    void bindGenericBuffer(const gl::BindingPointer<gl::Buffer> &binding) override;
    void bindIndexedBuffer(size_t index,
                           const gl::OffsetBindingPointer<gl::Buffer> &binding) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_TRANSFORMFEEDBACKVK_H_
