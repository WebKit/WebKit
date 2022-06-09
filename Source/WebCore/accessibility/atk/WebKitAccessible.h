/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2009, 2010, 2011, 2012, 2019 Igalia S.L.
 * Copyright (C) 2013 Samsung Electronics
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

#pragma once

#if ENABLE(ACCESSIBILITY) && USE(ATK)

#include <atk/atk.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class AccessibilityObject;
}

G_BEGIN_DECLS

#define WEBKIT_TYPE_ACCESSIBLE            (webkit_accessible_get_type())
#define WEBKIT_ACCESSIBLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_ACCESSIBLE, WebKitAccessible))
#define WEBKIT_ACCESSIBLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_ACCESSIBLE, WebKitAccessibleClass))
#define WEBKIT_IS_ACCESSIBLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_ACCESSIBLE))
#define WEBKIT_IS_ACCESSIBLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_ACCESSIBLE))
#define WEBKIT_ACCESSIBLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), WEBKIT_TYPE_ACCESSIBLE, WebKitAccessibleClass))

typedef struct _WebKitAccessible        WebKitAccessible;
typedef struct _WebKitAccessibleClass   WebKitAccessibleClass;
typedef struct _WebKitAccessiblePrivate WebKitAccessiblePrivate;


struct _WebKitAccessible {
    AtkObject parent;

    WebKitAccessiblePrivate *priv;
};

struct _WebKitAccessibleClass {
    AtkObjectClass parentClass;
};

GType webkit_accessible_get_type(void);

G_END_DECLS

// The definitions below are only used in C++ code, and some use C++ types,
// therefore they should be outside of the G_BEGIN_DECLS/G_END_DECLS block
// to make them use the C++ ABI.

enum AtkCachedProperty {
    AtkCachedAccessibleName,
    AtkCachedAccessibleDescription,
    AtkCachedActionName,
    AtkCachedActionKeyBinding,
    AtkCachedDocumentLocale,
    AtkCachedDocumentType,
    AtkCachedDocumentEncoding,
    AtkCachedDocumentURI,
    AtkCachedImageDescription
};

WebKitAccessible* webkitAccessibleNew(WebCore::AccessibilityObject*);

WebCore::AccessibilityObject& webkitAccessibleGetAccessibilityObject(WebKitAccessible*);

void webkitAccessibleDetach(WebKitAccessible*);

bool webkitAccessibleIsDetached(WebKitAccessible*);

const char* webkitAccessibleCacheAndReturnAtkProperty(WebKitAccessible*, AtkCachedProperty, CString&&);

#endif // ENABLE(ACCESSIBILITY) && USE(ATK)
