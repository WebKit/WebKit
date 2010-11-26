/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Xan Lopez <xan@gnome.org>
 * Copyright (C) 2008, 2010 Collabora Ltd.
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2009 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2009 Igalia S.L.
 * Copyright (C) 2009 John Kjellberg <john.kjellberg@power.alstom.com>
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
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ResourceHandle.h"

#include "Base64.h"
#include "CString.h"
#include "ChromeClient.h"
#include "CookieJarSoup.h"
#include "CachedResourceLoader.h"
#include "FileSystem.h"
#include "Frame.h"
#include "GOwnPtrSoup.h"
#include "HTTPParsers.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "Page.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "soup-request-http.h"
#include "TextEncoding.h"
#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace WebCore {

#define READ_BUFFER_SIZE 8192

class WebCoreSynchronousLoader : public ResourceHandleClient, public Noncopyable {
public:
    WebCoreSynchronousLoader(ResourceError&, ResourceResponse &, Vector<char>&);
    ~WebCoreSynchronousLoader();

    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
    virtual void didFinishLoading(ResourceHandle*, double /*finishTime*/);
    virtual void didFail(ResourceHandle*, const ResourceError&);

    void run();

private:
    ResourceError& m_error;
    ResourceResponse& m_response;
    Vector<char>& m_data;
    bool m_finished;
    GMainLoop* m_mainLoop;
};

WebCoreSynchronousLoader::WebCoreSynchronousLoader(ResourceError& error, ResourceResponse& response, Vector<char>& data)
    : m_error(error)
    , m_response(response)
    , m_data(data)
    , m_finished(false)
{
    m_mainLoop = g_main_loop_new(0, false);
}

WebCoreSynchronousLoader::~WebCoreSynchronousLoader()
{
    g_main_loop_unref(m_mainLoop);
}

void WebCoreSynchronousLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    m_response = response;
}

void WebCoreSynchronousLoader::didReceiveData(ResourceHandle*, const char* data, int length, int)
{
    m_data.append(data, length);
}

void WebCoreSynchronousLoader::didFinishLoading(ResourceHandle*, double)
{
    g_main_loop_quit(m_mainLoop);
    m_finished = true;
}

void WebCoreSynchronousLoader::didFail(ResourceHandle* handle, const ResourceError& error)
{
    m_error = error;
    didFinishLoading(handle, 0);
}

void WebCoreSynchronousLoader::run()
{
    if (!m_finished)
        g_main_loop_run(m_mainLoop);
}

static void cleanupSoupRequestOperation(ResourceHandle*, bool isDestroying);
static void sendRequestCallback(GObject*, GAsyncResult*, gpointer);
static void readCallback(GObject*, GAsyncResult*, gpointer);
static void closeCallback(GObject*, GAsyncResult*, gpointer);
static bool startGio(ResourceHandle*, KURL);

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_soupRequest)
        g_object_set_data(G_OBJECT(m_soupRequest.get()), "webkit-resource", 0);

    if (m_idleHandler) {
        g_source_remove(m_idleHandler);
        m_idleHandler = 0;
    }
}

ResourceHandle::~ResourceHandle()
{
    cleanupSoupRequestOperation(this, true);
}

void ResourceHandle::prepareForURL(const KURL &url)
{
#ifdef HAVE_LIBSOUP_2_29_90
    GOwnPtr<SoupURI> soupURI(soup_uri_new(url.prettyURL().utf8().data()));
    if (!soupURI)
        return;
    soup_session_prepare_for_uri(ResourceHandle::defaultSession(), soupURI.get());
#endif
}

// All other kinds of redirections, except for the *304* status code
// (SOUP_STATUS_NOT_MODIFIED) which needs to be fed into WebCore, will be
// handled by soup directly.
static gboolean statusWillBeHandledBySoup(guint statusCode)
{
    if (SOUP_STATUS_IS_TRANSPORT_ERROR(statusCode)
        || (SOUP_STATUS_IS_REDIRECTION(statusCode) && (statusCode != SOUP_STATUS_NOT_MODIFIED))
        || (statusCode == SOUP_STATUS_UNAUTHORIZED))
        return true;

    return false;
}

static void fillResponseFromMessage(SoupMessage* msg, ResourceResponse* response)
{
    response->updateFromSoupMessage(msg);
}

// Called each time the message is going to be sent again except the first time.
// It's used mostly to let webkit know about redirects.
static void restartedCallback(SoupMessage* msg, gpointer data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    if (!handle)
        return;
    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return;

    GOwnPtr<char> uri(soup_uri_to_string(soup_message_get_uri(msg), false));
    String location = String::fromUTF8(uri.get());
    KURL newURL = KURL(handle->firstRequest().url(), location);

    ResourceRequest request = handle->firstRequest();
    ResourceResponse response;
    request.setURL(newURL);
    request.setHTTPMethod(msg->method);
    fillResponseFromMessage(msg, &response);

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (!request.url().protocolIs("https") && protocolIs(request.httpReferrer(), "https")) {
        request.clearHTTPReferrer();
        soup_message_headers_remove(msg->request_headers, "Referer");
    }

    if (d->client())
        d->client()->willSendRequest(handle, request, response);

    if (d->m_cancelled)
        return;

#ifdef HAVE_LIBSOUP_2_29_90
    // Update the first party in case the base URL changed with the redirect
    String firstPartyString = request.firstPartyForCookies().string();
    if (!firstPartyString.isEmpty()) {
        GOwnPtr<SoupURI> firstParty(soup_uri_new(firstPartyString.utf8().data()));
        soup_message_set_first_party(d->m_soupMessage.get(), firstParty.get());
    }
#endif
}

static void gotHeadersCallback(SoupMessage* msg, gpointer data)
{
    // For 401, we will accumulate the resource body, and only use it
    // in case authentication with the soup feature doesn't happen.
    // For 302 we accumulate the body too because it could be used by
    // some servers to redirect with a clunky http-equiv=REFRESH
    if (statusWillBeHandledBySoup(msg->status_code)) {
        soup_message_body_set_accumulate(msg->response_body, TRUE);
        return;
    }

    // For all the other responses, we handle each chunk ourselves,
    // and we don't need msg->response_body to contain all of the data
    // we got, when we finish downloading.
    soup_message_body_set_accumulate(msg->response_body, FALSE);

    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(data);

    // The content-sniffed callback will handle the response if WebCore
    // require us to sniff.
    if (!handle || statusWillBeHandledBySoup(msg->status_code) || handle->shouldContentSniff())
        return;

    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return;
    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    ASSERT(d->m_response.isNull());

    fillResponseFromMessage(msg, &d->m_response);
    client->didReceiveResponse(handle.get(), d->m_response);
}

// This callback will not be called if the content sniffer is disabled in startHttp.
static void contentSniffedCallback(SoupMessage* msg, const char* sniffedType, GHashTable *params, gpointer data)
{
    if (sniffedType) {
        const char* officialType = soup_message_headers_get_one(msg->response_headers, "Content-Type");

        if (!officialType || strcmp(officialType, sniffedType))
            soup_message_headers_set_content_type(msg->response_headers, sniffedType, params);
    }

    if (statusWillBeHandledBySoup(msg->status_code))
        return;

    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(data);
    if (!handle)
        return;
    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return;
    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    ASSERT(d->m_response.isNull());

    fillResponseFromMessage(msg, &d->m_response);
    client->didReceiveResponse(handle.get(), d->m_response);
}

static void gotChunkCallback(SoupMessage* msg, SoupBuffer* chunk, gpointer data)
{
    if (statusWillBeHandledBySoup(msg->status_code))
        return;

    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(data);
    if (!handle)
        return;
    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return;
    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    ASSERT(!d->m_response.isNull());

    client->didReceiveData(handle.get(), chunk->data, chunk->length, false);
}

static gboolean parseDataUrl(gpointer callbackData)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(callbackData);
    ResourceHandleClient* client = handle->client();
    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return false;

    d->m_idleHandler = 0;

    ASSERT(client);
    if (!client)
        return false;

    String url = handle->firstRequest().url().string();
    ASSERT(url.startsWith("data:", false));

    int index = url.find(',');
    if (index == -1) {
        client->cannotShowURL(handle);
        return false;
    }

    String mediaType = url.substring(5, index - 5);

    bool isBase64 = mediaType.endsWith(";base64", false);
    if (isBase64)
        mediaType = mediaType.left(mediaType.length() - 7);

    if (mediaType.isEmpty())
        mediaType = "text/plain;charset=US-ASCII";

    String mimeType = extractMIMETypeFromMediaType(mediaType);
    String charset = extractCharsetFromMediaType(mediaType);

    ASSERT(d->m_response.isNull());

    d->m_response.setURL(handle->firstRequest().url());
    d->m_response.setMimeType(mimeType);

    // For non base64 encoded data we have to convert to UTF-16 early
    // due to limitations in KURL
    d->m_response.setTextEncodingName(isBase64 ? charset : "UTF-16");
    client->didReceiveResponse(handle, d->m_response);

    // The load may be cancelled, and the client may be destroyed
    // by any of the client reporting calls, so we check, and bail
    // out in either of those cases.
    if (d->m_cancelled || !handle->client())
        return false;

    SoupSession* session = handle->defaultSession();
    GOwnPtr<GError> error;
    d->m_soupRequest = adoptPlatformRef(webkit_soup_requester_request(d->m_requester.get(), handle->firstRequest().url().string().utf8().data(), session, &error.outPtr()));
    if (error) {
        d->m_soupRequest = 0;
        client->didFinishLoading(handle, 0);
        return false;
    }

    d->m_inputStream = adoptPlatformRef(webkit_soup_request_send(d->m_soupRequest.get(), 0, &error.outPtr()));
    if (error) {
        d->m_inputStream = 0;
        client->didFinishLoading(handle, 0);
        return false;
    }

    d->m_buffer = static_cast<char*>(g_slice_alloc0(READ_BUFFER_SIZE));
    d->m_total = 0;

    g_object_set_data(G_OBJECT(d->m_inputStream.get()), "webkit-resource", handle);
    // balanced by a deref() in cleanupSoupRequestOperation, which should always run
    handle->ref();

    d->m_cancellable = adoptPlatformRef(g_cancellable_new());
    g_input_stream_read_async(d->m_inputStream.get(), d->m_buffer, READ_BUFFER_SIZE, G_PRIORITY_DEFAULT,
                              d->m_cancellable.get(), readCallback, GINT_TO_POINTER(!isBase64));

    return false;
}

static bool startData(ResourceHandle* handle, String urlString)
{
    ASSERT(handle);

    ResourceHandleInternal* d = handle->getInternal();

    // If parseDataUrl is called synchronously the job is not yet effectively started
    // and webkit won't never know that the data has been parsed even didFinishLoading is called.
    d->m_idleHandler = g_timeout_add(0, parseDataUrl, handle);
    return true;
}

static SoupSession* createSoupSession()
{
    return soup_session_async_new();
}

// Values taken from http://stevesouders.com/ua/index.php following
// the rule "Do What Every Other Modern Browser Is Doing". They seem
// to significantly improve page loading time compared to soup's
// default values.
#define MAX_CONNECTIONS          60
#define MAX_CONNECTIONS_PER_HOST 6

static void ensureSessionIsInitialized(SoupSession* session)
{
    if (g_object_get_data(G_OBJECT(session), "webkit-init"))
        return;

    SoupCookieJar* jar = reinterpret_cast<SoupCookieJar*>(soup_session_get_feature(session, SOUP_TYPE_COOKIE_JAR));
    if (!jar)
        soup_session_add_feature(session, SOUP_SESSION_FEATURE(defaultCookieJar()));
    else
        setDefaultCookieJar(jar);

    if (!soup_session_get_feature(session, SOUP_TYPE_LOGGER) && LogNetwork.state == WTFLogChannelOn) {
        SoupLogger* logger = soup_logger_new(static_cast<SoupLoggerLogLevel>(SOUP_LOGGER_LOG_BODY), -1);
        soup_logger_attach(logger, session);
        g_object_unref(logger);
    }

    g_object_set(session,
                 SOUP_SESSION_MAX_CONNS, MAX_CONNECTIONS,
                 SOUP_SESSION_MAX_CONNS_PER_HOST, MAX_CONNECTIONS_PER_HOST,
                 NULL);

    g_object_set_data(G_OBJECT(session), "webkit-init", reinterpret_cast<void*>(0xdeadbeef));
}

static void cleanupSoupRequestOperation(ResourceHandle* handle, bool isDestroying = false)
{
    ResourceHandleInternal* d = handle->getInternal();

    if (d->m_soupRequest) {
        g_object_set_data(G_OBJECT(d->m_soupRequest.get()), "webkit-resource", 0);
        d->m_soupRequest.clear();
    }

    if (d->m_inputStream) {
        g_object_set_data(G_OBJECT(d->m_inputStream.get()), "webkit-resource", 0);
        d->m_inputStream.clear();
    }

    d->m_cancellable.clear();

    if (d->m_soupMessage) {
        g_signal_handlers_disconnect_matched(d->m_soupMessage.get(), G_SIGNAL_MATCH_DATA,
                                             0, 0, 0, 0, handle);
        d->m_soupMessage.clear();
    }

    if (d->m_buffer) {
        g_slice_free1(READ_BUFFER_SIZE, d->m_buffer);
        d->m_buffer = 0;
    }

    if (!isDestroying)
        handle->deref();
}

static void sendRequestCallback(GObject* source, GAsyncResult* res, gpointer userData)
{
    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_gotChunkHandler) {
        // No need to call gotChunkHandler anymore. Received data will
        // be reported by readCallback
        if (g_signal_handler_is_connected(d->m_soupMessage.get(), d->m_gotChunkHandler))
            g_signal_handler_disconnect(d->m_soupMessage.get(), d->m_gotChunkHandler);
    }

    if (d->m_cancelled || !client) {
        cleanupSoupRequestOperation(handle.get());
        return;
    }

    GOwnPtr<GError> error;
    GInputStream* in = webkit_soup_request_send_finish(d->m_soupRequest.get(), res, &error.outPtr());

    if (error) {
        SoupMessage* soupMsg = d->m_soupMessage.get();
        gboolean isTransportError = d->m_soupMessage && SOUP_STATUS_IS_TRANSPORT_ERROR(soupMsg->status_code);

        if (isTransportError || (error->domain == G_IO_ERROR)) {
            SoupURI* uri = webkit_soup_request_get_uri(d->m_soupRequest.get());
            GOwnPtr<char> uriStr(soup_uri_to_string(uri, false));
            gint errorCode = isTransportError ? soupMsg->status_code : error->code;
            const gchar* errorMsg = isTransportError ? soupMsg->reason_phrase : error->message;
            const gchar* quarkStr = isTransportError ? g_quark_to_string(SOUP_HTTP_ERROR) : g_quark_to_string(G_IO_ERROR);
            ResourceError resourceError(quarkStr, errorCode, uriStr.get(), String::fromUTF8(errorMsg));

            cleanupSoupRequestOperation(handle.get());
            client->didFail(handle.get(), resourceError);
            return;
        }

        if (d->m_soupMessage && statusWillBeHandledBySoup(d->m_soupMessage->status_code)) {
            ASSERT(d->m_response.isNull());

            fillResponseFromMessage(soupMsg, &d->m_response);
            client->didReceiveResponse(handle.get(), d->m_response);

            // WebCore might have cancelled the job in the while
            if (!d->m_cancelled && soupMsg->response_body->data)
                client->didReceiveData(handle.get(), soupMsg->response_body->data, soupMsg->response_body->length, true);
        }

        // didReceiveData above might have cancelled it
        if (d->m_cancelled || !client) {
            cleanupSoupRequestOperation(handle.get());
            return;
        }

        client->didFinishLoading(handle.get(), 0);
        return;
    }

    if (d->m_cancelled) {
        cleanupSoupRequestOperation(handle.get());
        return;
    }

    d->m_inputStream = adoptPlatformRef(in);
    d->m_buffer = static_cast<char*>(g_slice_alloc0(READ_BUFFER_SIZE));
    d->m_total = 0;

    // readCallback needs it
    g_object_set_data(G_OBJECT(d->m_inputStream.get()), "webkit-resource", handle.get());

    // Ensure a response is sent for any protocols that don't explicitly support responses
    // through got-headers signal or content sniffing.
    // (e.g. file and GIO based protocol).
    if (!handle->shouldContentSniff() && d->m_response.isNull()) {
        d->m_response.setURL(handle->firstRequest().url());
        d->m_response.setMimeType(webkit_soup_request_get_content_type(d->m_soupRequest.get()));
        d->m_response.setExpectedContentLength(webkit_soup_request_get_content_length(d->m_soupRequest.get()));
        client->didReceiveResponse(handle.get(), d->m_response);

        if (d->m_cancelled) {
            cleanupSoupRequestOperation(handle.get());
            return;
        }
    }

    g_input_stream_read_async(d->m_inputStream.get(), d->m_buffer, READ_BUFFER_SIZE,
                              G_PRIORITY_DEFAULT, d->m_cancellable.get(), readCallback, 0);
}

static bool startHttp(ResourceHandle* handle)
{
    ASSERT(handle);

    SoupSession* session = handle->defaultSession();
    ensureSessionIsInitialized(session);

    ResourceHandleInternal* d = handle->getInternal();

    ResourceRequest request(handle->firstRequest());
    KURL url(request.url());
    url.removeFragmentIdentifier();
    request.setURL(url);

    GOwnPtr<GError> error;
    d->m_soupRequest = adoptPlatformRef(webkit_soup_requester_request(d->m_requester.get(), url.string().utf8().data(), session, &error.outPtr()));
    if (error) {
        d->m_soupRequest = 0;
        return false;
    }

    g_object_set_data(G_OBJECT(d->m_soupRequest.get()), "webkit-resource", handle);

    d->m_soupMessage = adoptPlatformRef(webkit_soup_request_http_get_message(WEBKIT_SOUP_REQUEST_HTTP(d->m_soupRequest.get())));
    if (!d->m_soupMessage)
        return false;

    SoupMessage* soupMessage = d->m_soupMessage.get();
    request.updateSoupMessage(soupMessage);

    if (!handle->shouldContentSniff())
        soup_message_disable_feature(soupMessage, SOUP_TYPE_CONTENT_SNIFFER);
    else
        g_signal_connect(soupMessage, "content-sniffed", G_CALLBACK(contentSniffedCallback), handle);

    g_signal_connect(soupMessage, "restarted", G_CALLBACK(restartedCallback), handle);
    g_signal_connect(soupMessage, "got-headers", G_CALLBACK(gotHeadersCallback), handle);
    d->m_gotChunkHandler = g_signal_connect(soupMessage, "got-chunk", G_CALLBACK(gotChunkCallback), handle);

#ifdef HAVE_LIBSOUP_2_29_90
    String firstPartyString = request.firstPartyForCookies().string();
    if (!firstPartyString.isEmpty()) {
        GOwnPtr<SoupURI> firstParty(soup_uri_new(firstPartyString.utf8().data()));
        soup_message_set_first_party(soupMessage, firstParty.get());
    }
#endif

    FormData* httpBody = d->m_firstRequest.httpBody();
    if (httpBody && !httpBody->isEmpty()) {
        size_t numElements = httpBody->elements().size();

        // handle the most common case (i.e. no file upload)
        if (numElements < 2) {
            Vector<char> body;
            httpBody->flatten(body);
            soup_message_set_request(soupMessage, d->m_firstRequest.httpContentType().utf8().data(),
                                     SOUP_MEMORY_COPY, body.data(), body.size());
        } else {
            /*
             * we have more than one element to upload, and some may
             * be (big) files, which we will want to mmap instead of
             * copying into memory; TODO: support upload of non-local
             * (think sftp://) files by using GIO?
             */
            soup_message_body_set_accumulate(soupMessage->request_body, FALSE);
            for (size_t i = 0; i < numElements; i++) {
                const FormDataElement& element = httpBody->elements()[i];

                if (element.m_type == FormDataElement::data)
                    soup_message_body_append(soupMessage->request_body, SOUP_MEMORY_TEMPORARY, element.m_data.data(), element.m_data.size());
                else {
                    /*
                     * mapping for uploaded files code inspired by technique used in
                     * libsoup's simple-httpd test
                     */
                    GOwnPtr<GError> error;
                    CString fileName = fileSystemRepresentation(element.m_filename);
                    GMappedFile* fileMapping = g_mapped_file_new(fileName.data(), false, &error.outPtr());

                    if (error) {
                        g_signal_handlers_disconnect_matched(soupMessage, G_SIGNAL_MATCH_DATA,
                                                             0, 0, 0, 0, handle);
                        d->m_soupMessage.clear();

                        return false;
                    }

                    SoupBuffer* soupBuffer = soup_buffer_new_with_owner(g_mapped_file_get_contents(fileMapping),
                                                                        g_mapped_file_get_length(fileMapping),
                                                                        fileMapping,
                                                                        reinterpret_cast<GDestroyNotify>(g_mapped_file_unref));
                    soup_message_body_append_buffer(soupMessage->request_body, soupBuffer);
                    soup_buffer_free(soupBuffer);
                }
            }
        }
    }

    // balanced by a deref() in cleanupSoupRequestOperation, which should always run
    handle->ref();

    // Make sure we have an Accept header for subresources; some sites
    // want this to serve some of their subresources
    if (!soup_message_headers_get_one(soupMessage->request_headers, "Accept"))
        soup_message_headers_append(soupMessage->request_headers, "Accept", "*/*");

    d->m_cancellable = adoptPlatformRef(g_cancellable_new());
    webkit_soup_request_send_async(d->m_soupRequest.get(), d->m_cancellable.get(), sendRequestCallback, 0);

    return true;
}

bool ResourceHandle::start(NetworkingContext* context)
{
    ASSERT(!d->m_soupMessage);

    // The frame could be null if the ResourceHandle is not associated to any
    // Frame, e.g. if we are downloading a file.
    // If the frame is not null but the page is null this must be an attempted
    // load from an unload handler, so let's just block it.
    // If both the frame and the page are not null the context is valid.
    if (context && !context->isValid())
        return false;

    if (!(d->m_user.isEmpty() || d->m_pass.isEmpty())) {
        // If credentials were specified for this request, add them to the url,
        // so that they will be passed to NetworkRequest.
        KURL urlWithCredentials(firstRequest().url());
        urlWithCredentials.setUser(d->m_user);
        urlWithCredentials.setPass(d->m_pass);
        d->m_firstRequest.setURL(urlWithCredentials);
    }

    KURL url = firstRequest().url();
    String urlString = url.string();
    String protocol = url.protocol();

    // Used to set the authentication dialog toplevel; may be NULL
    d->m_context = context;

    if (equalIgnoringCase(protocol, "data"))
        return startData(this, urlString);

    if (equalIgnoringCase(protocol, "http") || equalIgnoringCase(protocol, "https")) {
        if (startHttp(this))
            return true;
    }

    if (equalIgnoringCase(protocol, "file") || equalIgnoringCase(protocol, "ftp") || equalIgnoringCase(protocol, "ftps")) {
        // FIXME: should we be doing any other protocols here?
        if (startGio(this, url))
            return true;
    }

    // Error must not be reported immediately
    this->scheduleFailure(InvalidURLFailure);

    return true;
}

void ResourceHandle::cancel()
{
    d->m_cancelled = true;
    if (d->m_soupMessage)
        soup_session_cancel_message(defaultSession(), d->m_soupMessage.get(), SOUP_STATUS_CANCELLED);
    else if (d->m_cancellable)
        g_cancellable_cancel(d->m_cancellable.get());
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool ResourceHandle::supportsBufferedData()
{
    return false;
}

void ResourceHandle::platformSetDefersLoading(bool)
{
    notImplemented();
}

bool ResourceHandle::loadsBlocked()
{
    return false;
}

bool ResourceHandle::willLoadFromCache(ResourceRequest&, Frame*)
{
    // Not having this function means that we'll ask the user about re-posting a form
    // even when we go back to a page that's still in the cache.
    notImplemented();
    return false;
}

void ResourceHandle::loadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentials /*storedCredentials*/, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    WebCoreSynchronousLoader syncLoader(error, response, data);
    // FIXME: we should use the ResourceHandle::create method here,
    // but it makes us timeout in a couple of tests. See
    // https://bugs.webkit.org/show_bug.cgi?id=41823
    RefPtr<ResourceHandle> handle = adoptRef(new ResourceHandle(request, &syncLoader, true, false));
    handle->start(context);

    syncLoader.run();
}

static void closeCallback(GObject* source, GAsyncResult* res, gpointer)
{
    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    g_input_stream_close_finish(d->m_inputStream.get(), res, 0);
    cleanupSoupRequestOperation(handle.get());

    // The load may have been cancelled, the client may have been
    // destroyed already. In such cases calling didFinishLoading is a
    // bad idea.
    if (d->m_cancelled || !client)
        return;

    client->didFinishLoading(handle.get(), 0);
}

static void readCallback(GObject* source, GAsyncResult* asyncResult, gpointer data)
{
    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    bool convertToUTF16 = static_cast<bool>(data);
    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_cancelled || !client) {
        cleanupSoupRequestOperation(handle.get());
        return;
    }

    GOwnPtr<GError> error;

    gssize bytesRead = g_input_stream_read_finish(d->m_inputStream.get(), asyncResult, &error.outPtr());
    if (error) {
        SoupURI* uri = webkit_soup_request_get_uri(d->m_soupRequest.get());
        GOwnPtr<char> uriStr(soup_uri_to_string(uri, false));
        ResourceError resourceError(g_quark_to_string(G_IO_ERROR), error->code, uriStr.get(),
                                    error ? String::fromUTF8(error->message) : String());
        cleanupSoupRequestOperation(handle.get());
        client->didFail(handle.get(), resourceError);
        return;
    }

    if (!bytesRead) {
        g_input_stream_close_async(d->m_inputStream.get(), G_PRIORITY_DEFAULT,
                                   0, closeCallback, 0);
        return;
    }

    // It's mandatory to have sent a response before sending data
    ASSERT(!d->m_response.isNull());

    d->m_total += bytesRead;
    if (G_LIKELY(!convertToUTF16))
        client->didReceiveData(handle.get(), d->m_buffer, bytesRead, d->m_total);
    else {
        // We have to convert it to UTF-16 due to limitations in KURL
        String data = String::fromUTF8(d->m_buffer, bytesRead);
        client->didReceiveData(handle.get(), reinterpret_cast<const char*>(data.characters()), data.length() * sizeof(UChar), 0);
    }

    // didReceiveData may cancel the load, which may release the last reference.
    if (d->m_cancelled || !client) {
        cleanupSoupRequestOperation(handle.get());
        return;
    }

    g_input_stream_read_async(d->m_inputStream.get(), d->m_buffer, READ_BUFFER_SIZE, G_PRIORITY_DEFAULT,
                              d->m_cancellable.get(), readCallback, data);
}

static bool startGio(ResourceHandle* handle, KURL url)
{
    ASSERT(handle);

    if (handle->firstRequest().httpMethod() != "GET" && handle->firstRequest().httpMethod() != "POST")
        return false;

    SoupSession* session = handle->defaultSession();
    ResourceHandleInternal* d = handle->getInternal();

    // GIO doesn't know how to handle refs and queries, so remove them
    // TODO: use KURL.fileSystemPath after KURLGtk and FileSystemGtk are
    // using GIO internally, and providing URIs instead of file paths
    url.removeFragmentIdentifier();
    url.setQuery(String());
    url.removePort();
    CString urlStr = url.string().utf8();

    GOwnPtr<GError> error;
    d->m_soupRequest = adoptPlatformRef(webkit_soup_requester_request(d->m_requester.get(), urlStr.data(), session, &error.outPtr()));
    if (error) {
        d->m_soupRequest = 0;
        return false;
    }

    g_object_set_data(G_OBJECT(d->m_soupRequest.get()), "webkit-resource", handle);

    // balanced by a deref() in cleanupSoupRequestOperation, which should always run
    handle->ref();

    d->m_cancellable = adoptPlatformRef(g_cancellable_new());
    webkit_soup_request_send_async(d->m_soupRequest.get(), d->m_cancellable.get(), sendRequestCallback, 0);

    return true;
}

SoupSession* ResourceHandle::defaultSession()
{
    static SoupSession* session = createSoupSession();

    return session;
}

}
