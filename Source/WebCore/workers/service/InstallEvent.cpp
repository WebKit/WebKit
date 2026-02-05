/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "InstallEvent.h"

#include "HTTPParsers.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "ScriptExecutionContextInlines.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerRoute.h"
#include "WorkerSWClientConnection.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InstallEvent);

InstallEvent::InstallEvent(const AtomString& type, ExtendableEventInit&& initializer, IsTrusted isTrusted)
    : ExtendableEvent(EventInterfaceType::InstallEvent, type, initializer, isTrusted)
{
}

InstallEvent::~InstallEvent() = default;

static ExceptionOr<ServiceWorkerRoutePattern> toServiceWorkerRoutePattern(const URLPattern& pattern)
{
    if (pattern.hasRegExpGroups())
        return Exception { ExceptionCode::TypeError, "Service Worker route url pattern has regexp groups"_s };

    return ServiceWorkerRoutePattern {
        pattern.shouldIgnoreCase(),
        pattern.protocol(),
        pattern.username(),
        pattern.password(),
        pattern.hostname(),
        pattern.port(),
        pattern.pathname(),
        pattern.search(),
        pattern.hash()
    };
}

static ExceptionOr<ServiceWorkerRouteCondition> toServiceWorkerRouteCondition(RouterCondition&& condition)
{
    std::optional<ServiceWorkerRoutePattern> pattern;
    if (condition.urlPattern) {
        Ref urlPattern = *std::get<RefPtr<URLPattern>>(*condition.urlPattern);
        auto patternOrException = toServiceWorkerRoutePattern(urlPattern);
        if (patternOrException.hasException())
            return patternOrException.releaseException();
        pattern = patternOrException.releaseReturnValue();
    }

    Vector<ServiceWorkerRouteCondition> orConditions;
    if (condition.orConditions) {
        for (auto& orCondition : *condition.orConditions) {
            auto orConditionOrException = toServiceWorkerRouteCondition(WTF::move(orCondition));
            if (orConditionOrException.hasException())
                return orConditionOrException.releaseException();
            orConditions.append(orConditionOrException.releaseReturnValue());
        }
    }

    std::unique_ptr<ServiceWorkerRouteCondition> notCondition;
    if (condition.notCondition) {
        auto notConditionOrException = toServiceWorkerRouteCondition(WTF::move(*condition.notCondition).value());
        if (notConditionOrException.hasException())
            return notConditionOrException.releaseException();
        notCondition = makeUnique<ServiceWorkerRouteCondition>(notConditionOrException.releaseReturnValue());
    }

    return ServiceWorkerRouteCondition {
        WTF::move(pattern),
        WTF::move(condition.requestMethod),
        WTF::move(condition.requestMode),
        WTF::move(condition.requestDestination),
        WTF::move(condition.runningStatus),
        WTF::move(orConditions),
        WTF::move(notCondition)
    };
}

static ExceptionOr<ServiceWorkerRoute> toServiceWorkerRoute(RouterRule&& rule)
{
    auto conditionOrException = toServiceWorkerRouteCondition(WTF::move(rule.condition));
    if (conditionOrException.hasException())
        return conditionOrException.releaseException();

    return ServiceWorkerRoute {
        conditionOrException.releaseReturnValue(),
        WTF::move(rule.source)
    };
}

// https://w3c.github.io/ServiceWorker/#verify-router-condition
static std::optional<Exception> verifyRouterCondition(RouterCondition& condition, ServiceWorkerGlobalScope& scope)
{
    bool hasCondition = false;
    if (condition.urlPattern) {
        auto urlPatternOrException = URLPattern::create(scope, std::exchange(*condition.urlPattern, { }), scope.contextData().scriptURL.string());
        if (urlPatternOrException.hasException())
            return urlPatternOrException.releaseException();
        condition.urlPattern = urlPatternOrException.releaseReturnValue();
    }
    if (!condition.requestMethod.isNull()) {
        if (!isValidHTTPToken(condition.requestMethod))
            return Exception { ExceptionCode::TypeError, "Method is not a valid HTTP token."_s };
        if (isForbiddenMethod(condition.requestMethod))
            return Exception { ExceptionCode::TypeError, "Method is forbidden."_s };
        condition.requestMethod = normalizeHTTPMethod(condition.requestMethod);

        hasCondition = true;
    }
    if (condition.requestMode)
        hasCondition = true;
    if (condition.requestDestination)
        hasCondition = true;
    if (condition.runningStatus)
        hasCondition = true;
    if (condition.orConditions && !condition.orConditions->isEmpty()) {
        if (hasCondition)
            return Exception { ExceptionCode::TypeError, "Or condition should not be present"_s };
        for (auto& orCondition : *condition.orConditions) {
            if (auto exception = verifyRouterCondition(orCondition, scope))
                return *exception;
        }
        hasCondition = true;
    }
    if (condition.notCondition) {
        if (hasCondition)
            return Exception { ExceptionCode::TypeError, "Not condition should not be present"_s };
        if (auto exception = verifyRouterCondition(condition.notCondition->value(), scope))
            return *exception;
    }
    return { };
}

static ExceptionOr<ServiceWorkerRoute> convertServiceWorkerRule(RouterRule&& rule, ServiceWorkerGlobalScope& scope)
{
    if (auto validationException = verifyRouterCondition(rule.condition, scope))
        return WTF::move(*validationException);

    return toServiceWorkerRoute(WTF::move(rule));
}

// https://w3c.github.io/ServiceWorker/#dom-installevent-addroutes
void InstallEvent::addRoutes(JSC::JSGlobalObject& globalObject, Variant<RouterRule, Vector<RouterRule>>&& rules, Ref<DeferredPromise>&& promise)
{
    auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(&globalObject);
    RefPtr serviceWorkerGlobalScope = dynamicDowncast<ServiceWorkerGlobalScope>(jsDOMGlobalObject.scriptExecutionContext());

    auto rulesVector = switchOn(WTF::move(rules), [](RouterRule&& rule) -> Vector<RouterRule> {
        return Vector<RouterRule>::from(WTF::move(rule));
    }, [](Vector<RouterRule>&& rules) -> Vector<RouterRule> {
        return WTF::move(rules);
    });

    Vector<ServiceWorkerRoute> routes;
    for (auto& rule : rulesVector) {
        auto routeOrException = convertServiceWorkerRule(WTF::move(rule), *serviceWorkerGlobalScope);
        if (routeOrException.hasException()) {
            promise->reject(routeOrException.releaseException());
            return;
        }

        auto route = routeOrException.releaseReturnValue();
        if (!serviceWorkerGlobalScope->hasFetchEventHandler()) {
            if (auto* source = std::get_if<RouterSourceEnum>(&route.source); *source == RouterSourceEnum::FetchEvent || *source == RouterSourceEnum::RaceNetworkAndFetchHandler) {
                promise->reject(Exception { ExceptionCode::TypeError, "Rule source requires registering a fetch event handler"_s });
                return;
            }
        }
        routes.append(WTF::move(route));
    }

    if (!isWaiting()) {
        promise->reject(Exception { ExceptionCode::TypeError, "Service worker is not installing"_s });
        return;
    }

    RefPtr delayPromise = DeferredPromise::create(jsDOMGlobalObject);
    if (delayPromise) {
        auto& jsPromise = *JSC::jsCast<JSC::JSPromise*>(delayPromise->promise());
        waitUntil(DOMPromise::create(jsDOMGlobalObject, jsPromise));
    }

    Ref<SWClientConnection> connection = serviceWorkerGlobalScope->swClientConnection();
    serviceWorkerGlobalScope->enqueueTaskWhenSettled(connection->addRoutes(serviceWorkerGlobalScope->registration().identifier(), WTF::move(routes)), TaskSource::Networking, [promise = WTF::move(promise), delayPromise = WTF::move(delayPromise)](auto&& result) mutable {
        if (!result) {
            promise->reject(result.error().toException());
            delayPromise->resolve();
            return;
        }
        promise->resolve();
        delayPromise->resolve();
    });
}

} // namespace WebCore
