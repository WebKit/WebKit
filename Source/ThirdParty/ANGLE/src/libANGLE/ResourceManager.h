//
// Copyright (c) 2002-2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ResourceManager.h : Defines the ResourceManager classes, which handle allocation and lifetime of
// GL objects.

#ifndef LIBANGLE_RESOURCEMANAGER_H_
#define LIBANGLE_RESOURCEMANAGER_H_

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/HandleAllocator.h"
#include "libANGLE/HandleRangeAllocator.h"
#include "libANGLE/ResourceMap.h"

namespace rx
{
class GLImplFactory;
}

namespace gl
{
class Buffer;
struct Caps;
class Context;
class Sync;
class Framebuffer;
struct Limitations;
class Path;
class Program;
class ProgramPipeline;
class Renderbuffer;
class Sampler;
class Shader;
class Texture;

template <typename HandleAllocatorType>
class ResourceManagerBase : angle::NonCopyable
{
  public:
    ResourceManagerBase();

    void addRef();
    void release(const Context *context);

  protected:
    virtual void reset(const Context *context) = 0;
    virtual ~ResourceManagerBase() {}

    HandleAllocatorType mHandleAllocator;

  private:
    size_t mRefCount;
};

template <typename ResourceType, typename HandleAllocatorType, typename ImplT>
class TypedResourceManager : public ResourceManagerBase<HandleAllocatorType>
{
  public:
    TypedResourceManager() {}

    void deleteObject(const Context *context, GLuint handle);
    bool isHandleGenerated(GLuint handle) const
    {
        // Zero is always assumed to have been generated implicitly.
        return handle == 0 || mObjectMap.contains(handle);
    }

  protected:
    ~TypedResourceManager() override;

    // Inlined in the header for performance.
    template <typename... ArgTypes>
    ResourceType *checkObjectAllocation(rx::GLImplFactory *factory, GLuint handle, ArgTypes... args)
    {
        ResourceType *value = mObjectMap.query(handle);
        if (value)
        {
            return value;
        }

        if (handle == 0)
        {
            return nullptr;
        }

        ResourceType *object = ImplT::AllocateNewObject(factory, handle, args...);

        if (!mObjectMap.contains(handle))
        {
            this->mHandleAllocator.reserve(handle);
        }
        mObjectMap.assign(handle, object);

        return object;
    }

    void reset(const Context *context) override;

    ResourceMap<ResourceType> mObjectMap;
};

class BufferManager : public TypedResourceManager<Buffer, HandleAllocator, BufferManager>
{
  public:
    GLuint createBuffer();
    Buffer *getBuffer(GLuint handle) const;

    Buffer *checkBufferAllocation(rx::GLImplFactory *factory, GLuint handle)
    {
        return checkObjectAllocation(factory, handle);
    }

    // TODO(jmadill): Investigate design which doesn't expose these methods publicly.
    static Buffer *AllocateNewObject(rx::GLImplFactory *factory, GLuint handle);
    static void DeleteObject(const Context *context, Buffer *buffer);

  protected:
    ~BufferManager() override {}
};

class ShaderProgramManager : public ResourceManagerBase<HandleAllocator>
{
  public:
    ShaderProgramManager();

    GLuint createShader(rx::GLImplFactory *factory,
                        const Limitations &rendererLimitations,
                        GLenum type);
    void deleteShader(const Context *context, GLuint shader);
    Shader *getShader(GLuint handle) const;

    GLuint createProgram(rx::GLImplFactory *factory);
    void deleteProgram(const Context *context, GLuint program);
    Program *getProgram(GLuint handle) const;

  protected:
    ~ShaderProgramManager() override;

  private:
    template <typename ObjectType>
    void deleteObject(const Context *context, ResourceMap<ObjectType> *objectMap, GLuint id);

    void reset(const Context *context) override;

    ResourceMap<Shader> mShaders;
    ResourceMap<Program> mPrograms;
};

class TextureManager : public TypedResourceManager<Texture, HandleAllocator, TextureManager>
{
  public:
    GLuint createTexture();
    Texture *getTexture(GLuint handle) const;

    void signalAllTexturesDirty() const;

    Texture *checkTextureAllocation(rx::GLImplFactory *factory, GLuint handle, GLenum target)
    {
        return checkObjectAllocation(factory, handle, target);
    }

    static Texture *AllocateNewObject(rx::GLImplFactory *factory, GLuint handle, GLenum target);
    static void DeleteObject(const Context *context, Texture *texture);

  protected:
    ~TextureManager() override {}
};

class RenderbufferManager
    : public TypedResourceManager<Renderbuffer, HandleAllocator, RenderbufferManager>
{
  public:
    GLuint createRenderbuffer();
    Renderbuffer *getRenderbuffer(GLuint handle) const;

    Renderbuffer *checkRenderbufferAllocation(rx::GLImplFactory *factory, GLuint handle)
    {
        return checkObjectAllocation(factory, handle);
    }

    static Renderbuffer *AllocateNewObject(rx::GLImplFactory *factory, GLuint handle);
    static void DeleteObject(const Context *context, Renderbuffer *renderbuffer);

  protected:
    ~RenderbufferManager() override {}
};

class SamplerManager : public TypedResourceManager<Sampler, HandleAllocator, SamplerManager>
{
  public:
    GLuint createSampler();
    Sampler *getSampler(GLuint handle) const;
    bool isSampler(GLuint sampler) const;

    Sampler *checkSamplerAllocation(rx::GLImplFactory *factory, GLuint handle)
    {
        return checkObjectAllocation(factory, handle);
    }

    static Sampler *AllocateNewObject(rx::GLImplFactory *factory, GLuint handle);
    static void DeleteObject(const Context *context, Sampler *sampler);

  protected:
    ~SamplerManager() override {}
};

class SyncManager : public TypedResourceManager<Sync, HandleAllocator, SyncManager>
{
  public:
    GLuint createSync(rx::GLImplFactory *factory);
    Sync *getSync(GLuint handle) const;

    static void DeleteObject(const Context *context, Sync *sync);

  protected:
    ~SyncManager() override {}
};

class PathManager : public ResourceManagerBase<HandleRangeAllocator>
{
  public:
    PathManager();

    ErrorOrResult<GLuint> createPaths(rx::GLImplFactory *factory, GLsizei range);
    void deletePaths(GLuint first, GLsizei range);
    Path *getPath(GLuint handle) const;
    bool hasPath(GLuint handle) const;

  protected:
    ~PathManager() override;
    void reset(const Context *context) override;

  private:
    ResourceMap<Path> mPaths;
};

class FramebufferManager
    : public TypedResourceManager<Framebuffer, HandleAllocator, FramebufferManager>
{
  public:
    GLuint createFramebuffer();
    Framebuffer *getFramebuffer(GLuint handle) const;
    void setDefaultFramebuffer(Framebuffer *framebuffer);

    void invalidateFramebufferComplenessCache() const;

    Framebuffer *checkFramebufferAllocation(rx::GLImplFactory *factory,
                                            const Caps &caps,
                                            GLuint handle)
    {
        return checkObjectAllocation<const Caps &>(factory, handle, caps);
    }

    static Framebuffer *AllocateNewObject(rx::GLImplFactory *factory,
                                          GLuint handle,
                                          const Caps &caps);
    static void DeleteObject(const Context *context, Framebuffer *framebuffer);

  protected:
    ~FramebufferManager() override {}
};

class ProgramPipelineManager
    : public TypedResourceManager<ProgramPipeline, HandleAllocator, ProgramPipelineManager>
{
  public:
    GLuint createProgramPipeline();
    ProgramPipeline *getProgramPipeline(GLuint handle) const;

    ProgramPipeline *checkProgramPipelineAllocation(rx::GLImplFactory *factory, GLuint handle)
    {
        return checkObjectAllocation(factory, handle);
    }

    static ProgramPipeline *AllocateNewObject(rx::GLImplFactory *factory, GLuint handle);
    static void DeleteObject(const Context *context, ProgramPipeline *pipeline);

  protected:
    ~ProgramPipelineManager() override {}
};

}  // namespace gl

#endif // LIBANGLE_RESOURCEMANAGER_H_
