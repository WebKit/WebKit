/*
 * Copyright (C) 2008 Nuanti Ltd.
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

#include "config.h"
#include "AccessibilityObjectWrapperAtk.h"

#if HAVE(ACCESSIBILITY)

#include "AXObjectCache.h"
#include "AccessibilityListBox.h"
#include "AccessibilityRenderObject.h"
#include "AtomicString.h"
#include "CString.h"
#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "FrameView.h"
#include "IntRect.h"
#include "NotImplemented.h"

#include <atk/atk.h>

using namespace WebCore;

// Used to provide const char* returns.
static const char* returnString(const String& str)
{
    static CString returnedString;
    returnedString = str.utf8();
    return returnedString.data();
}

static AccessibilityObject* core(WebKitAccessible* accessible)
{
    if (!accessible)
        return 0;

    return accessible->m_object;
}

static AccessibilityObject* core(AtkObject* object)
{
    if (!WEBKIT_IS_ACCESSIBLE(object))
        return 0;

    return core(WEBKIT_ACCESSIBLE(object));
}

static AccessibilityObject* core(AtkAction* action)
{
    return core(ATK_OBJECT(action));
}

static AccessibilityObject* core(AtkStreamableContent* streamable)
{
    return core(ATK_OBJECT(streamable));
}

static AccessibilityObject* core(AtkText* text)
{
    return core(ATK_OBJECT(text));
}

static AccessibilityObject* core(AtkEditableText* text)
{
    return core(ATK_OBJECT(text));
}

extern "C" {

static gpointer parent_class = NULL;

static void webkit_accessible_init(AtkObject* object, gpointer data)
{
    g_return_if_fail(WEBKIT_IS_ACCESSIBLE(object));
    g_return_if_fail(data);

    if (ATK_OBJECT_CLASS(parent_class)->initialize)
        ATK_OBJECT_CLASS(parent_class)->initialize(object, data);

    WEBKIT_ACCESSIBLE(object)->m_object = reinterpret_cast<AccessibilityObject*>(data);
}

static void webkit_accessible_finalize(GObject* object)
{
    // This is a good time to clear the return buffer.
    returnString(String());

    if (G_OBJECT_CLASS(parent_class)->finalize)
        G_OBJECT_CLASS(parent_class)->finalize(object);
}

static const gchar* webkit_accessible_get_name(AtkObject* object)
{
    // TODO: Deal with later changes.
    if (!object->name)
        atk_object_set_name(object, core(object)->stringValue().utf8().data());
    return object->name;
}

static const gchar* webkit_accessible_get_description(AtkObject* object)
{
    // TODO: the Mozilla MSAA implementation prepends "Description: "
    // Should we do this too?

    // TODO: Deal with later changes.
    if (!object->description)
        atk_object_set_description(object, core(object)->accessibilityDescription().utf8().data());
    return object->description;
}

static AtkObject* webkit_accessible_get_parent(AtkObject* object)
{
    AccessibilityObject* coreParent = core(object)->parentObject();

    if (!coreParent)
        return NULL;

    return coreParent->wrapper();
}

static gint webkit_accessible_get_n_children(AtkObject* object)
{
    return core(object)->children().size();
}

static AtkObject* webkit_accessible_ref_child(AtkObject* object, gint index)
{
    AccessibilityObject* coreObject = core(object);

    g_return_val_if_fail(index >= 0, NULL);
    g_return_val_if_fail(static_cast<size_t>(index) < coreObject->children().size(), NULL);

    AccessibilityObject* coreChild = coreObject->children().at(index).get();

    if (!coreChild)
        return NULL;

    AtkObject* child = coreChild->wrapper();
    // TODO: Should we call atk_object_set_parent() here?
    //atk_object_set_parent(child, object);
    g_object_ref(child);

    return child;
}

static gint webkit_accessible_get_index_in_parent(AtkObject* object)
{
    // FIXME: This needs to be implemented.
    notImplemented();
    return 0;
}

static AtkRole atkRole(AccessibilityRole role)
{
    switch (role) {
    case WebCore::ButtonRole:
        return ATK_ROLE_PUSH_BUTTON;
    case WebCore::RadioButtonRole:
        return ATK_ROLE_RADIO_BUTTON;
    case WebCore::CheckBoxRole:
        return ATK_ROLE_CHECK_BOX;
    case WebCore::SliderRole:
        return ATK_ROLE_SLIDER;
    case WebCore::TabGroupRole:
        return ATK_ROLE_PAGE_TAB_LIST;
    case WebCore::TextFieldRole:
    case WebCore::TextAreaRole:
    case WebCore::ListMarkerRole:
        return ATK_ROLE_ENTRY;
    case WebCore::StaticTextRole:
        return ATK_ROLE_TEXT; //?
    case WebCore::OutlineRole:
        return ATK_ROLE_TREE;
    case WebCore::ColumnRole:
        return ATK_ROLE_UNKNOWN; //?
    case WebCore::RowRole:
        return ATK_ROLE_LIST_ITEM;
    case WebCore::GroupRole:
        return ATK_ROLE_UNKNOWN; //?
    case WebCore::ListRole:
        return ATK_ROLE_LIST;
    case WebCore::TableRole:
        return ATK_ROLE_TABLE;
    case WebCore::LinkRole:
    case WebCore::WebCoreLinkRole:
        return ATK_ROLE_LINK;
    case WebCore::ImageMapRole:
    case WebCore::ImageRole:
        return ATK_ROLE_IMAGE;
    default:
        return ATK_ROLE_UNKNOWN;
    }
}

static AtkRole webkit_accessible_get_role(AtkObject* object)
{
    return atkRole(core(object)->roleValue());
}

static void webkit_accessible_class_init(AtkObjectClass* klass)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);

    parent_class = g_type_class_peek_parent(klass);

    klass->initialize = webkit_accessible_init;

    gobject_class->finalize = webkit_accessible_finalize;

    klass->get_name = webkit_accessible_get_name;
    klass->get_description = webkit_accessible_get_description;
    klass->get_parent = webkit_accessible_get_parent;
    klass->get_n_children = webkit_accessible_get_n_children;
    klass->ref_child = webkit_accessible_ref_child;
    //klass->get_index_in_parent = webkit_accessible_get_index_in_parent;
    klass->get_role = webkit_accessible_get_role;
    //klass->get_attributes = webkit_accessible_get_attributes;
    //klass->ref_state_set = webkit_accessible_ref_state_set;
    //klass->ref_relation_set = webkit_accessible_ref_relation_set;
}

static gboolean webkit_accessible_action_do_action(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, FALSE);
    return core(action)->performDefaultAction();
}

static gint webkit_accessible_action_get_n_actions(AtkAction* action)
{
    return 1;
}

static const gchar* webkit_accessible_action_get_description(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, NULL);
    // TODO: Need a way to provide/localize action descriptions.
    notImplemented();
    return "";
}

static const gchar* webkit_accessible_action_get_keybinding(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, NULL);
    // FIXME: Construct a proper keybinding string.
    return returnString(core(action)->accessKey().string());
}

static const gchar* webkit_accessible_action_get_name(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, NULL);
    return returnString(core(action)->actionVerb());
}

static void atk_action_interface_init(AtkActionIface* iface)
{
    g_return_if_fail(iface);

    iface->do_action = webkit_accessible_action_do_action;
    iface->get_n_actions = webkit_accessible_action_get_n_actions;
    iface->get_description = webkit_accessible_action_get_description;
    iface->get_keybinding = webkit_accessible_action_get_keybinding;
    iface->get_name = webkit_accessible_action_get_name;
}

// Text

static gchar* webkit_accessible_text_get_text(AtkText* text, gint start_offset, gint end_offset)
{
    String ret = core(text)->doAXStringForRange(PlainTextRange(start_offset, end_offset - start_offset));
    // TODO: Intentionally copied?
    return g_strdup(ret.utf8().data());
}

static gchar* webkit_accessible_text_get_text_after_offset(AtkText* text, gint offset, AtkTextBoundary boundary_type, gint* start_offset, gint* end_offset)
{
    notImplemented();
    return NULL;
}

static gchar* webkit_accessible_text_get_text_at_offset(AtkText* text, gint offset, AtkTextBoundary boundary_type, gint* start_offset, gint* end_offset)
{
    notImplemented();
    return NULL;
}

static gunichar webkit_accessible_text_get_character_at_offset(AtkText* text, gint offset)
{
    notImplemented();
    return 0;
}

static gchar* webkit_accessible_text_get_text_before_offset(AtkText* text, gint offset, AtkTextBoundary boundary_type, gint* start_offset, gint* end_offset)
{
    notImplemented();
    return NULL;
}

static gint webkit_accessible_text_get_caret_offset(AtkText* text)
{
    // TODO: Verify this, especially for RTL text.
    return core(text)->selectionStart();
}

static AtkAttributeSet* webkit_accessible_text_get_run_attributes(AtkText* text, gint offset, gint* start_offset, gint* end_offset)
{
    notImplemented();
    return NULL;
}

static AtkAttributeSet* webkit_accessible_text_get_default_attributes(AtkText* text)
{
    notImplemented();
    return NULL;
}

static void webkit_accessible_text_get_character_extents(AtkText* text, gint offset, gint* x, gint* y, gint* width, gint* height, AtkCoordType coords)
{
    IntRect extents = core(text)->doAXBoundsForRange(PlainTextRange(offset, 1));
    // FIXME: Use the AtkCoordType
    // Requires WebCore::ScrollView::contentsToScreen() to be implemented

#if 0
    switch(coords) {
    case ATK_XY_SCREEN:
        extents = core(text)->document()->view()->contentsToScreen(extents);
        break;
    case ATK_XY_WINDOW:
        // No-op
        break;
    }
#endif

    *x = extents.x();
    *y = extents.y();
    *width = extents.width();
    *height = extents.height();
}

static gint webkit_accessible_text_get_character_count(AtkText* text)
{
    return core(text)->textLength();
}

static gint webkit_accessible_text_get_offset_at_point(AtkText* text, gint x, gint y, AtkCoordType coords)
{
    // FIXME: Use the AtkCoordType
    // TODO: Is it correct to ignore range.length?
    IntPoint pos(x, y);
    PlainTextRange range = core(text)->doAXRangeForPosition(pos);
    return range.start;
}

static gint webkit_accessible_text_get_n_selections(AtkText* text)
{
    notImplemented();
    return 0;
}

static gchar* webkit_accessible_text_get_selection(AtkText* text, gint selection_num, gint* start_offset, gint* end_offset)
{
    notImplemented();
    return NULL;
}

static gboolean webkit_accessible_text_add_selection(AtkText* text, gint start_offset, gint end_offset)
{
    notImplemented();
    return FALSE;
}

static gboolean webkit_accessible_text_remove_selection(AtkText* text, gint selection_num)
{
    notImplemented();
    return FALSE;
}

static gboolean webkit_accessible_text_set_selection(AtkText* text, gint selection_num, gint start_offset, gint end_offset)
{
    notImplemented();
    return FALSE;
}

static gboolean webkit_accessible_text_set_caret_offset(AtkText* text, gint offset)
{
    // TODO: Verify
    //core(text)->setSelectedTextRange(PlainTextRange(offset, 0));
    AccessibilityObject* coreObject = core(text);
    coreObject->setSelectedVisiblePositionRange(coreObject->visiblePositionRangeForRange(PlainTextRange(offset, 0)));
    return TRUE;
}

#if 0
// Signal handlers
static void webkit_accessible_text_text_changed(AtkText* text, gint position, gint length)
{
}

static void webkit_accessible_text_text_caret_moved(AtkText* text, gint location)
{
}

static void webkit_accessible_text_text_selection_changed(AtkText* text)
{
}

static void webkit_accessible_text_text_attributes_changed(AtkText* text)
{
}

static void webkit_accessible_text_get_range_extents(AtkText* text, gint start_offset, gint end_offset, AtkCoordType coord_type, AtkTextRectangle* rect)
{
}

static AtkTextRange** webkit_accessible_text_get_bounded_ranges(AtkText* text, AtkTextRectangle* rect, AtkCoordType coord_type, AtkTextClipType x_clip_type, AtkTextClipType y_clip_type)
{
}
#endif

static void atk_text_interface_init(AtkTextIface* iface)
{
    g_return_if_fail(iface);

    iface->get_text = webkit_accessible_text_get_text;
    iface->get_text_after_offset = webkit_accessible_text_get_text_after_offset;
    iface->get_text_at_offset = webkit_accessible_text_get_text_at_offset;
    iface->get_character_at_offset = webkit_accessible_text_get_character_at_offset;
    iface->get_text_before_offset = webkit_accessible_text_get_text_before_offset;
    iface->get_caret_offset = webkit_accessible_text_get_caret_offset;
    iface->get_run_attributes = webkit_accessible_text_get_run_attributes;
    iface->get_default_attributes = webkit_accessible_text_get_default_attributes;
    iface->get_character_extents = webkit_accessible_text_get_character_extents;
    //iface->get_range_extents = ;
    iface->get_character_count = webkit_accessible_text_get_character_count;
    iface->get_offset_at_point = webkit_accessible_text_get_offset_at_point;
    iface->get_n_selections = webkit_accessible_text_get_n_selections;
    iface->get_selection = webkit_accessible_text_get_selection;

    // set methods
    iface->add_selection = webkit_accessible_text_add_selection;
    iface->remove_selection = webkit_accessible_text_remove_selection;
    iface->set_selection = webkit_accessible_text_set_selection;
    iface->set_caret_offset = webkit_accessible_text_set_caret_offset;
}

// EditableText

static gboolean webkit_accessible_editable_text_set_run_attributes(AtkEditableText* text, AtkAttributeSet* attrib_set, gint start_offset, gint end_offset)
{
    notImplemented();
    return FALSE;
}

static void webkit_accessible_editable_text_set_text_contents(AtkEditableText* text, const gchar* string)
{
    // FIXME: string nullcheck?
    core(text)->setValue(String::fromUTF8(string));
}

static void webkit_accessible_editable_text_insert_text(AtkEditableText* text, const gchar* string, gint length, gint* position)
{
    // FIXME: string nullcheck?

    AccessibilityObject* coreObject = core(text);
    // FIXME: Not implemented in WebCore
    //coreObject->setSelectedTextRange(PlainTextRange(*position, 0));
    //coreObject->setSelectedText(String::fromUTF8(string));

    if (!coreObject->document() || !coreObject->document()->frame())
        return;
    coreObject->setSelectedVisiblePositionRange(coreObject->visiblePositionRangeForRange(PlainTextRange(*position, 0)));
    coreObject->setFocused(true);
    // FIXME: We should set position to the actual inserted text length, which may be less than that requested.
    if (coreObject->document()->frame()->editor()->insertTextWithoutSendingTextEvent(String::fromUTF8(string), false, 0))
        *position += length;
}

static void webkit_accessible_editable_text_copy_text(AtkEditableText* text, gint start_pos, gint end_pos)
{
    notImplemented();
}

static void webkit_accessible_editable_text_cut_text(AtkEditableText* text, gint start_pos, gint end_pos)
{
    notImplemented();
}

static void webkit_accessible_editable_text_delete_text(AtkEditableText* text, gint start_pos, gint end_pos)
{
    AccessibilityObject* coreObject = core(text);
    // FIXME: Not implemented in WebCore
    //coreObject->setSelectedTextRange(PlainTextRange(start_pos, end_pos - start_pos));
    //coreObject->setSelectedText(String());

    if (!coreObject->document() || !coreObject->document()->frame())
        return;
    coreObject->setSelectedVisiblePositionRange(coreObject->visiblePositionRangeForRange(PlainTextRange(start_pos, end_pos - start_pos)));
    coreObject->setFocused(true);
    coreObject->document()->frame()->editor()->performDelete();
}

static void webkit_accessible_editable_text_paste_text(AtkEditableText* text, gint position)
{
    notImplemented();
}

static void atk_editable_text_interface_init(AtkEditableTextIface* iface)
{
    g_return_if_fail(iface);

    iface->set_run_attributes = webkit_accessible_editable_text_set_run_attributes;
    iface->set_text_contents = webkit_accessible_editable_text_set_text_contents;
    iface->insert_text = webkit_accessible_editable_text_insert_text;
    iface->copy_text = webkit_accessible_editable_text_copy_text;
    iface->cut_text = webkit_accessible_editable_text_cut_text;
    iface->delete_text = webkit_accessible_editable_text_delete_text;
    iface->paste_text = webkit_accessible_editable_text_paste_text;
}

// StreamableContent

static gint webkit_accessible_streamable_content_get_n_mime_types(AtkStreamableContent* streamable)
{
    notImplemented();
    return 0;
}

static G_CONST_RETURN gchar* webkit_accessible_streamable_content_get_mime_type(AtkStreamableContent* streamable, gint i)
{
    notImplemented();
    return "";
}

static GIOChannel* webkit_accessible_streamable_content_get_stream(AtkStreamableContent* streamable, const gchar* mime_type)
{
    notImplemented();
    return NULL;
}

static G_CONST_RETURN gchar* webkit_accessible_streamable_content_get_uri(AtkStreamableContent* streamable, const gchar* mime_type)
{
    notImplemented();
    return NULL;
}

static void atk_streamable_content_interface_init(AtkStreamableContentIface* iface)
{
    g_return_if_fail(iface);

    iface->get_n_mime_types = webkit_accessible_streamable_content_get_n_mime_types;
    iface->get_mime_type = webkit_accessible_streamable_content_get_mime_type;
    iface->get_stream = webkit_accessible_streamable_content_get_stream;
    iface->get_uri = webkit_accessible_streamable_content_get_uri;
}

GType webkit_accessible_get_type()
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(WebKitAccessibleClass),
            (GBaseInitFunc)NULL,
            (GBaseFinalizeFunc)NULL,
            (GClassInitFunc)webkit_accessible_class_init,
            (GClassFinalizeFunc)NULL,
            NULL, /* class data */
            sizeof(WebKitAccessible), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc)NULL,
            NULL /* value table */
        };

        type = g_type_register_static(ATK_TYPE_OBJECT, "WebKitAccessible", &tinfo, static_cast<GTypeFlags>(0));

        // TODO: Only implement interfaces when necessary, not for all objects.
        static const GInterfaceInfo atk_action_info =
        {
            (GInterfaceInitFunc) atk_action_interface_init,
            (GInterfaceFinalizeFunc) NULL,
            NULL
        };
        g_type_add_interface_static(type, ATK_TYPE_ACTION, &atk_action_info);

        static const GInterfaceInfo atk_text_info =
        {
            (GInterfaceInitFunc) atk_text_interface_init,
            (GInterfaceFinalizeFunc) NULL,
            NULL
        };
        g_type_add_interface_static(type, ATK_TYPE_TEXT, &atk_text_info);

        static const GInterfaceInfo atk_editable_text_info =
        {
            (GInterfaceInitFunc) atk_editable_text_interface_init,
            (GInterfaceFinalizeFunc) NULL,
            NULL
        };
        g_type_add_interface_static(type, ATK_TYPE_EDITABLE_TEXT, &atk_editable_text_info);

        static const GInterfaceInfo atk_streamable_content_info =
        {
            (GInterfaceInitFunc) atk_streamable_content_interface_init,
            (GInterfaceFinalizeFunc) NULL,
            NULL
        };
        g_type_add_interface_static(type, ATK_TYPE_STREAMABLE_CONTENT, &atk_streamable_content_info);
    }
    return type;
}

WebKitAccessible* webkit_accessible_new(AccessibilityObject* coreObject)
{
    GType type = WEBKIT_TYPE_ACCESSIBLE;
    AtkObject* object = static_cast<AtkObject*>(g_object_new(type, NULL));
    atk_object_initialize(object, coreObject);
    return WEBKIT_ACCESSIBLE(object);
}

AccessibilityObject* webkit_accessible_get_accessibility_object(WebKitAccessible* accessible)
{
    return accessible->m_object;
}

// FIXME: Remove this static initialization.
static AXObjectCache* fallbackCache = new AXObjectCache();

void webkit_accessible_detach(WebKitAccessible* accessible)
{
    ASSERT(accessible->m_object);

    // We replace the WebCore AccessibilityObject with a fallback object that
    // provides default implementations to avoid repetitive null-checking after
    // detachment.

    // FIXME: Using fallbackCache->get(ListBoxOptionRole) is a hack.
    accessible->m_object = fallbackCache->get(ListBoxOptionRole);
}

}

namespace WebCore {

// AccessibilityObject implementations

AccessibilityObjectWrapper* AccessibilityObject::wrapper() const
{
    return m_wrapper;
}

void AccessibilityObject::setWrapper(AccessibilityObjectWrapper* wrapper)
{
    if (m_wrapper)
        g_object_unref(m_wrapper);

    m_wrapper = wrapper;

    if (m_wrapper)
        g_object_ref(m_wrapper);
}

} // namespace WebCore

#endif // HAVE(ACCESSIBILITY)
