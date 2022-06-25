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

#ifndef JSCClass_h
#define JSCClass_h

#include <glib-object.h>
#include <jsc/JSCDefines.h>
#include <jsc/JSCValue.h>

G_BEGIN_DECLS

#define JSC_TYPE_CLASS            (jsc_class_get_type())
#define JSC_CLASS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), JSC_TYPE_CLASS, JSCClass))
#define JSC_IS_CLASS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), JSC_TYPE_CLASS))

typedef struct _JSCClass JSCClass;
typedef struct _JSCClassClass JSCClassClass;

typedef struct _JSCContext JSCContext;

typedef JSCValue *(*JSCClassGetPropertyFunction)        (JSCClass   *jsc_class,
                                                         JSCContext *context,
                                                         gpointer    instance,
                                                         const char *name);
typedef gboolean (*JSCClassSetPropertyFunction)         (JSCClass   *jsc_class,
                                                         JSCContext *context,
                                                         gpointer    instance,
                                                         const char *name,
                                                         JSCValue   *value);
typedef gboolean (*JSCClassHasPropertyFunction)         (JSCClass   *jsc_class,
                                                         JSCContext *context,
                                                         gpointer    instance,
                                                         const char *name);
typedef gboolean (*JSCClassDeletePropertyFunction)      (JSCClass   *jsc_class,
                                                         JSCContext *context,
                                                         gpointer    instance,
                                                         const char *name);
typedef gchar  **(*JSCClassEnumeratePropertiesFunction) (JSCClass   *jsc_class,
                                                         JSCContext *context,
                                                         gpointer    instance);


typedef struct {
    JSCClassGetPropertyFunction get_property;
    JSCClassSetPropertyFunction set_property;
    JSCClassHasPropertyFunction has_property;
    JSCClassDeletePropertyFunction delete_property;
    JSCClassEnumeratePropertiesFunction enumerate_properties;

    /*< private >*/
    void (*_jsc_reserved0) (void);
    void (*_jsc_reserved1) (void);
    void (*_jsc_reserved2) (void);
    void (*_jsc_reserved3) (void);
} JSCClassVTable;

JSC_API GType
jsc_class_get_type                 (void);

JSC_API const char *
jsc_class_get_name                 (JSCClass      *jsc_class);

JSC_API JSCClass *
jsc_class_get_parent               (JSCClass      *jsc_class);

JSC_API JSCValue *
jsc_class_add_constructor          (JSCClass      *jsc_class,
                                    const char    *name,
                                    GCallback      callback,
                                    gpointer       user_data,
                                    GDestroyNotify destroy_notify,
                                    GType          return_type,
                                    guint          n_params,
                                    ...);

JSC_API JSCValue *
jsc_class_add_constructorv         (JSCClass      *jsc_class,
                                    const char    *name,
                                    GCallback      callback,
                                    gpointer       user_data,
                                    GDestroyNotify destroy_notify,
                                    GType          return_type,
                                    guint          n_parameters,
                                    GType         *parameter_types);

JSC_API JSCValue *
jsc_class_add_constructor_variadic (JSCClass      *jsc_class,
                                    const char    *name,
                                    GCallback      callback,
                                    gpointer       user_data,
                                    GDestroyNotify destroy_notify,
                                    GType          return_type);

JSC_API void
jsc_class_add_method               (JSCClass      *jsc_class,
                                    const char    *name,
                                    GCallback      callback,
                                    gpointer       user_data,
                                    GDestroyNotify destroy_notify,
                                    GType          return_type,
                                    guint          n_params,
                                    ...);

JSC_API void
jsc_class_add_methodv              (JSCClass      *jsc_class,
                                    const char    *name,
                                    GCallback      callback,
                                    gpointer       user_data,
                                    GDestroyNotify destroy_notify,
                                    GType          return_type,
                                    guint          n_parameters,
                                    GType         *parameter_types);

JSC_API void
jsc_class_add_method_variadic      (JSCClass      *jsc_class,
                                    const char    *name,
                                    GCallback      callback,
                                    gpointer       user_data,
                                    GDestroyNotify destroy_notify,
                                    GType          return_type);

JSC_API void
jsc_class_add_property             (JSCClass      *jsc_class,
                                    const char    *name,
                                    GType          property_type,
                                    GCallback      getter,
                                    GCallback      setter,
                                    gpointer       user_data,
                                    GDestroyNotify destroy_notify);

G_END_DECLS

#endif /* JSCClass_h */
