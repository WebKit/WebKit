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

#include "config.h"
#include "ewk_window_features.h"

#include "WindowFeatures.h"
#include "ewk_private.h"

#include <Eina.h>

/**
 * \struct  _Ewk_Window_Features
 * @brief   Contains the window features data.
 */
struct _Ewk_Window_Features {
    unsigned int __ref;
    WebCore::WindowFeatures* core;
};

/**
 * Decreases the referece count of an Ewk_Window_Features, possibly freeing it.
 *
 * When the reference count of the object reaches 0, the one is freed.
 *
 * @param window_features the object to decrease reference count
 */
void ewk_window_features_unref(Ewk_Window_Features* window_features)
{
    EINA_SAFETY_ON_NULL_RETURN(window_features);
    EINA_SAFETY_ON_FALSE_RETURN(window_features->__ref > 0);

    if (--window_features->__ref)
        return;

    delete window_features->core;
    window_features->core = 0;
    free(window_features);
}

/**
 * Increases the reference count of an Ewk_Window_Features.
 *
 * @param window_features the object to increase reference count
 */
void ewk_window_features_ref(Ewk_Window_Features* window_features)
{
    EINA_SAFETY_ON_NULL_RETURN(window_features);
    window_features->__ref++;
}

/**
 * Gets boolean properties of an Ewk_Window_Features.
 *
 * Properties are returned in the respective pointers. Passing @c 0 to any of
 * these pointers will make that property to not be returned.
 *
 * @param window_features the object to get boolean properties
 * @param toolbar_visible the pointer to store if toolbar is visible
 * @param statusbar_visible the pointer to store if statusbar is visible
 * @param scrollbars_visible the pointer to store if scrollbars is visible
 * @param menubar_visible the pointer to store if menubar is visible
 * @param locationbar_visible the pointer to store if locationbar is visible
 * @param fullscreen the pointer to store if fullscreen is enabled
 *
 * @see ewk_window_features_int_property_get
 */
void ewk_window_features_bool_property_get(Ewk_Window_Features* window_features, Eina_Bool* toolbar_visible, Eina_Bool* statusbar_visible, Eina_Bool* scrollbars_visible, Eina_Bool* menubar_visible, Eina_Bool* locationbar_visible, Eina_Bool* fullscreen)
{
    EINA_SAFETY_ON_NULL_RETURN(window_features);
    EINA_SAFETY_ON_NULL_RETURN(window_features->core);

    if (toolbar_visible)
        *toolbar_visible = window_features->core->toolBarVisible;

    if (statusbar_visible)
        *statusbar_visible = window_features->core->statusBarVisible;

    if (scrollbars_visible)
        *scrollbars_visible = window_features->core->scrollbarsVisible;

    if (menubar_visible)
        *menubar_visible = window_features->core->menuBarVisible;

    if (locationbar_visible)
        *locationbar_visible = window_features->core->locationBarVisible;

    if (fullscreen)
        *fullscreen = window_features->core->fullscreen;
}

/**
 * Gets int properties of an Ewk_Window_Features.
 *
 * Properties are returned in the respective pointers. Passing @c 0 to any of
 * these pointers will make that property to not be returned.
 *
 * Make sure to check if the value returned is less than 0 before using it, since in
 * that case it means that property was not set in winwdow_features object.
 *
 * @param window_features the window's features
 * @param x the pointer to store x position
 * @param y the pointer to store y position
 * @param w the pointer to store width
 * @param h the pointer to store height
 *
 * @see ewk_window_features_bool_property_get
 */
void ewk_window_features_int_property_get(Ewk_Window_Features* window_features, int* x, int* y, int* w, int* h)
{
    EINA_SAFETY_ON_NULL_RETURN(window_features);
    EINA_SAFETY_ON_NULL_RETURN(window_features->core);

    if (x)
        *x = window_features->core->xSet ? static_cast<int>(window_features->core->x) : -1;

    if (y)
        *y = window_features->core->ySet ? static_cast<int>(window_features->core->y) : -1;

    if (w)
        *w = window_features->core->widthSet ? static_cast<int>(window_features->core->width) : -1;

    if (h)
        *h = window_features->core->heightSet ? static_cast<int>(window_features->core->height) : -1;
}

/* internal methods ****************************************************/

/**
 * @internal
 *
 * Creates a new Ewk_Window_Features object.
 *
 * @param core if not @c 0 a new WebCore::WindowFeatures is allocated copying core features and
 * it is embedded inside the Ewk_Window_Features whose ref count is initialized, if core is @c 0 a new one is created with the default features.
 * @returns a new allocated the Ewk_Window_Features object
 */
Ewk_Window_Features* ewk_window_features_new_from_core(const WebCore::WindowFeatures* core)
{
    Ewk_Window_Features* window_features = static_cast<Ewk_Window_Features*>(malloc(sizeof(*window_features)));

    if (core)
        window_features->core = new WebCore::WindowFeatures(*core);
    else
        window_features->core = new WebCore::WindowFeatures();

    window_features->__ref = 1;

    return window_features;
}
