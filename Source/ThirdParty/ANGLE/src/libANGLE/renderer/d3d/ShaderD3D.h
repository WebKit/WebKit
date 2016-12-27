//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderD3D.h: Defines the rx::ShaderD3D class which implements rx::ShaderImpl.

#ifndef LIBANGLE_RENDERER_D3D_SHADERD3D_H_
#define LIBANGLE_RENDERER_D3D_SHADERD3D_H_

#include "libANGLE/renderer/ShaderImpl.h"

#include <map>

namespace rx
{
class DynamicHLSL;
class RendererD3D;
struct D3DCompilerWorkarounds;
struct D3DUniform;
struct WorkaroundsD3D;

class ShaderD3D : public ShaderImpl
{
  public:
    ShaderD3D(const gl::ShaderState &data, const WorkaroundsD3D &workarounds);
    virtual ~ShaderD3D();

    // ShaderImpl implementation
    ShCompileOptions prepareSourceAndReturnOptions(std::stringstream *sourceStream,
                                                   std::string *sourcePath) override;
    bool postTranslateCompile(gl::Compiler *compiler, std::string *infoLog) override;
    std::string getDebugInfo() const override;

    // D3D-specific methods
    void uncompile();

    bool hasUniform(const D3DUniform *d3dUniform) const;

    // Query regular uniforms with their name. Query sampler fields of structs with field selection
    // using dot (.) operator.
    unsigned int getUniformRegister(const std::string &uniformName) const;

    unsigned int getInterfaceBlockRegister(const std::string &blockName) const;
    void appendDebugInfo(const std::string &info) const { mDebugInfo += info; }

    void generateWorkarounds(D3DCompilerWorkarounds *workarounds) const;

    bool usesMultipleRenderTargets() const { return mUsesMultipleRenderTargets; }
    bool usesFragColor() const { return mUsesFragColor; }
    bool usesFragData() const { return mUsesFragData; }
    bool usesFragCoord() const { return mUsesFragCoord; }
    bool usesFrontFacing() const { return mUsesFrontFacing; }
    bool usesPointSize() const { return mUsesPointSize; }
    bool usesPointCoord() const { return mUsesPointCoord; }
    bool usesDepthRange() const { return mUsesDepthRange; }
    bool usesFragDepth() const { return mUsesFragDepth; }

    ShShaderOutput getCompilerOutputType() const;

  private:
    bool mUsesMultipleRenderTargets;
    bool mUsesFragColor;
    bool mUsesFragData;
    bool mUsesFragCoord;
    bool mUsesFrontFacing;
    bool mUsesPointSize;
    bool mUsesPointCoord;
    bool mUsesDepthRange;
    bool mUsesFragDepth;
    bool mUsesDiscardRewriting;
    bool mUsesNestedBreak;
    bool mRequiresIEEEStrictCompiling;

    ShShaderOutput mCompilerOutputType;
    mutable std::string mDebugInfo;
    std::map<std::string, unsigned int> mUniformRegisterMap;
    std::map<std::string, unsigned int> mInterfaceBlockRegisterMap;
    ShCompileOptions mAdditionalOptions;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_SHADERD3D_H_
