/*
 * Copyright (C) 2019 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebDataListSuggestionsDropdownGtk.h"

#if ENABLE(DATALIST_ELEMENT)

#include "WebPageProxy.h"
#include <WebCore/DataListSuggestionInformation.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/IntPoint.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

static void firstTimeItemSelectedCallback(GtkTreeSelection* selection, GtkWidget* treeView)
{
    if (gtk_widget_is_focus(treeView))
        gtk_tree_selection_unselect_all(selection);
    g_signal_handlers_disconnect_by_func(selection, reinterpret_cast<gpointer>(firstTimeItemSelectedCallback), treeView);
}

WebDataListSuggestionsDropdownGtk::WebDataListSuggestionsDropdownGtk(GtkWidget* webView, WebPageProxy& page)
    : WebDataListSuggestionsDropdown(page)
    , m_webView(webView)
{
    GRefPtr<GtkListStore> model = adoptGRef(gtk_list_store_new(1, G_TYPE_STRING));
    m_treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model.get()));
    auto* treeView = GTK_TREE_VIEW(m_treeView);
    g_signal_connect(treeView, "row-activated", G_CALLBACK(treeViewRowActivatedCallback), this);
    gtk_tree_view_set_enable_search(treeView, FALSE);
    gtk_tree_view_set_activate_on_single_click(treeView, TRUE);
    gtk_tree_view_set_hover_selection(treeView, TRUE);
    gtk_tree_view_set_headers_visible(treeView, FALSE);
    gtk_tree_view_insert_column_with_attributes(treeView, 0, nullptr, gtk_cell_renderer_text_new(), "text", 0, nullptr);

    auto* selection = gtk_tree_view_get_selection(treeView);
    // The first time it's shown the first item is always selected, so we connect to selection changed to unselect it.
    g_signal_connect_object(selection, "changed", G_CALLBACK(firstTimeItemSelectedCallback), treeView, static_cast<GConnectFlags>(0));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    auto* swindow = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swindow), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(swindow), m_treeView);
    gtk_widget_show(m_treeView);

    m_popup = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_type_hint(GTK_WINDOW(m_popup), GDK_WINDOW_TYPE_HINT_COMBO);
    gtk_window_set_resizable(GTK_WINDOW(m_popup), FALSE);
    gtk_container_add(GTK_CONTAINER(m_popup), swindow);
    gtk_widget_show(swindow);

    g_signal_connect_object(m_webView, "focus-out-event", G_CALLBACK(gtk_widget_hide), m_popup, G_CONNECT_SWAPPED);
    g_signal_connect_object(m_webView, "unmap-event", G_CALLBACK(gtk_widget_hide), m_popup, G_CONNECT_SWAPPED);

#if ENABLE(DEVELOPER_MODE)
    g_object_set_data(G_OBJECT(m_webView), "wk-datalist-popup", m_popup);
#endif
}

WebDataListSuggestionsDropdownGtk::~WebDataListSuggestionsDropdownGtk()
{
    gtk_window_set_transient_for(GTK_WINDOW(m_popup), nullptr);
    gtk_window_set_attached_to(GTK_WINDOW(m_popup), nullptr);
#if ENABLE(DEVELOPER_MODE)
    g_object_set_data(G_OBJECT(m_webView), "wk-datalist-popup", nullptr);
#endif
    gtk_widget_destroy(m_popup);
}

void WebDataListSuggestionsDropdownGtk::treeViewRowActivatedCallback(GtkTreeView* treeView, GtkTreePath* path, GtkTreeViewColumn*, WebDataListSuggestionsDropdownGtk* menu)
{
    auto* model = gtk_tree_view_get_model(treeView);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, path);
    GUniqueOutPtr<char> item;
    gtk_tree_model_get(model, &iter, 0, &item.outPtr(), -1);

    menu->didSelectOption(String::fromUTF8(item.get()));
}

void WebDataListSuggestionsDropdownGtk::didSelectOption(const String& selectedOption)
{
    if (!m_page)
        return;

    m_page->didSelectOption(selectedOption);
    close();
}

void WebDataListSuggestionsDropdownGtk::show(WebCore::DataListSuggestionInformation&& information)
{
    auto* model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(m_treeView)));
    gtk_list_store_clear(model);
    for (const auto& suggestion : information.suggestions) {
        GtkTreeIter iter;
        gtk_list_store_append(model, &iter);
        gtk_list_store_set(model, &iter, 0, suggestion.utf8().data(), -1);
    }

    GtkRequisition treeViewRequisition;
    gtk_widget_get_preferred_size(m_treeView, &treeViewRequisition, nullptr);
    auto* column = gtk_tree_view_get_column(GTK_TREE_VIEW(m_treeView), 0);
    gint itemHeight;
    gtk_tree_view_column_cell_get_size(column, nullptr, nullptr, nullptr, nullptr, &itemHeight);
    gint verticalSeparator;
    gtk_widget_style_get(m_treeView, "vertical-separator", &verticalSeparator, nullptr);
    itemHeight += verticalSeparator;
    if (!itemHeight)
        return;

    auto* display = gtk_widget_get_display(m_webView);
    auto* monitor = gdk_display_get_monitor_at_window(display, gtk_widget_get_window(m_webView));
    GdkRectangle area;
    gdk_monitor_get_workarea(monitor, &area);
    int width = std::min(information.elementRect.width(), area.width);
    size_t itemCount = std::min<size_t>(information.suggestions.size(), (area.height / 3) / itemHeight);

    auto* swindow = GTK_SCROLLED_WINDOW(gtk_bin_get_child(GTK_BIN(m_popup)));
    // Disable scrollbars when there's only one item to ensure the scrolled window doesn't take them into account when calculating its minimum size.
    gtk_scrolled_window_set_policy(swindow, GTK_POLICY_NEVER, itemCount > 1 ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER);
    gtk_widget_realize(m_treeView);
    gtk_tree_view_columns_autosize(GTK_TREE_VIEW(m_treeView));
    gtk_scrolled_window_set_min_content_width(swindow, width);
    gtk_widget_set_size_request(m_popup, width, -1);
    gtk_scrolled_window_set_min_content_height(swindow, itemCount * itemHeight);

    GtkRequisition menuRequisition;
    gtk_widget_get_preferred_size(m_popup, &menuRequisition, nullptr);
    WebCore::IntPoint menuPosition = convertWidgetPointToScreenPoint(m_webView, information.elementRect.location());
    // FIXME: We can't ensure the menu will be on screen in Wayland.
    // https://blog.gtk.org/2016/07/15/future-of-relative-window-positioning/
    // https://gitlab.gnome.org/GNOME/gtk/issues/997
    if (menuPosition.x() + menuRequisition.width > area.x + area.width)
        menuPosition.setX(area.x + area.width - menuRequisition.width);

    if (menuPosition.y() + information.elementRect.height() + menuRequisition.height <= area.y + area.height
        || menuPosition.y() - area.y < (area.y + area.height) - (menuPosition.y() + information.elementRect.height()))
        menuPosition.move(0, information.elementRect.height());
    else
        menuPosition.move(0, -menuRequisition.height);
    gtk_window_move(GTK_WINDOW(m_popup), menuPosition.x(), menuPosition.y());

    auto* toplevel = gtk_widget_get_toplevel(m_webView);
    if (GTK_IS_WINDOW(toplevel)) {
        gtk_window_set_transient_for(GTK_WINDOW(m_popup), GTK_WINDOW(toplevel));
        gtk_window_group_add_window(gtk_window_get_group(GTK_WINDOW(toplevel)), GTK_WINDOW(m_popup));
    }
    gtk_window_set_attached_to(GTK_WINDOW(m_popup), m_webView);
    gtk_window_set_screen(GTK_WINDOW(m_popup), gtk_widget_get_screen(m_webView));

    gtk_widget_show(m_popup);
}

void WebDataListSuggestionsDropdownGtk::handleKeydownWithIdentifier(const String& key)
{
    auto* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(m_treeView));
    GtkTreeModel* model;
    GtkTreeIter iter;
    bool hasSelection = gtk_tree_selection_get_selected(selection, &model, &iter);
    if (key == "Enter") {
        if (hasSelection) {
            GUniqueOutPtr<char> item;
            gtk_tree_model_get(model, &iter, 0, &item.outPtr(), -1);
            m_page->didSelectOption(String::fromUTF8(item.get()));
        }
        close();
        return;
    }

    if (key == "Up") {
        if ((hasSelection && gtk_tree_model_iter_previous(model, &iter)) || gtk_tree_model_iter_nth_child(model, &iter, nullptr, gtk_tree_model_iter_n_children(model, nullptr) - 1))
            gtk_tree_selection_select_iter(selection, &iter);
        else
            return;
    } else if (key == "Down") {
        if ((hasSelection && gtk_tree_model_iter_next(model, &iter)) || gtk_tree_model_get_iter_first(model, &iter))
            gtk_tree_selection_select_iter(selection, &iter);
        else
            return;
    }

    GUniquePtr<GtkTreePath> path(gtk_tree_model_get_path(model, &iter));
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(m_treeView), path.get(), nullptr, FALSE, 0, 0);
}

void WebDataListSuggestionsDropdownGtk::close()
{
    gtk_widget_hide(m_popup);
}

} // namespace WebKit

#endif // ENABLE(DATALIST_ELEMENT)
