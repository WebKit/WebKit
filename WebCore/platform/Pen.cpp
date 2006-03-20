/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#include "Pen.h"

namespace WebCore {

Pen::Pen(const Color &color, unsigned width, PenStyle style) : m_style(style), m_width(width), m_color(color)
{
}

const Color &Pen::color() const
{
    return m_color;
}

unsigned Pen::width() const
{
    return m_width;
}

Pen::PenStyle Pen::style() const
{
    return m_style;
}

void Pen::setColor(const Color &color)
{
    m_color = color;
}

void Pen::setWidth(unsigned width)
{
    m_width = width;
}

void Pen::setStyle(PenStyle style)
{
    m_style = style;
}

bool Pen::operator==(const Pen &compareTo) const
{
    return (m_width == compareTo.m_width) &&
        (m_style == compareTo.m_style) &&
        (m_color == compareTo.m_color);
}

bool Pen::operator!=(const Pen &compareTo) const
{
    return !(*this == compareTo);
}

}
