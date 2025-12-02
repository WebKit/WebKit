/*
 * Copyright (C) 2016 Canon Inc.
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FetchRequest.h"

#include "ContextDestructionObserverInlines.h"
#include "DocumentQuirks.h"
#include "HTTPParsers.h"
#include "JSAbortSignal.h"
#include "Logging.h"
#include "OriginAccessPatterns.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WebCoreOpaqueRoot.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static std::optional<Exception> setMethod(ResourceRequest& request, const String& initMethod)
{
    if (!isValidHTTPToken(initMethod))
        return Exception { ExceptionCode::TypeError, "Method is not a valid HTTP token."_s };
    if (isForbiddenMethod(initMethod))
        return Exception { ExceptionCode::TypeError, "Method is forbidden."_s };
    request.setHTTPMethod(normalizeHTTPMethod(initMethod));
    return std::nullopt;
}

static ExceptionOr<String> computeReferrer(ScriptExecutionContext& context, const String& referrer)
{
    if (referrer.isEmpty())
        return "no-referrer"_str;

    URL referrerURL = context.completeURL(referrer, ScriptExecutionContext::ForceUTF8::Yes);
    if (!referrerURL.isValid())
        return Exception { ExceptionCode::TypeError, "Referrer is not a valid URL."_s };

    if (referrerURL.protocolIsAbout() && referrerURL.path() == "client"_s)
        return "client"_str;

    if (!(context.securityOrigin() && context.protectedSecurityOrigin()->canRequest(referrerURL, OriginAccessPatternsForWebProcess::singleton())))
        return "client"_str;

    return String { referrerURL.string() };
}

static std::optional<Exception> buildOptions(FetchOptions& options, ResourceRequest& request, String& referrer, RequestPriority& priority, ScriptExecutionContext& context, const FetchRequest::Init& init)
{
    if (!init.window.isUndefinedOrNull() && !init.window.isEmpty())
        return Exception { ExceptionCode::TypeError, "Window can only be null."_s };

    if (init.hasMembers()) {
        if (options.mode == FetchOptions::Mode::Navigate)
            options.mode = FetchOptions::Mode::SameOrigin;
        referrer = "client"_s;
        options.referrerPolicy = { };
    }

    if (!init.referrer.isNull()) {
        auto result = computeReferrer(context, init.referrer);
        if (result.hasException())
            return result.releaseException();
        referrer = result.releaseReturnValue();
    }

    if (init.referrerPolicy)
        options.referrerPolicy = init.referrerPolicy.value();

    if (init.priority)
        priority = *init.priority;

    if (init.mode) {
        options.mode = init.mode.value();
        if (options.mode == FetchOptions::Mode::Navigate)
            return Exception { ExceptionCode::TypeError, "Request constructor does not accept navigate fetch mode."_s };
    }

    if (init.credentials)
        options.credentials = init.credentials.value();

    if (init.cache)
        options.cache = init.cache.value();
    if (options.cache == FetchOptions::Cache::OnlyIfCached && options.mode != FetchOptions::Mode::SameOrigin)
        return Exception { ExceptionCode::TypeError, "only-if-cached cache option requires fetch mode to be same-origin."_s  };

    if (init.redirect)
        options.redirect = init.redirect.value();

    if (!init.integrity.isNull())
        options.integrity = init.integrity;

    if (init.keepalive && init.keepalive.value())
        options.keepAlive = true;

    if (!init.method.isNull()) {
        if (auto exception = setMethod(request, init.method))
            return exception;
    }

    return std::nullopt;
}

static bool methodCanHaveBody(const ResourceRequest& request)
{
    return request.httpMethod() != "GET"_s && request.httpMethod() != "HEAD"_s;
}

static IPAddressSpace updateTargetAddressSpaceIfNeeded(IPAddressSpace currentAddressSpace, const URL& url)
{
    auto host = url.host();
    if (host.isEmpty())
        return currentAddressSpace;

    if (WebCore::isLocalIPAddressSpace(url))
        return IPAddressSpace::Local;

    if (host.endsWithIgnoringASCIICase(".local"_s))
        return IPAddressSpace::Local;

    return currentAddressSpace;
}

inline FetchRequest::FetchRequest(ScriptExecutionContext& context, std::optional<FetchBody>&& body, Ref<FetchHeaders>&& headers, ResourceRequest&& request, FetchOptions&& options, String&& referrer)
    : FetchBodyOwner(&context, WTFMove(body), WTFMove(headers))
    , m_request(WTFMove(request))
    , m_requestURL({ m_request.url(), context.topOrigin().data() })
    , m_options(WTFMove(options))
    , m_referrer(WTFMove(referrer))
    , m_signal(AbortSignal::create(&context))
{
    m_request.setRequester(ResourceRequestRequester::Fetch);
}

ExceptionOr<void> FetchRequest::initializeOptions(const Init& init)
{
    ASSERT(scriptExecutionContext());

    auto exception = buildOptions(m_options, m_request, m_referrer, m_priority, *protectedScriptExecutionContext(), init);
    if (exception)
        return WTFMove(exception.value());

    if (m_options.mode == FetchOptions::Mode::NoCors) {
        const String& method = m_request.httpMethod();
        if (method != "GET"_s && method != "POST"_s && method != "HEAD"_s)
            return Exception { ExceptionCode::TypeError, "Method must be GET, POST or HEAD in no-cors mode."_s };
        m_headers->setGuard(FetchHeaders::Guard::RequestNoCors);
    }
    
    return { };
}

static inline std::optional<Exception> processInvalidSignal(ScriptExecutionContext& context)
{
    ASCIILiteral message { "FetchRequestInit.signal should be undefined, null or an AbortSignal object. This will throw in a future release."_s };
    context.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, message);

    RefPtr document = dynamicDowncast<Document>(context);
    if (document && document->quirks().shouldIgnoreInvalidSignal())
        return { };

    RELEASE_LOG_ERROR(ResourceLoading, "FetchRequestInit.signal should be undefined, null or an AbortSignal object.");
    return Exception { ExceptionCode::TypeError, message };
}

ExceptionOr<void> FetchRequest::initializeWith(const String& url, Init&& init)
{
    Ref context = *scriptExecutionContext();

    URL requestURL = context->completeURL(url, ScriptExecutionContext::ForceUTF8::Yes);
    if (!requestURL.isValid() || requestURL.hasCredentials())
        return Exception { ExceptionCode::TypeError, "URL is not valid or contains user credentials."_s };

    m_options.mode = Mode::Cors;
    m_options.credentials = Credentials::SameOrigin;
    m_referrer = "client"_s;
    m_request.setURL(WTFMove(requestURL));
    m_requestURL = { m_request.url(), context->topOrigin().data() };
    m_request.setInitiatorIdentifier(context->resourceRequestIdentifier());

    if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext()); document && document->settings().localNetworkAccessEnabled())
        m_targetAddressSpace = updateTargetAddressSpaceIfNeeded(m_targetAddressSpace, m_request.url());

    auto optionsResult = initializeOptions(init);
    if (optionsResult.hasException())
        return optionsResult.releaseException();

    if (init.signal) {
        if (RefPtr signal = JSAbortSignal::toWrapped(context->vm(), init.signal))
            m_signal->signalFollow(*signal);
        else if (!init.signal.isUndefinedOrNull())  {
            if (auto exception = processInvalidSignal(context.get()))
                return WTFMove(*exception);
        }
    }

    if (init.headers) {
        auto fillResult = m_headers->fill(*init.headers);
        if (fillResult.hasException())
            return fillResult.releaseException();
    }

    if (init.body) {
        auto setBodyResult = setBody(WTFMove(*init.body));
        if (setBodyResult.hasException())
            return setBodyResult.releaseException();
    }

    return { };
}

ExceptionOr<void> FetchRequest::initializeWith(FetchRequest& input, Init&& init)
{
    m_request = input.m_request;
    Ref context = *scriptExecutionContext();
    m_requestURL = { m_request.url(), context->topOrigin().data() };

    m_options = input.m_options;
    m_referrer = input.m_referrer;
    m_priority = input.m_priority;
    m_enableContentExtensionsCheck = input.m_enableContentExtensionsCheck;

    auto optionsResult = initializeOptions(init);
    if (optionsResult.hasException())
        return optionsResult.releaseException();

    if (init.signal && !init.signal.isUndefined()) {
        if (RefPtr signal = JSAbortSignal::toWrapped(context->vm(), init.signal))
            m_signal->signalFollow(*signal);
        else if (!init.signal.isNull()) {
            if (auto exception = processInvalidSignal(context.get()))
                return WTFMove(*exception);
        }

    } else
        m_signal->signalFollow(input.m_signal.get());

    if (init.hasMembers()) {
        auto fillResult = init.headers ? m_headers->fill(*init.headers) : m_headers->fill(input.headers());
        if (fillResult.hasException())
            return fillResult;
        m_navigationPreloadIdentifier = std::nullopt;
    } else {
        m_headers->setInternalHeaders(HTTPHeaderMap { input.headers().internalHeaders() });
        m_navigationPreloadIdentifier = input.m_navigationPreloadIdentifier;
    }

    if (RefPtr executionContext = scriptExecutionContext()) {
        if (RefPtr document = dynamicDowncast<Document>(*executionContext); document && document->settings().localNetworkAccessEnabled()) {
            switch (init.targetAddressSpace) {
            case IPAddressSpace::Public:
                m_targetAddressSpace = IPAddressSpace::Public;
                break;
            case IPAddressSpace::Local:
                m_targetAddressSpace = IPAddressSpace::Local;
                break;
            }

            m_targetAddressSpace = updateTargetAddressSpaceIfNeeded(m_targetAddressSpace, m_request.url());
        }
    }

    auto setBodyResult = init.body ? setBody(WTFMove(*init.body)) : setBody(input);
    if (setBodyResult.hasException())
        return setBodyResult;

    return { };
}

ExceptionOr<void> FetchRequest::setBody(FetchBody::Init&& body)
{
    if (!methodCanHaveBody(m_request))
        return Exception { ExceptionCode::TypeError, makeString("Request has method '"_s, m_request.httpMethod(), "' and cannot have a body"_s) };

    ASSERT(scriptExecutionContext());
    auto result = extractBody(WTFMove(body));
    if (result.hasException())
        return result;

    if (m_options.keepAlive && hasReadableStreamBody())
        return Exception { ExceptionCode::TypeError, "Request cannot have a ReadableStream body and keepalive set to true"_s };
    return { };
}

ExceptionOr<void> FetchRequest::setBody(FetchRequest& request)
{
    if (request.isDisturbedOrLocked())
        return Exception { ExceptionCode::TypeError, "Request input is disturbed or locked."_s };

    if (!request.isBodyNull()) {
        if (!methodCanHaveBody(m_request))
            return Exception { ExceptionCode::TypeError, makeString("Request has method '"_s, m_request.httpMethod(), "' and cannot have a body"_s) };

        RefPtr context = scriptExecutionContext();
        auto* globalObject = context ? JSC::jsCast<JSDOMGlobalObject*>(context->globalObject()) : nullptr;
        m_body = request.m_body->createProxy(*globalObject);
        request.setDisturbed();
    }

    if (m_options.keepAlive && hasReadableStreamBody())
        return Exception { ExceptionCode::TypeError, "Request cannot have a ReadableStream body and keepalive set to true"_s };
    return { };
}

ExceptionOr<Ref<FetchRequest>> FetchRequest::create(ScriptExecutionContext& context, Info&& input, Init&& init)
{
    auto request = adoptRef(*new FetchRequest(context, std::nullopt, FetchHeaders::create(FetchHeaders::Guard::Request), { }, { }, { }));
    request->suspendIfNeeded();

    if (std::holds_alternative<String>(input)) {
        auto result = request->initializeWith(std::get<String>(input), WTFMove(init));
        if (result.hasException())
            return result.releaseException();
    } else {
        auto result = request->initializeWith(Ref { *std::get<RefPtr<FetchRequest>>(input) }.get(), WTFMove(init));
        if (result.hasException())
            return result.releaseException();
    }

    return request;
}

Ref<FetchRequest> FetchRequest::create(ScriptExecutionContext& context, std::optional<FetchBody>&& body, Ref<FetchHeaders>&& headers, ResourceRequest&& request, FetchOptions&& options, String&& referrer)
{
    auto result = adoptRef(*new FetchRequest(context, WTFMove(body), WTFMove(headers), WTFMove(request), WTFMove(options), WTFMove(referrer)));
    result->suspendIfNeeded();
    return result;
}

String FetchRequest::referrer() const
{
    if (m_referrer == "no-referrer"_s)
        return String();
    if (m_referrer == "client"_s)
        return "about:client"_s;
    return m_referrer;
}

const String& FetchRequest::urlString() const
{
    return m_requestURL.url().string();
}

ResourceRequest FetchRequest::resourceRequest() const
{
    ASSERT(scriptExecutionContext());

    ResourceRequest request = m_request;
    request.setHTTPHeaderFields(m_headers->internalHeaders());

    if (!isBodyNull())
        request.setHTTPBody(body().bodyAsFormData());

    if (RefPtr context = scriptExecutionContext()) {
        if (RefPtr document = dynamicDowncast<Document>(*context); document && document->settings().localNetworkAccessEnabled())
            request.setTargetAddressSpace(m_targetAddressSpace);
    }

    return request;
}

ExceptionOr<Ref<FetchRequest>> FetchRequest::clone(JSDOMGlobalObject& globalObject)
{
    if (isDisturbedOrLocked())
        return Exception { ExceptionCode::TypeError, "Body is disturbed or locked"_s };
    RefPtr context = scriptExecutionContext();
    if (!context)
        return Exception { ExceptionCode::InvalidStateError, "Cannot clone FetchRequest without a valid script execution context"_s };
    auto clone = adoptRef(*new FetchRequest(*context, std::nullopt, FetchHeaders::create(m_headers.get()), ResourceRequest { m_request }, FetchOptions { m_options }, String { m_referrer }));
    clone->suspendIfNeeded();
    clone->cloneBody(globalObject, *this);
    clone->setNavigationPreloadIdentifier(m_navigationPreloadIdentifier);
    clone->m_enableContentExtensionsCheck = m_enableContentExtensionsCheck;
    if (RefPtr document = dynamicDowncast<Document>(*context); document && document->settings().localNetworkAccessEnabled())
        clone->m_targetAddressSpace = m_targetAddressSpace;
    clone->m_signal->signalFollow(m_signal);
    return clone;
}

void FetchRequest::stop()
{
    m_requestURL.clear();
    FetchBodyOwner::stop();
}

WebCoreOpaqueRoot root(FetchRequest* request)
{
    return WebCoreOpaqueRoot { request };
}

} // namespace WebCore
