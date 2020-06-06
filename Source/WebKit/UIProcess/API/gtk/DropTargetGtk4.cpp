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
#include "DropTarget.h"

#if ENABLE(DRAG_SUPPORT) && USE(GTK4)

#include "WebKitWebViewBasePrivate.h"
#include <WebCore/DragData.h>
#include <WebCore/GtkUtilities.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {
using namespace WebCore;

enum DropTargetType { Markup, Text, URIList, NetscapeURL, SmartPaste };

DropTarget::DropTarget(GtkWidget* webView)
    : m_webView(webView)
{
    auto* formatsBuilder = gdk_content_formats_builder_new();
    gdk_content_formats_builder_add_gtype(formatsBuilder, G_TYPE_STRING);
    gdk_content_formats_builder_add_mime_type(formatsBuilder, "text/html");
    gdk_content_formats_builder_add_mime_type(formatsBuilder, "text/uri-list");
    gdk_content_formats_builder_add_mime_type(formatsBuilder, "_NETSCAPE_URL");
    gdk_content_formats_builder_add_mime_type(formatsBuilder, "application/vnd.webkitgtk.smartpaste");
    auto* target = gtk_drop_target_async_new(gdk_content_formats_builder_free_to_formats(formatsBuilder),
        static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
    g_signal_connect(target, "accept", G_CALLBACK(+[](GtkDropTargetAsync*, GdkDrop* gdkDrop, gpointer userData) -> gboolean {
        auto& drop = *static_cast<DropTarget*>(userData);
        drop.m_drop = gdkDrop;
        drop.accept();
        return TRUE;
    }), this);

    g_signal_connect(target, "drag-enter", G_CALLBACK(+[](GtkDropTargetAsync*, GdkDrop* gdkDrop, double x, double y, gpointer userData) -> GdkDragAction {
        auto& drop = *static_cast<DropTarget*>(userData);
        if (drop.m_drop != gdkDrop)
            return static_cast<GdkDragAction>(0);

        drop.enter({ clampToInteger(x), clampToInteger(y) });
        return dragOperationToSingleGdkDragAction(drop.m_operation);
    }), this);

    g_signal_connect(target, "drag-motion", G_CALLBACK(+[](GtkDropTargetAsync*, GdkDrop* gdkDrop, double x, double y, gpointer userData) -> GdkDragAction {
        auto& drop = *static_cast<DropTarget*>(userData);
        if (drop.m_drop != gdkDrop)
            return static_cast<GdkDragAction>(0);

        drop.update({ clampToInteger(x), clampToInteger(y) });
        return dragOperationToSingleGdkDragAction(drop.m_operation);
    }), this);

    g_signal_connect(target, "drag-leave", G_CALLBACK(+[](GtkDropTargetAsync*, GdkDrop* gdkDrop, gpointer userData) {
        auto& drop = *static_cast<DropTarget*>(userData);
        if (drop.m_drop != gdkDrop)
            return;

        drop.leave();
    }), this);

    g_signal_connect(target, "drop", G_CALLBACK(+[](GtkDropTargetAsync*, GdkDrop* gdkDrop, double x, double y, gpointer userData) -> gboolean {
        auto& drop = *static_cast<DropTarget*>(userData);
        if (drop.m_drop != gdkDrop)
            return FALSE;

        drop.drop({ clampToInteger(x), clampToInteger(y) });
        return TRUE;
    }), this);

    gtk_widget_add_controller(m_webView, GTK_EVENT_CONTROLLER(target));
}

DropTarget::~DropTarget()
{
    g_cancellable_cancel(m_cancellable.get());
}

void DropTarget::accept(unsigned)
{
    m_position = WTF::nullopt;
    m_selectionData = SelectionData();
    m_dataRequestCount = 0;
    m_cancellable = adoptGRef(g_cancellable_new());

    // WebCore needs the selection data to decide, so we need to preload the
    // data of targets we support. Once all data requests are done we start
    // notifying the web process about the DND events.
    auto* formats = gdk_drop_get_formats(m_drop.get());
    if (gdk_content_formats_contain_gtype(formats, G_TYPE_STRING)) {
        m_dataRequestCount++;
        gdk_drop_read_value_async(m_drop.get(), G_TYPE_STRING, G_PRIORITY_DEFAULT, m_cancellable.get(), [](GObject* gdkDrop, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            const GValue* value = gdk_drop_read_value_finish(GDK_DROP(gdkDrop), result, &error.outPtr());
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto& drop = *static_cast<DropTarget*>(userData);
            if (value && G_VALUE_HOLDS(value, G_TYPE_STRING))
                drop.m_selectionData->setText(String::fromUTF8(g_value_get_string(value)));
            drop.didLoadData();
        }, this);
    }

    static const char* const supportedMimeTypes[] = {
        "text/html",
        "_NETSCAPE_URL",
        "text/uri-list",
        "application/vnd.webkitgtk.smartpaste"
    };

    for (unsigned i = 0; i < G_N_ELEMENTS(supportedMimeTypes); ++i) {
        if (!gdk_content_formats_contain_mime_type(formats, supportedMimeTypes[i]))
            continue;

        m_dataRequestCount++;
        loadData(supportedMimeTypes[i], [this, mimeType = String::fromUTF8(supportedMimeTypes[i]), cancellable = m_cancellable](GRefPtr<GBytes>&& data) {
            if (g_cancellable_is_cancelled(cancellable.get()))
                return;

            if (!data) {
                didLoadData();
                return;
            }

            if (mimeType == "text/html"_s) {
                gsize length;
                const auto* markupData = g_bytes_get_data(data.get(), &length);
                if (length) {
                    // If data starts with UTF-16 BOM assume it's UTF-16, otherwise assume UTF-8.
                    if (length >= 2 && reinterpret_cast<const UChar*>(markupData)[0] == 0xFEFF)
                        m_selectionData->setMarkup(String(reinterpret_cast<const UChar*>(markupData) + 1, (length / 2) - 1));
                    else
                        m_selectionData->setMarkup(String::fromUTF8(reinterpret_cast<const char*>(markupData), length));
                }
            } else if (mimeType == "_NETSCAPE_URL") {
                gsize length;
                const auto* urlData = g_bytes_get_data(data.get(), &length);
                if (length) {
                    Vector<String> tokens = String::fromUTF8(reinterpret_cast<const char*>(urlData), length).split('\n');
                    URL url({ }, tokens[0]);
                    if (url.isValid())
                        m_selectionData->setURL(url, tokens.size() > 1 ? tokens[1] : String());
                }
            } else if (mimeType == "text/uri-list") {
                gsize length;
                const auto* uriListData = g_bytes_get_data(data.get(), &length);
                if (length)
                    m_selectionData->setURIList(String::fromUTF8(reinterpret_cast<const char*>(uriListData), length));
            } else if (mimeType == "application/vnd.webkitgtk.smartpaste")
                m_selectionData->setCanSmartReplace(true);

            didLoadData();
        });
    }
}

struct DropReadAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    DropReadAsyncData(GCancellable* cancellable, CompletionHandler<void(GRefPtr<GBytes>&&)>&& handler)
        : cancellable(cancellable)
        , completionHandler(WTFMove(handler))
    {
    }

    GRefPtr<GCancellable> cancellable;
    CompletionHandler<void(GRefPtr<GBytes>&&)> completionHandler;
};

void DropTarget::loadData(const char* mimeType, CompletionHandler<void(GRefPtr<GBytes>&&)>&& completionHandler)
{
    const char* mimeTypes[] = { mimeType, nullptr };
    gdk_drop_read_async(m_drop.get(), mimeTypes, G_PRIORITY_DEFAULT, m_cancellable.get(), [](GObject* gdkDrop, GAsyncResult* result, gpointer userData) {
        std::unique_ptr<DropReadAsyncData> data(static_cast<DropReadAsyncData*>(userData));
        GRefPtr<GInputStream> inputStream = adoptGRef(gdk_drop_read_finish(GDK_DROP(gdkDrop), result, nullptr, nullptr));
        if (!inputStream) {
            data->completionHandler(nullptr);
            return;
        }

        auto* cancellable = data->cancellable.get();
        GRefPtr<GOutputStream> outputStream = adoptGRef(g_memory_output_stream_new_resizable());
        g_output_stream_splice_async(outputStream.get(), inputStream.get(),
            static_cast<GOutputStreamSpliceFlags>(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
            G_PRIORITY_DEFAULT, cancellable, [](GObject* stream, GAsyncResult* result, gpointer userData) {
                std::unique_ptr<DropReadAsyncData> data(static_cast<DropReadAsyncData*>(userData));
                GUniqueOutPtr<GError> error;
                gssize writtenBytes = g_output_stream_splice_finish(G_OUTPUT_STREAM(stream), result, &error.outPtr());
                if (writtenBytes <= 0) {
                    data->completionHandler(nullptr);
                    return;
                }
                GRefPtr<GBytes> bytes = adoptGRef(g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(stream)));
                data->completionHandler(WTFMove(bytes));
            }, data.release());
    }, new DropReadAsyncData(m_cancellable.get(), WTFMove(completionHandler)));
}

void DropTarget::didLoadData()
{
    if (--m_dataRequestCount)
        return;

    m_cancellable = nullptr;

    if (!m_position) {
        // Enter hasn't been emitted yet, so just wait for it.
        return;
    }

    // Call enter again.
    enter(IntPoint(m_position.value()));
}

void DropTarget::enter(IntPoint&& position, unsigned)
{
    m_position = WTFMove(position);
    if (m_cancellable)
        return;

    auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView));
    ASSERT(page);
    page->resetCurrentDragInformation();

    DragData dragData(&m_selectionData.value(), *m_position, *m_position, gdkDragActionToDragOperation(gdk_drop_get_actions(m_drop.get())));
    page->dragEntered(dragData);
}

void DropTarget::update(IntPoint&& position, unsigned)
{
    m_position = WTFMove(position);
    if (m_cancellable)
        return;

    auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView));
    ASSERT(page);

    DragData dragData(&m_selectionData.value(), *m_position, *m_position, gdkDragActionToDragOperation(gdk_drop_get_actions(m_drop.get())));
    page->dragUpdated(dragData);
}

void DropTarget::didPerformAction()
{
    if (!m_drop)
        return;

    auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView));
    ASSERT(page);

    auto operation = page->currentDragOperation();
    if ((!operation && !m_operation) || operation == m_operation)
        return;

    m_operation = operation;
    gdk_drop_status(m_drop.get(), dragOperationToGdkDragActions(m_operation), dragOperationToSingleGdkDragAction(m_operation));
}

void DropTarget::leave()
{
    g_cancellable_cancel(m_cancellable.get());
    m_drop = nullptr;
    m_position = WTF::nullopt;
    m_selectionData = WTF::nullopt;
    m_cancellable = nullptr;
}

void DropTarget::drop(IntPoint&& position, unsigned)
{
    m_position = WTFMove(position);
    auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView));
    ASSERT(page);

    DragData dragData(&m_selectionData.value(), *m_position, *m_position, gdkDragActionToDragOperation(gdk_drop_get_actions(m_drop.get())));
    page->performDragOperation(dragData, { }, { }, { });
    gdk_drop_finish(m_drop.get(), gdk_drop_get_actions(m_drop.get()));
}

} // namespace WebKit

#endif // ENABLE(DRAG_SUPPORT) && USE(GTK4)
