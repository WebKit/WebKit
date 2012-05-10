/*
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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
#include "DOMWindowProperty.h"

#include "DOMWindow.h"
#include "Frame.h"

namespace WebCore {

DOMWindowProperty::DOMWindowProperty(Frame* frame)
    : m_frame(frame)
    , m_disconnectedDOMWindow(0)
{
    if (m_frame)
        m_frame->domWindow()->registerProperty(this);
}

DOMWindowProperty::~DOMWindowProperty()
{
    if (m_frame) {
        ASSERT(!m_disconnectedDOMWindow);
        m_frame->domWindow()->unregisterProperty(this);
    } else if (m_disconnectedDOMWindow)
        m_disconnectedDOMWindow->unregisterProperty(this);
}

void DOMWindowProperty::disconnectFrameForPageCache()
{
    ASSERT(m_frame);
    ASSERT(!m_disconnectedDOMWindow);
    m_disconnectedDOMWindow = m_frame->domWindow();
    m_frame = 0;
}

void DOMWindowProperty::reconnectFrameFromPageCache(Frame* frame)
{
    ASSERT(frame);
    ASSERT(!m_frame);
    ASSERT(m_disconnectedDOMWindow);
    m_frame = frame;
    m_disconnectedDOMWindow = 0;
}

void DOMWindowProperty::willDestroyGlobalObjectInCachedFrame()
{
    ASSERT(m_disconnectedDOMWindow);
    // DOMWindowProperty lifetime isn't tied directly to the DOMWindow itself so it is important
    // that it unregister itself from any DOMWindow it is associated with.
    m_disconnectedDOMWindow->unregisterProperty(this);
    m_disconnectedDOMWindow = 0;
}

void DOMWindowProperty::willDestroyGlobalObjectInFrame()
{
    ASSERT(m_frame);
    ASSERT(!m_disconnectedDOMWindow);
    // DOMWindowProperty lifetime isn't tied directly to the DOMWindow itself so it is important that it unregister
    // itself from any DOMWindow it is associated with.
    m_frame->domWindow()->unregisterProperty(this);
    m_frame = 0;
}

void DOMWindowProperty::willDetachGlobalObjectFromFrame()
{
    ASSERT(m_frame);
    ASSERT(!m_disconnectedDOMWindow);
}

}
