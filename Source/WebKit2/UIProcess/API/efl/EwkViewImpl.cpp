/*
   Copyright (C) 2011 Samsung Electronics
   Copyright (C) 2012 Intel Corporation. All rights reserved.

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
#include "EwkViewImpl.h"

#include "EflScreenUtilities.h"
#include "FindClientEfl.h"
#include "FormClientEfl.h"
#include "PageClientImpl.h"
#include "PageLoadClientEfl.h"
#include "PagePolicyClientEfl.h"
#include "PageUIClientEfl.h"
#include "PageViewportController.h"
#include "PageViewportControllerClientEfl.h"
#include "ResourceLoadClientEfl.h"
#include "WebPageProxy.h"
#include "WebPopupMenuProxyEfl.h"
#include "ewk_back_forward_list_private.h"
#include "ewk_context_private.h"
#include "ewk_favicon_database_private.h"
#include "ewk_popup_menu_item_private.h"
#include "ewk_settings_private.h"
#include "ewk_view.h"
#include "ewk_view_private.h"
#include <Ecore_Evas.h>

using namespace WebCore;
using namespace WebKit;

static void _ewk_view_commit(void* data, Ecore_IMF_Context*, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    if (!eventInfo || !priv->isImfFocused)
        return;

    priv->pageProxy->confirmComposition(String::fromUTF8(static_cast<char*>(eventInfo)));
}

static void _ewk_view_preedit_changed(void* data, Ecore_IMF_Context* context, void*)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (!priv->pageProxy->focusedFrame() || !priv->isImfFocused)
        return;

    char* buffer = 0;
    ecore_imf_context_preedit_string_get(context, &buffer, 0);
    if (!buffer)
        return;

    String preeditString = String::fromUTF8(buffer);
    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, preeditString.length(), Color(0, 0, 0), false));
    priv->pageProxy->setComposition(preeditString, underlines, 0);
}

static void _ewk_view_imf_context_destroy(Ecore_IMF_Context* imfContext)
{
    if (!imfContext)
        return;

    ecore_imf_context_event_callback_del(imfContext, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, _ewk_view_preedit_changed);
    ecore_imf_context_event_callback_del(imfContext, ECORE_IMF_CALLBACK_COMMIT, _ewk_view_commit);
    ecore_imf_context_del(imfContext);
}

static Ecore_IMF_Context* _ewk_view_imf_context_create(Ewk_View_Smart_Data* smartData)
{
    const char* defaultContextID = ecore_imf_context_default_id_get();
    if (!defaultContextID)
        return 0;

    Ecore_IMF_Context* imfContext = ecore_imf_context_add(defaultContextID);
    if (!imfContext)
        return 0;

    ecore_imf_context_event_callback_add(imfContext, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, _ewk_view_preedit_changed, smartData);
    ecore_imf_context_event_callback_add(imfContext, ECORE_IMF_CALLBACK_COMMIT, _ewk_view_commit, smartData);

    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(smartData->base.evas);
    ecore_imf_context_client_window_set(imfContext, (void*)ecore_evas_window_get(ecoreEvas));
    ecore_imf_context_client_canvas_set(imfContext, smartData->base.evas);

    return imfContext;
}

EwkViewImpl::EwkViewImpl(Evas_Object* view)
    : areMouseEventsEnabled(false)
#if ENABLE(TOUCH_EVENTS)
    , areTouchEventsEnabled(false)
#endif
    , popupMenuProxy(0)
    , popupMenuItems(0)
    , imfContext(0)
    , isImfFocused(false)
#ifdef HAVE_ECORE_X
    , isUsingEcoreX(false)
#endif
#if USE(ACCELERATED_COMPOSITING)
    , evasGl(0)
    , evasGlContext(0)
    , evasGlSurface(0)
#endif
    , m_view(view)
{
    ASSERT(view);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(view));
    ASSERT(smartData);

    imfContext = _ewk_view_imf_context_create(smartData);

#ifdef HAVE_ECORE_X
    isUsingEcoreX = WebCore::isUsingEcoreX(smartData->base.evas);
#endif
}

EwkViewImpl::~EwkViewImpl()
{
    _ewk_view_imf_context_destroy(imfContext);

    void* item;
    EINA_LIST_FREE(popupMenuItems, item)
        delete static_cast<Ewk_Popup_Menu_Item*>(item);
}
