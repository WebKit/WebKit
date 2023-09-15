//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
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

#include <GLSLANG/ShaderLang.h>
#include "angle_gl.h"

#include "common/BinaryStream.h"
#include "common/CompiledShaderState.h"
#include "common/MemoryBuffer.h"
#include "common/Optional.h"
#include "common/angleutils.h"
#include "libANGLE/BlobCache.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Debug.h"
#include "libANGLE/angletypes.h"

namespace rx
{
class GLImplFactory;
class ShaderImpl;
class ShaderSh;
class WaitableCompileEvent;
}  // namespace rx

namespace angle
{
class WaitableEvent;
class WorkerThreadPool;
}  // namespace angle

namespace gl
{
class CompileTask;
class Context;
class ShaderProgramManager;
class State;
class BinaryInputStream;
class BinaryOutputStream;

// We defer the compile until link time, or until properties are queried.
enum class CompileStatus
{
    // Compilation never done, or has failed.
    NOT_COMPILED,
    // Compile is in progress.
    COMPILE_REQUESTED,
    // Compilation job is done, but is being resolved.  This enum value is there to allow access to
    // compiled state during resolve without triggering threading-related assertions (which ensure
    // no compile job is in progress).
    IS_RESOLVING,
    // Compilation was successful.
    COMPILED,
};

class ShaderState final : angle::NonCopyable
{
  public:
    ShaderState(ShaderType shaderType);
    ~ShaderState();

    const std::string &getLabel() const { return mLabel; }

    const std::string &getSource() const { return mSource; }
    bool compilePending() const { return mCompileStatus == CompileStatus::COMPILE_REQUESTED; }
    CompileStatus getCompileStatus() const { return mCompileStatus; }

    ShaderType getShaderType() const { return mCompiledState->shaderType; }

    const SharedCompiledShaderState &getCompiledState() const
    {
        ASSERT(!compilePending());
        return mCompiledState;
    }

  private:
    friend class Shader;

    std::string mLabel;
    std::string mSource;
    size_t mSourceHash = 0;

    SharedCompiledShaderState mCompiledState;

    // Indicates if this shader has been successfully compiled
    CompileStatus mCompileStatus = CompileStatus::NOT_COMPILED;
};

class Shader final : angle::NonCopyable, public LabeledObject
{
  public:
    Shader(ShaderProgramManager *manager,
           rx::GLImplFactory *implFactory,
           const gl::Limitations &rendererLimitations,
           ShaderType type,
           ShaderProgramID handle);

    void onDestroy(const Context *context);

    angle::Result setLabel(const Context *context, const std::string &label) override;
    const std::string &getLabel() const override;

    ShaderType getType() const { return mState.getShaderType(); }
    ShaderProgramID getHandle() const;

    rx::ShaderImpl *getImplementation() const { return mImplementation.get(); }

    void setSource(const Context *context,
                   GLsizei count,
                   const char *const *string,
                   const GLint *length);
    int getInfoLogLength(const Context *context);
    void getInfoLog(const Context *context, GLsizei bufSize, GLsizei *length, char *infoLog);
    std::string getInfoLogString() const { return mInfoLog; }
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

    size_t getSourceHash() const;

    void compile(const Context *context);
    bool isCompiled(const Context *context);
    bool isCompleted();

    // Return the compiled shader state for the program.  The program holds a reference to this
    // state, so the shader is free to recompile, get deleted, etc.
    const SharedCompiledShaderState &getCompiledState() const { return mState.getCompiledState(); }

    void addRef();
    void release(const Context *context);
    unsigned int getRefCount() const;
    bool isFlaggedForDeletion() const;
    void flagForDeletion();

    const ShaderState &getState() const { return mState; }

    GLuint getCurrentMaxComputeWorkGroupInvocations() const
    {
        return mCurrentMaxComputeWorkGroupInvocations;
    }

    unsigned int getMaxComputeSharedMemory() const { return mMaxComputeSharedMemory; }
    bool hasBeenDeleted() const { return mDeleteStatus; }

    // Block until compiling is finished and resolve it.
    void resolveCompile(const Context *context);

    // Writes a shader's binary to the output memory buffer.
    angle::Result serialize(const Context *context, angle::MemoryBuffer *binaryOut) const;
    angle::Result deserialize(BinaryInputStream &stream);

    // Load a binary from shader cache.
    angle::Result loadBinary(const Context *context, const void *binary, GLsizei length);
    // Load a binary from a glShaderBinary call.
    angle::Result loadShaderBinary(const Context *context, const void *binary, GLsizei length);

    void writeShaderKey(BinaryOutputStream *streamOut) const
    {
        ASSERT(streamOut && !mShaderHash.empty());
        streamOut->writeBytes(mShaderHash.data(), egl::BlobCache::kKeyLength);
        return;
    }

  private:
    struct CompilingState;

    ~Shader() override;
    static std::string joinShaderSources(GLsizei count,
                                         const char *const *string,
                                         const GLint *length);
    static void GetSourceImpl(const std::string &source,
                              GLsizei bufSize,
                              GLsizei *length,
                              char *buffer);
    angle::Result loadBinaryImpl(const Context *context,
                                 const void *binary,
                                 GLsizei length,
                                 bool generatedWithOfflineCompiler);

    // Compute a key to uniquely identify the shader object in memory caches.
    void setShaderKey(const Context *context,
                      const ShCompileOptions &compileOptions,
                      const ShShaderOutput &outputType,
                      const ShBuiltInResources &resources);

    ShaderState mState;
    std::unique_ptr<rx::ShaderImpl> mImplementation;
    const gl::Limitations mRendererLimitations;
    const ShaderProgramID mHandle;
    unsigned int mRefCount;  // Number of program objects this shader is attached to
    bool mDeleteStatus;  // Flag to indicate that the shader can be deleted when no longer in use
    std::string mInfoLog;

    // We keep a reference to the translator in order to defer compiles while preserving settings.
    BindingPointer<Compiler> mBoundCompiler;
    std::unique_ptr<CompilingState> mCompilingState;
    egl::BlobCache::Key mShaderHash;

    ShaderProgramManager *mResourceManager;

    GLuint mCurrentMaxComputeWorkGroupInvocations;
    unsigned int mMaxComputeSharedMemory;
};

const char *GetShaderTypeString(ShaderType type);
std::string GetShaderDumpFileDirectory();
std::string GetShaderDumpFileName(size_t shaderHash);

}  // namespace gl

#endif  // LIBANGLE_SHADER_H_
