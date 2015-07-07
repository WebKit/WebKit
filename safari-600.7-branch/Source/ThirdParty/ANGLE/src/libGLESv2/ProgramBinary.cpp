#include "precompiled.h"
//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.cpp: Implements the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#include "libGLESv2/BinaryStream.h"
#include "libGLESv2/ProgramBinary.h"
#include "libGLESv2/renderer/ShaderExecutable.h"

#include "common/debug.h"
#include "common/version.h"
#include "common/utilities.h"

#include "libGLESv2/main.h"
#include "libGLESv2/Shader.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/VertexDataManager.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/Buffer.h"
#include "libGLESv2/DynamicHLSL.h"

#undef near
#undef far

namespace gl
{

namespace 
{

unsigned int ParseAndStripArrayIndex(std::string* name)
{
    unsigned int subscript = GL_INVALID_INDEX;

    // Strip any trailing array operator and retrieve the subscript
    size_t open = name->find_last_of('[');
    size_t close = name->find_last_of(']');
    if (open != std::string::npos && close == name->length() - 1)
    {
        subscript = atoi(name->substr(open + 1).c_str());
        name->erase(open);
    }

    return subscript;
}

void GetInputLayoutFromShader(const std::vector<gl::Attribute> &shaderAttributes, VertexFormat inputLayout[MAX_VERTEX_ATTRIBS])
{
    size_t layoutIndex = 0;
    for (size_t attributeIndex = 0; attributeIndex < shaderAttributes.size(); attributeIndex++)
    {
        ASSERT(layoutIndex < MAX_VERTEX_ATTRIBS);

        const gl::Attribute &shaderAttr = shaderAttributes[attributeIndex];

        if (shaderAttr.type != GL_NONE)
        {
            GLenum transposedType = TransposeMatrixType(shaderAttr.type);

            for (size_t rowIndex = 0; static_cast<int>(rowIndex) < VariableRowCount(transposedType); rowIndex++, layoutIndex++)
            {
                VertexFormat *defaultFormat = &inputLayout[layoutIndex];

                defaultFormat->mType = UniformComponentType(transposedType);
                defaultFormat->mNormalized = false;
                defaultFormat->mPureInteger = (defaultFormat->mType != GL_FLOAT); // note: inputs can not be bool
                defaultFormat->mComponents = VariableColumnCount(transposedType);
            }
        }
    }
}

}

VariableLocation::VariableLocation(const std::string &name, unsigned int element, unsigned int index) 
    : name(name), element(element), index(index)
{
}

ProgramBinary::VertexExecutable::VertexExecutable(rx::Renderer *const renderer,
                                                  const VertexFormat inputLayout[],
                                                  rx::ShaderExecutable *shaderExecutable)
    : mShaderExecutable(shaderExecutable)
{
    for (size_t attributeIndex = 0; attributeIndex < gl::MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        mInputs[attributeIndex] = inputLayout[attributeIndex];
    }
}

ProgramBinary::VertexExecutable::~VertexExecutable()
{
    delete mShaderExecutable;
}

bool ProgramBinary::VertexExecutable::matchesInputLayout(const VertexFormat attributes[]) const
{
    for (size_t attributeIndex = 0; attributeIndex < gl::MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        if (mInputs[attributeIndex] != attributes[attributeIndex])
        {
            return false;
        }
    }

    return true;
}

LinkedVarying::LinkedVarying()
{
}

LinkedVarying::LinkedVarying(const std::string &name, GLenum type, GLsizei size, const std::string &semanticName,
                             unsigned int semanticIndex, unsigned int semanticIndexCount)
    : name(name), type(type), size(size), semanticName(semanticName), semanticIndex(semanticIndex), semanticIndexCount(semanticIndexCount)
{
}

unsigned int ProgramBinary::mCurrentSerial = 1;

ProgramBinary::ProgramBinary(rx::Renderer *renderer)
    : RefCountObject(0),
      mRenderer(renderer),
      mDynamicHLSL(NULL),
      mVertexWorkarounds(rx::ANGLE_D3D_WORKAROUND_NONE),
      mPixelExecutable(NULL),
      mGeometryExecutable(NULL),
      mUsedVertexSamplerRange(0),
      mUsedPixelSamplerRange(0),
      mUsesPointSize(false),
      mShaderVersion(100),
      mVertexUniformStorage(NULL),
      mFragmentUniformStorage(NULL),
      mValidated(false),
      mSerial(issueSerial())
{
    for (int index = 0; index < MAX_VERTEX_ATTRIBS; index++)
    {
        mSemanticIndex[index] = -1;
    }

    for (int index = 0; index < MAX_TEXTURE_IMAGE_UNITS; index++)
    {
        mSamplersPS[index].active = false;
    }

    for (int index = 0; index < IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS; index++)
    {
        mSamplersVS[index].active = false;
    }

    mDynamicHLSL = new DynamicHLSL(renderer);
}

ProgramBinary::~ProgramBinary()
{
    while (!mVertexExecutables.empty())
    {
        delete mVertexExecutables.back();
        mVertexExecutables.pop_back();
    }

    SafeDelete(mGeometryExecutable);
    SafeDelete(mPixelExecutable);

    while (!mUniforms.empty())
    {
        delete mUniforms.back();
        mUniforms.pop_back();
    }

    while (!mUniformBlocks.empty())
    {
        delete mUniformBlocks.back();
        mUniformBlocks.pop_back();
    }

    SafeDelete(mVertexUniformStorage);
    SafeDelete(mFragmentUniformStorage);
    SafeDelete(mDynamicHLSL);
}

unsigned int ProgramBinary::getSerial() const
{
    return mSerial;
}

int ProgramBinary::getShaderVersion() const
{
    return mShaderVersion;
}

unsigned int ProgramBinary::issueSerial()
{
    return mCurrentSerial++;
}

rx::ShaderExecutable *ProgramBinary::getPixelExecutable() const
{
    return mPixelExecutable;
}

rx::ShaderExecutable *ProgramBinary::getVertexExecutableForInputLayout(const VertexFormat inputLayout[gl::MAX_VERTEX_ATTRIBS])
{
    for (size_t executableIndex = 0; executableIndex < mVertexExecutables.size(); executableIndex++)
    {
        if (mVertexExecutables[executableIndex]->matchesInputLayout(inputLayout))
        {
            return mVertexExecutables[executableIndex]->shaderExecutable();
        }
    }

    // Generate new dynamic layout with attribute conversions
    const std::string &layoutHLSL = mDynamicHLSL->generateInputLayoutHLSL(inputLayout, mShaderAttributes);

    // Generate new shader source by replacing the attributes stub with the defined input layout
    std::string vertexHLSL = mVertexHLSL;
    size_t insertPos = vertexHLSL.find(DynamicHLSL::VERTEX_ATTRIBUTE_STUB_STRING);
    vertexHLSL.replace(insertPos, DynamicHLSL::VERTEX_ATTRIBUTE_STUB_STRING.length(), layoutHLSL);

    // Generate new vertex executable
    InfoLog tempInfoLog;
    rx::ShaderExecutable *vertexExecutable = mRenderer->compileToExecutable(tempInfoLog, vertexHLSL.c_str(),
                                                                            rx::SHADER_VERTEX,
                                                                            mTransformFeedbackLinkedVaryings,
                                                                            (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS),
                                                                            mVertexWorkarounds);

    if (!vertexExecutable)
    {
        std::vector<char> tempCharBuffer(tempInfoLog.getLength()+3);
        tempInfoLog.getLog(tempInfoLog.getLength(), NULL, &tempCharBuffer[0]);
        ERR("Error compiling dynamic vertex executable:\n%s\n", &tempCharBuffer[0]);
    }
    else
    {
        mVertexExecutables.push_back(new VertexExecutable(mRenderer, inputLayout, vertexExecutable));
    }

    return vertexExecutable;
}

rx::ShaderExecutable *ProgramBinary::getGeometryExecutable() const
{
    return mGeometryExecutable;
}

GLuint ProgramBinary::getAttributeLocation(const char *name)
{
    if (name)
    {
        for (int index = 0; index < MAX_VERTEX_ATTRIBS; index++)
        {
            if (mLinkedAttribute[index].name == std::string(name))
            {
                return index;
            }
        }
    }

    return -1;
}

int ProgramBinary::getSemanticIndex(int attributeIndex)
{
    ASSERT(attributeIndex >= 0 && attributeIndex < MAX_VERTEX_ATTRIBS);
    
    return mSemanticIndex[attributeIndex];
}

// Returns one more than the highest sampler index used.
GLint ProgramBinary::getUsedSamplerRange(SamplerType type)
{
    switch (type)
    {
      case SAMPLER_PIXEL:
        return mUsedPixelSamplerRange;
      case SAMPLER_VERTEX:
        return mUsedVertexSamplerRange;
      default:
        UNREACHABLE();
        return 0;
    }
}

bool ProgramBinary::usesPointSize() const
{
    return mUsesPointSize;
}

bool ProgramBinary::usesPointSpriteEmulation() const
{
    return mUsesPointSize && mRenderer->getMajorShaderModel() >= 4;
}

bool ProgramBinary::usesGeometryShader() const
{
    return usesPointSpriteEmulation();
}

// Returns the index of the texture image unit (0-19) corresponding to a Direct3D 9 sampler
// index (0-15 for the pixel shader and 0-3 for the vertex shader).
GLint ProgramBinary::getSamplerMapping(SamplerType type, unsigned int samplerIndex)
{
    GLint logicalTextureUnit = -1;

    switch (type)
    {
      case SAMPLER_PIXEL:
        ASSERT(samplerIndex < sizeof(mSamplersPS)/sizeof(mSamplersPS[0]));

        if (mSamplersPS[samplerIndex].active)
        {
            logicalTextureUnit = mSamplersPS[samplerIndex].logicalTextureUnit;
        }
        break;
      case SAMPLER_VERTEX:
        ASSERT(samplerIndex < sizeof(mSamplersVS)/sizeof(mSamplersVS[0]));

        if (mSamplersVS[samplerIndex].active)
        {
            logicalTextureUnit = mSamplersVS[samplerIndex].logicalTextureUnit;
        }
        break;
      default: UNREACHABLE();
    }

    if (logicalTextureUnit >= 0 && logicalTextureUnit < (GLint)mRenderer->getMaxCombinedTextureImageUnits())
    {
        return logicalTextureUnit;
    }

    return -1;
}

// Returns the texture type for a given Direct3D 9 sampler type and
// index (0-15 for the pixel shader and 0-3 for the vertex shader).
TextureType ProgramBinary::getSamplerTextureType(SamplerType type, unsigned int samplerIndex)
{
    switch (type)
    {
      case SAMPLER_PIXEL:
        ASSERT(samplerIndex < sizeof(mSamplersPS)/sizeof(mSamplersPS[0]));
        ASSERT(mSamplersPS[samplerIndex].active);
        return mSamplersPS[samplerIndex].textureType;
      case SAMPLER_VERTEX:
        ASSERT(samplerIndex < sizeof(mSamplersVS)/sizeof(mSamplersVS[0]));
        ASSERT(mSamplersVS[samplerIndex].active);
        return mSamplersVS[samplerIndex].textureType;
      default: UNREACHABLE();
    }

    return TEXTURE_2D;
}

GLint ProgramBinary::getUniformLocation(std::string name)
{
    unsigned int subscript = ParseAndStripArrayIndex(&name);

    unsigned int numUniforms = mUniformIndex.size();
    for (unsigned int location = 0; location < numUniforms; location++)
    {
        if (mUniformIndex[location].name == name)
        {
            const int index = mUniformIndex[location].index;
            const bool isArray = mUniforms[index]->isArray();

            if ((isArray && mUniformIndex[location].element == subscript) || 
                (subscript == GL_INVALID_INDEX))
            {
                return location;
            }
        }
    }

    return -1;
}

GLuint ProgramBinary::getUniformIndex(std::string name)
{
    unsigned int subscript = ParseAndStripArrayIndex(&name);

    // The app is not allowed to specify array indices other than 0 for arrays of basic types
    if (subscript != 0 && subscript != GL_INVALID_INDEX)
    {
        return GL_INVALID_INDEX;
    }

    unsigned int numUniforms = mUniforms.size();
    for (unsigned int index = 0; index < numUniforms; index++)
    {
        if (mUniforms[index]->name == name)
        {
            if (mUniforms[index]->isArray() || subscript == GL_INVALID_INDEX)
            {
                return index;
            }
        }
    }

    return GL_INVALID_INDEX;
}

GLuint ProgramBinary::getUniformBlockIndex(std::string name)
{
    unsigned int subscript = ParseAndStripArrayIndex(&name);

    unsigned int numUniformBlocks = mUniformBlocks.size();
    for (unsigned int blockIndex = 0; blockIndex < numUniformBlocks; blockIndex++)
    {
        const UniformBlock &uniformBlock = *mUniformBlocks[blockIndex];
        if (uniformBlock.name == name)
        {
            const bool arrayElementZero = (subscript == GL_INVALID_INDEX && uniformBlock.elementIndex == 0);
            if (subscript == uniformBlock.elementIndex || arrayElementZero)
            {
                return blockIndex;
            }
        }
    }

    return GL_INVALID_INDEX;
}

UniformBlock *ProgramBinary::getUniformBlockByIndex(GLuint blockIndex)
{
    ASSERT(blockIndex < mUniformBlocks.size());
    return mUniformBlocks[blockIndex];
}

GLint ProgramBinary::getFragDataLocation(const char *name) const
{
    std::string baseName(name);
    unsigned int arrayIndex;
    arrayIndex = ParseAndStripArrayIndex(&baseName);

    for (auto locationIt = mOutputVariables.begin(); locationIt != mOutputVariables.end(); locationIt++)
    {
        const VariableLocation &outputVariable = locationIt->second;

        if (outputVariable.name == baseName && (arrayIndex == GL_INVALID_INDEX || arrayIndex == outputVariable.element))
        {
            return static_cast<GLint>(locationIt->first);
        }
    }

    return -1;
}

size_t ProgramBinary::getTransformFeedbackVaryingCount() const
{
    return mTransformFeedbackLinkedVaryings.size();
}

const LinkedVarying &ProgramBinary::getTransformFeedbackVarying(size_t idx) const
{
    return mTransformFeedbackLinkedVaryings[idx];
}

GLenum ProgramBinary::getTransformFeedbackBufferMode() const
{
    return mTransformFeedbackBufferMode;
}

template <typename T>
bool ProgramBinary::setUniform(GLint location, GLsizei count, const T* v, GLenum targetUniformType)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    const int components = UniformComponentCount(targetUniformType);
    const GLenum targetBoolType = UniformBoolVectorType(targetUniformType);

    LinkedUniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    int elementCount = targetUniform->elementCount();

    if (elementCount == 1 && count > 1)
        return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

    count = std::min(elementCount - (int)mUniformIndex[location].element, count);

    if (targetUniform->type == targetUniformType)
    {
        T *target = (T*)targetUniform->data + mUniformIndex[location].element * 4;

        for (int i = 0; i < count; i++)
        {
            for (int c = 0; c < components; c++)
            {
                target[c] = v[c];
            }
            for (int c = components; c < 4; c++)
            {
                target[c] = 0;
            }
            target += 4;
            v += components;
        }
    }
    else if (targetUniform->type == targetBoolType)
    {
        GLint *boolParams = (GLint*)targetUniform->data + mUniformIndex[location].element * 4;

        for (int i = 0; i < count; i++)
        {
            for (int c = 0; c < components; c++)
            {
                boolParams[c] = (v[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE;
            }
            for (int c = components; c < 4; c++)
            {
                boolParams[c] = GL_FALSE;
            }
            boolParams += 4;
            v += components;
        }
    }
    else
    {
        return false;
    }

    return true;
}

bool ProgramBinary::setUniform1fv(GLint location, GLsizei count, const GLfloat* v)
{
    return setUniform(location, count, v, GL_FLOAT);
}

bool ProgramBinary::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    return setUniform(location, count, v, GL_FLOAT_VEC2);
}

bool ProgramBinary::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    return setUniform(location, count, v, GL_FLOAT_VEC3);
}

bool ProgramBinary::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    return setUniform(location, count, v, GL_FLOAT_VEC4);
}

template<typename T>
void transposeMatrix(T *target, const GLfloat *value, int targetWidth, int targetHeight, int srcWidth, int srcHeight)
{
    int copyWidth = std::min(targetHeight, srcWidth);
    int copyHeight = std::min(targetWidth, srcHeight);

    for (int x = 0; x < copyWidth; x++)
    {
        for (int y = 0; y < copyHeight; y++)
        {
            target[x * targetWidth + y] = static_cast<T>(value[y * srcWidth + x]);
        }
    }
    // clear unfilled right side
    for (int y = 0; y < copyWidth; y++)
    {
        for (int x = copyHeight; x < targetWidth; x++)
        {
            target[y * targetWidth + x] = static_cast<T>(0);
        }
    }
    // clear unfilled bottom.
    for (int y = copyWidth; y < targetHeight; y++)
    {
        for (int x = 0; x < targetWidth; x++)
        {
            target[y * targetWidth + x] = static_cast<T>(0);
        }
    }
}

template<typename T>
void expandMatrix(T *target, const GLfloat *value, int targetWidth, int targetHeight, int srcWidth, int srcHeight)
{
    int copyWidth = std::min(targetWidth, srcWidth);
    int copyHeight = std::min(targetHeight, srcHeight);

    for (int y = 0; y < copyHeight; y++)
    {
        for (int x = 0; x < copyWidth; x++)
        {
            target[y * targetWidth + x] = static_cast<T>(value[y * srcWidth + x]);
        }
    }
    // clear unfilled right side
    for (int y = 0; y < copyHeight; y++)
    {
        for (int x = copyWidth; x < targetWidth; x++)
        {
            target[y * targetWidth + x] = static_cast<T>(0);
        }
    }
    // clear unfilled bottom.
    for (int y = copyHeight; y < targetHeight; y++)
    {
        for (int x = 0; x < targetWidth; x++)
        {
            target[y * targetWidth + x] = static_cast<T>(0);
        }
    }
}

template <int cols, int rows>
bool ProgramBinary::setUniformMatrixfv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, GLenum targetUniformType) 
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    LinkedUniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type != targetUniformType)
    {
        return false;
    }

    int elementCount = targetUniform->elementCount();

    if (elementCount == 1 && count > 1)
        return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

    count = std::min(elementCount - (int)mUniformIndex[location].element, count);
    const unsigned int targetMatrixStride = (4 * rows);
    GLfloat *target = (GLfloat*)(targetUniform->data + mUniformIndex[location].element * sizeof(GLfloat) * targetMatrixStride);

    for (int i = 0; i < count; i++)
    {
        // Internally store matrices as transposed versions to accomodate HLSL matrix indexing
        if (transpose == GL_FALSE)
        {
            transposeMatrix<GLfloat>(target, value, 4, rows, rows, cols);
        }
        else
        {
            expandMatrix<GLfloat>(target, value, 4, rows, cols, rows);
        }
        target += targetMatrixStride;
        value += cols * rows;
    }

    return true;
}

bool ProgramBinary::setUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    return setUniformMatrixfv<2, 2>(location, count, transpose, value, GL_FLOAT_MAT2);
}

bool ProgramBinary::setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    return setUniformMatrixfv<3, 3>(location, count, transpose, value, GL_FLOAT_MAT3);
}

bool ProgramBinary::setUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    return setUniformMatrixfv<4, 4>(location, count, transpose, value, GL_FLOAT_MAT4);
}

bool ProgramBinary::setUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    return setUniformMatrixfv<2, 3>(location, count, transpose, value, GL_FLOAT_MAT2x3);
}

bool ProgramBinary::setUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    return setUniformMatrixfv<3, 2>(location, count, transpose, value, GL_FLOAT_MAT3x2);
}

bool ProgramBinary::setUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    return setUniformMatrixfv<2, 4>(location, count, transpose, value, GL_FLOAT_MAT2x4);
}

bool ProgramBinary::setUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    return setUniformMatrixfv<4, 2>(location, count, transpose, value, GL_FLOAT_MAT4x2);
}

bool ProgramBinary::setUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    return setUniformMatrixfv<3, 4>(location, count, transpose, value, GL_FLOAT_MAT3x4);
}

bool ProgramBinary::setUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    return setUniformMatrixfv<4, 3>(location, count, transpose, value, GL_FLOAT_MAT4x3);
}

bool ProgramBinary::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    LinkedUniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    int elementCount = targetUniform->elementCount();

    if (elementCount == 1 && count > 1)
        return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

    count = std::min(elementCount - (int)mUniformIndex[location].element, count);

    if (targetUniform->type == GL_INT || IsSampler(targetUniform->type))
    {
        GLint *target = (GLint*)targetUniform->data + mUniformIndex[location].element * 4;

        for (int i = 0; i < count; i++)
        {
            target[0] = v[0];
            target[1] = 0;
            target[2] = 0;
            target[3] = 0;
            target += 4;
            v += 1;
        }
    }
    else if (targetUniform->type == GL_BOOL)
    {
        GLint *boolParams = (GLint*)targetUniform->data + mUniformIndex[location].element * 4;

        for (int i = 0; i < count; i++)
        {
            boolParams[0] = (v[0] == 0) ? GL_FALSE : GL_TRUE;
            boolParams[1] = GL_FALSE;
            boolParams[2] = GL_FALSE;
            boolParams[3] = GL_FALSE;
            boolParams += 4;
            v += 1;
        }
    }
    else
    {
        return false;
    }

    return true;
}

bool ProgramBinary::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    return setUniform(location, count, v, GL_INT_VEC2);
}

bool ProgramBinary::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    return setUniform(location, count, v, GL_INT_VEC3);
}

bool ProgramBinary::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    return setUniform(location, count, v, GL_INT_VEC4);
}

bool ProgramBinary::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    return setUniform(location, count, v, GL_UNSIGNED_INT);
}

bool ProgramBinary::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    return setUniform(location, count, v, GL_UNSIGNED_INT_VEC2);
}

bool ProgramBinary::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    return setUniform(location, count, v, GL_UNSIGNED_INT_VEC3);
}

bool ProgramBinary::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    return setUniform(location, count, v, GL_UNSIGNED_INT_VEC4);
}

template <typename T>
bool ProgramBinary::getUniformv(GLint location, GLsizei *bufSize, T *params, GLenum uniformType)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    LinkedUniform *targetUniform = mUniforms[mUniformIndex[location].index];

    // sized queries -- ensure the provided buffer is large enough
    if (bufSize)
    {
        int requiredBytes = UniformExternalSize(targetUniform->type);
        if (*bufSize < requiredBytes)
        {
            return false;
        }
    }

    if (IsMatrixType(targetUniform->type))
    {
        const int rows = VariableRowCount(targetUniform->type);
        const int cols = VariableColumnCount(targetUniform->type);
        transposeMatrix(params, (GLfloat*)targetUniform->data + mUniformIndex[location].element * 4 * rows, rows, cols, 4, rows);
    }
    else if (uniformType == UniformComponentType(targetUniform->type))
    {
        unsigned int size = UniformComponentCount(targetUniform->type);
        memcpy(params, targetUniform->data + mUniformIndex[location].element * 4 * sizeof(T),
                size * sizeof(T));
    }
    else
    {
        unsigned int size = UniformComponentCount(targetUniform->type);
        switch (UniformComponentType(targetUniform->type))
        {
          case GL_BOOL:
            {
                GLint *boolParams = (GLint*)targetUniform->data + mUniformIndex[location].element * 4;

                for (unsigned int i = 0; i < size; i++)
                {
                    params[i] = (boolParams[i] == GL_FALSE) ? static_cast<T>(0) : static_cast<T>(1);
                }
            }
            break;

          case GL_FLOAT:
            {
                GLfloat *floatParams = (GLfloat*)targetUniform->data + mUniformIndex[location].element * 4;

                for (unsigned int i = 0; i < size; i++)
                {
                    params[i] = static_cast<T>(floatParams[i]);
                }
            }
            break;

          case GL_INT:
            {
                GLint *intParams = (GLint*)targetUniform->data + mUniformIndex[location].element * 4;

                for (unsigned int i = 0; i < size; i++)
                {
                    params[i] = static_cast<T>(intParams[i]);
                }
            }
            break;
       
          case GL_UNSIGNED_INT:
            {
                GLuint *uintParams = (GLuint*)targetUniform->data + mUniformIndex[location].element * 4;

                for (unsigned int i = 0; i < size; i++)
                {
                    params[i] = static_cast<T>(uintParams[i]);
                }
            }
            break;
          
          default: UNREACHABLE();
        }
    }

    return true;
}

bool ProgramBinary::getUniformfv(GLint location, GLsizei *bufSize, GLfloat *params)
{
    return getUniformv(location, bufSize, params, GL_FLOAT);
}

bool ProgramBinary::getUniformiv(GLint location, GLsizei *bufSize, GLint *params)
{
    return getUniformv(location, bufSize, params, GL_INT);
}

bool ProgramBinary::getUniformuiv(GLint location, GLsizei *bufSize, GLuint *params)
{
    return getUniformv(location, bufSize, params, GL_UNSIGNED_INT);
}

void ProgramBinary::dirtyAllUniforms()
{
    unsigned int numUniforms = mUniforms.size();
    for (unsigned int index = 0; index < numUniforms; index++)
    {
        mUniforms[index]->dirty = true;
    }
}

// Applies all the uniforms set for this program object to the renderer
void ProgramBinary::applyUniforms()
{
    // Retrieve sampler uniform values
    for (size_t uniformIndex = 0; uniformIndex < mUniforms.size(); uniformIndex++)
    {
        LinkedUniform *targetUniform = mUniforms[uniformIndex];

        if (targetUniform->dirty)
        {
            if (IsSampler(targetUniform->type))
            {
                int count = targetUniform->elementCount();
                GLint (*v)[4] = (GLint(*)[4])targetUniform->data;

                if (targetUniform->isReferencedByFragmentShader())
                {
                    unsigned int firstIndex = targetUniform->psRegisterIndex;

                    for (int i = 0; i < count; i++)
                    {
                        unsigned int samplerIndex = firstIndex + i;

                        if (samplerIndex < MAX_TEXTURE_IMAGE_UNITS)
                        {
                            ASSERT(mSamplersPS[samplerIndex].active);
                            mSamplersPS[samplerIndex].logicalTextureUnit = v[i][0];
                        }
                    }
                }

                if (targetUniform->isReferencedByVertexShader())
                {
                    unsigned int firstIndex = targetUniform->vsRegisterIndex;

                    for (int i = 0; i < count; i++)
                    {
                        unsigned int samplerIndex = firstIndex + i;

                        if (samplerIndex < IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS)
                        {
                            ASSERT(mSamplersVS[samplerIndex].active);
                            mSamplersVS[samplerIndex].logicalTextureUnit = v[i][0];
                        }
                    }
                }
            }
        }
    }

    mRenderer->applyUniforms(*this);

    for (size_t uniformIndex = 0; uniformIndex < mUniforms.size(); uniformIndex++)
    {
        mUniforms[uniformIndex]->dirty = false;
    }
}

bool ProgramBinary::applyUniformBuffers(const std::vector<gl::Buffer*> boundBuffers)
{
    const gl::Buffer *vertexUniformBuffers[gl::IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS] = {NULL};
    const gl::Buffer *fragmentUniformBuffers[gl::IMPLEMENTATION_MAX_FRAGMENT_SHADER_UNIFORM_BUFFERS] = {NULL};

    const unsigned int reservedBuffersInVS = mRenderer->getReservedVertexUniformBuffers();
    const unsigned int reservedBuffersInFS = mRenderer->getReservedFragmentUniformBuffers();

    ASSERT(boundBuffers.size() == mUniformBlocks.size());

    for (unsigned int uniformBlockIndex = 0; uniformBlockIndex < mUniformBlocks.size(); uniformBlockIndex++)
    {
        gl::UniformBlock *uniformBlock = getUniformBlockByIndex(uniformBlockIndex);
        gl::Buffer *uniformBuffer = boundBuffers[uniformBlockIndex];

        ASSERT(uniformBlock && uniformBuffer);

        if (uniformBuffer->size() < uniformBlock->dataSize)
        {
            // undefined behaviour
            return false;
        }

        ASSERT(uniformBlock->isReferencedByVertexShader() || uniformBlock->isReferencedByFragmentShader());

        if (uniformBlock->isReferencedByVertexShader())
        {
            unsigned int registerIndex = uniformBlock->vsRegisterIndex - reservedBuffersInVS;
            ASSERT(vertexUniformBuffers[registerIndex] == NULL);
            ASSERT(registerIndex < mRenderer->getMaxVertexShaderUniformBuffers());
            vertexUniformBuffers[registerIndex] = uniformBuffer;
        }

        if (uniformBlock->isReferencedByFragmentShader())
        {
            unsigned int registerIndex = uniformBlock->psRegisterIndex - reservedBuffersInFS;
            ASSERT(fragmentUniformBuffers[registerIndex] == NULL);
            ASSERT(registerIndex < mRenderer->getMaxFragmentShaderUniformBuffers());
            fragmentUniformBuffers[registerIndex] = uniformBuffer;
        }
    }

    return mRenderer->setUniformBuffers(vertexUniformBuffers, fragmentUniformBuffers);
}

bool ProgramBinary::linkVaryings(InfoLog &infoLog, FragmentShader *fragmentShader, VertexShader *vertexShader)
{
    std::vector<gl::Varying> &fragmentVaryings = fragmentShader->getVaryings();
    std::vector<gl::Varying> &vertexVaryings = vertexShader->getVaryings();

    for (size_t fragVaryingIndex = 0; fragVaryingIndex < fragmentVaryings.size(); fragVaryingIndex++)
    {
        gl::Varying *input = &fragmentVaryings[fragVaryingIndex];
        bool matched = false;

        for (size_t vertVaryingIndex = 0; vertVaryingIndex < vertexVaryings.size(); vertVaryingIndex++)
        {
            gl::Varying *output = &vertexVaryings[vertVaryingIndex];
            if (output->name == input->name)
            {
                if (!linkValidateVariables(infoLog, output->name, *input, *output))
                {
                    return false;
                }

                output->registerIndex = input->registerIndex;
                output->elementIndex = input->elementIndex;

                matched = true;
                break;
            }
        }

        if (!matched)
        {
            infoLog.append("Fragment varying %s does not match any vertex varying", input->name.c_str());
            return false;
        }
    }

    return true;
}

bool ProgramBinary::load(InfoLog &infoLog, const void *binary, GLsizei length)
{
    BinaryInputStream stream(binary, length);

    int format = 0;
    stream.read(&format);
    if (format != GL_PROGRAM_BINARY_ANGLE)
    {
        infoLog.append("Invalid program binary format.");
        return false;
    }

    int majorVersion = 0;
    int minorVersion = 0;
    stream.read(&majorVersion);
    stream.read(&minorVersion);
    if (majorVersion != ANGLE_MAJOR_VERSION || minorVersion != ANGLE_MINOR_VERSION)
    {
        infoLog.append("Invalid program binary version.");
        return false;
    }

    unsigned char commitString[ANGLE_COMMIT_HASH_SIZE];
    stream.read(commitString, ANGLE_COMMIT_HASH_SIZE);
    if (memcmp(commitString, ANGLE_COMMIT_HASH, sizeof(unsigned char) * ANGLE_COMMIT_HASH_SIZE) != 0)
    {
        infoLog.append("Invalid program binary version.");
        return false;
    }

    int compileFlags = 0;
    stream.read(&compileFlags);
    if (compileFlags != ANGLE_COMPILE_OPTIMIZATION_LEVEL)
    {
        infoLog.append("Mismatched compilation flags.");
        return false;
    }

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
    {
        stream.read(&mLinkedAttribute[i].type);
        std::string name;
        stream.read(&name);
        mLinkedAttribute[i].name = name;
        stream.read(&mShaderAttributes[i].type);
        stream.read(&mShaderAttributes[i].name);
        stream.read(&mSemanticIndex[i]);
    }

    initAttributesByLayout();

    for (unsigned int i = 0; i < MAX_TEXTURE_IMAGE_UNITS; ++i)
    {
        stream.read(&mSamplersPS[i].active);
        stream.read(&mSamplersPS[i].logicalTextureUnit);
        
        int textureType;
        stream.read(&textureType);
        mSamplersPS[i].textureType = (TextureType) textureType;
    }

    for (unsigned int i = 0; i < IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS; ++i)
    {
        stream.read(&mSamplersVS[i].active);
        stream.read(&mSamplersVS[i].logicalTextureUnit);
        
        int textureType;
        stream.read(&textureType);
        mSamplersVS[i].textureType = (TextureType) textureType;
    }

    stream.read(&mUsedVertexSamplerRange);
    stream.read(&mUsedPixelSamplerRange);
    stream.read(&mUsesPointSize);
    stream.read(&mShaderVersion);

    size_t size;
    stream.read(&size);
    if (stream.error())
    {
        infoLog.append("Invalid program binary.");
        return false;
    }

    mUniforms.resize(size);
    for (unsigned int i = 0; i < size; ++i)
    {
        GLenum type;
        GLenum precision;
        std::string name;
        unsigned int arraySize;
        int blockIndex;

        stream.read(&type);
        stream.read(&precision);
        stream.read(&name);
        stream.read(&arraySize);
        stream.read(&blockIndex);

        int offset;
        int arrayStride;
        int matrixStride;
        bool isRowMajorMatrix;

        stream.read(&offset);
        stream.read(&arrayStride);
        stream.read(&matrixStride);
        stream.read(&isRowMajorMatrix);

        const gl::BlockMemberInfo blockInfo(offset, arrayStride, matrixStride, isRowMajorMatrix);

        mUniforms[i] = new LinkedUniform(type, precision, name, arraySize, blockIndex, blockInfo);
        
        stream.read(&mUniforms[i]->psRegisterIndex);
        stream.read(&mUniforms[i]->vsRegisterIndex);
        stream.read(&mUniforms[i]->registerCount);
        stream.read(&mUniforms[i]->registerElement);
    }

    stream.read(&size);
    if (stream.error())
    {
        infoLog.append("Invalid program binary.");
        return false;
    }

    mUniformBlocks.resize(size);
    for (unsigned int uniformBlockIndex = 0; uniformBlockIndex < size; ++uniformBlockIndex)
    {
        std::string name;
        unsigned int elementIndex;
        unsigned int dataSize;

        stream.read(&name);
        stream.read(&elementIndex);
        stream.read(&dataSize);

        mUniformBlocks[uniformBlockIndex] = new UniformBlock(name, elementIndex, dataSize);

        UniformBlock& uniformBlock = *mUniformBlocks[uniformBlockIndex];
        stream.read(&uniformBlock.psRegisterIndex);
        stream.read(&uniformBlock.vsRegisterIndex);

        size_t numMembers;
        stream.read(&numMembers);
        uniformBlock.memberUniformIndexes.resize(numMembers);
        for (unsigned int blockMemberIndex = 0; blockMemberIndex < numMembers; blockMemberIndex++)
        {
            stream.read(&uniformBlock.memberUniformIndexes[blockMemberIndex]);
        }
    }

    stream.read(&size);
    if (stream.error())
    {
        infoLog.append("Invalid program binary.");
        return false;
    }

    mUniformIndex.resize(size);
    for (unsigned int i = 0; i < size; ++i)
    {
        stream.read(&mUniformIndex[i].name);
        stream.read(&mUniformIndex[i].element);
        stream.read(&mUniformIndex[i].index);
    }

    stream.read(&mTransformFeedbackBufferMode);
    stream.read(&size);
    mTransformFeedbackLinkedVaryings.resize(size);
    for (size_t i = 0; i < mTransformFeedbackLinkedVaryings.size(); i++)
    {
        LinkedVarying &varying = mTransformFeedbackLinkedVaryings[i];

        stream.read(&varying.name);
        stream.read(&varying.type);
        stream.read(&varying.size);
        stream.read(&varying.semanticName);
        stream.read(&varying.semanticIndex);
        stream.read(&varying.semanticIndexCount);
    }

    stream.read(&mVertexHLSL);
    stream.read(&mVertexWorkarounds);

    unsigned int vertexShaderCount;
    stream.read(&vertexShaderCount);

    for (unsigned int vertexShaderIndex = 0; vertexShaderIndex < vertexShaderCount; vertexShaderIndex++)
    {
        VertexFormat vertexInputs[gl::MAX_VERTEX_ATTRIBS];

        for (size_t inputIndex = 0; inputIndex < gl::MAX_VERTEX_ATTRIBS; inputIndex++)
        {
            VertexFormat *vertexInput = &vertexInputs[inputIndex];
            stream.read(&vertexInput->mType);
            stream.read(&vertexInput->mNormalized);
            stream.read(&vertexInput->mComponents);
            stream.read(&vertexInput->mPureInteger);
        }

        unsigned int vertexShaderSize;
        stream.read(&vertexShaderSize);

        const char *vertexShaderFunction = (const char*) binary + stream.offset();

        rx::ShaderExecutable *shaderExecutable = mRenderer->loadExecutable(reinterpret_cast<const DWORD*>(vertexShaderFunction),
                                                                           vertexShaderSize, rx::SHADER_VERTEX,
                                                                           mTransformFeedbackLinkedVaryings,
                                                                           (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS));
        if (!shaderExecutable)
        {
            infoLog.append("Could not create vertex shader.");
            return false;
        }

        mVertexExecutables.push_back(new VertexExecutable(mRenderer, vertexInputs, shaderExecutable));

        stream.skip(vertexShaderSize);
    }

    unsigned int pixelShaderSize;
    stream.read(&pixelShaderSize);

    const char *pixelShaderFunction = (const char*) binary + stream.offset();
    mPixelExecutable = mRenderer->loadExecutable(reinterpret_cast<const DWORD*>(pixelShaderFunction),
                                                 pixelShaderSize, rx::SHADER_PIXEL, mTransformFeedbackLinkedVaryings,
                                                 (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS));
    if (!mPixelExecutable)
    {
        infoLog.append("Could not create pixel shader.");
        return false;
    }
    stream.skip(pixelShaderSize);

    unsigned int geometryShaderSize;
    stream.read(&geometryShaderSize);

    if (geometryShaderSize > 0)
    {
        const char *geometryShaderFunction = (const char*) binary + stream.offset();
        mGeometryExecutable = mRenderer->loadExecutable(reinterpret_cast<const DWORD*>(geometryShaderFunction),
                                                        geometryShaderSize, rx::SHADER_GEOMETRY, mTransformFeedbackLinkedVaryings,
                                                        (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS));
        if (!mGeometryExecutable)
        {
            infoLog.append("Could not create geometry shader.");
            SafeDelete(mPixelExecutable);
            return false;
        }
        stream.skip(geometryShaderSize);
    }

    const char *ptr = (const char*) binary + stream.offset();

    const GUID *binaryIdentifier = (const GUID *) ptr;
    ptr += sizeof(GUID);

    GUID identifier = mRenderer->getAdapterIdentifier();
    if (memcmp(&identifier, binaryIdentifier, sizeof(GUID)) != 0)
    {
        infoLog.append("Invalid program binary.");
        return false;
    }

    initializeUniformStorage();

    return true;
}

bool ProgramBinary::save(void* binary, GLsizei bufSize, GLsizei *length)
{
    BinaryOutputStream stream;

    stream.write(GL_PROGRAM_BINARY_ANGLE);
    stream.write(ANGLE_MAJOR_VERSION);
    stream.write(ANGLE_MINOR_VERSION);
    stream.write(ANGLE_COMMIT_HASH, ANGLE_COMMIT_HASH_SIZE);
    stream.write(ANGLE_COMPILE_OPTIMIZATION_LEVEL);

    for (unsigned int i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
    {
        stream.write(mLinkedAttribute[i].type);
        stream.write(mLinkedAttribute[i].name);
        stream.write(mShaderAttributes[i].type);
        stream.write(mShaderAttributes[i].name);
        stream.write(mSemanticIndex[i]);
    }

    for (unsigned int i = 0; i < MAX_TEXTURE_IMAGE_UNITS; ++i)
    {
        stream.write(mSamplersPS[i].active);
        stream.write(mSamplersPS[i].logicalTextureUnit);
        stream.write((int) mSamplersPS[i].textureType);
    }

    for (unsigned int i = 0; i < IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS; ++i)
    {
        stream.write(mSamplersVS[i].active);
        stream.write(mSamplersVS[i].logicalTextureUnit);
        stream.write((int) mSamplersVS[i].textureType);
    }

    stream.write(mUsedVertexSamplerRange);
    stream.write(mUsedPixelSamplerRange);
    stream.write(mUsesPointSize);
    stream.write(mShaderVersion);

    stream.write(mUniforms.size());
    for (size_t uniformIndex = 0; uniformIndex < mUniforms.size(); ++uniformIndex)
    {
        const LinkedUniform &uniform = *mUniforms[uniformIndex];

        stream.write(uniform.type);
        stream.write(uniform.precision);
        stream.write(uniform.name);
        stream.write(uniform.arraySize);
        stream.write(uniform.blockIndex);

        stream.write(uniform.blockInfo.offset);
        stream.write(uniform.blockInfo.arrayStride);
        stream.write(uniform.blockInfo.matrixStride);
        stream.write(uniform.blockInfo.isRowMajorMatrix);

        stream.write(uniform.psRegisterIndex);
        stream.write(uniform.vsRegisterIndex);
        stream.write(uniform.registerCount);
        stream.write(uniform.registerElement);
    }

    stream.write(mUniformBlocks.size());
    for (size_t uniformBlockIndex = 0; uniformBlockIndex < mUniformBlocks.size(); ++uniformBlockIndex)
    {
        const UniformBlock& uniformBlock = *mUniformBlocks[uniformBlockIndex];

        stream.write(uniformBlock.name);
        stream.write(uniformBlock.elementIndex);
        stream.write(uniformBlock.dataSize);

        stream.write(uniformBlock.memberUniformIndexes.size());
        for (unsigned int blockMemberIndex = 0; blockMemberIndex < uniformBlock.memberUniformIndexes.size(); blockMemberIndex++)
        {
            stream.write(uniformBlock.memberUniformIndexes[blockMemberIndex]);
        }

        stream.write(uniformBlock.psRegisterIndex);
        stream.write(uniformBlock.vsRegisterIndex);
    }

    stream.write(mUniformIndex.size());
    for (size_t i = 0; i < mUniformIndex.size(); ++i)
    {
        stream.write(mUniformIndex[i].name);
        stream.write(mUniformIndex[i].element);
        stream.write(mUniformIndex[i].index);
    }

    stream.write(mTransformFeedbackBufferMode);
    stream.write(mTransformFeedbackLinkedVaryings.size());
    for (size_t i = 0; i < mTransformFeedbackLinkedVaryings.size(); i++)
    {
        const LinkedVarying &varying = mTransformFeedbackLinkedVaryings[i];

        stream.write(varying.name);
        stream.write(varying.type);
        stream.write(varying.size);
        stream.write(varying.semanticName);
        stream.write(varying.semanticIndex);
        stream.write(varying.semanticIndexCount);
    }

    stream.write(mVertexHLSL);
    stream.write(mVertexWorkarounds);

    stream.write(mVertexExecutables.size());
    for (size_t vertexExecutableIndex = 0; vertexExecutableIndex < mVertexExecutables.size(); vertexExecutableIndex++)
    {
        VertexExecutable *vertexExecutable = mVertexExecutables[vertexExecutableIndex];

        for (size_t inputIndex = 0; inputIndex < gl::MAX_VERTEX_ATTRIBS; inputIndex++)
        {
            const VertexFormat &vertexInput = vertexExecutable->inputs()[inputIndex];
            stream.write(vertexInput.mType);
            stream.write(vertexInput.mNormalized);
            stream.write(vertexInput.mComponents);
            stream.write(vertexInput.mPureInteger);
        }

        UINT vertexShaderSize = vertexExecutable->shaderExecutable()->getLength();
        stream.write(vertexShaderSize);

        unsigned char *vertexBlob = static_cast<unsigned char *>(vertexExecutable->shaderExecutable()->getFunction());
        stream.write(vertexBlob, vertexShaderSize);
    }

    UINT pixelShaderSize = mPixelExecutable->getLength();
    stream.write(pixelShaderSize);

    unsigned char *pixelBlob = static_cast<unsigned char *>(mPixelExecutable->getFunction());
    stream.write(pixelBlob, pixelShaderSize);

    UINT geometryShaderSize = (mGeometryExecutable != NULL) ? mGeometryExecutable->getLength() : 0;
    stream.write(geometryShaderSize);

    if (mGeometryExecutable != NULL && geometryShaderSize > 0)
    {
        unsigned char *geometryBlob = static_cast<unsigned char *>(mGeometryExecutable->getFunction());
        stream.write(geometryBlob, geometryShaderSize);
    }

    GUID identifier = mRenderer->getAdapterIdentifier();

    GLsizei streamLength = stream.length();
    const void *streamData = stream.data();

    GLsizei totalLength = streamLength + sizeof(GUID);
    if (totalLength > bufSize)
    {
        if (length)
        {
            *length = 0;
        }

        return false;
    }

    if (binary)
    {
        char *ptr = (char*) binary;

        memcpy(ptr, streamData, streamLength);
        ptr += streamLength;

        memcpy(ptr, &identifier, sizeof(GUID));
        ptr += sizeof(GUID);

        ASSERT(ptr - totalLength == binary);
    }

    if (length)
    {
        *length = totalLength;
    }

    return true;
}

GLint ProgramBinary::getLength()
{
    GLint length;
    if (save(NULL, INT_MAX, &length))
    {
        return length;
    }
    else
    {
        return 0;
    }
}

bool ProgramBinary::link(InfoLog &infoLog, const AttributeBindings &attributeBindings, FragmentShader *fragmentShader, VertexShader *vertexShader,
                         const std::vector<std::string>& transformFeedbackVaryings, GLenum transformFeedbackBufferMode)
{
    if (!fragmentShader || !fragmentShader->isCompiled())
    {
        return false;
    }

    if (!vertexShader || !vertexShader->isCompiled())
    {
        return false;
    }

    mTransformFeedbackLinkedVaryings.clear();
    mTransformFeedbackBufferMode = transformFeedbackBufferMode;

    mShaderVersion = vertexShader->getShaderVersion();

    std::string pixelHLSL = fragmentShader->getHLSL();
    mVertexHLSL = vertexShader->getHLSL();
    mVertexWorkarounds = vertexShader->getD3DWorkarounds();

    // Map the varyings to the register file
    const gl::ShaderVariable *packing[IMPLEMENTATION_MAX_VARYING_VECTORS][4] = {NULL};
    int registers = mDynamicHLSL->packVaryings(infoLog, packing, fragmentShader, vertexShader, transformFeedbackVaryings);

    if (registers < 0)
    {
        return false;
    }

    if (!linkVaryings(infoLog, fragmentShader, vertexShader))
    {
        return false;
    }

    mUsesPointSize = vertexShader->usesPointSize();
    std::vector<LinkedVarying> linkedVaryings;
    if (!mDynamicHLSL->generateShaderLinkHLSL(infoLog, registers, packing, pixelHLSL, mVertexHLSL,
                                              fragmentShader, vertexShader, transformFeedbackVaryings,
                                              &linkedVaryings, &mOutputVariables))
    {
        return false;
    }

    bool success = true;

    if (!linkAttributes(infoLog, attributeBindings, fragmentShader, vertexShader))
    {
        success = false;
    }

    if (!linkUniforms(infoLog, vertexShader->getUniforms(), fragmentShader->getUniforms()))
    {
        success = false;
    }

    // special case for gl_DepthRange, the only built-in uniform (also a struct)
    if (vertexShader->usesDepthRange() || fragmentShader->usesDepthRange())
    {
        mUniforms.push_back(new LinkedUniform(GL_FLOAT, GL_HIGH_FLOAT, "gl_DepthRange.near", 0, -1, gl::BlockMemberInfo::defaultBlockInfo));
        mUniforms.push_back(new LinkedUniform(GL_FLOAT, GL_HIGH_FLOAT, "gl_DepthRange.far", 0, -1, gl::BlockMemberInfo::defaultBlockInfo));
        mUniforms.push_back(new LinkedUniform(GL_FLOAT, GL_HIGH_FLOAT, "gl_DepthRange.diff", 0, -1, gl::BlockMemberInfo::defaultBlockInfo));
    }

    if (!linkUniformBlocks(infoLog, vertexShader->getInterfaceBlocks(), fragmentShader->getInterfaceBlocks()))
    {
        success = false;
    }

    if (!gatherTransformFeedbackLinkedVaryings(infoLog, linkedVaryings, transformFeedbackVaryings,
                                               transformFeedbackBufferMode, &mTransformFeedbackLinkedVaryings))
    {
        success = false;
    }

    if (success)
    {
        VertexFormat defaultInputLayout[MAX_VERTEX_ATTRIBS];
        GetInputLayoutFromShader(vertexShader->activeAttributes(), defaultInputLayout);

        rx::ShaderExecutable *defaultVertexExecutable = getVertexExecutableForInputLayout(defaultInputLayout);
        mPixelExecutable = mRenderer->compileToExecutable(infoLog, pixelHLSL.c_str(), rx::SHADER_PIXEL,
                                                          mTransformFeedbackLinkedVaryings,
                                                          (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS),
                                                          fragmentShader->getD3DWorkarounds());

        if (usesGeometryShader())
        {
            std::string geometryHLSL = mDynamicHLSL->generateGeometryShaderHLSL(registers, packing, fragmentShader, vertexShader);
            mGeometryExecutable = mRenderer->compileToExecutable(infoLog, geometryHLSL.c_str(), rx::SHADER_GEOMETRY,
                                                                 mTransformFeedbackLinkedVaryings,
                                                                 (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS),
                                                                 rx::ANGLE_D3D_WORKAROUND_NONE);
        }

        if (!defaultVertexExecutable || !mPixelExecutable || (usesGeometryShader() && !mGeometryExecutable))
        {
            infoLog.append("Failed to create D3D shaders.");
            success = false;

            while (!mVertexExecutables.empty())
            {
                delete mVertexExecutables.back();
                mVertexExecutables.pop_back();
            }

            SafeDelete(mGeometryExecutable);
            SafeDelete(mPixelExecutable);

            mTransformFeedbackLinkedVaryings.clear();
        }
    }

    return success;
}

// Determines the mapping between GL attributes and Direct3D 9 vertex stream usage indices
bool ProgramBinary::linkAttributes(InfoLog &infoLog, const AttributeBindings &attributeBindings, FragmentShader *fragmentShader, VertexShader *vertexShader)
{
    unsigned int usedLocations = 0;
    const std::vector<gl::Attribute> &activeAttributes = vertexShader->activeAttributes();

    // Link attributes that have a binding location
    for (unsigned int attributeIndex = 0; attributeIndex < activeAttributes.size(); attributeIndex++)
    {
        const gl::Attribute &attribute = activeAttributes[attributeIndex];
        const int location = attribute.location == -1 ? attributeBindings.getAttributeBinding(attribute.name) : attribute.location;

        mShaderAttributes[attributeIndex] = attribute;

        if (location != -1)   // Set by glBindAttribLocation or by location layout qualifier
        {
            const int rows = AttributeRegisterCount(attribute.type);

            if (rows + location > MAX_VERTEX_ATTRIBS)
            {
                infoLog.append("Active attribute (%s) at location %d is too big to fit", attribute.name.c_str(), location);

                return false;
            }

            for (int row = 0; row < rows; row++)
            {
                const int rowLocation = location + row;
                gl::ShaderVariable &linkedAttribute = mLinkedAttribute[rowLocation];

                // In GLSL 3.00, attribute aliasing produces a link error
                // In GLSL 1.00, attribute aliasing is allowed
                if (mShaderVersion >= 300)
                {
                    if (!linkedAttribute.name.empty())
                    {
                        infoLog.append("Attribute '%s' aliases attribute '%s' at location %d", attribute.name.c_str(), linkedAttribute.name.c_str(), rowLocation);
                        return false;
                    }
                }

                linkedAttribute = attribute;
                usedLocations |= 1 << rowLocation;
            }
        }
    }

    // Link attributes that don't have a binding location
    for (unsigned int attributeIndex = 0; attributeIndex < activeAttributes.size(); attributeIndex++)
    {
        const gl::Attribute &attribute = activeAttributes[attributeIndex];
        const int location = attribute.location == -1 ? attributeBindings.getAttributeBinding(attribute.name) : attribute.location;

        if (location == -1)   // Not set by glBindAttribLocation or by location layout qualifier
        {
            int rows = AttributeRegisterCount(attribute.type);
            int availableIndex = AllocateFirstFreeBits(&usedLocations, rows, MAX_VERTEX_ATTRIBS);

            if (availableIndex == -1 || availableIndex + rows > MAX_VERTEX_ATTRIBS)
            {
                infoLog.append("Too many active attributes (%s)", attribute.name.c_str());

                return false;   // Fail to link
            }

            mLinkedAttribute[availableIndex] = attribute;
        }
    }

    for (int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; )
    {
        int index = vertexShader->getSemanticIndex(mLinkedAttribute[attributeIndex].name);
        int rows = AttributeRegisterCount(mLinkedAttribute[attributeIndex].type);

        for (int r = 0; r < rows; r++)
        {
            mSemanticIndex[attributeIndex++] = index++;
        }
    }

    initAttributesByLayout();

    return true;
}

bool ProgramBinary::linkValidateVariablesBase(InfoLog &infoLog, const std::string &variableName, const gl::ShaderVariable &vertexVariable, const gl::ShaderVariable &fragmentVariable, bool validatePrecision)
{
    if (vertexVariable.type != fragmentVariable.type)
    {
        infoLog.append("Types for %s differ between vertex and fragment shaders", variableName.c_str());
        return false;
    }
    if (vertexVariable.arraySize != fragmentVariable.arraySize)
    {
        infoLog.append("Array sizes for %s differ between vertex and fragment shaders", variableName.c_str());
        return false;
    }
    if (validatePrecision && vertexVariable.precision != fragmentVariable.precision)
    {
        infoLog.append("Precisions for %s differ between vertex and fragment shaders", variableName.c_str());
        return false;
    }

    return true;
}

template <class ShaderVarType>
bool ProgramBinary::linkValidateFields(InfoLog &infoLog, const std::string &varName, const ShaderVarType &vertexVar, const ShaderVarType &fragmentVar)
{
    if (vertexVar.fields.size() != fragmentVar.fields.size())
    {
        infoLog.append("Structure lengths for %s differ between vertex and fragment shaders", varName.c_str());
        return false;
    }
    const unsigned int numMembers = vertexVar.fields.size();
    for (unsigned int memberIndex = 0; memberIndex < numMembers; memberIndex++)
    {
        const ShaderVarType &vertexMember = vertexVar.fields[memberIndex];
        const ShaderVarType &fragmentMember = fragmentVar.fields[memberIndex];

        if (vertexMember.name != fragmentMember.name)
        {
            infoLog.append("Name mismatch for field '%d' of %s: (in vertex: '%s', in fragment: '%s')",
                           memberIndex, varName.c_str(), vertexMember.name.c_str(), fragmentMember.name.c_str());
            return false;
        }

        const std::string memberName = varName.substr(0, varName.length()-1) + "." + vertexVar.name + "'";
        if (!linkValidateVariables(infoLog, memberName, vertexMember, fragmentMember))
        {
            return false;
        }
    }

    return true;
}

bool ProgramBinary::linkValidateVariables(InfoLog &infoLog, const std::string &uniformName, const gl::Uniform &vertexUniform, const gl::Uniform &fragmentUniform)
{
    if (!linkValidateVariablesBase(infoLog, uniformName, vertexUniform, fragmentUniform, true))
    {
        return false;
    }

    if (!linkValidateFields<gl::Uniform>(infoLog, uniformName, vertexUniform, fragmentUniform))
    {
        return false;
    }

    return true;
}

bool ProgramBinary::linkValidateVariables(InfoLog &infoLog, const std::string &varyingName, const gl::Varying &vertexVarying, const gl::Varying &fragmentVarying)
{
    if (!linkValidateVariablesBase(infoLog, varyingName, vertexVarying, fragmentVarying, false))
    {
        return false;
    }

    if (vertexVarying.interpolation != fragmentVarying.interpolation)
    {
        infoLog.append("Interpolation types for %s differ between vertex and fragment shaders", varyingName.c_str());
        return false;
    }

    if (!linkValidateFields<gl::Varying>(infoLog, varyingName, vertexVarying, fragmentVarying))
    {
        return false;
    }

    return true;
}

bool ProgramBinary::linkValidateVariables(InfoLog &infoLog, const std::string &uniformName, const gl::InterfaceBlockField &vertexUniform, const gl::InterfaceBlockField &fragmentUniform)
{
    if (!linkValidateVariablesBase(infoLog, uniformName, vertexUniform, fragmentUniform, true))
    {
        return false;
    }

    if (vertexUniform.isRowMajorMatrix != fragmentUniform.isRowMajorMatrix)
    {
        infoLog.append("Matrix packings for %s differ between vertex and fragment shaders", uniformName.c_str());
        return false;
    }

    if (!linkValidateFields<gl::InterfaceBlockField>(infoLog, uniformName, vertexUniform, fragmentUniform))
    {
        return false;
    }

    return true;
}

bool ProgramBinary::linkUniforms(InfoLog &infoLog, const std::vector<gl::Uniform> &vertexUniforms, const std::vector<gl::Uniform> &fragmentUniforms)
{
    // Check that uniforms defined in the vertex and fragment shaders are identical
    typedef std::map<std::string, const gl::Uniform*> UniformMap;
    UniformMap linkedUniforms;

    for (unsigned int vertexUniformIndex = 0; vertexUniformIndex < vertexUniforms.size(); vertexUniformIndex++)
    {
        const gl::Uniform &vertexUniform = vertexUniforms[vertexUniformIndex];
        linkedUniforms[vertexUniform.name] = &vertexUniform;
    }

    for (unsigned int fragmentUniformIndex = 0; fragmentUniformIndex < fragmentUniforms.size(); fragmentUniformIndex++)
    {
        const gl::Uniform &fragmentUniform = fragmentUniforms[fragmentUniformIndex];
        UniformMap::const_iterator entry = linkedUniforms.find(fragmentUniform.name);
        if (entry != linkedUniforms.end())
        {
            const gl::Uniform &vertexUniform = *entry->second;
            const std::string &uniformName = "uniform '" + vertexUniform.name + "'";
            if (!linkValidateVariables(infoLog, uniformName, vertexUniform, fragmentUniform))
            {
                return false;
            }
        }
    }

    for (unsigned int uniformIndex = 0; uniformIndex < vertexUniforms.size(); uniformIndex++)
    {
        if (!defineUniform(GL_VERTEX_SHADER, vertexUniforms[uniformIndex], infoLog))
        {
            return false;
        }
    }

    for (unsigned int uniformIndex = 0; uniformIndex < fragmentUniforms.size(); uniformIndex++)
    {
        if (!defineUniform(GL_FRAGMENT_SHADER, fragmentUniforms[uniformIndex], infoLog))
        {
            return false;
        }
    }

    initializeUniformStorage();

    return true;
}

int totalRegisterCount(const gl::Uniform &uniform)
{
    int registerCount = 0;

    if (!uniform.fields.empty())
    {
        for (unsigned int fieldIndex = 0; fieldIndex < uniform.fields.size(); fieldIndex++)
        {
            registerCount += totalRegisterCount(uniform.fields[fieldIndex]);
        }
    }
    else
    {
        registerCount = 1;
    }

    return (uniform.arraySize > 0) ? uniform.arraySize * registerCount : registerCount;
}

TextureType ProgramBinary::getTextureType(GLenum samplerType, InfoLog &infoLog)
{
    switch(samplerType)
    {
      case GL_SAMPLER_2D:
      case GL_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_2D:
      case GL_SAMPLER_2D_SHADOW:
        return TEXTURE_2D;
      case GL_SAMPLER_3D:
      case GL_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
        return TEXTURE_3D;
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_CUBE_SHADOW:
        return TEXTURE_CUBE;
      case GL_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
        return TEXTURE_CUBE;
      case GL_SAMPLER_2D_ARRAY:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
        return TEXTURE_2D_ARRAY;
      default: UNREACHABLE();
    }

    return TEXTURE_2D;
}

bool ProgramBinary::defineUniform(GLenum shader, const gl::Uniform &constant, InfoLog &infoLog)
{
    if (constant.isStruct())
    {
        if (constant.arraySize > 0)
        {
            unsigned int elementRegisterIndex = constant.registerIndex;

            for (unsigned int elementIndex = 0; elementIndex < constant.arraySize; elementIndex++)
            {
                for (size_t fieldIndex = 0; fieldIndex < constant.fields.size(); fieldIndex++)
                {
                    const gl::Uniform &field = constant.fields[fieldIndex];
                    const std::string &uniformName = constant.name + ArrayString(elementIndex) + "." + field.name;
                    gl::Uniform fieldUniform(field.type, field.precision, uniformName.c_str(), field.arraySize,
                                             elementRegisterIndex, field.elementIndex);

                    fieldUniform.fields = field.fields;
                    if (!defineUniform(shader, fieldUniform, infoLog))
                    {
                        return false;
                    }
                    elementRegisterIndex += totalRegisterCount(field);
                }
            }
        }
        else
        {
            for (size_t fieldIndex = 0; fieldIndex < constant.fields.size(); fieldIndex++)
            {
                const gl::Uniform &field = constant.fields[fieldIndex];
                const std::string &uniformName = constant.name + "." + field.name;

                gl::Uniform fieldUniform(field.type, field.precision, uniformName.c_str(), field.arraySize,
                                         field.registerIndex, field.elementIndex);

                fieldUniform.fields = field.fields;

                if (!defineUniform(shader, fieldUniform, infoLog))
                {
                    return false;
                }
            }
        }

        return true;
    }

    if (IsSampler(constant.type))
    {
        unsigned int samplerIndex = constant.registerIndex;
            
        do
        {
            if (shader == GL_VERTEX_SHADER)
            {
                if (samplerIndex < mRenderer->getMaxVertexTextureImageUnits())
                {
                    mSamplersVS[samplerIndex].active = true;
                    mSamplersVS[samplerIndex].textureType = getTextureType(constant.type, infoLog);
                    mSamplersVS[samplerIndex].logicalTextureUnit = 0;
                    mUsedVertexSamplerRange = std::max(samplerIndex + 1, mUsedVertexSamplerRange);
                }
                else
                {
                    infoLog.append("Vertex shader sampler count exceeds the maximum vertex texture units (%d).", mRenderer->getMaxVertexTextureImageUnits());
                    return false;
                }
            }
            else if (shader == GL_FRAGMENT_SHADER)
            {
                if (samplerIndex < MAX_TEXTURE_IMAGE_UNITS)
                {
                    mSamplersPS[samplerIndex].active = true;
                    mSamplersPS[samplerIndex].textureType = getTextureType(constant.type, infoLog);
                    mSamplersPS[samplerIndex].logicalTextureUnit = 0;
                    mUsedPixelSamplerRange = std::max(samplerIndex + 1, mUsedPixelSamplerRange);
                }
                else
                {
                    infoLog.append("Pixel shader sampler count exceeds MAX_TEXTURE_IMAGE_UNITS (%d).", MAX_TEXTURE_IMAGE_UNITS);
                    return false;
                }
            }
            else UNREACHABLE();

            samplerIndex++;
        }
        while (samplerIndex < constant.registerIndex + constant.arraySize);
    }

    LinkedUniform *uniform = NULL;
    GLint location = getUniformLocation(constant.name);

    if (location >= 0)   // Previously defined, type and precision must match
    {
        uniform = mUniforms[mUniformIndex[location].index];
    }
    else
    {
        uniform = new LinkedUniform(constant.type, constant.precision, constant.name, constant.arraySize, -1, gl::BlockMemberInfo::defaultBlockInfo);
        uniform->registerElement = constant.elementIndex;
    }

    if (!uniform)
    {
        return false;
    }

    if (shader == GL_FRAGMENT_SHADER)
    {
        uniform->psRegisterIndex = constant.registerIndex;
    }
    else if (shader == GL_VERTEX_SHADER)
    {
        uniform->vsRegisterIndex = constant.registerIndex;
    }
    else UNREACHABLE();

    if (location >= 0)
    {
        return uniform->type == constant.type;
    }

    mUniforms.push_back(uniform);
    unsigned int uniformIndex = mUniforms.size() - 1;

    for (unsigned int arrayElementIndex = 0; arrayElementIndex < uniform->elementCount(); arrayElementIndex++)
    {
        mUniformIndex.push_back(VariableLocation(uniform->name, arrayElementIndex, uniformIndex));
    }

    if (shader == GL_VERTEX_SHADER)
    {
        if (constant.registerIndex + uniform->registerCount > mRenderer->getReservedVertexUniformVectors() + mRenderer->getMaxVertexUniformVectors())
        {
            infoLog.append("Vertex shader active uniforms exceed GL_MAX_VERTEX_UNIFORM_VECTORS (%u)", mRenderer->getMaxVertexUniformVectors());
            return false;
        }
    }
    else if (shader == GL_FRAGMENT_SHADER)
    {
        if (constant.registerIndex + uniform->registerCount > mRenderer->getReservedFragmentUniformVectors() + mRenderer->getMaxFragmentUniformVectors())
        {
            infoLog.append("Fragment shader active uniforms exceed GL_MAX_FRAGMENT_UNIFORM_VECTORS (%u)", mRenderer->getMaxFragmentUniformVectors());
            return false;
        }
    }
    else UNREACHABLE();

    return true;
}

bool ProgramBinary::areMatchingInterfaceBlocks(InfoLog &infoLog, const gl::InterfaceBlock &vertexInterfaceBlock, const gl::InterfaceBlock &fragmentInterfaceBlock)
{
    const char* blockName = vertexInterfaceBlock.name.c_str();

    // validate blocks for the same member types
    if (vertexInterfaceBlock.fields.size() != fragmentInterfaceBlock.fields.size())
    {
        infoLog.append("Types for interface block '%s' differ between vertex and fragment shaders", blockName);
        return false;
    }

    if (vertexInterfaceBlock.arraySize != fragmentInterfaceBlock.arraySize)
    {
        infoLog.append("Array sizes differ for interface block '%s' between vertex and fragment shaders", blockName);
        return false;
    }

    if (vertexInterfaceBlock.layout != fragmentInterfaceBlock.layout || vertexInterfaceBlock.isRowMajorLayout != fragmentInterfaceBlock.isRowMajorLayout)
    {
        infoLog.append("Layout qualifiers differ for interface block '%s' between vertex and fragment shaders", blockName);
        return false;
    }

    const unsigned int numBlockMembers = vertexInterfaceBlock.fields.size();
    for (unsigned int blockMemberIndex = 0; blockMemberIndex < numBlockMembers; blockMemberIndex++)
    {
        const gl::InterfaceBlockField &vertexMember = vertexInterfaceBlock.fields[blockMemberIndex];
        const gl::InterfaceBlockField &fragmentMember = fragmentInterfaceBlock.fields[blockMemberIndex];

        if (vertexMember.name != fragmentMember.name)
        {
            infoLog.append("Name mismatch for field %d of interface block '%s': (in vertex: '%s', in fragment: '%s')",
                           blockMemberIndex, blockName, vertexMember.name.c_str(), fragmentMember.name.c_str());
            return false;
        }

        std::string uniformName = "interface block '" + vertexInterfaceBlock.name + "' member '" + vertexMember.name + "'";
        if (!linkValidateVariables(infoLog, uniformName, vertexMember, fragmentMember))
        {
            return false;
        }
    }

    return true;
}

bool ProgramBinary::linkUniformBlocks(InfoLog &infoLog, const std::vector<gl::InterfaceBlock> &vertexInterfaceBlocks,
                                      const std::vector<gl::InterfaceBlock> &fragmentInterfaceBlocks)
{
    // Check that interface blocks defined in the vertex and fragment shaders are identical
    typedef std::map<std::string, const gl::InterfaceBlock*> UniformBlockMap;
    UniformBlockMap linkedUniformBlocks;

    for (unsigned int blockIndex = 0; blockIndex < vertexInterfaceBlocks.size(); blockIndex++)
    {
        const gl::InterfaceBlock &vertexInterfaceBlock = vertexInterfaceBlocks[blockIndex];
        linkedUniformBlocks[vertexInterfaceBlock.name] = &vertexInterfaceBlock;
    }

    for (unsigned int blockIndex = 0; blockIndex < fragmentInterfaceBlocks.size(); blockIndex++)
    {
        const gl::InterfaceBlock &fragmentInterfaceBlock = fragmentInterfaceBlocks[blockIndex];
        UniformBlockMap::const_iterator entry = linkedUniformBlocks.find(fragmentInterfaceBlock.name);
        if (entry != linkedUniformBlocks.end())
        {
            const gl::InterfaceBlock &vertexInterfaceBlock = *entry->second;
            if (!areMatchingInterfaceBlocks(infoLog, vertexInterfaceBlock, fragmentInterfaceBlock))
            {
                return false;
            }
        }
    }

    for (unsigned int blockIndex = 0; blockIndex < vertexInterfaceBlocks.size(); blockIndex++)
    {
        if (!defineUniformBlock(infoLog, GL_VERTEX_SHADER, vertexInterfaceBlocks[blockIndex]))
        {
            return false;
        }
    }

    for (unsigned int blockIndex = 0; blockIndex < fragmentInterfaceBlocks.size(); blockIndex++)
    {
        if (!defineUniformBlock(infoLog, GL_FRAGMENT_SHADER, fragmentInterfaceBlocks[blockIndex]))
        {
            return false;
        }
    }

    return true;
}

bool ProgramBinary::gatherTransformFeedbackLinkedVaryings(InfoLog &infoLog, const std::vector<LinkedVarying> &linkedVaryings,
                                                          const std::vector<std::string> &transformFeedbackVaryingNames,
                                                          GLenum transformFeedbackBufferMode,
                                                          std::vector<LinkedVarying> *outTransformFeedbackLinkedVaryings) const
{
    size_t totalComponents = 0;
    const size_t maxSeparateComponents = mRenderer->getMaxTransformFeedbackSeparateComponents();
    const size_t maxInterleavedComponents = mRenderer->getMaxTransformFeedbackInterleavedComponents();

    // Gather the linked varyings that are used for transform feedback, they should all exist.
    outTransformFeedbackLinkedVaryings->clear();
    for (size_t i = 0; i < transformFeedbackVaryingNames.size(); i++)
    {
        bool found = false;
        for (size_t j = 0; j < linkedVaryings.size(); j++)
        {
            if (transformFeedbackVaryingNames[i] == linkedVaryings[j].name)
            {
                for (size_t k = 0; k < outTransformFeedbackLinkedVaryings->size(); k++)
                {
                    if (outTransformFeedbackLinkedVaryings->at(k).name == linkedVaryings[j].name)
                    {
                        infoLog.append("Two transform feedback varyings specify the same output variable (%s).", linkedVaryings[j].name.c_str());
                        return false;
                    }
                }

                size_t componentCount = linkedVaryings[j].semanticIndexCount * 4;
                if (transformFeedbackBufferMode == GL_SEPARATE_ATTRIBS &&
                    componentCount > maxSeparateComponents)
                {
                    infoLog.append("Transform feedback varying's %s components (%u) exceed the maximum separate components (%u).",
                                   linkedVaryings[j].name.c_str(), componentCount, maxSeparateComponents);
                    return false;
                }

                totalComponents += componentCount;

                outTransformFeedbackLinkedVaryings->push_back(linkedVaryings[j]);
                found = true;
                break;
            }
        }

        // All transform feedback varyings are expected to exist since packVaryings checks for them.
        ASSERT(found);
    }

    if (transformFeedbackBufferMode == GL_INTERLEAVED_ATTRIBS && totalComponents > maxInterleavedComponents)
    {
        infoLog.append("Transform feedback varying total components (%u) exceed the maximum interleaved components (%u).",
                       totalComponents, maxInterleavedComponents);
        return false;
    }

    return true;
}

void ProgramBinary::defineUniformBlockMembers(const std::vector<gl::InterfaceBlockField> &fields, const std::string &prefix, int blockIndex, BlockInfoItr *blockInfoItr, std::vector<unsigned int> *blockUniformIndexes)
{
    for (unsigned int uniformIndex = 0; uniformIndex < fields.size(); uniformIndex++)
    {
        const gl::InterfaceBlockField &field = fields[uniformIndex];
        const std::string &fieldName = (prefix.empty() ? field.name : prefix + "." + field.name);

        if (!field.fields.empty())
        {
            if (field.arraySize > 0)
            {
                for (unsigned int arrayElement = 0; arrayElement < field.arraySize; arrayElement++)
                {
                    const std::string uniformElementName = fieldName + ArrayString(arrayElement);
                    defineUniformBlockMembers(field.fields, uniformElementName, blockIndex, blockInfoItr, blockUniformIndexes);
                }
            }
            else
            {
                defineUniformBlockMembers(field.fields, fieldName, blockIndex, blockInfoItr, blockUniformIndexes);
            }
        }
        else
        {
            LinkedUniform *newUniform = new LinkedUniform(field.type, field.precision, fieldName, field.arraySize,
                                                          blockIndex, **blockInfoItr);

            // add to uniform list, but not index, since uniform block uniforms have no location
            blockUniformIndexes->push_back(mUniforms.size());
            mUniforms.push_back(newUniform);
            (*blockInfoItr)++;
        }
    }
}

bool ProgramBinary::defineUniformBlock(InfoLog &infoLog, GLenum shader, const gl::InterfaceBlock &interfaceBlock)
{
    // create uniform block entries if they do not exist
    if (getUniformBlockIndex(interfaceBlock.name) == GL_INVALID_INDEX)
    {
        std::vector<unsigned int> blockUniformIndexes;
        const unsigned int blockIndex = mUniformBlocks.size();

        // define member uniforms
        BlockInfoItr blockInfoItr = interfaceBlock.blockInfo.cbegin();
        defineUniformBlockMembers(interfaceBlock.fields, "", blockIndex, &blockInfoItr, &blockUniformIndexes);

        // create all the uniform blocks
        if (interfaceBlock.arraySize > 0)
        {
            for (unsigned int uniformBlockElement = 0; uniformBlockElement < interfaceBlock.arraySize; uniformBlockElement++)
            {
                gl::UniformBlock *newUniformBlock = new UniformBlock(interfaceBlock.name, uniformBlockElement, interfaceBlock.dataSize);
                newUniformBlock->memberUniformIndexes = blockUniformIndexes;
                mUniformBlocks.push_back(newUniformBlock);
            }
        }
        else
        {
            gl::UniformBlock *newUniformBlock = new UniformBlock(interfaceBlock.name, GL_INVALID_INDEX, interfaceBlock.dataSize);
            newUniformBlock->memberUniformIndexes = blockUniformIndexes;
            mUniformBlocks.push_back(newUniformBlock);
        }
    }

    // Assign registers to the uniform blocks
    const GLuint blockIndex = getUniformBlockIndex(interfaceBlock.name);
    const unsigned int elementCount = std::max(1u, interfaceBlock.arraySize);
    ASSERT(blockIndex != GL_INVALID_INDEX);
    ASSERT(blockIndex + elementCount <= mUniformBlocks.size());

    for (unsigned int uniformBlockElement = 0; uniformBlockElement < elementCount; uniformBlockElement++)
    {
        gl::UniformBlock *uniformBlock = mUniformBlocks[blockIndex + uniformBlockElement];
        ASSERT(uniformBlock->name == interfaceBlock.name);

        if (!assignUniformBlockRegister(infoLog, uniformBlock, shader, interfaceBlock.registerIndex + uniformBlockElement))
        {
            return false;
        }
    }

    return true;
}

bool ProgramBinary::assignUniformBlockRegister(InfoLog &infoLog, UniformBlock *uniformBlock, GLenum shader, unsigned int registerIndex)
{
    if (shader == GL_VERTEX_SHADER)
    {
        uniformBlock->vsRegisterIndex = registerIndex;
        unsigned int maximumBlocks = mRenderer->getMaxVertexShaderUniformBuffers();

        if (registerIndex - mRenderer->getReservedVertexUniformBuffers() >= maximumBlocks)
        {
            infoLog.append("Vertex shader uniform block count exceed GL_MAX_VERTEX_UNIFORM_BLOCKS (%u)", maximumBlocks);
            return false;
        }
    }
    else if (shader == GL_FRAGMENT_SHADER)
    {
        uniformBlock->psRegisterIndex = registerIndex;
        unsigned int maximumBlocks = mRenderer->getMaxFragmentShaderUniformBuffers();

        if (registerIndex - mRenderer->getReservedFragmentUniformBuffers() >= maximumBlocks)
        {
            infoLog.append("Fragment shader uniform block count exceed GL_MAX_FRAGMENT_UNIFORM_BLOCKS (%u)", maximumBlocks);
            return false;
        }
    }
    else UNREACHABLE();

    return true;
}

bool ProgramBinary::isValidated() const 
{
    return mValidated;
}

void ProgramBinary::getActiveAttribute(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) const
{
    // Skip over inactive attributes
    unsigned int activeAttribute = 0;
    unsigned int attribute;
    for (attribute = 0; attribute < MAX_VERTEX_ATTRIBS; attribute++)
    {
        if (mLinkedAttribute[attribute].name.empty())
        {
            continue;
        }

        if (activeAttribute == index)
        {
            break;
        }

        activeAttribute++;
    }

    if (bufsize > 0)
    {
        const char *string = mLinkedAttribute[attribute].name.c_str();

        strncpy(name, string, bufsize);
        name[bufsize - 1] = '\0';

        if (length)
        {
            *length = strlen(name);
        }
    }

    *size = 1;   // Always a single 'type' instance

    *type = mLinkedAttribute[attribute].type;
}

GLint ProgramBinary::getActiveAttributeCount() const
{
    int count = 0;

    for (int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        if (!mLinkedAttribute[attributeIndex].name.empty())
        {
            count++;
        }
    }

    return count;
}

GLint ProgramBinary::getActiveAttributeMaxLength() const
{
    int maxLength = 0;

    for (int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        if (!mLinkedAttribute[attributeIndex].name.empty())
        {
            maxLength = std::max((int)(mLinkedAttribute[attributeIndex].name.length() + 1), maxLength);
        }
    }

    return maxLength;
}

void ProgramBinary::getActiveUniform(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) const
{
    ASSERT(index < mUniforms.size());   // index must be smaller than getActiveUniformCount()

    if (bufsize > 0)
    {
        std::string string = mUniforms[index]->name;

        if (mUniforms[index]->isArray())
        {
            string += "[0]";
        }

        strncpy(name, string.c_str(), bufsize);
        name[bufsize - 1] = '\0';

        if (length)
        {
            *length = strlen(name);
        }
    }

    *size = mUniforms[index]->elementCount();

    *type = mUniforms[index]->type;
}

GLint ProgramBinary::getActiveUniformCount() const
{
    return mUniforms.size();
}

GLint ProgramBinary::getActiveUniformMaxLength() const
{
    int maxLength = 0;

    unsigned int numUniforms = mUniforms.size();
    for (unsigned int uniformIndex = 0; uniformIndex < numUniforms; uniformIndex++)
    {
        if (!mUniforms[uniformIndex]->name.empty())
        {
            int length = (int)(mUniforms[uniformIndex]->name.length() + 1);
            if (mUniforms[uniformIndex]->isArray())
            {
                length += 3;  // Counting in "[0]".
            }
            maxLength = std::max(length, maxLength);
        }
    }

    return maxLength;
}

GLint ProgramBinary::getActiveUniformi(GLuint index, GLenum pname) const
{
    const gl::LinkedUniform& uniform = *mUniforms[index];

    switch (pname)
    {
      case GL_UNIFORM_TYPE:         return static_cast<GLint>(uniform.type);
      case GL_UNIFORM_SIZE:         return static_cast<GLint>(uniform.elementCount());
      case GL_UNIFORM_NAME_LENGTH:  return static_cast<GLint>(uniform.name.size() + 1 + (uniform.isArray() ? 3 : 0));
      case GL_UNIFORM_BLOCK_INDEX:  return uniform.blockIndex;

      case GL_UNIFORM_OFFSET:       return uniform.blockInfo.offset;
      case GL_UNIFORM_ARRAY_STRIDE: return uniform.blockInfo.arrayStride;
      case GL_UNIFORM_MATRIX_STRIDE: return uniform.blockInfo.matrixStride;
      case GL_UNIFORM_IS_ROW_MAJOR: return static_cast<GLint>(uniform.blockInfo.isRowMajorMatrix);

      default:
        UNREACHABLE();
        break;
    }
    return 0;
}

void ProgramBinary::getActiveUniformBlockName(GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName) const
{
    ASSERT(uniformBlockIndex < mUniformBlocks.size());   // index must be smaller than getActiveUniformBlockCount()

    const UniformBlock &uniformBlock = *mUniformBlocks[uniformBlockIndex];

    if (bufSize > 0)
    {
        std::string string = uniformBlock.name;

        if (uniformBlock.isArrayElement())
        {
            string += ArrayString(uniformBlock.elementIndex);
        }

        strncpy(uniformBlockName, string.c_str(), bufSize);
        uniformBlockName[bufSize - 1] = '\0';

        if (length)
        {
            *length = strlen(uniformBlockName);
        }
    }
}

void ProgramBinary::getActiveUniformBlockiv(GLuint uniformBlockIndex, GLenum pname, GLint *params) const
{
    ASSERT(uniformBlockIndex < mUniformBlocks.size());   // index must be smaller than getActiveUniformBlockCount()

    const UniformBlock &uniformBlock = *mUniformBlocks[uniformBlockIndex];

    switch (pname)
    {
      case GL_UNIFORM_BLOCK_DATA_SIZE:
        *params = static_cast<GLint>(uniformBlock.dataSize);
        break;
      case GL_UNIFORM_BLOCK_NAME_LENGTH:
        *params = static_cast<GLint>(uniformBlock.name.size() + 1 + (uniformBlock.isArrayElement() ? 3 : 0));
        break;
      case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
        *params = static_cast<GLint>(uniformBlock.memberUniformIndexes.size());
        break;
      case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
        {
            for (unsigned int blockMemberIndex = 0; blockMemberIndex < uniformBlock.memberUniformIndexes.size(); blockMemberIndex++)
            {
                params[blockMemberIndex] = static_cast<GLint>(uniformBlock.memberUniformIndexes[blockMemberIndex]);
            }
        }
        break;
      case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
        *params = static_cast<GLint>(uniformBlock.isReferencedByVertexShader());
        break;
      case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
        *params = static_cast<GLint>(uniformBlock.isReferencedByFragmentShader());
        break;
      default: UNREACHABLE();
    }
}

GLuint ProgramBinary::getActiveUniformBlockCount() const
{
    return mUniformBlocks.size();
}

GLuint ProgramBinary::getActiveUniformBlockMaxLength() const
{
    unsigned int maxLength = 0;

    unsigned int numUniformBlocks = mUniformBlocks.size();
    for (unsigned int uniformBlockIndex = 0; uniformBlockIndex < numUniformBlocks; uniformBlockIndex++)
    {
        const UniformBlock &uniformBlock = *mUniformBlocks[uniformBlockIndex];
        if (!uniformBlock.name.empty())
        {
            const unsigned int length = uniformBlock.name.length() + 1;

            // Counting in "[0]".
            const unsigned int arrayLength = (uniformBlock.isArrayElement() ? 3 : 0);

            maxLength = std::max(length + arrayLength, maxLength);
        }
    }

    return maxLength;
}

void ProgramBinary::validate(InfoLog &infoLog)
{
    applyUniforms();
    if (!validateSamplers(&infoLog))
    {
        mValidated = false;
    }
    else
    {
        mValidated = true;
    }
}

bool ProgramBinary::validateSamplers(InfoLog *infoLog)
{
    // if any two active samplers in a program are of different types, but refer to the same
    // texture image unit, and this is the current program, then ValidateProgram will fail, and
    // DrawArrays and DrawElements will issue the INVALID_OPERATION error.

    const unsigned int maxCombinedTextureImageUnits = mRenderer->getMaxCombinedTextureImageUnits();
    TextureType textureUnitType[IMPLEMENTATION_MAX_COMBINED_TEXTURE_IMAGE_UNITS];

    for (unsigned int i = 0; i < IMPLEMENTATION_MAX_COMBINED_TEXTURE_IMAGE_UNITS; ++i)
    {
        textureUnitType[i] = TEXTURE_UNKNOWN;
    }

    for (unsigned int i = 0; i < mUsedPixelSamplerRange; ++i)
    {
        if (mSamplersPS[i].active)
        {
            unsigned int unit = mSamplersPS[i].logicalTextureUnit;
            
            if (unit >= maxCombinedTextureImageUnits)
            {
                if (infoLog)
                {
                    infoLog->append("Sampler uniform (%d) exceeds IMPLEMENTATION_MAX_COMBINED_TEXTURE_IMAGE_UNITS (%d)", unit, maxCombinedTextureImageUnits);
                }

                return false;
            }

            if (textureUnitType[unit] != TEXTURE_UNKNOWN)
            {
                if (mSamplersPS[i].textureType != textureUnitType[unit])
                {
                    if (infoLog)
                    {
                        infoLog->append("Samplers of conflicting types refer to the same texture image unit (%d).", unit);
                    }

                    return false;
                }
            }
            else
            {
                textureUnitType[unit] = mSamplersPS[i].textureType;
            }
        }
    }

    for (unsigned int i = 0; i < mUsedVertexSamplerRange; ++i)
    {
        if (mSamplersVS[i].active)
        {
            unsigned int unit = mSamplersVS[i].logicalTextureUnit;
            
            if (unit >= maxCombinedTextureImageUnits)
            {
                if (infoLog)
                {
                    infoLog->append("Sampler uniform (%d) exceeds IMPLEMENTATION_MAX_COMBINED_TEXTURE_IMAGE_UNITS (%d)", unit, maxCombinedTextureImageUnits);
                }

                return false;
            }

            if (textureUnitType[unit] != TEXTURE_UNKNOWN)
            {
                if (mSamplersVS[i].textureType != textureUnitType[unit])
                {
                    if (infoLog)
                    {
                        infoLog->append("Samplers of conflicting types refer to the same texture image unit (%d).", unit);
                    }

                    return false;
                }
            }
            else
            {
                textureUnitType[unit] = mSamplersVS[i].textureType;
            }
        }
    }

    return true;
}

ProgramBinary::Sampler::Sampler() : active(false), logicalTextureUnit(0), textureType(TEXTURE_2D)
{
}

struct AttributeSorter
{
    AttributeSorter(const int (&semanticIndices)[MAX_VERTEX_ATTRIBS])
        : originalIndices(semanticIndices)
    {
    }

    bool operator()(int a, int b)
    {
        if (originalIndices[a] == -1) return false;
        if (originalIndices[b] == -1) return true;
        return (originalIndices[a] < originalIndices[b]);
    }

    const int (&originalIndices)[MAX_VERTEX_ATTRIBS];
};

void ProgramBinary::initAttributesByLayout()
{
    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        mAttributesByLayout[i] = i;
    }

    std::sort(&mAttributesByLayout[0], &mAttributesByLayout[MAX_VERTEX_ATTRIBS], AttributeSorter(mSemanticIndex));
}

void ProgramBinary::sortAttributesByLayout(rx::TranslatedAttribute attributes[MAX_VERTEX_ATTRIBS], int sortedSemanticIndices[MAX_VERTEX_ATTRIBS]) const
{
    rx::TranslatedAttribute oldTranslatedAttributes[MAX_VERTEX_ATTRIBS];

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        oldTranslatedAttributes[i] = attributes[i];
    }

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        int oldIndex = mAttributesByLayout[i];
        sortedSemanticIndices[i] = mSemanticIndex[oldIndex];
        attributes[i] = oldTranslatedAttributes[oldIndex];
    }
}

void ProgramBinary::initializeUniformStorage()
{
    // Compute total default block size
    unsigned int vertexRegisters = 0;
    unsigned int fragmentRegisters = 0;
    for (size_t uniformIndex = 0; uniformIndex < mUniforms.size(); uniformIndex++)
    {
        const LinkedUniform &uniform = *mUniforms[uniformIndex];

        if (!IsSampler(uniform.type))
        {
            if (uniform.isReferencedByVertexShader())
            {
                vertexRegisters = std::max(vertexRegisters, uniform.vsRegisterIndex + uniform.registerCount);
            }
            if (uniform.isReferencedByFragmentShader())
            {
                fragmentRegisters = std::max(fragmentRegisters, uniform.psRegisterIndex + uniform.registerCount);
            }
        }
    }

    mVertexUniformStorage = mRenderer->createUniformStorage(vertexRegisters * 16u);
    mFragmentUniformStorage = mRenderer->createUniformStorage(fragmentRegisters * 16u);
}

}
