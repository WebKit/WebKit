/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "PlatformWebView.h"

#include "EWebKit2.h"
#include "WebKit2/WKAPICast.h"
#include <Ecore_Evas.h>

using namespace WebKit;

namespace WTR {

static bool useX11Window = false;

static Ecore_Evas* initEcoreEvas()
{
    Ecore_Evas* ecoreEvas = useX11Window ? ecore_evas_new(0, 0, 0, 800, 600, 0) : ecore_evas_buffer_new(800, 600);
    if (!ecoreEvas)
        return 0;

    ecore_evas_title_set(ecoreEvas, "EFL WebKitTestRunner");
    ecore_evas_show(ecoreEvas);

    return ecoreEvas;
}

PlatformWebView::PlatformWebView(WKContextRef context, WKPageGroupRef pageGroup)
{
    m_window = initEcoreEvas();
    Evas* evas = ecore_evas_get(m_window);
    m_view = toImpl(WKViewCreate(evas, context, pageGroup));

    ewk_view_theme_set(m_view, THEME_DIR"/default.edj");
    m_windowIsKey = false;
    evas_object_show(m_view);
}

PlatformWebView::~PlatformWebView()
{
    evas_object_del(m_view);
    ecore_evas_free(m_window);
}

void PlatformWebView::resizeTo(unsigned width, unsigned height)
{
    evas_object_resize(m_view, width, height);
}

WKPageRef PlatformWebView::page()
{
    return WKViewGetPage(toAPI(m_view));
}

void PlatformWebView::focus()
{
    evas_object_focus_set(m_view, true);
}

WKRect PlatformWebView::windowFrame()
{
    return WKRectMake(0, 0, 0, 0);
}

void PlatformWebView::setWindowFrame(WKRect frame)
{
    evas_object_move(m_view, frame.origin.x, frame.origin.y);
    resizeTo(frame.size.width, frame.size.height);
}

void PlatformWebView::addChromeInputField()
{
}

void PlatformWebView::removeChromeInputField()
{
}

void PlatformWebView::makeWebViewFirstResponder()
{
}

WKRetainPtr<WKImageRef> PlatformWebView::windowSnapshotImage()
{
    // FIXME: implement to capture pixels in the UI process,
    // which may be necessary to capture things like 3D transforms.
    return 0;
}

} // namespace WTR

