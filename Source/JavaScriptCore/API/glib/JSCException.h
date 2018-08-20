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

#ifndef JSCException_h
#define JSCException_h

#include <glib-object.h>
#include <jsc/JSCDefines.h>

G_BEGIN_DECLS

#define JSC_TYPE_EXCEPTION            (jsc_exception_get_type())
#define JSC_EXCEPTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), JSC_TYPE_EXCEPTION, JSCException))
#define JSC_IS_EXCEPTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), JSC_TYPE_EXCEPTION))
#define JSC_EXCEPTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  JSC_TYPE_EXCEPTION, JSCExceptionClass))
#define JSC_IS_EXCEPTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  JSC_TYPE_EXCEPTION))
#define JSC_EXCEPTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  JSC_TYPE_EXCEPTION, JSCExceptionClass))

typedef struct _JSCException JSCException;
typedef struct _JSCExceptionClass JSCExceptionClass;
typedef struct _JSCExceptionPrivate JSCExceptionPrivate;

typedef struct _JSCContext JSCContext;

struct _JSCException {
    GObject parent;

    /*< private >*/
    JSCExceptionPrivate *priv;
};

struct _JSCExceptionClass {
    GObjectClass parent_class;

    void (*_jsc_reserved0) (void);
    void (*_jsc_reserved1) (void);
    void (*_jsc_reserved2) (void);
    void (*_jsc_reserved3) (void);
};

JSC_API GType
jsc_exception_get_type              (void);

JSC_API JSCException *
jsc_exception_new                   (JSCContext   *context,
                                     const char   *message);

JSC_API JSCException *
jsc_exception_new_printf            (JSCContext   *context,
                                     const char   *format,
                                     ...) G_GNUC_PRINTF (2, 3);

JSC_API JSCException *
jsc_exception_new_vprintf           (JSCContext   *context,
                                     const char   *format,
                                     va_list       args) G_GNUC_PRINTF(2, 0);

JSC_API JSCException *
jsc_exception_new_with_name         (JSCContext   *context,
                                     const char   *name,
                                     const char   *message);

JSC_API JSCException *
jsc_exception_new_with_name_printf  (JSCContext   *context,
                                     const char   *name,
                                     const char   *format,
                                     ...) G_GNUC_PRINTF (3, 4);

JSC_API JSCException *
jsc_exception_new_with_name_vprintf (JSCContext   *context,
                                     const char   *name,
                                     const char   *format,
                                     va_list       args) G_GNUC_PRINTF (3, 0);

JSC_API const char *
jsc_exception_get_name              (JSCException *exception);

JSC_API const char *
jsc_exception_get_message           (JSCException *exception);

JSC_API guint
jsc_exception_get_line_number       (JSCException *exception);

JSC_API guint
jsc_exception_get_column_number     (JSCException *exception);

JSC_API const char *
jsc_exception_get_source_uri        (JSCException *exception);

JSC_API const char *
jsc_exception_get_backtrace_string  (JSCException *exception);

JSC_API char *
jsc_exception_to_string             (JSCException *exception);

JSC_API char *
jsc_exception_report                (JSCException *exception);

G_END_DECLS

#endif /* JSCException_h */
