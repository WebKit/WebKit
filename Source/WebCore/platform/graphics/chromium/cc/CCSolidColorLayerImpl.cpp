/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCSolidColorLayerImpl.h"

#include "LayerRendererChromium.h"
#include "cc/CCSolidColorDrawQuad.h"
#include <wtf/MathExtras.h>
#include <wtf/text/WTFString.h>

using namespace std;

namespace WebCore {

CCSolidColorLayerImpl::CCSolidColorLayerImpl(int id)
    : CCLayerImpl(id)
    , m_tileSize(256)
{
}

CCSolidColorLayerImpl::~CCSolidColorLayerImpl()
{
}

TransformationMatrix CCSolidColorLayerImpl::quadTransform() const
{
    TransformationMatrix solidColorTransform = drawTransform();
    solidColorTransform.translate(-bounds().width() / 2.0, -bounds().height() / 2.0);

    return solidColorTransform;
}

void CCSolidColorLayerImpl::appendQuads(CCQuadList& quadList, const CCSharedQuadState* sharedQuadState)
{
    // We create a series of smaller quads instead of just one large one so that the
    // culler can reduce the total pixels drawn.
    int width = bounds().width();
    int height = bounds().height();
    for (int x = 0; x < width; x += m_tileSize) {
        for (int y = 0; y < height; y += m_tileSize) {
            IntRect solidTileRect(x, y, min(width - x, m_tileSize), min(height - y, m_tileSize));
            quadList.append(CCSolidColorDrawQuad::create(sharedQuadState, solidTileRect, backgroundColor()));
        }
    }
}

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
