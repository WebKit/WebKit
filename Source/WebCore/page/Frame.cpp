/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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

#include "ContainerNodeInlines.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLIFrameElement.h"
#include "LocalDOMWindow.h"
#include "NavigationScheduler.h"
#include "NodeDocument.h"
#include "OwnerPermissionsPolicyData.h"
#include "Page.h"
#include "RemoteFrame.h"
#include "RenderElement.h"
#include "RenderWidget.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
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

Frame::Frame(Page& page, FrameIdentifier frameID, FrameType frameType, HTMLFrameOwnerElement* ownerElement, Frame* parent, Frame* opener, Ref<FrameTreeSyncData>&& frameTreeSyncData, AddToFrameTree addToFrameTree)
    : m_page(page)
    , m_frameID(frameID)
    , m_treeNode(*this, addToFrameTree == AddToFrameTree::Yes ? parent : nullptr)
    , m_windowProxy(WindowProxy::create(*this))
    , m_ownerElement(ownerElement)
    , m_mainFrame(parent ? page.mainFrame() : *this)
    , m_settings(page.settings())
    , m_frameType(frameType)
    , m_navigationScheduler(makeUniqueRefWithoutRefCountedCheck<NavigationScheduler>(*this))
    , m_opener(opener)
    , m_frameTreeSyncData(WTFMove(frameTreeSyncData))
{
    relaxAdoptionRequirement();
    if (parent && addToFrameTree == AddToFrameTree::Yes)
        parent->tree().appendChild(*this);

    if (ownerElement)
        ownerElement->setContentFrame(*this);

    if (opener)
        opener->m_openedFrames.add(*this);

#if ASSERT_ENABLED
    FrameLifetimeVerifier::singleton().frameCreated(*this);
#endif
}

Frame::~Frame()
{
    protectedWindowProxy()->detachFromFrame();
    protectedNavigationScheduler()->cancel();

#if ASSERT_ENABLED
    FrameLifetimeVerifier::singleton().frameDestroyed(*this);
#endif
}

void Frame::resetWindowProxy()
{
    m_windowProxy = WindowProxy::create(*this);
}

void Frame::detachFromPage()
{
    if (isRootFrame()) {
        if (RefPtr page = m_page.get()) {
            page->removeRootFrame(downcast<LocalFrame>(*this));
            if (RefPtr scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->rootFrameWasRemoved(frameID());
        }
    }
    m_page = nullptr;
}

void Frame::disconnectOwnerElement()
{
    if (RefPtr ownerElement = m_ownerElement.get()) {
        ownerElement->clearContentFrame();
        m_ownerElement = nullptr;
    }

    frameWasDisconnectedFromOwner();
}

void Frame::takeWindowProxyAndOpenerFrom(Frame& frame)
{
    ASSERT(is<LocalDOMWindow>(window()) != is<LocalDOMWindow>(frame.window()) || page() != frame.page());
    ASSERT(m_windowProxy->frame() == this);
    protectedWindowProxy()->detachFromFrame();
    m_windowProxy = frame.windowProxy();
    frame.resetWindowProxy();
    protectedWindowProxy()->replaceFrame(*this);

    ASSERT(!m_opener);
    m_opener = frame.m_opener;
    if (m_opener)
        m_opener->m_openedFrames.add(*this);

    for (Ref opened : frame.m_openedFrames) {
        ASSERT(opened->m_opener.get() == &frame);
        opened->m_opener = *this;
        m_openedFrames.add(opened);
    }
    frame.m_openedFrames.clear();
}

Ref<WindowProxy> Frame::protectedWindowProxy() const
{
    return m_windowProxy;
}

RefPtr<DOMWindow> Frame::protectedWindow() const
{
    return window();
}

Ref<NavigationScheduler> Frame::protectedNavigationScheduler() const
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

void Frame::updateOpener(Frame& newOpener, NotifyUIProcess notifyUIProcess)
{
    if (notifyUIProcess == NotifyUIProcess::Yes)
        loaderClient().updateOpener(newOpener);
    if (m_opener)
        m_opener->m_openedFrames.remove(*this);
    newOpener.m_openedFrames.add(*this);
    if (RefPtr page = this->page())
        page->setOpenedByDOMWithOpener(true);
    m_opener = newOpener;

    reinitializeDocumentSecurityContext();
}

void Frame::disownOpener()
{
    if (m_opener)
        m_opener->m_openedFrames.remove(*this);
    m_opener = nullptr;

    reinitializeDocumentSecurityContext();
}

void Frame::setOpenerForWebKitLegacy(Frame* frame)
{
    ASSERT(!m_opener);
    ASSERT(frame);
    m_opener = frame;
    m_page->setOpenedByDOMWithOpener(true);
    reinitializeDocumentSecurityContext();
}

void Frame::detachFromAllOpenedFrames()
{
    for (Ref frame : std::exchange(m_openedFrames, { }))
        frame->m_opener = nullptr;
}

bool Frame::hasOpenedFrames() const
{
    return !m_openedFrames.isEmptyIgnoringNullReferences();
}

void Frame::setOwnerElement(HTMLFrameOwnerElement* element)
{
    m_ownerElement = element;
    if (element) {
        element->clearContentFrame();
        element->setContentFrame(*this);
    }
    updateScrollingMode();
}

void Frame::setOwnerPermissionsPolicy(OwnerPermissionsPolicyData&& ownerPermissionsPolicy)
{
    m_ownerPermisssionsPolicyOverride = makeUnique<OwnerPermissionsPolicyData>(WTFMove(ownerPermissionsPolicy));
}

std::optional<OwnerPermissionsPolicyData> Frame::ownerPermissionsPolicy() const
{
    if (m_ownerPermisssionsPolicyOverride)
        return *m_ownerPermisssionsPolicyOverride;

    RefPtr owner = ownerElement();
    if (!owner)
        return std::nullopt;

    auto documentOrigin = owner->protectedDocument()->securityOrigin().data();
    auto documentPolicy = owner->protectedDocument()->permissionsPolicy();

    RefPtr iframe = dynamicDowncast<HTMLIFrameElement>(owner);
    auto containerPolicy = iframe ? PermissionsPolicy::processPermissionsPolicyAttribute(*iframe) : PermissionsPolicy::PolicyDirective { };
    return OwnerPermissionsPolicyData { WTFMove(documentOrigin), WTFMove(documentPolicy), WTFMove(containerPolicy) };
}

void Frame::updateSandboxFlags(SandboxFlags flags, NotifyUIProcess notifyUIProcess)
{
    if (notifyUIProcess == NotifyUIProcess::Yes)
        loaderClient().updateSandboxFlags(flags);
}

void Frame::stopForBackForwardCache()
{
    if (RefPtr localFrame = dynamicDowncast<LocalFrame>(*this))
        localFrame->loader().stopForBackForwardCache();
    else {
        for (RefPtr child = tree().firstChild(); child; child = child->tree().nextSibling())
            child->stopForBackForwardCache();
    }
}

void Frame::updateFrameTreeSyncData(Ref<FrameTreeSyncData>&& data)
{
    m_frameTreeSyncData = WTFMove(data);
}

void Frame::updateFrameTreeSyncData(const FrameTreeSyncSerializationData& data)
{
    protectedFrameTreeSyncData()->update(data);
}

bool Frame::frameCanCreatePaymentSession() const
{
    // Prefer the LocalFrame code path when site isolation is disabled.
    ASSERT(m_settings->siteIsolationEnabled());
    return m_frameTreeSyncData->frameCanCreatePaymentSession;
}

bool Frame::isPrinting() const
{
    return m_isPrinting;
}

void Frame::setPrinting(bool printing, FloatSize pageSize, FloatSize originalPageSize, float maximumShrinkRatio, AdjustViewSize shouldAdjustViewSize, NotifyUIProcess notifyUIProcess)
{
    m_isPrinting = printing;
    if (notifyUIProcess == NotifyUIProcess::Yes && m_settings->siteIsolationEnabled())
        loaderClient().setPrinting(printing, pageSize, originalPageSize, maximumShrinkRatio, shouldAdjustViewSize);
}

} // namespace WebCore
