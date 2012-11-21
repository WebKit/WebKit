/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ewk_window_features.h"

#include "WebNumber.h"
#include "ewk_window_features_private.h"
#include <Eina.h>

using namespace WebKit;

EwkWindowFeatures::EwkWindowFeatures(ImmutableDictionary* windowFeatures, EwkViewImpl* viewImpl)
    : m_viewImpl(viewImpl)
    , m_geometry(0, 0, 100, 100)
    , m_toolbarVisible(true)
    , m_statusBarVisible(true)
    , m_scrollbarsVisible(true)
    , m_menuBarVisible(true)
    , m_locationBarVisible(true)
    , m_resizable(true)
    , m_fullScreen(false)
{
    if (windowFeatures) {
        m_geometry.setX(getWindowFeatureValue<double, WebDouble>(windowFeatures, ASCIILiteral("x")));
        m_geometry.setY(getWindowFeatureValue<double, WebDouble>(windowFeatures, ASCIILiteral("y")));
        m_geometry.setWidth(getWindowFeatureValue<double, WebDouble>(windowFeatures, ASCIILiteral("width")));
        m_geometry.setHeight(getWindowFeatureValue<double, WebDouble>(windowFeatures, ASCIILiteral("height")));

        m_toolbarVisible = getWindowFeatureValue<bool, WebBoolean>(windowFeatures, ASCIILiteral("toolBarVisible"));
        m_statusBarVisible = getWindowFeatureValue<bool, WebBoolean>(windowFeatures, ASCIILiteral("statusBarVisible"));
        m_scrollbarsVisible = getWindowFeatureValue<bool, WebBoolean>(windowFeatures, ASCIILiteral("scrollbarsVisible"));
        m_menuBarVisible = getWindowFeatureValue<bool, WebBoolean>(windowFeatures, ASCIILiteral("menuBarVisible"));
        m_locationBarVisible = getWindowFeatureValue<bool, WebBoolean>(windowFeatures, ASCIILiteral("locationBarVisible"));
        m_resizable = getWindowFeatureValue<bool, WebBoolean>(windowFeatures, ASCIILiteral("resizable"));
        m_fullScreen = getWindowFeatureValue<bool, WebBoolean>(windowFeatures, ASCIILiteral("fullscreen"));
    }
}

template <typename T1, typename T2>
T1 EwkWindowFeatures::getWindowFeatureValue(ImmutableDictionary* windowFeatures, const String& featureName)
{
    T2* featureValue = static_cast<T2*>(windowFeatures->get(featureName));

    if (!featureValue)
        return false;

    return featureValue->value();
}

void EwkWindowFeatures::setToolbarVisible(bool toolbarVisible)
{
    m_toolbarVisible = toolbarVisible;
    m_viewImpl->smartCallback<EwkViewCallbacks::ToolbarVisible>().call(&toolbarVisible);
}

void EwkWindowFeatures::setStatusBarVisible(bool statusBarVisible)
{
    m_statusBarVisible = statusBarVisible;
    m_viewImpl->smartCallback<EwkViewCallbacks::StatusBarVisible>().call(&statusBarVisible);
}

void EwkWindowFeatures::setMenuBarVisible(bool menuBarVisible)
{
    m_menuBarVisible = menuBarVisible;
    m_viewImpl->smartCallback<EwkViewCallbacks::MenuBarVisible>().call(&menuBarVisible);
}

void EwkWindowFeatures::setResizable(bool resizable)
{
    m_resizable = resizable;
    m_viewImpl->smartCallback<EwkViewCallbacks::WindowResizable>().call(&resizable);
}

Eina_Bool ewk_window_features_toolbar_visible_get(const Ewk_Window_Features* window_features)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkWindowFeatures, window_features, impl, false);

    return impl->toolbarVisible();
}

Eina_Bool ewk_window_features_statusbar_visible_get(const Ewk_Window_Features* window_features)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkWindowFeatures, window_features, impl, false);

    return impl->statusBarVisible();
}

Eina_Bool ewk_window_features_scrollbars_visible_get(const Ewk_Window_Features* window_features)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkWindowFeatures, window_features, impl, false);

    return impl->scrollbarsVisible();
}

Eina_Bool ewk_window_features_menubar_visible_get(const Ewk_Window_Features* window_features)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkWindowFeatures, window_features, impl, false);

    return impl->menuBarVisible();
}

Eina_Bool ewk_window_features_locationbar_visible_get(const Ewk_Window_Features* window_features)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkWindowFeatures, window_features, impl, false);

    return impl->locationBarVisible();
}

Eina_Bool ewk_window_features_resizable_get(const Ewk_Window_Features* window_features)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkWindowFeatures, window_features, impl, false);

    return impl->resizable();
}

Eina_Bool ewk_window_features_fullscreen_get(const Ewk_Window_Features* window_features)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkWindowFeatures, window_features, impl, false);

    return impl->fullScreen();
}

void ewk_window_features_geometry_get(const Ewk_Window_Features* window_features, Evas_Coord* x, Evas_Coord* y, Evas_Coord* width, Evas_Coord* height)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkWindowFeatures, window_features, impl);

    if (x)
        *x = static_cast<Evas_Coord>(impl->geometry().x());

    if (y)
        *y = static_cast<Evas_Coord>(impl->geometry().y());

    if (width)
        *width = static_cast<Evas_Coord>(impl->geometry().width());

    if (height)
        *height = static_cast<Evas_Coord>(impl->geometry().height());
}
