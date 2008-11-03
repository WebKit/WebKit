/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Xan Lopez <xan@gnome.org>
 * Copyright (C) 2008 Collabora Ltd.
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
#include "CString.h"
#include "ResourceHandle.h"

#include "Base64.h"
#include "CookieJar.h"
#include "DocLoader.h"
#include "Frame.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "TextEncoding.h"

#include <gio/gio.h>
#include <libsoup/soup.h>
#include <libsoup/soup-message.h>

#if PLATFORM(GTK)
    #if GLIB_CHECK_VERSION(2,12,0)
        #define USE_GLIB_BASE64
    #endif
#endif

namespace WebCore {

static SoupSession* session = 0;

enum
{
    ERROR_TRANSPORT,
    ERROR_UNKNOWN_PROTOCOL,
    ERROR_BAD_NON_HTTP_METHOD
};

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_msg) {
        g_object_unref(m_msg);
        m_msg = 0;
    }
}

ResourceHandle::~ResourceHandle()
{
}

static void fillResponseFromMessage(SoupMessage* msg, ResourceResponse* response)
{
    SoupMessageHeadersIter iter;
    const char* name = NULL;
    const char* value = NULL;
    soup_message_headers_iter_init(&iter, msg->response_headers);
    while (soup_message_headers_iter_next(&iter, &name, &value))
        response->setHTTPHeaderField(name, value);

    String contentType = soup_message_headers_get(msg->response_headers, "Content-Type");
    char* uri = soup_uri_to_string(soup_message_get_uri(msg), FALSE);
    response->setUrl(KURL(uri));
    g_free(uri);
    response->setMimeType(extractMIMETypeFromMediaType(contentType));
    response->setTextEncodingName(extractCharsetFromMediaType(contentType));
    response->setExpectedContentLength(soup_message_headers_get_content_length(msg->response_headers));
    response->setHTTPStatusCode(msg->status_code);
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

    char* uri = soup_uri_to_string(soup_message_get_uri(msg), FALSE);
    String location = String(uri);
    g_free(uri);
    KURL newURL = KURL(handle->request().url(), location);

    ResourceRequest request = handle->request();
    ResourceResponse response;
    request.setURL(newURL);
    fillResponseFromMessage(msg, &response);
    if (d->client())
        d->client()->willSendRequest(handle, request, response);

    d->m_request.setURL(newURL);
}

static void gotHeadersCallback(SoupMessage* msg, gpointer data)
{
    if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
        return;

    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    if (!handle)
        return;
    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return;
    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    fillResponseFromMessage(msg, &d->m_response);
    client->didReceiveResponse(handle, d->m_response);
    soup_message_set_flags(msg, SOUP_MESSAGE_OVERWRITE_CHUNKS);
}

static void gotChunkCallback(SoupMessage* msg, SoupBuffer* chunk, gpointer data)
{
    if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
        return;

    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    if (!handle)
        return;
    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return;
    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    client->didReceiveData(handle, chunk->data, chunk->length, false);
}

// Called at the end of the message, with all the necessary about the last informations.
// Doesn't get called for redirects.
static void finishedCallback(SoupSession *session, SoupMessage* msg, gpointer data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    // TODO: maybe we should run this code even if there's no client?
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    // The message has been handled.
    d->m_msg = NULL;

    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    if (d->m_cancelled)
        return;

    if (SOUP_STATUS_IS_TRANSPORT_ERROR(msg->status_code)) {
        char* uri = soup_uri_to_string(soup_message_get_uri(msg), FALSE);
        ResourceError error("webkit-network-error", ERROR_TRANSPORT, uri, String::fromUTF8(msg->reason_phrase));
        g_free(uri);
        client->didFail(handle, error);
        return;
    } else if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
        fillResponseFromMessage(msg, &d->m_response);
        client->didReceiveResponse(handle, d->m_response);

        // WebCore might have cancelled the job in the while
        if (d->m_cancelled)
            return;

        if (msg->response_body->data)
            client->didReceiveData(handle, msg->response_body->data, msg->response_body->length, true);
    }

    client->didFinishLoading(handle);
}

// parseDataUrl() is taken from the CURL http backend.
static gboolean parseDataUrl(gpointer callback_data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(callback_data);
    ResourceHandleClient* client = handle->client();

    ASSERT(client);
    if (!client)
        return FALSE;

    String url = handle->request().url().string();
    ASSERT(url.startsWith("data:", false));

    int index = url.find(',');
    if (index == -1) {
        client->cannotShowURL(handle);
        return FALSE;
    }

    String mediaType = url.substring(5, index - 5);
    String data = url.substring(index + 1);

    bool base64 = mediaType.endsWith(";base64", false);
    if (base64)
        mediaType = mediaType.left(mediaType.length() - 7);

    if (mediaType.isEmpty())
        mediaType = "text/plain;charset=US-ASCII";

    String mimeType = extractMIMETypeFromMediaType(mediaType);
    String charset = extractCharsetFromMediaType(mediaType);

    ResourceResponse response;
    response.setMimeType(mimeType);

    if (base64) {
        data = decodeURLEscapeSequences(data);
        response.setTextEncodingName(charset);
        client->didReceiveResponse(handle, response);

        // Use the GLib Base64 if available, since WebCore's decoder isn't
        // general-purpose and fails on Acid3 test 97 (whitespace).
#ifdef USE_GLIB_BASE64
        size_t outLength = 0;
        char* outData = 0;
        outData = reinterpret_cast<char*>(g_base64_decode(data.utf8().data(), &outLength));
        if (outData && outLength > 0)
            client->didReceiveData(handle, outData, outLength, 0);
        g_free(outData);
#else
        Vector<char> out;
        if (base64Decode(data.latin1().data(), data.latin1().length(), out) && out.size() > 0)
            client->didReceiveData(handle, out.data(), out.size(), 0);
#endif
    } else {
        // We have to convert to UTF-16 early due to limitations in KURL
        data = decodeURLEscapeSequences(data, TextEncoding(charset));
        response.setTextEncodingName("UTF-16");
        client->didReceiveResponse(handle, response);
        if (data.length() > 0)
            client->didReceiveData(handle, reinterpret_cast<const char*>(data.characters()), data.length() * sizeof(UChar), 0);
    }

    client->didFinishLoading(handle);

    return FALSE;
}

bool ResourceHandle::startData(String urlString)
{
        // If parseDataUrl is called synchronously the job is not yet effectively started
        // and webkit won't never know that the data has been parsed even didFinishLoading is called.
        g_idle_add(parseDataUrl, this);
        return true;
}

bool ResourceHandle::startHttp(String urlString)
{
    if (!session) {
        session = soup_session_async_new();

        soup_session_add_feature(session, SOUP_SESSION_FEATURE(getCookieJar()));

        const char* soup_debug = g_getenv("WEBKIT_SOUP_LOGGING");
        if (soup_debug) {
            int soup_debug_level = atoi(soup_debug);

            SoupLogger* logger = soup_logger_new(static_cast<SoupLoggerLogLevel>(soup_debug_level), -1);
            soup_logger_attach(logger, session);
            g_object_unref(logger);
        }
    }

    SoupMessage* msg;
    msg = soup_message_new(request().httpMethod().utf8().data(), urlString.utf8().data());
    g_signal_connect(msg, "restarted", G_CALLBACK(restartedCallback), this);

    g_signal_connect(msg, "got-headers", G_CALLBACK(gotHeadersCallback), this);
    g_signal_connect(msg, "got-chunk", G_CALLBACK(gotChunkCallback), this);

    HTTPHeaderMap customHeaders = d->m_request.httpHeaderFields();
    if (!customHeaders.isEmpty()) {
        HTTPHeaderMap::const_iterator end = customHeaders.end();
        for (HTTPHeaderMap::const_iterator it = customHeaders.begin(); it != end; ++it)
            soup_message_headers_append(msg->request_headers, it->first.utf8().data(), it->second.utf8().data());
    }

    FormData* httpBody = d->m_request.httpBody();
    if (httpBody && !httpBody->isEmpty()) {
        // Making a copy of the request body isn't the most efficient way to
        // serialize it, but by far the most simple. Dealing with individual
        // FormData elements and shared buffers should be more memory
        // efficient.
        //
        // This possibly isn't handling file uploads/attachments, for which
        // shared buffers or streaming should definitely be used.
        Vector<char> body;
        httpBody->flatten(body);
        soup_message_set_request(msg, d->m_request.httpContentType().utf8().data(),
                                 SOUP_MEMORY_COPY, body.data(), body.size());
    }

    d->m_msg = static_cast<SoupMessage*>(g_object_ref(msg));
    soup_session_queue_message(session, d->m_msg, finishedCallback, this);

    return true;
}

bool ResourceHandle::start(Frame* frame)
{
    ASSERT(!d->m_msg);

    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if (!frame->page())
        return false;

    KURL url = request().url();
    String urlString = url.string();
    String protocol = url.protocol();

    if (equalIgnoringCase(protocol, "data"))
        return startData(urlString);
    else if (equalIgnoringCase(protocol, "http") || equalIgnoringCase(protocol, "https"))
        return startHttp(urlString);
    else if (equalIgnoringCase(protocol, "file") || equalIgnoringCase(protocol, "ftp") || equalIgnoringCase(protocol, "ftps"))
        // FIXME: should we be doing any other protocols here?
        return startGio(urlString);
    else {
        // If we don't call didFail the job is not complete for webkit even false is returned.
        if (d->client()) {
            ResourceError error("webkit-network-error", ERROR_UNKNOWN_PROTOCOL, urlString, protocol);
            d->client()->didFail(this, error);
        }
        return false;
    }
}

void ResourceHandle::cancel()
{
    d->m_cancelled = true;
    if (d->m_msg) {
        soup_session_cancel_message(session, d->m_msg, SOUP_STATUS_CANCELLED);
        // For re-entrancy troubles we call didFinishLoading when the message hasn't been handled yet.
        d->client()->didFinishLoading(this);
    } else if (d->m_cancellable) {
        g_cancellable_cancel(d->m_cancellable);
        d->client()->didFinishLoading(this);
    }
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

bool ResourceHandle::willLoadFromCache(ResourceRequest&)
{
    // Not having this function means that we'll ask the user about re-posting a form
    // even when we go back to a page that's still in the cache.
    notImplemented();
    return false;
}

void ResourceHandle::loadResourceSynchronously(const ResourceRequest&, ResourceError&, ResourceResponse&, Vector<char>&, Frame*)
{
    notImplemented();
}

// GIO-based loader

static inline ResourceError networkErrorForFile(GFile* file, GError* error)
{
    // FIXME: Map gio errors to a more detailed error code when we have it in WebKit.
    gchar* uri = g_file_get_uri(file);
    ResourceError resourceError("webkit-network-error", ERROR_TRANSPORT, uri, String::fromUTF8(error->message));
    g_free(uri);
    return resourceError;
}

static void cleanupGioOperation(ResourceHandle* handle)
{
    ResourceHandleInternal* d = handle->getInternal();

    if (d->m_gfile) {
        g_object_unref(d->m_gfile);
        d->m_gfile = NULL;
    }
    if (d->m_cancellable) {
        g_object_unref(d->m_cancellable);
        d->m_cancellable = NULL;
    }
    if (d->m_input_stream) {
        g_object_unref(d->m_input_stream);
        d->m_input_stream = NULL;
    }
    if (d->m_buffer) {
        g_free(d->m_buffer);
        d->m_buffer = NULL;
    }
}

static void closeCallback(GObject* source, GAsyncResult* res, gpointer data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    g_input_stream_close_finish(d->m_input_stream, res, NULL);
    cleanupGioOperation(handle);
    client->didFinishLoading(handle);
}

static void readCallback(GObject* source, GAsyncResult* res, gpointer data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_cancelled || !client) {
        cleanupGioOperation(handle);
        return;
    }

    gssize nread;
    GError *error = 0;

    nread = g_input_stream_read_finish(d->m_input_stream, res, &error);
    if (error) {
        client->didFail(handle, networkErrorForFile(d->m_gfile, error));
        cleanupGioOperation(handle);
        return;
    } else if (!nread) {
        g_input_stream_close_async(d->m_input_stream, G_PRIORITY_DEFAULT,
                                   NULL, closeCallback, handle);
        return;
    }

    d->m_total += nread;
    client->didReceiveData(handle, d->m_buffer, nread, d->m_total);

    g_input_stream_read_async(d->m_input_stream, d->m_buffer, d->m_bufsize,
                              G_PRIORITY_DEFAULT, d->m_cancellable,
                              readCallback, handle);
}

static void openCallback(GObject* source, GAsyncResult* res, gpointer data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_cancelled || !client) {
        cleanupGioOperation(handle);
        return;
    }

    GFileInputStream* in;
    GError *error = NULL;
    in = g_file_read_finish(G_FILE(source), res, &error);
    if (error) {
        client->didFail(handle, networkErrorForFile(d->m_gfile, error));
        cleanupGioOperation(handle);
        return;
    }

    d->m_input_stream = G_INPUT_STREAM(in);
    d->m_bufsize = 8192;
    d->m_buffer = static_cast<char*>(g_malloc(d->m_bufsize));
    d->m_total = 0;
    g_input_stream_read_async(d->m_input_stream, d->m_buffer, d->m_bufsize,
                              G_PRIORITY_DEFAULT, d->m_cancellable,
                              readCallback, handle);
}

static void queryInfoCallback(GObject* source, GAsyncResult* res, gpointer data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_cancelled) {
        cleanupGioOperation(handle);
        return;
    }

    ResourceResponse response;

    char* uri = g_file_get_uri(d->m_gfile);
    response.setUrl(KURL(uri));
    g_free(uri);

    GError *error = NULL;
    GFileInfo* info = g_file_query_info_finish(d->m_gfile, res, &error);

    if (error) {
        // FIXME: to be able to handle ftp URIs properly, we must
        // check if the error is G_IO_ERROR_NOT_MOUNTED, and if so,
        // call g_file_mount_enclosing_volume() to mount the ftp
        // server (and then keep track of the fact that we mounted it,
        // and set a timeout to unmount it later after it's been idle
        // for a while).

        client->didFail(handle, networkErrorForFile(d->m_gfile, error));
        cleanupGioOperation(handle);
        return;
    }

    if (g_file_info_get_file_type(info) != G_FILE_TYPE_REGULAR) {
        // FIXME: what if the URI points to a directory? Should we
        // generate a listing? How? What do other backends do here?

        client->didFail(handle, networkErrorForFile(d->m_gfile, error));
        cleanupGioOperation(handle);
        return;
    }

    response.setMimeType(g_file_info_get_content_type(info));
    response.setExpectedContentLength(g_file_info_get_size(info));
    response.setHTTPStatusCode(SOUP_STATUS_OK);

    GTimeVal tv;
    g_file_info_get_modification_time(info, &tv);
    response.setLastModifiedDate(tv.tv_sec);

    client->didReceiveResponse(handle, response);

    g_file_read_async(d->m_gfile, G_PRIORITY_DEFAULT, d->m_cancellable,
                      openCallback, handle);
}

bool ResourceHandle::startGio(String urlString)
{
    if (request().httpMethod() != "GET") {
        ResourceError error("webkit-network-error", ERROR_BAD_NON_HTTP_METHOD, urlString, request().httpMethod());
        d->client()->didFail(this, error);
        return false;
    }

    // Remove the fragment part of the URL since the file backend doesn't deal with it
    int fragPos;
    if ((fragPos = urlString.find("#")) != -1)
        urlString = urlString.left(fragPos);

    d->m_gfile = g_file_new_for_uri(urlString.utf8().data());
    d->m_cancellable = g_cancellable_new();
    g_file_query_info_async(d->m_gfile,
                            G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE,
                            G_PRIORITY_DEFAULT, d->m_cancellable,
                            queryInfoCallback, this);
    return true;
}

}

