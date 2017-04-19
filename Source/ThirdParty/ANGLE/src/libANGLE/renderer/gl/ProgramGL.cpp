//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramGL.cpp: Implements the class methods for ProgramGL.

#include "libANGLE/renderer/gl/ProgramGL.h"

#include "common/bitset_utils.h"
#include "common/angleutils.h"
#include "common/debug.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/ShaderGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/WorkaroundsGL.h"
#include "libANGLE/Uniform.h"
#include "platform/Platform.h"

namespace rx
{

ProgramGL::ProgramGL(const gl::ProgramState &data,
                     const FunctionsGL *functions,
                     const WorkaroundsGL &workarounds,
                     StateManagerGL *stateManager,
                     bool enablePathRendering)
    : ProgramImpl(data),
      mFunctions(functions),
      mWorkarounds(workarounds),
      mStateManager(stateManager),
      mEnablePathRendering(enablePathRendering),
      mProgramID(0)
{
    ASSERT(mFunctions);
    ASSERT(mStateManager);

    mProgramID = mFunctions->createProgram();
}

ProgramGL::~ProgramGL()
{
    mFunctions->deleteProgram(mProgramID);
    mProgramID = 0;
}

LinkResult ProgramGL::load(const ContextImpl *contextImpl,
                           gl::InfoLog &infoLog,
                           gl::BinaryInputStream *stream)
{
    preLink();

    // Read the binary format, size and blob
    GLenum binaryFormat   = stream->readInt<GLenum>();
    GLint binaryLength    = stream->readInt<GLint>();
    const uint8_t *binary = stream->data() + stream->offset();
    stream->skip(binaryLength);

    // Load the binary
    mFunctions->programBinary(mProgramID, binaryFormat, binary, binaryLength);

    // Verify that the program linked
    if (!checkLinkStatus(infoLog))
    {
        return false;
    }

    postLink();

    // Re-apply UBO bindings to work around driver bugs.
    const WorkaroundsGL &workaroundsGL = GetAs<ContextGL>(contextImpl)->getWorkaroundsGL();
    if (workaroundsGL.reapplyUBOBindingsAfterLoadingBinaryProgram)
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
        for (GLuint bindingIndex : angle::IterateBitSet(mState.getActiveUniformBlockBindingsMask()))
        {
            setUniformBlockBinding(bindingIndex, mState.getUniformBlockBinding(bindingIndex));
        }
#pragma clang diagnostic pop
    }

    return true;
}

gl::Error ProgramGL::save(gl::BinaryOutputStream *stream)
{
    GLint binaryLength = 0;
    mFunctions->getProgramiv(mProgramID, GL_PROGRAM_BINARY_LENGTH, &binaryLength);

    std::vector<uint8_t> binary(binaryLength);
    GLenum binaryFormat = GL_NONE;
    mFunctions->getProgramBinary(mProgramID, binaryLength, &binaryLength, &binaryFormat,
                                 &binary[0]);

    stream->writeInt(binaryFormat);
    stream->writeInt(binaryLength);
    stream->writeBytes(&binary[0], binaryLength);

    return gl::NoError();
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

LinkResult ProgramGL::link(ContextImpl *contextImpl,
                           const gl::VaryingPacking &packing,
                           gl::InfoLog &infoLog)
{
    preLink();

    if (mState.getAttachedComputeShader())
    {
        const ShaderGL *computeShaderGL = GetImplAs<ShaderGL>(mState.getAttachedComputeShader());

        mFunctions->attachShader(mProgramID, computeShaderGL->getShaderID());

        // Link and verify
        mFunctions->linkProgram(mProgramID);

        // Detach the shaders
        mFunctions->detachShader(mProgramID, computeShaderGL->getShaderID());
    }
    else
    {
        // Set the transform feedback state
        std::vector<const GLchar *> transformFeedbackVaryings;
        for (const auto &tfVarying : mState.getTransformFeedbackVaryingNames())
        {
            transformFeedbackVaryings.push_back(tfVarying.c_str());
        }

        if (transformFeedbackVaryings.empty())
        {
            if (mFunctions->transformFeedbackVaryings)
            {
                mFunctions->transformFeedbackVaryings(mProgramID, 0, nullptr,
                                                      mState.getTransformFeedbackBufferMode());
            }
        }
        else
        {
            ASSERT(mFunctions->transformFeedbackVaryings);
            mFunctions->transformFeedbackVaryings(
                mProgramID, static_cast<GLsizei>(transformFeedbackVaryings.size()),
                &transformFeedbackVaryings[0], mState.getTransformFeedbackBufferMode());
        }

        const ShaderGL *vertexShaderGL   = GetImplAs<ShaderGL>(mState.getAttachedVertexShader());
        const ShaderGL *fragmentShaderGL = GetImplAs<ShaderGL>(mState.getAttachedFragmentShader());

        // Attach the shaders
        mFunctions->attachShader(mProgramID, vertexShaderGL->getShaderID());
        mFunctions->attachShader(mProgramID, fragmentShaderGL->getShaderID());

        // Bind attribute locations to match the GL layer.
        for (const sh::Attribute &attribute : mState.getAttributes())
        {
            if (!attribute.staticUse || attribute.isBuiltIn())
            {
                continue;
            }

            mFunctions->bindAttribLocation(mProgramID, attribute.location, attribute.name.c_str());
        }

        // Link and verify
        mFunctions->linkProgram(mProgramID);

        // Detach the shaders
        mFunctions->detachShader(mProgramID, vertexShaderGL->getShaderID());
        mFunctions->detachShader(mProgramID, fragmentShaderGL->getShaderID());
    }

    // Verify the link
    if (!checkLinkStatus(infoLog))
    {
        return false;
    }

    if (mWorkarounds.alwaysCallUseProgramAfterLink)
    {
        mStateManager->forceUseProgram(mProgramID);
    }

    postLink();

    return true;
}

GLboolean ProgramGL::validate(const gl::Caps & /*caps*/, gl::InfoLog * /*infoLog*/)
{
    // TODO(jmadill): implement validate
    return true;
}

void ProgramGL::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    if (mFunctions->programUniform1fv != nullptr)
    {
        mFunctions->programUniform1fv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform1fv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    if (mFunctions->programUniform2fv != nullptr)
    {
        mFunctions->programUniform2fv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform2fv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    if (mFunctions->programUniform3fv != nullptr)
    {
        mFunctions->programUniform3fv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform3fv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    if (mFunctions->programUniform4fv != nullptr)
    {
        mFunctions->programUniform4fv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform4fv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    if (mFunctions->programUniform1iv != nullptr)
    {
        mFunctions->programUniform1iv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform1iv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    if (mFunctions->programUniform2iv != nullptr)
    {
        mFunctions->programUniform2iv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform2iv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    if (mFunctions->programUniform3iv != nullptr)
    {
        mFunctions->programUniform3iv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform3iv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    if (mFunctions->programUniform4iv != nullptr)
    {
        mFunctions->programUniform4iv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform4iv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    if (mFunctions->programUniform1uiv != nullptr)
    {
        mFunctions->programUniform1uiv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform1uiv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    if (mFunctions->programUniform2uiv != nullptr)
    {
        mFunctions->programUniform2uiv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform2uiv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    if (mFunctions->programUniform3uiv != nullptr)
    {
        mFunctions->programUniform3uiv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform3uiv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    if (mFunctions->programUniform4uiv != nullptr)
    {
        mFunctions->programUniform4uiv(mProgramID, uniLoc(location), count, v);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniform4uiv(uniLoc(location), count, v);
    }
}

void ProgramGL::setUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (mFunctions->programUniformMatrix2fv != nullptr)
    {
        mFunctions->programUniformMatrix2fv(mProgramID, uniLoc(location), count, transpose, value);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniformMatrix2fv(uniLoc(location), count, transpose, value);
    }
}

void ProgramGL::setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (mFunctions->programUniformMatrix3fv != nullptr)
    {
        mFunctions->programUniformMatrix3fv(mProgramID, uniLoc(location), count, transpose, value);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniformMatrix3fv(uniLoc(location), count, transpose, value);
    }
}

void ProgramGL::setUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (mFunctions->programUniformMatrix4fv != nullptr)
    {
        mFunctions->programUniformMatrix4fv(mProgramID, uniLoc(location), count, transpose, value);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniformMatrix4fv(uniLoc(location), count, transpose, value);
    }
}

void ProgramGL::setUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (mFunctions->programUniformMatrix2x3fv != nullptr)
    {
        mFunctions->programUniformMatrix2x3fv(mProgramID, uniLoc(location), count, transpose,
                                              value);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniformMatrix2x3fv(uniLoc(location), count, transpose, value);
    }
}

void ProgramGL::setUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (mFunctions->programUniformMatrix3x2fv != nullptr)
    {
        mFunctions->programUniformMatrix3x2fv(mProgramID, uniLoc(location), count, transpose,
                                              value);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniformMatrix3x2fv(uniLoc(location), count, transpose, value);
    }
}

void ProgramGL::setUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (mFunctions->programUniformMatrix2x4fv != nullptr)
    {
        mFunctions->programUniformMatrix2x4fv(mProgramID, uniLoc(location), count, transpose,
                                              value);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniformMatrix2x4fv(uniLoc(location), count, transpose, value);
    }
}

void ProgramGL::setUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (mFunctions->programUniformMatrix4x2fv != nullptr)
    {
        mFunctions->programUniformMatrix4x2fv(mProgramID, uniLoc(location), count, transpose,
                                              value);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniformMatrix4x2fv(uniLoc(location), count, transpose, value);
    }
}

void ProgramGL::setUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (mFunctions->programUniformMatrix3x4fv != nullptr)
    {
        mFunctions->programUniformMatrix3x4fv(mProgramID, uniLoc(location), count, transpose,
                                              value);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniformMatrix3x4fv(uniLoc(location), count, transpose, value);
    }
}

void ProgramGL::setUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (mFunctions->programUniformMatrix4x3fv != nullptr)
    {
        mFunctions->programUniformMatrix4x3fv(mProgramID, uniLoc(location), count, transpose,
                                              value);
    }
    else
    {
        mStateManager->useProgram(mProgramID);
        mFunctions->uniformMatrix4x3fv(uniLoc(location), count, transpose, value);
    }
}

void ProgramGL::setUniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    // Lazy init
    if (mUniformBlockRealLocationMap.empty())
    {
        mUniformBlockRealLocationMap.reserve(mState.getUniformBlocks().size());
        for (const gl::UniformBlock &uniformBlock : mState.getUniformBlocks())
        {
            const std::string &nameWithIndex = uniformBlock.nameWithArrayIndex();
            GLuint blockIndex = mFunctions->getUniformBlockIndex(mProgramID, nameWithIndex.c_str());
            mUniformBlockRealLocationMap.push_back(blockIndex);
        }
    }

    GLuint realBlockIndex = mUniformBlockRealLocationMap[uniformBlockIndex];
    if (realBlockIndex != GL_INVALID_INDEX)
    {
        mFunctions->uniformBlockBinding(mProgramID, realBlockIndex, uniformBlockBinding);
    }
}

GLuint ProgramGL::getProgramID() const
{
    return mProgramID;
}

bool ProgramGL::getUniformBlockSize(const std::string &blockName, size_t *sizeOut) const
{
    ASSERT(mProgramID != 0u);

    GLuint blockIndex = mFunctions->getUniformBlockIndex(mProgramID, blockName.c_str());
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

bool ProgramGL::getUniformBlockMemberInfo(const std::string &memberUniformName,
                                          sh::BlockMemberInfo *memberInfoOut) const
{
    GLuint uniformIndex;
    const GLchar *memberNameGLStr = memberUniformName.c_str();
    mFunctions->getUniformIndices(mProgramID, 1, &memberNameGLStr, &uniformIndex);

    if (uniformIndex == GL_INVALID_INDEX)
    {
        *memberInfoOut = sh::BlockMemberInfo::getDefaultBlockInfo();
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
    memberInfoOut->isRowMajorMatrix = isRowMajorMatrix != GL_FALSE;
    return true;
}

void ProgramGL::setPathFragmentInputGen(const std::string &inputName,
                                        GLenum genMode,
                                        GLint components,
                                        const GLfloat *coeffs)
{
    ASSERT(mEnablePathRendering);

    for (const auto &input : mPathRenderingFragmentInputs)
    {
        if (input.name == inputName)
        {
            mFunctions->programPathFragmentInputGenNV(mProgramID, input.location, genMode,
                                                      components, coeffs);
            ASSERT(mFunctions->getError() == GL_NO_ERROR);
            return;
        }
    }

}

void ProgramGL::preLink()
{
    // Reset the program state
    mUniformRealLocationMap.clear();
    mUniformBlockRealLocationMap.clear();
    mPathRenderingFragmentInputs.clear();
}

bool ProgramGL::checkLinkStatus(gl::InfoLog &infoLog)
{
    GLint linkStatus = GL_FALSE;
    mFunctions->getProgramiv(mProgramID, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE)
    {
        // Linking failed, put the error into the info log
        GLint infoLogLength = 0;
        mFunctions->getProgramiv(mProgramID, GL_INFO_LOG_LENGTH, &infoLogLength);

        // Info log length includes the null terminator, so 1 means that the info log is an empty
        // string.
        if (infoLogLength > 1)
        {
            std::vector<char> buf(infoLogLength);
            mFunctions->getProgramInfoLog(mProgramID, infoLogLength, nullptr, &buf[0]);

            mFunctions->deleteProgram(mProgramID);
            mProgramID = 0;

            infoLog << buf.data();

            WARN() << "Program link failed unexpectedly: " << buf.data();
        }
        else
        {
            WARN() << "Program link failed unexpectedly with no info log.";
        }

        // TODO, return GL_OUT_OF_MEMORY or just fail the link? This is an unexpected case
        return false;
    }

    return true;
}

void ProgramGL::postLink()
{
    // Query the uniform information
    ASSERT(mUniformRealLocationMap.empty());
    const auto &uniformLocations = mState.getUniformLocations();
    const auto &uniforms = mState.getUniforms();
    mUniformRealLocationMap.resize(uniformLocations.size(), GL_INVALID_INDEX);
    for (size_t uniformLocation = 0; uniformLocation < uniformLocations.size(); uniformLocation++)
    {
        const auto &entry = uniformLocations[uniformLocation];
        if (!entry.used)
        {
            continue;
        }

        // From the spec:
        // "Locations for sequential array indices are not required to be sequential."
        const gl::LinkedUniform &uniform = uniforms[entry.index];
        std::stringstream fullNameStr;
        fullNameStr << uniform.name;
        if (uniform.isArray())
        {
            fullNameStr << "[" << entry.element << "]";
        }
        const std::string &fullName = fullNameStr.str();

        GLint realLocation = mFunctions->getUniformLocation(mProgramID, fullName.c_str());
        mUniformRealLocationMap[uniformLocation] = realLocation;
    }

    // Discover CHROMIUM_path_rendering fragment inputs if enabled.
    if (!mEnablePathRendering)
        return;

    GLint numFragmentInputs = 0;
    mFunctions->getProgramInterfaceiv(mProgramID, GL_FRAGMENT_INPUT_NV, GL_ACTIVE_RESOURCES,
                                      &numFragmentInputs);
    if (numFragmentInputs <= 0)
        return;

    GLint maxNameLength = 0;
    mFunctions->getProgramInterfaceiv(mProgramID, GL_FRAGMENT_INPUT_NV, GL_MAX_NAME_LENGTH,
                                      &maxNameLength);
    ASSERT(maxNameLength);

    for (GLint i = 0; i < numFragmentInputs; ++i)
    {
        std::string name;
        name.resize(maxNameLength);

        GLsizei nameLen = 0;
        mFunctions->getProgramResourceName(mProgramID, GL_FRAGMENT_INPUT_NV, i, maxNameLength,
                                           &nameLen, &name[0]);
        name.resize(nameLen);

        // Ignore built-ins
        if (angle::BeginsWith(name, "gl_"))
            continue;

        const GLenum kQueryProperties[] = {GL_LOCATION, GL_ARRAY_SIZE};
        GLint queryResults[ArraySize(kQueryProperties)];
        GLsizei queryLength = 0;

        mFunctions->getProgramResourceiv(
            mProgramID, GL_FRAGMENT_INPUT_NV, i, static_cast<GLsizei>(ArraySize(kQueryProperties)),
            kQueryProperties, static_cast<GLsizei>(ArraySize(queryResults)), &queryLength,
            queryResults);

        ASSERT(queryLength == static_cast<GLsizei>(ArraySize(kQueryProperties)));

        PathRenderingFragmentInput baseElementInput;
        baseElementInput.name     = name;
        baseElementInput.location = queryResults[0];
        mPathRenderingFragmentInputs.push_back(std::move(baseElementInput));

        // If the input is an array it's denoted by [0] suffix on the variable
        // name. We'll then create an entry per each array index where index > 0
        if (angle::EndsWith(name, "[0]"))
        {
            // drop the suffix
            name.resize(name.size() - 3);

            const auto arraySize    = queryResults[1];
            const auto baseLocation = queryResults[0];

            for (GLint arrayIndex = 1; arrayIndex < arraySize; ++arrayIndex)
            {
                PathRenderingFragmentInput arrayElementInput;
                arrayElementInput.name     = name + "[" + ToString(arrayIndex) + "]";
                arrayElementInput.location = baseLocation + arrayIndex;
                mPathRenderingFragmentInputs.push_back(std::move(arrayElementInput));
            }
        }
    }
}

}  // namespace rx
