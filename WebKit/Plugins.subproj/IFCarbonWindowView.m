/*	
    IFCarbonWindowView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#define USE_CARBON 1

#import "IFCarbonWindowView.h"
#import <Foundation/Foundation.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSGraphicsContextPrivate.h>
#import <AppKit/NSWindow_Private.h>
#import <CoreGraphics/CGSGraphics.h>
#import <CoreGraphics/CGGraphicsPrivate.h>
#import <CoreGraphics/CGContext.h>
#import <CoreGraphics/CGGraphicsPrivate.h>
#import <CoreGraphics/CGContext.h>
#import <CoreGraphics/CGGraphicsPrivate.h>
#import <CoreGraphics/CGContext.h>
#import <CoreGraphics/CGSWindow.h>
#import <CarbonCore/MacMemory.h>
#import <QD/Quickdraw.h>
#import <QD/QuickdrawPriv.h>	// for QDBindPortToNativeWindow

@interface NSView(IFCarbonWindowView_Internal)
- _invalidateGStatesForTree;
@end

@interface IFCarbonWindowView(IFCarbonWindowView_Internal)
- (void) _lockQuickDrawPort;
- (void) _unlockQuickDrawPort;
- (void) _adjustPort;
@end

@implementation IFCarbonWindowView

- (void) dealloc
{
    // just in case
    //
    [self _unlockQuickDrawPort];

   // if (_qdPort != NULL) {
   //     DisposePort(_qdPort);
   // }

    [super dealloc];
}

// these are called when we need to update the quickdraw port to match
//
- (void)setFrameOrigin:(NSPoint)newOrigin
{
    _synchToView = YES;
    [super setFrameOrigin:newOrigin];
}

- (void)setFrameSize:(NSSize)newSize
{
    _synchToView = YES;
    [super setFrameSize:newSize];
}

- _invalidateGStatesForTree
{
    _synchToView = YES;
    return [super _invalidateGStatesForTree];
}

// here is where we setup QuickDraw focus

- (void)lockFocus
{
    [super lockFocus];
    [self _lockQuickDrawPort];
}

- (void)unlockFocus
{
    [super unlockFocus];
    [self _unlockQuickDrawPort];
}

- (void*) qdPort
{
    return _qdPort;
}

- (BOOL) isFlipped
{
    // so we match QD's version
    //
    return YES;
}

- (BOOL) isOpaque
{
    return NO;
}

@end

@implementation IFCarbonWindowView(IFCarbonWindowView_Internal)

- (void) _lockQuickDrawPort
{
    // create the port if this is the first time through
    //
    if (_qdPort == NULL)
    {
        [[self window] _windowRef];
        
        _qdPort = GetWindowPort([[self window] _windowRef]);
        // create a new QuickDraw port
        //
        //_qdPort = CreateNewPort();

        // and tell port that it should use windowID for output
        //
        //QDBindPortToNativeWindow(_qdPort, (CGSWindowID)[_window windowNumber]);

        // and force a synch
        //
        _synchToView = YES;
    }

    // if we have one now
    //
    if (_qdPort != NULL)
    {
        // in case we have moved to new window, reattach
        //
        //if (QDGetNativeWindowFromPort(_qdPort) != (CGSWindowID)[_window windowNumber]) {
        //    QDBindPortToNativeWindow(_qdPort, (CGSWindowID)[_window windowNumber]);
        //}
 
        // now set the port
        //
        GetPort((CGrafPtr*)&_savePort);
        SetPort(_qdPort);

        // adjust port origin and clip
        //
        if (_synchToView)
        {
            // make sure it is the right size and location
            //
            [self _adjustPort];

            // no longer need to synchronize
            //
            _synchToView = NO;
        }
    }
}

- (void) _unlockQuickDrawPort
{
    // just restore it if we have one
    //
    if (_savePort != NULL)
    {
        // restore saved port
        //
        SetPort((CGrafPtr)_savePort);
    }
}

- (void) _adjustPort
{
    CGSRegionObj windowShape;
    CGRect       windowRect;
    CGSWindowID  windowID = (CGSWindowID) [_window windowNumber];
    NSRect       portRect, bounds, visibleBounds;

    // don't try anything if window is no more
    //
    if ((int)windowID <= 0 || _qdPort == NULL)
        return;

    // get window bounds in CGS coordinates
    //
    (void) CGSGetWindowShape([[NSGraphicsContext currentContext] contextID], windowID, &windowShape);
    (void) CGSGetRegionBounds(windowShape, &windowRect);

    // get bounds for drawing. this bounds will have a lower left origin
    //
    bounds        = [self bounds];
    visibleBounds = [self visibleRect];
    portRect      = [self convertRect:visibleBounds toView:nil];

    // flip the vertical location
    //
    portRect.origin.y = windowRect.size.height - NSMaxY(portRect);

    MovePortTo ((int)NSMinX(portRect),  (int)NSMinY(portRect));
    PortSize   ((int)NSWidth(portRect), (int)NSHeight(portRect));

    // adjust port origin to account for scrolling. we don't need to invert
    // coordinates since this is already a flipped view
    //
    SetOrigin((int)floor(NSMinX(visibleBounds)), (int)floor(NSMinY(visibleBounds)));
}

@end

