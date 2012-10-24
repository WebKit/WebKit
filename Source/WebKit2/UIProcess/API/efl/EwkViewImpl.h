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

#ifndef EwkViewImpl_h
#define EwkViewImpl_h

#include "RefPtrEfl.h"
#include "WKEinaSharedString.h"
#include "WKRetainPtr.h"
#include <Ecore_IMF.h>
#include <Ecore_IMF_Evas.h>
#include <Evas.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

#define EWK_VIEW_PRIV_GET(smartData, priv)                                     \
    EwkViewImpl* priv = smartData->priv

#define EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, ...)                      \
    if (!smartData) {                                                          \
        EINA_LOG_CRIT("smart data is null");                                   \
        return __VA_ARGS__;                                                    \
    }                                                                          \
    EWK_VIEW_PRIV_GET(smartData, priv);                                        \
    do {                                                                       \
        if (!priv) {                                                           \
            EINA_LOG_CRIT("no private data for object %p (%s)",                \
                smartData->self, evas_object_type_get(smartData->self));       \
            return __VA_ARGS__;                                                \
        }                                                                      \
    } while (0)

namespace WebKit {
class FindClientEfl;
class FormClientEfl;
class PageClientImpl;
class PageLoadClientEfl;
class PagePolicyClientEfl;
class PageUIClientEfl;
class PageViewportControllerClientEfl;
class PageViewportController;
class ResourceLoadClientEfl;
class WebPageProxy;
class WebPopupMenuProxyEfl;
}

class Ewk_Back_Forward_List;
class Ewk_Context;
class Ewk_Settings;

class EwkViewImpl {
public:
    explicit EwkViewImpl(Evas_Object* view);
    ~EwkViewImpl();

    OwnPtr<WebKit::PageClientImpl> pageClient;
#if USE(TILED_BACKING_STORE)
    OwnPtr<WebKit::PageViewportControllerClientEfl> pageViewportControllerClient;
    OwnPtr<WebKit::PageViewportController> pageViewportController;
#endif
    RefPtr<WebKit::WebPageProxy> pageProxy;
    OwnPtr<WebKit::PageLoadClientEfl> pageLoadClient;
    OwnPtr<WebKit::PagePolicyClientEfl> pagePolicyClient;
    OwnPtr<WebKit::PageUIClientEfl> pageUIClient;
    OwnPtr<WebKit::ResourceLoadClientEfl> resourceLoadClient;
    OwnPtr<WebKit::FindClientEfl> findClient;
    OwnPtr<WebKit::FormClientEfl> formClient;

    WKEinaSharedString url;
    WKEinaSharedString title;
    WKEinaSharedString theme;
    WKEinaSharedString customEncoding;
    WKEinaSharedString cursorGroup;
    WKEinaSharedString faviconURL;
    RefPtr<Evas_Object> cursorObject;
    OwnPtr<Ewk_Back_Forward_List> backForwardList;
    OwnPtr<Ewk_Settings> settings;
    bool areMouseEventsEnabled;
    WKRetainPtr<WKColorPickerResultListenerRef> colorPickerResultListener;
    RefPtr<Ewk_Context> context;
#if ENABLE(TOUCH_EVENTS)
    bool areTouchEventsEnabled;
#endif

    WebKit::WebPopupMenuProxyEfl* popupMenuProxy;
    Eina_List* popupMenuItems;

    Ecore_IMF_Context* imfContext;
    bool isImfFocused;

#ifdef HAVE_ECORE_X
    bool isUsingEcoreX;
#endif

#if USE(ACCELERATED_COMPOSITING)
    Evas_GL* evasGl;
    Evas_GL_Context* evasGlContext;
    Evas_GL_Surface* evasGlSurface;
#endif

private:
    Evas_Object* m_view;
};

#endif // EwkViewImpl_h
