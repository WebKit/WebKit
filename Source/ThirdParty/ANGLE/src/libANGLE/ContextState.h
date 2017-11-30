//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ContextState: Container class for all GL context state, caps and objects.

#ifndef LIBANGLE_CONTEXTSTATE_H_
#define LIBANGLE_CONTEXTSTATE_H_

#include "common/MemoryBuffer.h"
#include "common/angleutils.h"
#include "libANGLE/State.h"
#include "libANGLE/Version.h"
#include "libANGLE/params.h"

namespace gl
{
class BufferManager;
class ContextState;
class FramebufferManager;
class PathManager;
class ProgramPipelineManager;
class RenderbufferManager;
class SamplerManager;
class ShaderProgramManager;
class SyncManager;
class TextureManager;
class ValidationContext;

static constexpr Version ES_2_0 = Version(2, 0);
static constexpr Version ES_3_0 = Version(3, 0);
static constexpr Version ES_3_1 = Version(3, 1);

using ContextID = uintptr_t;

class ContextState final : angle::NonCopyable
{
  public:
    ContextState(ContextID context,
                 const ContextState *shareContextState,
                 TextureManager *shareTextures,
                 const Version &clientVersion,
                 State *state,
                 const Caps &caps,
                 const TextureCapsMap &textureCaps,
                 const Extensions &extensions,
                 const Limitations &limitations);
    ~ContextState();

    ContextID getContextID() const { return mContext; }
    GLint getClientMajorVersion() const { return mClientVersion.major; }
    GLint getClientMinorVersion() const { return mClientVersion.minor; }
    const Version &getClientVersion() const { return mClientVersion; }
    const State &getState() const { return *mState; }
    const Caps &getCaps() const { return mCaps; }
    const TextureCapsMap &getTextureCaps() const { return mTextureCaps; }
    const Extensions &getExtensions() const { return mExtensions; }
    const Limitations &getLimitations() const { return mLimitations; }

    const TextureCaps &getTextureCap(GLenum internalFormat) const;

    bool usingDisplayTextureShareGroup() const;

    bool isWebGL() const;
    bool isWebGL1() const;

  private:
    friend class Context;
    friend class ValidationContext;

    Version mClientVersion;
    ContextID mContext;
    State *mState;
    const Caps &mCaps;
    const TextureCapsMap &mTextureCaps;
    const Extensions &mExtensions;
    const Limitations &mLimitations;

    BufferManager *mBuffers;
    ShaderProgramManager *mShaderPrograms;
    TextureManager *mTextures;
    RenderbufferManager *mRenderbuffers;
    SamplerManager *mSamplers;
    SyncManager *mSyncs;
    PathManager *mPaths;
    FramebufferManager *mFramebuffers;
    ProgramPipelineManager *mPipelines;
};

class ValidationContext : angle::NonCopyable
{
  public:
    ValidationContext(const ValidationContext *shareContext,
                      TextureManager *shareTextures,
                      const Version &clientVersion,
                      State *state,
                      const Caps &caps,
                      const TextureCapsMap &textureCaps,
                      const Extensions &extensions,
                      const Limitations &limitations,
                      bool skipValidation);
    virtual ~ValidationContext();

    virtual void handleError(const Error &error) = 0;

    const ContextState &getContextState() const { return mState; }
    GLint getClientMajorVersion() const { return mState.getClientMajorVersion(); }
    GLint getClientMinorVersion() const { return mState.getClientMinorVersion(); }
    const Version &getClientVersion() const { return mState.getClientVersion(); }
    const State &getGLState() const { return mState.getState(); }
    const Caps &getCaps() const { return mState.getCaps(); }
    const TextureCapsMap &getTextureCaps() const { return mState.getTextureCaps(); }
    const Extensions &getExtensions() const { return mState.getExtensions(); }
    const Limitations &getLimitations() const { return mState.getLimitations(); }
    bool skipValidation() const { return mSkipValidation; }

    // Specific methods needed for validation.
    bool getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams);
    bool getIndexedQueryParameterInfo(GLenum target, GLenum *type, unsigned int *numParams);

    Program *getProgram(GLuint handle) const;
    Shader *getShader(GLuint handle) const;

    bool isTextureGenerated(GLuint texture) const;
    bool isBufferGenerated(GLuint buffer) const;
    bool isRenderbufferGenerated(GLuint renderbuffer) const;
    bool isFramebufferGenerated(GLuint framebuffer) const;
    bool isProgramPipelineGenerated(GLuint pipeline) const;

    bool usingDisplayTextureShareGroup() const;

    // Hack for the special WebGL 1 "DEPTH_STENCIL" internal format.
    GLenum getConvertedRenderbufferFormat(GLenum internalformat) const;

    bool isWebGL() const { return mState.isWebGL(); }
    bool isWebGL1() const { return mState.isWebGL1(); }

    template <typename T>
    const T &getParams() const;

  protected:
    ContextState mState;
    bool mSkipValidation;
    bool mDisplayTextureShareGroup;

    // Caches entry point parameters and values re-used between layers.
    mutable const ParamTypeInfo *mSavedArgsType;
    static constexpr size_t kParamsBufferSize = 64u;
    mutable std::array<uint8_t, kParamsBufferSize> mParamsBuffer;
};

template <typename T>
const T &ValidationContext::getParams() const
{
    const T *params = reinterpret_cast<T *>(mParamsBuffer.data());
    ASSERT(mSavedArgsType->hasDynamicType(T::TypeInfo));
    return *params;
}

}  // namespace gl

#endif  // LIBANGLE_CONTEXTSTATE_H_
