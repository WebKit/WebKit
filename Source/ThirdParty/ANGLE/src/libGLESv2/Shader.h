//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Shader.h: Defines the abstract gl::Shader class and its concrete derived
// classes VertexShader and FragmentShader. Implements GL shader objects and
// related functionality. [OpenGL ES 2.0.24] section 2.10 page 24 and section
// 3.8 page 84.

#ifndef LIBGLESV2_SHADER_H_
#define LIBGLESV2_SHADER_H_

#define GL_APICALL
#include <GLES2/gl2.h>
#include <d3dx9.h>
#include <list>
#include <vector>

#include "libGLESv2/ResourceManager.h"

namespace gl
{
struct Varying
{
    Varying(GLenum type, const std::string &name, int size, bool array)
        : type(type), name(name), size(size), array(array), reg(-1), col(-1)
    {
    }

    GLenum type;
    std::string name;
    int size;   // Number of 'type' elements
    bool array;

    int reg;    // First varying register, assigned during link
    int col;    // First register element, assigned during link
};

typedef std::list<Varying> VaryingList;

class Shader
{
    friend Program;

  public:
    Shader(ResourceManager *manager, GLuint handle);

    virtual ~Shader();

    virtual GLenum getType() = 0;
    GLuint getHandle() const;

    void deleteSource();
    void setSource(GLsizei count, const char **string, const GLint *length);
    int getInfoLogLength() const;
    void getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog);
    int getSourceLength() const;
    void getSource(GLsizei bufSize, GLsizei *length, char *buffer);
    int getTranslatedSourceLength() const;
    void getTranslatedSource(GLsizei bufSize, GLsizei *length, char *buffer);

    virtual void compile() = 0;
    bool isCompiled();
    const char *getHLSL();

    void addRef();
    void release();
    unsigned int getRefCount() const;
    bool isFlaggedForDeletion() const;
    void flagForDeletion();

    static void releaseCompiler();

  protected:
    DISALLOW_COPY_AND_ASSIGN(Shader);

    void parseVaryings();

    void compileToHLSL(void *compiler);

    void getSourceImpl(char *source, GLsizei bufSize, GLsizei *length, char *buffer);

    static GLenum parseType(const std::string &type);
    static bool compareVarying(const Varying &x, const Varying &y);

    const GLuint mHandle;
    unsigned int mRefCount;     // Number of program objects this shader is attached to
    bool mDeleteStatus;         // Flag to indicate that the shader can be deleted when no longer in use

    char *mSource;
    char *mHlsl;
    char *mInfoLog;

    VaryingList varyings;

    bool mUsesFragCoord;
    bool mUsesFrontFacing;
    bool mUsesPointSize;
    bool mUsesPointCoord;

    ResourceManager *mResourceManager;

    static void *mFragmentCompiler;
    static void *mVertexCompiler;
};

struct Attribute
{
    Attribute() : type(GL_NONE), name("")
    {
    }

    Attribute(GLenum type, const std::string &name) : type(type), name(name)
    {
    }

    GLenum type;
    std::string name;
};

typedef std::vector<Attribute> AttributeArray;

class VertexShader : public Shader
{
    friend Program;

  public:
    VertexShader(ResourceManager *manager, GLuint handle);

    ~VertexShader();

    virtual GLenum getType();
    virtual void compile();
    int getSemanticIndex(const std::string &attributeName);

  private:
    DISALLOW_COPY_AND_ASSIGN(VertexShader);

    void parseAttributes();

    AttributeArray mAttributes;
};

class FragmentShader : public Shader
{
  public:
    FragmentShader(ResourceManager *manager, GLuint handle);

    ~FragmentShader();

    virtual GLenum getType();
    virtual void compile();

  private:
    DISALLOW_COPY_AND_ASSIGN(FragmentShader);
};
}

#endif   // LIBGLESV2_SHADER_H_
