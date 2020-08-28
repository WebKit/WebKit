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

static bool shouldRedirectAsGET(const ResourceRequest& request, int statusCode, bool crossOrigin)
{
    if (request.httpMethod() == "GET" || request.httpMethod() == "HEAD")
        return false;

    if (statusCode == 303)
        return true;

    if ((statusCode == 301 || statusCode == 302) && request.httpMethod() == "POST")
        return true;

    if (crossOrigin && request.httpMethod() == "DELETE")
        return true;

    return false;
}

RefPtr<BUrlRequestWrapper> BUrlRequestWrapper::create(BUrlProtocolHandler* handler, NetworkStorageSession* storageSession, ResourceRequest& request)
{
    return adoptRef(*new BUrlRequestWrapper(handler, storageSession, request));
}

BUrlRequestWrapper::BUrlRequestWrapper(BUrlProtocolHandler* handler, NetworkStorageSession* storageSession, ResourceRequest& request)
    : BUrlProtocolAsynchronousListener(true)
    , m_handler(handler)
{
    ASSERT(m_handler);
    ASSERT(storageSession);

    m_request = request.toNetworkRequest(&storageSession->platformSession());

    if (!m_request)
        return;

    m_request->SetListener(SynchronousListener());

    BHttpRequest* httpRequest = dynamic_cast<BHttpRequest*>(m_request);
    if (httpRequest) {
        if (request.httpMethod() == "POST" || request.httpMethod() == "PUT") {
            if (request.httpBody()) {
                auto postData = new BFormDataIO(request.httpBody(), storageSession->sessionID());
                httpRequest->AdoptInputData(postData, postData->Size());
            }
        }

        httpRequest->SetMethod(request.httpMethod().utf8().data());
        // Redirections will be handled by this class.
        httpRequest->SetFollowLocation(false);
    } else if (request.httpMethod() != "GET") {
        // Only the HTTP backend support things other than GET.
        // Remove m_request to signify to ResourceHandle that the request was
        // invalid.
        delete m_request;
        m_request = NULL;
        return;
    }

    if (m_request->Run() < B_OK) {
        ResourceError error("BUrlProtocol", 42,
            request.url(),
            "The service kit failed to start the request.");
        m_handler->didFail(error);

        return;
    }

    // Keep self alive while BUrlRequest is running as we hold
    // the main dispatcher.
    ref();
}

BUrlRequestWrapper::~BUrlRequestWrapper()
{
    abort();
    delete m_request;
}

void BUrlRequestWrapper::abort()
{
    if (m_request)
        m_request->Stop();

    m_handler = nullptr;
}

void BUrlRequestWrapper::HeadersReceived(BUrlRequest* caller, const BUrlResult& result)
{
    if (!m_handler)
        return;

    ResourceResponse response(URL(caller->Url()),
        extractMIMETypeFromMediaType(result.ContentType()), result.Length(),
        extractCharsetFromMediaType(result.ContentType()));

    const BHttpResult* httpResult = dynamic_cast<const BHttpResult*>(&result);
    if (httpResult) {
        String suggestedFilename = filenameFromHTTPContentDisposition(
            httpResult->Headers()["Content-Disposition"]);

        if (!suggestedFilename.isEmpty())
            response.setSuggestedFilename(suggestedFilename);

        response.setHTTPStatusCode(httpResult->StatusCode());
        response.setHTTPStatusText(httpResult->StatusText());

        // Add remaining headers.
        const BHttpHeaders& resultHeaders = httpResult->Headers();
        for (int i = 0; i < resultHeaders.CountHeaders(); i++) {
            BHttpHeader& headerPair = resultHeaders.HeaderAt(i);
            response.setHTTPHeaderField(headerPair.Name(), headerPair.Value());
        }

        if (response.isRedirection() && !response.httpHeaderField(HTTPHeaderName::Location).isEmpty()) {
            m_handler->willSendRequest(response);
            return;
        }

        if (response.httpStatusCode() == 401 && m_handler->didReceiveAuthenticationChallenge(response))
            return;
    }

    ResourceResponse responseCopy = response;
    m_handler->didReceiveResponse(WTFMove(responseCopy));
}

void BUrlRequestWrapper::DataReceived(BUrlRequest*, const char* data,
    off_t, ssize_t size)
{
    if (!m_handler)
        return;

    if (size > 0) {
        m_didReceiveData = true;
        m_handler->didReceiveData(data, size);
    }
}

void BUrlRequestWrapper::UploadProgress(BUrlRequest*, ssize_t bytesSent, ssize_t bytesTotal)
{
    if (!m_handler)
        return;

    m_handler->didSendData(bytesSent, bytesTotal);
}

void BUrlRequestWrapper::RequestCompleted(BUrlRequest* caller, bool success)
{
    // We held a pointer to keep the main dispatcher alive for the duration
    // of the request run.
    //
    // As the request completes, we adopt the ref here so that it can
    // release itself after completion.
    auto releaseThis = adoptRef(*this);

    if (!m_handler)
        return;

    BHttpRequest* httpRequest = dynamic_cast<BHttpRequest*>(m_request);

    if (success || (httpRequest && m_didReceiveData)) {
        m_handler->didFinishLoading();
        return;
    } else if (httpRequest) {
        const BHttpResult& result = static_cast<const BHttpResult&>(httpRequest->Result());
        int httpStatusCode = result.StatusCode();

        if (httpStatusCode != 0) {
            ResourceError error("HTTP", httpStatusCode,
                URL(caller->Url()), strerror(caller->Status()));

            m_handler->didFail(error);
            return;
        }
    }

    // If we get here, it means we are in failure without an HTTP error code
    // (DNS error, or error from a protocol other than HTTP).
    ResourceError error("BUrlRequest", caller->Status(), URL(caller->Url()), strerror(caller->Status()));
    m_handler->didFail(error);
}

bool BUrlRequestWrapper::CertificateVerificationFailed(BUrlRequest*,
    BCertificate& certificate, const char* message)
{
    if (!m_handler)
        return false;

    return m_handler->didReceiveInvalidCertificate(certificate, message);
}

BUrlProtocolHandler::BUrlProtocolHandler(ResourceHandle* handle)
    : m_resourceHandle(handle)
    , m_request()
{
    if (!m_resourceHandle)
        return;

    m_resourceRequest = m_resourceHandle->firstRequest();
    m_request = BUrlRequestWrapper::create(this,
        m_resourceHandle->context()->storageSession(),
        m_resourceRequest);
}

BUrlProtocolHandler::~BUrlProtocolHandler()
{
    abort();
}

void BUrlProtocolHandler::abort()
{
    if (m_request)
        m_request->abort();

    m_resourceHandle = nullptr;
}

void BUrlProtocolHandler::didFail(const ResourceError& error)
{
    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    client->didFail(m_resourceHandle, error);
}

void BUrlProtocolHandler::willSendRequest(const ResourceResponse& response)
{
    if (!m_resourceHandle)
        return;

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    ResourceRequest request = m_resourceHandle->firstRequest();

    m_redirectionTries++;

    if (m_redirectionTries > gMaxRecursionLimit) {
        ResourceError error(request.url().host().utf8().data(), 400, request.url(),
            "Redirection limit reached");
        client->didFail(m_resourceHandle, error);
        return;
    }

    URL newUrl = URL(request.url(), response.httpHeaderField(HTTPHeaderName::Location));

    bool crossOrigin = !protocolHostAndPortAreEqual(request.url(), newUrl);

    request.setURL(newUrl);

    if (!newUrl.protocolIsInHTTPFamily() || shouldRedirectAsGET(request, response.httpStatusCode(), crossOrigin)) {
        request.setHTTPMethod("GET");
        request.setHTTPBody(nullptr);
        request.clearHTTPContentType();
    }

    if (crossOrigin) {
        request.clearHTTPAuthorization();
        request.clearHTTPOrigin();
    }

    m_request->abort();
    ResourceResponse responseCopy = response;
    client->willSendRequestAsync(m_resourceHandle, WTFMove(request), WTFMove(responseCopy), [this] (ResourceRequest&& request) {
        continueAfterWillSendRequest(WTFMove(request));
    });
}

void BUrlProtocolHandler::continueAfterWillSendRequest(ResourceRequest&& request)
{
    // willSendRequestAsync might cancel the request
    if (!m_resourceHandle->client() || request.isNull())
        return;

    m_resourceRequest = request;
    m_request = BUrlRequestWrapper::create(this, m_resourceHandle->context()->storageSession(), request);
}

bool BUrlProtocolHandler::didReceiveAuthenticationChallenge(const ResourceResponse& response)
{
    if (!m_resourceHandle || !m_resourceHandle->client())
        return false;

    const URL& url = response.url();
    ProtectionSpaceServerType serverType = ProtectionSpaceServerHTTP;
    if (url.protocolIs("https"))
        serverType = ProtectionSpaceServerHTTPS;

    static NeverDestroyed<String> wwwAuthenticate(MAKE_STATIC_STRING_IMPL("www-authenticate"));
    String challenge = response.httpHeaderField(wwwAuthenticate);

    ProtectionSpaceAuthenticationScheme scheme = ProtectionSpaceAuthenticationSchemeDefault;
    // TODO according to RFC7235, there could be more than one challenge in WWW-Authenticate. We
    // should parse them all, instead of just the first one.
    if (challenge.startsWith("Digest"))
        scheme = ProtectionSpaceAuthenticationSchemeHTTPDigest;
    else if (challenge.startsWith("Basic"))
        scheme = ProtectionSpaceAuthenticationSchemeHTTPBasic;
    else {
        // Unknown authentication type, ignore (various websites are intercepting the auth and
        // handling it by themselves)
        return false;
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

    ResourceHandleInternal* d = m_resourceHandle->getInternal();

    Credential proposedCredential(d->m_user, d->m_password, CredentialPersistenceForSession);

    AuthenticationChallenge authenticationChallenge(protectionSpace,
        proposedCredential, m_authenticationTries++, response, resourceError);
    authenticationChallenge.m_authenticationClient = m_resourceHandle;
    m_resourceHandle->didReceiveAuthenticationChallenge(authenticationChallenge);
    // will set m_user and m_password in ResourceHandleInternal

    if (d->m_user != "") {
        ResourceRequest request = m_resourceRequest;
        ResourceResponse responseCopy = response;
        request.setCredentials(d->m_user.utf8().data(), d->m_password.utf8().data());
        m_request->abort();
        m_resourceHandle->client()->willSendRequestAsync(m_resourceHandle,
                WTFMove(request), WTFMove(responseCopy), [this] (ResourceRequest&& request) {
            continueAfterWillSendRequest(WTFMove(request));
        });
        return true;
    }

    return false;
}

void BUrlProtocolHandler::didReceiveResponse(ResourceResponse&& response)
{
    if (!m_resourceHandle)
        return;

    // Make sure the resource handle is not deleted immediately, otherwise
    // didReceiveResponse would crash. Keep a reference to it so it can be
    // deleted cleanly after the function returns.
    auto protectedHandle = makeRef(*m_resourceHandle);
    protectedHandle->didReceiveResponse(WTFMove(response), [this/*, protectedThis = makeRef(*this)*/] {
        //continueAfterDidReceiveResponse();
    });
}

void BUrlProtocolHandler::didReceiveData(const char* data, size_t size)
{
    if (!m_resourceHandle || !m_resourceHandle->client())
        return;

    m_resourceHandle->client()->didReceiveData(m_resourceHandle, data, size, size);
}

void BUrlProtocolHandler::didSendData(ssize_t bytesSent, ssize_t bytesTotal)
{
    if (!m_resourceHandle || !m_resourceHandle->client())
        return;

    m_resourceHandle->client()->didSendData(m_resourceHandle, bytesSent, bytesTotal);
}

void BUrlProtocolHandler::didFinishLoading()
{
    if (!m_resourceHandle || !m_resourceHandle->client())
        return;

    m_resourceHandle->client()->didFinishLoading(m_resourceHandle);
}

bool BUrlProtocolHandler::didReceiveInvalidCertificate(BCertificate& certificate, const char* message)
{
    if (!m_resourceHandle)
        return false;

    return m_resourceHandle->didReceiveInvalidCertificate(certificate, message);
}

} // namespace WebCore
