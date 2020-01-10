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

#ifndef JSCValue_h
#define JSCValue_h

#include <glib-object.h>
#include <jsc/JSCDefines.h>

G_BEGIN_DECLS

#define JSC_TYPE_VALUE            (jsc_value_get_type())
#define JSC_VALUE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), JSC_TYPE_VALUE, JSCValue))
#define JSC_IS_VALUE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), JSC_TYPE_VALUE))
#define JSC_VALUE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  JSC_TYPE_VALUE, JSCValueClass))
#define JSC_IS_VALUE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  JSC_TYPE_VALUE))
#define JSC_VALUE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  JSC_TYPE_VALUE, JSCValueClass))

typedef struct _JSCValue JSCValue;
typedef struct _JSCValueClass JSCValueClass;
typedef struct _JSCValuePrivate JSCValuePrivate;

typedef struct _JSCClass JSCClass;
typedef struct _JSCContext JSCContext;

typedef enum {
    JSC_VALUE_PROPERTY_CONFIGURABLE = 1 << 0,
    JSC_VALUE_PROPERTY_ENUMERABLE   = 1 << 1,
    JSC_VALUE_PROPERTY_WRITABLE     = 1 << 2
} JSCValuePropertyFlags;

struct _JSCValue {
    GObject parent;

    /*< private >*/
    JSCValuePrivate *priv;
};

struct _JSCValueClass {
    GObjectClass parent_class;

    void (*_jsc_reserved0) (void);
    void (*_jsc_reserved1) (void);
    void (*_jsc_reserved2) (void);
    void (*_jsc_reserved3) (void);
};

JSC_API GType
jsc_value_get_type                        (void);

JSC_API JSCContext *
jsc_value_get_context                     (JSCValue             *value);

JSC_API JSCValue *
jsc_value_new_undefined                   (JSCContext           *context);

JSC_API gboolean
jsc_value_is_undefined                    (JSCValue             *value);

JSC_API JSCValue *
jsc_value_new_null                        (JSCContext           *context);

JSC_API gboolean
jsc_value_is_null                         (JSCValue             *value);

JSC_API JSCValue *
jsc_value_new_number                      (JSCContext           *context,
                                           double                number);
JSC_API gboolean
jsc_value_is_number                       (JSCValue             *value);

JSC_API double
jsc_value_to_double                       (JSCValue             *value);

JSC_API gint32
jsc_value_to_int32                        (JSCValue             *value);

JSC_API JSCValue *
jsc_value_new_boolean                     (JSCContext           *context,
                                           gboolean              value);
JSC_API gboolean
jsc_value_is_boolean                      (JSCValue             *value);

JSC_API gboolean
jsc_value_to_boolean                      (JSCValue             *value);

JSC_API JSCValue *
jsc_value_new_string                      (JSCContext           *context,
                                           const char           *string);

JSC_API JSCValue *
jsc_value_new_string_from_bytes           (JSCContext           *context,
                                           GBytes               *bytes);

JSC_API gboolean
jsc_value_is_string                       (JSCValue             *value);

JSC_API char *
jsc_value_to_string                       (JSCValue             *value);

JSC_API GBytes *
jsc_value_to_string_as_bytes              (JSCValue             *value);

JSC_API JSCValue *
jsc_value_new_array                       (JSCContext           *context,
                                           GType                 first_item_type,
                                           ...);

JSC_API JSCValue *
jsc_value_new_array_from_garray           (JSCContext           *context,
                                           GPtrArray            *array);

JSC_API JSCValue *
jsc_value_new_array_from_strv             (JSCContext           *context,
                                           const char *const    *strv);

JSC_API gboolean
jsc_value_is_array                        (JSCValue             *value);

JSC_API JSCValue *
jsc_value_new_object                      (JSCContext           *context,
                                           gpointer              instance,
                                           JSCClass             *jsc_class);

JSC_API gboolean
jsc_value_is_object                       (JSCValue             *value);

JSC_API gboolean
jsc_value_object_is_instance_of           (JSCValue             *value,
                                           const char           *name);

JSC_API void
jsc_value_object_set_property             (JSCValue             *value,
                                           const char           *name,
                                           JSCValue             *property);

JSC_API JSCValue *
jsc_value_object_get_property             (JSCValue             *value,
                                           const char           *name);

JSC_API void
jsc_value_object_set_property_at_index    (JSCValue             *value,
                                           guint                 index,
                                           JSCValue             *property);

JSC_API JSCValue *
jsc_value_object_get_property_at_index    (JSCValue             *value,
                                           guint                 index);

JSC_API gboolean
jsc_value_object_has_property             (JSCValue             *value,
                                           const char           *name);

JSC_API gboolean
jsc_value_object_delete_property          (JSCValue             *value,
                                           const char           *name);

JSC_API gchar **
jsc_value_object_enumerate_properties     (JSCValue             *value);

JSC_API JSCValue *
jsc_value_object_invoke_method            (JSCValue             *value,
                                           const char           *name,
                                           GType                 first_parameter_type,
                                           ...) G_GNUC_WARN_UNUSED_RESULT;

JSC_API JSCValue *
jsc_value_object_invoke_methodv           (JSCValue             *value,
                                           const char           *name,
                                           guint                 n_parameters,
                                           JSCValue            **parameters) G_GNUC_WARN_UNUSED_RESULT;

JSC_API void
jsc_value_object_define_property_data     (JSCValue             *value,
                                           const char           *property_name,
                                           JSCValuePropertyFlags flags,
                                           JSCValue             *property_value);

JSC_API void
jsc_value_object_define_property_accessor (JSCValue             *value,
                                           const char           *property_name,
                                           JSCValuePropertyFlags flags,
                                           GType                 property_type,
                                           GCallback             getter,
                                           GCallback             setter,
                                           gpointer              user_data,
                                           GDestroyNotify        destroy_notify);

JSC_API JSCValue *
jsc_value_new_function                    (JSCContext           *context,
                                           const char           *name,
                                           GCallback             callback,
                                           gpointer              user_data,
                                           GDestroyNotify        destroy_notify,
                                           GType                 return_type,
                                           guint                 n_params,
                                           ...);

JSC_API JSCValue *
jsc_value_new_functionv                   (JSCContext           *context,
                                           const char           *name,
                                           GCallback             callback,
                                           gpointer              user_data,
                                           GDestroyNotify        destroy_notify,
                                           GType                 return_type,
                                           guint                 n_parameters,
                                           GType                *parameter_types);

JSC_API JSCValue *
jsc_value_new_function_variadic           (JSCContext           *context,
                                           const char           *name,
                                           GCallback             callback,
                                           gpointer              user_data,
                                           GDestroyNotify        destroy_notify,
                                           GType                 return_type);

JSC_API gboolean
jsc_value_is_function                     (JSCValue             *value);

JSC_API JSCValue *
jsc_value_function_call                   (JSCValue             *value,
                                           GType                 first_parameter_type,
                                           ...) G_GNUC_WARN_UNUSED_RESULT;

JSC_API JSCValue *
jsc_value_function_callv                  (JSCValue             *value,
                                           guint                 n_parameters,
                                           JSCValue            **parameters) G_GNUC_WARN_UNUSED_RESULT;

JSC_API gboolean
jsc_value_is_constructor                  (JSCValue             *value);

JSC_API JSCValue *
jsc_value_constructor_call                (JSCValue             *value,
                                           GType                 first_parameter_type,
                                           ...);

JSC_API JSCValue *
jsc_value_constructor_callv               (JSCValue             *value,
                                           guint                 n_parameters,
                                           JSCValue            **parameters);

JSC_API JSCValue *
jsc_value_new_from_json                   (JSCContext           *context,
                                           const char           *json);

JSC_API char *
jsc_value_to_json                         (JSCValue             *value,
                                           guint                 indent);

G_END_DECLS

#endif /* JSCValue_h */
