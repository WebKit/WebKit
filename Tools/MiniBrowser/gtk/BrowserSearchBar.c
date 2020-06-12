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

struct _BrowserSearchBar {
    GtkSearchBar parent;

    WebKitWebView *webView;
    GtkWidget *entry;
    GtkWidget *prevButton;
    GtkWidget *nextButton;
    GMenu *optionsMenu;
    GtkWidget *caseCheckButton;
    GtkWidget *begginigWordCheckButton;
    GtkWidget *capitalAsBegginigWordCheckButton;
};

G_DEFINE_TYPE(BrowserSearchBar, browser_search_bar, GTK_TYPE_SEARCH_BAR)

static void setFailedStyleForEntry(BrowserSearchBar *searchBar, gboolean failedSearch)
{
    if (failedSearch)
        gtk_style_context_add_class(gtk_widget_get_style_context(searchBar->entry), "search-failed");
    else
        gtk_style_context_remove_class(gtk_widget_get_style_context(searchBar->entry), "search-failed");
}

static void doSearch(BrowserSearchBar *searchBar)
{
    GtkEntry *entry = GTK_ENTRY(searchBar->entry);
    if (!gtk_entry_get_text_length(entry)) {
        setFailedStyleForEntry(searchBar, FALSE);
        webkit_find_controller_search_finish(webkit_web_view_get_find_controller(searchBar->webView));
        return;
    }

    WebKitFindOptions options = WEBKIT_FIND_OPTIONS_WRAP_AROUND;
    GActionGroup *actionGroup = gtk_widget_get_action_group(GTK_WIDGET(searchBar), "find");
    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(actionGroup), "case-sensitive");
    GVariant *state = g_action_get_state(action);
    if (!g_variant_get_boolean(state))
        options |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;
    g_variant_unref(state);

    action = g_action_map_lookup_action(G_ACTION_MAP(actionGroup), "beginning-word");
    state = g_action_get_state(action);
    if (g_variant_get_boolean(state))
        options |= WEBKIT_FIND_OPTIONS_AT_WORD_STARTS;
    g_variant_unref(state);

    action = g_action_map_lookup_action(G_ACTION_MAP(actionGroup), "capital-as-beginning-word");
    state = g_action_get_state(action);
    if (g_variant_get_boolean(state))
        options |= WEBKIT_FIND_OPTIONS_TREAT_MEDIAL_CAPITAL_AS_WORD_START;
    g_variant_unref(state);

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

static void searchEntryMenuIconPressedCallback(BrowserSearchBar *searchBar, GtkEntryIconPosition iconPosition, GdkEvent *event)
{
    if (iconPosition != GTK_ENTRY_ICON_PRIMARY)
        return;

    GtkWidget *popover = gtk_popover_new_from_model(searchBar->entry, G_MENU_MODEL(searchBar->optionsMenu));
    GdkRectangle rect;
    gtk_entry_get_icon_area(GTK_ENTRY(searchBar->entry), GTK_ENTRY_ICON_PRIMARY, &rect);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
    gtk_popover_popup(GTK_POPOVER(popover));
}

static void searchEntryChangedCallback(GtkSearchEntry *entry, BrowserSearchBar *searchBar)
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

static void searchMenuCheckButtonToggledCallback(GSimpleAction *action, GVariant *state, gpointer userData)
{
    g_simple_action_set_state(action, state);
    doSearch(BROWSER_SEARCH_BAR(userData));
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
    { "case-sensitive", NULL, NULL, "false", searchMenuCheckButtonToggledCallback, { 0 } },
    { "beginning-word", NULL, NULL, "false", searchMenuCheckButtonToggledCallback, { 0 } },
    { "capital-as-beginning-word", NULL, NULL, "false", searchMenuCheckButtonToggledCallback, { 0 } }
};

static void browser_search_bar_init(BrowserSearchBar *searchBar)
{
    GSimpleActionGroup *actionGroup = g_simple_action_group_new();
    g_action_map_add_action_entries(G_ACTION_MAP(actionGroup), actions, G_N_ELEMENTS(actions), searchBar);
    gtk_widget_insert_action_group(GTK_WIDGET(searchBar), "find", G_ACTION_GROUP(actionGroup));
    g_object_unref(actionGroup);

    GtkWidget *hBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkStyleContext *styleContext = gtk_widget_get_style_context(hBox);
    gtk_style_context_add_class(styleContext, GTK_STYLE_CLASS_LINKED);
    gtk_style_context_add_class(styleContext, GTK_STYLE_CLASS_RAISED);

    searchBar->entry = gtk_search_entry_new();
    gtk_entry_set_icon_activatable(GTK_ENTRY(searchBar->entry), GTK_ENTRY_ICON_PRIMARY, TRUE);
    gtk_entry_set_icon_sensitive(GTK_ENTRY(searchBar->entry), GTK_ENTRY_ICON_PRIMARY, TRUE);
    gtk_entry_set_icon_tooltip_text(GTK_ENTRY(searchBar->entry), GTK_ENTRY_ICON_PRIMARY, "Search options");
    gtk_box_pack_start(GTK_BOX(hBox), searchBar->entry, TRUE, TRUE, 0);
    gtk_widget_show(searchBar->entry);

    GtkCssProvider *cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(cssProvider, ".search-failed { background-color: #ff6666; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(searchBar->entry), GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(cssProvider);

    searchBar->prevButton = gtk_button_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_MENU);
    GtkButton *button = GTK_BUTTON(searchBar->prevButton);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), "find.previous");
    gtk_widget_set_focus_on_click(searchBar->prevButton, FALSE);
    gtk_box_pack_start(GTK_BOX(hBox), searchBar->prevButton, FALSE, FALSE, 0);
    gtk_widget_show(searchBar->prevButton);

    searchBar->nextButton = gtk_button_new_from_icon_name("go-down-symbolic", GTK_ICON_SIZE_MENU);
    button = GTK_BUTTON(searchBar->nextButton);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), "find.next");
    gtk_widget_set_focus_on_click(searchBar->nextButton, FALSE);
    gtk_box_pack_start(GTK_BOX(hBox), searchBar->nextButton, FALSE, FALSE, 0);
    gtk_widget_show(searchBar->nextButton);

    gtk_container_add(GTK_CONTAINER(searchBar), hBox);
    gtk_widget_show(hBox);
    gtk_search_bar_connect_entry(GTK_SEARCH_BAR(searchBar), GTK_ENTRY(searchBar->entry));
    gtk_widget_show(GTK_WIDGET(searchBar));

    searchBar->optionsMenu = g_menu_new();
    g_menu_append(searchBar->optionsMenu, "Ca_se sensitive", "find.case-sensitive");
    g_menu_append(searchBar->optionsMenu, "Only at the _beginning of words", "find.beginning-word");
    g_menu_append(searchBar->optionsMenu, "Capital _always as beginning of word", "find.capital-as-beginning-word");

    g_signal_connect_swapped(searchBar->entry, "icon-press", G_CALLBACK(searchEntryMenuIconPressedCallback), searchBar);
    g_signal_connect_after(searchBar->entry, "search-changed", G_CALLBACK(searchEntryChangedCallback), searchBar);
    g_signal_connect_swapped(searchBar->entry, "activate", G_CALLBACK(searchEntryActivatedCallback), searchBar);
}

static void browserSearchBarFinalize(GObject *gObject)
{
    BrowserSearchBar *searchBar = BROWSER_SEARCH_BAR(gObject);

    if (searchBar->webView) {
        g_object_unref(searchBar->webView);
        searchBar->webView = NULL;
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
    gtk_search_bar_set_show_close_button(GTK_SEARCH_BAR(searchBar), TRUE);

    WebKitFindController *controller = webkit_web_view_get_find_controller(webView);
    g_signal_connect_swapped(controller, "failed-to-find-text", G_CALLBACK(findControllerFailedToFindTextCallback), searchBar);
    g_signal_connect_swapped(controller, "found-text", G_CALLBACK(findControllerFoundTextCallback), searchBar);

    return searchBar;
}

void browser_search_bar_open(BrowserSearchBar *searchBar)
{
    g_return_if_fail(BROWSER_IS_SEARCH_BAR(searchBar));

    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(searchBar), TRUE);
}

void browser_search_bar_close(BrowserSearchBar *searchBar)
{
    g_return_if_fail(BROWSER_IS_SEARCH_BAR(searchBar));

    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(searchBar), FALSE);
}

gboolean browser_search_bar_is_open(BrowserSearchBar *searchBar)
{
    g_return_val_if_fail(BROWSER_IS_SEARCH_BAR(searchBar), FALSE);

    GtkWidget *revealer = gtk_bin_get_child(GTK_BIN(searchBar));
    return gtk_revealer_get_reveal_child(GTK_REVEALER(revealer));
}

#endif
