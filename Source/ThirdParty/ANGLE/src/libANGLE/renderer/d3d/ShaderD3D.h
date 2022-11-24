//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderD3D.h: Defines the rx::ShaderD3D class which implements rx::ShaderImpl.

#ifndef LIBANGLE_RENDERER_D3D_SHADERD3D_H_
#define LIBANGLE_RENDERER_D3D_SHADERD3D_H_

#include "libANGLE/renderer/ShaderImpl.h"

#include <map>

namespace angle
{
struct FeaturesD3D;
}  // namespace angle

namespace gl
{
struct Extensions;
}

namespace rx
{
class DynamicHLSL;
class RendererD3D;
struct D3DUniform;

// Workarounds attached to each shader. Do not need to expose information about these workarounds so
// a simple bool struct suffices.
struct CompilerWorkaroundsD3D
{
    bool skipOptimization = false;

    bool useMaxOptimization = false;

    // IEEE strictness needs to be enabled for NANs to work.
    bool enableIEEEStrictness = false;
};

class ShaderD3D : public ShaderImpl
{
  public:
    ShaderD3D(const gl::ShaderState &state, RendererD3D *renderer);
    ~ShaderD3D() override;

    std::shared_ptr<WaitableCompileEvent> compile(const gl::Context *context,
                                                  gl::ShCompilerInstance *compilerInstance,
                                                  ShCompileOptions *options) override;

    std::string getDebugInfo() const override;

    // D3D-specific methods
    void uncompile();

    bool hasUniform(const std::string &name) const;

    // Query regular uniforms with their name. Query sampler fields of structs with field selection
    // using dot (.) operator.
    unsigned int getUniformRegister(const std::string &uniformName) const;

    unsigned int getUniformBlockRegister(const std::string &blockName) const;
    bool shouldUniformBlockUseStructuredBuffer(const std::string &blockName) const;
    unsigned int getShaderStorageBlockRegister(const std::string &blockName) const;
    unsigned int getReadonlyImage2DRegisterIndex() const { return mReadonlyImage2DRegisterIndex; }
    unsigned int getImage2DRegisterIndex() const { return mImage2DRegisterIndex; }
    bool useImage2DFunction(const std::string &functionName) const;
    const std::set<std::string> &getSlowCompilingUniformBlockSet() const;
    void appendDebugInfo(const std::string &info) const { mDebugInfo += info; }

    void generateWorkarounds(CompilerWorkaroundsD3D *workarounds) const;

    bool usesClipDistance() const { return mUsesClipDistance; }
    bool usesCullDistance() const { return mUsesCullDistance; }
    bool usesMultipleRenderTargets() const { return mUsesMultipleRenderTargets; }
    bool usesFragColor() const { return mUsesFragColor; }
    bool usesFragData() const { return mUsesFragData; }
    bool usesSecondaryColor() const { return mUsesSecondaryColor; }
    bool usesFragCoord() const { return mUsesFragCoord; }
    bool usesFrontFacing() const { return mUsesFrontFacing; }
    bool usesHelperInvocation() const { return mUsesHelperInvocation; }
    bool usesPointSize() const { return mUsesPointSize; }
    bool usesPointCoord() const { return mUsesPointCoord; }
    bool usesDepthRange() const { return mUsesDepthRange; }
    bool usesFragDepth() const { return mUsesFragDepth; }
    bool usesVertexID() const { return mUsesVertexID; }
    bool usesViewID() const { return mUsesViewID; }
    bool hasANGLEMultiviewEnabled() const { return mHasANGLEMultiviewEnabled; }

    ShShaderOutput getCompilerOutputType() const;

  private:
    bool mUsesClipDistance;
    bool mUsesCullDistance;
    bool mUsesMultipleRenderTargets;
    bool mUsesFragColor;
    bool mUsesFragData;
    bool mUsesSecondaryColor;
    bool mUsesFragCoord;
    bool mUsesFrontFacing;
    bool mUsesHelperInvocation;
    bool mUsesPointSize;
    bool mUsesPointCoord;
    bool mUsesDepthRange;
    bool mUsesFragDepth;
    bool mHasANGLEMultiviewEnabled;
    bool mUsesVertexID;
    bool mUsesViewID;
    bool mUsesDiscardRewriting;
    bool mUsesNestedBreak;
    bool mRequiresIEEEStrictCompiling;

    RendererD3D *mRenderer;
    ShShaderOutput mCompilerOutputType;
    mutable std::string mDebugInfo;
    std::map<std::string, unsigned int> mUniformRegisterMap;
    std::map<std::string, unsigned int> mUniformBlockRegisterMap;
    std::map<std::string, bool> mUniformBlockUseStructuredBufferMap;
    std::set<std::string> mSlowCompilingUniformBlockSet;
    std::map<std::string, unsigned int> mShaderStorageBlockRegisterMap;
    unsigned int mReadonlyImage2DRegisterIndex;
    unsigned int mImage2DRegisterIndex;
    std::set<std::string> mUsedImage2DFunctionNames;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_SHADERD3D_H_
