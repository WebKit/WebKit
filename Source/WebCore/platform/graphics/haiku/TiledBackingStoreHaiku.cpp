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

#if USE(TILED_BACKING_STORE) && PLATFORM(HAIKU)

#include <View.h>

#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "TileHaiku.h"

namespace WebCore {

PassRefPtr<Tile> TiledBackingStoreBackend::createTile(TiledBackingStore* backingStore, const Tile::Coordinate& tileCoordinate)
{
    puts("create store");
    return TileHaiku::create(backingStore, tileCoordinate);
}

void TiledBackingStoreBackend::paintCheckerPattern(GraphicsContext* context, const FloatRect& target)
{
    puts("···TILED PATTERN···");
    BView* v = context->platformContext();
    v->PushState();

    pattern p = {0xF0, 0xF0, 0xF0, 0xf0, 0xf, 0xf, 0xf, 0xf};
    v->SetDrawingMode(B_OP_COPY);
    v->SetLowColor(ui_color(B_SUCCESS_COLOR));
    v->SetHighColor(ui_color(B_FAILURE_COLOR));
    v->FillRect(target, p);

    v->PopState();
}

}

#endif

