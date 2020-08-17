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

#if ENABLE(DRAG_SUPPORT) && !USE(GTK4)

#include "WebKitWebViewBasePrivate.h"
#include <WebCore/DragData.h>
#include <WebCore/GRefPtrGtk.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/PasteboardCustomData.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {
using namespace WebCore;

enum DropTargetType { Markup, Text, URIList, NetscapeURL, SmartPaste, Custom };

DropTarget::DropTarget(GtkWidget* webView)
    : m_webView(webView)
    , m_leaveTimer(RunLoop::main(), this, &DropTarget::leaveTimerFired)
{
    GRefPtr<GtkTargetList> list = adoptGRef(gtk_target_list_new(nullptr, 0));
    gtk_target_list_add_text_targets(list.get(), DropTargetType::Text);
    gtk_target_list_add(list.get(), gdk_atom_intern_static_string("text/html"), 0, DropTargetType::Markup);
    gtk_target_list_add_uri_targets(list.get(), DropTargetType::URIList);
    gtk_target_list_add(list.get(), gdk_atom_intern_static_string("_NETSCAPE_URL"), 0, DropTargetType::NetscapeURL);
    gtk_target_list_add(list.get(), gdk_atom_intern_static_string("application/vnd.webkitgtk.smartpaste"), 0, DropTargetType::SmartPaste);
    gtk_target_list_add(list.get(), gdk_atom_intern_static_string(PasteboardCustomData::gtkType()), 0, DropTargetType::Custom);
    gtk_drag_dest_set(m_webView, static_cast<GtkDestDefaults>(0), nullptr, 0,
        static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
    gtk_drag_dest_set_target_list(m_webView, list.get());

    g_signal_connect(m_webView, "drag-motion", G_CALLBACK(+[](GtkWidget*, GdkDragContext* context, gint x, gint y, guint time, gpointer userData) -> gboolean {
        auto& drop = *static_cast<DropTarget*>(userData);
        if (!drop.m_drop) {
            drop.m_drop = context;
            drop.m_position = IntPoint(x, y);
            drop.accept(time);
        } else if (drop.m_drop == context)
            drop.update({ x, y }, time);
        return TRUE;
    }), this);

    g_signal_connect(m_webView, "drag-leave", G_CALLBACK(+[](GtkWidget*, GdkDragContext* context, guint time, gpointer userData) {
        auto& drop = *static_cast<DropTarget*>(userData);
        if (drop.m_drop != context)
            return;
        drop.leave();
    }), this);

    g_signal_connect(m_webView, "drag-drop", G_CALLBACK(+[](GtkWidget*, GdkDragContext* context, gint x, gint y, guint time, gpointer userData) -> gboolean {
        auto& drop = *static_cast<DropTarget*>(userData);
        if (drop.m_drop != context) {
            gtk_drag_finish(context, FALSE, FALSE, time);
            return TRUE;
        }
        drop.drop({ x, y }, time);
        return TRUE;
    }), this);

    g_signal_connect(m_webView, "drag-data-received", G_CALLBACK(+[](GtkWidget*, GdkDragContext* context, gint x, gint y, GtkSelectionData* data, guint info, guint time, gpointer userData) {
        auto& drop = *static_cast<DropTarget*>(userData);
        if (drop.m_drop != context)
            return;
        drop.dataReceived({ x, y }, data, info, time);
    }), this);
}

DropTarget::~DropTarget()
{
    g_signal_handlers_disconnect_by_data(m_webView, this);
}

void DropTarget::accept(unsigned time)
{
    if (m_leaveTimer.isActive()) {
        m_leaveTimer.stop();
        leaveTimerFired();
    }

    m_dataRequestCount = 0;
    m_selectionData = WTF::nullopt;

    // WebCore needs the selection data to decide, so we need to preload the
    // data of targets we support. Once all data requests are done we start
    // notifying the web process about the DND events.
    auto* list = gdk_drag_context_list_targets(m_drop.get());
    static const char* const supportedTargets[] = {
        "text/plain;charset=utf-8",
        "text/html",
        "_NETSCAPE_URL",
        "text/uri-list",
        "application/vnd.webkitgtk.smartpaste",
        "org.webkitgtk.WebKit.custom-pasteboard-data"
    };
    Vector<GdkAtom, 4> targets;
    for (unsigned i = 0; i < G_N_ELEMENTS(supportedTargets); ++i) {
        GdkAtom atom = gdk_atom_intern_static_string(supportedTargets[i]);
        if (g_list_find(list, atom))
            targets.append(atom);
        else if (!i) {
            atom = gdk_atom_intern_static_string("text/plain");
            if (g_list_find(list, atom))
                targets.append(atom);
        }
    }

    if (targets.isEmpty())
        return;

    m_dataRequestCount = targets.size();
    m_selectionData = SelectionData();
    for (auto* atom : targets)
        gtk_drag_get_data(m_webView, m_drop.get(), atom, time);
}

void DropTarget::enter(IntPoint&& position, unsigned time)
{
    m_position = WTFMove(position);

    auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView));
    ASSERT(page);
    page->resetCurrentDragInformation();

    DragData dragData(&m_selectionData.value(), *m_position, convertWidgetPointToScreenPoint(m_webView, *m_position), gdkDragActionToDragOperation(gdk_drag_context_get_actions(m_drop.get())));
    page->dragEntered(dragData);
}

void DropTarget::update(IntPoint&& position, unsigned time)
{
    if (m_dataRequestCount || !m_selectionData)
        return;

    m_position = WTFMove(position);

    auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView));
    ASSERT(page);

    DragData dragData(&m_selectionData.value(), *m_position, convertWidgetPointToScreenPoint(m_webView, *m_position), gdkDragActionToDragOperation(gdk_drag_context_get_actions(m_drop.get())));
    page->dragUpdated(dragData);
}

void DropTarget::dataReceived(IntPoint&& position, GtkSelectionData* data, unsigned info, unsigned time)
{
    switch (info) {
    case DropTargetType::Text: {
        GUniquePtr<char> text(reinterpret_cast<char*>(gtk_selection_data_get_text(data)));
        m_selectionData->setText(String::fromUTF8(text.get()));
        break;
    }
    case DropTargetType::Markup: {
        gint length;
        const auto* markupData = gtk_selection_data_get_data_with_length(data, &length);
        if (length) {
            // If data starts with UTF-16 BOM assume it's UTF-16, otherwise assume UTF-8.
            if (length >= 2 && reinterpret_cast<const UChar*>(markupData)[0] == 0xFEFF)
                m_selectionData->setMarkup(String(reinterpret_cast<const UChar*>(markupData) + 1, (length / 2) - 1));
            else
                m_selectionData->setMarkup(String::fromUTF8(markupData, length));
        }
        break;
    }
    case DropTargetType::URIList: {
        gint length;
        const auto* uriListData = gtk_selection_data_get_data_with_length(data, &length);
        if (length)
            m_selectionData->setURIList(String::fromUTF8(uriListData, length));
        break;
    }
    case DropTargetType::NetscapeURL: {
        gint length;
        const auto* urlData = gtk_selection_data_get_data_with_length(data, &length);
        if (length) {
            Vector<String> tokens = String::fromUTF8(urlData, length).split('\n');
            URL url({ }, tokens[0]);
            if (url.isValid())
                m_selectionData->setURL(url, tokens.size() > 1 ? tokens[1] : String());
        }
        break;
    }
    case DropTargetType::SmartPaste:
        m_selectionData->setCanSmartReplace(true);
        break;
    case DropTargetType::Custom: {
        int length;
        const auto* customData = gtk_selection_data_get_data_with_length(data, &length);
        if (length)
            m_selectionData->setCustomData(SharedBuffer::create(customData, static_cast<size_t>(length)));
        break;
    }
    }

    if (--m_dataRequestCount)
        return;

    enter(WTFMove(position), time);
}

void DropTarget::didPerformAction()
{
    if (!m_drop)
        return;

    auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView));
    ASSERT(page);

    auto operation = page->currentDragOperation();
    if (operation == m_operation)
        return;

    m_operation = operation;
    gdk_drag_status(m_drop.get(), dragOperationToSingleGdkDragAction(m_operation), GDK_CURRENT_TIME);
}

void DropTarget::leaveTimerFired()
{
    auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView));
    ASSERT(page);

    SelectionData emptyData;
    DragData dragData(m_selectionData ? &m_selectionData.value() : &emptyData, *m_position, convertWidgetPointToScreenPoint(m_webView, *m_position), { });
    page->dragExited(dragData);
    page->resetCurrentDragInformation();

    m_drop = nullptr;
    m_position = WTF::nullopt;
    m_selectionData = WTF::nullopt;
}

void DropTarget::leave()
{
    // GTK emits drag-leave before drag-drop, but we need to know whether a drop
    // happened to notify the web process about the drag exit.
    m_leaveTimer.startOneShot(0_s);
}

void DropTarget::drop(IntPoint&& position, unsigned time)
{
    if (m_leaveTimer.isActive())
        m_leaveTimer.stop();

    auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView));
    ASSERT(page);

    OptionSet<DragApplicationFlags> flags;
    if (gdk_drag_context_get_selected_action(m_drop.get()) == GDK_ACTION_COPY)
        flags.add(DragApplicationFlags::IsCopyKeyDown);
    DragData dragData(&m_selectionData.value(), position, convertWidgetPointToScreenPoint(m_webView, position), gdkDragActionToDragOperation(gdk_drag_context_get_actions(m_drop.get())), flags);
    page->performDragOperation(dragData, { }, { }, { });
    gtk_drag_finish(m_drop.get(), TRUE, FALSE, time);

    m_drop = nullptr;
    m_position = WTF::nullopt;
    m_selectionData = WTF::nullopt;
}

} // namespace WebKit

#endif // ENABLE(DRAG_SUPPORT) && !USE(GTK4)
