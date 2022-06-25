/*
 * Copyright (C) 2019 Igalia S.L.
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

#ifndef JSCOptions_h
#define JSCOptions_h

#include <glib-object.h>
#include <jsc/JSCDefines.h>

G_BEGIN_DECLS

#define JSC_OPTIONS_USE_JIT   "useJIT"
#define JSC_OPTIONS_USE_DFG   "useDFGJIT"
#define JSC_OPTIONS_USE_FTL   "useFTLJIT"
#define JSC_OPTIONS_USE_LLINT "useLLInt"

JSC_API gboolean
jsc_options_set_boolean       (const char *option,
                               gboolean    value);
JSC_API gboolean
jsc_options_get_boolean       (const char *option,
                               gboolean   *value);

JSC_API gboolean
jsc_options_set_int           (const char *option,
                               gint        value);
JSC_API gboolean
jsc_options_get_int           (const char *option,
                               gint       *value);

JSC_API gboolean
jsc_options_set_uint          (const char *option,
                               guint       value);
JSC_API gboolean
jsc_options_get_uint          (const char *option,
                               guint      *value);

JSC_API gboolean
jsc_options_set_size          (const char *option,
                               gsize       value);
JSC_API gboolean
jsc_options_get_size          (const char *option,
                               gsize      *value);

JSC_API gboolean
jsc_options_set_double        (const char *option,
                               gdouble     value);
JSC_API gboolean
jsc_options_get_double        (const char *option,
                               gdouble    *value);

JSC_API gboolean
jsc_options_set_string        (const char *option,
                               const char *value);
JSC_API gboolean
jsc_options_get_string        (const char *option,
                               char       **value);

JSC_API gboolean
jsc_options_set_range_string  (const char *option,
                               const char *value);
JSC_API gboolean
jsc_options_get_range_string  (const char *option,
                               char       **value);

typedef enum {
    JSC_OPTION_BOOLEAN,
    JSC_OPTION_INT,
    JSC_OPTION_UINT,
    JSC_OPTION_SIZE,
    JSC_OPTION_DOUBLE,
    JSC_OPTION_STRING,
    JSC_OPTION_RANGE_STRING
} JSCOptionType;

typedef gboolean (* JSCOptionsFunc) (const char    *option,
                                     JSCOptionType  type,
                                     const char    *description,
                                     gpointer       user_data);

JSC_API void
jsc_options_foreach                 (JSCOptionsFunc function,
                                     gpointer       user_data);

JSC_API GOptionGroup *
jsc_options_get_option_group        (void);

G_END_DECLS

#endif /* JSCOptions_h */
