/*
 * Copyright (C) 2013, 2020 Igalia S.L.
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
#include "BrowserSearchBox.h"

struct _BrowserSearchBox {
    GtkBox parent;

    WebKitWebView *webView;
    GtkWidget *entry;
    GtkWidget *prevButton;
    GtkWidget *nextButton;
    GActionGroup *actionGroup;
    GMenu *optionsMenu;
    GtkWidget *optionsPopover;
    GtkWidget *caseCheckButton;
    GtkWidget *begginigWordCheckButton;
    GtkWidget *capitalAsBegginigWordCheckButton;
};

G_DEFINE_TYPE(BrowserSearchBox, browser_search_box, GTK_TYPE_BOX)

static void setFailedStyleForEntry(BrowserSearchBox *searchBox, gboolean failedSearch)
{
    if (failedSearch)
        gtk_style_context_add_class(gtk_widget_get_style_context(searchBox->entry), "search-failed");
    else
        gtk_style_context_remove_class(gtk_widget_get_style_context(searchBox->entry), "search-failed");
}

static void doSearch(BrowserSearchBox *searchBox)
{
    GtkEntry *entry = GTK_ENTRY(searchBox->entry);
    if (!gtk_entry_get_text_length(entry)) {
        setFailedStyleForEntry(searchBox, FALSE);
        gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
        webkit_find_controller_search_finish(webkit_web_view_get_find_controller(searchBox->webView));
        return;
    }

    if (!gtk_entry_get_icon_name(entry, GTK_ENTRY_ICON_SECONDARY))
        gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "edit-clear-symbolic");

    WebKitFindOptions options = WEBKIT_FIND_OPTIONS_WRAP_AROUND;
    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(searchBox->actionGroup), "case-sensitive");
    GVariant *state = g_action_get_state(action);
    if (!g_variant_get_boolean(state))
        options |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;
    g_variant_unref(state);

    action = g_action_map_lookup_action(G_ACTION_MAP(searchBox->actionGroup), "beginning-word");
    state = g_action_get_state(action);
    if (g_variant_get_boolean(state))
        options |= WEBKIT_FIND_OPTIONS_AT_WORD_STARTS;
    g_variant_unref(state);

    action = g_action_map_lookup_action(G_ACTION_MAP(searchBox->actionGroup), "capital-as-beginning-word");
    state = g_action_get_state(action);
    if (g_variant_get_boolean(state))
        options |= WEBKIT_FIND_OPTIONS_TREAT_MEDIAL_CAPITAL_AS_WORD_START;
    g_variant_unref(state);

#if GTK_CHECK_VERSION(3, 98, 5)
    const gchar *text = gtk_editable_get_text(GTK_EDITABLE(entry));
#else
    const gchar *text = gtk_entry_get_text(entry);
#endif
    webkit_find_controller_search(webkit_web_view_get_find_controller(searchBox->webView), text, options, G_MAXUINT);
}

static void searchNext(BrowserSearchBox *searchBox)
{
    webkit_find_controller_search_next(webkit_web_view_get_find_controller(searchBox->webView));
}

static void searchPrevious(BrowserSearchBox *searchBox)
{
    webkit_find_controller_search_previous(webkit_web_view_get_find_controller(searchBox->webView));
}

static void searchEntryMenuIconPressedCallback(BrowserSearchBox *searchBox, GtkEntryIconPosition iconPosition, GdkEvent *event)
{
    if (iconPosition != GTK_ENTRY_ICON_PRIMARY)
        return;

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(searchBox->optionsMenu));
    gtk_widget_set_parent(popover, GTK_WIDGET(searchBox));
#else
    GtkWidget *popover = gtk_popover_menu_new();
    gtk_popover_bind_model(GTK_POPOVER(popover), G_MENU_MODEL(searchBox->optionsMenu), NULL);
    gtk_popover_set_relative_to(GTK_POPOVER(popover), searchBox->entry);
#endif
    searchBox->optionsPopover = popover;
    GdkRectangle rect;
    gtk_entry_get_icon_area(GTK_ENTRY(searchBox->entry), GTK_ENTRY_ICON_PRIMARY, &rect);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
    gtk_popover_popup(GTK_POPOVER(popover));
}

static void searchEntryClearIconReleasedCallback(BrowserSearchBox *searchBox, GtkEntryIconPosition iconPosition)
{
    if (iconPosition != GTK_ENTRY_ICON_SECONDARY)
        return;

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_editable_set_text(GTK_EDITABLE(searchBox->entry), "");
#else
    gtk_entry_set_text(GTK_ENTRY(searchBox->entry), "");
#endif
}

static void searchEntryChangedCallback(GtkEntry *entry, BrowserSearchBox *searchBox)
{
    doSearch(searchBox);
}

static void searchEntryActivatedCallback(BrowserSearchBox *searchBox)
{
    searchNext(searchBox);
}

static void searchPreviousButtonCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    searchPrevious(BROWSER_SEARCH_BOX(userData));
}

static void searchNextButtonCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    searchNext(BROWSER_SEARCH_BOX(userData));
}

static void searchMenuCheckButtonToggledCallback(GSimpleAction *action, GVariant *state, gpointer userData)
{
    g_simple_action_set_state(action, state);
    doSearch(BROWSER_SEARCH_BOX(userData));
}

static void findControllerFailedToFindTextCallback(BrowserSearchBox *searchBox)
{
    setFailedStyleForEntry(searchBox, TRUE);
}

static void findControllerFoundTextCallback(BrowserSearchBox *searchBox)
{
    setFailedStyleForEntry(searchBox, FALSE);
}

static const GActionEntry actions[] = {
    { "next", searchNextButtonCallback, NULL, NULL, NULL, { 0 } },
    { "previous", searchPreviousButtonCallback, NULL, NULL, NULL, { 0 } },
    { "case-sensitive", NULL, NULL, "false", searchMenuCheckButtonToggledCallback, { 0 } },
    { "beginning-word", NULL, NULL, "false", searchMenuCheckButtonToggledCallback, { 0 } },
    { "capital-as-beginning-word", NULL, NULL, "false", searchMenuCheckButtonToggledCallback, { 0 } }
};

static void browser_search_box_init(BrowserSearchBox *searchBox)
{
    GSimpleActionGroup *actionGroup = g_simple_action_group_new();
    searchBox->actionGroup = G_ACTION_GROUP(actionGroup);
    g_action_map_add_action_entries(G_ACTION_MAP(actionGroup), actions, G_N_ELEMENTS(actions), searchBox);
    gtk_widget_insert_action_group(GTK_WIDGET(searchBox), "find", G_ACTION_GROUP(actionGroup));

    gtk_orientable_set_orientation(GTK_ORIENTABLE(searchBox), GTK_ORIENTATION_HORIZONTAL);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_add_css_class(GTK_WIDGET(searchBox), "linked");
    gtk_widget_add_css_class(GTK_WIDGET(searchBox), "raised");
#else
    GtkStyleContext *styleContext = gtk_widget_get_style_context(GTK_WIDGET(searchBox));
    gtk_style_context_add_class(styleContext, GTK_STYLE_CLASS_LINKED);
    gtk_style_context_add_class(styleContext, GTK_STYLE_CLASS_RAISED);
#endif

    searchBox->entry = gtk_entry_new();
    gtk_widget_set_halign(searchBox->entry, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(searchBox->entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(searchBox->entry), "Search");
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(searchBox->entry), GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");
    gtk_entry_set_icon_activatable(GTK_ENTRY(searchBox->entry), GTK_ENTRY_ICON_PRIMARY, TRUE);
    gtk_entry_set_icon_sensitive(GTK_ENTRY(searchBox->entry), GTK_ENTRY_ICON_PRIMARY, TRUE);
    gtk_entry_set_icon_tooltip_text(GTK_ENTRY(searchBox->entry), GTK_ENTRY_ICON_PRIMARY, "Search options");
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(searchBox), searchBox->entry);
#else
    gtk_container_add(GTK_CONTAINER(searchBox), searchBox->entry);
    gtk_widget_show(searchBox->entry);
#endif

    GtkCssProvider *cssProvider = gtk_css_provider_new();
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_css_provider_load_from_data(cssProvider, ".search-failed { background-color: #ff6666; }", -1);
#else
    gtk_css_provider_load_from_data(cssProvider, ".search-failed { background-color: #ff6666; }", -1, NULL);
#endif
    gtk_style_context_add_provider(gtk_widget_get_style_context(searchBox->entry), GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(cssProvider);

#if GTK_CHECK_VERSION(3, 98, 5)
    searchBox->prevButton = gtk_button_new_from_icon_name("go-up-symbolic");
#else
    searchBox->prevButton = gtk_button_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_MENU);
#endif
    GtkButton *button = GTK_BUTTON(searchBox->prevButton);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), "find.previous");
    gtk_widget_set_focus_on_click(searchBox->prevButton, FALSE);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(searchBox), searchBox->prevButton);
#else
    gtk_box_pack_start(GTK_BOX(searchBox), searchBox->prevButton, FALSE, FALSE, 0);
    gtk_widget_show(searchBox->prevButton);
#endif

#if GTK_CHECK_VERSION(3, 98, 5)
    searchBox->nextButton = gtk_button_new_from_icon_name("go-down-symbolic");
#else
    searchBox->nextButton = gtk_button_new_from_icon_name("go-down-symbolic", GTK_ICON_SIZE_MENU);
#endif
    button = GTK_BUTTON(searchBox->nextButton);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), "find.next");
    gtk_widget_set_focus_on_click(searchBox->nextButton, FALSE);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(searchBox), searchBox->nextButton);
#else
    gtk_box_pack_start(GTK_BOX(searchBox), searchBox->nextButton, FALSE, FALSE, 0);
    gtk_widget_show(searchBox->nextButton);
#endif

    searchBox->optionsMenu = g_menu_new();
    g_menu_append(searchBox->optionsMenu, "Ca_se sensitive", "find.case-sensitive");
    g_menu_append(searchBox->optionsMenu, "Only at the _beginning of words", "find.beginning-word");
    g_menu_append(searchBox->optionsMenu, "Capital _always as beginning of word", "find.capital-as-beginning-word");

    g_signal_connect_swapped(searchBox->entry, "icon-press", G_CALLBACK(searchEntryMenuIconPressedCallback), searchBox);
    g_signal_connect_swapped(searchBox->entry, "icon-release", G_CALLBACK(searchEntryClearIconReleasedCallback), searchBox);
    g_signal_connect_after(searchBox->entry, "changed", G_CALLBACK(searchEntryChangedCallback), searchBox);
    g_signal_connect_swapped(searchBox->entry, "activate", G_CALLBACK(searchEntryActivatedCallback), searchBox);
}

static void browserSearchBoxFinalize(GObject *gObject)
{
    BrowserSearchBox *searchBox = BROWSER_SEARCH_BOX(gObject);

    if (searchBox->webView) {
        g_object_unref(searchBox->webView);
        searchBox->webView = NULL;
    }

    if (searchBox->optionsMenu) {
        g_object_unref(searchBox->optionsMenu);
        searchBox->optionsMenu = NULL;
    }

    g_object_unref(searchBox->actionGroup);

    G_OBJECT_CLASS(browser_search_box_parent_class)->finalize(gObject);
}

static void browserSearchBoxDispose(GObject *object)
{
#if GTK_CHECK_VERSION(3, 98, 5)
    BrowserSearchBox *searchBox = BROWSER_SEARCH_BOX(object);
    g_clear_pointer(&searchBox->optionsPopover, gtk_widget_unparent);
#endif

    G_OBJECT_CLASS(browser_search_box_parent_class)->dispose(object);
}

#if GTK_CHECK_VERSION(3, 98, 5)
static void browserSearchBoxSizeAllocate(GtkWidget *widget, int width, int height, int baseline)
{
    GTK_WIDGET_CLASS(browser_search_box_parent_class)->size_allocate(widget, width, height, baseline);

    BrowserSearchBox *searchBox = BROWSER_SEARCH_BOX(widget);
    if (searchBox->optionsPopover)
        gtk_native_check_resize(GTK_NATIVE(searchBox->optionsPopover));
}
#endif

static void browser_search_box_class_init(BrowserSearchBoxClass *klass)
{
    GObjectClass *gObjectClass = G_OBJECT_CLASS(klass);
    gObjectClass->finalize = browserSearchBoxFinalize;
    gObjectClass->dispose = browserSearchBoxDispose;

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWidgetClass *widgetClass = GTK_WIDGET_CLASS(klass);
    widgetClass->size_allocate = browserSearchBoxSizeAllocate;
#endif
}

GtkWidget *browser_search_box_new(WebKitWebView *webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    GtkWidget *searchBox = GTK_WIDGET(g_object_new(BROWSER_TYPE_SEARCH_BOX, NULL));
    BROWSER_SEARCH_BOX(searchBox)->webView = g_object_ref(webView);

    WebKitFindController *controller = webkit_web_view_get_find_controller(webView);
    g_signal_connect_swapped(controller, "failed-to-find-text", G_CALLBACK(findControllerFailedToFindTextCallback), searchBox);
    g_signal_connect_swapped(controller, "found-text", G_CALLBACK(findControllerFoundTextCallback), searchBox);

    return searchBox;
}

GtkEntry *browser_search_box_get_entry(BrowserSearchBox *searchBox)
{
    g_return_val_if_fail(BROWSER_IS_SEARCH_BOX(searchBox), NULL);

    return GTK_ENTRY(searchBox->entry);
}
