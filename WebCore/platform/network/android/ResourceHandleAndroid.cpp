/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
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

#include "CachedResourceLoader.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "MainResourceLoader.h"
#include "NotImplemented.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceLoaderAndroid.h"
#include <wtf/text/CString.h>

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
}

bool ResourceHandle::start(Frame* frame)
{
    DocumentLoader* documentLoader = frame->loader()->activeDocumentLoader();
    MainResourceLoader* mainLoader = documentLoader->mainResourceLoader();
    bool isMainResource = mainLoader && (mainLoader->handle() == this);

    PassRefPtr<ResourceLoaderAndroid> loader = ResourceLoaderAndroid::start(this, d->m_request, frame->loader()->client(), isMainResource, false);

    if (loader) {
        d->m_loader = loader;
        return true;
    }

    return false;
}

void ResourceHandle::cancel()
{
    if (d->m_loader)
        d->m_loader->cancel();
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    notImplemented();
    return 0;
}

bool ResourceHandle::supportsBufferedData()
{
    // We don't support buffering data on the native side.
    notImplemented();
    return false;
}

void ResourceHandle::platformSetDefersLoading(bool)
{
    notImplemented();
}

// This static method is called to check to see if a POST response is in
// the cache. The JNI call through to the HTTP cache stored on the Java
// side may be slow, but is only used during a navigation to
// a POST response.
bool ResourceHandle::willLoadFromCache(ResourceRequest& request, Frame*)
{
    // set the cache policy correctly, copied from
    // network/mac/ResourceHandleMac.mm
    request.setCachePolicy(ReturnCacheDataDontLoad);
    FormData* formData = request.httpBody();
    return ResourceLoaderAndroid::willLoadFromCache(request.url(), formData ? formData->identifier() : 0);
}

bool ResourceHandle::loadsBlocked() 
{
    // FIXME, need to check whether connection pipe is blocked.
    // return false for now
    return false; 
}

// Class to handle synchronized loading of resources.
class SyncLoader : public ResourceHandleClient {
public:
    SyncLoader(ResourceError& error, ResourceResponse& response, WTF::Vector<char>& data)
    {
        m_error = &error;
        m_response = &response;
        m_data = &data;
    }
    ~SyncLoader() {}

    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
    {
        *m_response = response;
    }

    virtual void didReceiveData(ResourceHandle*, const char* data, int len, int lengthReceived)
    {
        m_data->append(data, len);
    }

    virtual void didFail(ResourceHandle*, const ResourceError& error)
    {
        *m_error = error;
    }

private:
    ResourceError* m_error;
    ResourceResponse* m_response;
    WTF::Vector<char>* m_data;
};

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request,
        StoredCredentials /*storedCredentials*/,
        ResourceError& error, ResourceResponse& response, WTF::Vector<char>& data,
        Frame* frame) 
{
    SyncLoader s(error, response, data);
    RefPtr<ResourceHandle> h = adoptRef(new ResourceHandle(request, &s, false, false, false));
    // This blocks until the load is finished.
    ResourceLoaderAndroid::start(h.get(), request, frame->loader()->client(), false, true);
}

} // namespace WebCore
