//
// Copyright (c) 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlslangUtils: Wrapper for Khronos's glslang compiler.
//

#include "libANGLE/renderer/metal/mtl_glslang_utils.h"

#include <regex>

#include <spirv_msl.hpp>

#include "common/apple_platform_utils.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"

namespace rx
{
namespace mtl
{
namespace
{

constexpr uint32_t kGlslangTextureDescSet        = 0;
constexpr uint32_t kGlslangDefaultUniformDescSet = 1;
constexpr uint32_t kGlslangDriverUniformsDescSet = 2;
constexpr uint32_t kGlslangShaderResourceDescSet = 3;

// Original mapping of front end from sampler name to multiple sampler slots (in form of
// slot:count pair)
using OriginalSamplerBindingMap =
    std::unordered_map<std::string, std::vector<std::pair<uint32_t, uint32_t>>>;

angle::Result HandleError(ErrorHandler *context, GlslangError)
{
    ANGLE_MTL_TRY(context, false);
    return angle::Result::Stop;
}

void ResetGlslangProgramInterfaceInfo(GlslangProgramInterfaceInfo *programInterfaceInfo)
{
    // These are binding options passed to glslang. The actual binding might be changed later
    // by spirv-cross.
    programInterfaceInfo->uniformsAndXfbDescriptorSetIndex  = kGlslangDefaultUniformDescSet;
    programInterfaceInfo->currentUniformBindingIndex        = 0;
    programInterfaceInfo->textureDescriptorSetIndex         = kGlslangTextureDescSet;
    programInterfaceInfo->currentTextureBindingIndex        = 0;
    programInterfaceInfo->driverUniformsDescriptorSetIndex  = kGlslangDriverUniformsDescSet;
    programInterfaceInfo->shaderResourceDescriptorSetIndex  = kGlslangShaderResourceDescSet;
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

spv::ExecutionModel ShaderTypeToSpvExecutionModel(gl::ShaderType shaderType)
{
    switch (shaderType)
    {
        case gl::ShaderType::Vertex:
            return spv::ExecutionModelVertex;
        case gl::ShaderType::Fragment:
            return spv::ExecutionModelFragment;
        default:
            UNREACHABLE();
            return spv::ExecutionModelMax;
    }
}

void BindBuffers(spirv_cross::CompilerMSL *compiler,
                 const spirv_cross::SmallVector<spirv_cross::Resource> &resources,
                 gl::ShaderType shaderType)
{
    auto &compilerMsl = *compiler;

    for (const spirv_cross::Resource &resource : resources)
    {
        spirv_cross::MSLResourceBinding resBinding;
        resBinding.stage = ShaderTypeToSpvExecutionModel(shaderType);

        if (compilerMsl.has_decoration(resource.id, spv::DecorationDescriptorSet))
        {
            resBinding.desc_set =
                compilerMsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
        }

        if (!compilerMsl.has_decoration(resource.id, spv::DecorationBinding))
        {
            continue;
        }

        resBinding.binding = compilerMsl.get_decoration(resource.id, spv::DecorationBinding);

        uint32_t bindingPoint = 0;
        // NOTE(hqle): We use separate discrete binding point for now, in future, we should use
        // one argument buffer for each descriptor set.
        switch (resBinding.desc_set)
        {
            case kGlslangTextureDescSet:
                // Texture binding point is ignored. We let spirv-cross automatically assign it and
                // retrieve it later
                continue;
            case kGlslangDriverUniformsDescSet:
                bindingPoint = mtl::kDriverUniformsBindingIndex;
                break;
            case kGlslangDefaultUniformDescSet:
                // NOTE(hqle): Properly handle transform feedbacks binding.
                if (shaderType != gl::ShaderType::Vertex || resBinding.binding == 0)
                {
                    bindingPoint = mtl::kDefaultUniformsBindingIndex;
                }
                else
                {
                    continue;
                }
                break;
            default:
                // We don't support this descriptor set.
                continue;
        }

        resBinding.msl_buffer = bindingPoint;

        compilerMsl.add_msl_resource_binding(resBinding);
    }  // for (resources)
}

void GetAssignedSamplerBindings(const spirv_cross::CompilerMSL &compilerMsl,
                                const OriginalSamplerBindingMap &originalBindings,
                                std::array<SamplerBinding, mtl::kMaxGLSamplerBindings> *bindings)
{
    for (const spirv_cross::Resource &resource : compilerMsl.get_shader_resources().sampled_images)
    {
        uint32_t descriptorSet = 0;
        if (compilerMsl.has_decoration(resource.id, spv::DecorationDescriptorSet))
        {
            descriptorSet = compilerMsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
        }

        // We already assigned descriptor set 0 to textures. Just to double check.
        ASSERT(descriptorSet == kGlslangTextureDescSet);
        ASSERT(compilerMsl.has_decoration(resource.id, spv::DecorationBinding));

        uint32_t actualTextureSlot = compilerMsl.get_automatic_msl_resource_binding(resource.id);
        uint32_t actualSamplerSlot =
            compilerMsl.get_automatic_msl_resource_binding_secondary(resource.id);

        // Assign sequential index for subsequent array elements
        const std::vector<std::pair<uint32_t, uint32_t>> &resOrignalBindings =
            originalBindings.at(resource.name);
        uint32_t currentTextureSlot = actualTextureSlot;
        uint32_t currentSamplerSlot = actualSamplerSlot;
        for (const std::pair<uint32_t, uint32_t> &originalBindingRange : resOrignalBindings)
        {
            SamplerBinding &actualBinding = bindings->at(originalBindingRange.first);
            actualBinding.textureBinding  = currentTextureSlot;
            actualBinding.samplerBinding  = currentSamplerSlot;

            currentTextureSlot += originalBindingRange.second;
            currentSamplerSlot += originalBindingRange.second;
        }
    }
}

std::string PostProcessTranslatedMsl(const std::string &translatedSource)
{
    // Add function_constant attribute to gl_SampleMask.
    // Even though this varying is only used when ANGLECoverageMaskEnabled is true,
    // the spirv-cross doesn't assign function_constant attribute to it. Thus it won't be dead-code
    // removed when ANGLECoverageMaskEnabled=false.
    std::string sampleMaskReplaceStr = std::string("[[sample_mask, function_constant(") +
                                       sh::mtl::kCoverageMaskEnabledConstName + ")]]";

    // This replaces "gl_SampleMask [[sample_mask]]"
    //          with "gl_SampleMask [[sample_mask, function_constant(ANGLECoverageMaskEnabled)]]"
    std::regex sampleMaskDeclareRegex(R"(\[\s*\[\s*sample_mask\s*\]\s*\])");
    return std::regex_replace(translatedSource, sampleMaskDeclareRegex, sampleMaskReplaceStr);
}

// Customized spirv-cross compiler
class SpirvToMslCompiler : public spirv_cross::CompilerMSL
{
  public:
    SpirvToMslCompiler(std::vector<uint32_t> &&spriv) : spirv_cross::CompilerMSL(spriv) {}

    std::string compileEx(gl::ShaderType shaderType,
                          const OriginalSamplerBindingMap &originalSamplerBindings,
                          TranslatedShaderInfo *mslShaderInfoOut)
    {
        spirv_cross::CompilerMSL::Options compOpt;

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
        compOpt.platform = spirv_cross::CompilerMSL::Options::macOS;
#else
        compOpt.platform = spirv_cross::CompilerMSL::Options::iOS;
#endif

        if (ANGLE_APPLE_AVAILABLE_XCI(10.14, 13.0, 12))
        {
            // Use Metal 2.1
            compOpt.set_msl_version(2, 1);
        }
        else
        {
            // Always use at least Metal 2.0.
            compOpt.set_msl_version(2);
        }

        compOpt.pad_fragment_output_components = true;

        // Tell spirv-cross to map default & driver uniform blocks as we want
        spirv_cross::ShaderResources mslRes = spirv_cross::CompilerMSL::get_shader_resources();

        BindBuffers(this, mslRes.uniform_buffers, shaderType);

        spirv_cross::CompilerMSL::set_msl_options(compOpt);

        // Actual compilation
        std::string translatedMsl = PostProcessTranslatedMsl(spirv_cross::CompilerMSL::compile());

        // Retrieve automatic texture slot assignments
        GetAssignedSamplerBindings(*this, originalSamplerBindings,
                                   &mslShaderInfoOut->actualSamplerBindings);

        return translatedMsl;
    }
};

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
                                        const gl::ShaderBitSet &linkedShaderStages,
                                        const gl::Caps &glCaps,
                                        const gl::ShaderMap<std::string> &shaderSources,
                                        const ShaderMapInterfaceVariableInfoMap &variableInfoMap,
                                        gl::ShaderMap<std::vector<uint32_t>> *shaderCodeOut)
{
    gl::ShaderMap<SpirvBlob> initialSpirvBlobs;

    ANGLE_TRY(rx::GlslangGetShaderSpirvCode(
        [context](GlslangError error) { return HandleError(context, error); }, linkedShaderStages,
        glCaps, shaderSources, variableInfoMap, &initialSpirvBlobs));

    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        // we pass in false here to skip modifications related to  early fragment tests
        // optimizations and line rasterization. These are done in the initProgram time since they
        // are related to context state. We must keep original untouched spriv blobs here because we
        // do not have ability to add back in at initProgram time.
        angle::Result status = GlslangTransformSpirvCode(
            [context](GlslangError error) { return HandleError(context, error); }, shaderType,
            false, false, variableInfoMap[shaderType], initialSpirvBlobs[shaderType],
            &(*shaderCodeOut)[shaderType]);
        if (status != angle::Result::Continue)
        {
            return status;
        }
    }

    return angle::Result::Continue;
}

angle::Result SpirvCodeToMsl(Context *context,
                             const gl::ProgramState &programState,
                             gl::ShaderMap<std::vector<uint32_t>> *sprivShaderCode,
                             gl::ShaderMap<TranslatedShaderInfo> *mslShaderInfoOut,
                             gl::ShaderMap<std::string> *mslCodeOut)
{
    // Retrieve original sampler bindings produced by front end.
    OriginalSamplerBindingMap originalSamplerBindings;
    const std::vector<gl::SamplerBinding> &samplerBindings = programState.getSamplerBindings();
    const std::vector<gl::LinkedUniform> &uniforms         = programState.getUniforms();

    for (uint32_t textureIndex = 0; textureIndex < samplerBindings.size(); ++textureIndex)
    {
        const gl::SamplerBinding &samplerBinding = samplerBindings[textureIndex];
        uint32_t uniformIndex = programState.getUniformIndexFromSamplerIndex(textureIndex);
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];
        std::string mappedSamplerName           = GlslangGetMappedSamplerName(samplerUniform.name);
        originalSamplerBindings[mappedSamplerName].push_back(
            {textureIndex, static_cast<uint32_t>(samplerBinding.boundTextureUnits.size())});
    }

    // Do the actual translation
    for (gl::ShaderType shaderType : gl::AllGLES2ShaderTypes())
    {
        std::vector<uint32_t> &sprivCode = sprivShaderCode->at(shaderType);
        SpirvToMslCompiler compilerMsl(std::move(sprivCode));

        // NOTE(hqle): spirv-cross uses exceptions to report error, what should we do here
        // in case of error?
        std::string translatedMsl = compilerMsl.compileEx(shaderType, originalSamplerBindings,
                                                          &mslShaderInfoOut->at(shaderType));
        if (translatedMsl.size() == 0)
        {
            ANGLE_MTL_CHECK(context, false, GL_INVALID_OPERATION);
        }

        mslCodeOut->at(shaderType) = std::move(translatedMsl);
    }  // for (gl::ShaderType shaderType

    return angle::Result::Continue;
}

}  // namespace mtl
}  // namespace rx
