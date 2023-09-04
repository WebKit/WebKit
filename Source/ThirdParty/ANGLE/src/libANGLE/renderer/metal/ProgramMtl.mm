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

#include "common/WorkerThread.h"
#include "common/debug.h"
#include "common/system_utils.h"

#include "libANGLE/Context.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/renderer/metal/BufferMtl.h"
#include "libANGLE/renderer/metal/CompilerMtl.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/TextureMtl.h"
#include "libANGLE/renderer/metal/blocklayoutMetal.h"
#include "libANGLE/renderer/metal/mtl_msl_utils.h"
#include "libANGLE/renderer/metal/mtl_utils.h"
#include "libANGLE/renderer/metal/renderermtl_utils.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/trace.h"

#define ANGLE_PARALLEL_LINK_RETURN(X) return std::make_unique<LinkEventDone>(X)
#define ANGLE_PARALLEL_LINK_TRY(EXPR) ANGLE_TRY_TEMPLATE(EXPR, ANGLE_PARALLEL_LINK_RETURN)

namespace rx
{

namespace
{

// Block Encoder Information

#define SHADER_ENTRY_NAME @"main0"
template <typename T>
class [[nodiscard]] ScopedAutoClearVector
{
  public:
    ScopedAutoClearVector(std::vector<T> *array) : mArray(*array) {}
    ~ScopedAutoClearVector() { mArray.clear(); }

  private:
    std::vector<T> &mArray;
};

inline void memcpy_guarded(void *dst, const void *src, const void *maxSrcPtr, size_t size)
{
    size_t bytesAvailable = maxSrcPtr > src ? (const uint8_t *)maxSrcPtr - (const uint8_t *)src : 0;
    size_t bytesToCopy    = std::min(size, bytesAvailable);
    size_t bytesToZero    = size - bytesToCopy;

    if (bytesToCopy)
        memcpy(dst, src, bytesToCopy);
    if (bytesToZero)
        memset((uint8_t *)dst + bytesToCopy, 0, bytesToZero);
}

// Copy matrix one column at a time
inline void copy_matrix(void *dst,
                        const void *src,
                        const void *maxSrcPtr,
                        size_t srcStride,
                        size_t dstStride,
                        GLenum type)
{
    size_t elemSize      = mtl::GetMetalSizeForGLType(gl::VariableComponentType(type));
    const size_t dstRows = gl::VariableRowCount(type);
    const size_t dstCols = gl::VariableColumnCount(type);

    for (size_t col = 0; col < dstCols; col++)
    {
        size_t srcOffset = col * srcStride;
        memcpy_guarded(((uint8_t *)dst) + dstStride * col, (const uint8_t *)src + srcOffset,
                       maxSrcPtr, elemSize * dstRows);
    }
}

// Copy matrix one element at a time to transpose.
inline void copy_matrix_row_major(void *dst,
                                  const void *src,
                                  const void *maxSrcPtr,
                                  size_t srcStride,
                                  size_t dstStride,
                                  GLenum type)
{
    size_t elemSize      = mtl::GetMetalSizeForGLType(gl::VariableComponentType(type));
    const size_t dstRows = gl::VariableRowCount(type);
    const size_t dstCols = gl::VariableColumnCount(type);

    for (size_t col = 0; col < dstCols; col++)
    {
        for (size_t row = 0; row < dstRows; row++)
        {
            size_t srcOffset = row * srcStride + col * elemSize;
            memcpy_guarded((uint8_t *)dst + dstStride * col + row * elemSize,
                           (const uint8_t *)src + srcOffset, maxSrcPtr, elemSize);
        }
    }
}

// TODO(angleproject:7979) Upgrade ANGLE Uniform buffer remapper to compute shaders
angle::Result ConvertUniformBufferData(ContextMtl *contextMtl,
                                       const UBOConversionInfo &blockConversionInfo,
                                       mtl::BufferPool *dynamicBuffer,
                                       const uint8_t *sourceData,
                                       size_t sizeToCopy,
                                       mtl::BufferRef *bufferOut,
                                       size_t *bufferOffsetOut)
{
    uint8_t *dst             = nullptr;
    const uint8_t *maxSrcPtr = sourceData + sizeToCopy;
    dynamicBuffer->releaseInFlightBuffers(contextMtl);

    // When converting a UBO buffer, we convert all of the data
    // supplied in a buffer at once (sizeToCopy = bufferMtl->size() - initial offset).
    // It's possible that a buffer could represent multiple instances of
    // a uniform block, so we loop over the number of block conversions we intend
    // to do.
    size_t numBlocksToCopy =
        (sizeToCopy + blockConversionInfo.stdSize() - 1) / blockConversionInfo.stdSize();
    size_t bytesToAllocate = numBlocksToCopy * blockConversionInfo.metalSize();
    ANGLE_TRY(dynamicBuffer->allocate(contextMtl, bytesToAllocate, &dst, bufferOut, bufferOffsetOut,
                                      nullptr));

    const std::vector<sh::BlockMemberInfo> &stdConversions = blockConversionInfo.stdInfo();
    const std::vector<sh::BlockMemberInfo> &mtlConversions = blockConversionInfo.metalInfo();
    for (size_t i = 0; i < numBlocksToCopy; ++i)
    {
        auto stdIterator = stdConversions.begin();
        auto mtlIterator = mtlConversions.begin();

        while (stdIterator != stdConversions.end())
        {
            for (int arraySize = 0; arraySize < stdIterator->arraySize; ++arraySize)
            {
                // For every entry in an array, calculate the offset based off of the
                // array element size.

                // Offset of a single entry is
                // blockIndex*blockSize + arrayOffset*arraySize + offset of field in base struct.
                // Fields are copied per block, per member, per array entry of member.

                size_t stdArrayOffset = stdIterator->arrayStride * arraySize;
                size_t mtlArrayOffset = mtlIterator->arrayStride * arraySize;

                if (gl::IsMatrixType(mtlIterator->type))
                {

                    void *dstMat = dst + mtlIterator->offset + mtlArrayOffset +
                                   blockConversionInfo.metalSize() * i;
                    const void *srcMat = sourceData + stdIterator->offset + stdArrayOffset +
                                         blockConversionInfo.stdSize() * i;
                    // Transpose matricies into column major order, if they're row major encoded.
                    if (stdIterator->isRowMajorMatrix)
                    {
                        copy_matrix_row_major(dstMat, srcMat, maxSrcPtr, stdIterator->matrixStride,
                                              mtlIterator->matrixStride, mtlIterator->type);
                    }
                    else
                    {
                        copy_matrix(dstMat, srcMat, maxSrcPtr, stdIterator->matrixStride,
                                    mtlIterator->matrixStride, mtlIterator->type);
                    }
                }
                // Compress bool from four bytes to one byte because bool values in GLSL
                // are uint-sized: ES 3.0 Section 2.12.6.3 "Uniform Buffer Object Storage".
                // Bools in metal are byte-sized. (Metal shading language spec Table 2.2)
                else if (gl::VariableComponentType(mtlIterator->type) == GL_BOOL)
                {
                    for (int boolCol = 0; boolCol < gl::VariableComponentCount(mtlIterator->type);
                         boolCol++)
                    {
                        const uint8_t *srcBool =
                            (sourceData + stdIterator->offset + stdArrayOffset +
                             blockConversionInfo.stdSize() * i +
                             gl::VariableComponentSize(GL_BOOL) * boolCol);
                        unsigned int srcValue =
                            srcBool < maxSrcPtr ? *((unsigned int *)(srcBool)) : 0;
                        uint8_t *dstBool = dst + mtlIterator->offset + mtlArrayOffset +
                                           blockConversionInfo.metalSize() * i +
                                           sizeof(bool) * boolCol;
                        *dstBool = (srcValue != 0);
                    }
                }
                else
                {
                    memcpy_guarded(dst + mtlIterator->offset + mtlArrayOffset +
                                       blockConversionInfo.metalSize() * i,
                                   sourceData + stdIterator->offset + stdArrayOffset +
                                       blockConversionInfo.stdSize() * i,
                                   maxSrcPtr, mtl::GetMetalSizeForGLType(mtlIterator->type));
                }
            }
            ++stdIterator;
            ++mtlIterator;
        }
    }

    ANGLE_TRY(dynamicBuffer->commit(contextMtl));
    return angle::Result::Continue;
}

inline std::map<std::string, std::string> GetDefaultSubstitutionDictionary()
{
    return {};
}

template <typename T>
void UpdateDefaultUniformBlockWithElementSize(GLsizei count,
                                              uint32_t arrayIndex,
                                              int componentCount,
                                              const T *v,
                                              size_t baseElementSize,
                                              const sh::BlockMemberInfo &layoutInfo,
                                              angle::MemoryBuffer *uniformData)
{
    const int elementSize = (int)(baseElementSize * componentCount);

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
                                 size_t elementSize,
                                 const sh::BlockMemberInfo &layoutInfo,
                                 const angle::MemoryBuffer *uniformData)
{
    ReadFromDefaultUniformBlockWithElementSize(componentCount, arrayIndex, dst, sizeof(T),
                                               layoutInfo, uniformData);
}

void ReadFromDefaultUniformBlockWithElementSize(int componentCount,
                                                uint32_t arrayIndex,
                                                void *dst,
                                                size_t baseElementSize,
                                                const sh::BlockMemberInfo &layoutInfo,
                                                const angle::MemoryBuffer *uniformData)
{
    ASSERT(layoutInfo.offset != -1);

    const size_t elementSize = (baseElementSize * componentCount);
    const uint8_t *source    = uniformData->data() + layoutInfo.offset;

    if (layoutInfo.arrayStride == 0 || (size_t)layoutInfo.arrayStride == elementSize)
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
    encoder->metalArgBufferEncoder =
        mtl::adoptObjCObj([function newArgumentEncoderWithBufferIndex:bufferIndex]);
    if (encoder->metalArgBufferEncoder)
    {
        encoder->bufferPool.initialize(context, encoder->metalArgBufferEncoder.get().encodedLength,
                                       mtl::kArgumentBufferOffsetAlignment, 0);
    }
}

constexpr size_t PipelineParametersToFragmentShaderVariantIndex(bool multisampledRendering,
                                                                bool allowFragDepthWrite)
{
    const size_t index = (allowFragDepthWrite << 1) | multisampledRendering;
    ASSERT(index < kFragmentShaderVariants);
    return index;
}

bool DisableFastMathForShaderCompilation(mtl::Context *context)
{
    return context->getDisplay()->getFeatures().intelDisableFastMath.enabled;
}

bool UsesInvariance(const mtl::TranslatedShaderInfo *translatedMslInfo)
{
    return translatedMslInfo->hasInvariant;
}

angle::Result CreateMslShaderLib(ContextMtl *context,
                                 gl::InfoLog &infoLog,
                                 mtl::TranslatedShaderInfo *translatedMslInfo,
                                 const std::map<std::string, std::string> &substitutionMacros)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        mtl::LibraryCache &libraryCache = context->getDisplay()->getLibraryCache();

        // Convert to actual binary shader
        mtl::AutoObjCPtr<NSError *> err = nil;
        bool disableFastMath            = DisableFastMathForShaderCompilation(context);
        bool usesInvariance             = UsesInvariance(translatedMslInfo);
        translatedMslInfo->metalLibrary = libraryCache.getOrCompileShaderLibrary(
            context, translatedMslInfo->metalShaderSource, substitutionMacros, disableFastMath,
            usesInvariance, &err);
        if (err && !translatedMslInfo->metalLibrary)
        {
            std::ostringstream ss;
            ss << "Internal error compiling shader with Metal backend.\n";
            ss << err.get().localizedDescription.UTF8String << "\n";
            ss << "-----\n";
            ss << *(translatedMslInfo->metalShaderSource);
            ss << "-----\n";

            infoLog << ss.str();

            ANGLE_MTL_HANDLE_ERROR(context, ss.str().c_str(), GL_INVALID_OPERATION);
            return angle::Result::Stop;
        }

        return angle::Result::Continue;
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
ProgramMtl::ProgramMtl(const gl::ProgramState &state)
    : ProgramImpl(state), mShadowCompareModes(), mAuxBufferPool(nullptr)
{}

ProgramMtl::~ProgramMtl() = default;

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
    getExecutable()->reset(context);

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
}

std::unique_ptr<rx::LinkEvent> ProgramMtl::load(const gl::Context *context,
                                                gl::BinaryInputStream *stream)
{

    ContextMtl *contextMtl = mtl::GetImpl(context);
    // NOTE(hqle): No transform feedbacks for now, since we only support ES 2.0 atm

    reset(contextMtl);

    ANGLE_PARALLEL_LINK_TRY(getExecutable()->load(contextMtl, stream));

    return compileMslShaderLibs(context);
}

void ProgramMtl::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    getExecutable()->save(stream);
}

void ProgramMtl::setBinaryRetrievableHint(bool retrievable) {}

void ProgramMtl::setSeparable(bool separable)
{
    UNIMPLEMENTED();
}

void ProgramMtl::prepareForLink(const gl::ShaderMap<ShaderImpl *> &shaders)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mAttachedShaders[shaderType].reset();

        if (shaders[shaderType] != nullptr)
        {
            const ShaderMtl *shaderMtl   = GetAs<ShaderMtl>(shaders[shaderType]);
            mAttachedShaders[shaderType] = shaderMtl->getCompiledState();
        }
    }
}

std::unique_ptr<LinkEvent> ProgramMtl::link(const gl::Context *context,
                                            const gl::ProgramLinkedResources &resources,
                                            gl::ProgramMergedVaryings &&mergedVaryings)
{
    ContextMtl *contextMtl              = mtl::GetImpl(context);
    ProgramExecutableMtl *executableMtl = getExecutable();

    // Link resources before calling GetShaderSource to make sure they are ready for the set/binding
    // assignment done in that function.
    linkResources(resources);

    reset(contextMtl);
    ANGLE_PARALLEL_LINK_TRY(
        executableMtl->initDefaultUniformBlocks(contextMtl, mState.getAttachedShaders()));
    executableMtl->linkUpdateHasFlatAttributes(mState.getAttachedShader(gl::ShaderType::Vertex));

    gl::ShaderMap<std::string> shaderSources;
    mtl::MSLGetShaderSource(mState, resources, &shaderSources);

    ANGLE_PARALLEL_LINK_TRY(
        mtl::MTLGetMSL(contextMtl, mState, contextMtl->getCaps(), shaderSources, mAttachedShaders,
                       &executableMtl->mMslShaderTranslateInfo,
                       mState.getExecutable().getTransformFeedbackBufferCount()));
    executableMtl->mMslXfbOnlyVertexShaderInfo =
        executableMtl->mMslShaderTranslateInfo[gl::ShaderType::Vertex];

    return compileMslShaderLibs(context);
}

class ProgramMtl::CompileMslTask final : public angle::Closure
{
  public:
    CompileMslTask(ContextMtl *context,
                   mtl::TranslatedShaderInfo *translatedMslInfo,
                   const std::map<std::string, std::string> &substitutionMacros)
        : mContext(context),
          mTranslatedMslInfo(translatedMslInfo),
          mSubstitutionMacros(substitutionMacros)
    {}

    void operator()() override
    {
        mResult = CreateMslShaderLib(mContext, mInfoLog, mTranslatedMslInfo, mSubstitutionMacros);
    }

    angle::Result getResult(gl::InfoLog &infoLog)
    {
        if (!mInfoLog.empty())
        {
            infoLog << mInfoLog.str();
        }

        return mResult;
    }

  private:
    ContextMtl *mContext;
    gl::InfoLog mInfoLog;
    mtl::TranslatedShaderInfo *mTranslatedMslInfo;
    std::map<std::string, std::string> mSubstitutionMacros;
    angle::Result mResult = angle::Result::Continue;
};

// The LinkEvent implementation for linking a Metal program
class ProgramMtl::ProgramLinkEvent final : public LinkEvent
{
  public:
    ProgramLinkEvent(gl::InfoLog &infoLog,
                     std::shared_ptr<angle::WorkerThreadPool> workerPool,
                     std::vector<std::shared_ptr<ProgramMtl::CompileMslTask>> &&compileTasks)
        : mInfoLog(infoLog), mTasks(std::move(compileTasks))
    {
        mWaitableEvents.reserve(mTasks.size());
        for (const auto &task : mTasks)
        {
            mWaitableEvents.push_back(workerPool->postWorkerTask(task));
        }
    }

    bool isLinking() override { return !angle::WaitableEvent::AllReady(&mWaitableEvents); }

    angle::Result wait(const gl::Context *context) override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramMtl::ProgramLinkEvent::wait");
        angle::WaitableEvent::WaitMany(&mWaitableEvents);

        for (const auto &task : mTasks)
        {
            ANGLE_TRY(task->getResult(mInfoLog));
        }

        return angle::Result::Continue;
    }

  private:
    gl::InfoLog &mInfoLog;
    std::vector<std::shared_ptr<ProgramMtl::CompileMslTask>> mTasks;
    std::vector<std::shared_ptr<angle::WaitableEvent>> mWaitableEvents;
};

std::unique_ptr<LinkEvent> ProgramMtl::compileMslShaderLibs(const gl::Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramMtl::compileMslShaderLibs");
    gl::InfoLog &infoLog = mState.getExecutable().getInfoLog();

    ContextMtl *contextMtl              = mtl::GetImpl(context);
    ProgramExecutableMtl *executableMtl = getExecutable();
    bool asyncCompile =
        contextMtl->getDisplay()->getFeatures().enableParallelMtlLibraryCompilation.enabled;
    mtl::LibraryCache &libraryCache = contextMtl->getDisplay()->getLibraryCache();

    std::vector<std::shared_ptr<ProgramMtl::CompileMslTask>> asyncTasks;
    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        mtl::TranslatedShaderInfo *translateInfo =
            &executableMtl->mMslShaderTranslateInfo[shaderType];
        std::map<std::string, std::string> macros = GetDefaultSubstitutionDictionary();
        bool disableFastMath                      = DisableFastMathForShaderCompilation(contextMtl);
        bool usesInvariance                       = UsesInvariance(translateInfo);

        // Check if the shader is already in the cache and use it instead of spawning a new thread
        translateInfo->metalLibrary = libraryCache.get(translateInfo->metalShaderSource, macros,
                                                       disableFastMath, usesInvariance);

        if (!translateInfo->metalLibrary)
        {
            if (asyncCompile)
            {
                auto task =
                    std::make_shared<ProgramMtl::CompileMslTask>(contextMtl, translateInfo, macros);
                asyncTasks.push_back(task);
            }
            else
            {
                ANGLE_PARALLEL_LINK_TRY(
                    CreateMslShaderLib(contextMtl, infoLog, translateInfo, macros));
            }
        }
    }

    if (asyncTasks.empty())
    {
        // All shaders were in the cache, no async work to do
        return std::make_unique<LinkEventDone>(angle::Result::Continue);
    }

    return std::make_unique<ProgramMtl::ProgramLinkEvent>(
        infoLog, context->getShaderCompileThreadPool(), std::move(asyncTasks));
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

angle::Result ProgramMtl::getSpecializedShader(ContextMtl *context,
                                               gl::ShaderType shaderType,
                                               const mtl::RenderPipelineDesc &renderPipelineDesc,
                                               id<MTLFunction> *shaderOut)
{
    ProgramExecutableMtl *executableMtl = getExecutable();

    static_assert(YES == 1, "YES should have value of 1");

    mtl::TranslatedShaderInfo *translatedMslInfo =
        &executableMtl->mMslShaderTranslateInfo[shaderType];
    ProgramShaderObjVariantMtl *shaderVariant;
    mtl::AutoObjCObj<MTLFunctionConstantValues> funcConstants;

    if (shaderType == gl::ShaderType::Vertex)
    {
        // For vertex shader, we need to create 3 variants, one with emulated rasterization
        // discard, one with true rasterization discard and one without.
        shaderVariant = &executableMtl->mVertexShaderVariants[renderPipelineDesc.rasterizationType];
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
            translatedMslInfo = &executableMtl->mMslXfbOnlyVertexShaderInfo;
            if (!translatedMslInfo->metalLibrary)
            {
                // Lazily compile XFB only shader
                gl::InfoLog infoLog;
                ANGLE_TRY(CreateMslShaderLib(context, infoLog,
                                             &executableMtl->mMslXfbOnlyVertexShaderInfo,
                                             {{"TRANSFORM_FEEDBACK_ENABLED", "1"}}));
                translatedMslInfo->metalLibrary.get().label = @"TransformFeedback";
            }
        }

        ANGLE_MTL_OBJC_SCOPE
        {
            BOOL emulateDiscard = renderPipelineDesc.rasterizationType ==
                                  mtl::RenderPipelineRasterization::EmulatedDiscard;

            NSString *discardEnabledStr =
                [NSString stringWithUTF8String:sh::mtl::kRasterizerDiscardEnabledConstName];

            funcConstants = mtl::adoptObjCObj([[MTLFunctionConstantValues alloc] init]);
            [funcConstants setConstantValue:&emulateDiscard
                                       type:MTLDataTypeBool
                                   withName:discardEnabledStr];
        }
    }  // if (shaderType == gl::ShaderType::Vertex)
    else if (shaderType == gl::ShaderType::Fragment)
    {
        // For fragment shader, we need to create 4 variants,
        // combining multisampled rendering and depth write enabled states.
        const bool multisampledRendering = renderPipelineDesc.outputDescriptor.sampleCount > 1;
        const bool allowFragDepthWrite =
            renderPipelineDesc.outputDescriptor.depthAttachmentPixelFormat != 0;
        shaderVariant =
            &executableMtl->mFragmentShaderVariants[PipelineParametersToFragmentShaderVariantIndex(
                multisampledRendering, allowFragDepthWrite)];
        if (shaderVariant->metalShader)
        {
            // Already created.
            *shaderOut = shaderVariant->metalShader;
            return angle::Result::Continue;
        }

        ANGLE_MTL_OBJC_SCOPE
        {
            NSString *multisampledRenderingStr =
                [NSString stringWithUTF8String:sh::mtl::kMultisampledRenderingConstName];

            NSString *depthWriteEnabledStr =
                [NSString stringWithUTF8String:sh::mtl::kDepthWriteEnabledConstName];

            funcConstants = mtl::adoptObjCObj([[MTLFunctionConstantValues alloc] init]);
            [funcConstants setConstantValue:&multisampledRendering
                                       type:MTLDataTypeBool
                                   withName:multisampledRenderingStr];
            [funcConstants setConstantValue:&allowFragDepthWrite
                                       type:MTLDataTypeBool
                                   withName:depthWriteEnabledStr];
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
    [funcConstants
        setConstantValue:&(context->getDisplay()->getFeatures().emulateAlphaToCoverage.enabled)
                    type:MTLDataTypeBool
                withName:@"ANGLEEmulateAlphaToCoverage"];
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

GLboolean ProgramMtl::validate(const gl::Caps &caps)
{
    // No-op. The spec is very vague about the behavior of validation.
    return GL_TRUE;
}

template <typename T>
void ProgramMtl::setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType)
{
    ProgramExecutableMtl *executableMtl = getExecutable();

    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    if (linkedUniform.isSampler())
    {
        // Sampler binding has changed.
        executableMtl->mSamplerBindingsDirty.set();
        return;
    }

    if (linkedUniform.type == entryPointType)
    {
        for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
        {
            DefaultUniformBlockMtl &uniformBlock = executableMtl->mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = (GLint)linkedUniform.getElementComponents();
            const GLint baseComponentSize =
                (GLint)mtl::GetMetalSizeForGLType(gl::VariableComponentType(linkedUniform.type));
            UpdateDefaultUniformBlockWithElementSize(count, locationInfo.arrayIndex, componentCount,
                                                     v, baseComponentSize, layoutInfo,
                                                     &uniformBlock.uniformData);
            executableMtl->mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
    else
    {
        for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
        {
            DefaultUniformBlockMtl &uniformBlock = executableMtl->mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.getElementComponents();

            ASSERT(linkedUniform.type == gl::VariableBoolVectorType(entryPointType));

            GLint initialArrayOffset =
                locationInfo.arrayIndex * layoutInfo.arrayStride + layoutInfo.offset;
            for (GLint i = 0; i < count; i++)
            {
                GLint elementOffset = i * layoutInfo.arrayStride + initialArrayOffset;
                bool *dest =
                    reinterpret_cast<bool *>(uniformBlock.uniformData.data() + elementOffset);
                const T *source = v + i * componentCount;

                for (int c = 0; c < componentCount; c++)
                {
                    dest[c] = (source[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE;
                }
            }

            executableMtl->mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
}

template <typename T>
void ProgramMtl::getUniformImpl(GLint location, T *v, GLenum entryPointType) const
{
    const ProgramExecutableMtl *executableMtl = getExecutable();

    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    ASSERT(!linkedUniform.isSampler());

    const gl::ShaderType shaderType = linkedUniform.getFirstActiveShaderType();
    ASSERT(shaderType != gl::ShaderType::InvalidEnum);

    const DefaultUniformBlockMtl &uniformBlock = executableMtl->mDefaultUniformBlocks[shaderType];
    const sh::BlockMemberInfo &layoutInfo      = uniformBlock.uniformLayout[location];

    ASSERT(gl::GetUniformTypeInfo(linkedUniform.type).componentType == entryPointType ||
           gl::GetUniformTypeInfo(linkedUniform.type).componentType ==
               gl::VariableBoolVectorType(entryPointType));
    const GLint baseComponentSize =
        (GLint)mtl::GetMetalSizeForGLType(gl::VariableComponentType(linkedUniform.type));

    if (gl::IsMatrixType(linkedUniform.getType()))
    {
        const uint8_t *ptrToElement = uniformBlock.uniformData.data() + layoutInfo.offset +
                                      (locationInfo.arrayIndex * layoutInfo.arrayStride);
        mtl::GetMatrixUniformMetal(linkedUniform.getType(), v,
                                   reinterpret_cast<const T *>(ptrToElement), false);
    }
    // Decompress bool from one byte to four bytes because bool values in GLSL
    // are uint-sized: ES 3.0 Section 2.12.6.3 "Uniform Buffer Object Storage".
    else if (gl::VariableComponentType(linkedUniform.getType()) == GL_BOOL)
    {
        bool bVals[4] = {0};
        ReadFromDefaultUniformBlockWithElementSize(
            linkedUniform.getElementComponents(), locationInfo.arrayIndex, bVals, baseComponentSize,
            layoutInfo, &uniformBlock.uniformData);
        for (int bCol = 0; bCol < linkedUniform.getElementComponents(); ++bCol)
        {
            unsigned int data = bVals[bCol];
            *(v + bCol)       = static_cast<T>(data);
        }
    }
    else
    {

        assert(baseComponentSize == sizeof(T));
        ReadFromDefaultUniformBlockWithElementSize(linkedUniform.getElementComponents(),
                                                   locationInfo.arrayIndex, v, baseComponentSize,
                                                   layoutInfo, &uniformBlock.uniformData);
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
    ProgramExecutableMtl *executableMtl = getExecutable();

    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        DefaultUniformBlockMtl &uniformBlock  = executableMtl->mDefaultUniformBlocks[shaderType];
        const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

        // Assume an offset of -1 means the block is unused.
        if (layoutInfo.offset == -1)
        {
            continue;
        }

        mtl::SetFloatUniformMatrixMetal<cols, rows>::Run(
            locationInfo.arrayIndex, linkedUniform.getBasicTypeElementCount(), count, transpose,
            value, uniformBlock.uniformData.data() + layoutInfo.offset);

        executableMtl->mDefaultUniformBlocksDirty.set(shaderType);
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
    ContextMtl *context                 = mtl::GetImpl(glContext);
    ProgramExecutableMtl *executableMtl = getExecutable();

    if (pipelineDescChanged || !cmdEncoder->hasPipelineState())
    {
        id<MTLFunction> vertexShader = nil;
        ANGLE_TRY(
            getSpecializedShader(context, gl::ShaderType::Vertex, pipelineDesc, &vertexShader));

        id<MTLFunction> fragmentShader = nil;
        ANGLE_TRY(
            getSpecializedShader(context, gl::ShaderType::Fragment, pipelineDesc, &fragmentShader));

        mtl::AutoObjCPtr<id<MTLRenderPipelineState>> pipelineState;
        ANGLE_TRY(context->getPipelineCache().getRenderPipeline(
            context, vertexShader, fragmentShader, pipelineDesc, &pipelineState));

        cmdEncoder->setRenderPipelineState(pipelineState);

        // We need to rebind uniform buffers & textures also
        executableMtl->mDefaultUniformBlocksDirty.set();
        executableMtl->mSamplerBindingsDirty.set();

        // Cache current shader variant references for easier querying.
        executableMtl->mCurrentShaderVariants[gl::ShaderType::Vertex] =
            &executableMtl->mVertexShaderVariants[pipelineDesc.rasterizationType];

        const bool multisampledRendering = pipelineDesc.outputDescriptor.sampleCount > 1;
        const bool allowFragDepthWrite =
            pipelineDesc.outputDescriptor.depthAttachmentPixelFormat != 0;
        executableMtl->mCurrentShaderVariants[gl::ShaderType::Fragment] =
            pipelineDesc.rasterizationEnabled()
                ? &executableMtl
                       ->mFragmentShaderVariants[PipelineParametersToFragmentShaderVariantIndex(
                           multisampledRendering, allowFragDepthWrite)]
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
    ProgramExecutableMtl *executableMtl = getExecutable();

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if (!executableMtl->mDefaultUniformBlocksDirty[shaderType] ||
            !executableMtl->mCurrentShaderVariants[shaderType])
        {
            continue;
        }
        DefaultUniformBlockMtl &uniformBlock = executableMtl->mDefaultUniformBlocks[shaderType];

        if (!uniformBlock.uniformData.size())
        {
            continue;
        }
        if (mAuxBufferPool)
        {
            mAuxBufferPool->releaseInFlightBuffers(context);
        }
        // If we exceed the default inline max size, try to allocate a buffer
        bool needsCommitUniform = true;
        if (needsCommitUniform && uniformBlock.uniformData.size() <= mtl::kInlineConstDataMaxSize)
        {
            ASSERT(uniformBlock.uniformData.size() <= mtl::kInlineConstDataMaxSize);
            cmdEncoder->setBytes(shaderType, uniformBlock.uniformData.data(),
                                 uniformBlock.uniformData.size(),
                                 mtl::kDefaultUniformsBindingIndex);
        }
        else if (needsCommitUniform)
        {
            ASSERT(uniformBlock.uniformData.size() <= mtl::kDefaultUniformsMaxSize);
            mtl::BufferRef mtlBufferOut;
            size_t offsetOut;
            uint8_t *ptrOut;
            // Allocate a new Uniform buffer
            ANGLE_TRY(getBufferPool(context)->allocate(context, uniformBlock.uniformData.size(),
                                                       &ptrOut, &mtlBufferOut, &offsetOut));
            // Copy the uniform result
            memcpy(ptrOut, uniformBlock.uniformData.data(), uniformBlock.uniformData.size());
            // Commit
            ANGLE_TRY(getBufferPool(context)->commit(context));
            // Set buffer
            cmdEncoder->setBuffer(shaderType, mtlBufferOut, (uint32_t)offsetOut,
                                  mtl::kDefaultUniformsBindingIndex);
        }

        executableMtl->mDefaultUniformBlocksDirty.reset(shaderType);
    }
    return angle::Result::Continue;
}

angle::Result ProgramMtl::updateTextures(const gl::Context *glContext,
                                         mtl::RenderCommandEncoder *cmdEncoder,
                                         bool forceUpdate)
{
    ContextMtl *contextMtl                          = mtl::GetImpl(glContext);
    const auto &glState                             = glContext->getState();
    ProgramExecutableMtl *executableMtl             = getExecutable();
    const gl::ActiveTexturesCache &completeTextures = glState.getActiveTexturesCache();

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if ((!executableMtl->mSamplerBindingsDirty[shaderType] && !forceUpdate) ||
            !executableMtl->mCurrentShaderVariants[shaderType])
        {
            continue;
        }

        const mtl::TranslatedShaderInfo &shaderInfo =
            executableMtl->mCurrentShaderVariants[shaderType]->translatedSrcInfo
                ? *executableMtl->mCurrentShaderVariants[shaderType]->translatedSrcInfo
                : executableMtl->mMslShaderTranslateInfo[shaderType];
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

            for (uint32_t arrayElement = 0; arrayElement < samplerBinding.textureUnitsCount;
                 ++arrayElement)
            {
                GLuint textureUnit = samplerBinding.getTextureUnit(
                    mState.getSamplerBoundTextureUnits(), arrayElement);
                gl::Texture *texture = completeTextures[textureUnit];
                gl::Sampler *sampler = contextMtl->getState().getSampler(textureUnit);
                uint32_t textureSlot = mslBinding.textureBinding + arrayElement;
                uint32_t samplerSlot = mslBinding.samplerBinding + arrayElement;
                if (!texture)
                {
                    ANGLE_TRY(contextMtl->getIncompleteTexture(glContext, textureType,
                                                               samplerBinding.format, &texture));
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

        for (const gl::ImageBinding &imageBinding : mState.getImageBindings())
        {
            if (imageBinding.boundImageUnits.size() != 1)
            {
                UNIMPLEMENTED();
                continue;
            }

            int glslImageBinding    = imageBinding.boundImageUnits[0];
            int mtlRWTextureBinding = shaderInfo.actualImageBindings[glslImageBinding];
            ASSERT(mtlRWTextureBinding < static_cast<int>(mtl::kMaxShaderImages));
            if (mtlRWTextureBinding < 0)
            {
                continue;  // The program does not have an image bound at this unit.
            }

            const gl::ImageUnit &imageUnit = glState.getImageUnit(glslImageBinding);
            TextureMtl *textureMtl         = mtl::GetImpl(imageUnit.texture.get());
            ANGLE_TRY(textureMtl->bindToShaderImage(
                glContext, cmdEncoder, shaderType, static_cast<uint32_t>(mtlRWTextureBinding),
                imageUnit.level, imageUnit.layer, imageUnit.format));
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

    ProgramExecutableMtl *executableMtl = getExecutable();

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
        if (!executableMtl->mCurrentShaderVariants[shaderType])
        {
            continue;
        }

        if (executableMtl->mCurrentShaderVariants[shaderType]
                ->translatedSrcInfo->hasUBOArgumentBuffer)
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
    ProgramExecutableMtl *executableMtl = getExecutable();
    const gl::State &glState            = context->getState();

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
        ASSERT(executableMtl->mUniformBlockConversions.find(block.name) !=
               executableMtl->mUniformBlockConversions.end());
        const UBOConversionInfo &conversionInfo =
            executableMtl->mUniformBlockConversions.at(block.name);
        if (conversionInfo.needsConversion())
        {

            UniformConversionBufferMtl *conversion =
                (UniformConversionBufferMtl *)bufferMtl->getUniformConversionBuffer(
                    context, std::pair<size_t, size_t>(bufferIndex, srcOffset),
                    conversionInfo.stdSize());
            // Has the content of the buffer has changed since last conversion?
            if (conversion->dirty)
            {
                const uint8_t *srcBytes = bufferMtl->getBufferDataReadOnly(context);
                srcBytes += srcOffset;
                size_t sizeToCopy = bufferMtl->size() - srcOffset;

                ANGLE_TRY(ConvertUniformBufferData(
                    context, conversionInfo, &conversion->data, srcBytes, sizeToCopy,
                    &conversion->convertedBuffer, &conversion->convertedOffset));

                conversion->dirty = false;
            }
            // Calculate offset in new block.
            size_t dstOffsetSource = srcOffset - conversion->initialSrcOffset();
            assert(dstOffsetSource % conversionInfo.stdSize() == 0);
            unsigned int numBlocksToOffset =
                (unsigned int)(dstOffsetSource / conversionInfo.stdSize());
            size_t bytesToOffset = numBlocksToOffset * conversionInfo.metalSize();

            mLegalizedOffsetedUniformBuffers[bufferIndex].first = conversion->convertedBuffer;
            mLegalizedOffsetedUniformBuffers[bufferIndex].second =
                static_cast<uint32_t>(conversion->convertedOffset + bytesToOffset);
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
    const gl::State &glState            = context->getState();
    ProgramExecutableMtl *executableMtl = getExecutable();
    const mtl::TranslatedShaderInfo &shaderInfo =
        *executableMtl->mCurrentShaderVariants[shaderType]->translatedSrcInfo;

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
    const gl::State &glState            = context->getState();
    ProgramExecutableMtl *executableMtl = getExecutable();

    ASSERT(executableMtl->mCurrentShaderVariants[shaderType]->translatedSrcInfo);
    const mtl::TranslatedShaderInfo &shaderInfo =
        *executableMtl->mCurrentShaderVariants[shaderType]->translatedSrcInfo;

    // Encode all uniform buffers into an argument buffer.
    ProgramArgumentBufferEncoderMtl &bufferEncoder =
        executableMtl->mCurrentShaderVariants[shaderType]->uboArgBufferEncoder;

    mtl::BufferRef argumentBuffer;
    size_t argumentBufferOffset;
    bufferEncoder.bufferPool.releaseInFlightBuffers(context);
    ANGLE_TRY(bufferEncoder.bufferPool.allocate(
        context, bufferEncoder.metalArgBufferEncoder.get().encodedLength, nullptr, &argumentBuffer,
        &argumentBufferOffset));

    // MTLArgumentEncoder is modifying the buffer indirectly on CPU. We need to call map()
    // so that the buffer's data changes could be flushed to the GPU side later.
    ANGLE_UNUSED_VARIABLE(argumentBuffer->mapWithOpt(context, /*readonly=*/false, /*noSync=*/true));

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

    // Flush changes made by MTLArgumentEncoder to GPU.
    argumentBuffer->unmapAndFlushSubset(context, argumentBufferOffset,
                                        bufferEncoder.metalArgBufferEncoder.get().encodedLength);

    cmdEncoder->setBuffer(shaderType, argumentBuffer, static_cast<uint32_t>(argumentBufferOffset),
                          mtl::kUBOArgumentBufferBindingIndex);
    return angle::Result::Continue;
}

angle::Result ProgramMtl::updateXfbBuffers(ContextMtl *context,
                                           mtl::RenderCommandEncoder *cmdEncoder,
                                           const mtl::RenderPipelineDesc &pipelineDesc)
{
    const gl::State &glState                 = context->getState();
    ProgramExecutableMtl *executableMtl      = getExecutable();
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
        uint32_t actualBufferIdx =
            executableMtl->mMslXfbOnlyVertexShaderInfo.actualXFBBindings[bufferIndex];

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
