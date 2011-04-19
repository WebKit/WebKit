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
#include "Settings.h"
#include <wtf/text/CString.h>

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
}

bool ResourceHandle::start(NetworkingContext* context)
{
    MainResourceLoader* mainLoader = context->mainResourceLoader();
    bool isMainResource = static_cast<void*>(mainLoader) == static_cast<void*>(client());
    RefPtr<ResourceLoaderAndroid> loader = ResourceLoaderAndroid::start(this, d->m_firstRequest, context->frameLoaderClient(), isMainResource, false);

    if (loader) {
        d->m_loader = loader.release();
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

    virtual void didReceiveData(ResourceHandle*, const char* data, int len, int encodedDataLength)
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

void ResourceHandle::loadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request,
        StoredCredentials, ResourceError& error, ResourceResponse& response, WTF::Vector<char>& data)
{
    SyncLoader s(error, response, data);
    RefPtr<ResourceHandle> h = adoptRef(new ResourceHandle(request, &s, false, false));
    // This blocks until the load is finished.
    // Use the request owned by the ResourceHandle. This has had the username
    // and password (if present) stripped from the URL in
    // ResourceHandleInternal::ResourceHandleInternal(). This matches the
    // behaviour in the asynchronous case.
    ResourceLoaderAndroid::start(h.get(), request, context->frameLoaderClient(), false, true);
}

} // namespace WebCore
