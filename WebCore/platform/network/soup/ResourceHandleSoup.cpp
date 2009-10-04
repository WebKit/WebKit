/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Xan Lopez <xan@gnome.org>
 * Copyright (C) 2008 Collabora Ltd.
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
#include "CookieJarSoup.h"
#include "ChromeClient.h"
#include "CString.h"
#include "DocLoader.h"
#include "FileSystem.h"
#include "Frame.h"
#include "HTTPParsers.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "Page.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "TextEncoding.h"

#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace WebCore {

class WebCoreSynchronousLoader : public ResourceHandleClient, public Noncopyable {
public:
    WebCoreSynchronousLoader(ResourceError&, ResourceResponse &, Vector<char>&);
    ~WebCoreSynchronousLoader();

    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
    virtual void didFinishLoading(ResourceHandle*);
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

void WebCoreSynchronousLoader::didFinishLoading(ResourceHandle*)
{
    g_main_loop_quit(m_mainLoop);
    m_finished = true;
}

void WebCoreSynchronousLoader::didFail(ResourceHandle* handle, const ResourceError& error)
{
    m_error = error;
    didFinishLoading(handle);
}

void WebCoreSynchronousLoader::run()
{
    if (!m_finished)
        g_main_loop_run(m_mainLoop);
}

static void cleanupGioOperation(ResourceHandle* handle, bool isDestroying);
static bool startData(ResourceHandle* handle, String urlString);
static bool startGio(ResourceHandle* handle, KURL url);

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_msg) {
        g_object_unref(m_msg);
        m_msg = 0;
    }

    if (m_idleHandler) {
        g_source_remove(m_idleHandler);
        m_idleHandler = 0;
    }
}

ResourceHandle::~ResourceHandle()
{
    if (d->m_msg)
        g_signal_handlers_disconnect_matched(d->m_msg, G_SIGNAL_MATCH_DATA,
                                             0, 0, 0, 0, this);

    cleanupGioOperation(this, true);
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
    SoupMessageHeadersIter iter;
    const char* name = 0;
    const char* value = 0;
    soup_message_headers_iter_init(&iter, msg->response_headers);
    while (soup_message_headers_iter_next(&iter, &name, &value))
        response->setHTTPHeaderField(name, value);

    String contentType = soup_message_headers_get_one(msg->response_headers, "Content-Type");
    response->setMimeType(extractMIMETypeFromMediaType(contentType));

    char* uri = soup_uri_to_string(soup_message_get_uri(msg), false);
    response->setURL(KURL(KURL(), uri));
    g_free(uri);
    response->setTextEncodingName(extractCharsetFromMediaType(contentType));
    response->setExpectedContentLength(soup_message_headers_get_content_length(msg->response_headers));
    response->setHTTPStatusCode(msg->status_code);
    response->setHTTPStatusText(msg->reason_phrase);
    response->setSuggestedFilename(filenameFromHTTPContentDisposition(response->httpHeaderField("Content-Disposition")));
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

    char* uri = soup_uri_to_string(soup_message_get_uri(msg), false);
    String location = String(uri);
    g_free(uri);
    KURL newURL = KURL(handle->request().url(), location);

    ResourceRequest request = handle->request();
    ResourceResponse response;
    request.setURL(newURL);
    request.setHTTPMethod(msg->method);
    fillResponseFromMessage(msg, &response);
    if (d->client())
        d->client()->willSendRequest(handle, request, response);
}

static void gotHeadersCallback(SoupMessage* msg, gpointer data)
{
    // For 401, we will accumulate the resource body, and only use it
    // in case authentication with the soup feature doesn't happen
    if (msg->status_code == SOUP_STATUS_UNAUTHORIZED) {
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
    if(!handle || statusWillBeHandledBySoup(msg->status_code) || handle->shouldContentSniff())
        return;

    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return;
    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

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

    client->didReceiveData(handle.get(), chunk->data, chunk->length, false);
}

// Called at the end of the message, with all the necessary about the last informations.
// Doesn't get called for redirects.
static void finishedCallback(SoupSession *session, SoupMessage* msg, gpointer data)
{
    RefPtr<ResourceHandle> handle = adoptRef(static_cast<ResourceHandle*>(data));
    // TODO: maybe we should run this code even if there's no client?
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();

    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    if (d->m_cancelled)
        return;

    if (SOUP_STATUS_IS_TRANSPORT_ERROR(msg->status_code)) {
        char* uri = soup_uri_to_string(soup_message_get_uri(msg), false);
        ResourceError error(g_quark_to_string(SOUP_HTTP_ERROR),
                            msg->status_code,
                            uri,
                            String::fromUTF8(msg->reason_phrase));
        g_free(uri);
        client->didFail(handle.get(), error);
        return;
    }

    if (msg->status_code == SOUP_STATUS_UNAUTHORIZED) {
        fillResponseFromMessage(msg, &d->m_response);
        client->didReceiveResponse(handle.get(), d->m_response);

        // WebCore might have cancelled the job in the while
        if (d->m_cancelled)
            return;

        if (msg->response_body->data)
            client->didReceiveData(handle.get(), msg->response_body->data, msg->response_body->length, true);
    }

    client->didFinishLoading(handle.get());
}

// parseDataUrl() is taken from the CURL http backend.
static gboolean parseDataUrl(gpointer callback_data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(callback_data);
    ResourceHandleClient* client = handle->client();
    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return false;

    d->m_idleHandler = 0;

    ASSERT(client);
    if (!client)
        return false;

    String url = handle->request().url().string();
    ASSERT(url.startsWith("data:", false));

    int index = url.find(',');
    if (index == -1) {
        client->cannotShowURL(handle);
        return false;
    }

    String mediaType = url.substring(5, index - 5);
    String data = url.substring(index + 1);

    bool isBase64 = mediaType.endsWith(";base64", false);
    if (isBase64)
        mediaType = mediaType.left(mediaType.length() - 7);

    if (mediaType.isEmpty())
        mediaType = "text/plain;charset=US-ASCII";

    String mimeType = extractMIMETypeFromMediaType(mediaType);
    String charset = extractCharsetFromMediaType(mediaType);

    ResourceResponse response;
    response.setMimeType(mimeType);

    if (isBase64) {
        data = decodeURLEscapeSequences(data);
        response.setTextEncodingName(charset);
        client->didReceiveResponse(handle, response);

        if (d->m_cancelled)
            return false;

        // Use the GLib Base64, since WebCore's decoder isn't
        // general-purpose and fails on Acid3 test 97 (whitespace).
        size_t outLength = 0;
        char* outData = 0;
        outData = reinterpret_cast<char*>(g_base64_decode(data.utf8().data(), &outLength));
        if (outData && outLength > 0)
            client->didReceiveData(handle, outData, outLength, 0);
        g_free(outData);
    } else {
        // We have to convert to UTF-16 early due to limitations in KURL
        data = decodeURLEscapeSequences(data, TextEncoding(charset));
        response.setTextEncodingName("UTF-16");
        client->didReceiveResponse(handle, response);

        if (d->m_cancelled)
            return false;

        if (data.length() > 0)
            client->didReceiveData(handle, reinterpret_cast<const char*>(data.characters()), data.length() * sizeof(UChar), 0);

        if (d->m_cancelled)
            return false;
    }

    client->didFinishLoading(handle);

    return false;
}

static bool startData(ResourceHandle* handle, String urlString)
{
    ASSERT(handle);

    ResourceHandleInternal* d = handle->getInternal();

    // If parseDataUrl is called synchronously the job is not yet effectively started
    // and webkit won't never know that the data has been parsed even didFinishLoading is called.
    d->m_idleHandler = g_idle_add(parseDataUrl, handle);
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

static bool startHttp(ResourceHandle* handle)
{
    ASSERT(handle);

    SoupSession* session = handle->defaultSession();
    ensureSessionIsInitialized(session);

    ResourceHandleInternal* d = handle->getInternal();

    ResourceRequest request(handle->request());
    KURL url(request.url());
    url.removeFragmentIdentifier();
    request.setURL(url);

    d->m_msg = request.toSoupMessage();
    if (!d->m_msg)
        return false;

    if(!handle->shouldContentSniff())
        soup_message_disable_feature(d->m_msg, SOUP_TYPE_CONTENT_SNIFFER);

    g_signal_connect(d->m_msg, "restarted", G_CALLBACK(restartedCallback), handle);
    g_signal_connect(d->m_msg, "got-headers", G_CALLBACK(gotHeadersCallback), handle);
    g_signal_connect(d->m_msg, "content-sniffed", G_CALLBACK(contentSniffedCallback), handle);
    g_signal_connect(d->m_msg, "got-chunk", G_CALLBACK(gotChunkCallback), handle);

    g_object_set_data(G_OBJECT(d->m_msg), "resourceHandle", reinterpret_cast<void*>(handle));

    FormData* httpBody = d->m_request.httpBody();
    if (httpBody && !httpBody->isEmpty()) {
        size_t numElements = httpBody->elements().size();

        // handle the most common case (i.e. no file upload)
        if (numElements < 2) {
            Vector<char> body;
            httpBody->flatten(body);
            soup_message_set_request(d->m_msg, d->m_request.httpContentType().utf8().data(),
                                     SOUP_MEMORY_COPY, body.data(), body.size());
        } else {
            /*
             * we have more than one element to upload, and some may
             * be (big) files, which we will want to mmap instead of
             * copying into memory; TODO: support upload of non-local
             * (think sftp://) files by using GIO?
             */
            soup_message_body_set_accumulate(d->m_msg->request_body, FALSE);
            for (size_t i = 0; i < numElements; i++) {
                const FormDataElement& element = httpBody->elements()[i];

                if (element.m_type == FormDataElement::data)
                    soup_message_body_append(d->m_msg->request_body, SOUP_MEMORY_TEMPORARY, element.m_data.data(), element.m_data.size());
                else {
                    /*
                     * mapping for uploaded files code inspired by technique used in
                     * libsoup's simple-httpd test
                     */
                    GError* error = 0;
                    gchar* fileName = filenameFromString(element.m_filename);
                    GMappedFile* fileMapping = g_mapped_file_new(fileName, false, &error);

                    g_free(fileName);

                    if (error) {
                        g_error_free(error);
                        g_signal_handlers_disconnect_matched(d->m_msg, G_SIGNAL_MATCH_DATA,
                                                             0, 0, 0, 0, handle);
                        g_object_unref(d->m_msg);
                        d->m_msg = 0;

                        return false;
                    }

                    SoupBuffer* soupBuffer = soup_buffer_new_with_owner(g_mapped_file_get_contents(fileMapping),
                                                                        g_mapped_file_get_length(fileMapping),
                                                                        fileMapping,
#if GLIB_CHECK_VERSION(2, 21, 3)
                                                                        reinterpret_cast<GDestroyNotify>(g_mapped_file_unref));
#else
                                                                        reinterpret_cast<GDestroyNotify>(g_mapped_file_free));
#endif
                    soup_message_body_append_buffer(d->m_msg->request_body, soupBuffer);
                    soup_buffer_free(soupBuffer);
                }
            }
        }
    }

    // balanced by a deref() in finishedCallback, which should always run
    handle->ref();

    // FIXME: For now, we cannot accept content encoded in anything
    // other than identity, so force servers to do it our way. When
    // libsoup gets proper Content-Encoding support we will want to
    // use it here instead.
    soup_message_headers_replace(d->m_msg->request_headers, "Accept-Encoding", "identity");

    // Balanced in ResourceHandleInternal's destructor; we need to
    // keep our own ref, because after queueing the message, the
    // session owns the initial reference.
    g_object_ref(d->m_msg);
    soup_session_queue_message(session, d->m_msg, finishedCallback, handle);

    return true;
}

bool ResourceHandle::start(Frame* frame)
{
    ASSERT(!d->m_msg);


    // The frame could be null if the ResourceHandle is not associated to any
    // Frame, e.g. if we are downloading a file.
    // If the frame is not null but the page is null this must be an attempted
    // load from an onUnload handler, so let's just block it.
    if (frame && !frame->page())
        return false;

    KURL url = request().url();
    String urlString = url.string();
    String protocol = url.protocol();

    // Used to set the authentication dialog toplevel; may be NULL
    d->m_frame = frame;

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
    if (d->m_msg)
        soup_session_cancel_message(defaultSession(), d->m_msg, SOUP_STATUS_CANCELLED);
    else if (d->m_cancellable)
        g_cancellable_cancel(d->m_cancellable);
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

void ResourceHandle::setDefersLoading(bool defers)
{
    d->m_defersLoading = defers;
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

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, StoredCredentials /*storedCredentials*/, ResourceError& error, ResourceResponse& response, Vector<char>& data, Frame* frame)
{
    WebCoreSynchronousLoader syncLoader(error, response, data);
    ResourceHandle handle(request, &syncLoader, true, false, true);

    handle.start(frame);
    syncLoader.run();
}

// GIO-based loader

static void cleanupGioOperation(ResourceHandle* handle, bool isDestroying = false)
{
    ResourceHandleInternal* d = handle->getInternal();

    if (d->m_gfile) {
        g_object_set_data(G_OBJECT(d->m_gfile), "webkit-resource", 0);
        g_object_unref(d->m_gfile);
        d->m_gfile = 0;
    }

    if (d->m_cancellable) {
        g_object_unref(d->m_cancellable);
        d->m_cancellable = 0;
    }

    if (d->m_inputStream) {
        g_object_set_data(G_OBJECT(d->m_inputStream), "webkit-resource", 0);
        g_object_unref(d->m_inputStream);
        d->m_inputStream = 0;
    }

    if (d->m_buffer) {
        g_free(d->m_buffer);
        d->m_buffer = 0;
    }

    if (!isDestroying)
        handle->deref();
}

static void closeCallback(GObject* source, GAsyncResult* res, gpointer)
{
    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    g_input_stream_close_finish(d->m_inputStream, res, 0);
    cleanupGioOperation(handle.get());
    client->didFinishLoading(handle.get());
}

static void readCallback(GObject* source, GAsyncResult* res, gpointer)
{
    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_cancelled || !client) {
        cleanupGioOperation(handle.get());
        return;
    }

    GError *error = 0;

    gssize bytesRead = g_input_stream_read_finish(d->m_inputStream, res, &error);
    if (error) {
        char* uri = g_file_get_uri(d->m_gfile);
        ResourceError resourceError(g_quark_to_string(G_IO_ERROR),
                                    error->code,
                                    uri,
                                    error ? String::fromUTF8(error->message) : String());
        g_free(uri);
        g_error_free(error);
        cleanupGioOperation(handle.get());
        client->didFail(handle.get(), resourceError);
        return;
    }

    if (!bytesRead) {
        g_input_stream_close_async(d->m_inputStream, G_PRIORITY_DEFAULT,
                                   0, closeCallback, 0);
        return;
    }

    d->m_total += bytesRead;
    client->didReceiveData(handle.get(), d->m_buffer, bytesRead, d->m_total);

    // didReceiveData may cancel the load, which may release the last reference.
    if (d->m_cancelled) {
        cleanupGioOperation(handle.get());
        return;
    }

    g_input_stream_read_async(d->m_inputStream, d->m_buffer, d->m_bufferSize,
                              G_PRIORITY_DEFAULT, d->m_cancellable,
                              readCallback, 0);
}

static void openCallback(GObject* source, GAsyncResult* res, gpointer)
{
    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_cancelled || !client) {
        cleanupGioOperation(handle.get());
        return;
    }

    GError *error = 0;
    GFileInputStream* in = g_file_read_finish(G_FILE(source), res, &error);
    if (error) {
        char* uri = g_file_get_uri(d->m_gfile);
        ResourceError resourceError(g_quark_to_string(G_IO_ERROR),
                                    error->code,
                                    uri,
                                    error ? String::fromUTF8(error->message) : String());
        g_free(uri);
        g_error_free(error);
        cleanupGioOperation(handle.get());
        client->didFail(handle.get(), resourceError);
        return;
    }

    d->m_inputStream = G_INPUT_STREAM(in);
    d->m_bufferSize = 8192;
    d->m_buffer = static_cast<char*>(g_malloc(d->m_bufferSize));
    d->m_total = 0;

    g_object_set_data(G_OBJECT(d->m_inputStream), "webkit-resource", handle.get());
    g_input_stream_read_async(d->m_inputStream, d->m_buffer, d->m_bufferSize,
                              G_PRIORITY_DEFAULT, d->m_cancellable,
                              readCallback, 0);
}

static void queryInfoCallback(GObject* source, GAsyncResult* res, gpointer)
{
    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_cancelled) {
        cleanupGioOperation(handle.get());
        return;
    }

    ResourceResponse response;

    char* uri = g_file_get_uri(d->m_gfile);
    response.setURL(KURL(KURL(), uri));
    g_free(uri);

    GError *error = 0;
    GFileInfo* info = g_file_query_info_finish(d->m_gfile, res, &error);

    if (error) {
        // FIXME: to be able to handle ftp URIs properly, we must
        // check if the error is G_IO_ERROR_NOT_MOUNTED, and if so,
        // call g_file_mount_enclosing_volume() to mount the ftp
        // server (and then keep track of the fact that we mounted it,
        // and set a timeout to unmount it later after it's been idle
        // for a while).
        char* uri = g_file_get_uri(d->m_gfile);
        ResourceError resourceError(g_quark_to_string(G_IO_ERROR),
                                    error->code,
                                    uri,
                                    error ? String::fromUTF8(error->message) : String());
        g_free(uri);
        g_error_free(error);
        cleanupGioOperation(handle.get());
        client->didFail(handle.get(), resourceError);
        return;
    }

    if (g_file_info_get_file_type(info) != G_FILE_TYPE_REGULAR) {
        // FIXME: what if the URI points to a directory? Should we
        // generate a listing? How? What do other backends do here?
        char* uri = g_file_get_uri(d->m_gfile);
        ResourceError resourceError(g_quark_to_string(G_IO_ERROR),
                                    G_IO_ERROR_FAILED,
                                    uri,
                                    String());
        g_free(uri);
        cleanupGioOperation(handle.get());
        client->didFail(handle.get(), resourceError);
        return;
    }

    response.setMimeType(g_file_info_get_content_type(info));
    response.setExpectedContentLength(g_file_info_get_size(info));

    GTimeVal tv;
    g_file_info_get_modification_time(info, &tv);
    response.setLastModifiedDate(tv.tv_sec);

    client->didReceiveResponse(handle.get(), response);

    if (d->m_cancelled) {
        cleanupGioOperation(handle.get());
        return;
    }

    g_file_read_async(d->m_gfile, G_PRIORITY_DEFAULT, d->m_cancellable,
                      openCallback, 0);
}
static bool startGio(ResourceHandle* handle, KURL url)
{
    ASSERT(handle);

    ResourceHandleInternal* d = handle->getInternal();

    if (handle->request().httpMethod() != "GET" && handle->request().httpMethod() != "POST")
        return false;

    // GIO doesn't know how to handle refs and queries, so remove them
    // TODO: use KURL.fileSystemPath after KURLGtk and FileSystemGtk are
    // using GIO internally, and providing URIs instead of file paths
    url.removeFragmentIdentifier();
    url.setQuery(String());
    url.setPort(0);

#if !PLATFORM(WIN_OS)
    // we avoid the escaping for local files, because
    // g_filename_from_uri (used internally by GFile) has problems
    // decoding strings with arbitrary percent signs
    if (url.isLocalFile())
        d->m_gfile = g_file_new_for_path(url.prettyURL().utf8().data() + sizeof("file://") - 1);
    else
#endif
        d->m_gfile = g_file_new_for_uri(url.string().utf8().data());
    g_object_set_data(G_OBJECT(d->m_gfile), "webkit-resource", handle);

    // balanced by a deref() in cleanupGioOperation, which should always run
    handle->ref();

    d->m_cancellable = g_cancellable_new();
    g_file_query_info_async(d->m_gfile,
                            G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE,
                            G_PRIORITY_DEFAULT, d->m_cancellable,
                            queryInfoCallback, 0);
    return true;
}

SoupSession* ResourceHandle::defaultSession()
{
    static SoupSession* session = createSoupSession();;

    return session;
}

}

