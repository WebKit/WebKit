//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TransformFeedback11.h: Implements the abstract rx::TransformFeedbackImpl class.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_TRANSFORMFEEDBACK11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_TRANSFORMFEEDBACK11_H_

#include "common/platform.h"

#include "libANGLE/angletypes.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/TransformFeedbackImpl.h"

namespace rx
{

class Renderer11;

class TransformFeedback11 : public TransformFeedbackImpl
{
  public:
    TransformFeedback11(const gl::TransformFeedbackState &state, Renderer11 *renderer);
    ~TransformFeedback11() override;

    void begin(GLenum primitiveMode) override;
    void end() override;
    void pause() override;
    void resume() override;

    void bindGenericBuffer(const BindingPointer<gl::Buffer> &binding) override;
    void bindIndexedBuffer(size_t index, const OffsetBindingPointer<gl::Buffer> &binding) override;

    void onApply();

    bool isDirty() const;

    UINT getNumSOBuffers() const;
    gl::ErrorOrResult<const std::vector<ID3D11Buffer *> *> getSOBuffers();
    const std::vector<UINT> &getSOBufferOffsets() const;

  private:
    Renderer11 *mRenderer;

    bool mIsDirty;
    std::vector<ID3D11Buffer *> mBuffers;
    std::vector<UINT> mBufferOffsets;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D11_TRANSFORMFEEDBACK11_H_
