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

#include <support/Locker.h>
#include <Messenger.h>
#include <HttpRequest.h>
#include <UrlProtocolAsynchronousListener.h>

class BFile;

namespace WebCore {

class NetworkingContext;
class ResourceHandle;
class ResourceResponse;

class BUrlProtocolHandler : public BUrlProtocolAsynchronousListener
{
public:
    BUrlProtocolHandler(NetworkingContext* context, ResourceHandle *handle, 
        bool synchronous);
    virtual ~BUrlProtocolHandler();
    void abort();

    bool isValid() { return m_request != NULL; }

private:
    void AuthenticationNeeded(BHttpRequest* caller, ResourceResponse& response);

    // BUrlListener hooks
    void ConnectionOpened(BUrlRequest* caller) override;
    void HeadersReceived(BUrlRequest* caller, const BUrlResult& result) override;
    void DataReceived(BUrlRequest* caller, const char* data, off_t position,
        ssize_t size) override;
    void UploadProgress(BUrlRequest* caller, ssize_t bytesSent, ssize_t bytesTotal) override;
    void RequestCompleted(BUrlRequest* caller, bool success) override;
    bool CertificateVerificationFailed(BUrlRequest* caller, BCertificate& certificate, const char* message) override;

private:
    ResourceHandle* m_resourceHandle;
    bool m_redirected;
    bool m_responseDataSent;
    BFormDataIO* m_postData;
    BUrlRequest* m_request;
    off_t m_position;
    URL m_baseUrl;

    int m_redirectionTries;
};

}

#endif // BURLPROTOCOLHANDLER_H
