//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.cpp: Implements the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#include "libGLESv2/BinaryStream.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/ProgramBinary.h"

#include "common/debug.h"
#include "common/version.h"

#include "libGLESv2/main.h"
#include "libGLESv2/Shader.h"
#include "libGLESv2/utilities.h"

#include <string>

#if !defined(ANGLE_COMPILE_OPTIMIZATION_LEVEL)
#define ANGLE_COMPILE_OPTIMIZATION_LEVEL D3DCOMPILE_OPTIMIZATION_LEVEL3
#endif

namespace gl
{
std::string str(int i)
{
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%d", i);
    return buffer;
}

Uniform::Uniform(GLenum type, const std::string &_name, unsigned int arraySize)
    : type(type), _name(_name), name(ProgramBinary::undecorateUniform(_name)), arraySize(arraySize)
{
    int bytes = UniformInternalSize(type) * arraySize;
    data = new unsigned char[bytes];
    memset(data, 0, bytes);
    dirty = true;
}

Uniform::~Uniform()
{
    delete[] data;
}

bool Uniform::isArray()
{
    size_t dot = _name.find_last_of('.');
    if (dot == std::string::npos) dot = -1;

    return _name.compare(dot + 1, dot + 4, "ar_") == 0;
}

UniformLocation::UniformLocation(const std::string &_name, unsigned int element, unsigned int index) 
    : name(ProgramBinary::undecorateUniform(_name)), element(element), index(index)
{
}

unsigned int ProgramBinary::mCurrentSerial = 1;

ProgramBinary::ProgramBinary() : RefCountObject(0), mSerial(issueSerial())
{
    mDevice = getDevice();

    mPixelExecutable = NULL;
    mVertexExecutable = NULL;
    mConstantTablePS = NULL;
    mConstantTableVS = NULL;

    mValidated = false;

    for (int index = 0; index < MAX_VERTEX_ATTRIBS; index++)
    {
        mSemanticIndex[index] = -1;
    }

    for (int index = 0; index < MAX_TEXTURE_IMAGE_UNITS; index++)
    {
        mSamplersPS[index].active = false;
    }

    for (int index = 0; index < MAX_VERTEX_TEXTURE_IMAGE_UNITS_VTF; index++)
    {
        mSamplersVS[index].active = false;
    }

    mUsedVertexSamplerRange = 0;
    mUsedPixelSamplerRange = 0;

    mDxDepthRangeLocation = -1;
    mDxDepthLocation = -1;
    mDxCoordLocation = -1;
    mDxHalfPixelSizeLocation = -1;
    mDxFrontCCWLocation = -1;
    mDxPointsOrLinesLocation = -1;
}

ProgramBinary::~ProgramBinary()
{
    if (mPixelExecutable)
    {
        mPixelExecutable->Release();
    }

    if (mVertexExecutable)
    {
        mVertexExecutable->Release();
    }

    delete mConstantTablePS;
    delete mConstantTableVS;

    while (!mUniforms.empty())
    {
        delete mUniforms.back();
        mUniforms.pop_back();
    }
}

unsigned int ProgramBinary::getSerial() const
{
    return mSerial;
}

unsigned int ProgramBinary::issueSerial()
{
    return mCurrentSerial++;
}

IDirect3DPixelShader9 *ProgramBinary::getPixelShader()
{
    return mPixelExecutable;
}

IDirect3DVertexShader9 *ProgramBinary::getVertexShader()
{
    return mVertexExecutable;
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

    if (logicalTextureUnit >= 0 && logicalTextureUnit < (GLint)getContext()->getMaximumCombinedTextureImageUnits())
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
    unsigned int subscript = 0;

    // Strip any trailing array operator and retrieve the subscript
    size_t open = name.find_last_of('[');
    size_t close = name.find_last_of(']');
    if (open != std::string::npos && close == name.length() - 1)
    {
        subscript = atoi(name.substr(open + 1).c_str());
        name.erase(open);
    }

    unsigned int numUniforms = mUniformIndex.size();
    for (unsigned int location = 0; location < numUniforms; location++)
    {
        if (mUniformIndex[location].name == name &&
            mUniformIndex[location].element == subscript)
        {
            return location;
        }
    }

    return -1;
}

bool ProgramBinary::setUniform1fv(GLint location, GLsizei count, const GLfloat* v)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type == GL_FLOAT)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);

        GLfloat *target = (GLfloat*)targetUniform->data + mUniformIndex[location].element * 4;

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
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);
        GLboolean *boolParams = (GLboolean*)targetUniform->data + mUniformIndex[location].element;

        for (int i = 0; i < count; ++i)
        {
            if (v[i] == 0.0f)
            {
                boolParams[i] = GL_FALSE;
            }
            else
            {
                boolParams[i] = GL_TRUE;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

bool ProgramBinary::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type == GL_FLOAT_VEC2)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);

        GLfloat *target = (GLfloat*)targetUniform->data + mUniformIndex[location].element * 4;

        for (int i = 0; i < count; i++)
        {
            target[0] = v[0];
            target[1] = v[1];
            target[2] = 0;
            target[3] = 0;
            target += 4;
            v += 2;
        }
    }
    else if (targetUniform->type == GL_BOOL_VEC2)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);

        GLboolean *boolParams = (GLboolean*)targetUniform->data + mUniformIndex[location].element * 2;

        for (int i = 0; i < count * 2; ++i)
        {
            if (v[i] == 0.0f)
            {
                boolParams[i] = GL_FALSE;
            }
            else
            {
                boolParams[i] = GL_TRUE;
            }
        }
    }
    else 
    {
        return false;
    }

    return true;
}

bool ProgramBinary::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type == GL_FLOAT_VEC3)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);

        GLfloat *target = (GLfloat*)targetUniform->data + mUniformIndex[location].element * 4;

        for (int i = 0; i < count; i++)
        {
            target[0] = v[0];
            target[1] = v[1];
            target[2] = v[2];
            target[3] = 0;
            target += 4;
            v += 3;
        }
    }
    else if (targetUniform->type == GL_BOOL_VEC3)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);
        GLboolean *boolParams = (GLboolean*)targetUniform->data + mUniformIndex[location].element * 3;

        for (int i = 0; i < count * 3; ++i)
        {
            if (v[i] == 0.0f)
            {
                boolParams[i] = GL_FALSE;
            }
            else
            {
                boolParams[i] = GL_TRUE;
            }
        }
    }
    else 
    {
        return false;
    }

    return true;
}

bool ProgramBinary::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type == GL_FLOAT_VEC4)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLfloat) * 4,
               v, 4 * sizeof(GLfloat) * count);
    }
    else if (targetUniform->type == GL_BOOL_VEC4)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);
        GLboolean *boolParams = (GLboolean*)targetUniform->data + mUniformIndex[location].element * 4;

        for (int i = 0; i < count * 4; ++i)
        {
            if (v[i] == 0.0f)
            {
                boolParams[i] = GL_FALSE;
            }
            else
            {
                boolParams[i] = GL_TRUE;
            }
        }
    }
    else 
    {
        return false;
    }

    return true;
}

template<typename T, int targetWidth, int targetHeight, int srcWidth, int srcHeight>
void transposeMatrix(T *target, const GLfloat *value)
{
    int copyWidth = std::min(targetWidth, srcWidth);
    int copyHeight = std::min(targetHeight, srcHeight);

    for (int x = 0; x < copyWidth; x++)
    {
        for (int y = 0; y < copyHeight; y++)
        {
            target[x * targetWidth + y] = (T)value[y * srcWidth + x];
        }
    }
    // clear unfilled right side
    for (int y = 0; y < copyHeight; y++)
    {
        for (int x = srcWidth; x < targetWidth; x++)
        {
            target[y * targetWidth + x] = (T)0;
        }
    }
    // clear unfilled bottom.
    for (int y = srcHeight; y < targetHeight; y++)
    {
        for (int x = 0; x < targetWidth; x++)
        {
            target[y * targetWidth + x] = (T)0;
        }
    }
}

bool ProgramBinary::setUniformMatrix2fv(GLint location, GLsizei count, const GLfloat *value)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type != GL_FLOAT_MAT2)
    {
        return false;
    }

    int arraySize = targetUniform->arraySize;

    if (arraySize == 1 && count > 1)
        return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

    count = std::min(arraySize - (int)mUniformIndex[location].element, count);

    GLfloat *target = (GLfloat*)targetUniform->data + mUniformIndex[location].element * 8;
    for (int i = 0; i < count; i++)
    {
        transposeMatrix<GLfloat,4,2,2,2>(target, value);
        target += 8;
        value += 4;
    }

    return true;
}

bool ProgramBinary::setUniformMatrix3fv(GLint location, GLsizei count, const GLfloat *value)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type != GL_FLOAT_MAT3)
    {
        return false;
    }

    int arraySize = targetUniform->arraySize;

    if (arraySize == 1 && count > 1)
        return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

    count = std::min(arraySize - (int)mUniformIndex[location].element, count);

    GLfloat *target = (GLfloat*)targetUniform->data + mUniformIndex[location].element * 12;
    for (int i = 0; i < count; i++)
    {
        transposeMatrix<GLfloat,4,3,3,3>(target, value);
        target += 12;
        value += 9;
    }

    return true;
}


bool ProgramBinary::setUniformMatrix4fv(GLint location, GLsizei count, const GLfloat *value)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type != GL_FLOAT_MAT4)
    {
        return false;
    }

    int arraySize = targetUniform->arraySize;

    if (arraySize == 1 && count > 1)
        return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

    count = std::min(arraySize - (int)mUniformIndex[location].element, count);

    GLfloat *target = (GLfloat*)(targetUniform->data + mUniformIndex[location].element * sizeof(GLfloat) * 16);
    for (int i = 0; i < count; i++)
    {
        transposeMatrix<GLfloat,4,4,4,4>(target, value);
        target += 16;
        value += 16;
    }

    return true;
}

bool ProgramBinary::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type == GL_INT ||
        targetUniform->type == GL_SAMPLER_2D ||
        targetUniform->type == GL_SAMPLER_CUBE)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLint),
               v, sizeof(GLint) * count);
    }
    else if (targetUniform->type == GL_BOOL)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);
        GLboolean *boolParams = (GLboolean*)targetUniform->data + mUniformIndex[location].element;

        for (int i = 0; i < count; ++i)
        {
            if (v[i] == 0)
            {
                boolParams[i] = GL_FALSE;
            }
            else
            {
                boolParams[i] = GL_TRUE;
            }
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
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type == GL_INT_VEC2)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLint) * 2,
               v, 2 * sizeof(GLint) * count);
    }
    else if (targetUniform->type == GL_BOOL_VEC2)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);
        GLboolean *boolParams = (GLboolean*)targetUniform->data + mUniformIndex[location].element * 2;

        for (int i = 0; i < count * 2; ++i)
        {
            if (v[i] == 0)
            {
                boolParams[i] = GL_FALSE;
            }
            else
            {
                boolParams[i] = GL_TRUE;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

bool ProgramBinary::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type == GL_INT_VEC3)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLint) * 3,
               v, 3 * sizeof(GLint) * count);
    }
    else if (targetUniform->type == GL_BOOL_VEC3)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);
        GLboolean *boolParams = (GLboolean*)targetUniform->data + mUniformIndex[location].element * 3;

        for (int i = 0; i < count * 3; ++i)
        {
            if (v[i] == 0)
            {
                boolParams[i] = GL_FALSE;
            }
            else
            {
                boolParams[i] = GL_TRUE;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

bool ProgramBinary::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];
    targetUniform->dirty = true;

    if (targetUniform->type == GL_INT_VEC4)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLint) * 4,
               v, 4 * sizeof(GLint) * count);
    }
    else if (targetUniform->type == GL_BOOL_VEC4)
    {
        int arraySize = targetUniform->arraySize;

        if (arraySize == 1 && count > 1)
            return false; // attempting to write an array to a non-array uniform is an INVALID_OPERATION

        count = std::min(arraySize - (int)mUniformIndex[location].element, count);
        GLboolean *boolParams = (GLboolean*)targetUniform->data + mUniformIndex[location].element * 4;

        for (int i = 0; i < count * 4; ++i)
        {
            if (v[i] == 0)
            {
                boolParams[i] = GL_FALSE;
            }
            else
            {
                boolParams[i] = GL_TRUE;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

bool ProgramBinary::getUniformfv(GLint location, GLsizei *bufSize, GLfloat *params)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];

    // sized queries -- ensure the provided buffer is large enough
    if (bufSize)
    {
        int requiredBytes = UniformExternalSize(targetUniform->type);
        if (*bufSize < requiredBytes)
        {
            return false;
        }
    }

    switch (targetUniform->type)
    {
      case GL_FLOAT_MAT2:
        transposeMatrix<GLfloat,2,2,4,2>(params, (GLfloat*)targetUniform->data + mUniformIndex[location].element * 8);
        break;
      case GL_FLOAT_MAT3:
        transposeMatrix<GLfloat,3,3,4,3>(params, (GLfloat*)targetUniform->data + mUniformIndex[location].element * 12);
        break;
      case GL_FLOAT_MAT4:
        transposeMatrix<GLfloat,4,4,4,4>(params, (GLfloat*)targetUniform->data + mUniformIndex[location].element * 16);
        break;
      default:
        {
            unsigned int count = UniformExternalComponentCount(targetUniform->type);
            unsigned int internalCount = UniformInternalComponentCount(targetUniform->type);

            switch (UniformComponentType(targetUniform->type))
            {
              case GL_BOOL:
                {
                    GLboolean *boolParams = (GLboolean*)targetUniform->data + mUniformIndex[location].element * internalCount;

                    for (unsigned int i = 0; i < count; ++i)
                    {
                        params[i] = (boolParams[i] == GL_FALSE) ? 0.0f : 1.0f;
                    }
                }
                break;
              case GL_FLOAT:
                memcpy(params, targetUniform->data + mUniformIndex[location].element * internalCount * sizeof(GLfloat),
                       count * sizeof(GLfloat));
                break;
              case GL_INT:
                {
                    GLint *intParams = (GLint*)targetUniform->data + mUniformIndex[location].element * internalCount;

                    for (unsigned int i = 0; i < count; ++i)
                    {
                        params[i] = (float)intParams[i];
                    }
                }
                break;
              default: UNREACHABLE();
            }
        }
    }

    return true;
}

bool ProgramBinary::getUniformiv(GLint location, GLsizei *bufSize, GLint *params)
{
    if (location < 0 || location >= (int)mUniformIndex.size())
    {
        return false;
    }

    Uniform *targetUniform = mUniforms[mUniformIndex[location].index];

    // sized queries -- ensure the provided buffer is large enough
    if (bufSize)
    {
        int requiredBytes = UniformExternalSize(targetUniform->type);
        if (*bufSize < requiredBytes)
        {
            return false;
        }
    }

    switch (targetUniform->type)
    {
      case GL_FLOAT_MAT2:
        {
            transposeMatrix<GLint,2,2,4,2>(params, (GLfloat*)targetUniform->data + mUniformIndex[location].element * 8);
        }
        break;
      case GL_FLOAT_MAT3:
        {
            transposeMatrix<GLint,3,3,4,3>(params, (GLfloat*)targetUniform->data + mUniformIndex[location].element * 12);
        }
        break;
      case GL_FLOAT_MAT4:
        {
            transposeMatrix<GLint,4,4,4,4>(params, (GLfloat*)targetUniform->data + mUniformIndex[location].element * 16);
        }
        break;
      default:
        {
            unsigned int count = UniformExternalComponentCount(targetUniform->type);
            unsigned int internalCount = UniformInternalComponentCount(targetUniform->type);

            switch (UniformComponentType(targetUniform->type))
            {
              case GL_BOOL:
                {
                    GLboolean *boolParams = targetUniform->data + mUniformIndex[location].element * internalCount;

                    for (unsigned int i = 0; i < count; ++i)
                    {
                        params[i] = (GLint)boolParams[i];
                    }
                }
                break;
              case GL_FLOAT:
                {
                    GLfloat *floatParams = (GLfloat*)targetUniform->data + mUniformIndex[location].element * internalCount;

                    for (unsigned int i = 0; i < count; ++i)
                    {
                        params[i] = (GLint)floatParams[i];
                    }
                }
                break;
              case GL_INT:
                memcpy(params, targetUniform->data + mUniformIndex[location].element * internalCount * sizeof(GLint),
                       count * sizeof(GLint));
                break;
              default: UNREACHABLE();
            }
        }
    }

    return true;
}

void ProgramBinary::dirtyAllUniforms()
{
    unsigned int numUniforms = mUniforms.size();
    for (unsigned int index = 0; index < numUniforms; index++)
    {
        mUniforms[index]->dirty = true;
    }
}

// Applies all the uniforms set for this program object to the Direct3D 9 device
void ProgramBinary::applyUniforms()
{
    for (std::vector<Uniform*>::iterator ub = mUniforms.begin(), ue = mUniforms.end(); ub != ue; ++ub) {
        Uniform *targetUniform = *ub;

        if (targetUniform->dirty)
        {
            int arraySize = targetUniform->arraySize;
            GLfloat *f = (GLfloat*)targetUniform->data;
            GLint *i = (GLint*)targetUniform->data;
            GLboolean *b = (GLboolean*)targetUniform->data;

            switch (targetUniform->type)
            {
              case GL_BOOL:       applyUniformnbv(targetUniform, arraySize, 1, b);    break;
              case GL_BOOL_VEC2:  applyUniformnbv(targetUniform, arraySize, 2, b);    break;
              case GL_BOOL_VEC3:  applyUniformnbv(targetUniform, arraySize, 3, b);    break;
              case GL_BOOL_VEC4:  applyUniformnbv(targetUniform, arraySize, 4, b);    break;
              case GL_FLOAT:
              case GL_FLOAT_VEC2:
              case GL_FLOAT_VEC3:
              case GL_FLOAT_VEC4:
              case GL_FLOAT_MAT2:
              case GL_FLOAT_MAT3:
              case GL_FLOAT_MAT4: applyUniformnfv(targetUniform, f);                  break;
              case GL_SAMPLER_2D:
              case GL_SAMPLER_CUBE:
              case GL_INT:        applyUniform1iv(targetUniform, arraySize, i);       break;
              case GL_INT_VEC2:   applyUniform2iv(targetUniform, arraySize, i);       break;
              case GL_INT_VEC3:   applyUniform3iv(targetUniform, arraySize, i);       break;
              case GL_INT_VEC4:   applyUniform4iv(targetUniform, arraySize, i);       break;
              default:
                UNREACHABLE();
            }

            targetUniform->dirty = false;
        }
    }
}

// Compiles the HLSL code of the attached shaders into executable binaries
ID3D10Blob *ProgramBinary::compileToBinary(InfoLog &infoLog, const char *hlsl, const char *profile, D3DConstantTable **constantTable)
{
    if (!hlsl)
    {
        return NULL;
    }

    HRESULT result = S_OK;
    UINT flags = 0;
    std::string sourceText;
    if (perfActive())
    {
        flags |= D3DCOMPILE_DEBUG;
#ifdef NDEBUG
        flags |= ANGLE_COMPILE_OPTIMIZATION_LEVEL;
#else
        flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        std::string sourcePath = getTempPath();
        sourceText = std::string("#line 2 \"") + sourcePath + std::string("\"\n\n") + std::string(hlsl);
        writeFile(sourcePath.c_str(), sourceText.c_str(), sourceText.size());
    }
    else
    {
        flags |= ANGLE_COMPILE_OPTIMIZATION_LEVEL;
        sourceText = hlsl;
    }

    // Sometimes D3DCompile will fail with the default compilation flags for complicated shaders when it would otherwise pass with alternative options.
    // Try the default flags first and if compilation fails, try some alternatives. 
    const static UINT extraFlags[] =
    {
        0,
        D3DCOMPILE_AVOID_FLOW_CONTROL,
        D3DCOMPILE_PREFER_FLOW_CONTROL
    };

    const static char * const extraFlagNames[] =
    {
        "default",
        "avoid flow control",
        "prefer flow control"
    };

    for (int i = 0; i < sizeof(extraFlags) / sizeof(UINT); ++i)
    {
        ID3D10Blob *errorMessage = NULL;
        ID3D10Blob *binary = NULL;
        result = getDisplay()->compileShaderSource(hlsl, g_fakepath, profile, flags | extraFlags[i], &binary, &errorMessage);
        if (errorMessage)
        {
            const char *message = (const char*)errorMessage->GetBufferPointer();

            infoLog.appendSanitized(message);
            TRACE("\n%s", hlsl);
            TRACE("\n%s", message);

            errorMessage->Release();
            errorMessage = NULL;
        }

        if (SUCCEEDED(result))
        {
            D3DConstantTable *table = new D3DConstantTable(binary->GetBufferPointer(), binary->GetBufferSize());
            if (table->error())
            {
                delete table;
                binary->Release();
                return NULL;
            }

            *constantTable = table;
    
            return binary;
        }
        else
        {
            if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
            {
                return error(GL_OUT_OF_MEMORY, (ID3D10Blob*) NULL);
            }

            infoLog.append("Warning: D3D shader compilation failed with ");
            infoLog.append(extraFlagNames[i]);
            infoLog.append(" flags.");
            if (i + 1 < sizeof(extraFlagNames) / sizeof(char*))
            {
                infoLog.append(" Retrying with ");
                infoLog.append(extraFlagNames[i + 1]);
                infoLog.append(".\n");
            }
        }
    }

    return NULL;
}

// Packs varyings into generic varying registers, using the algorithm from [OpenGL ES Shading Language 1.00 rev. 17] appendix A section 7 page 111
// Returns the number of used varying registers, or -1 if unsuccesful
int ProgramBinary::packVaryings(InfoLog &infoLog, const Varying *packing[][4], FragmentShader *fragmentShader)
{
    Context *context = getContext();
    const int maxVaryingVectors = context->getMaximumVaryingVectors();

    for (VaryingList::iterator varying = fragmentShader->mVaryings.begin(); varying != fragmentShader->mVaryings.end(); varying++)
    {
        int n = VariableRowCount(varying->type) * varying->size;
        int m = VariableColumnCount(varying->type);
        bool success = false;

        if (m == 2 || m == 3 || m == 4)
        {
            for (int r = 0; r <= maxVaryingVectors - n && !success; r++)
            {
                bool available = true;

                for (int y = 0; y < n && available; y++)
                {
                    for (int x = 0; x < m && available; x++)
                    {
                        if (packing[r + y][x])
                        {
                            available = false;
                        }
                    }
                }

                if (available)
                {
                    varying->reg = r;
                    varying->col = 0;

                    for (int y = 0; y < n; y++)
                    {
                        for (int x = 0; x < m; x++)
                        {
                            packing[r + y][x] = &*varying;
                        }
                    }

                    success = true;
                }
            }

            if (!success && m == 2)
            {
                for (int r = maxVaryingVectors - n; r >= 0 && !success; r--)
                {
                    bool available = true;

                    for (int y = 0; y < n && available; y++)
                    {
                        for (int x = 2; x < 4 && available; x++)
                        {
                            if (packing[r + y][x])
                            {
                                available = false;
                            }
                        }
                    }

                    if (available)
                    {
                        varying->reg = r;
                        varying->col = 2;

                        for (int y = 0; y < n; y++)
                        {
                            for (int x = 2; x < 4; x++)
                            {
                                packing[r + y][x] = &*varying;
                            }
                        }

                        success = true;
                    }
                }
            }
        }
        else if (m == 1)
        {
            int space[4] = {0};

            for (int y = 0; y < maxVaryingVectors; y++)
            {
                for (int x = 0; x < 4; x++)
                {
                    space[x] += packing[y][x] ? 0 : 1;
                }
            }

            int column = 0;

            for (int x = 0; x < 4; x++)
            {
                if (space[x] >= n && space[x] < space[column])
                {
                    column = x;
                }
            }

            if (space[column] >= n)
            {
                for (int r = 0; r < maxVaryingVectors; r++)
                {
                    if (!packing[r][column])
                    {
                        varying->reg = r;

                        for (int y = r; y < r + n; y++)
                        {
                            packing[y][column] = &*varying;
                        }

                        break;
                    }
                }

                varying->col = column;

                success = true;
            }
        }
        else UNREACHABLE();

        if (!success)
        {
            infoLog.append("Could not pack varying %s", varying->name.c_str());

            return -1;
        }
    }

    // Return the number of used registers
    int registers = 0;

    for (int r = 0; r < maxVaryingVectors; r++)
    {
        if (packing[r][0] || packing[r][1] || packing[r][2] || packing[r][3])
        {
            registers++;
        }
    }

    return registers;
}

bool ProgramBinary::linkVaryings(InfoLog &infoLog, std::string& pixelHLSL, std::string& vertexHLSL, FragmentShader *fragmentShader, VertexShader *vertexShader)
{
    if (pixelHLSL.empty() || vertexHLSL.empty())
    {
        return false;
    }

    // Reset the varying register assignments
    for (VaryingList::iterator fragVar = fragmentShader->mVaryings.begin(); fragVar != fragmentShader->mVaryings.end(); fragVar++)
    {
        fragVar->reg = -1;
        fragVar->col = -1;
    }

    for (VaryingList::iterator vtxVar = vertexShader->mVaryings.begin(); vtxVar != vertexShader->mVaryings.end(); vtxVar++)
    {
        vtxVar->reg = -1;
        vtxVar->col = -1;
    }

    // Map the varyings to the register file
    const Varying *packing[MAX_VARYING_VECTORS_SM3][4] = {NULL};
    int registers = packVaryings(infoLog, packing, fragmentShader);

    if (registers < 0)
    {
        return false;
    }

    // Write the HLSL input/output declarations
    Context *context = getContext();
    const bool sm3 = context->supportsShaderModel3();
    const int maxVaryingVectors = context->getMaximumVaryingVectors();

    if (registers == maxVaryingVectors && fragmentShader->mUsesFragCoord)
    {
        infoLog.append("No varying registers left to support gl_FragCoord");

        return false;
    }

    for (VaryingList::iterator input = fragmentShader->mVaryings.begin(); input != fragmentShader->mVaryings.end(); input++)
    {
        bool matched = false;

        for (VaryingList::iterator output = vertexShader->mVaryings.begin(); output != vertexShader->mVaryings.end(); output++)
        {
            if (output->name == input->name)
            {
                if (output->type != input->type || output->size != input->size)
                {
                    infoLog.append("Type of vertex varying %s does not match that of the fragment varying", output->name.c_str());

                    return false;
                }

                output->reg = input->reg;
                output->col = input->col;

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

    mUsesPointSize = vertexShader->mUsesPointSize;
    std::string varyingSemantic = (mUsesPointSize && sm3) ? "COLOR" : "TEXCOORD";

    vertexHLSL += "struct VS_INPUT\n"
                   "{\n";

    int semanticIndex = 0;
    for (AttributeArray::iterator attribute = vertexShader->mAttributes.begin(); attribute != vertexShader->mAttributes.end(); attribute++)
    {
        switch (attribute->type)
        {
          case GL_FLOAT:      vertexHLSL += "    float ";    break;
          case GL_FLOAT_VEC2: vertexHLSL += "    float2 ";   break;
          case GL_FLOAT_VEC3: vertexHLSL += "    float3 ";   break;
          case GL_FLOAT_VEC4: vertexHLSL += "    float4 ";   break;
          case GL_FLOAT_MAT2: vertexHLSL += "    float2x2 "; break;
          case GL_FLOAT_MAT3: vertexHLSL += "    float3x3 "; break;
          case GL_FLOAT_MAT4: vertexHLSL += "    float4x4 "; break;
          default:  UNREACHABLE();
        }

        vertexHLSL += decorateAttribute(attribute->name) + " : TEXCOORD" + str(semanticIndex) + ";\n";

        semanticIndex += VariableRowCount(attribute->type);
    }

    vertexHLSL += "};\n"
                   "\n"
                   "struct VS_OUTPUT\n"
                   "{\n"
                   "    float4 gl_Position : POSITION;\n";

    for (int r = 0; r < registers; r++)
    {
        int registerSize = packing[r][3] ? 4 : (packing[r][2] ? 3 : (packing[r][1] ? 2 : 1));

        vertexHLSL += "    float" + str(registerSize) + " v" + str(r) + " : " + varyingSemantic + str(r) + ";\n";
    }

    if (fragmentShader->mUsesFragCoord)
    {
        vertexHLSL += "    float4 gl_FragCoord : " + varyingSemantic + str(registers) + ";\n";
    }

    if (vertexShader->mUsesPointSize && sm3)
    {
        vertexHLSL += "    float gl_PointSize : PSIZE;\n";
    }

    vertexHLSL += "};\n"
                   "\n"
                   "VS_OUTPUT main(VS_INPUT input)\n"
                   "{\n";

    for (AttributeArray::iterator attribute = vertexShader->mAttributes.begin(); attribute != vertexShader->mAttributes.end(); attribute++)
    {
        vertexHLSL += "    " + decorateAttribute(attribute->name) + " = ";

        if (VariableRowCount(attribute->type) > 1)   // Matrix
        {
            vertexHLSL += "transpose";
        }

        vertexHLSL += "(input." + decorateAttribute(attribute->name) + ");\n";
    }

    vertexHLSL += "\n"
                   "    gl_main();\n"
                   "\n"
                   "    VS_OUTPUT output;\n"
                   "    output.gl_Position.x = gl_Position.x - dx_HalfPixelSize.x * gl_Position.w;\n"
                   "    output.gl_Position.y = -(gl_Position.y + dx_HalfPixelSize.y * gl_Position.w);\n"
                   "    output.gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n"
                   "    output.gl_Position.w = gl_Position.w;\n";

    if (vertexShader->mUsesPointSize && sm3)
    {
        vertexHLSL += "    output.gl_PointSize = gl_PointSize;\n";
    }

    if (fragmentShader->mUsesFragCoord)
    {
        vertexHLSL += "    output.gl_FragCoord = gl_Position;\n";
    }

    for (VaryingList::iterator varying = vertexShader->mVaryings.begin(); varying != vertexShader->mVaryings.end(); varying++)
    {
        if (varying->reg >= 0)
        {
            for (int i = 0; i < varying->size; i++)
            {
                int rows = VariableRowCount(varying->type);

                for (int j = 0; j < rows; j++)
                {
                    int r = varying->reg + i * rows + j;
                    vertexHLSL += "    output.v" + str(r);

                    bool sharedRegister = false;   // Register used by multiple varyings
                    
                    for (int x = 0; x < 4; x++)
                    {
                        if (packing[r][x] && packing[r][x] != packing[r][0])
                        {
                            sharedRegister = true;
                            break;
                        }
                    }

                    if(sharedRegister)
                    {
                        vertexHLSL += ".";

                        for (int x = 0; x < 4; x++)
                        {
                            if (packing[r][x] == &*varying)
                            {
                                switch(x)
                                {
                                  case 0: vertexHLSL += "x"; break;
                                  case 1: vertexHLSL += "y"; break;
                                  case 2: vertexHLSL += "z"; break;
                                  case 3: vertexHLSL += "w"; break;
                                }
                            }
                        }
                    }

                    vertexHLSL += " = " + varying->name;
                    
                    if (varying->array)
                    {
                        vertexHLSL += "[" + str(i) + "]";
                    }

                    if (rows > 1)
                    {
                        vertexHLSL += "[" + str(j) + "]";
                    }
                    
                    vertexHLSL += ";\n";
                }
            }
        }
    }

    vertexHLSL += "\n"
                   "    return output;\n"
                   "}\n";

    pixelHLSL += "struct PS_INPUT\n"
                  "{\n";
    
    for (VaryingList::iterator varying = fragmentShader->mVaryings.begin(); varying != fragmentShader->mVaryings.end(); varying++)
    {
        if (varying->reg >= 0)
        {
            for (int i = 0; i < varying->size; i++)
            {
                int rows = VariableRowCount(varying->type);
                for (int j = 0; j < rows; j++)
                {
                    std::string n = str(varying->reg + i * rows + j);
                    pixelHLSL += "    float4 v" + n + " : " + varyingSemantic + n + ";\n";
                }
            }
        }
        else UNREACHABLE();
    }

    if (fragmentShader->mUsesFragCoord)
    {
        pixelHLSL += "    float4 gl_FragCoord : " + varyingSemantic + str(registers) + ";\n";
        if (sm3) {
            pixelHLSL += "    float2 dx_VPos : VPOS;\n";
        }
    }

    if (fragmentShader->mUsesPointCoord && sm3)
    {
        pixelHLSL += "    float2 gl_PointCoord : TEXCOORD0;\n";
    }

    if (fragmentShader->mUsesFrontFacing)
    {
        pixelHLSL += "    float vFace : VFACE;\n";
    }

    pixelHLSL += "};\n"
                  "\n"
                  "struct PS_OUTPUT\n"
                  "{\n"
                  "    float4 gl_Color[1] : COLOR;\n";

    if (fragmentShader->mUsesFragDepth)
    {
        pixelHLSL += "    float gl_Depth : DEPTH;\n";
    }

    pixelHLSL +=  "};\n"
                  "\n"
                  "PS_OUTPUT main(PS_INPUT input)\n"
                  "{\n";

    if (fragmentShader->mUsesFragCoord)
    {
        pixelHLSL += "    float rhw = 1.0 / input.gl_FragCoord.w;\n";
        
        if (sm3)
        {
            pixelHLSL += "    gl_FragCoord.x = input.dx_VPos.x + 0.5;\n"
                          "    gl_FragCoord.y = input.dx_VPos.y + 0.5;\n";
        }
        else
        {
            // dx_Coord contains the viewport width/2, height/2, center.x and center.y. See Context::applyRenderTarget()
            pixelHLSL += "    gl_FragCoord.x = (input.gl_FragCoord.x * rhw) * dx_Coord.x + dx_Coord.z;\n"
                          "    gl_FragCoord.y = (input.gl_FragCoord.y * rhw) * dx_Coord.y + dx_Coord.w;\n";
        }
        
        pixelHLSL += "    gl_FragCoord.z = (input.gl_FragCoord.z * rhw) * dx_Depth.x + dx_Depth.y;\n"
                      "    gl_FragCoord.w = rhw;\n";
    }

    if (fragmentShader->mUsesPointCoord && sm3)
    {
        pixelHLSL += "    gl_PointCoord.x = input.gl_PointCoord.x;\n";
        pixelHLSL += "    gl_PointCoord.y = 1.0 - input.gl_PointCoord.y;\n";
    }

    if (fragmentShader->mUsesFrontFacing)
    {
        pixelHLSL += "    gl_FrontFacing = dx_PointsOrLines || (dx_FrontCCW ? (input.vFace >= 0.0) : (input.vFace <= 0.0));\n";
    }

    for (VaryingList::iterator varying = fragmentShader->mVaryings.begin(); varying != fragmentShader->mVaryings.end(); varying++)
    {
        if (varying->reg >= 0)
        {
            for (int i = 0; i < varying->size; i++)
            {
                int rows = VariableRowCount(varying->type);
                for (int j = 0; j < rows; j++)
                {
                    std::string n = str(varying->reg + i * rows + j);
                    pixelHLSL += "    " + varying->name;

                    if (varying->array)
                    {
                        pixelHLSL += "[" + str(i) + "]";
                    }

                    if (rows > 1)
                    {
                        pixelHLSL += "[" + str(j) + "]";
                    }

                    switch (VariableColumnCount(varying->type))
                    {
                      case 1: pixelHLSL += " = input.v" + n + ".x;\n";   break;
                      case 2: pixelHLSL += " = input.v" + n + ".xy;\n";  break;
                      case 3: pixelHLSL += " = input.v" + n + ".xyz;\n"; break;
                      case 4: pixelHLSL += " = input.v" + n + ";\n";     break;
                      default: UNREACHABLE();
                    }
                }
            }
        }
        else UNREACHABLE();
    }

    pixelHLSL += "\n"
                  "    gl_main();\n"
                  "\n"
                  "    PS_OUTPUT output;\n"
                  "    output.gl_Color[0] = gl_Color[0];\n";

    if (fragmentShader->mUsesFragDepth)
    {
        pixelHLSL += "    output.gl_Depth = gl_Depth;\n";
    }

    pixelHLSL += "\n"
                  "    return output;\n"
                  "}\n";

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

    int version = 0;
    stream.read(&version);
    if (version != VERSION_DWORD)
    {
        infoLog.append("Invalid program binary version.");
        return false;
    }

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
    {
        stream.read(&mLinkedAttribute[i].type);
        std::string name;
        stream.read(&name);
        mLinkedAttribute[i].name = name;
        stream.read(&mSemanticIndex[i]);
    }

    for (unsigned int i = 0; i < MAX_TEXTURE_IMAGE_UNITS; ++i)
    {
        stream.read(&mSamplersPS[i].active);
        stream.read(&mSamplersPS[i].logicalTextureUnit);
        
        int textureType;
        stream.read(&textureType);
        mSamplersPS[i].textureType = (TextureType) textureType;
    }

    for (unsigned int i = 0; i < MAX_VERTEX_TEXTURE_IMAGE_UNITS_VTF; ++i)
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
        std::string _name;
        unsigned int arraySize;

        stream.read(&type);
        stream.read(&_name);
        stream.read(&arraySize);

        mUniforms[i] = new Uniform(type, _name, arraySize);
        
        stream.read(&mUniforms[i]->ps.float4Index);
        stream.read(&mUniforms[i]->ps.samplerIndex);
        stream.read(&mUniforms[i]->ps.boolIndex);
        stream.read(&mUniforms[i]->ps.registerCount);

        stream.read(&mUniforms[i]->vs.float4Index);
        stream.read(&mUniforms[i]->vs.samplerIndex);
        stream.read(&mUniforms[i]->vs.boolIndex);
        stream.read(&mUniforms[i]->vs.registerCount);
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

    stream.read(&mDxDepthRangeLocation);
    stream.read(&mDxDepthLocation);
    stream.read(&mDxCoordLocation);
    stream.read(&mDxHalfPixelSizeLocation);
    stream.read(&mDxFrontCCWLocation);
    stream.read(&mDxPointsOrLinesLocation);

    unsigned int pixelShaderSize;
    stream.read(&pixelShaderSize);

    unsigned int vertexShaderSize;
    stream.read(&vertexShaderSize);

    const char *ptr = (const char*) binary + stream.offset();

    const D3DCAPS9 *binaryIdentifier = (const D3DCAPS9*) ptr;
    ptr += sizeof(GUID);

    D3DADAPTER_IDENTIFIER9 *currentIdentifier = getDisplay()->getAdapterIdentifier();
    if (memcmp(&currentIdentifier->DeviceIdentifier, binaryIdentifier, sizeof(GUID)) != 0)
    {
        infoLog.append("Invalid program binary.");
        return false;
    }

    const char *pixelShaderFunction = ptr;
    ptr += pixelShaderSize;

    const char *vertexShaderFunction = ptr;
    ptr += vertexShaderSize;

    mPixelExecutable = getDisplay()->createPixelShader(reinterpret_cast<const DWORD*>(pixelShaderFunction), pixelShaderSize);
    if (!mPixelExecutable)
    {
        infoLog.append("Could not create pixel shader.");
        return false;
    }

    mVertexExecutable = getDisplay()->createVertexShader(reinterpret_cast<const DWORD*>(vertexShaderFunction), vertexShaderSize);
    if (!mVertexExecutable)
    {
        infoLog.append("Could not create vertex shader.");
        mPixelExecutable->Release();
        mPixelExecutable = NULL;
        return false;
    }

    return true;
}

bool ProgramBinary::save(void* binary, GLsizei bufSize, GLsizei *length)
{
    BinaryOutputStream stream;

    stream.write(GL_PROGRAM_BINARY_ANGLE);
    stream.write(VERSION_DWORD);

    for (unsigned int i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
    {
        stream.write(mLinkedAttribute[i].type);
        stream.write(mLinkedAttribute[i].name);
        stream.write(mSemanticIndex[i]);
    }

    for (unsigned int i = 0; i < MAX_TEXTURE_IMAGE_UNITS; ++i)
    {
        stream.write(mSamplersPS[i].active);
        stream.write(mSamplersPS[i].logicalTextureUnit);
        stream.write((int) mSamplersPS[i].textureType);
    }

    for (unsigned int i = 0; i < MAX_VERTEX_TEXTURE_IMAGE_UNITS_VTF; ++i)
    {
        stream.write(mSamplersVS[i].active);
        stream.write(mSamplersVS[i].logicalTextureUnit);
        stream.write((int) mSamplersVS[i].textureType);
    }

    stream.write(mUsedVertexSamplerRange);
    stream.write(mUsedPixelSamplerRange);
    stream.write(mUsesPointSize);

    stream.write(mUniforms.size());
    for (unsigned int i = 0; i < mUniforms.size(); ++i)
    {
        stream.write(mUniforms[i]->type);
        stream.write(mUniforms[i]->_name);
        stream.write(mUniforms[i]->arraySize);

        stream.write(mUniforms[i]->ps.float4Index);
        stream.write(mUniforms[i]->ps.samplerIndex);
        stream.write(mUniforms[i]->ps.boolIndex);
        stream.write(mUniforms[i]->ps.registerCount);

        stream.write(mUniforms[i]->vs.float4Index);
        stream.write(mUniforms[i]->vs.samplerIndex);
        stream.write(mUniforms[i]->vs.boolIndex);
        stream.write(mUniforms[i]->vs.registerCount);
    }

    stream.write(mUniformIndex.size());
    for (unsigned int i = 0; i < mUniformIndex.size(); ++i)
    {
        stream.write(mUniformIndex[i].name);
        stream.write(mUniformIndex[i].element);
        stream.write(mUniformIndex[i].index);
    }

    stream.write(mDxDepthRangeLocation);
    stream.write(mDxDepthLocation);
    stream.write(mDxCoordLocation);
    stream.write(mDxHalfPixelSizeLocation);
    stream.write(mDxFrontCCWLocation);
    stream.write(mDxPointsOrLinesLocation);

    UINT pixelShaderSize;
    HRESULT result = mPixelExecutable->GetFunction(NULL, &pixelShaderSize);
    ASSERT(SUCCEEDED(result));
    stream.write(pixelShaderSize);

    UINT vertexShaderSize;
    result = mVertexExecutable->GetFunction(NULL, &vertexShaderSize);
    ASSERT(SUCCEEDED(result));
    stream.write(vertexShaderSize);

    D3DADAPTER_IDENTIFIER9 *identifier = getDisplay()->getAdapterIdentifier();

    GLsizei streamLength = stream.length();
    const void *streamData = stream.data();

    GLsizei totalLength = streamLength + sizeof(GUID) + pixelShaderSize + vertexShaderSize;
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

        memcpy(ptr, &identifier->DeviceIdentifier, sizeof(GUID));
        ptr += sizeof(GUID);

        result = mPixelExecutable->GetFunction(ptr, &pixelShaderSize);
        ASSERT(SUCCEEDED(result));
        ptr += pixelShaderSize;

        result = mVertexExecutable->GetFunction(ptr, &vertexShaderSize);
        ASSERT(SUCCEEDED(result));
        ptr += vertexShaderSize;

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

bool ProgramBinary::link(InfoLog &infoLog, const AttributeBindings &attributeBindings, FragmentShader *fragmentShader, VertexShader *vertexShader)
{
    if (!fragmentShader || !fragmentShader->isCompiled())
    {
        return false;
    }

    if (!vertexShader || !vertexShader->isCompiled())
    {
        return false;
    }

    std::string pixelHLSL = fragmentShader->getHLSL();
    std::string vertexHLSL = vertexShader->getHLSL();

    if (!linkVaryings(infoLog, pixelHLSL, vertexHLSL, fragmentShader, vertexShader))
    {
        return false;
    }

    Context *context = getContext();
    const char *vertexProfile = context->supportsShaderModel3() ? "vs_3_0" : "vs_2_0";
    const char *pixelProfile = context->supportsShaderModel3() ? "ps_3_0" : "ps_2_0";

    ID3D10Blob *vertexBinary = compileToBinary(infoLog, vertexHLSL.c_str(), vertexProfile, &mConstantTableVS);
    ID3D10Blob *pixelBinary = compileToBinary(infoLog, pixelHLSL.c_str(), pixelProfile, &mConstantTablePS);

    if (vertexBinary && pixelBinary)
    {
        mVertexExecutable = getDisplay()->createVertexShader((DWORD*)vertexBinary->GetBufferPointer(), vertexBinary->GetBufferSize());
        if (!mVertexExecutable)
        {
            return error(GL_OUT_OF_MEMORY, false);
        }

        mPixelExecutable = getDisplay()->createPixelShader((DWORD*)pixelBinary->GetBufferPointer(), pixelBinary->GetBufferSize());
        if (!mPixelExecutable)
        {
            mVertexExecutable->Release();
            mVertexExecutable = NULL;
            return error(GL_OUT_OF_MEMORY, false);
        }

        vertexBinary->Release();
        pixelBinary->Release();
        vertexBinary = NULL;
        pixelBinary = NULL;

        if (!linkAttributes(infoLog, attributeBindings, fragmentShader, vertexShader))
        {
            return false;
        }

        if (!linkUniforms(infoLog, GL_FRAGMENT_SHADER, mConstantTablePS))
        {
            return false;
        }

        if (!linkUniforms(infoLog, GL_VERTEX_SHADER, mConstantTableVS))
        {
            return false;
        }

        // these uniforms are searched as already-decorated because gl_ and dx_
        // are reserved prefixes, and do not receive additional decoration
        mDxDepthRangeLocation = getUniformLocation("dx_DepthRange");
        mDxDepthLocation = getUniformLocation("dx_Depth");
        mDxCoordLocation = getUniformLocation("dx_Coord");
        mDxHalfPixelSizeLocation = getUniformLocation("dx_HalfPixelSize");
        mDxFrontCCWLocation = getUniformLocation("dx_FrontCCW");
        mDxPointsOrLinesLocation = getUniformLocation("dx_PointsOrLines");

        context->markDxUniformsDirty();

        return true;
    }

    return false;
}

// Determines the mapping between GL attributes and Direct3D 9 vertex stream usage indices
bool ProgramBinary::linkAttributes(InfoLog &infoLog, const AttributeBindings &attributeBindings, FragmentShader *fragmentShader, VertexShader *vertexShader)
{
    unsigned int usedLocations = 0;

    // Link attributes that have a binding location
    for (AttributeArray::iterator attribute = vertexShader->mAttributes.begin(); attribute != vertexShader->mAttributes.end(); attribute++)
    {
        int location = attributeBindings.getAttributeBinding(attribute->name);

        if (location != -1)   // Set by glBindAttribLocation
        {
            if (!mLinkedAttribute[location].name.empty())
            {
                // Multiple active attributes bound to the same location; not an error
            }

            mLinkedAttribute[location] = *attribute;

            int rows = VariableRowCount(attribute->type);

            if (rows + location > MAX_VERTEX_ATTRIBS)
            {
                infoLog.append("Active attribute (%s) at location %d is too big to fit", attribute->name.c_str(), location);

                return false;
            }

            for (int i = 0; i < rows; i++)
            {
                usedLocations |= 1 << (location + i);
            }
        }
    }

    // Link attributes that don't have a binding location
    for (AttributeArray::iterator attribute = vertexShader->mAttributes.begin(); attribute != vertexShader->mAttributes.end(); attribute++)
    {
        int location = attributeBindings.getAttributeBinding(attribute->name);

        if (location == -1)   // Not set by glBindAttribLocation
        {
            int rows = VariableRowCount(attribute->type);
            int availableIndex = AllocateFirstFreeBits(&usedLocations, rows, MAX_VERTEX_ATTRIBS);

            if (availableIndex == -1 || availableIndex + rows > MAX_VERTEX_ATTRIBS)
            {
                infoLog.append("Too many active attributes (%s)", attribute->name.c_str());

                return false;   // Fail to link
            }

            mLinkedAttribute[availableIndex] = *attribute;
        }
    }

    for (int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; )
    {
        int index = vertexShader->getSemanticIndex(mLinkedAttribute[attributeIndex].name);
        int rows = std::max(VariableRowCount(mLinkedAttribute[attributeIndex].type), 1);

        for (int r = 0; r < rows; r++)
        {
            mSemanticIndex[attributeIndex++] = index++;
        }
    }

    return true;
}

bool ProgramBinary::linkUniforms(InfoLog &infoLog, GLenum shader, D3DConstantTable *constantTable)
{
    for (unsigned int constantIndex = 0; constantIndex < constantTable->constants(); constantIndex++)
    {
        const D3DConstant *constant = constantTable->getConstant(constantIndex);

        if (!defineUniform(infoLog, shader, constant))
        {
            return false;
        }
    }

    return true;
}

// Adds the description of a constant found in the binary shader to the list of uniforms
// Returns true if succesful (uniform not already defined)
bool ProgramBinary::defineUniform(InfoLog &infoLog, GLenum shader, const D3DConstant *constant, std::string name)
{
    if (constant->registerSet == D3DConstant::RS_SAMPLER)
    {
        for (unsigned int i = 0; i < constant->registerCount; i++)
        {
            const D3DConstant *psConstant = mConstantTablePS->getConstantByName(constant->name.c_str());
            const D3DConstant *vsConstant = mConstantTableVS->getConstantByName(constant->name.c_str());

            if (psConstant)
            {
                unsigned int samplerIndex = psConstant->registerIndex + i;

                if (samplerIndex < MAX_TEXTURE_IMAGE_UNITS)
                {
                    mSamplersPS[samplerIndex].active = true;
                    mSamplersPS[samplerIndex].textureType = (constant->type == D3DConstant::PT_SAMPLERCUBE) ? TEXTURE_CUBE : TEXTURE_2D;
                    mSamplersPS[samplerIndex].logicalTextureUnit = 0;
                    mUsedPixelSamplerRange = std::max(samplerIndex + 1, mUsedPixelSamplerRange);
                }
                else
                {
                    infoLog.append("Pixel shader sampler count exceeds MAX_TEXTURE_IMAGE_UNITS (%d).", MAX_TEXTURE_IMAGE_UNITS);
                    return false;
                }
            }
            
            if (vsConstant)
            {
                unsigned int samplerIndex = vsConstant->registerIndex + i;

                if (samplerIndex < getContext()->getMaximumVertexTextureImageUnits())
                {
                    mSamplersVS[samplerIndex].active = true;
                    mSamplersVS[samplerIndex].textureType = (constant->type == D3DConstant::PT_SAMPLERCUBE) ? TEXTURE_CUBE : TEXTURE_2D;
                    mSamplersVS[samplerIndex].logicalTextureUnit = 0;
                    mUsedVertexSamplerRange = std::max(samplerIndex + 1, mUsedVertexSamplerRange);
                }
                else
                {
                    infoLog.append("Vertex shader sampler count exceeds MAX_VERTEX_TEXTURE_IMAGE_UNITS (%d).", getContext()->getMaximumVertexTextureImageUnits());
                    return false;
                }
            }
        }
    }

    switch(constant->typeClass)
    {
      case D3DConstant::CLASS_STRUCT:
        {
            for (unsigned int arrayIndex = 0; arrayIndex < constant->elements; arrayIndex++)
            {
                for (unsigned int field = 0; field < constant->structMembers[arrayIndex].size(); field++)
                {
                    const D3DConstant *fieldConstant = constant->structMembers[arrayIndex][field];

                    std::string structIndex = (constant->elements > 1) ? ("[" + str(arrayIndex) + "]") : "";

                    if (!defineUniform(infoLog, shader, fieldConstant, name + constant->name + structIndex + "."))
                    {
                        return false;
                    }
                }
            }

            return true;
        }
      case D3DConstant::CLASS_SCALAR:
      case D3DConstant::CLASS_VECTOR:
      case D3DConstant::CLASS_MATRIX_COLUMNS:
      case D3DConstant::CLASS_OBJECT:
        return defineUniform(shader, constant, name + constant->name);
      default:
        UNREACHABLE();
        return false;
    }
}

bool ProgramBinary::defineUniform(GLenum shader, const D3DConstant *constant, const std::string &_name)
{
    Uniform *uniform = createUniform(constant, _name);

    if(!uniform)
    {
        return false;
    }

    // Check if already defined
    GLint location = getUniformLocation(uniform->name);
    GLenum type = uniform->type;

    if (location >= 0)
    {
        delete uniform;
        uniform = mUniforms[mUniformIndex[location].index];
    }

    if (shader == GL_FRAGMENT_SHADER) uniform->ps.set(constant);
    if (shader == GL_VERTEX_SHADER)   uniform->vs.set(constant);

    if (location >= 0)
    {
        return uniform->type == type;
    }

    mUniforms.push_back(uniform);
    unsigned int uniformIndex = mUniforms.size() - 1;

    for (unsigned int i = 0; i < uniform->arraySize; ++i)
    {
        mUniformIndex.push_back(UniformLocation(_name, i, uniformIndex));
    }

    return true;
}

Uniform *ProgramBinary::createUniform(const D3DConstant *constant, const std::string &_name)
{
    if (constant->rows == 1)   // Vectors and scalars
    {
        switch (constant->type)
        {
          case D3DConstant::PT_SAMPLER2D:
            switch (constant->columns)
            {
              case 1: return new Uniform(GL_SAMPLER_2D, _name, constant->elements);
              default: UNREACHABLE();
            }
            break;
          case D3DConstant::PT_SAMPLERCUBE:
            switch (constant->columns)
            {
              case 1: return new Uniform(GL_SAMPLER_CUBE, _name, constant->elements);
              default: UNREACHABLE();
            }
            break;
          case D3DConstant::PT_BOOL:
            switch (constant->columns)
            {
              case 1: return new Uniform(GL_BOOL, _name, constant->elements);
              case 2: return new Uniform(GL_BOOL_VEC2, _name, constant->elements);
              case 3: return new Uniform(GL_BOOL_VEC3, _name, constant->elements);
              case 4: return new Uniform(GL_BOOL_VEC4, _name, constant->elements);
              default: UNREACHABLE();
            }
            break;
          case D3DConstant::PT_INT:
            switch (constant->columns)
            {
              case 1: return new Uniform(GL_INT, _name, constant->elements);
              case 2: return new Uniform(GL_INT_VEC2, _name, constant->elements);
              case 3: return new Uniform(GL_INT_VEC3, _name, constant->elements);
              case 4: return new Uniform(GL_INT_VEC4, _name, constant->elements);
              default: UNREACHABLE();
            }
            break;
          case D3DConstant::PT_FLOAT:
            switch (constant->columns)
            {
              case 1: return new Uniform(GL_FLOAT, _name, constant->elements);
              case 2: return new Uniform(GL_FLOAT_VEC2, _name, constant->elements);
              case 3: return new Uniform(GL_FLOAT_VEC3, _name, constant->elements);
              case 4: return new Uniform(GL_FLOAT_VEC4, _name, constant->elements);
              default: UNREACHABLE();
            }
            break;
          default:
            UNREACHABLE();
        }
    }
    else if (constant->rows == constant->columns)  // Square matrices
    {
        switch (constant->type)
        {
          case D3DConstant::PT_FLOAT:
            switch (constant->rows)
            {
              case 2: return new Uniform(GL_FLOAT_MAT2, _name, constant->elements);
              case 3: return new Uniform(GL_FLOAT_MAT3, _name, constant->elements);
              case 4: return new Uniform(GL_FLOAT_MAT4, _name, constant->elements);
              default: UNREACHABLE();
            }
            break;
          default: UNREACHABLE();
        }
    }
    else UNREACHABLE();

    return 0;
}

// This method needs to match OutputHLSL::decorate
std::string ProgramBinary::decorateAttribute(const std::string &name)
{
    if (name.compare(0, 3, "gl_") != 0 && name.compare(0, 3, "dx_") != 0)
    {
        return "_" + name;
    }
    
    return name;
}

std::string ProgramBinary::undecorateUniform(const std::string &_name)
{
    std::string name = _name;
    
    // Remove any structure field decoration
    size_t pos = 0;
    while ((pos = name.find("._", pos)) != std::string::npos)
    {
        name.replace(pos, 2, ".");
    }

    // Remove the leading decoration
    if (name[0] == '_')
    {
        return name.substr(1);
    }
    else if (name.compare(0, 3, "ar_") == 0)
    {
        return name.substr(3);
    }
    
    return name;
}

void ProgramBinary::applyUniformnbv(Uniform *targetUniform, GLsizei count, int width, const GLboolean *v)
{
    float vector[D3D9_MAX_FLOAT_CONSTANTS * 4];
    BOOL boolVector[D3D9_MAX_BOOL_CONSTANTS];

    if (targetUniform->ps.float4Index >= 0 || targetUniform->vs.float4Index >= 0)
    {
        ASSERT(count <= D3D9_MAX_FLOAT_CONSTANTS);
        for (int i = 0; i < count; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                if (j < width)
                {
                    vector[i * 4 + j] = (v[i * width + j] == GL_FALSE) ? 0.0f : 1.0f;
                }
                else
                {
                    vector[i * 4 + j] = 0.0f;
                }
            }
        }
    }

    if (targetUniform->ps.boolIndex >= 0 || targetUniform->vs.boolIndex >= 0)
    {
        int psCount = targetUniform->ps.boolIndex >= 0 ? targetUniform->ps.registerCount : 0;
        int vsCount = targetUniform->vs.boolIndex >= 0 ? targetUniform->vs.registerCount : 0;
        int copyCount = std::min(count * width, std::max(psCount, vsCount));
        ASSERT(copyCount <= D3D9_MAX_BOOL_CONSTANTS);
        for (int i = 0; i < copyCount; i++)
        {
            boolVector[i] = v[i] != GL_FALSE;
        }
    }

    if (targetUniform->ps.float4Index >= 0)
    {
        mDevice->SetPixelShaderConstantF(targetUniform->ps.float4Index, vector, targetUniform->ps.registerCount);
    }
        
    if (targetUniform->ps.boolIndex >= 0)
    {
        mDevice->SetPixelShaderConstantB(targetUniform->ps.boolIndex, boolVector, targetUniform->ps.registerCount);
    }
    
    if (targetUniform->vs.float4Index >= 0)
    {
        mDevice->SetVertexShaderConstantF(targetUniform->vs.float4Index, vector, targetUniform->vs.registerCount);
    }
        
    if (targetUniform->vs.boolIndex >= 0)
    {
        mDevice->SetVertexShaderConstantB(targetUniform->vs.boolIndex, boolVector, targetUniform->vs.registerCount);
    }
}

bool ProgramBinary::applyUniformnfv(Uniform *targetUniform, const GLfloat *v)
{
    if (targetUniform->ps.registerCount)
    {
        mDevice->SetPixelShaderConstantF(targetUniform->ps.float4Index, v, targetUniform->ps.registerCount);
    }

    if (targetUniform->vs.registerCount)
    {
        mDevice->SetVertexShaderConstantF(targetUniform->vs.float4Index, v, targetUniform->vs.registerCount);
    }

    return true;
}

bool ProgramBinary::applyUniform1iv(Uniform *targetUniform, GLsizei count, const GLint *v)
{
    ASSERT(count <= D3D9_MAX_FLOAT_CONSTANTS);
    Vector4 vector[D3D9_MAX_FLOAT_CONSTANTS];

    for (int i = 0; i < count; i++)
    {
        vector[i] = Vector4((float)v[i], 0, 0, 0);
    }

    if (targetUniform->ps.registerCount)
    {
        if (targetUniform->ps.samplerIndex >= 0)
        {
            unsigned int firstIndex = targetUniform->ps.samplerIndex;

            for (int i = 0; i < count; i++)
            {
                unsigned int samplerIndex = firstIndex + i;

                if (samplerIndex < MAX_TEXTURE_IMAGE_UNITS)
                {
                    ASSERT(mSamplersPS[samplerIndex].active);
                    mSamplersPS[samplerIndex].logicalTextureUnit = v[i];
                }
            }
        }
        else
        {
            ASSERT(targetUniform->ps.float4Index >= 0);
            mDevice->SetPixelShaderConstantF(targetUniform->ps.float4Index, (const float*)vector, targetUniform->ps.registerCount);
        }
    }

    if (targetUniform->vs.registerCount)
    {
        if (targetUniform->vs.samplerIndex >= 0)
        {
            unsigned int firstIndex = targetUniform->vs.samplerIndex;

            for (int i = 0; i < count; i++)
            {
                unsigned int samplerIndex = firstIndex + i;

                if (samplerIndex < MAX_VERTEX_TEXTURE_IMAGE_UNITS_VTF)
                {
                    ASSERT(mSamplersVS[samplerIndex].active);
                    mSamplersVS[samplerIndex].logicalTextureUnit = v[i];
                }
            }
        }
        else
        {
            ASSERT(targetUniform->vs.float4Index >= 0);
            mDevice->SetVertexShaderConstantF(targetUniform->vs.float4Index, (const float *)vector, targetUniform->vs.registerCount);
        }
    }

    return true;
}

bool ProgramBinary::applyUniform2iv(Uniform *targetUniform, GLsizei count, const GLint *v)
{
    ASSERT(count <= D3D9_MAX_FLOAT_CONSTANTS);
    Vector4 vector[D3D9_MAX_FLOAT_CONSTANTS];

    for (int i = 0; i < count; i++)
    {
        vector[i] = Vector4((float)v[0], (float)v[1], 0, 0);

        v += 2;
    }

    applyUniformniv(targetUniform, count, vector);

    return true;
}

bool ProgramBinary::applyUniform3iv(Uniform *targetUniform, GLsizei count, const GLint *v)
{
    ASSERT(count <= D3D9_MAX_FLOAT_CONSTANTS);
    Vector4 vector[D3D9_MAX_FLOAT_CONSTANTS];

    for (int i = 0; i < count; i++)
    {
        vector[i] = Vector4((float)v[0], (float)v[1], (float)v[2], 0);

        v += 3;
    }

    applyUniformniv(targetUniform, count, vector);

    return true;
}

bool ProgramBinary::applyUniform4iv(Uniform *targetUniform, GLsizei count, const GLint *v)
{
    ASSERT(count <= D3D9_MAX_FLOAT_CONSTANTS);
    Vector4 vector[D3D9_MAX_FLOAT_CONSTANTS];

    for (int i = 0; i < count; i++)
    {
        vector[i] = Vector4((float)v[0], (float)v[1], (float)v[2], (float)v[3]);

        v += 4;
    }

    applyUniformniv(targetUniform, count, vector);

    return true;
}

void ProgramBinary::applyUniformniv(Uniform *targetUniform, GLsizei count, const Vector4 *vector)
{
    if (targetUniform->ps.registerCount)
    {
        ASSERT(targetUniform->ps.float4Index >= 0);
        mDevice->SetPixelShaderConstantF(targetUniform->ps.float4Index, (const float *)vector, targetUniform->ps.registerCount);
    }

    if (targetUniform->vs.registerCount)
    {
        ASSERT(targetUniform->vs.float4Index >= 0);
        mDevice->SetVertexShaderConstantF(targetUniform->vs.float4Index, (const float *)vector, targetUniform->vs.registerCount);
    }
}

bool ProgramBinary::isValidated() const 
{
    return mValidated;
}

void ProgramBinary::getActiveAttribute(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
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

GLint ProgramBinary::getActiveAttributeCount()
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

GLint ProgramBinary::getActiveAttributeMaxLength()
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

void ProgramBinary::getActiveUniform(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    // Skip over internal uniforms
    unsigned int activeUniform = 0;
    unsigned int uniform;
    for (uniform = 0; uniform < mUniforms.size(); uniform++)
    {
        if (mUniforms[uniform]->name.compare(0, 3, "dx_") == 0)
        {
            continue;
        }

        if (activeUniform == index)
        {
            break;
        }

        activeUniform++;
    }

    ASSERT(uniform < mUniforms.size());   // index must be smaller than getActiveUniformCount()

    if (bufsize > 0)
    {
        std::string string = mUniforms[uniform]->name;

        if (mUniforms[uniform]->isArray())
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

    *size = mUniforms[uniform]->arraySize;

    *type = mUniforms[uniform]->type;
}

GLint ProgramBinary::getActiveUniformCount()
{
    int count = 0;

    unsigned int numUniforms = mUniforms.size();
    for (unsigned int uniformIndex = 0; uniformIndex < numUniforms; uniformIndex++)
    {
        if (mUniforms[uniformIndex]->name.compare(0, 3, "dx_") != 0)
        {
            count++;
        }
    }

    return count;
}

GLint ProgramBinary::getActiveUniformMaxLength()
{
    int maxLength = 0;

    unsigned int numUniforms = mUniforms.size();
    for (unsigned int uniformIndex = 0; uniformIndex < numUniforms; uniformIndex++)
    {
        if (!mUniforms[uniformIndex]->name.empty() && mUniforms[uniformIndex]->name.compare(0, 3, "dx_") != 0)
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

    const unsigned int maxCombinedTextureImageUnits = getContext()->getMaximumCombinedTextureImageUnits();
    TextureType textureUnitType[MAX_COMBINED_TEXTURE_IMAGE_UNITS_VTF];

    for (unsigned int i = 0; i < MAX_COMBINED_TEXTURE_IMAGE_UNITS_VTF; ++i)
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
                    infoLog->append("Sampler uniform (%d) exceeds MAX_COMBINED_TEXTURE_IMAGE_UNITS (%d)", unit, maxCombinedTextureImageUnits);
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
                    infoLog->append("Sampler uniform (%d) exceeds MAX_COMBINED_TEXTURE_IMAGE_UNITS (%d)", unit, maxCombinedTextureImageUnits);
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

GLint ProgramBinary::getDxDepthRangeLocation() const
{
    return mDxDepthRangeLocation;
}

GLint ProgramBinary::getDxDepthLocation() const
{
    return mDxDepthLocation;
}

GLint ProgramBinary::getDxCoordLocation() const
{
    return mDxCoordLocation;
}

GLint ProgramBinary::getDxHalfPixelSizeLocation() const
{
    return mDxHalfPixelSizeLocation;
}

GLint ProgramBinary::getDxFrontCCWLocation() const
{
    return mDxFrontCCWLocation;
}

GLint ProgramBinary::getDxPointsOrLinesLocation() const
{
    return mDxPointsOrLinesLocation;
}

ProgramBinary::Sampler::Sampler() : active(false), logicalTextureUnit(0), textureType(TEXTURE_2D)
{
}

}
