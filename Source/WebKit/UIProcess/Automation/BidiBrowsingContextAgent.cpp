/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Microsoft Corporation. All rights reserved.
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
#include "BidiBrowsingContextAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "APIPageConfiguration.h"
#include "AutomationProtocolObjects.h"
#include "BrowsingContextGroup.h"
#include "FrameInfoData.h"
#include "FrameTreeNodeData.h"
#include "Logging.h"
#include "PageLoadState.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiFrontendDispatchers.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"

namespace WebKit {

using namespace Inspector;
using BrowsingContext = Inspector::Protocol::BidiBrowsingContext::BrowsingContext;
using ReadinessState = Inspector::Protocol::BidiBrowsingContext::ReadinessState;
using PageLoadStrategy = Inspector::Protocol::Automation::PageLoadStrategy;
using UserPromptType = Inspector::Protocol::BidiBrowsingContext::UserPromptType;
using UserPromptHandlerType = Inspector::Protocol::BidiSession::UserPromptHandlerType;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiBrowsingContextAgent);

BidiBrowsingContextAgent::BidiBrowsingContextAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_browsingContextDomainDispatcher(BidiBrowsingContextBackendDispatcher::create(backendDispatcher, this))
{
}

BidiBrowsingContextAgent::~BidiBrowsingContextAgent() = default;

void BidiBrowsingContextAgent::activate(const BrowsingContext& browsingContext, CommandCallback<void>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    RefPtr webPageProxy = session->webPageProxyForHandle(browsingContext);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!webPageProxy, WindowNotFound);

    // FIXME: detect non-top level browsing contexts, returning `invalid argument`.
    session->switchToBrowsingContext(browsingContext, emptyString(), [callback = WTFMove(callback)](CommandResult<void>&& result) {
        if (!result) {
            callback(makeUnexpected(result.error()));
            return;
        }

        callback({ });
    });
}

void BidiBrowsingContextAgent::close(const BrowsingContext& browsingContext, std::optional<bool>&& optionalPromptUnload, CommandCallback<void>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: implement `promptUnload` option.
    // FIXME: raise `invalid argument` if `browsingContext` is not a top-level traversable.

    session->closeBrowsingContext(browsingContext);

    callback({ });
}

static constexpr Inspector::Protocol::Automation::BrowsingContextPresentation defaultBrowsingContextPresentation = Inspector::Protocol::Automation::BrowsingContextPresentation::Tab;

static Inspector::Protocol::Automation::BrowsingContextPresentation browsingContextPresentationFromCreateType(Inspector::Protocol::BidiBrowsingContext::CreateType createType)
{
    switch (createType) {
    case Inspector::Protocol::BidiBrowsingContext::CreateType::Tab:
        return Inspector::Protocol::Automation::BrowsingContextPresentation::Tab;
    case Inspector::Protocol::BidiBrowsingContext::CreateType::Window:
        return Inspector::Protocol::Automation::BrowsingContextPresentation::Window;
    }

    ASSERT_NOT_REACHED();
    return defaultBrowsingContextPresentation;
}

void BidiBrowsingContextAgent::create(Inspector::Protocol::BidiBrowsingContext::CreateType createType, const BrowsingContext& optionalReferenceContext, std::optional<bool>&& optionalBackground, const String& optionalUserContext, CommandCallback<BrowsingContext>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: implement `referenceContext` option.
    // FIXME: implement `background` option.
    // FIXME: implement `userContext` option.

    session->createBrowsingContext(browsingContextPresentationFromCreateType(createType), [callback = WTFMove(callback)](CommandResultOf<BrowsingContext, Inspector::Protocol::Automation::BrowsingContextPresentation>&& result) {
        if (!result) {
            callback(makeUnexpected(result.error()));
            return;
        }

        auto [resultContext, resultPresentation] = WTFMove(result.value());
        callback(WTFMove(resultContext));
    });
}

class PageTreeAggregator : public RefCounted<PageTreeAggregator> {
public:
    static Ref<PageTreeAggregator> create(CommandCallback<Ref<JSON::ArrayOf<Protocol::BidiBrowsingContext::Info>>>&& completionHandler, WebAutomationSession& session, size_t expectedTrees, std::optional<double> maxDepth, std::optional<BrowsingContext> optionalRoot)
    {
        return adoptRef(*new PageTreeAggregator(WTFMove(completionHandler), session, expectedTrees, WTFMove(maxDepth), WTFMove(optionalRoot)));
    }

    void addTree(BrowsingContext rootHandle, std::optional<FrameTreeNodeData>&& data)
    {
        m_results.set(rootHandle, WTFMove(data));
        checkIfComplete();
    }

    void noteEmpty(BrowsingContext rootHandle)
    {
        m_results.set(rootHandle, std::nullopt);
        checkIfComplete();
    }

private:
    PageTreeAggregator(CommandCallback<Ref<JSON::ArrayOf<Protocol::BidiBrowsingContext::Info>>>&& completionHandler, WebAutomationSession& session, size_t expectedTrees, std::optional<double> maxDepth, std::optional<BrowsingContext> optionalRoot)
        : m_completionHandler(WTFMove(completionHandler)), m_session(session), m_expectedTrees(expectedTrees), m_maxDepth(WTFMove(maxDepth)), m_optionalRoot(WTFMove(optionalRoot))
    {
        if (m_expectedTrees <= 0)
            checkIfComplete();
    }

    std::optional<Ref<Protocol::BidiBrowsingContext::Info>> convertFrameTreeNodeDataToBidiInfo(const FrameTreeNodeData& node, int currentDepth)
    {

        auto contextHandle = m_session->handleForWebFrameID(node.info.frameID);
        if (contextHandle.isEmpty())
            return std::nullopt;

        RefPtr<WebPageProxy> page = m_session->webPageProxyForHandle(contextHandle);
        if (!page)
            return std::nullopt;

        auto info = Protocol::BidiBrowsingContext::Info::create()
            .setContext(contextHandle)
            .setUrl(node.info.request.url().string())
            .setUserContext("default"_s)
            .setClientWindow("defaut"_s)
            .release();

        if (page->mainFrame()->frameID() == node.info.frameID) {
            if (auto openerInfo = page->configuration().openerInfo()) {
                auto openerHandle = m_session->handleForWebFrameID(openerInfo->frameID);
                if (!openerHandle.isEmpty())
                    info->setOriginalOpener(openerHandle);
            }
        }

        if (node.info.parentFrameID) {
            auto parentContextHandle = m_session->handleForWebFrameID(node.info.parentFrameID);
            info->setParent(parentContextHandle);
        }

        bool atMaxDepth = m_maxDepth.has_value() && currentDepth >= m_maxDepth.value();
        if (!atMaxDepth) {
            auto childrenInfo = JSON::ArrayOf<Protocol::BidiBrowsingContext::Info>::create();
            for (const auto& childNode : node.children) {
                if (auto childInfo = convertFrameTreeNodeDataToBidiInfo(childNode, currentDepth + 1))
                    childrenInfo->addItem(WTFMove(childInfo.value()));
            }

            if (childrenInfo->length() > 0)
                info->setChildren(WTFMove(childrenInfo));
        }

        return info;
    }

    void checkIfComplete()
    {
        if (m_results.size() < m_expectedTrees)
            return;

        auto finalArray = JSON::ArrayOf<Protocol::BidiBrowsingContext::Info>::create();
        for (auto& entry : m_results) {
            const auto& rootHandle = entry.key;
            auto& rootData = entry.value;

            if (rootData) {
                if (auto rootInfo = convertFrameTreeNodeDataToBidiInfo(*rootData, 0)) {
                    if (m_optionalRoot.has_value() && m_optionalRoot.value() == rootHandle) {
                        if (rootData->info.parentFrameID) {
                            auto parentContextHandle = m_session->handleForWebFrameID(rootData->info.parentFrameID);
                            if (!parentContextHandle.isEmpty())
                                rootInfo.value()->setParent(parentContextHandle);
                        }
                    }
                    finalArray->addItem(rootInfo.value());
                }
            }
        }
        m_completionHandler({ { WTFMove(finalArray) } });
    }

    CommandCallback<Ref<JSON::ArrayOf<Protocol::BidiBrowsingContext::Info>>> m_completionHandler;
    Ref<WebAutomationSession> m_session;
    size_t m_expectedTrees { 0 };
    std::optional<double> m_maxDepth;
    std::optional<BrowsingContext> m_optionalRoot;
    HashMap<BrowsingContext, std::optional<FrameTreeNodeData>> m_results;
};

void BidiBrowsingContextAgent::getTree(const BrowsingContext& optionalRoot, std::optional<double>&& optionalMaxDepth, CommandCallback<Ref<JSON::ArrayOf<Protocol::BidiBrowsingContext::Info>>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    Vector<RefPtr<WebFrameProxy>> navigableRoots;

    if (!optionalRoot.isEmpty()) {
        auto page = session->webPageProxyForHandle(optionalRoot);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, InternalError);

        RefPtr<WebFrameProxy> rootFrame = page->mainFrame();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!rootFrame, InternalError);

        navigableRoots.append(rootFrame);

    } else {
        for (auto& process : m_session->protectedProcessPool()->processes()) {
            for (auto& page : process->pages()) {
                if (!page->isControlledByAutomation())
                    continue;
                navigableRoots.append(page->mainFrame());
            }
        }
    }

    if (navigableRoots.isEmpty()) {
        callback({ { JSON::ArrayOf<Protocol::BidiBrowsingContext::Info>::create() } });
        return;
    }

    Ref aggregator = PageTreeAggregator::create(WTFMove(callback), *session, navigableRoots.size(), WTFMove(optionalMaxDepth), optionalRoot);
    for (RefPtr rootFrame : navigableRoots) {
        if (!rootFrame) {
            continue;
        }
        auto rootContext = session->handleForWebPageProxy(*rootFrame->page());
        rootFrame->getFrameTree([aggregator, rootContext = WTFMove(rootContext)](std::optional<FrameTreeNodeData>&& data) mutable {
            if (data)
                aggregator->addTree(WTFMove(rootContext), WTFMove(data));
            else
                aggregator->noteEmpty(WTFMove(rootContext));
        });
    }

}

void BidiBrowsingContextAgent::handleUserPrompt(const BrowsingContext& browsingContext, std::optional<bool>&& optionalShouldAccept, const String&, CommandCallback<void>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: implement `userText` option.

    if (optionalShouldAccept && *optionalShouldAccept) {
        callback(session->acceptCurrentJavaScriptDialog(browsingContext));
        return;
    }

    // FIXME: this should consider the session's user prompt handler. <https://webkit.org/b/291666>
    callback(session->dismissCurrentJavaScriptDialog(browsingContext));
}


// https://www.w3.org/TR/webdriver/#dfn-session-page-load-timeout
static constexpr Seconds defaultPageLoadTimeout = 300_s;
static constexpr ReadinessState defaultReadinessState = ReadinessState::None;

static PageLoadStrategy pageLoadStrategyFromReadinessState(ReadinessState state)
{
    switch (state) {
    case ReadinessState::None:
        return PageLoadStrategy::None;
    case ReadinessState::Interactive:
        return PageLoadStrategy::Eager;
    case ReadinessState::Complete:
        return PageLoadStrategy::Normal;
    }

    ASSERT_NOT_REACHED();
    return PageLoadStrategy::Normal;
}

void BidiBrowsingContextAgent::navigate(const BrowsingContext& browsingContext, const String& url, std::optional<ReadinessState>&& optionalReadinessState, CommandCallbackOf<String, Inspector::Protocol::BidiBrowsingContext::Navigation>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    auto pageLoadStrategy = pageLoadStrategyFromReadinessState(optionalReadinessState.value_or(defaultReadinessState));
    session->navigateBrowsingContext(browsingContext, url, pageLoadStrategy, defaultPageLoadTimeout.milliseconds(), [url, callback = WTFMove(callback)](CommandResult<void>&& result) {
        if (!result) {
            callback(makeUnexpected(result.error()));
            return;
        }

        // FIXME: keep track of navigation IDs that we hand out.
        callback({ { url, "placeholder_navigation"_s } });
    });
}

void BidiBrowsingContextAgent::reload(const BrowsingContext& browsingContext, std::optional<bool>&& optionalIgnoreCache, std::optional<ReadinessState>&& optionalReadinessState, CommandCallbackOf<String, Inspector::Protocol::BidiBrowsingContext::Navigation>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: implement `ignoreCache` option.

    auto pageLoadStrategy = pageLoadStrategyFromReadinessState(optionalReadinessState.value_or(defaultReadinessState));
    session->reloadBrowsingContext(browsingContext, pageLoadStrategy, defaultPageLoadTimeout.milliseconds(), [session = WTFMove(session), browsingContext, callback = WTFMove(callback)](CommandResult<void>&& result) {
        if (!result) {
            callback(makeUnexpected(result.error()));
            return;
        }

        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

        RefPtr webPageProxy = session->webPageProxyForHandle(browsingContext);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!webPageProxy, WindowNotFound);

        // FIXME: keep track of navigation IDs that we hand out.
        callback({ { webPageProxy->currentURL(), "placeholder_navigation"_s } });
    });
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)

