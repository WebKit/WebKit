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
#include "HTMLIFrameElement.h"
#include "HistoryController.h"
#include "LocalDOMWindow.h"
#include "NavigationScheduler.h"
#include "Page.h"
#include "RemoteFrame.h"
#include "RenderElement.h"
#include "RenderWidget.h"
#include "ScrollingCoordinator.h"
#include "WindowProxy.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

#if ASSERT_ENABLED
class FrameLifetimeVerifier {
public:
    static FrameLifetimeVerifier& singleton()
    {
        static NeverDestroyed<FrameLifetimeVerifier> instance;
        return instance.get();
    }

    void frameCreated(Frame& frame)
    {
        auto& pair = m_map.ensure(frame.frameID(), [] {
            return std::pair<WeakPtr<LocalFrame>, WeakPtr<RemoteFrame>> { };
        }).iterator->value;

        switch (frame.frameType()) {
        case Frame::FrameType::Local:
            ASSERT_WITH_MESSAGE(!pair.first, "There should never be two LocalFrames with the same ID in the same process");
            pair.first = downcast<LocalFrame>(frame);
            break;
        case Frame::FrameType::Remote:
            ASSERT_WITH_MESSAGE(!pair.second, "There should never be two RemoteFrames with the same ID in the same process");
            pair.second = downcast<RemoteFrame>(frame);
            break;
        }
    }

    void frameDestroyed(Frame& frame)
    {
        auto it = m_map.find(frame.frameID());
        ASSERT(it != m_map.end());
        auto& pair = it->value;
        switch (frame.frameType()) {
        case Frame::FrameType::Local:
            ASSERT(pair.first == &frame);
            if (pair.second)
                pair.first = nullptr;
            else
                m_map.remove(it);
            break;
        case Frame::FrameType::Remote:
            ASSERT(pair.second == &frame);
            if (pair.first)
                pair.second = nullptr;
            else
                m_map.remove(it);
        }
    }

    bool isRootFrameIdentifier(FrameIdentifier identifier)
    {
        auto it = m_map.find(identifier);
        if (it == m_map.end())
            return false;
        return it->value.first && it->value.first->isRootFrame();
    }
private:
    HashMap<FrameIdentifier, std::pair<WeakPtr<LocalFrame>, WeakPtr<RemoteFrame>>> m_map;
};
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
    FrameLifetimeVerifier::singleton().frameCreated(*this);
#endif
}

Frame::~Frame()
{
    m_windowProxy->detachFromFrame();
    m_navigationScheduler->cancel();

#if ASSERT_ENABLED
    FrameLifetimeVerifier::singleton().frameDestroyed(*this);
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
    if (isRootFrame()) {
        if (m_page) {
            m_page->removeRootFrame(downcast<LocalFrame>(*this));
            if (auto* scrollingCoordinator = m_page->scrollingCoordinator())
                scrollingCoordinator->rootFrameWasRemoved(frameID());
        }
    }
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
    return FrameLifetimeVerifier::singleton().isRootFrameIdentifier(identifier);
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

void Frame::setOwnerPermissionsPolicy(OwnerPermissionsPolicyData&& ownerPermissionsPolicy)
{
    m_ownerPermisssionsPolicyOverride = WTFMove(ownerPermissionsPolicy);
}

std::optional<OwnerPermissionsPolicyData> Frame::ownerPermissionsPolicy() const
{
    if (m_ownerPermisssionsPolicyOverride)
        return m_ownerPermisssionsPolicyOverride;

    RefPtr owner = ownerElement();
    if (!owner)
        return std::nullopt;

    auto documentOrigin = owner->document().securityOrigin().data();
    auto documentPolicy = owner->document().permissionsPolicy();

    RefPtr iframe = dynamicDowncast<HTMLIFrameElement>(owner);
    auto containerPolicy = iframe ? PermissionsPolicy::processPermissionsPolicyAttribute(*iframe) : PermissionsPolicy::PolicyDirective { };
    return OwnerPermissionsPolicyData { WTFMove(documentOrigin), WTFMove(documentPolicy), WTFMove(containerPolicy) };
}

} // namespace WebCore
