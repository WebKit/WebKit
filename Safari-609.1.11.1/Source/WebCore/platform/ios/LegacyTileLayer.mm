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
#include "LegacyTileLayer.h"

#if PLATFORM(IOS_FAMILY)

#include "LegacyTileCache.h"
#include "LegacyTileGrid.h"
#include "WebCoreThread.h"
#include <wtf/SetForScope.h>

using WebCore::LegacyTileCache;
@implementation LegacyTileHostLayer

- (id)initWithTileGrid:(WebCore::LegacyTileGrid*)tileGrid
{
    self = [super init];
    if (!self)
        return nil;
    _tileGrid = tileGrid;
    [self setAnchorPoint:CGPointZero];
    return self;
}

- (id<CAAction>)actionForKey:(NSString *)key
{
    UNUSED_PARAM(key);
    // Disable all default actions
    return nil;
}

- (void)renderInContext:(CGContextRef)context
{
    if (pthread_main_np())
        WebThreadLock();

    CGRect dirtyRect = CGContextGetClipBoundingBox(context);
    auto useExistingTiles = _tileGrid->tileCache().setOverrideVisibleRect(WebCore::FloatRect(dirtyRect));
    if (!useExistingTiles)
        _tileGrid->tileCache().doLayoutTiles();

    [super renderInContext:context];

    _tileGrid->tileCache().clearOverrideVisibleRect();
    if (!useExistingTiles)
        _tileGrid->tileCache().doLayoutTiles();
}
@end

@implementation LegacyTileLayer
@synthesize paintCount = _paintCount;
@synthesize tileGrid = _tileGrid;
@synthesize isRenderingInContext = _isRenderingInContext;

- (void)setNeedsDisplayInRect:(CGRect)rect
{
    // We need to do WebKit layout before painting. Layout may generate new repaint rects and
    // invalidate more tiles, something that is not allowed in drawInContext.
    // Calling setNeedsLayout ensures that layoutSublayers will get called before drawInContext and
    // we do WebKit layout there.
    [self setNeedsLayout];
    [super setNeedsDisplayInRect:rect];
}

- (void)layoutSublayers
{
    if (pthread_main_np())
        WebThreadLock();
    // This may trigger WebKit layout and generate more repaint rects.
    if (_tileGrid)
        _tileGrid->tileCache().prepareToDraw();
}

- (void)renderInContext:(CGContextRef)context
{
    SetForScope<BOOL> change(_isRenderingInContext, YES);
    [super renderInContext:context];
}

- (void)drawInContext:(CGContextRef)context
{
    // Bugs in clients or other frameworks may cause tile invalidation from within a CA commit.
    // In that case we maybe left with dirty tiles that have display still pending. Some future
    // commit will flush such tiles and they will get painted without holding the web lock.
    // rdar://problem/21149759
    // Still assert as the condition is not normal and may cause graphical glitches.
    ASSERT(WebThreadIsLockedOrDisabled());
    if (pthread_main_np())
        WebThreadLock();

    if (_tileGrid)
        _tileGrid->tileCache().drawLayer(self, context, self.isRenderingInContext ? LegacyTileCache::DrawingFlags::Snapshotting : LegacyTileCache::DrawingFlags::None);
}

- (id<CAAction>)actionForKey:(NSString *)key
{
    UNUSED_PARAM(key);
    // Disable all default actions
    return nil;
}

@end

#endif // PLATFORM(IOS_FAMILY)
