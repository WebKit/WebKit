/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#ifndef BURLPROTOCOLHANDLER_H
#define BURLPROTOCOLHANDLER_H

#include "HaikuFormDataStream.h"
#include "ResourceRequest.h"
#include "ResourceError.h"

#include <support/Locker.h>
#include <Messenger.h>
#include <HttpRequest.h>
#include <UrlProtocolAsynchronousListener.h>

class BFile;

namespace WebCore {

class NetworkingContext;
class NetworkStorageSession;
class ResourceHandle;
class ResourceResponse;

class BUrlProtocolHandler;

class BUrlRequestWrapper : public RefCounted<BUrlRequestWrapper>, public BUrlProtocolAsynchronousListener {
public:
    static RefPtr<BUrlRequestWrapper> create(BUrlProtocolHandler*, NetworkStorageSession*, ResourceRequest&);
    virtual ~BUrlRequestWrapper();

    void abort();

    bool isValid() const { return m_request; };

    // BUrlProtocolListener hooks
    void HeadersReceived(BUrlRequest* caller, const BUrlResult&) override;
    void DataReceived(BUrlRequest* caller, const char* data, off_t position, ssize_t) override;
    void UploadProgress(BUrlRequest* caller, ssize_t bytesSent, ssize_t bytesTotal) override;
    void RequestCompleted(BUrlRequest* caller, bool success) override;
    bool CertificateVerificationFailed(BUrlRequest* caller, BCertificate&, const char* message) override;

private:
    BUrlRequestWrapper(BUrlProtocolHandler*, NetworkStorageSession*, ResourceRequest&);

private:
    BUrlProtocolHandler* m_handler { nullptr };
    BUrlRequest* m_request { nullptr };

    bool m_didReceiveData { false };
};

class BUrlProtocolHandler {
public:
    explicit BUrlProtocolHandler(ResourceHandle *handle);
    virtual ~BUrlProtocolHandler();

    void abort();

    bool isValid() const { return m_request && m_request->isValid(); }

private:
    void didFail(const ResourceError& error);
    void willSendRequest(const ResourceResponse& response);
    void continueAfterWillSendRequest(ResourceRequest&& request);
    bool didReceiveAuthenticationChallenge(const ResourceResponse& response);
    void didReceiveResponse(ResourceResponse&& response);
    void didReceiveData(const char* data, size_t);
    void didSendData(ssize_t bytesSent, ssize_t bytesTotal);
    void didFinishLoading();
    bool didReceiveInvalidCertificate(BCertificate&, const char* message);

private:
    friend class BUrlRequestWrapper;

    ResourceRequest m_resourceRequest;
    ResourceHandle* m_resourceHandle;
    RefPtr<BUrlRequestWrapper> m_request;

    unsigned m_redirectionTries { 0 };
    unsigned m_authenticationTries { 0 };
};

}

#endif // BURLPROTOCOLHANDLER_H
