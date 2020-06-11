/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 David Smith (catfish.man@gmail.com)
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

#import "WebViewData.h"

#import "WebKitLogging.h"
#import "WebPreferenceKeysPrivate.h"
#import "WebSelectionServiceController.h"
#import "WebViewGroup.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/AlternativeTextUIController.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/RunLoopObserver.h>
#import <WebCore/TextIndicatorWindow.h>
#import <WebCore/ValidationBubble.h>
#import <wtf/MainThread.h>
#import <wtf/RunLoop.h>

#if PLATFORM(IOS_FAMILY)
#import "WebGeolocationProviderIOS.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/WebCoreThreadInternal.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
#import "WebMediaPlaybackTargetPicker.h"
#endif

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
#import <WebCore/PlaybackSessionInterfaceMac.h>
#import <WebCore/PlaybackSessionModelMediaElement.h>
#endif

BOOL applicationIsTerminating = NO;
int pluginDatabaseClientCount = 0;

static CFRunLoopRef currentRunLoop()
{
#if PLATFORM(IOS_FAMILY)
    // A race condition during WebView deallocation can lead to a crash if the layer sync run loop
    // observer is added to the main run loop <rdar://problem/9798550>. However, for responsiveness,
    // we still allow this, see <rdar://problem/7403328>. Since the race condition and subsequent
    // crash are especially troublesome for iBooks, we never allow the observer to be added to the
    // main run loop in iBooks.
    if (WebCore::IOSApplication::isIBooks())
        return WebThreadRunLoop();
#endif
    return CFRunLoopGetCurrent();
}

void LayerFlushController::scheduleLayerFlush()
{
    m_layerFlushScheduler.schedule();
}

void LayerFlushController::invalidate()
{
    m_layerFlushScheduler.invalidate();
    m_webView = nullptr;
}

LayerFlushController::LayerFlushController(WebView* webView)
    : m_webView(webView)
    , m_layerFlushScheduler(this)
{
    ASSERT_ARG(webView, webView);
}

WebViewLayerFlushScheduler::WebViewLayerFlushScheduler(LayerFlushController* flushController)
    : m_flushController(flushController)
{
    m_runLoopObserver = makeUnique<WebCore::RunLoopObserver>(static_cast<CFIndex>(WebCore::RunLoopObserver::WellKnownRunLoopOrders::LayerFlush), [this]() {
        this->layerFlushCallback();
    });
}

WebViewLayerFlushScheduler::~WebViewLayerFlushScheduler()
{
}

void WebViewLayerFlushScheduler::schedule()
{
    m_runLoopObserver->schedule(currentRunLoop());
}

void WebViewLayerFlushScheduler::invalidate()
{
    m_runLoopObserver->invalidate();
}

void WebViewLayerFlushScheduler::layerFlushCallback()
{
    @autoreleasepool {
        RefPtr<LayerFlushController> protector = m_flushController;
        if (m_flushController->flushLayers())
            invalidate();
    }
}

#if PLATFORM(MAC)

@implementation WebWindowVisibilityObserver

- (instancetype)initWithView:(WebView *)view
{
    self = [super init];
    if (!self)
        return nil;

    _view = view;
    return self;
}

- (void)startObserving:(NSWindow *)window
{
    // An NSView derived object such as WebView cannot observe these notifications, because NSView itself observes them.
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowVisibilityChanged:)
                                                 name:@"NSWindowDidOrderOffScreenNotification" object:window];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowVisibilityChanged:)
                                                 name:@"_NSWindowDidBecomeVisible" object:window];
}

- (void)stopObserving:(NSWindow *)window
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:@"NSWindowDidOrderOffScreenNotification" object:window];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:@"_NSWindowDidBecomeVisible" object:window];
}

- (void)_windowVisibilityChanged:(NSNotification *)notification
{
    [_view _windowVisibilityChanged:notification];
}

@end

#endif // PLATFORM(MAC)

@implementation WebViewPrivate

+ (void)initialize
{
#if !PLATFORM(IOS_FAMILY)
    JSC::initializeThreading();
    RunLoop::initializeMain();
#endif
}

- (id)init 
{
    self = [super init];
    if (!self)
        return nil;

    allowsUndo = YES;
    usesPageCache = YES;
    shouldUpdateWhileOffscreen = YES;

#if !PLATFORM(IOS_FAMILY)
    windowOcclusionDetectionEnabled = YES;
#endif

    zoomMultiplier = 1;
    zoomsTextOnly = NO;

#if PLATFORM(MAC)
    interactiveFormValidationEnabled = YES;
#else
    interactiveFormValidationEnabled = NO;
#endif
    // The default value should be synchronized with WebCore/page/Settings.cpp.
    validationMessageTimerMagnification = 50;

#if PLATFORM(IOS_FAMILY)
    isStopping = NO;
    _geolocationProvider = [WebGeolocationProviderIOS sharedGeolocationProvider];
#endif

    shouldCloseWithWindow = false;

    pluginDatabaseClientCount++;

    m_alternativeTextUIController = makeUnique<WebCore::AlternativeTextUIController>();

    return self;
}

- (void)dealloc
{    
    ASSERT(applicationIsTerminating || !page);
    ASSERT(applicationIsTerminating || !preferences);
#if !PLATFORM(IOS_FAMILY)
    ASSERT(!insertionPasteboard);
#endif
#if ENABLE(VIDEO)
    ASSERT(!fullscreenController);
#endif

    [applicationNameForUserAgent release];
#if !PLATFORM(IOS_FAMILY)
    [backgroundColor release];
#else
    CGColorRelease(backgroundColor);
#endif
    [inspector release];
    [currentNodeHighlight release];
#if PLATFORM(MAC)
    [immediateActionController release];
#endif
    [hostWindow release];
    [policyDelegateForwarder release];
    [UIDelegateForwarder release];
    [frameLoadDelegateForwarder release];
    [editingDelegateForwarder release];
    [mediaStyle release];

#if ENABLE(REMOTE_INSPECTOR)
#if PLATFORM(IOS_FAMILY)
    [indicateLayer release];
#endif
#endif

#if PLATFORM(IOS_FAMILY)
    [UIKitDelegateForwarder release];
    [formDelegateForwarder release];
    [_caretChangeListeners release];
    [_fixedPositionContent release];
#endif

    [super dealloc];
}

@end
