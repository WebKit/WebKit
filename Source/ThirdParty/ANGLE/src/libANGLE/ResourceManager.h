//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ResourceManager.h : Defines the ResourceManager class, which tracks objects
// shared by multiple GL contexts.

#ifndef LIBANGLE_RESOURCEMANAGER_H_
#define LIBANGLE_RESOURCEMANAGER_H_

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/Error.h"
#include "libANGLE/HandleAllocator.h"
#include "libANGLE/HandleRangeAllocator.h"

namespace rx
{
class GLImplFactory;
}

namespace gl
{
class Buffer;
class FenceSync;
struct Limitations;
class Path;
class Program;
class Renderbuffer;
class Sampler;
class Shader;
class Texture;

class ResourceManager : angle::NonCopyable
{
  public:
    ResourceManager();
    ~ResourceManager();

    void addRef();
    void release();

    GLuint createBuffer();
    GLuint createShader(rx::GLImplFactory *factory,
                        const gl::Limitations &rendererLimitations,
                        GLenum type);
    GLuint createProgram(rx::GLImplFactory *factory);
    GLuint createTexture();
    GLuint createRenderbuffer();
    GLuint createSampler();
    GLuint createFenceSync(rx::GLImplFactory *factory);
    ErrorOrResult<GLuint> createPaths(rx::GLImplFactory *factory, GLsizei range);

    void deleteBuffer(GLuint buffer);
    void deleteShader(GLuint shader);
    void deleteProgram(GLuint program);
    void deleteTexture(GLuint texture);
    void deleteRenderbuffer(GLuint renderbuffer);
    void deleteSampler(GLuint sampler);
    void deleteFenceSync(GLuint fenceSync);
    void deletePaths(GLuint first, GLsizei range);

    Buffer *getBuffer(GLuint handle);
    Shader *getShader(GLuint handle) const;
    Program *getProgram(GLuint handle) const;
    Texture *getTexture(GLuint handle);
    Renderbuffer *getRenderbuffer(GLuint handle);
    Sampler *getSampler(GLuint handle);
    FenceSync *getFenceSync(GLuint handle);

    // CHROMIUM_path_rendering
    const Path *getPath(GLuint path) const;
    Path *getPath(GLuint path);
    bool hasPath(GLuint path) const;

    void setRenderbuffer(GLuint handle, Renderbuffer *renderbuffer);

    Buffer *checkBufferAllocation(rx::GLImplFactory *factory, GLuint handle);
    Texture *checkTextureAllocation(rx::GLImplFactory *factory, GLuint handle, GLenum type);
    Renderbuffer *checkRenderbufferAllocation(rx::GLImplFactory *factory, GLuint handle);
    Sampler *checkSamplerAllocation(rx::GLImplFactory *factory, GLuint samplerHandle);

    bool isSampler(GLuint sampler);

    // GL_CHROMIUM_bind_generates_resource
    bool isTextureGenerated(GLuint texture) const;
    bool isBufferGenerated(GLuint buffer) const;
    bool isRenderbufferGenerated(GLuint renderbuffer) const;

  private:
    void createTextureInternal(GLuint handle);

    std::size_t mRefCount;

    ResourceMap<Buffer> mBufferMap;
    HandleAllocator mBufferHandleAllocator;

    ResourceMap<Shader> mShaderMap;

    ResourceMap<Program> mProgramMap;
    HandleAllocator mProgramShaderHandleAllocator;

    ResourceMap<Texture> mTextureMap;
    HandleAllocator mTextureHandleAllocator;

    ResourceMap<Renderbuffer> mRenderbufferMap;
    HandleAllocator mRenderbufferHandleAllocator;

    ResourceMap<Sampler> mSamplerMap;
    HandleAllocator mSamplerHandleAllocator;

    ResourceMap<FenceSync> mFenceSyncMap;
    HandleAllocator mFenceSyncHandleAllocator;

    ResourceMap<Path> mPathMap;
    HandleRangeAllocator mPathHandleAllocator;
};

}  // namespace gl

#endif // LIBANGLE_RESOURCEMANAGER_H_
