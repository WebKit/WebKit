//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.cpp: Implements the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#include "libGLESv2/Program.h"
#include "libGLESv2/ProgramBinary.h"

#include "common/debug.h"

#include "libGLESv2/main.h"
#include "libGLESv2/Shader.h"
#include "libGLESv2/utilities.h"

#include <string>

namespace gl
{
const char * const g_fakepath = "C:\\fakepath";

unsigned int Program::mCurrentSerial = 1;

AttributeBindings::AttributeBindings()
{
}

AttributeBindings::~AttributeBindings()
{
}

InfoLog::InfoLog() : mInfoLog(NULL)
{
}

InfoLog::~InfoLog()
{
    delete[] mInfoLog;
}


int InfoLog::getLength() const
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

void InfoLog::getLog(GLsizei bufSize, GLsizei *length, char *infoLog)
{
    int index = 0;

    if (bufSize > 0)
    {
        if (mInfoLog)
        {
            index = std::min(bufSize - 1, (int)strlen(mInfoLog));
            memcpy(infoLog, mInfoLog, index);
        }

        infoLog[index] = '\0';
    }

    if (length)
    {
        *length = index;
    }
}

// append a santized message to the program info log.
// The D3D compiler includes a fake file path in some of the warning or error 
// messages, so lets remove all occurrences of this fake file path from the log.
void InfoLog::appendSanitized(const char *message)
{
    std::string msg(message);

    size_t found;
    do
    {
        found = msg.find(g_fakepath);
        if (found != std::string::npos)
        {
            msg.erase(found, strlen(g_fakepath));
        }
    }
    while (found != std::string::npos);

    append("%s\n", msg.c_str());
}

void InfoLog::append(const char *format, ...)
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

void InfoLog::reset()
{
    if (mInfoLog)
    {
        delete [] mInfoLog;
        mInfoLog = NULL;
    }
}

Program::Program(ResourceManager *manager, GLuint handle) : mResourceManager(manager), mHandle(handle), mSerial(issueSerial())
{
    mFragmentShader = NULL;
    mVertexShader = NULL;
    mProgramBinary = NULL;
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

void AttributeBindings::bindAttributeLocation(GLuint index, const char *name)
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

void Program::bindAttributeLocation(GLuint index, const char *name)
{
    mAttributeBindings.bindAttributeLocation(index, name);
}

// Links the HLSL code of the vertex and pixel shader by matching up their varyings,
// compiling them into binaries, determining the attribute mappings, and collecting
// a list of uniforms
void Program::link()
{
    unlink(false);

    mInfoLog.reset();

    mProgramBinary = new ProgramBinary;
    if (!mProgramBinary->link(mInfoLog, mAttributeBindings, mFragmentShader, mVertexShader))
    {
        unlink(false);
    }
}

int AttributeBindings::getAttributeBinding(const std::string &name) const
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

    if (mProgramBinary)
    {
        delete mProgramBinary;
        mProgramBinary = NULL;
    }
}

ProgramBinary* Program::getProgramBinary()
{
    return mProgramBinary;
}

void Program::setProgramBinary(ProgramBinary *programBinary)
{
    unlink(false);
    mProgramBinary = programBinary;
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
    return mInfoLog.getLength();
}

void Program::getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog)
{
    return mInfoLog.getLog(bufSize, length, infoLog);
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
    if (mProgramBinary)
    {
        mProgramBinary->getActiveAttribute(index, bufsize, length, size, type, name);
    }
    else
    {
        if (bufsize > 0)
        {
            name[0] = '\0';
        }
        
        if (length)
        {
            *length = 0;
        }

        *type = GL_NONE;
        *size = 1;
    }
}

GLint Program::getActiveAttributeCount()
{
    if (mProgramBinary)
    {
        return mProgramBinary->getActiveAttributeCount();
    }
    else
    {
        return 0;
    }
}

GLint Program::getActiveAttributeMaxLength()
{
    if (mProgramBinary)
    {
        return mProgramBinary->getActiveAttributeMaxLength();
    }
    else
    {
        return 0;
    }
}

void Program::getActiveUniform(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    if (mProgramBinary)
    {
        return mProgramBinary->getActiveUniform(index, bufsize, length, size, type, name);
    }
    else
    {
        if (bufsize > 0)
        {
            name[0] = '\0';
        }

        if (length)
        {
            *length = 0;
        }

        *size = 0;
        *type = GL_NONE;
    }
}

GLint Program::getActiveUniformCount()
{
    if (mProgramBinary)
    {
        return mProgramBinary->getActiveUniformCount();
    }
    else
    {
        return 0;
    }
}

GLint Program::getActiveUniformMaxLength()
{
    if (mProgramBinary)
    {
        return mProgramBinary->getActiveUniformMaxLength();
    }
    else
    {
        return 0;
    }
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
    mInfoLog.reset();

    if (mProgramBinary)
    {
        mProgramBinary->validate(mInfoLog);
    }
    else
    {
        mInfoLog.append("Program has not been successfully linked.");
    }
}

bool Program::isValidated() const
{
    if (mProgramBinary)
    {
        return mProgramBinary->isValidated();
    }
    else
    {
        return false;
    }
}

}
