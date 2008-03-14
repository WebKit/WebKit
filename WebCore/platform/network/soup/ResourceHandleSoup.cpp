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

static void dataCallback(SoupSession *session, SoupMessage* msg, gpointer data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    // TODO: maybe we should run this code even if there's no client?
    if (!handle)
        return;
    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    ResourceResponse response;

    String contentType = String(soup_message_headers_get(msg->response_headers, "Content-Type"));
    response.setMimeType(extractMIMETypeFromMediaType(contentType));
    response.setTextEncodingName(extractCharsetFromMediaType(contentType));

    response.setExpectedContentLength(msg->response_body->length);
    response.setHTTPStatusCode(msg->status_code);

    client->didReceiveResponse(handle, response);
    if (msg->response_body->data)
        client->didReceiveData(handle, msg->response_body->data, msg->response_body->length, 0);
    client->didFinishLoading(handle);
}

static void parseDataUrl(ResourceHandle* handle)
{
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
        parseDataUrl(this);
        return false;
    }

    if (!equalIgnoringCase(protocol, "http") && !equalIgnoringCase(protocol, "https")) {
        // TODO: didFail()?
        return false;
    }

    String urlString = url.string();

    if (url.isLocalFile()) {
        String query = url.query();
        // Remove any query part sent to a local file.
        if (!query.isEmpty())
            urlString = urlString.left(urlString.find(query));
        // Determine the MIME type based on the path.
        d->m_response.setMimeType(MIMETypeRegistry::getMIMETypeForPath(String(urlString)));
    }

    if (!session)
        session = soup_session_async_new();

    SoupMessage* msg;
    const char* method = 0;

    if (request().httpMethod() == "GET")
        method = SOUP_METHOD_GET;
    else if (request().httpMethod() == "POST")
        method = SOUP_METHOD_POST;
    else if (request().httpMethod() == "HEAD")
        method = SOUP_METHOD_HEAD;
    else if (request().httpMethod() == "PUT")
        method = SOUP_METHOD_PUT;
    else
        g_debug ("Unknown method!");

    msg = soup_message_new(method? method : SOUP_METHOD_GET, urlString.utf8().data());

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

    d->m_msg = (SoupMessage*)g_object_ref(msg);
    d->session = session;
    soup_session_queue_message(session, d->m_msg, dataCallback, this);

    return true;
}

void ResourceHandle::cancel()
{
    if (d->m_msg)
        soup_session_cancel_message (d->session, d->m_msg, SOUP_STATUS_CANCELLED);

    client()->didFinishLoading(this);
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
