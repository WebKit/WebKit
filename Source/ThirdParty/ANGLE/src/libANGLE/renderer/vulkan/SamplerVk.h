//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SamplerVk.h:
//    Defines the class interface for SamplerVk, implementing SamplerImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_SAMPLERVK_H_
#define LIBANGLE_RENDERER_VULKAN_SAMPLERVK_H_

#include "libANGLE/renderer/SamplerImpl.h"

namespace rx
{

class SamplerVk : public SamplerImpl
{
  public:
    SamplerVk();
    ~SamplerVk() override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SAMPLERVK_H_
