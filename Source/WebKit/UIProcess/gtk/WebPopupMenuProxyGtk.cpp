/*
 * Copyright (C) 2011 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
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
#include "WebPopupMenuProxyGtk.h"

#include "NativeWebMouseEvent.h"
#include "WebPopupItem.h"
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <WebCore/IntRect.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace WebKit {
using namespace WebCore;

enum Columns {
    Label,
    Tooltip,
    IsGroup,
    IsSelected,
    IsEnabled,
    Index,

    Count
};

WebPopupMenuProxyGtk::WebPopupMenuProxyGtk(GtkWidget* webView, WebPopupMenuProxy::Client& client)
    : WebPopupMenuProxy(client)
    , m_webView(webView)
{
}

WebPopupMenuProxyGtk::~WebPopupMenuProxyGtk()
{
    cancelTracking();
}

void WebPopupMenuProxyGtk::selectItem(unsigned itemIndex)
{
    if (m_client)
        m_client->setTextFromItemForPopupMenu(this, itemIndex);
    m_selectedItem = itemIndex;
}

void WebPopupMenuProxyGtk::activateItem(Optional<unsigned> itemIndex)
{
    if (m_client)
        m_client->valueChangedForPopupMenu(this, itemIndex.valueOr(m_selectedItem.valueOr(-1)));
}

bool WebPopupMenuProxyGtk::activateItemAtPath(GtkTreePath* path)
{
    auto* model = gtk_tree_view_get_model(GTK_TREE_VIEW(m_treeView));
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, path);
    gboolean isGroup, isEnabled;
    guint index;
    gtk_tree_model_get(model, &iter, Columns::IsGroup, &isGroup, Columns::IsEnabled, &isEnabled, Columns::Index, &index, -1);
    if (isGroup || !isEnabled)
        return false;

    activateItem(index);
    hidePopupMenu();
    return true;
}

bool WebPopupMenuProxyGtk::handleKeyPress(unsigned keyval, uint32_t timestamp)
{
    if (!m_popup)
        return false;

    if (keyval == GDK_KEY_Escape) {
        hidePopupMenu();
        return true;
    }

    return typeAheadFind(keyval, timestamp);
}

void WebPopupMenuProxyGtk::activateSelectedItem()
{
    if (!m_popup)
        return;

    GtkTreeModel* model;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(m_treeView)), &model, &iter))
        return;

    GUniquePtr<GtkTreePath> path(gtk_tree_model_get_path(model, &iter));
    activateItemAtPath(path.get());
}

#if !USE(GTK4)
gboolean WebPopupMenuProxyGtk::treeViewButtonReleaseEventCallback(GtkWidget* treeView, GdkEvent* event, WebPopupMenuProxyGtk* popupMenu)
{
    guint button;
    gdk_event_get_button(event, &button);
    if (button != GDK_BUTTON_PRIMARY)
        return FALSE;

    double x, y;
    gdk_event_get_coords(event, &x, &y);
    GUniqueOutPtr<GtkTreePath> path;
    if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeView), x, y, &path.outPtr(), nullptr, nullptr, nullptr))
        return FALSE;

    return popupMenu->activateItemAtPath(path.get());
}

gboolean WebPopupMenuProxyGtk::buttonPressEventCallback(GtkWidget* widget, GdkEventButton* event, WebPopupMenuProxyGtk* popupMenu)
{
    if (!popupMenu->m_device)
        return FALSE;

    popupMenu->hidePopupMenu();
    return TRUE;
}

gboolean WebPopupMenuProxyGtk::keyPressEventCallback(GtkWidget* widget, GdkEvent* event, WebPopupMenuProxyGtk* popupMenu)
{
    if (!popupMenu->m_device)
        return FALSE;

    guint keyval;
    gdk_event_get_keyval(event, &keyval);
    if (popupMenu->handleKeyPress(keyval, gdk_event_get_time(event)))
        return TRUE;

    // Forward the event to the tree view.
    gtk_widget_event(popupMenu->m_treeView, event);
    return TRUE;
}
#endif

void WebPopupMenuProxyGtk::createPopupMenu(const Vector<WebPopupItem>& items, int32_t selectedIndex)
{
    ASSERT(!m_popup);

    GRefPtr<GtkTreeStore> model = adoptGRef(gtk_tree_store_new(Columns::Count, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_UINT));
    GtkTreeIter parentIter;
    unsigned index = 0;
    m_paths.reserveInitialCapacity(items.size());
    for (const auto& item : items) {
        if (item.m_isLabel) {
            gtk_tree_store_insert_with_values(model.get(), &parentIter, nullptr, -1,
                Columns::Label, item.m_text.stripWhiteSpace().utf8().data(),
                Columns::IsGroup, TRUE,
                Columns::IsEnabled, TRUE,
                -1);
            // We never need the path for group labels.
            m_paths.uncheckedAppend(nullptr);
        } else {
            GtkTreeIter iter;
            bool isSelected = selectedIndex && static_cast<unsigned>(selectedIndex) == index;
            gtk_tree_store_insert_with_values(model.get(), &iter, item.m_text.startsWith("    ") ? &parentIter : nullptr, -1,
                Columns::Label, item.m_text.stripWhiteSpace().utf8().data(),
                Columns::Tooltip, item.m_toolTip.isEmpty() ? nullptr : item.m_toolTip.utf8().data(),
                Columns::IsGroup, FALSE,
                Columns::IsSelected, isSelected,
                Columns::IsEnabled, item.m_isEnabled,
                Columns::Index, index,
                -1);
            if (isSelected) {
                ASSERT(!m_selectedItem);
                m_selectedItem = index;
            }
            m_paths.uncheckedAppend(GUniquePtr<GtkTreePath>(gtk_tree_model_get_path(GTK_TREE_MODEL(model.get()), &iter)));
        }
        index++;
    }

    m_treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model.get()));
    auto* treeView = GTK_TREE_VIEW(m_treeView);
    g_signal_connect_swapped(treeView, "row-activated", G_CALLBACK(+[](WebPopupMenuProxyGtk* popupMenu, GtkTreePath* path) {
        popupMenu->activateItemAtPath(path);
    }), this);

#if !USE(GTK4)
    g_signal_connect_after(treeView, "button-release-event", G_CALLBACK(treeViewButtonReleaseEventCallback), this);
#endif
    gtk_tree_view_set_tooltip_column(treeView, Columns::Tooltip);
    gtk_tree_view_set_show_expanders(treeView, FALSE);
    gtk_tree_view_set_level_indentation(treeView, 12);
    gtk_tree_view_set_enable_search(treeView, FALSE);
    gtk_tree_view_set_activate_on_single_click(treeView, TRUE);
    gtk_tree_view_set_hover_selection(treeView, TRUE);
    gtk_tree_view_set_headers_visible(treeView, FALSE);
    gtk_tree_view_insert_column_with_data_func(treeView, 0, nullptr, gtk_cell_renderer_text_new(), [](GtkTreeViewColumn*, GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter, gpointer) {
        GUniqueOutPtr<char> label;
        gboolean isGroup, isEnabled;
        gtk_tree_model_get(model, iter, Columns::Label, &label.outPtr(), Columns::IsGroup, &isGroup, Columns::IsEnabled, &isEnabled, -1);
        if (isGroup) {
            GUniquePtr<char> markup(g_strdup_printf("<b>%s</b>", label.get()));
            g_object_set(renderer, "markup", markup.get(), nullptr);
        } else
            g_object_set(renderer, "text", label.get(), "sensitive", isEnabled, nullptr);
    }, nullptr, nullptr);
    gtk_tree_view_expand_all(treeView);

    auto* selection = gtk_tree_view_get_selection(treeView);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_set_select_function(selection, [](GtkTreeSelection*, GtkTreeModel* model, GtkTreePath* path, gboolean selected, gpointer) -> gboolean {
        GtkTreeIter iter;
        gtk_tree_model_get_iter(model, &iter, path);
        gboolean isGroup, isEnabled;
        gtk_tree_model_get(model, &iter, Columns::IsGroup, &isGroup, Columns::IsEnabled, &isEnabled, -1);
        return !isGroup && isEnabled;
    }, nullptr, nullptr);

    auto* swindow = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
#if USE(GTK4)
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(swindow), m_treeView);
#else
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swindow), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(swindow), m_treeView);
    gtk_widget_show(m_treeView);
#endif

#if USE(GTK4)
    m_popup = gtk_popover_new();
    g_signal_connect_swapped(m_popup, "closed", G_CALLBACK(+[](WebPopupMenuProxyGtk* popupMenu) {
        popupMenu->hidePopupMenu();
    }), this);
    gtk_popover_set_has_arrow(GTK_POPOVER(m_popup), FALSE);
    gtk_popover_set_position(GTK_POPOVER(m_popup), GTK_POS_BOTTOM);
    gtk_popover_set_child(GTK_POPOVER(m_popup), swindow);
    gtk_widget_set_parent(m_popup, m_webView);

    auto* controller = gtk_event_controller_key_new();
    g_signal_connect_swapped(controller, "key-pressed", G_CALLBACK(+[](WebPopupMenuProxyGtk* popupMenu, unsigned keyval, unsigned, GdkModifierType, GtkEventController* controller) {
        auto* event = gtk_event_controller_get_current_event(controller);
        if (popupMenu->typeAheadFind(keyval, gdk_event_get_time(event)))
            return GDK_EVENT_STOP;
        return gtk_event_controller_key_forward(GTK_EVENT_CONTROLLER_KEY(controller), popupMenu->m_treeView);
    }), this);
    gtk_widget_add_controller(m_popup, controller);
#else
    m_popup = gtk_window_new(GTK_WINDOW_POPUP);
    g_signal_connect(m_popup, "button-press-event", G_CALLBACK(buttonPressEventCallback), this);
    g_signal_connect(m_popup, "key-press-event", G_CALLBACK(keyPressEventCallback), this);
    gtk_window_set_type_hint(GTK_WINDOW(m_popup), GDK_WINDOW_TYPE_HINT_COMBO);
    gtk_window_set_resizable(GTK_WINDOW(m_popup), FALSE);
    gtk_container_add(GTK_CONTAINER(m_popup), swindow);
    gtk_widget_show(swindow);
#endif
}

void WebPopupMenuProxyGtk::show()
{
    if (m_selectedItem) {
        auto& selectedPath = m_paths[m_selectedItem.value()];
        ASSERT(selectedPath);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(m_treeView), selectedPath.get(), nullptr, TRUE, 0.5, 0);
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(m_treeView), selectedPath.get(), nullptr, FALSE);
    }
    gtk_widget_grab_focus(m_treeView);
    gtk_widget_show(m_popup);
}

void WebPopupMenuProxyGtk::showPopupMenu(const IntRect& rect, TextDirection, double /* pageScaleFactor */, const Vector<WebPopupItem>& items, const PlatformPopupMenuData&, int32_t selectedIndex)
{
    createPopupMenu(items, selectedIndex);
    ASSERT(m_popup);

    GtkRequisition treeViewRequisition;
    gtk_widget_get_preferred_size(m_treeView, &treeViewRequisition, nullptr);
    auto* column = gtk_tree_view_get_column(GTK_TREE_VIEW(m_treeView), Columns::Label);
    gint itemHeight;
    gtk_tree_view_column_cell_get_size(column, nullptr, nullptr, nullptr, nullptr, &itemHeight);
#if !USE(GTK4)
    gint verticalSeparator;
    gtk_widget_style_get(m_treeView, "vertical-separator", &verticalSeparator, nullptr);
    itemHeight += verticalSeparator;
#endif
    if (!itemHeight)
        return;

#if !USE(GTK4)
    auto* toplevel = gtk_widget_get_toplevel(m_webView);
    if (GTK_IS_WINDOW(toplevel)) {
        gtk_window_set_transient_for(GTK_WINDOW(m_popup), GTK_WINDOW(toplevel));
        gtk_window_group_add_window(gtk_window_get_group(GTK_WINDOW(toplevel)), GTK_WINDOW(m_popup));
    }
    gtk_window_set_attached_to(GTK_WINDOW(m_popup), m_webView);
    gtk_window_set_screen(GTK_WINDOW(m_popup), gtk_widget_get_screen(m_webView));
#endif

    auto* display = gtk_widget_get_display(m_webView);
#if USE(GTK4)
    auto* monitor = gdk_display_get_monitor_at_surface(display, gtk_native_get_surface(gtk_widget_get_native(m_webView)));
#else
    auto* monitor = gdk_display_get_monitor_at_window(display, gtk_widget_get_window(m_webView));
#endif
    GdkRectangle area;
    gdk_monitor_get_workarea(monitor, &area);
    int width = std::min(rect.width(), area.width);
    size_t itemCount = std::min<size_t>(items.size(), (area.height / 3) / itemHeight);

#if USE(GTK4)
    auto* swindow = GTK_SCROLLED_WINDOW(gtk_popover_get_child(GTK_POPOVER(m_popup)));
#else
    auto* swindow = GTK_SCROLLED_WINDOW(gtk_bin_get_child(GTK_BIN(m_popup)));
#endif
    // Disable scrollbars when there's only one item to ensure the scrolled window doesn't take them into account when calculating its minimum size.
    gtk_scrolled_window_set_policy(swindow, GTK_POLICY_NEVER, itemCount > 1 ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER);
    gtk_widget_realize(m_treeView);
    gtk_tree_view_columns_autosize(GTK_TREE_VIEW(m_treeView));
    gtk_scrolled_window_set_min_content_width(swindow, width);
    gtk_widget_set_size_request(m_popup, width, -1);
    gtk_scrolled_window_set_min_content_height(swindow, itemCount * itemHeight);

#if USE(GTK4)
    GdkRectangle windowRect = { rect.x(), rect.y(), rect.width(), rect.height() };
    gtk_popover_set_pointing_to(GTK_POPOVER(m_popup), &windowRect);
    show();
#else
#if GTK_CHECK_VERSION(3, 24, 0)
    GdkRectangle windowRect = { rect.x(), rect.y(), rect.width(), rect.height() };
    gtk_widget_translate_coordinates(m_webView, toplevel, windowRect.x, windowRect.y, &windowRect.x, &windowRect.y);
    gdk_window_move_to_rect(gtk_widget_get_window(m_popup), &windowRect, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST,
        static_cast<GdkAnchorHints>(GDK_ANCHOR_FLIP | GDK_ANCHOR_SLIDE | GDK_ANCHOR_RESIZE), 0, 0);
#else
    GtkRequisition menuRequisition;
    gtk_widget_get_preferred_size(m_popup, &menuRequisition, nullptr);

    IntPoint menuPosition = convertWidgetPointToScreenPoint(m_webView, rect.location());
    if (menuPosition.x() + menuRequisition.width > area.x + area.width)
        menuPosition.setX(area.x + area.width - menuRequisition.width);

    if (menuPosition.y() + rect.height() + menuRequisition.height <= area.y + area.height
        || menuPosition.y() - area.y < (area.y + area.height) - (menuPosition.y() + rect.height()))
        menuPosition.move(0, rect.height());
    else
        menuPosition.move(0, -menuRequisition.height);
    gtk_window_move(GTK_WINDOW(m_popup), menuPosition.x(), menuPosition.y());
#endif

    const GdkEvent* event = m_client->currentlyProcessedMouseDownEvent() ? m_client->currentlyProcessedMouseDownEvent()->nativeEvent() : nullptr;
    m_device = event ? gdk_event_get_device(event) : nullptr;
    if (!m_device)
        m_device = gtk_get_current_event_device();
    if (m_device && gdk_device_get_display(m_device) != display)
        m_device = nullptr;
    if (!m_device)
        m_device = gdk_seat_get_pointer(gdk_display_get_default_seat(display));
    ASSERT(m_device);
    if (gdk_device_get_source(m_device) == GDK_SOURCE_KEYBOARD)
        m_device = gdk_device_get_associated_device(m_device);

    gtk_grab_add(m_popup);
    auto grabResult = gdk_seat_grab(gdk_device_get_seat(m_device), gtk_widget_get_window(m_popup), GDK_SEAT_CAPABILITY_ALL, TRUE, nullptr, nullptr, [](GdkSeat*, GdkWindow*, gpointer userData) {
        static_cast<WebPopupMenuProxyGtk*>(userData)->show();
    }, this);

    // PopupMenu can fail to open when there is no mouse grab.
    // Ensure WebCore does not go into some pesky state.
    if (grabResult != GDK_GRAB_SUCCESS) {
       m_client->failedToShowPopupMenu();
       return;
    }
#endif
}

void WebPopupMenuProxyGtk::hidePopupMenu()
{
    if (!m_popup)
        return;

#if !USE(GTK4)
    if (m_device) {
        gdk_seat_ungrab(gdk_device_get_seat(m_device));
        gtk_grab_remove(m_popup);
        gtk_window_set_transient_for(GTK_WINDOW(m_popup), nullptr);
        gtk_window_set_attached_to(GTK_WINDOW(m_popup), nullptr);
        m_device = nullptr;
    }
#endif

    activateItem(WTF::nullopt);

    if (m_currentSearchString) {
        g_string_free(m_currentSearchString, TRUE);
        m_currentSearchString = nullptr;
    }

#if USE(GTK4)
    g_clear_pointer(&m_popup, gtk_widget_unparent);
#else
    gtk_widget_destroy(m_popup);
    m_popup = nullptr;
#endif
}

void WebPopupMenuProxyGtk::cancelTracking()
{
    if (!m_popup)
        return;

    g_signal_handlers_disconnect_matched(m_popup, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    hidePopupMenu();
}

Optional<unsigned> WebPopupMenuProxyGtk::typeAheadFindIndex(unsigned keyval, uint32_t time)
{
    gunichar keychar = gdk_keyval_to_unicode(keyval);
    if (!g_unichar_isprint(keychar))
        return WTF::nullopt;

    if (time < m_previousKeyEventTime)
        return WTF::nullopt;

    static const uint32_t typeaheadTimeoutMs = 1000;
    if (time - m_previousKeyEventTime > typeaheadTimeoutMs) {
        if (m_currentSearchString)
            g_string_truncate(m_currentSearchString, 0);
    }
    m_previousKeyEventTime = time;

    if (!m_currentSearchString)
        m_currentSearchString = g_string_new(nullptr);
    g_string_append_unichar(m_currentSearchString, keychar);

    int prefixLength = -1;
    if (keychar == m_repeatingCharacter)
        prefixLength = 1;
    else
        m_repeatingCharacter = m_currentSearchString->len == 1 ? keychar : '\0';

    GtkTreeModel* model;
    GtkTreeIter iter;
    guint selectedIndex = 0;
    if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(m_treeView)), &model, &iter))
        gtk_tree_model_get(model, &iter, Columns::Index, &selectedIndex, -1);

    unsigned index = selectedIndex;
    if (m_repeatingCharacter != '\0')
        index++;
    auto itemCount = m_paths.size();
    index %= itemCount;

    GUniquePtr<char> normalizedPrefix(g_utf8_normalize(m_currentSearchString->str, prefixLength, G_NORMALIZE_ALL));
    GUniquePtr<char> prefix(normalizedPrefix ? g_utf8_casefold(normalizedPrefix.get(), -1) : nullptr);
    if (!prefix)
        return WTF::nullopt;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(m_treeView));
    for (unsigned i = 0; i < itemCount; i++, index = (index + 1) % itemCount) {
        auto& path = m_paths[index];
        if (!path || !gtk_tree_model_get_iter(model, &iter, path.get()))
            continue;

        GUniqueOutPtr<char> label;
        gboolean isEnabled;
        gtk_tree_model_get(model, &iter, Columns::Label, &label.outPtr(), Columns::IsEnabled, &isEnabled, -1);
        if (!isEnabled)
            continue;

        GUniquePtr<char> normalizedText(g_utf8_normalize(label.get(), -1, G_NORMALIZE_ALL));
        GUniquePtr<char> text(normalizedText ? g_utf8_casefold(normalizedText.get(), -1) : nullptr);
        if (!text)
            continue;

        if (!strncmp(prefix.get(), text.get(), strlen(prefix.get())))
            return index;
    }

    return WTF::nullopt;
}

bool WebPopupMenuProxyGtk::typeAheadFind(unsigned keyval, uint32_t timestamp)
{
    auto searchIndex = typeAheadFindIndex(keyval, timestamp);
    if (!searchIndex)
        return false;

    auto& path = m_paths[searchIndex.value()];
    ASSERT(path);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(m_treeView), path.get(), nullptr, TRUE, 0.5, 0);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(m_treeView), path.get(), nullptr, FALSE);
    selectItem(searchIndex.value());

    return true;
}

} // namespace WebKit
