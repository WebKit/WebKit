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

#include "DrawingAreaProxy.h"
#include "Logging.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <wtf/DebugUtilities.h>
#include <wtf/HexNumber.h>
#include <wtf/URL.h>

namespace WebKit {
using namespace WebCore;

static const Seconds suspensionTimeout { 10_s };

#if !LOG_DISABLED
static const HashSet<IPC::StringReference>& messageNamesToIgnoreWhileSuspended()
{
    static NeverDestroyed<HashSet<IPC::StringReference>> messageNames;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        messageNames.get().add("BackForwardAddItem");
        messageNames.get().add("ClearAllEditCommands");
        messageNames.get().add("DidChangeContentSize");
        messageNames.get().add("DidChangeMainDocument");
        messageNames.get().add("DidChangeProgress");
        messageNames.get().add("DidCommitLoadForFrame");
        messageNames.get().add("DidDestroyNavigation");
        messageNames.get().add("DidFinishDocumentLoadForFrame");
        messageNames.get().add("DidFinishProgress");
        messageNames.get().add("DidCompletePageTransition");
        messageNames.get().add("DidFirstLayoutForFrame");
        messageNames.get().add("DidFirstVisuallyNonEmptyLayoutForFrame");
        messageNames.get().add("DidNavigateWithNavigationData");
        messageNames.get().add("DidReachLayoutMilestone");
        messageNames.get().add("DidRestoreScrollPosition");
        messageNames.get().add("DidSaveToPageCache");
        messageNames.get().add("DidStartProgress");
        messageNames.get().add("DidStartProvisionalLoadForFrame");
        messageNames.get().add("EditorStateChanged");
        messageNames.get().add("PageExtendedBackgroundColorDidChange");
        messageNames.get().add("SetRenderTreeSize");
        messageNames.get().add("SetStatusText");
        messageNames.get().add("SetNetworkRequestsInProgress");
    });

    return messageNames;
}
#endif

SuspendedPageProxy::SuspendedPageProxy(WebPageProxy& page, Ref<WebProcessProxy>&& process, WebBackForwardListItem& item, uint64_t mainFrameID)
    : m_page(page)
    , m_process(WTFMove(process))
    , m_mainFrameID(mainFrameID)
    , m_registrableDomain(toRegistrableDomain(URL(URL(), item.url())))
    , m_suspensionTimeoutTimer(RunLoop::main(), this, &SuspendedPageProxy::suspensionTimedOut)
#if PLATFORM(IOS_FAMILY)
    , m_suspensionToken(m_process->throttler().backgroundActivityToken())
#endif
{
    item.setSuspendedPage(this);
    m_process->incrementSuspendedPageCount();
    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_page.pageID(), *this);

    m_suspensionTimeoutTimer.startOneShot(suspensionTimeout);
    m_process->send(Messages::WebPage::SetIsSuspended(true), m_page.pageID());
}

SuspendedPageProxy::~SuspendedPageProxy()
{
    m_process->decrementSuspendedPageCount();

    if (m_readyToUnsuspendHandler) {
        RunLoop::main().dispatch([readyToUnsuspendHandler = WTFMove(m_readyToUnsuspendHandler)]() mutable {
            readyToUnsuspendHandler(nullptr);
        });
    }

    if (m_suspensionState == SuspensionState::Resumed)
        return;

    // If the suspended page was not consumed before getting destroyed, then close the corresponding page
    // on the WebProcess side.
    close();

    if (m_suspensionState == SuspensionState::Suspending)
        m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_page.pageID());

    // We call maybeShutDown() asynchronously since the SuspendedPage is currently being removed from the WebProcessPool
    // and we want to avoid re-entering WebProcessPool methods.
    RunLoop::main().dispatch([process = m_process.copyRef()] {
        process->maybeShutDown();
    });
}

void SuspendedPageProxy::waitUntilReadyToUnsuspend(CompletionHandler<void(SuspendedPageProxy*)>&& completionHandler)
{
    if (m_readyToUnsuspendHandler)
        m_readyToUnsuspendHandler(nullptr);

    switch (m_suspensionState) {
    case SuspensionState::Suspending:
        m_readyToUnsuspendHandler = WTFMove(completionHandler);
        break;
    case SuspensionState::FailedToSuspend:
    case SuspensionState::Suspended:
        completionHandler(this);
        break;
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
    m_process->send(Messages::WebPage::SetIsSuspended(false), m_page.pageID());
}

void SuspendedPageProxy::close()
{
    ASSERT(m_suspensionState != SuspensionState::Resumed);

    if (m_isClosed)
        return;

    m_isClosed = true;
    m_process->send(Messages::WebPage::Close(), m_page.pageID());
}

void SuspendedPageProxy::didProcessRequestToSuspend(SuspensionState newSuspensionState)
{
    LOG(ProcessSwapping, "SuspendedPageProxy %s from process %i finished transition to suspended", loggingString(), m_process->processIdentifier());

    ASSERT(m_suspensionState == SuspensionState::Suspending);
    ASSERT(newSuspensionState == SuspensionState::Suspended || newSuspensionState == SuspensionState::FailedToSuspend);

    m_suspensionState = newSuspensionState;

    m_suspensionTimeoutTimer.stop();

#if PLATFORM(IOS_FAMILY)
    m_suspensionToken = nullptr;
#endif

    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_page.pageID());

    bool shouldDelayClosingOnFailure = false;
#if PLATFORM(MAC)
    // With web process side tiles, we need to keep the suspended page around on failure to avoid flashing.
    // It is removed by WebPageProxy::enterAcceleratedCompositingMode when the target page is ready.
    shouldDelayClosingOnFailure = m_page.drawingArea() && m_page.drawingArea()->type() == DrawingAreaTypeTiledCoreAnimation;
#endif
    if (m_suspensionState == SuspensionState::FailedToSuspend && !shouldDelayClosingOnFailure)
        close();

    if (m_readyToUnsuspendHandler)
        m_readyToUnsuspendHandler(this);
}

void SuspendedPageProxy::suspensionTimedOut()
{
    RELEASE_LOG_ERROR(ProcessSwapping, "%p - SuspendedPageProxy::suspensionTimedOut() destroying the suspended page because it failed to suspend in time", this);
    m_process->processPool().removeSuspendedPage(*this); // Will destroy |this|.
}

void SuspendedPageProxy::didReceiveMessage(IPC::Connection&, IPC::Decoder& decoder)
{
    ASSERT(decoder.messageReceiverName() == Messages::WebPageProxy::messageReceiverName());

    if (decoder.messageName() == Messages::WebPageProxy::DidSuspendAfterProcessSwap::name()) {
        didProcessRequestToSuspend(SuspensionState::Suspended);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidFailToSuspendAfterProcessSwap::name()) {
        didProcessRequestToSuspend(SuspensionState::FailedToSuspend);
        return;
    }

#if !LOG_DISABLED
    if (!messageNamesToIgnoreWhileSuspended().contains(decoder.messageName()))
        LOG(ProcessSwapping, "SuspendedPageProxy received unexpected WebPageProxy message '%s'", decoder.messageName().toString().data());
#endif
}

void SuspendedPageProxy::didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&)
{
}

#if !LOG_DISABLED

const char* SuspendedPageProxy::loggingString() const
{
    return debugString("(0x", hex(reinterpret_cast<uintptr_t>(this)), " page ID ", m_page.pageID(), ", m_suspensionState ", static_cast<unsigned>(m_suspensionState), ')');
}

#endif

} // namespace WebKit
