/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import "WebFrameInternal.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitSystemInterface.h"
#import "WebPluginContainerCheck.h"
#import "WebNetscapeContainerCheckContextInfo.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebView.h"
#import "WebViewInternal.h"

#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/AuthenticationMac.h>
#import <WebCore/BitmapImage.h>
#import <WebCore/Credential.h>
#import <WebCore/CredentialStorage.h>
#import <WebCore/CString.h>
#import <WebCore/Document.h>
#import <WebCore/Element.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/HaltablePlugin.h>
#import <WebCore/Page.h>
#import <WebCore/ProtectionSpace.h>
#import <WebCore/RenderView.h>
#import <WebCore/RenderWidget.h>
#import <WebKit/DOMPrivate.h>
#import <runtime/InitializeThreading.h>
#import <wtf/Assertions.h>

#define LoginWindowDidSwitchFromUserNotification    @"WebLoginWindowDidSwitchFromUserNotification"
#define LoginWindowDidSwitchToUserNotification      @"WebLoginWindowDidSwitchToUserNotification"

using namespace WebCore;

class WebHaltablePlugin : public HaltablePlugin {
public:
    WebHaltablePlugin(WebBaseNetscapePluginView* view)
        : m_view(view)
    {
    }
    
private:
    virtual void halt();
    virtual void restart();
    virtual Node* node() const;

    WebBaseNetscapePluginView* m_view;
};

void WebHaltablePlugin::halt()
{
    [m_view halt];
}

void WebHaltablePlugin::restart()
{ 
    [m_view resumeFromHalt];
}
    
Node* WebHaltablePlugin::node() const
{
    return [m_view element];
}

@implementation WebBaseNetscapePluginView

+ (void)initialize
{
    JSC::initializeThreading();
#ifndef BUILDING_ON_TIGER
    WebCoreObjCFinalizeOnMainThread(self);
#endif
    WKSendUserChangeNotifications();
}

- (id)initWithFrame:(NSRect)frame
      pluginPackage:(WebNetscapePluginPackage *)pluginPackage
                URL:(NSURL *)URL
            baseURL:(NSURL *)baseURL
           MIMEType:(NSString *)MIME
      attributeKeys:(NSArray *)keys
    attributeValues:(NSArray *)values
       loadManually:(BOOL)loadManually
            element:(PassRefPtr<WebCore::HTMLPlugInElement>)element
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;
    
    _pluginPackage = pluginPackage;
    _element = element;
    _sourceURL.adoptNS([URL copy]);
    _baseURL.adoptNS([baseURL copy]);
    _MIMEType.adoptNS([MIME copy]);
    
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    // Enable "kiosk mode" when instantiating the QT plug-in inside of Dashboard. See <rdar://problem/6878105>
    if ([[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.dashboard.client"] &&
        [[[_pluginPackage.get() bundle] bundleIdentifier] isEqualToString:@"com.apple.QuickTime Plugin.plugin"]) {
        RetainPtr<NSMutableArray> mutableKeys(AdoptNS, [keys mutableCopy]);
        RetainPtr<NSMutableArray> mutableValues(AdoptNS, [values mutableCopy]);

        [mutableKeys.get() addObject:@"kioskmode"];
        [mutableValues.get() addObject:@"true"];
        [self setAttributeKeys:mutableKeys.get() andValues:mutableValues.get()];
    } else
#endif
         [self setAttributeKeys:keys andValues:values];

    if (loadManually)
        _mode = NP_FULL;
    else
        _mode = NP_EMBED;
    
    _loadManually = loadManually;
    _haltable = new WebHaltablePlugin(self);
    return self;
}

- (void)dealloc
{
    ASSERT(!_isStarted);

    [super dealloc];
}

- (void)finalize
{
    ASSERT_MAIN_THREAD();
    ASSERT(!_isStarted);

    [super finalize];
}

- (WebNetscapePluginPackage *)pluginPackage
{
    return _pluginPackage.get();
}
    
- (BOOL)isFlipped
{
    return YES;
}

- (NSURL *)URLWithCString:(const char *)URLCString
{
    if (!URLCString)
        return nil;
    
    CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, URLCString, kCFStringEncodingISOLatin1);
    ASSERT(string); // All strings should be representable in ISO Latin 1
    
    NSString *URLString = [(NSString *)string _web_stringByStrippingReturnCharacters];
    NSURL *URL = [NSURL _web_URLWithDataAsString:URLString relativeToURL:_baseURL.get()];
    CFRelease(string);
    if (!URL)
        return nil;
    
    return URL;
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
    [request _web_setHTTPReferrer:frame->loader()->outgoingReferrer()];
    return request;
}

// Methods that subclasses must override
- (void)setAttributeKeys:(NSArray *)keys andValues:(NSArray *)values
{
    ASSERT_NOT_REACHED();
}

- (void)handleMouseMoved:(NSEvent *)event
{
    ASSERT_NOT_REACHED();
}

- (void)focusChanged
{
    ASSERT_NOT_REACHED();
}

- (void)windowFocusChanged:(BOOL)hasFocus
{
    ASSERT_NOT_REACHED();
}

- (BOOL)createPlugin
{
    ASSERT_NOT_REACHED();
    return NO;
}

- (void)loadStream
{
    ASSERT_NOT_REACHED();
}

- (BOOL)shouldStop
{
    ASSERT_NOT_REACHED();
    return YES;
}

- (void)destroyPlugin
{
    ASSERT_NOT_REACHED();
}

- (void)updateAndSetWindow
{
    ASSERT_NOT_REACHED();
}

- (void)sendModifierEventWithKeyCode:(int)keyCode character:(char)character
{
    ASSERT_NOT_REACHED();
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
    ASSERT([self window]);
    
    [self stopTimers];
    
    if (!_isStarted || [[self window] isMiniaturized])
        return;
    
    [self startTimers];
}

- (NSRect)_windowClipRect
{
    RenderObject* renderer = _element->renderer();
    
    if (renderer && renderer->view()) {
        if (FrameView* frameView = renderer->view()->frameView())
            return frameView->windowClipRectForLayer(renderer->enclosingLayer(), true);
    }
    
    return NSZeroRect;
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
    page->didStartPlugin(_haltable.get());

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

    if (Frame* frame = core([self webFrame])) {
        if (Page* page = frame->page())
            page->didStopPlugin(_haltable.get());
    }
    
    _isStarted = NO;
    
    [[self webView] removePluginInstanceView:self];
    
    // Stop the timers
    [self stopTimers];
    
    // Stop notifications and callbacks.
    [self removeWindowObservers];
    
    [self destroyPlugin];
}

- (void)halt
{
    ASSERT(!_isHalted);
    ASSERT(_isStarted);
    Element *element = [self element];
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    CGImageRef cgImage = CGImageRetain([core([self webFrame])->nodeImage(element) CGImageForProposedRect:nil context:nil hints:nil]);
#else
    RetainPtr<CGImageSourceRef> imageRef(AdoptCF, CGImageSourceCreateWithData((CFDataRef)[core([self webFrame])->nodeImage(element) TIFFRepresentation], 0));
    CGImageRef cgImage = CGImageSourceCreateImageAtIndex(imageRef.get(), 0, 0);
#endif
    ASSERT(cgImage);
    
    // BitmapImage will release the passed in CGImage on destruction.
    RefPtr<Image> nodeImage = BitmapImage::create(cgImage);
    ASSERT(element->renderer());
    toRenderWidget(element->renderer())->showSubstituteImage(nodeImage);
    [self stop];
    _isHalted = YES;  
    _hasBeenHalted = YES;
}

- (void)resumeFromHalt
{
    ASSERT(_isHalted);
    ASSERT(!_isStarted);
    [self start];
    
    if (_isStarted)
        _isHalted = NO;
    
    ASSERT([self element]->renderer());
    toRenderWidget([self element]->renderer())->showSubstituteImage(0);
}

- (BOOL)isHalted
{
    return _isHalted;
}

- (BOOL)hasBeenHalted
{
    return _hasBeenHalted;
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
            
            // Stop observing WebPreferencesChangedNotification -- we only need to observe this when installed in the view hierarchy.
            // When not in the view hierarchy, -viewWillMoveToWindow: and -viewDidMoveToWindow will start/stop the plugin as needed.
            [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedNotification object:nil];
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
        
        // Stop observing WebPreferencesChangedNotification -- we only need to observe this when installed in the view hierarchy.
        // When not in the view hierarchy, -viewWillMoveToWindow: and -viewDidMoveToWindow will start/stop the plugin as needed.
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedNotification object:nil];
    }
}

- (void)viewDidMoveToWindow
{
    [self resetTrackingRect];
    
    if ([self window]) {
        // While in the view hierarchy, observe WebPreferencesChangedNotification so that we can start/stop depending
        // on whether plugins are enabled.
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(preferencesHaveChanged:)
                                                     name:WebPreferencesChangedNotification
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
        
        // Remove WebPreferencesChangedNotification observer -- we will observe once again when we move back into the window
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedNotification object:nil];
    }
}

- (void)viewDidMoveToHostWindow
{
    if ([[self webView] hostWindow]) {
        // View now has an associated window. Start it if not already started.
        [self start];
    }
}

#pragma mark NOTIFICATIONS

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

- (void)renewGState
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
    return kit(_element->document()->frame());
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
    if (sourceSpace == destSpace)
        return TRUE;
    
    NSPoint sourcePoint = NSMakePoint(sourceX, sourceY);
    
    NSPoint sourcePointInScreenSpace;
    
    // First convert to screen space
    switch (sourceSpace) {
        case NPCoordinateSpacePlugin:
            sourcePointInScreenSpace = [self convertPoint:sourcePoint toView:nil];
            sourcePointInScreenSpace = [[self currentWindow] convertBaseToScreen:sourcePointInScreenSpace];
            break;
            
        case NPCoordinateSpaceWindow:
            sourcePointInScreenSpace = [[self currentWindow] convertBaseToScreen:sourcePoint];
            break;
            
        case NPCoordinateSpaceFlippedWindow:
            sourcePoint.y = [[self currentWindow] frame].size.height - sourcePoint.y;
            sourcePointInScreenSpace = [[self currentWindow] convertBaseToScreen:sourcePoint];
            break;
            
        case NPCoordinateSpaceScreen:
            sourcePointInScreenSpace = sourcePoint;
            break;
            
        case NPCoordinateSpaceFlippedScreen:
            sourcePoint.y = [[[NSScreen screens] objectAtIndex:0] frame].size.height - sourcePoint.y;
            sourcePointInScreenSpace = sourcePoint;
            break;
        default:
            return FALSE;
    }
    
    NSPoint destPoint;
    
    // Then convert back to the destination space
    switch (destSpace) {
        case NPCoordinateSpacePlugin:
            destPoint = [[self currentWindow] convertScreenToBase:sourcePointInScreenSpace];
            destPoint = [self convertPoint:destPoint fromView:nil];
            break;
            
        case NPCoordinateSpaceWindow:
            destPoint = [[self currentWindow] convertScreenToBase:sourcePointInScreenSpace];
            break;
            
        case NPCoordinateSpaceFlippedWindow:
            destPoint = [[self currentWindow] convertScreenToBase:sourcePointInScreenSpace];
            destPoint.y = [[self currentWindow] frame].size.height - destPoint.y;
            break;
            
        case NPCoordinateSpaceScreen:
            destPoint = sourcePointInScreenSpace;
            break;
            
        case NPCoordinateSpaceFlippedScreen:
            destPoint = sourcePointInScreenSpace;
            destPoint.y = [[[NSScreen screens] objectAtIndex:0] frame].size.height - destPoint.y;
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


- (CString)resolvedURLStringForURL:(const char*)url target:(const char*)target;
{
    String relativeURLString = String::fromUTF8(url);
    if (relativeURLString.isNull())
        return CString();
    
    Frame* frame = core([self webFrame]);
    if (!frame)
        return CString();

    Frame* targetFrame = frame->tree()->find(String::fromUTF8(target));
    if (!targetFrame)
        return CString();
    
    if (!frame->document()->securityOrigin()->canAccess(targetFrame->document()->securityOrigin()))
        return CString();
  
    KURL absoluteURL = targetFrame->loader()->completeURL(relativeURLString);
    return absoluteURL.string().utf8();
}

- (void)invalidatePluginContentRect:(NSRect)rect
{
    if (RenderBoxModelObject *renderer = toRenderBoxModelObject(_element->renderer())) {
        IntRect contentRect(rect);
        contentRect.move(renderer->borderLeft() + renderer->paddingLeft(), renderer->borderTop() + renderer->paddingTop());
        
        renderer->repaintRectangle(contentRect);
    }
}

@end

namespace WebKit {

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
CString proxiesForURL(NSURL *url)
{
    RetainPtr<CFDictionaryRef> systemProxies(AdoptCF, CFNetworkCopySystemProxySettings());
    if (!systemProxies)
        return "DIRECT";
    
    RetainPtr<CFArrayRef> proxiesForURL(AdoptCF, CFNetworkCopyProxiesForURL((CFURLRef)url, systemProxies.get()));
    CFIndex proxyCount = proxiesForURL ? CFArrayGetCount(proxiesForURL.get()) : 0;
    if (!proxyCount)
        return "DIRECT";
 
    // proxiesForURL is a CFArray of CFDictionaries. Each dictionary represents a proxy.
    // The format of the result should be:
    // "PROXY host[:port]" (for HTTP proxy) or
    // "SOCKS host[:port]" (for SOCKS proxy) or
    // A combination of the above, separated by semicolon, in the order that they should be tried.
    String proxies;
    for (CFIndex i = 0; i < proxyCount; ++i) {
        CFDictionaryRef proxy = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(proxiesForURL.get(), i));
        if (!proxy)
            continue;

        CFStringRef type = static_cast<CFStringRef>(CFDictionaryGetValue(proxy, kCFProxyTypeKey));
        bool isHTTP = type == kCFProxyTypeHTTP || type == kCFProxyTypeHTTPS;
        bool isSOCKS = type == kCFProxyTypeSOCKS;
        
        // We can only report HTTP and SOCKS proxies.
        if (!isHTTP && !isSOCKS)
            continue;
        
        CFStringRef host = static_cast<CFStringRef>(CFDictionaryGetValue(proxy, kCFProxyHostNameKey));
        CFNumberRef port = static_cast<CFNumberRef>(CFDictionaryGetValue(proxy, kCFProxyPortNumberKey));
        
        // If we are inserting multiple entries, add a separator
        if (!proxies.isEmpty())
            proxies += ";";
        
        if (isHTTP)
            proxies += "PROXY ";
        else if (isSOCKS)
            proxies += "SOCKS ";
        
        proxies += host;

        if (port) {
            SInt32 intPort;
            CFNumberGetValue(port, kCFNumberSInt32Type, &intPort);
            
            proxies += ":" + String::number(intPort);
        }
    }
    
    if (proxies.isEmpty())
        return "DIRECT";
    
    return proxies.utf8();
}
#endif

bool getAuthenticationInfo(const char* protocolStr, const char* hostStr, int32_t port, const char* schemeStr, const char* realmStr,
                           CString& username, CString& password)
{
    if (strcasecmp(protocolStr, "http") != 0 && 
        strcasecmp(protocolStr, "https") != 0)
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
    if (!strcasecmp(protocolStr, "http")) {
        if (!strcasecmp(schemeStr, "basic"))
            authenticationMethod = NSURLAuthenticationMethodHTTPBasic;
        else if (!strcasecmp(schemeStr, "digest"))
            authenticationMethod = NSURLAuthenticationMethodHTTPDigest;
    }
    
    RetainPtr<NSURLProtectionSpace> protectionSpace(AdoptNS, [[NSURLProtectionSpace alloc] initWithHost:host port:port protocol:protocol realm:realm authenticationMethod:authenticationMethod]);
    
    NSURLCredential *credential = mac(CredentialStorage::get(core(protectionSpace.get())));
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
    
} // namespace WebKit

#endif //  ENABLE(NETSCAPE_PLUGIN_API)

