/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
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

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "WebBaseNetscapePluginView.h"

#import "NetworkStorageSessionMap.h"
#import "WebFrameInternal.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/AuthenticationMac.h>
#import <WebCore/BitmapImage.h>
#import <WebCore/Credential.h>
#import <WebCore/CredentialStorage.h>
#import <WebCore/Document.h>
#import <WebCore/Element.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/Page.h>
#import <WebCore/ProtectionSpace.h>
#import <WebCore/RenderEmbeddedObject.h>
#import <WebCore/RenderView.h>
#import <WebCore/SecurityOrigin.h>
#import <WebKitLegacy/DOMPrivate.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/RunLoop.h>
#import <wtf/text/CString.h>

#define LoginWindowDidSwitchFromUserNotification    @"WebLoginWindowDidSwitchFromUserNotification"
#define LoginWindowDidSwitchToUserNotification      @"WebLoginWindowDidSwitchToUserNotification"

using namespace WebCore;

@implementation WebBaseNetscapePluginView

+ (void)initialize
{
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
    RunLoop::initializeMainRunLoop();
    WebKit::sendUserChangeNotifications();
}

- (id)initWithFrame:(NSRect)frame
      pluginPackage:(WebNetscapePluginPackage *)pluginPackage
                URL:(NSURL *)URL
            baseURL:(NSURL *)baseURL
           MIMEType:(NSString *)MIME
      attributeKeys:(NSArray *)keys
    attributeValues:(NSArray *)values
       loadManually:(BOOL)loadManually
            element:(RefPtr<WebCore::HTMLPlugInElement>&&)element
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;
    
    _pluginPackage = pluginPackage;
    _element = WTFMove(element);
    _sourceURL = adoptNS([URL copy]);
    _baseURL = adoptNS([baseURL copy]);
    _MIMEType = adoptNS([MIME copy]);

    [self setAttributeKeys:keys andValues:values];

    if (loadManually)
        _mode = NP_FULL;
    else
        _mode = NP_EMBED;
    
    _loadManually = loadManually;
    return self;
}

- (void)dealloc
{
    ASSERT(!_isStarted);

    [super dealloc];
}

- (WebNetscapePluginPackage *)pluginPackage
{
    return _pluginPackage.get();
}
    
- (BOOL)isFlipped
{
    return YES;
}

- (NSURL *)URLWithCString:(const char *)cString
{
    if (!cString)
        return nil;
    
    NSString *string = [NSString stringWithCString:cString encoding:NSISOLatin1StringEncoding];
    string = [string stringByReplacingOccurrencesOfString:@"\r" withString:@""];
    string = [string stringByReplacingOccurrencesOfString:@"\n" withString:@""];
    return [NSURL _web_URLWithDataAsString:string relativeToURL:_baseURL.get()];
}

- (NSMutableURLRequest *)requestWithURLCString:(const char *)URLCString
{
    NSURL *URL = [self URLWithCString:URLCString];
    if (!URL)
        return nil;
    
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL];
    Frame* frame = core([self webFrame]);
    if (!frame)
        return nil;
    [request _web_setHTTPReferrer:frame->loader().outgoingReferrer()];
    return request;
}

// Methods that subclasses must override
- (void)setAttributeKeys:(NSArray *)keys andValues:(NSArray *)values
{
    // This needs to be overridden by subclasses.
}

- (void)handleMouseMoved:(NSEvent *)event
{
    // This needs to be overridden by subclasses.
}

- (void)handleMouseEntered:(NSEvent *)event
{
    // This needs to be overridden by subclasses.
}

- (void)handleMouseExited:(NSEvent *)event
{
    // This needs to be overridden by subclasses.
}

- (void)focusChanged
{
    // This needs to be overridden by subclasses.
}

- (void)windowFocusChanged:(BOOL)hasFocus
{
    // This needs to be overridden by subclasses.
}

- (BOOL)createPlugin
{
    // This needs to be overridden by subclasses.
    return NO;
}

- (void)loadStream
{
    // This needs to be overridden by subclasses.
}

- (BOOL)shouldStop
{
    // This needs to be overridden by subclasses.
    return YES;
}

- (void)destroyPlugin
{
    // This needs to be overridden by subclasses.
}

- (void)updateAndSetWindow
{
    // This needs to be overridden by subclasses.
}

- (void)sendModifierEventWithKeyCode:(int)keyCode character:(char)character
{
    // This needs to be overridden by subclasses.
}

- (void)privateBrowsingModeDidChange
{
}

- (void)removeTrackingRect
{
    if (_trackingTag) {
        [self removeTrackingRect:_trackingTag];
        _trackingTag = 0;
        
        // Do the following after setting trackingTag to 0 so we don't re-enter.
        
        // Balance the retain in resetTrackingRect. Use autorelease in case we hold 
        // the last reference to the window during tear-down, to avoid crashing AppKit. 
        [[self window] autorelease];
    }
}

- (void)resetTrackingRect
{
    [self removeTrackingRect];
    if (_isStarted) {
        // Retain the window so that removeTrackingRect can work after the window is closed.
        [[self window] retain];
        _trackingTag = [self addTrackingRect:[self bounds] owner:self userData:nil assumeInside:NO];
    }
}

- (void)stopTimers
{
    _shouldFireTimers = NO;
}

- (void)startTimers
{
    _shouldFireTimers = YES;
}

- (void)restartTimers
{
    [self stopTimers];
    
    if (!_isStarted || [[self window] isMiniaturized])
        return;
    
    [self startTimers];
}

- (NSRect)_windowClipRect
{
    auto* renderer = _element->renderer();
    if (!is<RenderEmbeddedObject>(renderer))
        return NSZeroRect;
    return downcast<RenderEmbeddedObject>(*renderer).windowClipRect();
}

- (NSRect)visibleRect
{
    // WebCore may impose an additional clip (via CSS overflow or clip properties).  Fetch
    // that clip now.    
    return NSIntersectionRect([self convertRect:[self _windowClipRect] fromView:nil], [super visibleRect]);
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)sendActivateEvent:(BOOL)activate
{
    if (!_isStarted)
        return;
    
    [self windowFocusChanged:activate];
}

- (void)setHasFocus:(BOOL)flag
{
    if (!_isStarted)
        return;
    
    if (_hasFocus == flag)
        return;
    
    _hasFocus = flag;
    
    [self focusChanged];
}

- (void)addWindowObservers
{
    ASSERT([self window]);
    
    NSWindow *theWindow = [self window];
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self selector:@selector(windowWillClose:) 
                               name:NSWindowWillCloseNotification object:theWindow]; 
    [notificationCenter addObserver:self selector:@selector(windowBecameKey:)
                               name:NSWindowDidBecomeKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowResignedKey:)
                               name:NSWindowDidResignKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowDidMiniaturize:)
                               name:NSWindowDidMiniaturizeNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowDidDeminiaturize:)
                               name:NSWindowDidDeminiaturizeNotification object:theWindow];
    
    [notificationCenter addObserver:self selector:@selector(loginWindowDidSwitchFromUser:)
                               name:LoginWindowDidSwitchFromUserNotification object:nil];
    [notificationCenter addObserver:self selector:@selector(loginWindowDidSwitchToUser:)
                               name:LoginWindowDidSwitchToUserNotification object:nil];
}

- (void)removeWindowObservers
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:NSWindowWillCloseNotification        object:nil]; 
    [notificationCenter removeObserver:self name:NSWindowDidBecomeKeyNotification     object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidResignKeyNotification     object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidMiniaturizeNotification   object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidDeminiaturizeNotification object:nil];
    [notificationCenter removeObserver:self name:LoginWindowDidSwitchFromUserNotification   object:nil];
    [notificationCenter removeObserver:self name:LoginWindowDidSwitchToUserNotification     object:nil];
}

- (void)start
{
    ASSERT([self currentWindow]);
    
    if (_isStarted)
        return;
    
    if (_triedAndFailedToCreatePlugin)
        return;
    
    ASSERT([self webView]);
    
    if (![[[self webView] preferences] arePlugInsEnabled])
        return;
   
    Frame* frame = core([self webFrame]);
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;
    
    bool wasDeferring = page->defersLoading();
    if (!wasDeferring)
        page->setDefersLoading(true);

    BOOL result = [self createPlugin];
    
    if (!wasDeferring)
        page->setDefersLoading(false);

    if (!result) {
        _triedAndFailedToCreatePlugin = YES;
        return;
    }
    
    _isStarted = YES;

    [[self webView] addPluginInstanceView:self];

    if ([self currentWindow])
        [self updateAndSetWindow];

    if ([self window]) {
        [self addWindowObservers];
        if ([[self window] isKeyWindow]) {
            [self sendActivateEvent:YES];
        }
        [self restartTimers];
    }
    
    [self resetTrackingRect];
    
    [self loadStream];
}

- (void)stop
{
    if (![self shouldStop])
        return;
    
    [self removeTrackingRect];
    
    if (!_isStarted)
        return;

    _isStarted = NO;
    
    [[self webView] removePluginInstanceView:self];
    
    // Stop the timers
    [self stopTimers];
    
    // Stop notifications and callbacks.
    [self removeWindowObservers];
    
    [self destroyPlugin];
}

- (BOOL)shouldClipOutPlugin
{
    NSWindow *window = [self window];
    return !window || [window isMiniaturized] || [NSApp isHidden] || ![self isDescendantOf:[[self window] contentView]] || [self isHiddenOrHasHiddenAncestor];
}

- (BOOL)supportsSnapshotting
{
    return [_pluginPackage.get() supportsSnapshotting];
}

- (void)cacheSnapshot
{
    NSSize boundsSize = [self bounds].size;
    if (!boundsSize.height || !boundsSize.width)
        return;

    NSImage *snapshot = [[NSImage alloc] initWithSize:boundsSize];
        
    _snapshotting = YES;
    [snapshot lockFocus];
    [self drawRect:[self bounds]];
    [snapshot unlockFocus];
    _snapshotting = NO;
    
    _cachedSnapshot = adoptNS(snapshot);
}

- (void)clearCachedSnapshot
{
    _cachedSnapshot.clear();
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
    // We must remove the tracking rect before we move to the new window.
    // Once we move to the new window, it will be too late.
    [self removeTrackingRect];
    [self removeWindowObservers];
    
    // Workaround for: <rdar://problem/3822871> resignFirstResponder is not sent to first responder view when it is removed from the window
    [self setHasFocus:NO];
    
    if (!newWindow) {
        if ([[self webView] hostWindow]) {
            // View will be moved out of the actual window but it still has a host window.
            [self stopTimers];
        } else {
            // View will have no associated windows.
            [self stop];
            
            // Stop observing WebPreferencesChangedInternalNotification -- we only need to observe this when installed in the view hierarchy.
            // When not in the view hierarchy, -viewWillMoveToWindow: and -viewDidMoveToWindow will start/stop the plugin as needed.
            [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedInternalNotification object:nil];
        }
    }
}

- (void)viewWillMoveToSuperview:(NSView *)newSuperview
{
    if (!newSuperview) {
        // Stop the plug-in when it is removed from its superview.  It is not sufficient to do this in -viewWillMoveToWindow:nil, because
        // the WebView might still has a hostWindow at that point, which prevents the plug-in from being destroyed.
        // There is no need to start the plug-in when moving into a superview.  -viewDidMoveToWindow takes care of that.
        [self stop];
        
        // Stop observing WebPreferencesChangedInternalNotification -- we only need to observe this when installed in the view hierarchy.
        // When not in the view hierarchy, -viewWillMoveToWindow: and -viewDidMoveToWindow will start/stop the plugin as needed.
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedInternalNotification object:nil];
    }
}

- (void)viewDidMoveToWindow
{
    [self resetTrackingRect];
    
    if ([self window]) {
        // While in the view hierarchy, observe WebPreferencesChangedInternalNotification so that we can start/stop depending
        // on whether plugins are enabled.
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(preferencesHaveChanged:)
                                                     name:WebPreferencesChangedInternalNotification
                                                   object:nil];

        _isPrivateBrowsingEnabled = [[[self webView] preferences] privateBrowsingEnabled];
        
        // View moved to an actual window. Start it if not already started.
        [self start];

        // Starting the plug-in can result in it removing itself from the window so we need to ensure that we're still in
        // place before doing anything that requires a window.
        if ([self window]) {
            [self restartTimers];
            [self addWindowObservers];
        }
    } else if ([[self webView] hostWindow]) {
        // View moved out of an actual window, but still has a host window.
        // Call setWindow to explicitly "clip out" the plug-in from sight.
        // FIXME: It would be nice to do this where we call stopNullEvents in viewWillMoveToWindow.
        [self updateAndSetWindow];
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    if (!hostWindow && ![self window]) {
        // View will have no associated windows.
        [self stop];
        
        // Remove WebPreferencesChangedInternalNotification observer -- we will observe once again when we move back into the window
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedInternalNotification object:nil];
    }
}

- (void)viewDidMoveToHostWindow
{
    if ([[self webView] hostWindow]) {
        // View now has an associated window. Start it if not already started.
        [self start];
    }
}

// MARK: NOTIFICATIONS

- (void)windowWillClose:(NSNotification *)notification 
{
    [self stop]; 
} 

- (void)windowBecameKey:(NSNotification *)notification
{
    [self sendActivateEvent:YES];
    [self invalidatePluginContentRect:[self bounds]];
    [self restartTimers];
}

- (void)windowResignedKey:(NSNotification *)notification
{
    [self sendActivateEvent:NO];
    [self invalidatePluginContentRect:[self bounds]];
    [self restartTimers];
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
    [self stopTimers];
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    [self restartTimers];
}

- (void)loginWindowDidSwitchFromUser:(NSNotification *)notification
{
    [self stopTimers];
}

-(void)loginWindowDidSwitchToUser:(NSNotification *)notification
{
    [self restartTimers];
}

- (void)preferencesHaveChanged:(NSNotification *)notification
{
    WebPreferences *preferences = [[self webView] preferences];

    if ([notification object] != preferences)
        return;
    
    BOOL arePlugInsEnabled = [preferences arePlugInsEnabled];
    if (_isStarted != arePlugInsEnabled) {
        if (arePlugInsEnabled) {
            if ([self currentWindow]) {
                [self start];
            }
        } else {
            [self stop];
            [self invalidatePluginContentRect:[self bounds]];
        }
    }
    
    BOOL isPrivateBrowsingEnabled = [preferences privateBrowsingEnabled];
    if (isPrivateBrowsingEnabled != _isPrivateBrowsingEnabled) {
        _isPrivateBrowsingEnabled = isPrivateBrowsingEnabled;
        [self privateBrowsingModeDidChange];
    }
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)renewGState
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    [super renewGState];
    
    // -renewGState is called whenever the view's geometry changes.  It's a little hacky to override this method, but
    // much safer than walking up the view hierarchy and observing frame/bounds changed notifications, since you don't
    // have to track subsequent changes to the view hierarchy and add/remove notification observers.
    // NSOpenGLView uses the exact same technique to reshape its OpenGL surface.
    
    // All of the work this method does may safely be skipped if the view is not in a window.  When the view
    // is moved back into a window, everything should be set up correctly.
    if (![self window])
        return;
    
    [self updateAndSetWindow];
    
    [self resetTrackingRect];
    
    // Check to see if the plugin view is completely obscured (scrolled out of view, for example).
    // For performance reasons, we send null events at a lower rate to plugins which are obscured.
    BOOL oldIsObscured = _isCompletelyObscured;
    _isCompletelyObscured = NSIsEmptyRect([self visibleRect]);
    if (_isCompletelyObscured != oldIsObscured)
        [self restartTimers];
}

- (BOOL)becomeFirstResponder
{
    [self setHasFocus:YES];
    return YES;
}

- (BOOL)resignFirstResponder
{
    [self setHasFocus:NO];    
    return YES;
}

- (WebDataSource *)dataSource
{
    return [[self webFrame] _dataSource];
}

- (WebFrame *)webFrame
{
    return kit(_element->document().frame());
}

- (WebView *)webView
{
    return [[self webFrame] webView];
}

- (NSWindow *)currentWindow
{
    return [self window] ? [self window] : [[self webView] hostWindow];
}

- (WebCore::HTMLPlugInElement*)element
{
    return _element.get();
}

- (void)cut:(id)sender
{
    [self sendModifierEventWithKeyCode:7 character:'x'];
}

- (void)copy:(id)sender
{
    [self sendModifierEventWithKeyCode:8 character:'c'];
}

- (void)paste:(id)sender
{
    [self sendModifierEventWithKeyCode:9 character:'v'];
}

- (void)selectAll:(id)sender
{
    [self sendModifierEventWithKeyCode:0 character:'a'];
}

// AppKit doesn't call mouseDown or mouseUp on right-click. Simulate control-click
// mouseDown and mouseUp so plug-ins get the right-click event as they do in Carbon (3125743).
- (void)rightMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}


- (BOOL)convertFromX:(double)sourceX andY:(double)sourceY space:(NPCoordinateSpace)sourceSpace
                 toX:(double *)destX andY:(double *)destY space:(NPCoordinateSpace)destSpace
{
    // Nothing to do
    if (sourceSpace == destSpace) {
        if (destX)
            *destX = sourceX;
        if (destY)
            *destY = sourceY;
        return YES;
    }
    
    NSPoint sourcePoint = NSMakePoint(sourceX, sourceY);
    
    NSPoint sourcePointInScreenSpace;
    
    // First convert to screen space
    switch (sourceSpace) {
        case NPCoordinateSpacePlugin:
            sourcePointInScreenSpace = [self convertPoint:sourcePoint toView:nil];
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            sourcePointInScreenSpace = [[self currentWindow] convertBaseToScreen:sourcePointInScreenSpace];
            ALLOW_DEPRECATED_DECLARATIONS_END
            break;
            
        case NPCoordinateSpaceWindow:
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            sourcePointInScreenSpace = [[self currentWindow] convertBaseToScreen:sourcePoint];
            ALLOW_DEPRECATED_DECLARATIONS_END
            break;
            
        case NPCoordinateSpaceFlippedWindow:
            sourcePoint.y = [[self currentWindow] frame].size.height - sourcePoint.y;
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            sourcePointInScreenSpace = [[self currentWindow] convertBaseToScreen:sourcePoint];
            ALLOW_DEPRECATED_DECLARATIONS_END
            break;
            
        case NPCoordinateSpaceScreen:
            sourcePointInScreenSpace = sourcePoint;
            break;
            
        case NPCoordinateSpaceFlippedScreen:
            sourcePoint.y = [(NSScreen *)[[NSScreen screens] objectAtIndex:0] frame].size.height - sourcePoint.y;
            sourcePointInScreenSpace = sourcePoint;
            break;
        default:
            return FALSE;
    }
    
    NSPoint destPoint;
    
    // Then convert back to the destination space
    switch (destSpace) {
        case NPCoordinateSpacePlugin:
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            destPoint = [[self currentWindow] convertScreenToBase:sourcePointInScreenSpace];
            ALLOW_DEPRECATED_DECLARATIONS_END
            destPoint = [self convertPoint:destPoint fromView:nil];
            break;
            
        case NPCoordinateSpaceWindow:
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            destPoint = [[self currentWindow] convertScreenToBase:sourcePointInScreenSpace];
            ALLOW_DEPRECATED_DECLARATIONS_END
            break;
            
        case NPCoordinateSpaceFlippedWindow:
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            destPoint = [[self currentWindow] convertScreenToBase:sourcePointInScreenSpace];
            ALLOW_DEPRECATED_DECLARATIONS_END
            destPoint.y = [[self currentWindow] frame].size.height - destPoint.y;
            break;
            
        case NPCoordinateSpaceScreen:
            destPoint = sourcePointInScreenSpace;
            break;
            
        case NPCoordinateSpaceFlippedScreen:
            destPoint = sourcePointInScreenSpace;
            destPoint.y = [(NSScreen *)[[NSScreen screens] objectAtIndex:0] frame].size.height - destPoint.y;
            break;
            
        default:
            return FALSE;
    }
    
    if (destX)
        *destX = destPoint.x;
    if (destY)
        *destY = destPoint.y;
    
    return TRUE;
}


- (void)invalidatePluginContentRect:(NSRect)rect
{
    auto* renderer = _element->renderer();
    if (!is<RenderEmbeddedObject>(renderer))
        return;
    auto& object = downcast<RenderEmbeddedObject>(*renderer);
    IntRect contentRect(rect);
    contentRect.move(object.borderLeft() + object.paddingLeft(), object.borderTop() + object.paddingTop());
    object.repaintRectangle(contentRect);
}

- (NSRect)actualVisibleRectInWindow
{
    auto* renderer = _element->renderer();
    if (!is<RenderEmbeddedObject>(renderer))
        return NSZeroRect;
    auto& object = downcast<RenderEmbeddedObject>(*renderer);
    auto widgetRect = object.view().frameView().contentsToWindow(object.pixelSnappedAbsoluteClippedOverflowRect());
    return intersection(object.windowClipRect(), widgetRect);
}

- (CALayer *)pluginLayer
{
    // This needs to be overridden by subclasses.
    return nil;
}

- (BOOL)getFormValue:(NSString **)value
{
    // This needs to be overridden by subclasses.
    return false;
}

@end

namespace WebKit {

bool getAuthenticationInfo(const char* protocolStr, const char* hostStr, int32_t port, const char* schemeStr, const char* realmStr, CString& username, CString& password)
{
    if (!equalLettersIgnoringASCIICase(protocolStr, "http") && !equalLettersIgnoringASCIICase(protocolStr, "https"))
        return false;

    NSString *host = [NSString stringWithUTF8String:hostStr];
    if (!hostStr)
        return false;
    
    NSString *protocol = [NSString stringWithUTF8String:protocolStr];
    if (!protocol)
        return false;
    
    NSString *realm = [NSString stringWithUTF8String:realmStr];
    if (!realm)
        return NPERR_GENERIC_ERROR;
    
    NSString *authenticationMethod = NSURLAuthenticationMethodDefault;
    if (equalLettersIgnoringASCIICase(protocolStr, "http")) {
        if (equalLettersIgnoringASCIICase(schemeStr, "basic"))
            authenticationMethod = NSURLAuthenticationMethodHTTPBasic;
        else if (equalLettersIgnoringASCIICase(schemeStr, "digest"))
            authenticationMethod = NSURLAuthenticationMethodHTTPDigest;
    }
    
    RetainPtr<NSURLProtectionSpace> protectionSpace = adoptNS([[NSURLProtectionSpace alloc] initWithHost:host port:port protocol:protocol realm:realm authenticationMethod:authenticationMethod]);
    
    NSURLCredential *credential = NetworkStorageSessionMap::defaultStorageSession().credentialStorage().get(emptyString(), ProtectionSpace(protectionSpace.get())).nsCredential();
    if (!credential)
        credential = [[NSURLCredentialStorage sharedCredentialStorage] defaultCredentialForProtectionSpace:protectionSpace.get()];
    if (!credential)
        return false;
  
    if (![credential hasPassword])
        return false;
    
    username = [[credential user] UTF8String];
    password = [[credential password] UTF8String];
    
    return true;
}

void sendUserChangeNotifications()
{
    auto consoleConnectionChangeNotifyProc = [](CGSNotificationType type, CGSNotificationData, CGSByteCount, CGSNotificationArg) {
        NSString *notificationName = nil;
        if (type == kCGSessionConsoleConnect)
            notificationName = LoginWindowDidSwitchToUserNotification;
        else if (type == kCGSessionConsoleDisconnect)
            notificationName = LoginWindowDidSwitchFromUserNotification;
        else
            ASSERT_NOT_REACHED();
        [[NSNotificationCenter defaultCenter] postNotificationName:notificationName object:nil];
    };

    CGSRegisterNotifyProc(consoleConnectionChangeNotifyProc, kCGSessionConsoleConnect, nullptr);
    CGSRegisterNotifyProc(consoleConnectionChangeNotifyProc, kCGSessionConsoleDisconnect, nullptr);
}

} // namespace WebKit

#endif //  ENABLE(NETSCAPE_PLUGIN_API)

