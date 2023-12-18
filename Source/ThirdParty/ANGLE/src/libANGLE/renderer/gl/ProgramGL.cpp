//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramGL.cpp: Implements the class methods for ProgramGL.

#include "libANGLE/renderer/gl/ProgramGL.h"

#include "common/WorkerThread.h"
#include "common/angleutils.h"
#include "common/bitset_utils.h"
#include "common/debug.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/ShaderGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/trace.h"
#include "platform/PlatformMethods.h"
#include "platform/autogen/FeaturesGL_autogen.h"

namespace rx
{
namespace
{

// Returns mapped name of a transform feedback varying. The original name may contain array
// brackets with an index inside, which will get copied to the mapped name. The varying must be
// known to be declared in the shader.
std::string GetTransformFeedbackVaryingMappedName(const gl::SharedCompiledShaderState &shaderState,
                                                  const std::string &tfVaryingName)
{
    ASSERT(shaderState->shaderType != gl::ShaderType::Fragment &&
           shaderState->shaderType != gl::ShaderType::Compute);
    const auto &varyings = shaderState->outputVaryings;
    auto bracketPos      = tfVaryingName.find("[");
    if (bracketPos != std::string::npos)
    {
        auto tfVaryingBaseName = tfVaryingName.substr(0, bracketPos);
        for (const auto &varying : varyings)
        {
            if (varying.name == tfVaryingBaseName)
            {
                std::string mappedNameWithArrayIndex =
                    varying.mappedName + tfVaryingName.substr(bracketPos);
                return mappedNameWithArrayIndex;
            }
        }
    }
    else
    {
        for (const auto &varying : varyings)
        {
            if (varying.name == tfVaryingName)
            {
                return varying.mappedName;
            }
            else if (varying.isStruct())
            {
                GLuint fieldIndex = 0;
                const auto *field = varying.findField(tfVaryingName, &fieldIndex);
                if (field == nullptr)
                {
                    continue;
                }
                ASSERT(field != nullptr && !field->isStruct() &&
                       (!field->isArray() || varying.isShaderIOBlock));
                std::string mappedName;
                // If it's an I/O block without an instance name, don't include the block name.
                if (!varying.isShaderIOBlock || !varying.name.empty())
                {
                    mappedName = varying.isShaderIOBlock ? varying.mappedStructOrBlockName
                                                         : varying.mappedName;
                    mappedName += '.';
                }
                return mappedName + field->mappedName;
            }
        }
    }
    UNREACHABLE();
    return std::string();
}

}  // anonymous namespace

class ProgramGL::LinkTaskGL final : public LinkTask
{
  public:
    LinkTaskGL(ProgramGL *program,
               bool hasNativeParallelCompile,
               const FunctionsGL *functions,
               const gl::Extensions &extensions,
               GLuint programID)
        : mProgram(program),
          mHasNativeParallelCompile(hasNativeParallelCompile),
          mFunctions(functions),
          mExtensions(extensions),
          mProgramID(programID)
    {}
    ~LinkTaskGL() override = default;

    std::vector<std::shared_ptr<LinkSubTask>> link(const gl::ProgramLinkedResources &resources,
                                                   const gl::ProgramMergedVaryings &mergedVaryings,
                                                   bool *areSubTasksOptionalOut) override
    {
        mProgram->linkJobImpl(mExtensions);

        // If there is no native parallel compile, do the post-link right away.
        if (!mHasNativeParallelCompile)
        {
            mResult = mProgram->postLinkJobImpl(resources);
        }

        // See comment on mResources
        mResources = &resources;
        return {};
    }

    angle::Result getResult(const gl::Context *context, gl::InfoLog &infoLog) override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "LinkTaskGL::getResult");

        if (mHasNativeParallelCompile)
        {
            mResult = mProgram->postLinkJobImpl(*mResources);
        }

        return mResult;
    }

    bool isLinkingInternally() override
    {
        GLint completionStatus = GL_TRUE;
        if (mHasNativeParallelCompile)
        {
            mFunctions->getProgramiv(mProgramID, GL_COMPLETION_STATUS, &completionStatus);
        }
        return completionStatus == GL_FALSE;
    }

  private:
    ProgramGL *mProgram;
    const bool mHasNativeParallelCompile;
    const FunctionsGL *mFunctions;
    const gl::Extensions &mExtensions;
    const GLuint mProgramID;

    angle::Result mResult = angle::Result::Continue;

    // Note: resources are kept alive by the front-end for the entire duration of the link,
    // including during resolve when getResult() and postLink() are called.
    const gl::ProgramLinkedResources *mResources = nullptr;
};

ProgramGL::ProgramGL(const gl::ProgramState &data,
                     const FunctionsGL *functions,
                     const angle::FeaturesGL &features,
                     StateManagerGL *stateManager,
                     const std::shared_ptr<RendererGL> &renderer)
    : ProgramImpl(data),
      mFunctions(functions),
      mFeatures(features),
      mStateManager(stateManager),
      mProgramID(0),
      mRenderer(renderer)
{
    ASSERT(mFunctions);
    ASSERT(mStateManager);

    mProgramID = mFunctions->createProgram();
}

ProgramGL::~ProgramGL() = default;

void ProgramGL::destroy(const gl::Context *context)
{
    mFunctions->deleteProgram(mProgramID);
    mProgramID = 0;
}

angle::Result ProgramGL::load(const gl::Context *context,
                              gl::BinaryInputStream *stream,
                              std::shared_ptr<LinkTask> *loadTaskOut,
                              bool *successOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramGL::load");
    ProgramExecutableGL *executableGL = getExecutable();

    // Read the binary format, size and blob
    GLenum binaryFormat   = stream->readInt<GLenum>();
    GLint binaryLength    = stream->readInt<GLint>();
    const uint8_t *binary = stream->data() + stream->offset();
    stream->skip(binaryLength);

    // Load the binary
    mFunctions->programBinary(mProgramID, binaryFormat, binary, binaryLength);

    // Verify that the program linked
    if (!checkLinkStatus())
    {
        return angle::Result::Continue;
    }

    executableGL->postLink(mFunctions, mStateManager, mFeatures, mProgramID);
    reapplyUBOBindingsIfNeeded(context);

    *loadTaskOut = {};
    *successOut  = true;

    return angle::Result::Continue;
}

void ProgramGL::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    GLint binaryLength = 0;
    mFunctions->getProgramiv(mProgramID, GL_PROGRAM_BINARY_LENGTH, &binaryLength);

    std::vector<uint8_t> binary(std::max(binaryLength, 1));
    GLenum binaryFormat = GL_NONE;
    mFunctions->getProgramBinary(mProgramID, binaryLength, &binaryLength, &binaryFormat,
                                 binary.data());

    stream->writeInt(binaryFormat);
    stream->writeInt(binaryLength);
    stream->writeBytes(binary.data(), binaryLength);

    reapplyUBOBindingsIfNeeded(context);
}

void ProgramGL::reapplyUBOBindingsIfNeeded(const gl::Context *context)
{
    // Re-apply UBO bindings to work around driver bugs.
    const angle::FeaturesGL &features = GetImplAs<ContextGL>(context)->getFeaturesGL();
    if (features.reapplyUBOBindingsAfterUsingBinaryProgram.enabled)
    {
        const auto &blocks = mState.getExecutable().getUniformBlocks();
        for (size_t blockIndex : mState.getExecutable().getActiveUniformBlockBindings())
        {
            setUniformBlockBinding(static_cast<GLuint>(blockIndex), blocks[blockIndex].pod.binding);
        }
    }
}

void ProgramGL::setBinaryRetrievableHint(bool retrievable)
{
    // glProgramParameteri isn't always available on ES backends.
    if (mFunctions->programParameteri)
    {
        mFunctions->programParameteri(mProgramID, GL_PROGRAM_BINARY_RETRIEVABLE_HINT,
                                      retrievable ? GL_TRUE : GL_FALSE);
    }
}

void ProgramGL::setSeparable(bool separable)
{
    mFunctions->programParameteri(mProgramID, GL_PROGRAM_SEPARABLE, separable ? GL_TRUE : GL_FALSE);
}

void ProgramGL::prepareForLink(const gl::ShaderMap<ShaderImpl *> &shaders)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mAttachedShaders[shaderType] = 0;

        if (shaders[shaderType] != nullptr)
        {
            const ShaderGL *shaderGL     = GetAs<ShaderGL>(shaders[shaderType]);
            mAttachedShaders[shaderType] = shaderGL->getShaderID();
        }
    }
}

angle::Result ProgramGL::link(const gl::Context *context, std::shared_ptr<LinkTask> *linkTaskOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramGL::link");

    *linkTaskOut = std::make_shared<LinkTaskGL>(this, mRenderer->hasNativeParallelCompile(),
                                                mFunctions, context->getExtensions(), mProgramID);

    return angle::Result::Continue;
}

void ProgramGL::linkJobImpl(const gl::Extensions &extensions)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramGL::linkJobImpl");
    const gl::ProgramExecutable &executable = mState.getExecutable();
    ProgramExecutableGL *executableGL       = getExecutable();

    if (mAttachedShaders[gl::ShaderType::Compute] != 0)
    {
        mFunctions->attachShader(mProgramID, mAttachedShaders[gl::ShaderType::Compute]);
    }
    else
    {
        // Set the transform feedback state
        std::vector<std::string> transformFeedbackVaryingMappedNames;
        const gl::ShaderType tfShaderType =
            executable.hasLinkedShaderStage(gl::ShaderType::Geometry) ? gl::ShaderType::Geometry
                                                                      : gl::ShaderType::Vertex;
        const gl::SharedCompiledShaderState &tfShaderState = mState.getAttachedShader(tfShaderType);
        for (const auto &tfVarying : mState.getTransformFeedbackVaryingNames())
        {
            std::string tfVaryingMappedName =
                GetTransformFeedbackVaryingMappedName(tfShaderState, tfVarying);
            transformFeedbackVaryingMappedNames.push_back(tfVaryingMappedName);
        }

        if (transformFeedbackVaryingMappedNames.empty())
        {
            // Only clear the transform feedback state if transform feedback varyings have already
            // been set.
            if (executableGL->mHasAppliedTransformFeedbackVaryings)
            {
                ASSERT(mFunctions->transformFeedbackVaryings);
                mFunctions->transformFeedbackVaryings(mProgramID, 0, nullptr,
                                                      mState.getTransformFeedbackBufferMode());
                executableGL->mHasAppliedTransformFeedbackVaryings = false;
            }
        }
        else
        {
            ASSERT(mFunctions->transformFeedbackVaryings);
            std::vector<const GLchar *> transformFeedbackVaryings;
            for (const auto &varying : transformFeedbackVaryingMappedNames)
            {
                transformFeedbackVaryings.push_back(varying.c_str());
            }
            mFunctions->transformFeedbackVaryings(
                mProgramID, static_cast<GLsizei>(transformFeedbackVaryingMappedNames.size()),
                &transformFeedbackVaryings[0], mState.getTransformFeedbackBufferMode());
            executableGL->mHasAppliedTransformFeedbackVaryings = true;
        }

        for (const gl::ShaderType shaderType : gl::kAllGraphicsShaderTypes)
        {
            if (mAttachedShaders[shaderType] != 0)
            {
                mFunctions->attachShader(mProgramID, mAttachedShaders[shaderType]);
            }
        }

        // Bind attribute locations to match the GL layer.
        for (const gl::ProgramInput &attribute : executable.getProgramInputs())
        {
            if (!attribute.isActive() || attribute.isBuiltIn())
            {
                continue;
            }

            mFunctions->bindAttribLocation(mProgramID, attribute.getLocation(),
                                           attribute.mappedName.c_str());
        }

        // Bind the secondary fragment color outputs defined in EXT_blend_func_extended. We only use
        // the API to bind fragment output locations in case EXT_blend_func_extended is enabled.
        // Otherwise shader-assigned locations will work.
        if (extensions.blendFuncExtendedEXT)
        {
            const gl::SharedCompiledShaderState &fragmentShader =
                mState.getAttachedShader(gl::ShaderType::Fragment);
            if (fragmentShader && fragmentShader->shaderVersion == 100 &&
                mFunctions->standard == STANDARD_GL_DESKTOP)
            {
                const auto &shaderOutputs = fragmentShader->activeOutputVariables;
                for (const auto &output : shaderOutputs)
                {
                    // TODO(http://anglebug.com/1085) This could be cleaner if the transformed names
                    // would be set correctly in ShaderVariable::mappedName. This would require some
                    // refactoring in the translator. Adding a mapped name dictionary for builtins
                    // into the symbol table would be one fairly clean way to do it.
                    if (output.name == "gl_SecondaryFragColorEXT")
                    {
                        mFunctions->bindFragDataLocationIndexed(mProgramID, 0, 0,
                                                                "webgl_FragColor");
                        mFunctions->bindFragDataLocationIndexed(mProgramID, 0, 1,
                                                                "webgl_SecondaryFragColor");
                    }
                    else if (output.name == "gl_SecondaryFragDataEXT")
                    {
                        // Basically we should have a loop here going over the output
                        // array binding "webgl_FragData[i]" and "webgl_SecondaryFragData[i]" array
                        // indices to the correct color buffers and color indices.
                        // However I'm not sure if this construct is legal or not, neither ARB or
                        // EXT version of the spec mention this. They only mention that
                        // automatically assigned array locations for ESSL 3.00 output arrays need
                        // to have contiguous locations.
                        //
                        // In practice it seems that binding array members works on some drivers and
                        // fails on others. One option could be to modify the shader translator to
                        // expand the arrays into individual output variables instead of using an
                        // array.
                        //
                        // For now we're going to have a limitation of assuming that
                        // GL_MAX_DUAL_SOURCE_DRAW_BUFFERS is *always* 1 and then only bind the
                        // basename of the variable ignoring any indices. This appears to work
                        // uniformly.
                        ASSERT(output.isArray() && output.getOutermostArraySize() == 1);

                        mFunctions->bindFragDataLocationIndexed(mProgramID, 0, 0, "webgl_FragData");
                        mFunctions->bindFragDataLocationIndexed(mProgramID, 0, 1,
                                                                "webgl_SecondaryFragData");
                    }
                }
            }
            else if (fragmentShader && fragmentShader->shaderVersion >= 300)
            {
                // ESSL 3.00 and up.
                const auto &outputLocations          = executable.getOutputLocations();
                const auto &secondaryOutputLocations = executable.getSecondaryOutputLocations();
                for (size_t outputLocationIndex = 0u; outputLocationIndex < outputLocations.size();
                     ++outputLocationIndex)
                {
                    const gl::VariableLocation &outputLocation =
                        outputLocations[outputLocationIndex];
                    if (outputLocation.arrayIndex == 0 && outputLocation.used() &&
                        !outputLocation.ignored)
                    {
                        const gl::ProgramOutput &outputVar =
                            executable.getOutputVariables()[outputLocation.index];
                        if (outputVar.pod.location == -1 || outputVar.pod.index == -1)
                        {
                            // We only need to assign the location and index via the API in case the
                            // variable doesn't have a shader-assigned location and index. If a
                            // variable doesn't have its location set in the shader it doesn't have
                            // the index set either.
                            ASSERT(outputVar.pod.index == -1);
                            mFunctions->bindFragDataLocationIndexed(
                                mProgramID, static_cast<int>(outputLocationIndex), 0,
                                outputVar.mappedName.c_str());
                        }
                    }
                }
                for (size_t outputLocationIndex = 0u;
                     outputLocationIndex < secondaryOutputLocations.size(); ++outputLocationIndex)
                {
                    const gl::VariableLocation &outputLocation =
                        secondaryOutputLocations[outputLocationIndex];
                    if (outputLocation.arrayIndex == 0 && outputLocation.used() &&
                        !outputLocation.ignored)
                    {
                        const gl::ProgramOutput &outputVar =
                            executable.getOutputVariables()[outputLocation.index];
                        if (outputVar.pod.location == -1 || outputVar.pod.index == -1)
                        {
                            // We only need to assign the location and index via the API in case the
                            // variable doesn't have a shader-assigned location and index.  If a
                            // variable doesn't have its location set in the shader it doesn't have
                            // the index set either.
                            ASSERT(outputVar.pod.index == -1);
                            mFunctions->bindFragDataLocationIndexed(
                                mProgramID, static_cast<int>(outputLocationIndex), 1,
                                outputVar.mappedName.c_str());
                        }
                    }
                }
            }
        }
    }

    mFunctions->linkProgram(mProgramID);
}

angle::Result ProgramGL::postLinkJobImpl(const gl::ProgramLinkedResources &resources)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramGL::postLinkJobImpl");

    if (mAttachedShaders[gl::ShaderType::Compute] != 0)
    {
        mFunctions->detachShader(mProgramID, mAttachedShaders[gl::ShaderType::Compute]);
    }
    else
    {
        for (const gl::ShaderType shaderType : gl::kAllGraphicsShaderTypes)
        {
            if (mAttachedShaders[shaderType] != 0)
            {
                mFunctions->detachShader(mProgramID, mAttachedShaders[shaderType]);
            }
        }
    }

    // Verify the link
    if (!checkLinkStatus())
    {
        return angle::Result::Stop;
    }

    if (mFeatures.alwaysCallUseProgramAfterLink.enabled)
    {
        mStateManager->forceUseProgram(mProgramID);
    }

    linkResources(resources);
    getExecutable()->postLink(mFunctions, mStateManager, mFeatures, mProgramID);

    return angle::Result::Continue;
}

GLboolean ProgramGL::validate(const gl::Caps & /*caps*/)
{
    // TODO(jmadill): implement validate
    return true;
}

void ProgramGL::setUniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    const gl::ProgramExecutable &executable = mState.getExecutable();
    ProgramExecutableGL *executableGL       = getExecutable();

    // Lazy init
    if (executableGL->mUniformBlockRealLocationMap.empty())
    {
        executableGL->mUniformBlockRealLocationMap.reserve(executable.getUniformBlocks().size());
        for (const gl::InterfaceBlock &uniformBlock : executable.getUniformBlocks())
        {
            const std::string &mappedNameWithIndex = uniformBlock.mappedNameWithArrayIndex();
            GLuint blockIndex =
                mFunctions->getUniformBlockIndex(mProgramID, mappedNameWithIndex.c_str());
            executableGL->mUniformBlockRealLocationMap.push_back(blockIndex);
        }
    }

    GLuint realBlockIndex = executableGL->mUniformBlockRealLocationMap[uniformBlockIndex];
    if (realBlockIndex != GL_INVALID_INDEX)
    {
        mFunctions->uniformBlockBinding(mProgramID, realBlockIndex, uniformBlockBinding);
    }
}

bool ProgramGL::getUniformBlockSize(const std::string & /* blockName */,
                                    const std::string &blockMappedName,
                                    size_t *sizeOut) const
{
    ASSERT(mProgramID != 0u);

    GLuint blockIndex = mFunctions->getUniformBlockIndex(mProgramID, blockMappedName.c_str());
    if (blockIndex == GL_INVALID_INDEX)
    {
        *sizeOut = 0;
        return false;
    }

    GLint dataSize = 0;
    mFunctions->getActiveUniformBlockiv(mProgramID, blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE,
                                        &dataSize);
    *sizeOut = static_cast<size_t>(dataSize);
    return true;
}

bool ProgramGL::getUniformBlockMemberInfo(const std::string & /* memberUniformName */,
                                          const std::string &memberUniformMappedName,
                                          sh::BlockMemberInfo *memberInfoOut) const
{
    GLuint uniformIndex;
    const GLchar *memberNameGLStr = memberUniformMappedName.c_str();
    mFunctions->getUniformIndices(mProgramID, 1, &memberNameGLStr, &uniformIndex);

    if (uniformIndex == GL_INVALID_INDEX)
    {
        *memberInfoOut = sh::kDefaultBlockMemberInfo;
        return false;
    }

    mFunctions->getActiveUniformsiv(mProgramID, 1, &uniformIndex, GL_UNIFORM_OFFSET,
                                    &memberInfoOut->offset);
    mFunctions->getActiveUniformsiv(mProgramID, 1, &uniformIndex, GL_UNIFORM_ARRAY_STRIDE,
                                    &memberInfoOut->arrayStride);
    mFunctions->getActiveUniformsiv(mProgramID, 1, &uniformIndex, GL_UNIFORM_MATRIX_STRIDE,
                                    &memberInfoOut->matrixStride);

    // TODO(jmadill): possibly determine this at the gl::Program level.
    GLint isRowMajorMatrix = 0;
    mFunctions->getActiveUniformsiv(mProgramID, 1, &uniformIndex, GL_UNIFORM_IS_ROW_MAJOR,
                                    &isRowMajorMatrix);
    memberInfoOut->isRowMajorMatrix = gl::ConvertToBool(isRowMajorMatrix);
    return true;
}

bool ProgramGL::getShaderStorageBlockMemberInfo(const std::string & /* memberName */,
                                                const std::string &memberUniformMappedName,
                                                sh::BlockMemberInfo *memberInfoOut) const
{
    const GLchar *memberNameGLStr = memberUniformMappedName.c_str();
    GLuint index =
        mFunctions->getProgramResourceIndex(mProgramID, GL_BUFFER_VARIABLE, memberNameGLStr);

    if (index == GL_INVALID_INDEX)
    {
        *memberInfoOut = sh::kDefaultBlockMemberInfo;
        return false;
    }

    constexpr int kPropCount             = 5;
    std::array<GLenum, kPropCount> props = {
        {GL_ARRAY_STRIDE, GL_IS_ROW_MAJOR, GL_MATRIX_STRIDE, GL_OFFSET, GL_TOP_LEVEL_ARRAY_STRIDE}};
    std::array<GLint, kPropCount> params;
    GLsizei length;
    mFunctions->getProgramResourceiv(mProgramID, GL_BUFFER_VARIABLE, index, kPropCount,
                                     props.data(), kPropCount, &length, params.data());
    ASSERT(kPropCount == length);
    memberInfoOut->arrayStride         = params[0];
    memberInfoOut->isRowMajorMatrix    = params[1] != 0;
    memberInfoOut->matrixStride        = params[2];
    memberInfoOut->offset              = params[3];
    memberInfoOut->topLevelArrayStride = params[4];

    return true;
}

bool ProgramGL::getShaderStorageBlockSize(const std::string &name,
                                          const std::string &mappedName,
                                          size_t *sizeOut) const
{
    const GLchar *nameGLStr = mappedName.c_str();
    GLuint index =
        mFunctions->getProgramResourceIndex(mProgramID, GL_SHADER_STORAGE_BLOCK, nameGLStr);

    if (index == GL_INVALID_INDEX)
    {
        *sizeOut = 0;
        return false;
    }

    GLenum prop    = GL_BUFFER_DATA_SIZE;
    GLsizei length = 0;
    GLint dataSize = 0;
    mFunctions->getProgramResourceiv(mProgramID, GL_SHADER_STORAGE_BLOCK, index, 1, &prop, 1,
                                     &length, &dataSize);
    *sizeOut = static_cast<size_t>(dataSize);
    return true;
}

void ProgramGL::getAtomicCounterBufferSizeMap(std::map<int, unsigned int> *sizeMapOut) const
{
    if (mFunctions->getProgramInterfaceiv == nullptr)
    {
        return;
    }

    int resourceCount = 0;
    mFunctions->getProgramInterfaceiv(mProgramID, GL_ATOMIC_COUNTER_BUFFER, GL_ACTIVE_RESOURCES,
                                      &resourceCount);

    for (int index = 0; index < resourceCount; index++)
    {
        constexpr int kPropCount             = 2;
        std::array<GLenum, kPropCount> props = {{GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE}};
        std::array<GLint, kPropCount> params;
        GLsizei length;
        mFunctions->getProgramResourceiv(mProgramID, GL_ATOMIC_COUNTER_BUFFER, index, kPropCount,
                                         props.data(), kPropCount, &length, params.data());
        ASSERT(kPropCount == length);
        int bufferBinding           = params[0];
        unsigned int bufferDataSize = params[1];
        sizeMapOut->insert(std::pair<int, unsigned int>(bufferBinding, bufferDataSize));
    }
}

bool ProgramGL::checkLinkStatus()
{
    GLint linkStatus = GL_FALSE;
    mFunctions->getProgramiv(mProgramID, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE)
    {
        // Linking or program binary loading failed, put the error into the info log.
        GLint infoLogLength = 0;
        mFunctions->getProgramiv(mProgramID, GL_INFO_LOG_LENGTH, &infoLogLength);

        // Info log length includes the null terminator, so 1 means that the info log is an empty
        // string.
        if (infoLogLength > 1)
        {
            std::vector<char> buf(infoLogLength);
            mFunctions->getProgramInfoLog(mProgramID, infoLogLength, nullptr, &buf[0]);

            mState.getExecutable().getInfoLog() << buf.data();

            WARN() << "Program link or binary loading failed: " << buf.data();
        }
        else
        {
            WARN() << "Program link or binary loading failed with no info log.";
        }

        // This may happen under normal circumstances if we're loading program binaries and the
        // driver or hardware has changed.
        ASSERT(mProgramID != 0);
        return false;
    }

    return true;
}

void ProgramGL::markUnusedUniformLocations(std::vector<gl::VariableLocation> *uniformLocations,
                                           std::vector<gl::SamplerBinding> *samplerBindings,
                                           std::vector<gl::ImageBinding> *imageBindings)
{
    const gl::ProgramExecutable &executable = mState.getExecutable();
    const ProgramExecutableGL *executableGL = getExecutable();

    GLint maxLocation = static_cast<GLint>(uniformLocations->size());
    for (GLint location = 0; location < maxLocation; ++location)
    {
        if (executableGL->mUniformRealLocationMap[location] == -1)
        {
            auto &locationRef = (*uniformLocations)[location];
            if (executable.isSamplerUniformIndex(locationRef.index))
            {
                GLuint samplerIndex = executable.getSamplerIndexFromUniformIndex(locationRef.index);
                gl::SamplerBinding &samplerBinding = (*samplerBindings)[samplerIndex];
                if (locationRef.arrayIndex <
                    static_cast<unsigned int>(samplerBinding.textureUnitsCount))
                {
                    // Crop unused sampler bindings in the sampler array.
                    SetBitField(samplerBinding.textureUnitsCount, locationRef.arrayIndex);
                }
            }
            else if (executable.isImageUniformIndex(locationRef.index))
            {
                GLuint imageIndex = executable.getImageIndexFromUniformIndex(locationRef.index);
                gl::ImageBinding &imageBinding = (*imageBindings)[imageIndex];
                if (locationRef.arrayIndex < imageBinding.boundImageUnits.size())
                {
                    // Crop unused image bindings in the image array.
                    imageBinding.boundImageUnits.resize(locationRef.arrayIndex);
                }
            }
            // If the location has been previously bound by a glBindUniformLocation call, it should
            // be marked as ignored. Otherwise it's unused.
            if (mState.getUniformLocationBindings().getBindingByLocation(location) != -1)
            {
                locationRef.markIgnored();
            }
            else
            {
                locationRef.markUnused();
            }
        }
    }
}

void ProgramGL::linkResources(const gl::ProgramLinkedResources &resources)
{
    // Gather interface block info.
    auto getUniformBlockSize = [this](const std::string &name, const std::string &mappedName,
                                      size_t *sizeOut) {
        return this->getUniformBlockSize(name, mappedName, sizeOut);
    };

    auto getUniformBlockMemberInfo = [this](const std::string &name, const std::string &mappedName,
                                            sh::BlockMemberInfo *infoOut) {
        return this->getUniformBlockMemberInfo(name, mappedName, infoOut);
    };

    resources.uniformBlockLinker.linkBlocks(getUniformBlockSize, getUniformBlockMemberInfo);

    auto getShaderStorageBlockSize = [this](const std::string &name, const std::string &mappedName,
                                            size_t *sizeOut) {
        return this->getShaderStorageBlockSize(name, mappedName, sizeOut);
    };

    auto getShaderStorageBlockMemberInfo = [this](const std::string &name,
                                                  const std::string &mappedName,
                                                  sh::BlockMemberInfo *infoOut) {
        return this->getShaderStorageBlockMemberInfo(name, mappedName, infoOut);
    };
    resources.shaderStorageBlockLinker.linkBlocks(getShaderStorageBlockSize,
                                                  getShaderStorageBlockMemberInfo);

    // Gather atomic counter buffer info.
    std::map<int, unsigned int> sizeMap;
    getAtomicCounterBufferSizeMap(&sizeMap);
    resources.atomicCounterBufferLinker.link(sizeMap);
}

angle::Result ProgramGL::syncState(const gl::Context *context)
{
    const gl::ProgramExecutable &executable = mState.getExecutable();

    gl::ProgramExecutable::DirtyBits dirtyBits = executable.getAndResetDirtyBits();
    for (size_t dirtyBit : dirtyBits)
    {
        ASSERT(dirtyBit <= gl::ProgramExecutable::DIRTY_BIT_UNIFORM_BLOCK_BINDING_MAX);
        GLuint binding = static_cast<GLuint>(dirtyBit);
        setUniformBlockBinding(binding, executable.getUniformBlockBinding(binding));
    }
    return angle::Result::Continue;
}
}  // namespace rx
