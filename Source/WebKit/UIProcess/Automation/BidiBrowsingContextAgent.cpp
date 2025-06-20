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

#include "AutomationProtocolObjects.h"
#include "FrameTreeNodeData.h"
#include "Logging.h"
#include "PageLoadState.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <JavaScriptCore/MathCommon.h>
#include <wtf/Ref.h>
#include <wtf/Unexpected.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

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

    session->closeBrowsingContext(browsingContext, WTFMove(callback));
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

Protocol::BidiBrowsingContext::BrowsingContext BidiBrowsingContextAgent::getBrowsingContextID(const WebCore::FrameIdentifier& frameID) const
{
    RefPtr session = m_session.get();
    if (!session)
        return { };

    if (RefPtr frame = WebFrameProxy::webFrame(frameID); frame && frame->isMainFrame()) {
        if (RefPtr page = frame->page())
            return session->handleForWebPageProxy(*page);
        return WTF::emptyString();
    }
    return session->handleForWebFrameID(frameID);
}

Ref<Protocol::BidiBrowsingContext::Info> BidiBrowsingContextAgent::getNavigableInfo(const WebKit::FrameTreeNodeData& tree, std::optional<uint64_t> maxDepth, IncludeParentID includeParentID)
{
    // https://w3c.github.io/webdriver-bidi/#get-the-navigable-info

    // FIXME: Properly support different user contexts, which will likely map to different WebAutomationSessions.
    // https://bugs.webkit.org/show_bug.cgi?id=288104
    auto info = Inspector::Protocol::BidiBrowsingContext::Info::create()
        .setContext(getBrowsingContextID(tree.info.frameID))
        .setUrl(tree.info.request.url().string())
        .setClientWindow("placeholder_window"_s)
        .setUserContext("default"_s)
        .setChildrenIsNull()
        .setOriginalOpenerIsNull()
        .release();

    // FIXME: Support originalOpener attribute.
    // https://w3c.github.io/webdriver-bidi/#original-opener

    if (includeParentID == IncludeParentID::Yes) {
        if (tree.info.parentFrameID)
            info->setParent(getBrowsingContextID(tree.info.parentFrameID.value()));
        else
            info->setParentIsNull();
    }

    if (maxDepth && !*maxDepth) {
        info->setChildrenIsNull();
        return info;
    }

    auto childrenInfo = JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>::create();
    auto newDepth = maxDepth ? std::optional<uint64_t>(*maxDepth - 1) : std::nullopt;
    for (auto& child : tree.children)
        childrenInfo->addItem(getNavigableInfo(child, newDepth, IncludeParentID::No));

    info->setChildren(WTFMove(childrenInfo));
    return info;
}

// Recursively traverses the frame tree of the given pages, one page at a time.
// We need such recursion because we need to wait for the frame tree of the current page to be fully processed before moving on to the next page.
void BidiBrowsingContextAgent::getNextTree(Vector<Ref<WebPageProxy>>&& pagesToProcess, Ref<JSON::ArrayOf<Protocol::BidiBrowsingContext::Info>> resultsObject, std::optional<uint64_t> maxDepth, CommandCallback<Ref<JSON::ArrayOf<Protocol::BidiBrowsingContext::Info>>>&& callback)
{
    if (pagesToProcess.isEmpty()) {
        callback(WTFMove(resultsObject));
        return;
    }

    Ref webPageProxy = pagesToProcess.takeLast();
    webPageProxy->getAllFrameTrees([this, pagesToProcess = WTFMove(pagesToProcess), resultsObject = WTFMove(resultsObject), callback = WTFMove(callback), maxDepth, protectedPage = Ref { webPageProxy }](Vector<WebKit::FrameTreeNodeData>&& trees) mutable {
        for (auto& tree : trees) {
            auto infoTree = getNavigableInfo(tree, maxDepth, IncludeParentID::Yes);
            resultsObject->addItem(WTFMove(infoTree));
        }
        getNextTree(WTFMove(pagesToProcess), WTFMove(resultsObject), maxDepth, WTFMove(callback));
    });
}

void BidiBrowsingContextAgent::getTree(const BrowsingContext& optionalRoot, std::optional<double>&& optionalMaxDepth, CommandCallback<Ref<JSON::ArrayOf<Protocol::BidiBrowsingContext::Info>>>&& callback)
{
    // https://w3c.github.io/webdriver-bidi/#command-browsingContext-getTree
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    std::optional<uint64_t> maxDepth = std::nullopt;
    if (optionalMaxDepth) {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(*optionalMaxDepth < 0, InvalidParameter);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(std::floor(*optionalMaxDepth) != *optionalMaxDepth, InvalidParameter);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(*optionalMaxDepth > JSC::maxSafeInteger(), InvalidParameter);
        maxDepth = std::optional<uint64_t>(static_cast<uint64_t>(*optionalMaxDepth));
    }

    Vector<Ref<WebPageProxy>> pagesToProcess;

    for (Ref process : session->protectedProcessPool()->processes()) {
        for (Ref page : process->pages()) {
            if (!page->isControlledByAutomation())
                continue;

            if (!optionalRoot.isEmpty()) {
                if (session->handleForWebPageProxy(page) == optionalRoot) {
                    pagesToProcess.append(page);
                    break;
                }
            } else
                pagesToProcess.append(page);
        }
    }

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!optionalRoot.isEmpty() && pagesToProcess.isEmpty(), FrameNotFound);

    if (pagesToProcess.isEmpty()) {
        callback({ { JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>::create() } });
        return;
    }

    pagesToProcess.reverse();

    auto resultsObject = JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>::create();
    getNextTree(WTFMove(pagesToProcess), WTFMove(resultsObject), WTFMove(maxDepth), [callback = WTFMove(callback)](auto&& result) {
        callback({ { result.value() } });
    });
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

void BidiBrowsingContextAgent::locateNodes(const Inspector::Protocol::BidiBrowsingContext::BrowsingContext& context, Ref<JSON::Object>&& locatorObject, std::optional<double>&& optionalMaxNodeCount, RefPtr<JSON::Object>&& optionalSerializationOptionsObject, RefPtr<JSON::Array>&& optionalStartNodesArray, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiScript::RemoteValue>>>&& callback)
{
    RefPtr<WebAutomationSession> session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    auto page = session->webPageProxyForHandle(context);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    String locateNodeFunction = R"(
        (function (locator, contextNodes, maxNodeCount) {
            "use strict";

            if (!Array.isArray(contextNodes) || contextNodes.length === 0) {
                contextNodes = [document.documentElement];
            }

            function validateLimit(maxCount) {
                if (maxCount == null) return Infinity;
                if (typeof maxCount !== "number" || !Number.isInteger(maxCount) || maxCount < 1) {
                    throw { name: "InvalidArgument", message: "maxNodeCount must be an integer ≥ 1." };
                }
                return maxCount;
            }

            function findByCss(selector, roots, limit) {
                try {
                    document.createDocumentFragment().querySelector(selector);
                } catch (error) {
                    throw { name: "InvalidSelector", message: error.message };
                }
                const nodes = [];
                for (const root of roots) {
                    let matches;
                    try {
                        matches = root.querySelectorAll(selector);
                    } catch (error) {
                        throw { name: "InvalidSelector", message: error.message };
                    }
                    for (let i = 0; i < matches.length && nodes.length < limit; i++) {
                        nodes.push(matches[i]);
                    }
                    if (nodes.length >= limit) break;
                }
                return nodes;
            }

            function findByXPath(expression, roots, limit) {
                const nodes = [];
                for (const root of roots) {
                    let result;
                    try {
                        result = document.evaluate(expression, root, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
                    } catch (e) {
                        if (e instanceof DOMException && e.name === "SyntaxError") {
                            throw { name: "InvalidSelector", message: e.message };
                        }
                        throw { name: "UnknownError", message: e.message };
                    }
                    for (let i = 0; i < result.snapshotLength && nodes.length < limit; i++) {
                        const node = result.snapshotItem(i);
                        nodes.push(node);
                    }
                }
                return nodes;
            }

            function findByInnerText(selector, roots, maxDepth, matchType, ignoreCase, limit) {
                if (typeof selector !== "string" || selector === "") {
                    throw { name: "InvalidSelector", message: "Selector must be a non-empty string." };
                }

                const matches = [];
                const searchText = ignoreCase ? selector.toUpperCase() : selector;

                function recurse(nodes, depth) {
                    for (const node of nodes) {
                        if (node.nodeType !== 1) continue; // HTMLElement only
                        let nodeText = node.innerText ?? "";
                        if (ignoreCase) nodeText = nodeText.toUpperCase();

                        const isMatch =
                            matchType === "full" ? nodeText === searchText :
                            matchType === "partial" ? nodeText.includes(searchText) : false;

                        if (isMatch) {
                            matches.push(node);
                            if (matches.length >= limit) return;
                        }

                        if ((maxDepth === null || depth < maxDepth) && node.children.length > 0) {
                            recurse(node.children, depth + 1);
                            if (matches.length >= limit) return;
                        }
                    }
                }

                for (const root of roots) {
                    recurse([root], 0);
                    if (matches.length >= limit) break;
                }

                return matches;
            }

            function findByAccessibility(selector, roots, limit) {
                const matches = [];

                function getRole(el) {
                    return el.getAttribute("role");
                }

                function getAccessibleName(el) {
                    return el.getAttribute("aria-label") || el.getAttribute("aria-labelledby");
                }

                function recurse(nodes) {
                    for (const node of nodes) {
                        if (node.nodeType !== 1) continue;

                        let match = true;
                        if ("role" in selector) {
                            match = getRole(node) === selector.role;
                        }
                        if (match && "name" in selector) {
                            match = getAccessibleName(node) === selector.name;
                        }

                        if (match) {
                            matches.push(node);
                            if (matches.length >= limit) return;
                        }

                        if (node.children.length > 0) {
                            recurse(Array.from(node.children));
                            if (matches.length >= limit) return;
                        }
                    }
                }

                if (!("role" in selector || "name" in selector)) {
                    throw { name: "InvalidSelector", message: "Accessibility selector must include 'role' or 'name'." };
                }

                for (const root of roots) {
                    recurse([root]);
                    if (matches.length >= limit) break;
                }

                return matches;
            }

            function findByContext(selector, roots, limit) {
                return [];
            }

            const type = locator.type;
            const value = locator.value;
            const limit = validateLimit(maxNodeCount);

            switch (type) {
                case "css":
                    return findByCss(value, contextNodes, limit);

                case "xpath":
                    return findByXPath(value, contextNodes, limit);

                case "innerText":
                    return findByInnerText(
                        value,
                        contextNodes,
                        locator.maxDepth ?? null,
                        locator.matchType || "full",
                        !!locator.ignoreCase,
                        limit
                    );

                case "accessibility":
                    return findByAccessibility(value, contextNodes, limit);

                case "context":
                    return findByContext(value, contextNodes, limit);

                default:
                    throw { name: "InvalidArgument", message: `Unsupported locator type: ${type}.` };
            }
        })
    )"_s;

    RefPtr<JSON::Array> arguments = JSON::Array::create();

    arguments->pushValue(WTFMove(locatorObject));

    if (optionalStartNodesArray) {
        Ref<JSON::Value> startNodesValue = *optionalStartNodesArray;
        arguments->pushValue(WTFMove(startNodesValue));
    } else {
        Ref<JSON::Value> nullValue = JSON::Value::null();
        arguments->pushValue(WTFMove(nullValue));
    }

    if (optionalMaxNodeCount) {
        Ref<JSON::Value> numberValue = JSON::Value::create(*optionalMaxNodeCount);
        arguments->pushValue(WTFMove(numberValue));
    } else {
        Ref<JSON::Value> nullValue = JSON::Value::null();
        arguments->pushValue(WTFMove(nullValue));
    }

    std::optional<double> callbackTimeout = 250;

    session->evaluateJavaScriptFunction(context, emptyString(), locateNodeFunction, *arguments, true, false, callbackTimeout.value(),
    [callback = WTFMove(callback)](Inspector::CommandResult<String>&& result) mutable {

        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!result, InternalError);

        // FIXME: Await resolution of https://bugs.webkit.org/show_bug.cgi?id=294633 to implement a complete callback. Currently, we can't convert the result into objects due to uncertainty about its return format.
    });
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)

