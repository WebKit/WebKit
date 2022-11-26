//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextImpl:
//   Implementation-specific functionality associated with a GL Context.
//

#include "libANGLE/renderer/ContextImpl.h"

#include "common/third_party/base/anglebase/no_destructor.h"
#include "libANGLE/Context.h"

namespace rx
{
ContextImpl::ContextImpl(const gl::State &state, gl::ErrorSet *errorSet)
    : mState(state), mMemoryProgramCache(nullptr), mErrors(errorSet)
{}

ContextImpl::~ContextImpl() {}

void ContextImpl::invalidateTexture(gl::TextureType target)
{
    UNREACHABLE();
}

angle::Result ContextImpl::onUnMakeCurrent(const gl::Context *context)
{
    return angle::Result::Continue;
}

angle::Result ContextImpl::handleNoopDrawEvent()
{
    return angle::Result::Continue;
}

void ContextImpl::setMemoryProgramCache(gl::MemoryProgramCache *memoryProgramCache)
{
    mMemoryProgramCache = memoryProgramCache;
}

void ContextImpl::handleError(GLenum errorCode,
                              const char *message,
                              const char *file,
                              const char *function,
                              unsigned int line)
{
    std::stringstream errorStream;
    errorStream << "Internal error: " << gl::FmtHex(errorCode) << ": " << message;
    mErrors->handleError(errorCode, errorStream.str().c_str(), file, function, line);
}

egl::ContextPriority ContextImpl::getContextPriority() const
{
    return egl::ContextPriority::Medium;
}

egl::Error ContextImpl::releaseHighPowerGPU(gl::Context *)
{
    return egl::NoError();
}

egl::Error ContextImpl::reacquireHighPowerGPU(gl::Context *)
{
    return egl::NoError();
}

angle::Result ContextImpl::acquireTextures(const gl::Context *context,
                                           const gl::TextureBarrierVector &textureBarriers)
{
    UNREACHABLE();
    return angle::Result::Stop;
}

angle::Result ContextImpl::releaseTextures(const gl::Context *context,
                                           gl::TextureBarrierVector *textureBarriers)
{
    UNREACHABLE();
    return angle::Result::Stop;
}

const angle::PerfMonitorCounterGroups &ContextImpl::getPerfMonitorCounters()
{
    static angle::base::NoDestructor<angle::PerfMonitorCounterGroups> sCounters;
    return *sCounters;
}

angle::Result ContextImpl::drawPixelLocalStorageEXTEnable(gl::Context *,
                                                          GLsizei n,
                                                          const gl::PixelLocalStoragePlane[],
                                                          const GLenum loadops[])
{
    ASSERT(getNativePixelLocalStorageType() == ShPixelLocalStorageType::PixelLocalStorageEXT);
    UNREACHABLE();
    return angle::Result::Stop;
}

angle::Result ContextImpl::drawPixelLocalStorageEXTDisable(gl::Context *,
                                                           const gl::PixelLocalStoragePlane[],
                                                           const GLenum storeops[])
{
    ASSERT(getNativePixelLocalStorageType() == ShPixelLocalStorageType::PixelLocalStorageEXT);
    UNREACHABLE();
    return angle::Result::Stop;
}
}  // namespace rx
