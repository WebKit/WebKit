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

#include "BrowserSettingsDialog.h"
#include "BrowserCellRendererVariant.h"

enum {
    PROP_0,

    PROP_SETTINGS
};

enum {
    SETTINGS_LIST_COLUMN_NAME,
    SETTINGS_LIST_COLUMN_NICK,
    SETTINGS_LIST_COLUMN_BLURB,
    SETTINGS_LIST_COLUMN_VALUE,
    SETTINGS_LIST_COLUMN_ADJUSTMENT,

    SETTINGS_LIST_N_COLUMNS
};

enum {
    FEATURES_LIST_COLUMN_NAME,
    FEATURES_LIST_COLUMN_DETAILS,
    FEATURES_LIST_COLUMN_FEATURE,

    FEATURES_LIST_N_COLUMNS,
};

struct _BrowserSettingsDialog {
    GtkDialog parent;

    GtkWidget *stack;
    GtkWidget *settingsList;
    WebKitSettings *settings;
};

struct _BrowserSettingsDialogClass {
    GtkDialogClass parent;
};

G_DEFINE_TYPE(BrowserSettingsDialog, browser_settings_dialog, GTK_TYPE_DIALOG)

static const char *hardwareAccelerationPolicyToString(WebKitHardwareAccelerationPolicy policy)
{
    switch (policy) {
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS:
        return "always";
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER:
        return "never";
#if !GTK_CHECK_VERSION(3, 98, 0)
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND:
        return "ondemand";
#endif

    }

    g_assert_not_reached();
    return "ondemand";
}

static int stringToHardwareAccelerationPolicy(const char *policy)
{
    if (!g_ascii_strcasecmp(policy, "always"))
        return WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS;
    if (!g_ascii_strcasecmp(policy, "never"))
        return WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER;
#if !GTK_CHECK_VERSION(3, 98, 0)
    if (!g_ascii_strcasecmp(policy, "ondemand"))
        return WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND;
#endif

    g_warning("Invalid value %s for hardware-acceleration-policy setting", policy);
    return -1;
}

static void cellRendererChanged(GtkCellRenderer *renderer, const char *path, const GValue *value, BrowserSettingsDialog *dialog)
{
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(dialog->settingsList));
    GtkTreePath *treePath = gtk_tree_path_new_from_string(path);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, treePath);

    gboolean updateTreeStore = TRUE;
    char *name;
    gtk_tree_model_get(model, &iter, SETTINGS_LIST_COLUMN_NAME, &name, -1);
    if (!g_strcmp0(name, "hardware-acceleration-policy")) {
        int policy = stringToHardwareAccelerationPolicy(g_value_get_string(value));
        if (policy != -1)
            webkit_settings_set_hardware_acceleration_policy(dialog->settings, policy);
        else
            updateTreeStore = FALSE;
    } else
        g_object_set_property(G_OBJECT(dialog->settings), name, value);
    g_free(name);

    if (updateTreeStore)
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, SETTINGS_LIST_COLUMN_VALUE, value, -1);
    gtk_tree_path_free(treePath);
}

static void featureTreeViewRowActivated(GtkTreeView *tree, GtkTreePath *path, GtkTreeViewColumn *column, BrowserSettingsDialog *dialog)
{
    g_autoptr(WebKitFeature) feature = NULL;
    GtkTreeModel *model = gtk_tree_view_get_model(tree);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter,
                       FEATURES_LIST_COLUMN_FEATURE, &feature,
                       -1);
    gboolean enabled = webkit_settings_get_feature_enabled(dialog->settings, feature);
    webkit_settings_set_feature_enabled(dialog->settings, feature, !enabled);
    gtk_tree_model_row_changed(model, path, &iter);
}

static void cellRendererToggled(GtkCellRendererToggle *renderer, const char *path, BrowserSettingsDialog *dialog)
{
    GtkWidget *scrolledWindow = gtk_stack_get_visible_child(GTK_STACK(dialog->stack));

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkTreeView *tree = GTK_TREE_VIEW(gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(scrolledWindow)));
#else
    GtkTreeView *tree = GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(scrolledWindow)));
#endif

    g_autoptr(GtkTreePath) treePath = gtk_tree_path_new_from_string(path);
    featureTreeViewRowActivated(tree, treePath, NULL, dialog);
}

static void browserSettingsDialogSetProperty(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    BrowserSettingsDialog *dialog = BROWSER_SETTINGS_DIALOG(object);

    switch (propId) {
    case PROP_SETTINGS:
        dialog->settings = g_value_get_object(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void featureTreeViewRenderEnabledData(GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    g_autoptr(WebKitFeature) feature = NULL;
    gtk_tree_model_get(model, iter, FEATURES_LIST_COLUMN_FEATURE, &feature, -1);
    gtk_cell_renderer_toggle_set_active(GTK_CELL_RENDERER_TOGGLE(renderer),
                                        webkit_settings_get_feature_enabled(data, feature));
}

static void featureTreeViewRenderStatusData(GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    g_autoptr(WebKitFeature) feature = NULL;
    gtk_tree_model_get(model, iter, FEATURES_LIST_COLUMN_FEATURE, &feature, -1);
    g_autoptr(GEnumClass) enumClass = g_type_class_ref(WEBKIT_TYPE_FEATURE_STATUS);
    g_object_set(renderer,
                 "markup", NULL,
                 "text", g_enum_get_value(enumClass, webkit_feature_get_status(feature))->value_nick,
                 NULL);
}

static void featureTreeViewRenderCategoryData(GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    g_autoptr(WebKitFeature) feature = NULL;
    gtk_tree_model_get(model, iter, FEATURES_LIST_COLUMN_FEATURE, &feature, -1);
    g_object_set(renderer,
                 "markup", NULL,
                 "text", webkit_feature_get_category(feature),
                 NULL);
}

static gboolean featureTreeViewSearchMatchSubstring(GtkTreeModel *model, gint column, const char *key, GtkTreeIter *iter, gpointer data)
{
    g_autoptr(WebKitFeature) feature = NULL;
    gtk_tree_model_get(model, iter, FEATURES_LIST_COLUMN_FEATURE, &feature, -1);

    if (g_str_match_string(key, webkit_feature_get_name(feature), TRUE))
        return FALSE;

    if (!webkit_feature_get_details(feature))
        return TRUE;

    return !g_str_match_string(key, webkit_feature_get_details(feature), TRUE);
}

static GtkWidget* createFeatureTreeView(BrowserSettingsDialog *dialog, WebKitFeatureList *featureList)
{
    g_autoptr(GtkListStore) model = gtk_list_store_new(FEATURES_LIST_N_COLUMNS,
                                                       G_TYPE_STRING,  /* name */
                                                       G_TYPE_STRING,  /* details */
                                                       WEBKIT_TYPE_FEATURE);
    for (gsize i = 0; i < webkit_feature_list_get_length(featureList); i++) {
        WebKitFeature *feature = webkit_feature_list_get(featureList, i);
        g_autofree char *featureName = webkit_feature_get_name(feature)
            ? g_markup_escape_text(webkit_feature_get_name(feature), -1)
            : g_strdup(webkit_feature_get_identifier(feature));
        g_autofree char* featureDetails = webkit_feature_get_details(feature)
            ? g_markup_escape_text(webkit_feature_get_details(feature), -1)
            : NULL;

        GtkTreeIter iter;
        gtk_list_store_append(model, &iter);
        gtk_list_store_set(model, &iter,
                           FEATURES_LIST_COLUMN_NAME, featureName,
                           FEATURES_LIST_COLUMN_DETAILS, featureDetails,
                           FEATURES_LIST_COLUMN_FEATURE, feature,
                           -1);
    }
    webkit_feature_list_unref(featureList);

    GtkWidget *tree = g_object_new(GTK_TYPE_TREE_VIEW,
                                   "model", model,
                                   "search-column", FEATURES_LIST_COLUMN_NAME,
                                   "tooltip-column", FEATURES_LIST_COLUMN_DETAILS,
                                   NULL);
    g_signal_connect(tree, "row-activated", G_CALLBACK(featureTreeViewRowActivated), dialog);
    gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(tree), featureTreeViewSearchMatchSubstring, NULL, NULL);

    GtkCellRenderer *textRenderer = gtk_cell_renderer_text_new();
    GtkCellRenderer *toggleRenderer = gtk_cell_renderer_toggle_new();
    g_signal_connect(toggleRenderer, "toggled", G_CALLBACK(cellRendererToggled), dialog);

    gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(tree),
                                               -1, "Enabled", toggleRenderer,
                                               featureTreeViewRenderEnabledData,
                                               dialog->settings, NULL);
    gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(tree),
                                               -1, "Category", textRenderer,
                                               featureTreeViewRenderCategoryData,
                                               NULL, NULL);
    gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(tree),
                                               -1, "Status", textRenderer,
                                               featureTreeViewRenderStatusData,
                                               NULL, NULL);

    guint n = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree),
                                                          -1, "Feature", textRenderer,
                                                          "markup", FEATURES_LIST_COLUMN_NAME,
                                                          NULL);
    GtkTreeViewColumn *column = gtk_tree_view_get_column(GTK_TREE_VIEW(tree), n - 1);
    gtk_tree_view_column_set_sort_column_id(column, FEATURES_LIST_COLUMN_NAME);
    gtk_tree_view_column_set_expand(column, TRUE);

    return g_object_new(GTK_TYPE_SCROLLED_WINDOW,
                        "halign", GTK_ALIGN_FILL,
                        "valign", GTK_ALIGN_FILL,
                        "hexpand", TRUE,
                        "vexpand", TRUE,
                        "child", tree,
                        NULL);
}

static gboolean settingsTreeViewSearchMatchSubstring(GtkTreeModel *model, gint column, const char *key, GtkTreeIter *iter, gpointer data)
{
    g_autofree char *nick = NULL;
    g_autofree char *blurb = NULL;
    gtk_tree_model_get(model, iter,
                       SETTINGS_LIST_COLUMN_NICK, &nick,
                       SETTINGS_LIST_COLUMN_BLURB, &blurb,
                       -1);

    if (g_str_match_string(key, nick, TRUE))
        return FALSE;

    return blurb ? !g_str_match_string(key, blurb, TRUE) : TRUE;

}

static void browser_settings_dialog_init(BrowserSettingsDialog *dialog)
{
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);
#if !GTK_CHECK_VERSION(3, 98, 5)
    gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
#endif
    gtk_window_set_title(GTK_WINDOW(dialog), "WebKit Settings");
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

    /* Settings list */
    dialog->settingsList = g_object_new(GTK_TYPE_TREE_VIEW,
                                        "search-column", SETTINGS_LIST_COLUMN_NICK,
                                        "tooltip-column", SETTINGS_LIST_COLUMN_BLURB,
                                        NULL);
    gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(dialog->settingsList),
                                        settingsTreeViewSearchMatchSubstring, NULL, NULL);
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(dialog->settingsList),
                                                0, "Name", renderer,
                                                "text", SETTINGS_LIST_COLUMN_NICK,
                                                NULL);
    renderer = browser_cell_renderer_variant_new();
    g_signal_connect(renderer, "changed", G_CALLBACK(cellRendererChanged), dialog);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(dialog->settingsList),
                                                1, "Value", renderer,
                                                "value", SETTINGS_LIST_COLUMN_VALUE,
                                                "adjustment", SETTINGS_LIST_COLUMN_ADJUSTMENT,
                                                NULL);
}

static void browserSettingsDialogConstructed(GObject *object)
{
    G_OBJECT_CLASS(browser_settings_dialog_parent_class)->constructed(object);

    BrowserSettingsDialog *dialog = BROWSER_SETTINGS_DIALOG(object);
    WebKitSettings *settings = dialog->settings;

    /* Settings */
    GtkListStore *model = gtk_list_store_new(SETTINGS_LIST_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                             G_TYPE_VALUE, G_TYPE_OBJECT);
    guint propertiesCount;
    GParamSpec **properties = g_object_class_list_properties(G_OBJECT_GET_CLASS(settings), &propertiesCount);
    guint i;
    for (i = 0; i < propertiesCount; i++) {
        GParamSpec *property = properties[i];
        const char *name = g_param_spec_get_name(property);
        const char *nick = g_param_spec_get_nick(property);
        char *blurb = g_markup_escape_text(g_param_spec_get_blurb(property), -1);

        GValue value = { 0, { { 0 } } };
        if (!g_strcmp0(name, "hardware-acceleration-policy")) {
            g_value_init(&value, G_TYPE_STRING);
            g_value_set_string(&value, hardwareAccelerationPolicyToString(webkit_settings_get_hardware_acceleration_policy(settings)));
            char *extendedBlutb = g_strdup_printf("%s (always, never or ondemand)", blurb);
            g_free(blurb);
            blurb = extendedBlutb;
        } else {
            g_value_init(&value, G_PARAM_SPEC_VALUE_TYPE(property));
            g_object_get_property(G_OBJECT(settings), name, &value);
        }

        GtkAdjustment *adjustment = NULL;
        if (G_PARAM_SPEC_VALUE_TYPE(property) == G_TYPE_UINT) {
            GParamSpecUInt *uIntProperty = G_PARAM_SPEC_UINT(property);
            adjustment = gtk_adjustment_new(uIntProperty->default_value, uIntProperty->minimum,
                                            uIntProperty->maximum, 1, 1, 1);
        }

        GtkTreeIter iter;
        gtk_list_store_append(model, &iter);
        gtk_list_store_set(model, &iter,
                           SETTINGS_LIST_COLUMN_NAME, name,
                           SETTINGS_LIST_COLUMN_NICK, nick,
                           SETTINGS_LIST_COLUMN_BLURB, blurb,
                           SETTINGS_LIST_COLUMN_VALUE, &value,
                           SETTINGS_LIST_COLUMN_ADJUSTMENT, adjustment,
                           -1);
        g_free(blurb);
        g_value_unset(&value);
    }
    g_free(properties);

    gtk_tree_view_set_model(GTK_TREE_VIEW(dialog->settingsList), GTK_TREE_MODEL(model));
    g_object_unref(model);

    dialog->stack = gtk_stack_new();

    /* Settings list */
    gtk_stack_add_titled(GTK_STACK(dialog->stack),
                         g_object_new(GTK_TYPE_SCROLLED_WINDOW,
                                      "halign", GTK_ALIGN_FILL,
                                      "valign", GTK_ALIGN_FILL,
                                      "hexpand", TRUE,
                                      "vexpand", TRUE,
                                      "child", dialog->settingsList,
                                      NULL),
                         "settings",
                         "Settings");

    /* Experimental and development features */
    gtk_stack_add_titled(GTK_STACK(dialog->stack),
                         createFeatureTreeView(dialog, webkit_settings_get_experimental_features()),
                         "experimental", "Experimental");
    gtk_stack_add_titled(GTK_STACK(dialog->stack),
                         createFeatureTreeView(dialog, webkit_settings_get_development_features()),
                         "development", "Development");

    GtkWidget *switcher = g_object_new(GTK_TYPE_STACK_SWITCHER, "stack", dialog->stack, NULL);
    GtkHeaderBar *header = GTK_HEADER_BAR(gtk_dialog_get_header_bar(GTK_DIALOG(dialog)));

    GtkBox *contentArea = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(contentArea, dialog->stack);
    gtk_header_bar_set_title_widget(header, switcher);
#else
    gtk_box_pack_start(contentArea, dialog->stack, TRUE, TRUE, 0);
    gtk_widget_show_all(GTK_WIDGET(contentArea));
    gtk_header_bar_set_custom_title(header, switcher);
    gtk_widget_show_all(GTK_WIDGET(header));
#endif
}

static void browser_settings_dialog_class_init(BrowserSettingsDialogClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);

    gobjectClass->constructed = browserSettingsDialogConstructed;
    gobjectClass->set_property = browserSettingsDialogSetProperty;

    g_object_class_install_property(gobjectClass,
                                    PROP_SETTINGS,
                                    g_param_spec_object("settings",
                                                        NULL, NULL,
                                                        WEBKIT_TYPE_SETTINGS,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

GtkWidget *browser_settings_dialog_new(WebKitSettings *settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), NULL);

    return GTK_WIDGET(g_object_new(BROWSER_TYPE_SETTINGS_DIALOG,
                                   "settings", settings,
                                   "use-header-bar", TRUE,
                                   NULL));
}
