//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.h: Defines the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#ifndef LIBANGLE_PROGRAM_H_
#define LIBANGLE_PROGRAM_H_

#include "libANGLE/angletypes.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"

#include "common/angleutils.h"

#include <GLES2/gl2.h>
#include <GLSLANG/ShaderLang.h>

#include <vector>
#include <sstream>
#include <string>
#include <set>

namespace rx
{
class Renderer;
class Renderer;
struct TranslatedAttribute;
class ProgramImpl;
}

namespace gl
{
struct Caps;
struct Data;
class ResourceManager;
class Shader;
class InfoLog;
class AttributeBindings;
class Buffer;
class Framebuffer;
struct UniformBlock;
struct LinkedUniform;

extern const char * const g_fakepath;

class AttributeBindings
{
  public:
    AttributeBindings();
    ~AttributeBindings();

    void bindAttributeLocation(GLuint index, const char *name);
    int getAttributeBinding(const std::string &name) const;

  private:
    std::set<std::string> mAttributeBinding[MAX_VERTEX_ATTRIBS];
};

class InfoLog : angle::NonCopyable
{
  public:
    InfoLog();
    ~InfoLog();

    size_t getLength() const;
    void getLog(GLsizei bufSize, GLsizei *length, char *infoLog);

    void appendSanitized(const char *message);
    void append(const char *info, ...);
    void reset();
  private:
    std::stringstream mStream;
};

// Struct used for correlating uniforms/elements of uniform arrays to handles
struct VariableLocation
{
    VariableLocation();
    VariableLocation(const std::string &name, unsigned int element, unsigned int index);

    std::string name;
    unsigned int element;
    unsigned int index;
};

struct LinkedVarying
{
    LinkedVarying();
    LinkedVarying(const std::string &name, GLenum type, GLsizei size, const std::string &semanticName,
        unsigned int semanticIndex, unsigned int semanticIndexCount);

    // Original GL name
    std::string name;

    GLenum type;
    GLsizei size;

    // DirectX semantic information
    std::string semanticName;
    unsigned int semanticIndex;
    unsigned int semanticIndexCount;
};

class Program : angle::NonCopyable
{
  public:
    Program(rx::ProgramImpl *impl, ResourceManager *manager, GLuint handle);
    ~Program();

    GLuint id() const { return mHandle; }

    rx::ProgramImpl *getImplementation() { return mProgram; }
    const rx::ProgramImpl *getImplementation() const { return mProgram; }

    bool attachShader(Shader *shader);
    bool detachShader(Shader *shader);
    int getAttachedShadersCount() const;

    void bindAttributeLocation(GLuint index, const char *name);

    Error link(const Data &data);
    bool isLinked();

    Error loadBinary(GLenum binaryFormat, const void *binary, GLsizei length);
    Error saveBinary(GLenum *binaryFormat, void *binary, GLsizei bufSize, GLsizei *length) const;
    GLint getBinaryLength() const;

    int getInfoLogLength() const;
    void getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog);
    void getAttachedShaders(GLsizei maxCount, GLsizei *count, GLuint *shaders);

    GLuint getAttributeLocation(const std::string &name);
    int getSemanticIndex(int attributeIndex);
    const int *getSemanticIndexes() const;

    void getActiveAttribute(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    GLint getActiveAttributeCount();
    GLint getActiveAttributeMaxLength();

    GLint getSamplerMapping(SamplerType type, unsigned int samplerIndex, const Caps &caps);
    GLenum getSamplerTextureType(SamplerType type, unsigned int samplerIndex);
    GLint getUsedSamplerRange(SamplerType type);
    bool usesPointSize() const;

    GLint getFragDataLocation(const std::string &name) const;

    void getActiveUniform(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    GLint getActiveUniformCount();
    GLint getActiveUniformMaxLength();
    GLint getActiveUniformi(GLuint index, GLenum pname) const;
    bool isValidUniformLocation(GLint location) const;
    LinkedUniform *getUniformByLocation(GLint location) const;
    LinkedUniform *getUniformByName(const std::string &name) const;

    GLint getUniformLocation(const std::string &name);
    GLuint getUniformIndex(const std::string &name);
    void setUniform1fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform2fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform3fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform4fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform1iv(GLint location, GLsizei count, const GLint *v);
    void setUniform2iv(GLint location, GLsizei count, const GLint *v);
    void setUniform3iv(GLint location, GLsizei count, const GLint *v);
    void setUniform4iv(GLint location, GLsizei count, const GLint *v);
    void setUniform1uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform2uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform3uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform4uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

    void getUniformfv(GLint location, GLfloat *params);
    void getUniformiv(GLint location, GLint *params);
    void getUniformuiv(GLint location, GLuint *params);

    Error applyUniforms();
    Error applyUniformBuffers(const gl::Data &data);

    void getActiveUniformBlockName(GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName) const;
    void getActiveUniformBlockiv(GLuint uniformBlockIndex, GLenum pname, GLint *params) const;
    GLuint getActiveUniformBlockCount();
    GLint getActiveUniformBlockMaxLength();

    GLuint getUniformBlockIndex(const std::string &name);

    void bindUniformBlock(GLuint uniformBlockIndex, GLuint uniformBlockBinding);
    GLuint getUniformBlockBinding(GLuint uniformBlockIndex) const;

    const UniformBlock *getUniformBlockByIndex(GLuint index) const;

    void setTransformFeedbackVaryings(GLsizei count, const GLchar *const *varyings, GLenum bufferMode);
    void getTransformFeedbackVarying(GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name) const;
    GLsizei getTransformFeedbackVaryingCount() const;
    GLsizei getTransformFeedbackVaryingMaxLength() const;
    GLenum getTransformFeedbackBufferMode() const;

    static bool linkVaryings(InfoLog &infoLog, Shader *fragmentShader, Shader *vertexShader);
    static bool linkValidateUniforms(InfoLog &infoLog, const std::string &uniformName, const sh::Uniform &vertexUniform, const sh::Uniform &fragmentUniform);
    static bool linkValidateInterfaceBlockFields(InfoLog &infoLog, const std::string &uniformName, const sh::InterfaceBlockField &vertexUniform, const sh::InterfaceBlockField &fragmentUniform);

    void addRef();
    void release();
    unsigned int getRefCount() const;
    void flagForDeletion();
    bool isFlaggedForDeletion() const;

    void validate(const Caps &caps);
    bool validateSamplers(InfoLog *infoLog, const Caps &caps);
    bool isValidated() const;
    void updateSamplerMapping();

  private:
    void unlink(bool destroy = false);
    void resetUniformBlockBindings();

    bool linkAttributes(const Data &data,
                        InfoLog &infoLog,
                        const AttributeBindings &attributeBindings,
                        const Shader *vertexShader);
    bool linkUniformBlocks(InfoLog &infoLog, const Shader &vertexShader, const Shader &fragmentShader, const Caps &caps);
    bool areMatchingInterfaceBlocks(gl::InfoLog &infoLog, const sh::InterfaceBlock &vertexInterfaceBlock,
                                    const sh::InterfaceBlock &fragmentInterfaceBlock);

    static bool linkValidateVariablesBase(InfoLog &infoLog,
                                          const std::string &variableName,
                                          const sh::ShaderVariable &vertexVariable,
                                          const sh::ShaderVariable &fragmentVariable,
                                          bool validatePrecision);

    static bool linkValidateVaryings(InfoLog &infoLog, const std::string &varyingName, const sh::Varying &vertexVarying, const sh::Varying &fragmentVarying);
    bool gatherTransformFeedbackLinkedVaryings(InfoLog &infoLog, const std::vector<LinkedVarying> &linkedVaryings,
                                               const std::vector<std::string> &transformFeedbackVaryingNames,
                                               GLenum transformFeedbackBufferMode,
                                               std::vector<LinkedVarying> *outTransformFeedbackLinkedVaryings,
                                               const Caps &caps) const;
    bool assignUniformBlockRegister(InfoLog &infoLog, UniformBlock *uniformBlock, GLenum shader, unsigned int registerIndex, const Caps &caps);
    void defineOutputVariables(Shader *fragmentShader);

    rx::ProgramImpl *mProgram;

    sh::Attribute mLinkedAttribute[MAX_VERTEX_ATTRIBS];

    std::map<int, VariableLocation> mOutputVariables;

    bool mValidated;

    Shader *mFragmentShader;
    Shader *mVertexShader;

    AttributeBindings mAttributeBindings;

    GLuint mUniformBlockBindings[IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS];

    std::vector<std::string> mTransformFeedbackVaryings;
    GLenum mTransformFeedbackBufferMode;

    bool mLinked;
    bool mDeleteStatus;   // Flag to indicate that the program can be deleted when no longer in use

    unsigned int mRefCount;

    ResourceManager *mResourceManager;
    const GLuint mHandle;

    InfoLog mInfoLog;
};
}

#endif   // LIBANGLE_PROGRAM_H_
