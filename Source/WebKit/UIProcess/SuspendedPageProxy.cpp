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
#include "SuspendedPageProxy.h"

#include "APIPageConfiguration.h"
#include "BrowsingContextGroup.h"
#include "DrawingAreaProxy.h"
#include "EnhancedSecurity.h"
#include "HandleMessage.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "RemotePageProxy.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListMessages.h"
#include "WebFrameProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include <wtf/CallbackAggregator.h>
#include <wtf/DebugUtilities.h>
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

static const Seconds suspensionTimeout { 10_s };

static WeakHashSet<SuspendedPageProxy>& NODELETE allSuspendedPages()
{
    static NeverDestroyed<WeakHashSet<SuspendedPageProxy>> map;
    return map;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(SuspendedPageProxy);

RefPtr<WebProcessProxy> SuspendedPageProxy::findReusableSuspendedPageProcess(WebProcessPool& processPool, const RegistrableDomain& registrableDomain, WebsiteDataStore& dataStore, WebProcessProxy::LockdownMode lockdownMode, EnhancedSecurity enhancedSecurity, const API::PageConfiguration& pageConfiguration)
{
    for (Ref suspendedPage : allSuspendedPages()) {
        if (!suspendedPage->hasSuspensionStarted())
            continue;
        Ref process = suspendedPage->process();
        if (&process->processPool() == &processPool
            && process->site() && process->site()->domain() == registrableDomain
            && process->websiteDataStore() == &dataStore
            && process->crossOriginMode() != CrossOriginMode::Isolated
            && process->lockdownMode() == lockdownMode
            && process->enhancedSecurity() == enhancedSecurity
            && !process->wasTerminated()
            && process->hasSameGPUAndNetworkProcessPreferencesAs(pageConfiguration)) {
            return process;
        }
    }
    return nullptr;
}

#if !LOG_DISABLED
using MessageNameSet = HashSet<IPC::MessageName, WTF::IntHash<IPC::MessageName>, WTF::StrongEnumHashTraits<IPC::MessageName>>;
static const MessageNameSet& messageNamesToIgnoreWhileSuspended()
{
    static NeverDestroyed<MessageNameSet> messageNames = [] {
        MessageNameSet messageNames;
        messageNames.add(IPC::MessageName::WebBackForwardList_BackForwardAddItem);
        messageNames.add(IPC::MessageName::WebPageProxy_ClearAllEditCommands);
        messageNames.add(IPC::MessageName::WebPageProxy_DidChangeContentSize);
        messageNames.add(IPC::MessageName::WebPageProxy_DidChangeMainDocument);
        messageNames.add(IPC::MessageName::WebPageProxy_DidChangeProgress);
        messageNames.add(IPC::MessageName::WebPageProxy_DidCommitLoadForFrame);
        messageNames.add(IPC::MessageName::WebPageProxy_DidFinishDocumentLoadForFrame);
        messageNames.add(IPC::MessageName::WebPageProxy_DidFinishProgress);
        messageNames.add(IPC::MessageName::WebPageProxy_DidFirstLayoutForFrame);
        messageNames.add(IPC::MessageName::WebPageProxy_DidFirstVisuallyNonEmptyLayoutForFrame);
        messageNames.add(IPC::MessageName::WebPageProxy_DidNavigateWithNavigationData);
        messageNames.add(IPC::MessageName::WebPageProxy_DidReachLayoutMilestone);
        messageNames.add(IPC::MessageName::WebPageProxy_DidRestoreScrollPosition);
        messageNames.add(IPC::MessageName::WebPageProxy_DidStartProgress);
        messageNames.add(IPC::MessageName::WebPageProxy_DidStartProvisionalLoadForFrame);
        messageNames.add(IPC::MessageName::WebPageProxy_EditorStateChanged);
        messageNames.add(IPC::MessageName::WebPageProxy_PageExtendedBackgroundColorDidChange);
        messageNames.add(IPC::MessageName::WebPageProxy_SetRenderTreeSize);
        messageNames.add(IPC::MessageName::WebPageProxy_SetNetworkRequestsInProgress);
        return messageNames;
    }();

    return messageNames;
}
#endif

Ref<SuspendedPageProxy> SuspendedPageProxy::create(WebPageProxy& page, Ref<WebProcessProxy>&& process, Ref<WebFrameProxy>&& mainFrame, Ref<BrowsingContextGroup>&& browsingContextGroup, ShouldDelayClosingUntilFirstLayerFlush shouldDelayClosingUntilFirstLayerFlush)
{
    return adoptRef(*new SuspendedPageProxy(page, WTF::move(process), WTF::move(mainFrame), WTF::move(browsingContextGroup), shouldDelayClosingUntilFirstLayerFlush));
}

SuspendedPageProxy::SuspendedPageProxy(WebPageProxy& page, Ref<WebProcessProxy>&& process, Ref<WebFrameProxy>&& mainFrame, Ref<BrowsingContextGroup>&& browsingContextGroup, ShouldDelayClosingUntilFirstLayerFlush shouldDelayClosingUntilFirstLayerFlush)
    : m_page(page)
    , m_webPageID(page.webPageIDInMainFrameProcess())
    , m_process(WTF::move(process))
    , m_mainFrame(WTF::move(mainFrame))
    , m_browsingContextGroup(WTF::move(browsingContextGroup))
    , m_shouldDelayClosingUntilFirstLayerFlush(shouldDelayClosingUntilFirstLayerFlush)
    , m_suspensionTimeoutTimer(RunLoop::mainSingleton(), "SuspendedPageProxy::SuspensionTimeoutTimer"_s, this, &SuspendedPageProxy::suspensionTimedOut)
#if USE(RUNNINGBOARD)
    , m_suspensionActivity(protect(m_process->throttler())->backgroundActivity("Page suspension for back/forward cache"_s))
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    , m_contextIDForVisibilityPropagationInWebProcess(page.contextIDForVisibilityPropagationInWebProcess())
#if ENABLE(GPU_PROCESS)
    , m_contextIDForVisibilityPropagationInGPUProcess(page.contextIDForVisibilityPropagationInGPUProcess())
#endif
#endif
{
    allSuspendedPages().add(*this);
}

template<typename M>
void SuspendedPageProxy::send(M&& message)
{
    m_process->send(std::forward<M>(message), m_webPageID);
}

template<typename M, typename C>
void SuspendedPageProxy::sendWithAsyncReply(M&& message, C&& completionHandler)
{
    m_process->sendWithAsyncReply(std::forward<M>(message), std::forward<C>(completionHandler), m_webPageID);
}

void SuspendedPageProxy::startSuspension(std::optional<BackForwardFrameItemIdentifier> mainFrameItemID)
{
    ASSERT(m_suspensionState == SuspensionState::BeforeStart);
    ASSERT(!m_browsingContextGroup->hasMultiplePages());

    m_process->addSuspendedPageProxy(*this);
    m_suspensionState = SuspensionState::Suspending;

    if (mainFrameItemID)
        suspendSubframeProcesses(*mainFrameItemID);
    else
        m_allSubframesSuspended = true;

    m_messageReceiverRegistration.startReceivingMessages(m_process, m_webPageID, *this, *this);
    m_suspensionTimeoutTimer.startOneShot(suspensionTimeout);
    sendWithAsyncReply(Messages::WebPage::SetIsSuspended(true), [weakThis = WeakPtr { *this }](std::optional<bool> didSuspend) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !didSuspend)
            return;
        protectedThis->didProcessRequestToSuspend(*didSuspend ? SuspensionState::Suspended : SuspensionState::FailedToSuspend);
    });
}

SuspendedPageProxy::~SuspendedPageProxy()
{
    if (auto handler = std::exchange(m_readyToUnsuspendHandler, nullptr)) {
        RunLoop::mainSingleton().dispatch([handler = WTF::move(handler)]() mutable {
            handler(nullptr);
        });
    }
    teardown();
}

void SuspendedPageProxy::teardown()
{
    allSuspendedPages().remove(*this);

    if (RefPtr page = m_page.get()) {
        if (hasSuspensionStarted()) {
            m_browsingContextGroup->forEachRemotePage(*page, [suspendedPage = Ref { *this }](auto& remotePage) {
                protect(remotePage.siteIsolatedProcess())->removeSuspendedPageProxy(suspendedPage);
            });
        }
        if (m_suspensionState != SuspensionState::Resumed)
            m_browsingContextGroup->closeRemotePagesForPage(*page);
    }

    if (m_suspensionState != SuspensionState::Resumed)
        close();

    if (hasSuspensionStarted())
        m_process->removeSuspendedPageProxy(*this);
}

void SuspendedPageProxy::didDestroyNavigation(WebCore::NavigationIdentifier navigationID)
{
    if (RefPtr page = m_page.get())
        page->didDestroyNavigationShared(m_process.copyRef(), navigationID);
}

WebBackForwardCache& SuspendedPageProxy::backForwardCache() const
{
    return process().processPool().backForwardCache();
}

void SuspendedPageProxy::waitUntilReadyToUnsuspend(CompletionHandler<void(SuspendedPageProxy*)>&& completionHandler)
{
    if (m_readyToUnsuspendHandler)
        m_readyToUnsuspendHandler(nullptr);

    switch (m_suspensionState) {
    case SuspensionState::Suspending:
        m_readyToUnsuspendHandler = WTF::move(completionHandler);
        break;
    case SuspensionState::FailedToSuspend:
    case SuspensionState::Suspended:
        completionHandler(this);
        break;
    case SuspensionState::BeforeStart:
    case SuspensionState::Resumed:
        ASSERT_NOT_REACHED();
        completionHandler(nullptr);
        break;
    }
}

void SuspendedPageProxy::unsuspend()
{
    ASSERT(m_suspensionState == SuspensionState::Suspended);

    m_suspensionState = SuspensionState::Resumed;
    sendWithAsyncReply(Messages::WebPage::SetIsSuspended(false), [](std::optional<bool> didSuspend) {
        ASSERT(!didSuspend.has_value());
    });
}

void SuspendedPageProxy::close()
{
    ASSERT(m_suspensionState != SuspensionState::Resumed);

    if (m_isClosed)
        return;

    RELEASE_LOG(ProcessSwapping, "%p - SuspendedPageProxy::close()", this);
    m_isClosed = true;
    send(Messages::WebPage::Close());
}

void SuspendedPageProxy::pageDidFirstLayerFlush()
{
    m_shouldDelayClosingUntilFirstLayerFlush = ShouldDelayClosingUntilFirstLayerFlush::No;

    if (m_shouldCloseWhenEnteringAcceleratedCompositingMode) {
        // We needed the suspended page to stay alive to avoid flashing. Now we can get rid of it.
        close();
    }
}

bool SuspendedPageProxy::pageIsClosedOrClosing() const
{
    return m_isClosed || m_shouldCloseWhenEnteringAcceleratedCompositingMode;
}

void SuspendedPageProxy::closeWithoutFlashing()
{
    RELEASE_LOG(ProcessSwapping, "%p - SuspendedPageProxy::closeWithoutFlashing() shouldDelayClosingUntilFirstLayerFlush? %d", this, m_shouldDelayClosingUntilFirstLayerFlush == ShouldDelayClosingUntilFirstLayerFlush::Yes);
    if (m_shouldDelayClosingUntilFirstLayerFlush == ShouldDelayClosingUntilFirstLayerFlush::Yes) {
        m_shouldCloseWhenEnteringAcceleratedCompositingMode = true;
        return;
    }
    close();
}

void SuspendedPageProxy::didProcessRequestToSuspend(SuspensionState newSuspensionState)
{
    LOG(ProcessSwapping, "SuspendedPageProxy %s from process %i finished transition to suspended", loggingString().utf8().data(), m_process->processID());
    RELEASE_LOG(ProcessSwapping, "%p - SuspendedPageProxy::didProcessRequestToSuspend() success? %d", this, newSuspensionState == SuspensionState::Suspended);

    ASSERT(newSuspensionState == SuspensionState::Suspended || newSuspensionState == SuspensionState::FailedToSuspend);

    if (m_suspensionState != SuspensionState::Suspending)
        return;

#if USE(RUNNINGBOARD)
    m_suspensionActivity = nullptr;
#endif

    m_messageReceiverRegistration.stopReceivingMessages();

    if (newSuspensionState == SuspensionState::FailedToSuspend) {
        m_suspensionState = SuspensionState::FailedToSuspend;
        m_suspensionTimeoutTimer.stop();
        closeWithoutFlashing();
        if (auto handler = std::exchange(m_readyToUnsuspendHandler, nullptr))
            handler(this);
        return;
    }

    m_mainFrameSuspended = true;
    maybeCompleteSuspension();
}

void SuspendedPageProxy::suspensionTimedOut()
{
    if (m_suspensionState != SuspensionState::Suspending)
        return;

    RELEASE_LOG_ERROR(ProcessSwapping, "%p - SuspendedPageProxy::suspensionTimedOut() destroying the suspended page because it failed to suspend in time", this);
    protect(backForwardCache())->removeEntry(*this); // Will destroy |this|.
}

void SuspendedPageProxy::suspendSubframeProcesses(BackForwardFrameItemIdentifier mainFrameItemID)
{
    ASSERT(!m_allSubframesSuspended);
    ASSERT(!m_readyToUnsuspendHandler);

    RefPtr page = m_page.get();
    if (!page) {
        RELEASE_LOG_ERROR(ProcessSwapping, "%p - SuspendedPageProxy::suspendSubframeProcesses: page is null", this);
        m_allSubframesSuspended = true;
        return;
    }

    // The aggregator collects success/failure from all subframe processes.
    // Its destructor fires the callback when all chain() handlers have been
    // called (or destroyed). On failure, we remove the WebBackForwardCache
    // entry, which is the sole owner of this SuspendedPageProxy — dropping
    // it triggers our destructor -> teardown() -> process cleanup.
    auto aggregator = MainRunLoopSuccessCallbackAggregator::create([weakThis = WeakPtr { *this }](bool success) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || protectedThis->m_suspensionState != SuspensionState::Suspending)
            return;

        if (!success) {
            RELEASE_LOG_ERROR(ProcessSwapping, "%p - SuspendedPageProxy::suspendSubframeProcesses: failed, invalidating cache entry", protectedThis.get());
            // removeEntry drops the WebBackForwardCache's Ref to this
            // SuspendedPageProxy. If no other Ref exists, this triggers
            // destruction -> teardown() -> subframe process cleanup.
            protect(protectedThis->backForwardCache())->removeEntry(*protectedThis);
            return;
        }
        protectedThis->m_allSubframesSuspended = true;
        protectedThis->maybeCompleteSuspension();
    });

    m_browsingContextGroup->forEachRemotePage(*page, [suspendedPage = Ref { *this }, &aggregator, mainFrameItemID](auto& remotePage) {
        Ref process = remotePage.siteIsolatedProcess();
        process->addSuspendedPageProxy(suspendedPage);

        RELEASE_LOG(ProcessSwapping, "%p - SuspendedPageProxy::suspendSubframeProcesses: Sending SetSubframesSuspended to pid %i", &suspendedPage, process->processID());

        process->sendWithAsyncReply(Messages::WebPage::SetSubframesSuspended(true, mainFrameItemID), aggregator->chain(), remotePage.identifierInSiteIsolatedProcess());
    });
}

bool SuspendedPageProxy::hasSubframeInProcess(WebCore::ProcessIdentifier processIdentifier) const
{
    // FIXME: Add WebFrameProxy::forEachDescendant() to avoid manual traverseNext() loops.
    for (RefPtr frame = m_mainFrame->traverseNext().frame; frame; frame = frame->traverseNext().frame) {
        if (protect(frame->process())->coreProcessIdentifier() == processIdentifier)
            return true;
    }
    return false;
}

void SuspendedPageProxy::maybeCompleteSuspension()
{
    if (m_suspensionState != SuspensionState::Suspending)
        return;

    if (!m_mainFrameSuspended || !m_allSubframesSuspended)
        return;

    m_suspensionTimeoutTimer.stop();
    m_suspensionState = SuspensionState::Suspended;

    if (auto handler = std::exchange(m_readyToUnsuspendHandler, nullptr))
        handler(this);
}

WebPageProxy* SuspendedPageProxy::page() const
{
    return m_page.get();
}

void SuspendedPageProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT(decoder.messageReceiverName() == Messages::WebPageProxy::messageReceiverName() || decoder.messageReceiverName() == Messages::WebBackForwardList::messageReceiverName());

    if (decoder.messageName() == Messages::WebPageProxy::DidDestroyNavigation::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidDestroyNavigation>(connection, decoder, this, &SuspendedPageProxy::didDestroyNavigation);
        return;
    }

#if !LOG_DISABLED
    if (!messageNamesToIgnoreWhileSuspended().contains(decoder.messageName()))
        LOG(ProcessSwapping, "SuspendedPageProxy received unexpected WebPageProxy message '%s'", description(decoder.messageName()).characters());
#endif
}

void SuspendedPageProxy::didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&)
{
}

#if !LOG_DISABLED

String SuspendedPageProxy::loggingString() const
{
    return makeString("(0x"_s, hex(reinterpret_cast<uintptr_t>(this)), " WebPage ID "_s, m_webPageID.toUInt64(), ", m_suspensionState "_s, static_cast<unsigned>(m_suspensionState), ')');
}

#endif

} // namespace WebKit
