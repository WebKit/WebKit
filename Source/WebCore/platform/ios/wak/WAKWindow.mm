/*
 * Copyright (C) 2005-2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WAKWindow.h"

#if PLATFORM(IOS_FAMILY)

#import "LegacyTileCache.h"
#import "PlatformScreen.h"
#import "WAKViewInternal.h"
#import "WebCoreThreadRun.h"
#import "WebEvent.h"
#import "WKContentObservation.h"
#import "WKViewPrivate.h"
#import <QuartzCore/QuartzCore.h>
#import <wtf/Lock.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>

WEBCORE_EXPORT NSString * const WAKWindowScreenScaleDidChangeNotification = @"WAKWindowScreenScaleDidChangeNotification";
WEBCORE_EXPORT NSString * const WAKWindowVisibilityDidChangeNotification = @"WAKWindowVisibilityDidChangeNotification";

@protocol OrientationProvider
- (BOOL)hasLandscapeOrientation;
@end

static RetainPtr<id<OrientationProvider>>& orientationProvider()
{
    static NeverDestroyed<RetainPtr<id<OrientationProvider>>> provider;
    return provider;
}

static RetainPtr<WebEvent>& currentEvent()
{
    static NeverDestroyed<RetainPtr<WebEvent>> event;
    return event;
}

@implementation WAKWindow {
    Lock _exposedScrollViewRectLock;
    CGRect _exposedScrollViewRect;
}

@synthesize useOrientationDependentFontAntialiasing = _useOrientationDependentFontAntialiasing;

- (id)initWithLayer:(CALayer *)layer
{
    self = [super init];
    if (!self)
        return nil;

    _hostLayer = [layer retain];

    _frame = [_hostLayer frame];
    _screenScale = WebCore::screenScaleFactor();

    _tileCache = new LegacyTileCache(self);

    _frozenVisibleRect = CGRectNull;

    _exposedScrollViewRect = CGRectNull;

    return self;
}

// This is used for WebViews that are not backed by a tile cache. Their content must be painted manually.
- (id)initWithFrame:(CGRect)frame
{
    self = [super init];
    if (!self)
        return nil;

    _frame = frame;
    _screenScale = WebCore::screenScaleFactor();

    _exposedScrollViewRect = CGRectNull;

    return self;
}

- (void)dealloc
{
    delete _tileCache;
    [_hostLayer release];
    
    [super dealloc];
}

- (void)setContentView:(WAKView *)view
{
    [view retain];
    [_contentView release];

    if (view)
        _WKViewSetWindow([view _viewRef], self);
    _contentView = view;
}

- (WAKView *)contentView
{
    return _contentView;
}

- (void)close
{
    if (_contentView) {
        _WKViewSetWindow([_contentView _viewRef], nil);
        [_contentView release];
        _contentView = nil;
    }

    [_responderView release];
    _responderView = nil;
}

- (WAKView *)firstResponder
{
    return _responderView;
}

- (WAKView *)_newFirstResponderAfterResigning
{
    return _nextResponder;
}

- (NSPoint)convertBaseToScreen:(NSPoint)aPoint
{
    CALayer* rootLayer = _hostLayer;
    while (rootLayer.superlayer)
        rootLayer = rootLayer.superlayer;
    
    return [_hostLayer convertPoint:aPoint toLayer:rootLayer];
}

- (NSPoint)convertScreenToBase:(NSPoint)aPoint
{
    CALayer* rootLayer = _hostLayer;
    while (rootLayer.superlayer)
        rootLayer = rootLayer.superlayer;
    
    return [_hostLayer convertPoint:aPoint fromLayer:rootLayer];
}

- (NSRect)convertRectToScreen:(NSRect)windowRect
{
    CALayer* rootLayer = _hostLayer;
    while (rootLayer.superlayer)
        rootLayer = rootLayer.superlayer;

    return [_hostLayer convertRect:windowRect toLayer:rootLayer];
}

- (NSRect)convertRectFromScreen:(NSRect)screenRect
{
    CALayer* rootLayer = _hostLayer;
    while (rootLayer.superlayer)
        rootLayer = rootLayer.superlayer;

    return [_hostLayer convertRect:screenRect fromLayer:rootLayer];
}

- (BOOL)isKeyWindow
{
    return YES;
}

- (void)makeKeyWindow
{
}

- (BOOL)isVisible
{
    return _visible;
}

- (void)setVisible:(BOOL)visible
{
    if (_visible == visible)
        return;

    _visible = visible;

    WebThreadRun(^{
        [[NSNotificationCenter defaultCenter] postNotificationName:WAKWindowVisibilityDidChangeNotification object:self userInfo:nil];
    });
}

- (NSSelectionDirection)keyViewSelectionDirection
{
    return (NSSelectionDirection)0;
}

- (BOOL)resignFirstResponder
{
    BOOL shouldResign = [super resignFirstResponder];
    if (shouldResign && _responderView && WKViewResignFirstResponder([_responderView _viewRef])) {
        _nextResponder = nil;
        [_responderView release];
        _responderView = nil;
        return YES;
    }
    return NO;
}

- (BOOL)makeFirstResponder:(NSResponder *)responder
{
    if (![responder isKindOfClass:[WAKView class]])
        return NO;

    WAKView *view = static_cast<WAKView*>(responder);
    BOOL result = YES;
    if (view != _responderView) {
        // We need to handle the case of the view not accepting to be a first responder,
        // where we don't need to resign as first responder.
        BOOL acceptsFirstResponder = view ? WKViewAcceptsFirstResponder([view _viewRef]) : YES;
        if (acceptsFirstResponder && _responderView) {
            _nextResponder = view;
            if (WKViewResignFirstResponder([_responderView _viewRef])) {
                _nextResponder = nil;
                [_responderView release];
                _responderView = nil;
            }  else {
                _nextResponder = nil;
                result = NO;
            }
        }

        if (result && view) {
            if (acceptsFirstResponder && WKViewBecomeFirstResponder([view _viewRef]))
                _responderView = [view retain];
            else
                result = NO;
        }
    }

    return result;
}

// FIXME: This method can lead to incorrect state. Remove if possible.
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag
{
    UNUSED_PARAM(flag);
    _frame = frameRect;
}

// FIXME: the correct value if there is a host layer is likely to return [_hostLayer frame].
- (CGRect)frame
{
    return _frame;
}

- (void)setContentRect:(CGRect)rect
{
    if (CGRectEqualToRect(rect, _frame))
        return;
    _frame = rect;

    // FIXME: Size of the host layer is directly available so there is no real reason to save it here.
    // However we currently use this call to catch changes to the host layer size.
    if (_tileCache)
        _tileCache->hostLayerSizeChanged();
}

- (void)setScreenSize:(CGSize)size
{
    _screenSize = size;
}

- (CGSize)screenSize
{
    return _screenSize;
}

- (void)setAvailableScreenSize:(CGSize)size
{
    _availableScreenSize = size;
}

- (CGSize)availableScreenSize
{
    return _availableScreenSize;
}

- (void)setScreenScale:(CGFloat)scale
{
    _screenScale = scale;

    WebThreadRun(^{
        [[NSNotificationCenter defaultCenter] postNotificationName:WAKWindowScreenScaleDidChangeNotification object:self userInfo:nil];
    });
}

- (CGFloat)screenScale
{
    return _screenScale;
}

- (void)setRootLayer:(CALayer *)layer
{
    _rootLayer = layer;
}

- (CALayer *)rootLayer
{
    return _rootLayer;
}

- (void)sendEvent:(WebEvent *)event
{
    ASSERT(event);
    WebThreadRun(^{
        [self sendEventSynchronously:event];
    });
}

- (void)sendEventSynchronously:(WebEvent *)event
{
    ASSERT(event);
    ASSERT(WebThreadIsLockedOrDisabled());
    auto lastEvent = std::exchange(currentEvent(), event);

    switch (event.type) {
    case WebEventMouseMoved:
    case WebEventScrollWheel:
        if (WAKView *hitView = [_contentView hitTest:event.locationInWindow])
            [hitView handleEvent:event];
        break;

    case WebEventMouseUp:
    case WebEventKeyDown:
    case WebEventKeyUp:
    case WebEventTouchChange:
        [_responderView handleEvent:event];
        break;

    case WebEventMouseDown:
    case WebEventTouchBegin:
    case WebEventTouchEnd:
    case WebEventTouchCancel:
        if (WAKView *hitView = [_contentView hitTest:event.locationInWindow]) {
            [self makeFirstResponder:hitView];
            [hitView handleEvent:event];
        }
        break;
    }

    currentEvent() = WTFMove(lastEvent);
}

- (void)sendMouseMoveEvent:(WebEvent *)event contentChange:(WKContentChange *)aContentChange
{
    WebThreadRun(^{
        [self sendEvent:event];

        if (aContentChange) {
            // We always make the decision asynchronously. See EventHandler::mouseMoved.
            *aContentChange = WKContentIndeterminateChange;
        }
    });
}

- (void)setExposedScrollViewRect:(CGRect)exposedScrollViewRect
{
    Locker locker { _exposedScrollViewRectLock };
    _exposedScrollViewRect = exposedScrollViewRect;
}

- (CGRect)exposedScrollViewRect
{
    {
        Locker locker { _exposedScrollViewRectLock };
        if (!CGRectIsNull(_exposedScrollViewRect))
            return _exposedScrollViewRect;
    }
    return [self visibleRect];
}

// Tiling

- (void)layoutTiles
{
    if (!_tileCache)
        return;    
    _tileCache->layoutTiles();
}

- (void)layoutTilesNow
{
    if (!_tileCache)
        return;
    _tileCache->layoutTilesNow();
}

- (void)layoutTilesNowForRect:(CGRect)rect
{
    if (!_tileCache)
        return;
    _tileCache->layoutTilesNowForRect(WebCore::enclosingIntRect(rect));
}

- (void)setNeedsDisplay
{
    if (!_tileCache)
        return;
    _tileCache->setNeedsDisplay();
}

- (void)setNeedsDisplayInRect:(CGRect)rect
{
    if (!_tileCache)
        return;
    _tileCache->setNeedsDisplayInRect(WebCore::enclosingIntRect(rect));
}

- (BOOL)tilesOpaque
{
    if (!_tileCache)
        return NO;
    return _tileCache->tilesOpaque();
}

- (void)setTilesOpaque:(BOOL)opaque
{
    if (!_tileCache)
        return;
    _tileCache->setTilesOpaque(opaque);
}

- (void)setIsInSnapshottingPaint:(BOOL)isInSnapshottingPaint
{
    _isInSnapshottingPaint = isInSnapshottingPaint;
}

- (BOOL)isInSnapshottingPaint
{
    return _isInSnapshottingPaint;
}

- (void)setEntireWindowVisibleForTesting:(BOOL)entireWindowVisible
{
    _entireWindowVisibleForTesting = entireWindowVisible;
}

- (CGRect)_visibleRectRespectingMasksToBounds:(BOOL)respectsMasksToBounds
{
    if (!CGRectIsNull(_frozenVisibleRect))
        return _frozenVisibleRect;

    CALayer* layer = _hostLayer;
    CGRect bounds = [layer bounds];
    if (_entireWindowVisibleForTesting)
        return bounds;
    CGRect rect = bounds;
    CALayer* superlayer = [layer superlayer];

    static Class windowClass = NSClassFromString(@"UIWindow");

    while (superlayer && layer != _rootLayer && (!layer.delegate || ![layer.delegate isKindOfClass:windowClass])) {
        CGRect rectInSuper = [superlayer convertRect:rect fromLayer:layer];
        if ([superlayer masksToBounds] || !respectsMasksToBounds)
            rect = CGRectIntersection([superlayer bounds], rectInSuper);
        else
            rect = rectInSuper;
        layer = superlayer;
        superlayer = [layer superlayer];
    }
    
    if (layer != _hostLayer) {
        rect = CGRectIntersection([layer bounds], rect);
        rect = [_hostLayer convertRect:rect fromLayer:layer];
    }
    
    return CGRectIntersection(bounds, rect);
}

- (CGRect)visibleRect
{
    return [self _visibleRectRespectingMasksToBounds:NO];
}

- (CGRect)extendedVisibleRect
{
    return [self _visibleRectRespectingMasksToBounds:YES];
}

- (void)removeAllNonVisibleTiles
{
    if (!_tileCache)
        return;
    _tileCache->removeAllNonVisibleTiles();
}

- (void)removeAllTiles
{
    if (!_tileCache)
        return;
    _tileCache->removeAllTiles();
}

- (void)removeForegroundTiles
{
    if (!_tileCache)
        return;
    _tileCache->removeForegroundTiles();
}

- (void)setTilingMode:(WAKWindowTilingMode)mode
{
    if (!_tileCache)
        return;
    Ref { *_tileCache }->setTilingMode((LegacyTileCache::TilingMode)mode);
}

- (WAKWindowTilingMode)tilingMode
{
    if (!_tileCache)
        return kWAKWindowTilingModeDisabled;
    return (WAKWindowTilingMode)_tileCache->tilingMode();
}

- (void)setTilingDirection:(WAKTilingDirection)tilingDirection
{
    if (!_tileCache)
        return;
    Ref { *_tileCache }->setTilingDirection((LegacyTileCache::TilingDirection)tilingDirection);
}

- (WAKTilingDirection)tilingDirection
{
    if (!_tileCache)
        return kWAKTilingDirectionDown;
    return (WAKTilingDirection)_tileCache->tilingDirection();
}

- (void)setZoomedOutTileScale:(float)scale
{
    if (!_tileCache)
        return;
    _tileCache->setZoomedOutScale(scale);
}

- (float)zoomedOutTileScale
{
    if (!_tileCache)
        return 1.0f;
    return _tileCache->zoomedOutScale();
}

- (void)setCurrentTileScale:(float)scale
{
    if (!_tileCache)
        return;
    _tileCache->setCurrentScale(scale);
}

- (float)currentTileScale
{
    if (!_tileCache)
        return 1.0f;
    return _tileCache->currentScale();
}

- (void)setKeepsZoomedOutTiles:(BOOL)keepsZoomedOutTiles
{
    if (!_tileCache)
        return;
    _tileCache->setKeepsZoomedOutTiles(keepsZoomedOutTiles);
}

- (BOOL)keepsZoomedOutTiles
{
    if (!_tileCache)
        return NO;
    return _tileCache->keepsZoomedOutTiles();
}

- (LegacyTileCache*)tileCache
{
    return _tileCache;
}

- (void)setContentReplacementImage:(CGImageRef)contentReplacementImage
{
    if (!_tileCache)
        return;
    _tileCache->setContentReplacementImage(contentReplacementImage);
}

- (CGImageRef)contentReplacementImage
{
    if (!_tileCache)
        return NULL;
    return _tileCache->contentReplacementImage().get();
}

- (void)displayRect:(NSRect)rect
{
    [[self contentView] displayRect:rect];
}

- (void)willRotate
{
    _frozenVisibleRect = [self visibleRect];
}

- (void)didRotate
{
    _frozenVisibleRect = CGRectNull;
}

+ (void)setOrientationProvider:(id)provider
{
    // This is really the UIWebDocumentView class that calls into UIApplication to get the orientation.
    orientationProvider() = provider;
}

+ (BOOL)hasLandscapeOrientation
{
    // this should be perfectly thread safe
    return [orientationProvider() hasLandscapeOrientation];
}

- (CALayer*)hostLayer
{
    return _hostLayer;
}

- (void)dumpTiles
{
    CGRect savedFrozenVisibleRect = _frozenVisibleRect;
    NSLog(@"=================");
    if (!CGRectIsNull(_frozenVisibleRect)) {
        NSLog(@"Visibility::Visible RECT IS CACHED: [%6.1f %6.1f %6.1f %6.1f]", _frozenVisibleRect.origin.x, _frozenVisibleRect.origin.y, _frozenVisibleRect.size.width, _frozenVisibleRect.size.height);
        _frozenVisibleRect = CGRectNull;
    }
    CGRect visibleRect = [self visibleRect];
    NSLog(@"visibleRect = [%6.1f %6.1f %6.1f %6.1f]", visibleRect.origin.x, visibleRect.origin.y, visibleRect.size.width, visibleRect.size.height);
    _frozenVisibleRect = savedFrozenVisibleRect;
    _tileCache->dumpTiles();
}

- (NSString *)description
{
    NSMutableString *description = [NSMutableString stringWithFormat:@"<%@: WAK: %p; ", [self class], self];

    CGRect frame = [self frame];
    [description appendFormat:@"frame = (%g %g; %g %g); ", frame.origin.x, frame.origin.y, frame.size.width, frame.size.height];

    [description appendFormat:@"first responder = WK %p; ", _responderView];
    [description appendFormat:@"next responder = WK %p; ", _nextResponder];

    [description appendFormat:@"layer = %@>", [_hostLayer description]];

    return description;
}

+ (WebEvent *)currentEvent
{
    return currentEvent().get();
}

- (NSString *)recursiveDescription
{
    NSMutableString *info = [NSMutableString string];

    [info appendString:[self description]];

    [[self contentView] _appendDescriptionToString:info atLevel:1];

    return info;
}

@end

#endif // PLATFORM(IOS_FAMILY)
