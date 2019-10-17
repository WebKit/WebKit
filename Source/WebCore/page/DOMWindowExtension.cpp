/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMWindowExtension.h"

#include "DOMWindow.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include <wtf/Ref.h>

namespace WebCore {

DOMWindowExtension::DOMWindowExtension(DOMWindow* window, DOMWrapperWorld& world)
    : m_window(makeWeakPtr(window))
    , m_world(world)
    , m_wasDetached(false)
{
    ASSERT(this->frame());
    if (m_window)
        m_window->registerObserver(*this);
}

DOMWindowExtension::~DOMWindowExtension()
{
    if (m_window)
        m_window->unregisterObserver(*this);
}

Frame* DOMWindowExtension::frame() const
{
    return m_window ? m_window->frame() : nullptr;
}

void DOMWindowExtension::suspendForBackForwardCache()
{
    // Calling out to the client might result in this DOMWindowExtension being destroyed
    // while there is still work to do.
    Ref<DOMWindowExtension> protectedThis(*this);

    auto frame = makeRef(*this->frame());
    frame->loader().client().dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(this);

    m_disconnectedFrame = WTFMove(frame);
}

void DOMWindowExtension::resumeFromBackForwardCache()
{
    ASSERT(frame());
    ASSERT(m_disconnectedFrame == frame());
    ASSERT(frame()->document()->domWindow() == m_window);

    m_disconnectedFrame = nullptr;

    frame()->loader().client().dispatchDidReconnectDOMWindowExtensionToGlobalObject(this);
}

void DOMWindowExtension::willDestroyGlobalObjectInCachedFrame()
{
    ASSERT(m_disconnectedFrame); // Somehow m_disconnectedFrame can be null here. See <rdar://problem/49613448>.

    // Calling out to the client might result in this DOMWindowExtension being destroyed
    // while there is still work to do.
    Ref<DOMWindowExtension> protectedThis(*this);

    if (m_disconnectedFrame)
        m_disconnectedFrame->loader().client().dispatchWillDestroyGlobalObjectForDOMWindowExtension(this);
    m_disconnectedFrame = nullptr;

    // DOMWindowExtension lifetime isn't tied directly to the DOMWindow itself so it is important that it unregister
    // itself from any DOMWindow it is associated with if that DOMWindow is going away.
    ASSERT(m_window);
    if (m_window)
        m_window->unregisterObserver(*this);
    m_window = nullptr;
}

void DOMWindowExtension::willDestroyGlobalObjectInFrame()
{
    ASSERT(!m_disconnectedFrame);

    // Calling out to the client might result in this DOMWindowExtension being destroyed
    // while there is still work to do.
    Ref<DOMWindowExtension> protectedThis(*this);

    if (!m_wasDetached) {
        Frame* frame = this->frame();
        ASSERT(frame);
        frame->loader().client().dispatchWillDestroyGlobalObjectForDOMWindowExtension(this);
    }

    // DOMWindowExtension lifetime isn't tied directly to the DOMWindow itself so it is important that it unregister
    // itself from any DOMWindow it is associated with if that DOMWindow is going away.
    ASSERT(m_window);
    if (m_window)
        m_window->unregisterObserver(*this);
    m_window = nullptr;
}

void DOMWindowExtension::willDetachGlobalObjectFromFrame()
{
    ASSERT(!m_disconnectedFrame);
    ASSERT(!m_wasDetached);

    // Calling out to the client might result in this DOMWindowExtension being destroyed
    // while there is still work to do.
    Ref<DOMWindowExtension> protectedThis(*this);

    Frame* frame = this->frame();
    ASSERT(frame);
    frame->loader().client().dispatchWillDestroyGlobalObjectForDOMWindowExtension(this);

    m_wasDetached = true;
}

} // namespace WebCore
