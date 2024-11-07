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
#include <variant>
#include <wtf/RefCounted.h>
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
    g_signal_connect(m_clipboard, "changed", G_CALLBACK(+[](GdkClipboard*, gpointer userData) {
        auto& clipboard = *static_cast<Clipboard*>(userData);
        clipboard.m_changeCount++;
    }), this);
}

Clipboard::~Clipboard()
{
    g_signal_handlers_disconnect_by_data(m_clipboard, this);
}

Clipboard::Type Clipboard::type() const
{
    return m_clipboard == gdk_display_get_primary_clipboard(gdk_display_get_default()) ? Type::Primary : Type::Clipboard;
}

void Clipboard::formats(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GTK port
    gsize mimeTypesCount;
    const char* const* mimeTypes = gdk_content_formats_get_mime_types(gdk_clipboard_get_formats(m_clipboard), &mimeTypesCount);

    Vector<String> result(mimeTypesCount, [&](size_t i) {
        return String::fromUTF8(mimeTypes[i]);
    });
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    completionHandler(WTFMove(result));
}

class ClipboardTask final : public RefCounted<ClipboardTask> {
public:

    static Ref<ClipboardTask> create(GdkClipboard* clipboard, Clipboard::ReadMode readMode)
    {
        return adoptRef(*new ClipboardTask(clipboard, readMode));
    }

    ~ClipboardTask() = default;

    using ReadTextCompletionHandler = CompletionHandler<void(String&&)>;

    void readText(ReadTextCompletionHandler&& completionHandler)
    {
        RefPtr protectedThis = RefPtr { this };
        m_completionHandler = WTFMove(completionHandler);
        gdk_clipboard_read_text_async(m_clipboard, m_cancellable.get(), [](GObject* clipboard, GAsyncResult* result, gpointer userData) {
            auto task = adoptRef(static_cast<ClipboardTask*>(userData));
            GUniqueOutPtr<GError> error;
            GUniquePtr<char> text(gdk_clipboard_read_text_finish(GDK_CLIPBOARD(clipboard), result, &error.outPtr()));
            std::get<ReadTextCompletionHandler>(task->m_completionHandler)(String::fromUTF8(text.get()));
            task->done(error.get());
        }, protectedThis.leakRef());
        run();
    }

    using ReadFilePathsCompletionHandler = CompletionHandler<void(Vector<String>&&)>;

    void readFilePaths(ReadFilePathsCompletionHandler&& completionHandler)
    {
        RefPtr protectedThis = RefPtr { this };
        m_completionHandler = WTFMove(completionHandler);
        gdk_clipboard_read_value_async(m_clipboard, GDK_TYPE_FILE_LIST, G_PRIORITY_DEFAULT, m_cancellable.get(), [](GObject* clipboard, GAsyncResult* result, gpointer userData) {
            auto task = adoptRef(static_cast<ClipboardTask*>(userData));
            Vector<String> filePaths;
            GUniqueOutPtr<GError> error;
            if (const GValue* value = gdk_clipboard_read_value_finish(GDK_CLIPBOARD(clipboard), result, &error.outPtr())) {
                auto* list = static_cast<GSList*>(g_value_get_boxed(value));
                for (auto* iter = list; iter && iter->data; iter = g_slist_next(iter)) {
                    auto* file = G_FILE(iter->data);
                    if (!g_file_is_native(file))
                        continue;

                    GUniquePtr<gchar> filename(g_file_get_path(file));
                    if (filename)
                        filePaths.append(String::fromUTF8(filename.get()));
                }
            }
            std::get<ReadFilePathsCompletionHandler>(task->m_completionHandler)(WTFMove(filePaths));
            task->done(error.get());
        }, protectedThis.leakRef());
        run();
    }

    using ReadBufferCompletionHandler = CompletionHandler<void(Ref<WebCore::SharedBuffer>&&)>;

    void readBuffer(const char* format, ReadBufferCompletionHandler&& completionHandler)
    {
        RefPtr protectedThis = RefPtr { this };
        m_completionHandler = WTFMove(completionHandler);
        const char* mimeTypes[] = { format, nullptr };
        gdk_clipboard_read_async(m_clipboard, mimeTypes, G_PRIORITY_DEFAULT, m_cancellable.get(), [](GObject* clipboard, GAsyncResult* result, gpointer userData) {
            auto task = adoptRef(static_cast<ClipboardTask*>(userData));
            GUniqueOutPtr<GError> error;
            GRefPtr<GInputStream> inputStream = adoptGRef(gdk_clipboard_read_finish(GDK_CLIPBOARD(clipboard), result, nullptr, &error.outPtr()));
            if (!inputStream) {
                std::get<ReadBufferCompletionHandler>(task->m_completionHandler)(WebCore::SharedBuffer::create());
                task->done(error.get());
                return;
            }

            auto* cancellable = task->m_cancellable.get();
            GRefPtr<GOutputStream> outputStream = adoptGRef(g_memory_output_stream_new_resizable());
            g_output_stream_splice_async(outputStream.get(), inputStream.get(), static_cast<GOutputStreamSpliceFlags>(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                G_PRIORITY_DEFAULT, cancellable, [](GObject* stream, GAsyncResult* result, gpointer userData) {
                    auto task = adoptRef(static_cast<ClipboardTask*>(userData));
                    GUniqueOutPtr<GError> error;
                    gssize writtenBytes = g_output_stream_splice_finish(G_OUTPUT_STREAM(stream), result, &error.outPtr());
                    if (writtenBytes <= 0) {
                        std::get<ReadBufferCompletionHandler>(task->m_completionHandler)(WebCore::SharedBuffer::create());
                        task->done(error.get());
                        return;
                    }

                    GRefPtr<GBytes> bytes = adoptGRef(g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(stream)));
                    std::get<ReadBufferCompletionHandler>(task->m_completionHandler)(WebCore::SharedBuffer::create(bytes.get()));
                    task->done(error.get());
                }, task.leakRef());
        }, protectedThis.leakRef());
        run();
    }

    using ReadURLCompletionHandler = CompletionHandler<void(String&& url, String&& title)>;

    void readURL(ReadURLCompletionHandler&& completionHandler)
    {
        RefPtr protectedThis = RefPtr { this };
        m_completionHandler = WTFMove(completionHandler);
        gdk_clipboard_read_value_async(m_clipboard, GDK_TYPE_FILE_LIST, G_PRIORITY_DEFAULT, m_cancellable.get(), [](GObject* clipboard, GAsyncResult* result, gpointer userData) {
            auto task = adoptRef(static_cast<ClipboardTask*>(userData));
            GUniqueOutPtr<GError> error;
            const GValue* value = gdk_clipboard_read_value_finish(GDK_CLIPBOARD(clipboard), result, &error.outPtr());
            auto* list = static_cast<GSList*>(g_value_get_boxed(value));
            auto* file = list && list->data ? G_FILE(list->data) : nullptr;
            GUniquePtr<char> uri(g_file_get_uri(file));
            std::get<ReadURLCompletionHandler>(task->m_completionHandler)(uri ? String::fromUTF8(uri.get()) : String(), { });
            task->done(error.get());
        }, protectedThis.leakRef());
        run();
    }

private:
    ClipboardTask(GdkClipboard* clipboard, Clipboard::ReadMode readMode)
        : m_clipboard(clipboard)
        , m_readMode(readMode)
        , m_cancellable(readMode == Clipboard::ReadMode::Synchronous ? adoptGRef(g_cancellable_new()) : nullptr)
        , m_mainLoop(readMode == Clipboard::ReadMode::Synchronous ? adoptGRef(g_main_loop_new(nullptr, FALSE)) : nullptr)
    {
    }

    void run()
    {
        if (m_readMode == Clipboard::ReadMode::Asynchronous)
            return;

        m_timeoutSourceID = g_timeout_add(500, [](gpointer userData) -> gboolean {
            auto& task = *static_cast<ClipboardTask*>(userData);
            task.timeout();
            return G_SOURCE_REMOVE;
        }, this);

        g_main_loop_run(m_mainLoop.get());

        g_cancellable_cancel(m_cancellable.get());
        g_clear_handle_id(&m_timeoutSourceID, g_source_remove);
    }

    void timeout()
    {
        RELEASE_ASSERT(m_readMode == Clipboard::ReadMode::Synchronous);
        m_timeoutSourceID = 0;
        done(nullptr);
    }

    void done(GError* error)
    {
        if (m_readMode == Clipboard::ReadMode::Asynchronous)
            return;

        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_main_loop_quit(m_mainLoop.get());
    }

    GdkClipboard* m_clipboard { nullptr };
    Clipboard::ReadMode m_readMode { Clipboard::ReadMode::Asynchronous };
    GRefPtr<GCancellable> m_cancellable;
    GRefPtr<GMainLoop> m_mainLoop;
    unsigned m_timeoutSourceID { 0 };
    std::variant<ReadTextCompletionHandler, ReadFilePathsCompletionHandler, ReadBufferCompletionHandler, ReadURLCompletionHandler> m_completionHandler;
};

void Clipboard::readText(CompletionHandler<void(String&&)>&& completionHandler, ReadMode readMode)
{
    ClipboardTask::create(m_clipboard, readMode)->readText(WTFMove(completionHandler));
}

void Clipboard::readFilePaths(CompletionHandler<void(Vector<String>&&)>&& completionHandler, ReadMode readMode)
{
    ClipboardTask::create(m_clipboard, readMode)->readFilePaths(WTFMove(completionHandler));
}

void Clipboard::readBuffer(const char* format, CompletionHandler<void(Ref<WebCore::SharedBuffer>&&)>&& completionHandler, ReadMode readMode)
{
    ClipboardTask::create(m_clipboard, readMode)->readBuffer(format, WTFMove(completionHandler));
}

void Clipboard::readURL(CompletionHandler<void(String&& url, String&& title)>&& completionHandler, ReadMode readMode)
{
    ClipboardTask::create(m_clipboard, readMode)->readURL(WTFMove(completionHandler));
}

void Clipboard::write(WebCore::SelectionData&& selectionData, CompletionHandler<void(int64_t)>&& completionHandler)
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
        auto pixbuf = selectionData.image()->adapter().gdkPixbuf();
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

    for (const auto& it : selectionData.buffers()) {
        GRefPtr<GBytes> bytes = it.value->createGBytes();
        providers.append(gdk_content_provider_new_for_bytes(it.key.utf8().data(), bytes.get()));
    }

    if (providers.isEmpty()) {
        clear();
        completionHandler(m_changeCount);
        return;
    }

    GRefPtr<GdkContentProvider> provider = adoptGRef(gdk_content_provider_new_union(providers.data(), providers.size()));
    gdk_clipboard_set_content(m_clipboard, provider.get());
    completionHandler(m_changeCount);
}

void Clipboard::clear()
{
    gdk_clipboard_set_content(m_clipboard, nullptr);
}

} // namespace WebKit

#endif
