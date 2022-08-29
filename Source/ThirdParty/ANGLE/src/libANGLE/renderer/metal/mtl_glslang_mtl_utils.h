//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_glslang_mtl_utils.h: Wrapper for Khronos's glslang compiler for MSL.
//

#ifndef mtl_glslang_mtl_utils_h
#define mtl_glslang_mtl_utils_h
#include "libANGLE/Context.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/metal/mtl_common.h"

namespace rx
{
namespace mtl
{
struct SamplerBinding
{
    uint32_t textureBinding = 0;
    uint32_t samplerBinding = 0;
};

struct TranslatedShaderInfo
{
    void reset();
    // Translated Metal source code
    std::string metalShaderSource;
    // Metal library compiled from source code above. Used by ProgramMtl.
    AutoObjCPtr<id<MTLLibrary>> metalLibrary;
    std::array<SamplerBinding, kMaxGLSamplerBindings> actualSamplerBindings;
    std::array<uint32_t, kMaxGLUBOBindings> actualUBOBindings;
    std::array<uint32_t, kMaxShaderXFBs> actualXFBBindings;
    bool hasUBOArgumentBuffer;
    bool hasInvariantOrAtan;
};
void MSLGetShaderSource(const gl::Context *context,
                        const gl::ProgramState &programState,
                        const gl::ProgramLinkedResources &resources,
                        gl::ShaderMap<std::string> *shaderSourcesOut,
                        ShaderInterfaceVariableInfoMap *variableInfoMapOut);

angle::Result GlslangGetMSL(const gl::Context *glContext,
                            const gl::ProgramState &programState,
                            const gl::Caps &glCaps,
                            const gl::ShaderMap<std::string> &shaderSources,
                            const ShaderInterfaceVariableInfoMap &variableInfoMap,
                            gl::ShaderMap<TranslatedShaderInfo> *mslShaderInfoOut,
                            gl::ShaderMap<std::string> *mslCodeOut,
                            size_t xfbBufferCount);

// Get equivalent shadow compare mode that is used in translated msl shader.
uint MslGetShaderShadowCompareMode(GLenum mode, GLenum func);

}  // namespace mtl
}  // namespace rx

#endif /* mtl_glslang_mtl_utils_h */
