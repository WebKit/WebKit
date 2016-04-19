//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Stream11.h: Defines the rx::Stream11 class which implements rx::StreamImpl.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_STREAM11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_STREAM11_H_

#include "libANGLE/renderer/StreamImpl.h"

namespace rx
{
class Renderer11;

class Stream11 : public StreamImpl
{
  public:
    Stream11(Renderer11 *renderer);
    ~Stream11() override;

  private:
    Renderer11 *mRenderer;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D11_STREAM11_H_
