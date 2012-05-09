/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
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
#include "NavigatorRegisterProtocolHandler.h"

#if ENABLE(REGISTER_PROTOCOL_HANDLER)

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "Navigator.h"
#include "Page.h"
#include <wtf/HashSet.h>

namespace WebCore {

static HashSet<String>* protocolWhitelist;

static void initProtocolHandlerWhitelist()
{
    protocolWhitelist = new HashSet<String>;
    static const char* protocols[] = {
        "irc",
        "mailto",
        "mms",
        "news",
        "nntp",
        "sms",
        "smsto",
        "tel",
        "urn",
        "webcal",
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(protocols); ++i)
        protocolWhitelist->add(protocols[i]);
}

static bool verifyCustomHandlerURL(const String& baseURL, const String& url, ExceptionCode& ec)
{
    // The specification requires that it is a SYNTAX_ERR if the "%s" token is
    // not present.
    static const char token[] = "%s";
    int index = url.find(token);
    if (-1 == index) {
        ec = SYNTAX_ERR;
        return false;
    }

    // It is also a SYNTAX_ERR if the custom handler URL, as created by removing
    // the "%s" token and prepending the base url, does not resolve.
    String newURL = url;
    newURL.remove(index, WTF_ARRAY_LENGTH(token) - 1);

    KURL base(ParsedURLString, baseURL);
    KURL kurl(base, newURL);

    if (kurl.isEmpty() || !kurl.isValid()) {
        ec = SYNTAX_ERR;
        return false;
    }

    return true;
}

static bool isProtocolWhitelisted(const String& scheme)
{
    if (!protocolWhitelist)
        initProtocolHandlerWhitelist();
    return protocolWhitelist->contains(scheme);
}

static bool verifyProtocolHandlerScheme(const String& scheme, ExceptionCode& ec)
{
    if (scheme.startsWith("web+")) {
        if (isValidProtocol(scheme))
            return true;
        ec = SECURITY_ERR;
        return false;
    }

    if (isProtocolWhitelisted(scheme))
        return true;
    ec = SECURITY_ERR;
    return false;
}

NavigatorRegisterProtocolHandler::NavigatorRegisterProtocolHandler()
{
}

NavigatorRegisterProtocolHandler::~NavigatorRegisterProtocolHandler()
{
}

void NavigatorRegisterProtocolHandler::registerProtocolHandler(Navigator* navigator, const String& scheme, const String& url, const String& title, ExceptionCode& ec)
{
    if (!navigator->frame())
        return;

    Document* document = navigator->frame()->document();
    if (!document)
        return;

    String baseURL = document->baseURL().baseAsString();

    if (!verifyCustomHandlerURL(baseURL, url, ec))
        return;

    if (!verifyProtocolHandlerScheme(scheme, ec))
        return;

    Page* page = navigator->frame()->page();
    if (!page)
        return;

    page->chrome()->client()->registerProtocolHandler(scheme, baseURL, url, navigator->frame()->displayStringModifiedByEncoding(title));
}

} // namespace WebCore

#endif // ENABLE(REGISTER_PROTOCOL_HANDLER)
