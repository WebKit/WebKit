/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Xan Lopez <xan@gnome.org>
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
#include "DocLoader.h"
#include "Frame.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"

#include <libsoup/soup.h>
#include <libsoup/soup-message.h>

namespace WebCore {

static SoupSession* session = 0;

typedef enum
{
    ERROR_TRANSPORT,
    ERROR_UNKNOWN_PROTOCOL
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
    response->setExpectedContentLength(msg->response_body->length);
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

// Called at the end of the message, with all the necessary about the last informations.
// Doesn't get called for redirects.
static void dataCallback(SoupSession *session, SoupMessage* msg, gpointer data)
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
    }
   
    fillResponseFromMessage(msg, &d->m_response);
    client->didReceiveResponse(handle, d->m_response);

    // WebCore might have cancelled the job in the while
    if (d->m_cancelled)
        return;

    if (msg->response_body->data)
        client->didReceiveData(handle, msg->response_body->data, msg->response_body->length, 0);
    client->didFinishLoading(handle);
}

static gboolean parseDataUrl(gpointer callback_data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(callback_data);
    String data = handle->request().url().string();

    ASSERT(data.startsWith("data:", false));

    String header;
    bool base64 = false;

    int index = data.find(',');
    if (index != -1) {
        header = data.substring(5, index - 5).lower();
        data = data.substring(index + 1);

        if (header.endsWith(";base64")) {
            base64 = true;
            header = header.left(header.length() - 7);
        }
    } else
        data = String();

    data = decodeURLEscapeSequences(data);

    size_t outLength = 0;
    char* outData = 0;
    if (base64 && !data.isEmpty()) {
        // Use the GLib Base64 if available, since WebCore's decoder isn't
        // general-purpose and fails on Acid3 test 97 (whitespace).
        outData = reinterpret_cast<char*>(g_base64_decode(data.utf8().data(), &outLength));
    }

    if (header.isEmpty())
        header = "text/plain;charset=US-ASCII";

    ResourceHandleClient* client = handle->getInternal()->client();

    ResourceResponse response;

    response.setMimeType(extractMIMETypeFromMediaType(header));
    response.setTextEncodingName(extractCharsetFromMediaType(header));
    if (outData)
        response.setExpectedContentLength(outLength);
    else
        response.setExpectedContentLength(data.length());
    response.setHTTPStatusCode(200);

    client->didReceiveResponse(handle, response);

    if (outData)
        client->didReceiveData(handle, outData, outLength, 0);
    else
        client->didReceiveData(handle, data.latin1().data(), data.length(), 0);

    g_free(outData);

    client->didFinishLoading(handle);

    return FALSE;
}

bool ResourceHandle::start(Frame* frame)
{
    ASSERT(!d->m_msg);

    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if (!frame->page())
        return false;

    KURL url = request().url();
    String protocol = url.protocol();

    if (equalIgnoringCase(protocol, "data")) {
        // If parseDataUrl is called syncronously the job is not yet effectively started
        // and webkit won't never know that the data has been parsed even didFinishLoading is called.
        g_idle_add(parseDataUrl, this);
        return true;
    }

    String urlString = url.string();

    if (!equalIgnoringCase(protocol, "http") && !equalIgnoringCase(protocol, "https")) {
        // If we don't call didFail the job is not complete for webkit even false is returned.
        if (d->client()) {
            ResourceError error("webkit-network-error", ERROR_UNKNOWN_PROTOCOL, urlString, protocol);
            d->client()->didFail(this, error);
        }
        return false;
    }

    if (url.isLocalFile()) {
        String query = url.query();
        // Remove any query part sent to a local file.
        if (!query.isEmpty())
            urlString = urlString.left(urlString.find(query));
        // Determine the MIME type based on the path.
        d->m_response.setMimeType(MIMETypeRegistry::getMIMETypeForPath(String(urlString)));
    }

    if (!session) {
        session = soup_session_async_new();
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
    soup_session_queue_message(session, d->m_msg, dataCallback, this);

    return true;
}

void ResourceHandle::cancel()
{
    d->m_cancelled = true;
    if (d->m_msg) {
        soup_session_cancel_message(session, d->m_msg, SOUP_STATUS_CANCELLED);
        // For re-entrancy troubles we call didFinishLoading when the message hasn't been handled yet.
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

}
