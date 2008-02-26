// -*- mode: c++; c-basic-offset: 4 -*-
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

String ResourceResponseBase::httpHeaderField(const String& name) const
{
    lazyInit();

    return m_httpHeaderFields.get(name); 
}

void ResourceResponseBase::setHTTPHeaderField(const String& name, const String& value)
{
    lazyInit();

    m_httpHeaderFields.set(name, value);
}

const HTTPHeaderMap& ResourceResponseBase::httpHeaderFields() const
{
    lazyInit();

    return m_httpHeaderFields; 
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

}
