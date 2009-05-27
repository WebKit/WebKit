/*
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include <algorithm>

namespace WebCore {

static bool shouldForceContentSniffing;

static bool portAllowed(const ResourceRequest&);

ResourceHandle::ResourceHandle(const ResourceRequest& request, ResourceHandleClient* client, bool defersLoading,
         bool shouldContentSniff, bool mightDownloadFromHandle)
    : d(new ResourceHandleInternal(this, request, client, defersLoading, shouldContentSniff, mightDownloadFromHandle))
{
}

PassRefPtr<ResourceHandle> ResourceHandle::create(const ResourceRequest& request, ResourceHandleClient* client,
    Frame* frame, bool defersLoading, bool shouldContentSniff, bool mightDownloadFromHandle)
{
    if (shouldContentSniff)
        shouldContentSniff = shouldContentSniffURL(request.url());

    RefPtr<ResourceHandle> newHandle(adoptRef(new ResourceHandle(request, client, defersLoading, shouldContentSniff, mightDownloadFromHandle)));

    if (!request.url().isValid()) {
        newHandle->scheduleFailure(InvalidURLFailure);
        return newHandle.release();
    }

    if (!portAllowed(request)) {
        newHandle->scheduleFailure(BlockedFailure);
        return newHandle.release();
    }
        
    if (newHandle->start(frame))
        return newHandle.release();

    return 0;
}

void ResourceHandle::scheduleFailure(FailureType type)
{
    d->m_failureType = type;
    d->m_failureTimer.startOneShot(0);
}

void ResourceHandle::fireFailure(Timer<ResourceHandle>*)
{
    if (!client())
        return;

    switch (d->m_failureType) {
        case BlockedFailure:
            client()->wasBlocked(this);
            return;
        case InvalidURLFailure:
            client()->cannotShowURL(this);
            return;
    }

    ASSERT_NOT_REACHED();
}

ResourceHandleClient* ResourceHandle::client() const
{
    return d->m_client;
}

void ResourceHandle::setClient(ResourceHandleClient* client)
{
    d->m_client = client;
}

const ResourceRequest& ResourceHandle::request() const
{
    return d->m_request;
}

void ResourceHandle::clearAuthentication()
{
#if PLATFORM(MAC)
    d->m_currentMacChallenge = nil;
#elif USE(CFNETWORK)
    d->m_currentCFChallenge = 0;
#endif
    d->m_currentWebChallenge.nullify();
}

static bool portAllowed(const ResourceRequest& request)
{
    unsigned short port = request.url().port();

    // Since most URLs don't have a port, return early for the "no port" case.
    if (!port)
        return true;

    // This blocked port list matches the port blocking that Mozilla implements.
    // See http://www.mozilla.org/projects/netlib/PortBanning.html for more information.
    static const unsigned short blockedPortList[] = {
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
        3659, // apple-sasl / PasswordServer [Apple addition]
        4045, // lockd
        6000, // X11
    };
    const unsigned short* const blockedPortListEnd = blockedPortList
        + sizeof(blockedPortList) / sizeof(blockedPortList[0]);

    // If the port is not in the blocked port list, allow it.
    if (!std::binary_search(blockedPortList, blockedPortListEnd, port))
        return true;

    // Allow ports 21 and 22 for FTP URLs, as Mozilla does.
    if ((port == 21 || port == 22) && request.url().protocolIs("ftp"))
        return true;

    // Allow any port number in a file URL, since the port number is ignored.
    if (request.url().protocolIs("file"))
        return true;

    return false;
}
  
bool ResourceHandle::shouldContentSniff() const
{
    return d->m_shouldContentSniff;
}

bool ResourceHandle::shouldContentSniffURL(const KURL& url)
{
#if PLATFORM(MAC)
    if (shouldForceContentSniffing)
        return true;
#endif
    // We shouldn't content sniff file URLs as their MIME type should be established via their extension.
    return !url.protocolIs("file");
}

void ResourceHandle::forceContentSniffing()
{
    shouldForceContentSniffing = true;
}

} // namespace WebCore
