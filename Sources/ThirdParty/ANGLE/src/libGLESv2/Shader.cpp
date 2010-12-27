//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Shader.cpp: Implements the gl::Shader class and its  derived classes
// VertexShader and FragmentShader. Implements GL shader objects and related
// functionality. [OpenGL ES 2.0.24] section 2.10 page 24 and section 3.8 page 84.

#include "libGLESv2/Shader.h"

#include <string>

#include "GLSLANG/Shaderlang.h"
#include "libGLESv2/main.h"
#include "libGLESv2/utilities.h"

namespace gl
{
void *Shader::mFragmentCompiler = NULL;
void *Shader::mVertexCompiler = NULL;

Shader::Shader(ResourceManager *manager, GLuint handle) : mHandle(handle), mResourceManager(manager)
{
    mSource = NULL;
    mHlsl = NULL;
    mInfoLog = NULL;

    // Perform a one-time initialization of the shader compiler (or after being destructed by releaseCompiler)
    if (!mFragmentCompiler)
    {
        int result = ShInitialize();

        if (result)
        {
            TBuiltInResource resources;
            ShInitBuiltInResource(&resources);
            resources.MaxVertexAttribs = MAX_VERTEX_ATTRIBS;
            resources.MaxVertexUniformVectors = MAX_VERTEX_UNIFORM_VECTORS;
            resources.MaxVaryingVectors = MAX_VARYING_VECTORS;
            resources.MaxVertexTextureImageUnits = MAX_VERTEX_TEXTURE_IMAGE_UNITS;
            resources.MaxCombinedTextureImageUnits = MAX_COMBINED_TEXTURE_IMAGE_UNITS;
            resources.MaxTextureImageUnits = MAX_TEXTURE_IMAGE_UNITS;
            resources.MaxFragmentUniformVectors = MAX_FRAGMENT_UNIFORM_VECTORS;
            resources.MaxDrawBuffers = MAX_DRAW_BUFFERS;

            mFragmentCompiler = ShConstructCompiler(EShLangFragment, EShSpecGLES2, &resources);
            mVertexCompiler = ShConstructCompiler(EShLangVertex, EShSpecGLES2, &resources);
        }
    }

    mRefCount = 0;
    mDeleteStatus = false;
}

Shader::~Shader()
{
    delete[] mSource;
    delete[] mHlsl;
    delete[] mInfoLog;
}

GLuint Shader::getHandle() const
{
    return mHandle;
}

void Shader::setSource(GLsizei count, const char **string, const GLint *length)
{
    delete[] mSource;
    int totalLength = 0;

    for (int i = 0; i < count; i++)
    {
        if (length && length[i] >= 0)
        {
            totalLength += length[i];
        }
        else
        {
            totalLength += (int)strlen(string[i]);
        }
    }

    mSource = new char[totalLength + 1];
    char *code = mSource;

    for (int i = 0; i < count; i++)
    {
        int stringLength;

        if (length && length[i] >= 0)
        {
            stringLength = length[i];
        }
        else
        {
            stringLength = (int)strlen(string[i]);
        }

        strncpy(code, string[i], stringLength);
        code += stringLength;
    }

    mSource[totalLength] = '\0';
}

int Shader::getInfoLogLength() const
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

void Shader::getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog)
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

int Shader::getSourceLength() const
{
    if (!mSource)
    {
        return 0;
    }
    else
    {
       return strlen(mSource) + 1;
    }
}

void Shader::getSource(GLsizei bufSize, GLsizei *length, char *source)
{
    int index = 0;

    if (mSource)
    {
        while (index < bufSize - 1 && index < (int)strlen(mSource))
        {
            source[index] = mSource[index];
            index++;
        }
    }

    if (bufSize)
    {
        source[index] = '\0';
    }

    if (length)
    {
        *length = index;
    }
}

bool Shader::isCompiled()
{
    return mHlsl != NULL;
}

const char *Shader::getHLSL()
{
    return mHlsl;
}

void Shader::addRef()
{
    mRefCount++;
}

void Shader::release()
{
    mRefCount--;

    if (mRefCount == 0 && mDeleteStatus)
    {
        mResourceManager->deleteShader(mHandle);
    }
}

unsigned int Shader::getRefCount() const
{
    return mRefCount;
}

bool Shader::isFlaggedForDeletion() const
{
    return mDeleteStatus;
}

void Shader::flagForDeletion()
{
    mDeleteStatus = true;
}

void Shader::releaseCompiler()
{
    ShDestruct(mFragmentCompiler);
    ShDestruct(mVertexCompiler);

    mFragmentCompiler = NULL;
    mVertexCompiler = NULL;

    ShFinalize();
}

void Shader::parseVaryings()
{
    if (mHlsl)
    {
        const char *input = strstr(mHlsl, "// Varyings") + 12;

        while(true)
        {
            char varyingType[256];
            char varyingName[256];

            int matches = sscanf(input, "static %s %s", varyingType, varyingName);

            if (matches != 2)
            {
                break;
            }

            char *array = strstr(varyingName, "[");
            int size = 1;

            if (array)
            {
                size = atoi(array + 1);
                *array = '\0';
            }

            varyings.push_back(Varying(parseType(varyingType), varyingName, size, array != NULL));

            input = strstr(input, ";") + 2;
        }

        mUsesFragCoord = strstr(mHlsl, "GL_USES_FRAG_COORD") != NULL;
        mUsesFrontFacing = strstr(mHlsl, "GL_USES_FRONT_FACING") != NULL;
        mUsesPointSize = strstr(mHlsl, "GL_USES_POINT_SIZE") != NULL;
        mUsesPointCoord = strstr(mHlsl, "GL_USES_POINT_COORD") != NULL;
    }
}

void Shader::compileToHLSL(void *compiler)
{
    if (isCompiled() || !mSource)
    {
        return;
    }

    TRACE("\n%s", mSource);

    delete[] mInfoLog;
    mInfoLog = NULL;

    int result = ShCompile(compiler, &mSource, 1, EShOptNone, EDebugOpNone);
    const char *obj = ShGetObjectCode(compiler);
    const char *info = ShGetInfoLog(compiler);

    if (result)
    {
        mHlsl = new char[strlen(obj) + 1];
        strcpy(mHlsl, obj);

        TRACE("\n%s", mHlsl);
    }
    else
    {
        mInfoLog = new char[strlen(info) + 1];
        strcpy(mInfoLog, info);

        TRACE("\n%s", mInfoLog);
    }
}

GLenum Shader::parseType(const std::string &type)
{
    if (type == "float")
    {
        return GL_FLOAT;
    }
    else if (type == "float2")
    {
        return GL_FLOAT_VEC2;
    }
    else if (type == "float3")
    {
        return GL_FLOAT_VEC3;
    }
    else if (type == "float4")
    {
        return GL_FLOAT_VEC4;
    }
    else if (type == "float2x2")
    {
        return GL_FLOAT_MAT2;
    }
    else if (type == "float3x3")
    {
        return GL_FLOAT_MAT3;
    }
    else if (type == "float4x4")
    {
        return GL_FLOAT_MAT4;
    }
    else UNREACHABLE();

    return GL_NONE;
}

// true if varying x has a higher priority in packing than y
bool Shader::compareVarying(const Varying &x, const Varying &y)
{
    if(x.type == y.type)
    {
        return x.size > y.size;
    }

    switch (x.type)
    {
      case GL_FLOAT_MAT4: return true;
      case GL_FLOAT_MAT2:
        switch(y.type)
        {
          case GL_FLOAT_MAT4: return false;
          case GL_FLOAT_MAT2: return true;
          case GL_FLOAT_VEC4: return true;
          case GL_FLOAT_MAT3: return true;
          case GL_FLOAT_VEC3: return true;
          case GL_FLOAT_VEC2: return true;
          case GL_FLOAT:      return true;
          default: UNREACHABLE();
        }
        break;
      case GL_FLOAT_VEC4:
        switch(y.type)
        {
          case GL_FLOAT_MAT4: return false;
          case GL_FLOAT_MAT2: return false;
          case GL_FLOAT_VEC4: return true;
          case GL_FLOAT_MAT3: return true;
          case GL_FLOAT_VEC3: return true;
          case GL_FLOAT_VEC2: return true;
          case GL_FLOAT:      return true;
          default: UNREACHABLE();
        }
        break;
      case GL_FLOAT_MAT3:
        switch(y.type)
        {
          case GL_FLOAT_MAT4: return false;
          case GL_FLOAT_MAT2: return false;
          case GL_FLOAT_VEC4: return false;
          case GL_FLOAT_MAT3: return true;
          case GL_FLOAT_VEC3: return true;
          case GL_FLOAT_VEC2: return true;
          case GL_FLOAT:      return true;
          default: UNREACHABLE();
        }
        break;
      case GL_FLOAT_VEC3:
        switch(y.type)
        {
          case GL_FLOAT_MAT4: return false;
          case GL_FLOAT_MAT2: return false;
          case GL_FLOAT_VEC4: return false;
          case GL_FLOAT_MAT3: return false;
          case GL_FLOAT_VEC3: return true;
          case GL_FLOAT_VEC2: return true;
          case GL_FLOAT:      return true;
          default: UNREACHABLE();
        }
        break;
      case GL_FLOAT_VEC2:
        switch(y.type)
        {
          case GL_FLOAT_MAT4: return false;
          case GL_FLOAT_MAT2: return false;
          case GL_FLOAT_VEC4: return false;
          case GL_FLOAT_MAT3: return false;
          case GL_FLOAT_VEC3: return false;
          case GL_FLOAT_VEC2: return true;
          case GL_FLOAT:      return true;
          default: UNREACHABLE();
        }
        break;
      case GL_FLOAT: return false;
      default: UNREACHABLE();
    }

    return false;
}

VertexShader::VertexShader(ResourceManager *manager, GLuint handle) : Shader(manager, handle)
{
}

VertexShader::~VertexShader()
{
}

GLenum VertexShader::getType()
{
    return GL_VERTEX_SHADER;
}

void VertexShader::compile()
{
    compileToHLSL(mVertexCompiler);
    parseAttributes();
    parseVaryings();
}

int VertexShader::getSemanticIndex(const std::string &attributeName)
{
    if (!attributeName.empty())
    {
        int semanticIndex = 0;
        for (AttributeArray::iterator attribute = mAttributes.begin(); attribute != mAttributes.end(); attribute++)
        {
            if (attribute->name == attributeName)
            {
                return semanticIndex;
            }

            semanticIndex += VariableRowCount(attribute->type);
        }
    }

    return -1;
}

void VertexShader::parseAttributes()
{
    if (mHlsl)
    {
        const char *input = strstr(mHlsl, "// Attributes") + 14;

        while(true)
        {
            char attributeType[256];
            char attributeName[256];

            int matches = sscanf(input, "static %s _%s", attributeType, attributeName);

            if (matches != 2)
            {
                break;
            }

            mAttributes.push_back(Attribute(parseType(attributeType), attributeName));

            input = strstr(input, ";") + 2;
        }
    }
}

FragmentShader::FragmentShader(ResourceManager *manager, GLuint handle) : Shader(manager, handle)
{
}

FragmentShader::~FragmentShader()
{
}

GLenum FragmentShader::getType()
{
    return GL_FRAGMENT_SHADER;
}

void FragmentShader::compile()
{
    compileToHLSL(mFragmentCompiler);
    parseVaryings();
    varyings.sort(compareVarying);
}
}
