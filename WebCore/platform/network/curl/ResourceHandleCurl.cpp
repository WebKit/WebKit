/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com
 * All rights reserved.
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

#include "DocLoader.h"
#include "NotImplemented.h"
#include "ResourceHandleInternal.h"
#include "ResourceHandleManager.h"
#include "SharedBuffer.h"

#if PLATFORM(WIN) && PLATFORM(CF)
#include <wtf/PassRefPtr.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

class WebCoreSynchronousLoader : public ResourceHandleClient {
public:
    WebCoreSynchronousLoader();

    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
    virtual void didFinishLoading(ResourceHandle*);
    virtual void didFail(ResourceHandle*, const ResourceError&);

    ResourceResponse resourceResponse() const { return m_response; }
    ResourceError resourceError() const { return m_error; }
    Vector<char> data() const { return m_data; }

private:
    ResourceResponse m_response;
    ResourceError m_error;
    Vector<char> m_data;
};

WebCoreSynchronousLoader::WebCoreSynchronousLoader()
{
}

void WebCoreSynchronousLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    m_response = response;
}

void WebCoreSynchronousLoader::didReceiveData(ResourceHandle*, const char* data, int length, int)
{
    m_data.append(data, length);
}

void WebCoreSynchronousLoader::didFinishLoading(ResourceHandle*)
{
}

void WebCoreSynchronousLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    m_error = error;
}


static HashSet<String>& allowsAnyHTTPSCertificateHosts()
{
    static HashSet<String> hosts;

    return hosts;
}

ResourceHandleInternal::~ResourceHandleInternal()
{
    fastFree(m_url);
    if (m_customHeaders)
        curl_slist_free_all(m_customHeaders);
}

ResourceHandle::~ResourceHandle()
{
    cancel();
}

bool ResourceHandle::start(Frame* frame)
{
    // The frame could be null if the ResourceHandle is not associated to any
    // Frame, e.g. if we are downloading a file.
    // If the frame is not null but the page is null this must be an attempted
    // load from an onUnload handler, so let's just block it.
    if (frame && !frame->page())
        return false;

    ResourceHandleManager::sharedInstance()->add(this);
    return true;
}

void ResourceHandle::cancel()
{
    ResourceHandleManager::sharedInstance()->cancel(this);
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    return 0;
}

bool ResourceHandle::supportsBufferedData()
{
    return false;
}

#if PLATFORM(WIN) && PLATFORM(CF)
void ResourceHandle::setHostAllowsAnyHTTPSCertificate(const String& host)
{
    allowsAnyHTTPSCertificateHosts().add(host.lower());
}
#endif

#if PLATFORM(WIN) && PLATFORM(CF)
// FIXME:  The CFDataRef will need to be something else when
// building without 
static HashMap<String, RetainPtr<CFDataRef> >& clientCerts()
{
    static HashMap<String, RetainPtr<CFDataRef> > certs;
    return certs;
}

void ResourceHandle::setClientCertificate(const String& host, CFDataRef cert)
{
    clientCerts().set(host.lower(), cert);
}
#endif

void ResourceHandle::setDefersLoading(bool defers)
{
    if (d->m_defersLoading == defers)
        return;

#if LIBCURL_VERSION_NUM > 0x071200
    if (!d->m_handle)
        d->m_defersLoading = defers;
    else if (defers) {
        CURLcode error = curl_easy_pause(d->m_handle, CURLPAUSE_ALL);
        // If we could not defer the handle, so don't do it.
        if (error != CURLE_OK)
            return;

        d->m_defersLoading = defers;
    } else {
        // We need to set defersLoading before restarting a connection
        // or libcURL will call the callbacks in curl_easy_pause and
        // we would ASSERT.
        d->m_defersLoading = defers;

        CURLcode error = curl_easy_pause(d->m_handle, CURLPAUSE_CONT);
        if (error != CURLE_OK)
            // Restarting the handle has failed so just cancel it.
            cancel();
    }
#else
    d->m_defersLoading = defers;
    LOG_ERROR("Deferred loading is implemented if libcURL version is above 7.18.0");
#endif
}

bool ResourceHandle::willLoadFromCache(ResourceRequest&, Frame*)
{
    notImplemented();
    return false;
}

bool ResourceHandle::loadsBlocked()
{
    notImplemented();
    return false;
}

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data, Frame*)
{
    WebCoreSynchronousLoader syncLoader;
    ResourceHandle handle(request, &syncLoader, true, false);

    ResourceHandleManager* manager = ResourceHandleManager::sharedInstance();

    manager->dispatchSynchronousJob(&handle);

    error = syncLoader.resourceError();
    data = syncLoader.data();
    response = syncLoader.resourceResponse();
}

//stubs needed for windows version
void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge&) 
{
    notImplemented();
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge&, const Credential&) 
{
    notImplemented();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&) 
{
    notImplemented();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge&)
{
    notImplemented();
}

} // namespace WebCore
