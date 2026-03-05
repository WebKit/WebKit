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
#include "ServiceWorkerRoute.h"

#include "ExceptionOr.h"
#include "FetchOptions.h"
#include "HTTPParsers.h"
#include "ResourceRequest.h"
#include "URLPatternCanonical.h"
#include "URLPatternParser.h"
#include <wtf/CrossThreadCopier.h>

namespace WebCore {

// https://w3c.github.io/ServiceWorker/#count-router-inner-conditions
std::optional<size_t> countRouterInnerConditions(const ServiceWorkerRouteCondition& routeCondition, size_t result, size_t depth)
{
    --result;
    if (!result || !depth)
        return { };

    for (auto& condition : routeCondition.orConditions) {
        auto orResult = countRouterInnerConditions(condition, result, depth - 1);
        if (!orResult)
            return { };
        result = *orResult;
    }

    if (routeCondition.notCondition) {
        auto notResult = countRouterInnerConditions(*routeCondition.notCondition, result, depth - 1);
        if (!notResult)
            return { };
        result = *notResult;
    }
    return result;
}

static URLPatternUtilities::URLPatternStringOptions computeOptions(EncodingCallbackType type, bool shouldIgnoreCase)
{
    switch (type) {
    case EncodingCallbackType::Protocol:
        return { };
    case EncodingCallbackType::Username:
        return { };
    case EncodingCallbackType::Password:
        return { };
    case EncodingCallbackType::Host:
    case EncodingCallbackType::IPv6Host:
        return { .delimiterCodepoint = "."_s };
    case EncodingCallbackType::Path:
        return { "/"_s, "/"_s, shouldIgnoreCase };
    case EncodingCallbackType::OpaquePath:
        return { { }, { }, shouldIgnoreCase };
    case EncodingCallbackType::Port:
        return { };
    case EncodingCallbackType::Search:
        return { { }, { }, shouldIgnoreCase };
    case EncodingCallbackType::Hash:
        return { { }, { }, shouldIgnoreCase };
    }

    ASSERT_NOT_REACHED();
    return { };
}

static Expected<String, ExceptionData> validateAndCompileURLPatternComponent(StringView component, EncodingCallbackType type, bool shouldIgnoreCase)
{
    auto options = computeOptions(type, shouldIgnoreCase);
    auto result = URLPatternUtilities::URLPatternParser::parse(component, options, type);
    if (result.hasException())
        return makeUnexpected(ExceptionData { result.exception().code(), result.releaseException().releaseMessage() });

    auto parts = result.releaseReturnValue();

    bool hasRegexGroups = parts.containsIf([](auto& part) {
        return part.type == URLPatternUtilities::PartType::Regexp;
    });
    ASSERT(!hasRegexGroups);
    if (hasRegexGroups)
        return makeUnexpected(ExceptionData { ExceptionCode::TypeError, "Service Worker route url pattern has regexp groups"_s });

    return generateRegexAndNameList(parts, options).first;
}

static std::optional<ExceptionData> validateAndUpdateURLPatternComponent(String& component, EncodingCallbackType type, bool shouldIgnoreCase)
{
    if (component == "*"_s) {
        component = { };
        return { };
    }

    auto exceptionOrCompiledExpression = validateAndCompileURLPatternComponent(component, type, shouldIgnoreCase);
    if (!exceptionOrCompiledExpression)
        return exceptionOrCompiledExpression.error();

    component = exceptionOrCompiledExpression.value();
    return { };
}

static inline std::optional<ExceptionData> validateServiceWorkerRouteCondition(ServiceWorkerRouteCondition& condition)
{
    if (condition.urlPattern) {
        if (auto exception = validateAndUpdateURLPatternComponent(condition.urlPattern->protocol, EncodingCallbackType::Protocol, condition.urlPattern->shouldIgnoreCase))
            return exception;
        if (auto exception = validateAndUpdateURLPatternComponent(condition.urlPattern->username, EncodingCallbackType::Username, condition.urlPattern->shouldIgnoreCase))
            return exception;
        if (auto exception = validateAndUpdateURLPatternComponent(condition.urlPattern->password, EncodingCallbackType::Password, condition.urlPattern->shouldIgnoreCase))
            return exception;
        if (auto exception = validateAndUpdateURLPatternComponent(condition.urlPattern->hostname, EncodingCallbackType::Host, condition.urlPattern->shouldIgnoreCase))
            return exception;
        if (auto exception = validateAndUpdateURLPatternComponent(condition.urlPattern->pathname, EncodingCallbackType::Path, condition.urlPattern->shouldIgnoreCase))
            return exception;
        if (auto exception = validateAndUpdateURLPatternComponent(condition.urlPattern->port, EncodingCallbackType::Port, condition.urlPattern->shouldIgnoreCase))
            return exception;
        if (auto exception = validateAndUpdateURLPatternComponent(condition.urlPattern->search, EncodingCallbackType::Search, condition.urlPattern->shouldIgnoreCase))
            return exception;
        if (auto exception = validateAndUpdateURLPatternComponent(condition.urlPattern->hash, EncodingCallbackType::Hash, condition.urlPattern->shouldIgnoreCase))
            return exception;
    }

    Vector<ServiceWorkerRouteCondition> orConditions;
    for (auto& orCondition : condition.orConditions) {
        if (auto exception = validateServiceWorkerRouteCondition(orCondition))
            return *exception;
    }

    if (condition.notCondition) {
        if (auto exception = validateServiceWorkerRouteCondition(*condition.notCondition))
            return *exception;
    }

    if (!condition.requestMethod.isNull()) {
        if (!isValidHTTPToken(condition.requestMethod))
            return ExceptionData { ExceptionCode::TypeError, "Method is not a valid HTTP token."_s };
        if (isForbiddenMethod(condition.requestMethod))
            return ExceptionData { ExceptionCode::TypeError, "Method is forbidden."_s };

        condition.requestMethod = normalizeHTTPMethod(condition.requestMethod);
    }

    return { };
}

std::optional<ExceptionData> validateServiceWorkerRoute(ServiceWorkerRoute& route)
{
    return validateServiceWorkerRouteCondition(route.condition);
}

static bool matchURLPatternComponent(const String& pattern, StringView value, bool shouldIgnoreCase)
{
    if (pattern.isNull())
        return true;

#if !PLATFORM(COCOA)
    UNUSED_PARAM(shouldIgnoreCase);
    // FIXME: Fully support pattern matching, check for case, whitespace...
    bool isPatternFinishingByStar = pattern.endsWith("*"_s);
    return isPatternFinishingByStar ? value.startsWith(pattern.substring(pattern.length() - 1)) : value == pattern;
#else
    return isRegexpMatching(pattern, value, shouldIgnoreCase);
#endif
}

static bool matchURLPattern(const ServiceWorkerRoutePattern& urlPattern, const URL& url)
{
    if (!matchURLPatternComponent(urlPattern.protocol, url.protocol(), urlPattern.shouldIgnoreCase))
        return false;

    if (!matchURLPatternComponent(urlPattern.username, url.encodedUser(), urlPattern.shouldIgnoreCase))
        return false;

    if (!matchURLPatternComponent(urlPattern.password, url.encodedPassword(), urlPattern.shouldIgnoreCase))
        return false;

    if (!matchURLPatternComponent(urlPattern.hostname, url.host(), urlPattern.shouldIgnoreCase))
        return false;

    String port;
    if (auto portNumber = url.port())
        port = String::number(*portNumber);
    if (!matchURLPatternComponent(urlPattern.port, port, urlPattern.shouldIgnoreCase))
        return false;

    if (!matchURLPatternComponent(urlPattern.pathname, url.path(), urlPattern.shouldIgnoreCase))
        return false;

    if (!matchURLPatternComponent(urlPattern.search, url.query(), urlPattern.shouldIgnoreCase))
        return false;

    return matchURLPatternComponent(urlPattern.hash, url.fragmentIdentifier(), urlPattern.shouldIgnoreCase);
}

// https://w3c.github.io/ServiceWorker/#match-router-condition
bool matchRouterCondition(const ServiceWorkerRouteCondition& condition, const FetchOptions& options, const ResourceRequest& request, bool isServiceWorkerRunning)
{
    if (!condition.orConditions.isEmpty()) {
        for (auto& condition : condition.orConditions) {
            if (matchRouterCondition(condition, options, request, isServiceWorkerRunning))
                return true;
        }
        return false;
    }

    if (condition.notCondition)
        return !matchRouterCondition(*condition.notCondition, options, request, isServiceWorkerRunning);

    if (condition.urlPattern) {
        if (!matchURLPattern(*condition.urlPattern, request.url()))
            return false;
    }

    if (!condition.requestMethod.isNull()) {
        if (condition.requestMethod != request.httpMethod())
            return false;
    }

    if (condition.requestMode) {
        if (*condition.requestMode != options.mode)
            return false;
    }

    if (condition.requestDestination) {
        if (*condition.requestDestination != options.destination)
            return false;
    }

    if (condition.runningStatus) {
        bool isRunningStatus = *condition.runningStatus == RunningStatus::Running;
        if (isRunningStatus != isServiceWorkerRunning)
            return false;
    }

    return true;
}

ServiceWorkerRouteCondition ServiceWorkerRouteCondition::isolatedCopy() &&
{
    std::unique_ptr<ServiceWorkerRouteCondition> notConditionCopy;
    if (notCondition)
        notConditionCopy = makeUnique<ServiceWorkerRouteCondition>(WTF::move(*notCondition));
    return {
        crossThreadCopy(WTF::move(urlPattern)),
        crossThreadCopy(WTF::move(requestMethod)),
        requestMode,
        requestDestination,
        runningStatus,
        crossThreadCopy(WTF::move(orConditions)),
        WTF::move(notConditionCopy)
    };
}

ServiceWorkerRouteCondition ServiceWorkerRouteCondition::isolatedCopy() const &
{
    std::unique_ptr<ServiceWorkerRouteCondition> notConditionCopy;
    if (notCondition)
        notConditionCopy = makeUnique<ServiceWorkerRouteCondition>(notCondition->isolatedCopy());
    return {
        crossThreadCopy(urlPattern),
        crossThreadCopy(requestMethod),
        requestMode,
        requestDestination,
        runningStatus,
        crossThreadCopy(orConditions),
        WTF::move(notConditionCopy)
    };
}

ServiceWorkerRouteCondition ServiceWorkerRouteCondition::copy() const
{
    std::unique_ptr<ServiceWorkerRouteCondition> notConditionCopy;
    if (notCondition)
        notConditionCopy = makeUnique<ServiceWorkerRouteCondition>(notCondition->copy());

    return {
        urlPattern,
        requestMethod,
        requestMode,
        requestDestination,
        runningStatus,
        orConditions.map([](auto& condition) { return condition.copy(); }),
        WTF::move(notConditionCopy)
    };
}

ServiceWorkerRoutePattern ServiceWorkerRoutePattern::isolatedCopy() &&
{
    return {
        shouldIgnoreCase,
        crossThreadCopy(WTF::move(protocol)),
        crossThreadCopy(WTF::move(username)),
        crossThreadCopy(WTF::move(password)),
        crossThreadCopy(WTF::move(hostname)),
        crossThreadCopy(WTF::move(port)),
        crossThreadCopy(WTF::move(pathname)),
        crossThreadCopy(WTF::move(search)),
        crossThreadCopy(WTF::move(hash))
    };
}

ServiceWorkerRoutePattern ServiceWorkerRoutePattern::isolatedCopy() const  &
{
    return {
        shouldIgnoreCase,
        crossThreadCopy(protocol),
        crossThreadCopy(username),
        crossThreadCopy(password),
        crossThreadCopy(hostname),
        crossThreadCopy(port),
        crossThreadCopy(pathname),
        crossThreadCopy(search),
        crossThreadCopy(hash)
    };
}

static RouterSource crossThreadCopyRouterSource(RouterSource&& source)
{
    return WTF::switchOn(source, [](RouterSourceDict& dict) -> RouterSource {
        return WTF::move(dict).isolatedCopy();
    }, [](auto value) -> RouterSource {
        return value;
    });
}

static RouterSource crossThreadCopyRouterSource(const RouterSource& source)
{
    return WTF::switchOn(source, [](const RouterSourceDict& dict) -> RouterSource {
        return dict.isolatedCopy();
    }, [](auto value) -> RouterSource {
        return value;
    });
}

ServiceWorkerRoute ServiceWorkerRoute::isolatedCopy() &&
{
    return {
        WTF::move(condition).isolatedCopy(),
        crossThreadCopyRouterSource(WTF::move(source))
    };
}

ServiceWorkerRoute ServiceWorkerRoute::isolatedCopy() const &
{
    return {
        condition.isolatedCopy(),
        crossThreadCopyRouterSource(source)
    };
}

} // namespace WebCore
