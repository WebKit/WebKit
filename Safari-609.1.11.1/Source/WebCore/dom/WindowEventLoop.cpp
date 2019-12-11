/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WindowEventLoop.h"

#include "CommonVM.h"
#include "Document.h"
#include "Microtasks.h"

namespace WebCore {

static HashMap<RegistrableDomain, WindowEventLoop*>& windowEventLoopMap()
{
    RELEASE_ASSERT(isMainThread());
    static NeverDestroyed<HashMap<RegistrableDomain, WindowEventLoop*>> map;
    return map.get();
}

Ref<WindowEventLoop> WindowEventLoop::ensureForRegistrableDomain(const RegistrableDomain& domain)
{
    auto addResult = windowEventLoopMap().add(domain, nullptr);
    if (UNLIKELY(addResult.isNewEntry)) {
        auto newEventLoop = adoptRef(*new WindowEventLoop(domain));
        addResult.iterator->value = newEventLoop.ptr();
        return newEventLoop;
    }
    return *addResult.iterator->value;
}

inline WindowEventLoop::WindowEventLoop(const RegistrableDomain& domain)
    : m_domain(domain)
{
}

WindowEventLoop::~WindowEventLoop()
{
    auto didRemove = windowEventLoopMap().remove(m_domain);
    RELEASE_ASSERT(didRemove);
}

void WindowEventLoop::scheduleToRun()
{
    callOnMainThread([eventLoop = makeRef(*this)] () {
        eventLoop->run();
    });
}

bool WindowEventLoop::isContextThread() const
{
    return isMainThread();
}

MicrotaskQueue& WindowEventLoop::microtaskQueue()
{
    // MicrotaskQueue must be one per event loop.
    static NeverDestroyed<MicrotaskQueue> queue(commonVM());
    return queue;
}

} // namespace WebCore
