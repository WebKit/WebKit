/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "DrawingAreaUpdateChunk.h"

#include "UpdateChunk.h"
#include "WebPage.h"
#include <WebCore/GraphicsContext.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

namespace WebKit {

void DrawingAreaUpdateChunk::paintIntoUpdateChunk(UpdateChunk* updateChunk)
{
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGContextRef> bitmapContext(AdoptCF, CGBitmapContextCreate(updateChunk->data(), updateChunk->rect().width(), updateChunk->rect().height(), 8, updateChunk->rect().width() * 4, colorSpace.get(), kCGImageAlphaPremultipliedLast));

    // WebCore expects a flipped coordinate system.
    CGContextTranslateCTM(bitmapContext.get(), 0.0, updateChunk->rect().height());
    CGContextScaleCTM(bitmapContext.get(), 1.0, -1.0);

    // Now paint into the backing store.
    GraphicsContext graphicsContext(bitmapContext.get());
    graphicsContext.translate(-updateChunk->rect().x(), -updateChunk->rect().y());
    
    m_webPage->drawRect(graphicsContext, updateChunk->rect());
}

} // namespace WebKit
