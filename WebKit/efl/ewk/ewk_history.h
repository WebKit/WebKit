/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2010 Samsung Electronics

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

#ifndef ewk_history_h
#define ewk_history_h

#include "ewk_eapi.h"

#include <Eina.h>
#include <Evas.h>
#include <cairo.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The history (back-forward list) associated with a given ewk_view.
 *
 * Changing the history affects immediately the view, changing the
 * current uri, for example.
 *
 * When ewk_view is navigated or uris are set, history automatically
 * updates. That's why no direct access to history structure is
 * allowed.
 */
typedef struct _Ewk_History         Ewk_History;

/**
 * Represents one item from Ewk_History.
 */
typedef struct _Ewk_History_Item    Ewk_History_Item;



EAPI Eina_Bool         ewk_history_forward(Ewk_History *history);
EAPI Eina_Bool         ewk_history_back(Ewk_History *history);

EAPI Eina_Bool         ewk_history_history_item_add(Ewk_History *history, const Ewk_History_Item *item);
EAPI Eina_Bool         ewk_history_history_item_set(Ewk_History *history,  const Ewk_History_Item *item);
EAPI Ewk_History_Item *ewk_history_history_item_back_get(const Ewk_History *history);
EAPI Ewk_History_Item *ewk_history_history_item_current_get(const Ewk_History *history);
EAPI Ewk_History_Item *ewk_history_history_item_forward_get(const Ewk_History *history);
EAPI Ewk_History_Item *ewk_history_history_item_nth_get(const Ewk_History *history, int index);
EAPI Eina_Bool         ewk_history_history_item_contains(const Ewk_History *history, const Ewk_History_Item *item);

EAPI Eina_List        *ewk_history_forward_list_get(const Ewk_History *history);
EAPI Eina_List        *ewk_history_forward_list_get_with_limit(const Ewk_History *history, int limit);
EAPI int               ewk_history_forward_list_length(const Ewk_History *history);

EAPI Eina_List        *ewk_history_back_list_get(const Ewk_History *history);
EAPI Eina_List        *ewk_history_back_list_get_with_limit(const Ewk_History *history, int limit);
EAPI int               ewk_history_back_list_length(const Ewk_History *history);

EAPI int               ewk_history_limit_get(Ewk_History *history);
EAPI Eina_Bool         ewk_history_limit_set(const Ewk_History *history, int limit);

EAPI Ewk_History_Item *ewk_history_item_new(const char *uri, const char *title);
EAPI void              ewk_history_item_free(Ewk_History_Item *item);
EAPI void              ewk_history_item_list_free(Eina_List *history_items);

EAPI const char       *ewk_history_item_title_get(const Ewk_History_Item *item);
EAPI const char       *ewk_history_item_title_alternate_get(const Ewk_History_Item *item);
EAPI void              ewk_history_item_title_alternate_set(Ewk_History_Item *item, const char *title);
EAPI const char       *ewk_history_item_uri_get(const Ewk_History_Item *item);
EAPI const char       *ewk_history_item_uri_original_get(const Ewk_History_Item *item);
EAPI double            ewk_history_item_time_last_visited_get(const Ewk_History_Item *item);

EAPI cairo_surface_t  *ewk_history_item_icon_surface_get(const Ewk_History_Item *item);
EAPI Evas_Object      *ewk_history_item_icon_object_add(const Ewk_History_Item *item, Evas *canvas);

EAPI Eina_Bool         ewk_history_item_page_cache_exists(const Ewk_History_Item *item);
EAPI int               ewk_history_item_visit_count(const Ewk_History_Item *item);
EAPI Eina_Bool         ewk_history_item_visit_last_failed(const Ewk_History_Item *item);

#ifdef __cplusplus
}
#endif
#endif // ewk_history_h
