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
#include "Frame.h"
#include "FrameLoaderClient.h"

namespace WebCore {

DOMWindowExtension::DOMWindowExtension(Frame* frame, DOMWrapperWorld* world)
    : DOMWindowProperty(frame)
    , m_world(world)
    , m_disconnectedDOMWindow(0)
    , m_wasDetached(false)
{
    ASSERT(this->frame());
    ASSERT(m_world);
}

DOMWindowExtension::~DOMWindowExtension()
{
    // DOMWindowExtension lifetime isn't tied directly to the DOMWindow itself so it is important that it unregister
    // itself from any DOMWindow it is associated with when destroyed.
    // This might happen if the DOMWindowExtension is destroyed while its DOMWindow is in a CachedPage.
    if (m_disconnectedDOMWindow)
        m_disconnectedDOMWindow->unregisterProperty(this);
}

void DOMWindowExtension::disconnectFrame()
{
    // The DOMWindow destructor calls disconnectFrame on all its DOMWindowProperties, even if it
    // did that already when entering the page cache.
    if (m_disconnectedFrame) {
        ASSERT(!frame());
        return;
    }

    // Calling out to the client might result in this DOMWindowExtension being destroyed
    // while there is still work to do.
    RefPtr<DOMWindowExtension> protector = this;
    
    // DOMWindowProperties are disconnected from frames after they are detached.
    // DOMWindowExtensions only want to stay prepared for client callbacks if they've never been detached.
    if (!m_wasDetached) {
        Frame* frame = this->frame();
        frame->loader()->client()->dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(this);

        m_disconnectedFrame = frame;
        m_disconnectedDOMWindow = frame->domWindow();
    }

    DOMWindowProperty::disconnectFrame();
}

void DOMWindowExtension::reconnectFrame(Frame* frame)
{
    // DOMWindowProperties should never reconnect to a frame after they've been detached from the page.
    ASSERT(!m_wasDetached);
    ASSERT(m_disconnectedFrame == frame);
    
    DOMWindowProperty::reconnectFrame(frame);
    m_disconnectedFrame = 0;
    m_disconnectedDOMWindow = 0;

    this->frame()->loader()->client()->dispatchDidReconnectDOMWindowExtensionToGlobalObject(this);
}

void DOMWindowExtension::willDetachPage()
{
    // willDetachPage might be called multiple times but we only want to react once.
    if (m_wasDetached)
        return;
    
    // Calling out to the client might result in this DOMWindowExtension being destroyed
    // while there is still work to do.
    RefPtr<DOMWindowExtension> protector = this;
    
    Frame* frame = m_disconnectedFrame.get();
    if (!frame)
        frame = this->frame();
    ASSERT(frame);

    // DOMWindowExtension lifetime isn't tied directly to the DOMWindow itself so it is important that it unregister
    // itself from any DOMWindow it is associated with when detached.
    // This might be the disconnected DOMWindow if the DOMWindow is in a CachedPage that is pruned.
    DOMWindow* associatedDOMWindow = m_disconnectedDOMWindow ? m_disconnectedDOMWindow : frame->domWindow();
    associatedDOMWindow->unregisterProperty(this);
    m_disconnectedDOMWindow = 0;
    
    frame->loader()->client()->dispatchWillDestroyGlobalObjectForDOMWindowExtension(this);

    m_disconnectedFrame = 0;

    DOMWindowProperty::willDetachPage();
    
    m_wasDetached = true;
}

} // namespace WebCore
