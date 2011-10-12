/*
 * Copyright (C) 2011 Igalia S.L.
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

#include "config.h"
#include "WebViewTest.h"

WebViewTest::WebViewTest()
    : m_webView(WEBKIT_WEB_VIEW(g_object_ref_sink(webkit_web_view_new())))
    , m_mainLoop(g_main_loop_new(0, TRUE))
{
}

WebViewTest::~WebViewTest()
{
    g_object_unref(m_webView);
    g_main_loop_unref(m_mainLoop);
}

static gboolean testLoadTimeoutFinishLoop(GMainLoop* loop)
{
    g_main_loop_quit(loop);
    return FALSE;
}

void WebViewTest::wait(double seconds)
{
    g_timeout_add_seconds(seconds, reinterpret_cast<GSourceFunc>(testLoadTimeoutFinishLoop), m_mainLoop);
    g_main_loop_run(m_mainLoop);
}
