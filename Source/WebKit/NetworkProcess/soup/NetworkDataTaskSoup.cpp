/*
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NetworkDataTaskSoup.h"

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationManager.h"
#include "DataReference.h"
#include "Download.h"
#include "NetworkLoad.h"
#include "NetworkProcess.h"
#include "NetworkSessionSoup.h"
#include "WebErrors.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SoupNetworkSession.h>
#include <WebCore/TextEncoding.h>
#include <wtf/MainThread.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace WebKit {
using namespace WebCore;

static const size_t gDefaultReadBufferSize = 8192;

NetworkDataTaskSoup::NetworkDataTaskSoup(NetworkSession& session, NetworkDataTaskClient& client, const ResourceRequest& requestWithCredentials, StoredCredentialsPolicy storedCredentialsPolicy, ContentSniffingPolicy shouldContentSniff, WebCore::ContentEncodingSniffingPolicy, bool shouldClearReferrerOnHTTPSToHTTPRedirect, bool dataTaskIsForMainFrameNavigation)
    : NetworkDataTask(session, client, requestWithCredentials, storedCredentialsPolicy, shouldClearReferrerOnHTTPSToHTTPRedirect, dataTaskIsForMainFrameNavigation)
    , m_shouldContentSniff(shouldContentSniff)
    , m_timeoutSource(RunLoop::main(), this, &NetworkDataTaskSoup::timeoutFired)
{
    m_session->registerNetworkDataTask(*this);
    if (m_scheduledFailureType != NoFailure)
        return;

    auto request = requestWithCredentials;
    if (request.url().protocolIsInHTTPFamily()) {
        m_startTime = MonotonicTime::now();
        auto url = request.url();
        if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
            m_user = url.user();
            m_password = url.pass();
            request.removeCredentials();

            if (m_user.isEmpty() && m_password.isEmpty())
                m_initialCredential = m_session->networkStorageSession().credentialStorage().get(m_partition, request.url());
            else
                m_session->networkStorageSession().credentialStorage().set(m_partition, Credential(m_user, m_password, CredentialPersistenceNone), request.url());
        }
        applyAuthenticationToRequest(request);
    }
    createRequest(WTFMove(request));
}

NetworkDataTaskSoup::~NetworkDataTaskSoup()
{
    clearRequest();
    m_session->unregisterNetworkDataTask(*this);
}

String NetworkDataTaskSoup::suggestedFilename() const
{
    if (!m_suggestedFilename.isEmpty())
        return m_suggestedFilename;

    String suggestedFilename = m_response.suggestedFilename();
    if (!suggestedFilename.isEmpty())
        return suggestedFilename;

    return decodeURLEscapeSequences(m_response.url().lastPathComponent());
}

void NetworkDataTaskSoup::setPendingDownloadLocation(const String& filename, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
    NetworkDataTask::setPendingDownloadLocation(filename, WTFMove(sandboxExtensionHandle), allowOverwrite);
    m_allowOverwriteDownload = allowOverwrite;
}

void NetworkDataTaskSoup::createRequest(ResourceRequest&& request)
{
    m_currentRequest = WTFMove(request);

    GUniquePtr<SoupURI> soupURI = m_currentRequest.createSoupURI();
    if (!soupURI) {
        scheduleFailure(InvalidURLFailure);
        return;
    }

    GRefPtr<SoupRequest> soupRequest = adoptGRef(soup_session_request_uri(static_cast<NetworkSessionSoup&>(m_session.get()).soupSession(), soupURI.get(), nullptr));
    if (!soupRequest) {
        scheduleFailure(InvalidURLFailure);
        return;
    }

    m_currentRequest.updateSoupRequest(soupRequest.get());

    if (!m_currentRequest.url().protocolIsInHTTPFamily()) {
        m_soupRequest = WTFMove(soupRequest);
        return;
    }

    // HTTP request.
    GRefPtr<SoupMessage> soupMessage = adoptGRef(soup_request_http_get_message(SOUP_REQUEST_HTTP(soupRequest.get())));
    if (!soupMessage) {
        scheduleFailure(InvalidURLFailure);
        return;
    }

    unsigned messageFlags = SOUP_MESSAGE_NO_REDIRECT;

    m_currentRequest.updateSoupMessage(soupMessage.get());
    if (m_shouldContentSniff == ContentSniffingPolicy::DoNotSniffContent)
        soup_message_disable_feature(soupMessage.get(), SOUP_TYPE_CONTENT_SNIFFER);
    if (m_user.isEmpty() && m_password.isEmpty() && m_storedCredentialsPolicy == StoredCredentialsPolicy::DoNotUse) {
#if SOUP_CHECK_VERSION(2, 57, 1)
        messageFlags |= SOUP_MESSAGE_DO_NOT_USE_AUTH_CACHE;
#else
        // In case credential is not available and credential storage should not to be used,
        // disable authentication manager so that credentials stored in libsoup are not used.
        soup_message_disable_feature(soupMessage.get(), SOUP_TYPE_AUTH_MANAGER);
#endif
    }

    // Make sure we have an Accept header for subresources; some sites want this to serve some of their subresources.
    if (!soup_message_headers_get_one(soupMessage->request_headers, "Accept"))
        soup_message_headers_append(soupMessage->request_headers, "Accept", "*/*");

    // In the case of XHR .send() and .send("") explicitly tell libsoup to send a zero content-lenght header
    // for consistency with other UA implementations like Firefox. It's done in the backend here instead of
    // in XHR code since in XHR CORS checking prevents us from this kind of late header manipulation.
    if ((soupMessage->method == SOUP_METHOD_POST || soupMessage->method == SOUP_METHOD_PUT) && !soupMessage->request_body->length)
        soup_message_headers_set_content_length(soupMessage->request_headers, 0);

    soup_message_set_flags(soupMessage.get(), static_cast<SoupMessageFlags>(soup_message_get_flags(soupMessage.get()) | messageFlags));

#if SOUP_CHECK_VERSION(2, 43, 1)
    soup_message_set_priority(soupMessage.get(), toSoupMessagePriority(m_currentRequest.priority()));
#endif

    m_soupRequest = WTFMove(soupRequest);
    m_soupMessage = WTFMove(soupMessage);

    g_signal_connect(m_soupMessage.get(), "got-headers", G_CALLBACK(gotHeadersCallback), this);
    g_signal_connect(m_soupMessage.get(), "wrote-body-data", G_CALLBACK(wroteBodyDataCallback), this);
    g_signal_connect(static_cast<NetworkSessionSoup&>(m_session.get()).soupSession(), "authenticate",  G_CALLBACK(authenticateCallback), this);
    g_signal_connect(m_soupMessage.get(), "network-event", G_CALLBACK(networkEventCallback), this);
    g_signal_connect(m_soupMessage.get(), "restarted", G_CALLBACK(restartedCallback), this);
#if SOUP_CHECK_VERSION(2, 49, 91)
    g_signal_connect(m_soupMessage.get(), "starting", G_CALLBACK(startingCallback), this);
#else
    g_signal_connect(static_cast<NetworkSessionSoup&>(m_session.get()).soupSession(), "request-started", G_CALLBACK(requestStartedCallback), this);
#endif
}

void NetworkDataTaskSoup::clearRequest()
{
    if (m_state == State::Completed)
        return;

    m_state = State::Completed;

    stopTimeout();
    m_pendingResult = nullptr;
    m_soupRequest = nullptr;
    m_inputStream = nullptr;
    m_multipartInputStream = nullptr;
    m_downloadOutputStream = nullptr;
    g_cancellable_cancel(m_cancellable.get());
    m_cancellable = nullptr;
    if (m_soupMessage) {
        g_signal_handlers_disconnect_matched(m_soupMessage.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        soup_session_cancel_message(static_cast<NetworkSessionSoup&>(m_session.get()).soupSession(), m_soupMessage.get(), SOUP_STATUS_CANCELLED);
        m_soupMessage = nullptr;
    }
    g_signal_handlers_disconnect_matched(static_cast<NetworkSessionSoup&>(m_session.get()).soupSession(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
}

void NetworkDataTaskSoup::resume()
{
    ASSERT(m_state != State::Running);
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Running;

    if (m_scheduledFailureType != NoFailure) {
        ASSERT(m_failureTimer.isActive());
        return;
    }

    startTimeout();

    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    if (m_soupRequest && !m_cancellable) {
        m_cancellable = adoptGRef(g_cancellable_new());
        soup_request_send_async(m_soupRequest.get(), m_cancellable.get(), reinterpret_cast<GAsyncReadyCallback>(sendRequestCallback), protectedThis.leakRef());
        return;
    }

    if (m_pendingResult) {
        GRefPtr<GAsyncResult> pendingResult = WTFMove(m_pendingResult);
        if (m_inputStream)
            readCallback(m_inputStream.get(), pendingResult.get(), protectedThis.leakRef());
        else if (m_multipartInputStream)
            requestNextPartCallback(m_multipartInputStream.get(), pendingResult.get(), protectedThis.leakRef());
        else if (m_soupRequest)
            sendRequestCallback(m_soupRequest.get(), pendingResult.get(), protectedThis.leakRef());
        else
            ASSERT_NOT_REACHED();
    }
}

void NetworkDataTaskSoup::suspend()
{
    ASSERT(m_state != State::Suspended);
    if (m_state == State::Canceling || m_state == State::Completed)
        return;
    m_state = State::Suspended;

    stopTimeout();
}

void NetworkDataTaskSoup::cancel()
{
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Canceling;

    if (m_soupMessage)
        soup_session_cancel_message(static_cast<NetworkSessionSoup&>(m_session.get()).soupSession(), m_soupMessage.get(), SOUP_STATUS_CANCELLED);

    g_cancellable_cancel(m_cancellable.get());

    if (isDownload())
        cleanDownloadFiles();
}

void NetworkDataTaskSoup::invalidateAndCancel()
{
    cancel();
    clearRequest();
}

NetworkDataTask::State NetworkDataTaskSoup::state() const
{
    return m_state;
}

void NetworkDataTaskSoup::timeoutFired()
{
    if (m_state == State::Canceling || m_state == State::Completed || !m_client) {
        clearRequest();
        return;
    }

    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    invalidateAndCancel();
    dispatchDidCompleteWithError(ResourceError::timeoutError(m_firstRequest.url()));
}

void NetworkDataTaskSoup::startTimeout()
{
    if (m_firstRequest.timeoutInterval() > 0)
        m_timeoutSource.startOneShot(1_s * m_firstRequest.timeoutInterval());
}

void NetworkDataTaskSoup::stopTimeout()
{
    m_timeoutSource.stop();
}

void NetworkDataTaskSoup::sendRequestCallback(SoupRequest* soupRequest, GAsyncResult* result, NetworkDataTaskSoup* task)
{
    RefPtr<NetworkDataTaskSoup> protectedThis = adoptRef(task);
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }
    ASSERT(soupRequest == task->m_soupRequest.get());

    if (task->state() == State::Suspended) {
        ASSERT(!task->m_pendingResult);
        task->m_pendingResult = result;
        return;
    }

    GUniqueOutPtr<GError> error;
    GRefPtr<GInputStream> inputStream = adoptGRef(soup_request_send_finish(soupRequest, result, &error.outPtr()));
    if (error)
        task->didFail(ResourceError::httpError(task->m_soupMessage.get(), error.get(), soupRequest));
    else
        task->didSendRequest(WTFMove(inputStream));
}

void NetworkDataTaskSoup::didSendRequest(GRefPtr<GInputStream>&& inputStream)
{
    if (m_soupMessage) {
        if (m_shouldContentSniff == ContentSniffingPolicy::SniffContent && m_soupMessage->status_code != SOUP_STATUS_NOT_MODIFIED)
            m_response.setSniffedContentType(soup_request_get_content_type(m_soupRequest.get()));
        m_response.updateFromSoupMessage(m_soupMessage.get());
        if (m_response.mimeType().isEmpty() && m_soupMessage->status_code != SOUP_STATUS_NOT_MODIFIED)
            m_response.setMimeType(MIMETypeRegistry::getMIMETypeForPath(m_response.url().path()));

        if (shouldStartHTTPRedirection()) {
            m_inputStream = WTFMove(inputStream);
            skipInputStreamForRedirection();
            return;
        }

        if (m_response.isMultipart())
            m_multipartInputStream = adoptGRef(soup_multipart_input_stream_new(m_soupMessage.get(), inputStream.get()));
        else
            m_inputStream = WTFMove(inputStream);

        m_networkLoadMetrics.responseStart = MonotonicTime::now() - m_startTime;
    } else {
        m_response.setURL(m_firstRequest.url());
        const gchar* contentType = soup_request_get_content_type(m_soupRequest.get());
        m_response.setMimeType(extractMIMETypeFromMediaType(contentType));
        m_response.setTextEncodingName(extractCharsetFromMediaType(contentType));
        m_response.setExpectedContentLength(soup_request_get_content_length(m_soupRequest.get()));
        if (m_response.mimeType().isEmpty())
            m_response.setMimeType(MIMETypeRegistry::getMIMETypeForPath(m_response.url().path()));

        m_inputStream = WTFMove(inputStream);
    }

    dispatchDidReceiveResponse();
}

void NetworkDataTaskSoup::dispatchDidReceiveResponse()
{
    ASSERT(!m_response.isNull());

    // FIXME: Remove this once nobody depends on deprecatedNetworkLoadMetrics.
    NetworkLoadMetrics& deprecatedResponseMetrics = m_response.deprecatedNetworkLoadMetrics();
    deprecatedResponseMetrics.responseStart = m_networkLoadMetrics.responseStart;
    deprecatedResponseMetrics.domainLookupStart = m_networkLoadMetrics.domainLookupStart;
    deprecatedResponseMetrics.domainLookupEnd = m_networkLoadMetrics.domainLookupEnd;
    deprecatedResponseMetrics.connectStart = m_networkLoadMetrics.connectStart;
    deprecatedResponseMetrics.secureConnectionStart = m_networkLoadMetrics.secureConnectionStart;
    deprecatedResponseMetrics.connectEnd = m_networkLoadMetrics.connectEnd;
    deprecatedResponseMetrics.requestStart = m_networkLoadMetrics.requestStart;
    deprecatedResponseMetrics.responseStart = m_networkLoadMetrics.responseStart;

    didReceiveResponse(ResourceResponse(m_response), [this, protectedThis = makeRef(*this)](PolicyAction policyAction) {
        if (m_state == State::Canceling || m_state == State::Completed) {
            clearRequest();
            return;
        }

        switch (policyAction) {
        case PolicyAction::Use:
            if (m_inputStream)
                read();
            else if (m_multipartInputStream)
                requestNextPart();
            else
                ASSERT_NOT_REACHED();

            break;
        case PolicyAction::Ignore:
            clearRequest();
            break;
        case PolicyAction::Download:
            download();
            break;
        }
    });
}

void NetworkDataTaskSoup::dispatchDidCompleteWithError(const ResourceError& error)
{
    m_networkLoadMetrics.responseEnd = MonotonicTime::now() - m_startTime;
    m_networkLoadMetrics.markComplete();

    m_client->didCompleteWithError(error, m_networkLoadMetrics);
}

gboolean NetworkDataTaskSoup::tlsConnectionAcceptCertificateCallback(GTlsConnection* connection, GTlsCertificate* certificate, GTlsCertificateFlags errors, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return FALSE;
    }

    auto* connectionMessage = g_object_get_data(G_OBJECT(connection), "wk-soup-message");
    if (connectionMessage != task->m_soupMessage.get())
        return FALSE;

    return task->tlsConnectionAcceptCertificate(certificate, errors);
}

bool NetworkDataTaskSoup::tlsConnectionAcceptCertificate(GTlsCertificate* certificate, GTlsCertificateFlags tlsErrors)
{
    ASSERT(m_soupRequest);
    URL url = soupURIToURL(soup_request_get_uri(m_soupRequest.get()));
    auto error = SoupNetworkSession::checkTLSErrors(url, certificate, tlsErrors);
    if (!error)
        return true;

    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    invalidateAndCancel();
    dispatchDidCompleteWithError(error.value());
    return false;
}

void NetworkDataTaskSoup::applyAuthenticationToRequest(ResourceRequest& request)
{
    if (m_user.isEmpty() && m_password.isEmpty())
        return;

    auto url = request.url();
    url.setUser(m_user);
    url.setPass(m_password);
    request.setURL(url);

    m_user = String();
    m_password = String();
}

void NetworkDataTaskSoup::authenticateCallback(SoupSession* session, SoupMessage* soupMessage, SoupAuth* soupAuth, gboolean retrying, NetworkDataTaskSoup* task)
{
    ASSERT(session == static_cast<NetworkSessionSoup&>(task->m_session.get()).soupSession());

    // We don't return early here in case the given soupMessage is different to m_soupMessage when
    // it's proxy authentication and the request URL is HTTPS, because in that case libsoup uses a
    // tunnel internally and the SoupMessage used for the authentication is the tunneling one.
    // See https://bugs.webkit.org/show_bug.cgi?id=175378.
    if (soupMessage != task->m_soupMessage.get() && (soupMessage->status_code != SOUP_STATUS_PROXY_AUTHENTICATION_REQUIRED || !task->m_currentRequest.url().protocolIs("https")))
        return;

    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }

    task->authenticate(AuthenticationChallenge(soupMessage, soupAuth, retrying));
}

static inline bool isAuthenticationFailureStatusCode(int httpStatusCode)
{
    return httpStatusCode == SOUP_STATUS_PROXY_AUTHENTICATION_REQUIRED || httpStatusCode == SOUP_STATUS_UNAUTHORIZED;
}

void NetworkDataTaskSoup::authenticate(AuthenticationChallenge&& challenge)
{
    ASSERT(m_soupMessage);
    if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        if (!m_initialCredential.isEmpty() || challenge.previousFailureCount()) {
            // The stored credential wasn't accepted, stop using it. There is a race condition
            // here, since a different credential might have already been stored by another
            // NetworkDataTask, but the observable effect should be very minor, if any.
            m_session->networkStorageSession().credentialStorage().remove(m_partition, challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            auto credential = m_session->networkStorageSession().credentialStorage().get(m_partition, challenge.protectionSpace());
            if (!credential.isEmpty() && credential != m_initialCredential) {
                ASSERT(credential.persistence() == CredentialPersistenceNone);

                if (isAuthenticationFailureStatusCode(challenge.failureResponse().httpStatusCode())) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    m_session->networkStorageSession().credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }
                soup_auth_authenticate(challenge.soupAuth(), credential.user().utf8().data(), credential.password().utf8().data());
                return;
            }
        }
    }

    soup_session_pause_message(static_cast<NetworkSessionSoup&>(m_session.get()).soupSession(), challenge.soupMessage());

    // We could also do this before we even start the request, but that would be at the expense
    // of all request latency, versus a one-time latency for the small subset of requests that
    // use HTTP authentication. In the end, this doesn't matter much, because persistent credentials
    // will become session credentials after the first use.
    if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        auto protectionSpace = challenge.protectionSpace();
        m_session->networkStorageSession().getCredentialFromPersistentStorage(protectionSpace, m_cancellable.get(),
            [this, protectedThis = makeRef(*this), authChallenge = WTFMove(challenge)] (Credential&& credential) mutable {
                if (m_state == State::Canceling || m_state == State::Completed || !m_client) {
                    clearRequest();
                    return;
                }

                authChallenge.setProposedCredential(WTFMove(credential));
                continueAuthenticate(WTFMove(authChallenge));
        });
    } else
        continueAuthenticate(WTFMove(challenge));
}

void NetworkDataTaskSoup::continueAuthenticate(AuthenticationChallenge&& challenge)
{
    m_client->didReceiveChallenge(AuthenticationChallenge(challenge), [this, protectedThis = makeRef(*this), challenge](AuthenticationChallengeDisposition disposition, const Credential& credential) {
        if (m_state == State::Canceling || m_state == State::Completed) {
            clearRequest();
            return;
        }

        if (disposition == AuthenticationChallengeDisposition::Cancel) {
            cancel();
            didFail(cancelledError(m_soupRequest.get()));
            return;
        }

        if (disposition == AuthenticationChallengeDisposition::UseCredential && !credential.isEmpty()) {
            if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
                // Eventually we will manage per-session credentials only internally or use some newly-exposed API from libsoup,
                // because once we authenticate via libsoup, there is no way to ignore it for a particular request. Right now,
                // we place the credentials in the store even though libsoup will never fire the authenticate signal again for
                // this protection space.
                if (credential.persistence() == CredentialPersistenceForSession || credential.persistence() == CredentialPersistencePermanent)
                    m_session->networkStorageSession().credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());

                if (credential.persistence() == CredentialPersistencePermanent) {
                    m_protectionSpaceForPersistentStorage = challenge.protectionSpace();
                    m_credentialForPersistentStorage = credential;
                }
            }

            soup_auth_authenticate(challenge.soupAuth(), credential.user().utf8().data(), credential.password().utf8().data());
        }

        soup_session_unpause_message(static_cast<NetworkSessionSoup&>(m_session.get()).soupSession(), challenge.soupMessage());
    });
}

void NetworkDataTaskSoup::skipInputStreamForRedirectionCallback(GInputStream* inputStream, GAsyncResult* result, NetworkDataTaskSoup* task)
{
    RefPtr<NetworkDataTaskSoup> protectedThis = adoptRef(task);
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }
    ASSERT(inputStream == task->m_inputStream.get());

    GUniqueOutPtr<GError> error;
    gssize bytesSkipped = g_input_stream_skip_finish(inputStream, result, &error.outPtr());
    if (error)
        task->didFail(ResourceError::genericGError(error.get(), task->m_soupRequest.get()));
    else if (bytesSkipped > 0)
        task->skipInputStreamForRedirection();
    else
        task->didFinishSkipInputStreamForRedirection();
}

void NetworkDataTaskSoup::skipInputStreamForRedirection()
{
    ASSERT(m_inputStream);
    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    g_input_stream_skip_async(m_inputStream.get(), gDefaultReadBufferSize, RunLoopSourcePriority::AsyncIONetwork, m_cancellable.get(),
        reinterpret_cast<GAsyncReadyCallback>(skipInputStreamForRedirectionCallback), protectedThis.leakRef());
}

void NetworkDataTaskSoup::didFinishSkipInputStreamForRedirection()
{
    g_input_stream_close(m_inputStream.get(), nullptr, nullptr);
    continueHTTPRedirection();
}

static bool shouldRedirectAsGET(SoupMessage* message, bool crossOrigin)
{
    if (message->method == SOUP_METHOD_GET || message->method == SOUP_METHOD_HEAD)
        return false;

    switch (message->status_code) {
    case SOUP_STATUS_SEE_OTHER:
        return true;
    case SOUP_STATUS_FOUND:
    case SOUP_STATUS_MOVED_PERMANENTLY:
        if (message->method == SOUP_METHOD_POST)
            return true;
        break;
    }

    if (crossOrigin && message->method == SOUP_METHOD_DELETE)
        return true;

    return false;
}

bool NetworkDataTaskSoup::shouldStartHTTPRedirection()
{
    ASSERT(m_soupMessage);
    ASSERT(!m_response.isNull());

    auto status = m_response.httpStatusCode();
    if (!SOUP_STATUS_IS_REDIRECTION(status))
        return false;

    // Some 3xx status codes aren't actually redirects.
    if (status == 300 || status == 304 || status == 305 || status == 306)
        return false;

    if (m_response.httpHeaderField(HTTPHeaderName::Location).isEmpty())
        return false;

    return true;
}

void NetworkDataTaskSoup::continueHTTPRedirection()
{
    ASSERT(m_soupMessage);
    ASSERT(!m_response.isNull());

    static const unsigned maxRedirects = 20;
    if (m_redirectCount++ > maxRedirects) {
        didFail(ResourceError::transportError(m_soupRequest.get(), SOUP_STATUS_TOO_MANY_REDIRECTS, "Too many redirects"));
        return;
    }

    ResourceRequest request = m_currentRequest;
    URL redirectedURL = URL(m_response.url(), m_response.httpHeaderField(HTTPHeaderName::Location));
    if (!redirectedURL.hasFragmentIdentifier() && request.url().hasFragmentIdentifier())
        redirectedURL.setFragmentIdentifier(request.url().fragmentIdentifier());
    request.setURL(redirectedURL);

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (m_shouldClearReferrerOnHTTPSToHTTPRedirect && !request.url().protocolIs("https") && protocolIs(request.httpReferrer(), "https"))
        request.clearHTTPReferrer();

    bool isCrossOrigin = !protocolHostAndPortAreEqual(m_currentRequest.url(), request.url());
    if (!equalLettersIgnoringASCIICase(request.httpMethod(), "get")) {
        // Change newRequest method to GET if change was made during a previous redirection or if current redirection says so.
        if (m_soupMessage->method == SOUP_METHOD_GET || !request.url().protocolIsInHTTPFamily() || shouldRedirectAsGET(m_soupMessage.get(), isCrossOrigin)) {
            request.setHTTPMethod("GET");
            request.setHTTPBody(nullptr);
            request.clearHTTPContentType();
        }
    }

    const auto& url = request.url();
    m_user = url.user();
    m_password = url.pass();
    m_lastHTTPMethod = request.httpMethod();
    request.removeCredentials();

    if (isCrossOrigin) {
        // The network layer might carry over some headers from the original request that
        // we want to strip here because the redirect is cross-origin.
        request.clearHTTPAuthorization();
        request.clearHTTPOrigin();
    } else if (url.protocolIsInHTTPFamily() && m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        if (m_user.isEmpty() && m_password.isEmpty()) {
            auto credential = m_session->networkStorageSession().credentialStorage().get(m_partition, request.url());
            if (!credential.isEmpty())
                m_initialCredential = credential;
        }
    }

    clearRequest();

    auto response = ResourceResponse(m_response);
    m_client->willPerformHTTPRedirection(WTFMove(response), WTFMove(request), [this, protectedThis = makeRef(*this), isCrossOrigin](const ResourceRequest& newRequest) {
        if (newRequest.isNull() || m_state == State::Canceling)
            return;

        auto request = newRequest;
        if (request.url().protocolIsInHTTPFamily()) {
            if (isCrossOrigin) {
                m_startTime = MonotonicTime::now();
                m_networkLoadMetrics.reset();
            }

            applyAuthenticationToRequest(request);
        }
        createRequest(WTFMove(request));
        if (m_soupRequest && m_state != State::Suspended) {
            m_state = State::Suspended;
            resume();
        }
    });
}

void NetworkDataTaskSoup::readCallback(GInputStream* inputStream, GAsyncResult* result, NetworkDataTaskSoup* task)
{
    RefPtr<NetworkDataTaskSoup> protectedThis = adoptRef(task);
    if (task->state() == State::Canceling || task->state() == State::Completed || (!task->m_client && !task->isDownload())) {
        task->clearRequest();
        return;
    }
    ASSERT(inputStream == task->m_inputStream.get());

    if (task->state() == State::Suspended) {
        ASSERT(!task->m_pendingResult);
        task->m_pendingResult = result;
        return;
    }

    GUniqueOutPtr<GError> error;
    gssize bytesRead = g_input_stream_read_finish(inputStream, result, &error.outPtr());
    if (error)
        task->didFail(ResourceError::genericGError(error.get(), task->m_soupRequest.get()));
    else if (bytesRead > 0)
        task->didRead(bytesRead);
    else
        task->didFinishRead();
}

void NetworkDataTaskSoup::read()
{
    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    ASSERT(m_inputStream);
    m_readBuffer.grow(gDefaultReadBufferSize);
    g_input_stream_read_async(m_inputStream.get(), m_readBuffer.data(), m_readBuffer.size(), RunLoopSourcePriority::AsyncIONetwork, m_cancellable.get(),
        reinterpret_cast<GAsyncReadyCallback>(readCallback), protectedThis.leakRef());
}

void NetworkDataTaskSoup::didRead(gssize bytesRead)
{
    m_readBuffer.shrink(bytesRead);
    if (m_downloadOutputStream) {
        ASSERT(isDownload());
        writeDownload();
    } else {
        ASSERT(m_client);
        m_client->didReceiveData(SharedBuffer::create(WTFMove(m_readBuffer)));
        read();
    }
}

void NetworkDataTaskSoup::didFinishRead()
{
    ASSERT(m_inputStream);
    g_input_stream_close(m_inputStream.get(), nullptr, nullptr);
    m_inputStream = nullptr;
    if (m_multipartInputStream) {
        requestNextPart();
        return;
    }

    if (m_downloadOutputStream) {
        didFinishDownload();
        return;
    }

    clearRequest();
    ASSERT(m_client);
    dispatchDidCompleteWithError({ });
}

void NetworkDataTaskSoup::requestNextPartCallback(SoupMultipartInputStream* multipartInputStream, GAsyncResult* result, NetworkDataTaskSoup* task)
{
    RefPtr<NetworkDataTaskSoup> protectedThis = adoptRef(task);
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }
    ASSERT(multipartInputStream == task->m_multipartInputStream.get());

    if (task->state() == State::Suspended) {
        ASSERT(!task->m_pendingResult);
        task->m_pendingResult = result;
        return;
    }

    GUniqueOutPtr<GError> error;
    GRefPtr<GInputStream> inputStream = adoptGRef(soup_multipart_input_stream_next_part_finish(multipartInputStream, result, &error.outPtr()));
    if (error)
        task->didFail(ResourceError::httpError(task->m_soupMessage.get(), error.get(), task->m_soupRequest.get()));
    else if (inputStream)
        task->didRequestNextPart(WTFMove(inputStream));
    else
        task->didFinishRequestNextPart();
}

void NetworkDataTaskSoup::requestNextPart()
{
    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    ASSERT(m_multipartInputStream);
    ASSERT(!m_inputStream);
    soup_multipart_input_stream_next_part_async(m_multipartInputStream.get(), RunLoopSourcePriority::AsyncIONetwork, m_cancellable.get(),
        reinterpret_cast<GAsyncReadyCallback>(requestNextPartCallback), protectedThis.leakRef());
}

void NetworkDataTaskSoup::didRequestNextPart(GRefPtr<GInputStream>&& inputStream)
{
    ASSERT(!m_inputStream);
    m_inputStream = WTFMove(inputStream);
    m_response = ResourceResponse();
    m_response.setURL(m_firstRequest.url());
    m_response.updateFromSoupMessageHeaders(soup_multipart_input_stream_get_headers(m_multipartInputStream.get()));
    dispatchDidReceiveResponse();
}

void NetworkDataTaskSoup::didFinishRequestNextPart()
{
    ASSERT(!m_inputStream);
    ASSERT(m_multipartInputStream);
    g_input_stream_close(G_INPUT_STREAM(m_multipartInputStream.get()), nullptr, nullptr);
    clearRequest();
    dispatchDidCompleteWithError({ });
}

void NetworkDataTaskSoup::gotHeadersCallback(SoupMessage* soupMessage, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }
    ASSERT(task->m_soupMessage.get() == soupMessage);
    task->didGetHeaders();
}

void NetworkDataTaskSoup::didGetHeaders()
{
    // We are a bit more conservative with the persistent credential storage than the session store,
    // since we are waiting until we know that this authentication succeeded before actually storing.
    // This is because we want to avoid hitting the disk twice (once to add and once to remove) for
    // incorrect credentials or polluting the keychain with invalid credentials.
    if (!isAuthenticationFailureStatusCode(m_soupMessage->status_code) && m_soupMessage->status_code < 500) {
        m_session->networkStorageSession().saveCredentialToPersistentStorage(m_protectionSpaceForPersistentStorage, m_credentialForPersistentStorage);
        m_protectionSpaceForPersistentStorage = ProtectionSpace();
        m_credentialForPersistentStorage = Credential();
    }

    // Soup adds more headers to the request after starting signal is emitted, and got-headers
    // is the first one we receive after starting, so we use it also to get information about the
    // request headers.
    if (shouldCaptureExtraNetworkLoadMetrics()) {
        HTTPHeaderMap requestHeaders;
        SoupMessageHeadersIter headersIter;
        soup_message_headers_iter_init(&headersIter, m_soupMessage->request_headers);
        const char* headerName;
        const char* headerValue;
        while (soup_message_headers_iter_next(&headersIter, &headerName, &headerValue))
            requestHeaders.set(String(headerName), String(headerValue));
        m_networkLoadMetrics.requestHeaders = WTFMove(requestHeaders);
    }
}

void NetworkDataTaskSoup::wroteBodyDataCallback(SoupMessage* soupMessage, SoupBuffer* buffer, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }
    ASSERT(task->m_soupMessage.get() == soupMessage);
    task->didWriteBodyData(buffer->length);
}

void NetworkDataTaskSoup::didWriteBodyData(uint64_t bytesSent)
{
    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    m_bodyDataTotalBytesSent += bytesSent;
    m_client->didSendData(m_bodyDataTotalBytesSent, m_soupMessage->request_body->length);
}

void NetworkDataTaskSoup::download()
{
    ASSERT(isDownload());
    ASSERT(m_pendingDownloadLocation);
    ASSERT(!m_response.isNull());

    if (m_response.httpStatusCode() >= 400) {
        didFailDownload(downloadNetworkError(m_response.url(), m_response.httpStatusText()));
        return;
    }

    CString downloadDestinationPath = m_pendingDownloadLocation.utf8();
    m_downloadDestinationFile = adoptGRef(g_file_new_for_path(downloadDestinationPath.data()));
    GRefPtr<GFileOutputStream> outputStream;
    GUniqueOutPtr<GError> error;
    if (m_allowOverwriteDownload)
        outputStream = adoptGRef(g_file_replace(m_downloadDestinationFile.get(), nullptr, FALSE, G_FILE_CREATE_NONE, nullptr, &error.outPtr()));
    else
        outputStream = adoptGRef(g_file_create(m_downloadDestinationFile.get(), G_FILE_CREATE_NONE, nullptr, &error.outPtr()));
    if (!outputStream) {
        didFailDownload(downloadDestinationError(m_response, error->message));
        return;
    }

    GUniquePtr<char> intermediatePath(g_strdup_printf("%s.wkdownload", downloadDestinationPath.data()));
    m_downloadIntermediateFile = adoptGRef(g_file_new_for_path(intermediatePath.get()));
    outputStream = adoptGRef(g_file_replace(m_downloadIntermediateFile.get(), nullptr, TRUE, G_FILE_CREATE_NONE, nullptr, &error.outPtr()));
    if (!outputStream) {
        didFailDownload(downloadDestinationError(m_response, error->message));
        return;
    }
    m_downloadOutputStream = adoptGRef(G_OUTPUT_STREAM(outputStream.leakRef()));

    auto& downloadManager = NetworkProcess::singleton().downloadManager();
    auto download = std::make_unique<Download>(downloadManager, m_pendingDownloadID, *this, m_session->sessionID(), suggestedFilename());
    auto* downloadPtr = download.get();
    downloadManager.dataTaskBecameDownloadTask(m_pendingDownloadID, WTFMove(download));
    downloadPtr->didCreateDestination(m_pendingDownloadLocation);

    ASSERT(!m_client);
    read();
}

void NetworkDataTaskSoup::writeDownloadCallback(GOutputStream* outputStream, GAsyncResult* result, NetworkDataTaskSoup* task)
{
    RefPtr<NetworkDataTaskSoup> protectedThis = adoptRef(task);
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->isDownload()) {
        task->clearRequest();
        return;
    }
    ASSERT(outputStream == task->m_downloadOutputStream.get());

    GUniqueOutPtr<GError> error;
    gsize bytesWritten;
#if GLIB_CHECK_VERSION(2, 44, 0)
    g_output_stream_write_all_finish(outputStream, result, &bytesWritten, &error.outPtr());
#else
    gssize writeTaskResult = g_task_propagate_int(G_TASK(result), &error.outPtr());
    if (writeTaskResult != -1)
        bytesWritten = writeTaskResult;
#endif
    if (error)
        task->didFailDownload(downloadDestinationError(task->m_response, error->message));
    else
        task->didWriteDownload(bytesWritten);
}

void NetworkDataTaskSoup::writeDownload()
{
    RefPtr<NetworkDataTaskSoup> protectedThis(this);
#if GLIB_CHECK_VERSION(2, 44, 0)
    g_output_stream_write_all_async(m_downloadOutputStream.get(), m_readBuffer.data(), m_readBuffer.size(), RunLoopSourcePriority::AsyncIONetwork, m_cancellable.get(),
        reinterpret_cast<GAsyncReadyCallback>(writeDownloadCallback), protectedThis.leakRef());
#else
    GRefPtr<GTask> writeTask = adoptGRef(g_task_new(m_downloadOutputStream.get(), m_cancellable.get(),
        reinterpret_cast<GAsyncReadyCallback>(writeDownloadCallback), protectedThis.leakRef()));
    g_task_set_task_data(writeTask.get(), this, nullptr);
    g_task_run_in_thread(writeTask.get(), [](GTask* writeTask, gpointer source, gpointer userData, GCancellable* cancellable) {
        auto* task = static_cast<NetworkDataTaskSoup*>(userData);
        GOutputStream* outputStream = G_OUTPUT_STREAM(source);
        RELEASE_ASSERT(task->m_downloadOutputStream.get() == outputStream);
        RELEASE_ASSERT(task->m_cancellable.get() == cancellable);
        GError* error = nullptr;
        if (g_cancellable_set_error_if_cancelled(cancellable, &error)) {
            g_task_return_error(writeTask, error);
            return;
        }

        gsize bytesWritten;
        if (g_output_stream_write_all(outputStream, task->m_readBuffer.data(), task->m_readBuffer.size(), &bytesWritten, cancellable, &error))
            g_task_return_int(writeTask, bytesWritten);
        else
            g_task_return_error(writeTask, error);
    });
#endif
}

void NetworkDataTaskSoup::didWriteDownload(gsize bytesWritten)
{
    ASSERT(bytesWritten == m_readBuffer.size());
    auto* download = NetworkProcess::singleton().downloadManager().download(m_pendingDownloadID);
    ASSERT(download);
    download->didReceiveData(bytesWritten);
    read();
}

void NetworkDataTaskSoup::didFinishDownload()
{
    ASSERT(!m_response.isNull());
    ASSERT(m_downloadOutputStream);
    g_output_stream_close(m_downloadOutputStream.get(), nullptr, nullptr);
    m_downloadOutputStream = nullptr;

    ASSERT(m_downloadDestinationFile);
    ASSERT(m_downloadIntermediateFile);
    GUniqueOutPtr<GError> error;
    if (!g_file_move(m_downloadIntermediateFile.get(), m_downloadDestinationFile.get(), G_FILE_COPY_OVERWRITE, m_cancellable.get(), nullptr, nullptr, &error.outPtr())) {
        didFailDownload(downloadDestinationError(m_response, error->message));
        return;
    }

    GRefPtr<GFileInfo> info = adoptGRef(g_file_info_new());
    CString uri = m_response.url().string().utf8();
    g_file_info_set_attribute_string(info.get(), "metadata::download-uri", uri.data());
    g_file_info_set_attribute_string(info.get(), "xattr::xdg.origin.url", uri.data());
    g_file_set_attributes_async(m_downloadDestinationFile.get(), info.get(), G_FILE_QUERY_INFO_NONE, RunLoopSourcePriority::AsyncIONetwork, nullptr, nullptr, nullptr);

    clearRequest();
    auto* download = NetworkProcess::singleton().downloadManager().download(m_pendingDownloadID);
    ASSERT(download);
    download->didFinish();
}

void NetworkDataTaskSoup::didFailDownload(const ResourceError& error)
{
    clearRequest();
    cleanDownloadFiles();
    if (m_client)
        dispatchDidCompleteWithError(error);
    else {
        auto* download = NetworkProcess::singleton().downloadManager().download(m_pendingDownloadID);
        ASSERT(download);
        download->didFail(error, IPC::DataReference());
    }
}

void NetworkDataTaskSoup::cleanDownloadFiles()
{
    if (m_downloadDestinationFile) {
        g_file_delete(m_downloadDestinationFile.get(), nullptr, nullptr);
        m_downloadDestinationFile = nullptr;
    }
    if (m_downloadIntermediateFile) {
        g_file_delete(m_downloadIntermediateFile.get(), nullptr, nullptr);
        m_downloadIntermediateFile = nullptr;
    }
}

void NetworkDataTaskSoup::didFail(const ResourceError& error)
{
    if (isDownload()) {
        didFailDownload(downloadNetworkError(error.failingURL(), error.localizedDescription()));
        return;
    }

    clearRequest();
    ASSERT(m_client);
    dispatchDidCompleteWithError(error);
}

void NetworkDataTaskSoup::networkEventCallback(SoupMessage* soupMessage, GSocketClientEvent event, GIOStream* stream, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client)
        return;

    ASSERT(task->m_soupMessage.get() == soupMessage);
    task->networkEvent(event, stream);
}

void NetworkDataTaskSoup::networkEvent(GSocketClientEvent event, GIOStream* stream)
{
    Seconds deltaTime = MonotonicTime::now() - m_startTime;
    switch (event) {
    case G_SOCKET_CLIENT_RESOLVING:
        m_networkLoadMetrics.domainLookupStart = deltaTime;
        break;
    case G_SOCKET_CLIENT_RESOLVED:
        m_networkLoadMetrics.domainLookupEnd = deltaTime;
        break;
    case G_SOCKET_CLIENT_CONNECTING:
        m_networkLoadMetrics.connectStart = deltaTime;
        break;
    case G_SOCKET_CLIENT_CONNECTED:
        // Web Timing considers that connection time involves dns, proxy & TLS negotiation...
        // so we better pick G_SOCKET_CLIENT_COMPLETE for connectEnd
        break;
    case G_SOCKET_CLIENT_PROXY_NEGOTIATING:
        break;
    case G_SOCKET_CLIENT_PROXY_NEGOTIATED:
        break;
    case G_SOCKET_CLIENT_TLS_HANDSHAKING:
        m_networkLoadMetrics.secureConnectionStart = deltaTime;
        RELEASE_ASSERT(G_IS_TLS_CONNECTION(stream));
        g_object_set_data(G_OBJECT(stream), "wk-soup-message", m_soupMessage.get());
        g_signal_connect(stream, "accept-certificate", G_CALLBACK(tlsConnectionAcceptCertificateCallback), this);
        break;
    case G_SOCKET_CLIENT_TLS_HANDSHAKED:
        break;
    case G_SOCKET_CLIENT_COMPLETE:
        m_networkLoadMetrics.connectEnd = deltaTime;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

#if SOUP_CHECK_VERSION(2, 49, 91)
void NetworkDataTaskSoup::startingCallback(SoupMessage* soupMessage, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client)
        return;

    ASSERT(task->m_soupMessage.get() == soupMessage);
    task->didStartRequest();
}
#else
void NetworkDataTaskSoup::requestStartedCallback(SoupSession* session, SoupMessage* soupMessage, SoupSocket*, NetworkDataTaskSoup* task)
{
    ASSERT(session == static_cast<NetworkSessionSoup&>(task->m_session.get()).soupSession());
    if (soupMessage != task->m_soupMessage.get())
        return;

    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client)
        return;

    task->didStartRequest();
}
#endif

void NetworkDataTaskSoup::didStartRequest()
{
    m_networkLoadMetrics.requestStart = MonotonicTime::now() - m_startTime;
}

void NetworkDataTaskSoup::restartedCallback(SoupMessage* soupMessage, NetworkDataTaskSoup* task)
{
    // Called each time the message is going to be sent again except the first time.
    // This happens when libsoup handles HTTP authentication.
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client)
        return;

    ASSERT(task->m_soupMessage.get() == soupMessage);
    task->didRestart();
}

void NetworkDataTaskSoup::didRestart()
{
    m_startTime = MonotonicTime::now();
    m_networkLoadMetrics.reset();
}

} // namespace WebKit

