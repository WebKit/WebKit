/*
 * Copyright (C) 2017 Igalia S.L.
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

void WebViewTest::platformDestroy()
{
}

void WebViewTest::platformInitializeWebView()
{
}

void WebViewTest::quitMainLoopAfterProcessingPendingEvents()
{
    // FIXME: implement if needed.
    quitMainLoop();
}

void WebViewTest::resizeView(int width, int height)
{
    // FIXME: implement.
}

void WebViewTest::hideView()
{
    // FIXME: implement.
}

void WebViewTest::mouseMoveTo(int x, int y, unsigned mouseModifiers)
{
    // FIXME: implement.
}

void WebViewTest::clickMouseButton(int x, int y, unsigned button, unsigned mouseModifiers)
{
    // FIXME: implement.
}

void WebViewTest::keyStroke(unsigned keyVal, unsigned keyModifiers)
{
    // FIXME: implement.
}
