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

#if !USE(GTK4)

#include "WebPasteboardProxy.h"
#include <WebCore/GRefPtrGtk.h>
#include <WebCore/PasteboardCustomData.h>
#include <WebCore/SelectionData.h>
#include <WebCore/SharedBuffer.h>
#include <gtk/gtk.h>
#include <wtf/SetForScope.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

Clipboard::Clipboard(Type type)
    : m_clipboard(gtk_clipboard_get_for_display(gdk_display_get_default(), type == Type::Clipboard ? GDK_SELECTION_CLIPBOARD : GDK_SELECTION_PRIMARY))
{
    g_signal_connect(m_clipboard, "owner-change", G_CALLBACK(+[](GtkClipboard*, GdkEvent*, gpointer userData) {
        auto& clipboard = *static_cast<Clipboard*>(userData);
        clipboard.m_changeCount++;
    }), this);
}

Clipboard::~Clipboard()
{
    g_signal_handlers_disconnect_by_data(m_clipboard, this);
}

static bool isPrimaryClipboard(GtkClipboard* clipboard)
{
    return clipboard == gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_PRIMARY);
}

Clipboard::Type Clipboard::type() const
{
    return isPrimaryClipboard(m_clipboard) ? Type::Primary : Type::Clipboard;
}

struct FormatsAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    explicit FormatsAsyncData(CompletionHandler<void(Vector<String>&&)>&& handler)
        : completionHandler(WTFMove(handler))
    {
    }

    CompletionHandler<void(Vector<String>&&)> completionHandler;
};

void Clipboard::formats(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    gtk_clipboard_request_targets(m_clipboard, [](GtkClipboard* clipboard, GdkAtom* atoms, gint atomsCount, gpointer userData) {
        std::unique_ptr<FormatsAsyncData> data(static_cast<FormatsAsyncData*>(userData));

        Vector<String> result;
        for (int i = 0; i < atomsCount; ++i) {
            GUniquePtr<char> atom(gdk_atom_name(atoms[i]));
            result.append(String::fromUTF8(atom.get()));
        }
        data->completionHandler(WTFMove(result));
    }, new FormatsAsyncData(WTFMove(completionHandler)));
}

struct ReadTextAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    explicit ReadTextAsyncData(CompletionHandler<void(String&&)>&& handler)
        : completionHandler(WTFMove(handler))
    {
    }

    CompletionHandler<void(String&&)> completionHandler;
};

void Clipboard::readText(CompletionHandler<void(String&&)>&& completionHandler, ReadMode)
{
    gtk_clipboard_request_text(m_clipboard, [](GtkClipboard*, const char* text, gpointer userData) {
        std::unique_ptr<ReadTextAsyncData> data(static_cast<ReadTextAsyncData*>(userData));
        data->completionHandler(String::fromUTF8(text));
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

void Clipboard::readFilePaths(CompletionHandler<void(Vector<String>&&)>&& completionHandler, ReadMode)
{
    gtk_clipboard_request_uris(m_clipboard, [](GtkClipboard*, char** uris, gpointer userData) {
        std::unique_ptr<ReadFilePathsAsyncData> data(static_cast<ReadFilePathsAsyncData*>(userData));
        Vector<String> result;
        for (unsigned i = 0; uris && uris[i]; ++i) {
            GUniquePtr<gchar> filename(g_filename_from_uri(uris[i], nullptr, nullptr));
            if (filename)
                result.append(String::fromUTF8(filename.get()));
        }
        data->completionHandler(WTFMove(result));
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

struct ReadURLAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    explicit ReadURLAsyncData(CompletionHandler<void(String&& url, String&& title)>&& handler)
        : completionHandler(WTFMove(handler))
    {
    }

    CompletionHandler<void(String&& url, String&& title)> completionHandler;
};

void Clipboard::readURL(CompletionHandler<void(String&& url, String&& title)>&& completionHandler, ReadMode)
{
    gtk_clipboard_request_uris(m_clipboard, [](GtkClipboard*, char** uris, gpointer userData) {
        std::unique_ptr<ReadURLAsyncData> data(static_cast<ReadURLAsyncData*>(userData));
        data->completionHandler(uris && uris[0] ? String::fromUTF8(uris[0]) : String(), { });
    }, new ReadURLAsyncData(WTFMove(completionHandler)));
}

void Clipboard::readBuffer(const char* format, CompletionHandler<void(Ref<WebCore::SharedBuffer>&&)>&& completionHandler, ReadMode)
{
    gtk_clipboard_request_contents(m_clipboard, gdk_atom_intern(format, TRUE), [](GtkClipboard*, GtkSelectionData* selection, gpointer userData) {
        std::unique_ptr<ReadBufferAsyncData> data(static_cast<ReadBufferAsyncData*>(userData));
        int contentsLength;
        const auto* contents = gtk_selection_data_get_data_with_length(selection, &contentsLength);
        data->completionHandler(WebCore::SharedBuffer::create(std::span { contents, static_cast<size_t>(std::max(0, contentsLength)) }));
    }, new ReadBufferAsyncData(WTFMove(completionHandler)));
}

struct WriteAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    WriteAsyncData(WebCore::SelectionData&& selection, Clipboard& clipboard)
        : selectionData(WTFMove(selection))
        , clipboard(clipboard)
    {
    }

    WebCore::SelectionData selectionData;
    Clipboard& clipboard;
};

enum ClipboardTargetType { Markup, Text, Image, URIList, SmartPaste, Custom, Buffer };

void Clipboard::write(WebCore::SelectionData&& selectionData, CompletionHandler<void(int64_t)>&& completionHandler)
{
    SetForScope frameWritingToClipboard(m_frameWritingToClipboard, WebPasteboardProxy::singleton().primarySelectionOwner());

    GRefPtr<GtkTargetList> list = adoptGRef(gtk_target_list_new(nullptr, 0));
    if (selectionData.hasURIList())
        gtk_target_list_add(list.get(), gdk_atom_intern_static_string("text/uri-list"), 0, ClipboardTargetType::URIList);
    if (selectionData.hasMarkup())
        gtk_target_list_add(list.get(), gdk_atom_intern_static_string("text/html"), 0, ClipboardTargetType::Markup);
    if (selectionData.hasImage())
        gtk_target_list_add_image_targets(list.get(), ClipboardTargetType::Image, TRUE);
    if (selectionData.hasText())
        gtk_target_list_add_text_targets(list.get(), ClipboardTargetType::Text);
    if (selectionData.canSmartReplace())
        gtk_target_list_add(list.get(), gdk_atom_intern_static_string("application/vnd.webkitgtk.smartpaste"), 0, ClipboardTargetType::SmartPaste);
    if (selectionData.hasCustomData())
        gtk_target_list_add(list.get(), gdk_atom_intern_static_string(WebCore::PasteboardCustomData::gtkType().characters()), 0, ClipboardTargetType::Custom);
    for (const auto& type : selectionData.buffers().keys())
        gtk_target_list_add(list.get(), gdk_atom_intern(type.utf8().data(), FALSE), 0, ClipboardTargetType::Buffer);

    int numberOfTargets;
    GtkTargetEntry* table = gtk_target_table_new_from_list(list.get(), &numberOfTargets);
    if (!numberOfTargets) {
        gtk_clipboard_clear(m_clipboard);
        completionHandler(m_changeCount + 1);
        return;
    }

    auto data = WTF::makeUnique<WriteAsyncData>(WTFMove(selectionData), *this);
    gboolean succeeded = gtk_clipboard_set_with_data(m_clipboard, table, numberOfTargets,
        [](GtkClipboard*, GtkSelectionData* selection, guint info, gpointer userData) {
            auto& data = *static_cast<WriteAsyncData*>(userData);
            switch (info) {
            case ClipboardTargetType::Markup: {
                CString markup = data.selectionData.markup().utf8();
                gtk_selection_data_set(selection, gdk_atom_intern_static_string("text/html"), 8, reinterpret_cast<const guchar*>(markup.data()), markup.length());
                break;
            }
            case ClipboardTargetType::Text:
                gtk_selection_data_set_text(selection, data.selectionData.text().utf8().data(), -1);
                break;
            case ClipboardTargetType::Image: {
                if (data.selectionData.hasImage()) {
                    auto pixbuf = data.selectionData.image()->adapter().gdkPixbuf();
                    gtk_selection_data_set_pixbuf(selection, pixbuf.get());
                }
                break;
            }
            case ClipboardTargetType::URIList: {
                CString uriList = data.selectionData.uriList().utf8();
                gtk_selection_data_set(selection, gdk_atom_intern_static_string("text/uri-list"), 8, reinterpret_cast<const guchar*>(uriList.data()), uriList.length());
                break;
            }
            case ClipboardTargetType::SmartPaste:
                gtk_selection_data_set_text(selection, "", -1);
                break;
            case ClipboardTargetType::Custom:
                if (data.selectionData.hasCustomData()) {
                    auto buffer = data.selectionData.customData()->span();
                    gtk_selection_data_set(selection, gdk_atom_intern_static_string(WebCore::PasteboardCustomData::gtkType().characters()), 8, reinterpret_cast<const guchar*>(buffer.data()), buffer.size());
                }
                break;
            case ClipboardTargetType::Buffer: {
                auto* atom = gtk_selection_data_get_target(selection);
                GUniquePtr<char> type(gdk_atom_name(atom));
                if (auto* buffer = data.selectionData.buffer(String::fromUTF8(type.get()))) {
                    auto span = buffer->span();
                    gtk_selection_data_set(selection, atom, 8, reinterpret_cast<const guchar*>(span.data()), span.size());
                }
                break;
            }
            }
        },
        [](GtkClipboard* clipboard, gpointer userData) {
            std::unique_ptr<WriteAsyncData> data(static_cast<WriteAsyncData*>(userData));
            if (isPrimaryClipboard(clipboard) && WebPasteboardProxy::singleton().primarySelectionOwner() != data->clipboard.m_frameWritingToClipboard)
                WebPasteboardProxy::singleton().setPrimarySelectionOwner(nullptr);
        }, data.get());

    if (succeeded) {
        // In case of success the clipboard owns data, so we release it here.
        data.release();

        gtk_clipboard_set_can_store(m_clipboard, nullptr, 0);
    }
    gtk_target_table_free(table, numberOfTargets);
    completionHandler(succeeded ? m_changeCount + 1 : m_changeCount);
}

void Clipboard::clear()
{
    gtk_clipboard_clear(m_clipboard);
}

} // namespace WebKit

#endif
