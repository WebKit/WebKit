//
// Copyright (c) 2002-2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ResourceManager.cpp: Implements the the ResourceManager classes, which handle allocation and
// lifetime of GL objects.

#include "libANGLE/ResourceManager.h"

#include "libANGLE/Buffer.h"
#include "libANGLE/Fence.h"
#include "libANGLE/Path.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramPipeline.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Texture.h"
#include "libANGLE/renderer/GLImplFactory.h"

namespace gl
{

namespace
{

template <typename ResourceType>
GLuint AllocateEmptyObject(HandleAllocator *handleAllocator, ResourceMap<ResourceType> *objectMap)
{
    GLuint handle = handleAllocator->allocate();
    objectMap->assign(handle, nullptr);
    return handle;
}

}  // anonymous namespace

template <typename HandleAllocatorType>
ResourceManagerBase<HandleAllocatorType>::ResourceManagerBase() : mRefCount(1)
{
}

template <typename HandleAllocatorType>
void ResourceManagerBase<HandleAllocatorType>::addRef()
{
    mRefCount++;
}

template <typename HandleAllocatorType>
void ResourceManagerBase<HandleAllocatorType>::release(const Context *context)
{
    if (--mRefCount == 0)
    {
        reset(context);
        delete this;
    }
}

template <typename ResourceType, typename HandleAllocatorType, typename ImplT>
TypedResourceManager<ResourceType, HandleAllocatorType, ImplT>::~TypedResourceManager()
{
    ASSERT(mObjectMap.empty());
}

template <typename ResourceType, typename HandleAllocatorType, typename ImplT>
void TypedResourceManager<ResourceType, HandleAllocatorType, ImplT>::reset(const Context *context)
{
    this->mHandleAllocator.reset();
    for (const auto &resource : mObjectMap)
    {
        if (resource.second)
        {
            ImplT::DeleteObject(context, resource.second);
        }
    }
    mObjectMap.clear();
}

template <typename ResourceType, typename HandleAllocatorType, typename ImplT>
void TypedResourceManager<ResourceType, HandleAllocatorType, ImplT>::deleteObject(
    const Context *context,
    GLuint handle)
{
    ResourceType *resource = nullptr;
    if (!mObjectMap.erase(handle, &resource))
    {
        return;
    }

    // Requires an explicit this-> because of C++ template rules.
    this->mHandleAllocator.release(handle);

    if (resource)
    {
        ImplT::DeleteObject(context, resource);
    }
}

template class ResourceManagerBase<HandleAllocator>;
template class ResourceManagerBase<HandleRangeAllocator>;
template class TypedResourceManager<Buffer, HandleAllocator, BufferManager>;
template class TypedResourceManager<Texture, HandleAllocator, TextureManager>;
template class TypedResourceManager<Renderbuffer, HandleAllocator, RenderbufferManager>;
template class TypedResourceManager<Sampler, HandleAllocator, SamplerManager>;
template class TypedResourceManager<Sync, HandleAllocator, SyncManager>;
template class TypedResourceManager<Framebuffer, HandleAllocator, FramebufferManager>;
template class TypedResourceManager<ProgramPipeline, HandleAllocator, ProgramPipelineManager>;

// BufferManager Implementation.

// static
Buffer *BufferManager::AllocateNewObject(rx::GLImplFactory *factory, GLuint handle)
{
    Buffer *buffer = new Buffer(factory, handle);
    buffer->addRef();
    return buffer;
}

// static
void BufferManager::DeleteObject(const Context *context, Buffer *buffer)
{
    buffer->release(context);
}

GLuint BufferManager::createBuffer()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

Buffer *BufferManager::getBuffer(GLuint handle) const
{
    return mObjectMap.query(handle);
}

// ShaderProgramManager Implementation.

ShaderProgramManager::ShaderProgramManager()
{
}

ShaderProgramManager::~ShaderProgramManager()
{
    ASSERT(mPrograms.empty());
    ASSERT(mShaders.empty());
}

void ShaderProgramManager::reset(const Context *context)
{
    while (!mPrograms.empty())
    {
        deleteProgram(context, mPrograms.begin()->first);
    }
    mPrograms.clear();
    while (!mShaders.empty())
    {
        deleteShader(context, mShaders.begin()->first);
    }
    mShaders.clear();
}

GLuint ShaderProgramManager::createShader(rx::GLImplFactory *factory,
                                          const gl::Limitations &rendererLimitations,
                                          GLenum type)
{
    ASSERT(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER || type == GL_COMPUTE_SHADER ||
           type == GL_GEOMETRY_SHADER_EXT);
    GLuint handle    = mHandleAllocator.allocate();
    mShaders.assign(handle, new Shader(this, factory, rendererLimitations, type, handle));
    return handle;
}

void ShaderProgramManager::deleteShader(const Context *context, GLuint shader)
{
    deleteObject(context, &mShaders, shader);
}

Shader *ShaderProgramManager::getShader(GLuint handle) const
{
    return mShaders.query(handle);
}

GLuint ShaderProgramManager::createProgram(rx::GLImplFactory *factory)
{
    GLuint handle = mHandleAllocator.allocate();
    mPrograms.assign(handle, new Program(factory, this, handle));
    return handle;
}

void ShaderProgramManager::deleteProgram(const gl::Context *context, GLuint program)
{
    deleteObject(context, &mPrograms, program);
}

Program *ShaderProgramManager::getProgram(GLuint handle) const
{
    return mPrograms.query(handle);
}

template <typename ObjectType>
void ShaderProgramManager::deleteObject(const Context *context,
                                        ResourceMap<ObjectType> *objectMap,
                                        GLuint id)
{
    ObjectType *object = objectMap->query(id);
    if (!object)
    {
        return;
    }

    if (object->getRefCount() == 0)
    {
        mHandleAllocator.release(id);
        object->onDestroy(context);
        objectMap->erase(id, &object);
    }
    else
    {
        object->flagForDeletion();
    }
}

// TextureManager Implementation.

// static
Texture *TextureManager::AllocateNewObject(rx::GLImplFactory *factory, GLuint handle, GLenum target)
{
    Texture *texture = new Texture(factory, handle, target);
    texture->addRef();
    return texture;
}

// static
void TextureManager::DeleteObject(const Context *context, Texture *texture)
{
    texture->release(context);
}

GLuint TextureManager::createTexture()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

Texture *TextureManager::getTexture(GLuint handle) const
{
    ASSERT(mObjectMap.query(0) == nullptr);
    return mObjectMap.query(handle);
}

void TextureManager::signalAllTexturesDirty() const
{
    for (const auto &texture : mObjectMap)
    {
        if (texture.second)
        {
            // We don't know if the Texture needs init, but that's ok, since it will only force
            // a re-check, and will not initialize the pixels if it's not needed.
            texture.second->signalDirty(InitState::MayNeedInit);
        }
    }
}

// RenderbufferManager Implementation.

// static
Renderbuffer *RenderbufferManager::AllocateNewObject(rx::GLImplFactory *factory, GLuint handle)
{
    Renderbuffer *renderbuffer = new Renderbuffer(factory->createRenderbuffer(), handle);
    renderbuffer->addRef();
    return renderbuffer;
}

// static
void RenderbufferManager::DeleteObject(const Context *context, Renderbuffer *renderbuffer)
{
    renderbuffer->release(context);
}

GLuint RenderbufferManager::createRenderbuffer()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

Renderbuffer *RenderbufferManager::getRenderbuffer(GLuint handle) const
{
    return mObjectMap.query(handle);
}

// SamplerManager Implementation.

// static
Sampler *SamplerManager::AllocateNewObject(rx::GLImplFactory *factory, GLuint handle)
{
    Sampler *sampler = new Sampler(factory, handle);
    sampler->addRef();
    return sampler;
}

// static
void SamplerManager::DeleteObject(const Context *context, Sampler *sampler)
{
    sampler->release(context);
}

GLuint SamplerManager::createSampler()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

Sampler *SamplerManager::getSampler(GLuint handle) const
{
    return mObjectMap.query(handle);
}

bool SamplerManager::isSampler(GLuint sampler) const
{
    return mObjectMap.contains(sampler);
}

// SyncManager Implementation.

// static
void SyncManager::DeleteObject(const Context *context, Sync *sync)
{
    sync->release(context);
}

GLuint SyncManager::createSync(rx::GLImplFactory *factory)
{
    GLuint handle = mHandleAllocator.allocate();
    Sync *sync    = new Sync(factory->createSync(), handle);
    sync->addRef();
    mObjectMap.assign(handle, sync);
    return handle;
}

Sync *SyncManager::getSync(GLuint handle) const
{
    return mObjectMap.query(handle);
}

// PathManager Implementation.

PathManager::PathManager()
{
}

ErrorOrResult<GLuint> PathManager::createPaths(rx::GLImplFactory *factory, GLsizei range)
{
    // Allocate client side handles.
    const GLuint client = mHandleAllocator.allocateRange(static_cast<GLuint>(range));
    if (client == HandleRangeAllocator::kInvalidHandle)
        return OutOfMemory() << "Failed to allocate path handle range.";

    const auto &paths = factory->createPaths(range);
    if (paths.empty())
    {
        mHandleAllocator.releaseRange(client, range);
        return OutOfMemory() << "Failed to allocate path objects.";
    }

    for (GLsizei i = 0; i < range; ++i)
    {
        rx::PathImpl *impl = paths[static_cast<unsigned>(i)];
        const auto id   = client + i;
        mPaths.assign(id, new Path(impl));
    }
    return client;
}

void PathManager::deletePaths(GLuint first, GLsizei range)
{
    for (GLsizei i = 0; i < range; ++i)
    {
        const auto id = first + i;
        Path *p       = nullptr;
        if (!mPaths.erase(id, &p))
            continue;
        delete p;
    }
    mHandleAllocator.releaseRange(first, static_cast<GLuint>(range));
}

Path *PathManager::getPath(GLuint handle) const
{
    return mPaths.query(handle);
}

bool PathManager::hasPath(GLuint handle) const
{
    return mHandleAllocator.isUsed(handle);
}

PathManager::~PathManager()
{
    ASSERT(mPaths.empty());
}

void PathManager::reset(const Context *context)
{
    for (auto path : mPaths)
    {
        SafeDelete(path.second);
    }
    mPaths.clear();
}

// FramebufferManager Implementation.

// static
Framebuffer *FramebufferManager::AllocateNewObject(rx::GLImplFactory *factory,
                                                   GLuint handle,
                                                   const Caps &caps)
{
    return new Framebuffer(caps, factory, handle);
}

// static
void FramebufferManager::DeleteObject(const Context *context, Framebuffer *framebuffer)
{
    // Default framebuffer are owned by their respective Surface
    if (framebuffer->id() != 0)
    {
        framebuffer->onDestroy(context);
        delete framebuffer;
    }
}

GLuint FramebufferManager::createFramebuffer()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

Framebuffer *FramebufferManager::getFramebuffer(GLuint handle) const
{
    return mObjectMap.query(handle);
}

void FramebufferManager::setDefaultFramebuffer(Framebuffer *framebuffer)
{
    ASSERT(framebuffer == nullptr || framebuffer->id() == 0);
    mObjectMap.assign(0, framebuffer);
}

void FramebufferManager::invalidateFramebufferComplenessCache() const
{
    for (const auto &framebuffer : mObjectMap)
    {
        if (framebuffer.second)
        {
            framebuffer.second->invalidateCompletenessCache();
        }
    }
}

// ProgramPipelineManager Implementation.

// static
ProgramPipeline *ProgramPipelineManager::AllocateNewObject(rx::GLImplFactory *factory,
                                                           GLuint handle)
{
    ProgramPipeline *pipeline = new ProgramPipeline(factory, handle);
    pipeline->addRef();
    return pipeline;
}

// static
void ProgramPipelineManager::DeleteObject(const Context *context, ProgramPipeline *pipeline)
{
    pipeline->release(context);
}

GLuint ProgramPipelineManager::createProgramPipeline()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

ProgramPipeline *ProgramPipelineManager::getProgramPipeline(GLuint handle) const
{
    return mObjectMap.query(handle);
}

}  // namespace gl
