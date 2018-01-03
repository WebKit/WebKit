/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "CookieJarCurl.h"

#if USE(CURL)
#include "Cookie.h"
#include "CurlContext.h"
#include "NotImplemented.h"
#include "URL.h"

#include <wtf/DateMath.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static void readCurlCookieToken(const char*& cookie, String& token)
{
    // Read the next token from a cookie with the Netscape cookie format.
    // Curl separates each token in line with tab character.
    const char* cookieStart = cookie;
    while (cookie && cookie[0] && cookie[0] != '\t')
        cookie++;
    token = String(cookieStart, cookie - cookieStart);
    if (cookie[0] == '\t')
        cookie++;
}

static bool domainMatch(const String& cookieDomain, const String& host)
{
    size_t index = host.find(cookieDomain);

    bool tailMatch = (index != WTF::notFound && index + cookieDomain.length() == host.length());

    // Check if host equals cookie domain.
    if (tailMatch && !index)
        return true;

    // Check if host is a subdomain of the domain in the cookie.
    // Curl uses a '.' in front of domains to indicate it's valid on subdomains.
    if (tailMatch && index > 0 && host[index] == '.')
        return true;

    // Check the special case where host equals the cookie domain, except for a leading '.' in the cookie domain.
    // E.g. cookie domain is .apple.com and host is apple.com. 
    if (cookieDomain[0] == '.' && cookieDomain.find(host) == 1)
        return true;

    return false;
}

static void addMatchingCurlCookie(const char* cookie, const String& domain, const String& path, StringBuilder& cookies, bool httponly)
{
    // Check if the cookie matches domain and path, and is not expired.
    // If so, add it to the list of cookies.
    //
    // Description of the Netscape cookie file format which Curl uses:
    //
    // .netscape.com     TRUE   /  FALSE  946684799   NETSCAPE_ID  100103
    //
    // Each line represents a single piece of stored information. A tab is inserted between each of the fields.
    //
    // From left-to-right, here is what each field represents:
    //
    // domain - The domain that created AND that can read the variable.
    // flag - A TRUE/FALSE value indicating if all machines within a given domain can access the variable. This value is set automatically by the browser, depending on the value you set for domain.
    // path - The path within the domain that the variable is valid for.
    // secure - A TRUE/FALSE value indicating if a secure connection with the domain is needed to access the variable.
    // expiration - The UNIX time that the variable will expire on. UNIX time is defined as the number of seconds since Jan 1, 1970 00:00:00 GMT.
    // name - The name of the variable.
    // value - The value of the variable.

    if (!cookie)
        return;

    String cookieDomain;
    readCurlCookieToken(cookie, cookieDomain);

    // HttpOnly cookie entries begin with "#HttpOnly_".
    if (cookieDomain.startsWith("#HttpOnly_")) {
        if (httponly)
            cookieDomain.remove(0, 10);
        else
            return;
    }

    if (!domainMatch(cookieDomain, domain))
        return;

    String strBoolean;
    readCurlCookieToken(cookie, strBoolean);

    String strPath;
    readCurlCookieToken(cookie, strPath);

    // Check if path matches
    int index = path.find(strPath);
    if (index)
        return;

    String strSecure;
    readCurlCookieToken(cookie, strSecure);

    String strExpires;
    readCurlCookieToken(cookie, strExpires);

    int expires = strExpires.toInt();

    time_t now = 0;
    time(&now);

    // Check if cookie has expired
    if (expires && now > expires)
        return;

    String strName;
    readCurlCookieToken(cookie, strName);

    String strValue;
    readCurlCookieToken(cookie, strValue);

    // The cookie matches, add it to the cookie list.

    if (cookies.length() > 0)
        cookies.append("; ");

    cookies.append(strName);
    cookies.append("=");
    cookies.append(strValue);

}

static String getNetscapeCookieFormat(const URL& url, const String& value)
{
    // Constructs a cookie string in Netscape Cookie file format.

    if (value.isEmpty())
        return "";

    String valueStr;
    if (value.is8Bit())
        valueStr = value;
    else
        valueStr = String::make8BitFrom16BitSource(value.characters16(), value.length());

    Vector<String> attributes;
    valueStr.split(';', false, attributes);

    if (!attributes.size())
        return "";

    // First attribute should be <cookiename>=<cookievalue>
    String cookieName, cookieValue;
    Vector<String>::iterator attribute = attributes.begin();
    if (attribute->contains('=')) {
        Vector<String> nameValuePair;
        attribute->split('=', true, nameValuePair);
        cookieName = nameValuePair[0];
        cookieValue = nameValuePair[1];
    } else {
        // According to RFC6265 we should ignore the entire
        // set-cookie string now, but other browsers appear
        // to treat this as <cookiename>=<empty>
        cookieName = *attribute;
    }
    
    int expires = 0;
    String secure = "FALSE";
    String path = url.baseAsString().substring(url.pathStart());
    if (path.length() > 1 && path.endsWith('/'))
        path.remove(path.length() - 1);
    String domain = url.host();

    // Iterate through remaining attributes
    for (++attribute; attribute != attributes.end(); ++attribute) {
        if (attribute->contains('=')) {
            Vector<String> keyValuePair;
            attribute->split('=', true, keyValuePair);
            String key = keyValuePair[0].stripWhiteSpace();
            String val = keyValuePair[1].stripWhiteSpace();
            if (equalLettersIgnoringASCIICase(key, "expires")) {
                CString dateStr(reinterpret_cast<const char*>(val.characters8()), val.length());
                expires = WTF::parseDateFromNullTerminatedCharacters(dateStr.data()) / WTF::msPerSecond;
            } else if (equalLettersIgnoringASCIICase(key, "max-age"))
                expires = time(0) + val.toInt();
            else if (equalLettersIgnoringASCIICase(key, "domain"))
                domain = val;
            else if (equalLettersIgnoringASCIICase(key, "path"))
                path = val;
        } else {
            String key = attribute->stripWhiteSpace();
            if (equalLettersIgnoringASCIICase(key, "secure"))
                secure = "TRUE";
        }
    }
    
    String allowSubdomains = domain.startsWith('.') ? "TRUE" : "FALSE";
    String expiresStr = String::number(expires);

    int finalStringLength = domain.length() + path.length() + expiresStr.length() + cookieName.length();
    finalStringLength += cookieValue.length() + secure.length() + allowSubdomains.length();
    finalStringLength += 6; // Account for \t separators.
    
    StringBuilder cookieStr;
    cookieStr.reserveCapacity(finalStringLength);
    cookieStr.append(domain);
    cookieStr.append("\t");
    cookieStr.append(allowSubdomains);
    cookieStr.append("\t");
    cookieStr.append(path);
    cookieStr.append("\t");
    cookieStr.append(secure);
    cookieStr.append("\t");
    cookieStr.append(expiresStr);
    cookieStr.append("\t");
    cookieStr.append(cookieName);
    cookieStr.append("\t");
    cookieStr.append(cookieValue);

    return cookieStr.toString();
}

void CookieJarCurlFileSystem::setCookiesFromDOM(const NetworkStorageSession&, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String& value)
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    CurlHandle curlHandle;

    curlHandle.enableShareHandle();
    curlHandle.enableCookieJarIfExists();

    // CURL accepts cookies in either Set-Cookie or Netscape file format.
    // However with Set-Cookie format, there is no way to specify that we
    // should not allow cookies to be read from subdomains, which is the
    // required behavior if the domain field is not explicity specified.
    String cookie = getNetscapeCookieFormat(url, value);

    if (!cookie.is8Bit())
        cookie = String::make8BitFrom16BitSource(cookie.characters16(), cookie.length());

    CString strCookie(reinterpret_cast<const char*>(cookie.characters8()), cookie.length());

    curlHandle.setCookieList(strCookie.data());
}

static String cookiesForSession(const NetworkStorageSession&, const URL&, const URL& url, bool httponly)
{
    String cookies;

    CurlHandle curlHandle;
    curlHandle.enableShareHandle();

    CurlSList cookieList;
    curlHandle.fetchCookieList(cookieList);
    const struct curl_slist* list = cookieList.head();
    if (list) {
        String domain = url.host();
        String path = url.path();
        StringBuilder cookiesBuilder;

        while (list) {
            const char* cookie = list->data;
            addMatchingCurlCookie(cookie, domain, path, cookiesBuilder, httponly);
            list = list->next;
        }

        cookies = cookiesBuilder.toString();
    }

    return cookies;
}

std::pair<String, bool> CookieJarCurlFileSystem::cookiesForDOM(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies)
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    // FIXME: This should filter secure cookies out if the caller requests it.
    return { cookiesForSession(session, firstParty, url, false), false };
}

std::pair<String, bool> CookieJarCurlFileSystem::cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies)
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    // FIXME: This should filter secure cookies out if the caller requests it.
    return { cookiesForSession(session, firstParty, url, true), false };
}

bool CookieJarCurlFileSystem::cookiesEnabled(const NetworkStorageSession&)
{
    return true;
}

bool CookieJarCurlFileSystem::getRawCookies(const NetworkStorageSession&, const URL& firstParty, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>& rawCookies)
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    // FIXME: Not yet implemented
    rawCookies.clear();
    return false; // return true when implemented
}

void CookieJarCurlFileSystem::deleteCookie(const NetworkStorageSession&, const URL&, const String&)
{
    // FIXME: Not yet implemented
}

void CookieJarCurlFileSystem::getHostnamesWithCookies(const NetworkStorageSession&, HashSet<String>& hostnames)
{
    // FIXME: Not yet implemented
}

void CookieJarCurlFileSystem::deleteCookiesForHostnames(const NetworkStorageSession&, const Vector<String>& cookieHostNames)
{
    // FIXME: Not yet implemented
}

void CookieJarCurlFileSystem::deleteAllCookies(const NetworkStorageSession&)
{
    // FIXME: Not yet implemented
}

void CookieJarCurlFileSystem::deleteAllCookiesModifiedSince(const NetworkStorageSession&, WallTime)
{
    // FIXME: Not yet implemented
}

// dispatcher functions

std::pair<String, bool> cookiesForDOM(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    return CurlContext::singleton().cookieJar().cookiesForDOM(session, firstParty, url, frameID, pageID, includeSecureCookies);
}

void setCookiesFromDOM(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String& value)
{
    CurlContext::singleton().cookieJar().setCookiesFromDOM(session, firstParty, url, frameID, pageID, value);
}

std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    return CurlContext::singleton().cookieJar().cookieRequestHeaderFieldValue(session, firstParty, url, frameID, pageID, includeSecureCookies);
}

bool cookiesEnabled(const NetworkStorageSession& session)
{
    return CurlContext::singleton().cookieJar().cookiesEnabled(session);
}

bool getRawCookies(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>& rawCookies)
{
    return CurlContext::singleton().cookieJar().getRawCookies(session, firstParty, url, frameID, pageID, rawCookies);
}

void deleteCookie(const NetworkStorageSession& session, const URL& url, const String& cookie)
{
    CurlContext::singleton().cookieJar().deleteCookie(session, url, cookie);
}

void getHostnamesWithCookies(const NetworkStorageSession& session, HashSet<String>& hostnames)
{
    CurlContext::singleton().cookieJar().getHostnamesWithCookies(session, hostnames);
}

void deleteCookiesForHostnames(const NetworkStorageSession& session, const Vector<String>& cookieHostNames)
{
    CurlContext::singleton().cookieJar().deleteCookiesForHostnames(session, cookieHostNames);
}

void deleteAllCookies(const NetworkStorageSession& session)
{
    CurlContext::singleton().cookieJar().deleteAllCookies(session);
}

void deleteAllCookiesModifiedSince(const NetworkStorageSession& session, WallTime since)
{
    CurlContext::singleton().cookieJar().deleteAllCookiesModifiedSince(session, since);
}

}

#endif
