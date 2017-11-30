//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Shader.h: Defines the abstract gl::Shader class and its concrete derived
// classes VertexShader and FragmentShader. Implements GL shader objects and
// related functionality. [OpenGL ES 2.0.24] section 2.10 page 24 and section
// 3.8 page 84.

#ifndef LIBANGLE_SHADER_H_
#define LIBANGLE_SHADER_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "angle_gl.h"
#include <GLSLANG/ShaderLang.h>

#include "common/Optional.h"
#include "common/angleutils.h"
#include "libANGLE/Debug.h"
#include "libANGLE/angletypes.h"

namespace rx
{
class GLImplFactory;
class ShaderImpl;
class ShaderSh;
}

namespace gl
{
class Compiler;
class ContextState;
struct Limitations;
class ShaderProgramManager;
class Context;

// We defer the compile until link time, or until properties are queried.
enum class CompileStatus
{
    NOT_COMPILED,
    COMPILE_REQUESTED,
    COMPILED,
};

class ShaderState final : angle::NonCopyable
{
  public:
    ShaderState(GLenum shaderType);
    ~ShaderState();

    const std::string &getLabel() const { return mLabel; }

    const std::string &getSource() const { return mSource; }
    const std::string &getTranslatedSource() const { return mTranslatedSource; }

    GLenum getShaderType() const { return mShaderType; }
    int getShaderVersion() const { return mShaderVersion; }

    const std::vector<sh::Varying> &getInputVaryings() const { return mInputVaryings; }
    const std::vector<sh::Varying> &getOutputVaryings() const { return mOutputVaryings; }
    const std::vector<sh::Uniform> &getUniforms() const { return mUniforms; }
    const std::vector<sh::InterfaceBlock> &getUniformBlocks() const { return mUniformBlocks; }
    const std::vector<sh::InterfaceBlock> &getShaderStorageBlocks() const
    {
        return mShaderStorageBlocks;
    }
    const std::vector<sh::Attribute> &getActiveAttributes() const { return mActiveAttributes; }
    const std::vector<sh::OutputVariable> &getActiveOutputVariables() const
    {
        return mActiveOutputVariables;
    }

    bool compilePending() const { return mCompileStatus == CompileStatus::COMPILE_REQUESTED; }

  private:
    friend class Shader;

    std::string mLabel;

    GLenum mShaderType;
    int mShaderVersion;
    std::string mTranslatedSource;
    std::string mSource;

    sh::WorkGroupSize mLocalSize;

    std::vector<sh::Varying> mInputVaryings;
    std::vector<sh::Varying> mOutputVaryings;
    std::vector<sh::Uniform> mUniforms;
    std::vector<sh::InterfaceBlock> mUniformBlocks;
    std::vector<sh::InterfaceBlock> mShaderStorageBlocks;
    std::vector<sh::Attribute> mActiveAttributes;
    std::vector<sh::OutputVariable> mActiveOutputVariables;

    // ANGLE_multiview.
    int mNumViews;

    // Geometry Shader.
    GLenum mGeometryShaderInputPrimitiveType;
    GLenum mGeometryShaderOutputPrimitiveType;
    int mGeometryShaderInvocations;
    int mGeometryShaderMaxVertices;

    // Indicates if this shader has been successfully compiled
    CompileStatus mCompileStatus;
};

class Shader final : angle::NonCopyable, public LabeledObject
{
  public:
    Shader(ShaderProgramManager *manager,
           rx::GLImplFactory *implFactory,
           const gl::Limitations &rendererLimitations,
           GLenum type,
           GLuint handle);

    void onDestroy(const Context *context);

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    GLenum getType() const { return mType; }
    GLuint getHandle() const;

    rx::ShaderImpl *getImplementation() const { return mImplementation.get(); }

    void setSource(GLsizei count, const char *const *string, const GLint *length);
    int getInfoLogLength(const Context *context);
    void getInfoLog(const Context *context, GLsizei bufSize, GLsizei *length, char *infoLog);
    int getSourceLength() const;
    const std::string &getSourceString() const { return mState.getSource(); }
    void getSource(GLsizei bufSize, GLsizei *length, char *buffer) const;
    int getTranslatedSourceLength(const Context *context);
    int getTranslatedSourceWithDebugInfoLength(const Context *context);
    const std::string &getTranslatedSource(const Context *context);
    void getTranslatedSource(const Context *context,
                             GLsizei bufSize,
                             GLsizei *length,
                             char *buffer);
    void getTranslatedSourceWithDebugInfo(const Context *context,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          char *buffer);

    void compile(const Context *context);
    bool isCompiled(const Context *context);

    void addRef();
    void release(const Context *context);
    unsigned int getRefCount() const;
    bool isFlaggedForDeletion() const;
    void flagForDeletion();

    int getShaderVersion(const Context *context);

    const std::vector<sh::Varying> &getInputVaryings(const Context *context);
    const std::vector<sh::Varying> &getOutputVaryings(const Context *context);
    const std::vector<sh::Uniform> &getUniforms(const Context *context);
    const std::vector<sh::InterfaceBlock> &getUniformBlocks(const Context *context);
    const std::vector<sh::InterfaceBlock> &getShaderStorageBlocks(const Context *context);
    const std::vector<sh::Attribute> &getActiveAttributes(const Context *context);
    const std::vector<sh::OutputVariable> &getActiveOutputVariables(const Context *context);

    // Returns mapped name of a transform feedback varying. The original name may contain array
    // brackets with an index inside, which will get copied to the mapped name. The varying must be
    // known to be declared in the shader.
    std::string getTransformFeedbackVaryingMappedName(const std::string &tfVaryingName,
                                                      const Context *context);

    const sh::WorkGroupSize &getWorkGroupSize(const Context *context);

    int getNumViews(const Context *context);

    const std::string &getCompilerResourcesString() const;

  private:
    ~Shader() override;
    static void GetSourceImpl(const std::string &source,
                              GLsizei bufSize,
                              GLsizei *length,
                              char *buffer);

    void resolveCompile(const Context *context);

    ShaderState mState;
    std::string mLastCompiledSource;
    std::string mLastCompiledSourcePath;
    ShCompileOptions mLastCompileOptions;
    std::unique_ptr<rx::ShaderImpl> mImplementation;
    const gl::Limitations &mRendererLimitations;
    const GLuint mHandle;
    const GLenum mType;
    unsigned int mRefCount;     // Number of program objects this shader is attached to
    bool mDeleteStatus;         // Flag to indicate that the shader can be deleted when no longer in use
    std::string mInfoLog;

    // We keep a reference to the translator in order to defer compiles while preserving settings.
    BindingPointer<Compiler> mBoundCompiler;

    ShaderProgramManager *mResourceManager;
};

bool CompareShaderVar(const sh::ShaderVariable &x, const sh::ShaderVariable &y);
}  // namespace gl

#endif   // LIBANGLE_SHADER_H_
