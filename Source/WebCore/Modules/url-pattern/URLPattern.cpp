/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "URLPattern.h"

#include "ScriptExecutionContext.h"
#include "URLPatternCanonical.h"
#include "URLPatternInit.h"
#include "URLPatternOptions.h"
#include "URLPatternParser.h"
#include "URLPatternResult.h"
#include <JavaScriptCore/RegExp.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/URLParser.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(URLPattern);

// https://urlpattern.spec.whatwg.org/#process-a-base-url-string
static String processBaseURLString(StringView input, BaseURLStringType type)
{
    if (type != BaseURLStringType::Pattern)
        return input.toString();

    return URLPatternUtilities::escapePatternString(input);
}

URLPattern::URLPattern() = default;

// https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
static ExceptionOr<URLPatternInit> processInit(URLPatternInit&& init, BaseURLStringType type, String&& protocol = { }, String&& username = { }, String&& password = { }, String&& hostname = { }, String&& port = { }, String&& pathname = { }, String&& search = { }, String&& hash = { })
{
    URLPatternInit result { WTFMove(protocol), WTFMove(username), WTFMove(password), WTFMove(hostname), WTFMove(port), WTFMove(pathname), WTFMove(search), WTFMove(hash), { } };

    URL baseURL;

    if  (!init.baseURL.isNull()) {
        baseURL = URL(init.baseURL);

        if (!baseURL.isValid()) {
            // FIXME: Check if empty string should be allowed.
            return Exception { ExceptionCode::TypeError, "Invalid baseURL."_s };
        }

        if (init.protocol.isNull())
            result.protocol = processBaseURLString(baseURL.protocol(), type);

        if (type != BaseURLStringType::Pattern
            && init.protocol.isNull()
            && init.hostname.isNull()
            && init.port.isNull()
            && init.username.isNull())
            result.username = processBaseURLString(baseURL.user(), type);

        if (type != BaseURLStringType::Pattern
            && init.protocol.isNull()
            && init.hostname.isNull()
            && init.port.isNull()
            && init.username.isNull()
            && init.password.isNull())
            result.password = processBaseURLString(baseURL.password(), type);

        if (init.protocol.isNull()
            && init.hostname.isNull()) {
            result.hostname = processBaseURLString(!baseURL.host().isNull() ? baseURL.host() : StringView { emptyString() }, type);
        }

        if (init.protocol.isNull()
            && init.hostname.isNull()
            && init.port.isNull()) {
            auto port = baseURL.port();
            result.port = port ? String::number(*port) : emptyString();
        }

        if (init.protocol.isNull()
            && init.hostname.isNull()
            && init.port.isNull()
            && init.pathname.isNull()) {
            result.pathname = processBaseURLString(baseURL.path(), type);
        }

        if (init.protocol.isNull()
            && init.hostname.isNull()
            && init.port.isNull()
            && init.pathname.isNull()
            && init.search.isNull()) {
            result.search = processBaseURLString(baseURL.hasQuery() ? baseURL.query() : StringView { emptyString() }, type);
        }

        if (init.protocol.isNull()
            && init.hostname.isNull()
            && init.port.isNull()
            && init.pathname.isNull()
            && init.search.isNull()
            && init.hash.isNull()) {
            result.hash = processBaseURLString(baseURL.hasFragmentIdentifier() ? baseURL.fragmentIdentifier() : StringView { emptyString() }, type);
        }
    }

    if (!init.protocol.isNull()) {
        auto protocolResult = canonicalizeProtocol(init.protocol, type);

        if (protocolResult.hasException())
            return protocolResult.releaseException();

        result.protocol = protocolResult.releaseReturnValue();
    }

    if (!init.username.isNull())
        result.username = canonicalizeUsername(init.username, type);

    if (!init.password.isNull())
        result.password = canonicalizePassword(init.password, type);

    if (!init.hostname.isNull()) {
        auto hostResult = canonicalizeHostname(init.hostname, type);

        if (hostResult.hasException())
            return hostResult.releaseException();

        result.hostname = hostResult.releaseReturnValue();
    }

    if (!init.port.isNull()) {
        auto portResult = canonicalizePort(init.port, init.protocol, type);

        if (portResult.hasException())
            return portResult.releaseException();

        result.port = portResult.releaseReturnValue();
    }

    if (!init.pathname.isNull()) {
        result.pathname = init.pathname;

        if (!baseURL.isNull() && baseURL.hasOpaquePath() && !isAbsolutePathname(result.pathname, type)) {
            auto baseURLPath = processBaseURLString(baseURL.path(), type);
            size_t slashIndex = baseURLPath.reverseFind('/');

            if (slashIndex != notFound)
                result.pathname = makeString(StringView { baseURLPath }.left(slashIndex + 1), result.pathname);

            auto pathResult = processPathname(result.pathname, baseURL.protocol(), type);

            if (pathResult.hasException())
                return pathResult.releaseException();

            result.pathname = pathResult.releaseReturnValue();
        }
    }

    if (!init.search.isNull()) {
        auto queryResult = canonicalizeSearch(init.search, type);

        if (queryResult.hasException())
            return queryResult.releaseException();

        result.search = queryResult.releaseReturnValue();
    }

    if (!init.hash.isNull()) {
        auto fragmentResult = canonicalizeHash(init.hash, type);

        if (fragmentResult.hasException())
            return fragmentResult.releaseException();

        result.hash = fragmentResult.releaseReturnValue();
    }

    return result;
}

ExceptionOr<Ref<URLPattern>> URLPattern::create(ScriptExecutionContext& context, URLPatternInput&&, String&& baseURL, URLPatternOptions&&)
{
    UNUSED_PARAM(baseURL);
    UNUSED_PARAM(context);

    return Exception { ExceptionCode::NotSupportedError, "Not implemented."_s };
}

ExceptionOr<Ref<URLPattern>> URLPattern::create(ScriptExecutionContext& context, std::optional<URLPatternInput>&& input, URLPatternOptions&& options)
{
    URLPatternInit init;

    if (!input)
        return Exception { ExceptionCode::NotSupportedError, "Not implemented."_s };
    if (std::holds_alternative<String>(*input) && !std::get<String>(*input).isNull())
        return Exception { ExceptionCode::NotSupportedError, "Not implemented."_s };
    if (std::holds_alternative<URLPatternInit>(*input))
        init = std::get<URLPatternInit>(*input);

    auto maybeProcessedInit = processInit(WTFMove(init), BaseURLStringType::Pattern);

    if (maybeProcessedInit.hasException())
        return maybeProcessedInit.releaseException();

    auto processedInit = maybeProcessedInit.releaseReturnValue();
    if (!processedInit.protocol)
        processedInit.protocol = "*"_s;
    if (!processedInit.username)
        processedInit.username = "*"_s;
    if (!processedInit.password)
        processedInit.password = "*"_s;
    if (!processedInit.hostname)
        processedInit.hostname= "*"_s;
    if (!processedInit.pathname)
        processedInit.pathname = "*"_s;
    if (!processedInit.search)
        processedInit.search = "*"_s;
    if (!processedInit.hash)
        processedInit.hash = "*"_s;
    if (!processedInit.port)
        processedInit.port = "*"_s;

    if (auto parsedPort = parseInteger<uint16_t>(processedInit.port)) {
        if (WTF::URLParser::isSpecialScheme(processedInit.protocol) && isDefaultPortForProtocol(*parsedPort, processedInit.protocol))
            processedInit.port = emptyString();
    }

    Ref result = adoptRef(*new URLPattern);

    auto maybeCompileException = result->compileAllComponents(context, WTFMove(processedInit), options);
    if (maybeCompileException.hasException())
        return maybeCompileException.releaseException();

    return result;
}

URLPattern::~URLPattern() = default;

ExceptionOr<bool> URLPattern::test(std::optional<URLPatternInput>&&, String&& baseURL) const
{
    UNUSED_PARAM(baseURL);

    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<std::optional<URLPatternResult>> URLPattern::exec(std::optional<URLPatternInput>&&, String&& baseURL) const
{
    UNUSED_PARAM(baseURL);

    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<void> URLPattern::compileAllComponents(ScriptExecutionContext& context, URLPatternInit&& processedInit, const URLPatternOptions& options)
{
    Ref vm = context.vm();
    JSLockHolder lock(vm);

    auto maybeProtocolComponent = URLPatternUtilities::URLPatternComponent::compile(vm, processedInit.protocol, EncodingCallbackType::Protocol, URLPatternUtilities::URLPatternStringOptions { });
    if (maybeProtocolComponent.hasException())
        return maybeProtocolComponent.releaseException();
    m_protocolComponent = maybeProtocolComponent.releaseReturnValue();

    auto maybeUsernameComponent = URLPatternUtilities::URLPatternComponent::compile(vm, processedInit.username, EncodingCallbackType::Username, URLPatternUtilities::URLPatternStringOptions { });
    if (maybeUsernameComponent.hasException())
        return maybeUsernameComponent.releaseException();
    m_usernameComponent = maybeUsernameComponent.releaseReturnValue();

    auto maybePasswordComponent = URLPatternUtilities::URLPatternComponent::compile(vm, processedInit.password, EncodingCallbackType::Password, URLPatternUtilities::URLPatternStringOptions { });
    if (maybePasswordComponent.hasException())
        return maybePasswordComponent.releaseException();
    m_passwordComponent = maybePasswordComponent.releaseReturnValue();

    auto hostnameEncodingCallbackType = URL::isIPv6Address(processedInit.hostname) ? EncodingCallbackType::IPv6Host : EncodingCallbackType::Host;
    auto maybeHostnameComponent = URLPatternUtilities::URLPatternComponent::compile(vm, processedInit.hostname, hostnameEncodingCallbackType, URLPatternUtilities::URLPatternStringOptions { .delimiterCodepoint = "."_s });
    if (maybeHostnameComponent.hasException())
        return maybeHostnameComponent.releaseException();
    m_hostnameComponent = maybeHostnameComponent.releaseReturnValue();

    auto maybePortComponent = URLPatternUtilities::URLPatternComponent::compile(vm, processedInit.port, EncodingCallbackType::Port, URLPatternUtilities::URLPatternStringOptions { });
    if (maybePortComponent.hasException())
        return maybePortComponent.releaseException();
    m_portComponent = maybePortComponent.releaseReturnValue();

    URLPatternUtilities::URLPatternStringOptions compileOptions { .ignoreCase = options.ignoreCase };

    // FIXME: Implement https://urlpattern.spec.whatwg.org/#protocol-component-matches-a-special-scheme
    m_pathname = processedInit.pathname;

    auto maybeSearchComponent = URLPatternUtilities::URLPatternComponent::compile(vm, processedInit.search, EncodingCallbackType::Search, compileOptions);
    if (maybeSearchComponent.hasException())
        return maybeSearchComponent.releaseException();
    m_searchComponent = maybeSearchComponent.releaseReturnValue();

    auto maybeHashComponent = URLPatternUtilities::URLPatternComponent::compile(vm, processedInit.hash, EncodingCallbackType::Hash, compileOptions);
    if (maybeHashComponent.hasException())
        return maybeHashComponent.releaseException();
    m_hashComponent = maybeHashComponent.releaseReturnValue();

    return { };
}

}
