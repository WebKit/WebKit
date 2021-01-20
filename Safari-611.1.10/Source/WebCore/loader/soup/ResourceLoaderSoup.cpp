/*
 * Copyright (C) 2018 Igalia S.L.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "ResourceLoader.h"

#include "HTTPParsers.h"
#include "ResourceError.h"
#include "SharedBuffer.h"
#include <gio/gio.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace WebCore {

void ResourceLoader::loadGResource()
{
    RefPtr<ResourceLoader> protectedThis(this);
    GRefPtr<GTask> task = adoptGRef(g_task_new(nullptr, nullptr, [](GObject*, GAsyncResult* result, gpointer userData) {
        RefPtr<ResourceLoader> loader = adoptRef(static_cast<ResourceLoader*>(userData));
        if (loader->reachedTerminalState())
            return;

        auto* task = G_TASK(result);
        URL url({ }, String::fromUTF8(static_cast<const char*>(g_task_get_task_data(task))));

        GUniqueOutPtr<GError> error;
        GRefPtr<GBytes> bytes = adoptGRef(static_cast<GBytes*>(g_task_propagate_pointer(task, &error.outPtr())));
        if (!bytes) {
            loader->didFail(ResourceError(g_quark_to_string(error->domain), error->code, url, String::fromUTF8(error->message)));
            return;
        }

        if (loader->wasCancelled())
            return;

        gsize dataSize;
        const auto* data = static_cast<const guchar*>(g_bytes_get_data(bytes.get(), &dataSize));
        GUniquePtr<char> fileName(g_path_get_basename(url.path().utf8().data()));
        GUniquePtr<char> contentType(g_content_type_guess(fileName.get(), data, dataSize, nullptr));
        ResourceResponse response { url, extractMIMETypeFromMediaType(contentType.get()), static_cast<long long>(dataSize), extractCharsetFromMediaType(contentType.get()) };
        response.setHTTPStatusCode(200);
        response.setHTTPStatusText("OK"_s);
        response.setHTTPHeaderField(HTTPHeaderName::ContentType, contentType.get());
        response.setSource(ResourceResponse::Source::Network);
        loader->deliverResponseAndData(response, SharedBuffer::create(bytes.get()));
    }, protectedThis.leakRef()));

    g_task_set_priority(task.get(), RunLoopSourcePriority::AsyncIONetwork);
    g_task_set_task_data(task.get(), g_strdup(m_request.url().string().utf8().data()), g_free);
    g_task_run_in_thread(task.get(), [](GTask* task, gpointer, gpointer taskData, GCancellable*) {
        URL url({ }, String::fromUTF8(static_cast<const char*>(taskData)));
        GError* error = nullptr;
        GBytes* bytes = g_resources_lookup_data(url.path().utf8().data(), G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
        if (!bytes)
            g_task_return_error(task, error);
        else
            g_task_return_pointer(task, bytes, reinterpret_cast<GDestroyNotify>(g_bytes_unref));
    });
}

} // namespace WebCore
