/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "webkitgtknetworkrequest.h"
#include "webkitgtkprivate.h"

using namespace WebKit;
using namespace WebCore;

extern "C" {

G_DEFINE_TYPE(WebKitNetworkRequest, webkit_network_request, G_TYPE_OBJECT);

static void webkit_network_request_finalize(GObject* object)
{
    WebKitNetworkRequestPrivate* requestPrivate = WEBKIT_NETWORK_REQUEST_GET_PRIVATE(object);

    g_free(requestPrivate->url);

    G_OBJECT_CLASS(webkit_network_request_parent_class)->finalize(object);
}

static void webkit_network_request_class_init(WebKitNetworkRequestClass* requestClass)
{
    g_type_class_add_private(requestClass, sizeof(WebKitNetworkRequestPrivate));

    G_OBJECT_CLASS(requestClass)->finalize = webkit_network_request_finalize;
}

static void webkit_network_request_init(WebKitNetworkRequest* request)
{
}

WebKitNetworkRequest* webkit_network_request_new(const gchar* url)
{
    WebKitNetworkRequest* request = WEBKIT_NETWORK_REQUEST(g_object_new(WEBKIT_TYPE_NETWORK_REQUEST, NULL));
    WebKitNetworkRequestPrivate* requestPrivate = WEBKIT_NETWORK_REQUEST_GET_PRIVATE(request);

    requestPrivate->url = g_strdup(url);

    return request;
}

void webkit_network_request_set_url(WebKitNetworkRequest* request, const gchar* url)
{
    WebKitNetworkRequestPrivate* requestPrivate;

    g_return_if_fail(WEBKIT_IS_NETWORK_REQUEST(request));

    requestPrivate = WEBKIT_NETWORK_REQUEST_GET_PRIVATE(request);

    g_free(requestPrivate->url);
    requestPrivate->url = g_strdup(url);
}

const gchar* webkit_network_request_get_url(WebKitNetworkRequest* request)
{
    WebKitNetworkRequestPrivate* requestPrivate;

    g_return_val_if_fail(WEBKIT_IS_NETWORK_REQUEST(request), NULL);

    requestPrivate = WEBKIT_NETWORK_REQUEST_GET_PRIVATE(request);

    return requestPrivate->url;
}

}
