//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ResourceManager.cpp: Implements the the ResourceManager classes, which handle allocation and
// lifetime of GL objects.

#include "libANGLE/ResourceManager.h"

#include "libANGLE/Buffer.h"
#include "libANGLE/Context.h"
#include "libANGLE/Fence.h"
#include "libANGLE/MemoryObject.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramPipeline.h"
#include "libANGLE/Query.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/Semaphore.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Texture.h"
#include "libANGLE/renderer/ContextImpl.h"

namespace gl
{

namespace
{

template <typename ResourceType, typename IDType>
bool AllocateEmptyObject(HandleAllocator *handleAllocator,
                         ResourceMap<ResourceType, IDType> *objectMap,
                         IDType *outID)
{
    if (!handleAllocator->allocate(&outID->value))
    {
        return false;
    }
    objectMap->assign(*outID, nullptr);
    return true;
}

}  // anonymous namespace

ResourceManagerBase::ResourceManagerBase()
    : mHandleAllocator(IMPLEMENTATION_MAX_OBJECT_HANDLES), mRefCount(1)
{}

ResourceManagerBase::~ResourceManagerBase() = default;

void ResourceManagerBase::addRef()
{
    mRefCount++;
}

void ResourceManagerBase::release(const Context *context)
{
    if (--mRefCount == 0)
    {
        reset(context);
        delete this;
    }
}

template <typename ResourceType, typename ImplT, typename IDType>
TypedResourceManager<ResourceType, ImplT, IDType>::~TypedResourceManager()
{
    using UnsafeResourceMapIterTyped = UnsafeResourceMapIter<ResourceType, IDType>;
    ASSERT(UnsafeResourceMapIterTyped(mObjectMap).empty());
}

template <typename ResourceType, typename ImplT, typename IDType>
void TypedResourceManager<ResourceType, ImplT, IDType>::reset(const Context *context)
{
    // Note: this function is called when the last context in the share group is destroyed.  Thus
    // there are no thread safety concerns.
    this->mHandleAllocator.reset();
    for (const auto &resource : UnsafeResourceMapIter(mObjectMap))
    {
        if (resource.second)
        {
            ImplT::DeleteObject(context, resource.second);
        }
    }
    mObjectMap.clear();
}

template <typename ResourceType, typename ImplT, typename IDType>
void TypedResourceManager<ResourceType, ImplT, IDType>::deleteObject(const Context *context,
                                                                     IDType handle)
{
    ResourceType *resource = nullptr;
    if (!mObjectMap.erase(handle, &resource))
    {
        return;
    }

    // Requires an explicit this-> because of C++ template rules.
    this->mHandleAllocator.release(GetIDValue(handle));

    if (resource)
    {
        ImplT::DeleteObject(context, resource);
    }
}

template class TypedResourceManager<Buffer, BufferManager, BufferID>;
template class TypedResourceManager<Texture, TextureManager, TextureID>;
template class TypedResourceManager<Renderbuffer, RenderbufferManager, RenderbufferID>;
template class TypedResourceManager<Sampler, SamplerManager, SamplerID>;
template class TypedResourceManager<Sync, SyncManager, SyncID>;
template class TypedResourceManager<Framebuffer, FramebufferManager, FramebufferID>;
template class TypedResourceManager<ProgramPipeline, ProgramPipelineManager, ProgramPipelineID>;

// BufferManager Implementation.
BufferManager::~BufferManager() = default;

// static
Buffer *BufferManager::AllocateNewObject(rx::GLImplFactory *factory, BufferID handle)
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

bool BufferManager::createBuffer(BufferID *outBuffer)
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap, outBuffer);
}

Buffer *BufferManager::getBuffer(BufferID handle) const
{
    return mObjectMap.query(handle);
}

// ShaderProgramManager Implementation.

ShaderProgramManager::ShaderProgramManager() {}

ShaderProgramManager::~ShaderProgramManager()
{
    ASSERT(UnsafeResourceMapIter(mPrograms).empty());
    ASSERT(UnsafeResourceMapIter(mShaders).empty());
}

void ShaderProgramManager::reset(const Context *context)
{
    // Note: this function is called when the last context in the share group is destroyed.  Thus
    // there are no thread safety concerns.
    mHandleAllocator.reset();
    for (const auto &program : UnsafeResourceMapIter(mPrograms))
    {
        if (program.second)
        {
            program.second->onDestroy(context);
        }
    }
    for (const auto &shader : UnsafeResourceMapIter(mShaders))
    {
        if (shader.second)
        {
            shader.second->onDestroy(context);
        }
    }
    mPrograms.clear();
    mShaders.clear();
}

bool ShaderProgramManager::createShader(rx::GLImplFactory *factory,
                                        const gl::Limitations &rendererLimitations,
                                        ShaderType type,
                                        ShaderProgramID *outShader)
{
    ASSERT(type != ShaderType::InvalidEnum);
    if (!mHandleAllocator.allocate(&outShader->value))
    {
        return false;
    }
    mShaders.assign(*outShader, new Shader(this, factory, rendererLimitations, type, *outShader));
    return true;
}

void ShaderProgramManager::deleteShader(const Context *context, ShaderProgramID shader)
{
    deleteObject(context, &mShaders, shader);
}

Shader *ShaderProgramManager::getShader(ShaderProgramID handle) const
{
    return mShaders.query(handle);
}

bool ShaderProgramManager::createProgram(rx::GLImplFactory *factory, ShaderProgramID *outProgram)
{
    if (!mHandleAllocator.allocate(&outProgram->value))
    {
        return false;
    }
    mPrograms.assign(*outProgram, new Program(factory, this, *outProgram));
    return true;
}

void ShaderProgramManager::deleteProgram(const gl::Context *context, ShaderProgramID program)
{
    deleteObject(context, &mPrograms, program);
}

template <typename ObjectType, typename IDType>
void ShaderProgramManager::deleteObject(const Context *context,
                                        ResourceMap<ObjectType, IDType> *objectMap,
                                        IDType id)
{
    ObjectType *object = objectMap->query(id);
    if (!object)
    {
        return;
    }

    if (object->getRefCount() == 0)
    {
        mHandleAllocator.release(id.value);
        object->onDestroy(context);
        objectMap->erase(id, &object);
    }
    else
    {
        object->flagForDeletion();
    }
}

// TextureManager Implementation.

TextureManager::~TextureManager() = default;

// static
Texture *TextureManager::AllocateNewObject(rx::GLImplFactory *factory,
                                           TextureID handle,
                                           TextureType type)
{
    Texture *texture = new Texture(factory, handle, type);
    texture->addRef();
    return texture;
}

// static
void TextureManager::DeleteObject(const Context *context, Texture *texture)
{
    texture->release(context);
}

bool TextureManager::createTexture(TextureID *outTexture)
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap, outTexture);
}

void TextureManager::signalAllTexturesDirty() const
{
    // Note: this function is called with glRequestExtensionANGLE and glDisableExtensionANGLE.  The
    // GL_ANGLE_request_extension explicitly requires the application to ensure thread safety.
    for (const auto &texture : UnsafeResourceMapIter(mObjectMap))
    {
        if (texture.second)
        {
            // We don't know if the Texture needs init, but that's ok, since it will only force
            // a re-check, and will not initialize the pixels if it's not needed.
            texture.second->signalDirtyStorage(InitState::MayNeedInit);
        }
    }
}

void TextureManager::enableHandleAllocatorLogging()
{
    mHandleAllocator.enableLogging(true);
}

size_t TextureManager::getTotalMemorySize() const
{
    size_t totalBytes = 0;

    for (const auto &texture : UnsafeResourceMapIter(mObjectMap))
    {
        if (texture.second->getBoundSurface() || texture.second->isEGLImageTarget())
        {
            // Skip external texture
            continue;
        }
        totalBytes += static_cast<size_t>(texture.second->getMemorySize());
    }
    return totalBytes;
}

// RenderbufferManager Implementation.

RenderbufferManager::~RenderbufferManager() = default;

// static
Renderbuffer *RenderbufferManager::AllocateNewObject(rx::GLImplFactory *factory,
                                                     RenderbufferID handle)
{
    Renderbuffer *renderbuffer = new Renderbuffer(factory, handle);
    renderbuffer->addRef();
    return renderbuffer;
}

// static
void RenderbufferManager::DeleteObject(const Context *context, Renderbuffer *renderbuffer)
{
    renderbuffer->release(context);
}

bool RenderbufferManager::createRenderbuffer(RenderbufferID *outRenderbuffer)
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap, outRenderbuffer);
}

Renderbuffer *RenderbufferManager::getRenderbuffer(RenderbufferID handle) const
{
    return mObjectMap.query(handle);
}

// SamplerManager Implementation.

SamplerManager::~SamplerManager() = default;

// static
Sampler *SamplerManager::AllocateNewObject(rx::GLImplFactory *factory, SamplerID handle)
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

bool SamplerManager::createSampler(SamplerID *outSampler)
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap, outSampler);
}

// SyncManager Implementation.

SyncManager::~SyncManager() = default;

// static
void SyncManager::DeleteObject(const Context *context, Sync *sync)
{
    sync->release(context);
}

bool SyncManager::createSync(rx::GLImplFactory *factory, SyncID *outSync)
{
    if (!mHandleAllocator.allocate(&outSync->value))
    {
        return false;
    }
    Sync *sync = new Sync(factory, *outSync);
    sync->addRef();
    mObjectMap.assign(*outSync, sync);
    return true;
}

Sync *SyncManager::getSync(SyncID handle) const
{
    return mObjectMap.query(handle);
}

// FramebufferManager Implementation.

FramebufferManager::~FramebufferManager() = default;

// static
Framebuffer *FramebufferManager::AllocateNewObject(rx::GLImplFactory *factory,
                                                   FramebufferID handle,
                                                   const Context *context)
{
    // Make sure the caller isn't using a reserved handle.
    ASSERT(handle != Framebuffer::kDefaultDrawFramebufferHandle);
    return new Framebuffer(context, factory, handle);
}

// static
void FramebufferManager::DeleteObject(const Context *context, Framebuffer *framebuffer)
{
    framebuffer->onDestroy(context);
    delete framebuffer;
}

bool FramebufferManager::createFramebuffer(FramebufferID *outFramebuffer)
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap, outFramebuffer);
}

Framebuffer *FramebufferManager::getFramebuffer(FramebufferID handle) const
{
    return mObjectMap.query(handle);
}

void FramebufferManager::setDefaultFramebuffer(Framebuffer *framebuffer)
{
    ASSERT(framebuffer == nullptr || framebuffer->isDefault());
    mObjectMap.assign(Framebuffer::kDefaultDrawFramebufferHandle, framebuffer);
}

Framebuffer *FramebufferManager::getDefaultFramebuffer() const
{
    return getFramebuffer(Framebuffer::kDefaultDrawFramebufferHandle);
}

void FramebufferManager::invalidateFramebufferCompletenessCache() const
{
    // Note: framebuffer objects are private to context and so the map doesn't need locking
    for (const auto &framebuffer : UnsafeResourceMapIter(mObjectMap))
    {
        if (framebuffer.second)
        {
            framebuffer.second->invalidateCompletenessCache();
        }
    }
}

// ProgramPipelineManager Implementation.

ProgramPipelineManager::~ProgramPipelineManager() = default;

// static
ProgramPipeline *ProgramPipelineManager::AllocateNewObject(rx::GLImplFactory *factory,
                                                           ProgramPipelineID handle)
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

bool ProgramPipelineManager::createProgramPipeline(ProgramPipelineID *outProgramPipeline)
{
    return AllocateEmptyObject(&mHandleAllocator, &mObjectMap, outProgramPipeline);
}

ProgramPipeline *ProgramPipelineManager::getProgramPipeline(ProgramPipelineID handle) const
{
    return mObjectMap.query(handle);
}

// MemoryObjectManager Implementation.

MemoryObjectManager::MemoryObjectManager() {}

MemoryObjectManager::~MemoryObjectManager()
{
    ASSERT(UnsafeResourceMapIter(mMemoryObjects).empty());
}

void MemoryObjectManager::reset(const Context *context)
{
    // Note: this function is called when the last context in the share group is destroyed.  Thus
    // there are no thread safety concerns.
    mHandleAllocator.reset();
    for (const auto &memoryObject : UnsafeResourceMapIter(mMemoryObjects))
    {
        if (memoryObject.second)
        {
            memoryObject.second->release(context);
        }
    }
    mMemoryObjects.clear();
}

bool MemoryObjectManager::createMemoryObject(rx::GLImplFactory *factory,
                                             MemoryObjectID *outMemoryObject)
{
    if (!mHandleAllocator.allocate(&outMemoryObject->value))
    {
        return false;
    }
    MemoryObject *memoryObject = new MemoryObject(factory, *outMemoryObject);
    memoryObject->addRef();
    mMemoryObjects.assign(*outMemoryObject, memoryObject);
    return true;
}

void MemoryObjectManager::deleteMemoryObject(const Context *context, MemoryObjectID handle)
{
    MemoryObject *memoryObject = nullptr;
    if (!mMemoryObjects.erase(handle, &memoryObject))
    {
        return;
    }

    // Requires an explicit this-> because of C++ template rules.
    this->mHandleAllocator.release(handle.value);

    if (memoryObject)
    {
        memoryObject->release(context);
    }
}

MemoryObject *MemoryObjectManager::getMemoryObject(MemoryObjectID handle) const
{
    return mMemoryObjects.query(handle);
}

// SemaphoreManager Implementation.

SemaphoreManager::SemaphoreManager() {}

SemaphoreManager::~SemaphoreManager()
{
    ASSERT(UnsafeResourceMapIter(mSemaphores).empty());
}

void SemaphoreManager::reset(const Context *context)
{
    // Note: this function is called when the last context in the share group is destroyed.  Thus
    // there are no thread safety concerns.
    mHandleAllocator.reset();
    for (const auto &semaphore : UnsafeResourceMapIter(mSemaphores))
    {
        if (semaphore.second)
        {
            semaphore.second->release(context);
        }
    }
    mSemaphores.clear();
}

bool SemaphoreManager::createSemaphore(rx::GLImplFactory *factory, SemaphoreID *outSemaphoreHandle)
{
    if (!mHandleAllocator.allocate(&outSemaphoreHandle->value))
    {
        return false;
    }
    Semaphore *semaphore = new Semaphore(factory, *outSemaphoreHandle);
    semaphore->addRef();
    mSemaphores.assign(*outSemaphoreHandle, semaphore);
    return true;
}

void SemaphoreManager::deleteSemaphore(const Context *context, SemaphoreID handle)
{
    Semaphore *semaphore = nullptr;
    if (!mSemaphores.erase(handle, &semaphore))
    {
        return;
    }

    // Requires an explicit this-> because of C++ template rules.
    this->mHandleAllocator.release(handle.value);

    if (semaphore)
    {
        semaphore->release(context);
    }
}

Semaphore *SemaphoreManager::getSemaphore(SemaphoreID handle) const
{
    return mSemaphores.query(handle);
}
}  // namespace gl
