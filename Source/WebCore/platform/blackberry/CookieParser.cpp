/*
 * Copyright (C) 2009 Julien Chaffraix <jchaffraix@pleyo.com>
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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

#include "config.h"
#include "CookieParser.h"

#include "Logging.h"
#include "ParsedCookie.h"
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

namespace WebCore {

#define LOG_AND_DELETE(format, ...) \
    { \
        LOG_ERROR(format, ## __VA_ARGS__); \
        delete res; \
        return 0; \
    }

static inline bool isCookieHeaderSeparator(UChar c)
{
    return (c == '\r' || c =='\n');
}

static inline bool isLightweightSpace(UChar c)
{
    return (c == ' ' || c == '\t');
}

CookieParser::CookieParser(const KURL& defaultCookieURL)
    : m_defaultCookieURL(defaultCookieURL)
{
}

CookieParser::~CookieParser()
{
}

Vector<ParsedCookie*> CookieParser::parse(const String& cookies)
{
    unsigned cookieStart, cookieEnd = 0;
    double curTime = currentTime();
    Vector<ParsedCookie*, 4> parsedCookies;

    unsigned cookiesLength = cookies.length();
    if (!cookiesLength) // Code below doesn't handle this case
        return parsedCookies;

    // Iterate over the header to parse all the cookies.
    while (cookieEnd <= cookiesLength) {
        cookieStart = cookieEnd;
        
        // Find a cookie separator.
        while (cookieEnd <= cookiesLength && !isCookieHeaderSeparator(cookies[cookieEnd]))
            cookieEnd++;

        // Detect an empty cookie and go to the next one.
        if (cookieStart == cookieEnd) {
            ++cookieEnd;
            continue;
        }

        if (cookieEnd < cookiesLength && isCookieHeaderSeparator(cookies[cookieEnd]))
            ++cookieEnd;

        ParsedCookie* cookie = parseOneCookie(cookies, cookieStart, cookieEnd - 1, curTime);
        if (cookie)
            parsedCookies.append(cookie);
    }
    return parsedCookies;
}

// The cookie String passed into this method will only contian the name value pairs as well as other related cookie
// attributes such as max-age and domain. Set-Cookie should never be part of this string.
ParsedCookie* CookieParser::parseOneCookie(const String& cookie, unsigned start, unsigned end, double curTime)
{
    ParsedCookie* res = new ParsedCookie(curTime);

    if (!res)
        LOG_AND_DELETE("Out of memory");

    res->setProtocol(m_defaultCookieURL.protocol());

    // Parse [NAME "="] VALUE
    unsigned tokenEnd = start; // Token end contains the position of the '=' or the end of a token
    unsigned pairEnd = start; // Pair end contains always the position of the ';'

    // find the *first* ';' and the '=' (if they exist)
    bool quoteFound = false;
    bool foundEqual = false;
    while (pairEnd < end && (cookie[pairEnd] != ';' || quoteFound)) {
        if (tokenEnd == start && cookie[pairEnd] == '=') {
            tokenEnd = pairEnd;
            foundEqual = true;
        }
        if (cookie[pairEnd] == '"')
            quoteFound = !quoteFound;
        pairEnd++;
    }

    unsigned tokenStart = start;

    bool hasName = false; // This is a hack to avoid changing too much in this
                          // brutally brittle code.
    if (tokenEnd != start) {
        // There is a '=' so parse the NAME
        unsigned nameEnd = tokenEnd;

        // The tokenEnd is the position of the '=' so the nameEnd is one less
        nameEnd--;

        // Remove lightweight spaces.
        while (nameEnd && isLightweightSpace(cookie[nameEnd]))
            nameEnd--;

        while (tokenStart < nameEnd && isLightweightSpace(cookie[tokenStart]))
            tokenStart++;

        if (nameEnd + 1 <= tokenStart)
            LOG_AND_DELETE("Empty name. Rejecting the cookie");

        String name = cookie.substring(tokenStart, nameEnd + 1 - start);
        res->setName(name);
        hasName = true;
    }

    // Now parse the VALUE
    tokenStart = tokenEnd + 1;
    if (!hasName)
        --tokenStart;

    // Skip lightweight spaces in our token
    while (tokenStart < pairEnd && isLightweightSpace(cookie[tokenStart]))
        tokenStart++;

    tokenEnd = pairEnd;
    while (tokenEnd > tokenStart && isLightweightSpace(cookie[tokenEnd - 1]))
        tokenEnd--;

    String value;
    if (tokenEnd == tokenStart) {
        // Firefox accepts empty value so we will do the same
        value = String();
    } else
        value = cookie.substring(tokenStart, tokenEnd - tokenStart);

    if (hasName)
        res->setValue(value);
    else if (foundEqual) {
        delete res;
        return 0;
    } else
        res->setName(value); // No NAME=VALUE, only NAME

    while (pairEnd < end) {
        // Switch to the next pair as pairEnd is on the ';' and fast-forward any lightweight spaces.
        pairEnd++;
        while (pairEnd < end && isLightweightSpace(cookie[pairEnd]))
            pairEnd++;

        tokenStart = pairEnd;
        tokenEnd = tokenStart; // initialize token end to catch first '='

        while (pairEnd < end && cookie[pairEnd] != ';') {
            if (tokenEnd == tokenStart && cookie[pairEnd] == '=')
                tokenEnd = pairEnd;
            pairEnd++;
        }

        // FIXME : should we skip lightweight spaces here ?

        unsigned length = tokenEnd - tokenStart;
        unsigned tokenStartSvg = tokenStart;

        String parsedValue;
        if (tokenStart != tokenEnd) {
            // There is an equal sign so remove lightweight spaces in VALUE
            tokenStart = tokenEnd + 1;
            while (tokenStart < pairEnd && isLightweightSpace(cookie[tokenStart]))
                tokenStart++;

            tokenEnd = pairEnd;
            while (tokenEnd > tokenStart && isLightweightSpace(cookie[tokenEnd - 1]))
                tokenEnd--;

            parsedValue = cookie.substring(tokenStart, tokenEnd - tokenStart);
        } else {
            // If the parsedValue is empty, initialise it in case we need it
            parsedValue = String();
            // Handle a token without value.
            length = pairEnd - tokenStart;
        }

       // Detect which "cookie-av" is parsed
       // Look at the first char then parse the whole for performance issue
        switch (cookie[tokenStartSvg]) {
        case 'P':
        case 'p' : {
            if (length >= 4 && cookie.find("ath", tokenStartSvg + 1, false)) {
                // We need the path to be decoded to match those returned from KURL::path().
                // The path attribute may or may not include percent-encoded characters. Fortunately
                // if there are no percent-encoded characters, decoding the url is a no-op.
                res->setPath(decodeURLEscapeSequences(parsedValue));
            } else
                LOG_AND_DELETE("Invalid cookie %s (path)", cookie.ascii().data());
            break;
        }

        case 'D':
        case 'd' : {
            if (length >= 6 && cookie.find("omain", tokenStartSvg + 1, false)) {
                if (parsedValue.length() > 1 && parsedValue[0] == '"' && parsedValue[parsedValue.length() - 1] == '"')
                    parsedValue = parsedValue.substring(1, parsedValue.length() - 2);
                // If the domain does not start with a dot, add one for security checks,
                // For example: ab.c.com dose not domain match b.c.com;
                String realDomain = parsedValue[0] == '.' ? parsedValue : "." + parsedValue;
                res->setDomain(realDomain);
            } else
                LOG_AND_DELETE("Invalid cookie %s (domain)", cookie.ascii().data());
            break;
        }

        case 'E' :
        case 'e' : {
            if (length >= 7 && cookie.find("xpires", tokenStartSvg + 1, false))
                res->setExpiry(parsedValue);
            else
                LOG_AND_DELETE("Invalid cookie %s (expires)", cookie.ascii().data());
            break;
        }

        case 'M' :
        case 'm' : {
            if (length >= 7 && cookie.find("ax-age", tokenStartSvg + 1, false))
                res->setMaxAge(parsedValue);
            else
                LOG_AND_DELETE("Invalid cookie %s (max-age)", cookie.ascii().data());
            break;
        }

        case 'C' :
        case 'c' : {
            if (length >= 7 && cookie.find("omment", tokenStartSvg + 1, false))
                // We do not have room for the comment part (and so do Mozilla) so just log the comment.
                LOG(Network, "Comment %s for ParsedCookie : %s\n", parsedValue.ascii().data(), cookie.ascii().data());
            else
                LOG_AND_DELETE("Invalid cookie %s (comment)", cookie.ascii().data());
            break;
        }

        case 'V' :
        case 'v' : {
            if (length >= 7 && cookie.find("ersion", tokenStartSvg + 1, false)) {
                // Although the out-of-dated Cookie Spec(RFC2965, http://tools.ietf.org/html/rfc2965) defined
                // the value of version can only contain DIGIT, some random sites, e.g. https://devforums.apple.com
                // would use double quotation marks to quote the digit. So we need to get rid of them for compliance.
                if (parsedValue.length() > 1 && parsedValue[0] == '"' && parsedValue[parsedValue.length() - 1] == '"')
                    parsedValue = parsedValue.substring(1, parsedValue.length() - 2);

                if (parsedValue.toInt() != 1)
                    LOG_AND_DELETE("ParsedCookie version %d not supported (only support version=1)", parsedValue.toInt());
            } else
                LOG_AND_DELETE("Invalid cookie %s (version)", cookie.ascii().data());
            break;
        }

        case 'S' :
        case 's' : {
            // Secure is a standalone token ("Secure;")
            if (length >= 6 && cookie.find("ecure", tokenStartSvg + 1, false))
                res->setSecureFlag(true);
            else
                LOG_AND_DELETE("Invalid cookie %s (secure)", cookie.ascii().data());
            break;
        }
        case 'H':
        case 'h': {
            // HttpOnly is a standalone token ("HttpOnly;")
            if (length >= 8 && cookie.find("ttpOnly", tokenStartSvg + 1, false))
                res->setIsHttpOnly(true);
            else
                LOG_AND_DELETE("Invalid cookie %s (HttpOnly)", cookie.ascii().data());
            break;
        }

        default : {
            // If length == 0, we should be at the end of the cookie (case : ";\r") so ignore it
            if (length)
                LOG_ERROR("Invalid token for cookie %s", cookie.ascii().data());
        }
        }
    }

    // Check if the cookie is valid with respect to the size limit.
    if (!res->isUnderSizeLimit())
        LOG_AND_DELETE("ParsedCookie %s is above the 4kb in length : REJECTED", cookie.ascii().data());

    // If some pair was not provided, during parsing then apply some default value
    // the rest has been done in the constructor.

    // If no domain was provided, set it to the host
    if (!res->domain())
        res->setDomain(m_defaultCookieURL.host());

    // According to the Cookie Specificaiton (RFC6265, section 4.1.2.4 and 5.2.4, http://tools.ietf.org/html/rfc6265),
    // If no path was provided or the first character of the path value is not '/', set it to the host's path
    //
    // REFERENCE
    // 4.1.2.4. The Path Attribute
    //
    // The scope of each cookie is limited to a set of paths, controlled by
    // the Path attribute. If the server omits the Path attribute, the user
    // agent will use the "directory" of the request-uri's path component as
    // the default value. (See Section 5.1.4 for more details.)
    // ...........
    // 5.2.4. The Path Attribute
    //
    // If the attribute-name case-insensitively matches the string "Path",
    // the user agent MUST process the cookie-av as follows.
    //
    // If the attribute-value is empty or if the first character of the
    // attribute-value is not %x2F ("/"):
    //
    // Let cookie-path be the default-path.
    //
    // Otherwise:
    //
    // Let cookie-path be the attribute-value.
    //
    // Append an attribute to the cookie-attribute-list with an attribute-
    // name of Path and an attribute-value of cookie-path.
    if (!res->path() || !res->path().length() || !res->path().startsWith("/", false)) {
        String path = m_defaultCookieURL.string().substring(m_defaultCookieURL.pathStart(), m_defaultCookieURL.pathAfterLastSlash() - m_defaultCookieURL.pathStart() - 1);
        if (path.isEmpty())
            path = "/";
        // Since this is reading the raw url string, it could contain percent-encoded sequences. We
        // want it to be comparable to the return value of url.path(), which is not percent-encoded,
        // so we must remove the escape sequences.
        res->setPath(decodeURLEscapeSequences(path));
    }
 
    return res;
}

} // namespace WebCore
