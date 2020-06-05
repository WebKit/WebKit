/*
 * Copyright (C) 2013 Igalia S.L.
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

#include "cmakeconfig.h"
#include "BrowserSearchBar.h"

#if !GTK_CHECK_VERSION(3, 98, 0)

static const char *searchEntryFailedStyle = "GtkEntry#searchEntry {background-color: #ff6666;}";

struct _BrowserSearchBar {
    GtkToolbar parent;

    WebKitWebView *webView;
    GtkWidget *entry;
    GtkCssProvider *cssProvider;
    GtkWidget *prevButton;
    GtkWidget *nextButton;
    GtkWidget *optionsMenu;
    GtkWidget *caseCheckButton;
    GtkWidget *begginigWordCheckButton;
    GtkWidget *capitalAsBegginigWordCheckButton;
};

G_DEFINE_TYPE(BrowserSearchBar, browser_search_bar, GTK_TYPE_TOOLBAR)

static void setFailedStyleForEntry(BrowserSearchBar *searchBar, gboolean failedSearch)
{
    gtk_css_provider_load_from_data(searchBar->cssProvider, failedSearch ? searchEntryFailedStyle : "", -1, NULL);
}

static void doSearch(BrowserSearchBar *searchBar)
{
    GtkEntry *entry = GTK_ENTRY(searchBar->entry);

    if (!gtk_entry_get_text_length(entry)) {
        webkit_find_controller_search_finish(webkit_web_view_get_find_controller(searchBar->webView));
        gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
        setFailedStyleForEntry(searchBar, FALSE);
        return;
    }

    if (!gtk_entry_get_icon_name(entry, GTK_ENTRY_ICON_SECONDARY))
        gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "edit-clear");

    WebKitFindOptions options = WEBKIT_FIND_OPTIONS_WRAP_AROUND;
    if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(searchBar->caseCheckButton)))
        options |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(searchBar->begginigWordCheckButton)))
        options |= WEBKIT_FIND_OPTIONS_AT_WORD_STARTS;
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(searchBar->capitalAsBegginigWordCheckButton)))
        options |= WEBKIT_FIND_OPTIONS_TREAT_MEDIAL_CAPITAL_AS_WORD_START;

    const gchar *text = gtk_entry_get_text(entry);
    webkit_find_controller_search(webkit_web_view_get_find_controller(searchBar->webView), text, options, G_MAXUINT);
}

static void searchNext(BrowserSearchBar *searchBar)
{
    webkit_find_controller_search_next(webkit_web_view_get_find_controller(searchBar->webView));
}

static void searchPrevious(BrowserSearchBar *searchBar)
{
    webkit_find_controller_search_previous(webkit_web_view_get_find_controller(searchBar->webView));
}

static void searchCloseButtonClickedCallback(BrowserSearchBar *searchBar)
{
    browser_search_bar_close(searchBar);
}

static void searchEntryMenuIconPressedCallback(BrowserSearchBar *searchBar, GtkEntryIconPosition iconPosition, GdkEvent *event)
{
    if (iconPosition == GTK_ENTRY_ICON_PRIMARY)
        gtk_menu_popup_at_pointer(GTK_MENU(searchBar->optionsMenu), event);
}

static void searchEntryClearIconReleasedCallback(BrowserSearchBar *searchBar, GtkEntryIconPosition iconPosition)
{
    if (iconPosition == GTK_ENTRY_ICON_SECONDARY)
        gtk_entry_set_text(GTK_ENTRY(searchBar->entry), "");
}

static void searchEntryChangedCallback(GtkEntry *entry, BrowserSearchBar *searchBar)
{
    doSearch(searchBar);
}

static void searchEntryActivatedCallback(BrowserSearchBar *searchBar)
{
    searchNext(searchBar);
}

static void searchPreviousButtonCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    searchPrevious(BROWSER_SEARCH_BAR(userData));
}

static void searchNextButtonCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    searchNext(BROWSER_SEARCH_BAR(userData));
}

static void searchMenuCheckButtonToggledCallback(BrowserSearchBar *searchBar)
{
    doSearch(searchBar);
}

static void findControllerFailedToFindTextCallback(BrowserSearchBar *searchBar)
{
    setFailedStyleForEntry(searchBar, TRUE);
}

static void findControllerFoundTextCallback(BrowserSearchBar *searchBar)
{
    setFailedStyleForEntry(searchBar, FALSE);
}

static const GActionEntry actions[] = {
    { "next", searchNextButtonCallback, NULL, NULL, NULL, { 0 } },
    { "previous", searchPreviousButtonCallback, NULL, NULL, NULL, { 0 } },
};

static void browser_search_bar_init(BrowserSearchBar *searchBar)
{
    GSimpleActionGroup *actionGroup = g_simple_action_group_new();
    g_action_map_add_action_entries(G_ACTION_MAP(actionGroup), actions, G_N_ELEMENTS(actions), searchBar);
    gtk_widget_insert_action_group(GTK_WIDGET(searchBar), "find", G_ACTION_GROUP(actionGroup));
    g_object_unref(actionGroup);

    gtk_widget_set_hexpand(GTK_WIDGET(searchBar), TRUE);

    GtkToolItem *toolItem = gtk_tool_item_new();
    gtk_tool_item_set_expand(toolItem, TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(searchBar), toolItem, 0);

    GtkBox *hBox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_set_homogeneous(hBox, TRUE);
    gtk_container_add(GTK_CONTAINER(toolItem), GTK_WIDGET(hBox));

    gtk_box_pack_start(hBox, gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), TRUE, TRUE, 0);

    searchBar->entry = gtk_entry_new();
    gtk_widget_set_name(searchBar->entry, "searchEntry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(searchBar->entry), "Search");
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(searchBar->entry), GTK_ENTRY_ICON_PRIMARY, "edit-find");
    gtk_box_pack_start(hBox, searchBar->entry, TRUE, TRUE, 0);

    searchBar->cssProvider = gtk_css_provider_new();
    gtk_style_context_add_provider(gtk_widget_get_style_context(searchBar->entry), GTK_STYLE_PROVIDER(searchBar->cssProvider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkBox *hBoxButtons = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_box_pack_start(hBox, GTK_WIDGET(hBoxButtons), TRUE, TRUE, 0);

    searchBar->prevButton = gtk_button_new_from_icon_name("go-up", GTK_ICON_SIZE_SMALL_TOOLBAR);
    GtkButton *button = GTK_BUTTON(searchBar->prevButton);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), "find.previous");
    gtk_button_set_relief(button, GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(searchBar->prevButton, FALSE);
    gtk_box_pack_start(hBoxButtons, searchBar->prevButton, FALSE, FALSE, 0);

    searchBar->nextButton = gtk_button_new_from_icon_name("go-down", GTK_ICON_SIZE_SMALL_TOOLBAR);
    button = GTK_BUTTON(searchBar->nextButton);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), "find.next");
    gtk_button_set_relief(button, GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(searchBar->nextButton, FALSE);
    gtk_box_pack_start(hBoxButtons, searchBar->nextButton, FALSE, FALSE, 0);

    GtkWidget *closeButton = gtk_button_new_from_icon_name("window-close", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_button_set_relief(GTK_BUTTON(closeButton), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(closeButton, FALSE);
    gtk_box_pack_end(hBoxButtons, closeButton, FALSE, FALSE, 0);

    searchBar->optionsMenu = g_object_ref_sink(gtk_menu_new());

    searchBar->caseCheckButton = gtk_check_menu_item_new_with_mnemonic("Ca_se sensitive");
    gtk_container_add(GTK_CONTAINER(searchBar->optionsMenu), searchBar->caseCheckButton);

    searchBar->begginigWordCheckButton = gtk_check_menu_item_new_with_mnemonic("Only at the _beginning of words");
    gtk_container_add(GTK_CONTAINER(searchBar->optionsMenu), searchBar->begginigWordCheckButton);

    searchBar->capitalAsBegginigWordCheckButton = gtk_check_menu_item_new_with_mnemonic("Capital _always as beginning of word");
    gtk_container_add(GTK_CONTAINER(searchBar->optionsMenu), searchBar->capitalAsBegginigWordCheckButton);

    g_signal_connect_swapped(closeButton, "clicked", G_CALLBACK(searchCloseButtonClickedCallback), searchBar);
    g_signal_connect_swapped(searchBar->entry, "icon-press", G_CALLBACK(searchEntryMenuIconPressedCallback), searchBar);
    g_signal_connect_swapped(searchBar->entry, "icon-release", G_CALLBACK(searchEntryClearIconReleasedCallback), searchBar);
    g_signal_connect_after(searchBar->entry, "changed", G_CALLBACK(searchEntryChangedCallback), searchBar);
    g_signal_connect_swapped(searchBar->entry, "activate", G_CALLBACK(searchEntryActivatedCallback), searchBar);
    g_signal_connect_swapped(searchBar->caseCheckButton, "toggled", G_CALLBACK(searchMenuCheckButtonToggledCallback), searchBar);
    g_signal_connect_swapped(searchBar->begginigWordCheckButton, "toggled", G_CALLBACK(searchMenuCheckButtonToggledCallback), searchBar);
    g_signal_connect_swapped(searchBar->capitalAsBegginigWordCheckButton, "toggled", G_CALLBACK(searchMenuCheckButtonToggledCallback), searchBar);

    gtk_widget_show_all(GTK_WIDGET(toolItem));
    gtk_widget_show_all(searchBar->optionsMenu);
}

static void browserSearchBarFinalize(GObject *gObject)
{
    BrowserSearchBar *searchBar = BROWSER_SEARCH_BAR(gObject);

    if (searchBar->webView) {
        g_object_unref(searchBar->webView);
        searchBar->webView = NULL;
    }

    if (searchBar->cssProvider) {
        g_object_unref(searchBar->cssProvider);
        searchBar->cssProvider = NULL;
    }

    if (searchBar->optionsMenu) {
        g_object_unref(searchBar->optionsMenu);
        searchBar->optionsMenu = NULL;
    }

    G_OBJECT_CLASS(browser_search_bar_parent_class)->finalize(gObject);
}

static void browser_search_bar_class_init(BrowserSearchBarClass *klass)
{
    GObjectClass *gObjectClass = G_OBJECT_CLASS(klass);

    gObjectClass->finalize = browserSearchBarFinalize;
}

GtkWidget *browser_search_bar_new(WebKitWebView *webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    GtkWidget *searchBar = GTK_WIDGET(g_object_new(BROWSER_TYPE_SEARCH_BAR, NULL));
    BROWSER_SEARCH_BAR(searchBar)->webView = g_object_ref(webView);

    WebKitFindController *controller = webkit_web_view_get_find_controller(webView);
    g_signal_connect_swapped(controller, "failed-to-find-text", G_CALLBACK(findControllerFailedToFindTextCallback), searchBar);
    g_signal_connect_swapped(controller, "found-text", G_CALLBACK(findControllerFoundTextCallback), searchBar);

    return searchBar;
}

void browser_search_bar_open(BrowserSearchBar *searchBar)
{
    g_return_if_fail(BROWSER_IS_SEARCH_BAR(searchBar));

    GtkEntry *entry = GTK_ENTRY(searchBar->entry);

    gtk_widget_show(GTK_WIDGET(searchBar));
    gtk_widget_grab_focus(GTK_WIDGET(entry));
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    if (gtk_entry_get_text_length(entry))
        doSearch(searchBar);
}

void browser_search_bar_close(BrowserSearchBar *searchBar)
{
    g_return_if_fail(BROWSER_IS_SEARCH_BAR(searchBar));

    gtk_widget_hide(GTK_WIDGET(searchBar));
    WebKitFindController *controller = webkit_web_view_get_find_controller(searchBar->webView);
    webkit_find_controller_search_finish(controller);
}

#endif
