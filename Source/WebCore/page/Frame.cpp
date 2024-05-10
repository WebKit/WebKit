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

#include "HTMLFrameOwnerElement.h"
#include "HistoryController.h"
#include "NavigationScheduler.h"
#include "Page.h"
#include "RemoteFrame.h"
#include "RenderElement.h"
#include "RenderWidget.h"
#include "WindowProxy.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

#if ASSERT_ENABLED
static HashMap<FrameIdentifier, WeakRef<Frame>>& allFrames()
{
    static NeverDestroyed<HashMap<FrameIdentifier, WeakRef<Frame>>> map;
    return map;
}
#endif

Frame::Frame(Page& page, FrameIdentifier frameID, FrameType frameType, HTMLFrameOwnerElement* ownerElement, Frame* parent, Frame* opener)
    : m_page(page)
    , m_frameID(frameID)
    , m_treeNode(*this, parent)
    , m_windowProxy(WindowProxy::create(*this))
    , m_ownerElement(ownerElement)
    , m_mainFrame(parent ? page.mainFrame() : *this)
    , m_settings(page.settings())
    , m_frameType(frameType)
    , m_navigationScheduler(makeUniqueRef<NavigationScheduler>(*this))
    , m_opener(opener)
    , m_history(makeUniqueRef<HistoryController>(*this))
{
    if (parent)
        parent->tree().appendChild(*this);

    if (ownerElement)
        ownerElement->setContentFrame(*this);

#if ASSERT_ENABLED
    WeakPtr maybeOldFrame = allFrames().take(frameID);
    auto addResult = allFrames().add(frameID, *this);
    ASSERT(addResult.isNewEntry);
    ASSERT_WITH_MESSAGE(!maybeOldFrame || is<LocalFrame>(maybeOldFrame) != is<LocalFrame>(*this), "The only time there should be two frames with the same ID in the same process is when swapping between local and remote frames.");
#endif
}

Frame::~Frame()
{
    m_windowProxy->detachFromFrame();
    m_navigationScheduler->cancel();

#if ASSERT_ENABLED
    auto it = allFrames().find(m_frameID);
    if (it == allFrames().end())
        return;
    if (it->value.ptr() == this)
        allFrames().remove(it);
    else
        ASSERT_WITH_MESSAGE(is<LocalFrame>(it->value) != is<LocalFrame>(this), "The only time there should be two frames with the same ID in the same process is when swapping between local and remote frames.");
#endif
}

std::optional<PageIdentifier> Frame::pageID() const
{
    if (auto* page = this->page())
        return page->identifier();
    return std::nullopt;
}

void Frame::resetWindowProxy()
{
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

    frameWasDisconnectedFromOwner();
}

void Frame::takeWindowProxyFrom(Frame& frame)
{
    ASSERT(is<LocalDOMWindow>(window()) != is<LocalDOMWindow>(frame.window()));
    ASSERT(m_windowProxy->frame() == this);
    m_windowProxy->detachFromFrame();
    m_windowProxy = frame.windowProxy();
    frame.resetWindowProxy();
    m_windowProxy->replaceFrame(*this);
}

Ref<WindowProxy> Frame::protectedWindowProxy() const
{
    return m_windowProxy;
}

CheckedRef<NavigationScheduler> Frame::checkedNavigationScheduler() const
{
    return m_navigationScheduler.get();
}

RenderWidget* Frame::ownerRenderer() const
{
    RefPtr ownerElement = this->ownerElement();
    if (!ownerElement)
        return nullptr;
    // FIXME: If <object> is ever fixed to disassociate itself from frames
    // that it has started but canceled, then this can turn into an ASSERT
    // since ownerElement would be nullptr when the load is canceled.
    // https://bugs.webkit.org/show_bug.cgi?id=18585
    return dynamicDowncast<RenderWidget>(ownerElement->renderer());
}

RefPtr<FrameView> Frame::protectedVirtualView() const
{
    return virtualView();
}

#if ASSERT_ENABLED
bool Frame::isRootFrameIdentifier(FrameIdentifier identifier)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(allFrames().get(identifier));
    return localFrame && localFrame->isRootFrame();
}
#endif

void Frame::setOpener(Frame* opener)
{
    if (m_opener)
        m_opener->m_openedFrames.remove(*this);
    if (opener) {
        opener->m_openedFrames.add(*this);
        if (RefPtr page = this->page())
            page->setOpenedByDOMWithOpener(true);
    }
    m_opener = opener;

    reinitializeDocumentSecurityContext();
}

void Frame::detachFromAllOpenedFrames()
{
    for (auto& frame : std::exchange(m_openedFrames, { }))
        frame.m_opener = nullptr;
}

Vector<Ref<Frame>> Frame::openedFrames()
{
    return WTF::map(m_openedFrames, [] (auto& frame) {
        return Ref { frame };
    });
}

bool Frame::hasOpenedFrames() const
{
    return !m_openedFrames.isEmptyIgnoringNullReferences();
}

CheckedRef<HistoryController> Frame::checkedHistory() const
{
    return m_history.get();
}

void Frame::setOwnerElement(HTMLFrameOwnerElement* element)
{
    m_ownerElement = element;
    if (element) {
        element->clearContentFrame();
        element->setContentFrame(*this);
    }
}

} // namespace WebCore
