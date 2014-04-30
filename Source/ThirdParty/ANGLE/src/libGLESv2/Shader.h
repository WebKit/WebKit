//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
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
#include <GLES3/gl3.h>
#include <GLES2/gl2.h>
#include <string>
#include <list>
#include <vector>

#include "common/shadervars.h"
#include "common/angleutils.h"
#include "libGLESv2/angletypes.h"

namespace rx
{
class Renderer;
}

namespace gl
{
class ResourceManager;

class Shader
{
    friend class DynamicHLSL;

  public:
    Shader(ResourceManager *manager, const rx::Renderer *renderer, GLuint handle);

    virtual ~Shader();

    virtual GLenum getType() = 0;
    GLuint getHandle() const;

    void deleteSource();
    void setSource(GLsizei count, const char *const *string, const GLint *length);
    int getInfoLogLength() const;
    void getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const;
    int getSourceLength() const;
    void getSource(GLsizei bufSize, GLsizei *length, char *buffer) const;
    int getTranslatedSourceLength() const;
    void getTranslatedSource(GLsizei bufSize, GLsizei *length, char *buffer) const;
    const std::vector<Uniform> &getUniforms() const;
    const std::vector<InterfaceBlock> &getInterfaceBlocks() const;
    std::vector<Varying> &getVaryings();

    virtual void compile() = 0;
    virtual void uncompile();
    bool isCompiled() const;
    const std::string &getHLSL() const;

    void addRef();
    void release();
    unsigned int getRefCount() const;
    bool isFlaggedForDeletion() const;
    void flagForDeletion();
    int getShaderVersion() const;
    void resetVaryingsRegisterAssignment();

    static void releaseCompiler();

    bool usesDepthRange() const { return mUsesDepthRange; }
    bool usesPointSize() const { return mUsesPointSize; }
    rx::D3DWorkaroundType getD3DWorkarounds() const;

  protected:
    void parseVaryings(void *compiler);

    void compileToHLSL(void *compiler);

    void getSourceImpl(const std::string &source, GLsizei bufSize, GLsizei *length, char *buffer) const;

    static bool compareVarying(const ShaderVariable &x, const ShaderVariable &y);

    const rx::Renderer *const mRenderer;

    std::vector<Varying> mVaryings;

    bool mUsesMultipleRenderTargets;
    bool mUsesFragColor;
    bool mUsesFragData;
    bool mUsesFragCoord;
    bool mUsesFrontFacing;
    bool mUsesPointSize;
    bool mUsesPointCoord;
    bool mUsesDepthRange;
    bool mUsesFragDepth;
    int mShaderVersion;
    bool mUsesDiscardRewriting;
    bool mUsesNestedBreak;

    static void *mFragmentCompiler;
    static void *mVertexCompiler;

  private:
    DISALLOW_COPY_AND_ASSIGN(Shader);

    void initializeCompiler();

    const GLuint mHandle;
    unsigned int mRefCount;     // Number of program objects this shader is attached to
    bool mDeleteStatus;         // Flag to indicate that the shader can be deleted when no longer in use

    std::string mSource;
    std::string mHlsl;
    std::string mInfoLog;
    std::vector<Uniform> mActiveUniforms;
    std::vector<InterfaceBlock> mActiveInterfaceBlocks;

    ResourceManager *mResourceManager;
};

class VertexShader : public Shader
{
    friend class DynamicHLSL;

  public:
    VertexShader(ResourceManager *manager, const rx::Renderer *renderer, GLuint handle);

    ~VertexShader();

    virtual GLenum getType();
    virtual void compile();
    virtual void uncompile();
    int getSemanticIndex(const std::string &attributeName);

    const std::vector<Attribute> &activeAttributes() const { return mActiveAttributes; }

  private:
    DISALLOW_COPY_AND_ASSIGN(VertexShader);

    void parseAttributes();

    std::vector<Attribute> mActiveAttributes;
};

class FragmentShader : public Shader
{
  public:
    FragmentShader(ResourceManager *manager,const rx::Renderer *renderer, GLuint handle);

    ~FragmentShader();

    virtual GLenum getType();
    virtual void compile();
    virtual void uncompile();
    const std::vector<Attribute> &getOutputVariables() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(FragmentShader);

    std::vector<Attribute> mActiveOutputVariables;
};
}

#endif   // LIBGLESV2_SHADER_H_
