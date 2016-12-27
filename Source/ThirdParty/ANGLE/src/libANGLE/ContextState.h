//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ContextState: Container class for all GL context state, caps and objects.

#ifndef LIBANGLE_CONTEXTSTATE_H_
#define LIBANGLE_CONTEXTSTATE_H_

#include "common/angleutils.h"
#include "libANGLE/State.h"

namespace gl
{
class ValidationContext;
class ContextState;

class GLVersion final : angle::NonCopyable
{
  public:
    GLVersion(GLint clientMajorVersion, GLint clientMinorVersion)
        : mClientMajorVersion(clientMajorVersion), mClientMinorVersion(clientMinorVersion)
    {
    }

    GLint getClientMajorVersion() const { return mClientMajorVersion; }
    GLint getClientMinorVersion() const { return mClientMinorVersion; }

    bool isES2() const { return mClientMajorVersion == 2; }
    bool isES3() const { return mClientMajorVersion == 3 && mClientMinorVersion == 0; }
    bool isES31() const { return mClientMajorVersion == 3 && mClientMinorVersion == 1; }
    bool isES3OrGreater() const { return mClientMajorVersion >= 3; }

  private:
    GLint mClientMajorVersion;
    GLint mClientMinorVersion;
};

class ContextState final : public angle::NonCopyable
{
  public:
    ContextState(uintptr_t context,
                 GLint clientMajorVersion,
                 GLint clientMinorVersion,
                 State *state,
                 const Caps &caps,
                 const TextureCapsMap &textureCaps,
                 const Extensions &extensions,
                 const ResourceManager *resourceManager,
                 const Limitations &limitations);
    ~ContextState();

    uintptr_t getContext() const { return mContext; }
    GLint getClientMajorVersion() const { return mGLVersion.getClientMajorVersion(); }
    GLint getClientMinorVersion() const { return mGLVersion.getClientMinorVersion(); }
    const GLVersion &getGLVersion() const { return mGLVersion; }
    const State &getState() const { return *mState; }
    const Caps &getCaps() const { return mCaps; }
    const TextureCapsMap &getTextureCaps() const { return mTextureCaps; }
    const Extensions &getExtensions() const { return mExtensions; }
    const ResourceManager &getResourceManager() const { return *mResourceManager; }
    const Limitations &getLimitations() const { return mLimitations; }

    const TextureCaps &getTextureCap(GLenum internalFormat) const;

  private:
    friend class Context;
    friend class ValidationContext;

    GLVersion mGLVersion;
    uintptr_t mContext;
    State *mState;
    const Caps &mCaps;
    const TextureCapsMap &mTextureCaps;
    const Extensions &mExtensions;
    const ResourceManager *mResourceManager;
    const Limitations &mLimitations;
};

class ValidationContext : angle::NonCopyable
{
  public:
    ValidationContext(GLint clientMajorVersion,
                      GLint clientMinorVersion,
                      State *state,
                      const Caps &caps,
                      const TextureCapsMap &textureCaps,
                      const Extensions &extensions,
                      const ResourceManager *resourceManager,
                      const Limitations &limitations,
                      bool skipValidation);
    virtual ~ValidationContext() {}

    virtual void handleError(const Error &error) = 0;

    const ContextState &getContextState() const { return mState; }
    int getClientMajorVersion() const { return mState.getClientMajorVersion(); }
    int getClientMinorVersion() const { return mState.getClientMinorVersion(); }
    const GLVersion &getGLVersion() const { return mState.mGLVersion; }
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

  protected:
    ContextState mState;
    bool mSkipValidation;
};
}  // namespace gl

#endif  // LIBANGLE_CONTEXTSTATE_H_
