//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextImpl:
//   Implementation-specific functionality associated with a GL Context.
//

#include "libANGLE/renderer/ContextImpl.h"

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

}  // namespace rx
