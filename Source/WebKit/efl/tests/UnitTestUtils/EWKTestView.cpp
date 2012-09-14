/*
    Copyright (C) 2012 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "config.h"
#include "EWKTestView.h"

#include "EWKTestConfig.h"
#include <EWebKit.h>

#include <wtf/PassOwnPtr.h>

namespace EWKUnitTests {

EWKTestEcoreEvas::EWKTestEcoreEvas(int useX11Window)
{
    if (useX11Window)
        m_ecoreEvas = adoptPtr(ecore_evas_new(0, 0, 0, Config::defaultViewWidth, Config::defaultViewHeight, 0));
    else
        m_ecoreEvas = adoptPtr(ecore_evas_buffer_new(Config::defaultViewWidth, Config::defaultViewHeight));
}

EWKTestEcoreEvas::EWKTestEcoreEvas(const char* engine_name, int viewport_x, int viewport_y, int viewport_w, int viewport_h, const char* extra_options, int useX11Window)
{
    if (useX11Window)
        m_ecoreEvas = adoptPtr(ecore_evas_new(engine_name, viewport_x, viewport_y, viewport_w, viewport_h, extra_options));
    else
        m_ecoreEvas = adoptPtr(ecore_evas_buffer_new(viewport_x, viewport_y));
}

Evas* EWKTestEcoreEvas::evas()
{
    if (m_ecoreEvas.get())
        return ecore_evas_get(m_ecoreEvas.get());
    return 0;
}

void EWKTestEcoreEvas::show()
{
    if (m_ecoreEvas.get())
        ecore_evas_show(m_ecoreEvas.get());
}

EWKTestView::EWKTestView(Evas* evas)
    : m_evas(evas)
    , m_url(Config::defaultTestPage)
    , m_defaultViewType(TiledView)
    , m_width(Config::defaultViewWidth)
    , m_height(Config::defaultViewHeight)
{
}

EWKTestView::EWKTestView(Evas* evas, const char* url)
    : m_evas(evas)
    , m_url(url)
    , m_defaultViewType(TiledView)
    , m_width(Config::defaultViewWidth)
    , m_height(Config::defaultViewHeight)
{
}

EWKTestView::EWKTestView(Evas* evas, EwkViewType type, const char* url)
    : m_evas(evas)
    , m_url(url)
    , m_defaultViewType(type)
    , m_width(Config::defaultViewWidth)
    , m_height(Config::defaultViewHeight)
{
}

EWKTestView::EWKTestView(Evas* evas, EwkViewType type, const char* url, int width, int height)
    : m_evas(evas)
    , m_url(url)
    , m_defaultViewType(type)
    , m_width(width)
    , m_height(height)
{
}

bool EWKTestView::init()
{
    if (!m_evas || m_url.empty())
        return false;

    switch (m_defaultViewType) {
    case SingleView:
        m_webView = adoptPtr(ewk_view_single_add(m_evas));
        break;

    case TiledView:
        m_webView = adoptPtr(ewk_view_tiled_add(m_evas));
        break;
    }

    if (!m_webView.get())
        return false;

    ewk_view_theme_set(m_webView.get(), Config::defaultThemePath);
    ewk_view_uri_set(m_webView.get(), m_url.c_str());
}

void EWKTestView::show()
{
    if (!m_webView.get())
        return;
    evas_object_resize(m_webView.get(), m_width, m_height);
    evas_object_show(m_webView.get());
    evas_object_focus_set(m_webView.get(), EINA_TRUE);
}

Evas_Object* EWKTestView::mainFrame()
{
    if (m_webView.get())
        return ewk_view_frame_main_get(m_webView.get());
    return 0;
}

Evas* EWKTestView::evas()
{
    if (m_webView.get())
        return evas_object_evas_get(m_webView.get());
    return 0;
}

void EWKTestView::bindEvents(void (*callback)(void*, Evas_Object*, void*), const char* eventName, void* ptr)
{
    if (!m_webView.get())
        return;

    evas_object_smart_callback_del(m_webView.get(), eventName, callback);
    evas_object_smart_callback_add(m_webView.get(), eventName, callback, ptr);
}

}
