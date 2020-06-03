/*
 * Copyright (C) 2019 Igalia S.L.
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

// GtkEmojiChooser is private in GTK 3, so this is based in the GTK code, just adapted to
// WebKit coding style, using some internal types from WTF to simplify the implementation
// and not using GtkBuilder for the UI.

#include "config.h"
#include "WebKitEmojiChooser.h"

#if GTK_CHECK_VERSION(3, 24, 0) && !USE(GTK4)

#include <glib/gi18n-lib.h>
#include <wtf/HashSet.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

enum {
    EMOJI_PICKED,

    LAST_SIGNAL
};

struct EmojiSection {
    GtkWidget* heading { nullptr };
    GtkWidget* box { nullptr };
    GtkWidget* button { nullptr };
    bool isEmpty { false };
    const char* firstEmojiName { nullptr };
};

using SectionList = Vector<EmojiSection, 9>;

class CallbackTimer final : public RunLoop::TimerBase {
public:
    CallbackTimer(Function<void()>&& callback)
        : RunLoop::TimerBase(RunLoop::main())
        , m_callback(WTFMove(callback))
    {
    }

    ~CallbackTimer() = default;

private:
    void fired() override
    {
        m_callback();
    }

    Function<void()> m_callback;
};

struct _WebKitEmojiChooserPrivate {
    GtkWidget* stack;
    GtkWidget* swindow;
    GtkWidget* searchEntry;
    SectionList sections;
    GRefPtr<GSettings> settings;
    HashSet<GRefPtr<GtkGesture>> gestures;
    int emojiMaxWidth;
    std::unique_ptr<CallbackTimer> populateSectionsTimer;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitEmojiChooser, webkit_emoji_chooser, GTK_TYPE_POPOVER)

static void emojiPopupMenu(GtkWidget*, WebKitEmojiChooser*);

static const unsigned boxSpace = 6;

static void emojiHovered(GtkWidget* widget, GdkEvent* event)
{
    if (gdk_event_get_event_type(event) == GDK_ENTER_NOTIFY)
        gtk_widget_set_state_flags(widget, GTK_STATE_FLAG_PRELIGHT, FALSE);
    else
        gtk_widget_unset_state_flags(widget, GTK_STATE_FLAG_PRELIGHT);
}

static GtkWidget* webkitEmojiChooserAddEmoji(WebKitEmojiChooser* chooser, GtkFlowBox* parent, GVariant* item, bool prepend = false, gunichar modifier = 0)
{
    char text[64];
    char* textPtr = text;
    GRefPtr<GVariant> codes = adoptGRef(g_variant_get_child_value(item, 0));
    for (unsigned i = 0; i < g_variant_n_children(codes.get()); ++i) {
        gunichar code;
        g_variant_get_child(codes.get(), i, "u", &code);
        if (!code)
            code = modifier;
        if (code)
            textPtr += g_unichar_to_utf8(code, textPtr);
    }
    // U+FE0F is the Emoji variation selector
    textPtr += g_unichar_to_utf8(0xFE0F, textPtr);
    textPtr[0] = '\0';

    GtkWidget* label = gtk_label_new(text);
    PangoAttrList* attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes, pango_attr_scale_new(PANGO_SCALE_X_LARGE));
    gtk_label_set_attributes(GTK_LABEL(label), attributes);
    pango_attr_list_unref(attributes);

    PangoLayout* layout = gtk_label_get_layout(GTK_LABEL(label));
    PangoRectangle rect;
    pango_layout_get_extents(layout, &rect, nullptr);
    // Check for fallback rendering that generates too wide items.
    if (pango_layout_get_unknown_glyphs_count(layout) || rect.width >= 1.5 * chooser->priv->emojiMaxWidth) {
        gtk_widget_destroy(label);
        return nullptr;
    }

    GtkWidget* child = gtk_flow_box_child_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(child), "emoji");
    g_object_set_data_full(G_OBJECT(child), "emoji-data", g_variant_ref(item), reinterpret_cast<GDestroyNotify>(g_variant_unref));
    if (modifier)
        g_object_set_data(G_OBJECT(child), "modifier", GUINT_TO_POINTER(modifier));

    GtkWidget* eventBox = gtk_event_box_new();
    gtk_widget_add_events(eventBox, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(eventBox, "enter-notify-event", G_CALLBACK(emojiHovered), nullptr);
    g_signal_connect(eventBox, "leave-notify-event", G_CALLBACK(emojiHovered), nullptr);
    gtk_container_add(GTK_CONTAINER(eventBox), label);
    gtk_widget_show(label);

    gtk_container_add(GTK_CONTAINER(child), eventBox);
    gtk_widget_show(eventBox);

    gtk_flow_box_insert(parent, child, prepend ? 0 : -1);
    gtk_widget_show(child);

    return child;
}

static void webkitEmojiChooserAddRecentItem(WebKitEmojiChooser* chooser, GVariant* item, gunichar modifier)
{
    GRefPtr<GVariant> protectItem(item);
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a((auss)u)"));
    g_variant_builder_add(&builder, "(@(auss)u)", item, modifier);

    auto& section = chooser->priv->sections.first();

    static const unsigned maxRecentItems = 7 * 3;

    GUniquePtr<GList> children(gtk_container_get_children(GTK_CONTAINER(section.box)));
    unsigned i = 1;
    for (auto* l = children.get(); l; l = g_list_next(l), ++i) {
        auto* item2 = static_cast<GVariant*>(g_object_get_data(G_OBJECT(l->data), "emoji-data"));
        auto modifier2 = static_cast<gunichar>(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(l->data), "modifier")));
        if (modifier == modifier2 && g_variant_equal(item, item2)) {
            gtk_widget_destroy(GTK_WIDGET(l->data));
            --i;
            continue;
        }

        if (i >= maxRecentItems) {
            gtk_widget_destroy(GTK_WIDGET(l->data));
            continue;
        }

        g_variant_builder_add(&builder, "(@(auss)u)", item2, modifier2);
    }

    auto* child = webkitEmojiChooserAddEmoji(chooser, GTK_FLOW_BOX(section.box), item, true, modifier);
    if (child)
        g_signal_connect(child, "popup-menu", G_CALLBACK(emojiPopupMenu), chooser);

    gtk_widget_show(section.box);
    gtk_widget_set_sensitive(section.button, TRUE);

    g_settings_set_value(chooser->priv->settings.get(), "recent-emoji", g_variant_builder_end(&builder));
}

static void emojiActivated(GtkFlowBox* box, GtkFlowBoxChild* child, WebKitEmojiChooser* chooser)
{
    gtk_popover_popdown(GTK_POPOVER(chooser));

    GtkWidget* label = gtk_bin_get_child(GTK_BIN(gtk_bin_get_child(GTK_BIN(child))));
    GUniquePtr<char> text(g_strdup(gtk_label_get_label(GTK_LABEL(label))));

    auto* item = static_cast<GVariant*>(g_object_get_data(G_OBJECT(child), "emoji-data"));
    auto modifier = static_cast<gunichar>(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(child), "modifier")));
    webkitEmojiChooserAddRecentItem(chooser, item, modifier);
    g_signal_emit(chooser, signals[EMOJI_PICKED], 0, text.get());
}

static bool emojiDataHasVariations(GVariant* emojiData)
{
    GRefPtr<GVariant> codes = adoptGRef(g_variant_get_child_value(emojiData, 0));
    for (size_t i = 0; i < g_variant_n_children(codes.get()); ++i) {
        gunichar code;
        g_variant_get_child(codes.get(), i, "u", &code);
        if (!code)
            return true;
    }
    return false;
}

static void webkitEmojiChooserShowVariations(WebKitEmojiChooser* chooser, GtkWidget* child)
{
    if (!child)
        return;

    auto* emojiData = static_cast<GVariant*>(g_object_get_data(G_OBJECT(child), "emoji-data"));
    if (!emojiData)
        return;

    if (!emojiDataHasVariations(emojiData))
        return;

    GtkWidget* popover = gtk_popover_new(child);
    GtkWidget* view = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(view), "view");
    GtkWidget* box = gtk_flow_box_new();
    g_signal_connect(box, "child-activated", G_CALLBACK(emojiActivated), chooser);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(box), TRUE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(box), 6);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(box), 6);
    gtk_flow_box_set_activate_on_single_click(GTK_FLOW_BOX(box), TRUE);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(box), GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(view), box);
    gtk_widget_show(box);
    gtk_container_add(GTK_CONTAINER(popover), view);
    gtk_widget_show(view);

    webkitEmojiChooserAddEmoji(chooser, GTK_FLOW_BOX(box), emojiData);
    for (gunichar modifier = 0x1F3FB; modifier <= 0x1F3FF; ++modifier)
        webkitEmojiChooserAddEmoji(chooser, GTK_FLOW_BOX(box), emojiData, false, modifier);

    gtk_popover_popup(GTK_POPOVER(popover));
}

static void emojiLongPressed(GtkGesture* gesture, double x, double y, WebKitEmojiChooser* chooser)
{
    auto* box = GTK_FLOW_BOX(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture)));
    webkitEmojiChooserShowVariations(chooser, GTK_WIDGET(gtk_flow_box_get_child_at_pos(box, x, y)));
}

static void emojiPressed(GtkGesture* gesture, int, double x, double y, WebKitEmojiChooser* chooser)
{
    emojiLongPressed(gesture, x, y, chooser);
}

static void emojiPopupMenu(GtkWidget* child, WebKitEmojiChooser* chooser)
{
    webkitEmojiChooserShowVariations(chooser, child);
}

static void verticalAdjustmentChanged(GtkAdjustment* adjustment, WebKitEmojiChooser* chooser)
{
    double value = gtk_adjustment_get_value(adjustment);
    EmojiSection* sectionToSelect = nullptr;
    for (auto& section : chooser->priv->sections) {
        GtkAllocation allocation;
        if (section.heading)
            gtk_widget_get_allocation(section.heading, &allocation);
        else
            gtk_widget_get_allocation(section.box, &allocation);

        if (value < allocation.y - boxSpace)
            break;

        sectionToSelect = &section;
    }

    if (!sectionToSelect)
        sectionToSelect = &chooser->priv->sections[0];

    for (auto& section : chooser->priv->sections) {
        if (&section == sectionToSelect)
            gtk_widget_set_state_flags(section.button, GTK_STATE_FLAG_CHECKED, FALSE);
        else
            gtk_widget_unset_state_flags(section.button, GTK_STATE_FLAG_CHECKED);
    }
}

static GtkWidget* webkitEmojiChooserSetupSectionBox(WebKitEmojiChooser* chooser, GtkBox* parent, const char* firstEmojiName, const char* title, GtkAdjustment* adjustment, gboolean canHaveVariations = FALSE)
{
    EmojiSection section;
    section.firstEmojiName = firstEmojiName;
    if (title) {
        GtkWidget* label = gtk_label_new(title);
        section.heading = label;
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_box_pack_start(parent, label, FALSE, FALSE, 0);
        gtk_widget_show(label);
    }

    GtkWidget* box = gtk_flow_box_new();
    section.box = box;
    g_signal_connect(box, "child-activated", G_CALLBACK(emojiActivated), chooser);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(box), TRUE);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(box), GTK_SELECTION_NONE);
    gtk_container_set_focus_vadjustment(GTK_CONTAINER(box), adjustment);
    gtk_box_pack_start(parent, box, FALSE, FALSE, 0);
    gtk_widget_show(box);

    if (canHaveVariations) {
        GRefPtr<GtkGesture> gesture = adoptGRef(gtk_gesture_long_press_new(box));
        g_signal_connect(gesture.get(), "pressed", G_CALLBACK(emojiLongPressed), chooser);
        chooser->priv->gestures.add(WTFMove(gesture));
        GRefPtr<GtkGesture> multiGesture = adoptGRef(gtk_gesture_multi_press_new(box));
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(multiGesture.get()), GDK_BUTTON_SECONDARY);
        g_signal_connect(multiGesture.get(), "pressed", G_CALLBACK(emojiPressed), chooser);
        chooser->priv->gestures.add(WTFMove(multiGesture));
    }

    chooser->priv->sections.append(WTFMove(section));
    return box;
}

static void scrollToSection(GtkButton* button, gpointer data)
{
    auto* chooser = WEBKIT_EMOJI_CHOOSER(gtk_widget_get_ancestor(GTK_WIDGET(button), WEBKIT_TYPE_EMOJI_CHOOSER));
    auto& section = chooser->priv->sections[GPOINTER_TO_UINT(data)];
    GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(chooser->priv->swindow));
    if (section.heading) {
        GtkAllocation allocation = { 0, 0, 0, 0 };
        gtk_widget_get_allocation(section.heading, &allocation);
        gtk_adjustment_set_value(adjustment, allocation.y - boxSpace);
    } else
        gtk_adjustment_set_value(adjustment, 0);
}

static void webkitEmojiChooserSetupSectionButton(WebKitEmojiChooser* chooser, GtkBox* parent, const char* iconName, const char* tooltip)
{
    GtkWidget* button = gtk_button_new_from_icon_name(iconName, GTK_ICON_SIZE_BUTTON);
    chooser->priv->sections.last().button = button;
    gtk_style_context_add_class(gtk_widget_get_style_context(button), "emoji-section");
    gtk_widget_set_tooltip_text(button, tooltip);
    g_signal_connect(button, "clicked", G_CALLBACK(scrollToSection), GUINT_TO_POINTER(chooser->priv->sections.size() - 1));
    gtk_box_pack_start(parent, button, FALSE, FALSE, 0);
    gtk_widget_show(button);
}

static void webkitEmojiChooserSetupRecent(WebKitEmojiChooser* chooser, GtkBox* emojiBox, GtkBox* buttonBox, GtkAdjustment* adjustment)
{
    GtkWidget* flowBox = webkitEmojiChooserSetupSectionBox(chooser, emojiBox, nullptr, nullptr, adjustment, true);
    webkitEmojiChooserSetupSectionButton(chooser, buttonBox, "emoji-recent-symbolic", _("Recent"));

    bool isEmpty = true;
    GRefPtr<GVariant> variant = adoptGRef(g_settings_get_value(chooser->priv->settings.get(), "recent-emoji"));
    GVariantIter iter;
    g_variant_iter_init(&iter, variant.get());
    while (GRefPtr<GVariant> item = adoptGRef(g_variant_iter_next_value(&iter))) {
        GRefPtr<GVariant> emojiData = adoptGRef(g_variant_get_child_value(item.get(), 0));
        gunichar modifier;
        g_variant_get_child(item.get(), 1, "u", &modifier);
        if (auto* child = webkitEmojiChooserAddEmoji(chooser, GTK_FLOW_BOX(flowBox), emojiData.get(), true, modifier))
            g_signal_connect(child, "popup-menu", G_CALLBACK(emojiPopupMenu), chooser);
        isEmpty = false;
    }

    if (isEmpty) {
        gtk_widget_hide(flowBox);
        gtk_widget_set_sensitive(chooser->priv->sections.first().button, FALSE);
    }
}

static void webkitEmojiChooserEnsureEmptyResult(WebKitEmojiChooser* chooser)
{
    if (gtk_stack_get_child_by_name(GTK_STACK(chooser->priv->stack), "empty"))
        return;

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(grid), "dim-label");

    GtkWidget* image = gtk_image_new_from_icon_name("edit-find-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(image), 72);
    gtk_style_context_add_class(gtk_widget_get_style_context(image), "dim-label");
    gtk_grid_attach(GTK_GRID(grid), image, 0, 0, 1, 1);
    gtk_widget_show(image);

    GtkWidget* label = gtk_label_new(_("No Results Found"));
    PangoAttrList* attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes, pango_attr_scale_new(1.44));
    pango_attr_list_insert(attributes, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(label), attributes);
    pango_attr_list_unref(attributes);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
    gtk_widget_show(label);

    label = gtk_label_new(_("Try a different search"));
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "dim-label");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
    gtk_widget_show(label);

    gtk_stack_add_named(GTK_STACK(chooser->priv->stack), grid, "empty");
    gtk_widget_show(grid);
}

static void webkitEmojiChooserSearchChanged(WebKitEmojiChooser* chooser)
{
    for (auto& section : chooser->priv->sections) {
        section.isEmpty = true;
        gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(section.box));
    }

    bool resultsFound = false;
    for (auto& section : chooser->priv->sections) {
        if (section.heading) {
            gtk_widget_set_visible(section.heading, !section.isEmpty);
            gtk_widget_set_visible(section.box, !section.isEmpty);
        }
        resultsFound = resultsFound || !section.isEmpty;
    }

    if (!resultsFound) {
        webkitEmojiChooserEnsureEmptyResult(chooser);
        gtk_stack_set_visible_child_name(GTK_STACK(chooser->priv->stack), "empty");
    } else
        gtk_stack_set_visible_child_name(GTK_STACK(chooser->priv->stack), "list");
}

static void webkitEmojiChooserSetupFilters(WebKitEmojiChooser* chooser)
{
    for (size_t i = 0; i < chooser->priv->sections.size(); ++i) {
        gtk_flow_box_set_filter_func(GTK_FLOW_BOX(chooser->priv->sections[i].box), [](GtkFlowBoxChild* child, gpointer userData) -> gboolean {
            auto* chooser = WEBKIT_EMOJI_CHOOSER(gtk_widget_get_ancestor(GTK_WIDGET(child), WEBKIT_TYPE_EMOJI_CHOOSER));
            auto& section = chooser->priv->sections[GPOINTER_TO_UINT(userData)];
            const char* text = gtk_entry_get_text(GTK_ENTRY(chooser->priv->searchEntry));
            if (!text || !*text) {
                section.isEmpty = false;
                return TRUE;
            }

            auto* emojiData = static_cast<GVariant*>(g_object_get_data(G_OBJECT(child), "emoji-data"));
            if (!emojiData) {
                section.isEmpty = false;
                return TRUE;
            }

            const char* name;
            g_variant_get_child(emojiData, 1, "&s", &name);
            if (g_str_match_string(text, name, TRUE)) {
                section.isEmpty = false;
                return TRUE;
            }

            return FALSE;
        }, GUINT_TO_POINTER(i), nullptr);
    }
}

static void webkitEmojiChooserSetupEmojiSections(WebKitEmojiChooser* chooser, GtkBox* emojiBox, GtkBox* buttonBox)
{
    static const struct {
        const char* firstEmojiName;
        const char* title;
        const char* iconName;
        bool canHaveVariations;
    } sections[] = {
        { "grinning face", N_("Smileys & People"), "emoji-people-symbolic", true },
        { "selfie", N_("Body & Clothing"), "emoji-body-symbolic", true },
        { "monkey", N_("Animals & Nature"), "emoji-nature-symbolic", false },
        { "grapes", N_("Food & Drink"), "emoji-food-symbolic", false },
        { "globe showing Europe-Africa", N_("Travel & Places"), "emoji-travel-symbolic", false },
        { "jack-o-lantern", N_("Activities"), "emoji-activities-symbolic", false },
        { "muted speaker", _("Objects"), "emoji-objects-symbolic", false },
        { "ATM sign", N_("Symbols"), "emoji-symbols-symbolic", false },
        { "chequered flag", _("Flags"), "emoji-flags-symbolic", false }
    };

    auto* vAdjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(chooser->priv->swindow));

    GtkWidget* flowBox = nullptr;
    for (unsigned i = 0; i < G_N_ELEMENTS(sections); ++i) {
        auto* box = webkitEmojiChooserSetupSectionBox(chooser, emojiBox, sections[i].firstEmojiName, sections[i].title, vAdjustment, sections[i].canHaveVariations);
        webkitEmojiChooserSetupSectionButton(chooser, buttonBox, sections[i].iconName, sections[i].title);
        if (!i)
            flowBox = box;
    }

    GRefPtr<GBytes> bytes = adoptGRef(g_resources_lookup_data("/org/gtk/libgtk/emoji/emoji.data", G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr));
    GRefPtr<GVariant> data = g_variant_new_from_bytes(G_VARIANT_TYPE("a(auss)"), bytes.get(), TRUE);
    GUniquePtr<GVariantIter> iter(g_variant_iter_new(data.get()));

    Function<void()> populateSections = [chooser, iter = WTFMove(iter), flowBox]() mutable {
        auto start = MonotonicTime::now();
        while (GRefPtr<GVariant> item = adoptGRef(g_variant_iter_next_value(iter.get()))) {
            const char* name;
            g_variant_get_child(item.get(), 1, "&s", &name);

            auto index = chooser->priv->sections.findMatching([&name](const auto& section) {
                return !g_strcmp0(name, section.firstEmojiName);
            });
            flowBox = index == notFound ? flowBox : chooser->priv->sections[index].box;
            auto* child = webkitEmojiChooserAddEmoji(chooser, GTK_FLOW_BOX(flowBox), item.get());
            if (child)
                g_signal_connect(child, "popup-menu", G_CALLBACK(emojiPopupMenu), chooser);

            if (MonotonicTime::now() - start >= 8_ms)
                return;
        }
        chooser->priv->populateSectionsTimer = nullptr;
    };

    chooser->priv->populateSectionsTimer = makeUnique<CallbackTimer>(WTFMove(populateSections));
    chooser->priv->populateSectionsTimer->setPriority(G_PRIORITY_DEFAULT_IDLE);
    chooser->priv->populateSectionsTimer->setName("[WebKitEmojiChooser] populate sections timer");
    chooser->priv->populateSectionsTimer->startRepeating({ });
}

static void webkitEmojiChooserInitializeEmojiMaxWidth(WebKitEmojiChooser* chooser)
{
    // Get a reasonable maximum width for an emoji. We do this to skip overly wide fallback
    // rendering for certain emojis the font does not contain and therefore end up being
    // rendered as multiple glyphs.
    GRefPtr<PangoLayout> layout = adoptGRef(gtk_widget_create_pango_layout(GTK_WIDGET(chooser), "🙂"));
    auto* attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes, pango_attr_scale_new(PANGO_SCALE_X_LARGE));
    pango_layout_set_attributes(layout.get(), attributes);
    pango_attr_list_unref(attributes);

    PangoRectangle rect;
    pango_layout_get_extents(layout.get(), &rect, nullptr);
    chooser->priv->emojiMaxWidth = rect.width;
}

static void webkitEmojiChooserConstructed(GObject* object)
{
    WebKitEmojiChooser* chooser = WEBKIT_EMOJI_CHOOSER(object);
    chooser->priv->settings = adoptGRef(g_settings_new("org.gtk.Settings.EmojiChooser"));

    G_OBJECT_CLASS(webkit_emoji_chooser_parent_class)->constructed(object);

    webkitEmojiChooserInitializeEmojiMaxWidth(chooser);

    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(object)), "emoji-picker");

    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* searchEntry = gtk_search_entry_new();
    chooser->priv->searchEntry = searchEntry;
    g_signal_connect_swapped(searchEntry, "search-changed", G_CALLBACK(webkitEmojiChooserSearchChanged), chooser);
    gtk_entry_set_input_hints(GTK_ENTRY(searchEntry), GTK_INPUT_HINT_NO_EMOJI);
    gtk_box_pack_start(GTK_BOX(mainBox), searchEntry, TRUE, FALSE, 0);
    gtk_widget_show(searchEntry);

    GtkWidget* stack = gtk_stack_new();
    chooser->priv->stack = stack;
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* swindow = gtk_scrolled_window_new(nullptr, nullptr);
    chooser->priv->swindow = swindow;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(swindow), 250);
    gtk_style_context_add_class(gtk_widget_get_style_context(swindow), "view");
    gtk_box_pack_start(GTK_BOX(box), swindow, TRUE, TRUE, 0);
    gtk_widget_show(swindow);

    GtkWidget* emojiBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    g_object_set(emojiBox, "margin", 6, nullptr);
    gtk_container_add(GTK_CONTAINER(swindow), emojiBox);
    gtk_widget_show(emojiBox);

    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(box), buttonBox, TRUE, FALSE, 0);
    gtk_widget_show(buttonBox);

    GtkAdjustment* vAdjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(swindow));
    g_signal_connect(vAdjustment, "value-changed", G_CALLBACK(verticalAdjustmentChanged), chooser);

    webkitEmojiChooserSetupRecent(chooser, GTK_BOX(emojiBox), GTK_BOX(buttonBox), vAdjustment);

    webkitEmojiChooserSetupEmojiSections(chooser, GTK_BOX(emojiBox), GTK_BOX(buttonBox));

    gtk_widget_set_state_flags(chooser->priv->sections.first().button, GTK_STATE_FLAG_CHECKED, FALSE);

    gtk_stack_add_named(GTK_STACK(stack), box, "list");
    gtk_widget_show(box);

    gtk_box_pack_start(GTK_BOX(mainBox), stack, TRUE, TRUE, 0);
    gtk_widget_show(stack);

    gtk_container_add(GTK_CONTAINER(object), mainBox);
    gtk_widget_show(mainBox);

    webkitEmojiChooserSetupFilters(chooser);
}

static void webkitEmojiChooserShow(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_emoji_chooser_parent_class)->show(widget);

    WebKitEmojiChooser* chooser = WEBKIT_EMOJI_CHOOSER(widget);
    auto* adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(chooser->priv->swindow));
    gtk_adjustment_set_value(adjustment, 0);

    gtk_entry_set_text(GTK_ENTRY(chooser->priv->searchEntry), "");
}

static void webkit_emoji_chooser_class_init(WebKitEmojiChooserClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webkitEmojiChooserConstructed;

    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(klass);
    widgetClass->show = webkitEmojiChooserShow;

    signals[EMOJI_PICKED] = g_signal_new(
        "emoji-picked",
        G_OBJECT_CLASS_TYPE(objectClass),
        G_SIGNAL_RUN_LAST,
        0,
        nullptr, nullptr,
        nullptr,
        G_TYPE_NONE, 1,
        G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
}

GtkWidget* webkitEmojiChooserNew()
{
    WebKitEmojiChooser* authDialog = WEBKIT_EMOJI_CHOOSER(g_object_new(WEBKIT_TYPE_EMOJI_CHOOSER, nullptr));
    return GTK_WIDGET(authDialog);
}

#endif // GTK_CHECK_VERSION(3, 24, 0) && !USE(GTK4)
