//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ResourceManager.cpp: Implements the gl::ResourceManager class, which tracks and
// retrieves objects which may be shared by multiple Contexts.

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
ResourceManager::ResourceManager() : mRefCount(1)
{
}

ResourceManager::~ResourceManager()
{
    while (!mBufferMap.empty())
    {
        deleteBuffer(mBufferMap.begin()->first);
    }

    while (!mProgramMap.empty())
    {
        deleteProgram(mProgramMap.begin()->first);
    }

    while (!mShaderMap.empty())
    {
        deleteShader(mShaderMap.begin()->first);
    }

    while (!mRenderbufferMap.empty())
    {
        deleteRenderbuffer(mRenderbufferMap.begin()->first);
    }

    while (!mTextureMap.empty())
    {
        deleteTexture(mTextureMap.begin()->first);
    }

    while (!mSamplerMap.empty())
    {
        deleteSampler(mSamplerMap.begin()->first);
    }

    while (!mFenceSyncMap.empty())
    {
        deleteFenceSync(mFenceSyncMap.begin()->first);
    }

    for (auto it = mPathMap.begin(); it != mPathMap.end(); ++it)
    {
        const auto *p = it->second;
        delete p;
    }
}

void ResourceManager::addRef()
{
    mRefCount++;
}

void ResourceManager::release()
{
    if (--mRefCount == 0)
    {
        delete this;
    }
}

// Returns an unused buffer name
GLuint ResourceManager::createBuffer()
{
    GLuint handle = mBufferHandleAllocator.allocate();

    mBufferMap[handle] = nullptr;

    return handle;
}

// Returns an unused shader/program name
GLuint ResourceManager::createShader(rx::GLImplFactory *factory,
                                     const gl::Limitations &rendererLimitations,
                                     GLenum type)
{
    ASSERT(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER || type == GL_COMPUTE_SHADER);
    GLuint handle = mProgramShaderHandleAllocator.allocate();

    mShaderMap[handle] = new Shader(this, factory, rendererLimitations, type, handle);

    return handle;
}

// Returns an unused program/shader name
GLuint ResourceManager::createProgram(rx::GLImplFactory *factory)
{
    GLuint handle = mProgramShaderHandleAllocator.allocate();

    mProgramMap[handle] = new Program(factory, this, handle);

    return handle;
}

// Returns an unused texture name
GLuint ResourceManager::createTexture()
{
    GLuint handle = mTextureHandleAllocator.allocate();

    mTextureMap[handle] = nullptr;

    return handle;
}

// Returns an unused renderbuffer name
GLuint ResourceManager::createRenderbuffer()
{
    GLuint handle = mRenderbufferHandleAllocator.allocate();

    mRenderbufferMap[handle] = nullptr;

    return handle;
}

// Returns an unused sampler name
GLuint ResourceManager::createSampler()
{
    GLuint handle = mSamplerHandleAllocator.allocate();

    mSamplerMap[handle] = nullptr;

    return handle;
}

// Returns the next unused fence name, and allocates the fence
GLuint ResourceManager::createFenceSync(rx::GLImplFactory *factory)
{
    GLuint handle = mFenceSyncHandleAllocator.allocate();

    FenceSync *fenceSync = new FenceSync(factory->createFenceSync(), handle);
    fenceSync->addRef();
    mFenceSyncMap[handle] = fenceSync;

    return handle;
}

ErrorOrResult<GLuint> ResourceManager::createPaths(rx::GLImplFactory *factory, GLsizei range)
{
    // Allocate client side handles.
    const GLuint client = mPathHandleAllocator.allocateRange(static_cast<GLuint>(range));
    if (client == HandleRangeAllocator::kInvalidHandle)
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to allocate path handle range.");

    const auto &paths = factory->createPaths(range);
    if (paths.empty())
    {
        mPathHandleAllocator.releaseRange(client, range);
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to allocate path objects.");
    }

    auto hint = mPathMap.begin();

    for (GLsizei i = 0; i < range; ++i)
    {
        const auto impl = paths[static_cast<unsigned>(i)];
        const auto id   = client + i;
        hint            = mPathMap.insert(hint, std::make_pair(id, new Path(impl)));
    }
    return client;
}

void ResourceManager::deleteBuffer(GLuint buffer)
{
    auto bufferObject = mBufferMap.find(buffer);

    if (bufferObject != mBufferMap.end())
    {
        mBufferHandleAllocator.release(bufferObject->first);
        if (bufferObject->second) bufferObject->second->release();
        mBufferMap.erase(bufferObject);
    }
}

void ResourceManager::deleteShader(GLuint shader)
{
    auto shaderObject = mShaderMap.find(shader);

    if (shaderObject != mShaderMap.end())
    {
        if (shaderObject->second->getRefCount() == 0)
        {
            mProgramShaderHandleAllocator.release(shaderObject->first);
            delete shaderObject->second;
            mShaderMap.erase(shaderObject);
        }
        else
        {
            shaderObject->second->flagForDeletion();
        }
    }
}

void ResourceManager::deleteProgram(GLuint program)
{
    auto programObject = mProgramMap.find(program);

    if (programObject != mProgramMap.end())
    {
        if (programObject->second->getRefCount() == 0)
        {
            mProgramShaderHandleAllocator.release(programObject->first);
            delete programObject->second;
            mProgramMap.erase(programObject);
        }
        else
        {
            programObject->second->flagForDeletion();
        }
    }
}

void ResourceManager::deleteTexture(GLuint texture)
{
    auto textureObject = mTextureMap.find(texture);

    if (textureObject != mTextureMap.end())
    {
        mTextureHandleAllocator.release(textureObject->first);
        if (textureObject->second) textureObject->second->release();
        mTextureMap.erase(textureObject);
    }
}

void ResourceManager::deleteRenderbuffer(GLuint renderbuffer)
{
    auto renderbufferObject = mRenderbufferMap.find(renderbuffer);

    if (renderbufferObject != mRenderbufferMap.end())
    {
        mRenderbufferHandleAllocator.release(renderbufferObject->first);
        if (renderbufferObject->second) renderbufferObject->second->release();
        mRenderbufferMap.erase(renderbufferObject);
    }
}

void ResourceManager::deleteSampler(GLuint sampler)
{
    auto samplerObject = mSamplerMap.find(sampler);

    if (samplerObject != mSamplerMap.end())
    {
        mSamplerHandleAllocator.release(samplerObject->first);
        if (samplerObject->second) samplerObject->second->release();
        mSamplerMap.erase(samplerObject);
    }
}

void ResourceManager::deleteFenceSync(GLuint fenceSync)
{
    auto fenceObjectIt = mFenceSyncMap.find(fenceSync);

    if (fenceObjectIt != mFenceSyncMap.end())
    {
        mFenceSyncHandleAllocator.release(fenceObjectIt->first);
        if (fenceObjectIt->second) fenceObjectIt->second->release();
        mFenceSyncMap.erase(fenceObjectIt);
    }
}

void ResourceManager::deletePaths(GLuint first, GLsizei range)
{
    for (GLsizei i = 0; i < range; ++i)
    {
        const auto id = first + i;
        const auto it = mPathMap.find(id);
        if (it == mPathMap.end())
            continue;
        Path *p = it->second;
        delete p;
        mPathMap.erase(it);
    }
    mPathHandleAllocator.releaseRange(first, static_cast<GLuint>(range));
}

Buffer *ResourceManager::getBuffer(unsigned int handle)
{
    auto buffer = mBufferMap.find(handle);

    if (buffer == mBufferMap.end())
    {
        return nullptr;
    }
    else
    {
        return buffer->second;
    }
}

Shader *ResourceManager::getShader(unsigned int handle) const
{
    auto shader = mShaderMap.find(handle);

    if (shader == mShaderMap.end())
    {
        return nullptr;
    }
    else
    {
        return shader->second;
    }
}

Texture *ResourceManager::getTexture(unsigned int handle)
{
    if (handle == 0)
        return nullptr;

    auto texture = mTextureMap.find(handle);

    if (texture == mTextureMap.end())
    {
        return nullptr;
    }
    else
    {
        return texture->second;
    }
}

Program *ResourceManager::getProgram(unsigned int handle) const
{
    auto program = mProgramMap.find(handle);

    if (program == mProgramMap.end())
    {
        return nullptr;
    }
    else
    {
        return program->second;
    }
}

Renderbuffer *ResourceManager::getRenderbuffer(unsigned int handle)
{
    auto renderbuffer = mRenderbufferMap.find(handle);

    if (renderbuffer == mRenderbufferMap.end())
    {
        return nullptr;
    }
    else
    {
        return renderbuffer->second;
    }
}

Sampler *ResourceManager::getSampler(unsigned int handle)
{
    auto sampler = mSamplerMap.find(handle);

    if (sampler == mSamplerMap.end())
    {
        return nullptr;
    }
    else
    {
        return sampler->second;
    }
}

FenceSync *ResourceManager::getFenceSync(unsigned int handle)
{
    auto fenceObjectIt = mFenceSyncMap.find(handle);

    if (fenceObjectIt == mFenceSyncMap.end())
    {
        return nullptr;
    }
    else
    {
        return fenceObjectIt->second;
    }
}

const Path *ResourceManager::getPath(GLuint handle) const
{
    auto it = mPathMap.find(handle);
    if (it == std::end(mPathMap))
        return nullptr;
    return it->second;
}

Path *ResourceManager::getPath(GLuint handle)
{
    auto it = mPathMap.find(handle);
    if (it == std::end(mPathMap))
        return nullptr;

    return it->second;
}

bool ResourceManager::hasPath(GLuint handle) const
{
    return mPathHandleAllocator.isUsed(handle);
}

void ResourceManager::setRenderbuffer(GLuint handle, Renderbuffer *buffer)
{
    mRenderbufferMap[handle] = buffer;
}

Buffer *ResourceManager::checkBufferAllocation(rx::GLImplFactory *factory, GLuint handle)
{
    if (handle == 0)
    {
        return nullptr;
    }

    auto bufferMapIt     = mBufferMap.find(handle);
    bool handleAllocated = (bufferMapIt != mBufferMap.end());

    if (handleAllocated && bufferMapIt->second != nullptr)
    {
        return bufferMapIt->second;
    }

    Buffer *buffer = new Buffer(factory->createBuffer(), handle);
    buffer->addRef();

    if (handleAllocated)
    {
        bufferMapIt->second = buffer;
    }
    else
    {
        mBufferHandleAllocator.reserve(handle);
        mBufferMap[handle] = buffer;
    }

    return buffer;
}

Texture *ResourceManager::checkTextureAllocation(rx::GLImplFactory *factory,
                                                 GLuint handle,
                                                 GLenum type)
{
    if (handle == 0)
    {
        return nullptr;
    }

    auto textureMapIt    = mTextureMap.find(handle);
    bool handleAllocated = (textureMapIt != mTextureMap.end());

    if (handleAllocated && textureMapIt->second != nullptr)
    {
        return textureMapIt->second;
    }

    Texture *texture = new Texture(factory, handle, type);
    texture->addRef();

    if (handleAllocated)
    {
        textureMapIt->second = texture;
    }
    else
    {
        mTextureHandleAllocator.reserve(handle);
        mTextureMap[handle] = texture;
    }

    return texture;
}

Renderbuffer *ResourceManager::checkRenderbufferAllocation(rx::GLImplFactory *factory,
                                                           GLuint handle)
{
    if (handle == 0)
    {
        return nullptr;
    }

    auto renderbufferMapIt = mRenderbufferMap.find(handle);
    bool handleAllocated   = (renderbufferMapIt != mRenderbufferMap.end());

    if (handleAllocated && renderbufferMapIt->second != nullptr)
    {
        return renderbufferMapIt->second;
    }

    Renderbuffer *renderbuffer = new Renderbuffer(factory->createRenderbuffer(), handle);
    renderbuffer->addRef();

    if (handleAllocated)
    {
        renderbufferMapIt->second = renderbuffer;
    }
    else
    {
        mRenderbufferHandleAllocator.reserve(handle);
        mRenderbufferMap[handle] = renderbuffer;
    }

    return renderbuffer;
}

Sampler *ResourceManager::checkSamplerAllocation(rx::GLImplFactory *factory, GLuint samplerHandle)
{
    // Samplers cannot be created via Bind
    if (samplerHandle == 0)
    {
        return nullptr;
    }

    Sampler *sampler = getSampler(samplerHandle);

    if (!sampler)
    {
        sampler                    = new Sampler(factory, samplerHandle);
        mSamplerMap[samplerHandle] = sampler;
        sampler->addRef();
    }

    return sampler;
}

bool ResourceManager::isSampler(GLuint sampler)
{
    return mSamplerMap.find(sampler) != mSamplerMap.end();
}

bool ResourceManager::isTextureGenerated(GLuint texture) const
{
    return texture == 0 || mTextureMap.find(texture) != mTextureMap.end();
}

bool ResourceManager::isBufferGenerated(GLuint buffer) const
{
    return buffer == 0 || mBufferMap.find(buffer) != mBufferMap.end();
}

bool ResourceManager::isRenderbufferGenerated(GLuint renderbuffer) const
{
    return renderbuffer == 0 || mRenderbufferMap.find(renderbuffer) != mRenderbufferMap.end();
}

}  // namespace gl
