/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#import "config.h"
#import "WebTileCacheLayer.h"

#import "IntRect.h"
#import "TileCache.h"

using namespace WebCore;

@implementation WebTileCacheLayer

- (id)init
{
    self = [super init];
    if (!self)
        return nil;

    // FIXME: The tile size should be configurable.
    _tileCache = TileCache::create(self, IntSize(512, 512));

    return self;
}

- (void)setBounds:(CGRect)bounds
{
    [super setBounds:bounds];

    _tileCache->tileCacheLayerBoundsChanged();
}

- (void)setNeedsDisplay
{
    _tileCache->setNeedsDisplay();
}

- (void)setNeedsDisplayInRect:(CGRect)rect
{
    _tileCache->setNeedsDisplayInRect(enclosingIntRect(rect));
}

- (CALayer *)tileContainerLayer
{
    return _tileCache->tileContainerLayer();
}

@end
