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

#include "AXObjectCache.h"
#include "AccessibilityListBox.h"
#include "AccessibilityRenderObject.h"
#include "CString.h"
#include "NotImplemented.h"

#include <atk/atk.h>

extern "C" {

using namespace WebCore;

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
    if (G_OBJECT_CLASS(parent_class)->finalize)
        G_OBJECT_CLASS(parent_class)->finalize(object);
}

static const gchar* webkit_accessible_get_name(AtkObject* object)
{
    AccessibilityObject* coreObject = WEBKIT_ACCESSIBLE(object)->m_object;

    // TODO: Deal with later changes.
    if (!object->name)
        atk_object_set_name(object, coreObject->title().utf8().data());
    return object->name;
}

static const gchar* webkit_accessible_get_description(AtkObject* object)
{
    AccessibilityObject* coreObject = WEBKIT_ACCESSIBLE(object)->m_object;

    // TODO: the Mozilla MSAA implementation prepends "Description: "
    // Should we do this too?

    // TODO: Deal with later changes.
    if (!object->description)
        atk_object_set_description(object, coreObject->accessibilityDescription().utf8().data());
    return object->description;
}

AtkObject* webkit_accessible_get_parent(AtkObject* object)
{
    AccessibilityObject* coreParent = WEBKIT_ACCESSIBLE(object)->m_object->parentObject();

    if (!coreParent)
        return NULL;

    return coreParent->wrapper();
}

static gint webkit_accessible_get_n_children(AtkObject* object)
{
    AccessibilityObject* coreObject = WEBKIT_ACCESSIBLE(object)->m_object;
    return coreObject->children().size();
}

static AtkObject* webkit_accessible_ref_child(AtkObject* object, gint index)
{
    AccessibilityObject* coreObject = WEBKIT_ACCESSIBLE(object)->m_object;

    g_return_val_if_fail(index >= 0, NULL);
    g_return_val_if_fail(index < coreObject->children().size(), NULL);

    AccessibilityObject* coreChild = coreObject->children().at(index).get();

    if (!coreChild)
        return NULL;

    AtkObject* child = coreChild->wrapper();
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

AtkRole webkit_accessible_get_role(AtkObject* object)
{
    AccessibilityObject* coreObject = WEBKIT_ACCESSIBLE(object)->m_object;
    return atkRole(coreObject->roleValue());
}

static void webkit_accessible_class_init(AtkObjectClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

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

void webkit_accessible_detach(WebKitAccessible* accessible)
{
    accessible->m_object = 0;
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
