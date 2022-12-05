/*
 * Copyright (C) 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Clipboard.h"

#if USE(GTK4)

#include "WebPasteboardProxy.h"
#include <WebCore/PasteboardCustomData.h>
#include <WebCore/SelectionData.h>
#include <WebCore/SharedBuffer.h>
#include <gtk/gtk.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

Clipboard::Clipboard(Type type)
    : m_clipboard(type == Type::Clipboard ? gdk_display_get_clipboard(gdk_display_get_default()) : gdk_display_get_primary_clipboard(gdk_display_get_default()))
{
    if (type == Type::Primary) {
        g_signal_connect(m_clipboard, "notify::local", G_CALLBACK(+[](GdkClipboard* clipboard, GParamSpec*, gpointer) {
            if (!gdk_clipboard_is_local(clipboard))
                WebPasteboardProxy::singleton().setPrimarySelectionOwner(nullptr);
        }), nullptr);
    }
}

Clipboard::Type Clipboard::type() const
{
    return m_clipboard == gdk_display_get_primary_clipboard(gdk_display_get_default()) ? Type::Primary : Type::Clipboard;
}

void Clipboard::formats(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    gsize mimeTypesCount;
    const char* const* mimeTypes = gdk_content_formats_get_mime_types(gdk_clipboard_get_formats(m_clipboard), &mimeTypesCount);

    Vector<String> result;
    result.reserveInitialCapacity(mimeTypesCount);
    for (size_t i = 0; i < mimeTypesCount; ++i)
        result.uncheckedAppend(String::fromUTF8(mimeTypes[i]));
    completionHandler(WTFMove(result));
}

struct ReadTextAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    explicit ReadTextAsyncData(CompletionHandler<void(String&&)>&& handler)
        : completionHandler(WTFMove(handler))
    {
    }

    CompletionHandler<void(String&&)> completionHandler;
};

void Clipboard::readText(CompletionHandler<void(String&&)>&& completionHandler)
{
    gdk_clipboard_read_text_async(m_clipboard, nullptr, [](GObject* clipboard, GAsyncResult* result, gpointer userData) {
        std::unique_ptr<ReadTextAsyncData> data(static_cast<ReadTextAsyncData*>(userData));
        GUniquePtr<char> text(gdk_clipboard_read_text_finish(GDK_CLIPBOARD(clipboard), result, nullptr));
        data->completionHandler(String::fromUTF8(text.get()));
    }, new ReadTextAsyncData(WTFMove(completionHandler)));
}

struct ReadFilePathsAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    explicit ReadFilePathsAsyncData(CompletionHandler<void(Vector<String>&&)>&& handler)
        : completionHandler(WTFMove(handler))
    {
    }

    CompletionHandler<void(Vector<String>&&)> completionHandler;
};

void Clipboard::readFilePaths(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    gdk_clipboard_read_value_async(m_clipboard, GDK_TYPE_FILE_LIST, G_PRIORITY_DEFAULT, nullptr, [](GObject* clipboard, GAsyncResult* result, gpointer userData) {
        std::unique_ptr<ReadFilePathsAsyncData> data(static_cast<ReadFilePathsAsyncData*>(userData));
        Vector<String> filePaths;
        if (const GValue* value = gdk_clipboard_read_value_finish(GDK_CLIPBOARD(clipboard), result, nullptr)) {
            auto* list = static_cast<GSList*>(g_value_get_boxed(value));
            for (auto* l = list; l && l->data; l = g_slist_next(l)) {
                auto* file = G_FILE(l->data);
                if (!g_file_is_native(file))
                    continue;

                GUniquePtr<gchar> filename(g_file_get_path(file));
                if (filename)
                    filePaths.append(String::fromUTF8(filename.get()));
            }
        }
        data->completionHandler(WTFMove(filePaths));
    }, new ReadFilePathsAsyncData(WTFMove(completionHandler)));
}

struct ReadBufferAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    explicit ReadBufferAsyncData(CompletionHandler<void(Ref<WebCore::SharedBuffer>&&)>&& handler)
        : completionHandler(WTFMove(handler))
    {
    }

    CompletionHandler<void(Ref<WebCore::SharedBuffer>&&)> completionHandler;
};

void Clipboard::readBuffer(const char* format, CompletionHandler<void(Ref<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    const char* mimeTypes[] = { format, nullptr };
    gdk_clipboard_read_async(m_clipboard, mimeTypes, G_PRIORITY_DEFAULT, nullptr, [](GObject* clipboard, GAsyncResult* result, gpointer userData) {
        std::unique_ptr<ReadBufferAsyncData> data(static_cast<ReadBufferAsyncData*>(userData));
        GRefPtr<GInputStream> inputStream = adoptGRef(gdk_clipboard_read_finish(GDK_CLIPBOARD(clipboard), result, nullptr, nullptr));
        if (!inputStream) {
            data->completionHandler(WebCore::SharedBuffer::create());
            return;
        }

        GRefPtr<GOutputStream> outputStream = adoptGRef(g_memory_output_stream_new_resizable());
        g_output_stream_splice_async(outputStream.get(), inputStream.get(),
            static_cast<GOutputStreamSpliceFlags>(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
            G_PRIORITY_DEFAULT, nullptr, [](GObject* stream, GAsyncResult* result, gpointer userData) {
                std::unique_ptr<ReadBufferAsyncData> data(static_cast<ReadBufferAsyncData*>(userData));
                gssize writtenBytes = g_output_stream_splice_finish(G_OUTPUT_STREAM(stream), result, nullptr);
                if (writtenBytes <= 0) {
                    data->completionHandler(WebCore::SharedBuffer::create());
                    return;
                }

                GRefPtr<GBytes> bytes = adoptGRef(g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(stream)));
                data->completionHandler(WebCore::SharedBuffer::create(bytes.get()));
            }, data.release());
    }, new ReadBufferAsyncData(WTFMove(completionHandler)));
}

void Clipboard::write(WebCore::SelectionData&& selectionData)
{
    Vector<GdkContentProvider*> providers;
    if (selectionData.hasMarkup()) {
        CString markup = selectionData.markup().utf8();
        GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(markup.data(), markup.length()));
        providers.append(gdk_content_provider_new_for_bytes("text/html", bytes.get()));
    }

    if (selectionData.hasURIList()) {
        CString uriList = selectionData.uriList().utf8();
        GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(uriList.data(), uriList.length()));
        providers.append(gdk_content_provider_new_for_bytes("text/uri-list", bytes.get()));
    }

    if (selectionData.hasImage()) {
        GRefPtr<GdkPixbuf> pixbuf = adoptGRef(selectionData.image()->getGdkPixbuf());
        providers.append(gdk_content_provider_new_typed(GDK_TYPE_PIXBUF, pixbuf.get()));
    }

    if (selectionData.hasText())
        providers.append(gdk_content_provider_new_typed(G_TYPE_STRING, selectionData.text().utf8().data()));

    if (selectionData.canSmartReplace()) {
        GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(nullptr, 0));
        providers.append(gdk_content_provider_new_for_bytes("application/vnd.webkitgtk.smartpaste", bytes.get()));
    }

    if (selectionData.hasCustomData()) {
        GRefPtr<GBytes> bytes = selectionData.customData()->createGBytes();
        providers.append(gdk_content_provider_new_for_bytes(WebCore::PasteboardCustomData::gtkType().characters(), bytes.get()));
    }

    if (providers.isEmpty()) {
        clear();
        return;
    }

    GRefPtr<GdkContentProvider> provider = adoptGRef(gdk_content_provider_new_union(providers.data(), providers.size()));
    gdk_clipboard_set_content(m_clipboard, provider.get());
}

void Clipboard::clear()
{
    gdk_clipboard_set_content(m_clipboard, nullptr);
}

} // namespace WebKit

#endif
