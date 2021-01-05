/*
 * Copyright (C) 2014 Haiku, inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TileHaiku.h"

#if USE(TILED_BACKING_STORE) && PLATFORM(HAIKU)

#include "NotImplemented.h"

namespace WebCore {

TileHaiku::TileHaiku(TiledBackingStore*, const Coordinate&)
    : m_buffer(BRect(0,0,0,0), B_RGBA32)
{
    puts("create");
    notImplemented();
}

TileHaiku::~TileHaiku()
{
    puts("destroy");
}

bool TileHaiku::isDirty() const
{
    puts("isdirty");
    notImplemented();
    return false;
}

void TileHaiku::invalidate(const IntRect&)
{
    puts("invalidate");
    notImplemented();
}

Vector<IntRect> TileHaiku::updateBackBuffer()
{
    puts("update");
    notImplemented();
}

void TileHaiku::swapBackBufferToFront()
{
    puts("swap");
    notImplemented();
}

bool TileHaiku::isReadyToPaint() const
{
    puts("ready");
    notImplemented();
    return false;
}

void TileHaiku::paint(GraphicsContext*, const IntRect&)
{
    puts("paint");
    notImplemented();
}

void TileHaiku::resize(const WebCore::IntSize&)
{
    puts("resize");
    notImplemented();
}

}

#endif
