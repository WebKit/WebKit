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
#include <WebCore/FloatRect.h>
#include <limits>
#include <wtf/Borrow.h>
#include <wtf/Ref.h>
#include <wtf/URL.h>
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
    session->switchToBrowsingContext(browsingContext, emptyString(), [callback = WTF::move(callback)](CommandResult<void>&& result) {
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
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(browsingContext.isEmpty(), FrameNotFound);

    auto handles = session->extractBrowsingContextHandles(browsingContext);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(handles);
    auto [pageHandle, frameHandle] = handles.value();

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!frameHandle.isEmpty(), InvalidParameter);

    RefPtr webPageProxy = session->webPageProxyForHandle(pageHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!webPageProxy, FrameNotFound);

    session->closeBrowsingContext(pageHandle, WTF::move(callback));
}

void BidiBrowsingContextAgent::captureScreenshot(const BrowsingContext& browsingContext, std::optional<Inspector::Protocol::BidiBrowsingContext::ScreenshotOrigin>&& optionalOrigin, RefPtr<JSON::Object>&& format, RefPtr<JSON::Object>&& clip, CommandCallback<String>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    RefPtr webPageProxy = session->webPageProxyForHandle(browsingContext);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!webPageProxy, WindowNotFound);

    ScreenshotParameters params;

    if (optionalOrigin) {
        auto origin = *optionalOrigin;
        switch (origin) {
        case Inspector::Protocol::BidiBrowsingContext::ScreenshotOrigin::Viewport:
            params.origin = ScreenshotOrigin::Viewport;
            break;
        case Inspector::Protocol::BidiBrowsingContext::ScreenshotOrigin::Document:
            params.origin = ScreenshotOrigin::Document;
            break;
        }
    } else
        params.origin = ScreenshotOrigin::Viewport;

    if (format) {
        String typeValue = format->getString("type"_s);
        if (!typeValue.isEmpty()) {
            auto parsedFormat = parseImageFormatType(typeValue);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!parsedFormat, InvalidParameter);
            params.format = *parsedFormat;
        }
        if (auto qualityValue = format->getDouble("quality"_s))
            params.quality = qualityValue.value();
    }

    auto originRect = getOriginRectangle(params.origin, *webPageProxy);

    if (clip) {
        auto clipRect = parseClipRectangle(clip, originRect);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!clipRect, InvalidParameter);
        params.clipRect = rectangleIntersection(originRect, *clipRect);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(params.clipRect->isEmpty(), InternalError);
    } else
        params.clipRect = originRect;

    performScreenshotCapture(*webPageProxy, params, WTF::move(callback));
}

static constexpr Inspector::Protocol::Automation::BrowsingContextPresentation defaultBrowsingContextPresentation = Inspector::Protocol::Automation::BrowsingContextPresentation::Tab;

static Inspector::Protocol::Automation::BrowsingContextPresentation NODELETE browsingContextPresentationFromCreateType(Inspector::Protocol::BidiBrowsingContext::CreateType createType)
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

    if (!optionalUserContext.isNull()) {
        bool isValid = session->isValidUserContext(optionalUserContext);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!isValid, NoSuchUserContext);
    }

    // FIXME: implement `referenceContext` option.
    // FIXME: implement `background` option.
    // FIXME: implement `userContext` option (use validated context to create in specific user context).

    session->createBrowsingContext(browsingContextPresentationFromCreateType(createType), [callback = WTF::move(callback)](CommandResultOf<BrowsingContext, Inspector::Protocol::Automation::BrowsingContextPresentation>&& result) {
        if (!result) {
            callback(makeUnexpected(result.error()));
            return;
        }

        auto [resultContext, resultPresentation] = WTF::move(result.value());
        callback(WTF::move(resultContext));
    });
}

Inspector::Protocol::BidiBrowsingContext::BrowsingContext BidiBrowsingContextAgent::getBrowsingContextID(const WebCore::FrameIdentifier& frameID) const
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

Ref<Inspector::Protocol::BidiBrowsingContext::Info> BidiBrowsingContextAgent::getNavigableInfo(const WebKit::FrameTreeNodeData& tree, std::optional<uint64_t> maxDepth, IncludeParentID includeParentID)
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

    info->setChildren(WTF::move(childrenInfo));
    return info;
}

// Recursively traverses the frame tree of the given pages, one page at a time.
// We need such recursion because we need to wait for the frame tree of the current page to be fully processed before moving on to the next page.
void BidiBrowsingContextAgent::getNextTree(Vector<Ref<WebPageProxy>>&& pagesToProcess, Ref<JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>> resultsObject, std::optional<uint64_t> maxDepth, CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>>>&& callback)
{
    if (pagesToProcess.isEmpty()) {
        callback(WTF::move(resultsObject));
        return;
    }

    Ref webPageProxy = pagesToProcess.takeLast();
    webPageProxy->getAllFrameTrees([this, pagesToProcess = WTF::move(pagesToProcess), resultsObject = WTF::move(resultsObject), callback = WTF::move(callback), maxDepth, protectedPage = Ref { webPageProxy }](Vector<WebKit::FrameTreeNodeData>&& trees) mutable {
        for (auto& tree : trees) {
            auto infoTree = getNavigableInfo(tree, maxDepth, IncludeParentID::Yes);
            resultsObject->addItem(WTF::move(infoTree));
        }
        getNextTree(WTF::move(pagesToProcess), WTF::move(resultsObject), maxDepth, WTF::move(callback));
    });
}

void BidiBrowsingContextAgent::getTree(const BrowsingContext& optionalRoot, std::optional<double>&& optionalMaxDepth, CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>>>&& callback)
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

    RefPtr processPool = session->processPool();
    for (Ref process : borrow(processPool->processes()).get()) {
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
    getNextTree(WTF::move(pagesToProcess), WTF::move(resultsObject), WTF::move(maxDepth), [callback = WTF::move(callback)](auto&& result) {
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
static constexpr ReadinessState defaultReadinessState = ReadinessState::Interactive;

static PageLoadStrategy NODELETE pageLoadStrategyFromReadinessState(ReadinessState state)
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

void BidiBrowsingContextAgent::navigate(const BrowsingContext& browsingContext, const String& url, const String& optionalWait, CommandCallbackOf<String, Inspector::Protocol::BidiBrowsingContext::NavigationID>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    RefPtr webPageProxy = session->webPageProxyForHandle(browsingContext);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!webPageProxy, FrameNotFound);

    URL baseURL { webPageProxy->currentURL() };
    URL urlRecord { baseURL, url };
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!urlRecord.isValid(), InvalidParameter);

    // `wait` is modeled as an optional string so we can reject invalid provided values (e.g. "" per WPT).
    std::optional<ReadinessState> waitCondition;
    if (!optionalWait.isNull()) {
        waitCondition = Inspector::Protocol::WebDriverBidiHelpers::parseEnumValueFromString<ReadinessState>(optionalWait);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!waitCondition, InvalidParameter);
    }

    auto readinessState = waitCondition.value_or(defaultReadinessState);
    auto pageLoadStrategy = pageLoadStrategyFromReadinessState(readinessState);
    session->navigateBrowsingContext(browsingContext, urlRecord.string(), pageLoadStrategy, defaultPageLoadTimeout.milliseconds(), [urlRecord, callback = WTF::move(callback)](CommandResult<void>&& result) {
        if (!result) {
            callback(makeUnexpected(result.error()));
            return;
        }

        // FIXME: keep track of navigation IDs that we hand out.
        callback({ { urlRecord.string(), "placeholder_navigation"_s } });
    });
}

void BidiBrowsingContextAgent::reload(const BrowsingContext& browsingContext, std::optional<bool>&& optionalIgnoreCache, std::optional<ReadinessState>&& optionalReadinessState, CommandCallbackOf<String, Inspector::Protocol::BidiBrowsingContext::NavigationID>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: implement `ignoreCache` option.

    auto pageLoadStrategy = pageLoadStrategyFromReadinessState(optionalReadinessState.value_or(defaultReadinessState));
    session->reloadBrowsingContext(browsingContext, pageLoadStrategy, defaultPageLoadTimeout.milliseconds(), [session = WTF::move(session), browsingContext, callback = WTF::move(callback)](CommandResult<void>&& result) {
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

void BidiBrowsingContextAgent::traverseHistory(const BrowsingContext& browsingContext, int delta, CommandCallback<void>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    session->traverseHistoryInBrowsingContext(browsingContext, delta, WTF::move(callback));
}

static std::optional<int> parseNonNegativeInteger(const JSON::Object& object, const String& key)
{
    auto value = object.getDouble(key);
    if (!value || !JSC::isInteger(*value))
        return std::nullopt;

    if (*value < 0 || *value > std::numeric_limits<int>::max())
        return std::nullopt;

    return static_cast<int>(*value);
}

void BidiBrowsingContextAgent::setViewport(const BrowsingContext& optionalContext, RefPtr<JSON::Object>&& optionalViewport, std::optional<double>&& optionalDevicePixelRatio, RefPtr<JSON::Array>&& optionalUserContexts, CommandCallback<void>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    bool hasContext = !optionalContext.isEmpty();
    bool hasUserContexts = optionalUserContexts && optionalUserContexts->length() > 0;

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(hasContext && hasUserContexts, InvalidParameter);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!hasContext && !hasUserContexts, InvalidParameter);

    std::optional<int> viewportWidth;
    std::optional<int> viewportHeight;
    if (optionalViewport) {
        viewportWidth = parseNonNegativeInteger(*optionalViewport, "width"_s);
        viewportHeight = parseNonNegativeInteger(*optionalViewport, "height"_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!viewportWidth || !viewportHeight, InvalidParameter);
    }

    if (optionalDevicePixelRatio)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(*optionalDevicePixelRatio <= 0, InvalidParameter);

    if (hasContext) {
        RefPtr page = session->webPageProxyForHandle(optionalContext);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, FrameNotFound);

        RefPtr mainFrame = page->mainFrame();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!mainFrame, InvalidParameter);
        auto mainFrameID = getBrowsingContextID(mainFrame->frameID());
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(mainFrameID != optionalContext, InvalidParameter);

        session->setViewportForPage(*page, viewportWidth, viewportHeight, optionalDevicePixelRatio, WTF::move(callback));
    } else {
        for (const auto& userContextValue : *optionalUserContexts) {
            auto userContext = userContextValue->asString();
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(userContext.isEmpty(), InvalidParameter);
        }
        // FIXME: Support applying the viewport to user contexts.
        // https://bugs.webkit.org/show_bug.cgi?id=288104
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(true, NotImplemented);
    }
}

WebCore::FloatRect BidiBrowsingContextAgent::getOriginRectangle(ScreenshotOrigin origin, const WebPageProxy& webPageProxy)
{
    auto viewSize = webPageProxy.viewSize();

    if (origin == ScreenshotOrigin::Viewport)
        return WebCore::FloatRect(0, 0, viewSize.width(), viewSize.height());

    // FIXME: Get actual document scroll dimensions when available
    return WebCore::FloatRect(0, 0, viewSize.width(), viewSize.height());
}

WebCore::FloatRect BidiBrowsingContextAgent::normalizeRect(const WebCore::FloatRect& rect)
{
    float x = rect.x(), y = rect.y();
    float width = rect.width(), height = rect.height();

    if (width < 0) {
        x += width;
        width = -width;
    }
    if (height < 0) {
        y += height;
        height = -height;
    }
    return { x, y, width, height };
}

WebCore::FloatRect BidiBrowsingContextAgent::rectangleIntersection(const WebCore::FloatRect& rect1, const WebCore::FloatRect& rect2)
{
    auto norm1 = normalizeRect(rect1);
    auto norm2 = normalizeRect(rect2);

    float x0 = std::max(norm1.x(), norm2.x());
    float x1 = std::min(norm1.maxX(), norm2.maxX());
    float y0 = std::max(norm1.y(), norm2.y());
    float y1 = std::min(norm1.maxY(), norm2.maxY());

    float width = (x1 < x0) ? 0 : (x1 - x0);
    float height = (y1 < y0) ? 0 : (y1 - y0);
    return WebCore::FloatRect(x0, y0, width, height);
}

std::optional<WebCore::FloatRect> BidiBrowsingContextAgent::parseClipRectangle(const RefPtr<JSON::Object>& clip, const WebCore::FloatRect& originRect)
{
    if (!clip)
        return std::nullopt;

    String clipType = clip->getString("type"_s);

    if (clipType == "element"_s)
        return parseElementClipRectangle(clip, originRect);

    return parseBoxClipRectangle(clip, originRect);
}

std::optional<WebCore::FloatRect> BidiBrowsingContextAgent::parseBoxClipRectangle(const RefPtr<JSON::Object>& clip, const WebCore::FloatRect& originRect)
{
    auto x = clip->getDouble("x"_s).value_or(0);
    auto y = clip->getDouble("y"_s).value_or(0);
    auto width = clip->getDouble("width"_s).value_or(0);
    auto height = clip->getDouble("height"_s).value_or(0);
    return WebCore::FloatRect(originRect.x() + x, originRect.y() + y, width, height);
}

std::optional<WebCore::FloatRect> BidiBrowsingContextAgent::parseElementClipRectangle(const RefPtr<JSON::Object>& clip, const WebCore::FloatRect& originRect)
{
    // FIXME: rdar://160959983 - implement element reference resolution
    auto elementObj = clip->getObject("element"_s);
    if (!elementObj)
        return std::nullopt;
    return std::nullopt;
}

void BidiBrowsingContextAgent::performScreenshotCapture(WebPageProxy& page, const ScreenshotParameters& params, Inspector::CommandCallback<String>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    String frameHandle = ""_s;
    String nodeHandle = ""_s;
    bool scrollIntoViewIfNeeded = false;
    bool clipToViewport = (params.origin == ScreenshotOrigin::Viewport);

    session->takeScreenshot(session->handleForWebPageProxy(page), frameHandle, nodeHandle, scrollIntoViewIfNeeded, clipToViewport, [this, params, callback = WTF::move(callback)](CommandResult<String>&& result) mutable {
        if (!result) {
            callback(makeUnexpected(result.error()));
            return;
        }

        String finalImage = result.value();
        if (params.format != ImageFormatType::Png)
            finalImage = convertImageFormat(result.value(), imageFormatTypeToString(params.format), params.quality);

        callback(WTF::move(finalImage));
    });
}

std::optional<BidiBrowsingContextAgent::ImageFormatType> BidiBrowsingContextAgent::parseImageFormatType(const String& format)
{
    if (format == "png"_s || format == "image/png"_s)
        return ImageFormatType::Png;
    if (format == "jpeg"_s || format == "image/jpeg"_s)
        return ImageFormatType::Jpeg;
    if (format == "webp"_s || format == "image/webp"_s)
        return ImageFormatType::Webp;
    return std::nullopt;
}

String BidiBrowsingContextAgent::imageFormatTypeToString(ImageFormatType format)
{
    switch (format) {
    case ImageFormatType::Png:
        return "image/png"_s;
    case ImageFormatType::Jpeg:
        return "image/jpeg"_s;
    case ImageFormatType::Webp:
        return "image/webp"_s;
    }
    ASSERT_NOT_REACHED();
    return "image/png"_s;
}

String BidiBrowsingContextAgent::convertImageFormat(const String& base64PNG, const String& targetFormat, std::optional<double> quality) const
{
    UNUSED_PARAM(targetFormat);
    UNUSED_PARAM(quality);

    // FIXME: Implement image format conversion
    return base64PNG;
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
