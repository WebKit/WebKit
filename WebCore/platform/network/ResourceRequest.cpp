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
#include "ResourceRequest.h"

namespace WebCore {

bool ResourceRequest::isEmpty() const
{
    updateResourceRequest(); 
    
    return m_url.isEmpty(); 
}

const KURL& ResourceRequest::url() const 
{
    updateResourceRequest(); 
    
    return m_url;
}

void ResourceRequest::setURL(const KURL& url)
{ 
    updateResourceRequest(); 

    m_url = url; 
    
    m_platformRequestUpdated = false;
}

const ResourceRequestCachePolicy ResourceRequest::cachePolicy() const
{
    updateResourceRequest(); 
    
    return m_cachePolicy; 
}

void ResourceRequest::setCachePolicy(ResourceRequestCachePolicy cachePolicy)
{
    updateResourceRequest(); 
    
    m_cachePolicy = cachePolicy;
    
    m_platformRequestUpdated = false;
}

double ResourceRequest::timeoutInterval() const
{
    updateResourceRequest(); 
    
    return m_timeoutInterval; 
}

void ResourceRequest::setTimeoutInterval(double timeoutInterval) 
{
    updateResourceRequest(); 
    
    m_timeoutInterval = timeoutInterval; 
    
    m_platformRequestUpdated = false;
}

const KURL& ResourceRequest::mainDocumentURL() const
{
    updateResourceRequest(); 
    
    return m_mainDocumentURL; 
}

void ResourceRequest::setMainDocumentURL(const KURL& mainDocumentURL)
{ 
    updateResourceRequest(); 
    
    m_mainDocumentURL = mainDocumentURL; 
    
    m_platformRequestUpdated = false;
}

const String& ResourceRequest::httpMethod() const
{
    updateResourceRequest(); 
    
    return m_httpMethod; 
}

void ResourceRequest::setHTTPMethod(const String& httpMethod) 
{
    updateResourceRequest(); 

    m_httpMethod = httpMethod;
    
    m_platformRequestUpdated = false;
}

const HTTPHeaderMap& ResourceRequest::httpHeaderFields() const
{
    updateResourceRequest(); 

    return m_httpHeaderFields; 
}

String ResourceRequest::httpHeaderField(const String& name) const
{
    updateResourceRequest(); 
    
    return m_httpHeaderFields.get(name);
}

void ResourceRequest::setHTTPHeaderField(const String& name, const String& value)
{
    updateResourceRequest(); 
    
    m_httpHeaderFields.set(name, value); 
    
    m_platformRequestUpdated = false;
}

FormData* ResourceRequest::httpBody() const 
{ 
    updateResourceRequest(); 
    
    return m_httpBody.get(); 
}

void ResourceRequest::setHTTPBody(PassRefPtr<FormData> httpBody)
{
    updateResourceRequest(); 
    
    m_httpBody = httpBody; 
    
    m_platformRequestUpdated = false;
} 

bool ResourceRequest::allowHTTPCookies() const 
{
    updateResourceRequest(); 
    
    return m_allowHTTPCookies; 
}

void ResourceRequest::setAllowHTTPCookies(bool allowHTTPCookies)
{
    updateResourceRequest(); 
    
    m_allowHTTPCookies = allowHTTPCookies; 
    
    m_platformRequestUpdated = false;
}

void ResourceRequest::updatePlatformRequest() const
{
#if PLATFORM(MAC) || USE(CFNETWORK)
    if (m_platformRequestUpdated)
        return;
    
    const_cast<ResourceRequest*>(this)->doUpdatePlatformRequest();
    m_platformRequestUpdated = true;
#endif
}

void ResourceRequest::updateResourceRequest() const
{
#if PLATFORM(MAC) || USE(CFNETWORK)
    if (m_resourceRequestUpdated)
        return;

    const_cast<ResourceRequest*>(this)->doUpdateResourceRequest();
    m_resourceRequestUpdated = true;
#endif
}

void ResourceRequest::addHTTPHeaderField(const String& name, const String& value) 
{
    updateResourceRequest();
    pair<HTTPHeaderMap::iterator, bool> result = m_httpHeaderFields.add(name, value); 
    if (!result.second)
        result.first->second += "," + value;
}

void ResourceRequest::addHTTPHeaderFields(const HTTPHeaderMap& headerFields)
{
    HTTPHeaderMap::const_iterator end = headerFields.end();
    for (HTTPHeaderMap::const_iterator it = headerFields.begin(); it != end; ++it)
        addHTTPHeaderField(it->first, it->second);
}

}
