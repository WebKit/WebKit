/*
 * Copyright (C) 2021 Igalia S.L.
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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitMemoryPressureSettings_h
#define WebKitMemoryPressureSettings_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_MEMORY_PRESSURE_SETTINGS (webkit_memory_pressure_settings_get_type())

typedef struct _WebKitMemoryPressureSettings WebKitMemoryPressureSettings;

WEBKIT_API GType
webkit_memory_pressure_settings_get_type                   (void);

WEBKIT_API WebKitMemoryPressureSettings *
webkit_memory_pressure_settings_new                        (void);

WEBKIT_API WebKitMemoryPressureSettings *
webkit_memory_pressure_settings_copy                       (WebKitMemoryPressureSettings *settings);

WEBKIT_API void
webkit_memory_pressure_settings_free                       (WebKitMemoryPressureSettings *settings);

WEBKIT_API void
webkit_memory_pressure_settings_set_memory_limit           (WebKitMemoryPressureSettings *settings,
                                                            guint                         memory_limit);

WEBKIT_API guint
webkit_memory_pressure_settings_get_memory_limit           (WebKitMemoryPressureSettings *settings);

WEBKIT_API void
webkit_memory_pressure_settings_set_conservative_threshold (WebKitMemoryPressureSettings *settings,
                                                            gdouble                       value);

WEBKIT_API gdouble
webkit_memory_pressure_settings_get_conservative_threshold (WebKitMemoryPressureSettings *settings);

WEBKIT_API void
webkit_memory_pressure_settings_set_strict_threshold       (WebKitMemoryPressureSettings *settings,
                                                            gdouble                       value);

WEBKIT_API gdouble
webkit_memory_pressure_settings_get_strict_threshold       (WebKitMemoryPressureSettings *settings);

WEBKIT_API void
webkit_memory_pressure_settings_set_kill_threshold         (WebKitMemoryPressureSettings *settings,
                                                            gdouble                       value);

WEBKIT_API gdouble
webkit_memory_pressure_settings_get_kill_threshold         (WebKitMemoryPressureSettings *settings);

WEBKIT_API void
webkit_memory_pressure_settings_set_poll_interval          (WebKitMemoryPressureSettings *settings,
                                                            gdouble                       value);

WEBKIT_API gdouble
webkit_memory_pressure_settings_get_poll_interval          (WebKitMemoryPressureSettings *settings);

G_END_DECLS

#endif /* WebKitMemoryPressureSettings_h */
