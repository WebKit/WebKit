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

#include "Font.h"
#include "GraphicsContext.h"
#include <cairo.h>
#include <cairo-win32.h>


namespace WebCore {

void Font::drawText(const GraphicsContext* context, int x, int y, int tabWidth, int xpos, const QChar* str, int len, int from, int to,
                    int toAdd, TextDirection d, bool visuallyOrdered) const
{
    cairo_surface_t* surface = cairo_get_target(context->platformContext());
    HDC dc = cairo_win32_surface_get_dc(surface);

    int offset = 0;
    int length = len;
    if (from > 0) {
        offset = from;
        length = len - from;
    }
    if (to > 0)
        length = to - from;

    TextOut(dc, x, y, (LPCWSTR)(str+offset), length);
}

void Font::drawHighlightForText(const GraphicsContext* context, int x, int y, int h, int tabWidth, int xpos, const QChar* str,
                                int len, int from, int to, int toAdd,
                                TextDirection d, bool visuallyOrdered, const Color& backgroundColor) const
{
}

void Font::drawLineForText(const GraphicsContext* context, int x, int y, int yOffset, int width) const
{
}

void Font::drawLineForMisspelling(const GraphicsContext* context, int x, int y, int width) const
{
}

int Font::misspellingLineThickness(const GraphicsContext* context) const
{
    return 0;
}


}
