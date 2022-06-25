/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "RemoteDOMWindow.h"

#include "RemoteFrame.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RemoteDOMWindow);

RemoteDOMWindow::RemoteDOMWindow(Ref<RemoteFrame>&& frame, GlobalWindowIdentifier&& identifier)
    : AbstractDOMWindow(WTFMove(identifier))
    , m_frame(WTFMove(frame))
{
    m_frame->setWindow(this);
}

RemoteDOMWindow::~RemoteDOMWindow()
{
    if (m_frame)
        m_frame->setWindow(nullptr);
}

WindowProxy* RemoteDOMWindow::self() const
{
    if (!m_frame)
        return nullptr;
    return &m_frame->windowProxy();
}

Location* RemoteDOMWindow::location() const
{
    // FIXME: Implemented this.
    return nullptr;
}

void RemoteDOMWindow::close(Document&)
{
    // FIXME: Implemented this.
}

bool RemoteDOMWindow::closed() const
{
    return !m_frame;
}

void RemoteDOMWindow::focus(DOMWindow& incumbentWindow)
{
    UNUSED_PARAM(incumbentWindow);
    // FIXME: Implemented this.
}

void RemoteDOMWindow::blur()
{
    // FIXME: Implemented this.
}

unsigned RemoteDOMWindow::length() const
{
    // FIXME: Implemented this.
    return 0;
}

WindowProxy* RemoteDOMWindow::top() const
{
    if (!m_frame)
        return nullptr;

    // FIXME: Implemented this.
    return &m_frame->windowProxy();
}

WindowProxy* RemoteDOMWindow::opener() const
{
    if (!m_frame)
        return nullptr;

    auto* openerFrame = m_frame->opener();
    if (!openerFrame)
        return nullptr;

    return &openerFrame->windowProxy();
}

WindowProxy* RemoteDOMWindow::parent() const
{
    if (!m_frame)
        return nullptr;

    // FIXME: Implemented this.
    return &m_frame->windowProxy();
}

void RemoteDOMWindow::postMessage(JSC::JSGlobalObject&, DOMWindow& incumbentWindow, JSC::JSValue message, const String& targetOrigin, Vector<JSC::Strong<JSC::JSObject>>&&)
{
    UNUSED_PARAM(incumbentWindow);
    UNUSED_PARAM(message);
    UNUSED_PARAM(targetOrigin);

    // FIXME: Implemented this.
}

} // namespace WebCore
