/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "NavigatorContentUtils.h"

#if ENABLE(NAVIGATOR_CONTENT_UTILS)

#include "Document.h"
#include "Frame.h"
#include "Navigator.h"
#include "Page.h"
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static bool verifyCustomHandlerURL(const URL& baseURL, const String& url)
{
    // The specification requires that it is a SyntaxError if the "%s" token is
    // not present.
    static const char token[] = "%s";
    int index = url.find(token);
    if (-1 == index)
        return false;

    // It is also a SyntaxError if the custom handler URL, as created by removing
    // the "%s" token and prepending the base url, does not resolve.
    String newURL = url;
    newURL.remove(index, WTF_ARRAY_LENGTH(token) - 1);

    URL kurl(baseURL, newURL);

    if (kurl.isEmpty() || !kurl.isValid())
        return false;

    return true;
}

static inline bool isProtocolWhitelisted(const String& scheme)
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> protocolWhitelist = []() {
        HashSet<String, ASCIICaseInsensitiveHash> set;
        for (auto* protocol : { "bitcoin", "geo", "im", "irc", "ircs", "magnet", "mailto", "mms", "news", "nntp", "sip", "sms", "smsto", "ssh", "tel", "urn", "webcal", "wtai", "xmpp" })
            set.add(protocol);
        return set;
    }();
    return protocolWhitelist.get().contains(scheme);
}

static bool verifyProtocolHandlerScheme(const String& scheme)
{
    if (isProtocolWhitelisted(scheme))
        return true;

    // FIXME: Should this be case sensitive, or should it be ASCII case-insensitive?
    if (scheme.startsWith("web+")) {
        // The specification requires that the length of scheme is at least five characters (including 'web+' prefix).
        if (scheme.length() >= 5 && isValidProtocol(scheme))
            return true;
    }

    return false;
}

NavigatorContentUtils* NavigatorContentUtils::from(Page* page)
{
    return static_cast<NavigatorContentUtils*>(Supplement<Page>::from(page, supplementName()));
}

NavigatorContentUtils::~NavigatorContentUtils() = default;

ExceptionOr<void> NavigatorContentUtils::registerProtocolHandler(Navigator& navigator, const String& scheme, const String& url, const String& title)
{
    if (!navigator.frame())
        return { };

    URL baseURL = navigator.frame()->document()->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url))
        return Exception { SyntaxError };

    if (!verifyProtocolHandlerScheme(scheme))
        return Exception { SecurityError };

    NavigatorContentUtils::from(navigator.frame()->page())->client()->registerProtocolHandler(scheme, baseURL, URL({ }, url), navigator.frame()->displayStringModifiedByEncoding(title));
    return { };
}

#if ENABLE(CUSTOM_SCHEME_HANDLER)

static String customHandlersStateString(const NavigatorContentUtilsClient::CustomHandlersState state)
{
    static NeverDestroyed<String> newHandler(MAKE_STATIC_STRING_IMPL("new"));
    static NeverDestroyed<String> registeredHandler(MAKE_STATIC_STRING_IMPL("registered"));
    static NeverDestroyed<String> declinedHandler(MAKE_STATIC_STRING_IMPL("declined"));

    switch (state) {
    case NavigatorContentUtilsClient::CustomHandlersNew:
        return newHandler;
    case NavigatorContentUtilsClient::CustomHandlersRegistered:
        return registeredHandler;
    case NavigatorContentUtilsClient::CustomHandlersDeclined:
        return declinedHandler;
    }

    ASSERT_NOT_REACHED();
    return String();
}

ExceptionOr<String> NavigatorContentUtils::isProtocolHandlerRegistered(Navigator& navigator, const String& scheme, const String& url)
{
    static NeverDestroyed<String> declined(MAKE_STATIC_STRING_IMPL("declined"));

    if (!navigator.frame())
        return String { declined };

    URL baseURL = navigator.frame()->document()->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url))
        return Exception { SyntaxError };

    if (!verifyProtocolHandlerScheme(scheme))
        return Exception { SecurityError };

    return customHandlersStateString(NavigatorContentUtils::from(navigator.frame()->page())->client()->isProtocolHandlerRegistered(scheme, baseURL, URL({ }, url)));
}

ExceptionOr<void> NavigatorContentUtils::unregisterProtocolHandler(Navigator& navigator, const String& scheme, const String& url)
{
    if (!navigator.frame())
        return { };

    URL baseURL = navigator.frame()->document()->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url))
        return Exception { SyntaxError };

    if (!verifyProtocolHandlerScheme(scheme))
        return Exception { SecurityError };

    NavigatorContentUtils::from(navigator.frame()->page())->client()->unregisterProtocolHandler(scheme, baseURL, URL({ }, url));
    return { };
}

#endif

const char* NavigatorContentUtils::supplementName()
{
    return "NavigatorContentUtils";
}

void provideNavigatorContentUtilsTo(Page* page, std::unique_ptr<NavigatorContentUtilsClient> client)
{
    NavigatorContentUtils::provideTo(page, NavigatorContentUtils::supplementName(), std::make_unique<NavigatorContentUtils>(WTFMove(client)));
}

} // namespace WebCore

#endif // ENABLE(NAVIGATOR_CONTENT_UTILS)
