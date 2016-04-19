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
#include "libANGLE/HandleAllocator.h"

namespace rx
{
class ImplFactory;
}

namespace gl
{
class Buffer;
struct Data;
class FenceSync;
struct Limitations;
class Program;
class Renderbuffer;
class Sampler;
class Shader;
class Texture;

class ResourceManager : angle::NonCopyable
{
  public:
    explicit ResourceManager(rx::ImplFactory *factory);
    ~ResourceManager();

    void addRef();
    void release();

    GLuint createBuffer();
    GLuint createShader(const gl::Limitations &rendererLimitations, GLenum type);
    GLuint createProgram();
    GLuint createTexture();
    GLuint createRenderbuffer();
    GLuint createSampler();
    GLuint createFenceSync();

    void deleteBuffer(GLuint buffer);
    void deleteShader(GLuint shader);
    void deleteProgram(GLuint program);
    void deleteTexture(GLuint texture);
    void deleteRenderbuffer(GLuint renderbuffer);
    void deleteSampler(GLuint sampler);
    void deleteFenceSync(GLuint fenceSync);

    Buffer *getBuffer(GLuint handle);
    Shader *getShader(GLuint handle);
    Program *getProgram(GLuint handle) const;
    Texture *getTexture(GLuint handle);
    Renderbuffer *getRenderbuffer(GLuint handle);
    Sampler *getSampler(GLuint handle);
    FenceSync *getFenceSync(GLuint handle);

    void setRenderbuffer(GLuint handle, Renderbuffer *renderbuffer);

    Buffer *checkBufferAllocation(GLuint handle);
    Texture *checkTextureAllocation(GLuint handle, GLenum type);
    Renderbuffer *checkRenderbufferAllocation(GLuint handle);
    Sampler *checkSamplerAllocation(GLuint samplerHandle);

    bool isSampler(GLuint sampler);

  private:
    void createTextureInternal(GLuint handle);

    rx::ImplFactory *mFactory;
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
};

}  // namespace gl

#endif // LIBANGLE_RESOURCEMANAGER_H_
