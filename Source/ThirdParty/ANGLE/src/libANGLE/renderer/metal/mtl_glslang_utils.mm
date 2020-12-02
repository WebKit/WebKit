//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
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

constexpr uint32_t kGlslangTextureDescSet              = 0;
constexpr uint32_t kGlslangDefaultUniformAndXfbDescSet = 1;
constexpr uint32_t kGlslangDriverUniformsDescSet       = 2;
constexpr uint32_t kGlslangShaderResourceDescSet       = 3;

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
    programInterfaceInfo->uniformsAndXfbDescriptorSetIndex  = kGlslangDefaultUniformAndXfbDescSet;
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
                 gl::ShaderType shaderType,
                 const std::unordered_map<std::string, uint32_t> &uboOriginalBindings,
                 const std::unordered_map<uint32_t, uint32_t> &xfbOriginalBindings,
                 std::array<uint32_t, kMaxGLUBOBindings> *uboBindingsRemapOut,
                 std::array<uint32_t, kMaxShaderXFBs> *xfbBindingRemapOut,
                 bool *uboArgumentBufferUsed)
{
    auto &compilerMsl = *compiler;

    uint32_t totalUniformBufferSlots = 0;
    uint32_t totalXfbSlots           = 0;
    struct UniformBufferVar
    {
        const char *name = nullptr;
        spirv_cross::MSLResourceBinding resBinding;
        uint32_t arraySize;
    };
    std::vector<UniformBufferVar> uniformBufferBindings;

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
            case kGlslangDefaultUniformAndXfbDescSet:
                if (shaderType != gl::ShaderType::Vertex ||
                    !xfbOriginalBindings.count(resBinding.binding))
                {
                    bindingPoint = mtl::kDefaultUniformsBindingIndex;
                }
                else
                {
                    // XFB buffer
                    uint32_t xfbSlot = xfbOriginalBindings.at(resBinding.binding);

                    totalXfbSlots++;
                    // XFB buffer is allocated slot starting from last discrete Metal buffer slot.
                    bindingPoint = kMaxShaderBuffers - 1 - xfbSlot;

                    xfbBindingRemapOut->at(xfbSlot) = bindingPoint;
                }
                break;
            case kGlslangShaderResourceDescSet:
            {
                UniformBufferVar uboVar;
                uboVar.name                       = resource.name.c_str();
                uboVar.resBinding                 = resBinding;
                const spirv_cross::SPIRType &type = compilerMsl.get_type_from_variable(resource.id);
                if (!type.array.empty())
                {
                    uboVar.arraySize = type.array[0];
                }
                else
                {
                    uboVar.arraySize = 1;
                }
                totalUniformBufferSlots += uboVar.arraySize;
                uniformBufferBindings.push_back(uboVar);
            }
                continue;
            default:
                // We don't support this descriptor set.
                continue;
        }

        resBinding.msl_buffer = bindingPoint;

        compilerMsl.add_msl_resource_binding(resBinding);
    }  // for (resources)

    if (totalUniformBufferSlots == 0)
    {
        return;
    }

    // Remap the uniform buffers bindings. glslang allows uniform buffers array to use exactly
    // one slot in the descriptor set. However, metal enforces that the uniform buffers array
    // use (n) slots where n=array size.
    uint32_t currentSlot = 0;
    uint32_t maxUBODiscreteSlots =
        kMaxShaderBuffers - totalXfbSlots - kUBOArgumentBufferBindingIndex;

    if (totalUniformBufferSlots > maxUBODiscreteSlots)
    {
        // If shader uses more than maxUBODiscreteSlots number of UBOs, encode them all into
        // an argument buffer. Each buffer will be assigned [[id(n)]] attribute.
        *uboArgumentBufferUsed = true;
    }
    else
    {
        // Use discrete buffer binding slot for UBOs which translates each slot to [[buffer(n)]]
        *uboArgumentBufferUsed = false;
        // Discrete buffer binding slot starts at kUBOArgumentBufferBindingIndex
        currentSlot += kUBOArgumentBufferBindingIndex;
    }

    for (UniformBufferVar &uboVar : uniformBufferBindings)
    {
        spirv_cross::MSLResourceBinding &resBinding = uboVar.resBinding;
        resBinding.msl_buffer                       = currentSlot;

        uint32_t originalBinding = uboOriginalBindings.at(uboVar.name);

        for (uint32_t i = 0; i < uboVar.arraySize; ++i, ++currentSlot)
        {
            // Use consecutive slot for member in array
            uboBindingsRemapOut->at(originalBinding + i) = currentSlot;
        }

        compilerMsl.add_msl_resource_binding(resBinding);
    }
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

    void compileEx(gl::ShaderType shaderType,
                   const std::unordered_map<std::string, uint32_t> &uboOriginalBindings,
                   const std::unordered_map<uint32_t, uint32_t> &xfbOriginalBindings,
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

        // Tell spirv-cross to map default & driver uniform & storage blocks as we want
        spirv_cross::ShaderResources mslRes = spirv_cross::CompilerMSL::get_shader_resources();

        spirv_cross::SmallVector<spirv_cross::Resource> buffers = std::move(mslRes.uniform_buffers);
        buffers.insert(buffers.end(), mslRes.storage_buffers.begin(), mslRes.storage_buffers.end());

        BindBuffers(this, buffers, shaderType, uboOriginalBindings, xfbOriginalBindings,
                    &mslShaderInfoOut->actualUBOBindings, &mslShaderInfoOut->actualXFBBindings,
                    &mslShaderInfoOut->hasUBOArgumentBuffer);

        if (mslShaderInfoOut->hasUBOArgumentBuffer)
        {
            // Enable argument buffer.
            compOpt.argument_buffers = true;

            // Force UBO argument buffer binding to start at kUBOArgumentBufferBindingIndex.
            spirv_cross::MSLResourceBinding argBufferBinding = {};
            argBufferBinding.stage    = ShaderTypeToSpvExecutionModel(shaderType);
            argBufferBinding.desc_set = kGlslangShaderResourceDescSet;
            argBufferBinding.binding =
                spirv_cross::kArgumentBufferBinding;  // spirv-cross built-in binding.
            argBufferBinding.msl_buffer = kUBOArgumentBufferBindingIndex;  // Actual binding.
            spirv_cross::CompilerMSL::add_msl_resource_binding(argBufferBinding);

            // Force discrete slot bindings for textures, default uniforms & driver uniforms
            // instead of using argument buffer.
            spirv_cross::CompilerMSL::add_discrete_descriptor_set(kGlslangTextureDescSet);
            spirv_cross::CompilerMSL::add_discrete_descriptor_set(
                kGlslangDefaultUniformAndXfbDescSet);
            spirv_cross::CompilerMSL::add_discrete_descriptor_set(kGlslangDriverUniformsDescSet);
        }
        else
        {
            // Disable argument buffer generation for uniform buffers
            compOpt.argument_buffers = false;
        }

        spirv_cross::CompilerMSL::set_msl_options(compOpt);

        // Actual compilation
        mslShaderInfoOut->metalShaderSource =
            PostProcessTranslatedMsl(spirv_cross::CompilerMSL::compile());

        // Retrieve automatic texture slot assignments
        GetAssignedSamplerBindings(*this, originalSamplerBindings,
                                   &mslShaderInfoOut->actualSamplerBindings);
    }
};

angle::Result ConvertSpirvToMsl(
    Context *context,
    gl::ShaderType shaderType,
    const std::unordered_map<std::string, uint32_t> &uboOriginalBindings,
    const std::unordered_map<uint32_t, uint32_t> &xfbOriginalBindings,
    const OriginalSamplerBindingMap &originalSamplerBindings,
    std::vector<uint32_t> *sprivCode,
    TranslatedShaderInfo *translatedShaderInfoOut)
{
    if (!sprivCode || sprivCode->empty())
    {
        return angle::Result::Continue;
    }

    SpirvToMslCompiler compilerMsl(std::move(*sprivCode));

    // NOTE(hqle): spirv-cross uses exceptions to report error, what should we do here
    // in case of error?
    compilerMsl.compileEx(shaderType, uboOriginalBindings, xfbOriginalBindings,
                          originalSamplerBindings, translatedShaderInfoOut);
    if (translatedShaderInfoOut->metalShaderSource.size() == 0)
    {
        ANGLE_MTL_CHECK(context, false, GL_INVALID_OPERATION);
    }

    return angle::Result::Continue;
}

}  // namespace

void TranslatedShaderInfo::reset()
{
    metalShaderSource.clear();
    metalLibrary         = nil;
    hasUBOArgumentBuffer = false;
    for (mtl::SamplerBinding &binding : actualSamplerBindings)
    {
        binding.textureBinding = mtl::kMaxShaderSamplers;
    }

    for (uint32_t &binding : actualUBOBindings)
    {
        binding = mtl::kMaxShaderBuffers;
    }

    for (uint32_t &binding : actualXFBBindings)
    {
        binding = mtl::kMaxShaderBuffers;
    }
}

void GlslangGetShaderSource(const gl::ProgramState &programState,
                            const gl::ProgramLinkedResources &resources,
                            gl::ShaderMap<std::string> *shaderSourcesOut,
                            std::string *xfbOnlyShaderSourceOut,
                            ShaderMapInterfaceVariableInfoMap *variableInfoMapOut,
                            ShaderInterfaceVariableInfoMap *xfbOnlyVSVariableInfoMapOut)
{
    GlslangSourceOptions options = CreateSourceOptions();
    GlslangProgramInterfaceInfo programInterfaceInfo;
    ResetGlslangProgramInterfaceInfo(&programInterfaceInfo);

    rx::GlslangGetShaderSource(options, programState, resources, &programInterfaceInfo,
                               shaderSourcesOut, variableInfoMapOut);

    // Special version for XFB only
    if (xfbOnlyShaderSourceOut && !programState.getLinkedTransformFeedbackVaryings().empty())
    {
        gl::Shader *glShader    = programState.getAttachedShader(gl::ShaderType::Vertex);
        *xfbOnlyShaderSourceOut = glShader ? glShader->getTranslatedSource() : "";

        GlslangProgramInterfaceInfo xfbOnlyInterfaceInfo;
        ResetGlslangProgramInterfaceInfo(&xfbOnlyInterfaceInfo);
        ShaderMapInterfaceVariableInfoMap xfbOnlyVariableMaps;

        options.emulateTransformFeedback = true;

        rx::GlslangGenTransformFeedbackEmulationOutputs(
            options, programState, &xfbOnlyInterfaceInfo, xfbOnlyShaderSourceOut,
            &xfbOnlyVariableMaps[gl::ShaderType::Vertex]);

        GlslangAssignLocations(options, programState.getExecutable(), gl::ShaderType::Vertex,
                               &xfbOnlyInterfaceInfo, &xfbOnlyVariableMaps);
        *xfbOnlyVSVariableInfoMapOut = std::move(xfbOnlyVariableMaps[gl::ShaderType::Vertex]);
    }
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
        glCaps, shaderSources, &initialSpirvBlobs));

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
                             const ShaderInterfaceVariableInfoMap &xfbVSVariableInfoMap,
                             gl::ShaderMap<std::vector<uint32_t>> *spirvShaderCode,
                             std::vector<uint32_t> *xfbOnlySpirvCode /** nullable */,
                             gl::ShaderMap<TranslatedShaderInfo> *mslShaderInfoOut,
                             TranslatedShaderInfo *mslXfbOnlyShaderInfoOut /** nullable */)
{
    // Retrieve original uniform buffer bindings generated by front end. We will need to do a remap.
    std::unordered_map<std::string, uint32_t> uboOriginalBindings;
    const std::vector<gl::InterfaceBlock> &blocks = programState.getUniformBlocks();
    for (uint32_t bufferIdx = 0; bufferIdx < blocks.size(); ++bufferIdx)
    {
        const gl::InterfaceBlock &block = blocks[bufferIdx];
        if (!uboOriginalBindings.count(block.mappedName))
        {
            uboOriginalBindings[block.mappedName] = bufferIdx;
        }
    }
    // Retrieve original XFB buffers bindings produced by front end.
    std::unordered_map<uint32_t, uint32_t> xfbOriginalBindings;
    for (uint32_t bufferIdx = 0; bufferIdx < kMaxShaderXFBs; ++bufferIdx)
    {
        std::string bufferName = rx::GetXfbBufferName(bufferIdx);
        if (xfbVSVariableInfoMap.count(bufferName))
        {
            xfbOriginalBindings[xfbVSVariableInfoMap.at(bufferName).binding] = bufferIdx;
        }
    }

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
        std::vector<uint32_t> &sprivCode = spirvShaderCode->at(shaderType);
        ANGLE_TRY(ConvertSpirvToMsl(context, shaderType, uboOriginalBindings, xfbOriginalBindings,
                                    originalSamplerBindings, &sprivCode,
                                    &mslShaderInfoOut->at(shaderType)));
    }  // for (gl::ShaderType shaderType

    // Special version of XFB only
    if (xfbOnlySpirvCode && !programState.getLinkedTransformFeedbackVaryings().empty())
    {
        ANGLE_TRY(ConvertSpirvToMsl(context, gl::ShaderType::Vertex, uboOriginalBindings,
                                    xfbOriginalBindings, originalSamplerBindings, xfbOnlySpirvCode,
                                    mslXfbOnlyShaderInfoOut));
    }

    return angle::Result::Continue;
}

}  // namespace mtl
}  // namespace rx
