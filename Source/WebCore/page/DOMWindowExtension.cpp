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

#include "DOMWrapperWorld.h"
#include "Document.h"
#include "FrameLoader.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include <wtf/Ref.h>

namespace WebCore {

DOMWindowExtension::DOMWindowExtension(LocalDOMWindow* window, DOMWrapperWorld& world)
    : m_window(window)
    , m_world(world)
    , m_wasDetached(false)
{
    ASSERT(this->frame());
    if (RefPtr window = m_window.get())
        window->registerObserver(*this);
}

DOMWindowExtension::~DOMWindowExtension()
{
    if (RefPtr window = m_window.get())
        window->unregisterObserver(*this);
}

LocalFrame* DOMWindowExtension::frame() const
{
    return m_window ? m_window->frame() : nullptr;
}

RefPtr<LocalFrame> DOMWindowExtension::protectedFrame() const
{
    return frame();
}

void DOMWindowExtension::suspendForBackForwardCache()
{
    // Calling out to the client might result in this DOMWindowExtension being destroyed
    // while there is still work to do.
    Ref protectedThis { *this };

    Ref frame = *this->frame();
    frame->checkedLoader()->client().dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(this);

    m_disconnectedFrame = WTFMove(frame);
}

void DOMWindowExtension::resumeFromBackForwardCache()
{
    ASSERT(frame());
    ASSERT(m_disconnectedFrame == frame());
    ASSERT(frame()->document()->domWindow() == m_window);

    m_disconnectedFrame = nullptr;

    protectedFrame()->checkedLoader()->client().dispatchDidReconnectDOMWindowExtensionToGlobalObject(this);
}

void DOMWindowExtension::willDestroyGlobalObjectInCachedFrame()
{
    ASSERT(m_disconnectedFrame); // Somehow m_disconnectedFrame can be null here. See <rdar://problem/49613448>.

    // Calling out to the client might result in this DOMWindowExtension being destroyed
    // while there is still work to do.
    Ref protectedThis { *this };

    if (RefPtr disconnectedFrame = m_disconnectedFrame)
        disconnectedFrame->checkedLoader()->client().dispatchWillDestroyGlobalObjectForDOMWindowExtension(this);
    m_disconnectedFrame = nullptr;

    // DOMWindowExtension lifetime isn't tied directly to the LocalDOMWindow itself so it is important that it unregister
    // itself from any LocalDOMWindow it is associated with if that LocalDOMWindow is going away.
    ASSERT(m_window);
    if (RefPtr window = m_window.get())
        window->unregisterObserver(*this);
    m_window = nullptr;
}

void DOMWindowExtension::willDestroyGlobalObjectInFrame()
{
    ASSERT(!m_disconnectedFrame);

    // Calling out to the client might result in this DOMWindowExtension being destroyed
    // while there is still work to do.
    Ref protectedThis { *this };

    if (!m_wasDetached) {
        RefPtr frame = this->frame();
        ASSERT(frame);
        frame->checkedLoader()->client().dispatchWillDestroyGlobalObjectForDOMWindowExtension(this);
    }

    // DOMWindowExtension lifetime isn't tied directly to the LocalDOMWindow itself so it is important that it unregister
    // itself from any LocalDOMWindow it is associated with if that LocalDOMWindow is going away.
    ASSERT(m_window);
    if (RefPtr window = m_window.get())
        window->unregisterObserver(*this);
    m_window = nullptr;
}

void DOMWindowExtension::willDetachGlobalObjectFromFrame()
{
    ASSERT(!m_disconnectedFrame);
    ASSERT(!m_wasDetached);

    // Calling out to the client might result in this DOMWindowExtension being destroyed
    // while there is still work to do.
    Ref protectedThis { *this };

    RefPtr frame = this->frame();
    ASSERT(frame);
    frame->checkedLoader()->client().dispatchWillDestroyGlobalObjectForDOMWindowExtension(this);

    m_wasDetached = true;
}

} // namespace WebCore
