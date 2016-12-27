//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayVk.h:
//    Defines the class interface for VertexArrayVk, implementing VertexArrayImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_
#define LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_

#include "libANGLE/renderer/VertexArrayImpl.h"

namespace rx
{

class VertexArrayVk : public VertexArrayImpl
{
  public:
    VertexArrayVk(const gl::VertexArrayState &data);
    ~VertexArrayVk() override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_
