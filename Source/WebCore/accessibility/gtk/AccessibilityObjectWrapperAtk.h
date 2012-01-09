/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
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

#ifndef AccessibilityObjectWrapperAtk_h
#define AccessibilityObjectWrapperAtk_h

#include <atk/atk.h>

namespace WebCore {
    class AccessibilityObject;
}

G_BEGIN_DECLS

#define WEBKIT_TYPE_ACCESSIBLE                  (webkit_accessible_get_type ())
#define WEBKIT_ACCESSIBLE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_ACCESSIBLE, WebKitAccessible))
#define WEBKIT_ACCESSIBLE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_ACCESSIBLE, WebKitAccessibleClass))
#define WEBKIT_IS_ACCESSIBLE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_ACCESSIBLE))
#define WEBKIT_IS_ACCESSIBLE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_ACCESSIBLE))
#define WEBKIT_ACCESSIBLE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), WEBKIT_TYPE_ACCESSIBLE, WebKitAccessibleClass))

typedef struct _WebKitAccessible                WebKitAccessible;
typedef struct _WebKitAccessibleClass           WebKitAccessibleClass;

struct _WebKitAccessible
{
    AtkObject parent;
    WebCore::AccessibilityObject* m_object;
};

struct _WebKitAccessibleClass
{
    AtkObjectClass parent_class;
};

GType webkit_accessible_get_type (void) G_GNUC_CONST;

WebKitAccessible* webkit_accessible_new (WebCore::AccessibilityObject* core_object);

WebCore::AccessibilityObject* webkit_accessible_get_accessibility_object (WebKitAccessible* accessible);

void webkit_accessible_detach (WebKitAccessible* accessible);

AtkObject* webkit_accessible_get_focused_element(WebKitAccessible* accessible);

WebCore::AccessibilityObject* objectFocusedAndCaretOffsetUnignored(WebCore::AccessibilityObject*, int& offset);

G_END_DECLS

#endif // AccessibilityObjectWrapperAtk_h
