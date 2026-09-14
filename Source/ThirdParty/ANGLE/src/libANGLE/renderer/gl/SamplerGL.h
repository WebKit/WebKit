//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SamplerGL.h: Defines the rx::SamplerGL class, an implementation of SamplerImpl.

#ifndef LIBANGLE_RENDERER_GL_SAMPLERGL_H_
#define LIBANGLE_RENDERER_GL_SAMPLERGL_H_

#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/SamplerImpl.h"

namespace rx
{

class SamplerGL : public SamplerImpl
{
  public:
    SamplerGL(const gl::SamplerState &state, GLuint sampler);
    ~SamplerGL() override;

    void onDestroy(const gl::Context *context) override;

    angle::Result syncState(const gl::Context *context, const bool dirty) override;

    GLuint getSamplerID() const;

  private:
    mutable gl::SamplerState mAppliedSamplerState;
    GLuint mSamplerID;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_SAMPLERGL_H_
