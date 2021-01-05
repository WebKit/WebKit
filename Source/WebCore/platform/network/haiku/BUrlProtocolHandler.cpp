/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.  <info@staikos.net>
    Copyright (C) 2008 Holger Hans Peter Freyther

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
#include "config.h"
#include "BUrlProtocolHandler.h"

#include "FormData.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "NetworkStorageSession.h"
#include "ProtectionSpace.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "ResourceRequest.h"
#include <wtf/CompletionHandler.h>
#include <wtf/text/CString.h>

#include <Debug.h>
#include <File.h>
#include <Url.h>
#include <UrlRequest.h>
#include <HttpRequest.h>

#include <assert.h>

static const int gMaxRecursionLimit = 10;

namespace WebCore {

BUrlProtocolHandler::BUrlProtocolHandler(NetworkingContext* context,
        ResourceHandle* handle, bool synchronous)
    : BUrlProtocolAsynchronousListener(!synchronous)
    , m_resourceHandle(handle)
    , m_redirected(false)
    , m_responseDataSent(false)
    , m_postData(NULL)
    , m_request(handle->firstRequest().toNetworkRequest(
        context ? &context->storageSession()->platformSession() : nullptr))
    , m_position(0)
    , m_redirectionTries(gMaxRecursionLimit)
{
    if (!m_resourceHandle)
        return;

    BString method = BString(m_resourceHandle->firstRequest().httpMethod());

    m_postData = NULL;

    if (m_request == NULL)
        return;

    m_baseUrl = URL(m_request->Url());

    BHttpRequest* httpRequest = dynamic_cast<BHttpRequest*>(m_request);
    if(httpRequest) {
        // TODO maybe we have data to send in other cases ?
        if(method == B_HTTP_POST || method == B_HTTP_PUT) {
            FormData* form = m_resourceHandle->firstRequest().httpBody();
            if(form) {
                m_postData = new BFormDataIO(form, context->storageSession()->sessionID());
                httpRequest->AdoptInputData(m_postData, m_postData->Size());
            }
        }

        httpRequest->SetMethod(method.String());
    }

    // In synchronous mode, call this listener directly.
    // In asynchronous mode, go through a BMessage
    if(this->SynchronousListener()) {
        m_request->SetListener(this->SynchronousListener());
    } else {
        m_request->SetListener(this);
    }

    if (m_request->Run() < B_OK) {
        ResourceHandleClient* client = m_resourceHandle->client();
        if (!client)
            return;

        ResourceError error("BUrlProtocol", 42,
            handle->firstRequest().url(),
            "The service kit failed to start the request.");
        client->didFail(m_resourceHandle, error);
    }
}

BUrlProtocolHandler::~BUrlProtocolHandler()
{
    abort();
    if (m_request)
        m_request->SetListener(NULL);
    delete m_request;
}

void BUrlProtocolHandler::abort()
{
    if (m_resourceHandle != NULL && m_request != NULL)
        m_request->Stop();

    m_resourceHandle = NULL;
}

static bool ignoreHttpError(BHttpRequest* reply, bool receivedData)
{
    int httpStatusCode = static_cast<const BHttpResult&>(reply->Result()).StatusCode();

    if (httpStatusCode == 401 || httpStatusCode == 407)
        return false;

    if (receivedData && (httpStatusCode >= 400 && httpStatusCode < 600))
        return true;

    return false;
}

void BUrlProtocolHandler::RequestCompleted(BUrlRequest* caller, bool success)
{
    if (!m_resourceHandle)
        return;

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    BHttpRequest* httpRequest = dynamic_cast<BHttpRequest*>(m_request);

    if (success || (httpRequest && ignoreHttpError(httpRequest, m_responseDataSent))) {
        client->didFinishLoading(m_resourceHandle);
        return;
    } else if(httpRequest) {
        const BHttpResult& result = static_cast<const BHttpResult&>(httpRequest->Result());
        int httpStatusCode = result.StatusCode();

        if (httpStatusCode != 0) {
            ResourceError error("HTTP", httpStatusCode,
                URL(caller->Url()), strerror(caller->Status()));

            client->didFail(m_resourceHandle, error);
            return;
        }
    }

    // If we get here, it means we are in failure without an HTTP error code
    // (DNS error, or error from a protocol other than HTTP).
    ResourceError error("BUrlRequest", caller->Status(), URL(caller->Url()), strerror(caller->Status()));
    client->didFail(m_resourceHandle, error);
}


bool BUrlProtocolHandler::CertificateVerificationFailed(BUrlRequest*,
    BCertificate& certificate, const char* message)
{
    return m_resourceHandle->didReceiveInvalidCertificate(certificate, message);
}


void BUrlProtocolHandler::AuthenticationNeeded(BHttpRequest* request, ResourceResponse& response)
{
    if (!m_resourceHandle)
        return;

    ResourceHandleInternal* d = m_resourceHandle->getInternal();
    unsigned failureCount = 0;

    const URL& url = m_resourceHandle->firstRequest().url();
    ProtectionSpaceServerType serverType = ProtectionSpaceServerHTTP;
    if (url.protocolIs("https"))
        serverType = ProtectionSpaceServerHTTPS;

    String challenge = static_cast<const BHttpResult&>(request->Result()).Headers()["WWW-Authenticate"];
    ProtectionSpaceAuthenticationScheme scheme = ProtectionSpaceAuthenticationSchemeDefault;

    ResourceHandleClient* client = m_resourceHandle->client();

    // TODO according to RFC7235, there could be more than one challenge in WWW-Authenticate. We
    // should parse them all, instead of just the first one.
    if (challenge.startsWith("Digest"))
        scheme = ProtectionSpaceAuthenticationSchemeHTTPDigest;
    else if (challenge.startsWith("Basic"))
        scheme = ProtectionSpaceAuthenticationSchemeHTTPBasic;
    else {
        // Unknown authentication type, ignore (various websites are intercepting the auth and
        // handling it by themselves)
        return;
    }

    String realm;
    int realmStart = challenge.find("realm=\"", 0);
    if (realmStart > 0) {
        realmStart += 7;
        int realmEnd = challenge.find("\"", realmStart);
        if (realmEnd >= 0)
            realm = challenge.substring(realmStart, realmEnd - realmStart);
    }

    int port;
    if (url.port())
        port = *url.port();
    else if (url.protocolIs("https"))
        port = 443;
    else
        port = 80;
    ProtectionSpace protectionSpace(url.host().utf8().data(), port, serverType, realm, scheme);
    ResourceError resourceError(url.host().utf8().data(), 401, url, String());

    m_redirectionTries--;
    if(m_redirectionTries == 0)
    {
        client->didFinishLoading(m_resourceHandle);
        return;
    }

    Credential proposedCredential(d->m_user, d->m_password, CredentialPersistenceForSession);

    AuthenticationChallenge authenticationChallenge(protectionSpace,
        proposedCredential, failureCount++, response, resourceError);
    authenticationChallenge.m_authenticationClient = m_resourceHandle;
    m_resourceHandle->didReceiveAuthenticationChallenge(authenticationChallenge);
            // will set m_user and m_password in ResourceHandleInternal

    if (d->m_user != "") {
        // Handle this just like redirects.
        m_redirected = true;

        ResourceRequest request = m_resourceHandle->firstRequest();
		ResourceResponse responseCopy = response;
        request.setCredentials(d->m_user.utf8().data(), d->m_password.utf8().data());
		client->willSendRequestAsync(m_resourceHandle, WTFMove(request), WTFMove(responseCopy),
    	[handle = makeRef(*m_resourceHandle)] (ResourceRequest&& request) {
        	//continueAfterWillSendRequest(handle.ptr(), WTFMove(request));
    	});
    } else {
        // Anything to do in case of failure?
    }
}


void BUrlProtocolHandler::ConnectionOpened(BUrlRequest*)
{
    m_responseDataSent = false;
}


void BUrlProtocolHandler::HeadersReceived(BUrlRequest* caller,
    const BUrlResult& result)
{
    if (!m_resourceHandle)
        return;

    const BHttpResult* httpResult = dynamic_cast<const BHttpResult*>(&result);

    WTF::String contentType = result.ContentType();
    int contentLength = result.Length();
    URL url;

    WTF::String encoding = extractCharsetFromMediaType(contentType);
    WTF::String mimeType = extractMIMETypeFromMediaType(contentType);

    if (httpResult) {
        url = URL(httpResult->Url());

        BString location = httpResult->Headers()["Location"];
        if (location.Length() > 0) {
            m_redirected = true;
            url = URL(url, location);
        } else {
            m_redirected = false;
        }
    } else {
        url = m_baseUrl;
    }

    ResourceResponse response(url, mimeType, contentLength, encoding);

    if (httpResult) {
        int statusCode = httpResult->StatusCode();

        String suggestedFilename = filenameFromHTTPContentDisposition(
            httpResult->Headers()["Content-Disposition"]);

        if (!suggestedFilename.isEmpty())
            response.setSuggestedFilename(suggestedFilename);

        response.setHTTPStatusCode(statusCode);
        response.setHTTPStatusText(httpResult->StatusText());

        // Add remaining headers.
        const BHttpHeaders& resultHeaders = httpResult->Headers();
        for (int i = 0; i < resultHeaders.CountHeaders(); i++) {
            BHttpHeader& headerPair = resultHeaders.HeaderAt(i);
            response.setHTTPHeaderField(headerPair.Name(), headerPair.Value());
        }

        if (statusCode == 401) {
            AuthenticationNeeded((BHttpRequest*)m_request, response);
            // AuthenticationNeeded may have aborted the request
            // so we need to make sure we can continue.
            if (!m_resourceHandle)
                return;
        }
    }

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    if (m_redirected) {
        m_redirectionTries--;

        if (m_redirectionTries == 0) {
            ResourceError error(url.host().utf8().data(), 400, url,
                "Redirection limit reached");
            client->didFail(m_resourceHandle, error);
            return;
        }

        // Notify the client that we are redirecting.
        ResourceRequest request = m_resourceHandle->firstRequest();
        ResourceResponse responseCopy = response;
        request.setURL(url);

		client->willSendRequestAsync(m_resourceHandle, WTFMove(request), WTFMove(responseCopy),
    	[handle = makeRef(*m_resourceHandle)] (ResourceRequest&& request) {
        	//continueAfterWillSendRequest(handle.ptr(), WTFMove(request));
    	});
    } else {
        ResourceResponse responseCopy = response;
        // Make sure the resource handle is not deleted immediately, otherwise
        // didReceiveResponse would crash. Keep a reference to it so it can be
        // deleted cleanly after the function returns.
        RefPtr<ResourceHandle> protectedHandle(m_resourceHandle);
        protectedHandle->didReceiveResponse(WTFMove(responseCopy), [this/*, protectedThis = makeRef(*this)*/] {
            //continueAfterDidReceiveResponse();
        });
    }
}

void BUrlProtocolHandler::DataReceived(BUrlRequest* caller, const char* data,
    off_t position, ssize_t size)
{
    if (!m_resourceHandle)
        return;

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    // don't emit the "Document has moved here" type of HTML
    if (m_redirected)
        return;

    if (position != m_position)
    {
        debugger("bad redirect");
        return;
    }

    if (size > 0) {
        m_responseDataSent = true;
        client->didReceiveData(m_resourceHandle, data, size, size);
    }

    m_position += size;
}

void BUrlProtocolHandler::UploadProgress(BUrlRequest* caller, ssize_t bytesSent, ssize_t bytesTotal)
{
    if (!m_resourceHandle)
        return;

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    client->didSendData(m_resourceHandle, bytesSent, bytesTotal);
}


}
