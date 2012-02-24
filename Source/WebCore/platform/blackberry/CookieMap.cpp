/*
 * Copyright (C) 2008, 2009 Julien Chaffraix <julien.chaffraix@gmail.com>
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define ENABLE_COOKIE_DEBUG 0

#include "config.h"
#include "CookieMap.h"

#include "CookieManager.h"
#include "Logging.h"
#include "ParsedCookie.h"
#include <wtf/text/CString.h>

#if ENABLE_COOKIE_DEBUG
#include <BlackBerryPlatformLog.h>
#define CookieLog(format, ...) BlackBerry::Platform::logAlways(BlackBerry::Platform::LogLevelInfo, format, ## __VA_ARGS__)
#else
#define CookieLog(format, ...)
#endif // ENABLE_COOKIE_DEBUG

namespace WebCore {

CookieMap::CookieMap(const String& name)
    : m_oldestCookie(0)
    , m_name(name)
{
}

CookieMap::~CookieMap()
{
    deleteAllCookiesAndDomains();
}

bool CookieMap::existsCookie(const ParsedCookie* cookie) const
{
    String key = cookie->name() + cookie->path();
    return m_cookieMap.contains(key);
}

void CookieMap::addCookie(ParsedCookie* cookie)
{
    String key = cookie->name() + cookie->path();

    CookieLog("CookieMap - Attempting to add cookie - %s", cookie->name().utf8().data());

    ASSERT(!m_cookieMap.contains(key));
    m_cookieMap.add(key, cookie);
    if (!cookie->isSession())
        cookieManager().addedCookie();
    if (!m_oldestCookie || m_oldestCookie->lastAccessed() > cookie->lastAccessed())
        m_oldestCookie = cookie;
}

ParsedCookie* CookieMap::updateCookie(ParsedCookie* newCookie)
{
    String key = newCookie->name() + newCookie->path();
    ParsedCookie* oldCookie = m_cookieMap.take(key);
    ASSERT(oldCookie);
    m_cookieMap.add(key, newCookie);
    if (oldCookie == m_oldestCookie)
        updateOldestCookie();
    return oldCookie;
}

ParsedCookie* CookieMap::removeCookie(const ParsedCookie* cookie)
{
    // Find a previous entry for deletion
    String key = cookie->name() + cookie->path();
    ParsedCookie* prevCookie = m_cookieMap.take(key);

    if (!prevCookie)
        return 0;

    if (prevCookie == m_oldestCookie)
        updateOldestCookie();
    else if (prevCookie != cookie) {
        // The cookie we used to search is force expired, we must do the same
        // to the cookie in memory too.
        if (cookie->isForceExpired())
            prevCookie->forceExpire();
        delete cookie;
    }

    if (!prevCookie->isSession())
        cookieManager().removedCookie();
    return prevCookie;
}

CookieMap* CookieMap::getSubdomainMap(const String& subdomain)
{
#if ENABLE_COOKIE_DEBUG
    if (!m_subdomains.contains(subdomain))
        CookieLog("CookieMap - %s does not exist in this map", subdomain.utf8().data());
#endif
    return m_subdomains.get(subdomain);
}

void CookieMap::addSubdomainMap(const String& subdomain, CookieMap* newDomain)
{
    CookieLog("CookieMap - Attempting to add subdomain - %s", subdomain.utf8().data());
    m_subdomains.add(subdomain, newDomain);
}

void CookieMap::getAllCookies(Vector<ParsedCookie*>* stackOfCookies)
{
    CookieLog("CookieMap - Attempting to copy Map:%s cookies with %d cookies into vectors", m_name.utf8().data(), m_cookieMap.size());

    Vector<ParsedCookie*> newCookies;
    copyValuesToVector(m_cookieMap, newCookies);
    for (size_t i = 0; i < newCookies.size(); i++) {
        ParsedCookie* newCookie = newCookies[i];
        if (newCookie->hasExpired()) {
            // Notice that we don't delete from backingstore. These expired cookies will be
            // deleted when manager loads the backingstore again.
            ParsedCookie* expired = removeCookie(newCookie);
            delete expired;
        } else
            stackOfCookies->append(newCookie);
    }

    CookieLog("CookieMap - stack of cookies now have %d cookies in it", (*stackOfCookies).size());
}

ParsedCookie* CookieMap::removeOldestCookie()
{
    // FIXME: Make sure it finds the GLOBAL oldest cookie, not the first oldestcookie it finds.
    ParsedCookie* oldestCookie = m_oldestCookie;

    // If this map has an oldestCookie, remove it. If not, do a DFS to search for a child that does
    if (!oldestCookie) {

        CookieLog("CookieMap - no oldestCookie exist");

        // Base case is if the map has no child and no cookies, we return a null.
        if (!m_subdomains.size()) {
            CookieLog("CookieMap - no subdomains, base case reached, return 0");
            return 0;
        }

        CookieLog("CookieMap - looking into subdomains");

        for (HashMap<String, CookieMap*>::iterator it = m_subdomains.begin(); it != m_subdomains.end(); ++it) {
            oldestCookie = it->second->removeOldestCookie();
            if (oldestCookie)
                break;
        }
    } else {
        CookieLog("CookieMap - oldestCookie exist.");
        oldestCookie = removeCookie(m_oldestCookie);
    }

    return oldestCookie;
}

void CookieMap::updateOldestCookie()
{
    if (!m_cookieMap.size())
        m_oldestCookie = 0;
    else {
        HashMap<String, ParsedCookie*>::iterator it = m_cookieMap.begin();
        m_oldestCookie = it->second;
        ++it;
        for (; it != m_cookieMap.end(); ++it)
            if (m_oldestCookie->lastAccessed() > it->second->lastAccessed())
                m_oldestCookie = it->second;
    }
}

void CookieMap::deleteAllCookiesAndDomains()
{
    deleteAllValues(m_subdomains);
    m_subdomains.clear();
    deleteAllValues(m_cookieMap);
    m_cookieMap.clear();

    m_oldestCookie = 0;
}

void CookieMap::getAllChildCookies(Vector<ParsedCookie*>* stackOfCookies)
{
    CookieLog("CookieMap - getAllChildCookies in Map - %s", getName().utf8().data());
    getAllCookies(stackOfCookies);
    for (HashMap<String, CookieMap*>::iterator it = m_subdomains.begin(); it != m_subdomains.end(); ++it)
        it->second->getAllChildCookies(stackOfCookies);
}

} // namespace WebCore
