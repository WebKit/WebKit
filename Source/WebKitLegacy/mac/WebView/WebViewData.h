/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Igalia S.L
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

#import "WebDelegateImplementationCaching.h"
#import "WebUIDelegate.h"
#if HAVE(TOUCH_BAR)
#import <pal/spi/cocoa/AVKitSPI.h>
#endif
#import <WebCore/AlternativeTextClient.h>
#import <WebCore/WebCoreKeyboardUIMode.h>
#import <wtf/HashMap.h>
#import <wtf/Lock.h>
#import <wtf/RetainPtr.h>
#import <wtf/ThreadingPrimitives.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import "WebCaretChangeListener.h"
#endif

namespace WebCore {
class AlternativeTextUIController;
class HistoryItem;
class Page;
class RunLoopObserver;
class TextIndicatorWindow;
class ValidationBubble;
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
class PlaybackSessionInterfaceMac;
class PlaybackSessionModelMediaElement;
#endif
}

@class UIImage;
@class WebUITextIndicatorData;
@class WebImmediateActionController;
@class WebInspector;
@class WebNodeHighlight;
@class WebPluginDatabase;
@class WebPreferences;
@class WebTextCompletionController;
@protocol WebFormDelegate;
@protocol WebDeviceOrientationProvider;
@protocol WebGeolocationProvider;
@protocol WebNotificationProvider;
#if ENABLE(VIDEO)
@class WebVideoFullscreenController;
#endif
#if ENABLE(FULLSCREEN_API)
@class WebFullScreenController;
#endif
#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(IOS_FAMILY)
@class WebIndicateLayer;
#endif

#if PLATFORM(IOS_FAMILY)
@class WAKWindow;
@class WebEvent;
@class WebFixedPositionContent;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
class WebMediaPlaybackTargetPicker;
#endif

#if PLATFORM(MAC)
@class WebWindowVisibilityObserver;
#endif

extern BOOL applicationIsTerminating;
extern int pluginDatabaseClientCount;

class WebViewGroup;
class WebViewRenderingUpdateScheduler;

#if ENABLE(SERVICE_CONTROLS)
class WebSelectionServiceController;
#endif

#if HAVE(TOUCH_BAR)
@class WebTextTouchBarItemController;
#endif

@interface WebWindowVisibilityObserver : NSObject {
    WebView *_view;
}

- (instancetype)initWithView:(WebView *)view;
- (void)startObserving:(NSWindow *)window;
- (void)stopObserving:(NSWindow *)window;
@end

// FIXME: This should be renamed to WebViewData.
@interface WebViewPrivate : NSObject {
@public
    WebCore::Page* page;
    RefPtr<WebViewGroup> group;

    id UIDelegate;
    RetainPtr<id> UIDelegateForwarder;
    id resourceProgressDelegate;
    id downloadDelegate;
    id policyDelegate;
    RetainPtr<id> policyDelegateForwarder;
    id frameLoadDelegate;
    RetainPtr<id> frameLoadDelegateForwarder;
    id <WebFormDelegate> formDelegate;
    id editingDelegate;
    RetainPtr<id> editingDelegateForwarder;
    id scriptDebugDelegate;
    id historyDelegate;
#if PLATFORM(IOS_FAMILY)
    RetainPtr<id> resourceProgressDelegateForwarder;
    RetainPtr<id> formDelegateForwarder;
#endif

    RetainPtr<WebInspector> inspector;
    RetainPtr<WebNodeHighlight> currentNodeHighlight;

#if PLATFORM(MAC)
    RetainPtr<WebImmediateActionController> immediateActionController;

#if HAVE(TOUCH_BAR)
    RetainPtr<NSTouchBar> _currentTouchBar;
    RetainPtr<NSTouchBar> _plainTextTouchBar;
    RetainPtr<NSTouchBar> _richTextTouchBar;
    RetainPtr<NSTouchBar> _passwordTextTouchBar;
    RetainPtr<WebTextTouchBarItemController> _textTouchBarItemController;
    RetainPtr<NSCandidateListTouchBarItem> _richTextCandidateListTouchBarItem;
    RetainPtr<NSCandidateListTouchBarItem> _plainTextCandidateListTouchBarItem;
    RetainPtr<NSCandidateListTouchBarItem> _passwordTextCandidateListTouchBarItem;
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    RetainPtr<AVTouchBarPlaybackControlsProvider> mediaTouchBarProvider;
    RetainPtr<AVTouchBarScrubber> mediaPlaybackControlsView;
#endif // ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

    BOOL _canCreateTouchBars;
    BOOL _isUpdatingTextTouchBar;
    BOOL _startedListeningToCustomizationEvents;
    BOOL _isCustomizingTouchBar;
    BOOL _isDeferringTextTouchBarUpdates;
    BOOL _needsDeferredTextTouchBarUpdate;
#endif // HAVE(TOUCH_BAR)

    std::unique_ptr<WebCore::TextIndicatorWindow> textIndicatorWindow;
    BOOL hasInitializedLookupObserver;
    RetainPtr<WebWindowVisibilityObserver> windowVisibilityObserver;
    BOOL windowOcclusionDetectionEnabled;
    RetainPtr<NSEvent> pressureEvent;
#endif // PLATFORM(MAC)

    RefPtr<WebCore::ValidationBubble> formValidationBubble;

    BOOL shouldMaintainInactiveSelection;

    BOOL allowsUndo;

    float zoomMultiplier;
    BOOL zoomsTextOnly;

    RetainPtr<NSString> applicationNameForUserAgent;
    WTF::String userAgent;
    BOOL userAgentOverridden;
    
    RetainPtr<WebPreferences> preferences;
#if PLATFORM(IOS_FAMILY)
    NSURL *userStyleSheetLocation;
#endif

    RetainPtr<NSWindow> hostWindow;

    int programmaticFocusCount;
    
    WebResourceDelegateImplementationCache resourceLoadDelegateImplementations;
    WebFrameLoadDelegateImplementationCache frameLoadDelegateImplementations;
    WebScriptDebugDelegateImplementationCache scriptDebugDelegateImplementations;
    WebHistoryDelegateImplementationCache historyDelegateImplementations;

    BOOL closed;
#if PLATFORM(IOS_FAMILY)
    BOOL closing;
#if ENABLE(ORIENTATION_EVENTS)
    NSUInteger deviceOrientation;
#endif
#endif
    BOOL shouldCloseWithWindow;
    BOOL mainFrameDocumentReady;
    BOOL drawsBackground;
    BOOL tabKeyCyclesThroughElementsChanged;
    BOOL becomingFirstResponder;
    BOOL becomingFirstResponderFromOutside;
    BOOL usesPageCache;

#if !PLATFORM(IOS_FAMILY)
    RetainPtr<NSColor> backgroundColor;
#else
    RetainPtr<CGColorRef> backgroundColor;
#endif

    RetainPtr<NSString> mediaStyle;
    
    BOOL hasSpellCheckerDocumentTag;
    NSInteger spellCheckerDocumentTag;

#if PLATFORM(IOS_FAMILY)
    BOOL isStopping;

    id UIKitDelegate;
    RetainPtr<id> UIKitDelegateForwarder;

    id WebMailDelegate;

    BOOL allowsMessaging;
    RetainPtr<NSMutableSet> _caretChangeListeners;
    id <WebCaretChangeListener> _caretChangeListener;

    CGSize fixedLayoutSize;
    BOOL mainViewIsScrollingOrZooming;
    int32_t didDrawTiles;
    WTF::Lock pendingFixedPositionLayoutRectMutex;
    CGRect pendingFixedPositionLayoutRect;
#endif
    
#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
    RetainPtr<WebUITextIndicatorData> textIndicatorData;
    RetainPtr<WebUITextIndicatorData> dataOperationTextIndicator;
    CGRect dragPreviewFrameInRootViewCoordinates;
    WebDragSourceAction dragSourceAction;
    RetainPtr<NSURL> draggedLinkURL;
    RetainPtr<NSString> draggedLinkTitle;
#endif

#if !PLATFORM(IOS_FAMILY)
    // WebKit has both a global plug-in database and a separate, per WebView plug-in database. Dashboard uses the per WebView database.
    RetainPtr<WebPluginDatabase> pluginDatabase;
#endif
    
    HashMap<unsigned long, RetainPtr<id>> identifierMap;

    BOOL _keyboardUIModeAccessed;
    WebCore::KeyboardUIMode _keyboardUIMode;

    BOOL shouldUpdateWhileOffscreen;

    // When this flag is set, next time a WebHTMLView draws, it needs to temporarily disable screen updates
    // so that the NSView drawing is visually synchronized with CALayer updates.
    BOOL needsOneShotDrawingSynchronization;
    BOOL postsAcceleratedCompositingNotifications;
    std::unique_ptr<WebViewRenderingUpdateScheduler> renderingUpdateScheduler;

#if !PLATFORM(IOS_FAMILY)
    NSPasteboard *insertionPasteboard;
    RetainPtr<NSImage> _mainFrameIcon;
#endif

    NSSize lastLayoutSize;

#if ENABLE(VIDEO)
    RetainPtr<WebVideoFullscreenController> fullscreenController;
    Vector<RetainPtr<WebVideoFullscreenController>> fullscreenControllersExiting;
#endif

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    RefPtr<WebCore::PlaybackSessionModelMediaElement> playbackSessionModel;
    RefPtr<WebCore::PlaybackSessionInterfaceMac> playbackSessionInterface;
#endif
    
#if ENABLE(FULLSCREEN_API)
    RetainPtr<WebFullScreenController> newFullscreenController;
#endif

#if ENABLE(REMOTE_INSPECTOR)
#if PLATFORM(IOS_FAMILY)
    RetainPtr<WebIndicateLayer> indicateLayer;
#endif
#endif

    id<WebGeolocationProvider> _geolocationProvider;
    id<WebDeviceOrientationProvider> m_deviceOrientationProvider;
    id<WebNotificationProvider> _notificationProvider;

#if ENABLE(SERVICE_CONTROLS)
    std::unique_ptr<WebSelectionServiceController> _selectionServiceController;
#endif

    BOOL interactiveFormValidationEnabled;
    int validationMessageTimerMagnification;

    float customDeviceScaleFactor;
#if PLATFORM(IOS_FAMILY)
    RetainPtr<WebFixedPositionContent> _fixedPositionContent;
#endif

    std::unique_ptr<WebCore::AlternativeTextUIController> m_alternativeTextUIController;

    RetainPtr<NSData> sourceApplicationAuditData;

    BOOL _didPerformFirstNavigation;

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    std::unique_ptr<WebMediaPlaybackTargetPicker> m_playbackTargetPicker;
#endif
}
@end
