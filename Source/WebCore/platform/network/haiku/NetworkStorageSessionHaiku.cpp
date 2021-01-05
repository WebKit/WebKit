/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 University of Szeged. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS''
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
#include "NetworkStorageSession.h"

#include <support/Locker.h>
#include <UrlContext.h>

#include "Cookie.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "HTTPCookieAcceptPolicy.h"
#include "NetworkingContext.h"
#include "NotImplemented.h"
#include "ResourceHandle.h"
#include "wtf/URL.h"

#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#define TRACE_COOKIE_JAR 0

namespace WebCore {

NetworkStorageSession::NetworkStorageSession(PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_context(nullptr)
{
}

NetworkStorageSession::~NetworkStorageSession()
{
}

static std::unique_ptr<NetworkStorageSession>& defaultSession()
{
    ASSERT(isMainThread());
    static NeverDestroyed<std::unique_ptr<NetworkStorageSession>> session;
    return session;
}

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty,
	const SameSiteInfo& sameSiteInfo, const URL& url,
	WTF::Optional<FrameIdentifier> frameID, WTF::Optional<PageIdentifier> pageID,
	ShouldAskITP, const String& value, ShouldRelaxThirdPartyCookieBlocking) const
{
	BNetworkCookie* heapCookie
		= new BNetworkCookie(value, BUrl(url));

#if TRACE_COOKIE_JAR
	printf("CookieJar: Add %s for %s\n", heapCookie->RawCookie(true).String(),
        url.string().utf8().data());
	printf("  from %s\n", value.utf8().data());
#endif
	platformSession().GetCookieJar().AddCookie(heapCookie);
}

HTTPCookieAcceptPolicy NetworkStorageSession::cookieAcceptPolicy() const
{
    return HTTPCookieAcceptPolicy::AlwaysAccept;
}

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty,
	const SameSiteInfo& sameSiteInfo, const URL& url,
	WTF::Optional<FrameIdentifier> frameID, WTF::Optional<PageIdentifier> pageID,
	IncludeSecureCookies includeSecureCookies, ShouldAskITP,
	ShouldRelaxThirdPartyCookieBlocking) const
{
#if TRACE_COOKIE_JAR
	printf("CookieJar: Request for %s\n", url.string().utf8().data());
#endif

	BString result;
	BUrl hUrl(url);
	bool secure = false;

	const BNetworkCookie* c;
	for (BNetworkCookieJar::UrlIterator it(
            platformSession().GetCookieJar().GetUrlIterator(hUrl));
		    (c = it.Next()); ) {
        // filter out httpOnly cookies,as this method is used to get cookies
        // from JS code and these shouldn't be visible there.
        if(c->HttpOnly())
			continue;

		// filter out secure cookies if they should be
		if (c->Secure())
		{
			secure = true;
            if (includeSecureCookies == IncludeSecureCookies::No)
				continue;
		}
		
		result << "; " << c->RawCookie(false);
	}
	result.Remove(0, 2);

    return {result, secure};
}

void NetworkStorageSession::setCookies(const Vector<Cookie>&, const URL&, const URL&)
{
    // FIXME: Implement for WebKit to use.
}

void NetworkStorageSession::setCookie(const Cookie&)
{
    // FIXME: Implement for WebKit to use.
}

void NetworkStorageSession::deleteCookie(const Cookie&)
{
    // FIXME: Implement for WebKit to use.
}

void NetworkStorageSession::deleteCookie(const URL& url, const String& cookie) const
{
#if TRACE_COOKIE_JAR
	printf("CookieJar: delete cookie for %s (NOT IMPLEMENTED)\n", url.string().utf8().data());
#endif
	notImplemented();
}

void NetworkStorageSession::deleteAllCookies()
{
    notImplemented();
}

void NetworkStorageSession::deleteAllCookiesModifiedSince(WallTime since)
{
    notImplemented();
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& cookieHostNames)
{
    notImplemented();
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    // FIXME: Implement for WebKit to use.
    return { };
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& hostnames)
{
    notImplemented();
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL&)
{
    // FIXME: Implement for WebKit to use.
    return { };
}

bool NetworkStorageSession::getRawCookies(const URL& firstParty,
	const SameSiteInfo& sameSiteInfo, const URL& url, WTF::Optional<FrameIdentifier> frameID,
	WTF::Optional<PageIdentifier> pageID, ShouldAskITP, ShouldRelaxThirdPartyCookieBlocking, Vector<Cookie>& rawCookies) const
{
#if TRACE_COOKIE_JAR
	printf("CookieJar: get raw cookies for %s (NOT IMPLEMENTED)\n", url.string().utf8().data());
#endif
	notImplemented();

    rawCookies.clear();
    return false; // return true when implemented
}

void NetworkStorageSession::flushCookieStore()
{
    // FIXME: Implement for WebKit to use.
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty,
	const SameSiteInfo& sameSiteInfo, const URL& url, WTF::Optional<FrameIdentifier> frameID,
	WTF::Optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ShouldAskITP,
	ShouldRelaxThirdPartyCookieBlocking) const
{
#if TRACE_COOKIE_JAR
	printf("CookieJar: RequestHeaderField for %s\n", url.string().utf8().data());
#endif

	BString result;
	BUrl hUrl(url);
	bool secure = false;

	const BNetworkCookie* c;
	for (BNetworkCookieJar::UrlIterator it(
        	platformSession().GetCookieJar().GetUrlIterator(hUrl));
		    (c = it.Next()); ) {
		// filter out secure cookies if they should be
		if (c->Secure())
		{
			secure = true;
            if (includeSecureCookies == IncludeSecureCookies::No)
				continue;
		}
		
		result << "; " << c->RawCookie(false);
	}
	result.Remove(0, 2);

    return {result, secure};
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(
    const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookieRequestHeaderFieldValue(headerFieldProxy.firstParty,
        headerFieldProxy.sameSiteInfo, headerFieldProxy.url,
        headerFieldProxy.frameID, headerFieldProxy.pageID,
        headerFieldProxy.includeSecureCookies, ShouldAskITP::Yes,
        ShouldRelaxThirdPartyCookieBlocking::No);
}

BUrlContext& NetworkStorageSession::platformSession() const
{
    static BUrlContext sDefaultContext;
    return m_context ? *m_context : sDefaultContext;
}

void NetworkStorageSession::setPlatformSession(BUrlContext* context)
{
    m_context = context;
}

}

