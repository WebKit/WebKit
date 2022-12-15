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
#include "PrivateRelayed.h"
#include "WebErrors.h"
#include "WebKitDirectoryInputStream.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PublicSuffix.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>
#include <WebCore/SoupNetworkSession.h>
#include <WebCore/SoupVersioning.h>
#include <WebCore/TimingAllowOrigin.h>
#include <pal/text/TextEncoding.h>
#include <wtf/MainThread.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace WebKit {
using namespace WebCore;

static const size_t gDefaultReadBufferSize = 8192;

NetworkDataTaskSoup::NetworkDataTaskSoup(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters)
    : NetworkDataTask(session, client, parameters.request, parameters.storedCredentialsPolicy, parameters.shouldClearReferrerOnHTTPSToHTTPRedirect, parameters.isMainFrameNavigation)
    , m_frameID(parameters.webFrameID)
    , m_pageID(parameters.webPageID)
    , m_shouldContentSniff(parameters.contentSniffingPolicy)
    , m_shouldPreconnectOnly(parameters.shouldPreconnectOnly)
    , m_sourceOrigin(parameters.sourceOrigin)
    , m_timeoutSource(RunLoop::main(), this, &NetworkDataTaskSoup::timeoutFired)
{
    m_session->registerNetworkDataTask(*this);

    auto request = parameters.request;
    if (request.url().protocolIsInHTTPFamily()) {
#if USE(SOUP2)
        m_networkLoadMetrics.fetchStart = MonotonicTime::now();
        m_networkLoadMetrics.redirectStart = m_networkLoadMetrics.fetchStart;
#endif
        auto url = request.url();
        if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
            m_user = url.user();
            m_password = url.password();
            request.removeCredentials();

            if (m_user.isEmpty() && m_password.isEmpty())
                m_initialCredential = m_session->networkStorageSession()->credentialStorage().get(m_partition, request.url());
            else
                m_session->networkStorageSession()->credentialStorage().set(m_partition, Credential(m_user, m_password, CredentialPersistenceNone), request.url());
        }
        applyAuthenticationToRequest(request);
    }
    createRequest(WTFMove(request), WasBlockingCookies::No);
}

NetworkDataTaskSoup::~NetworkDataTaskSoup()
{
    clearRequest();
}

String NetworkDataTaskSoup::suggestedFilename() const
{
    if (!m_suggestedFilename.isEmpty())
        return m_suggestedFilename;

    String suggestedFilename = m_response.suggestedFilename();
    if (!suggestedFilename.isEmpty())
        return suggestedFilename;

    return PAL::decodeURLEscapeSequences(m_response.url().lastPathComponent());
}

void NetworkDataTaskSoup::setPriority(ResourceLoadPriority priority)
{
    if (!m_soupMessage)
        return;

    SoupMessagePriority soupPriority = SOUP_MESSAGE_PRIORITY_NORMAL;
    switch (priority) {
    case ResourceLoadPriority::VeryLow:
        soupPriority = SOUP_MESSAGE_PRIORITY_VERY_LOW;
        break;
    case ResourceLoadPriority::Low:
        soupPriority = SOUP_MESSAGE_PRIORITY_LOW;
        break;
    case ResourceLoadPriority::Medium:
        soupPriority = SOUP_MESSAGE_PRIORITY_NORMAL;
        break;
    case ResourceLoadPriority::High:
        soupPriority = SOUP_MESSAGE_PRIORITY_HIGH;
        break;
    case ResourceLoadPriority::VeryHigh:
        soupPriority = SOUP_MESSAGE_PRIORITY_VERY_HIGH;
        break;
    }

    soup_message_set_priority(m_soupMessage.get(), soupPriority);
}

void NetworkDataTaskSoup::setPendingDownloadLocation(const String& filename, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
    NetworkDataTask::setPendingDownloadLocation(filename, WTFMove(sandboxExtensionHandle), allowOverwrite);
    m_allowOverwriteDownload = allowOverwrite;
}

void NetworkDataTaskSoup::createRequest(ResourceRequest&& request, WasBlockingCookies wasBlockingCookies)
{
    m_currentRequest = WTFMove(request);
    if (m_currentRequest.url().isLocalFile()) {
        m_file = adoptGRef(g_file_new_for_path(m_currentRequest.url().fileSystemPath().utf8().data()));
        return;
    }

    if (m_currentRequest.url().protocolIsData())
        return;

    if (!m_currentRequest.url().protocolIsInHTTPFamily()) {
        scheduleFailure(FailureType::InvalidURL);
        return;
    }

    restrictRequestReferrerToOriginIfNeeded(m_currentRequest);

    m_soupMessage = m_currentRequest.createSoupMessage(m_session->blobRegistry());
    if (!m_soupMessage) {
        scheduleFailure(FailureType::InvalidURL);
        return;
    }

    if (m_shouldPreconnectOnly == PreconnectOnly::Yes) {
#if !USE(SOUP2)
        g_signal_connect(m_soupMessage.get(), "accept-certificate", G_CALLBACK(acceptCertificateCallback), this);
#endif
        return;
    }

    m_networkLoadMetrics.redirectCount = m_currentRequest.redirectCount();

    unsigned messageFlags = SOUP_MESSAGE_NO_REDIRECT;
#if !USE(SOUP2)
    messageFlags |= SOUP_MESSAGE_COLLECT_METRICS;
#endif
    if (m_shouldContentSniff == ContentSniffingPolicy::DoNotSniffContent)
        soup_message_disable_feature(m_soupMessage.get(), SOUP_TYPE_CONTENT_SNIFFER);
    if (m_user.isEmpty() && m_password.isEmpty() && m_storedCredentialsPolicy == StoredCredentialsPolicy::DoNotUse) {
#if SOUP_CHECK_VERSION(2, 57, 1)
        messageFlags |= SOUP_MESSAGE_DO_NOT_USE_AUTH_CACHE;
#else
        // In case credential is not available and credential storage should not to be used,
        // disable authentication manager so that credentials stored in libsoup are not used.
        soup_message_disable_feature(m_soupMessage.get(), SOUP_TYPE_AUTH_MANAGER);
#endif
    }
    soup_message_set_flags(m_soupMessage.get(), static_cast<SoupMessageFlags>(soup_message_get_flags(m_soupMessage.get()) | messageFlags));

#if ENABLE(TRACKING_PREVENTION)
    bool shouldBlockCookies = wasBlockingCookies == WasBlockingCookies::Yes ? true : m_storedCredentialsPolicy == StoredCredentialsPolicy::EphemeralStateless;
    if (!shouldBlockCookies) {
        if (auto* networkStorageSession = m_session->networkStorageSession())
            shouldBlockCookies = networkStorageSession->shouldBlockCookies(m_currentRequest, m_frameID, m_pageID, WebCore::ShouldRelaxThirdPartyCookieBlocking::No);
    }
    if (shouldBlockCookies)
        soup_message_disable_feature(m_soupMessage.get(), SOUP_TYPE_COOKIE_JAR);
    m_isBlockingCookies = shouldBlockCookies;
#endif

#if SOUP_CHECK_VERSION(2, 67, 1)
    if ((m_currentRequest.url().protocolIs("https"_s) && !shouldAllowHSTSPolicySetting()) || (m_currentRequest.url().protocolIs("http"_s) && !shouldAllowHSTSProtocolUpgrade()))
        soup_message_disable_feature(m_soupMessage.get(), SOUP_TYPE_HSTS_ENFORCER);
    else {
#if USE(SOUP2)
        g_signal_connect(soup_session_get_feature(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), SOUP_TYPE_HSTS_ENFORCER), "hsts-enforced", G_CALLBACK(hstsEnforced), this);
#else
        g_signal_connect(m_soupMessage.get(), "hsts-enforced", G_CALLBACK(hstsEnforced), this);
#endif
    }
#endif

    // Make sure we have an Accept header for subresources; some sites want this to serve some of their subresources.
    auto* requestHeaders = soup_message_get_request_headers(m_soupMessage.get());
    if (!soup_message_headers_get_one(requestHeaders, "Accept"))
        soup_message_headers_append(requestHeaders, "Accept", "*/*");

#if USE(SOUP2)
    // In the case of XHR .send() and .send("") explicitly tell libsoup to send a zero content-lenght header
    // for consistency with other UA implementations like Firefox. It's done in the backend here instead of
    // in XHR code since in XHR CORS checking prevents us from this kind of late header manipulation.
    if ((m_soupMessage->method == SOUP_METHOD_POST || m_soupMessage->method == SOUP_METHOD_PUT) && !m_soupMessage->request_body->length)
        soup_message_headers_set_content_length(m_soupMessage->request_headers, 0);
#endif

    g_signal_connect(m_soupMessage.get(), "got-headers", G_CALLBACK(gotHeadersCallback), this);
    g_signal_connect(m_soupMessage.get(), "wrote-body-data", G_CALLBACK(wroteBodyDataCallback), this);
#if USE(SOUP2)
    g_signal_connect(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), "authenticate",  G_CALLBACK(authenticateCallback), this);
    g_signal_connect(m_soupMessage.get(), "network-event", G_CALLBACK(networkEventCallback), this);
#else
    g_signal_connect(m_soupMessage.get(), "authenticate", G_CALLBACK(authenticateCallback), this);
    g_signal_connect(m_soupMessage.get(), "accept-certificate", G_CALLBACK(acceptCertificateCallback), this);
    g_signal_connect(m_soupMessage.get(), "got-body", G_CALLBACK(gotBodyCallback), this);
    if (shouldCaptureExtraNetworkLoadMetrics()) {
        g_signal_connect(m_soupMessage.get(), "wrote-headers", G_CALLBACK(wroteHeadersCallback), this);
        g_signal_connect(m_soupMessage.get(), "wrote-body", G_CALLBACK(wroteBodyCallback), this);
    }
    g_signal_connect(m_soupMessage.get(), "request-certificate", G_CALLBACK(requestCertificateCallback), this);
    g_signal_connect(m_soupMessage.get(), "request-certificate-password", G_CALLBACK(requestCertificatePasswordCallback), this);
#endif
    g_signal_connect(m_soupMessage.get(), "restarted", G_CALLBACK(restartedCallback), this);
    g_signal_connect(m_soupMessage.get(), "starting", G_CALLBACK(startingCallback), this);
    if (m_shouldContentSniff == ContentSniffingPolicy::SniffContent)
        g_signal_connect(m_soupMessage.get(), "content-sniffed", G_CALLBACK(didSniffContentCallback), this);
}

void NetworkDataTaskSoup::clearRequest()
{
    if (m_state == State::Completed)
        return;

    m_state = State::Completed;

    stopTimeout();
    m_pendingResult = nullptr;
    m_pendingDataURLResult = std::nullopt;
    m_file = nullptr;
    m_inputStream = nullptr;
    m_multipartInputStream = nullptr;
    m_downloadOutputStream = nullptr;
    m_sniffedContentType = { };
    g_cancellable_cancel(m_cancellable.get());
    m_cancellable = nullptr;
    m_isBlockingCookies = false;
    if (m_soupMessage) {
        g_signal_handlers_disconnect_matched(m_soupMessage.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
#if USE(SOUP2)
        if (m_session)
            soup_session_cancel_message(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), m_soupMessage.get(), SOUP_STATUS_CANCELLED);
#else
        if (m_networkLoadMetrics.fetchStart && !m_networkLoadMetrics.responseEnd) {
            auto* metrics = soup_message_get_metrics(m_soupMessage.get());
            auto responseEnd = Seconds::fromMicroseconds(soup_message_metrics_get_response_end(metrics));
            m_networkLoadMetrics.responseEnd = MonotonicTime::fromRawSeconds(responseEnd.seconds());
            m_networkLoadMetrics.markComplete();
        }
#endif
        m_soupMessage = nullptr;
    }
    if (m_session) {
#if USE(SOUP2)
        g_signal_handlers_disconnect_matched(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
#if SOUP_CHECK_VERSION(2, 67, 1)
        g_signal_handlers_disconnect_by_data(soup_session_get_feature(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), SOUP_TYPE_HSTS_ENFORCER), this);
#endif
#endif
    }
}

void NetworkDataTaskSoup::resume()
{
    ASSERT(m_state != State::Running);
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Running;

    startTimeout();

    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    if (m_soupMessage && !m_cancellable) {
        m_cancellable = adoptGRef(g_cancellable_new());
        if (m_shouldPreconnectOnly == PreconnectOnly::Yes) {
#if !USE(SOUP2)
            soup_session_preconnect_async(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), m_soupMessage.get(), RunLoopSourcePriority::AsyncIONetwork, m_cancellable.get(),
                reinterpret_cast<GAsyncReadyCallback>(preconnectCallback), protectedThis.leakRef());
#else
            RELEASE_ASSERT_NOT_REACHED();
#endif
        } else {
            // We need to protect cancellable here, because soup_session_send_async uses it after emitting SoupSession::request-queued, and we
            // might cancel the operation in a feature callback emitted on request-queued, for example hsts-enforced.
            GRefPtr<GCancellable> protectCancellable(m_cancellable);
            soup_session_send_async(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), m_soupMessage.get(), RunLoopSourcePriority::AsyncIONetwork, m_cancellable.get(),
                reinterpret_cast<GAsyncReadyCallback>(sendRequestCallback), new SendRequestData({ m_soupMessage, WTFMove(protectedThis) }));
            if (!g_cancellable_is_cancelled(protectCancellable.get()) && !m_networkLoadMetrics.fetchStart) {
#if USE(SOUP2)
                m_networkLoadMetrics.fetchStart = MonotonicTime::now();
#else
                auto* metrics = soup_message_get_metrics(m_soupMessage.get());
                m_networkLoadMetrics.fetchStart = MonotonicTime::fromRawSeconds(Seconds::fromMicroseconds(soup_message_metrics_get_fetch_start(metrics)).seconds());
#endif
                if (!m_networkLoadMetrics.redirectStart)
                    m_networkLoadMetrics.redirectStart = m_networkLoadMetrics.fetchStart;
            }
        }
        return;
    }

    if (m_file && !m_cancellable) {
        m_networkLoadMetrics.fetchStart = MonotonicTime::now();
        m_cancellable = adoptGRef(g_cancellable_new());
        g_file_query_info_async(m_file.get(), G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SIZE,
            G_FILE_QUERY_INFO_NONE, RunLoopSourcePriority::AsyncIONetwork, m_cancellable.get(), reinterpret_cast<GAsyncReadyCallback>(fileQueryInfoCallback), protectedThis.leakRef());
        return;
    }

    if (m_currentRequest.url().protocolIsData() && !m_cancellable) {
        m_networkLoadMetrics.fetchStart = MonotonicTime::now();
        m_cancellable = adoptGRef(g_cancellable_new());
        DataURLDecoder::decode(m_currentRequest.url(), { }, DataURLDecoder::Mode::Legacy, [this, protectedThis = WTFMove(protectedThis)](auto decodeResult) mutable {
            if (m_state == State::Canceling || m_state == State::Completed || !m_client) {
                clearRequest();
                return;
            }

            if (m_state == State::Suspended) {
                m_pendingDataURLResult = WTFMove(decodeResult);
                return;
            }

            didReadDataURL(WTFMove(decodeResult));
        });
        return;
    }

    if (m_pendingResult) {
        GRefPtr<GAsyncResult> pendingResult = WTFMove(m_pendingResult);
        if (m_inputStream)
            readCallback(m_inputStream.get(), pendingResult.get(), protectedThis.leakRef());
        else if (m_multipartInputStream)
            requestNextPartCallback(m_multipartInputStream.get(), pendingResult.get(), protectedThis.leakRef());
        else if (m_soupMessage) {
            sendRequestCallback(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), pendingResult.get(),
                static_cast<SendRequestData*>(g_object_steal_data(G_OBJECT(pendingResult.get()), "wk-send-request-data")));
        } else if (m_file) {
            if (m_response.expectedContentLength() == -1)
                enumerateFileChildrenCallback(m_file.get(), pendingResult.get(), protectedThis.leakRef());
            else
                readFileCallback(m_file.get(), pendingResult.get(), protectedThis.leakRef());
        } else
            ASSERT_NOT_REACHED();
    } else if (m_currentRequest.url().protocolIsData())
        didReadDataURL(WTFMove(m_pendingDataURLResult));
}

void NetworkDataTaskSoup::cancel()
{
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Canceling;

#if USE(SOUP2)
    if (m_soupMessage)
        soup_session_cancel_message(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), m_soupMessage.get(), SOUP_STATUS_CANCELLED);
#endif

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

void NetworkDataTaskSoup::sendRequestCallback(SoupSession* soupSession, GAsyncResult* result, SendRequestData* data)
{
    std::unique_ptr<SendRequestData> protectedData(data);
    auto* task = data->task.get();

    if (task->m_soupMessage && task->m_soupMessage != data->soupMessage.get()) {
        // This can happen when the request is cancelled and a new one is started before
        // the previous async operation completed. This is common when forcing a redirection
        // due to HSTS. We can simply ignore this old request.
#if ASSERT_ENABLED
        GUniqueOutPtr<GError> error;
        GRefPtr<GInputStream> inputStream = adoptGRef(soup_session_send_finish(soupSession, result, &error.outPtr()));
        ASSERT(g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED));
#endif
        return;
    }

    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }

    if (task->state() == State::Suspended) {
        ASSERT(!task->m_pendingResult);
        task->m_pendingResult = result;
        g_object_set_data_full(G_OBJECT(task->m_pendingResult.get()), "wk-send-request-data", protectedData.release(), [](gpointer data) {
            delete static_cast<SendRequestData*>(data);
        });
        return;
    }

    GUniqueOutPtr<GError> error;
    GRefPtr<GInputStream> inputStream = adoptGRef(soup_session_send_finish(soupSession, result, &error.outPtr()));
    if (error)
        task->didFail(ResourceError::httpError(data->soupMessage.get(), error.get()));
    else
        task->didSendRequest(WTFMove(inputStream));
}

void NetworkDataTaskSoup::didSendRequest(GRefPtr<GInputStream>&& inputStream)
{
    m_response = ResourceResponse(m_soupMessage.get(), m_sniffedContentType);

    if (shouldStartHTTPRedirection()) {
        m_inputStream = WTFMove(inputStream);
        skipInputStreamForRedirection();
        return;
    }

    if (m_response.isMultipart())
        m_multipartInputStream = adoptGRef(soup_multipart_input_stream_new(m_soupMessage.get(), inputStream.get()));
    else
        m_inputStream = WTFMove(inputStream);

#if USE(SOUP2)
    m_networkLoadMetrics.responseStart = MonotonicTime::now();
#endif

    if (!m_networkLoadMetrics.failsTAOCheck) {
        RefPtr<SecurityOrigin> origin = isTopLevelNavigation() ? SecurityOrigin::create(firstRequest().url()) : m_sourceOrigin;
        if (origin)
            m_networkLoadMetrics.failsTAOCheck = !passesTimingAllowOriginCheck(m_response, *origin);
    }

    dispatchDidReceiveResponse();
}

void NetworkDataTaskSoup::dispatchDidReceiveResponse()
{
    ASSERT(!m_response.isNull());

    // FIXME: This cannot be eliminated until other code no longer relies on ResourceResponse's NetworkLoadMetrics.
    m_response.setDeprecatedNetworkLoadMetrics(Box<NetworkLoadMetrics>::create(m_networkLoadMetrics));

    didReceiveResponse(ResourceResponse(m_response), NegotiatedLegacyTLS::No, PrivateRelayed::No, [this, protectedThis = Ref { *this }](PolicyAction policyAction) {
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
        case PolicyAction::StopAllLoads:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

#if !USE(SOUP2)
void NetworkDataTaskSoup::preconnectCallback(SoupSession* session, GAsyncResult* result, NetworkDataTaskSoup* task)
{
    RefPtr<NetworkDataTaskSoup> protectedThis = adoptRef(task);
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }

    ResourceError resourceError;
    GUniqueOutPtr<GError> error;
    if (!soup_session_preconnect_finish(session, result, &error.outPtr()))
        resourceError = ResourceError::genericGError(task->m_currentRequest.url(), error.get());
    task->clearRequest();
    task->dispatchDidCompleteWithError(resourceError);
}
#endif

void NetworkDataTaskSoup::dispatchDidCompleteWithError(const ResourceError& error)
{
#if USE(SOUP2)
    m_networkLoadMetrics.responseEnd = MonotonicTime::now();
    m_networkLoadMetrics.markComplete();
#endif

    m_client->didCompleteWithError(error, m_networkLoadMetrics);
}

#if USE(SOUP2)
gboolean NetworkDataTaskSoup::tlsConnectionAcceptCertificateCallback(GTlsConnection* connection, GTlsCertificate* certificate, GTlsCertificateFlags errors, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return FALSE;
    }

    auto* connectionMessage = g_object_get_data(G_OBJECT(connection), "wk-soup-message");
    if (connectionMessage != task->m_soupMessage.get())
        return FALSE;

    return task->acceptCertificate(certificate, errors);
}
#else
gboolean NetworkDataTaskSoup::acceptCertificateCallback(SoupMessage* message, GTlsCertificate* certificate, GTlsCertificateFlags errors, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client)
        return FALSE;

    ASSERT(task->m_soupMessage.get() == message);

    return task->acceptCertificate(certificate, errors);
}
#endif

bool NetworkDataTaskSoup::acceptCertificate(GTlsCertificate* certificate, GTlsCertificateFlags tlsErrors)
{
    ASSERT(m_soupMessage);
    URL url = soupURIToURL(soup_message_get_uri(m_soupMessage.get()));
    auto error = static_cast<NetworkSessionSoup&>(*m_session).soupNetworkSession().checkTLSErrors(url, certificate, tlsErrors);
    if (!error)
        return true;

    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    invalidateAndCancel();
    dispatchDidCompleteWithError(error.value());
    return false;
}

void NetworkDataTaskSoup::didSniffContentCallback(SoupMessage* soupMessage, const char* contentType, GHashTable* parameters, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client)
        return;

    ASSERT(task->m_soupMessage.get() == soupMessage);
    if (!parameters) {
        task->didSniffContent(contentType);
        return;
    }

    GString* sniffedType = g_string_new(contentType);
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, parameters);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        g_string_append(sniffedType, "; ");
        soup_header_g_string_append_param(sniffedType, static_cast<const char*>(key), static_cast<const char*>(value));
    }
    task->didSniffContent(sniffedType->str);
    g_string_free(sniffedType, TRUE);
}

void NetworkDataTaskSoup::didSniffContent(CString&& contentType)
{
    m_sniffedContentType = WTFMove(contentType);
}

bool NetworkDataTaskSoup::persistentCredentialStorageEnabled() const
{
    return static_cast<NetworkSessionSoup&>(*m_session).persistentCredentialStorageEnabled();
}

void NetworkDataTaskSoup::applyAuthenticationToRequest(ResourceRequest& request)
{
    if (m_user.isEmpty() && m_password.isEmpty())
        return;

    auto url = request.url();
    url.setUser(m_user);
    url.setPassword(m_password);
    request.setURL(url);

    m_user = String();
    m_password = String();
}

#if USE(SOUP2)
void NetworkDataTaskSoup::authenticateCallback(SoupSession* session, SoupMessage* soupMessage, SoupAuth* soupAuth, gboolean retrying, NetworkDataTaskSoup* task)
{
    ASSERT(session == static_cast<NetworkSessionSoup&>(*task->m_session).soupSession());

    // We don't return early here in case the given soupMessage is different to m_soupMessage when
    // it's proxy authentication and the request URL is HTTPS, because in that case libsoup uses a
    // tunnel internally and the SoupMessage used for the authentication is the tunneling one.
    // See https://bugs.webkit.org/show_bug.cgi?id=175378.
    if (soupMessage != task->m_soupMessage.get() && (soupMessage->status_code != SOUP_STATUS_PROXY_AUTHENTICATION_REQUIRED || !task->m_currentRequest.url().protocolIs("https"_s)))
        return;

    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }

    task->authenticate(AuthenticationChallenge(soupMessage, soupAuth, retrying));
}
#else
gboolean NetworkDataTaskSoup::authenticateCallback(SoupMessage* soupMessage, SoupAuth* soupAuth, gboolean retrying, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return FALSE;
    }
    ASSERT(task->m_soupMessage.get() == soupMessage);

    task->authenticate(AuthenticationChallenge(soupMessage, soupAuth, retrying));
    return TRUE;
}
#endif

static inline bool isAuthenticationFailureStatusCode(int httpStatusCode)
{
    return httpStatusCode == SOUP_STATUS_PROXY_AUTHENTICATION_REQUIRED || httpStatusCode == SOUP_STATUS_UNAUTHORIZED;
}

void NetworkDataTaskSoup::completeAuthentication(const AuthenticationChallenge& challenge, const Credential& credential)
{
    switch (challenge.protectionSpace().authenticationScheme()) {
    case ProtectionSpace::AuthenticationScheme::Default:
    case ProtectionSpace::AuthenticationScheme::HTTPBasic:
    case ProtectionSpace::AuthenticationScheme::HTTPDigest:
    case ProtectionSpace::AuthenticationScheme::HTMLForm:
    case ProtectionSpace::AuthenticationScheme::NTLM:
    case ProtectionSpace::AuthenticationScheme::Negotiate:
    case ProtectionSpace::AuthenticationScheme::OAuth:
        soup_auth_authenticate(challenge.soupAuth(), credential.user().utf8().data(), credential.password().utf8().data());
        break;
    case ProtectionSpace::AuthenticationScheme::ClientCertificatePINRequested: {
#if USE(SOUP2)
        ASSERT_NOT_REACHED();
#else
        CString password = credential.password().utf8();
        g_tls_password_set_value(challenge.tlsPassword(), reinterpret_cast<const unsigned char*>(password.data()), password.length());
        soup_message_tls_client_certificate_password_request_complete(m_soupMessage.get());
#endif
        break;
    }
    case ProtectionSpace::AuthenticationScheme::ClientCertificateRequested:
#if USE(SOUP2)
        ASSERT_NOT_REACHED();
#else
        soup_message_set_tls_client_certificate(m_soupMessage.get(), credential.certificate());
#endif
        break;
    case ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested:
    case ProtectionSpace::AuthenticationScheme::Unknown:
        break;
    }
}

void NetworkDataTaskSoup::cancelAuthentication(const AuthenticationChallenge& challenge)
{
    switch (challenge.protectionSpace().authenticationScheme()) {
    case ProtectionSpace::AuthenticationScheme::Default:
    case ProtectionSpace::AuthenticationScheme::HTTPBasic:
    case ProtectionSpace::AuthenticationScheme::HTTPDigest:
    case ProtectionSpace::AuthenticationScheme::HTMLForm:
    case ProtectionSpace::AuthenticationScheme::NTLM:
    case ProtectionSpace::AuthenticationScheme::Negotiate:
    case ProtectionSpace::AuthenticationScheme::OAuth:
        soup_auth_cancel(challenge.soupAuth());
        break;
    case ProtectionSpace::AuthenticationScheme::ClientCertificatePINRequested:
#if USE(SOUP2)
        ASSERT_NOT_REACHED();
#else
        soup_message_tls_client_certificate_password_request_complete(m_soupMessage.get());
#endif
        break;
    case ProtectionSpace::AuthenticationScheme::ClientCertificateRequested:
#if USE(SOUP2)
        ASSERT_NOT_REACHED();
#else
        soup_message_set_tls_client_certificate(m_soupMessage.get(), nullptr);
#endif
        break;
    case ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested:
    case ProtectionSpace::AuthenticationScheme::Unknown:
        break;
    }
}

void NetworkDataTaskSoup::authenticate(AuthenticationChallenge&& challenge)
{
    ASSERT(m_soupMessage);
    if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        if (!m_initialCredential.isEmpty() || challenge.previousFailureCount()) {
            // The stored credential wasn't accepted, stop using it. There is a race condition
            // here, since a different credential might have already been stored by another
            // NetworkDataTask, but the observable effect should be very minor, if any.
            m_session->networkStorageSession()->credentialStorage().remove(m_partition, challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            auto credential = m_session->networkStorageSession()->credentialStorage().get(m_partition, challenge.protectionSpace());
            if (!credential.isEmpty() && credential != m_initialCredential) {
                ASSERT(credential.persistence() == CredentialPersistenceNone);

                if (isAuthenticationFailureStatusCode(challenge.failureResponse().httpStatusCode())) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    m_session->networkStorageSession()->credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }

                completeAuthentication(challenge, credential);
                return;
            }
        }
    }

#if USE(SOUP2)
    soup_session_pause_message(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), challenge.soupMessage());
#endif

    // We could also do this before we even start the request, but that would be at the expense
    // of all request latency, versus a one-time latency for the small subset of requests that
    // use HTTP authentication. In the end, this doesn't matter much, because persistent credentials
    // will become session credentials after the first use.
    if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use && persistentCredentialStorageEnabled()) {
        auto protectionSpace = challenge.protectionSpace();
        m_session->networkStorageSession()->getCredentialFromPersistentStorage(protectionSpace, m_cancellable.get(),
            [this, protectedThis = Ref { *this }, authChallenge = WTFMove(challenge)] (Credential&& credential) mutable {
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
    m_client->didReceiveChallenge(AuthenticationChallenge(challenge), NegotiatedLegacyTLS::No, [this, protectedThis = Ref { *this }, challenge](AuthenticationChallengeDisposition disposition, const Credential& credential) {
        if (m_state == State::Canceling || m_state == State::Completed) {
            cancelAuthentication(challenge);
            clearRequest();
            return;
        }

        if (disposition == AuthenticationChallengeDisposition::Cancel) {
            cancelAuthentication(challenge);
            cancel();
            didFail(cancelledError(m_currentRequest));
            return;
        }

        if (disposition == AuthenticationChallengeDisposition::UseCredential && !credential.isEmpty()) {
            if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
                // Eventually we will manage per-session credentials only internally or use some newly-exposed API from libsoup,
                // because once we authenticate via libsoup, there is no way to ignore it for a particular request. Right now,
                // we place the credentials in the store even though libsoup will never fire the authenticate signal again for
                // this protection space.
                if (credential.persistence() == CredentialPersistenceForSession || credential.persistence() == CredentialPersistencePermanent)
                    m_session->networkStorageSession()->credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());

                if (credential.persistence() == CredentialPersistencePermanent && persistentCredentialStorageEnabled()) {
                    m_protectionSpaceForPersistentStorage = challenge.protectionSpace();
                    m_credentialForPersistentStorage = credential;
                }
            }

            completeAuthentication(challenge, credential);
        } else
            cancelAuthentication(challenge);

#if USE(SOUP2)
        soup_session_unpause_message(static_cast<NetworkSessionSoup&>(*m_session).soupSession(), challenge.soupMessage());
#endif
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
        task->didFail(ResourceError::genericGError(task->m_currentRequest.url(), error.get()));
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
    auto method = soup_message_get_method(message);
    if (method == SOUP_METHOD_GET || method == SOUP_METHOD_HEAD)
        return false;

    switch (soup_message_get_status(message)) {
    case SOUP_STATUS_SEE_OTHER:
        return true;
    case SOUP_STATUS_FOUND:
    case SOUP_STATUS_MOVED_PERMANENTLY:
        if (method == SOUP_METHOD_POST)
            return true;
        break;
    default:
        break;
    }

    if (crossOrigin && method == SOUP_METHOD_DELETE)
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
    if (m_currentRequest.redirectCount() > maxRedirects) {
#if USE(SOUP2)
        didFail(ResourceError::transportError(m_currentRequest.url(), SOUP_STATUS_TOO_MANY_REDIRECTS, "Too many redirects"_s));
#else
        didFail(ResourceError(String::fromLatin1(g_quark_to_string(SOUP_SESSION_ERROR)), SOUP_SESSION_ERROR_TOO_MANY_REDIRECTS, m_currentRequest.url(), String::fromUTF8("Too many redirects")));
#endif
        return;
    }

    m_currentRequest.incrementRedirectCount();
    m_networkLoadMetrics.redirectCount = m_currentRequest.redirectCount();

    ResourceRequest request = m_currentRequest;
    URL redirectedURL = URL(m_response.url(), m_response.httpHeaderField(HTTPHeaderName::Location));
    if (!redirectedURL.hasFragmentIdentifier() && request.url().hasFragmentIdentifier())
        redirectedURL.setFragmentIdentifier(request.url().fragmentIdentifier());
    request.setURL(redirectedURL);

    m_networkLoadMetrics.hasCrossOriginRedirect = m_networkLoadMetrics.hasCrossOriginRedirect || !SecurityOrigin::create(m_currentRequest.url())->canRequest(request.url());

    // Clear the user agent to ensure a new one is computed.
    auto userAgent = request.httpUserAgent();
    request.clearHTTPUserAgent();

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (m_shouldClearReferrerOnHTTPSToHTTPRedirect && !request.url().protocolIs("https"_s) && protocolIs(request.httpReferrer(), "https"_s))
        request.clearHTTPReferrer();

    bool isCrossOrigin = !protocolHostAndPortAreEqual(m_currentRequest.url(), request.url());
    if (!equalLettersIgnoringASCIICase(request.httpMethod(), "get"_s)) {
        // Change newRequest method to GET if change was made during a previous redirection or if current redirection says so.
        if (soup_message_get_method(m_soupMessage.get()) == SOUP_METHOD_GET || !request.url().protocolIsInHTTPFamily() || shouldRedirectAsGET(m_soupMessage.get(), isCrossOrigin)) {
            request.setHTTPMethod("GET"_s);
            request.setHTTPBody(nullptr);
            request.clearHTTPContentType();
        }
    }

    const auto& url = request.url();
    m_user = url.user();
    m_password = url.password();
    m_lastHTTPMethod = request.httpMethod();
    request.removeCredentials();

    if (isTopLevelNavigation())
        request.setFirstPartyForCookies(request.url());

    if (isCrossOrigin) {
        // The network layer might carry over some headers from the original request that
        // we want to strip here because the redirect is cross-origin.
        request.clearHTTPAuthorization();
        request.clearHTTPOrigin();
    } else if (url.protocolIsInHTTPFamily() && m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        if (m_user.isEmpty() && m_password.isEmpty()) {
            auto credential = m_session->networkStorageSession()->credentialStorage().get(m_partition, request.url());
            if (!credential.isEmpty())
                m_initialCredential = credential;
        }
    }

    auto wasBlockingCookies = m_isBlockingCookies ? WasBlockingCookies::Yes : WasBlockingCookies::No;

    clearRequest();

    auto response = ResourceResponse(m_response);
    m_client->willPerformHTTPRedirection(WTFMove(response), WTFMove(request), [this, protectedThis = Ref { *this }, wasBlockingCookies, userAgent = WTFMove(userAgent)](const ResourceRequest& newRequest) {
        if (newRequest.isNull() || m_state == State::Canceling)
            return;

        auto request = newRequest;
        if (request.url().protocolIsInHTTPFamily()) {
            m_networkLoadMetrics.fetchStart = { };
#if !USE(SOUP2)
            m_networkLoadMetrics.responseEnd = { };
            m_networkLoadMetrics.complete = false;
#endif
            applyAuthenticationToRequest(request);

            if (!request.hasHTTPHeaderField(HTTPHeaderName::UserAgent))
                request.setHTTPUserAgent(userAgent);
        }
        createRequest(WTFMove(request), wasBlockingCookies);
        if (m_soupMessage && m_state != State::Suspended) {
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
    if (error) {
        if (task->m_soupMessage)
            task->didFail(ResourceError::genericGError(task->m_currentRequest.url(), error.get()));
        else if (task->m_file)
            task->didFail(ResourceError(String::fromLatin1(g_quark_to_string(error->domain)), error->code, task->m_firstRequest.url(), String::fromUTF8(error->message)));
        else
            RELEASE_ASSERT_NOT_REACHED();
    } else if (bytesRead > 0)
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
        task->didFail(ResourceError::httpError(task->m_soupMessage.get(), error.get()));
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
    auto* headers = soup_multipart_input_stream_get_headers(m_multipartInputStream.get());
    auto contentType = String::fromLatin1(soup_message_headers_get_one(headers, "Content-Type"));
    m_response = ResourceResponse(m_firstRequest.url(), extractMIMETypeFromMediaType(contentType),
        soup_message_headers_get_content_length(headers), extractCharsetFromMediaType(contentType).toString());
    m_response.updateFromSoupMessageHeaders(headers);
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

static WebCore::NetworkLoadPriority toNetworkLoadPriority(SoupMessagePriority priority)
{
    switch (priority) {
    case SOUP_MESSAGE_PRIORITY_VERY_LOW:
    case SOUP_MESSAGE_PRIORITY_LOW:
        return WebCore::NetworkLoadPriority::Low;
    case SOUP_MESSAGE_PRIORITY_NORMAL:
        return WebCore::NetworkLoadPriority::Medium;
    case SOUP_MESSAGE_PRIORITY_HIGH:
    case SOUP_MESSAGE_PRIORITY_VERY_HIGH:
        return WebCore::NetworkLoadPriority::High;
    }

    return WebCore::NetworkLoadPriority::Unknown;
}

static AtomString soupHTTPVersionToString(SoupHTTPVersion version)
{
    switch (version) {
    case SOUP_HTTP_1_0:
        return "http/1.0"_s;
    case SOUP_HTTP_1_1:
        return "http/1.1"_s;
#if SOUP_CHECK_VERSION(2, 99, 3)
    case SOUP_HTTP_2_0:
        return "h2"_s;
#endif
    }

    return { };
}

#if !USE(SOUP2)
static String tlsProtocolVersionToString(GTlsProtocolVersion version)
{
    switch (version) {
    case G_TLS_PROTOCOL_VERSION_UNKNOWN:
        return "Unknown"_s;
    case G_TLS_PROTOCOL_VERSION_SSL_3_0:
        return "SSL 3.0"_s;
    case G_TLS_PROTOCOL_VERSION_TLS_1_0:
        return "TLS 1.0"_s;
    case G_TLS_PROTOCOL_VERSION_TLS_1_1:
        return "TLS 1.1"_s;
    case G_TLS_PROTOCOL_VERSION_TLS_1_2:
        return "TLS 1.2"_s;
    case G_TLS_PROTOCOL_VERSION_TLS_1_3:
        return "TLS 1.3"_s;
    case G_TLS_PROTOCOL_VERSION_DTLS_1_0:
        return "DTLS 1.0"_s;
    case G_TLS_PROTOCOL_VERSION_DTLS_1_2:
        return "DTLS 1.2"_s;
    }

    return { };
}
#endif

WebCore::AdditionalNetworkLoadMetricsForWebInspector& NetworkDataTaskSoup::additionalNetworkLoadMetricsForWebInspector()
{
    if (!m_networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector)
        m_networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector = WebCore::AdditionalNetworkLoadMetricsForWebInspector::create();
    return *m_networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector;
}

#if USE(SOUP2)
static void addHeaderSizes(const char *name, const char *value, gpointer pointer)
{
    uint64_t* size = static_cast<uint64_t*>(pointer);
    // Each header is formatted as "<name>: <value>\r\n"
    *size += strlen(name) + strlen(value) + 4;
}
#endif

void NetworkDataTaskSoup::didGetHeaders()
{
    // We are a bit more conservative with the persistent credential storage than the session store,
    // since we are waiting until we know that this authentication succeeded before actually storing.
    // This is because we want to avoid hitting the disk twice (once to add and once to remove) for
    // incorrect credentials or polluting the keychain with invalid credentials.
    auto statusCode = soup_message_get_status(m_soupMessage.get());
    if (!isAuthenticationFailureStatusCode(statusCode) && statusCode < 500 && persistentCredentialStorageEnabled()) {
        m_session->networkStorageSession()->saveCredentialToPersistentStorage(m_protectionSpaceForPersistentStorage, m_credentialForPersistentStorage);
        m_protectionSpaceForPersistentStorage = ProtectionSpace();
        m_credentialForPersistentStorage = Credential();
    }

#if !USE(SOUP2)
    auto* metrics = soup_message_get_metrics(m_soupMessage.get());
    auto responseStart = Seconds::fromMicroseconds(soup_message_metrics_get_response_start(metrics));
    m_networkLoadMetrics.responseStart = MonotonicTime::fromRawSeconds(responseStart.seconds());
#endif

    // Soup adds more headers to the request after starting signal is emitted, and got-headers
    // is the first one we receive after starting, so we use it also to get information about the
    // request headers.
    if (shouldCaptureExtraNetworkLoadMetrics()) {
        auto& additionalMetrics = additionalNetworkLoadMetricsForWebInspector();

        HTTPHeaderMap requestHeaders;
        SoupMessageHeadersIter headersIter;
        soup_message_headers_iter_init(&headersIter, soup_message_get_request_headers(m_soupMessage.get()));
        const char* headerName;
        const char* headerValue;
        while (soup_message_headers_iter_next(&headersIter, &headerName, &headerValue))
            requestHeaders.set(String::fromLatin1(headerName), String::fromLatin1(headerValue));
        additionalMetrics.requestHeaders = WTFMove(requestHeaders);

        additionalMetrics.priority = toNetworkLoadPriority(soup_message_get_priority(m_soupMessage.get()));
#if !USE(SOUP2)
        additionalMetrics.connectionIdentifier = String::number(soup_message_get_connection_id(m_soupMessage.get()));
        auto* address = soup_message_get_remote_address(m_soupMessage.get());
        if (G_IS_INET_SOCKET_ADDRESS(address)) {
            GUniquePtr<char> ipAddress(g_inet_address_to_string(g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(address))));
            additionalMetrics.remoteAddress = makeString(ipAddress.get(), ':', g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address)));
        }
        additionalMetrics.tlsProtocol = tlsProtocolVersionToString(soup_message_get_tls_protocol_version(m_soupMessage.get()));
        additionalMetrics.tlsCipher = String::fromUTF8(soup_message_get_tls_ciphersuite_name(m_soupMessage.get()));
        additionalMetrics.responseHeaderBytesReceived = soup_message_metrics_get_response_header_bytes_received(metrics);
#else
        {
            auto* requestHeaders = soup_message_get_request_headers(m_soupMessage.get());
            uint64_t requestHeadersSize = 0;
            soup_message_headers_foreach(requestHeaders, addHeaderSizes, &requestHeadersSize);
            additionalMetrics.requestHeaderBytesSent = requestHeadersSize;
        }

        {
            auto* responseHeaders = soup_message_get_response_headers(m_soupMessage.get());
            uint64_t responseHeadersSize = 0;
            soup_message_headers_foreach(responseHeaders, addHeaderSizes, &responseHeadersSize);
            additionalMetrics.responseHeaderBytesReceived = responseHeadersSize;
        }
#endif
    }

    m_networkLoadMetrics.protocol = soupHTTPVersionToString(soup_message_get_http_version(m_soupMessage.get()));
}

#if !USE(SOUP2)
void NetworkDataTaskSoup::wroteHeadersCallback(SoupMessage* soupMessage, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }
    ASSERT(task->m_soupMessage.get() == soupMessage);
    auto* metrics = soup_message_get_metrics(soupMessage);
    task->additionalNetworkLoadMetricsForWebInspector().requestHeaderBytesSent = soup_message_metrics_get_request_header_bytes_sent(metrics);
}

void NetworkDataTaskSoup::wroteBodyCallback(SoupMessage* soupMessage, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }
    ASSERT(task->m_soupMessage.get() == soupMessage);
    auto* metrics = soup_message_get_metrics(soupMessage);
    task->additionalNetworkLoadMetricsForWebInspector().requestBodyBytesSent = soup_message_metrics_get_request_body_bytes_sent(metrics);
}

void NetworkDataTaskSoup::gotBodyCallback(SoupMessage* soupMessage, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || (!task->m_client && !task->isDownload())) {
        task->clearRequest();
        return;
    }
    ASSERT(task->m_soupMessage.get() == soupMessage);
    auto* metrics = soup_message_get_metrics(soupMessage);
    task->m_networkLoadMetrics.responseBodyBytesReceived = soup_message_metrics_get_response_body_bytes_received(metrics);
    task->m_networkLoadMetrics.responseBodyDecodedSize = soup_message_metrics_get_response_body_size(metrics);
}

gboolean NetworkDataTaskSoup::requestCertificateCallback(SoupMessage* soupMessage, GTlsClientConnection* connection, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return FALSE;
    }
    ASSERT(task->m_soupMessage.get() == soupMessage);
    task->authenticate(AuthenticationChallenge(soupMessage, connection));
    return TRUE;
}

gboolean NetworkDataTaskSoup::requestCertificatePasswordCallback(SoupMessage* soupMessage, GTlsPassword* tlsPassword, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return FALSE;
    }
    ASSERT(task->m_soupMessage.get() == soupMessage);
    task->authenticate(AuthenticationChallenge(soupMessage, tlsPassword));
    return TRUE;
}
#endif

#if USE(SOUP2)
void NetworkDataTaskSoup::wroteBodyDataCallback(SoupMessage* soupMessage, SoupBuffer* buffer, NetworkDataTaskSoup* task)
#else
void NetworkDataTaskSoup::wroteBodyDataCallback(SoupMessage* soupMessage, unsigned length, NetworkDataTaskSoup* task)
#endif
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }
    ASSERT(task->m_soupMessage.get() == soupMessage);
#if USE(SOUP2)
    task->didWriteBodyData(buffer->length);
#else
    task->didWriteBodyData(length);
#endif
}

void NetworkDataTaskSoup::didWriteBodyData(uint64_t bytesSent)
{
    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    m_bodyDataTotalBytesSent += bytesSent;
#if USE(SOUP2)
    m_client->didSendData(m_bodyDataTotalBytesSent, m_soupMessage->request_body->length);
#else
    m_client->didSendData(m_bodyDataTotalBytesSent, soup_message_headers_get_content_length(soup_message_get_request_headers(m_soupMessage.get())));
#endif
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
        didFailDownload(downloadDestinationError(m_response, String::fromUTF8(error->message)));
        return;
    }

    GUniquePtr<char> intermediatePath(g_strdup_printf("%s.wkdownload", downloadDestinationPath.data()));
    m_downloadIntermediateFile = adoptGRef(g_file_new_for_path(intermediatePath.get()));
    outputStream = adoptGRef(g_file_replace(m_downloadIntermediateFile.get(), nullptr, TRUE, G_FILE_CREATE_NONE, nullptr, &error.outPtr()));
    if (!outputStream) {
        didFailDownload(downloadDestinationError(m_response, String::fromUTF8(error->message)));
        return;
    }
    m_downloadOutputStream = adoptGRef(G_OUTPUT_STREAM(outputStream.leakRef()));

    auto& downloadManager = m_session->networkProcess().downloadManager();
    auto download = makeUnique<Download>(downloadManager, m_pendingDownloadID, *this, *m_session, suggestedFilename());
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
    g_output_stream_write_all_finish(outputStream, result, &bytesWritten, &error.outPtr());
    if (error)
        task->didFailDownload(downloadDestinationError(task->m_response, String::fromUTF8(error->message)));
    else
        task->didWriteDownload(bytesWritten);
}

void NetworkDataTaskSoup::writeDownload()
{
    RefPtr<NetworkDataTaskSoup> protectedThis(this);
    g_output_stream_write_all_async(m_downloadOutputStream.get(), m_readBuffer.data(), m_readBuffer.size(), RunLoopSourcePriority::AsyncIONetwork, m_cancellable.get(),
        reinterpret_cast<GAsyncReadyCallback>(writeDownloadCallback), protectedThis.leakRef());
}

void NetworkDataTaskSoup::didWriteDownload(gsize bytesWritten)
{
    ASSERT(bytesWritten == m_readBuffer.size());
    auto* download = m_session->networkProcess().downloadManager().download(m_pendingDownloadID);
    ASSERT(download);
    download->didReceiveData(bytesWritten, 0, 0);
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
        didFailDownload(downloadDestinationError(m_response, String::fromUTF8(error->message)));
        return;
    }

    GRefPtr<GFileInfo> info = adoptGRef(g_file_info_new());
    CString uri = m_response.url().string().utf8();
    g_file_info_set_attribute_string(info.get(), "metadata::download-uri", uri.data());
    g_file_info_set_attribute_string(info.get(), "xattr::xdg.origin.url", uri.data());
    g_file_set_attributes_async(m_downloadDestinationFile.get(), info.get(), G_FILE_QUERY_INFO_NONE, RunLoopSourcePriority::AsyncIONetwork, nullptr, nullptr, nullptr);

    clearRequest();
    auto* download = m_session->networkProcess().downloadManager().download(m_pendingDownloadID);
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
        auto* download = m_session->networkProcess().downloadManager().download(m_pendingDownloadID);
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

#if USE(SOUP2)
void NetworkDataTaskSoup::networkEventCallback(SoupMessage* soupMessage, GSocketClientEvent event, GIOStream* stream, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client)
        return;

    ASSERT(task->m_soupMessage.get() == soupMessage);
    task->networkEvent(event, stream);
}

void NetworkDataTaskSoup::networkEvent(GSocketClientEvent event, GIOStream* stream)
{
    auto time = MonotonicTime::now();
    switch (event) {
    case G_SOCKET_CLIENT_RESOLVING:
        m_networkLoadMetrics.domainLookupStart = time;
        break;
    case G_SOCKET_CLIENT_RESOLVED:
        m_networkLoadMetrics.domainLookupEnd = time;
        break;
    case G_SOCKET_CLIENT_CONNECTING:
        m_networkLoadMetrics.connectStart = time;
        break;
    case G_SOCKET_CLIENT_CONNECTED:
        if (shouldCaptureExtraNetworkLoadMetrics() && G_IS_SOCKET_CONNECTION(stream)) {
            GRefPtr<GSocketAddress> address = adoptGRef(g_socket_connection_get_remote_address(G_SOCKET_CONNECTION(stream), nullptr));
            if (G_IS_INET_SOCKET_ADDRESS(address.get())) {
                GUniquePtr<char> ipAddress(g_inet_address_to_string(g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(address.get()))));
                additionalNetworkLoadMetricsForWebInspector().remoteAddress = makeString(ipAddress.get(), ':', g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address.get())));
            }
        }
        // Web Timing considers that connection time involves dns, proxy & TLS negotiation...
        // so we better pick G_SOCKET_CLIENT_COMPLETE for connectEnd.
        break;
    case G_SOCKET_CLIENT_PROXY_NEGOTIATING:
        break;
    case G_SOCKET_CLIENT_PROXY_NEGOTIATED:
        break;
    case G_SOCKET_CLIENT_TLS_HANDSHAKING:
        m_networkLoadMetrics.secureConnectionStart = time;
        RELEASE_ASSERT(G_IS_TLS_CONNECTION(stream));
        g_object_set_data(G_OBJECT(stream), "wk-soup-message", m_soupMessage.get());
        g_signal_connect(stream, "accept-certificate", G_CALLBACK(tlsConnectionAcceptCertificateCallback), this);
        break;
    case G_SOCKET_CLIENT_TLS_HANDSHAKED:
        break;
    case G_SOCKET_CLIENT_COMPLETE:
        m_networkLoadMetrics.connectEnd = time;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}
#endif

void NetworkDataTaskSoup::startingCallback(SoupMessage* soupMessage, NetworkDataTaskSoup* task)
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client)
        return;

    ASSERT(task->m_soupMessage.get() == soupMessage);
    task->didStartRequest();
}

#if SOUP_CHECK_VERSION(2, 67, 1)

bool NetworkDataTaskSoup::shouldAllowHSTSPolicySetting() const
{
    // Follow Apple's HSTS abuse mitigation 1:
    //  "Limit HSTS State to the Hostname, or the Top Level Domain + 1"
#if ENABLE(PUBLIC_SUFFIX_LIST)
    return isTopLevelNavigation()
        || m_currentRequest.url().host() == m_currentRequest.firstPartyForCookies().host()
        || isPublicSuffix(m_currentRequest.url().host());
#else
    return isTopLevelNavigation()
        || m_currentRequest.url().host() == m_currentRequest.firstPartyForCookies().host();
#endif
}

bool NetworkDataTaskSoup::shouldAllowHSTSProtocolUpgrade() const
{
    // Follow Apple's HSTS abuse mitigation 2:
    // "Ignore HSTS State for Subresource Requests to Blocked Domains"
    return isTopLevelNavigation()
        && !m_isBlockingCookies;
}

void NetworkDataTaskSoup::protocolUpgradedViaHSTS(SoupMessage* soupMessage)
{
    m_response = ResourceResponse::syntheticRedirectResponse(m_currentRequest.url(), soupURIToURL(soup_message_get_uri(soupMessage)));
    continueHTTPRedirection();
}

#if USE(SOUP2)
void NetworkDataTaskSoup::hstsEnforced(SoupHSTSEnforcer*, SoupMessage* soupMessage, NetworkDataTaskSoup* task)
#else
void NetworkDataTaskSoup::hstsEnforced(SoupMessage* soupMessage, NetworkDataTaskSoup* task)
#endif
{
    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }

    if (soupMessage == task->m_soupMessage.get())
        task->protocolUpgradedViaHSTS(soupMessage);
}

#endif

void NetworkDataTaskSoup::didStartRequest()
{
#if USE(SOUP2)
    m_networkLoadMetrics.requestStart = MonotonicTime::now();
    if (!m_networkLoadMetrics.secureConnectionStart && m_currentRequest.url().protocolIs("https"_s))
        m_networkLoadMetrics.secureConnectionStart = WebCore::reusedTLSConnectionSentinel;
#else
    auto* metrics = soup_message_get_metrics(m_soupMessage.get());
    auto domainLookupStart = Seconds::fromMicroseconds(soup_message_metrics_get_dns_start(metrics));
    auto domainLookupEnd = Seconds::fromMicroseconds(soup_message_metrics_get_dns_end(metrics));
    auto connectStart = Seconds::fromMicroseconds(soup_message_metrics_get_connect_start(metrics));
    auto connectEnd = Seconds::fromMicroseconds(soup_message_metrics_get_connect_end(metrics));
    auto secureConnectionStart = Seconds::fromMicroseconds(soup_message_metrics_get_tls_start(metrics));
    auto requestStart = Seconds::fromMicroseconds(soup_message_metrics_get_request_start(metrics));

    m_networkLoadMetrics.domainLookupStart = MonotonicTime::fromRawSeconds(domainLookupStart.seconds());
    m_networkLoadMetrics.domainLookupEnd = MonotonicTime::fromRawSeconds(domainLookupEnd.seconds());
    m_networkLoadMetrics.connectStart = MonotonicTime::fromRawSeconds(connectStart.seconds());
    m_networkLoadMetrics.connectEnd = MonotonicTime::fromRawSeconds(connectEnd.seconds());
    if (!secureConnectionStart && m_currentRequest.url().protocolIs("https"_s))
        m_networkLoadMetrics.secureConnectionStart = WebCore::reusedTLSConnectionSentinel;
    else
        m_networkLoadMetrics.secureConnectionStart = MonotonicTime::fromRawSeconds(secureConnectionStart.seconds());
    m_networkLoadMetrics.requestStart = MonotonicTime::fromRawSeconds(requestStart.seconds());
#endif
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
    m_networkLoadMetrics = NetworkLoadMetrics::emptyMetrics();
#if USE(SOUP2)
    m_networkLoadMetrics.fetchStart = MonotonicTime::now();
#else
    auto* metrics = soup_message_get_metrics(m_soupMessage.get());
    m_networkLoadMetrics.fetchStart = MonotonicTime::fromRawSeconds(Seconds::fromMicroseconds(soup_message_metrics_get_fetch_start(metrics)).seconds());
    m_currentRequest.updateSoupMessageBody(m_soupMessage.get(), m_session->blobRegistry());
#endif
    m_networkLoadMetrics.redirectStart = m_networkLoadMetrics.fetchStart;
}

void NetworkDataTaskSoup::fileQueryInfoCallback(GFile* file, GAsyncResult* result, NetworkDataTaskSoup* task)
{
    RefPtr<NetworkDataTaskSoup> protectedThis = adoptRef(task);
    ASSERT(file == task->m_file.get());

    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }

    // Ignore the error here, it will be handled by g_file_read_async below.
    if (GRefPtr<GFileInfo> info = adoptGRef(g_file_query_info_finish(file, result, nullptr))) {
        task->didGetFileInfo(info.get());

        if (g_file_info_get_file_type(info.get()) == G_FILE_TYPE_DIRECTORY) {
            g_file_enumerate_children_async(file, "*", G_FILE_QUERY_INFO_NONE, RunLoopSourcePriority::AsyncIONetwork, task->m_cancellable.get(),
                reinterpret_cast<GAsyncReadyCallback>(enumerateFileChildrenCallback), protectedThis.leakRef());
            return;
        }
    }

    g_file_read_async(file, RunLoopSourcePriority::AsyncIONetwork, task->m_cancellable.get(), reinterpret_cast<GAsyncReadyCallback>(readFileCallback), protectedThis.leakRef());
}

void NetworkDataTaskSoup::didGetFileInfo(GFileInfo* info)
{
    m_response.setURL(m_firstRequest.url());
    if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
        m_response.setMimeType("text/html"_s);
        m_response.setExpectedContentLength(-1);
    } else {
        auto contentType = String::fromLatin1(g_file_info_get_content_type(info));
        m_response.setMimeType(AtomString { extractMIMETypeFromMediaType(contentType) });
        m_response.setTextEncodingName(extractCharsetFromMediaType(contentType).toAtomString());
        if (m_response.mimeType().isEmpty())
            m_response.setMimeType(AtomString { MIMETypeRegistry::mimeTypeForPath(m_response.url().path().toString()) });
        m_response.setExpectedContentLength(g_file_info_get_size(info));
    }
}

void NetworkDataTaskSoup::readFileCallback(GFile* file, GAsyncResult* result, NetworkDataTaskSoup* task)
{
    RefPtr<NetworkDataTaskSoup> protectedThis = adoptRef(task);
    ASSERT(file == task->m_file.get());

    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }

    if (task->state() == State::Suspended) {
        ASSERT(!task->m_pendingResult);
        task->m_pendingResult = result;
        return;
    }

    GUniqueOutPtr<GError> error;
    GRefPtr<GInputStream> inputStream = adoptGRef(G_INPUT_STREAM(g_file_read_finish(file, result, &error.outPtr())));
    if (error)
        task->didFail(ResourceError(String::fromLatin1(g_quark_to_string(error->domain)), error->code, task->m_firstRequest.url(), String::fromUTF8(error->message)));
    else
        task->didReadFile(WTFMove(inputStream));
}

void NetworkDataTaskSoup::enumerateFileChildrenCallback(GFile* file, GAsyncResult* result, NetworkDataTaskSoup* task)
{
    RefPtr<NetworkDataTaskSoup> protectedThis = adoptRef(task);
    ASSERT(file == task->m_file.get());

    if (task->state() == State::Canceling || task->state() == State::Completed || !task->m_client) {
        task->clearRequest();
        return;
    }

    if (task->state() == State::Suspended) {
        ASSERT(!task->m_pendingResult);
        task->m_pendingResult = result;
        return;
    }

    GUniqueOutPtr<GError> error;
    GRefPtr<GFileEnumerator> enumerator = adoptGRef(g_file_enumerate_children_finish(file, result, &error.outPtr()));
    if (error)
        task->didFail(ResourceError(String::fromLatin1(g_quark_to_string(error->domain)), error->code, task->m_firstRequest.url(), String::fromUTF8(error->message)));
    else
        task->didReadFile(webkitDirectoryInputStreamNew(WTFMove(enumerator), task->m_firstRequest.url().string().utf8()));
}

void NetworkDataTaskSoup::didReadFile(GRefPtr<GInputStream>&& inputStream)
{
    m_inputStream = WTFMove(inputStream);
    dispatchDidReceiveResponse();
}

void NetworkDataTaskSoup::didReadDataURL(std::optional<DataURLDecoder::Result>&& result)
{
    if (g_cancellable_is_cancelled(m_cancellable.get())) {
        didFail(cancelledError(m_currentRequest));
        return;
    }

    if (!result) {
        didFail(internalError(m_currentRequest.url()));
        return;
    }

    m_response = ResourceResponse::dataURLResponse(m_currentRequest.url(), result.value());
    auto bytes = SharedBuffer::create(WTFMove(result->data))->createGBytes();
    m_inputStream = adoptGRef(g_memory_input_stream_new_from_bytes(bytes.get()));
    dispatchDidReceiveResponse();
}

} // namespace WebKit

