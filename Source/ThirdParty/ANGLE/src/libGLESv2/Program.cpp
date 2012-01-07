//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.cpp: Implements the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#include "libGLESv2/Program.h"

#include "common/debug.h"

#include "libGLESv2/main.h"
#include "libGLESv2/Shader.h"
#include "libGLESv2/utilities.h"

#include <string>

#if !defined(ANGLE_COMPILE_OPTIMIZATION_LEVEL)
#define ANGLE_COMPILE_OPTIMIZATION_LEVEL D3DCOMPILE_OPTIMIZATION_LEVEL3
#endif

namespace gl
{
unsigned int Program::mCurrentSerial = 1;
const char *fakepath = "C:\\fakepath";

std::string str(int i)
{
    char buffer[20];
    sprintf(buffer, "%d", i);
    return buffer;
}

Uniform::Uniform(GLenum type, const std::string &_name, unsigned int arraySize)
    : type(type), _name(_name), name(Program::undecorateUniform(_name)), arraySize(arraySize)
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
    return _name.compare(0, 3, "ar_") == 0;
}

UniformLocation::UniformLocation(const std::string &_name, unsigned int element, unsigned int index) 
    : name(Program::undecorateUniform(_name)), element(element), index(index)
{
}

Program::Program(ResourceManager *manager, GLuint handle) : mResourceManager(manager), mHandle(handle), mSerial(issueSerial())
{
    mDevice = getDevice();
    mFragmentShader = NULL;
    mVertexShader = NULL;

    mPixelExecutable = NULL;
    mVertexExecutable = NULL;
    mConstantTablePS = NULL;
    mConstantTableVS = NULL;

    mInfoLog = NULL;
    mValidated = false;

    unlink();

    mDeleteStatus = false;

    mRefCount = 0;
}

Program::~Program()
{
    unlink(true);

    if (mVertexShader != NULL)
    {
        mVertexShader->release();
    }

    if (mFragmentShader != NULL)
    {
        mFragmentShader->release();
    }
}

bool Program::attachShader(Shader *shader)
{
    if (shader->getType() == GL_VERTEX_SHADER)
    {
        if (mVertexShader)
        {
            return false;
        }

        mVertexShader = (VertexShader*)shader;
        mVertexShader->addRef();
    }
    else if (shader->getType() == GL_FRAGMENT_SHADER)
    {
        if (mFragmentShader)
        {
            return false;
        }

        mFragmentShader = (FragmentShader*)shader;
        mFragmentShader->addRef();
    }
    else UNREACHABLE();

    return true;
}

bool Program::detachShader(Shader *shader)
{
    if (shader->getType() == GL_VERTEX_SHADER)
    {
        if (mVertexShader != shader)
        {
            return false;
        }

        mVertexShader->release();
        mVertexShader = NULL;
    }
    else if (shader->getType() == GL_FRAGMENT_SHADER)
    {
        if (mFragmentShader != shader)
        {
            return false;
        }

        mFragmentShader->release();
        mFragmentShader = NULL;
    }
    else UNREACHABLE();

    return true;
}

int Program::getAttachedShadersCount() const
{
    return (mVertexShader ? 1 : 0) + (mFragmentShader ? 1 : 0);
}

IDirect3DPixelShader9 *Program::getPixelShader()
{
    return mPixelExecutable;
}

IDirect3DVertexShader9 *Program::getVertexShader()
{
    return mVertexExecutable;
}

void Program::bindAttributeLocation(GLuint index, const char *name)
{
    if (index < MAX_VERTEX_ATTRIBS)
    {
        for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
        {
            mAttributeBinding[i].erase(name);
        }

        mAttributeBinding[index].insert(name);
    }
}

GLuint Program::getAttributeLocation(const char *name)
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

int Program::getSemanticIndex(int attributeIndex)
{
    ASSERT(attributeIndex >= 0 && attributeIndex < MAX_VERTEX_ATTRIBS);
    
    return mSemanticIndex[attributeIndex];
}

// Returns one more than the highest sampler index used.
GLint Program::getUsedSamplerRange(SamplerType type)
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

// Returns the index of the texture image unit (0-19) corresponding to a Direct3D 9 sampler
// index (0-15 for the pixel shader and 0-3 for the vertex shader).
GLint Program::getSamplerMapping(SamplerType type, unsigned int samplerIndex)
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
TextureType Program::getSamplerTextureType(SamplerType type, unsigned int samplerIndex)
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

GLint Program::getUniformLocation(std::string name)
{
    int subscript = 0;

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

bool Program::setUniform1fv(GLint location, GLsizei count, const GLfloat* v)
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
        GLboolean *boolParams = new GLboolean[count];

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

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLboolean),
               boolParams, sizeof(GLboolean) * count);

        delete [] boolParams;
    }
    else
    {
        return false;
    }

    return true;
}

bool Program::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
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

        GLboolean *boolParams = new GLboolean[count * 2];

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

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLboolean) * 2,
               boolParams, 2 * sizeof(GLboolean) * count);

        delete [] boolParams;
    }
    else 
    {
        return false;
    }

    return true;
}

bool Program::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
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
        GLboolean *boolParams = new GLboolean[count * 3];

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

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLboolean) * 3,
               boolParams, 3 * sizeof(GLboolean) * count);

        delete [] boolParams;
    }
    else 
    {
        return false;
    }

    return true;
}

bool Program::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
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
        GLboolean *boolParams = new GLboolean[count * 4];

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

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLboolean) * 4,
               boolParams, 4 * sizeof(GLboolean) * count);

        delete [] boolParams;
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

bool Program::setUniformMatrix2fv(GLint location, GLsizei count, const GLfloat *value)
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

bool Program::setUniformMatrix3fv(GLint location, GLsizei count, const GLfloat *value)
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


bool Program::setUniformMatrix4fv(GLint location, GLsizei count, const GLfloat *value)
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

bool Program::setUniform1iv(GLint location, GLsizei count, const GLint *v)
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
        GLboolean *boolParams = new GLboolean[count];

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

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLboolean),
               boolParams, sizeof(GLboolean) * count);

        delete [] boolParams;
    }
    else
    {
        return false;
    }

    return true;
}

bool Program::setUniform2iv(GLint location, GLsizei count, const GLint *v)
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
        GLboolean *boolParams = new GLboolean[count * 2];

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

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLboolean) * 2,
               boolParams, 2 * sizeof(GLboolean) * count);

        delete [] boolParams;
    }
    else
    {
        return false;
    }

    return true;
}

bool Program::setUniform3iv(GLint location, GLsizei count, const GLint *v)
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
        GLboolean *boolParams = new GLboolean[count * 3];

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

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLboolean) * 3,
               boolParams, 3 * sizeof(GLboolean) * count);

        delete [] boolParams;
    }
    else
    {
        return false;
    }

    return true;
}

bool Program::setUniform4iv(GLint location, GLsizei count, const GLint *v)
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
        GLboolean *boolParams = new GLboolean[count * 4];

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

        memcpy(targetUniform->data + mUniformIndex[location].element * sizeof(GLboolean) * 4,
               boolParams, 4 * sizeof(GLboolean) * count);

        delete [] boolParams;
    }
    else
    {
        return false;
    }

    return true;
}

bool Program::getUniformfv(GLint location, GLsizei *bufSize, GLfloat *params)
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

bool Program::getUniformiv(GLint location, GLsizei *bufSize, GLint *params)
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

void Program::dirtyAllUniforms()
{
    unsigned int numUniforms = mUniforms.size();
    for (unsigned int index = 0; index < numUniforms; index++)
    {
        mUniforms[index]->dirty = true;
    }
}

// Applies all the uniforms set for this program object to the Direct3D 9 device
void Program::applyUniforms()
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
ID3D10Blob *Program::compileToBinary(const char *hlsl, const char *profile, ID3DXConstantTable **constantTable)
{
    if (!hlsl)
    {
        return NULL;
    }

    DWORD result;
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

    ID3D10Blob *binary = NULL;
    ID3D10Blob *errorMessage = NULL;
    result = D3DCompile(hlsl, strlen(hlsl), fakepath, NULL, NULL, "main", profile, flags, 0, &binary, &errorMessage);

    if (errorMessage)
    {
        const char *message = (const char*)errorMessage->GetBufferPointer();

        appendToInfoLogSanitized(message);
        TRACE("\n%s", hlsl);
        TRACE("\n%s", message);

        errorMessage->Release();
        errorMessage = NULL;
    }

    if (FAILED(result))
    {
        if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
        {
            error(GL_OUT_OF_MEMORY);
        }

        return NULL;
    }

    result = D3DXGetShaderConstantTable(static_cast<const DWORD*>(binary->GetBufferPointer()), constantTable);

    if (FAILED(result))
    {
        if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
        {
            error(GL_OUT_OF_MEMORY);
        }

        binary->Release();

        return NULL;
    }

    return binary;
}

// Packs varyings into generic varying registers, using the algorithm from [OpenGL ES Shading Language 1.00 rev. 17] appendix A section 7 page 111
// Returns the number of used varying registers, or -1 if unsuccesful
int Program::packVaryings(const Varying *packing[][4])
{
    Context *context = getContext();
    const int maxVaryingVectors = context->getMaximumVaryingVectors();

    for (VaryingList::iterator varying = mFragmentShader->varyings.begin(); varying != mFragmentShader->varyings.end(); varying++)
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
            appendToInfoLog("Could not pack varying %s", varying->name.c_str());

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

bool Program::linkVaryings()
{
    if (mPixelHLSL.empty() || mVertexHLSL.empty())
    {
        return false;
    }

    // Reset the varying register assignments
    for (VaryingList::iterator fragVar = mFragmentShader->varyings.begin(); fragVar != mFragmentShader->varyings.end(); fragVar++)
    {
        fragVar->reg = -1;
        fragVar->col = -1;
    }

    for (VaryingList::iterator vtxVar = mVertexShader->varyings.begin(); vtxVar != mVertexShader->varyings.end(); vtxVar++)
    {
        vtxVar->reg = -1;
        vtxVar->col = -1;
    }

    // Map the varyings to the register file
    const Varying *packing[MAX_VARYING_VECTORS_SM3][4] = {NULL};
    int registers = packVaryings(packing);

    if (registers < 0)
    {
        return false;
    }

    // Write the HLSL input/output declarations
    Context *context = getContext();
    const bool sm3 = context->supportsShaderModel3();
    const int maxVaryingVectors = context->getMaximumVaryingVectors();

    if (registers == maxVaryingVectors && mFragmentShader->mUsesFragCoord)
    {
        appendToInfoLog("No varying registers left to support gl_FragCoord");

        return false;
    }

    for (VaryingList::iterator input = mFragmentShader->varyings.begin(); input != mFragmentShader->varyings.end(); input++)
    {
        bool matched = false;

        for (VaryingList::iterator output = mVertexShader->varyings.begin(); output != mVertexShader->varyings.end(); output++)
        {
            if (output->name == input->name)
            {
                if (output->type != input->type || output->size != input->size)
                {
                    appendToInfoLog("Type of vertex varying %s does not match that of the fragment varying", output->name.c_str());

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
            appendToInfoLog("Fragment varying %s does not match any vertex varying", input->name.c_str());

            return false;
        }
    }

    std::string varyingSemantic = (sm3 ? "COLOR" : "TEXCOORD");

    mVertexHLSL += "struct VS_INPUT\n"
                   "{\n";

    int semanticIndex = 0;
    for (AttributeArray::iterator attribute = mVertexShader->mAttributes.begin(); attribute != mVertexShader->mAttributes.end(); attribute++)
    {
        switch (attribute->type)
        {
          case GL_FLOAT:      mVertexHLSL += "    float ";    break;
          case GL_FLOAT_VEC2: mVertexHLSL += "    float2 ";   break;
          case GL_FLOAT_VEC3: mVertexHLSL += "    float3 ";   break;
          case GL_FLOAT_VEC4: mVertexHLSL += "    float4 ";   break;
          case GL_FLOAT_MAT2: mVertexHLSL += "    float2x2 "; break;
          case GL_FLOAT_MAT3: mVertexHLSL += "    float3x3 "; break;
          case GL_FLOAT_MAT4: mVertexHLSL += "    float4x4 "; break;
          default:  UNREACHABLE();
        }

        mVertexHLSL += decorateAttribute(attribute->name) + " : TEXCOORD" + str(semanticIndex) + ";\n";

        semanticIndex += VariableRowCount(attribute->type);
    }

    mVertexHLSL += "};\n"
                   "\n"
                   "struct VS_OUTPUT\n"
                   "{\n"
                   "    float4 gl_Position : POSITION;\n";

    for (int r = 0; r < registers; r++)
    {
        int registerSize = packing[r][3] ? 4 : (packing[r][2] ? 3 : (packing[r][1] ? 2 : 1));

        mVertexHLSL += "    float" + str(registerSize) + " v" + str(r) + " : " + varyingSemantic + str(r) + ";\n";
    }

    if (mFragmentShader->mUsesFragCoord)
    {
        mVertexHLSL += "    float4 gl_FragCoord : " + varyingSemantic + str(registers) + ";\n";
    }

    if (mVertexShader->mUsesPointSize && sm3)
    {
        mVertexHLSL += "    float gl_PointSize : PSIZE;\n";
    }

    mVertexHLSL += "};\n"
                   "\n"
                   "VS_OUTPUT main(VS_INPUT input)\n"
                   "{\n";

    for (AttributeArray::iterator attribute = mVertexShader->mAttributes.begin(); attribute != mVertexShader->mAttributes.end(); attribute++)
    {
        mVertexHLSL += "    " + decorateAttribute(attribute->name) + " = ";

        if (VariableRowCount(attribute->type) > 1)   // Matrix
        {
            mVertexHLSL += "transpose";
        }

        mVertexHLSL += "(input." + decorateAttribute(attribute->name) + ");\n";
    }

    mVertexHLSL += "\n"
                   "    gl_main();\n"
                   "\n"
                   "    VS_OUTPUT output;\n"
                   "    output.gl_Position.x = gl_Position.x - dx_HalfPixelSize.x * gl_Position.w;\n"
                   "    output.gl_Position.y = gl_Position.y - dx_HalfPixelSize.y * gl_Position.w;\n"
                   "    output.gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n"
                   "    output.gl_Position.w = gl_Position.w;\n";

    if (mVertexShader->mUsesPointSize && sm3)
    {
        mVertexHLSL += "    output.gl_PointSize = clamp(gl_PointSize, 1.0, " + str((int)ALIASED_POINT_SIZE_RANGE_MAX_SM3) + ");\n";
    }

    if (mFragmentShader->mUsesFragCoord)
    {
        mVertexHLSL += "    output.gl_FragCoord = gl_Position;\n";
    }

    for (VaryingList::iterator varying = mVertexShader->varyings.begin(); varying != mVertexShader->varyings.end(); varying++)
    {
        if (varying->reg >= 0)
        {
            for (int i = 0; i < varying->size; i++)
            {
                int rows = VariableRowCount(varying->type);

                for (int j = 0; j < rows; j++)
                {
                    int r = varying->reg + i * rows + j;
                    mVertexHLSL += "    output.v" + str(r);

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
                        mVertexHLSL += ".";

                        for (int x = 0; x < 4; x++)
                        {
                            if (packing[r][x] == &*varying)
                            {
                                switch(x)
                                {
                                  case 0: mVertexHLSL += "x"; break;
                                  case 1: mVertexHLSL += "y"; break;
                                  case 2: mVertexHLSL += "z"; break;
                                  case 3: mVertexHLSL += "w"; break;
                                }
                            }
                        }
                    }

                    mVertexHLSL += " = " + varying->name;
                    
                    if (varying->array)
                    {
                        mVertexHLSL += "[" + str(i) + "]";
                    }

                    if (rows > 1)
                    {
                        mVertexHLSL += "[" + str(j) + "]";
                    }
                    
                    mVertexHLSL += ";\n";
                }
            }
        }
    }

    mVertexHLSL += "\n"
                   "    return output;\n"
                   "}\n";

    mPixelHLSL += "struct PS_INPUT\n"
                  "{\n";
    
    for (VaryingList::iterator varying = mFragmentShader->varyings.begin(); varying != mFragmentShader->varyings.end(); varying++)
    {
        if (varying->reg >= 0)
        {
            for (int i = 0; i < varying->size; i++)
            {
                int rows = VariableRowCount(varying->type);
                for (int j = 0; j < rows; j++)
                {
                    std::string n = str(varying->reg + i * rows + j);
                    mPixelHLSL += "    float4 v" + n + " : " + varyingSemantic + n + ";\n";
                }
            }
        }
        else UNREACHABLE();
    }

    if (mFragmentShader->mUsesFragCoord)
    {
        mPixelHLSL += "    float4 gl_FragCoord : " + varyingSemantic + str(registers) + ";\n";
        if (sm3) {
            mPixelHLSL += "    float2 dx_VPos : VPOS;\n";
        }
    }

    if (mFragmentShader->mUsesPointCoord && sm3)
    {
        mPixelHLSL += "    float2 gl_PointCoord : TEXCOORD0;\n";
    }

    if (mFragmentShader->mUsesFrontFacing)
    {
        mPixelHLSL += "    float vFace : VFACE;\n";
    }

    mPixelHLSL += "};\n"
                  "\n"
                  "struct PS_OUTPUT\n"
                  "{\n"
                  "    float4 gl_Color[1] : COLOR;\n"
                  "};\n"
                  "\n"
                  "PS_OUTPUT main(PS_INPUT input)\n"
                  "{\n";

    if (mFragmentShader->mUsesFragCoord)
    {
        mPixelHLSL += "    float rhw = 1.0 / input.gl_FragCoord.w;\n";
        
        if (sm3)
        {
            // dx_Coord.y contains the render target height. See Context::applyRenderTarget()
            mPixelHLSL += "    gl_FragCoord.x = input.dx_VPos.x + 0.5;\n"
                          "    gl_FragCoord.y = dx_Coord.y - input.dx_VPos.y - 0.5;\n";
        }
        else
        {
            // dx_Coord contains the viewport width/2, height/2, center.x and center.y. See Context::applyRenderTarget()
            mPixelHLSL += "    gl_FragCoord.x = (input.gl_FragCoord.x * rhw) * dx_Coord.x + dx_Coord.z;\n"
                          "    gl_FragCoord.y = -(input.gl_FragCoord.y * rhw) * dx_Coord.y + dx_Coord.w;\n";
        }
        
        mPixelHLSL += "    gl_FragCoord.z = (input.gl_FragCoord.z * rhw) * dx_Depth.x + dx_Depth.y;\n"
                      "    gl_FragCoord.w = rhw;\n";
    }

    if (mFragmentShader->mUsesPointCoord && sm3)
    {
        mPixelHLSL += "    gl_PointCoord = input.gl_PointCoord;\n";
    }

    if (mFragmentShader->mUsesFrontFacing)
    {
        mPixelHLSL += "    gl_FrontFacing = dx_PointsOrLines || (dx_FrontCCW ? (input.vFace >= 0.0) : (input.vFace <= 0.0));\n";
    }

    for (VaryingList::iterator varying = mFragmentShader->varyings.begin(); varying != mFragmentShader->varyings.end(); varying++)
    {
        if (varying->reg >= 0)
        {
            for (int i = 0; i < varying->size; i++)
            {
                int rows = VariableRowCount(varying->type);
                for (int j = 0; j < rows; j++)
                {
                    std::string n = str(varying->reg + i * rows + j);
                    mPixelHLSL += "    " + varying->name;

                    if (varying->array)
                    {
                        mPixelHLSL += "[" + str(i) + "]";
                    }

                    if (rows > 1)
                    {
                        mPixelHLSL += "[" + str(j) + "]";
                    }

                    mPixelHLSL += " = input.v" + n + ";\n";
                }
            }
        }
        else UNREACHABLE();
    }

    mPixelHLSL += "\n"
                  "    gl_main();\n"
                  "\n"
                  "    PS_OUTPUT output;\n"                 
                  "    output.gl_Color[0] = gl_Color[0];\n"
                  "\n"
                  "    return output;\n"
                  "}\n";

    return true;
}

// Links the HLSL code of the vertex and pixel shader by matching up their varyings,
// compiling them into binaries, determining the attribute mappings, and collecting
// a list of uniforms
void Program::link()
{
    unlink();

    if (!mFragmentShader || !mFragmentShader->isCompiled())
    {
        return;
    }

    if (!mVertexShader || !mVertexShader->isCompiled())
    {
        return;
    }

    mPixelHLSL = mFragmentShader->getHLSL();
    mVertexHLSL = mVertexShader->getHLSL();

    if (!linkVaryings())
    {
        return;
    }

    Context *context = getContext();
    const char *vertexProfile = context->supportsShaderModel3() ? "vs_3_0" : "vs_2_0";
    const char *pixelProfile = context->supportsShaderModel3() ? "ps_3_0" : "ps_2_0";

    ID3D10Blob *vertexBinary = compileToBinary(mVertexHLSL.c_str(), vertexProfile, &mConstantTableVS);
    ID3D10Blob *pixelBinary = compileToBinary(mPixelHLSL.c_str(), pixelProfile, &mConstantTablePS);

    if (vertexBinary && pixelBinary)
    {
        HRESULT vertexResult = mDevice->CreateVertexShader((DWORD*)vertexBinary->GetBufferPointer(), &mVertexExecutable);
        HRESULT pixelResult = mDevice->CreatePixelShader((DWORD*)pixelBinary->GetBufferPointer(), &mPixelExecutable);

        if (vertexResult == D3DERR_OUTOFVIDEOMEMORY || vertexResult == E_OUTOFMEMORY || pixelResult == D3DERR_OUTOFVIDEOMEMORY || pixelResult == E_OUTOFMEMORY)
        {
            return error(GL_OUT_OF_MEMORY);
        }

        ASSERT(SUCCEEDED(vertexResult) && SUCCEEDED(pixelResult));

        vertexBinary->Release();
        pixelBinary->Release();
        vertexBinary = NULL;
        pixelBinary = NULL;

        if (mVertexExecutable && mPixelExecutable)
        {
            if (!linkAttributes())
            {
                return;
            }

            if (!linkUniforms(mConstantTablePS))
            {
                return;
            }

            if (!linkUniforms(mConstantTableVS))
            {
                return;
            }

            // these uniforms are searched as already-decorated because gl_ and dx_
            // are reserved prefixes, and do not receive additional decoration
            mDxDepthRangeLocation = getUniformLocation("dx_DepthRange");
            mDxDepthLocation = getUniformLocation("dx_Depth");
            mDxCoordLocation = getUniformLocation("dx_Coord");
            mDxHalfPixelSizeLocation = getUniformLocation("dx_HalfPixelSize");
            mDxFrontCCWLocation = getUniformLocation("dx_FrontCCW");
            mDxPointsOrLinesLocation = getUniformLocation("dx_PointsOrLines");

            mLinked = true;   // Success
        }
    }
}

// Determines the mapping between GL attributes and Direct3D 9 vertex stream usage indices
bool Program::linkAttributes()
{
    unsigned int usedLocations = 0;

    // Link attributes that have a binding location
    for (AttributeArray::iterator attribute = mVertexShader->mAttributes.begin(); attribute != mVertexShader->mAttributes.end(); attribute++)
    {
        int location = getAttributeBinding(attribute->name);

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
                appendToInfoLog("Active attribute (%s) at location %d is too big to fit", attribute->name.c_str(), location);

                return false;
            }

            for (int i = 0; i < rows; i++)
            {
                usedLocations |= 1 << (location + i);
            }
        }
    }

    // Link attributes that don't have a binding location
    for (AttributeArray::iterator attribute = mVertexShader->mAttributes.begin(); attribute != mVertexShader->mAttributes.end(); attribute++)
    {
        int location = getAttributeBinding(attribute->name);

        if (location == -1)   // Not set by glBindAttribLocation
        {
            int rows = VariableRowCount(attribute->type);
            int availableIndex = AllocateFirstFreeBits(&usedLocations, rows, MAX_VERTEX_ATTRIBS);

            if (availableIndex == -1 || availableIndex + rows > MAX_VERTEX_ATTRIBS)
            {
                appendToInfoLog("Too many active attributes (%s)", attribute->name.c_str());

                return false;   // Fail to link
            }

            mLinkedAttribute[availableIndex] = *attribute;
        }
    }

    for (int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; )
    {
        int index = mVertexShader->getSemanticIndex(mLinkedAttribute[attributeIndex].name);
        int rows = std::max(VariableRowCount(mLinkedAttribute[attributeIndex].type), 1);

        for (int r = 0; r < rows; r++)
        {
            mSemanticIndex[attributeIndex++] = index++;
        }
    }

    return true;
}

int Program::getAttributeBinding(const std::string &name)
{
    for (int location = 0; location < MAX_VERTEX_ATTRIBS; location++)
    {
        if (mAttributeBinding[location].find(name) != mAttributeBinding[location].end())
        {
            return location;
        }
    }

    return -1;
}

bool Program::linkUniforms(ID3DXConstantTable *constantTable)
{
    D3DXCONSTANTTABLE_DESC constantTableDescription;
    D3DXCONSTANT_DESC constantDescription;
    UINT descriptionCount = 1;

    constantTable->GetDesc(&constantTableDescription);

    for (unsigned int constantIndex = 0; constantIndex < constantTableDescription.Constants; constantIndex++)
    {
        D3DXHANDLE constantHandle = constantTable->GetConstant(0, constantIndex);
        HRESULT result = constantTable->GetConstantDesc(constantHandle, &constantDescription, &descriptionCount);
        ASSERT(SUCCEEDED(result));

        if (!defineUniform(constantHandle, constantDescription))
        {
            return false;
        }
    }

    return true;
}

// Adds the description of a constant found in the binary shader to the list of uniforms
// Returns true if succesful (uniform not already defined)
bool Program::defineUniform(const D3DXHANDLE &constantHandle, const D3DXCONSTANT_DESC &constantDescription, std::string name)
{
    if (constantDescription.RegisterSet == D3DXRS_SAMPLER)
    {
        for (unsigned int i = 0; i < constantDescription.RegisterCount; i++)
        {
            D3DXHANDLE psConstant = mConstantTablePS->GetConstantByName(NULL, constantDescription.Name);
            D3DXHANDLE vsConstant = mConstantTableVS->GetConstantByName(NULL, constantDescription.Name);

            if (psConstant)
            {
                unsigned int samplerIndex = mConstantTablePS->GetSamplerIndex(psConstant) + i;

                if (samplerIndex < MAX_TEXTURE_IMAGE_UNITS)
                {
                    mSamplersPS[samplerIndex].active = true;
                    mSamplersPS[samplerIndex].textureType = (constantDescription.Type == D3DXPT_SAMPLERCUBE) ? TEXTURE_CUBE : TEXTURE_2D;
                    mSamplersPS[samplerIndex].logicalTextureUnit = 0;
                    mUsedPixelSamplerRange = std::max(samplerIndex + 1, mUsedPixelSamplerRange);
                }
                else
                {
                    appendToInfoLog("Pixel shader sampler count exceeds MAX_TEXTURE_IMAGE_UNITS (%d).", MAX_TEXTURE_IMAGE_UNITS);
                    return false;
                }
            }
            
            if (vsConstant)
            {
                unsigned int samplerIndex = mConstantTableVS->GetSamplerIndex(vsConstant) + i;

                if (samplerIndex < getContext()->getMaximumVertexTextureImageUnits())
                {
                    mSamplersVS[samplerIndex].active = true;
                    mSamplersVS[samplerIndex].textureType = (constantDescription.Type == D3DXPT_SAMPLERCUBE) ? TEXTURE_CUBE : TEXTURE_2D;
                    mSamplersVS[samplerIndex].logicalTextureUnit = 0;
                    mUsedVertexSamplerRange = std::max(samplerIndex + 1, mUsedVertexSamplerRange);
                }
                else
                {
                    appendToInfoLog("Vertex shader sampler count exceeds MAX_VERTEX_TEXTURE_IMAGE_UNITS (%d).", getContext()->getMaximumVertexTextureImageUnits());
                    return false;
                }
            }
        }
    }

    switch(constantDescription.Class)
    {
      case D3DXPC_STRUCT:
        {
            for (unsigned int arrayIndex = 0; arrayIndex < constantDescription.Elements; arrayIndex++)
            {
                for (unsigned int field = 0; field < constantDescription.StructMembers; field++)
                {
                    D3DXHANDLE fieldHandle = mConstantTablePS->GetConstant(constantHandle, field);

                    D3DXCONSTANT_DESC fieldDescription;
                    UINT descriptionCount = 1;

                    HRESULT result = mConstantTablePS->GetConstantDesc(fieldHandle, &fieldDescription, &descriptionCount);
                    ASSERT(SUCCEEDED(result));

                    std::string structIndex = (constantDescription.Elements > 1) ? ("[" + str(arrayIndex) + "]") : "";

                    if (!defineUniform(fieldHandle, fieldDescription, name + constantDescription.Name + structIndex + "."))
                    {
                        return false;
                    }
                }
            }

            return true;
        }
      case D3DXPC_SCALAR:
      case D3DXPC_VECTOR:
      case D3DXPC_MATRIX_COLUMNS:
      case D3DXPC_OBJECT:
        return defineUniform(constantDescription, name + constantDescription.Name);
      default:
        UNREACHABLE();
        return false;
    }
}

bool Program::defineUniform(const D3DXCONSTANT_DESC &constantDescription, std::string &_name)
{
    Uniform *uniform = createUniform(constantDescription, _name);

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

        if (mUniforms[mUniformIndex[location].index]->type != type)
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    initializeConstantHandles(uniform, &uniform->ps, mConstantTablePS);
    initializeConstantHandles(uniform, &uniform->vs, mConstantTableVS);

    mUniforms.push_back(uniform);
    unsigned int uniformIndex = mUniforms.size() - 1;

    for (unsigned int i = 0; i < uniform->arraySize; ++i)
    {
        mUniformIndex.push_back(UniformLocation(_name, i, uniformIndex));
    }

    return true;
}

Uniform *Program::createUniform(const D3DXCONSTANT_DESC &constantDescription, std::string &_name)
{
    if (constantDescription.Rows == 1)   // Vectors and scalars
    {
        switch (constantDescription.Type)
        {
          case D3DXPT_SAMPLER2D:
            switch (constantDescription.Columns)
            {
              case 1: return new Uniform(GL_SAMPLER_2D, _name, constantDescription.Elements);
              default: UNREACHABLE();
            }
            break;
          case D3DXPT_SAMPLERCUBE:
            switch (constantDescription.Columns)
            {
              case 1: return new Uniform(GL_SAMPLER_CUBE, _name, constantDescription.Elements);
              default: UNREACHABLE();
            }
            break;
          case D3DXPT_BOOL:
            switch (constantDescription.Columns)
            {
              case 1: return new Uniform(GL_BOOL, _name, constantDescription.Elements);
              case 2: return new Uniform(GL_BOOL_VEC2, _name, constantDescription.Elements);
              case 3: return new Uniform(GL_BOOL_VEC3, _name, constantDescription.Elements);
              case 4: return new Uniform(GL_BOOL_VEC4, _name, constantDescription.Elements);
              default: UNREACHABLE();
            }
            break;
          case D3DXPT_INT:
            switch (constantDescription.Columns)
            {
              case 1: return new Uniform(GL_INT, _name, constantDescription.Elements);
              case 2: return new Uniform(GL_INT_VEC2, _name, constantDescription.Elements);
              case 3: return new Uniform(GL_INT_VEC3, _name, constantDescription.Elements);
              case 4: return new Uniform(GL_INT_VEC4, _name, constantDescription.Elements);
              default: UNREACHABLE();
            }
            break;
          case D3DXPT_FLOAT:
            switch (constantDescription.Columns)
            {
              case 1: return new Uniform(GL_FLOAT, _name, constantDescription.Elements);
              case 2: return new Uniform(GL_FLOAT_VEC2, _name, constantDescription.Elements);
              case 3: return new Uniform(GL_FLOAT_VEC3, _name, constantDescription.Elements);
              case 4: return new Uniform(GL_FLOAT_VEC4, _name, constantDescription.Elements);
              default: UNREACHABLE();
            }
            break;
          default:
            UNREACHABLE();
        }
    }
    else if (constantDescription.Rows == constantDescription.Columns)  // Square matrices
    {
        switch (constantDescription.Type)
        {
          case D3DXPT_FLOAT:
            switch (constantDescription.Rows)
            {
              case 2: return new Uniform(GL_FLOAT_MAT2, _name, constantDescription.Elements);
              case 3: return new Uniform(GL_FLOAT_MAT3, _name, constantDescription.Elements);
              case 4: return new Uniform(GL_FLOAT_MAT4, _name, constantDescription.Elements);
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
std::string Program::decorateAttribute(const std::string &name)
{
    if (name.compare(0, 3, "gl_") != 0 && name.compare(0, 3, "dx_") != 0)
    {
        return "_" + name;
    }
    
    return name;
}

std::string Program::undecorateUniform(const std::string &_name)
{
    if (_name[0] == '_')
    {
        return _name.substr(1);
    }
    else if (_name.compare(0, 3, "ar_") == 0)
    {
        return _name.substr(3);
    }
    
    return _name;
}

void Program::applyUniformnbv(Uniform *targetUniform, GLsizei count, int width, const GLboolean *v)
{
    float *vector = NULL;
    BOOL *boolVector = NULL;

    if (targetUniform->ps.registerCount && targetUniform->ps.registerSet == D3DXRS_FLOAT4 ||
        targetUniform->vs.registerCount && targetUniform->vs.registerSet == D3DXRS_FLOAT4)
    {
        vector = new float[4 * count];

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

    if (targetUniform->ps.registerCount && targetUniform->ps.registerSet == D3DXRS_BOOL ||
        targetUniform->vs.registerCount && targetUniform->vs.registerSet == D3DXRS_BOOL)
    {
        boolVector = new BOOL[count * width];
        for (int i = 0; i < count * width; i++)
        {
            boolVector[i] = v[i] != GL_FALSE;
        }
    }

    if (targetUniform->ps.registerCount)
    {
        if (targetUniform->ps.registerSet == D3DXRS_FLOAT4)
        {
            mDevice->SetPixelShaderConstantF(targetUniform->ps.registerIndex, vector, targetUniform->ps.registerCount);
        }
        else if (targetUniform->ps.registerSet == D3DXRS_BOOL)
        {
            mDevice->SetPixelShaderConstantB(targetUniform->ps.registerIndex, boolVector, targetUniform->ps.registerCount);
        }
        else UNREACHABLE();
    }

    if (targetUniform->vs.registerCount)
    {
        if (targetUniform->vs.registerSet == D3DXRS_FLOAT4)
        {
            mDevice->SetVertexShaderConstantF(targetUniform->vs.registerIndex, vector, targetUniform->vs.registerCount);
        }
        else if (targetUniform->vs.registerSet == D3DXRS_BOOL)
        {
            mDevice->SetVertexShaderConstantB(targetUniform->vs.registerIndex, boolVector, targetUniform->vs.registerCount);
        }
        else UNREACHABLE();
    }

    delete [] vector;
    delete [] boolVector;
}

bool Program::applyUniformnfv(Uniform *targetUniform, const GLfloat *v)
{
    if (targetUniform->ps.registerCount)
    {
        mDevice->SetPixelShaderConstantF(targetUniform->ps.registerIndex, v, targetUniform->ps.registerCount);
    }

    if (targetUniform->vs.registerCount)
    {
        mDevice->SetVertexShaderConstantF(targetUniform->vs.registerIndex, v, targetUniform->vs.registerCount);
    }

    return true;
}

bool Program::applyUniform1iv(Uniform *targetUniform, GLsizei count, const GLint *v)
{
    D3DXVECTOR4 *vector = new D3DXVECTOR4[count];

    for (int i = 0; i < count; i++)
    {
        vector[i] = D3DXVECTOR4((float)v[i], 0, 0, 0);
    }

    if (targetUniform->ps.registerCount)
    {
        if (targetUniform->ps.registerSet == D3DXRS_SAMPLER)
        {
            unsigned int firstIndex = targetUniform->ps.registerIndex;

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
            ASSERT(targetUniform->ps.registerSet == D3DXRS_FLOAT4);
            mDevice->SetPixelShaderConstantF(targetUniform->ps.registerIndex, (const float*)vector, targetUniform->ps.registerCount);
        }
    }

    if (targetUniform->vs.registerCount)
    {
        if (targetUniform->vs.registerSet == D3DXRS_SAMPLER)
        {
            unsigned int firstIndex = targetUniform->vs.registerIndex;

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
            ASSERT(targetUniform->vs.registerSet == D3DXRS_FLOAT4);
            mDevice->SetVertexShaderConstantF(targetUniform->vs.registerIndex, (const float *)vector, targetUniform->vs.registerCount);
        }
    }

    delete [] vector;

    return true;
}

bool Program::applyUniform2iv(Uniform *targetUniform, GLsizei count, const GLint *v)
{
    D3DXVECTOR4 *vector = new D3DXVECTOR4[count];

    for (int i = 0; i < count; i++)
    {
        vector[i] = D3DXVECTOR4((float)v[0], (float)v[1], 0, 0);

        v += 2;
    }

    applyUniformniv(targetUniform, count, vector);

    delete[] vector;

    return true;
}

bool Program::applyUniform3iv(Uniform *targetUniform, GLsizei count, const GLint *v)
{
    D3DXVECTOR4 *vector = new D3DXVECTOR4[count];

    for (int i = 0; i < count; i++)
    {
        vector[i] = D3DXVECTOR4((float)v[0], (float)v[1], (float)v[2], 0);

        v += 3;
    }

    applyUniformniv(targetUniform, count, vector);

    delete[] vector;

    return true;
}

bool Program::applyUniform4iv(Uniform *targetUniform, GLsizei count, const GLint *v)
{
    D3DXVECTOR4 *vector = new D3DXVECTOR4[count];

    for (int i = 0; i < count; i++)
    {
        vector[i] = D3DXVECTOR4((float)v[0], (float)v[1], (float)v[2], (float)v[3]);

        v += 4;
    }

    applyUniformniv(targetUniform, count, vector);

    delete [] vector;

    return true;
}

void Program::applyUniformniv(Uniform *targetUniform, GLsizei count, const D3DXVECTOR4 *vector)
{
    if (targetUniform->ps.registerCount)
    {
        ASSERT(targetUniform->ps.registerSet == D3DXRS_FLOAT4);
        mDevice->SetPixelShaderConstantF(targetUniform->ps.registerIndex, (const float *)vector, targetUniform->ps.registerCount);
    }

    if (targetUniform->vs.registerCount)
    {
        ASSERT(targetUniform->vs.registerSet == D3DXRS_FLOAT4);
        mDevice->SetVertexShaderConstantF(targetUniform->vs.registerIndex, (const float *)vector, targetUniform->vs.registerCount);
    }
}

// append a santized message to the program info log.
// The D3D compiler includes a fake file path in some of the warning or error 
// messages, so lets remove all occurrences of this fake file path from the log.
void Program::appendToInfoLogSanitized(const char *message)
{
    std::string msg(message);

    size_t found;
    do
    {
        found = msg.find(fakepath);
        if (found != std::string::npos)
        {
            msg.erase(found, strlen(fakepath));
        }
    }
    while (found != std::string::npos);

    appendToInfoLog("%s\n", msg.c_str());
}

void Program::appendToInfoLog(const char *format, ...)
{
    if (!format)
    {
        return;
    }

    char info[1024];

    va_list vararg;
    va_start(vararg, format);
    vsnprintf(info, sizeof(info), format, vararg);
    va_end(vararg);

    size_t infoLength = strlen(info);

    if (!mInfoLog)
    {
        mInfoLog = new char[infoLength + 1];
        strcpy(mInfoLog, info);
    }
    else
    {
        size_t logLength = strlen(mInfoLog);
        char *newLog = new char[logLength + infoLength + 1];
        strcpy(newLog, mInfoLog);
        strcpy(newLog + logLength, info);

        delete[] mInfoLog;
        mInfoLog = newLog;
    }
}

void Program::resetInfoLog()
{
    if (mInfoLog)
    {
        delete [] mInfoLog;
        mInfoLog = NULL;
    }
}

// Returns the program object to an unlinked state, before re-linking, or at destruction
void Program::unlink(bool destroy)
{
    if (destroy)   // Object being destructed
    {
        if (mFragmentShader)
        {
            mFragmentShader->release();
            mFragmentShader = NULL;
        }

        if (mVertexShader)
        {
            mVertexShader->release();
            mVertexShader = NULL;
        }
    }

    if (mPixelExecutable)
    {
        mPixelExecutable->Release();
        mPixelExecutable = NULL;
    }

    if (mVertexExecutable)
    {
        mVertexExecutable->Release();
        mVertexExecutable = NULL;
    }

    if (mConstantTablePS)
    {
        mConstantTablePS->Release();
        mConstantTablePS = NULL;
    }

    if (mConstantTableVS)
    {
        mConstantTableVS->Release();
        mConstantTableVS = NULL;
    }

    for (int index = 0; index < MAX_VERTEX_ATTRIBS; index++)
    {
        mLinkedAttribute[index].name.clear();
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

    while (!mUniforms.empty())
    {
        delete mUniforms.back();
        mUniforms.pop_back();
    }

    mDxDepthRangeLocation = -1;
    mDxDepthLocation = -1;
    mDxCoordLocation = -1;
    mDxHalfPixelSizeLocation = -1;
    mDxFrontCCWLocation = -1;
    mDxPointsOrLinesLocation = -1;

    mUniformIndex.clear();

    mPixelHLSL.clear();
    mVertexHLSL.clear();

    delete[] mInfoLog;
    mInfoLog = NULL;

    mLinked = false;
}

bool Program::isLinked()
{
    return mLinked;
}

bool Program::isValidated() const 
{
    return mValidated;
}

void Program::release()
{
    mRefCount--;

    if (mRefCount == 0 && mDeleteStatus)
    {
        mResourceManager->deleteProgram(mHandle);
    }
}

void Program::addRef()
{
    mRefCount++;
}

unsigned int Program::getRefCount() const
{
    return mRefCount;
}

unsigned int Program::getSerial() const
{
    return mSerial;
}

unsigned int Program::issueSerial()
{
    return mCurrentSerial++;
}

int Program::getInfoLogLength() const
{
    if (!mInfoLog)
    {
        return 0;
    }
    else
    {
       return strlen(mInfoLog) + 1;
    }
}

void Program::getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog)
{
    int index = 0;

    if (mInfoLog)
    {
        while (index < bufSize - 1 && index < (int)strlen(mInfoLog))
        {
            infoLog[index] = mInfoLog[index];
            index++;
        }
    }

    if (bufSize)
    {
        infoLog[index] = '\0';
    }

    if (length)
    {
        *length = index;
    }
}

void Program::getAttachedShaders(GLsizei maxCount, GLsizei *count, GLuint *shaders)
{
    int total = 0;

    if (mVertexShader)
    {
        if (total < maxCount)
        {
            shaders[total] = mVertexShader->getHandle();
        }

        total++;
    }

    if (mFragmentShader)
    {
        if (total < maxCount)
        {
            shaders[total] = mFragmentShader->getHandle();
        }

        total++;
    }

    if (count)
    {
        *count = total;
    }
}

void Program::getActiveAttribute(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
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

GLint Program::getActiveAttributeCount()
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

GLint Program::getActiveAttributeMaxLength()
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

void Program::getActiveUniform(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
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

GLint Program::getActiveUniformCount()
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

GLint Program::getActiveUniformMaxLength()
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

void Program::flagForDeletion()
{
    mDeleteStatus = true;
}

bool Program::isFlaggedForDeletion() const
{
    return mDeleteStatus;
}

void Program::validate()
{
    resetInfoLog();

    if (!isLinked()) 
    {
        appendToInfoLog("Program has not been successfully linked.");
        mValidated = false;
    }
    else
    {
        applyUniforms();
        if (!validateSamplers(true))
        {
            mValidated = false;
        }
        else
        {
            mValidated = true;
        }
    }
}

bool Program::validateSamplers(bool logErrors)
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
                if (logErrors)
                {
                    appendToInfoLog("Sampler uniform (%d) exceeds MAX_COMBINED_TEXTURE_IMAGE_UNITS (%d)", unit, maxCombinedTextureImageUnits);
                }

                return false;
            }

            if (textureUnitType[unit] != TEXTURE_UNKNOWN)
            {
                if (mSamplersPS[i].textureType != textureUnitType[unit])
                {
                    if (logErrors)
                    {
                        appendToInfoLog("Samplers of conflicting types refer to the same texture image unit (%d).", unit);
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
                if (logErrors)
                {
                    appendToInfoLog("Sampler uniform (%d) exceeds MAX_COMBINED_TEXTURE_IMAGE_UNITS (%d)", unit, maxCombinedTextureImageUnits);
                }

                return false;
            }

            if (textureUnitType[unit] != TEXTURE_UNKNOWN)
            {
                if (mSamplersVS[i].textureType != textureUnitType[unit])
                {
                    if (logErrors)
                    {
                        appendToInfoLog("Samplers of conflicting types refer to the same texture image unit (%d).", unit);
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

void Program::initializeConstantHandles(Uniform *targetUniform, Uniform::RegisterInfo *ri, ID3DXConstantTable *constantTable)
{
    D3DXHANDLE handle = constantTable->GetConstantByName(0, targetUniform->_name.c_str());
    if (handle)
    {
        UINT descriptionCount = 1;
        D3DXCONSTANT_DESC constantDescription;
        HRESULT result = constantTable->GetConstantDesc(handle, &constantDescription, &descriptionCount);
        ASSERT(SUCCEEDED(result));
        ri->registerIndex = constantDescription.RegisterIndex;
        ri->registerCount = constantDescription.RegisterCount;
        ri->registerSet = constantDescription.RegisterSet;
    }
    else
    {
        ri->registerCount = 0;
    }
}

GLint Program::getDxDepthRangeLocation() const
{
    return mDxDepthRangeLocation;
}

GLint Program::getDxDepthLocation() const
{
    return mDxDepthLocation;
}

GLint Program::getDxCoordLocation() const
{
    return mDxCoordLocation;
}

GLint Program::getDxHalfPixelSizeLocation() const
{
    return mDxHalfPixelSizeLocation;
}

GLint Program::getDxFrontCCWLocation() const
{
    return mDxFrontCCWLocation;
}

GLint Program::getDxPointsOrLinesLocation() const
{
    return mDxPointsOrLinesLocation;
}

}
