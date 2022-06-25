/*
 * Copyright (C) 2018 Igalia S.L.
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

#if !defined(__JSC_H_INSIDE__) && !defined(JSC_COMPILATION) && !defined(WEBKIT2_COMPILATION)
#error "Only <jsc/jsc.h> can be included directly."
#endif

#ifndef JSCWeakValue_h
#define JSCWeakValue_h

#include <glib-object.h>
#include <jsc/JSCDefines.h>
#include <jsc/JSCValue.h>

G_BEGIN_DECLS

#define JSC_TYPE_WEAK_VALUE            (jsc_weak_value_get_type())
#define JSC_WEAK_VALUE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), JSC_TYPE_WEAK_VALUE, JSCWeakValue))
#define JSC_IS_WEAK_VALUE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), JSC_TYPE_WEAK_VALUE))
#define JSC_WEAK_VALUE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  JSC_TYPE_WEAK_VALUE, JSCWeakValueClass))
#define JSC_IS_WEAK_VALUE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  JSC_TYPE_WEAK_VALUE))
#define JSC_WEAK_VALUE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  JSC_TYPE_WEAK_VALUE, JSCWeakValueClass))

typedef struct _JSCWeakValue JSCWeakValue;
typedef struct _JSCWeakValueClass JSCWeakValueClass;
typedef struct _JSCWeakValuePrivate JSCWeakValuePrivate;

struct _JSCWeakValue {
    GObject parent;

    /*< private >*/
    JSCWeakValuePrivate *priv;
};

struct _JSCWeakValueClass {
    GObjectClass parent_class;

    void (*_jsc_reserved0) (void);
    void (*_jsc_reserved1) (void);
    void (*_jsc_reserved2) (void);
    void (*_jsc_reserved3) (void);
};

JSC_API GType
jsc_weak_value_get_type  (void);

JSC_API JSCWeakValue *
jsc_weak_value_new       (JSCValue     *value);

JSC_API JSCValue *
jsc_weak_value_get_value (JSCWeakValue *weak_value);

G_END_DECLS

#endif /* JSCWeakValue_h */
