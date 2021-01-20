/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TileControllerMemoryHandlerIOS.h"

#if PLATFORM(IOS_FAMILY)

#include "TileController.h"
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const unsigned kMaxCountOfUnparentedTiledLayers = 16;

void TileControllerMemoryHandler::removeTileController(TileController* controller)
{
    if (m_tileControllers.contains(controller))
        m_tileControllers.remove(controller);
}

unsigned TileControllerMemoryHandler::totalUnparentedTiledLayers() const
{
    unsigned totalUnparentedLayers = 0;
    for (ListHashSet<TileController*>::const_iterator it = m_tileControllers.begin(); it != m_tileControllers.end(); ++it) {
        TileController* tileController = *it;
        totalUnparentedLayers += tileController->numberOfUnparentedTiles();
    }
    return totalUnparentedLayers;
}

void TileControllerMemoryHandler::tileControllerGainedUnparentedTiles(TileController* controller)
{
    m_tileControllers.appendOrMoveToLast(controller);

    // If we are under memory pressure, remove all unparented tiles now.
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        trimUnparentedTilesToTarget(0);
        return;
    }

    if (totalUnparentedTiledLayers() < kMaxCountOfUnparentedTiledLayers)
        return;

    trimUnparentedTilesToTarget(kMaxCountOfUnparentedTiledLayers);
}

void TileControllerMemoryHandler::trimUnparentedTilesToTarget(int target)
{
    while (!m_tileControllers.isEmpty()) {
        TileController* tileController = m_tileControllers.first();
        tileController->removeUnparentedTilesNow();
        m_tileControllers.removeFirst();

        if (target > 0 && totalUnparentedTiledLayers() < static_cast<unsigned>(target))
            return;
    }
}

TileControllerMemoryHandler& tileControllerMemoryHandler()
{
    static NeverDestroyed<TileControllerMemoryHandler> staticTileControllerMemoryHandler;
    return staticTileControllerMemoryHandler;
}

}

#endif // PLATFORM(IOS_FAMILY)
