/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "IntRect.h"

namespace WebCore {

IntSize Widget::sizeHint() const 
{
    return IntSize();
}

void Widget::resize(int w, int h) 
{
    setFrameGeometry(IntRect(x(), y(), w, h));
}

int Widget::x() const
{
    return frameGeometry().x();
}

int Widget::y() const 
{
    return frameGeometry().y();
}

int Widget::width() const 
{ 
    return frameGeometry().width();
}

int Widget::height() const 
{
    return frameGeometry().height();
}

IntSize Widget::size() const 
{
    return frameGeometry().size();
}

void Widget::resize(const IntSize &s) 
{
    resize(s.width(), s.height());
}

IntPoint Widget::pos() const 
{
    return frameGeometry().location();
}

void Widget::move(int x, int y) 
{
    setFrameGeometry(IntRect(x, y, width(), height()));
}

void Widget::move(const IntPoint &p) 
{
    move(p.x(), p.y());
}

int Widget::baselinePosition(int height) const
{
    return height;
}

bool Widget::checksDescendantsForFocus() const
{
    return false;
}

bool Widget::isFrameView() const
{
    return false;
}

}
