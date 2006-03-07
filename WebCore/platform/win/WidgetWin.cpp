/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Widget.h"

#include "Cursor.h"
#include "GraphicsContext.h"
#include "FrameWin.h"
#include "IntRect.h"
#include "Font.h"

namespace WebCore {

class WidgetPrivate
{
public:
    HWND windowHandle;
    Font font;
    WidgetClient* client;
};

Widget::Widget()
    : data(new WidgetPrivate)
{
    data->windowHandle = 0;
    data->client = 0;
}

Widget::Widget(HWND hWnd)
    : data(new WidgetPrivate)
{
    data->windowHandle = hWnd;
}

Widget::~Widget() 
{
    delete data;
}

HWND Widget::windowHandle()
{
    return data->windowHandle;
}

void Widget::setWindowHandle(HWND hWnd)
{
    data->windowHandle = hWnd;
}

void Widget::setActiveWindow() 
{
    BringWindowToTop(data->windowHandle);
}

void Widget::setClient(WidgetClient* c)
{
    data->client = c;
}

WidgetClient* Widget::client() const
{
    return data->client;
}

IntRect Widget::frameGeometry() const
{
    RECT frame;
    if (GetWindowRect(data->windowHandle, &frame)) {
        if (HWND parent = GetParent(data->windowHandle))
            MapWindowPoints(NULL, parent, (LPPOINT)&frame, 2);
        return frame;
    }
    
    return IntRect();
}

bool Widget::hasFocus() const
{
    return (data->windowHandle == GetForegroundWindow());
}

void Widget::setFocus()
{
    SetFocus(data->windowHandle);
}

void Widget::clearFocus()
{
    FrameWin::clearDocumentFocus(this);
    SetFocus(0);
}

const Font& Widget::font() const
{
    return data->font;
}

void Widget::setFont(const Font& font)
{
    data->font = font;
}

void Widget::setCursor(const Cursor& cursor)
{
    SetProp(data->windowHandle, L"PROP_CURSOR", cursor.impl());
}

void Widget::show()
{
    ShowWindow(data->windowHandle, SW_SHOWNA);
}

void Widget::hide()
{
    ShowWindow(data->windowHandle, SW_HIDE);
}

void Widget::setFrameGeometry(const IntRect &rect)
{
    MoveWindow(data->windowHandle, rect.x(), rect.y(), rect.width(), rect.height(), false);
}

IntPoint Widget::mapFromGlobal(const IntPoint &p) const
{
    POINT point = p;
    ScreenToClient(data->windowHandle, &point);
    return point;
}

}
