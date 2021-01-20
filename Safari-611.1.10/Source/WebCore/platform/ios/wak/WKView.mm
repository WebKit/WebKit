/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#import "WKViewPrivate.h"

#if PLATFORM(IOS_FAMILY)

#import "WAKViewInternal.h"
#import "WAKWindow.h"
#import "WKUtilities.h"
#import <wtf/Assertions.h>

void _WKViewSetSuperview (WKViewRef view, WKViewRef superview)
{
    // Not retained.
    view->superview = superview;
}

void _WKViewWillRemoveSubview(WKViewRef view, WKViewRef subview)
{
    if (view->context && view->context->willRemoveSubviewCallback)
        view->context->willRemoveSubviewCallback(view, subview);
}

void _WKViewSetWindow (WKViewRef view, WAKWindow *window)
{
    if (view->window == window)
        return;

    [window retain];
    [view->window release];

    view->window = window;

    // Set the window on all subviews.
    WKViewRef firstChild = WKViewFirstChild(view);
    if (firstChild) {
        _WKViewSetWindow (firstChild, window);
        WKViewRef nextSibling = WKViewNextSibling(firstChild);
        while (nextSibling) {
            _WKViewSetWindow (nextSibling, window);
            nextSibling = WKViewNextSibling(nextSibling);
        }
    }

    if (view->context && view->context->notificationCallback)
        view->context->notificationCallback (view, WKViewNotificationViewDidMoveToWindow, view->context->notificationUserInfo);
}

static void _WKViewClearSuperview(const void *value, void *context)
{
    UNUSED_PARAM(context);
    _WKViewSetSuperview(static_cast<WKViewRef>(const_cast<void*>(value)), 0);
}

static void _WKViewDealloc(WAKObjectRef v)
{
    WKViewRef view = (WKViewRef)v;
    
    if (view->subviews) {
        CFArrayApplyFunction(view->subviews, CFRangeMake(0, CFArrayGetCount(view->subviews)), _WKViewClearSuperview, NULL);
        CFRelease (view->subviews);
        view->subviews = 0;
    }

    [view->window release];
    view->window = nil;
}

void WKViewInitialize (WKViewRef view, CGRect frame, WKViewContext *context)
{
    view->origin = frame.origin;
    view->bounds.origin = CGPointZero;
    view->bounds.size = frame.size;
    
    view->context = context;
    view->autoresizingMask = NSViewNotSizable;
    view->scale = 1.0f;
}

WKClassInfo WKViewClassInfo = { &WAKObjectClass, "WKView", _WKViewDealloc };

WKViewRef WKViewCreateWithFrame (CGRect frame, WKViewContext *context)
{
    WKViewRef view = static_cast<WKViewRef>(const_cast<void*>(WKCreateObjectWithSize(sizeof(struct _WKView), &WKViewClassInfo)));
    if (!view)
        return 0;
    
    WKViewInitialize (view, frame, context);
    
    return view;
}

void _WKViewSetViewContext (WKViewRef view, WKViewContext *context)
{
    if (!view) {
        WKError ("invalid parameter");
        return;
    }
    view->context = context;
}

CGRect WKViewGetBounds (WKViewRef view)
{
    if (!view) {
        WKError ("invalid parameter");
        return CGRectZero;
    }
    
    return view->bounds;
}

CGRect WKViewGetFrame (WKViewRef view)
{
    if (!view) {
        WKError ("invalid parameter");
        return CGRectZero;
    }
    
    return WKViewConvertRectToSuperview(view, view->bounds);
}

CGPoint WKViewGetOrigin(WKViewRef view)
{
    if (!view) {
        WKError("invalid parameter");
        return CGPointZero;
    }

    return view->origin;
}

static void _WKViewRecursivelyInvalidateGState(WKViewRef view)
{
    if (!view) {
        WKError ("invalid parameter");
        return;
    }

    if (view->context && view->context->invalidateGStateCallback)
        view->context->invalidateGStateCallback(view);
    
    WKViewRef subview = WKViewFirstChild(view);
    while (subview) {
        _WKViewRecursivelyInvalidateGState(subview);
        subview = WKViewNextSibling(subview);
    }
}

void WKViewSetFrameOrigin (WKViewRef view, CGPoint newOrigin)
{
    if (!view) {
        WKError ("invalid parameter");
        return;
    }
    
    ASSERT(!isinf(newOrigin.x));
    ASSERT(!isnan(newOrigin.x));
    ASSERT(!isinf(newOrigin.y));
    ASSERT(!isnan(newOrigin.y));
    
    if (!CGPointEqualToPoint(view->origin, newOrigin)) {
        view->origin = newOrigin;
        _WKViewRecursivelyInvalidateGState(view);
    }
}

#define XSIZING_BITS(mask)      ((mask) & 7)
#define YSIZING_BITS(mask)      (((mask) >> 3) & 7)

static void _WKViewAutoresizeCoord(bool bByHeight, unsigned int sizingMethod, const CGRect *origSuperFrame, const CGRect *newSuperFrame, CGRect *newFrame)
{
    CGFloat origSuperFrameWidthOrHeight;
    CGFloat newSuperFrameWidthOrHeight;
    CGFloat *origFrameXorY;
    CGFloat *origFrameWidthOrHeight;
    CGFloat prop;
    CGFloat tmp;
    CGFloat xOrY;
    CGFloat widthOrHeight;
    CGFloat origMarginsTotal;
    CGFloat origWidthMinusMinMargin;
    
    if (bByHeight) {
        sizingMethod = YSIZING_BITS(sizingMethod);
        origSuperFrameWidthOrHeight = origSuperFrame->size.height;
        newSuperFrameWidthOrHeight = newSuperFrame->size.height;
        origFrameXorY = &newFrame->origin.y;
        origFrameWidthOrHeight = &newFrame->size.height;
    } else {
        sizingMethod = XSIZING_BITS(sizingMethod);
        origSuperFrameWidthOrHeight = origSuperFrame->size.width;
        newSuperFrameWidthOrHeight = newSuperFrame->size.width;
        origFrameXorY = &newFrame->origin.x;
        origFrameWidthOrHeight = &newFrame->size.width;
    }
    
    xOrY = *origFrameXorY;
    widthOrHeight = *origFrameWidthOrHeight;
    
    switch (sizingMethod) {
        case NSViewNotSizable:
        case NSViewMaxXMargin:
            break;
        case NSViewWidthSizable:
            widthOrHeight = newSuperFrameWidthOrHeight - (origSuperFrameWidthOrHeight - widthOrHeight);
            if (widthOrHeight < 0.0f)
                widthOrHeight = 0.0f;
                break;
        case NSViewWidthSizable | NSViewMaxXMargin:
            origWidthMinusMinMargin = origSuperFrameWidthOrHeight - xOrY;
            if (widthOrHeight) {
                if (origWidthMinusMinMargin != 0.0f) {
                    prop = widthOrHeight / origWidthMinusMinMargin;
                } else {
                    prop = 0.0f;
                }
            } else {
                prop = 1.0f;
            }
                widthOrHeight = ((newSuperFrameWidthOrHeight - xOrY)) * prop;
            if (widthOrHeight < 0.0f)
                widthOrHeight = 0.0f;
                break;
        case NSViewMinXMargin:
            xOrY = newSuperFrameWidthOrHeight - (origSuperFrameWidthOrHeight - xOrY);
            if (xOrY < 0.0f)
                xOrY = 0.0f;
            break;
        case NSViewMinXMargin | NSViewMaxXMargin:
            origMarginsTotal = origSuperFrameWidthOrHeight - widthOrHeight;
            if (xOrY && origMarginsTotal != 0.0f) {
                prop = xOrY / origMarginsTotal;
            }
                // Do the 50/50 split even if XorY = 0 and the WidthOrHeight is only
                // one pixel shorter...
                // FIXME: If origMarginsTotal is in the range (0, 1) then we won't do the 50/50 split. Is this right?
                else if (origMarginsTotal == 0.0f 
                    || (abs(static_cast<int>(origMarginsTotal)) == 1)) {
                    prop = 0.5f;  // Then split it 50:50.
                }
                else {
                    prop = 1.0f;
                }
                xOrY = ((newSuperFrameWidthOrHeight - widthOrHeight)) * prop;
            if (xOrY < 0.0f)
                xOrY = 0.0f;
                break;
        case NSViewMinXMargin | NSViewWidthSizable:
            tmp = xOrY + widthOrHeight;
            if (tmp)
                prop = xOrY / tmp;
            else
                prop = 0.5f;
            xOrY = ((newSuperFrameWidthOrHeight - (origSuperFrameWidthOrHeight - tmp))) * prop;
            widthOrHeight  = newSuperFrameWidthOrHeight - (xOrY + (origSuperFrameWidthOrHeight - tmp));
            if (xOrY < 0.0f)
                xOrY = 0.0f;
                if (widthOrHeight < 0.0f)
                    widthOrHeight = 0.0f;
                    break;
        case NSViewMinXMargin | NSViewWidthSizable | NSViewMaxXMargin:
            if (origSuperFrameWidthOrHeight)
                prop = xOrY / origSuperFrameWidthOrHeight;
            else
                prop = 1.0f / 3.0f;
            xOrY = (newSuperFrameWidthOrHeight * prop);
            if (origSuperFrameWidthOrHeight)
                prop = widthOrHeight / origSuperFrameWidthOrHeight;
            else
                prop = 1.0f / 3.0f;
            widthOrHeight = (newSuperFrameWidthOrHeight * prop);
            break;
    }
    
    *origFrameXorY = floorf(xOrY);
    *origFrameWidthOrHeight = floorf(widthOrHeight);
}

void _WKViewAutoresize(WKViewRef view, const CGRect *oldSuperFrame, const CGRect *newSuperFrame)
{
    if (view->autoresizingMask != NSViewNotSizable) {
        CGRect newFrame = WKViewGetFrame(view);
        _WKViewAutoresizeCoord(false, view->autoresizingMask, oldSuperFrame, newSuperFrame, &newFrame);
        _WKViewAutoresizeCoord(true, view->autoresizingMask, oldSuperFrame, newSuperFrame, &newFrame);
        WKViewSetFrameOrigin(view, newFrame.origin);
        WKViewSetFrameSize(view, newFrame.size);
    }
}

static void _WKViewAutoresizeChildren(WKViewRef view, const CGRect *oldSuperFrame, const CGRect *newSuperFrame)
{
    WKViewRef child;
    for (child = WKViewFirstChild(view); child != NULL; child = WKViewNextSibling(child)) {
        _WKViewAutoresize(child, oldSuperFrame, newSuperFrame);
    }        
}

void WKViewSetFrameSize (WKViewRef view, CGSize newSize)
{
    if (!view) {
        WKError ("invalid parameter");
        return;
    }
    
    ASSERT(!isinf(newSize.width));
    ASSERT(!isnan(newSize.width));
    ASSERT(!isinf(newSize.height));
    ASSERT(!isnan(newSize.height));
    
    CGRect frame;
    frame.origin = CGPointZero;
    frame.size = newSize;
    CGSize boundsSize = WKViewConvertRectFromSuperview(view, frame).size;
    WKViewSetBoundsSize(view, boundsSize);
}

void WKViewSetBoundsSize (WKViewRef view, CGSize newSize)
{
    if (CGSizeEqualToSize(view->bounds.size, newSize))
        return;
    
    CGRect oldFrame = WKViewGetFrame(view);
    view->bounds.size = newSize;
    CGRect newFrame = WKViewGetFrame(view);
    
    if (view->context && view->context->notificationCallback)
        view->context->notificationCallback (view, WKViewNotificationViewFrameSizeChanged, view->context->notificationUserInfo);
    
    _WKViewAutoresizeChildren(view, &oldFrame, &newFrame);    
    _WKViewRecursivelyInvalidateGState(view);
}

void WKViewSetBoundsOrigin(WKViewRef view, CGPoint newOrigin)
{
    if (CGPointEqualToPoint(view->bounds.origin, newOrigin))
        return;

    view->bounds.origin = newOrigin;

    _WKViewRecursivelyInvalidateGState(view);    
}

void WKViewSetScale(WKViewRef view, float scale)
{
    ASSERT(!isinf(scale));
    ASSERT(!isnan(scale));
    
    if (view->scale == scale)
        return;
    
    view->scale = scale;
    
    if (view->context && view->context->notificationCallback)
        view->context->notificationCallback (view, WKViewNotificationViewFrameSizeChanged, view->context->notificationUserInfo);
}

float WKViewGetScale(WKViewRef view)
{
    return view->scale;
}

WAKWindow *WKViewGetWindow (WKViewRef view)
{
    if (!view) {
        WKError ("invalid parameter");
        return 0;
    }
    
    return view->window;
}

CFArrayRef WKViewGetSubviews (WKViewRef view)
{
    if (!view) {
        WKError ("invalid parameter");
        return 0;
    }
    
    return view->subviews;
}

void WKViewAddSubview (WKViewRef view, WKViewRef subview)
{
    if (!view || !subview) {
        WKError ("invalid parameter");
        return;
    }
    
    if (!view->subviews) {
        view->subviews = CFArrayCreateMutable(NULL, 0, &WKCollectionArrayCallBacks);
    }
    CFArrayAppendValue (view->subviews, subview);
    _WKViewSetSuperview (subview, view);
    
    // Set the window on subview and all it's children.
    _WKViewSetWindow (subview, view->window);
}

void WKViewRemoveFromSuperview (WKViewRef view)
{
    if (!view) {
        WKError ("invalid parameter");
        return;
    }

    _WKViewSetWindow (view, 0);

    if (!view->superview) {
        return;
    }
    
    CFMutableArrayRef svs = view->superview->subviews;
    if (!svs) {
        WKError ("superview has no subviews");
        return;
    }

    CFIndex index = WKArrayIndexOfValue (svs, view);
    if (index < 0) {
        WKError ("view not in superview subviews");
        return;
    }

    _WKViewWillRemoveSubview(view->superview, view);
    
    CFArrayRemoveValueAtIndex (svs, index);

    _WKViewSetSuperview (view, 0);
}

WKViewRef WKViewFirstChild (WKViewRef view)
{
    if (!view) {
        WKError ("invalid parameter");
        return 0;
    }

    CFArrayRef sv = view->subviews;
    
    if (!sv)
        return 0;
        
    CFIndex count = CFArrayGetCount (sv);
    if (!count)
        return 0;
        
    return static_cast<WKViewRef>(const_cast<void*>(CFArrayGetValueAtIndex(sv, 0)));
}

WKViewRef WKViewNextSibling (WKViewRef view)
{
    if (!view) {
        WKError ("invalid parameter");
        return 0;
    }

    if (!view->superview)
        return 0;
        
    CFArrayRef svs = view->superview->subviews;
    if (!svs)
        return 0;
        
    CFIndex thisIndex = WKArrayIndexOfValue (svs, view);
    if (thisIndex < 0) {
        WKError ("internal error, view is not present in superview subviews");
        return 0;
    }
    
    CFIndex count = CFArrayGetCount (svs);
    if (thisIndex+1 >= count)
        return 0;
        
    return static_cast<WKViewRef>(const_cast<void*>(CFArrayGetValueAtIndex(svs, thisIndex + 1)));
}

// To remove, see: <rdar://problem/10360425> Remove WKViewTraverseNext from Skankphone
WKViewRef WKViewTraverseNext(WKViewRef view)
{
    if (!view) {
        WKError ("invalid parameter");
        return 0;
    }

    WKViewRef firstChild = WKViewFirstChild(view);
    if (firstChild)
        return firstChild;

    WKViewRef nextSibling = WKViewNextSibling(view);
    if (nextSibling)
        return nextSibling;

    while (view && !WKViewNextSibling(view)) {
        WAKView *wakView = WAKViewForWKViewRef(view);
        WAKView *superView = [wakView superview];
        view = [superView _viewRef];
    }
        
    if (view)
        return WKViewNextSibling(view);
    
    return 0;
}

CGAffineTransform _WKViewGetTransform(WKViewRef view)
{
    CGAffineTransform transform;
    transform = CGAffineTransformMakeTranslation(view->origin.x, view->origin.y);
    transform = CGAffineTransformScale(transform, view->scale, view->scale);
    transform = CGAffineTransformTranslate(transform, -view->bounds.origin.x, -view->bounds.origin.y);
    return transform;
}

CGRect WKViewGetVisibleRect(WKViewRef viewRef)
{
    if (!viewRef) {
        WKError ("invalid parameter");
        return CGRectZero;
    }

    WKViewRef view = viewRef;
    CGRect rect = WKViewGetBounds(view);
    WKViewRef superview = view->superview;
    
    while (superview != NULL) {
        rect = WKViewConvertRectToSuperview(view, rect);
        rect = CGRectIntersection(WKViewGetBounds(superview), rect);
        view = superview;
        superview = superview->superview;
    }
    
    if (view != viewRef) {
        rect = WKViewConvertRectToBase(view, rect);
        rect = WKViewConvertRectFromBase(viewRef, rect);
    }
    
    return rect;
}

CGRect WKViewConvertRectToSuperview(WKViewRef view, CGRect r)
{
    if (!view) {
        WKError ("invalid parameter");
        return CGRectZero;
    }
    
    return CGRectApplyAffineTransform(r, _WKViewGetTransform(view));
}

CGRect WKViewConvertRectToBase(WKViewRef view, CGRect r)
{
    if (!view) {
        WKError ("invalid parameter");
        return CGRectZero;
    }

    CGRect aRect = r;
    
    while (view) {
        aRect = WKViewConvertRectToSuperview (view, aRect);    
        view = view->superview;
    }
    
    return aRect;
}

CGPoint WKViewConvertPointToSuperview(WKViewRef view, CGPoint p)
{
    if (!view) {
        WKError ("invalid parameter");
        return CGPointZero;
    }

    return CGPointApplyAffineTransform(p, _WKViewGetTransform(view));
}

CGPoint WKViewConvertPointFromSuperview(WKViewRef view, CGPoint p)
{
    if (!view) {
        WKError ("invalid parameter");
        return CGPointZero;
    }
    
    CGAffineTransform transform = CGAffineTransformInvert(_WKViewGetTransform(view));
    return CGPointApplyAffineTransform(p, transform);
}

CGPoint WKViewConvertPointToBase(WKViewRef view, CGPoint p)
{
    if (!view) {
        WKError ("invalid parameter");
        return CGPointZero;
    }

    CGPoint aPoint = p;
    
    while (view) {
        aPoint = WKViewConvertPointToSuperview (view, aPoint);    
        view = view->superview;
    }
    
    return aPoint;
}

#define VIEW_ARRAY_SIZE 128

static void _WKViewGetAncestorViewsIncludingView (WKViewRef view, WKViewRef *views, unsigned maxViews, unsigned *viewCount)
{
    unsigned count = 0;
    
    views[count++] = view;
    WKViewRef superview = view->superview;
    while (superview) {
        views[count++] = superview;
        if (count >= maxViews) {
            WKError ("Exceeded maxViews, use malloc/realloc");
            *viewCount = 0;
            return;
        }
        superview = superview->superview;
    }
    *viewCount = count;
}

CGPoint WKViewConvertPointFromBase(WKViewRef view, CGPoint p)
{
    if (!view) {
        WKError ("invalid parameter");
        return CGPointZero;
    }

    WKViewRef views[VIEW_ARRAY_SIZE];
    unsigned viewCount = 0;

    _WKViewGetAncestorViewsIncludingView (view, views, VIEW_ARRAY_SIZE, &viewCount);
    if (viewCount == 0)
        return CGPointZero;

    CGPoint aPoint = p;
    int i;
    for (i = viewCount-1; i >= 0; i--) {
        aPoint = WKViewConvertPointFromSuperview (views[i], aPoint);
    }
        
    return aPoint;
}

CGRect WKViewConvertRectFromSuperview(WKViewRef view, CGRect r)
{
    if (!view) {
        WKError ("invalid parameter");
        return CGRectZero;
    }
    
    CGAffineTransform transform = CGAffineTransformInvert(_WKViewGetTransform(view));
    return CGRectApplyAffineTransform(r, transform);
}

CGRect WKViewConvertRectFromBase(WKViewRef view, CGRect r)
{
    if (!view) {
        WKError ("invalid parameter");
        return CGRectZero;
    }

    WKViewRef views[VIEW_ARRAY_SIZE];
    unsigned viewCount = 0;

    _WKViewGetAncestorViewsIncludingView (view, views, VIEW_ARRAY_SIZE, &viewCount);
    if (viewCount == 0)
        return CGRectZero;
    
    CGRect aRect = r;
    int i;
    for (i = viewCount-1; i >= 0; i--) {
        aRect = WKViewConvertRectFromSuperview (views[i], aRect);
    }
        
    return aRect;
}

bool WKViewAcceptsFirstResponder (WKViewRef view)
{
    bool result = TRUE;
    if (view && view->context && view->context->responderCallback)
        result = view->context->responderCallback(view, WKViewResponderAcceptsFirstResponder, view->context->responderUserInfo);
    return result;
}

bool WKViewBecomeFirstResponder (WKViewRef view)
{
    bool result = TRUE;
    if (view && view->context && view->context->responderCallback)
        result = view->context->responderCallback(view, WKViewResponderBecomeFirstResponder, view->context->responderUserInfo);
    return result;
}

bool WKViewResignFirstResponder (WKViewRef view)
{
    bool result = TRUE;
    if (view && view->context && view->context->responderCallback)
        result = view->context->responderCallback(view, WKViewResponderResignFirstResponder, view->context->responderUserInfo);
    return result;
}

unsigned int WKViewGetAutoresizingMask(WKViewRef view)
{
    if (!view) {
        WKError ("invalid parameter");
        return 0;
    }    
    return view->autoresizingMask;
}

void WKViewSetAutoresizingMask (WKViewRef view, unsigned int mask)
{
    if (!view) {
        WKError ("invalid parameter");
        return;
    }    
    view->autoresizingMask = mask;
}

#endif // PLATFORM(IOS_FAMILY)
