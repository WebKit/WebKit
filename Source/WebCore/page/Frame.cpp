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
#include "Frame.h"

#include "DocumentInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "LocalFrame.h"
#include "NavigationScheduler.h"
#include "Page.h"
#include "RemoteFrame.h"
#include "WindowProxy.h"

namespace WebCore {

Frame::Frame(Page& page, FrameIdentifier frameID, FrameType frameType, HTMLFrameOwnerElement* ownerElement, Frame* parent)
    : m_page(page)
    , m_frameID(frameID)
    , m_treeNode(*this, parent)
    , m_windowProxy(WindowProxy::create(*this))
    , m_ownerElement(ownerElement)
    , m_mainFrame(parent ? page.mainFrame() : *this)
    , m_settings(page.settings())
    , m_frameType(frameType)
    , m_navigationScheduler(makeUniqueRef<NavigationScheduler>(*this))
{
    if (parent)
        parent->tree().appendChild(*this);

    if (ownerElement)
        ownerElement->setContentFrame(*this);
}

Frame::~Frame()
{
    m_windowProxy->detachFromFrame();
    m_navigationScheduler->cancel();
}

std::optional<PageIdentifier> Frame::pageID() const
{
    if (auto* page = this->page())
        return page->identifier();
    return std::nullopt;
}

bool Frame::isRootFrame() const
{
    // A root frame is a local frame with a remote frame parent or no parent.
    // It is the root of its local frame tree in this process but might have
    // a parent in another process.
    switch (m_frameType) {
    case FrameType::Local:
        if (auto* parent = tree().parent())
            return is<RemoteFrame>(parent);
        ASSERT(&m_mainFrame == this);
        return true;
    case FrameType::Remote:
        break;
    }
    return false;
}

void Frame::resetWindowProxy()
{
    ASSERT(m_windowProxy->frame() == this);
    m_windowProxy->detachFromFrame();
    m_windowProxy = WindowProxy::create(*this);
}

void Frame::detachFromPage()
{
    m_page = nullptr;
}

void Frame::disconnectOwnerElement()
{
    if (m_ownerElement) {
        m_ownerElement->clearContentFrame();
        m_ownerElement = nullptr;
    }

    // FIXME: This is a layering violation. Move this code so Frame doesn't do anything with its Document.
    if (auto* document = is<LocalFrame>(*this) ? downcast<LocalFrame>(*this).document() : nullptr)
        document->frameWasDisconnectedFromOwner();
}

void Frame::takeWindowProxyFrom(Frame& frame)
{
    ASSERT(m_windowProxy->frame() == this);
    m_windowProxy->detachFromFrame();
    m_windowProxy = frame.windowProxy();
    m_windowProxy->replaceFrame(*this);
}

} // namespace WebCore
