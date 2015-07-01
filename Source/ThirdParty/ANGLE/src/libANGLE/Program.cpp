//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.cpp: Implements the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#include "libANGLE/Program.h"

#include <algorithm>

#include "common/debug.h"
#include "common/platform.h"
#include "common/utilities.h"
#include "common/version.h"
#include "compiler/translator/blocklayout.h"
#include "libANGLE/Data.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/features.h"
#include "libANGLE/renderer/Renderer.h"
#include "libANGLE/renderer/ProgramImpl.h"

namespace gl
{
const char * const g_fakepath = "C:\\fakepath";

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

}

AttributeBindings::AttributeBindings()
{
}

AttributeBindings::~AttributeBindings()
{
}

InfoLog::InfoLog()
{
}

InfoLog::~InfoLog()
{
}

size_t InfoLog::getLength() const
{
    return mStream.str().length();
}

void InfoLog::getLog(GLsizei bufSize, GLsizei *length, char *infoLog)
{
    size_t index = 0;

    if (bufSize > 0)
    {
        const std::string str(mStream.str());

        if (!str.empty())
        {
            index = std::min(static_cast<size_t>(bufSize) - 1, str.length());
            memcpy(infoLog, str.c_str(), index);
        }

        infoLog[index] = '\0';
    }

    if (length)
    {
        *length = static_cast<GLsizei>(index);
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

    mStream << message << "\n";
}

void InfoLog::append(const char *format, ...)
{
    if (!format)
    {
        return;
    }

    va_list vararg;
    va_start(vararg, format);
    std::string tempString(FormatString(format, vararg));
    va_end(vararg);

    mStream << tempString << "\n";
}

void InfoLog::reset()
{
}

VariableLocation::VariableLocation()
    : name(),
      element(0),
      index(0)
{
}

VariableLocation::VariableLocation(const std::string &name, unsigned int element, unsigned int index)
    : name(name),
      element(element),
      index(index)
{
}

LinkedVarying::LinkedVarying()
{
}

LinkedVarying::LinkedVarying(const std::string &name, GLenum type, GLsizei size, const std::string &semanticName,
                             unsigned int semanticIndex, unsigned int semanticIndexCount)
    : name(name), type(type), size(size), semanticName(semanticName), semanticIndex(semanticIndex), semanticIndexCount(semanticIndexCount)
{
}

Program::Program(rx::ProgramImpl *impl, ResourceManager *manager, GLuint handle)
    : mProgram(impl),
      mValidated(false),
      mTransformFeedbackVaryings(),
      mTransformFeedbackBufferMode(GL_NONE),
      mFragmentShader(nullptr),
      mVertexShader(nullptr),
      mLinked(false),
      mDeleteStatus(false),
      mRefCount(0),
      mResourceManager(manager),
      mHandle(handle)
{
    ASSERT(mProgram);

    resetUniformBlockBindings();
    unlink();
}

Program::~Program()
{
    unlink(true);

    if (mVertexShader != nullptr)
    {
        mVertexShader->release();
    }

    if (mFragmentShader != nullptr)
    {
        mFragmentShader->release();
    }

    SafeDelete(mProgram);
}

bool Program::attachShader(Shader *shader)
{
    if (shader->getType() == GL_VERTEX_SHADER)
    {
        if (mVertexShader)
        {
            return false;
        }

        mVertexShader = shader;
        mVertexShader->addRef();
    }
    else if (shader->getType() == GL_FRAGMENT_SHADER)
    {
        if (mFragmentShader)
        {
            return false;
        }

        mFragmentShader = shader;
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
        mVertexShader = nullptr;
    }
    else if (shader->getType() == GL_FRAGMENT_SHADER)
    {
        if (mFragmentShader != shader)
        {
            return false;
        }

        mFragmentShader->release();
        mFragmentShader = nullptr;
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
Error Program::link(const Data &data)
{
    unlink(false);

    mInfoLog.reset();
    resetUniformBlockBindings();

    if (!mFragmentShader || !mFragmentShader->isCompiled())
    {
        return Error(GL_NO_ERROR);
    }
    ASSERT(mFragmentShader->getType() == GL_FRAGMENT_SHADER);

    if (!mVertexShader || !mVertexShader->isCompiled())
    {
        return Error(GL_NO_ERROR);
    }
    ASSERT(mVertexShader->getType() == GL_VERTEX_SHADER);

    if (!linkAttributes(data, mInfoLog, mAttributeBindings, mVertexShader))
    {
        return Error(GL_NO_ERROR);
    }

    int registers;
    std::vector<LinkedVarying> linkedVaryings;
    rx::LinkResult result = mProgram->link(data, mInfoLog, mFragmentShader, mVertexShader, mTransformFeedbackVaryings, mTransformFeedbackBufferMode,
                                           &registers, &linkedVaryings, &mOutputVariables);
    if (result.error.isError() || !result.linkSuccess)
    {
        return result.error;
    }

    if (!mProgram->linkUniforms(mInfoLog, *mVertexShader, *mFragmentShader, *data.caps))
    {
        return Error(GL_NO_ERROR);
    }

    if (!linkUniformBlocks(mInfoLog, *mVertexShader, *mFragmentShader, *data.caps))
    {
        return Error(GL_NO_ERROR);
    }

    if (!gatherTransformFeedbackLinkedVaryings(mInfoLog, linkedVaryings, mTransformFeedbackVaryings,
                                               mTransformFeedbackBufferMode, &mProgram->getTransformFeedbackLinkedVaryings(), *data.caps))
    {
        return Error(GL_NO_ERROR);
    }

    // TODO: The concept of "executables" is D3D only, and as such this belongs in ProgramD3D. It must be called,
    // however, last in this function, so it can't simply be moved to ProgramD3D::link without further shuffling.
    result = mProgram->compileProgramExecutables(mInfoLog, mFragmentShader, mVertexShader, registers);
    if (result.error.isError() || !result.linkSuccess)
    {
        mInfoLog.append("Failed to create D3D shaders.");
        unlink(false);
        return result.error;
    }

    mLinked = true;
    return gl::Error(GL_NO_ERROR);
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
            mFragmentShader = nullptr;
        }

        if (mVertexShader)
        {
            mVertexShader->release();
            mVertexShader = nullptr;
        }
    }

    std::fill(mLinkedAttribute, mLinkedAttribute + ArraySize(mLinkedAttribute), sh::Attribute());
    mOutputVariables.clear();

    mProgram->reset();

    mValidated = false;

    mLinked = false;
}

bool Program::isLinked()
{
    return mLinked;
}

Error Program::loadBinary(GLenum binaryFormat, const void *binary, GLsizei length)
{
    unlink(false);

#if ANGLE_PROGRAM_BINARY_LOAD != ANGLE_ENABLED
    return Error(GL_NO_ERROR);
#else
    ASSERT(binaryFormat == mProgram->getBinaryFormat());

    BinaryInputStream stream(binary, length);

    GLenum format = stream.readInt<GLenum>();
    if (format != mProgram->getBinaryFormat())
    {
        mInfoLog.append("Invalid program binary format.");
        return Error(GL_NO_ERROR);
    }

    int majorVersion = stream.readInt<int>();
    int minorVersion = stream.readInt<int>();
    if (majorVersion != ANGLE_MAJOR_VERSION || minorVersion != ANGLE_MINOR_VERSION)
    {
        mInfoLog.append("Invalid program binary version.");
        return Error(GL_NO_ERROR);
    }

    unsigned char commitString[ANGLE_COMMIT_HASH_SIZE];
    stream.readBytes(commitString, ANGLE_COMMIT_HASH_SIZE);
    if (memcmp(commitString, ANGLE_COMMIT_HASH, sizeof(unsigned char) * ANGLE_COMMIT_HASH_SIZE) != 0)
    {
        mInfoLog.append("Invalid program binary version.");
        return Error(GL_NO_ERROR);
    }

    // TODO(jmadill): replace MAX_VERTEX_ATTRIBS
    for (int i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
    {
        stream.readInt(&mLinkedAttribute[i].type);
        stream.readString(&mLinkedAttribute[i].name);
        stream.readInt(&mProgram->getSemanticIndexes()[i]);
    }

    unsigned int attribCount = stream.readInt<unsigned int>();
    for (unsigned int attribIndex = 0; attribIndex < attribCount; ++attribIndex)
    {
        GLenum type = stream.readInt<GLenum>();
        GLenum precision = stream.readInt<GLenum>();
        std::string name = stream.readString();
        GLint arraySize = stream.readInt<GLint>();
        int location = stream.readInt<int>();
        mProgram->setShaderAttribute(attribIndex, type, precision, name, arraySize, location);
    }

    rx::LinkResult result = mProgram->load(mInfoLog, &stream);
    if (result.error.isError() || !result.linkSuccess)
    {
        return result.error;
    }

    mLinked = true;
    return Error(GL_NO_ERROR);
#endif // #if ANGLE_PROGRAM_BINARY_LOAD == ANGLE_ENABLED
}

Error Program::saveBinary(GLenum *binaryFormat, void *binary, GLsizei bufSize, GLsizei *length) const
{
    if (binaryFormat)
    {
        *binaryFormat = mProgram->getBinaryFormat();
    }

    BinaryOutputStream stream;

    stream.writeInt(mProgram->getBinaryFormat());
    stream.writeInt(ANGLE_MAJOR_VERSION);
    stream.writeInt(ANGLE_MINOR_VERSION);
    stream.writeBytes(reinterpret_cast<const unsigned char*>(ANGLE_COMMIT_HASH), ANGLE_COMMIT_HASH_SIZE);

    // TODO(jmadill): replace MAX_VERTEX_ATTRIBS
    for (unsigned int i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
    {
        stream.writeInt(mLinkedAttribute[i].type);
        stream.writeString(mLinkedAttribute[i].name);
        stream.writeInt(mProgram->getSemanticIndexes()[i]);
    }

    const auto &shaderAttributes = mProgram->getShaderAttributes();
    stream.writeInt(shaderAttributes.size());
    for (const auto &attrib : shaderAttributes)
    {
        stream.writeInt(attrib.type);
        stream.writeInt(attrib.precision);
        stream.writeString(attrib.name);
        stream.writeInt(attrib.arraySize);
        stream.writeInt(attrib.location);
    }

    gl::Error error = mProgram->save(&stream);
    if (error.isError())
    {
        return error;
    }

    GLsizei streamLength = static_cast<GLsizei>(stream.length());
    const void *streamData = stream.data();

    if (streamLength > bufSize)
    {
        if (length)
        {
            *length = 0;
        }

        // TODO: This should be moved to the validation layer but computing the size of the binary before saving
        // it causes the save to happen twice.  It may be possible to write the binary to a separate buffer, validate
        // sizes and then copy it.
        return Error(GL_INVALID_OPERATION);
    }

    if (binary)
    {
        char *ptr = reinterpret_cast<char*>(binary);

        memcpy(ptr, streamData, streamLength);
        ptr += streamLength;

        ASSERT(ptr - streamLength == binary);
    }

    if (length)
    {
        *length = streamLength;
    }

    return Error(GL_NO_ERROR);
}

GLint Program::getBinaryLength() const
{
    GLint length;
    Error error = saveBinary(nullptr, nullptr, std::numeric_limits<GLint>::max(), &length);
    if (error.isError())
    {
        return 0;
    }

    return length;
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

int Program::getInfoLogLength() const
{
    return static_cast<int>(mInfoLog.getLength());
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

GLuint Program::getAttributeLocation(const std::string &name)
{
    for (int index = 0; index < MAX_VERTEX_ATTRIBS; index++)
    {
        if (mLinkedAttribute[index].name == name)
        {
            return index;
        }
    }

    return static_cast<GLuint>(-1);
}

const int *Program::getSemanticIndexes() const
{
    return mProgram->getSemanticIndexes();
}

int Program::getSemanticIndex(int attributeIndex)
{
    ASSERT(attributeIndex >= 0 && attributeIndex < MAX_VERTEX_ATTRIBS);

    return mProgram->getSemanticIndexes()[attributeIndex];
}

void Program::getActiveAttribute(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    if (mLinked)
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
                *length = static_cast<GLsizei>(strlen(name));
            }
        }

        *size = 1;   // Always a single 'type' instance

        *type = mLinkedAttribute[attribute].type;
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
    int count = 0;

    if (mLinked)
    {
        for (int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; attributeIndex++)
        {
            if (!mLinkedAttribute[attributeIndex].name.empty())
            {
                count++;
            }
        }
    }

    return count;
}

GLint Program::getActiveAttributeMaxLength()
{
    int maxLength = 0;

    if (mLinked)
    {
        for (int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; attributeIndex++)
        {
            if (!mLinkedAttribute[attributeIndex].name.empty())
            {
                maxLength = std::max((int)(mLinkedAttribute[attributeIndex].name.length() + 1), maxLength);
            }
        }
    }

    return maxLength;
}

// Returns one more than the highest sampler index used.
GLint Program::getUsedSamplerRange(SamplerType type)
{
    return mProgram->getUsedSamplerRange(type);
}

bool Program::usesPointSize() const
{
    return mProgram->usesPointSize();
}

GLint Program::getSamplerMapping(SamplerType type, unsigned int samplerIndex, const Caps &caps)
{
    return mProgram->getSamplerMapping(type, samplerIndex, caps);
}

GLenum Program::getSamplerTextureType(SamplerType type, unsigned int samplerIndex)
{
    return mProgram->getSamplerTextureType(type, samplerIndex);
}

GLint Program::getFragDataLocation(const std::string &name) const
{
    std::string baseName(name);
    unsigned int arrayIndex = ParseAndStripArrayIndex(&baseName);
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

void Program::getActiveUniform(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    if (mLinked)
    {
        ASSERT(index < mProgram->getUniforms().size());   // index must be smaller than getActiveUniformCount()
        LinkedUniform *uniform = mProgram->getUniforms()[index];

        if (bufsize > 0)
        {
            std::string string = uniform->name;
            if (uniform->isArray())
            {
                string += "[0]";
            }

            strncpy(name, string.c_str(), bufsize);
            name[bufsize - 1] = '\0';

            if (length)
            {
                *length = static_cast<GLsizei>(strlen(name));
            }
        }

        *size = uniform->elementCount();
        *type = uniform->type;
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
    if (mLinked)
    {
        return static_cast<GLint>(mProgram->getUniforms().size());
    }
    else
    {
        return 0;
    }
}

GLint Program::getActiveUniformMaxLength()
{
    int maxLength = 0;

    if (mLinked)
    {
        unsigned int numUniforms = static_cast<unsigned int>(mProgram->getUniforms().size());
        for (unsigned int uniformIndex = 0; uniformIndex < numUniforms; uniformIndex++)
        {
            if (!mProgram->getUniforms()[uniformIndex]->name.empty())
            {
                int length = (int)(mProgram->getUniforms()[uniformIndex]->name.length() + 1);
                if (mProgram->getUniforms()[uniformIndex]->isArray())
                {
                    length += 3;  // Counting in "[0]".
                }
                maxLength = std::max(length, maxLength);
            }
        }
    }

    return maxLength;
}

GLint Program::getActiveUniformi(GLuint index, GLenum pname) const
{
    const gl::LinkedUniform& uniform = *mProgram->getUniforms()[index];
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

bool Program::isValidUniformLocation(GLint location) const
{
    ASSERT(rx::IsIntegerCastSafe<GLint>(mProgram->getUniformIndices().size()));
    return (location >= 0 && location < static_cast<GLint>(mProgram->getUniformIndices().size()));
}

LinkedUniform *Program::getUniformByLocation(GLint location) const
{
    return mProgram->getUniformByLocation(location);
}

LinkedUniform *Program::getUniformByName(const std::string &name) const
{
    return mProgram->getUniformByName(name);
}

GLint Program::getUniformLocation(const std::string &name)
{
    return mProgram->getUniformLocation(name);
}

GLuint Program::getUniformIndex(const std::string &name)
{
    return mProgram->getUniformIndex(name);
}

void Program::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    mProgram->setUniform1fv(location, count, v);
}

void Program::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    mProgram->setUniform2fv(location, count, v);
}

void Program::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    mProgram->setUniform3fv(location, count, v);
}

void Program::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    mProgram->setUniform4fv(location, count, v);
}

void Program::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    mProgram->setUniform1iv(location, count, v);
}

void Program::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    mProgram->setUniform2iv(location, count, v);
}

void Program::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    mProgram->setUniform3iv(location, count, v);
}

void Program::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    mProgram->setUniform4iv(location, count, v);
}

void Program::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    mProgram->setUniform1uiv(location, count, v);
}

void Program::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    mProgram->setUniform2uiv(location, count, v);
}

void Program::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    mProgram->setUniform3uiv(location, count, v);
}

void Program::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    mProgram->setUniform4uiv(location, count, v);
}

void Program::setUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v)
{
    mProgram->setUniformMatrix2fv(location, count, transpose, v);
}

void Program::setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v)
{
    mProgram->setUniformMatrix3fv(location, count, transpose, v);
}

void Program::setUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v)
{
    mProgram->setUniformMatrix4fv(location, count, transpose, v);
}

void Program::setUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v)
{
    mProgram->setUniformMatrix2x3fv(location, count, transpose, v);
}

void Program::setUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v)
{
    mProgram->setUniformMatrix2x4fv(location, count, transpose, v);
}

void Program::setUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v)
{
    mProgram->setUniformMatrix3x2fv(location, count, transpose, v);
}

void Program::setUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v)
{
    mProgram->setUniformMatrix3x4fv(location, count, transpose, v);
}

void Program::setUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v)
{
    mProgram->setUniformMatrix4x2fv(location, count, transpose, v);
}

void Program::setUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v)
{
    mProgram->setUniformMatrix4x3fv(location, count, transpose, v);
}

void Program::getUniformfv(GLint location, GLfloat *v)
{
    mProgram->getUniformfv(location, v);
}

void Program::getUniformiv(GLint location, GLint *v)
{
    mProgram->getUniformiv(location, v);
}

void Program::getUniformuiv(GLint location, GLuint *v)
{
    mProgram->getUniformuiv(location, v);
}

// Applies all the uniforms set for this program object to the renderer
Error Program::applyUniforms()
{
    return mProgram->applyUniforms();
}

Error Program::applyUniformBuffers(const gl::Data &data)
{
    return mProgram->applyUniformBuffers(data, mUniformBlockBindings);
}

void Program::flagForDeletion()
{
    mDeleteStatus = true;
}

bool Program::isFlaggedForDeletion() const
{
    return mDeleteStatus;
}

void Program::validate(const Caps &caps)
{
    mInfoLog.reset();
    mValidated = false;

    if (mLinked)
    {
        applyUniforms();
        mValidated = mProgram->validateSamplers(&mInfoLog, caps);
    }
    else
    {
        mInfoLog.append("Program has not been successfully linked.");
    }
}

bool Program::validateSamplers(InfoLog *infoLog, const Caps &caps)
{
    return mProgram->validateSamplers(infoLog, caps);
}

bool Program::isValidated() const
{
    return mValidated;
}

void Program::updateSamplerMapping()
{
    return mProgram->updateSamplerMapping();
}

GLuint Program::getActiveUniformBlockCount()
{
    return static_cast<GLuint>(mProgram->getUniformBlocks().size());
}

void Program::getActiveUniformBlockName(GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName) const
{
    ASSERT(uniformBlockIndex < mProgram->getUniformBlocks().size());   // index must be smaller than getActiveUniformBlockCount()

    const UniformBlock &uniformBlock = *mProgram->getUniformBlocks()[uniformBlockIndex];

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
            *length = static_cast<GLsizei>(strlen(uniformBlockName));
        }
    }
}

void Program::getActiveUniformBlockiv(GLuint uniformBlockIndex, GLenum pname, GLint *params) const
{
    ASSERT(uniformBlockIndex < mProgram->getUniformBlocks().size());   // index must be smaller than getActiveUniformBlockCount()

    const UniformBlock &uniformBlock = *mProgram->getUniformBlocks()[uniformBlockIndex];

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

GLint Program::getActiveUniformBlockMaxLength()
{
    int maxLength = 0;

    if (mLinked)
    {
        unsigned int numUniformBlocks = static_cast<unsigned int>(mProgram->getUniformBlocks().size());
        for (unsigned int uniformBlockIndex = 0; uniformBlockIndex < numUniformBlocks; uniformBlockIndex++)
        {
            const UniformBlock &uniformBlock = *mProgram->getUniformBlocks()[uniformBlockIndex];
            if (!uniformBlock.name.empty())
            {
                const int length = static_cast<int>(uniformBlock.name.length() + 1);

                // Counting in "[0]".
                const int arrayLength = (uniformBlock.isArrayElement() ? 3 : 0);

                maxLength = std::max(length + arrayLength, maxLength);
            }
        }
    }

    return maxLength;
}

GLuint Program::getUniformBlockIndex(const std::string &name)
{
    return mProgram->getUniformBlockIndex(name);
}

const UniformBlock *Program::getUniformBlockByIndex(GLuint index) const
{
    return mProgram->getUniformBlockByIndex(index);
}

void Program::bindUniformBlock(GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    mUniformBlockBindings[uniformBlockIndex] = uniformBlockBinding;
}

GLuint Program::getUniformBlockBinding(GLuint uniformBlockIndex) const
{
    return mUniformBlockBindings[uniformBlockIndex];
}

void Program::resetUniformBlockBindings()
{
    for (unsigned int blockId = 0; blockId < IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS; blockId++)
    {
        mUniformBlockBindings[blockId] = 0;
    }
}

void Program::setTransformFeedbackVaryings(GLsizei count, const GLchar *const *varyings, GLenum bufferMode)
{
    mTransformFeedbackVaryings.resize(count);
    for (GLsizei i = 0; i < count; i++)
    {
        mTransformFeedbackVaryings[i] = varyings[i];
    }

    mTransformFeedbackBufferMode = bufferMode;
}

void Program::getTransformFeedbackVarying(GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name) const
{
    if (mLinked)
    {
        ASSERT(index < mProgram->getTransformFeedbackLinkedVaryings().size());
        const LinkedVarying &varying = mProgram->getTransformFeedbackLinkedVaryings()[index];
        GLsizei lastNameIdx = std::min(bufSize - 1, static_cast<GLsizei>(varying.name.length()));
        if (length)
        {
            *length = lastNameIdx;
        }
        if (size)
        {
            *size = varying.size;
        }
        if (type)
        {
            *type = varying.type;
        }
        if (name)
        {
            memcpy(name, varying.name.c_str(), lastNameIdx);
            name[lastNameIdx] = '\0';
        }
    }
}

GLsizei Program::getTransformFeedbackVaryingCount() const
{
    if (mLinked)
    {
        return static_cast<GLsizei>(mProgram->getTransformFeedbackLinkedVaryings().size());
    }
    else
    {
        return 0;
    }
}

GLsizei Program::getTransformFeedbackVaryingMaxLength() const
{
    if (mLinked)
    {
        GLsizei maxSize = 0;
        for (size_t i = 0; i < mProgram->getTransformFeedbackLinkedVaryings().size(); i++)
        {
            const LinkedVarying &varying = mProgram->getTransformFeedbackLinkedVaryings()[i];
            maxSize = std::max(maxSize, static_cast<GLsizei>(varying.name.length() + 1));
        }

        return maxSize;
    }
    else
    {
        return 0;
    }
}

GLenum Program::getTransformFeedbackBufferMode() const
{
    return mTransformFeedbackBufferMode;
}

bool Program::linkVaryings(InfoLog &infoLog, Shader *fragmentShader, Shader *vertexShader)
{
    std::vector<PackedVarying> &fragmentVaryings = fragmentShader->getVaryings();
    std::vector<PackedVarying> &vertexVaryings = vertexShader->getVaryings();

    for (size_t fragVaryingIndex = 0; fragVaryingIndex < fragmentVaryings.size(); fragVaryingIndex++)
    {
        PackedVarying *input = &fragmentVaryings[fragVaryingIndex];
        bool matched = false;

        // Built-in varyings obey special rules
        if (input->isBuiltIn())
        {
            continue;
        }

        for (size_t vertVaryingIndex = 0; vertVaryingIndex < vertexVaryings.size(); vertVaryingIndex++)
        {
            PackedVarying *output = &vertexVaryings[vertVaryingIndex];
            if (output->name == input->name)
            {
                if (!linkValidateVaryings(infoLog, output->name, *input, *output))
                {
                    return false;
                }

                output->registerIndex = input->registerIndex;
                output->columnIndex = input->columnIndex;

                matched = true;
                break;
            }
        }

        // We permit unmatched, unreferenced varyings
        if (!matched && input->staticUse)
        {
            infoLog.append("Fragment varying %s does not match any vertex varying", input->name.c_str());
            return false;
        }
    }

    return true;
}

bool Program::linkValidateInterfaceBlockFields(InfoLog &infoLog, const std::string &uniformName, const sh::InterfaceBlockField &vertexUniform, const sh::InterfaceBlockField &fragmentUniform)
{
    if (!linkValidateVariablesBase(infoLog, uniformName, vertexUniform, fragmentUniform, true))
    {
        return false;
    }

    if (vertexUniform.isRowMajorLayout != fragmentUniform.isRowMajorLayout)
    {
        infoLog.append("Matrix packings for %s differ between vertex and fragment shaders", uniformName.c_str());
        return false;
    }

    return true;
}

// Determines the mapping between GL attributes and Direct3D 9 vertex stream usage indices
bool Program::linkAttributes(const Data &data,
                             InfoLog &infoLog,
                             const AttributeBindings &attributeBindings,
                             const Shader *vertexShader)
{
    unsigned int usedLocations = 0;
    const std::vector<sh::Attribute> &shaderAttributes = vertexShader->getActiveAttributes();
    GLuint maxAttribs = data.caps->maxVertexAttributes;

    // TODO(jmadill): handle aliasing robustly
    if (shaderAttributes.size() >= maxAttribs)
    {
        infoLog.append("Too many vertex attributes.");
        return false;
    }

    // Link attributes that have a binding location
    for (unsigned int attributeIndex = 0; attributeIndex < shaderAttributes.size(); attributeIndex++)
    {
        const sh::Attribute &attribute = shaderAttributes[attributeIndex];

        ASSERT(attribute.staticUse);

        const int location = attribute.location == -1 ? attributeBindings.getAttributeBinding(attribute.name) : attribute.location;

        mProgram->setShaderAttribute(attributeIndex, attribute);

        if (location != -1)   // Set by glBindAttribLocation or by location layout qualifier
        {
            const int rows = VariableRegisterCount(attribute.type);

            if (static_cast<GLuint>(rows + location) > maxAttribs)
            {
                infoLog.append("Active attribute (%s) at location %d is too big to fit", attribute.name.c_str(), location);

                return false;
            }

            for (int row = 0; row < rows; row++)
            {
                const int rowLocation = location + row;
                sh::ShaderVariable &linkedAttribute = mLinkedAttribute[rowLocation];

                // In GLSL 3.00, attribute aliasing produces a link error
                // In GLSL 1.00, attribute aliasing is allowed, but ANGLE currently has a bug
                // TODO(jmadill): fix aliasing on ES2
                // if (mProgram->getShaderVersion() >= 300)
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
    for (unsigned int attributeIndex = 0; attributeIndex < shaderAttributes.size(); attributeIndex++)
    {
        const sh::Attribute &attribute = shaderAttributes[attributeIndex];

        ASSERT(attribute.staticUse);

        const int location = attribute.location == -1 ? attributeBindings.getAttributeBinding(attribute.name) : attribute.location;

        if (location == -1)   // Not set by glBindAttribLocation or by location layout qualifier
        {
            int rows = VariableRegisterCount(attribute.type);
            int availableIndex = AllocateFirstFreeBits(&usedLocations, rows, maxAttribs);

            if (availableIndex == -1 || static_cast<GLuint>(availableIndex + rows) > maxAttribs)
            {
                infoLog.append("Too many active attributes (%s)", attribute.name.c_str());

                return false;   // Fail to link
            }

            mLinkedAttribute[availableIndex] = attribute;
        }
    }

    for (GLuint attributeIndex = 0; attributeIndex < maxAttribs;)
    {
        int index = vertexShader->getSemanticIndex(mLinkedAttribute[attributeIndex].name);
        int rows = VariableRegisterCount(mLinkedAttribute[attributeIndex].type);

        for (int r = 0; r < rows; r++)
        {
            mProgram->getSemanticIndexes()[attributeIndex++] = index++;
        }
    }

    return true;
}

bool Program::linkUniformBlocks(InfoLog &infoLog, const Shader &vertexShader, const Shader &fragmentShader, const Caps &caps)
{
    const std::vector<sh::InterfaceBlock> &vertexInterfaceBlocks = vertexShader.getInterfaceBlocks();
    const std::vector<sh::InterfaceBlock> &fragmentInterfaceBlocks = fragmentShader.getInterfaceBlocks();
    // Check that interface blocks defined in the vertex and fragment shaders are identical
    typedef std::map<std::string, const sh::InterfaceBlock*> UniformBlockMap;
    UniformBlockMap linkedUniformBlocks;
    for (unsigned int blockIndex = 0; blockIndex < vertexInterfaceBlocks.size(); blockIndex++)
    {
        const sh::InterfaceBlock &vertexInterfaceBlock = vertexInterfaceBlocks[blockIndex];
        linkedUniformBlocks[vertexInterfaceBlock.name] = &vertexInterfaceBlock;
    }
    for (unsigned int blockIndex = 0; blockIndex < fragmentInterfaceBlocks.size(); blockIndex++)
    {
        const sh::InterfaceBlock &fragmentInterfaceBlock = fragmentInterfaceBlocks[blockIndex];
        UniformBlockMap::const_iterator entry = linkedUniformBlocks.find(fragmentInterfaceBlock.name);
        if (entry != linkedUniformBlocks.end())
        {
            const sh::InterfaceBlock &vertexInterfaceBlock = *entry->second;
            if (!areMatchingInterfaceBlocks(infoLog, vertexInterfaceBlock, fragmentInterfaceBlock))
            {
                return false;
            }
        }
    }
    for (unsigned int blockIndex = 0; blockIndex < vertexInterfaceBlocks.size(); blockIndex++)
    {
        const sh::InterfaceBlock &interfaceBlock = vertexInterfaceBlocks[blockIndex];
        // Note: shared and std140 layouts are always considered active
        if (interfaceBlock.staticUse || interfaceBlock.layout != sh::BLOCKLAYOUT_PACKED)
        {
            if (!mProgram->defineUniformBlock(infoLog, vertexShader, interfaceBlock, caps))
            {
                return false;
            }
        }
    }
    for (unsigned int blockIndex = 0; blockIndex < fragmentInterfaceBlocks.size(); blockIndex++)
    {
        const sh::InterfaceBlock &interfaceBlock = fragmentInterfaceBlocks[blockIndex];
        // Note: shared and std140 layouts are always considered active
        if (interfaceBlock.staticUse || interfaceBlock.layout != sh::BLOCKLAYOUT_PACKED)
        {
            if (!mProgram->defineUniformBlock(infoLog, fragmentShader, interfaceBlock, caps))
            {
                return false;
            }
        }
    }
    return true;
}

bool Program::areMatchingInterfaceBlocks(gl::InfoLog &infoLog, const sh::InterfaceBlock &vertexInterfaceBlock,
                                         const sh::InterfaceBlock &fragmentInterfaceBlock)
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
    const unsigned int numBlockMembers = static_cast<unsigned int>(vertexInterfaceBlock.fields.size());
    for (unsigned int blockMemberIndex = 0; blockMemberIndex < numBlockMembers; blockMemberIndex++)
    {
        const sh::InterfaceBlockField &vertexMember = vertexInterfaceBlock.fields[blockMemberIndex];
        const sh::InterfaceBlockField &fragmentMember = fragmentInterfaceBlock.fields[blockMemberIndex];
        if (vertexMember.name != fragmentMember.name)
        {
            infoLog.append("Name mismatch for field %d of interface block '%s': (in vertex: '%s', in fragment: '%s')",
                           blockMemberIndex, blockName, vertexMember.name.c_str(), fragmentMember.name.c_str());
            return false;
        }
        std::string memberName = "interface block '" + vertexInterfaceBlock.name + "' member '" + vertexMember.name + "'";
        if (!linkValidateInterfaceBlockFields(infoLog, memberName, vertexMember, fragmentMember))
        {
            return false;
        }
    }
    return true;
}

bool Program::linkValidateVariablesBase(InfoLog &infoLog, const std::string &variableName, const sh::ShaderVariable &vertexVariable,
                                              const sh::ShaderVariable &fragmentVariable, bool validatePrecision)
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

    if (vertexVariable.fields.size() != fragmentVariable.fields.size())
    {
        infoLog.append("Structure lengths for %s differ between vertex and fragment shaders", variableName.c_str());
        return false;
    }
    const unsigned int numMembers = static_cast<unsigned int>(vertexVariable.fields.size());
    for (unsigned int memberIndex = 0; memberIndex < numMembers; memberIndex++)
    {
        const sh::ShaderVariable &vertexMember = vertexVariable.fields[memberIndex];
        const sh::ShaderVariable &fragmentMember = fragmentVariable.fields[memberIndex];

        if (vertexMember.name != fragmentMember.name)
        {
            infoLog.append("Name mismatch for field '%d' of %s: (in vertex: '%s', in fragment: '%s')",
                           memberIndex, variableName.c_str(),
                           vertexMember.name.c_str(), fragmentMember.name.c_str());
            return false;
        }

        const std::string memberName = variableName.substr(0, variableName.length() - 1) + "." +
                                       vertexMember.name + "'";

        if (!linkValidateVariablesBase(infoLog, vertexMember.name, vertexMember, fragmentMember, validatePrecision))
        {
            return false;
        }
    }

    return true;
}

bool Program::linkValidateUniforms(InfoLog &infoLog, const std::string &uniformName, const sh::Uniform &vertexUniform, const sh::Uniform &fragmentUniform)
{
    if (!linkValidateVariablesBase(infoLog, uniformName, vertexUniform, fragmentUniform, true))
    {
        return false;
    }

    return true;
}

bool Program::linkValidateVaryings(InfoLog &infoLog, const std::string &varyingName, const sh::Varying &vertexVarying, const sh::Varying &fragmentVarying)
{
    if (!linkValidateVariablesBase(infoLog, varyingName, vertexVarying, fragmentVarying, false))
    {
        return false;
    }

    if (!sh::InterpolationTypesMatch(vertexVarying.interpolation, fragmentVarying.interpolation))
    {
        infoLog.append("Interpolation types for %s differ between vertex and fragment shaders", varyingName.c_str());
        return false;
    }

    return true;
}

bool Program::gatherTransformFeedbackLinkedVaryings(InfoLog &infoLog, const std::vector<LinkedVarying> &linkedVaryings,
                                                    const std::vector<std::string> &transformFeedbackVaryingNames,
                                                    GLenum transformFeedbackBufferMode,
                                                    std::vector<LinkedVarying> *outTransformFeedbackLinkedVaryings,
                                                    const Caps &caps) const
{
    size_t totalComponents = 0;

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
                    componentCount > caps.maxTransformFeedbackSeparateComponents)
                {
                    infoLog.append("Transform feedback varying's %s components (%u) exceed the maximum separate components (%u).",
                                   linkedVaryings[j].name.c_str(), componentCount, caps.maxTransformFeedbackSeparateComponents);
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

    if (transformFeedbackBufferMode == GL_INTERLEAVED_ATTRIBS && totalComponents > caps.maxTransformFeedbackInterleavedComponents)
    {
        infoLog.append("Transform feedback varying total components (%u) exceed the maximum interleaved components (%u).",
                       totalComponents, caps.maxTransformFeedbackInterleavedComponents);
        return false;
    }

    return true;
}

}
