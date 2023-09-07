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
#include "libANGLE/renderer/metal/CompilerMtl.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
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
inline std::map<std::string, std::string> GetDefaultSubstitutionDictionary()
{
    return {};
}

bool DisableFastMathForShaderCompilation(mtl::Context *context)
{
    return context->getDisplay()->getFeatures().intelDisableFastMath.enabled;
}

bool UsesInvariance(const mtl::TranslatedShaderInfo *translatedMslInfo)
{
    return translatedMslInfo->hasInvariant;
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
ProgramMtl::ProgramMtl(const gl::ProgramState &state) : ProgramImpl(state) {}

ProgramMtl::~ProgramMtl() = default;

void ProgramMtl::destroy(const gl::Context *context)
{
    reset(mtl::GetImpl(context));
}

void ProgramMtl::reset(ContextMtl *context)
{
    getExecutable()->reset(context);
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

    ANGLE_PARALLEL_LINK_TRY(mtl::MTLGetMSL(contextMtl, mState.getExecutable(),
                                           contextMtl->getCaps(), shaderSources, mAttachedShaders,
                                           &executableMtl->mMslShaderTranslateInfo));
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

void ProgramMtl::linkResources(const gl::ProgramLinkedResources &resources)
{
    Std140BlockLayoutEncoderFactory std140EncoderFactory;
    gl::ProgramLinkedResourcesLinker linker(&std140EncoderFactory);

    linker.linkResources(mState, resources);
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

}  // namespace rx
