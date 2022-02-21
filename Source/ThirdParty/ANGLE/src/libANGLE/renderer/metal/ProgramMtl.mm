//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramMtl.mm:
//    Implements the class methods for ProgramMtl.
//

#include "libANGLE/renderer/metal/ProgramMtl.h"

#include <TargetConditionals.h>

#include <sstream>

#include "common/debug.h"
#include "common/system_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/renderer/metal/BufferMtl.h"
#include "libANGLE/renderer/metal/CompilerMtl.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/TextureMtl.h"
#include "libANGLE/renderer/metal/mtl_glslang_mtl_utils.h"
#include "libANGLE/renderer/metal/mtl_utils.h"
#include "libANGLE/renderer/renderer_utils.h"

#if ANGLE_ENABLE_METAL_SPIRV
#    include "libANGLE/renderer/metal/mtl_glslang_utils.h"
#endif

namespace rx
{

namespace
{

#define SHADER_ENTRY_NAME @"main0"
#if ANGLE_ENABLE_METAL_SPIRV
constexpr char kSpirvCrossSpecConstSuffix[] = "_tmp";
#endif
template <typename T>
class ScopedAutoClearVector
{
  public:
    ScopedAutoClearVector(std::vector<T> *array) : mArray(*array) {}
    ~ScopedAutoClearVector() { mArray.clear(); }

  private:
    std::vector<T> &mArray;
};

angle::Result StreamUniformBufferData(ContextMtl *contextMtl,
                                      mtl::BufferPool *dynamicBuffer,
                                      const uint8_t *sourceData,
                                      size_t bytesToAllocate,
                                      size_t sizeToCopy,
                                      mtl::BufferRef *bufferOut,
                                      size_t *bufferOffsetOut)
{
    uint8_t *dst = nullptr;
    dynamicBuffer->releaseInFlightBuffers(contextMtl);
    ANGLE_TRY(dynamicBuffer->allocate(contextMtl, bytesToAllocate, &dst, bufferOut, bufferOffsetOut,
                                      nullptr));
    memcpy(dst, sourceData, sizeToCopy);

    ANGLE_TRY(dynamicBuffer->commit(contextMtl));
    return angle::Result::Continue;
}

void InitDefaultUniformBlock(const std::vector<sh::Uniform> &uniforms,
                             gl::Shader *shader,
                             sh::BlockLayoutMap *blockLayoutMapOut,
                             size_t *blockSizeOut)
{
    if (uniforms.empty())
    {
        *blockSizeOut = 0;
        return;
    }

    sh::Std140BlockEncoder blockEncoder;
    sh::GetActiveUniformBlockInfo(uniforms, "", &blockEncoder, blockLayoutMapOut);

    size_t blockSize = blockEncoder.getCurrentOffset();

    // TODO(jmadill): I think we still need a valid block for the pipeline even if zero sized.
    if (blockSize == 0)
    {
        *blockSizeOut = 0;
        return;
    }

    // Need to round up to multiple of vec4
    *blockSizeOut = roundUp(blockSize, static_cast<size_t>(16));
    return;
}

inline NSDictionary<NSString *, NSObject *> *getDefaultSubstitutionDictionary()
{
    return @{};
}

template <typename T>
void UpdateDefaultUniformBlock(GLsizei count,
                               uint32_t arrayIndex,
                               int componentCount,
                               const T *v,
                               const sh::BlockMemberInfo &layoutInfo,
                               angle::MemoryBuffer *uniformData)
{
    const int elementSize = sizeof(T) * componentCount;

    uint8_t *dst = uniformData->data() + layoutInfo.offset;
    if (layoutInfo.arrayStride == 0 || layoutInfo.arrayStride == elementSize)
    {
        uint32_t arrayOffset = arrayIndex * layoutInfo.arrayStride;
        uint8_t *writePtr    = dst + arrayOffset;
        ASSERT(writePtr + (elementSize * count) <= uniformData->data() + uniformData->size());
        memcpy(writePtr, v, elementSize * count);
    }
    else
    {
        // Have to respect the arrayStride between each element of the array.
        int maxIndex = arrayIndex + count;
        for (int writeIndex = arrayIndex, readIndex = 0; writeIndex < maxIndex;
             writeIndex++, readIndex++)
        {
            const int arrayOffset = writeIndex * layoutInfo.arrayStride;
            uint8_t *writePtr     = dst + arrayOffset;
            const T *readPtr      = v + (readIndex * componentCount);
            ASSERT(writePtr + elementSize <= uniformData->data() + uniformData->size());
            memcpy(writePtr, readPtr, elementSize);
        }
    }
}

template <typename T>
void ReadFromDefaultUniformBlock(int componentCount,
                                 uint32_t arrayIndex,
                                 T *dst,
                                 const sh::BlockMemberInfo &layoutInfo,
                                 const angle::MemoryBuffer *uniformData)
{
    ASSERT(layoutInfo.offset != -1);

    const int elementSize = sizeof(T) * componentCount;
    const uint8_t *source = uniformData->data() + layoutInfo.offset;

    if (layoutInfo.arrayStride == 0 || layoutInfo.arrayStride == elementSize)
    {
        const uint8_t *readPtr = source + arrayIndex * layoutInfo.arrayStride;
        memcpy(dst, readPtr, elementSize);
    }
    else
    {
        // Have to respect the arrayStride between each element of the array.
        const int arrayOffset  = arrayIndex * layoutInfo.arrayStride;
        const uint8_t *readPtr = source + arrayOffset;
        memcpy(dst, readPtr, elementSize);
    }
}

class Std140BlockLayoutEncoderFactory : public gl::CustomBlockLayoutEncoderFactory
{
  public:
    sh::BlockLayoutEncoder *makeEncoder() override { return new sh::Std140BlockEncoder(); }
};

void InitArgumentBufferEncoder(mtl::Context *context,
                               id<MTLFunction> function,
                               uint32_t bufferIndex,
                               ProgramArgumentBufferEncoderMtl *encoder)
{
    encoder->metalArgBufferEncoder = [function newArgumentEncoderWithBufferIndex:bufferIndex];
    if (encoder->metalArgBufferEncoder)
    {
        encoder->bufferPool.initialize(context, encoder->metalArgBufferEncoder.get().encodedLength,
                                       mtl::kArgumentBufferOffsetAlignment, 0);
    }
}

}  // namespace

// ProgramArgumentBufferEncoderMtl implementation
void ProgramArgumentBufferEncoderMtl::reset(ContextMtl *contextMtl)
{
    metalArgBufferEncoder = nil;
    bufferPool.destroy(contextMtl);
}

// ProgramShaderObjVariantMtl implementation
void ProgramShaderObjVariantMtl::reset(ContextMtl *contextMtl)
{
    metalShader = nil;

    uboArgBufferEncoder.reset(contextMtl);

    translatedSrcInfo = nullptr;
}

// ProgramMtl implementation
ProgramMtl::DefaultUniformBlock::DefaultUniformBlock() {}

ProgramMtl::DefaultUniformBlock::~DefaultUniformBlock() = default;

ProgramMtl::ProgramMtl(const gl::ProgramState &state)
    : ProgramImpl(state),
      mProgramHasFlatAttributes(false),
      mShadowCompareModes(),
      mMetalRenderPipelineCache(this),
      mAuxBufferPool(nullptr)
{}

ProgramMtl::~ProgramMtl() {}

void ProgramMtl::destroy(const gl::Context *context)
{
    auto contextMtl = mtl::GetImpl(context);
    if (mAuxBufferPool)
    {
        mAuxBufferPool->destroy(contextMtl);
        delete mAuxBufferPool;
        mAuxBufferPool = nullptr;
    }
    reset(contextMtl);
}

void ProgramMtl::reset(ContextMtl *context)
{
    mProgramHasFlatAttributes = false;

    for (auto &block : mDefaultUniformBlocks)
    {
        block.uniformLayout.clear();
    }

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mMslShaderTranslateInfo[shaderType].reset();
        mCurrentShaderVariants[shaderType] = nullptr;
    }
    mMslXfbOnlyVertexShaderInfo.reset();

    for (ProgramShaderObjVariantMtl &var : mVertexShaderVariants)
    {
        var.reset(context);
    }
    for (ProgramShaderObjVariantMtl &var : mFragmentShaderVariants)
    {
        var.reset(context);
    }
    if (mAuxBufferPool)
    {
        if (mAuxBufferPool->reset(context, mtl::kDefaultUniformsMaxSize * 2,
                                  mtl::kUniformBufferSettingOffsetMinAlignment,
                                  3) != angle::Result::Continue)
        {
            mAuxBufferPool->destroy(context);
            delete mAuxBufferPool;
            mAuxBufferPool = nullptr;
        }
    }
    mMetalRenderPipelineCache.clear();
}

void ProgramMtl::saveTranslatedShaders(gl::BinaryOutputStream *stream)
{
    // Write out shader sources for all shader types
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeString(mMslShaderTranslateInfo[shaderType].metalShaderSource);
    }
    stream->writeString(mMslXfbOnlyVertexShaderInfo.metalShaderSource);
}

void ProgramMtl::loadTranslatedShaders(gl::BinaryInputStream *stream)
{
    // Read in shader sources for all shader types
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mMslShaderTranslateInfo[shaderType].metalShaderSource = stream->readString();
    }
    mMslXfbOnlyVertexShaderInfo.metalShaderSource = stream->readString();
}

std::unique_ptr<rx::LinkEvent> ProgramMtl::load(const gl::Context *context,
                                                gl::BinaryInputStream *stream,
                                                gl::InfoLog &infoLog)
{

    return std::make_unique<LinkEventDone>(linkTranslatedShaders(context, stream, infoLog));
}

void ProgramMtl::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    saveTranslatedShaders(stream);
    saveShaderInternalInfo(stream);
    saveDefaultUniformBlocksInfo(stream);
}

void ProgramMtl::setBinaryRetrievableHint(bool retrievable)
{
    UNIMPLEMENTED();
}

void ProgramMtl::setSeparable(bool separable)
{
    UNIMPLEMENTED();
}

std::unique_ptr<LinkEvent> ProgramMtl::link(const gl::Context *context,
                                            const gl::ProgramLinkedResources &resources,
                                            gl::InfoLog &infoLog,
                                            const gl::ProgramMergedVaryings &mergedVaryings)
{
    // Link resources before calling GetShaderSource to make sure they are ready for the set/binding
    // assignment done in that function.
    linkResources(resources);

    // NOTE(hqle): Parallelize linking.
    return std::make_unique<LinkEventDone>(linkImpl(context, resources, infoLog));
}

#if ANGLE_ENABLE_METAL_SPIRV
angle::Result ProgramMtl::linkImplSpirv(const gl::Context *glContext,
                                        const gl::ProgramLinkedResources &resources,
                                        gl::InfoLog &infoLog)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);

    reset(contextMtl);

    ANGLE_TRY(initDefaultUniformBlocks(glContext));

    // Gather variable info and transform sources.
    gl::ShaderMap<const angle::spirv::Blob *> spirvBlobs;
    ShaderInterfaceVariableInfoMap variableInfoMap;
    ShaderInterfaceVariableInfoMap xfbOnlyVariableInfoMap;
    mtl::GlslangGetShaderSpirvCode(mState, resources, &spirvBlobs, &variableInfoMap,
                                   &xfbOnlyVariableInfoMap);

    // Convert GLSL to spirv code
    gl::ShaderMap<angle::spirv::Blob> shaderCodes;
    gl::ShaderMap<angle::spirv::Blob> xfbOnlyShaderCodes;  // only vertex shader is needed.
    ANGLE_TRY(mtl::GlslangTransformSpirvCode(mState.getExecutable().getLinkedShaderStages(),
                                             spirvBlobs, false, variableInfoMap, &shaderCodes));

    if (!mState.getLinkedTransformFeedbackVaryings().empty())
    {
        gl::ShaderBitSet onlyVS;
        onlyVS.set(gl::ShaderType::Vertex);
        ANGLE_TRY(mtl::GlslangTransformSpirvCode(onlyVS, spirvBlobs, true, xfbOnlyVariableInfoMap,
                                                 &xfbOnlyShaderCodes));
    }

    // Convert spirv code to MSL
    ANGLE_TRY(mtl::SpirvCodeToMsl(contextMtl, mState, xfbOnlyVariableInfoMap, &shaderCodes,
                                  &xfbOnlyShaderCodes[gl::ShaderType::Vertex],
                                  &mMslShaderTranslateInfo, &mMslXfbOnlyVertexShaderInfo));

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        // Create actual Metal shader library
        ANGLE_TRY(createMslShaderLib(contextMtl, shaderType, infoLog,
                                     &mMslShaderTranslateInfo[shaderType],
                                     getDefaultSubstitutionDictionary()));
    }

    return angle::Result::Continue;
}
#endif

angle::Result ProgramMtl::linkImplDirect(const gl::Context *glContext,
                                         const gl::ProgramLinkedResources &resources,
                                         gl::InfoLog &infoLog)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);

    reset(contextMtl);
    ANGLE_TRY(initDefaultUniformBlocks(glContext));
    ShaderInterfaceVariableInfoMap variableInfoMap;

    gl::ShaderMap<std::string> shaderSources;
    gl::ShaderMap<std::string> translatedMslShaders;
    mtl::MSLGetShaderSource(mState, resources, &shaderSources, &variableInfoMap);

    ANGLE_TRY(mtl::GlslangGetMSL(glContext, mState, contextMtl->getCaps(), shaderSources,
                                 variableInfoMap, &mMslShaderTranslateInfo, &translatedMslShaders,
                                 mState.getExecutable().getTransformFeedbackBufferCount()));
    mMslXfbOnlyVertexShaderInfo = mMslShaderTranslateInfo[gl::ShaderType::Vertex];
    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        // Create actual Metal shader
        ANGLE_TRY(createMslShaderLib(contextMtl, shaderType, infoLog,
                                     &mMslShaderTranslateInfo[shaderType],
                                     getDefaultSubstitutionDictionary()));
    }
    return angle::Result::Continue;
}

void ProgramMtl::linkUpdateHasFlatAttributes()
{
    mProgramHasFlatAttributes = false;

    const auto &programInputs = mState.getProgramInputs();
    for (auto &attribute : programInputs)
    {
        if (attribute.interpolation == sh::INTERPOLATION_FLAT)
        {
            mProgramHasFlatAttributes = true;
            return;
        }
    }

    const auto &flatVaryings =
        mState.getAttachedShader(gl::ShaderType::Vertex)->getOutputVaryings();
    for (auto &attribute : flatVaryings)
    {
        if (attribute.interpolation == sh::INTERPOLATION_FLAT)
        {
            mProgramHasFlatAttributes = true;
            return;
        }
    }
}

angle::Result ProgramMtl::linkImpl(const gl::Context *glContext,
                                   const gl::ProgramLinkedResources &resources,
                                   gl::InfoLog &infoLog)
{
#if ANGLE_ENABLE_METAL_SPIRV
    ContextMtl *contextMtl = mtl::GetImpl(glContext);
    if (contextMtl->getDisplay()->useDirectToMetalCompiler())
    {
        ANGLE_TRY(linkImplDirect(glContext, resources, infoLog));
    }
    else
    {
        ANGLE_TRY(linkImplSpirv(glContext, resources, infoLog));
    }
#else
    ANGLE_TRY(linkImplDirect(glContext, resources, infoLog));
#endif
    linkUpdateHasFlatAttributes();
    return angle::Result::Continue;
}

angle::Result ProgramMtl::linkTranslatedShaders(const gl::Context *glContext,
                                                gl::BinaryInputStream *stream,
                                                gl::InfoLog &infoLog)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);
    // NOTE(hqle): No transform feedbacks for now, since we only support ES 2.0 atm

    reset(contextMtl);

    loadTranslatedShaders(stream);
    loadShaderInternalInfo(stream);
    ANGLE_TRY(loadDefaultUniformBlocksInfo(glContext, stream));
    ANGLE_TRY(createMslShaderLib(contextMtl, gl::ShaderType::Vertex, infoLog,
                                 &mMslShaderTranslateInfo[gl::ShaderType::Vertex],
                                 getDefaultSubstitutionDictionary()));
    ANGLE_TRY(createMslShaderLib(contextMtl, gl::ShaderType::Fragment, infoLog,
                                 &mMslShaderTranslateInfo[gl::ShaderType::Fragment],
                                 getDefaultSubstitutionDictionary()));

    return angle::Result::Continue;
}

mtl::BufferPool *ProgramMtl::getBufferPool(ContextMtl *context)
{
    if (mAuxBufferPool == nullptr)
    {
        mAuxBufferPool = new mtl::BufferPool(true);
        mAuxBufferPool->initialize(context, mtl::kDefaultUniformsMaxSize * 2,
                                   mtl::kUniformBufferSettingOffsetMinAlignment, 3);
    }
    return mAuxBufferPool;
}
void ProgramMtl::linkResources(const gl::ProgramLinkedResources &resources)
{
    Std140BlockLayoutEncoderFactory std140EncoderFactory;
    gl::ProgramLinkedResourcesLinker linker(&std140EncoderFactory);

    linker.linkResources(mState, resources);
}

angle::Result ProgramMtl::initDefaultUniformBlocks(const gl::Context *glContext)
{
    // Process vertex and fragment uniforms into std140 packing.
    gl::ShaderMap<sh::BlockLayoutMap> layoutMap;
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        gl::Shader *shader = mState.getAttachedShader(shaderType);
        if (shader)
        {
            const std::vector<sh::Uniform> &uniforms = shader->getUniforms();
            InitDefaultUniformBlock(uniforms, shader, &layoutMap[shaderType],
                                    &requiredBufferSize[shaderType]);
        }
    }

    // Init the default block layout info.
    const auto &uniforms         = mState.getUniforms();
    const auto &uniformLocations = mState.getUniformLocations();
    for (size_t locSlot = 0; locSlot < uniformLocations.size(); ++locSlot)
    {
        const gl::VariableLocation &location = uniformLocations[locSlot];
        gl::ShaderMap<sh::BlockMemberInfo> layoutInfo;

        if (location.used() && !location.ignored)
        {
            const gl::LinkedUniform &uniform = uniforms[location.index];
            if (uniform.isInDefaultBlock() && !uniform.isSampler())
            {
                std::string uniformName = uniform.name;
                if (uniform.isArray())
                {
                    // Gets the uniform name without the [0] at the end.
                    uniformName = gl::ParseResourceName(uniformName, nullptr);
                }

                bool found = false;

                for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
                {
                    auto it = layoutMap[shaderType].find(uniformName);
                    if (it != layoutMap[shaderType].end())
                    {
                        found                  = true;
                        layoutInfo[shaderType] = it->second;
                    }
                }

                ASSERT(found);
            }
        }

        for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
        {
            mDefaultUniformBlocks[shaderType].uniformLayout.push_back(layoutInfo[shaderType]);
        }
    }

    return resizeDefaultUniformBlocksMemory(glContext, requiredBufferSize);
}

angle::Result ProgramMtl::resizeDefaultUniformBlocksMemory(
    const gl::Context *glContext,
    const gl::ShaderMap<size_t> &requiredBufferSize)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if (requiredBufferSize[shaderType] > 0)
        {
            ASSERT(requiredBufferSize[shaderType] <= mtl::kDefaultUniformsMaxSize);

            if (!mDefaultUniformBlocks[shaderType].uniformData.resize(
                    requiredBufferSize[shaderType]))
            {
                ANGLE_MTL_CHECK(contextMtl, false, GL_OUT_OF_MEMORY);
            }

            // Initialize uniform buffer memory to zero by default.
            mDefaultUniformBlocks[shaderType].uniformData.fill(0);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }

    return angle::Result::Continue;
}

angle::Result ProgramMtl::getSpecializedShader(ContextMtl *context,
                                               gl::ShaderType shaderType,
                                               const mtl::RenderPipelineDesc &renderPipelineDesc,
                                               id<MTLFunction> *shaderOut)
{
    static_assert(YES == 1, "YES should have value of 1");
#if ANGLE_ENABLE_METAL_SPIRV
    const bool useSpirv = !context->getDisplay()->useDirectToMetalCompiler();
#endif

    mtl::TranslatedShaderInfo *translatedMslInfo = &mMslShaderTranslateInfo[shaderType];
    ProgramShaderObjVariantMtl *shaderVariant;
    mtl::AutoObjCObj<MTLFunctionConstantValues> funcConstants;

    if (shaderType == gl::ShaderType::Vertex)
    {
        // For vertex shader, we need to create 3 variants, one with emulated rasterization
        // discard, one with true rasterization discard and one without.
        shaderVariant = &mVertexShaderVariants[renderPipelineDesc.rasterizationType];
        if (shaderVariant->metalShader)
        {
            // Already created.
            *shaderOut = shaderVariant->metalShader;
            return angle::Result::Continue;
        }

        if (renderPipelineDesc.rasterizationType == mtl::RenderPipelineRasterization::Disabled)
        {
            // Special case: XFB output only vertex shader.
            ASSERT(!mState.getLinkedTransformFeedbackVaryings().empty());
            translatedMslInfo = &mMslXfbOnlyVertexShaderInfo;
            if (!translatedMslInfo->metalLibrary)
            {
                // Lazily compile XFB only shader
                gl::InfoLog infoLog;
                ANGLE_TRY(createMslShaderLib(context, shaderType, infoLog,
                                             &mMslXfbOnlyVertexShaderInfo,
                                             @{@"TRANSFORM_FEEDBACK_ENABLED" : @"1"}));
                translatedMslInfo->metalLibrary.get().label = @"TransformFeedback";
            }
        }

        ANGLE_MTL_OBJC_SCOPE
        {
            BOOL emulateDiscard = renderPipelineDesc.rasterizationType ==
                                  mtl::RenderPipelineRasterization::EmulatedDiscard;

            NSString *discardEnabledStr;
#if ANGLE_ENABLE_METAL_SPIRV
            if (useSpirv)
            {
                discardEnabledStr =
                    [NSString stringWithFormat:@"%s%s", sh::mtl::kRasterizerDiscardEnabledConstName,
                                               kSpirvCrossSpecConstSuffix];
            }
            else
#endif
            {
                discardEnabledStr =
                    [NSString stringWithUTF8String:sh::mtl::kRasterizerDiscardEnabledConstName];
            }

            funcConstants = mtl::adoptObjCObj([[MTLFunctionConstantValues alloc] init]);
            [funcConstants setConstantValue:&emulateDiscard
                                       type:MTLDataTypeBool
                                   withName:discardEnabledStr];
        }
    }  // if (shaderType == gl::ShaderType::Vertex)
    else if (shaderType == gl::ShaderType::Fragment)
    {
        // For fragment shader, we need to create 2 variants, one with sample coverage mask
        // disabled, one with the mask enabled.
        BOOL emulateCoverageMask = renderPipelineDesc.emulateCoverageMask;
        shaderVariant            = &mFragmentShaderVariants[emulateCoverageMask];
        if (shaderVariant->metalShader)
        {
            // Already created.
            *shaderOut = shaderVariant->metalShader;
            return angle::Result::Continue;
        }

        ANGLE_MTL_OBJC_SCOPE
        {
            NSString *coverageMaskEnabledStr;
#if ANGLE_ENABLE_METAL_SPIRV
            if (useSpirv)
            {
                coverageMaskEnabledStr =
                    [NSString stringWithFormat:@"%s%s", sh::mtl::kCoverageMaskEnabledConstName,
                                               kSpirvCrossSpecConstSuffix];
            }
            else
#endif
            {
                coverageMaskEnabledStr =
                    [NSString stringWithUTF8String:sh::mtl::kCoverageMaskEnabledConstName];
            }

            funcConstants = mtl::adoptObjCObj([[MTLFunctionConstantValues alloc] init]);
            [funcConstants setConstantValue:&emulateCoverageMask
                                       type:MTLDataTypeBool
                                   withName:coverageMaskEnabledStr];
        }

    }  // gl::ShaderType::Fragment
    else
    {
        UNREACHABLE();
        return angle::Result::Stop;
    }
    [funcConstants
        setConstantValue:&(context->getDisplay()->getFeatures().allowSamplerCompareGradient.enabled)
                    type:MTLDataTypeBool
                withName:@"ANGLEUseSampleCompareGradient"];
    [funcConstants
        setConstantValue:&(context->getDisplay()->getFeatures().allowSamplerCompareLod.enabled)
                    type:MTLDataTypeBool
                withName:@"ANGLEUseSampleCompareLod"];

    // Create Metal shader object
    ANGLE_MTL_OBJC_SCOPE
    {
        ANGLE_TRY(CreateMslShader(context, translatedMslInfo->metalLibrary, SHADER_ENTRY_NAME,
                                  funcConstants.get(), &shaderVariant->metalShader));
    }

    // Store reference to the translated source for easily querying mapped bindings later.
    shaderVariant->translatedSrcInfo = translatedMslInfo;

    // Initialize argument buffer encoder if required
    if (translatedMslInfo->hasUBOArgumentBuffer)
    {
        InitArgumentBufferEncoder(context, shaderVariant->metalShader,
                                  mtl::kUBOArgumentBufferBindingIndex,
                                  &shaderVariant->uboArgBufferEncoder);
    }

    *shaderOut = shaderVariant->metalShader;

    return angle::Result::Continue;
}

bool ProgramMtl::hasSpecializedShader(gl::ShaderType shaderType,
                                      const mtl::RenderPipelineDesc &renderPipelineDesc)
{
    return true;
}

angle::Result ProgramMtl::createMslShaderLib(
    ContextMtl *context,
    gl::ShaderType shaderType,
    gl::InfoLog &infoLog,
    mtl::TranslatedShaderInfo *translatedMslInfo,
    NSDictionary<NSString *, NSObject *> *substitutionMacros)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        const mtl::ContextDevice &metalDevice = context->getMetalDevice();

        // Convert to actual binary shader
        mtl::AutoObjCPtr<NSError *> err = nil;
        bool disableFastMath = (context->getDisplay()->getFeatures().intelDisableFastMath.enabled &&
                                translatedMslInfo->hasInvariantOrAtan);
        translatedMslInfo->metalLibrary =
            mtl::CreateShaderLibrary(metalDevice, translatedMslInfo->metalShaderSource,
                                     substitutionMacros, !disableFastMath, &err);
        if (err && !translatedMslInfo->metalLibrary)
        {
            std::ostringstream ss;
            ss << "Internal error compiling shader with Metal backend.\n";
#if !defined(NDEBUG)
            ss << err.get().localizedDescription.UTF8String << "\n";
            ss << "-----\n";
            ss << translatedMslInfo->metalShaderSource;
            ss << "-----\n";
#else
            ss << "Please submit this shader, or website as a bug to https://bugs.webkit.org\n";
#endif
            ERR() << ss.str();

            infoLog << ss.str();

            ANGLE_MTL_CHECK(context, false, GL_INVALID_OPERATION);
        }

        return angle::Result::Continue;
    }
}

void ProgramMtl::saveDefaultUniformBlocksInfo(gl::BinaryOutputStream *stream)
{
    // Serializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const size_t uniformCount = mDefaultUniformBlocks[shaderType].uniformLayout.size();
        stream->writeInt<size_t>(uniformCount);
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
        {
            sh::BlockMemberInfo &blockInfo =
                mDefaultUniformBlocks[shaderType].uniformLayout[uniformIndex];
            gl::WriteBlockMemberInfo(stream, blockInfo);
        }
    }

    // Serializes required uniform block memory sizes
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt(mDefaultUniformBlocks[shaderType].uniformData.size());
    }
}

angle::Result ProgramMtl::loadDefaultUniformBlocksInfo(const gl::Context *glContext,
                                                       gl::BinaryInputStream *stream)
{
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);

    // Deserializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const size_t uniformCount = stream->readInt<size_t>();
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
        {
            sh::BlockMemberInfo blockInfo;
            gl::LoadBlockMemberInfo(stream, &blockInfo);
            mDefaultUniformBlocks[shaderType].uniformLayout.push_back(blockInfo);
        }
    }

    // Deserializes required uniform block memory sizes
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        requiredBufferSize[shaderType] = stream->readInt<size_t>();
    }

    return resizeDefaultUniformBlocksMemory(glContext, requiredBufferSize);
}

void ProgramMtl::saveShaderInternalInfo(gl::BinaryOutputStream *stream)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt<int>(mMslShaderTranslateInfo[shaderType].hasUBOArgumentBuffer);
        for (const mtl::SamplerBinding &binding :
             mMslShaderTranslateInfo[shaderType].actualSamplerBindings)
        {
            stream->writeInt<uint32_t>(binding.textureBinding);
            stream->writeInt<uint32_t>(binding.samplerBinding);
        }

        for (uint32_t uboBinding : mMslShaderTranslateInfo[shaderType].actualUBOBindings)
        {
            stream->writeInt<uint32_t>(uboBinding);
        }
        stream->writeBool(mMslShaderTranslateInfo[shaderType].hasInvariantOrAtan);
    }
    for (size_t xfbBindIndex = 0; xfbBindIndex < mtl::kMaxShaderXFBs; xfbBindIndex++)
    {
        stream->writeInt(
            mMslShaderTranslateInfo[gl::ShaderType::Vertex].actualXFBBindings[xfbBindIndex]);
    }

    // Write out XFB info.
    {
        stream->writeInt<int>(mMslXfbOnlyVertexShaderInfo.hasUBOArgumentBuffer);
        for (mtl::SamplerBinding &binding : mMslXfbOnlyVertexShaderInfo.actualSamplerBindings)
        {
            stream->writeInt<uint32_t>(binding.textureBinding);
            stream->writeInt<uint32_t>(binding.samplerBinding);
        }

        for (uint32_t &uboBinding : mMslXfbOnlyVertexShaderInfo.actualUBOBindings)
        {
            stream->writeInt<uint32_t>(uboBinding);
        }
        for (size_t xfbBindIndex = 0; xfbBindIndex < mtl::kMaxShaderXFBs; xfbBindIndex++)
        {
            stream->writeInt(mMslXfbOnlyVertexShaderInfo.actualXFBBindings[xfbBindIndex]);
        }
    }

    stream->writeBool(mProgramHasFlatAttributes);
}

void ProgramMtl::loadShaderInternalInfo(gl::BinaryInputStream *stream)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mMslShaderTranslateInfo[shaderType].hasUBOArgumentBuffer = stream->readInt<int>() != 0;
        for (mtl::SamplerBinding &binding :
             mMslShaderTranslateInfo[shaderType].actualSamplerBindings)
        {
            binding.textureBinding = stream->readInt<uint32_t>();
            binding.samplerBinding = stream->readInt<uint32_t>();
        }

        for (uint32_t &uboBinding : mMslShaderTranslateInfo[shaderType].actualUBOBindings)
        {
            uboBinding = stream->readInt<uint32_t>();
        }
        mMslShaderTranslateInfo[shaderType].hasInvariantOrAtan = stream->readBool();
    }

    for (size_t xfbBindIndex = 0; xfbBindIndex < mtl::kMaxShaderXFBs; xfbBindIndex++)
    {
        stream->readInt(
            &mMslShaderTranslateInfo[gl::ShaderType::Vertex].actualXFBBindings[xfbBindIndex]);
    }
    // Load Transform Feedback info
    {
        mMslXfbOnlyVertexShaderInfo.hasUBOArgumentBuffer = stream->readInt<int>() != 0;
        for (mtl::SamplerBinding &binding : mMslXfbOnlyVertexShaderInfo.actualSamplerBindings)
        {
            binding.textureBinding = stream->readInt<uint32_t>();
            binding.samplerBinding = stream->readInt<uint32_t>();
        }

        for (uint32_t &uboBinding : mMslXfbOnlyVertexShaderInfo.actualUBOBindings)
        {
            uboBinding = stream->readInt<uint32_t>();
        }
        for (size_t xfbBindIndex = 0; xfbBindIndex < mtl::kMaxShaderXFBs; xfbBindIndex++)
        {
            stream->readInt(&mMslXfbOnlyVertexShaderInfo.actualXFBBindings[xfbBindIndex]);
        }
        mMslXfbOnlyVertexShaderInfo.metalLibrary = nullptr;
    }

    mProgramHasFlatAttributes = stream->readBool();
}

GLboolean ProgramMtl::validate(const gl::Caps &caps, gl::InfoLog *infoLog)
{
    // No-op. The spec is very vague about the behavior of validation.
    return GL_TRUE;
}

template <typename T>
void ProgramMtl::setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType)
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    if (linkedUniform.isSampler())
    {
        // Sampler binding has changed.
        mSamplerBindingsDirty.set();
        return;
    }

    if (linkedUniform.typeInfo->type == entryPointType)
    {
        for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
        {
            DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.typeInfo->componentCount;
            UpdateDefaultUniformBlock(count, locationInfo.arrayIndex, componentCount, v, layoutInfo,
                                      &uniformBlock.uniformData);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
    else
    {
        for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
        {
            DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.typeInfo->componentCount;

            ASSERT(linkedUniform.typeInfo->type == gl::VariableBoolVectorType(entryPointType));

            GLint initialArrayOffset =
                locationInfo.arrayIndex * layoutInfo.arrayStride + layoutInfo.offset;
            for (GLint i = 0; i < count; i++)
            {
                GLint elementOffset = i * layoutInfo.arrayStride + initialArrayOffset;
                GLint *dest =
                    reinterpret_cast<GLint *>(uniformBlock.uniformData.data() + elementOffset);
                const T *source = v + i * componentCount;

                for (int c = 0; c < componentCount; c++)
                {
                    dest[c] = (source[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE;
                }
            }

            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
}

template <typename T>
void ProgramMtl::getUniformImpl(GLint location, T *v, GLenum entryPointType) const
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    ASSERT(!linkedUniform.isSampler());

    const gl::ShaderType shaderType = linkedUniform.getFirstShaderTypeWhereActive();
    ASSERT(shaderType != gl::ShaderType::InvalidEnum);

    const DefaultUniformBlock &uniformBlock = mDefaultUniformBlocks[shaderType];
    const sh::BlockMemberInfo &layoutInfo   = uniformBlock.uniformLayout[location];

    ASSERT(linkedUniform.typeInfo->componentType == entryPointType ||
           linkedUniform.typeInfo->componentType == gl::VariableBoolVectorType(entryPointType));

    if (gl::IsMatrixType(linkedUniform.type))
    {
        const uint8_t *ptrToElement = uniformBlock.uniformData.data() + layoutInfo.offset +
                                      (locationInfo.arrayIndex * layoutInfo.arrayStride);
        GetMatrixUniform(linkedUniform.type, v, reinterpret_cast<const T *>(ptrToElement), false);
    }
    else
    {
        ReadFromDefaultUniformBlock(linkedUniform.typeInfo->componentCount, locationInfo.arrayIndex,
                                    v, layoutInfo, &uniformBlock.uniformData);
    }
}

void ProgramMtl::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT);
}

void ProgramMtl::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC2);
}

void ProgramMtl::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC3);
}

void ProgramMtl::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC4);
}

void ProgramMtl::setUniform1iv(GLint startLocation, GLsizei count, const GLint *v)
{
    setUniformImpl(startLocation, count, v, GL_INT);
}

void ProgramMtl::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC2);
}

void ProgramMtl::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC3);
}

void ProgramMtl::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC4);
}

void ProgramMtl::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT);
}

void ProgramMtl::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC2);
}

void ProgramMtl::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC3);
}

void ProgramMtl::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC4);
}

template <int cols, int rows>
void ProgramMtl::setUniformMatrixfv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
        const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

        // Assume an offset of -1 means the block is unused.
        if (layoutInfo.offset == -1)
        {
            continue;
        }

        SetFloatUniformMatrixGLSL<cols, rows>::Run(
            locationInfo.arrayIndex, linkedUniform.getArraySizeProduct(), count, transpose, value,
            uniformBlock.uniformData.data() + layoutInfo.offset);

        mDefaultUniformBlocksDirty.set(shaderType);
    }
}

void ProgramMtl::setUniformMatrix2fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfv<2, 2>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix3fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfv<3, 3>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix4fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfv<4, 4>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix2x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<2, 3>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix3x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<3, 2>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix2x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<2, 4>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix4x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<4, 2>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix3x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<3, 4>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix4x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<4, 3>(location, count, transpose, value);
}

void ProgramMtl::getUniformfv(const gl::Context *context, GLint location, GLfloat *params) const
{
    getUniformImpl(location, params, GL_FLOAT);
}

void ProgramMtl::getUniformiv(const gl::Context *context, GLint location, GLint *params) const
{
    getUniformImpl(location, params, GL_INT);
}

void ProgramMtl::getUniformuiv(const gl::Context *context, GLint location, GLuint *params) const
{
    getUniformImpl(location, params, GL_UNSIGNED_INT);
}

angle::Result ProgramMtl::setupDraw(const gl::Context *glContext,
                                    mtl::RenderCommandEncoder *cmdEncoder,
                                    const mtl::RenderPipelineDesc &pipelineDesc,
                                    bool pipelineDescChanged,
                                    bool forceTexturesSetting,
                                    bool uniformBuffersDirty)
{
    ContextMtl *context = mtl::GetImpl(glContext);
    if (pipelineDescChanged)
    {
        // Render pipeline state needs to be changed
        id<MTLRenderPipelineState> pipelineState =
            mMetalRenderPipelineCache.getRenderPipelineState(context, pipelineDesc);
        if (!pipelineState)
        {
            // Error already logged inside getRenderPipelineState()
            return angle::Result::Stop;
        }
        cmdEncoder->setRenderPipelineState(pipelineState);

        // We need to rebind uniform buffers & textures also
        mDefaultUniformBlocksDirty.set();
        mSamplerBindingsDirty.set();

        // Cache current shader variant references for easier querying.
        mCurrentShaderVariants[gl::ShaderType::Vertex] =
            &mVertexShaderVariants[pipelineDesc.rasterizationType];
        mCurrentShaderVariants[gl::ShaderType::Fragment] =
            pipelineDesc.rasterizationEnabled()
                ? &mFragmentShaderVariants[pipelineDesc.emulateCoverageMask]
                : nullptr;
    }

    ANGLE_TRY(commitUniforms(context, cmdEncoder));
    ANGLE_TRY(updateTextures(glContext, cmdEncoder, forceTexturesSetting));

    if (uniformBuffersDirty || pipelineDescChanged)
    {
        ANGLE_TRY(updateUniformBuffers(context, cmdEncoder, pipelineDesc));
    }

    if (pipelineDescChanged)
    {
        ANGLE_TRY(updateXfbBuffers(context, cmdEncoder, pipelineDesc));
    }

    return angle::Result::Continue;
}

angle::Result ProgramMtl::commitUniforms(ContextMtl *context, mtl::RenderCommandEncoder *cmdEncoder)
{
    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if (!mDefaultUniformBlocksDirty[shaderType] || !mCurrentShaderVariants[shaderType])
        {
            continue;
        }
        DefaultUniformBlock &uniformBlock = mDefaultUniformBlocks[shaderType];

        if (!uniformBlock.uniformData.size())
        {
            continue;
        }
        // If we exceed the default uniform max size, try to allocate a buffer. Worst case
        // scenario, fall back on a large setBytes.
        bool needsCommitUniform = true;
        if (needsCommitUniform)
        {
            ASSERT(uniformBlock.uniformData.size() <= mtl::kDefaultUniformsMaxSize);
            cmdEncoder->setBytes(shaderType, uniformBlock.uniformData.data(),
                                 uniformBlock.uniformData.size(),
                                 mtl::kDefaultUniformsBindingIndex);
        }

        mDefaultUniformBlocksDirty.reset(shaderType);
    }

    return angle::Result::Continue;
}

angle::Result ProgramMtl::updateTextures(const gl::Context *glContext,
                                         mtl::RenderCommandEncoder *cmdEncoder,
                                         bool forceUpdate)
{
    ContextMtl *contextMtl                          = mtl::GetImpl(glContext);
    const auto &glState                             = glContext->getState();
    const gl::ActiveTexturesCache &completeTextures = glState.getActiveTexturesCache();

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if ((!mSamplerBindingsDirty[shaderType] && !forceUpdate) ||
            !mCurrentShaderVariants[shaderType])
        {
            continue;
        }

        const mtl::TranslatedShaderInfo &shaderInfo =
            mCurrentShaderVariants[shaderType]->translatedSrcInfo
                ? *mCurrentShaderVariants[shaderType]->translatedSrcInfo
                : mMslShaderTranslateInfo[shaderType];
        bool hasDepthSampler = false;

        for (uint32_t textureIndex = 0; textureIndex < mState.getSamplerBindings().size();
             ++textureIndex)
        {
            const gl::SamplerBinding &samplerBinding = mState.getSamplerBindings()[textureIndex];
            const mtl::SamplerBinding &mslBinding = shaderInfo.actualSamplerBindings[textureIndex];
            if (mslBinding.textureBinding >= mtl::kMaxShaderSamplers)
            {
                // No binding assigned
                continue;
            }

            gl::TextureType textureType = samplerBinding.textureType;

            for (uint32_t arrayElement = 0; arrayElement < samplerBinding.boundTextureUnits.size();
                 ++arrayElement)
            {
                GLuint textureUnit   = samplerBinding.boundTextureUnits[arrayElement];
                gl::Texture *texture = completeTextures[textureUnit];
                gl::Sampler *sampler = contextMtl->getState().getSampler(textureUnit);
                uint32_t textureSlot = mslBinding.textureBinding + arrayElement;
                uint32_t samplerSlot = mslBinding.samplerBinding + arrayElement;
                if (!texture)
                {
                    ANGLE_TRY(contextMtl->getIncompleteTexture(glContext, textureType, &texture));
                }
                const gl::SamplerState *samplerState =
                    sampler ? &sampler->getSamplerState() : &texture->getSamplerState();
                TextureMtl *textureMtl = mtl::GetImpl(texture);
                if (samplerBinding.format == gl::SamplerFormat::Shadow)
                {
                    hasDepthSampler                  = true;
                    mShadowCompareModes[textureSlot] = mtl::MslGetShaderShadowCompareMode(
                        samplerState->getCompareMode(), samplerState->getCompareFunc());
                }
                ANGLE_TRY(textureMtl->bindToShader(glContext, cmdEncoder, shaderType, sampler,
                                                   textureSlot, samplerSlot));
            }  // for array elements
        }      // for sampler bindings

        if (hasDepthSampler)
        {
            cmdEncoder->setData(shaderType, mShadowCompareModes,
                                mtl::kShadowSamplerCompareModesBindingIndex);
        }
    }  // for shader types

    return angle::Result::Continue;
}

angle::Result ProgramMtl::updateUniformBuffers(ContextMtl *context,
                                               mtl::RenderCommandEncoder *cmdEncoder,
                                               const mtl::RenderPipelineDesc &pipelineDesc)
{
    const std::vector<gl::InterfaceBlock> &blocks = mState.getUniformBlocks();
    if (blocks.empty())
    {
        return angle::Result::Continue;
    }

    // This array is only used inside this function and its callees.
    ScopedAutoClearVector<uint32_t> scopeArrayClear(&mArgumentBufferRenderStageUsages);
    ScopedAutoClearVector<std::pair<mtl::BufferRef, uint32_t>> scopeArrayClear2(
        &mLegalizedOffsetedUniformBuffers);
    mArgumentBufferRenderStageUsages.resize(blocks.size());
    mLegalizedOffsetedUniformBuffers.resize(blocks.size());

    ANGLE_TRY(legalizeUniformBufferOffsets(context, blocks));

    const gl::State &glState = context->getState();

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if (!mCurrentShaderVariants[shaderType])
        {
            continue;
        }

        if (mCurrentShaderVariants[shaderType]->translatedSrcInfo->hasUBOArgumentBuffer)
        {
            ANGLE_TRY(
                encodeUniformBuffersInfoArgumentBuffer(context, cmdEncoder, blocks, shaderType));
        }
        else
        {
            ANGLE_TRY(bindUniformBuffersToDiscreteSlots(context, cmdEncoder, blocks, shaderType));
        }
    }  // for shader types

    // After encode the uniform buffers into an argument buffer, we need to tell Metal that
    // the buffers are being used by what shader stages.
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedUniformBuffer(block.binding);
        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        // Remove any other stages other than vertex and fragment.
        uint32_t stages = mArgumentBufferRenderStageUsages[bufferIndex] &
                          (mtl::kRenderStageVertex | mtl::kRenderStageFragment);

        if (stages == 0)
        {
            continue;
        }

        cmdEncoder->useResource(mLegalizedOffsetedUniformBuffers[bufferIndex].first,
                                MTLResourceUsageRead, static_cast<mtl::RenderStages>(stages));
    }

    return angle::Result::Continue;
}

angle::Result ProgramMtl::legalizeUniformBufferOffsets(
    ContextMtl *context,
    const std::vector<gl::InterfaceBlock> &blocks)
{
    const gl::State &glState = context->getState();

    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedUniformBuffer(block.binding);

        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        BufferMtl *bufferMtl = mtl::GetImpl(bufferBinding.get());
        size_t srcOffset     = std::min<size_t>(bufferBinding.getOffset(), bufferMtl->size());
        size_t offsetModulo  = srcOffset % mtl::kUniformBufferSettingOffsetMinAlignment;
        if (offsetModulo)
        {
            ConversionBufferMtl *conversion =
                bufferMtl->getUniformConversionBuffer(context, offsetModulo);
            // Has the content of the buffer has changed since last conversion?
            if (conversion->dirty)
            {
                const uint8_t *srcBytes = bufferMtl->getClientShadowCopyData(context);
                srcBytes += offsetModulo;
                size_t sizeToCopy      = bufferMtl->size() - offsetModulo;
                size_t bytesToAllocate = roundUp<size_t>(sizeToCopy, 16u);
                ANGLE_TRY(StreamUniformBufferData(
                    context, &conversion->data, srcBytes, bytesToAllocate, sizeToCopy,
                    &conversion->convertedBuffer, &conversion->convertedOffset));
#ifndef NDEBUG
                ANGLE_MTL_OBJC_SCOPE
                {
                    conversion->convertedBuffer->get().label = [NSString
                        stringWithFormat:@"Converted from %p offset=%zu", bufferMtl, offsetModulo];
                }
#endif
                conversion->dirty = false;
            }
            // reuse the converted buffer
            mLegalizedOffsetedUniformBuffers[bufferIndex].first = conversion->convertedBuffer;
            mLegalizedOffsetedUniformBuffers[bufferIndex].second =
                static_cast<uint32_t>(conversion->convertedOffset + srcOffset - offsetModulo);
        }
        else
        {
            mLegalizedOffsetedUniformBuffers[bufferIndex].first = bufferMtl->getCurrentBuffer();
            mLegalizedOffsetedUniformBuffers[bufferIndex].second =
                static_cast<uint32_t>(bufferBinding.getOffset());
        }
    }
    return angle::Result::Continue;
}

angle::Result ProgramMtl::bindUniformBuffersToDiscreteSlots(
    ContextMtl *context,
    mtl::RenderCommandEncoder *cmdEncoder,
    const std::vector<gl::InterfaceBlock> &blocks,
    gl::ShaderType shaderType)
{
    const gl::State &glState = context->getState();
    const mtl::TranslatedShaderInfo &shaderInfo =
        *mCurrentShaderVariants[shaderType]->translatedSrcInfo;
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedUniformBuffer(block.binding);

        if (bufferBinding.get() == nullptr || !block.activeShaders().test(shaderType))
        {
            continue;
        }

        uint32_t actualBufferIdx = shaderInfo.actualUBOBindings[bufferIndex];

        if (actualBufferIdx >= mtl::kMaxShaderBuffers)
        {
            continue;
        }

        mtl::BufferRef mtlBuffer = mLegalizedOffsetedUniformBuffers[bufferIndex].first;
        uint32_t offset          = mLegalizedOffsetedUniformBuffers[bufferIndex].second;
        cmdEncoder->setBuffer(shaderType, mtlBuffer, offset, actualBufferIdx);
    }
    return angle::Result::Continue;
}
angle::Result ProgramMtl::encodeUniformBuffersInfoArgumentBuffer(
    ContextMtl *context,
    mtl::RenderCommandEncoder *cmdEncoder,
    const std::vector<gl::InterfaceBlock> &blocks,
    gl::ShaderType shaderType)
{
    const gl::State &glState = context->getState();
    ASSERT(mCurrentShaderVariants[shaderType]->translatedSrcInfo);
    const mtl::TranslatedShaderInfo &shaderInfo =
        *mCurrentShaderVariants[shaderType]->translatedSrcInfo;

    // Encode all uniform buffers into an argument buffer.
    ProgramArgumentBufferEncoderMtl &bufferEncoder =
        mCurrentShaderVariants[shaderType]->uboArgBufferEncoder;

    mtl::BufferRef argumentBuffer;
    size_t argumentBufferOffset;
    bufferEncoder.bufferPool.releaseInFlightBuffers(context);
    ANGLE_TRY(bufferEncoder.bufferPool.allocate(
        context, bufferEncoder.metalArgBufferEncoder.get().encodedLength, nullptr, &argumentBuffer,
        &argumentBufferOffset));

    [bufferEncoder.metalArgBufferEncoder setArgumentBuffer:argumentBuffer->get()
                                                    offset:argumentBufferOffset];

    constexpr gl::ShaderMap<MTLRenderStages> kShaderStageMap = {
        {gl::ShaderType::Vertex, mtl::kRenderStageVertex},
        {gl::ShaderType::Fragment, mtl::kRenderStageFragment},
    };

    auto mtlRenderStage = kShaderStageMap[shaderType];

    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedUniformBuffer(block.binding);

        if (bufferBinding.get() == nullptr || !block.activeShaders().test(shaderType))
        {
            continue;
        }

        mArgumentBufferRenderStageUsages[bufferIndex] |= mtlRenderStage;

        uint32_t actualBufferIdx = shaderInfo.actualUBOBindings[bufferIndex];
        if (actualBufferIdx >= mtl::kMaxShaderBuffers)
        {
            continue;
        }

        mtl::BufferRef mtlBuffer = mLegalizedOffsetedUniformBuffers[bufferIndex].first;
        uint32_t offset          = mLegalizedOffsetedUniformBuffers[bufferIndex].second;
        [bufferEncoder.metalArgBufferEncoder setBuffer:mtlBuffer->get()
                                                offset:offset
                                               atIndex:actualBufferIdx];
    }

    ANGLE_TRY(bufferEncoder.bufferPool.commit(context));

    cmdEncoder->setBuffer(shaderType, argumentBuffer, static_cast<uint32_t>(argumentBufferOffset),
                          mtl::kUBOArgumentBufferBindingIndex);
    return angle::Result::Continue;
}

angle::Result ProgramMtl::updateXfbBuffers(ContextMtl *context,
                                           mtl::RenderCommandEncoder *cmdEncoder,
                                           const mtl::RenderPipelineDesc &pipelineDesc)
{
    const gl::State &glState                 = context->getState();
    gl::TransformFeedback *transformFeedback = glState.getCurrentTransformFeedback();

    if (pipelineDesc.rasterizationEnabled() || !glState.isTransformFeedbackActiveUnpaused() ||
        ANGLE_UNLIKELY(!transformFeedback))
    {
        // XFB output can only be used with rasterization disabled.
        return angle::Result::Continue;
    }

    size_t xfbBufferCount = glState.getProgramExecutable()->getTransformFeedbackBufferCount();

    ASSERT(xfbBufferCount > 0);
    ASSERT(mState.getTransformFeedbackBufferMode() != GL_INTERLEAVED_ATTRIBS ||
           xfbBufferCount == 1);

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        uint32_t actualBufferIdx = mMslXfbOnlyVertexShaderInfo.actualXFBBindings[bufferIndex];

        if (actualBufferIdx >= mtl::kMaxShaderBuffers)
        {
            continue;
        }

        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            transformFeedback->getIndexedBuffer(bufferIndex);
        gl::Buffer *buffer = bufferBinding.get();
        ASSERT((bufferBinding.getOffset() % 4) == 0);
        ASSERT(buffer != nullptr);

        BufferMtl *bufferMtl = mtl::GetImpl(buffer);

        // Use offset=0, actual offset will be set in Driver Uniform inside ContextMtl.
        cmdEncoder->setBufferForWrite(gl::ShaderType::Vertex, bufferMtl->getCurrentBuffer(), 0,
                                      actualBufferIdx);
    }

    return angle::Result::Continue;
}

}  // namespace rx
