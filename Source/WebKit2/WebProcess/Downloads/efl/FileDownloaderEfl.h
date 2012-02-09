/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef FileDownloaderEfl_h
#define FileDownloaderEfl_h

#include <WebCore/ResourceError.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceHandleClient.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {
class AuthenticationChallenge;
class ResourceError;
class ResourceHandle;
class ResourceHandleClient;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {

class Download;
class WebPage;

class FileDownloaderEfl : public WebCore::ResourceHandleClient {
public:
    static PassOwnPtr<FileDownloaderEfl> create(Download*);
    virtual ~FileDownloaderEfl();

    void start(WebPage*, WebCore::ResourceRequest&);

    //  callbacks for ResourceHandleClient, which are called by ResourceHandle
    virtual void didReceiveResponse(WebCore::ResourceHandle*, const WebCore::ResourceResponse&) OVERRIDE;
    virtual void didReceiveData(WebCore::ResourceHandle*, const char* data, int length, int encodedDataLength) OVERRIDE;
    virtual void didFinishLoading(WebCore::ResourceHandle*, double finishTime) OVERRIDE;
    virtual void didFail(WebCore::ResourceHandle*, const WebCore::ResourceError&) OVERRIDE;
    virtual bool shouldUseCredentialStorage(WebCore::ResourceHandle*) OVERRIDE;
    virtual void didReceiveAuthenticationChallenge(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) OVERRIDE;
    virtual void didCancelAuthenticationChallenge(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) OVERRIDE;
    virtual void receivedCancellation(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) OVERRIDE;

private:
    FileDownloaderEfl(Download*);

    Download* m_download;
};

} // namespace WebKit

#endif // FileDownloaderEfl_h
