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
    GLuint handle        = handleAllocator->allocate();
    (*objectMap)[handle] = nullptr;
    return handle;
}

template <typename ResourceType>
ResourceType *GetObject(const ResourceMap<ResourceType> &objectMap, GLuint handle)
{
    auto iter = objectMap.find(handle);
    return iter != objectMap.end() ? iter->second : nullptr;
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
    while (!mObjectMap.empty())
    {
        deleteObject(context, mObjectMap.begin()->first);
    }
    mObjectMap.clear();
}

template <typename ResourceType, typename HandleAllocatorType, typename ImplT>
void TypedResourceManager<ResourceType, HandleAllocatorType, ImplT>::deleteObject(
    const Context *context,
    GLuint handle)
{
    auto objectIter = mObjectMap.find(handle);
    if (objectIter == mObjectMap.end())
    {
        return;
    }

    if (objectIter->second != nullptr)
    {
        objectIter->second->destroy(context);
        ImplT::DeleteObject(objectIter->second);
    }

    // Requires an explicit this-> because of C++ template rules.
    this->mHandleAllocator.release(objectIter->first);
    mObjectMap.erase(objectIter);
}

template <typename ResourceType, typename HandleAllocatorType, typename ImplT>
template <typename... ArgTypes>
ResourceType *TypedResourceManager<ResourceType, HandleAllocatorType, ImplT>::allocateObject(
    typename ResourceMap<ResourceType>::iterator &objectMapIter,
    rx::GLImplFactory *factory,
    GLuint handle,
    ArgTypes... args)
{
    ResourceType *object = ImplT::AllocateNewObject(factory, handle, args...);

    if (objectMapIter != mObjectMap.end())
    {
        objectMapIter->second = object;
    }
    else
    {
        this->mHandleAllocator.reserve(handle);
        mObjectMap[handle] = object;
    }

    return object;
}

template class ResourceManagerBase<HandleAllocator>;
template class ResourceManagerBase<HandleRangeAllocator>;
template class TypedResourceManager<Buffer, HandleAllocator, BufferManager>;
template Buffer *TypedResourceManager<Buffer, HandleAllocator, BufferManager>::allocateObject(
    ResourceMap<Buffer>::iterator &,
    rx::GLImplFactory *,
    GLuint);
template class TypedResourceManager<Texture, HandleAllocator, TextureManager>;
template Texture *TypedResourceManager<Texture, HandleAllocator, TextureManager>::allocateObject(
    ResourceMap<Texture>::iterator &,
    rx::GLImplFactory *,
    GLuint,
    GLenum);
template class TypedResourceManager<Renderbuffer, HandleAllocator, RenderbufferManager>;
template Renderbuffer *
TypedResourceManager<Renderbuffer, HandleAllocator, RenderbufferManager>::allocateObject(
    ResourceMap<Renderbuffer>::iterator &,
    rx::GLImplFactory *,
    GLuint);
template class TypedResourceManager<Sampler, HandleAllocator, SamplerManager>;
template Sampler *TypedResourceManager<Sampler, HandleAllocator, SamplerManager>::allocateObject(
    ResourceMap<Sampler>::iterator &,
    rx::GLImplFactory *,
    GLuint);
template class TypedResourceManager<FenceSync, HandleAllocator, FenceSyncManager>;
template class TypedResourceManager<Framebuffer, HandleAllocator, FramebufferManager>;
template Framebuffer *
TypedResourceManager<Framebuffer, HandleAllocator, FramebufferManager>::allocateObject(
    ResourceMap<Framebuffer>::iterator &,
    rx::GLImplFactory *,
    GLuint,
    const Caps &);

// BufferManager Implementation.

// static
Buffer *BufferManager::AllocateNewObject(rx::GLImplFactory *factory, GLuint handle)
{
    Buffer *buffer = new Buffer(factory, handle);
    buffer->addRef();
    return buffer;
}

// static
void BufferManager::DeleteObject(Buffer *buffer)
{
    buffer->release();
}

GLuint BufferManager::createBuffer()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

Buffer *BufferManager::getBuffer(GLuint handle) const
{
    return GetObject(mObjectMap, handle);
}

bool BufferManager::isBufferGenerated(GLuint buffer) const
{
    return buffer == 0 || mObjectMap.find(buffer) != mObjectMap.end();
}

// ShaderProgramManager Implementation.

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
    ASSERT(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER || type == GL_COMPUTE_SHADER);
    GLuint handle    = mHandleAllocator.allocate();
    mShaders[handle] = new Shader(this, factory, rendererLimitations, type, handle);
    return handle;
}

void ShaderProgramManager::deleteShader(const Context *context, GLuint shader)
{
    deleteObject(context, &mShaders, shader);
}

Shader *ShaderProgramManager::getShader(GLuint handle) const
{
    return GetObject(mShaders, handle);
}

GLuint ShaderProgramManager::createProgram(rx::GLImplFactory *factory)
{
    GLuint handle     = mHandleAllocator.allocate();
    mPrograms[handle] = new Program(factory, this, handle);
    return handle;
}

void ShaderProgramManager::deleteProgram(const gl::Context *context, GLuint program)
{
    deleteObject(context, &mPrograms, program);
}

Program *ShaderProgramManager::getProgram(GLuint handle) const
{
    return GetObject(mPrograms, handle);
}

template <typename ObjectType>
void ShaderProgramManager::deleteObject(const Context *context,
                                        ResourceMap<ObjectType> *objectMap,
                                        GLuint id)
{
    auto iter = objectMap->find(id);
    if (iter == objectMap->end())
    {
        return;
    }

    auto object = iter->second;
    if (object->getRefCount() == 0)
    {
        mHandleAllocator.release(id);
        object->destroy(context);
        SafeDelete(object);
        objectMap->erase(iter);
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
void TextureManager::DeleteObject(Texture *texture)
{
    texture->release();
}

GLuint TextureManager::createTexture()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

Texture *TextureManager::getTexture(GLuint handle) const
{
    ASSERT(GetObject(mObjectMap, 0) == nullptr);
    return GetObject(mObjectMap, handle);
}

bool TextureManager::isTextureGenerated(GLuint texture) const
{
    return texture == 0 || mObjectMap.find(texture) != mObjectMap.end();
}

void TextureManager::invalidateTextureComplenessCache()
{
    for (auto &texture : mObjectMap)
    {
        if (texture.second)
        {
            texture.second->invalidateCompletenessCache();
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
void RenderbufferManager::DeleteObject(Renderbuffer *renderbuffer)
{
    renderbuffer->release();
}

GLuint RenderbufferManager::createRenderbuffer()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

Renderbuffer *RenderbufferManager::getRenderbuffer(GLuint handle)
{
    return GetObject(mObjectMap, handle);
}

bool RenderbufferManager::isRenderbufferGenerated(GLuint renderbuffer) const
{
    return renderbuffer == 0 || mObjectMap.find(renderbuffer) != mObjectMap.end();
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
void SamplerManager::DeleteObject(Sampler *sampler)
{
    sampler->release();
}

GLuint SamplerManager::createSampler()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

Sampler *SamplerManager::getSampler(GLuint handle)
{
    return GetObject(mObjectMap, handle);
}

bool SamplerManager::isSampler(GLuint sampler)
{
    return mObjectMap.find(sampler) != mObjectMap.end();
}

// FenceSyncManager Implementation.

// static
void FenceSyncManager::DeleteObject(FenceSync *fenceSync)
{
    fenceSync->release();
}

GLuint FenceSyncManager::createFenceSync(rx::GLImplFactory *factory)
{
    GLuint handle        = mHandleAllocator.allocate();
    FenceSync *fenceSync = new FenceSync(factory->createFenceSync(), handle);
    fenceSync->addRef();
    mObjectMap[handle] = fenceSync;
    return handle;
}

FenceSync *FenceSyncManager::getFenceSync(GLuint handle)
{
    return GetObject(mObjectMap, handle);
}

// PathManager Implementation.

ErrorOrResult<GLuint> PathManager::createPaths(rx::GLImplFactory *factory, GLsizei range)
{
    // Allocate client side handles.
    const GLuint client = mHandleAllocator.allocateRange(static_cast<GLuint>(range));
    if (client == HandleRangeAllocator::kInvalidHandle)
        return Error(GL_OUT_OF_MEMORY, "Failed to allocate path handle range.");

    const auto &paths = factory->createPaths(range);
    if (paths.empty())
    {
        mHandleAllocator.releaseRange(client, range);
        return Error(GL_OUT_OF_MEMORY, "Failed to allocate path objects.");
    }

    auto hint = mPaths.begin();

    for (GLsizei i = 0; i < range; ++i)
    {
        const auto impl = paths[static_cast<unsigned>(i)];
        const auto id   = client + i;
        hint            = mPaths.insert(hint, std::make_pair(id, new Path(impl)));
    }
    return client;
}

void PathManager::deletePaths(GLuint first, GLsizei range)
{
    for (GLsizei i = 0; i < range; ++i)
    {
        const auto id = first + i;
        const auto it = mPaths.find(id);
        if (it == mPaths.end())
            continue;
        Path *p = it->second;
        delete p;
        mPaths.erase(it);
    }
    mHandleAllocator.releaseRange(first, static_cast<GLuint>(range));
}

Path *PathManager::getPath(GLuint handle) const
{
    auto iter = mPaths.find(handle);
    return iter != mPaths.end() ? iter->second : nullptr;
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
void FramebufferManager::DeleteObject(Framebuffer *framebuffer)
{
    // Default framebuffer are owned by their respective Surface
    if (framebuffer->id() != 0)
    {
        delete framebuffer;
    }
}

GLuint FramebufferManager::createFramebuffer()
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap);
}

Framebuffer *FramebufferManager::getFramebuffer(GLuint handle) const
{
    return GetObject(mObjectMap, handle);
}

void FramebufferManager::setDefaultFramebuffer(Framebuffer *framebuffer)
{
    ASSERT(framebuffer == nullptr || framebuffer->id() == 0);
    mObjectMap[0] = framebuffer;
}

bool FramebufferManager::isFramebufferGenerated(GLuint framebuffer)
{
    ASSERT(mObjectMap.find(0) != mObjectMap.end());
    return mObjectMap.find(framebuffer) != mObjectMap.end();
}

void FramebufferManager::invalidateFramebufferComplenessCache()
{
    for (auto &framebuffer : mObjectMap)
    {
        if (framebuffer.second)
        {
            framebuffer.second->invalidateCompletenessCache();
        }
    }
}

}  // namespace gl
