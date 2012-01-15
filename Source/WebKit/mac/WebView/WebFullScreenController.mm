/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#if ENABLE(FULLSCREEN_API)

#import "WebFullScreenController.h"

#import "WebNSWindowExtras.h"
#import "WebPreferencesPrivate.h"
#import "WebViewInternal.h"
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <OSServices/Power.h>
#import <WebCore/AnimationList.h>
#import <WebCore/CSSPropertyNames.h>
#import <WebCore/Color.h>
#import <WebCore/DOMDocument.h>
#import <WebCore/DOMDocumentInternal.h>
#import <WebCore/DOMHTMLElement.h>
#import <WebCore/DOMWindow.h>
#import <WebCore/DisplaySleepDisabler.h>
#import <WebCore/Document.h>
#import <WebCore/EventListener.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLMediaElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/IntRect.h>
#import <WebCore/NodeList.h>
#import <WebCore/RenderBlock.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderLayerBacking.h>
#import <WebCore/SoftLinking.h>
#import <objc/objc-runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/UnusedParam.h>

static NSString* const isEnteringFullscreenKey = @"isEnteringFullscreen";

using namespace WebCore;

@interface WebFullscreenWindow : NSWindow
#ifndef BUILDING_ON_LEOPARD
<NSAnimationDelegate>
#endif
{
    NSView* _animationView;
    
    CALayer* _rendererLayer;
    CALayer* _backgroundLayer;
}
- (CALayer*)rendererLayer;
- (void)setRendererLayer:(CALayer*)rendererLayer;
- (CALayer*)backgroundLayer;
- (NSView*)animationView;
@end

class MediaEventListener : public EventListener {
public:
    static PassRefPtr<MediaEventListener> create(WebFullScreenController* delegate);
    virtual bool operator==(const EventListener&);
    virtual void handleEvent(ScriptExecutionContext*, Event*);

private:
    MediaEventListener(WebFullScreenController* delegate); 
    WebFullScreenController* delegate;
};

@interface WebFullScreenController(Private)
- (void)_requestExitFullscreenWithAnimation:(BOOL)animation;
- (void)_updateMenuAndDockForFullscreen;
- (void)_updatePowerAssertions;
- (WebFullscreenWindow *)_fullscreenWindow;
- (Document*)_document;
- (CFTimeInterval)_animationDuration;
- (BOOL)_isAnyMoviePlaying;
@end

@interface NSWindow(IsOnActiveSpaceAdditionForTigerAndLeopard)
- (BOOL)isOnActiveSpace;
@end

@implementation WebFullScreenController

#pragma mark -
#pragma mark Initialization
- (id)init
{
    // Do not defer window creation, to make sure -windowNumber is created (needed by WebWindowScaleAnimation).
    NSWindow *window = [[WebFullscreenWindow alloc] initWithContentRect:NSZeroRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    self = [super initWithWindow:window];
    [window release];
    if (!self)
        return nil;
    [self windowDidLoad];
    _mediaEventListener = MediaEventListener::create(self);
    return self;
    
}

- (void)dealloc
{
    [self setWebView:nil];
    [_placeholderView release];
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)windowDidLoad
{
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidResignActive:) name:NSApplicationDidResignActiveNotification object:NSApp];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidChangeScreenParameters:) name:NSApplicationDidChangeScreenParametersNotification object:NSApp];
}

#pragma mark -
#pragma mark Accessors

- (WebView*)webView
{
    return _webView;
}

- (void)setWebView:(WebView *)webView
{
    [webView retain];
    [_webView release];
    _webView = webView;
}

- (Element*)element
{
    return _element.get();
}

- (void)setElement:(PassRefPtr<Element>)element
{
    // When a new Element is set as the current full screen element, register event
    // listeners on that Element's window, listening for changes in media play states.
    // We will use these events to determine whether to disable the screensaver and 
    // display sleep timers when playing video in full screen. Make sure to unregister
    // the events on the old element's window, if necessary, as well.
    
    EventNames& eventNames = WebCore::eventNames();
    
    if (_element) {
        DOMWindow* window = _element->document()->domWindow();
        if (window) {
            window->removeEventListener(eventNames.playEvent,  _mediaEventListener.get(), true);
            window->removeEventListener(eventNames.pauseEvent, _mediaEventListener.get(), true);
            window->removeEventListener(eventNames.endedEvent, _mediaEventListener.get(), true);
        }
    }
        
    _element = element;
    
    if (_element) {
        DOMWindow* window = _element->document()->domWindow();
        if (window) {
            window->addEventListener(eventNames.playEvent,  _mediaEventListener, true);
            window->addEventListener(eventNames.pauseEvent, _mediaEventListener, true);
            window->addEventListener(eventNames.endedEvent, _mediaEventListener, true);
        }
    }
}

- (RenderBox*)renderer 
{
    return _renderer;
}

- (void)setRenderer:(RenderBox*)renderer
{
    _renderer = renderer;
}

#pragma mark -
#pragma mark Notifications

- (void)windowDidExitFullscreen:(BOOL)finished
{
    if (!_isAnimating)
        return;
    
    if (_isFullscreen)
        return;
    
    NSDisableScreenUpdates();
    ASSERT(_element);
    [self _document]->setFullScreenRendererBackgroundColor(Color::black);
    [self _document]->webkitDidExitFullScreenForElement(_element.get());
    [self setElement:nil];

    if (finished) {
        [self _updateMenuAndDockForFullscreen];   
        [self _updatePowerAssertions];
        
        [[_webView window] display];
        [[self _fullscreenWindow] setRendererLayer:nil];
        [[self window] close];
    }

    NSEnableScreenUpdates();

    _isAnimating = NO;
    [self autorelease]; // Associated -retain is in -exitFullscreen.
}

- (void)windowDidEnterFullscreen:(BOOL)finished
{
    if (!_isAnimating)
        return;
    
    if (!_isFullscreen)
        return;
    
    NSDisableScreenUpdates();
    [self _document]->webkitDidEnterFullScreenForElement(_element.get());
    [self _document]->setFullScreenRendererBackgroundColor(Color::black);
    
    if (finished) {
        [self _updateMenuAndDockForFullscreen];
        [self _updatePowerAssertions];
        [NSCursor setHiddenUntilMouseMoves:YES];
    
        // Move the webView into our fullscreen Window
        if (!_placeholderView)
            _placeholderView = [[NSView alloc] init];

        WebView *webView = [self webView];
        NSWindow *webWindow = [webView window];
        NSResponder *webWindowFirstResponder = [webWindow firstResponder];

        // Do not swap the placeholder into place if already is in a window,
        // assuming the placeholder's window will always be the webView's 
        // original window.
        if (![_placeholderView window]) {
            [_placeholderView setFrame:[webView frame]];        
            [_placeholderView setAutoresizingMask:[webView autoresizingMask]];
            [_placeholderView removeFromSuperview];
            [[webView superview] replaceSubview:webView with:_placeholderView];
            
            [[[self window] contentView] addSubview:webView];
            [[self window] makeResponder:webWindowFirstResponder firstResponderIfDescendantOfView:webView];
            [webView setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
            [webView setFrame:[(NSView *)[[self window] contentView] bounds]];
        }

#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
        // In Lion, NSWindow will animate into and out of orderOut operations. Suppress that
        // behavior here, making sure to reset the animation behavior afterward.
        NSWindowAnimationBehavior animationBehavior = [webWindow animationBehavior];
        [webWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];
#endif
        [webWindow orderOut:self];
#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
        [webWindow setAnimationBehavior:animationBehavior];
#endif
        
        WebFullscreenWindow* window = [self _fullscreenWindow];
        [window setBackgroundColor:[NSColor blackColor]];
        [window setOpaque:YES];

        [CATransaction begin];
        [CATransaction setValue:[NSNumber numberWithBool:YES] forKey:kCATransactionDisableActions];
        [[[window animationView] layer] setOpacity:0];
        [CATransaction commit];
    }
    NSEnableScreenUpdates();

    _isAnimating = NO;
}

- (void)animationDidStop:(CAAnimation *)anim finished:(BOOL)finished
{
    BOOL isEnteringFullscreenAnimation = [[anim valueForKey:isEnteringFullscreenKey] boolValue];
    
    if (!isEnteringFullscreenAnimation)
        [self windowDidExitFullscreen:finished];
    else
        [self windowDidEnterFullscreen:finished];
}

- (void)applicationDidResignActive:(NSNotification*)notification
{   
    // Check to see if the fullscreenWindow is on the active space; this function is available
    // on 10.6 and later, so default to YES if the function is not available:
    NSWindow* fullscreenWindow = [self _fullscreenWindow];
    BOOL isOnActiveSpace = ([fullscreenWindow respondsToSelector:@selector(isOnActiveSpace)] ? [fullscreenWindow isOnActiveSpace] : YES);

    // Replicate the QuickTime Player (X) behavior when losing active application status:
    // Is the fullscreen screen the main screen? (Note: this covers the case where only a 
    // single screen is available.)  Is the fullscreen screen on the current space? IFF so, 
    // then exit fullscreen mode. 
    if ([fullscreenWindow screen] == [[NSScreen screens] objectAtIndex:0] && isOnActiveSpace)
         [self _requestExitFullscreenWithAnimation:NO];
}

- (void)applicationDidChangeScreenParameters:(NSNotification*)notification
{
    // The user may have changed the main screen by moving the menu bar, or they may have changed
    // the Dock's size or location, or they may have changed the fullscreen screen's dimensions. 
    // Update our presentation parameters, and ensure that the full screen window occupies the 
    // entire screen:
    [self _updateMenuAndDockForFullscreen];
    NSWindow* window = [self window];
    [window setFrame:[[window screen] frame] display:YES];
}

#pragma mark -
#pragma mark Exposed Interface

- (void)enterFullscreen:(NSScreen *)screen
{
    // Disable animation if we are already in full-screen mode.
    BOOL shouldAnimate = !_isFullscreen;
    
    if (_isAnimating) {
        // The CAAnimation delegate functions will only be called the
        // next trip through the run-loop, so manually call the delegate
        // function here, letting it know the animation did not complete:
        [self windowDidExitFullscreen:NO];
        ASSERT(!_isAnimating);
    }
    _isFullscreen = YES;
    _isAnimating = YES;
    
    // setElement: must be called with a non-nil value before calling enterFullscreen:.
    ASSERT(_element);
    
    NSDisableScreenUpdates();
    
    if (!screen)
        screen = [NSScreen mainScreen];
    NSRect screenFrame = [screen frame];
    
    WebView* webView = [self webView];
    NSRect webViewFrame = [webView convertRectToBase:[webView frame]];
    webViewFrame.origin = [[webView window] convertBaseToScreen:webViewFrame.origin];
    
    NSRect elementFrame = _element->screenRect();
    
    // In the case of a multi-monitor setup where the webView straddles two
    // monitors, we must create a window large enough to contain the destination
    // frame and the initial frame.
    NSRect windowFrame = NSUnionRect(screenFrame, elementFrame);
    [[self window] setFrame:windowFrame display:YES];
    
    // In a previous incarnation, the NSWindow attached to this controller may have
    // been on a different screen. Temporarily change the collectionBehavior of the window:
    NSWindowCollectionBehavior behavior = [[self window] collectionBehavior];
    [[self window] setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces];
    [[self window] makeKeyAndOrderFront:self];
    [[self window] setCollectionBehavior:behavior];
    
    NSView* animationView = [[self _fullscreenWindow] animationView];
    
    NSRect backgroundBounds = {[[self window] convertScreenToBase:screenFrame.origin], screenFrame.size};
    backgroundBounds = [animationView convertRectFromBase:backgroundBounds];
    // Flip the background layer's coordinate system.
    backgroundBounds.origin.y = windowFrame.size.height - NSMaxY(backgroundBounds);
    
    // Set our fullscreen element's initial frame, and flip the coordinate systems from
    // screen coordinates (bottom/left) to layer coordinates (top/left):
    _initialFrame = NSRectToCGRect(NSIntersectionRect(elementFrame, webViewFrame));
    _initialFrame.origin.y = screenFrame.size.height - CGRectGetMaxY(_initialFrame);
    
    // Inform the document that we will begin entering full screen. This will change
    // pseudo-classes on the fullscreen element and the document element.
    Document* document = [self _document];
    document->webkitWillEnterFullScreenForElement(_element.get());
    
    // Check to see if the fullscreen renderer is composited. If not, accelerated graphics 
    // may be disabled. In this case, do not attempt to animate the contents into place; 
    // merely snap to the final position:
    if (!shouldAnimate || !_renderer || !_renderer->layer()->isComposited()) {
        [self windowDidEnterFullscreen:YES];
        NSEnableScreenUpdates();
        return;
    }    
    
    // Set up the final style of the FullScreen render block. Set an absolute
    // width and height equal to the size of the screen, and anchor the layer
    // at the top, left at (0,0). The RenderFullScreen style is already set 
    // to position:fixed.
    [self _document]->setFullScreenRendererSize(IntSize(screenFrame.size));
    [self _document]->setFullScreenRendererBackgroundColor(Color::transparent);
    
    // Cause the document to layout, thus calculating a new fullscreen element size:
    [self _document]->updateLayout();
    
    // FIXME: try to use the fullscreen element's calculated x, y, width, and height instead of the
    // renderBox functions:
    RenderBox* childRenderer = _renderer->firstChildBox();
    CGRect destinationFrame = CGRectMake(childRenderer->x(), childRenderer->y(), childRenderer->width(), childRenderer->height());    
    
    // Some properties haven't propogated from the GraphicsLayer to the CALayer yet. So
    // tell the renderer's layer to sync it's compositing state:
    GraphicsLayer* rendererGraphics = _renderer->layer()->backing()->graphicsLayer();
    rendererGraphics->syncCompositingState(destinationFrame);    

    CALayer* rendererLayer = rendererGraphics->platformLayer();
    [[self _fullscreenWindow] setRendererLayer:rendererLayer];    
    
    CFTimeInterval duration = [self _animationDuration];

    // Create a transformation matrix that will transform the renderer layer such that 
    // the fullscreen element appears to move from its starting position and size to its
    // final one. Perform the transformation in two steps, using the CALayer's matrix 
    // math to calculate the effects of each step:
    // 1. Apply a scale tranform to shrink the apparent size of the layer to the original
    //    element screen size.
    // 2. Apply a translation transform to move the shrunk layer into the same screen position
    //    as the original element.
    CATransform3D shrinkTransform = CATransform3DMakeScale(_initialFrame.size.width / destinationFrame.size.width, _initialFrame.size.height / destinationFrame.size.height, 1);
    [rendererLayer setTransform:shrinkTransform];
    CGRect shrunkDestinationFrame = [rendererLayer convertRect:destinationFrame toLayer:[animationView layer]];
    CATransform3D moveTransform = CATransform3DMakeTranslation(_initialFrame.origin.x - shrunkDestinationFrame.origin.x, _initialFrame.origin.y - shrunkDestinationFrame.origin.y, 0);
    CATransform3D finalTransform = CATransform3DConcat(shrinkTransform, moveTransform);
    [rendererLayer setTransform:finalTransform];

    CALayer* backgroundLayer = [[self _fullscreenWindow] backgroundLayer];

    // Start the opacity animation. We can use implicit animations here because we don't care when
    // the animation finishes.
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithDouble:duration] forKey:kCATransactionAnimationDuration];
    [backgroundLayer setOpacity:1];
    [CATransaction commit];
    
    // Use a CABasicAnimation here for the zoom effect. We want to be notified that the animation has 
    // completed by way of the CAAnimation delegate.
    CABasicAnimation* zoomAnimation = [CABasicAnimation animationWithKeyPath:@"transform"];
    [zoomAnimation setFromValue:[NSValue valueWithCATransform3D:finalTransform]];
    [zoomAnimation setToValue:[NSValue valueWithCATransform3D:CATransform3DIdentity]];
    [zoomAnimation setDelegate:self];
    [zoomAnimation setDuration:duration];
    [zoomAnimation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut]];
    [zoomAnimation setFillMode:kCAFillModeForwards];
    [zoomAnimation setValue:(id)kCFBooleanTrue forKey:isEnteringFullscreenKey];
    
    // Disable implicit animations and set the layer's transformation matrix to its final state.
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithBool:YES] forKey:kCATransactionDisableActions];
    [rendererLayer setTransform:CATransform3DIdentity];
    [rendererLayer addAnimation:zoomAnimation forKey:@"zoom"];
    [backgroundLayer setFrame:NSRectToCGRect(backgroundBounds)];
    [CATransaction commit];
    
    NSEnableScreenUpdates();
}

- (void)exitFullscreen
{
    if (!_isFullscreen)
        return;    
      
    CATransform3D startTransform = CATransform3DIdentity;
    if (_isAnimating) {
        if (_renderer && _renderer->layer()->isComposited()) {
            CALayer* rendererLayer = _renderer->layer()->backing()->graphicsLayer()->platformLayer();
            startTransform = [[rendererLayer presentationLayer] transform];
        }
        
        // The CAAnimation delegate functions will only be called the
        // next trip through the run-loop, so manually call the delegate
        // function here, letting it know the animation did not complete:
        [self windowDidEnterFullscreen:NO];
        ASSERT(!_isAnimating);
    }
    _isFullscreen = NO;
    _isAnimating = YES;
    
    NSDisableScreenUpdates();
    
    
    // The fullscreen animation may have been cancelled before the 
    // webView was moved to the fullscreen window. Check to see
    // if the _placeholderView exists and is in a window before
    // attempting to swap the webView back to it's original tree:
    if (_placeholderView && [_placeholderView window]) {
        // Move the webView back to its own native window:
        WebView* webView = [self webView];
        NSResponder *fullScreenWindowFirstResponder = [[self window] firstResponder];
        [webView setFrame:[_placeholderView frame]];
        [webView setAutoresizingMask:[_placeholderView autoresizingMask]];
        [webView removeFromSuperview];
        [[_placeholderView superview] replaceSubview:_placeholderView with:webView];
        [[webView window] makeResponder:fullScreenWindowFirstResponder firstResponderIfDescendantOfView:webView];
        
        NSWindow *webWindow = [[self webView] window];
#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
        // In Lion, NSWindow will animate into and out of orderOut operations. Suppress that
        // behavior here, making sure to reset the animation behavior afterward.
        NSWindowAnimationBehavior animationBehavior = [webWindow animationBehavior];
        [webWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];
#endif
        // The user may have moved the fullscreen window in Spaces, so temporarily change 
        // the collectionBehavior of the fullscreen window:
        NSWindowCollectionBehavior behavior = [webWindow collectionBehavior];
        [webWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces];
        [webWindow orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];
        [webWindow setCollectionBehavior:behavior];
#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
        [webWindow setAnimationBehavior:animationBehavior];
#endif

        // Because the animation view is layer-hosted, make sure to 
        // disable animations when changing the layer's opacity. Other-
        // wise, the content will appear to fade into view.
        [CATransaction begin];
        [CATransaction setValue:[NSNumber numberWithBool:YES] forKey:kCATransactionDisableActions];
        WebFullscreenWindow* window = [self _fullscreenWindow];
        [[[window animationView] layer] setOpacity:1];
        [window setBackgroundColor:[NSColor clearColor]];
        [window setOpaque:NO];
        [CATransaction commit];
    }

    NSView* animationView = [[self _fullscreenWindow] animationView];
    CGRect layerEndFrame = NSRectToCGRect([animationView convertRect:NSRectFromCGRect(_initialFrame) fromView:nil]);

    // The _renderer might be NULL due to its ancestor being removed:
    CGRect layerStartFrame = CGRectZero;
    if (_renderer) {
        RenderBox* childRenderer = _renderer->firstChildBox();
        layerStartFrame = CGRectMake(childRenderer->x(), childRenderer->y(), childRenderer->width(), childRenderer->height());
    }
    
    [self _document]->webkitWillExitFullScreenForElement(_element.get());
    [self _document]->updateLayout();
    
    // We have to retain ourselves because we want to be alive for the end of the animation.
    // If our owner releases us we could crash if this is not the case.
    // Balanced in windowDidExitFullscreen
    [self retain];
    
    // Check to see if the fullscreen renderer is composited. If not, accelerated graphics 
    // may be disabled. In this case, do not attempt to animate the contents into place; 
    // merely snap to the final position:
    if (!_renderer || !_renderer->layer()->isComposited()) {
        [self windowDidExitFullscreen:YES];
        NSEnableScreenUpdates();
        return;
    }
        
    GraphicsLayer* rendererGraphics = _renderer->layer()->backing()->graphicsLayer();
    
    [self _document]->setFullScreenRendererBackgroundColor(Color::transparent);

    rendererGraphics->syncCompositingState(layerEndFrame);    

    CALayer* rendererLayer = rendererGraphics->platformLayer();
    [[self _fullscreenWindow] setRendererLayer:rendererLayer];    
    
    // Create a transformation matrix that will transform the renderer layer such that 
    // the fullscreen element appears to move from the full screen to its original position 
    // and size. Perform the transformation in two steps, using the CALayer's matrix 
    // math to calculate the effects of each step:
    // 1. Apply a scale tranform to shrink the apparent size of the layer to the original
    //    element screen size.
    // 2. Apply a translation transform to move the shrunk layer into the same screen position
    //    as the original element.
    CATransform3D shrinkTransform = CATransform3DMakeScale(layerEndFrame.size.width / layerStartFrame.size.width, layerEndFrame.size.height / layerStartFrame.size.height, 1);
    [rendererLayer setTransform:shrinkTransform];
    CGRect shrunkDestinationFrame = [rendererLayer convertRect:layerStartFrame toLayer:[animationView layer]];
    CATransform3D moveTransform = CATransform3DMakeTranslation(layerEndFrame.origin.x - shrunkDestinationFrame.origin.x, layerEndFrame.origin.y - shrunkDestinationFrame.origin.y, 0);
    CATransform3D finalTransform = CATransform3DConcat(shrinkTransform, moveTransform);
    [rendererLayer setTransform:finalTransform];

    CFTimeInterval duration = [self _animationDuration];

    CALayer* backgroundLayer = [[self _fullscreenWindow] backgroundLayer];
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithDouble:duration] forKey:kCATransactionAnimationDuration];
    [backgroundLayer setOpacity:0];
    [CATransaction commit];
    
    CABasicAnimation* zoomAnimation = [CABasicAnimation animationWithKeyPath:@"transform"];
    [zoomAnimation setFromValue:[NSValue valueWithCATransform3D:startTransform]];
    [zoomAnimation setToValue:[NSValue valueWithCATransform3D:finalTransform]];
    [zoomAnimation setDelegate:self];
    [zoomAnimation setDuration:duration];
    [zoomAnimation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut]];
    [zoomAnimation setFillMode:kCAFillModeBoth];
    [zoomAnimation setRemovedOnCompletion:NO];
    [zoomAnimation setValue:(id)kCFBooleanFalse forKey:isEnteringFullscreenKey];
    
    [rendererLayer addAnimation:zoomAnimation forKey:@"zoom"];
    
    NSEnableScreenUpdates();
}

#pragma mark -
#pragma mark Internal Interface

- (void)_updateMenuAndDockForFullscreen
{
    // NSApplicationPresentationOptions is available on > 10.6 only:
#ifndef BUILDING_ON_LEOPARD
    NSApplicationPresentationOptions options = NSApplicationPresentationDefault;
    NSScreen* fullscreenScreen = [[self window] screen];
    
    if (_isFullscreen) {
        // Auto-hide the menu bar if the fullscreenScreen contains the menu bar:
        // NOTE: if the fullscreenScreen contains the menu bar but not the dock, we must still 
        // auto-hide the dock, or an exception will be thrown.
        if ([[NSScreen screens] objectAtIndex:0] == fullscreenScreen)
            options |= (NSApplicationPresentationAutoHideMenuBar | NSApplicationPresentationAutoHideDock);
        // Check if the current screen contains the dock by comparing the screen's frame to its
        // visibleFrame; if a dock is present, the visibleFrame will differ. If the current screen
        // contains the dock, hide it.
        else if (!NSEqualRects([fullscreenScreen frame], [fullscreenScreen visibleFrame]))
            options |= NSApplicationPresentationAutoHideDock;
    }
    
    if ([NSApp respondsToSelector:@selector(setPresentationOptions:)])
        [NSApp setPresentationOptions:options];
    else
#endif
        SetSystemUIMode(_isFullscreen ? kUIModeNormal : kUIModeAllHidden, 0);
}

- (void)_updatePowerAssertions
{
    BOOL isPlaying = [self _isAnyMoviePlaying];
    
    if (isPlaying && _isFullscreen) {
        if (!_displaySleepDisabler)
            _displaySleepDisabler = DisplaySleepDisabler::create("com.apple.WebKit - Fullscreen video");
    } else
        _displaySleepDisabler = nullptr;
}

- (void)_requestExit
{
    [self exitFullscreen];
    _forceDisableAnimation = NO;
}

- (void)_requestExitFullscreenWithAnimation:(BOOL)animation
{
    _forceDisableAnimation = !animation;
    [self performSelector:@selector(_requestExit) withObject:nil afterDelay:0];

}

- (BOOL)_isAnyMoviePlaying
{
#if ENABLE(VIDEO)
    if (!_element)
        return NO;
    
    Node* nextNode = _element.get();
    while (nextNode)
    {
        if (nextNode->hasTagName(HTMLNames::videoTag) && static_cast<Element*>(nextNode)->isMediaElement()) {
            HTMLMediaElement* element = static_cast<HTMLMediaElement*>(nextNode);
            if (!element->paused() && !element->ended())
                return YES;
        }
        
        nextNode = nextNode->traverseNextNode(_element.get());
    }
#endif
    
    return NO;
}

#pragma mark -
#pragma mark Utility Functions

- (WebFullscreenWindow *)_fullscreenWindow
{
    return (WebFullscreenWindow *)[self window];
}

- (Document*)_document 
{
    return _element->document();
}

- (CFTimeInterval)_animationDuration
{
    static const CFTimeInterval defaultDuration = 0.5;
    CFTimeInterval duration = defaultDuration;
#ifndef BUILDING_ON_LEOPARD
    NSUInteger modifierFlags = [NSEvent modifierFlags];
#else
    NSUInteger modifierFlags = [[NSApp currentEvent] modifierFlags];
#endif
    if ((modifierFlags & NSControlKeyMask) == NSControlKeyMask)
        duration *= 2;
    if ((modifierFlags & NSShiftKeyMask) == NSShiftKeyMask)
        duration *= 10;
    if (_forceDisableAnimation) {
        // This will disable scale animation
        duration = 0;
    }
    return duration;
}

@end

#pragma mark -
@implementation WebFullscreenWindow

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
{
    UNUSED_PARAM(aStyle);
    self = [super initWithContentRect:contentRect styleMask:NSBorderlessWindowMask backing:bufferingType defer:flag];
    if (!self)
        return nil;
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setIgnoresMouseEvents:NO];
    [self setAcceptsMouseMovedEvents:YES];
    [self setReleasedWhenClosed:NO];
    [self setHasShadow:YES];
#ifndef BUILDING_ON_LEOPARD
    [self setMovable:NO];
#else
    [self setMovableByWindowBackground:NO];
#endif
    
    NSView* contentView = [self contentView];
    _animationView = [[NSView alloc] initWithFrame:[contentView bounds]];
    
    RetainPtr<CALayer> contentLayer(AdoptNS, [[CALayer alloc] init]);
    [_animationView setLayer:contentLayer.get()];
    [_animationView setWantsLayer:YES];
    [_animationView setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    [contentView addSubview:_animationView];
    
    _backgroundLayer = [[CALayer alloc] init];
    [contentLayer.get() addSublayer:_backgroundLayer];
#ifndef BUILDING_ON_LEOPARD
    [contentLayer.get() setGeometryFlipped:YES];
#else
    [contentLayer.get() setSublayerTransform:CATransform3DMakeScale(1, -1, 1)];
#endif
    [contentLayer.get() setOpacity:0];
    
    [_backgroundLayer setBackgroundColor:CGColorGetConstantColor(kCGColorBlack)];
    [_backgroundLayer setOpacity:0];
    return self;
}

- (void)dealloc
{
    [_animationView release];
    [_backgroundLayer release];
    [_rendererLayer release];
    [super dealloc];
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (void)keyDown:(NSEvent *)theEvent
{
    if ([[theEvent charactersIgnoringModifiers] isEqual:@"\e"]) // Esacpe key-code
        [self cancelOperation:self];
    else [super keyDown:theEvent];
}

- (void)cancelOperation:(id)sender
{
    UNUSED_PARAM(sender);
    [[self windowController] _requestExitFullscreenWithAnimation:YES];
}

- (CALayer*)rendererLayer
{
    return _rendererLayer;
}

- (void)setRendererLayer:(CALayer *)rendererLayer
{
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithBool:YES] forKey:kCATransactionDisableActions];
    [rendererLayer retain];
    [_rendererLayer removeFromSuperlayer];
    [_rendererLayer release];
    _rendererLayer = rendererLayer;
    
    if (_rendererLayer)
        [[[self animationView] layer] addSublayer:_rendererLayer];
    [CATransaction commit];
}

- (CALayer*)backgroundLayer
{
    return _backgroundLayer;
}

- (NSView*)animationView
{
    return _animationView;
}
@end

#pragma mark -
#pragma mark MediaEventListener

MediaEventListener::MediaEventListener(WebFullScreenController* delegate)
    : EventListener(CPPEventListenerType)
    , delegate(delegate)
{ 
}

PassRefPtr<MediaEventListener> MediaEventListener::create(WebFullScreenController* delegate) 
{ 
    return adoptRef(new MediaEventListener(delegate)); 
}

bool MediaEventListener::operator==(const EventListener& listener)
{
    return this == &listener;
}

void MediaEventListener::handleEvent(ScriptExecutionContext* context, Event* event)
{
    [delegate _updatePowerAssertions];
}

#endif /* ENABLE(FULLSCREEN_API) */
