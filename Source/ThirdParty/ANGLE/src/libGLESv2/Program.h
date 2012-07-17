//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.h: Defines the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#ifndef LIBGLESV2_PROGRAM_H_
#define LIBGLESV2_PROGRAM_H_

#include <d3dx9.h>
#include <string>
#include <set>

#include "libGLESv2/Shader.h"
#include "libGLESv2/Context.h"

namespace gl
{
class ResourceManager;
class FragmentShader;
class VertexShader;

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

class InfoLog
{
  public:
    InfoLog();
    ~InfoLog();

    int getLength() const;
    void getLog(GLsizei bufSize, GLsizei *length, char *infoLog);

    void appendSanitized(const char *message);
    void append(const char *info, ...);
    void reset();
  private:
    DISALLOW_COPY_AND_ASSIGN(InfoLog);
    char *mInfoLog;
};

class Program
{
  public:
    Program(ResourceManager *manager, GLuint handle);

    ~Program();

    bool attachShader(Shader *shader);
    bool detachShader(Shader *shader);
    int getAttachedShadersCount() const;

    void bindAttributeLocation(GLuint index, const char *name);

    void link();
    void setProgramBinary(ProgramBinary *programBinary);
    ProgramBinary *getProgramBinary();

    int getInfoLogLength() const;
    void getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog);
    void getAttachedShaders(GLsizei maxCount, GLsizei *count, GLuint *shaders);

    void getActiveAttribute(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    GLint getActiveAttributeCount();
    GLint getActiveAttributeMaxLength();

    void getActiveUniform(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    GLint getActiveUniformCount();
    GLint getActiveUniformMaxLength();

    void addRef();
    void release();
    unsigned int getRefCount() const;
    void flagForDeletion();
    bool isFlaggedForDeletion() const;

    void validate();
    bool isValidated() const;

    unsigned int getSerial() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Program);

    void unlink(bool destroy = false);

    static unsigned int issueSerial();

    FragmentShader *mFragmentShader;
    VertexShader *mVertexShader;

    AttributeBindings mAttributeBindings;

    ProgramBinary* mProgramBinary;
    bool mDeleteStatus;   // Flag to indicate that the program can be deleted when no longer in use

    unsigned int mRefCount;

    const unsigned int mSerial;

    static unsigned int mCurrentSerial;

    ResourceManager *mResourceManager;
    const GLuint mHandle;

    InfoLog mInfoLog;
};
}

#endif   // LIBGLESV2_PROGRAM_H_
