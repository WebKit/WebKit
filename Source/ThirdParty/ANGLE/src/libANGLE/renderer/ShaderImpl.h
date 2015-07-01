//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderImpl.h: Defines the abstract rx::ShaderImpl class.

#ifndef LIBANGLE_RENDERER_SHADERIMPL_H_
#define LIBANGLE_RENDERER_SHADERIMPL_H_

#include <vector>

#include "common/angleutils.h"
#include "libANGLE/Shader.h"

namespace rx
{

class ShaderImpl : angle::NonCopyable
{
  public:
    ShaderImpl() { }
    virtual ~ShaderImpl() { }

    virtual bool compile(gl::Compiler *compiler, const std::string &source) = 0;
    virtual std::string getDebugInfo() const = 0;

    virtual const std::string &getInfoLog() const { return mInfoLog; }
    virtual const std::string &getTranslatedSource() const { return mTranslatedSource; }

    const std::vector<gl::PackedVarying> &getVaryings() const { return mVaryings; }
    const std::vector<sh::Uniform> &getUniforms() const { return mUniforms; }
    const std::vector<sh::InterfaceBlock> &getInterfaceBlocks() const  { return mInterfaceBlocks; }
    const std::vector<sh::Attribute> &getActiveAttributes() const { return mActiveAttributes; }
    const std::vector<sh::Attribute> &getActiveOutputVariables() const { return mActiveOutputVariables; }

    std::vector<gl::PackedVarying> &getVaryings() { return mVaryings; }
    std::vector<sh::Uniform> &getUniforms() { return mUniforms; }
    std::vector<sh::InterfaceBlock> &getInterfaceBlocks() { return mInterfaceBlocks; }
    std::vector<sh::Attribute> &getActiveAttributes() { return mActiveAttributes; }
    std::vector<sh::Attribute> &getActiveOutputVariables() { return mActiveOutputVariables; }

  protected:
    std::string mInfoLog;
    std::string mTranslatedSource;

    std::vector<gl::PackedVarying> mVaryings;
    std::vector<sh::Uniform> mUniforms;
    std::vector<sh::InterfaceBlock> mInterfaceBlocks;
    std::vector<sh::Attribute> mActiveAttributes;
    std::vector<sh::Attribute> mActiveOutputVariables;
};

}

#endif // LIBANGLE_RENDERER_SHADERIMPL_H_
