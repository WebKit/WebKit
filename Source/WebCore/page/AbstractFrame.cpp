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
#include "AbstractFrame.h"

#include "DocumentInlines.h"
#include "Frame.h"
#include "HTMLFrameOwnerElement.h"
#include "Page.h"
#include "WindowProxy.h"

namespace WebCore {

static AbstractFrame* parentFrame(HTMLFrameOwnerElement* ownerElement)
{
    return ownerElement ? ownerElement->document().frame() : nullptr;
}

AbstractFrame::AbstractFrame(Page& page, FrameIdentifier frameID, HTMLFrameOwnerElement* ownerElement)
    : m_page(page)
    , m_frameID(frameID)
    , m_treeNode(*this, parentFrame(ownerElement))
    , m_windowProxy(WindowProxy::create(*this))
    , m_ownerElement(ownerElement)
{
    if (auto* parent = parentFrame(ownerElement))
        parent->tree().appendChild(*this);
}

AbstractFrame::~AbstractFrame()
{
    m_windowProxy->detachFromFrame();
}

void AbstractFrame::resetWindowProxy()
{
    m_windowProxy->detachFromFrame();
    m_windowProxy = WindowProxy::create(*this);
}

Page* AbstractFrame::page() const
{
    return m_page.get();
}

HTMLFrameOwnerElement* AbstractFrame::ownerElement() const
{
    return m_ownerElement.get();
}

void AbstractFrame::detachFromPage()
{
    m_page = nullptr;
}

void AbstractFrame::disconnectOwnerElement()
{
    if (m_ownerElement) {
        m_ownerElement->clearContentFrame();
        m_ownerElement = nullptr;
    }

    // FIXME: This is a layering violation. Move this code so AbstractFrame doesn't do anything with its Document.
    if (auto* document = is<LocalFrame>(*this) ? downcast<LocalFrame>(*this).document() : nullptr)
        document->frameWasDisconnectedFromOwner();
}

} // namespace WebCore
