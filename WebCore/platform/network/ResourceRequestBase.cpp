// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "ResourceRequestBase.h"
#include "ResourceRequest.h"

namespace WebCore {

inline const ResourceRequest& ResourceRequestBase::asResourceRequest() const
{
    return *static_cast<const ResourceRequest*>(this);
}

bool ResourceRequestBase::isEmpty() const
{
    updateResourceRequest(); 
    
    return m_url.isEmpty(); 
}

bool ResourceRequestBase::isNull() const
{
    updateResourceRequest(); 
    
    return m_url.isNull();
}

const KURL& ResourceRequestBase::url() const 
{
    updateResourceRequest(); 
    
    return m_url;
}

void ResourceRequestBase::setURL(const KURL& url)
{ 
    updateResourceRequest(); 

    m_url = url; 
    
    m_platformRequestUpdated = false;
}

const ResourceRequestCachePolicy ResourceRequestBase::cachePolicy() const
{
    updateResourceRequest(); 
    
    return m_cachePolicy; 
}

void ResourceRequestBase::setCachePolicy(ResourceRequestCachePolicy cachePolicy)
{
    updateResourceRequest(); 
    
    m_cachePolicy = cachePolicy;
    
    m_platformRequestUpdated = false;
}

double ResourceRequestBase::timeoutInterval() const
{
    updateResourceRequest(); 
    
    return m_timeoutInterval; 
}

void ResourceRequestBase::setTimeoutInterval(double timeoutInterval) 
{
    updateResourceRequest(); 
    
    m_timeoutInterval = timeoutInterval; 
    
    m_platformRequestUpdated = false;
}

const KURL& ResourceRequestBase::mainDocumentURL() const
{
    updateResourceRequest(); 
    
    return m_mainDocumentURL; 
}

void ResourceRequestBase::setMainDocumentURL(const KURL& mainDocumentURL)
{ 
    updateResourceRequest(); 
    
    m_mainDocumentURL = mainDocumentURL; 
    
    m_platformRequestUpdated = false;
}

const String& ResourceRequestBase::httpMethod() const
{
    updateResourceRequest(); 
    
    return m_httpMethod; 
}

void ResourceRequestBase::setHTTPMethod(const String& httpMethod) 
{
    updateResourceRequest(); 

    m_httpMethod = httpMethod;
    
    m_platformRequestUpdated = false;
}

const HTTPHeaderMap& ResourceRequestBase::httpHeaderFields() const
{
    updateResourceRequest(); 

    return m_httpHeaderFields; 
}

String ResourceRequestBase::httpHeaderField(const String& name) const
{
    updateResourceRequest(); 
    
    return m_httpHeaderFields.get(name);
}

void ResourceRequestBase::setHTTPHeaderField(const String& name, const String& value)
{
    updateResourceRequest(); 
    
    m_httpHeaderFields.set(name, value); 
    
    m_platformRequestUpdated = false;
}

FormData* ResourceRequestBase::httpBody() const 
{ 
    updateResourceRequest(); 
    
    return m_httpBody.get(); 
}

void ResourceRequestBase::setHTTPBody(PassRefPtr<FormData> httpBody)
{
    updateResourceRequest(); 
    
    m_httpBody = httpBody; 
    
    m_platformRequestUpdated = false;
} 

bool ResourceRequestBase::allowHTTPCookies() const 
{
    updateResourceRequest(); 
    
    return m_allowHTTPCookies; 
}

void ResourceRequestBase::setAllowHTTPCookies(bool allowHTTPCookies)
{
    updateResourceRequest(); 
    
    m_allowHTTPCookies = allowHTTPCookies; 
    
    m_platformRequestUpdated = false;
}

void ResourceRequestBase::addHTTPHeaderField(const String& name, const String& value) 
{
    updateResourceRequest();
    pair<HTTPHeaderMap::iterator, bool> result = m_httpHeaderFields.add(name, value); 
    if (!result.second)
        result.first->second += "," + value;
}

void ResourceRequestBase::addHTTPHeaderFields(const HTTPHeaderMap& headerFields)
{
    HTTPHeaderMap::const_iterator end = headerFields.end();
    for (HTTPHeaderMap::const_iterator it = headerFields.begin(); it != end; ++it)
        addHTTPHeaderField(it->first, it->second);
}

bool equalIgnoringHeaderFields(const ResourceRequestBase& a, const ResourceRequestBase& b)
{
    if (a.url() != b.url())
        return false;
    
    if (a.cachePolicy() != b.cachePolicy())
        return false;
    
    if (a.timeoutInterval() != b.timeoutInterval())
        return false;
    
    if (a.mainDocumentURL() != b.mainDocumentURL())
        return false;
    
    if (a.httpMethod() != b.httpMethod())
        return false;
    
    if (a.allowHTTPCookies() != b.allowHTTPCookies())
        return false;
    
    FormData* formDataA = a.httpBody();
    FormData* formDataB = b.httpBody();
    
    if (!formDataA)
        return !formDataB;
    if (!formDataB)
        return !formDataA;
    
    if (*formDataA != *formDataB)
        return false;
    
    return true;
}

bool operator==(const ResourceRequestBase& a, const ResourceRequestBase& b)
{
    if (!equalIgnoringHeaderFields(a, b))
        return false;
    
    if (a.httpHeaderFields() != b.httpHeaderFields())
        return false;
        
    return true;
}

bool ResourceRequestBase::isConditional() const
{
    return (m_httpHeaderFields.contains("If-Match") ||
            m_httpHeaderFields.contains("If-Modified-Since") ||
            m_httpHeaderFields.contains("If-None-Match") ||
            m_httpHeaderFields.contains("If-Range") ||
            m_httpHeaderFields.contains("If-Unmodified-Since"));
}

void ResourceRequestBase::updatePlatformRequest() const
{
    if (m_platformRequestUpdated)
        return;
    
    const_cast<ResourceRequest&>(asResourceRequest()).doUpdatePlatformRequest();
    m_platformRequestUpdated = true;
}

void ResourceRequestBase::updateResourceRequest() const
{
    if (m_resourceRequestUpdated)
        return;

    const_cast<ResourceRequest&>(asResourceRequest()).doUpdateResourceRequest();
    m_resourceRequestUpdated = true;
}

}
