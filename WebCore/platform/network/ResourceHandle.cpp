/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"

#include "Logging.h"
#include "ResourceHandleClient.h"
#include "Timer.h"

#include <wtf/HashSet.h>

namespace WebCore {

ResourceHandle::ResourceHandle(const ResourceRequest& request, ResourceHandleClient* client, bool defersLoading, bool mightDownloadFromHandle)
    : d(new ResourceHandleInternal(this, request, client, defersLoading, mightDownloadFromHandle))
{
}

PassRefPtr<ResourceHandle> ResourceHandle::create(const ResourceRequest& request, ResourceHandleClient* client, Frame* frame, bool defersLoading, bool mightDownloadFromHandle)
{
    RefPtr<ResourceHandle> newHandle(new ResourceHandle(request, client, defersLoading, mightDownloadFromHandle));

    if (!portAllowed(request)) {
        newHandle->scheduleBlockedFailure();
        return newHandle.release();
    }
        
    if (newHandle->start(frame))
        return newHandle.release();

    return 0;
}

void ResourceHandle::scheduleBlockedFailure()
{
    Timer<ResourceHandle>* blockedTimer = new Timer<ResourceHandle>(this, &ResourceHandle::fireBlockedFailure);
    blockedTimer->startOneShot(0);
}

void ResourceHandle::fireBlockedFailure(Timer<ResourceHandle>* timer)
{
    client()->wasBlocked(this);
    delete timer;
}

const HTTPHeaderMap& ResourceHandle::requestHeaders() const
{
    return d->m_request.httpHeaderFields();
}

const KURL& ResourceHandle::url() const
{
    return d->m_request.url();
}

PassRefPtr<FormData> ResourceHandle::postData() const
{
    return d->m_request.httpBody();
}

const String& ResourceHandle::method() const
{
    return d->m_request.httpMethod();
}

ResourceHandleClient* ResourceHandle::client() const
{
    return d->m_client;
}

const ResourceRequest& ResourceHandle::request() const
{
    return d->m_request;
}

void ResourceHandle::clearAuthentication()
{
#if PLATFORM(MAC)
    d->m_currentMacChallenge = nil;
#endif
    d->m_currentWebChallenge.nullify();
}

bool ResourceHandle::portAllowed(const ResourceRequest& request)
{
    uint16_t port = request.url().port();
    if (!port)
        return true;
        
    // The blocked port list matches the port blocking mozilla implements
    // See http://www.mozilla.org/projects/netlib/PortBanning.html for more information
    static uint16_t blockedPortList[] = { 
    1,    // tcpmux          
    7,    // echo     
    9,    // discard          
    11,   // systat   
    13,   // daytime          
    15,   // netstat  
    17,   // qotd             
    19,   // chargen  
    20,   // FTP-data 
    21,   // FTP-control        
    22,   // SSH              
    23,   // telnet   
    25,   // SMTP     
    37,   // time     
    42,   // name     
    43,   // nicname  
    53,   // domain  
    77,   // priv-rjs 
    79,   // finger   
    87,   // ttylink  
    95,   // supdup   
    101,  // hostriame
    102,  // iso-tsap 
    103,  // gppitnp  
    104,  // acr-nema 
    109,  // POP2     
    110,  // POP3     
    111,  // sunrpc   
    113,  // auth     
    115,  // SFTP     
    117,  // uucp-path
    119,  // nntp     
    123,  // NTP
    135,  // loc-srv / epmap         
    139,  // netbios
    143,  // IMAP2  
    179,  // BGP
    389,  // LDAP
    465,  // SMTP+SSL
    512,  // print / exec          
    513,  // login         
    514,  // shell         
    515,  // printer         
    526,  // tempo         
    530,  // courier        
    531,  // Chat         
    532,  // netnews        
    540,  // UUCP       
    556,  // remotefs    
    563,  // NNTP+SSL
    587,  // ESMTP
    601,  // syslog-conn  
    636,  // LDAP+SSL
    993,  // IMAP+SSL
    995,  // POP3+SSL
    2049, // NFS
    4045, // lockd
    6000, // X11        
    0 };   
    
    static HashSet<int>* blockedPortHash = 0;
    if (!blockedPortHash) {
        blockedPortHash = new HashSet<int>;
        
        for (int i = 0; blockedPortList[i]; ++i)
            blockedPortHash->add(blockedPortList[i]);
    }
    
    bool restricted = blockedPortHash->contains(port);
    
    if (restricted) {
        // An exception in the mozilla port blocking is they allow 21 and 22 for FTP (and Secure FTP), which we have to do also
        if ((port == 21 || port == 22) && request.url().url().startsWith("ftp:", false))
            return true;
    }
        
    return !restricted;
}


} // namespace WebCore

