/*
    Copyright (C) 2010 ProFUSION embedded systems
    Copyright (C) 2010 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

/**
 * @file    ewk_window_features.h
 * @brief   Access to the features of window.
 */

#ifndef ewk_window_features_h
#define ewk_window_features_h

#include "ewk_eapi.h"

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Creates a type name for _Ewk_Window_Features. */
typedef struct _Ewk_Window_Features Ewk_Window_Features;

EAPI void         ewk_window_features_unref(Ewk_Window_Features* window_features);
EAPI void         ewk_window_features_ref(Ewk_Window_Features* window_features);

EAPI void         ewk_window_features_bool_property_get(Ewk_Window_Features* window_features, Eina_Bool* toolbar_visible, Eina_Bool* statusbar_visible, Eina_Bool* scrollbars_visible, Eina_Bool* menubar_visible, Eina_Bool* locationbar_visible, Eina_Bool* fullscreen);
EAPI void         ewk_window_features_int_property_get(Ewk_Window_Features* window_features, int* x, int* y, int* w, int* h);

#ifdef __cplusplus
}
#endif
#endif // ewk_window_features_h
