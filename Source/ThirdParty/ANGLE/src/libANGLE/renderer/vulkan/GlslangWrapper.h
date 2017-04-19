//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlslangWrapper: Wrapper for Vulkan's glslang compiler.
//

#ifndef LIBANGLE_RENDERER_VULKAN_GLSLANG_WRAPPER_H_
#define LIBANGLE_RENDERER_VULKAN_GLSLANG_WRAPPER_H_

#include "libANGLE/RefCountObject.h"
#include "libANGLE/renderer/ProgramImpl.h"

namespace rx
{

class GlslangWrapper : public RefCountObjectNoID
{
  public:
    // Increases the reference count.
    // TODO(jmadill): Determine how to handle this atomically.
    static GlslangWrapper *GetReference();
    static void ReleaseReference();

    LinkResult linkProgram(const std::string &vertexSource,
                           const std::string &fragmentSource,
                           std::vector<uint32_t> *vertexCodeOut,
                           std::vector<uint32_t> *fragmentCodeOut);

  private:
    GlslangWrapper();
    ~GlslangWrapper() override;

    static GlslangWrapper *mInstance;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_GLSLANG_WRAPPER_H_
