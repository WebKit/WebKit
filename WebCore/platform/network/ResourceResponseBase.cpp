/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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
#include "ResourceResponseBase.h"
#include "ResourceResponse.h"

namespace WebCore {

static void parseCacheHeader(const String& header, Vector<pair<String, String> >& result);
static void parseCacheControlDirectiveValues(const String& directives, Vector<String>& result);

bool ResourceResponseBase::isHTTP() const
{
    lazyInit();

    String protocol = m_url.protocol();

    return equalIgnoringCase(protocol, "http")  || equalIgnoringCase(protocol, "https");
}

const KURL& ResourceResponseBase::url() const
{
    lazyInit();
    
    return m_url; 
}

void ResourceResponseBase::setUrl(const KURL& url)
{
    lazyInit();
    m_isNull = false;
    
    m_url = url; 
}

const String& ResourceResponseBase::mimeType() const
{
    lazyInit();

    return m_mimeType; 
}

void ResourceResponseBase::setMimeType(const String& mimeType)
{
    lazyInit();
    m_isNull = false;
    
    m_mimeType = mimeType; 
}

long long ResourceResponseBase::expectedContentLength() const 
{
    lazyInit();

    return m_expectedContentLength;
}

void ResourceResponseBase::setExpectedContentLength(long long expectedContentLength)
{
    lazyInit();
    m_isNull = false;
    
    m_expectedContentLength = expectedContentLength; 
}

const String& ResourceResponseBase::textEncodingName() const
{
    lazyInit();

    return m_textEncodingName;
}

void ResourceResponseBase::setTextEncodingName(const String& encodingName)
{
    lazyInit();
    m_isNull = false;
    
    m_textEncodingName = encodingName; 
}

// FIXME should compute this on the fly
const String& ResourceResponseBase::suggestedFilename() const
{
    lazyInit();

    return m_suggestedFilename;
}

void ResourceResponseBase::setSuggestedFilename(const String& suggestedName)
{
    lazyInit();
    m_isNull = false;
    
    m_suggestedFilename = suggestedName; 
}

int ResourceResponseBase::httpStatusCode() const
{
    lazyInit();

    return m_httpStatusCode;
}

void ResourceResponseBase::setHTTPStatusCode(int statusCode)
{
    lazyInit();

    m_httpStatusCode = statusCode;
}

const String& ResourceResponseBase::httpStatusText() const 
{
    lazyInit();

    return m_httpStatusText; 
}

void ResourceResponseBase::setHTTPStatusText(const String& statusText) 
{
    lazyInit();

    m_httpStatusText = statusText; 
}

String ResourceResponseBase::httpHeaderField(const AtomicString& name) const
{
    lazyInit();

    return m_httpHeaderFields.get(name); 
}

void ResourceResponseBase::setHTTPHeaderField(const AtomicString& name, const String& value)
{
    lazyInit();

    if (equalIgnoringCase(name, "cache-control"))
        m_haveParsedCacheControlHeader = false;
    else if (equalIgnoringCase(name, "pragma"))
        m_haveParsedPragmaHeader = false;

    m_httpHeaderFields.set(name, value);
}

const HTTPHeaderMap& ResourceResponseBase::httpHeaderFields() const
{
    lazyInit();

    return m_httpHeaderFields; 
}

const PragmaDirectiveMap& ResourceResponseBase::parsePragmaDirectives() const
{
    lazyInit();

    if (m_haveParsedPragmaHeader)
        return m_pragmaDirectiveMap;

    m_pragmaDirectiveMap.clear();
    m_haveParsedPragmaHeader = true;

    const String pragmaHeader = httpHeaderField("Pragma");
    if (pragmaHeader.isEmpty())
        return m_pragmaDirectiveMap;

    Vector<pair<String, String> > directives;
    parseCacheHeader(pragmaHeader, directives);

    unsigned max = directives.size();
    for (unsigned i = 0; i < max; ++i)
        m_pragmaDirectiveMap.set(directives[i].first, directives[i].second);

    return m_pragmaDirectiveMap;
}

const CacheControlDirectiveMap& ResourceResponseBase::parseCacheControlDirectives() const
{
    lazyInit();

    if (m_haveParsedCacheControlHeader)
        return m_cacheControlDirectiveMap;

    m_cacheControlDirectiveMap.clear();
    m_haveParsedCacheControlHeader = true;

    const String cacheControlHeader = httpHeaderField("Cache-Control");
    if (cacheControlHeader.isEmpty())
        return m_cacheControlDirectiveMap;

    Vector<pair<String, String> > directives;
    parseCacheHeader(cacheControlHeader, directives);

    unsigned iMax = directives.size();
    for (unsigned i = 0; i < iMax; ++i) {
        Vector<String> directiveValues;
        if ((equalIgnoringCase(directives[i].first, "private") || equalIgnoringCase(directives[i].first, "no-cache")) && !directives[i].second.isEmpty())
            parseCacheControlDirectiveValues(directives[i].second, directiveValues);
        else
            directiveValues.append(directives[i].second);

        if (m_cacheControlDirectiveMap.contains(directives[i].first)) {
            CacheControlDirectiveMap::iterator entry = m_cacheControlDirectiveMap.find(directives[i].first);
            unsigned jMax = directiveValues.size();
            for (unsigned j = 0; j < jMax; ++j)
                entry->second.add(directiveValues[j]);
        } else {
            HashSet<String> values;
            unsigned jMax = directiveValues.size();
            for (unsigned j = 0; j < jMax; ++j)
                values.add(directiveValues[j]);
            m_cacheControlDirectiveMap.set(directives[i].first, values);
        }
    }

    return m_cacheControlDirectiveMap;
}

bool ResourceResponseBase::isAttachment() const
{
    lazyInit();

    String value = m_httpHeaderFields.get("Content-Disposition");
    int loc = value.find(';');
    if (loc != -1)
        value = value.left(loc);
    value = value.stripWhiteSpace();
    return equalIgnoringCase(value, "attachment");
}

void ResourceResponseBase::setExpirationDate(time_t expirationDate)
{
    lazyInit();

    m_expirationDate = expirationDate;
}

time_t ResourceResponseBase::expirationDate() const
{
    lazyInit();

    return m_expirationDate; 
}

void ResourceResponseBase::setLastModifiedDate(time_t lastModifiedDate) 
{
    lazyInit();

    m_lastModifiedDate = lastModifiedDate;
}

time_t ResourceResponseBase::lastModifiedDate() const
{
    lazyInit();

    return m_lastModifiedDate;
}

void ResourceResponseBase::lazyInit() const
{
    const_cast<ResourceResponse*>(static_cast<const ResourceResponse*>(this))->platformLazyInit();
}

bool ResourceResponseBase::compare(const ResourceResponse& a, const ResourceResponse& b)
{
    if (a.isNull() != b.isNull())
        return false;  
    if (a.url() != b.url())
        return false;
    if (a.mimeType() != b.mimeType())
        return false;
    if (a.expectedContentLength() != b.expectedContentLength())
        return false;
    if (a.textEncodingName() != b.textEncodingName())
        return false;
    if (a.suggestedFilename() != b.suggestedFilename())
        return false;
    if (a.httpStatusCode() != b.httpStatusCode())
        return false;
    if (a.httpStatusText() != b.httpStatusText())
        return false;
    if (a.httpHeaderFields() != b.httpHeaderFields())
        return false;
    if (a.expirationDate() != b.expirationDate())
        return false;
    return ResourceResponse::platformCompare(a, b);
}

static bool isCacheHeaderSeparator(UChar c)
{
    // See RFC 2616, Section 2.2
    switch (c) {
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ',':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '[':
        case ']':
        case '?':
        case '=':
        case '{':
        case '}':
        case ' ':
        case '\t':
            return true;
        default:
            return false;
    }
}

static bool isControlCharacter(UChar c)
{
    return c < ' ' || c == 127;
}

static inline String trimToNextSeparator(const String& str)
{
    return str.substring(0, str.find(isCacheHeaderSeparator, 0));
}

static void parseCacheHeader(const String& header, Vector<pair<String, String> >& result)
{
    const String safeHeader = header.removeCharacters(isControlCharacter);
    unsigned max = safeHeader.length();
    for (unsigned pos = 0; pos < max; /* pos incremented in loop */) {
        int nextCommaPosition = safeHeader.find(',', pos);
        int nextEqualSignPosition = safeHeader.find('=', pos);
        if (nextEqualSignPosition >= 0 && (nextEqualSignPosition < nextCommaPosition || nextCommaPosition < 0)) {
            // Get directive name, parse right hand side of equal sign, then add to map
            String directive = trimToNextSeparator(safeHeader.substring(pos, nextEqualSignPosition - pos).stripWhiteSpace());
            pos += nextEqualSignPosition - pos + 1;

            String value = safeHeader.substring(pos, max - pos).stripWhiteSpace();
            if (value[0] == '"') {
                // The value is a quoted string
                int nextDoubleQuotePosition = value.find('"', 1);
                if (nextDoubleQuotePosition >= 0) {
                    // Store the value as a quoted string without quotes
                    result.append(pair<String, String>(directive, value.substring(1, nextDoubleQuotePosition - 1).stripWhiteSpace()));
                    pos += (safeHeader.find('"', pos) - pos) + nextDoubleQuotePosition + 1;
                    // Move past next comma, if there is one
                    int nextCommaPosition2 = safeHeader.find(',', pos);
                    if (nextCommaPosition2 >= 0)
                        pos += nextCommaPosition2 - pos + 1;
                    else
                        return; // Parse error if there is anything left with no comma
                } else {
                    // Parse error; just use the rest as the value
                    result.append(pair<String, String>(directive, trimToNextSeparator(value.substring(1, value.length() - 1).stripWhiteSpace())));
                    return;
                }
            } else {
                // The value is a token until the next comma
                int nextCommaPosition2 = value.find(',', 0);
                if (nextCommaPosition2 >= 0) {
                    // The value is delimited by the next comma
                    result.append(pair<String, String>(directive, trimToNextSeparator(value.substring(0, nextCommaPosition2).stripWhiteSpace())));
                    pos += (safeHeader.find(',', pos) - pos) + 1;
                } else {
                    // The rest is the value; no change to value needed
                    result.append(pair<String, String>(directive, trimToNextSeparator(value)));
                    return;
                }
            }
        } else if (nextCommaPosition >= 0 && (nextCommaPosition < nextEqualSignPosition || nextEqualSignPosition < 0)) {
            // Add directive to map with empty string as value
            result.append(pair<String, String>(trimToNextSeparator(safeHeader.substring(pos, nextCommaPosition - pos).stripWhiteSpace()), ""));
            pos += nextCommaPosition - pos + 1;
        } else {
            // Add last directive to map with empty string as value
            result.append(pair<String, String>(trimToNextSeparator(safeHeader.substring(pos, max - pos).stripWhiteSpace()), ""));
            return;
        }
    }
}

static void parseCacheControlDirectiveValues(const String& directives, Vector<String>& result)
{
    directives.split(',', false, result);
    unsigned max = result.size();
    for (unsigned i = 0; i < max; ++i)
        result[i] = result[i].stripWhiteSpace();
}

}
