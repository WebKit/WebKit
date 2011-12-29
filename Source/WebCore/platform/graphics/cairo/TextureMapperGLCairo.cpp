/*
 * Copyright (C) 2011 ChangSeok Oh <shivamidow@gmail.com>
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
#include "TextureMapperGLCairo.h"

#include "NotImplemented.h"

namespace WebCore {

BGRA32PremultimpliedBufferCairo::BGRA32PremultimpliedBufferCairo()
    : m_context(0)
    , m_cairoContext(0)
    , m_cairoSurface(0) 
{
    notImplemented();
}

BGRA32PremultimpliedBufferCairo::~BGRA32PremultimpliedBufferCairo()
{
    notImplemented();
}

PlatformGraphicsContext* BGRA32PremultimpliedBufferCairo::beginPaint(const IntRect& dirtyRect, bool opaque)
{
    notImplemented();
    return 0;
}

void* BGRA32PremultimpliedBufferCairo::data()
{
    notImplemented();
    return 0;
}

void BGRA32PremultimpliedBufferCairo::endPaint()
{
    notImplemented();
}

uint64_t uidForImage(Image* image)
{
    notImplemented();
    return 0;
}

PassOwnPtr<BGRA32PremultimpliedBuffer> BGRA32PremultimpliedBuffer::create()
{
    notImplemented();
    return adoptPtr(new BGRA32PremultimpliedBufferCairo());
}

};
