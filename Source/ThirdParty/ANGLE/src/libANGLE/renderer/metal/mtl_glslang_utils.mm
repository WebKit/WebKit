//
// Copyright (c) 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlslangUtils: Wrapper for Khronos's glslang compiler.
//

#include "libANGLE/renderer/metal/mtl_glslang_utils.h"

#include "libANGLE/renderer/glslang_wrapper_utils.h"

namespace rx
{
namespace mtl
{
namespace
{
angle::Result HandleError(ErrorHandler *context, GlslangError)
{
    ANGLE_MTL_TRY(context, false);
    return angle::Result::Stop;
}

void ResetGlslangProgramInterfaceInfo(GlslangProgramInterfaceInfo *programInterfaceInfo)
{
    // We don't actually use descriptor set for now, the actual binding will be done inside
    // ProgramMtl using spirv-cross.
    programInterfaceInfo->uniformsAndXfbDescriptorSetIndex = kDefaultUniformsBindingIndex;
    programInterfaceInfo->currentUniformBindingIndex       = 0;
    programInterfaceInfo->textureDescriptorSetIndex        = 0;
    programInterfaceInfo->currentTextureBindingIndex       = 0;
    programInterfaceInfo->driverUniformsDescriptorSetIndex = kDriverUniformsBindingIndex;
    // NOTE(hqle): Unused for now, until we support ES 3.0
    programInterfaceInfo->shaderResourceDescriptorSetIndex  = -1;
    programInterfaceInfo->currentShaderResourceBindingIndex = 0;
    programInterfaceInfo->locationsUsedForXfbExtension      = 0;

    static_assert(kDefaultUniformsBindingIndex != 0, "kDefaultUniformsBindingIndex must not be 0");
    static_assert(kDriverUniformsBindingIndex != 0, "kDriverUniformsBindingIndex must not be 0");
}

GlslangSourceOptions CreateSourceOptions()
{
    GlslangSourceOptions options;
    return options;
}
}  // namespace

void GlslangGetShaderSource(const gl::ProgramState &programState,
                            const gl::ProgramLinkedResources &resources,
                            gl::ShaderMap<std::string> *shaderSourcesOut,
                            ShaderMapInterfaceVariableInfoMap *variableInfoMapOut)
{
    GlslangSourceOptions options = CreateSourceOptions();
    GlslangProgramInterfaceInfo programInterfaceInfo;
    ResetGlslangProgramInterfaceInfo(&programInterfaceInfo);

    rx::GlslangGetShaderSource(options, programState, resources, &programInterfaceInfo,
                               shaderSourcesOut, variableInfoMapOut);
}

angle::Result GlslangGetShaderSpirvCode(ErrorHandler *context,
                                        const gl::Caps &glCaps,
                                        const gl::ShaderMap<std::string> &shaderSources,
                                        const ShaderMapInterfaceVariableInfoMap &variableInfoMap,
                                        gl::ShaderMap<std::vector<uint32_t>> *shaderCodeOut)
{
    return rx::GlslangGetShaderSpirvCode(
        [context](GlslangError error) { return HandleError(context, error); }, glCaps,
        shaderSources, variableInfoMap, shaderCodeOut);
}
}  // namespace mtl
}  // namespace rx
