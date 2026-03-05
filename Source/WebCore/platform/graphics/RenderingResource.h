/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/AbstractCanMakeCheckedPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {
namespace DisplayList {
class DisplayList;
}
class Gradient;
class NativeImage;
class PathImpl;

class RenderingResourceObserver : public AbstractCanMakeCheckedPtr {
public:
    using WeakValueType = RenderingResourceObserver;
    virtual ~RenderingResourceObserver() = default;

    virtual void willDestroyNativeImage(const NativeImage&) = 0;
    virtual void willDestroyGradient(const Gradient&) = 0;
    virtual void willDestroyPathImpl(const PathImpl&) = 0;
    virtual void willDestroyFilter(RenderingResourceIdentifier) = 0;
    virtual void willDestroyDisplayList(const DisplayList::DisplayList&) = 0;

protected:
    RenderingResourceObserver() = default;
};

class RenderingResource
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RenderingResource> {
public:
    virtual ~RenderingResource() = default;

    virtual bool isFilter() const { return false; }

    bool hasValidRenderingResourceIdentifier() const
    {
        return m_renderingResourceIdentifier.has_value();
    }

    RenderingResourceIdentifier renderingResourceIdentifier() const
    {
        ASSERT(m_renderingResourceIdentifier);
        return *m_renderingResourceIdentifier;
    }

    std::optional<RenderingResourceIdentifier> renderingResourceIdentifierIfExists() const
    {
        return m_renderingResourceIdentifier;
    }

    void addObserver(WeakRef<RenderingResourceObserver>&& observer)
    {
        ASSERT(hasValidRenderingResourceIdentifier());
        m_observers.add(WTF::move(observer));
    }

protected:
    RenderingResource(std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
        : m_renderingResourceIdentifier(renderingResourceIdentifier)
    {
    }

    WeakHashSet<RenderingResourceObserver> m_observers;
    std::optional<RenderingResourceIdentifier> m_renderingResourceIdentifier;
};

} // namespace WebCore
