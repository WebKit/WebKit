//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Resource:
//    Resource lifetime tracking in the Vulkan back-end.
//

#include "libANGLE/renderer/vulkan/ResourceVk.h"

#include "libANGLE/renderer/vulkan/ContextVk.h"

namespace rx
{
namespace vk
{
// Resource implementation.
angle::Result Resource::waitForIdle(ContextVk *contextVk,
                                    const char *debugMessage,
                                    RenderPassClosureReason reason)
{
    // If there are pending commands for the resource, flush them.
    if (contextVk->hasUnsubmittedUse(mUse))
    {
        ANGLE_TRY(contextVk->flushImpl(nullptr, reason));
    }

    RendererVk *renderer = contextVk->getRenderer();
    // Make sure the driver is done with the resource.
    if (renderer->hasUnfinishedUse(mUse))
    {
        if (debugMessage)
        {
            ANGLE_VK_PERF_WARNING(contextVk, GL_DEBUG_SEVERITY_HIGH, "%s", debugMessage);
        }
        ANGLE_TRY(renderer->finishResourceUse(contextVk, mUse));
    }

    ASSERT(!renderer->hasUnfinishedUse(mUse));

    return angle::Result::Continue;
}

// SharedGarbage implementation.
SharedGarbage::SharedGarbage() = default;

SharedGarbage::SharedGarbage(SharedGarbage &&other)
{
    *this = std::move(other);
}

SharedGarbage::SharedGarbage(const ResourceUse &use, GarbageList &&garbage)
    : mLifetime(use), mGarbage(std::move(garbage))
{}

SharedGarbage::~SharedGarbage() = default;

SharedGarbage &SharedGarbage::operator=(SharedGarbage &&rhs)
{
    std::swap(mLifetime, rhs.mLifetime);
    std::swap(mGarbage, rhs.mGarbage);
    return *this;
}

bool SharedGarbage::destroyIfComplete(RendererVk *renderer)
{
    if (renderer->hasUnfinishedUse(mLifetime))
    {
        return false;
    }

    for (GarbageObject &object : mGarbage)
    {
        object.destroy(renderer);
    }

    return true;
}

bool SharedGarbage::hasUnsubmittedUse(RendererVk *renderer) const
{
    return renderer->hasUnsubmittedUse(mLifetime);
}
}  // namespace vk
}  // namespace rx
