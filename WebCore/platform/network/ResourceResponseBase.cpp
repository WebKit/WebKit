/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

using namespace std;

namespace WebCore {

static void parseCacheHeader(const String& header, Vector<pair<String, String> >& result);

auto_ptr<ResourceResponse> ResourceResponseBase::adopt(auto_ptr<CrossThreadResourceResponseData> data)
{
    auto_ptr<ResourceResponse> response(new ResourceResponse());
    response->setURL(data->m_url);
    response->setMimeType(data->m_mimeType);
    response->setExpectedContentLength(data->m_expectedContentLength);
    response->setTextEncodingName(data->m_textEncodingName);
    response->setSuggestedFilename(data->m_suggestedFilename);

    response->setHTTPStatusCode(data->m_httpStatusCode);
    response->setHTTPStatusText(data->m_httpStatusText);

    response->lazyInit();
    response->m_httpHeaderFields.adopt(std::auto_ptr<CrossThreadHTTPHeaderMapData>(data->m_httpHeaders.release()));

    response->setExpirationDate(data->m_expirationDate);
    response->setLastModifiedDate(data->m_lastModifiedDate);
    response->m_haveParsedCacheControl = data->m_haveParsedCacheControl;
    response->m_cacheControlContainsMustRevalidate = data->m_cacheControlContainsMustRevalidate;
    response->m_cacheControlContainsNoCache = data->m_cacheControlContainsNoCache;
    return response;
}

auto_ptr<CrossThreadResourceResponseData> ResourceResponseBase::copyData() const
{
    auto_ptr<CrossThreadResourceResponseData> data(new CrossThreadResourceResponseData());
    data->m_url = url().copy();
    data->m_mimeType = mimeType().copy();
    data->m_expectedContentLength = expectedContentLength();
    data->m_textEncodingName = textEncodingName().copy();
    data->m_suggestedFilename = suggestedFilename().copy();
    data->m_httpStatusCode = httpStatusCode();
    data->m_httpStatusText = httpStatusText().copy();
    data->m_httpHeaders.adopt(httpHeaderFields().copyData());
    data->m_expirationDate = expirationDate();
    data->m_lastModifiedDate = lastModifiedDate();
    data->m_haveParsedCacheControl = m_haveParsedCacheControl;
    data->m_cacheControlContainsMustRevalidate = m_cacheControlContainsMustRevalidate;
    data->m_cacheControlContainsNoCache = m_cacheControlContainsNoCache;
    return data;
}

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

void ResourceResponseBase::setURL(const KURL& url)
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
        m_haveParsedCacheControl = false;
    m_httpHeaderFields.set(name, value);
}

const HTTPHeaderMap& ResourceResponseBase::httpHeaderFields() const
{
    lazyInit();

    return m_httpHeaderFields;
}

void ResourceResponseBase::parseCacheControlDirectives() const
{
    ASSERT(!m_haveParsedCacheControl);

    lazyInit();

    m_haveParsedCacheControl = true;
    m_cacheControlContainsMustRevalidate = false;
    m_cacheControlContainsNoCache = false;

    String cacheControlValue = httpHeaderField("cache-control");
    if (cacheControlValue.isEmpty())
        return;

    // FIXME: It would probably be much more efficient to parse this without creating all these data structures.

    Vector<pair<String, String> > directives;
    parseCacheHeader(cacheControlValue, directives);

    size_t directivesSize = directives.size();
    for (size_t i = 0; i < directivesSize; ++i) {
        // The no-cache directive can have a value, which is ignored here. This means that in very rare cases, responses that could be taken from cache won't be.
        if (equalIgnoringCase(directives[i].first, "no-cache"))
            m_cacheControlContainsNoCache = true;
        else if (equalIgnoringCase(directives[i].first, "must-revalidate"))
            m_cacheControlContainsMustRevalidate = true;
    }
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

}
