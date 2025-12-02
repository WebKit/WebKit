/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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
#import "WebViewImpl.h"

#if PLATFORM(MAC)

#import "APIAttachment.h"
#import "APILegacyContextHistoryClient.h"
#import "APINavigation.h"
#import "APIPageConfiguration.h"
#import "CoreTextHelpers.h"
#import "FrameProcess.h"
#import "FullscreenClient.h"
#import "InsertTextOptions.h"
#import "Logging.h"
#import "NativeWebGestureEvent.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebMouseEvent.h"
#import "NativeWebWheelEvent.h"
#import "NetworkProcessMessages.h"
#import "PDFPluginIdentifier.h"
#import "PageClient.h"
#import "PageClientImplMac.h"
#import "PasteboardTypes.h"
#import "PickerDismissalReason.h"
#import "PlatformFontInfo.h"
#import "PlatformWritingToolsUtilities.h"
#import "PlaybackSessionManagerProxy.h"
#import "RemoteLayerTreeDrawingAreaProxyMac.h"
#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "TextChecker.h"
#import "TextCheckerState.h"
#import "TiledCoreAnimationDrawingAreaProxy.h"
#import "UIGamepadProvider.h"
#import "UndoOrRedo.h"
#import "ViewGestureController.h"
#import "WKEditCommand.h"
#import "WKErrorInternal.h"
#import "WKFullScreenWindowController.h"
#import "WKImmediateActionController.h"
#import "WKNSURLExtras.h"
#import "WKPDFHUDView.h"
#import "WKPanGestureController.h"
#import "WKPrintingView.h"
#import "WKQuickLookPreviewController.h"
#import "WKRevealItemPresenter.h"
#import "WKTextAnimationManagerMac.h"
#import "WKTextPlaceholder.h"
#import "WKViewLayoutStrategy.h"
#import "WKWebViewMac.h"
#import "WebBackForwardList.h"
#import "WebEditCommandProxy.h"
#import "WebEventFactory.h"
#import "WebFrameProxy.h"
#import "WebInspectorUIProxy.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "_WKCaptionStyleMenuController.h"
#import "_WKDragActionsInternal.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import "_WKThumbnailViewInternal.h"
#import "_WKWarningView.h"
#import "_WKWebViewTextInputNotifications.h"
#import <Carbon/Carbon.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/ActivityState.h>
#import <WebCore/AttributedString.h>
#import <WebCore/CGWindowUtilities.h>
#import <WebCore/CaretRectComputation.h>
#import <WebCore/CharacterRange.h>
#import <WebCore/ColorMac.h>
#import <WebCore/ColorSerialization.h>
#import <WebCore/CompositionHighlight.h>
#import <WebCore/DataDetectorElementInfo.h>
#import <WebCore/DestinationColorSpace.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DigitalCredentialsRequestData.h>
#import <WebCore/DragData.h>
#import <WebCore/DragItem.h>
#import <WebCore/Editor.h>
#import <WebCore/FixedContainerEdges.h>
#import <WebCore/FontAttributeChanges.h>
#import <WebCore/FontAttributes.h>
#import <WebCore/FrameIdentifier.h>
#import <WebCore/ImageAdapter.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/NowPlayingInfo.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PlatformDynamicRangeLimitCocoa.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/PlatformPlaybackSessionInterface.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/PlaybackSessionInterfaceMac.h>
#import <WebCore/PromisedAttachmentInfo.h>
#import <WebCore/ReferrerPolicy.h>
#import <WebCore/ResolvedCaptionDisplaySettingsOptions.h>
#import <WebCore/ShareableBitmap.h>
#import <WebCore/Site.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/TextRecognitionResult.h>
#import <WebCore/TextUndoInsertionMarkupMac.h>
#import <WebCore/TranslationContextMenuInfo.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <WebCore/WebCoreFullScreenPlaceholderView.h>
#import <WebCore/WebCoreFullScreenWindow.h>
#import <WebCore/WebCoreNSFontManagerExtras.h>
#import <WebCore/WebPlaybackControlsManager.h>
#import <WebCore/WebTextIndicatorLayer.h>
#import <WebKit/WKShareSheet.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebBackForwardList.h>
#import <pal/HysteresisActivity.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/cocoa/NSTouchBarSPI.h>
#import <pal/spi/cocoa/VisionKitCoreSPI.h>
#import <pal/spi/cocoa/WritingToolsSPI.h>
#import <pal/spi/cocoa/WritingToolsUISPI.h>
#import <pal/spi/mac/LookupSPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <pal/spi/mac/NSPasteboardSPI.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>
#import <pal/spi/mac/NSTextFinderSPI.h>
#import <pal/spi/mac/NSTextInputContextSPI.h>
#import <pal/spi/mac/NSViewSPI.h>
#import <pal/spi/mac/NSWindowSPI.h>
#import <ranges>
#import <sys/stat.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/FileSystem.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SetForScope.h>
#import <wtf/SoftLinking.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/spi/darwin/OSVariantSPI.h>
#import <wtf/text/MakeString.h>

#if HAVE(DIGITAL_CREDENTIALS_UI)
#import <WebKit/WKDigitalCredentialsPicker.h>
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
#include "MediaSessionCoordinatorProxyPrivate.h"
#endif

#import "AppKitSoftLink.h"
#import <pal/cocoa/RevealSoftLink.h>
#import <pal/cocoa/TranslationUIServicesSoftLink.h>
#import <pal/cocoa/VisionKitCoreSoftLink.h>
#import <pal/cocoa/WritingToolsUISoftLink.h>
#import <pal/mac/DataDetectorsSoftLink.h>

#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVTouchBarPlaybackControlsProvider)
SOFT_LINK_CLASS(AVKit, AVTouchBarScrubber)

static NSString * const WKMediaExitFullScreenItem = @"WKMediaExitFullScreenItem";
#endif // HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

WTF_DECLARE_CF_TYPE_TRAIT(CGImage);

@interface NSApplication ()
- (BOOL)isSpeaking;
- (void)speakString:(NSString *)string;
- (void)stopSpeaking:(id)sender;
@end

#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
// FIXME: Remove this once -setCanShowMediaSelectionButton: is declared in an SDK used by Apple's buildbot.
@interface AVTouchBarScrubber ()
- (void)setCanShowMediaSelectionButton:(BOOL)canShowMediaSelectionButton;
@end
#endif

// We use a WKMouseTrackingObserver as tracking area owner instead of the WKWebView. This is because WKWebView
// gets an implicit tracking area when it is first responder and we only want to process mouse events from our
// tracking area. Otherwise, it would lead to duplicate mouse events (rdar://88025610).
@interface WKMouseTrackingObserver : NSObject
@end

@implementation WKMouseTrackingObserver {
    WeakPtr<WebKit::WebViewImpl> _impl;
}

- (instancetype)initWithViewImpl:(WebKit::WebViewImpl&)impl
{
    if ((self = [super init]))
        _impl = impl;
    return self;
}

- (void)mouseMoved:(NSEvent *)event
{
    if (CheckedPtr impl = _impl.get())
        impl->mouseMoved(event);
}

- (void)mouseEntered:(NSEvent *)event
{
    if (CheckedPtr impl = _impl.get())
        impl->mouseEntered(event);
}

- (void)mouseExited:(NSEvent *)event
{
    if (CheckedPtr impl = _impl.get())
        impl->mouseExited(event);
}

@end

@interface WKAccessibilitySettingsObserver : NSObject {
    WeakPtr<WebKit::WebViewImpl> _impl;
}

- (instancetype)initWithImpl:(WebKit::WebViewImpl&)impl;
@end

@implementation WKAccessibilitySettingsObserver

- (instancetype)initWithImpl:(WebKit::WebViewImpl&)impl
{
    self = [super init];
    if (!self)
        return nil;

    _impl = &impl;

    RetainPtr workspaceNotificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
    [workspaceNotificationCenter addObserver:self selector:@selector(_settingsDidChange:) name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:nil];

    return self;
}

- (void)dealloc
{
    RetainPtr workspaceNotificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
    [workspaceNotificationCenter removeObserver:self name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:nil];

    [super dealloc];
}

- (void)_settingsDidChange:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->accessibilitySettingsDidChange();
}

@end

@interface WKWindowVisibilityObserver : NSObject
- (instancetype)initWithView:(NSView *)view impl:(WebKit::WebViewImpl&)impl;
- (void)startObserving:(NSWindow *)window;
- (void)stopObserving;
- (void)enableObservingFontPanel;
- (void)startObservingFontPanel;
- (void)startObservingLookupDismissalIfNeeded;
@end

@implementation WKWindowVisibilityObserver {
    WeakPtr<WebKit::WebViewImpl> _impl;
    WeakObjCPtr<NSWindow> _window;

    BOOL _didRegisterForLookupPopoverCloseNotifications;
    BOOL _shouldObserveFontPanel;
    BOOL _isObservingFontPanel;
}

- (instancetype)initWithView:(NSView *)view impl:(WebKit::WebViewImpl&)impl
{
    RELEASE_ASSERT(isMainRunLoop());
    self = [super init];
    if (!self)
        return nil;

    _impl = impl;

    RetainPtr workspaceNotificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
    [workspaceNotificationCenter addObserver:self selector:@selector(_activeSpaceDidChange:) name:NSWorkspaceActiveSpaceDidChangeNotification object:nil];

    return self;
}

- (void)dealloc
{
    RELEASE_ASSERT(isMainRunLoop());

    [self stopObserving];

    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [retainPtr([[NSWorkspace sharedWorkspace] notificationCenter]) removeObserver:self];

    [super dealloc];
}

static void* keyValueObservingContext = &keyValueObservingContext;

- (void)startObserving:(NSWindow *)window
{
    RELEASE_ASSERT(isMainRunLoop());

    if (_window.get() == window)
        return;

    [self stopObserving];

    _window = window;
    if (!window)
        return;

    RetainPtr defaultNotificationCenter = [NSNotificationCenter defaultCenter];

    // An NSView derived object such as WKView cannot observe these notifications, because NSView itself observes them.
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidOrderOffScreen:) name:NSWindowDidOrderOffScreenNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidOrderOnScreen:) name:NSWindowDidOrderOnScreenNotification object:window];

    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:nil];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidResignKey:) name:NSWindowDidResignKeyNotification object:nil];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidMiniaturize:) name:NSWindowDidMiniaturizeNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidDeminiaturize:) name:NSWindowDidDeminiaturizeNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidMove:) name:NSWindowDidMoveNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidResize:) name:NSWindowDidResizeNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowWillBeginSheet:) name:NSWindowWillBeginSheetNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeBackingProperties:) name:NSWindowDidChangeBackingPropertiesNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeScreen:) name:NSWindowDidChangeScreenNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeOcclusionState:) name:NSWindowDidChangeOcclusionStateNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowWillClose:) name:NSWindowWillCloseNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowWillEnterOrExitFullScreen:) name:NSWindowWillEnterFullScreenNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidEnterOrExitFullScreen:) name:NSWindowDidEnterFullScreenNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowWillEnterOrExitFullScreen:) name:NSWindowWillExitFullScreenNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidEnterOrExitFullScreen:) name:NSWindowDidExitFullScreenNotification object:window];

    [defaultNotificationCenter addObserver:self selector:@selector(_screenDidChangeColorSpace:) name:NSScreenColorSpaceDidChangeNotification object:nil];
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    [defaultNotificationCenter addObserver:self selector:@selector(_applicationShouldBeginSuppressingHDR:) name:NSApplicationShouldBeginSuppressingHighDynamicRangeContentNotification object:NSApp];
    [defaultNotificationCenter addObserver:self selector:@selector(_applicationShouldEndSuppressingHDR:) name:NSApplicationShouldEndSuppressingHighDynamicRangeContentNotification object:NSApp];
#endif // HAVE(SUPPORT_HDR_DISPLAY_APIS)

    if (_shouldObserveFontPanel) {
        ASSERT(!_isObservingFontPanel);
        [self startObservingFontPanel];
    }

    if (objc_getAssociatedObject(window, _impl.get()))
        return;

    objc_setAssociatedObject(window, _impl.get(), @YES, OBJC_ASSOCIATION_COPY_NONATOMIC);
    [window addObserver:self forKeyPath:@"contentLayoutRect" options:NSKeyValueObservingOptionInitial context:keyValueObservingContext];
    [window addObserver:self forKeyPath:@"titlebarAppearsTransparent" options:NSKeyValueObservingOptionInitial context:keyValueObservingContext];
}

- (void)stopObserving
{
    RELEASE_ASSERT(isMainRunLoop());

    if (_isObservingFontPanel) {
        ASSERT(_shouldObserveFontPanel);
        _isObservingFontPanel = NO;
        [[NSFontPanel sharedFontPanel] removeObserver:self forKeyPath:@"visible" context:keyValueObservingContext];
    }

    RetainPtr<NSWindow> window = std::exchange(_window, nil).get();
    if (!window)
        return;

    RetainPtr defaultNotificationCenter = [NSNotificationCenter defaultCenter];

    [defaultNotificationCenter removeObserver:self name:NSWindowDidOrderOffScreenNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidOrderOnScreenNotification object:window.get()];

    [defaultNotificationCenter removeObserver:self name:NSWindowDidBecomeKeyNotification object:nil];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidResignKeyNotification object:nil];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidMiniaturizeNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidDeminiaturizeNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidMoveNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidResizeNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowWillBeginSheetNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidChangeBackingPropertiesNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidChangeScreenNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:_NSWindowDidChangeContentsHostedInLayerSurfaceNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidChangeOcclusionStateNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowWillCloseNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowWillEnterFullScreenNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidEnterFullScreenNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowWillExitFullScreenNotification object:window.get()];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidExitFullScreenNotification object:window.get()];

    [defaultNotificationCenter removeObserver:self name:NSScreenColorSpaceDidChangeNotification object:nil];

    if (!objc_getAssociatedObject(window.get(), _impl.get()))
        return;

    objc_setAssociatedObject(window.get(), _impl.get(), nil, OBJC_ASSOCIATION_COPY_NONATOMIC);
    [window removeObserver:self forKeyPath:@"contentLayoutRect" context:keyValueObservingContext];
    [window removeObserver:self forKeyPath:@"titlebarAppearsTransparent" context:keyValueObservingContext];
}

- (void)enableObservingFontPanel
{
    RELEASE_ASSERT(isMainRunLoop());
    _shouldObserveFontPanel = YES;
    [self startObservingFontPanel];
}

- (void)startObservingFontPanel
{
    ASSERT(_shouldObserveFontPanel);
    if (_isObservingFontPanel)
        return;
    _isObservingFontPanel = YES;
    [[NSFontPanel sharedFontPanel] addObserver:self forKeyPath:@"visible" options:0 context:keyValueObservingContext];
}

- (void)startObservingLookupDismissalIfNeeded
{
    RELEASE_ASSERT(isMainRunLoop());
    if (_didRegisterForLookupPopoverCloseNotifications)
        return;

    _didRegisterForLookupPopoverCloseNotifications = YES;
#if !ENABLE(REVEAL)
    if (PAL::canLoad_Lookup_LUNotificationPopoverWillClose())
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_dictionaryLookupPopoverWillClose:) name:PAL::get_Lookup_LUNotificationPopoverWillCloseSingleton() object:nil];
#endif
}

- (void)_windowDidOrderOnScreen:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowDidOrderOnScreen();
}

- (void)_windowDidOrderOffScreen:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowDidOrderOffScreen();
}

- (void)_windowDidBecomeKey:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowDidBecomeKey(retainPtr([notification object]).get());
}

- (void)_windowDidResignKey:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowDidResignKey(retainPtr([notification object]).get());
}

- (void)_windowDidMiniaturize:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowDidMiniaturize();
}

- (void)_windowDidDeminiaturize:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowDidDeminiaturize();
}

- (void)_windowDidMove:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowDidMove();
}

- (void)_windowDidResize:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowDidResize();
}

- (void)_windowWillBeginSheet:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowWillBeginSheet();
}

- (void)_windowDidChangeBackingProperties:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get()) {
        CGFloat oldBackingScaleFactor = [[retainPtr(notification.userInfo) objectForKey:NSBackingPropertyOldScaleFactorKey] doubleValue];
        impl->windowDidChangeBackingProperties(oldBackingScaleFactor);
    }
}

- (void)_windowDidChangeScreen:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowDidChangeScreen();
}

- (void)_windowDidChangeOcclusionState:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowDidChangeOcclusionState();
}

- (void)_windowWillClose:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->windowWillClose();
}

- (void)_screenDidChangeColorSpace:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->screenDidChangeColorSpace();
}

#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
- (void)_applicationShouldBeginSuppressingHDR:(NSNotification *)notification
{
    if (_impl)
        _impl->applicationShouldSuppressHDR(true);
}

- (void)_applicationShouldEndSuppressingHDR:(NSNotification *)notification
{
    if (_impl)
        _impl->applicationShouldSuppressHDR(false);
}
#endif // HAVE(SUPPORT_HDR_DISPLAY_APIS)

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context != keyValueObservingContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }

    if (CheckedPtr impl = _impl.get()) {
        if ([keyPath isEqualToString:@"visible"] && [NSFontPanel sharedFontPanelExists] && object == [NSFontPanel sharedFontPanel]) {
            impl->updateFontManagerIfNeeded();
            return;
        }
        if ([keyPath isEqualToString:@"contentLayoutRect"] || [keyPath isEqualToString:@"titlebarAppearsTransparent"])
            impl->updateContentInsetsIfAutomatic();
    }
}

#if !ENABLE(REVEAL)
- (void)_dictionaryLookupPopoverWillClose:(NSNotification *)notification
{
    if (_impl)
        _impl->clearTextIndicatorWithAnimation(WebCore::TextIndicatorDismissalAnimation::None);
}
#endif

- (void)_windowDidEnterOrExitFullScreen:(NSNotification *)notification
{
    if (_impl)
        _impl->windowDidEnterOrExitFullScreen();
}

- (void)_windowWillEnterOrExitFullScreen:(NSNotification *)notification
{
    if (_impl)
        _impl->windowWillEnterOrExitFullScreen();
}

- (void)_activeSpaceDidChange:(NSNotification *)notification
{
    if (CheckedPtr impl = _impl.get())
        impl->activeSpaceDidChange();
}

@end

@interface WKFlippedView : NSView
@end

@implementation WKFlippedView

- (instancetype)initWithFrame:(NSRect)frame
{
    if (self = [super initWithFrame:frame])
        [self _commonInitialize];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (self = [super initWithCoder:coder])
        [self _commonInitialize];
    return self;
}

- (void)_commonInitialize
{
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
}

- (BOOL)isFlipped
{
    return YES;
}

@end

@interface WKResponderChainSink : NSResponder {
    WeakObjCPtr<NSResponder> _lastResponderInChain;
    bool _didReceiveUnhandledCommand;
}

- (id)initWithResponderChain:(NSResponder *)chain;
- (void)detach;
- (bool)didReceiveUnhandledCommand;
@end

@implementation WKResponderChainSink

- (id)initWithResponderChain:(NSResponder *)chain
{
    self = [super init];
    if (!self)
        return nil;
    RetainPtr current = chain;
    while (RetainPtr next = [current nextResponder])
        current = next.get();
    [current setNextResponder:self];
    _lastResponderInChain = current.get();
    return self;
}

- (void)detach
{
    // This assumes that the responder chain was either unmodified since
    // -initWithResponderChain: was called, or was modified in such a way
    // that _lastResponderInChain is still in the chain, and self was not
    // moved earlier in the chain than _lastResponderInChain.
    RetainPtr responderBeforeSelf = _lastResponderInChain.get();
    RetainPtr next = [responderBeforeSelf nextResponder];
    for (; next && next != self; next = [next nextResponder])
        responderBeforeSelf = next;

    // Nothing to be done if we are no longer in the responder chain.
    if (next != self)
        return;

    [responderBeforeSelf setNextResponder:[self nextResponder]];
    _lastResponderInChain = nil;
}

- (bool)didReceiveUnhandledCommand
{
    return _didReceiveUnhandledCommand;
}

- (void)noResponderFor:(SEL)selector
{
    _didReceiveUnhandledCommand = true;
}

- (void)doCommandBySelector:(SEL)selector
{
    _didReceiveUnhandledCommand = true;
}

- (BOOL)tryToPerform:(SEL)action with:(id)object
{
    _didReceiveUnhandledCommand = true;
    return YES;
}

@end

@interface WKDOMPasteMenuDelegate : NSObject<NSMenuDelegate>
- (instancetype)initWithWebViewImpl:(WebKit::WebViewImpl&)impl pasteAccessCategory:(WebCore::DOMPasteAccessCategory)category;
@end

@implementation WKDOMPasteMenuDelegate {
    WeakPtr<WebKit::WebViewImpl> _impl;
    WebCore::DOMPasteAccessCategory _category;
}

- (instancetype)initWithWebViewImpl:(WebKit::WebViewImpl&)impl pasteAccessCategory:(WebCore::DOMPasteAccessCategory)category
{
    if (!(self = [super init]))
        return nil;

    _impl = impl;
    _category = category;
    return self;
}

- (void)menuDidClose:(NSMenu *)menu
{
    RunLoop::mainSingleton().dispatch([impl = _impl] {
        if (impl)
            impl->hideDOMPasteMenuWithResult(WebCore::DOMPasteAccessResponse::DeniedForGesture);
    });
}

- (NSInteger)numberOfItemsInMenu:(NSMenu *)menu
{
    return 1;
}

- (void)_web_grantDOMPasteAccess
{
    if (CheckedPtr impl = _impl.get())
        impl->handleDOMPasteRequestForCategoryWithResult(_category, WebCore::DOMPasteAccessResponse::GrantedForGesture);
}

@end

@interface WKPromisedAttachmentContext : NSObject {
@private
    RetainPtr<NSString> _fileName;
    RetainPtr<NSString> _attachmentIdentifier;
}

- (instancetype)initWithIdentifier:(NSString *)identifier fileName:(NSString *)fileName;

@property (nonatomic, readonly) NSString *fileName;
@property (nonatomic, readonly) NSString *attachmentIdentifier;

@end

@implementation WKPromisedAttachmentContext

- (instancetype)initWithIdentifier:(NSString *)identifier fileName:(NSString *)fileName
{
    if (!(self = [super init]))
        return nil;

    _fileName = fileName;
    _attachmentIdentifier = identifier;
    return self;
}

- (NSString *)fileName
{
    return _fileName.get();
}

- (NSString *)attachmentIdentifier
{
    return _attachmentIdentifier.get();
}

@end

#if HAVE(TOUCH_BAR)

@interface WKTextListTouchBarViewController : NSViewController {
@private
    WeakPtr<WebKit::WebViewImpl> _webViewImpl;
    WebKit::ListType _currentListType;
}

@property (nonatomic) WebKit::ListType currentListType;

- (instancetype)initWithWebViewImpl:(WebKit::WebViewImpl*)webViewImpl;

@end

@implementation WKTextListTouchBarViewController

@synthesize currentListType = _currentListType;

static const CGFloat listControlSegmentWidth = 67;
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER) && ENABLE(FULLSCREEN_API)
static const CGFloat exitFullScreenButtonWidth = 64;
#endif

static const NSUInteger noListSegment = 0;
static const NSUInteger unorderedListSegment = 1;
static const NSUInteger orderedListSegment = 2;

- (instancetype)initWithWebViewImpl:(WebKit::WebViewImpl*)webViewImpl
{
    if (!(self = [super init]))
        return nil;

    _webViewImpl = webViewImpl;

    RetainPtr insertListControl = [NSSegmentedControl segmentedControlWithLabels:@[ WebCore::insertListTypeNone().createNSString().get(), WebCore::insertListTypeBulleted().createNSString().get(), WebCore::insertListTypeNumbered().createNSString().get() ] trackingMode:NSSegmentSwitchTrackingSelectOne target:self action:@selector(_selectList:)];
    [insertListControl setWidth:listControlSegmentWidth forSegment:noListSegment];
    [insertListControl setWidth:listControlSegmentWidth forSegment:unorderedListSegment];
    [insertListControl setWidth:listControlSegmentWidth forSegment:orderedListSegment];
    insertListControl.get().font = [NSFont systemFontOfSize:15];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<id> segmentElement = NSAccessibilityUnignoredDescendant(insertListControl.get());
    RetainPtr<NSArray> segments = [segmentElement accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
    ASSERT(segments.get().count == 3);
    [retainPtr(segments.get()[noListSegment]) accessibilitySetOverrideValue:WebCore::insertListTypeNone().createNSString().get() forAttribute:NSAccessibilityDescriptionAttribute];
    [retainPtr(segments.get()[unorderedListSegment]) accessibilitySetOverrideValue:WebCore::insertListTypeBulletedAccessibilityTitle().createNSString().get() forAttribute:NSAccessibilityDescriptionAttribute];
    [retainPtr(segments.get()[orderedListSegment]) accessibilitySetOverrideValue:WebCore::insertListTypeNumberedAccessibilityTitle().createNSString().get() forAttribute:NSAccessibilityDescriptionAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END

    self.view = insertListControl.get();

    return self;
}

- (void)didDestroyView
{
    _webViewImpl = nullptr;
}

- (void)_selectList:(id)sender
{
    CheckedPtr webViewImpl = _webViewImpl.get();
    if (!webViewImpl)
        return;

    RetainPtr insertListControl = dynamic_objc_cast<NSSegmentedControl>(self.view);
    switch (insertListControl.get().selectedSegment) {
    case noListSegment:
        // There is no "remove list" edit command, but InsertOrderedList and InsertUnorderedList both
        // behave as toggles, so we can invoke the appropriate edit command depending on our _currentListType
        // to remove an existing list. We don't have to do anything if _currentListType is NoList.
        if (_currentListType == WebKit::ListType::OrderedList)
            webViewImpl->page().executeEditCommand(@"InsertOrderedList", @"");
        else if (_currentListType == WebKit::ListType::UnorderedList)
            webViewImpl->page().executeEditCommand(@"InsertUnorderedList", @"");
        break;
    case unorderedListSegment:
        webViewImpl->page().executeEditCommand(@"InsertUnorderedList", @"");
        break;
    case orderedListSegment:
        webViewImpl->page().executeEditCommand(@"InsertOrderedList", @"");
        break;
    }

    webViewImpl->dismissTextTouchBarPopoverItemWithIdentifier(NSTouchBarItemIdentifierTextList);
}

- (void)setCurrentListType:(WebKit::ListType)listType
{
    RetainPtr insertListControl = dynamic_objc_cast<NSSegmentedControl>(self.view);
    switch (listType) {
    case WebKit::ListType::None:
        [insertListControl setSelected:YES forSegment:noListSegment];
        break;
    case WebKit::ListType::OrderedList:
        [insertListControl setSelected:YES forSegment:orderedListSegment];
        break;
    case WebKit::ListType::UnorderedList:
        [insertListControl setSelected:YES forSegment:unorderedListSegment];
        break;
    }

    _currentListType = listType;
}

@end

@interface WKTextTouchBarItemController : NSTextTouchBarItemController <NSCandidateListTouchBarItemDelegate, NSTouchBarDelegate> {
@private
    BOOL _textIsBold;
    BOOL _textIsItalic;
    BOOL _textIsUnderlined;
    NSTextAlignment _currentTextAlignment;
    RetainPtr<NSColor> _textColor;
    RetainPtr<WKTextListTouchBarViewController> _textListTouchBarViewController;

@private
    WeakPtr<WebKit::WebViewImpl> _webViewImpl;
}

@property (nonatomic) BOOL textIsBold;
@property (nonatomic) BOOL textIsItalic;
@property (nonatomic) BOOL textIsUnderlined;
@property (nonatomic) NSTextAlignment currentTextAlignment;
@property (nonatomic, retain, readwrite) NSColor *textColor;

- (instancetype)initWithWebViewImpl:(WebKit::WebViewImpl*)webViewImpl;
@end

@implementation WKTextTouchBarItemController

@synthesize textIsBold = _textIsBold;
@synthesize textIsItalic = _textIsItalic;
@synthesize textIsUnderlined = _textIsUnderlined;
@synthesize currentTextAlignment = _currentTextAlignment;

- (instancetype)initWithWebViewImpl:(WebKit::WebViewImpl*)webViewImpl
{
    if (!(self = [super init]))
        return nil;

    _webViewImpl = webViewImpl;

    return self;
}

- (void)didDestroyView
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    _webViewImpl = nullptr;
    [_textListTouchBarViewController didDestroyView];
}

#pragma mark NSTouchBarDelegate

- (NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar makeItemForIdentifier:(NSString *)identifier
{
    return [self itemForIdentifier:identifier];
}

- (NSTouchBarItem *)itemForIdentifier:(NSString *)identifier
{
    RetainPtr item = [super itemForIdentifier:identifier];
    BOOL isTextFormatItem = [identifier isEqualToString:NSTouchBarItemIdentifierTextFormat];

    if (isTextFormatItem || [identifier isEqualToString:NSTouchBarItemIdentifierTextStyle])
        self.textStyle.action = @selector(_wkChangeTextStyle:);

    if (isTextFormatItem || [identifier isEqualToString:NSTouchBarItemIdentifierTextAlignment])
        self.textAlignments.action = @selector(_wkChangeTextAlignment:);

    RetainPtr<NSColorPickerTouchBarItem> colorPickerItem;
    if ([identifier isEqualToString:NSTouchBarItemIdentifierTextColorPicker])
        colorPickerItem = dynamic_objc_cast<NSColorPickerTouchBarItem>(item.get());
    if (isTextFormatItem)
        colorPickerItem = self.colorPickerItem;
    if (colorPickerItem) {
        colorPickerItem.get().target = self;
        colorPickerItem.get().action = @selector(_wkChangeColor:);
        colorPickerItem.get().showsAlpha = NO;
        colorPickerItem.get().allowedColorSpaces = @[ [NSColorSpace sRGBColorSpace] ];
    }

    return item.autorelease();
}

#pragma mark NSCandidateListTouchBarItemDelegate

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem *)anItem endSelectingCandidateAtIndex:(NSInteger)index
{
    if (index == NSNotFound)
        return;

    CheckedPtr webViewImpl = _webViewImpl.get();
    if (!_webViewImpl)
        return;

    RetainPtr<NSArray> candidates = anItem.candidates;
    if ((NSUInteger)index >= candidates.get().count)
        return;

    RetainPtr candidate = checked_objc_cast<NSTextCheckingResult>(candidates.get()[index]);
    webViewImpl->handleAcceptedCandidate(candidate.get());
}

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem *)anItem changedCandidateListVisibility:(BOOL)isVisible
{
    CheckedPtr webViewImpl = _webViewImpl.get();
    if (!webViewImpl)
        return;

    if (isVisible)
        webViewImpl->requestCandidatesForSelectionIfNeeded();

    webViewImpl->updateTouchBar();
}

#pragma mark NSNotificationCenter observers

- (void)touchBarDidExitCustomization:(NSNotification *)notification
{
    CheckedPtr webViewImpl = _webViewImpl.get();
    if (!webViewImpl)
        return;

    webViewImpl->setIsCustomizingTouchBar(false);
    webViewImpl->updateTouchBar();
}

- (void)touchBarWillEnterCustomization:(NSNotification *)notification
{
    if (!_webViewImpl)
        return;

    _webViewImpl->setIsCustomizingTouchBar(true);
}

- (void)didChangeAutomaticTextCompletion:(NSNotification *)notification
{
    CheckedPtr webViewImpl = _webViewImpl.get();
    if (!webViewImpl)
        return;

    webViewImpl->updateTouchBarAndRefreshTextBarIdentifiers();
}


#pragma mark NSTextTouchBarItemController

- (WKTextListTouchBarViewController *)textListTouchBarViewController
{
    return (WKTextListTouchBarViewController *)self.textListViewController;
}

- (void)setTextIsBold:(BOOL)bold
{
    _textIsBold = bold;
    auto textStyle = retainPtr(self.textStyle);
    if ([textStyle isSelectedForSegment:0] != _textIsBold)
        [textStyle setSelected:_textIsBold forSegment:0];
}

- (void)setTextIsItalic:(BOOL)italic
{
    _textIsItalic = italic;
    auto textStyle = retainPtr(self.textStyle);
    if ([textStyle isSelectedForSegment:1] != _textIsItalic)
        [textStyle setSelected:_textIsItalic forSegment:1];
}

- (void)setTextIsUnderlined:(BOOL)underlined
{
    _textIsUnderlined = underlined;
    auto textStyle = retainPtr(self.textStyle);
    if ([textStyle isSelectedForSegment:2] != _textIsUnderlined)
        [textStyle setSelected:_textIsUnderlined forSegment:2];
}

- (void)_wkChangeTextStyle:(id)sender
{
    if (!_webViewImpl)
        return;

    auto textStyle = retainPtr(self.textStyle);

    if ([textStyle isSelectedForSegment:0] != _textIsBold) {
        _textIsBold = !_textIsBold;
        _webViewImpl->page().executeEditCommand("ToggleBold"_s, emptyString());
    }

    if ([textStyle isSelectedForSegment:1] != _textIsItalic) {
        _textIsItalic = !_textIsItalic;
        _webViewImpl->page().executeEditCommand("ToggleItalic"_s, emptyString());
    }

    if ([textStyle isSelectedForSegment:2] != _textIsUnderlined) {
        _textIsUnderlined = !_textIsUnderlined;
        _webViewImpl->page().executeEditCommand("ToggleUnderline"_s, emptyString());
    }
}

- (void)setCurrentTextAlignment:(NSTextAlignment)alignment
{
    _currentTextAlignment = alignment;
    [retainPtr(self.textAlignments) selectSegmentWithTag:_currentTextAlignment];
}

- (void)_wkChangeTextAlignment:(id)sender
{
    CheckedPtr webViewImpl = _webViewImpl.get();
    if (!webViewImpl)
        return;

    NSTextAlignment alignment = (NSTextAlignment)[retainPtr(self.textAlignments.cell) tagForSegment:self.textAlignments.selectedSegment];
    switch (alignment) {
    case NSTextAlignmentLeft:
        _currentTextAlignment = NSTextAlignmentLeft;
        webViewImpl->page().executeEditCommand("AlignLeft"_s, emptyString());
        break;
    case NSTextAlignmentRight:
        _currentTextAlignment = NSTextAlignmentRight;
        webViewImpl->page().executeEditCommand("AlignRight"_s, emptyString());
        break;
    case NSTextAlignmentCenter:
        _currentTextAlignment = NSTextAlignmentCenter;
        webViewImpl->page().executeEditCommand("AlignCenter"_s, emptyString());
        break;
    case NSTextAlignmentJustified:
        _currentTextAlignment = NSTextAlignmentJustified;
        webViewImpl->page().executeEditCommand("AlignJustified"_s, emptyString());
        break;
    default:
        break;
    }

    webViewImpl->dismissTextTouchBarPopoverItemWithIdentifier(NSTouchBarItemIdentifierTextAlignment);
}

- (NSColor *)textColor
{
    return _textColor.get();
}

- (void)setTextColor:(NSColor *)color
{
    _textColor = color;
    self.colorPickerItem.color = _textColor.get();
}

- (void)_wkChangeColor:(id)sender
{
    if (!_webViewImpl)
        return;

    _textColor = self.colorPickerItem.color;
    _webViewImpl->page().executeEditCommand("ForeColor"_s, WebCore::serializationForHTML(WebCore::colorFromCocoaColor(_textColor.get())));
}

- (NSViewController *)textListViewController
{
    if (!_textListTouchBarViewController)
        _textListTouchBarViewController = adoptNS([[WKTextListTouchBarViewController alloc] initWithWebViewImpl:CheckedPtr { _webViewImpl.get() }.get()]);
    return _textListTouchBarViewController.get();
}

@end

static NSArray<NSString *> *textTouchBarCustomizationAllowedIdentifiers()
{
    return @[ NSTouchBarItemIdentifierCharacterPicker, NSTouchBarItemIdentifierTextColorPicker, NSTouchBarItemIdentifierTextStyle, NSTouchBarItemIdentifierTextAlignment, NSTouchBarItemIdentifierTextList, NSTouchBarItemIdentifierFlexibleSpace ];
}

static NSArray<NSString *> *plainTextTouchBarDefaultItemIdentifiers()
{
    return @[ NSTouchBarItemIdentifierCharacterPicker, NSTouchBarItemIdentifierCandidateList ];
}

static NSArray<NSString *> *richTextTouchBarDefaultItemIdentifiers()
{
    return @[ NSTouchBarItemIdentifierCharacterPicker, NSTouchBarItemIdentifierTextFormat, NSTouchBarItemIdentifierCandidateList ];
}

static NSArray<NSString *> *passwordTextTouchBarDefaultItemIdentifiers()
{
    return @[ NSTouchBarItemIdentifierCandidateList ];
}

#endif // HAVE(TOUCH_BAR)

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

@interface WKImageAnalysisOverlayViewDelegate : NSObject<VKCImageAnalysisOverlayViewDelegate>
- (instancetype)initWithWebViewImpl:(WebKit::WebViewImpl&)impl;
@end

@implementation WKImageAnalysisOverlayViewDelegate {
    WeakPtr<WebKit::WebViewImpl> _impl;
    WeakObjCPtr<VKCImageAnalysisOverlayView> _overlayView;
    WeakObjCPtr<NSResponder> _lastOverlayResponderView;
}

static void* imageOverlayObservationContext = &imageOverlayObservationContext;

- (instancetype)initWithWebViewImpl:(WebKit::WebViewImpl&)impl
{
    if (!(self = [super init]))
        return nil;

    _impl = impl;
    _overlayView = RetainPtr { impl.imageAnalysisOverlayView() }.get();
    [_overlayView.get() addObserver:self forKeyPath:@"hasActiveTextSelection" options:NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew context:imageOverlayObservationContext];
    return self;
}

- (void)dealloc
{
    [_overlayView.get() removeObserver:self forKeyPath:@"hasActiveTextSelection"];
    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context != imageOverlayObservationContext)
        return;

    BOOL oldHasActiveTextSelection = [change[NSKeyValueChangeOldKey] boolValue];
    BOOL newHasActiveTextSelection = [change[NSKeyValueChangeNewKey] boolValue];
    RetainPtr webView = _impl ? CheckedPtr { _impl.get() }->view() : nil;
    RetainPtr<NSResponder> currentResponder = webView.get().window.firstResponder;
    if (oldHasActiveTextSelection && !newHasActiveTextSelection) {
        if (self.firstResponderIsInsideImageOverlay) {
            _lastOverlayResponderView = currentResponder.get();
            [retainPtr(webView.get().window) makeFirstResponder:webView.get()];
        }
    } else if (!oldHasActiveTextSelection && newHasActiveTextSelection) {
        if (RetainPtr lastOverlayResponderView = _lastOverlayResponderView.get(); lastOverlayResponderView && currentResponder.get() != lastOverlayResponderView.get())
            [retainPtr(webView.get().window) makeFirstResponder:lastOverlayResponderView.get()];
    }
}

- (BOOL)firstResponderIsInsideImageOverlay
{
    if (!_impl)
        return NO;

    for (RetainPtr view = dynamic_objc_cast<NSView>(CheckedPtr { _impl.get() }->protectedView().get().window.firstResponder); view; view = view.get().superview) {
        if (view == _overlayView.get())
            return YES;
    }
    return NO;
}

#pragma mark - VKCImageAnalysisOverlayViewDelegate

- (BOOL)imageAnalysisOverlay:(VKCImageAnalysisOverlayView *)overlayView shouldHandleKeyDownEvent:(NSEvent *)event
{
    return ![event.charactersIgnoringModifiers isEqualToString:@"\e"];
}

- (CGRect)contentsRectForImageAnalysisOverlayView:(VKCImageAnalysisOverlayView *)overlayView
{
    if (!_impl)
        return CGRectMake(0, 0, 1, 1);

    auto unitInteractionRect = _impl->imageAnalysisInteractionBounds();
    WebCore::FloatRect unobscuredRect = CheckedPtr { _impl.get() }->protectedView().get().bounds;
    unitInteractionRect.moveBy(-unobscuredRect.location());
    unitInteractionRect.scale(1 / unobscuredRect.size());
    return unitInteractionRect;
}

@end

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

namespace WebKit {

using namespace WebCore;

static NSTrackingAreaOptions trackingAreaOptions()
{
    // Legacy style scrollbars have design details that rely on tracking the mouse all the time.
    NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingCursorUpdate;
    if (_NSRecommendedScrollerStyle() == NSScrollerStyleLegacy)
        options |= NSTrackingActiveAlways;
    else
        options |= NSTrackingActiveInKeyWindow;
    return options;
}

static NSTrackingAreaOptions flagsChangedEventMonitorTrackingAreaOptions()
{
    return NSTrackingInVisibleRect | NSTrackingActiveInActiveApp | NSTrackingMouseEnteredAndExited;
}

#if HAVE(REDESIGNED_TEXT_CURSOR) && PLATFORM(MAC)
static RetainPtr<_WKWebViewTextInputNotifications> subscribeToTextInputNotifications(WebViewImpl*);
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebViewImpl);

WebViewImpl::WebViewImpl(WKWebView *view, WebProcessPool& processPool, Ref<API::PageConfiguration>&& configuration)
    : m_view(view)
    , m_pageClient(makeUniqueRefWithoutRefCountedCheck<PageClientImpl>(view, view))
    , m_page(processPool.createWebPage(m_pageClient, WTFMove(configuration)))
    , m_needsViewFrameInWindowCoordinates(false)
    , m_intrinsicContentSize(CGSizeMake(NSViewNoIntrinsicMetric, NSViewNoIntrinsicMetric))
    , m_layoutStrategy([WKViewLayoutStrategy layoutStrategyWithPage:m_page.get() view:view viewImpl:*this mode:kWKLayoutModeViewSize])
    , m_undoTarget(adoptNS([[WKEditorUndoTarget alloc] init]))
    , m_windowVisibilityObserver(adoptNS([[WKWindowVisibilityObserver alloc] initWithView:view impl:*this]))
    , m_accessibilitySettingsObserver(adoptNS([[WKAccessibilitySettingsObserver alloc] initWithImpl:*this]))
    , m_contentRelativeViewsHysteresis(makeUniqueRef<PAL::HysteresisActivity>([this](auto state) { this->contentRelativeViewsHysteresisTimerFired(state); }, 500_ms))
    , m_mouseTrackingObserver(adoptNS([[WKMouseTrackingObserver alloc] initWithViewImpl:*this]))
    , m_primaryTrackingArea(adoptNS([[NSTrackingArea alloc] initWithRect:view.frame options:trackingAreaOptions() owner:m_mouseTrackingObserver.get() userInfo:nil]))
    , m_flagsChangedEventMonitorTrackingArea(adoptNS([[NSTrackingArea alloc] initWithRect:view.frame options:flagsChangedEventMonitorTrackingAreaOptions() owner:m_mouseTrackingObserver.get() userInfo:nil]))
{
    static_cast<PageClientImpl&>(m_pageClient.get()).setImpl(*this);

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    [NSApp registerServicesMenuSendTypes:PasteboardTypes::forSelectionSingleton() returnTypes:PasteboardTypes::forEditingSingleton()];

#if ENABLE(TILED_CA_DRAWING_AREA)
    auto useRemoteLayerTree = [&]() {
        bool result = false;
#if ENABLE(REMOTE_LAYER_TREE_ON_MAC_BY_DEFAULT)
        result = WTF::numberOfPhysicalProcessorCores() >= 4 || m_page->configuration().lockdownModeEnabled();
#endif
        if (RetainPtr<id> useRemoteLayerTreeBoolean = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKit2UseRemoteLayerTreeDrawingArea"])
            result = [useRemoteLayerTreeBoolean boolValue];

        if (m_page->protectedPreferences()->siteIsolationEnabled())
            result = true;

        return result;
    };

    if (useRemoteLayerTree())
        m_drawingAreaType = DrawingAreaType::RemoteLayerTree;
#endif

    [view addTrackingArea:m_primaryTrackingArea.get()];
    [view addTrackingArea:m_flagsChangedEventMonitorTrackingArea.get()];

    for (NSView *subview in view.subviews) {
        if (RetainPtr layerHostingView = dynamic_objc_cast<WKFlippedView>(subview)) {
            // A layer hosting view may have already been created and added to the view hierarchy
            // in the process of initializing the WKWebView from an NSCoder.
            m_layerHostingView = layerHostingView.get();
            [layerHostingView setFrame:[m_view.get() bounds]];
            break;
        }
    }

    if (!m_layerHostingView) {
        // Create an NSView that will host our layer tree.
        m_layerHostingView = adoptNS([[WKFlippedView alloc] initWithFrame:[m_view.get() bounds]]);
        [view addSubview:m_layerHostingView.get() positioned:NSWindowBelow relativeTo:nil];
    }

    [m_layerHostingView setClipsToBounds:YES];

    // Create a root layer that will back the NSView.
    RetainPtr<CALayer> layer = adoptNS([[CALayer alloc] init]);
    [layer setDelegate:[WebActionDisablingCALayerDelegate shared]];
#ifndef NDEBUG
    [layer setName:@"Hosting root layer"];
#endif

    [m_layerHostingView setLayer:layer.get()];
    [m_layerHostingView setWantsLayer:YES];

    m_page->setIntrinsicDeviceScaleFactor(intrinsicDeviceScaleFactor());

    if (Class gestureClass = NSClassFromString(@"NSImmediateActionGestureRecognizer")) {
        m_immediateActionGestureRecognizer = adoptNS([(NSImmediateActionGestureRecognizer *)[gestureClass alloc] init]);
        m_immediateActionController = adoptNS([[WKImmediateActionController alloc] initWithPage:m_page.get() view:view viewImpl:*this recognizer:m_immediateActionGestureRecognizer.get()]);
        [m_immediateActionGestureRecognizer setDelegate:m_immediateActionController.get()];
        [m_immediateActionGestureRecognizer setDelaysPrimaryMouseButtonEvents:NO];
    }

    m_page->setAddsVisitedLinks(processPool.historyClient().addsVisitedLinks());

    auto& pageConfiguration = m_page->configuration();
    m_page->initializeWebPage(pageConfiguration.openedSite(), pageConfiguration.initialSandboxFlags(), pageConfiguration.initialReferrerPolicy());

    registerDraggedTypes();

    view.wantsLayer = YES;

    // Explicitly set the layer contents placement so AppKit will make sure that our layer has masksToBounds set to YES.
    view.layerContentsPlacement = NSViewLayerContentsPlacementTopLeft;

#if ENABLE(FULLSCREEN_API)
    m_page->setFullscreenClient(makeUnique<WebKit::FullscreenClient>(view));
#endif

    m_lastScrollViewFrame = scrollViewFrame();

#if HAVE(REDESIGNED_TEXT_CURSOR) && PLATFORM(MAC)
    m_textInputNotifications = subscribeToTextInputNotifications(this);
#endif

    m_panGestureController = adoptNS([[WKPanGestureController alloc] initWithPage:m_page viewImpl:*this]);

    WebProcessPool::statistics().wkViewCount++;
}

WebViewImpl::~WebViewImpl()
{
    if (m_remoteObjectRegistry) {
        m_page->configuration().protectedProcessPool()->removeMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), m_page->identifier());
        [m_remoteObjectRegistry _invalidate];
        m_remoteObjectRegistry = nil;
    }

    ASSERT(!m_inSecureInputState);
    ASSERT(!m_thumbnailView);

    [m_layoutStrategy invalidate];

    [m_immediateActionController willDestroyView:m_view.get().get()];

#if HAVE(TOUCH_BAR)
    [m_textTouchBarItemController didDestroyView];
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [m_mediaTouchBarProvider setPlaybackControlsController:nil];
    [m_mediaPlaybackControlsView setPlaybackControlsController:nil];
#endif
#endif

    [m_windowVisibilityObserver stopObserving];
    m_targetWindowForMovePreparation = nil;

    m_page->close();

    WebProcessPool::statistics().wkViewCount--;

}

NSWindow *WebViewImpl::window()
{
    return [m_view.get() window];
}

RetainPtr<NSWindow> WebViewImpl::protectedWindow()
{
    return window();
}

void WebViewImpl::handleProcessSwapOrExit()
{
    dismissContentRelativeChildWindowsWithAnimation(true);
    m_page->closeSharedPreviewPanelIfNecessary();

    notifyInputContextAboutDiscardedComposition();

    updateRemoteAccessibilityRegistration(false);

    hideDOMPasteMenuWithResult(WebCore::DOMPasteAccessResponse::DeniedForGesture);

    [m_view.get() _updateFixedContainerEdges:FixedContainerEdges { }];
}

void WebViewImpl::processWillSwap()
{
    handleProcessSwapOrExit();
    if (RefPtr gestureController = m_gestureController)
        gestureController->disconnectFromProcess();
}

void WebViewImpl::processDidExit()
{
    handleProcessSwapOrExit();
    m_gestureController = nullptr;
}

void WebViewImpl::pageClosed()
{
    updateRemoteAccessibilityRegistration(false);
}

void WebViewImpl::didRelaunchProcess()
{
    if (RefPtr gestureController = m_gestureController)
        gestureController->connectToProcess();

    accessibilityRegisterUIProcessTokens();
    windowDidChangeScreen(); // Make sure DisplayID is set.
}

void WebViewImpl::setDrawsBackground(bool drawsBackground)
{
    std::optional<WebCore::Color> backgroundColor;
    if (!drawsBackground)
        backgroundColor = WebCore::Color(WebCore::Color::transparentBlack);
    m_page->setBackgroundColor(backgroundColor);

    // Make sure updateLayer gets called on the web view.
    [m_view.get() setNeedsDisplay:YES];
}

bool WebViewImpl::drawsBackground() const
{
    auto& backgroundColor = m_page->backgroundColor();
    return !backgroundColor || backgroundColor.value().isVisible();
}

void WebViewImpl::setBackgroundColor(NSColor *backgroundColor)
{
    m_backgroundColor = backgroundColor;

    // Make sure updateLayer gets called on the web view.
    [m_view.get() setNeedsDisplay:YES];
}

NSColor *WebViewImpl::backgroundColor() const
{
    if (!m_backgroundColor)
#if ENABLE(DARK_MODE_CSS)
        return [NSColor controlBackgroundColor];
#else
        return [NSColor whiteColor];
#endif
    return m_backgroundColor.get();
}

bool WebViewImpl::isOpaque() const
{
    return drawsBackground();
}

void WebViewImpl::setShouldSuppressFirstResponderChanges(bool shouldSuppress)
{
    m_pageClient->setShouldSuppressFirstResponderChanges(shouldSuppress);
}

bool WebViewImpl::acceptsFirstResponder()
{
    return true;
}

bool WebViewImpl::becomeFirstResponder()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    // If we just became first responder again, there is no need to do anything,
    // since resignFirstResponder has correctly detected this situation.
    if (m_willBecomeFirstResponderAgain) {
        m_willBecomeFirstResponderAgain = false;
        return true;
    }

    NSSelectionDirection direction = [[m_view.get() window] keyViewSelectionDirection];

    m_inBecomeFirstResponder = true;

    updateSecureInputState();
    m_page->activityStateDidChange(WebCore::ActivityState::IsFocused);
    // Restore the selection in the editable region if resigning first responder cleared selection.
    m_page->restoreSelectionInFocusedEditableElement();

    m_inBecomeFirstResponder = false;

#if HAVE(TOUCH_BAR)
    updateTouchBar();
#endif

    if (direction != NSDirectSelection) {
        RetainPtr event = [NSApp currentEvent];
        RetainPtr<NSEvent> keyboardEvent;
        if ([event type] == NSEventTypeKeyDown || [event type] == NSEventTypeKeyUp)
            keyboardEvent = event;
        m_page->setInitialFocus(direction == NSSelectingNext, !!keyboardEvent, NativeWebKeyboardEvent(keyboardEvent.get(), false, false, { }), [] { });
    }
    return true;
}

bool WebViewImpl::resignFirstResponder()
{
    // Predict the case where we are losing first responder status only to
    // gain it back again. We want resignFirstResponder to do nothing in that case.
    // FIXME: This is a safer cpp false-positive.
    SUPPRESS_RETAINPTR_CTOR_ADOPT RetainPtr<id> nextResponder = [protectedWindow() _newFirstResponderAfterResigning];

    // FIXME: This will probably need to change once WKWebView doesn't contain a WKView.
    if ([nextResponder isKindOfClass:[WKWebView class]] && [m_view.get() superview] == nextResponder.get()) {
        m_willBecomeFirstResponderAgain = true;
        return true;
    }

    m_willBecomeFirstResponderAgain = false;
    m_inResignFirstResponder = true;

    m_page->confirmCompositionAsync();

    notifyInputContextAboutDiscardedComposition();

    resetSecureInputState();

    auto shouldClearSelection = [&] {
        if (m_page->maintainsInactiveSelection())
            return false;

        RetainPtr<NSWindow> nextResponderWindow = dynamic_objc_cast<NSView>(nextResponder.get()).window;
        return !dynamic_objc_cast<NSPanel>(nextResponderWindow.get());
    }();

    if (shouldClearSelection)
        m_page->clearSelection();

    m_page->activityStateDidChange(WebCore::ActivityState::IsFocused);

    m_inResignFirstResponder = false;

    return true;
}

void WebViewImpl::takeFocus(WebCore::FocusDirection direction)
{
    RetainPtr webView = m_view.get();
    RetainPtr<NSWindow> window = webView.get().window;
    if (direction == WebCore::FocusDirection::Forward) {
        // Since we're trying to move focus out of m_webView, and because
        // m_webView may contain subviews within it, we ask it for the next key
        // view of the last view in its key view loop. This makes m_webView
        // behave as if it had no subviews, which is the behavior we want.
        [window selectKeyViewFollowingView:retainPtr([webView _findLastViewInKeyViewLoop]).get()];
    } else
        [window selectKeyViewPrecedingView:webView.get()];
}

void WebViewImpl::showWarningView(const BrowsingWarning& warning, CompletionHandler<void(Variant<ContinueUnsafeLoad, URL>&&)>&& completionHandler)
{
    if (!m_view)
        return completionHandler(ContinueUnsafeLoad::Yes);

    WebCore::DiagnosticLoggingClient::ValueDictionary showedWarningDictionary;
    showedWarningDictionary.set("source"_s, "service"_s);

    m_page->logDiagnosticMessageWithValueDictionary("SafeBrowsing.ShowedWarning"_s, "Safari"_s, showedWarningDictionary, WebCore::ShouldSample::No);

    m_warningView = adoptNS([[_WKWarningView alloc] initWithFrame:[m_view.get() bounds] browsingWarning:warning completionHandler:[weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (auto&& result) mutable {
        completionHandler(WTFMove(result));
        if (!weakThis)
            return;
        bool navigatesFrame = WTF::switchOn(result,
            [] (ContinueUnsafeLoad continueUnsafeLoad) { return continueUnsafeLoad == ContinueUnsafeLoad::Yes; },
            [] (const URL&) { return true; }
        );
        bool forMainFrameNavigation = [weakThis->m_warningView forMainFrameNavigation];

        WebCore::DiagnosticLoggingClient::ValueDictionary dictionary;
        dictionary.set("source"_s, "service"_s);
        if (navigatesFrame && forMainFrameNavigation) {
            // The safe browsing warning will be hidden once the next page is shown.
            bool continuingUnsafeLoad = WTF::switchOn(result,
                [] (ContinueUnsafeLoad continueUnsafeLoad) { return continueUnsafeLoad == ContinueUnsafeLoad::Yes; },
                [] (const URL&) { return false; }
            );

            if (continuingUnsafeLoad)
                dictionary.set("action"_s, "visit website"_s);
            else
                dictionary.set("action"_s, "redirect to url"_s);

            weakThis->m_page->logDiagnosticMessageWithValueDictionary("SafeBrowsing.PerformedAction"_s, "Safari"_s, dictionary, WebCore::ShouldSample::No);
            return;
        }

        dictionary.set("action"_s, "go back"_s);
        weakThis->m_page->logDiagnosticMessageWithValueDictionary("SafeBrowsing.PerformedAction"_s, "Safari"_s, dictionary, WebCore::ShouldSample::No);

        if (!navigatesFrame && weakThis->m_warningView && !forMainFrameNavigation) {
            weakThis->m_page->goBack();
            return;
        }
        [std::exchange(weakThis->m_warningView, nullptr) removeFromSuperview];
    }]);
    [m_view.get() addSubview:m_warningView.get()];
}

void WebViewImpl::clearWarningView()
{
    [std::exchange(m_warningView, nullptr) removeFromSuperview];
}

void WebViewImpl::clearWarningViewIfForMainFrameNavigation()
{
    if ([m_warningView forMainFrameNavigation])
        clearWarningView();
}

bool WebViewImpl::isFocused() const
{
    if (m_inBecomeFirstResponder)
        return true;
    if (m_inResignFirstResponder)
        return false;
    return [m_view.get() window].firstResponder == m_view.getAutoreleased();
}

void WebViewImpl::viewWillStartLiveResize()
{
    m_page->viewWillStartLiveResize();

    [m_layoutStrategy willStartLiveResize];
}

void WebViewImpl::viewDidEndLiveResize()
{
    m_page->viewWillEndLiveResize();

    [m_layoutStrategy didEndLiveResize];
}

void WebViewImpl::createPDFHUD(PDFPluginIdentifier identifier, WebCore::FrameIdentifier frameID, const WebCore::IntRect& rect)
{
    removePDFHUD(identifier);
    RetainPtr hud = adoptNS([[WKPDFHUDView alloc] initWithFrame:rect pluginIdentifier:identifier frameIdentifier:frameID page:m_page.get()]);
    [m_view.get() addSubview:hud.get()];
    _pdfHUDViews.add(identifier, WTFMove(hud));
}

void WebViewImpl::updatePDFHUDLocation(PDFPluginIdentifier identifier, const WebCore::IntRect& rect)
{
    if (RetainPtr hud = _pdfHUDViews.get(identifier))
        [hud setFrame:rect];
}

void WebViewImpl::removePDFHUD(PDFPluginIdentifier identifier)
{
    if (RetainPtr hud = _pdfHUDViews.take(identifier))
        [hud removeFromSuperview];
}

void WebViewImpl::removeAllPDFHUDs()
{
    for (auto& hud : _pdfHUDViews.values())
        [hud removeFromSuperview];
    _pdfHUDViews.clear();
}

RetainPtr<NSSet> WebViewImpl::pdfHUDs()
{
    RetainPtr<NSMutableSet<NSView *>> set = adoptNS([[NSMutableSet alloc] initWithCapacity:_pdfHUDViews.size()]);
    for (auto& hud : _pdfHUDViews.values())
        [set addObject:hud.get()];
    return set;
}

void WebViewImpl::renewGState()
{
    suppressContentRelativeChildViews(ContentRelativeChildViewsSuppressionType::TemporarilyRemove);

    // Update the view frame.
    if ([m_view.get() window])
        updateWindowAndViewFrames();

    updateContentInsetsIfAutomatic();
}

void WebViewImpl::setFrameSize(CGSize)
{
    [m_layoutStrategy didChangeFrameSize];
    [m_warningView setFrame:[m_view.get() bounds]];
}

void WebViewImpl::disableFrameSizeUpdates()
{
    [m_layoutStrategy disableFrameSizeUpdates];
}

void WebViewImpl::enableFrameSizeUpdates()
{
    [m_layoutStrategy enableFrameSizeUpdates];
}

bool WebViewImpl::frameSizeUpdatesDisabled() const
{
    return [m_layoutStrategy frameSizeUpdatesDisabled];
}

void WebViewImpl::setFrameAndScrollBy(CGRect frame, CGSize scrollDelta)
{
    if (!CGSizeEqualToSize(scrollDelta, CGSizeZero))
        m_scrollOffsetAdjustment = scrollDelta;

    [m_view.get() frame] = NSRectFromCGRect(frame);
}

void WebViewImpl::updateWindowAndViewFrames()
{
    if (clipsToVisibleRect())
        updateViewExposedRect();

    NSRect scrollViewFrame = this->scrollViewFrame();
    if (!NSEqualRects(m_lastScrollViewFrame, scrollViewFrame)) {
        m_lastScrollViewFrame = scrollViewFrame;
        [m_view.get() didChangeValueForKey:@"scrollViewFrame"];
    }

    updateTitlebarAdjacencyState();

    if (m_didScheduleWindowAndViewFrameUpdate)
        return;

    m_didScheduleWindowAndViewFrameUpdate = true;

    RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;

        weakThis->m_didScheduleWindowAndViewFrameUpdate = false;

        NSRect viewFrameInWindowCoordinates = NSZeroRect;
        NSPoint accessibilityPosition = NSZeroPoint;

        if (weakThis->m_needsViewFrameInWindowCoordinates) {
            RetainPtr view = weakThis->m_view.get();
            viewFrameInWindowCoordinates = [view convertRect:[view frame] toView:nil];
        }

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (WebCore::AXObjectCache::accessibilityEnabled())
            accessibilityPosition = [[weakThis->m_view.get() accessibilityAttributeValue:NSAccessibilityPositionAttribute] pointValue];
ALLOW_DEPRECATED_DECLARATIONS_END

        weakThis->m_page->windowAndViewFramesChanged(viewFrameInWindowCoordinates, accessibilityPosition);
    });
}

void WebViewImpl::setFixedLayoutSize(CGSize fixedLayoutSize)
{
    m_lastRequestedFixedLayoutSize = fixedLayoutSize;

    if (supportsArbitraryLayoutModes())
        m_page->setFixedLayoutSize(WebCore::expandedIntSize(WebCore::FloatSize(fixedLayoutSize)));
}

CGSize WebViewImpl::fixedLayoutSize() const
{
    return m_page->fixedLayoutSize();
}

Ref<WebKit::DrawingAreaProxy> WebViewImpl::createDrawingAreaProxy(WebProcessProxy& webProcessProxy)
{
#if ENABLE(TILED_CA_DRAWING_AREA)
    switch (m_drawingAreaType) {
    case DrawingAreaType::TiledCoreAnimation:
        return TiledCoreAnimationDrawingAreaProxy::create(m_page, webProcessProxy);
    case DrawingAreaType::RemoteLayerTree:
        return RemoteLayerTreeDrawingAreaProxyMac::create(m_page, webProcessProxy);
    }
    ASSERT_NOT_REACHED();
#endif
    return RemoteLayerTreeDrawingAreaProxyMac::create(m_page, webProcessProxy);
}

bool WebViewImpl::isUsingUISideCompositing() const
{
#if ENABLE(TILED_CA_DRAWING_AREA)
    return m_drawingAreaType == DrawingAreaType::RemoteLayerTree;
#else
    return true;
#endif
}

void WebViewImpl::setDrawingAreaSize(CGSize size)
{
    RefPtr drawingArea = m_page->drawingArea();
    if (!drawingArea)
        return;

    drawingArea->setSize(WebCore::IntSize(size), WebCore::IntSize(m_scrollOffsetAdjustment));
    m_scrollOffsetAdjustment = CGSizeZero;
}

void WebViewImpl::updateLayer()
{
    [m_view.get() layer].backgroundColor = RetainPtr { drawsBackground() ? RetainPtr { backgroundColor() }.get().CGColor : CGColorGetConstantColor(kCGColorClear) }.get();
}

void WebViewImpl::drawRect(CGRect rect)
{
    LOG(Printing, "drawRect: x:%g, y:%g, width:%g, height:%g", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
    m_page->endPrinting();
}

bool WebViewImpl::canChangeFrameLayout(WebFrameProxy& frame)
{
    // PDF documents are already paginated, so we can't change them to add headers and footers.
    return !frame.isDisplayingPDFDocument();
}

RetainPtr<NSPrintOperation> WebViewImpl::printOperationWithPrintInfo(NSPrintInfo *printInfo, WebFrameProxy& frame)
{
    LOG(Printing, "Creating an NSPrintOperation for frame '%s'", frame.url().string().utf8().data());

    // FIXME: If the frame cannot be printed (e.g. if it contains an encrypted PDF that disallows
    // printing), this function should return nil.
    RetainPtr<WKPrintingView> printingView = adoptNS([[WKPrintingView alloc] initWithFrameProxy:frame view:m_view.get().get()]);
    // NSPrintOperation takes ownership of the view.
    RetainPtr<NSPrintOperation> printOperation = [NSPrintOperation printOperationWithView:printingView.get() printInfo:printInfo];
    [printOperation setCanSpawnSeparateThread:YES];
    [printOperation setJobTitle:frame.title().createNSString().get()];
    printingView->_printOperation = printOperation.get();
    return printOperation;
}

void WebViewImpl::setAutomaticallyAdjustsContentInsets(bool automaticallyAdjustsContentInsets)
{
    m_page->setAutomaticallyAdjustsContentInsets(automaticallyAdjustsContentInsets);
}

bool WebViewImpl::automaticallyAdjustsContentInsets() const
{
    return m_page->automaticallyAdjustsContentInsets();
}

void WebViewImpl::updateContentInsetsIfAutomatic()
{
    m_page->updateContentInsetsIfAutomatic();
}

FloatBoxExtent WebViewImpl::obscuredContentInsets() const
{
    return m_page->pendingOrActualObscuredContentInsets();
}

void WebViewImpl::setObscuredContentInsets(const FloatBoxExtent& insets)
{
    m_page->setObscuredContentInsetsAsync(insets);
}

void WebViewImpl::flushPendingObscuredContentInsetChanges()
{
    m_page->dispatchSetObscuredContentInsets();
}

void WebViewImpl::prepareContentInRect(CGRect rect)
{
    m_contentPreparationRect = rect;
    m_useContentPreparationRectForVisibleRect = true;

    updateViewExposedRect();
}

void WebViewImpl::updateViewExposedRect()
{
    CGRect exposedRect = NSRectToCGRect([m_view.get() visibleRect]);

    if (m_useContentPreparationRectForVisibleRect)
        exposedRect = CGRectUnion(m_contentPreparationRect, exposedRect);

    m_page->setViewExposedRect(m_clipsToVisibleRect ? std::optional<WebCore::FloatRect>(exposedRect) : std::nullopt);
}

void WebViewImpl::setClipsToVisibleRect(bool clipsToVisibleRect)
{
    m_clipsToVisibleRect = clipsToVisibleRect;
    updateViewExposedRect();
}

void WebViewImpl::setMinimumSizeForAutoLayout(CGSize minimumSizeForAutoLayout)
{
    bool expandsToFit = minimumSizeForAutoLayout.width > 0;

    m_page->setMinimumSizeForAutoLayout(WebCore::IntSize(minimumSizeForAutoLayout));
    m_page->setMainFrameIsScrollable(!expandsToFit);

    setClipsToVisibleRect(expandsToFit);
}

CGSize WebViewImpl::minimumSizeForAutoLayout() const
{
    return m_page->minimumSizeForAutoLayout();
}

void WebViewImpl::setSizeToContentAutoSizeMaximumSize(CGSize sizeToContentAutoSizeMaximumSize)
{
    bool expandsToFit = sizeToContentAutoSizeMaximumSize.width > 0 && sizeToContentAutoSizeMaximumSize.height > 0;

    m_page->setSizeToContentAutoSizeMaximumSize(WebCore::IntSize(sizeToContentAutoSizeMaximumSize));
    m_page->setMainFrameIsScrollable(!expandsToFit);

    setClipsToVisibleRect(expandsToFit);
}

CGSize WebViewImpl::sizeToContentAutoSizeMaximumSize() const
{
    return m_page->sizeToContentAutoSizeMaximumSize();
}

void WebViewImpl::setShouldExpandToViewHeightForAutoLayout(bool shouldExpandToViewHeightForAutoLayout)
{
    m_page->setAutoSizingShouldExpandToViewHeight(shouldExpandToViewHeightForAutoLayout);
}

bool WebViewImpl::shouldExpandToViewHeightForAutoLayout() const
{
    return m_page->autoSizingShouldExpandToViewHeight();
}

void WebViewImpl::setIntrinsicContentSize(CGSize intrinsicContentSize)
{
    // If the intrinsic content size is less than the minimum layout width, the content flowed to fit,
    // so we can report that that dimension is flexible. If not, we need to report our intrinsic width
    // so that autolayout will know to provide space for us.

    // FIXME: what to do here?
    CGSize intrinsicContentSizeAcknowledgingFlexibleWidth = intrinsicContentSize;
    if (intrinsicContentSize.width < m_page->minimumSizeForAutoLayout().width())
        intrinsicContentSizeAcknowledgingFlexibleWidth.width = NSViewNoIntrinsicMetric;

    m_intrinsicContentSize = intrinsicContentSizeAcknowledgingFlexibleWidth;
    [m_view.get() invalidateIntrinsicContentSize];
}

CGSize WebViewImpl::intrinsicContentSize() const
{
    return m_intrinsicContentSize;
}

void WebViewImpl::setViewScale(CGFloat viewScale)
{
    m_lastRequestedViewScale = viewScale;

    if (!supportsArbitraryLayoutModes() && viewScale != 1)
        return;

    m_page->scaleView(viewScale);
    [m_layoutStrategy didChangeViewScale];
}

CGFloat WebViewImpl::viewScale() const
{
    return m_page->viewScaleFactor();
}

WKLayoutMode WebViewImpl::layoutMode() const
{
    return [m_layoutStrategy layoutMode];
}

void WebViewImpl::setLayoutMode(WKLayoutMode layoutMode)
{
    m_lastRequestedLayoutMode = layoutMode;

    if (!supportsArbitraryLayoutModes() && layoutMode != kWKLayoutModeViewSize)
        return;

    if (layoutMode == [m_layoutStrategy layoutMode])
        return;

    [m_layoutStrategy willChangeLayoutStrategy];
    m_layoutStrategy = [WKViewLayoutStrategy layoutStrategyWithPage:m_page.get() view:m_view.get().get() viewImpl:*this mode:layoutMode];
}

bool WebViewImpl::supportsArbitraryLayoutModes() const
{
    if ([m_fullScreenWindowController isFullScreen])
        return false;

    RefPtr frame = m_page->mainFrame();
    if (!frame)
        return true;

    // If we have a plugin document in the main frame, avoid using custom WKLayoutModes
    // and fall back to the defaults, because there's a good chance that it won't work (e.g. with PDFPlugin).
    if (frame->containsPluginDocument())
        return false;

    return true;
}

void WebViewImpl::updateSupportsArbitraryLayoutModes()
{
    if (!supportsArbitraryLayoutModes()) {
        WKLayoutMode oldRequestedLayoutMode = m_lastRequestedLayoutMode;
        CGFloat oldRequestedViewScale = m_lastRequestedViewScale;
        CGSize oldRequestedFixedLayoutSize = m_lastRequestedFixedLayoutSize;
        setViewScale(1);
        setLayoutMode(kWKLayoutModeViewSize);
        setFixedLayoutSize(CGSizeZero);

        // The 'last requested' parameters will have been overwritten by setting them above, but we don't
        // want this to count as a request (only changes from the client count), so reset them.
        m_lastRequestedLayoutMode = oldRequestedLayoutMode;
        m_lastRequestedViewScale = oldRequestedViewScale;
        m_lastRequestedFixedLayoutSize = oldRequestedFixedLayoutSize;
    } else if (m_lastRequestedLayoutMode != [m_layoutStrategy layoutMode]) {
        setViewScale(m_lastRequestedViewScale);
        setLayoutMode(m_lastRequestedLayoutMode);
        setFixedLayoutSize(m_lastRequestedFixedLayoutSize);
    }
}

float WebViewImpl::intrinsicDeviceScaleFactor() const
{
    if (m_targetWindowForMovePreparation)
        return [m_targetWindowForMovePreparation backingScaleFactor];
    if (RetainPtr window = [m_view.get() window])
        return window.get().backingScaleFactor;
    return [NSScreen mainScreen].backingScaleFactor;
}

void WebViewImpl::windowWillEnterOrExitFullScreen()
{
    m_windowIsEnteringOrExitingFullScreen = true;
}

void WebViewImpl::windowDidEnterOrExitFullScreen()
{
    m_windowIsEnteringOrExitingFullScreen = false;

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    updateScrollPocket();
#endif
}

void WebViewImpl::windowDidOrderOffScreen()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) windowDidOrderOffScreen", this, m_page->identifier().toUInt64());
    m_page->activityStateDidChange({ WebCore::ActivityState::IsVisible, WebCore::ActivityState::WindowIsActive });
}

void WebViewImpl::windowDidOrderOnScreen()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) windowDidOrderOnScreen", this, m_page->identifier().toUInt64());
    m_page->activityStateDidChange({ WebCore::ActivityState::IsVisible, WebCore::ActivityState::WindowIsActive });
}

void WebViewImpl::windowDidBecomeKey(NSWindow *keyWindow)
{
    if (keyWindow == window() || keyWindow == [protectedWindow() attachedSheet]) {
#if ENABLE(GAMEPAD)
        UIGamepadProvider::singleton().viewBecameActive(m_page.get());
#endif
        updateSecureInputState();
        m_page->activityStateDidChange(WebCore::ActivityState::WindowIsActive);
    }
}

void WebViewImpl::windowDidResignKey(NSWindow *formerKeyWindow)
{
    if (formerKeyWindow == window() || formerKeyWindow == [protectedWindow() attachedSheet]) {
#if ENABLE(GAMEPAD)
        UIGamepadProvider::singleton().viewBecameInactive(m_page.get());
#endif
        updateSecureInputState();
        m_page->activityStateDidChange(WebCore::ActivityState::WindowIsActive);
    }
}

void WebViewImpl::windowDidMiniaturize()
{
    m_page->activityStateDidChange(WebCore::ActivityState::IsVisible);
}

void WebViewImpl::windowDidDeminiaturize()
{
    m_page->activityStateDidChange(WebCore::ActivityState::IsVisible);
}

void WebViewImpl::windowDidMove()
{
    updateWindowAndViewFrames();
}

void WebViewImpl::windowDidResize()
{
    updateWindowAndViewFrames();
}

void WebViewImpl::windowWillBeginSheet()
{
#if ENABLE(POINTER_LOCK)
    m_page->resetPointerLockState();
#endif
}

void WebViewImpl::windowDidChangeBackingProperties(CGFloat oldBackingScaleFactor)
{
    CGFloat newBackingScaleFactor = intrinsicDeviceScaleFactor();
    if (oldBackingScaleFactor == newBackingScaleFactor)
        return;

    m_page->setIntrinsicDeviceScaleFactor(newBackingScaleFactor);
    for (auto& hud : _pdfHUDViews.values())
        [hud setDeviceScaleFactor:newBackingScaleFactor];
}

void WebViewImpl::windowDidChangeScreen()
{
    RetainPtr window = m_targetWindowForMovePreparation ? m_targetWindowForMovePreparation.get() : WebViewImpl::window();
    auto displayID = WebCore::displayID(retainPtr(window.get().screen).get());
    m_page->windowScreenDidChange(displayID);
}

void WebViewImpl::windowDidChangeOcclusionState()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) windowDidChangeOcclusionState", this, m_page->identifier().toUInt64());
    m_page->activityStateDidChange(WebCore::ActivityState::IsVisible);
}

void WebViewImpl::windowWillClose()
{
    resetSecureInputState();
}

void WebViewImpl::screenDidChangeColorSpace()
{
    m_page->configuration().protectedProcessPool()->screenPropertiesChanged();
}

void WebViewImpl::applicationShouldSuppressHDR(bool suppress)
{
    m_page->setShouldSuppressHDR(suppress);
}

bool WebViewImpl::mightBeginDragWhileInactive()
{
    if ([m_view.get() window].isKeyWindow)
        return false;

    if (m_page->editorState().selectionIsNone || !m_page->editorState().selectionIsRange)
        return false;

    return true;
}

bool WebViewImpl::mightBeginScrollWhileInactive()
{
    // Legacy style scrollbars have design details that rely on tracking the mouse all the time.
    if (_NSRecommendedScrollerStyle() == NSScrollerStyleLegacy)
        return true;

    return false;
}

void WebViewImpl::accessibilitySettingsDidChange()
{
    m_page->accessibilitySettingsDidChange();
}

bool WebViewImpl::acceptsFirstMouse(NSEvent *event)
{
    if (!mightBeginDragWhileInactive() && !mightBeginScrollWhileInactive())
        return false;

    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    retainPtr(event).autorelease();

    if (![m_view.get() hitTest:event.locationInWindow])
        return false;

    auto previousEvent = setLastMouseDownEvent(event);
    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(event, m_lastPressureEvent.get(), m_view.get().get());
    bool result = m_page->acceptsFirstMouse(event.eventNumber, mouseEvent);
    setLastMouseDownEvent(previousEvent.get());
    return result;
}

bool WebViewImpl::shouldDelayWindowOrderingForEvent(NSEvent *event)
{
    if (!mightBeginDragWhileInactive())
        return false;

    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    retainPtr(event).autorelease();

    if (![m_view.get() hitTest:event.locationInWindow])
        return false;

    if (!page().protectedLegacyMainFrameProcess()->isResponsive())
        return false;

    if (page().editorState().hasPostLayoutData()) {
        auto locationInView = [m_view.get() convertPoint:event.locationInWindow fromView:nil];
        if (!page().selectionBoundingRectInRootViewCoordinates().contains(roundedIntPoint(locationInView)))
            return false;
    }

    auto previousEvent = setLastMouseDownEvent(event);
    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(event, m_lastPressureEvent.get(), m_view.get().get());
    bool result = m_page->shouldDelayWindowOrderingForEvent(mouseEvent);
    setLastMouseDownEvent(previousEvent.get());
    return result;
}

bool WebViewImpl::windowResizeMouseLocationIsInVisibleScrollerThumb(CGPoint point)
{
    NSPoint localPoint = [m_view.get() convertPoint:NSPointFromCGPoint(point) fromView:nil];
    NSRect visibleThumbRect = NSRect(m_page->visibleScrollerThumbRect());
    return NSMouseInRect(localPoint, visibleThumbRect, [m_view.get() isFlipped]);
}

void WebViewImpl::viewWillMoveToWindowImpl(NSWindow *window)
{
    RetainPtr currentWindow = WebViewImpl::window();
    if (window == currentWindow.get())
        return;

    clearAllEditCommands();

    if (!m_isPreparingToUnparentView)
        [m_windowVisibilityObserver startObserving:window];

    if (m_isRegisteredScrollViewSeparatorTrackingAdapter) {
        [currentWindow unregisterScrollViewSeparatorTrackingAdapter:(NSObject<NSScrollViewSeparatorTrackingAdapter> *)m_view.get().get()];
        m_isRegisteredScrollViewSeparatorTrackingAdapter = false;
    }

    m_windowIsEnteringOrExitingFullScreen = false;
}

void WebViewImpl::viewWillMoveToWindow(NSWindow *window)
{
    // If we're in the middle of preparing to move to a window, we should only be moved to that window.
    ASSERT_IMPLIES(m_targetWindowForMovePreparation, m_targetWindowForMovePreparation == window);
    viewWillMoveToWindowImpl(window);
    m_targetWindowForMovePreparation = nil;
    m_isPreparingToUnparentView = false;
}

void WebViewImpl::viewDidMoveToWindow()
{
    RetainPtr window = m_targetWindowForMovePreparation ? m_targetWindowForMovePreparation.get() : WebViewImpl::window();

    LOG(ActivityState, "WebViewImpl %p viewDidMoveToWindow %p", this, window.get());

    if (window) {
        windowDidChangeScreen();

        OptionSet<WebCore::ActivityState> activityStateChanges { WebCore::ActivityState::WindowIsActive, WebCore::ActivityState::IsVisible };
        if (m_shouldDeferViewInWindowChanges)
            m_viewInWindowChangeWasDeferred = true;
        else
            activityStateChanges.add(WebCore::ActivityState::IsInWindow);
        m_page->activityStateDidChange(activityStateChanges);

        updateWindowAndViewFrames();

        accessibilityRegisterUIProcessTokens();

        if (m_immediateActionGestureRecognizer && ![retainPtr([m_view.get() gestureRecognizers]) containsObject:m_immediateActionGestureRecognizer.get()] && !m_ignoresNonWheelEvents && m_allowsLinkPreview)
            [m_view.get() addGestureRecognizer:m_immediateActionGestureRecognizer.get()];
    } else {
        OptionSet<WebCore::ActivityState> activityStateChanges { WebCore::ActivityState::WindowIsActive, WebCore::ActivityState::IsVisible };
        if (m_shouldDeferViewInWindowChanges)
            m_viewInWindowChangeWasDeferred = true;
        else
            activityStateChanges.add(WebCore::ActivityState::IsInWindow);
        m_page->activityStateDidChange(activityStateChanges);

        dismissContentRelativeChildWindowsWithAnimation(false);
        m_page->closeSharedPreviewPanelIfNecessary();

        if (m_immediateActionGestureRecognizer) {
            // Work around <rdar://problem/22646404> by explicitly cancelling the animation.
            cancelImmediateActionAnimation();
            [m_view.get() removeGestureRecognizer:m_immediateActionGestureRecognizer.get()];
        }

        removeFlagsChangedEventMonitor();
    }

    m_page->setIntrinsicDeviceScaleFactor(intrinsicDeviceScaleFactor());
    m_page->webViewDidMoveToWindow();
}

void WebViewImpl::viewDidChangeBackingProperties()
{
    RetainPtr<NSColorSpace> colorSpace = protectedWindow().get().colorSpace;
    if ([colorSpace isEqualTo:m_colorSpace.get()])
        return;

    m_colorSpace = nullptr;
    if (RefPtr drawingArea = m_page->drawingArea())
        drawingArea->colorSpaceDidChange();
}

void WebViewImpl::viewDidHide()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) viewDidHide", this, m_page->identifier().toUInt64());
    m_page->activityStateDidChange(WebCore::ActivityState::IsVisible);
    updateTitlebarAdjacencyState();
}

void WebViewImpl::viewDidUnhide()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) viewDidUnhide", this, m_page->identifier().toUInt64());
    m_page->activityStateDidChange(WebCore::ActivityState::IsVisible);
    updateTitlebarAdjacencyState();
}

void WebViewImpl::activeSpaceDidChange()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) activeSpaceDidChange", this, m_page->identifier().toUInt64());
    m_page->activityStateDidChange(WebCore::ActivityState::IsVisible);
}

void WebViewImpl::pageDidScroll(const IntPoint& scrollPosition)
{
    bool pageIsScrolledToTop = scrollPosition.y() <= 0;
    if (pageIsScrolledToTop == m_pageIsScrolledToTop)
        return;

    [m_view.get() willChangeValueForKey:@"hasScrolledContentsUnderTitlebar"];

    m_pageIsScrolledToTop = pageIsScrolledToTop;

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    updateScrollPocketVisibilityWhenScrolledToTop();
    updatePrefersSolidColorHardPocket();
#endif

    [m_view.get() didChangeValueForKey:@"hasScrolledContentsUnderTitlebar"];
}

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)

void WebViewImpl::updateScrollPocketVisibilityWhenScrolledToTop()
{
    RetainPtr view = m_view.get();
    if ([view _usesAutomaticContentInsetBackgroundFill] && m_pageIsScrolledToTop)
        [view _addReasonToHideTopScrollPocket:HideScrollPocketReason::ScrolledToTop];
    else
        [view _removeReasonToHideTopScrollPocket:HideScrollPocketReason::ScrolledToTop];
}

void WebViewImpl::updateTopScrollPocketCaptureColor()
{
    RetainPtr view = m_view.get();
    if ([view _usesAutomaticContentInsetBackgroundFill])
        return;

    RetainPtr captureColor = [view _overrideTopScrollEdgeEffectColor];
    if (!captureColor)
        captureColor = [view _sampledTopFixedPositionContentColor];

    if (!captureColor) {
        if (auto backgroundColor = m_page->underPageBackgroundColorIgnoringPlatformColor(); backgroundColor.isValid())
            captureColor = cocoaColor(backgroundColor);
        else
            captureColor = NSColor.controlBackgroundColor;
    }

    captureColor = [view _adjustedColorForTopContentInsetColorFromUIDelegate:captureColor.get()];
    [m_topScrollPocket setCaptureColor:captureColor.get()];

    if (RetainPtr attachedInspectorWebView = [view _horizontallyAttachedInspectorWebView])
        [attachedInspectorWebView _setOverrideTopScrollEdgeEffectColor:captureColor.get()];
}

#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)

NSRect WebViewImpl::scrollViewFrame()
{
    auto insets = obscuredContentInsets();
    FloatRect boundsAdjustedByHorizontalInsets = [m_view.get() bounds];
    boundsAdjustedByHorizontalInsets.shiftXEdgeBy(insets.left());
    boundsAdjustedByHorizontalInsets.shiftMaxXEdgeBy(-insets.right());
    return [m_view.get() convertRect:boundsAdjustedByHorizontalInsets toView:nil];
}

bool WebViewImpl::hasScrolledContentsUnderTitlebar()
{
    return m_isRegisteredScrollViewSeparatorTrackingAdapter && !m_pageIsScrolledToTop;
}

void WebViewImpl::updateTitlebarAdjacencyState()
{
    RetainPtr window = WebViewImpl::window();
    bool visible = ![m_view.get() isHiddenOrHasHiddenAncestor];
    CGFloat topOfWindowContentLayoutRectInSelf = NSMinY([m_view.get() convertRect:[window contentLayoutRect] fromView:nil]);
    bool topOfWindowContentLayoutRectAdjacent = NSMinY([m_view.get() bounds]) <= topOfWindowContentLayoutRectInSelf;

    bool shouldRegister = topOfWindowContentLayoutRectAdjacent && visible && [retainPtr([m_view.get() effectiveAppearance]) _usesMetricsAppearance];

    if (shouldRegister && !m_isRegisteredScrollViewSeparatorTrackingAdapter && [m_view.get() conformsToProtocol:RetainPtr { @protocol(NSScrollViewSeparatorTrackingAdapter) }.get()]) {
        m_isRegisteredScrollViewSeparatorTrackingAdapter = [window registerScrollViewSeparatorTrackingAdapter:(NSObject<NSScrollViewSeparatorTrackingAdapter> *)m_view.get().get()];
    } else if (!shouldRegister && m_isRegisteredScrollViewSeparatorTrackingAdapter) {
        [window unregisterScrollViewSeparatorTrackingAdapter:(NSObject<NSScrollViewSeparatorTrackingAdapter> *)m_view.get().get()];
        m_isRegisteredScrollViewSeparatorTrackingAdapter = false;
    }
}

void WebViewImpl::scrollToRect(const WebCore::FloatRect& targetRect, const WebCore::FloatPoint& origin)
{
    m_page->scrollToRect(targetRect, origin);
}

RetainPtr<NSView> WebViewImpl::hitTest(CGPoint point)
{
    RetainPtr hitView = [m_view.get() _web_superHitTest:NSPointFromCGPoint(point)];
    if (hitView && hitView == m_layerHostingView)
        hitView = m_view.get();

    return hitView;
}

void WebViewImpl::scheduleMouseDidMoveOverElement(NSEvent *flagsChangedEvent)
{
    RetainPtr fakeEvent = [NSEvent mouseEventWithType:NSEventTypeMouseMoved location:flagsChangedEvent.window.mouseLocationOutsideOfEventStream
        modifierFlags:flagsChangedEvent.modifierFlags timestamp:flagsChangedEvent.timestamp windowNumber:flagsChangedEvent.windowNumber
        context:nullptr eventNumber:0 clickCount:0 pressure:0];
    NativeWebMouseEvent webEvent(fakeEvent.get(), m_lastPressureEvent.get(), m_view.get().get());
    m_page->dispatchMouseDidMoveOverElementAsynchronously(webEvent);
}

WebCore::DestinationColorSpace WebViewImpl::colorSpace()
{
    if (!m_colorSpace) {
        m_colorSpace = [&] () -> NSColorSpace * {
            if (m_targetWindowForMovePreparation)
                return [m_targetWindowForMovePreparation colorSpace];

            if (RetainPtr window = [m_view.get() window])
                return [window colorSpace];

            return nil;
        }();

        if (!m_colorSpace)
            m_colorSpace = [NSScreen mainScreen].colorSpace;

        if (!m_colorSpace)
            m_colorSpace = [NSColorSpace sRGBColorSpace];
    }

    ASSERT(m_colorSpace);
    return WebCore::DestinationColorSpace { [m_colorSpace CGColorSpace] };
}

void WebViewImpl::setUnderlayColor(NSColor *underlayColor)
{
    m_page->setUnderlayColor(WebCore::colorFromCocoaColor(underlayColor));
}

RetainPtr<NSColor> WebViewImpl::underlayColor() const
{
    return WebCore::cocoaColorOrNil(m_page->underlayColor()).autorelease();
}

RetainPtr<NSColor> WebViewImpl::pageExtendedBackgroundColor() const
{
    return WebCore::cocoaColorOrNil(m_page->pageExtendedBackgroundColor()).autorelease();
}

void WebViewImpl::setOverlayScrollbarStyle(std::optional<WebCore::ScrollbarOverlayStyle> scrollbarStyle)
{
    m_page->setOverlayScrollbarStyle(scrollbarStyle);
}

std::optional<WebCore::ScrollbarOverlayStyle> WebViewImpl::overlayScrollbarStyle() const
{
    return m_page->overlayScrollbarStyle();
}

void WebViewImpl::beginDeferringViewInWindowChanges()
{
    if (m_shouldDeferViewInWindowChanges) {
        NSLog(@"beginDeferringViewInWindowChanges was called while already deferring view-in-window changes!");
        return;
    }

    m_shouldDeferViewInWindowChanges = true;
}

void WebViewImpl::endDeferringViewInWindowChanges()
{
    if (!m_shouldDeferViewInWindowChanges) {
        NSLog(@"endDeferringViewInWindowChanges was called without beginDeferringViewInWindowChanges!");
        return;
    }

    m_shouldDeferViewInWindowChanges = false;

    if (m_viewInWindowChangeWasDeferred) {
        flushPendingObscuredContentInsetChanges();
        m_page->activityStateDidChange(WebCore::ActivityState::IsInWindow);
        m_viewInWindowChangeWasDeferred = false;
    }
}

void WebViewImpl::endDeferringViewInWindowChangesSync()
{
    if (!m_shouldDeferViewInWindowChanges) {
        NSLog(@"endDeferringViewInWindowChangesSync was called without beginDeferringViewInWindowChanges!");
        return;
    }

    m_shouldDeferViewInWindowChanges = false;

    if (m_viewInWindowChangeWasDeferred) {
        flushPendingObscuredContentInsetChanges();
        m_page->activityStateDidChange(WebCore::ActivityState::IsInWindow);
        m_viewInWindowChangeWasDeferred = false;
    }
}

void WebViewImpl::prepareForMoveToWindow(NSWindow *targetWindow, WTF::Function<void()>&& completionHandler)
{
    m_shouldDeferViewInWindowChanges = true;
    viewWillMoveToWindowImpl(targetWindow);
    m_isPreparingToUnparentView = !targetWindow;
    m_targetWindowForMovePreparation = targetWindow;
    viewDidMoveToWindow();

    m_shouldDeferViewInWindowChanges = false;

    WeakPtr weakThis { *this };
    m_page->installActivityStateChangeCompletionHandler(WTFMove(completionHandler));

    flushPendingObscuredContentInsetChanges();
    m_page->activityStateDidChange(WebCore::ActivityState::IsInWindow, WebPageProxy::ActivityStateChangeDispatchMode::Immediate);
    m_viewInWindowChangeWasDeferred = false;
}

void WebViewImpl::setFontForWebView(NSFont *font, id sender)
{
    RetainPtr fontManager = [NSFontManager sharedFontManager];
    NSFontTraitMask fontTraits = [fontManager traitsOfFont:font];

    WebCore::FontChanges changes;
    changes.setFontFamily(font.familyName);
    changes.setFontName(font.fontName);
    changes.setFontSize(font.pointSize);
    changes.setBold(fontTraits & NSBoldFontMask);
    changes.setItalic(fontTraits & NSItalicFontMask);

    if (RetainPtr<NSString> textStyleAttribute = [retainPtr(font.fontDescriptor) objectForKey:(__bridge NSString *)kCTFontDescriptorTextStyleAttribute])
        changes.setFontFamily(textStyleAttribute.get());

    m_page->changeFont(WTFMove(changes));
}

void WebViewImpl::updateSecureInputState()
{
    if (![protectedWindow() isKeyWindow] || !isFocused()) {
        if (m_inSecureInputState) {
            DisableSecureEventInput();
            m_inSecureInputState = false;
        }
        return;
    }
    // WKView has a single input context for all editable areas (except for plug-ins).
    RetainPtr context = [m_view.get() _web_superInputContext];
    bool isInPasswordField = m_page->editorState().isInPasswordField;

    if (isInPasswordField) {
        if (!m_inSecureInputState)
            EnableSecureEventInput();
        static NeverDestroyed<RetainPtr<NSArray>> romanInputSources = adoptNS([[NSArray alloc] initWithObjects:&NSAllRomanInputSourcesLocaleIdentifier count:1]);
        LOG(TextInput, "-> setAllowedInputSourceLocales:romanInputSources");
        [context setAllowedInputSourceLocales:romanInputSources.get().get()];
    } else {
        if (m_inSecureInputState)
            DisableSecureEventInput();
        LOG(TextInput, "-> setAllowedInputSourceLocales:nil");
        [context setAllowedInputSourceLocales:nil];
    }
    m_inSecureInputState = isInPasswordField;
}

void WebViewImpl::resetSecureInputState()
{
    if (m_inSecureInputState) {
        DisableSecureEventInput();
        m_inSecureInputState = false;
    }
}

void WebViewImpl::notifyInputContextAboutDiscardedComposition()
{
    // <rdar://problem/9359055>: -discardMarkedText can only be called for active contexts.
    // FIXME: We fail to ever notify the input context if something (e.g. a navigation) happens while the window is not key.
    // This is not a problem when the window is key, because we discard marked text on resigning first responder.
    if (![[m_view.get() window] isKeyWindow] || m_view.getAutoreleased() != [[m_view.get() window] firstResponder])
        return;

    LOG(TextInput, "-> discardMarkedText");

    [retainPtr([m_view.get() _web_superInputContext]) discardMarkedText]; // Inform the input method that we won't have an inline input area despite having been asked to.
}

void WebViewImpl::handleAcceptedAlternativeText(const String& acceptedAlternative)
{
    m_page->handleAlternativeTextUIResult(acceptedAlternative);
}


NSInteger WebViewImpl::spellCheckerDocumentTag()
{
    return m_page->spellDocumentTag();
}

void WebViewImpl::pressureChangeWithEvent(NSEvent *event)
{
    if (event == m_lastPressureEvent)
        return;

    if (m_ignoresNonWheelEvents)
        return;

    if (event.phase != NSEventPhaseChanged && event.phase != NSEventPhaseBegan && event.phase != NSEventPhaseEnded)
        return;

    NativeWebMouseEvent webEvent(event, m_lastPressureEvent.get(), m_view.get().get());
    m_page->handleMouseEvent(webEvent);

    m_lastPressureEvent = event;
}

#if ENABLE(FULLSCREEN_API)
bool WebViewImpl::hasFullScreenWindowController() const
{
    return !!m_fullScreenWindowController;
}

WKFullScreenWindowController *WebViewImpl::fullScreenWindowController()
{
    if (!m_fullScreenWindowController)
        m_fullScreenWindowController = adoptNS([[WKFullScreenWindowController alloc] initWithWindow:RetainPtr { fullScreenWindow() }.get() webView:m_view.get().get() page:m_page.get()]);

    return m_fullScreenWindowController.get();
}

RetainPtr<WKFullScreenWindowController> WebViewImpl::protectedFullScreenWindowController()
{
    return fullScreenWindowController();
}

void WebViewImpl::closeFullScreenWindowController()
{
    if (!m_fullScreenWindowController)
        return;

    [m_fullScreenWindowController close];
    m_fullScreenWindowController = nullptr;
}
#endif

NSView *WebViewImpl::fullScreenPlaceholderView()
{
#if ENABLE(FULLSCREEN_API)
    if (m_fullScreenWindowController && [m_fullScreenWindowController isFullScreen])
        return [m_fullScreenWindowController webViewPlaceholder];
#endif
    return nil;
}

NSWindow *WebViewImpl::fullScreenWindow()
{
#if ENABLE(FULLSCREEN_API)
    return adoptNS([[WebCoreFullScreenWindow alloc] initWithContentRect:[[NSScreen mainScreen] frame] styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUnifiedTitleAndToolbar | NSWindowStyleMaskFullSizeContentView | NSWindowStyleMaskResizable | NSWindowStyleMaskClosable) backing:NSBackingStoreBuffered defer:NO]).autorelease();
#else
    return nil;
#endif
}

bool WebViewImpl::isEditable() const
{
    return m_page->isEditable();
}

typedef HashMap<SEL, String> SelectorNameMap;

// Map selectors into Editor command names.
// This is not needed for any selectors that have the same name as the Editor command.
static const SelectorNameMap& selectorExceptionMap()
{
    static NeverDestroyed<SelectorNameMap> map;

    struct SelectorAndCommandName {
        SEL selector;
        ASCIILiteral commandName;
    };

    static const SelectorAndCommandName names[] = {
        { @selector(insertNewlineIgnoringFieldEditor:), "InsertNewline"_s },
        { @selector(insertParagraphSeparator:), "InsertNewline"_s },
        { @selector(insertTabIgnoringFieldEditor:), "InsertTab"_s },
        { @selector(pageDown:), "MovePageDown"_s },
        { @selector(pageDownAndModifySelection:), "MovePageDownAndModifySelection"_s },
        { @selector(pageUp:), "MovePageUp"_s },
        { @selector(pageUpAndModifySelection:), "MovePageUpAndModifySelection"_s },
        { @selector(scrollPageDown:), "ScrollPageForward"_s },
        { @selector(scrollPageUp:), "ScrollPageBackward"_s },
        { @selector(_pasteAsQuotation:), "PasteAsQuotation"_s },
    };

    for (auto& name : names)
        map.get().add(name.selector, name.commandName);

    return map;
}

static String commandNameForSelector(SEL selector)
{
    // Check the exception map first.
    static const SelectorNameMap& exceptionMap = selectorExceptionMap();
    SelectorNameMap::const_iterator it = exceptionMap.find(selector);
    if (it != exceptionMap.end())
        return it->value;

    // Remove the trailing colon.
    // No need to capitalize the command name since Editor command names are
    // not case sensitive.
    auto selectorName = unsafeSpan(sel_getName(selector));
    if (selectorName.size() < 2 || selectorName[selectorName.size() - 1] != ':')
        return String();
    return String(selectorName.first(selectorName.size() - 1));
}

bool WebViewImpl::executeSavedCommandBySelector(SEL selector)
{
    LOG(TextInput, "Executing previously saved command %s", sel_getName(selector));
    // The sink does two things: 1) Tells us if the responder went unhandled, and
    // 2) prevents any NSBeep; we don't ever want to beep here.
    RetainPtr<WKResponderChainSink> sink = adoptNS([[WKResponderChainSink alloc] initWithResponderChain:m_view.get().get()]);
    [m_view.get() _web_superDoCommandBySelector:selector];
    [sink detach];
    return ![sink didReceiveUnhandledCommand];
}

void WebViewImpl::executeEditCommandForSelector(SEL selector, const String& argument)
{
    m_page->executeEditCommand(commandNameForSelector(selector), argument);
}

void WebViewImpl::registerEditCommand(Ref<WebEditCommandProxy>&& command, UndoOrRedo undoOrRedo)
{
    auto actionName = command->label();
    auto commandObjC = adoptNS([[WKEditCommand alloc] initWithWebEditCommandProxy:WTFMove(command)]);

    RetainPtr undoManager = [m_view.get() undoManager];
    [undoManager registerUndoWithTarget:m_undoTarget.get() selector:((undoOrRedo == UndoOrRedo::Undo) ? @selector(undoEditing:) : @selector(redoEditing:)) object:commandObjC.get()];
    if (!actionName.isEmpty())
        [undoManager setActionName:actionName.createNSString().get()];
}

void WebViewImpl::clearAllEditCommands()
{
    [[m_view.get() undoManager] removeAllActionsWithTarget:m_undoTarget.get()];
}

bool WebViewImpl::writeSelectionToPasteboard(NSPasteboard *pasteboard, NSArray *types)
{
    size_t numTypes = types.count;
    [pasteboard clearContents];
    if (m_page->sessionID().isEphemeral())
        [pasteboard _setExpirationDate:[NSDate dateWithTimeIntervalSinceNow:pasteboardExpirationDelay.seconds()]];
    [pasteboard addTypes:types owner:nil];
    for (size_t i = 0; i < numTypes; ++i) {
        BOOL wantsPlainText = [retainPtr([types objectAtIndex:i]) isEqualTo:WebCore::legacyStringPasteboardTypeSingleton()];
        RELEASE_LOG(Pasteboard, "Synchronously requesting %{public}s for selected range", wantsPlainText ? "plain text" : "data");
        if (wantsPlainText)
            [pasteboard setString:m_page->stringSelectionForPasteboard().createNSString().get() forType:WebCore::legacyStringPasteboardTypeSingleton()];
        else {
            RetainPtr type = [types objectAtIndex:i];
            RefPtr<WebCore::SharedBuffer> buffer = m_page->dataSelectionForPasteboard(type.get());
            [pasteboard setData:buffer ? buffer->createNSData().get() : nil forType:type.get()];
        }
    }
    return true;
}

bool WebViewImpl::readSelectionFromPasteboard(NSPasteboard *pasteboard)
{
    return m_page->readSelectionFromPasteboard([pasteboard name]);
}

id WebViewImpl::validRequestorForSendAndReturnTypes(NSString *sendType, NSString *returnType)
{
    EditorState editorState = m_page->editorState();
    bool isValidSendType = !sendType;

    if (sendType && !editorState.selectionIsNone) {
        if (editorState.isInPlugin)
            isValidSendType = [sendType isEqualToString:WebCore::legacyStringPasteboardTypeSingleton()];
        else
            isValidSendType = [PasteboardTypes::forSelectionSingleton() containsObject:sendType];
    }

    bool isValidReturnType = false;
    if (!returnType)
        isValidReturnType = true;
    else if ([PasteboardTypes::forEditingSingleton() containsObject:returnType] && editorState.isContentEditable) {
        // We can insert strings in any editable context.  We can insert other types, like images, only in rich edit contexts.
        isValidReturnType = editorState.isContentRichlyEditable || [returnType isEqualToString:WebCore::legacyStringPasteboardTypeSingleton()];
    }
    if (isValidSendType && isValidReturnType)
        return m_view.getAutoreleased();
    return [retainPtr([m_view.get() nextResponder]) validRequestorForSendType:sendType returnType:returnType];
}

void WebViewImpl::centerSelectionInVisibleArea()
{
    m_page->centerSelectionInVisibleArea();
}

void WebViewImpl::selectionDidChange()
{
    updateFontManagerIfNeeded();
    if (!m_isHandlingAcceptedCandidate)
        m_softSpaceRange = NSMakeRange(NSNotFound, 0);
#if HAVE(TOUCH_BAR)
    updateTouchBar();
    if (m_page->editorState().hasPostLayoutData())
        requestCandidatesForSelectionIfNeeded();
#endif

#if HAVE(REDESIGNED_TEXT_CURSOR)
    if (m_page->editorState().hasPostLayoutData())
        updateCursorAccessoryPlacement();
#endif

#if ENABLE(WRITING_TOOLS)
    if (isEditable() || m_page->configuration().writingToolsBehavior() == WebCore::WritingTools::Behavior::Complete) {
        auto isRange = m_page->editorState().hasPostLayoutData() && m_page->editorState().selectionIsRange;
        auto selectionRect = isRange ? m_page->editorState().postLayoutData->selectionBoundingRect : IntRect { };

        // The affordance will only show up if the selected range consists of >= 50 characters.
        [[PAL::getWTWritingToolsClassSingleton() sharedInstance] scheduleShowAffordanceForSelectionRect:selectionRect ofView:m_view.get().get() forDelegate:(NSObject<WTWritingToolsDelegate> *)m_view.get().get()];
    }
#endif

    RetainPtr window = WebViewImpl::window();
    if (window.get().firstResponder == m_view.get().get()) {
        RetainPtr<NSInspectorBar> inspectorBar = window.get().inspectorBar;
        if (inspectorBar.get().visible)
            [inspectorBar _update];
    }

    [m_view.get() _web_editorStateDidChange];
}

void WebViewImpl::showShareSheet(WebCore::ShareDataWithParsedURL&& data, WTF::CompletionHandler<void(bool)>&& completionHandler, WKWebView *view)
{
    if (_shareSheet)
        [_shareSheet dismissIfNeededWithReason:WebKit::PickerDismissalReason::ResetState];

    ASSERT([view respondsToSelector:@selector(shareSheetDidDismiss:)]);
    _shareSheet = adoptNS([[WKShareSheet alloc] initWithView:view]);
    [_shareSheet setDelegate:view];

    [_shareSheet presentWithParameters:data inRect:std::nullopt completionHandler:WTFMove(completionHandler)];
}

void WebViewImpl::shareSheetDidDismiss(WKShareSheet *shareSheet)
{
    ASSERT(_shareSheet == shareSheet);

    [_shareSheet setDelegate:nil];
    _shareSheet = nil;
}

#if HAVE(DIGITAL_CREDENTIALS_UI)
void WebViewImpl::showDigitalCredentialsPicker(const WebCore::DigitalCredentialsRequestData& requestData, CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&& completionHandler, WKWebView* webView)
{
    if (!_digitalCredentialsPicker)
        _digitalCredentialsPicker = adoptNS([[WKDigitalCredentialsPicker alloc] initWithView:webView page:m_page.ptr()]);

    [_digitalCredentialsPicker presentWithRequestData:requestData completionHandler:WTFMove(completionHandler)];
}

void WebViewImpl::dismissDigitalCredentialsPicker(CompletionHandler<void(bool)>&& completionHandler, WKWebView* webView)
{
    if (!_digitalCredentialsPicker) {
        LOG(DigitalCredentials, "Digital credentials picker is not being presented.");
        completionHandler(false);
        return;
    }

    [_digitalCredentialsPicker dismissWithCompletionHandler:WTFMove(completionHandler)];
}
#endif

void WebViewImpl::didBecomeEditable()
{
    [m_windowVisibilityObserver enableObservingFontPanel];

    RunLoop::mainSingleton().dispatch([] {
        [[NSSpellChecker sharedSpellChecker] _preflightChosenSpellServer];
    });
}

void WebViewImpl::updateFontManagerIfNeeded()
{
    BOOL fontPanelIsVisible = NSFontPanel.sharedFontPanelExists && NSFontPanel.sharedFontPanel.visible;
    if (!fontPanelIsVisible && !(m_page->isEditable() && m_page->editorState().isContentRichlyEditable))
        return;

    m_page->requestFontAttributesAtSelectionStart([] (auto& attributes) {
        if (!attributes.font)
            return;

        RetainPtr nsFont = (__bridge NSFont *)attributes.font->ctFont();
        if (!nsFont)
            return;

        [NSFontManager.sharedFontManager setSelectedFont:nsFont.get() isMultiple:attributes.hasMultipleFonts];
        [NSFontManager.sharedFontManager setSelectedAttributes:attributes.createDictionary().get() isMultiple:attributes.hasMultipleFonts];
    });
}

void WebViewImpl::typingAttributesWithCompletionHandler(void(^completion)(NSDictionary<NSString *, id> *))
{
    m_page->requestFontAttributesAtSelectionStart([completion = makeBlockPtr(completion)] (const WebCore::FontAttributes& attributes) {
        auto attributesAsDictionary = attributes.createDictionary();
        completion(attributesAsDictionary.get());
    });
}

void WebViewImpl::changeFontColorFromSender(id sender)
{
    if (![sender respondsToSelector:@selector(color)])
        return;

    RetainPtr color = dynamic_objc_cast<NSColor>([sender color]);
    if (!color)
        return;

    auto& editorState = m_page->editorState();
    if (!editorState.isContentEditable || editorState.selectionIsNone)
        return;

    WebCore::FontAttributeChanges changes;
    changes.setForegroundColor(WebCore::colorFromCocoaColor(color.get()));
    m_page->changeFontAttributes(WTFMove(changes));
}

void WebViewImpl::changeFontAttributesFromSender(id sender)
{
    auto& editorState = m_page->editorState();
    if (!editorState.isContentEditable || editorState.selectionIsNone)
        return;

    m_page->changeFontAttributes(WebCore::computedFontAttributeChanges(NSFontManager.sharedFontManager, sender));
}

void WebViewImpl::changeFontFromFontManager()
{
    auto& editorState = m_page->editorState();
    if (!editorState.isContentEditable || editorState.selectionIsNone)
        return;

    m_page->changeFont(WebCore::computedFontChanges(NSFontManager.sharedFontManager));
}

static NSMenuItem *menuItem(id<NSValidatedUserInterfaceItem> item)
{
    if (![(NSObject *)item isKindOfClass:[NSMenuItem class]])
        return nil;
    return (NSMenuItem *)item;
}

static RetainPtr<NSMenuItem> protectedMenuItem(id<NSValidatedUserInterfaceItem> item)
{
    return menuItem(item);
}

static NSToolbarItem *toolbarItem(id<NSValidatedUserInterfaceItem> item)
{
    if (![(NSObject *)item isKindOfClass:[NSToolbarItem class]])
        return nil;
    return (NSToolbarItem *)item;
}

bool WebViewImpl::validateUserInterfaceItem(id<NSValidatedUserInterfaceItem> item)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    SEL action = [item action];

    if (action == @selector(showGuessPanel:)) {
        if (RetainPtr menuItem = WebKit::menuItem(item))
            [menuItem setTitle:WebCore::contextMenuItemTagShowSpellingPanel(![[[NSSpellChecker sharedSpellChecker] spellingPanel] isVisible]).createNSString().get()];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(checkSpelling:) || action == @selector(changeSpelling:))
        return m_page->editorState().isContentEditable;

    if (action == @selector(toggleContinuousSpellChecking:)) {
        bool enabled = TextChecker::isContinuousSpellCheckingAllowed();
        bool checked = enabled && TextChecker::state().contains(TextCheckerState::ContinuousSpellCheckingEnabled);
        [protectedMenuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return enabled;
    }

    if (action == @selector(toggleGrammarChecking:)) {
        bool checked = TextChecker::state().contains(TextCheckerState::GrammarCheckingEnabled);
        [protectedMenuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return true;
    }

    if (action == @selector(toggleAutomaticSpellingCorrection:)) {
        bool enable = m_page->editorState().canEnableAutomaticSpellingCorrection;
        protectedMenuItem(item).get().state = TextChecker::state().contains(TextCheckerState::AutomaticSpellingCorrectionEnabled) && enable ? NSControlStateValueOn : NSControlStateValueOff;
        return enable;
    }

    if (action == @selector(orderFrontSubstitutionsPanel:)) {
        if (RetainPtr menuItem = WebKit::menuItem(item))
            [menuItem setTitle:WebCore::contextMenuItemTagShowSubstitutions(![[[NSSpellChecker sharedSpellChecker] substitutionsPanel] isVisible]).createNSString().get()];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(toggleSmartInsertDelete:)) {
        bool checked = m_page->isSmartInsertDeleteEnabled();
        [protectedMenuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticQuoteSubstitution:)) {
        bool checked = TextChecker::state().contains(TextCheckerState::AutomaticQuoteSubstitutionEnabled);
        [protectedMenuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticDashSubstitution:)) {
        bool checked = TextChecker::state().contains(TextCheckerState::AutomaticDashSubstitutionEnabled);
        [protectedMenuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticLinkDetection:)) {
        bool checked = TextChecker::state().contains(TextCheckerState::AutomaticLinkDetectionEnabled);
        [protectedMenuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticTextReplacement:)) {
        bool checked = TextChecker::state().contains(TextCheckerState::AutomaticTextReplacementEnabled);
        [protectedMenuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(uppercaseWord:) || action == @selector(lowercaseWord:) || action == @selector(capitalizeWord:))
        return m_page->editorState().selectionIsRange && m_page->editorState().isContentEditable;

    if (action == @selector(stopSpeaking:))
        return [NSApp isSpeaking];

    // The centerSelectionInVisibleArea: selector is enabled if there's a selection range or if there's an insertion point in an editable area.
    if (action == @selector(centerSelectionInVisibleArea:))
        return m_page->editorState().selectionIsRange || (m_page->editorState().isContentEditable && !m_page->editorState().selectionIsNone);

#if ENABLE(WRITING_TOOLS) && HAVE(NSRESPONDER_WRITING_TOOLS_SUPPORT)
    if (action == @selector(showWritingTools:))
        return m_page->shouldEnableWritingToolsRequestedTool(convertToWebRequestedTool((WTRequestedTool)[item tag]));
#endif

    // Next, handle editor commands. Start by returning true for anything that is not an editor command.
    // Returning true is the default thing to do in an AppKit validate method for any selector that is not recognized.
    String commandName = commandNameForSelector([item action]);
    if (!WebCore::Editor::commandIsSupportedFromMenuOrKeyBinding(commandName))
        return true;

    // Add this item to the vector of items for a given command that are awaiting validation.
    ValidationMap::AddResult addResult = m_validationMap.add(commandName, ValidationVector());
    addResult.iterator->value.append(item);
    if (addResult.isNewEntry) {
        // If we are not already awaiting validation for this command, start the asynchronous validation process.
        // FIXME: Theoretically, there is a race here; when we get the answer it might be old, from a previous time
        // we asked for the same command; there is no guarantee the answer is still valid.
        m_page->validateCommand(commandName, [weakThis = WeakPtr { *this }, commandName](bool isEnabled, int32_t state) {
            if (!weakThis)
                return;

            weakThis->setUserInterfaceItemState(commandName.createNSString().get(), isEnabled, state);
        });
    }

    // Treat as enabled until we get the result back from the web process and _setUserInterfaceItemState is called.
    // FIXME <rdar://problem/8803459>: This means disabled items will flash enabled at first for a moment.
    // But returning NO here would be worse; that would make keyboard commands such as command-C fail.
    return true;
}

void WebViewImpl::setUserInterfaceItemState(NSString *commandName, bool enabled, int state)
{
    ValidationVector items = m_validationMap.take(commandName);
    for (auto& item : items) {
        RetainPtr protectedMenuItem = menuItem(item.get());
        RetainPtr protectedToolbarItem = toolbarItem(item.get());
        [protectedMenuItem setState:state];
        [protectedMenuItem setEnabled:enabled];
        [protectedToolbarItem setEnabled:enabled];
        // FIXME <rdar://problem/8803392>: If the item is neither a menu nor toolbar item, it will be left enabled.
    }
}

void WebViewImpl::startSpeaking()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    m_page->getSelectionOrContentsAsString([](const String& string) {
        if (!string)
            return;

        [NSApp speakString:string.createNSString().get()];
    });
}

void WebViewImpl::stopSpeaking(id sender)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    [NSApp stopSpeaking:sender];
}

void WebViewImpl::showGuessPanel(id sender)
{
    RetainPtr checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    RetainPtr spellingPanel = [checker spellingPanel];
    if ([spellingPanel isVisible]) {
        [spellingPanel orderOut:sender];
        return;
    }

    m_page->advanceToNextMisspelling(true);
    [spellingPanel orderFront:sender];
}

void WebViewImpl::checkSpelling()
{
    m_page->advanceToNextMisspelling(false);
}

void WebViewImpl::changeSpelling(id sender)
{
    RetainPtr word = [[sender selectedCell] stringValue];

    m_page->changeSpellingToWord(word.get());
}

void WebViewImpl::setContinuousSpellCheckingEnabled(bool enabled)
{
    if (TextChecker::state().contains(TextCheckerState::ContinuousSpellCheckingEnabled) == enabled)
        return;

    TextChecker::setContinuousSpellCheckingEnabled(enabled);
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

void WebViewImpl::toggleContinuousSpellChecking()
{
    bool spellCheckingEnabled = !TextChecker::state().contains(TextCheckerState::ContinuousSpellCheckingEnabled);
    TextChecker::setContinuousSpellCheckingEnabled(spellCheckingEnabled);

    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

bool WebViewImpl::isGrammarCheckingEnabled()
{
    return TextChecker::state().contains(TextCheckerState::GrammarCheckingEnabled);
}

void WebViewImpl::setGrammarCheckingEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().contains(TextCheckerState::GrammarCheckingEnabled))
        return;

    TextChecker::setGrammarCheckingEnabled(flag);
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

void WebViewImpl::toggleGrammarChecking()
{
    bool grammarCheckingEnabled = !TextChecker::state().contains(TextCheckerState::GrammarCheckingEnabled);
    TextChecker::setGrammarCheckingEnabled(grammarCheckingEnabled);

    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticSpellingCorrection()
{
    TextChecker::setAutomaticSpellingCorrectionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticSpellingCorrectionEnabled));

    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

void WebViewImpl::orderFrontSubstitutionsPanel(id sender)
{
    RetainPtr checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    RetainPtr substitutionsPanel = [checker substitutionsPanel];
    if ([substitutionsPanel isVisible]) {
        [substitutionsPanel orderOut:sender];
        return;
    }
    [substitutionsPanel orderFront:sender];
}

void WebViewImpl::toggleSmartInsertDelete()
{
    m_page->setSmartInsertDeleteEnabled(!m_page->isSmartInsertDeleteEnabled());
}

bool WebViewImpl::isAutomaticQuoteSubstitutionEnabled()
{
    return TextChecker::state().contains(TextCheckerState::AutomaticQuoteSubstitutionEnabled);
}

void WebViewImpl::setAutomaticQuoteSubstitutionEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().contains(TextCheckerState::AutomaticQuoteSubstitutionEnabled))
        return;

    TextChecker::setAutomaticQuoteSubstitutionEnabled(flag);
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticQuoteSubstitution()
{
    TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticQuoteSubstitutionEnabled));
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

bool WebViewImpl::isAutomaticDashSubstitutionEnabled()
{
    return TextChecker::state().contains(TextCheckerState::AutomaticDashSubstitutionEnabled);
}

void WebViewImpl::setAutomaticDashSubstitutionEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().contains(TextCheckerState::AutomaticDashSubstitutionEnabled))
        return;

    TextChecker::setAutomaticDashSubstitutionEnabled(flag);
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticDashSubstitution()
{
    TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticDashSubstitutionEnabled));
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

bool WebViewImpl::isAutomaticLinkDetectionEnabled()
{
    return TextChecker::state().contains(TextCheckerState::AutomaticLinkDetectionEnabled);
}

void WebViewImpl::setAutomaticLinkDetectionEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().contains(TextCheckerState::AutomaticLinkDetectionEnabled))
        return;

    TextChecker::setAutomaticLinkDetectionEnabled(flag);
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticLinkDetection()
{
    TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticLinkDetectionEnabled));
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

bool WebViewImpl::isAutomaticTextReplacementEnabled()
{
    return TextChecker::state().contains(TextCheckerState::AutomaticTextReplacementEnabled);
}

void WebViewImpl::setAutomaticTextReplacementEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().contains(TextCheckerState::AutomaticTextReplacementEnabled))
        return;

    TextChecker::setAutomaticTextReplacementEnabled(flag);
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticTextReplacement()
{
    TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticTextReplacementEnabled));
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

bool WebViewImpl::isSmartListsEnabled()
{
    if (!m_page->protectedPreferences()->smartListsAvailable())
        return false;

    return TextChecker::state().contains(TextCheckerState::SmartListsEnabled);
}

void WebViewImpl::setSmartListsEnabled(bool flag)
{
    if (!m_page->protectedPreferences()->smartListsAvailable())
        return;

    if (flag == TextChecker::state().contains(TextCheckerState::SmartListsEnabled))
        return;

    TextChecker::setSmartListsEnabled(flag);
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

void WebViewImpl::toggleSmartLists()
{
    if (!m_page->protectedPreferences()->smartListsAvailable())
        return;

    TextChecker::setSmartListsEnabled(!TextChecker::state().contains(TextCheckerState::SmartListsEnabled));
    m_page->protectedLegacyMainFrameProcess()->updateTextCheckerState();
}

void WebViewImpl::uppercaseWord()
{
    m_page->uppercaseWord();
}

void WebViewImpl::lowercaseWord()
{
    m_page->lowercaseWord();
}

void WebViewImpl::capitalizeWord()
{
    m_page->capitalizeWord();
}

NSTextCheckingTypes WebViewImpl::getTextCheckingTypes() const
{
    NSTextCheckingTypes types = NSTextCheckingTypeSpelling | NSTextCheckingTypeReplacement | NSTextCheckingTypeCorrection;
#if HAVE(INLINE_PREDICTIONS)
    if (allowsInlinePredictions()) {
        types |= (NSTextCheckingType)_NSTextCheckingTypeSingleCompletion;

#if HAVE(NS_TEXT_CHECKING_TYPE_MATH_COMPLETION)
        types |= (NSTextCheckingType)_NSTextCheckingTypeMathCompletion;
#endif
    }
#endif

    return types;
}

void WebViewImpl::requestCandidatesForSelectionIfNeeded()
{
    if (!shouldRequestCandidates())
        return;

    auto postLayoutData = postLayoutDataForContentEditable();
    if (!postLayoutData)
        return;

    m_lastStringForCandidateRequest = postLayoutData->stringForCandidateRequest;

    NSRange selectedRange = NSMakeRange(postLayoutData->candidateRequestStartPosition, postLayoutData->selectedTextLength);
    NSTextCheckingTypes checkingTypes = getTextCheckingTypes();

    WeakPtr weakThis { *this };
    m_lastCandidateRequestSequenceNumber = [[NSSpellChecker sharedSpellChecker] requestCandidatesForSelectedRange:selectedRange inString:postLayoutData->paragraphContextForCandidateRequest.createNSString().get() types:checkingTypes options:nil inSpellDocumentWithTag:spellCheckerDocumentTag() completionHandler:[weakThis](NSInteger sequenceNumber, NSArray<NSTextCheckingResult *> *candidates) {
        RunLoop::mainSingleton().dispatch([weakThis, sequenceNumber, candidates = retainPtr(candidates)] {
            if (!weakThis)
                return;
            weakThis->handleRequestedCandidates(sequenceNumber, candidates.get());
        });
    }];
}

std::optional<EditorState::PostLayoutData> WebViewImpl::postLayoutDataForContentEditable()
{
    const EditorState& editorState = m_page->editorState();

    // FIXME: It's pretty lame that we have to depend on the most recent EditorState having post layout data,
    // and that we just bail if it is missing.
    if (!editorState.hasPostLayoutData())
        return std::nullopt;

    return editorState.postLayoutData;
}

void WebViewImpl::handleRequestedCandidates(NSInteger sequenceNumber, NSArray<NSTextCheckingResult *> *candidates)
{
    if (!shouldRequestCandidates())
        return;

    if (m_lastCandidateRequestSequenceNumber != sequenceNumber)
        return;

    auto postLayoutData = postLayoutDataForContentEditable();
    if (!postLayoutData)
        return;

    if (m_lastStringForCandidateRequest != postLayoutData->stringForCandidateRequest)
        return;

#if HAVE(TOUCH_BAR) || HAVE(INLINE_PREDICTIONS)
    NSRange selectedRange = NSMakeRange(postLayoutData->candidateRequestStartPosition, postLayoutData->selectedTextLength);
    WebCore::IntRect offsetSelectionRect = postLayoutData->selectionBoundingRect;
    offsetSelectionRect.move(0, offsetSelectionRect.height());

    RetainPtr candidateList = candidateListTouchBarItem();
    [candidateList setCandidates:candidates forSelectedRange:selectedRange inString:postLayoutData->paragraphContextForCandidateRequest.createNSString().get() rect:offsetSelectionRect view:m_view.get().get() completionHandler:nil];

#if HAVE(INLINE_PREDICTIONS)
    if (allowsInlinePredictions())
        showInlinePredictionsForCandidates(candidates);
#endif
#else
    UNUSED_PARAM(candidates);
#endif
}

static constexpr WebCore::TextCheckingType coreTextCheckingType(NSTextCheckingType type)
{
    switch (type) {
    case NSTextCheckingTypeCorrection:
        return WebCore::TextCheckingType::Correction;
    case NSTextCheckingTypeReplacement:
        return WebCore::TextCheckingType::Replacement;
    case NSTextCheckingTypeSpelling:
        return WebCore::TextCheckingType::Spelling;
    default:
        return WebCore::TextCheckingType::None;
    }
}

static WebCore::TextCheckingResult textCheckingResultFromNSTextCheckingResult(NSTextCheckingResult *nsResult)
{
    WebCore::TextCheckingResult result;
    result.type = coreTextCheckingType(nsResult.resultType);
    result.range = nsResult.range;
    result.replacement = nsResult.replacementString;
    return result;
}

void WebViewImpl::handleAcceptedCandidate(NSTextCheckingResult *acceptedCandidate)
{
    auto postLayoutData = postLayoutDataForContentEditable();
    if (!postLayoutData)
        return;

    if (m_lastStringForCandidateRequest != postLayoutData->stringForCandidateRequest)
        return;

    m_isHandlingAcceptedCandidate = true;
    NSRange range = [acceptedCandidate range];
    if (acceptedCandidate.replacementString && [acceptedCandidate.replacementString length] > 0) {
        NSRange replacedRange = NSMakeRange(range.location, [acceptedCandidate.replacementString length]);
        NSRange softSpaceRange = NSMakeRange(NSMaxRange(replacedRange) - 1, 1);
        if ([retainPtr(acceptedCandidate.replacementString) hasSuffix:@" "])
            m_softSpaceRange = softSpaceRange;
    }

    m_page->handleAcceptedCandidate(textCheckingResultFromNSTextCheckingResult(acceptedCandidate));
    m_page->callAfterNextPresentationUpdate([viewImpl = WeakPtr { *this }] {
        if (!viewImpl)
            return;

        viewImpl->m_isHandlingAcceptedCandidate = false;
        [viewImpl->m_view.get() _didHandleAcceptedCandidate];
    });
}

void WebViewImpl::updateNeedsViewFrameInWindowCoordinatesIfNeeded()
{
    BOOL needsViewFrameInWindowCoordinates = false;

    if (!!needsViewFrameInWindowCoordinates == !!m_needsViewFrameInWindowCoordinates)
        return;

    m_needsViewFrameInWindowCoordinates = needsViewFrameInWindowCoordinates;
    if ([m_view.get() window])
        updateWindowAndViewFrames();
}

void WebViewImpl::preferencesDidChange()
{
    updateNeedsViewFrameInWindowCoordinatesIfNeeded();

    if (RetainPtr panGestureController = m_panGestureController)
        [panGestureController enablePanGestureIfNeeded];
}

CALayer* WebViewImpl::textIndicatorInstallationLayer()
{
    return [m_layerHostingView layer];
}

void WebViewImpl::dismissContentRelativeChildWindowsWithAnimation(bool animate)
{
    [m_view.get() _web_dismissContentRelativeChildWindowsWithAnimation:animate];
}

void WebViewImpl::dismissContentRelativeChildWindowsWithAnimationFromViewOnly(bool animate)
{
    // Calling _clearTextIndicatorWithAnimation here will win out over the animated clear in dismissContentRelativeChildWindowsFromViewOnly.
    // We can't invert these because clients can override (and have overridden) _dismissContentRelativeChildWindows, so it needs to be called.
    // For this same reason, this can't be moved to WebViewImpl without care.
    [m_view.get() _web_dismissContentRelativeChildWindows];
}

void WebViewImpl::dismissContentRelativeChildWindowsFromViewOnly()
{
    bool hasActiveImmediateAction = false;
    hasActiveImmediateAction = [m_immediateActionController hasActiveImmediateAction];

    // FIXME: We don't know which panel we are dismissing, it may not even be in the current page (see <rdar://problem/13875766>).
    if ([m_view.get() window].isKeyWindow || hasActiveImmediateAction) {
        WebCore::DictionaryLookup::hidePopup();

        if (PAL::isDataDetectorsFrameworkAvailable())
            [[PAL::getDDActionsManagerClassSingleton() sharedManager] requestBubbleClosureUnanchorOnFailure:YES];
    }

    m_page->clearTextIndicatorWithAnimation(WebCore::TextIndicatorDismissalAnimation::FadeOut);

    [m_immediateActionController dismissContentRelativeChildWindows];

    m_pageClient->dismissCorrectionPanel(WebCore::ReasonForDismissingAlternativeText::Ignored);

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
    [std::exchange(m_lastContextMenuTranslationPopover, nil).get() close];
#endif
}

bool WebViewImpl::hasContentRelativeChildViews() const
{
#if ENABLE(WRITING_TOOLS)
    return [m_view.get() _web_hasActiveIntelligenceTextEffects] || [m_textAnimationTypeManager hasActiveTextAnimationType];
#else
    return false;
#endif
}

void WebViewImpl::suppressContentRelativeChildViews(ContentRelativeChildViewsSuppressionType type)
{
    if (!hasContentRelativeChildViews())
        return;

    switch (type) {
    case ContentRelativeChildViewsSuppressionType::Remove:
        return m_contentRelativeViewsHysteresis->start();

    case ContentRelativeChildViewsSuppressionType::Restore:
        return m_contentRelativeViewsHysteresis->stop();

    case ContentRelativeChildViewsSuppressionType::TemporarilyRemove:
        return m_contentRelativeViewsHysteresis->impulse();
    }
}

void WebViewImpl::contentRelativeViewsHysteresisTimerFired(PAL::HysteresisState state)
{
    if (!hasContentRelativeChildViews())
        return;

    if (state == PAL::HysteresisState::Started)
        suppressContentRelativeChildViews();
    else
        restoreContentRelativeChildViews();
}

void WebViewImpl::suppressContentRelativeChildViews()
{
#if ENABLE(WRITING_TOOLS)
    [m_view.get() _web_suppressContentRelativeChildViews];
    [m_textAnimationTypeManager suppressTextAnimationType];
#endif
}

void WebViewImpl::restoreContentRelativeChildViews()
{
#if ENABLE(WRITING_TOOLS)
    [m_view.get() _web_restoreContentRelativeChildViews];
    [m_textAnimationTypeManager restoreTextAnimationType];
#endif
}

void WebViewImpl::hideWordDefinitionWindow()
{
    WebCore::DictionaryLookup::hidePopup();
}

void WebViewImpl::quickLookWithEvent(NSEvent *event)
{
    if (ignoresNonWheelEvents())
        return;

    if (m_immediateActionGestureRecognizer) {
        [m_view.get() _web_superQuickLookWithEvent:event];
        return;
    }

    NSPoint locationInViewCoordinates = [m_view.get() convertPoint:[event locationInWindow] fromView:nil];
    m_page->performDictionaryLookupAtLocation(WebCore::FloatPoint(locationInViewCoordinates));
}

void WebViewImpl::prepareForDictionaryLookup()
{
    [m_windowVisibilityObserver startObservingLookupDismissalIfNeeded];
}

void WebViewImpl::setAllowsLinkPreview(bool allowsLinkPreview)
{
    if (m_allowsLinkPreview == allowsLinkPreview)
        return;

    m_allowsLinkPreview = allowsLinkPreview;

    if (!allowsLinkPreview)
        [m_view.get() removeGestureRecognizer:m_immediateActionGestureRecognizer.get()];
    else if (RetainPtr immediateActionRecognizer = m_immediateActionGestureRecognizer.get())
        [m_view.get() addGestureRecognizer:immediateActionRecognizer.get()];
}

NSObject *WebViewImpl::immediateActionAnimationControllerForHitTestResult(API::HitTestResult* hitTestResult, uint32_t type, API::Object* userData)
{
    return [m_view.get() _web_immediateActionAnimationControllerForHitTestResultInternal:hitTestResult withType:type userData:userData];
}

void WebViewImpl::didPerformImmediateActionHitTest(const WebHitTestResultData& result, bool contentPreventsDefault, API::Object* userData)
{
    [m_immediateActionController didPerformImmediateActionHitTest:result contentPreventsDefault:contentPreventsDefault userData:userData];
}

void WebViewImpl::prepareForImmediateActionAnimation()
{
    [m_view.get() _web_prepareForImmediateActionAnimation];
}

void WebViewImpl::cancelImmediateActionAnimation()
{
    [m_view.get() _web_cancelImmediateActionAnimation];
}

void WebViewImpl::completeImmediateActionAnimation()
{
    [m_view.get() _web_completeImmediateActionAnimation];
}

void WebViewImpl::didChangeContentSize(CGSize newSize)
{
    [m_view.get() _web_didChangeContentSize:NSSizeFromCGSize(newSize)];
}

void WebViewImpl::videoControlsManagerDidChange()
{
#if HAVE(TOUCH_BAR)
    updateTouchBar();
#endif

#if ENABLE(FULLSCREEN_API)
    if (hasFullScreenWindowController())
        [protectedFullScreenWindowController() videoControlsManagerDidChange];
#endif
}

void WebViewImpl::setIgnoresNonWheelEvents(bool ignoresNonWheelEvents)
{
    RELEASE_LOG(MouseHandling, "[pageProxyID=%lld] WebViewImpl::setIgnoresNonWheelEvents:%d", m_page->identifier().toUInt64(), ignoresNonWheelEvents);

    if (m_ignoresNonWheelEvents == ignoresNonWheelEvents)
        return;

    m_ignoresNonWheelEvents = ignoresNonWheelEvents;
    m_page->setShouldDispatchFakeMouseMoveEvents(!ignoresNonWheelEvents);

    if (ignoresNonWheelEvents)
        [m_view.get() removeGestureRecognizer:m_immediateActionGestureRecognizer.get()];
    else if (RetainPtr immediateActionRecognizer = m_immediateActionGestureRecognizer.get()) {
        if (m_allowsLinkPreview)
            [m_view.get() addGestureRecognizer:immediateActionRecognizer.get()];
    }
}

void WebViewImpl::setIgnoresAllEvents(bool ignoresAllEvents)
{
    RELEASE_LOG(MouseHandling, "[pageProxyID=%lld] WebViewImpl::setIgnoresAllEvents:%d", m_page->identifier().toUInt64(), ignoresAllEvents);
    m_ignoresAllEvents = ignoresAllEvents;
    setIgnoresNonWheelEvents(ignoresAllEvents);
}

void WebViewImpl::setIgnoresMouseDraggedEvents(bool ignoresMouseDraggedEvents)
{
    m_ignoresMouseDraggedEvents = ignoresMouseDraggedEvents;
}

void WebViewImpl::setAccessibilityWebProcessToken(NSData *data, pid_t pid)
{
    if (pid == m_page->legacyMainFrameProcess().processID()) {
        m_remoteAccessibilityChild = data.length ? adoptNS([[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:data]) : nil;
        m_remoteAccessibilityChildToken = data;
        updateRemoteAccessibilityRegistration(true);
    }
}

NSUInteger WebViewImpl::accessibilityRemoteChildTokenHash()
{
    return [m_remoteAccessibilityChildToken hash];
}

NSUInteger WebViewImpl::accessibilityUIProcessLocalTokenHash()
{
    return [m_remoteAccessibilityTokenGeneratedByUIProcess hash];
}

NSArray<NSNumber *> *WebViewImpl::registeredRemoteAccessibilityPids()
{
    NSMutableArray<NSNumber *> *result = [NSMutableArray new];
    for (pid_t pid : m_registeredRemoteAccessibilityPids)
        [result addObject:@(pid)];
    return result;
}

bool WebViewImpl::hasRemoteAccessibilityChild()
{
    return !!remoteAccessibilityChildIfNotSuspended();
}

void WebViewImpl::updateRemoteAccessibilityRegistration(bool registerProcess)
{
    // When the tree is connected/disconnected, the remote accessibility registration
    // needs to be updated with the pid of the remote process. If the process is going
    // away, that information is not present in WebProcess
    pid_t pid = 0;
    if (registerProcess)
        pid = m_page->legacyMainFrameProcess().processID();
    else if (!registerProcess) {
        pid = [m_remoteAccessibilityChild processIdentifier];
        m_remoteAccessibilityChild = nil;
        m_remoteAccessibilityChildToken = nil;
    }
    if (!pid)
        return;

    if (registerProcess) {
        [NSAccessibilityRemoteUIElement registerRemoteUIProcessIdentifier:pid];
        m_registeredRemoteAccessibilityPids.add(pid);
    } else {
        [NSAccessibilityRemoteUIElement unregisterRemoteUIProcessIdentifier:pid];
        m_registeredRemoteAccessibilityPids.remove(pid);
    }
}

void WebViewImpl::accessibilityRegisterUIProcessTokens()
{
    // Initialize remote accessibility when the window connection has been established.
    RetainPtr remoteElementToken = [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:m_view.get().get()];
    m_remoteAccessibilityTokenGeneratedByUIProcess = remoteElementToken.get();
    RetainPtr remoteWindowToken = [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:retainPtr([m_view.get() window]).get()];
    m_page->registerUIProcessAccessibilityTokens(span(remoteElementToken.get()), span(remoteWindowToken.get()));
}

id WebViewImpl::accessibilityFocusedUIElement()
{
    enableAccessibilityIfNecessary();
    return remoteAccessibilityChildIfNotSuspended().unsafeGet();
}

id WebViewImpl::accessibilityHitTest(CGPoint)
{
    return accessibilityFocusedUIElement();
}

void WebViewImpl::enableAccessibilityIfNecessary(NSString *attribute)
{
#if ENABLE(INITIALIZE_ACCESSIBILITY_ON_DEMAND)
    // The attributes NSAccessibilityParentAttribute and NSAccessibilityPositionAttribute do not require AX initialization in the WebContent process.
    if (![attribute isEqualToString:NSAccessibilityParentAttribute] && ![attribute isEqualToString:NSAccessibilityPositionAttribute]) {
        Ref processPool = m_page->configuration().processPool();
        processPool->initializeAccessibilityIfNecessary();
    }
#endif

    if (WebCore::AXObjectCache::accessibilityEnabled())
        return;

    // After enabling accessibility update the window frame on the web process so that the
    // correct accessibility position is transmitted (when AX is off, that position is not calculated).
    WebCore::AXObjectCache::enableAccessibility();
    updateWindowAndViewFrames();
}

id WebViewImpl::accessibilityAttributeValue(NSString *attribute, id parameter)
{
    enableAccessibilityIfNecessary(attribute);

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {

        RetainPtr<id> child;
        if (m_warningView)
            child = m_warningView.get();
        else if ((child = remoteAccessibilityChildIfNotSuspended().get())) {
#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
            m_page->takeAccessibilityActivityWhenInWindow();
#endif
        }

        if (!child)
            return nil;
        return @[child.get()];
    }
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return NSAccessibilityGroupRole;
    if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
        return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, nil);
    if ([attribute isEqualToString:NSAccessibilityParentAttribute])
        return NSAccessibilityUnignoredAncestor(retainPtr([m_view.get() superview]).get());
    if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
        return @YES;

    if ([attribute isEqualToString:@"AXConvertRelativeFrame"]) {
        if ([parameter isKindOfClass:[NSValue class]]) {
            NSRect rect = [(NSValue *)parameter rectValue];
            return [NSValue valueWithRect:m_pageClient->rootViewToScreen(WebCore::IntRect(rect))];
        }
    }

    return [m_view.get() _web_superAccessibilityAttributeValue:attribute];
}

RetainPtr<NSAccessibilityRemoteUIElement> WebViewImpl::remoteAccessibilityChildIfNotSuspended()
{
    if (m_page->legacyMainFrameProcess().throttler().isSuspended())
        return nil;
    return m_remoteAccessibilityChild.get();
}

void WebViewImpl::updatePrimaryTrackingAreaOptions(NSTrackingAreaOptions options)
{
    auto trackingArea = adoptNS([[NSTrackingArea alloc] initWithRect:[m_view.get() frame] options:options owner:m_mouseTrackingObserver.get() userInfo:nil]);
    [m_view.get() removeTrackingArea:m_primaryTrackingArea.get()];
    m_primaryTrackingArea = trackingArea;
    [m_view.get() addTrackingArea:trackingArea.get()];

}

// Any non-zero value will do, but using something recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

NSTrackingRectTag WebViewImpl::addTrackingRect(CGRect, id owner, void* userData, bool assumeInside)
{
    ASSERT(!m_trackingRectOwner);
    m_trackingRectOwner = owner;
    m_trackingRectUserData = userData;
    return TRACKING_RECT_TAG;
}

NSTrackingRectTag WebViewImpl::addTrackingRectWithTrackingNum(CGRect, id owner, void* userData, bool assumeInside, int tag)
{
    ASSERT(tag == 0 || tag == TRACKING_RECT_TAG);
    ASSERT(!m_trackingRectOwner);
    m_trackingRectOwner = owner;
    m_trackingRectUserData = userData;
    return TRACKING_RECT_TAG;
}

void WebViewImpl::addTrackingRectsWithTrackingNums(Vector<CGRect> cgRects, id owner, void** userDataList, bool assumeInside, NSTrackingRectTag *trackingNums)
{
    ASSERT_UNUSED(cgRects, cgRects.size() == 1);
    ASSERT(trackingNums[0] == 0 || trackingNums[0] == TRACKING_RECT_TAG);
    ASSERT(!m_trackingRectOwner);
    m_trackingRectOwner = owner;
    m_trackingRectUserData = userDataList[0];
    trackingNums[0] = TRACKING_RECT_TAG;
}

void WebViewImpl::removeTrackingRect(NSTrackingRectTag tag)
{
    if (tag == 0)
        return;

    if (tag == TRACKING_RECT_TAG) {
        m_trackingRectOwner = nil;
        return;
    }

    if (tag == m_lastToolTipTag) {
        [m_view.get() _web_superRemoveTrackingRect:tag];
        m_lastToolTipTag = 0;
        return;
    }

    // If any other tracking rect is being removed, we don't know how it was created
    // and it's possible there's a leak involved (see 3500217)
    ASSERT_NOT_REACHED();
}

void WebViewImpl::removeTrackingRects(std::span<NSTrackingRectTag> tags)
{
    for (auto& tag : tags) {
        if (tag == 0)
            continue;
        ASSERT(tag == TRACKING_RECT_TAG);
        m_trackingRectOwner = nil;
    }
}

RetainPtr<id> WebViewImpl::toolTipOwnerForSendingMouseEvents() const
{
    if (RetainPtr<id> owner = m_trackingRectOwner.get())
        return owner;

    for (NSTrackingArea *trackingArea in protectedView().get().trackingAreas) {
        static Class managerClass = NSClassFromString(@"NSToolTipManager");
        RetainPtr<id> owner = trackingArea.owner;
        if ([owner class] == managerClass)
            return owner;
    }
    return nil;
}

void WebViewImpl::sendToolTipMouseExited()
{
    // Nothing matters except window, trackingNumber, and userData.
    RetainPtr fakeEvent = [NSEvent enterExitEventWithType:NSEventTypeMouseExited
        location:NSZeroPoint
        modifierFlags:0
        timestamp:0
        windowNumber:protectedWindow().get().windowNumber
        context:nil
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:m_trackingRectUserData];
    [toolTipOwnerForSendingMouseEvents() mouseExited:fakeEvent.get()];
}

void WebViewImpl::sendToolTipMouseEntered()
{
    // Nothing matters except window, trackingNumber, and userData.
    RetainPtr fakeEvent = [NSEvent enterExitEventWithType:NSEventTypeMouseEntered
        location:NSZeroPoint
        modifierFlags:0
        timestamp:0
        windowNumber:protectedWindow().get().windowNumber
        context:nil
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:m_trackingRectUserData];
    [toolTipOwnerForSendingMouseEvents() mouseEntered:fakeEvent.get()];
}

NSString *WebViewImpl::stringForToolTip(NSToolTipTag tag)
{
    return m_page->toolTip().createNSString().autorelease();
}

void WebViewImpl::toolTipChanged(const String& oldToolTip, const String& newToolTip)
{
    if (!oldToolTip.isNull())
        sendToolTipMouseExited();

    if (!newToolTip.isEmpty()) {
        // See radar 3500217 for why we remove all tooltips rather than just the single one we created.
        [m_view.get() removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        m_lastToolTipTag = [m_view.get() addToolTipRect:wideOpenRect owner:m_view.get().get() userData:NULL];
        sendToolTipMouseEntered();
    }
}

void WebViewImpl::enterAcceleratedCompositingWithRootLayer(CALayer *rootLayer)
{
    // This is the process-swap case. We add the new layer behind the existing root layer and mark it as hidden.
    // This way, the new layer gets accelerated compositing but won't be visible until
    // setAcceleratedCompositingRootLayer() is called by didFirstLayerFlush(), in order to prevent flashing.
    if (m_rootLayer && rootLayer && m_rootLayer != rootLayer) {
        if (m_thumbnailView)
            return;

        [rootLayer web_disableAllActions];

        [CATransaction begin];
        [CATransaction setDisableActions:YES];

        rootLayer.hidden = YES;
        [retainPtr([m_layerHostingView layer]) insertSublayer:rootLayer atIndex:0];

        [CATransaction commit];
        return;
    }

    setAcceleratedCompositingRootLayer(rootLayer);
}

void WebViewImpl::setAcceleratedCompositingRootLayer(CALayer *rootLayer)
{
    [rootLayer web_disableAllActions];

    m_rootLayer = rootLayer;
    rootLayer.hidden = NO;

    if (m_thumbnailView && updateThumbnailViewLayer())
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_layerHostingView layer].sublayers = rootLayer ? @[ rootLayer ] : nil;

    [CATransaction commit];

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    updateScrollPocket();
#endif
}

void WebViewImpl::setHeaderBannerLayer(CALayer *headerBannerLayer)
{
    if (m_headerBannerLayer)
        [m_headerBannerLayer removeFromSuperlayer];

    m_headerBannerLayer = headerBannerLayer;

    // If WebPage has not been created yet, WebPageCreationParameters.headerBannerHeight
    // will be used to adjust the page content size.
    if (page().hasRunningProcess()) {
        int headerBannerHeight = headerBannerLayer ? headerBannerLayer.frame.size.height : 0;
        page().setHeaderBannerHeight(headerBannerHeight);
    }
}

void WebViewImpl::setFooterBannerLayer(CALayer *footerBannerLayer)
{
    if (m_footerBannerLayer)
        [m_footerBannerLayer removeFromSuperlayer];

    m_footerBannerLayer = footerBannerLayer;

    // If WebPage has not been created yet, WebPageCreationParameters.footerBannerHeight
    // will be used to adjust the page content size.
    if (page().hasRunningProcess()) {
        int footerBannerHeight = footerBannerLayer ? footerBannerLayer.frame.size.height : 0;
        page().setFooterBannerHeight(footerBannerHeight);
    }
}

void WebViewImpl::setThumbnailView(_WKThumbnailView *thumbnailView)
{
    ASSERT(!m_thumbnailView || !thumbnailView);

    m_thumbnailView = thumbnailView;

    if (thumbnailView)
        updateThumbnailViewLayer();
    else {
        setAcceleratedCompositingRootLayer(m_rootLayer.get());
        m_page->activityStateDidChange(WebCore::allActivityStates());
    }
}

void WebViewImpl::reparentLayerTreeInThumbnailView()
{
    [m_thumbnailView.get() _setThumbnailLayer: m_rootLayer.get()];
}

bool WebViewImpl::updateThumbnailViewLayer()
{
    RetainPtr thumbnailView = m_thumbnailView.get();
    ASSERT(thumbnailView);

    if ([thumbnailView _waitingForSnapshot] && window()) {
        reparentLayerTreeInThumbnailView();
        return true;
    }

    return false;
}

void WebViewImpl::setInspectorAttachmentView(NSView *newView)
{
    RetainPtr oldView = m_inspectorAttachmentView.get();
    if (oldView.get() == newView)
        return;

    m_inspectorAttachmentView = newView;
    
    if (RefPtr inspector = m_page->inspector())
        inspector->attachmentViewDidChange(oldView ? oldView.get() : m_view.get().get(), newView ? RetainPtr { newView }.get() : m_view.get().get());
}

RetainPtr<NSView> WebViewImpl::inspectorAttachmentView()
{
    if (RetainPtr attachmentView = m_inspectorAttachmentView.get())
        return attachmentView;
    return m_view.get();
}

_WKRemoteObjectRegistry *WebViewImpl::remoteObjectRegistry()
{
    if (!m_remoteObjectRegistry) {
        m_remoteObjectRegistry = adoptNS([[_WKRemoteObjectRegistry alloc] _initWithWebPageProxy:m_page.get()]);
        Ref webRemoteObjectRegistry = [m_remoteObjectRegistry remoteObjectRegistry];
        m_page->configuration().protectedProcessPool()->addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), m_page->identifier(), webRemoteObjectRegistry);
    }

    return m_remoteObjectRegistry.get();
}

#if ENABLE(DRAG_SUPPORT)
void WebViewImpl::draggedImage(NSImage *, CGPoint endPoint, NSDragOperation operation)
{
    sendDragEndToPage(endPoint, operation);
}

void WebViewImpl::sendDragEndToPage(CGPoint endPoint, NSDragOperation dragOperationMask)
{
    NSPoint windowImageLoc = [protectedWindow() convertPointFromScreen:NSPointFromCGPoint(endPoint)];
    NSPoint windowMouseLoc = windowImageLoc;

    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    m_ignoresMouseDraggedEvents = true;

    m_page->dragEnded(WebCore::IntPoint(windowMouseLoc), WebCore::IntPoint(WebCore::globalPoint(windowMouseLoc, protectedWindow().get())), coreDragOperationMask(dragOperationMask));
}

static OptionSet<WebCore::DragApplicationFlags> applicationFlagsForDrag(NSView *view, id<NSDraggingInfo> draggingInfo)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    OptionSet<WebCore::DragApplicationFlags> flags;
    if ([NSApp modalWindow])
        flags.add(WebCore::DragApplicationFlags::IsModal);
    if (view.window.attachedSheet)
        flags.add(WebCore::DragApplicationFlags::HasAttachedSheet);
    if (draggingInfo.draggingSource == view)
        flags.add(WebCore::DragApplicationFlags::IsSource);
    if ([NSApp currentEvent].modifierFlags & NSEventModifierFlagOption)
        flags.add(WebCore::DragApplicationFlags::IsCopyKeyDown);
    return flags;
}

NSDragOperation WebViewImpl::draggingEntered(id<NSDraggingInfo> draggingInfo)
{
    RetainPtr view = m_view.get();
    WebCore::IntPoint client([view convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, retainPtr([view window]).get()));
    auto dragDestinationActionMask = coreDragDestinationActionMask([view _web_dragDestinationActionForDraggingInfo:draggingInfo]);
    auto dragOperationMask = coreDragOperationMask(draggingInfo.draggingSourceOperationMask);
    WebCore::DragData dragData(draggingInfo, client, global, dragOperationMask, applicationFlagsForDrag(view.get(), draggingInfo), dragDestinationActionMask, m_page->webPageIDInMainFrameProcess());
    dragData.setPromisedFileMIMETypes([view _promisedFileMIMETypes:draggingInfo]);

    m_page->resetCurrentDragInformation();
    m_page->dragEntered(dragData, draggingInfo.draggingPasteboard.name);
    m_initialNumberOfValidItemsForDrop = draggingInfo.numberOfValidItemsForDrop;
    return NSDragOperationCopy;
}

static NSDragOperation kit(std::optional<WebCore::DragOperation> dragOperation)
{
    if (!dragOperation)
        return NSDragOperationNone;

    switch (*dragOperation) {
    case WebCore::DragOperation::Copy:
        return NSDragOperationCopy;
    case WebCore::DragOperation::Link:
        return NSDragOperationLink;
    case WebCore::DragOperation::Generic:
        return NSDragOperationGeneric;
    case WebCore::DragOperation::Private:
        return NSDragOperationPrivate;
    case WebCore::DragOperation::Move:
        return NSDragOperationMove;
    case WebCore::DragOperation::Delete:
        return NSDragOperationDelete;
    }

    ASSERT_NOT_REACHED();
    return NSDragOperationNone;
}

NSDragOperation WebViewImpl::draggingUpdated(id<NSDraggingInfo> draggingInfo)
{
    RetainPtr view = m_view.get();
    WebCore::IntPoint client([view convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, retainPtr([view window]).get()));
    auto dragDestinationActionMask = coreDragDestinationActionMask([view _web_dragDestinationActionForDraggingInfo:draggingInfo]);
    auto dragOperationMask = coreDragOperationMask(draggingInfo.draggingSourceOperationMask);
    WebCore::DragData dragData(draggingInfo, client, global, dragOperationMask, applicationFlagsForDrag(view.get(), draggingInfo), dragDestinationActionMask, m_page->webPageIDInMainFrameProcess());
    dragData.setPromisedFileMIMETypes([view _promisedFileMIMETypes:draggingInfo]);
    m_page->dragUpdated(dragData, draggingInfo.draggingPasteboard.name);

    NSInteger numberOfValidItemsForDrop = m_page->currentDragNumberOfFilesToBeAccepted();

    if (!m_page->currentDragOperation())
        numberOfValidItemsForDrop = m_initialNumberOfValidItemsForDrop;

    NSDraggingFormation draggingFormation = NSDraggingFormationNone;
    if (m_page->currentDragIsOverFileInput() && numberOfValidItemsForDrop > 0)
        draggingFormation = NSDraggingFormationList;

    if (draggingInfo.numberOfValidItemsForDrop != numberOfValidItemsForDrop)
        [draggingInfo setNumberOfValidItemsForDrop:numberOfValidItemsForDrop];
    if (draggingInfo.draggingFormation != draggingFormation)
        [draggingInfo setDraggingFormation:draggingFormation];

    return kit(m_page->currentDragOperation());
}

void WebViewImpl::draggingExited(id<NSDraggingInfo> draggingInfo)
{
    WebCore::IntPoint client([m_view.get() convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, retainPtr([m_view.get() window]).get()));
    WebCore::DragData dragData(draggingInfo, client, global, coreDragOperationMask(draggingInfo.draggingSourceOperationMask), applicationFlagsForDrag(m_view.get().get(), draggingInfo), WebCore::anyDragDestinationAction(), m_page->webPageIDInMainFrameProcess());
    m_page->dragExited(dragData);
    m_page->resetCurrentDragInformation();
    draggingInfo.numberOfValidItemsForDrop = m_initialNumberOfValidItemsForDrop;
    m_initialNumberOfValidItemsForDrop = 0;
}

bool WebViewImpl::prepareForDragOperation(id<NSDraggingInfo>)
{
    return true;
}

static void performDragWithLegacyFiles(WebPageProxy& page, Box<Vector<String>>&& fileNames, Box<WebCore::DragData>&& dragData, const String& pasteboardName)
{
    RefPtr networkProcess = page.websiteDataStore().networkProcessIfExists();
    if (!networkProcess)
        return;
    networkProcess->sendWithAsyncReply(Messages::NetworkProcess::AllowFilesAccessFromWebProcess(page.protectedLegacyMainFrameProcess()->coreProcessIdentifier(), *fileNames), [page = Ref { page }, fileNames, dragData, pasteboardName]() mutable {
        SandboxExtension::Handle sandboxExtensionHandle;
        Vector<SandboxExtension::Handle> sandboxExtensionForUpload;

        page->createSandboxExtensionsIfNeeded(*fileNames, sandboxExtensionHandle, sandboxExtensionForUpload);
        dragData->setFileNames(*fileNames);
        page->performDragOperation(*dragData, pasteboardName, WTFMove(sandboxExtensionHandle), WTFMove(sandboxExtensionForUpload));
    });
}

static bool handleLegacyFilesPromisePasteboard(id<NSDraggingInfo> draggingInfo, Box<WebCore::DragData>&& dragData, WebPageProxy& page, RetainPtr<NSView<WebViewImplDelegate>> view)
{
    // FIXME: legacyFilesPromisePasteboardTypeSingleton() contains UTIs, not path names. Also, it's not
    // guaranteed that the count of UTIs equals the count of files, since some clients only write
    // unique UTIs.
    RetainPtr files = dynamic_objc_cast<NSArray>([retainPtr(draggingInfo.draggingPasteboard) propertyListForType:WebCore::legacyFilesPromisePasteboardTypeSingleton()]);
    if (!files)
        return false;

    RetainPtr dropDestinationPath = FileSystem::createTemporaryDirectory(@"WebKitDropDestination");
    if (!dropDestinationPath)
        return false;

    size_t fileCount = files.get().count;
    auto fileNames = Box<Vector<String>>::create();
    RetainPtr dropDestination = [NSURL fileURLWithPath:dropDestinationPath.get() isDirectory:YES];
    String pasteboardName = draggingInfo.draggingPasteboard.name;
    Ref protectedPage { page };
    [draggingInfo enumerateDraggingItemsWithOptions:0 forView:view.get() classes:@[NSFilePromiseReceiver.class] searchOptions:@{ } usingBlock:[&](NSDraggingItem *draggingItem, NSInteger idx, BOOL *stop) {
        auto queue = adoptNS([NSOperationQueue new]);
        [retainPtr(draggingItem.item) receivePromisedFilesAtDestination:dropDestination.get() options:@{ } operationQueue:queue.get() reader:[protectedPage, fileNames, fileCount, dragData, pasteboardName](NSURL *fileURL, NSError *errorOrNil) mutable {
            if (errorOrNil)
                return;

            RunLoop::mainSingleton().dispatch([protectedPage = WTFMove(protectedPage), path = RetainPtr { fileURL.path }, fileNames, fileCount, dragData, pasteboardName] () mutable {
                fileNames->append(path.get());
                if (fileNames->size() != fileCount)
                    return;
                performDragWithLegacyFiles(protectedPage, WTFMove(fileNames), WTFMove(dragData), pasteboardName);
            });
        }];
    }];

    return true;
}

static bool handleLegacyFilesPasteboard(id<NSDraggingInfo> draggingInfo, Box<WebCore::DragData>&& dragData, WebPageProxy& page)
{
    RetainPtr files = dynamic_objc_cast<NSArray>([retainPtr(draggingInfo.draggingPasteboard) propertyListForType:WebCore::legacyFilenamesPasteboardTypeSingleton()]);
    if (!files)
        return false;

    String pasteboardName = draggingInfo.draggingPasteboard.name;

    RetainPtr originalFileURLs = adoptNS([[NSMutableArray alloc] initWithCapacity:[files count]]);
    for (NSString *file in files.get())
        [originalFileURLs addObject:adoptNS([[NSURL alloc] initFileURLWithPath:file]).get()];

    auto task = makeBlockPtr([protectedPage = Ref { page }, originalFileURLs, dragData, pasteboardName = pasteboardName.isolatedCopy()] mutable {
        ASSERT(!RunLoop::isMain());

        RetainPtr coordinator = adoptNS([[NSFileCoordinator alloc] initWithFilePresenter:nil]);

        NSError *prepareError = nil;
        [coordinator prepareForReadingItemsAtURLs:originalFileURLs.get() options:0 writingItemsAtURLs:@[] options:0 error:&prepareError byAccessor:[coordinator, originalFileURLs, protectedPage = WTFMove(protectedPage), dragData, pasteboardName](void (^completionHandler)(void)) mutable {
            auto fileNames = Box<Vector<String>>::create();

            for (NSURL *originalFileURL in originalFileURLs.get()) {
                NSError *error = nil;
                [coordinator coordinateReadingItemAtURL:originalFileURL options:NSFileCoordinatorReadingWithoutChanges error:&error byAccessor:[fileNames](NSURL *newURL) {
                    fileNames->append(newURL.path);
                }];

                RELEASE_LOG_ERROR_IF(error, DragAndDrop, "Failed to coordinate reading file: %@.", error.localizedDescription);
            }

            RunLoop::mainSingleton().dispatch([protectedPage = WTFMove(protectedPage), fileNames, dragData, pasteboardName, completionHandler = makeBlockPtr(completionHandler)] mutable {
                performDragWithLegacyFiles(protectedPage, WTFMove(fileNames), WTFMove(dragData), pasteboardName);
                completionHandler();
            });
        }];

        RELEASE_LOG_ERROR_IF(prepareError, DragAndDrop, "Failed to prepare for reading files with error: %@.", prepareError.localizedDescription);
    });

    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), task.get());
    return true;
}

bool WebViewImpl::performDragOperation(id<NSDraggingInfo> draggingInfo)
{
    WebCore::IntPoint client([m_view.get() convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, retainPtr([m_view.get() window]).get()));
    auto dragData = Box<WebCore::DragData>::create(draggingInfo, client, global, coreDragOperationMask(draggingInfo.draggingSourceOperationMask), applicationFlagsForDrag(m_view.get().get(), draggingInfo), WebCore::anyDragDestinationAction(), m_page->webPageIDInMainFrameProcess());

    RetainPtr<NSArray> types = draggingInfo.draggingPasteboard.types;
    SandboxExtension::Handle sandboxExtensionHandle;
    Vector<SandboxExtension::Handle> sandboxExtensionForUpload;

    if (![types containsObject:PasteboardTypes::WebArchivePboardType] && [types containsObject:WebCore::legacyFilesPromisePasteboardTypeSingleton()])
        return handleLegacyFilesPromisePasteboard(draggingInfo, WTFMove(dragData), page(), m_view.get());

    if ([types containsObject:WebCore::legacyFilenamesPasteboardTypeSingleton()])
        return handleLegacyFilesPasteboard(draggingInfo, WTFMove(dragData), page());

    String draggingPasteboardName = draggingInfo.draggingPasteboard.name;
    m_page->performDragOperation(*dragData, draggingPasteboardName, WTFMove(sandboxExtensionHandle), WTFMove(sandboxExtensionForUpload));

    return true;
}

NSView *WebViewImpl::hitTestForDragTypes(CGPoint point, NSSet *types)
{
    // This code is needed to support drag and drop when the drag types cannot be matched.
    // This is the case for elements that do not place content
    // in the drag pasteboard automatically when the drag start (i.e. dragging a DIV element).
    if ([retainPtr([m_view.get() superview]) mouse:NSPointFromCGPoint(point) inRect:[m_view.get() frame]])
        return m_view.getAutoreleased();
    return nil;
}

void WebViewImpl::registerDraggedTypes()
{
    auto types = adoptNS([[NSMutableSet alloc] initWithArray:PasteboardTypes::forEditingSingleton()]);
    [types addObjectsFromArray:PasteboardTypes::forURLSingleton()];
    [types addObject:PasteboardTypes::WebDummyPboardType];
    [m_view.get() registerForDraggedTypes:retainPtr([types allObjects]).get()];
}

NSString *WebViewImpl::fileNameForFilePromiseProvider(NSFilePromiseProvider *provider, NSString *)
{
    RetainPtr userInfo = dynamic_objc_cast<WKPromisedAttachmentContext>(provider.userInfo);
    if (!userInfo)
        return nil;

    return [userInfo fileName];
}

static NSError *webKitUnknownError()
{
    return [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil];
}

void WebViewImpl::didPerformDragOperation(bool handled)
{
    [m_view.get() _web_didPerformDragOperation:handled];
}

void WebViewImpl::writeToURLForFilePromiseProvider(NSFilePromiseProvider *provider, NSURL *fileURL, void(^completionHandler)(NSError *))
{
    RetainPtr userInfo = dynamic_objc_cast<WKPromisedAttachmentContext>(provider.userInfo);
    if (!userInfo) {
        completionHandler(webKitUnknownError());
        return;
    }

    if (auto attachment = m_page->attachmentForIdentifier(userInfo.get().attachmentIdentifier)) {
        NSError *attachmentWritingError = nil;
        attachment->doWithFileWrapper([&](NSFileWrapper *fileWrapper) {
            if ([fileWrapper writeToURL:fileURL options:0 originalContentsURL:nil error:&attachmentWritingError])
                completionHandler(nil);
            else
                completionHandler(attachmentWritingError);
        });
        return;
    }

    completionHandler(webKitUnknownError());
}

NSDragOperation WebViewImpl::dragSourceOperationMask(NSDraggingSession *, NSDraggingContext context)
{
    if (context == NSDraggingContextOutsideApplication || m_page->currentDragIsOverFileInput())
        return NSDragOperationCopy;
    return NSDragOperationGeneric | NSDragOperationMove | NSDragOperationCopy;
}

void WebViewImpl::draggingSessionEnded(NSDraggingSession *, NSPoint endPoint, NSDragOperation operation)
{
    sendDragEndToPage(NSPointToCGPoint(endPoint), operation);
}

#endif // ENABLE(DRAG_SUPPORT)

void WebViewImpl::startWindowDrag()
{
    [protectedWindow() performWindowDragWithEvent:m_lastMouseDownEvent.get()];
}

void WebViewImpl::startDrag(const WebCore::DragItem& item, ShareableBitmap::Handle&& dragImageHandle)
{
    auto dragImageAsBitmap = ShareableBitmap::create(WTFMove(dragImageHandle));
    if (!dragImageAsBitmap) {
        m_page->dragCancelled();
        return;
    }

    RetainPtr dragCGImage = dragImageAsBitmap->createPlatformImage(DontCopyBackingStore);
    auto dragNSImage = adoptNS([[NSImage alloc] initWithCGImage:dragCGImage.get() size:dragImageAsBitmap->size()]);

    WebCore::IntSize size([dragNSImage size]);
    size.scale(1.0 / m_page->deviceScaleFactor());
    [dragNSImage setSize:size];

    // The call below could release the view.
    auto protector = m_view.get();

    if (RefPtr frame = WebFrameProxy::webFrame(item.rootFrameID)) {
        m_page->convertPointToMainFrameCoordinates(item.dragLocationInWindowCoordinates, item.rootFrameID, [weakThis = WeakPtr { *this }, promisedAttachmentInfo = item.promisedAttachmentInfo, dragNSImage = WTFMove(dragNSImage), size, lastMouseDownEvent = m_lastMouseDownEvent] (std::optional<FloatPoint> dragLocationInMainFrameCoordinates) mutable {
            CheckedPtr protectedThis = weakThis.get();
            if (!protectedThis || !dragLocationInMainFrameCoordinates)
                return;
            RefPtr page = protectedThis->page();
            RetainPtr view = protectedThis->view();

            auto clientDragLocation = IntPoint(dragLocationInMainFrameCoordinates.value());

            RetainPtr pasteboard = [NSPasteboard pasteboardWithName:NSPasteboardNameDrag];

            if (promisedAttachmentInfo) {
                RefPtr attachment = page->attachmentForIdentifier(promisedAttachmentInfo.attachmentIdentifier);
                if (!attachment) {
                    page->dragCancelled();
                    return;
                }

                RetainPtr utiType = attachment->utiType().createNSString();
                if (![utiType length]) {
                    page->dragCancelled();
                    return;
                }

                RetainPtr fileName = attachment->fileName().createNSString();
                RetainPtr provider = adoptNS([[NSFilePromiseProvider alloc] initWithFileType:utiType.get() delegate:(id<NSFilePromiseProviderDelegate>)view.get()]);
                RetainPtr context = adoptNS([[WKPromisedAttachmentContext alloc] initWithIdentifier:promisedAttachmentInfo.attachmentIdentifier.createNSString().get() fileName:fileName.get()]);
                [provider setUserInfo:context.get()];

                RetainPtr draggingItem = adoptNS([[NSDraggingItem alloc] initWithPasteboardWriter:provider.get()]);
                [draggingItem setDraggingFrame:NSMakeRect(clientDragLocation.x(), clientDragLocation.y() - size.height(), size.width(), size.height()) contents:dragNSImage.get()];

                if (!lastMouseDownEvent) {
                    page->dragCancelled();
                    return;
                }

                [view beginDraggingSessionWithItems:@[draggingItem.get()] event:lastMouseDownEvent.get() source:(id<NSDraggingSource>)view.get()];

                for (size_t index = 0; index < promisedAttachmentInfo.additionalTypesAndData.size(); ++index) {
                    RetainPtr nsData = Ref { *promisedAttachmentInfo.additionalTypesAndData[index].second }->createNSData();
                    [pasteboard setData:nsData.get() forType:promisedAttachmentInfo.additionalTypesAndData[index].first.createNSString().get()];
                }
                page->didStartDrag();
                return;
            }
            page->didStartDrag();

            [pasteboard setString:@"" forType:PasteboardTypes::WebDummyPboardType];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [view dragImage:dragNSImage.get() at:NSPointFromCGPoint(clientDragLocation) offset:NSZeroSize event:lastMouseDownEvent.get() pasteboard:pasteboard.get() source:view.get() slideBack:YES];
        ALLOW_DEPRECATED_DECLARATIONS_END
        });
    }
}

static bool matchesExtensionOrEquivalent(const String& filename, const String& extension)
{
    return filename.endsWithIgnoringASCIICase(makeString('.', extension))
        || (equalLettersIgnoringASCIICase(extension, "jpeg"_s) && filename.endsWithIgnoringASCIICase(".jpg"_s));
}

void WebViewImpl::setFileAndURLTypes(NSString *filename, NSString *extension, NSString* uti, NSString *title, NSString *url, NSString *visibleURL, NSPasteboard *pasteboard)
{
    RetainPtr<NSString> filenameWithExtension;
    if (matchesExtensionOrEquivalent(filename, extension))
        filenameWithExtension = filename;
    else
        filenameWithExtension = [retainPtr([filename stringByAppendingString:@"."]) stringByAppendingString:extension];

    [pasteboard setString:visibleURL forType:WebCore::legacyStringPasteboardTypeSingleton()];
    [pasteboard setString:visibleURL forType:PasteboardTypes::WebURLPboardType];
    [pasteboard setString:title forType:PasteboardTypes::WebURLNamePboardType];
    [pasteboard setPropertyList:@[@[visibleURL], @[title]] forType:PasteboardTypes::WebURLsWithTitlesPboardType];
    [pasteboard setPropertyList:@[uti] forType:WebCore::legacyFilesPromisePasteboardTypeSingleton()];
    m_promisedFilename = filenameWithExtension.get();
    m_promisedURL = url;
}

void WebViewImpl::setPromisedDataForImage(WebCore::Image& image, NSString *filename, NSString *extension, NSString *title, NSString *url, NSString *visibleURL, WebCore::FragmentedSharedBuffer* archiveBuffer, NSString *pasteboardName, NSString *originIdentifier)
{
    RetainPtr pasteboard = [NSPasteboard pasteboardWithName:pasteboardName];
    RetainPtr types = adoptNS([[NSMutableArray alloc] initWithObjects:WebCore::legacyFilesPromisePasteboardTypeSingleton(), nil]);

    auto uti = image.uti();
    if (!uti.isEmpty() && image.data() && !image.data()->isEmpty())
        [types addObject:uti.createNSString().get()];

    RetainPtr<NSData> customDataBuffer;
    if (originIdentifier.length) {
        [types addObject:RetainPtr { @(WebCore::PasteboardCustomData::cocoaType().characters()) }.get()];
        WebCore::PasteboardCustomData customData;
        customData.setOrigin(originIdentifier);
        customDataBuffer = customData.createSharedBuffer()->createNSData();
    }

    [types addObjectsFromArray:RetainPtr { archiveBuffer ? PasteboardTypes::forImagesWithArchiveSingleton() : PasteboardTypes::forImagesSingleton() }.get()];

    [pasteboard clearContents];
    if (m_page->sessionID().isEphemeral())
        [pasteboard _setExpirationDate:[NSDate dateWithTimeIntervalSinceNow:pasteboardExpirationDelay.seconds()]];
    [pasteboard addTypes:types.get() owner:m_view.get().get()];
    setFileAndURLTypes(filename, extension, uti.createNSString().get(), title, url, visibleURL, pasteboard.get());

    if (archiveBuffer) {
        auto nsData = archiveBuffer->makeContiguous()->createNSData();
        [pasteboard setData:nsData.get() forType:UTTypeWebArchive.identifier];
        [pasteboard setData:nsData.get() forType:PasteboardTypes::WebArchivePboardType];
    }

    if (customDataBuffer)
        [pasteboard setData:customDataBuffer.get() forType:@(WebCore::PasteboardCustomData::cocoaType().characters())];

    m_promisedImage = image;
}

void WebViewImpl::clearPromisedDragImage()
{
    m_promisedImage = nullptr;
}

void WebViewImpl::pasteboardChangedOwner(NSPasteboard *pasteboard)
{
    clearPromisedDragImage();
    m_promisedFilename = emptyString();
    m_promisedURL = emptyString();
}

void WebViewImpl::provideDataForPasteboard(NSPasteboard *pasteboard, NSString *type)
{
    RefPtr promisedImage = m_promisedImage;
    if (!promisedImage)
        return;

    if ([type isEqual:promisedImage->uti().createNSString().get()] && promisedImage->data()) {
        if (auto platformData = promisedImage->protectedData()->makeContiguous()->createNSData())
            [pasteboard setData:(__bridge NSData *)platformData.get() forType:type];
    }

    // FIXME: Need to support NSRTFDPboardType.
    if ([type isEqual:WebCore::legacyTIFFPasteboardTypeSingleton()])
        [pasteboard setData:(__bridge NSData *)RetainPtr { promisedImage->adapter().tiffRepresentation() }.get() forType:WebCore::legacyTIFFPasteboardTypeSingleton()];
}

static BOOL fileExists(NSString *path)
{
    struct stat statBuffer;
    return !lstat([path fileSystemRepresentation], &statBuffer);
}

static RetainPtr<NSString> pathWithUniqueFilenameForPath(NSString *path)
{
    // "Fix" the filename of the path.
    RetainPtr filename = filenameByFixingIllegalCharacters(retainPtr([path lastPathComponent]).get());
    RetainPtr updatedPath = [retainPtr([path stringByDeletingLastPathComponent]) stringByAppendingPathComponent:filename.get()];

    if (fileExists(updatedPath.get())) {
        // Don't overwrite existing file by appending "-n", "-n.ext" or "-n.ext.ext" to the filename.
        RetainPtr<NSString> extensions;
        RetainPtr<NSString> pathWithoutExtensions;
        RetainPtr<NSString> lastPathComponent = [updatedPath lastPathComponent];
        NSRange periodRange = [lastPathComponent rangeOfString:@"."];

        if (periodRange.location == NSNotFound) {
            pathWithoutExtensions = updatedPath;
        } else {
            extensions = [lastPathComponent substringFromIndex:periodRange.location + 1];
            lastPathComponent = [lastPathComponent substringToIndex:periodRange.location];
            pathWithoutExtensions = [retainPtr([updatedPath stringByDeletingLastPathComponent]) stringByAppendingPathComponent:lastPathComponent.get()];
        }

        for (unsigned i = 1; ; i++) {
            RetainPtr pathWithAppendedNumber = adoptNS([[NSString alloc] initWithFormat:@"%@-%d", pathWithoutExtensions.get(), i]);
            updatedPath = [extensions length] ? [pathWithAppendedNumber stringByAppendingPathExtension:extensions.get()] : pathWithAppendedNumber;
            if (!fileExists(updatedPath.get()))
                break;
        }
    }

    return updatedPath;
}

NSArray *WebViewImpl::namesOfPromisedFilesDroppedAtDestination(NSURL *dropDestination)
{
    RetainPtr<NSFileWrapper> wrapper;
    RetainPtr<NSData> data;

    if (RefPtr promisedImage = m_promisedImage) {
        data = promisedImage->protectedData()->makeContiguous()->createNSData();
        wrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data.get()]);
    } else
        wrapper = adoptNS([[NSFileWrapper alloc] initWithURL:adoptNS([[NSURL alloc] initWithString:m_promisedURL.createNSString().get()]).get() options:NSFileWrapperReadingImmediate error:nil]);

    if (wrapper)
        [wrapper setPreferredFilename:m_promisedFilename.createNSString().get()];
    else {
        LOG_ERROR("Failed to create image file.");
        return nil;
    }

    // FIXME: Report an error if we fail to create a file.
    RetainPtr<NSString> path = [retainPtr([dropDestination path]) stringByAppendingPathComponent:retainPtr([wrapper preferredFilename]).get()];
    path = pathWithUniqueFilenameForPath(path.get());
    if (![wrapper writeToURL:[NSURL fileURLWithPath:path.get() isDirectory:NO] options:NSFileWrapperWritingWithNameUpdating originalContentsURL:nil error:nullptr])
        LOG_ERROR("Failed to create image file via -[NSFileWrapper writeToURL:options:originalContentsURL:error:]");

    if (!m_promisedURL.isEmpty())
        FileSystem::setMetadataURL(String(path.get()), m_promisedURL);

    return @[[path lastPathComponent]];
}

static NSPasteboardName pasteboardNameForAccessCategory(WebCore::DOMPasteAccessCategory pasteAccessCategory)
{
    switch (pasteAccessCategory) {
    case WebCore::DOMPasteAccessCategory::General:
        return NSPasteboardNameGeneral;

    case WebCore::DOMPasteAccessCategory::Fonts:
        return NSPasteboardNameFont;
    }
}

static RetainPtr<NSPasteboard> pasteboardForAccessCategory(WebCore::DOMPasteAccessCategory pasteAccessCategory)
{
    switch (pasteAccessCategory) {
    case WebCore::DOMPasteAccessCategory::General:
        return NSPasteboard.generalPasteboard;

    case WebCore::DOMPasteAccessCategory::Fonts:
        return [NSPasteboard pasteboardWithName:NSPasteboardNameFont];
    }
}

void WebViewImpl::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory pasteAccessCategory, WebCore::DOMPasteRequiresInteraction requiresInteraction, const WebCore::IntRect&, const String& originIdentifier, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completion)
{
    ASSERT(!m_domPasteRequestHandler);
    hideDOMPasteMenuWithResult(WebCore::DOMPasteAccessResponse::DeniedForGesture);

    RetainPtr data = [pasteboardForAccessCategory(pasteAccessCategory).get() dataForType:RetainPtr { @(WebCore::PasteboardCustomData::cocoaType().characters()) }.get()];
    auto buffer = WebCore::SharedBuffer::create(data.get());
    if (requiresInteraction == WebCore::DOMPasteRequiresInteraction::No && WebCore::PasteboardCustomData::fromSharedBuffer(buffer.get()).origin() == originIdentifier) {
        m_page->grantAccessToCurrentPasteboardData(pasteboardNameForAccessCategory(pasteAccessCategory), [completion = WTFMove(completion)] () mutable {
            completion(WebCore::DOMPasteAccessResponse::GrantedForGesture);
        });
        return;
    }

    m_domPasteMenuDelegate = adoptNS([[WKDOMPasteMenuDelegate alloc] initWithWebViewImpl:*this pasteAccessCategory:pasteAccessCategory]);
    m_domPasteRequestHandler = WTFMove(completion);
    m_domPasteMenu = adoptNS([[NSMenu alloc] initWithTitle:WebCore::contextMenuItemTagPaste().createNSString().get()]);

    [m_domPasteMenu setDelegate:m_domPasteMenuDelegate.get()];
    [m_domPasteMenu setAllowsContextMenuPlugIns:NO];

    auto pasteMenuItem = RetainPtr([m_domPasteMenu insertItemWithTitle:WebCore::contextMenuItemTagPaste().createNSString().get() action:@selector(_web_grantDOMPasteAccess) keyEquivalent:@"" atIndex:0]);
    [pasteMenuItem setTarget:m_domPasteMenuDelegate.get()];

    RetainPtr window = [m_view.get() window];
    RetainPtr event = m_page->createSyntheticEventForContextMenu([window convertPointFromScreen:NSEvent.mouseLocation]);
    [NSMenu popUpContextMenu:m_domPasteMenu.get() withEvent:event.get() forView:retainPtr(window.get().contentView).get()];
}

void WebViewImpl::handleDOMPasteRequestForCategoryWithResult(WebCore::DOMPasteAccessCategory pasteAccessCategory, WebCore::DOMPasteAccessResponse response)
{
    if (response == WebCore::DOMPasteAccessResponse::GrantedForCommand || response == WebCore::DOMPasteAccessResponse::GrantedForGesture)
        m_page->grantAccessToCurrentPasteboardData(pasteboardNameForAccessCategory(pasteAccessCategory), [] () { });

    hideDOMPasteMenuWithResult(response);
}

void WebViewImpl::hideDOMPasteMenuWithResult(WebCore::DOMPasteAccessResponse response)
{
    if (auto handler = std::exchange(m_domPasteRequestHandler, { }))
        handler(response);
    [m_domPasteMenu removeAllItems];
    [m_domPasteMenu update];
    [m_domPasteMenu cancelTracking];
    m_domPasteMenu = nil;
    m_domPasteMenuDelegate = nil;
}

static RetainPtr<CGImageRef> takeWindowSnapshot(CGSWindowID windowID, bool captureAtNominalResolution, ForceSoftwareCapturingViewportSnapshot forceSoftwareCapturing)
{
    // FIXME <https://webkit.org/b/277572>: CGSHWCaptureWindowList is currently bugged where
    // the kCGSCaptureIgnoreGlobalClipShape option has no effect and the resulting screenshot
    // still contains the window's rounded corners. There are WPT tests relying on comparing
    // WebDriver's screenshots that cannot tolerate this inconsistency, especially due to
    // CGSHWCaptureWindowList not always succeeding. So for WebDriver only, we bypass that bug
    // and always use deprecated CGWindowListCreateImage instead.

    if (forceSoftwareCapturing == ForceSoftwareCapturingViewportSnapshot::No) {
        CGSWindowCaptureOptions options = kCGSCaptureIgnoreGlobalClipShape;
        if (captureAtNominalResolution)
            options |= kCGSWindowCaptureNominalResolution;
        RetainPtr<CFArrayRef> windowSnapshotImages = adoptCF(CGSHWCaptureWindowList(CGSMainConnectionID(), &windowID, 1, options));

        if (windowSnapshotImages && CFArrayGetCount(windowSnapshotImages.get()))
            return checked_cf_cast<CGImageRef>(CFArrayGetValueAtIndex(windowSnapshotImages.get(), 0));
    }

    // Fall back to the non-hardware capture path if we didn't get a snapshot
    // (which usually happens if the window is fully off-screen).
    CGWindowImageOption imageOptions = kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque;
    if (captureAtNominalResolution)
        imageOptions |= kCGWindowImageNominalResolution;
    return WebCore::cgWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, windowID, imageOptions);
}

RefPtr<ViewSnapshot> WebViewImpl::takeViewSnapshot()
{
    return takeViewSnapshot(ForceSoftwareCapturingViewportSnapshot::No);
}

RefPtr<ViewSnapshot> WebViewImpl::takeViewSnapshot(ForceSoftwareCapturingViewportSnapshot forceSoftwareCapturing)
{
    RetainPtr window = WebViewImpl::window();

    CGSWindowID windowID = (CGSWindowID)window.get().windowNumber;
    if (!windowID || !window.get().isVisible)
        return nullptr;

    RetainPtr<CGImageRef> windowSnapshotImage = takeWindowSnapshot(windowID, false, forceSoftwareCapturing);
    if (!windowSnapshotImage)
        return nullptr;

    // Work around <rdar://problem/17084993>; re-request the snapshot at kCGWindowImageNominalResolution if it was captured at the wrong scale.
    CGFloat desiredSnapshotWidth = window.get().frame.size.width * window.get().screen.backingScaleFactor;
    if (CGImageGetWidth(windowSnapshotImage.get()) != desiredSnapshotWidth)
        windowSnapshotImage = takeWindowSnapshot(windowID, true, forceSoftwareCapturing);

    if (!windowSnapshotImage)
        return nullptr;

    NSRect windowCaptureRect;
    WebCore::FloatRect boundsForCustomSwipeViews = ensureProtectedGestureController()->windowRelativeBoundsForCustomSwipeViews();
    if (!boundsForCustomSwipeViews.isEmpty())
        windowCaptureRect = boundsForCustomSwipeViews;
    else {
        RetainPtr view = m_view.get();
        FloatRect unobscuredBounds = [view bounds];
        unobscuredBounds.contract(m_page->obscuredContentInsets());
        windowCaptureRect = [view convertRect:unobscuredBounds toView:nil];
    }

    NSRect windowCaptureScreenRect = [window convertRectToScreen:windowCaptureRect];
    CGRect windowScreenRect;
    CGSGetScreenRectForWindow(CGSMainConnectionID(), (CGSWindowID)[window windowNumber], &windowScreenRect);

    NSRect croppedImageRect = windowCaptureRect;
    croppedImageRect.origin.y = windowScreenRect.size.height - windowCaptureScreenRect.size.height - NSMinY(windowCaptureRect);

    auto croppedSnapshotImage = adoptCF(CGImageCreateWithImageInRect(windowSnapshotImage.get(), NSRectToCGRect([window convertRectToBacking:croppedImageRect])));

    auto surface = WebCore::IOSurface::createFromImage(nullptr, croppedSnapshotImage.get());
    if (!surface)
        return nullptr;

    return ViewSnapshot::create(WTFMove(surface));
}

void WebViewImpl::saveBackForwardSnapshotForCurrentItem()
{
    if (RefPtr item = m_page->backForwardList().currentItem())
        m_page->recordNavigationSnapshot(*item);
}

void WebViewImpl::saveBackForwardSnapshotForItem(WebBackForwardListItem& item)
{
    m_page->recordNavigationSnapshot(item);
}

void WebViewImpl::insertTextPlaceholderWithSize(CGSize size, void(^completionHandler)(NSTextPlaceholder *placeholder))
{
    m_page->insertTextPlaceholder(WebCore::IntSize { size }, [completionHandler = makeBlockPtr(completionHandler)](const std::optional<WebCore::ElementContext>& placeholder) {
        if (!placeholder) {
            completionHandler(nil);
            return;
        }
        completionHandler(adoptNS([[WKTextPlaceholder alloc] initWithElementContext:*placeholder]).get());
    });
}

void WebViewImpl::removeTextPlaceholder(NSTextPlaceholder *placeholder, bool willInsertText, void(^completionHandler)())
{
    // FIXME: Implement support for willInsertText. See <https://bugs.webkit.org/show_bug.cgi?id=208747>.
    if (RetainPtr wkTextPlaceholder = dynamic_objc_cast<WKTextPlaceholder>(placeholder))
        m_page->removeTextPlaceholder([wkTextPlaceholder elementContext], makeBlockPtr(completionHandler));
    else
        completionHandler();
}

#if ENABLE(WRITING_TOOLS)

void WebViewImpl::showWritingTools(WTRequestedTool tool)
{
    FloatRect selectionRect;

    auto& editorState = m_page->editorState();
    if (editorState.selectionIsRange)
        selectionRect = page().selectionBoundingRectInRootViewCoordinates();

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[PAL::getWTWritingToolsClassSingleton() sharedInstance] showTool:tool forSelectionRect:selectionRect ofView:m_view.get().get() forDelegate:(NSObject<WTWritingToolsDelegate> *)m_view.get().get()];
ALLOW_DEPRECATED_DECLARATIONS_END
}

void WebViewImpl::addTextAnimationForAnimationID(WTF::UUID uuid, const WebCore::TextAnimationData& data)
{
    if (!m_page->protectedPreferences()->textAnimationsEnabled())
        return;

    if (!m_textAnimationTypeManager)
        m_textAnimationTypeManager = adoptNS([[WKTextAnimationManager alloc] initWithWebViewImpl:*this]);

    [m_textAnimationTypeManager addTextAnimationForAnimationID:uuid.createNSUUID().get() withData:data];
}

void WebViewImpl::removeTextAnimationForAnimationID(WTF::UUID uuid)
{
    if (!m_page->protectedPreferences()->textAnimationsEnabled())
        return;

    [m_textAnimationTypeManager removeTextAnimationForAnimationID:uuid.createNSUUID().get()];
}

void WebViewImpl::hideTextAnimationView()
{
    [m_textAnimationTypeManager hideTextAnimationView];
}

#endif // ENABLE(WRITING_TOOLS)

ViewGestureController& WebViewImpl::ensureGestureController()
{
    if (!m_gestureController)
        m_gestureController = ViewGestureController::create(m_page);
    return *m_gestureController;
}

Ref<ViewGestureController> WebViewImpl::ensureProtectedGestureController()
{
    return ensureGestureController();
}

void WebViewImpl::setAllowsBackForwardNavigationGestures(bool allowsBackForwardNavigationGestures)
{
    m_allowsBackForwardNavigationGestures = allowsBackForwardNavigationGestures;
    m_page->setShouldRecordNavigationSnapshots(allowsBackForwardNavigationGestures);
    m_page->setShouldUseImplicitRubberBandControl(allowsBackForwardNavigationGestures);
}

void WebViewImpl::setAllowsMagnification(bool allowsMagnification)
{
    m_allowsMagnification = allowsMagnification;
}

void WebViewImpl::setMagnification(double magnification, CGPoint centerPoint)
{
    if (magnification <= 0 || isnan(magnification) || isinf(magnification))
        [NSException raise:NSInvalidArgumentException format:@"Magnification should be a positive number"];

    dismissContentRelativeChildWindowsWithAnimation(false);
    suppressContentRelativeChildViews(ContentRelativeChildViewsSuppressionType::TemporarilyRemove);

    m_page->scalePageInViewCoordinates(magnification, WebCore::roundedIntPoint(centerPoint));
}

void WebViewImpl::setMagnification(double magnification)
{
    if (magnification <= 0 || isnan(magnification) || isinf(magnification))
        [NSException raise:NSInvalidArgumentException format:@"Magnification should be a positive number"];

    dismissContentRelativeChildWindowsWithAnimation(false);
    suppressContentRelativeChildViews(ContentRelativeChildViewsSuppressionType::TemporarilyRemove);

    RetainPtr view = m_view.get();
    WebCore::FloatPoint viewCenter(NSMidX([view bounds]), NSMidY([view bounds]));
    m_page->scalePageInViewCoordinates(magnification, roundedIntPoint(viewCenter));
}

double WebViewImpl::magnification() const
{
    if (RefPtr gestureController = m_gestureController)
        return gestureController->magnification();
    return m_page->pageScaleFactor();
}

void WebViewImpl::setCustomSwipeViews(NSArray *customSwipeViews)
{
    if (!customSwipeViews.count && !m_gestureController)
        return;

    Vector<RetainPtr<NSView>> views;
    views.reserveInitialCapacity(customSwipeViews.count);
    for (NSView *view in customSwipeViews)
        views.append(view);

    ensureProtectedGestureController()->setCustomSwipeViews(views);
}

FloatRect WebViewImpl::windowRelativeBoundsForCustomSwipeViews() const
{
    if (!m_gestureController)
        return { };

    return protectedGestureController()->windowRelativeBoundsForCustomSwipeViews();
}

FloatBoxExtent WebViewImpl::customSwipeViewsObscuredContentInsets() const
{
    if (!m_gestureController)
        return { };

    return m_gestureController->customSwipeViewsObscuredContentInsets();
}

RefPtr<ViewGestureController> WebViewImpl::protectedGestureController() const
{
    return m_gestureController;
}

void WebViewImpl::setCustomSwipeViewsObscuredContentInsets(FloatBoxExtent&& insets)
{
    ensureProtectedGestureController()->setCustomSwipeViewsObscuredContentInsets(WTFMove(insets));
}

bool WebViewImpl::tryToSwipeWithEvent(NSEvent *event, bool ignoringPinnedState)
{
    if (!m_allowsBackForwardNavigationGestures)
        return false;

    Ref gestureController = ensureGestureController();

    bool wasIgnoringPinnedState = gestureController->shouldIgnorePinnedState();
    gestureController->setShouldIgnorePinnedState(ignoringPinnedState);

    bool handledEvent = gestureController->handleScrollWheelEvent(event);

    gestureController->setShouldIgnorePinnedState(wasIgnoringPinnedState);

    return handledEvent;
}

void WebViewImpl::setDidMoveSwipeSnapshotCallback(BlockPtr<void (CGRect)>&& callback)
{
    if (!m_allowsBackForwardNavigationGestures)
        return;

    ensureProtectedGestureController()->setDidMoveSwipeSnapshotCallback(WTFMove(callback));
}

void WebViewImpl::scrollWheel(NSEvent *event)
{
    if (m_ignoresAllEvents) {
        RELEASE_LOG(MouseHandling, "[pageProxyID=%lld] WebViewImpl::scrollWheel: ignored event", m_page->identifier().toUInt64());
        return;
    }

    if (event.phase == NSEventPhaseBegan)
        dismissContentRelativeChildWindowsWithAnimation(false);

    if (m_allowsBackForwardNavigationGestures && ensureProtectedGestureController()->handleScrollWheelEvent(event)) {
        RELEASE_LOG(MouseHandling, "[pageProxyID=%lld] WebViewImpl::scrollWheel: Gesture controller handled wheel event", m_page->identifier().toUInt64());
        return;
    }

    auto webEvent = NativeWebWheelEvent(event, m_view.getAutoreleased());
    m_page->handleNativeWheelEvent(webEvent);
}

void WebViewImpl::swipeWithEvent(NSEvent *event)
{
    if (m_ignoresNonWheelEvents)
        return;

    if (!m_allowsBackForwardNavigationGestures) {
        [m_view.get() _web_superSwipeWithEvent:event];
        return;
    }

    if (event.deltaX > 0.0)
        m_page->goBack();
    else if (event.deltaX < 0.0)
        m_page->goForward();
    else
        [m_view.get() _web_superSwipeWithEvent:event];
}

void WebViewImpl::magnifyWithEvent(NSEvent *event)
{
    if (!m_allowsMagnification) {
#if ENABLE(MAC_GESTURE_EVENTS)
        if (auto webEvent = NativeWebGestureEvent::create(event, m_view.getAutoreleased()))
            m_page->handleGestureEvent(*webEvent);
#endif
        [m_view.get() _web_superMagnifyWithEvent:event];
        return;
    }

    dismissContentRelativeChildWindowsWithAnimation(false);

    Ref gestureController = ensureGestureController();

#if ENABLE(MAC_GESTURE_EVENTS)
    if (gestureController->hasActiveMagnificationGesture()) {
        gestureController->handleMagnificationGestureEvent(event, [m_view.get() convertPoint:event.locationInWindow fromView:nil]);
        return;
    }

    if (auto webEvent = NativeWebGestureEvent::create(event, m_view.getAutoreleased()))
        m_page->handleGestureEvent(*webEvent);
#else
    gestureController->handleMagnificationGestureEvent(event, [m_view.get() convertPoint:event.locationInWindow fromView:nil]);
#endif
}

void WebViewImpl::smartMagnifyWithEvent(NSEvent *event)
{
    if (!m_allowsMagnification) {
        [m_view.get() _web_superSmartMagnifyWithEvent:event];
        return;
    }

    dismissContentRelativeChildWindowsWithAnimation(false);

    ensureProtectedGestureController()->handleSmartMagnificationGesture([m_view.get() convertPoint:event.locationInWindow fromView:nil]);
}

RetainPtr<NSEvent> WebViewImpl::setLastMouseDownEvent(NSEvent *event)
{
    ASSERT(!event || event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown || event.type == NSEventTypeOtherMouseDown);

    return std::exchange(m_lastMouseDownEvent, event);
}

#if ENABLE(MAC_GESTURE_EVENTS)
void WebViewImpl::rotateWithEvent(NSEvent *event)
{
    if (auto webEvent = NativeWebGestureEvent::create(event, m_view.getAutoreleased()))
        m_page->handleGestureEvent(*webEvent);
}
#endif

void WebViewImpl::gestureEventWasNotHandledByWebCore(NSEvent *event)
{
    [m_view.get() _web_gestureEventWasNotHandledByWebCore:event];
}

void WebViewImpl::gestureEventWasNotHandledByWebCoreFromViewOnly(NSEvent *event)
{
#if ENABLE(MAC_GESTURE_EVENTS)
    if (m_allowsMagnification && m_gestureController)
        m_gestureController->gestureEventWasNotHandledByWebCore(event, [m_view.get() convertPoint:event.locationInWindow fromView:nil]);
#endif
}

void WebViewImpl::didRestoreScrollPosition()
{
    if (RefPtr gestureController = m_gestureController)
        gestureController->didRestoreScrollPosition();
}

void WebViewImpl::doneWithKeyEvent(NSEvent *event, bool eventWasHandled)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    if ([event type] != NSEventTypeKeyDown)
        return;

    if (eventWasHandled) {
        [NSCursor setHiddenUntilMouseMoves:YES];
        return;
    }

    // resending the event may destroy this WKView
    auto protector = m_view.get();

    ASSERT(!m_keyDownEventBeingResent);
    m_keyDownEventBeingResent = event;
    [NSApp _setCurrentEvent:event];
    [NSApp sendEvent:event];

    m_keyDownEventBeingResent = nullptr;
}

NSArray *WebViewImpl::validAttributesForMarkedTextSingleton()
{
    static NeverDestroyed<RetainPtr<NSArray>> validAttributes = @[
        NSUnderlineStyleAttributeName,
        NSUnderlineColorAttributeName,
        NSMarkedClauseSegmentAttributeName,
        NSTextAlternativesAttributeName,
        NSTextInsertionUndoableAttributeName,
        NSBackgroundColorAttributeName,
        NSForegroundColorAttributeName,
    ];
    // NSText also supports the following attributes, but it's
    // hard to tell which are really required for text input to
    // work well; I have not seen any input method make use of them yet.
    //     NSFontAttributeName, NSForegroundColorAttributeName,
    //     NSBackgroundColorAttributeName, NSLanguageAttributeName.
    LOG(TextInput, "validAttributesForMarkedText -> (...)");
    return validAttributes.get().get();
}

static bool eventKeyCodeIsZeroOrNumLockOrFn(NSEvent *event)
{
    unsigned short keyCode = [event keyCode];
    return !keyCode || keyCode == 10 || keyCode == 63;
}

Vector<WebCore::KeypressCommand> WebViewImpl::collectKeyboardLayoutCommandsForEvent(NSEvent *event)
{
    if ([event type] != NSEventTypeKeyDown)
        return { };

    ASSERT(!m_collectedKeypressCommands);
    m_collectedKeypressCommands = Vector<WebCore::KeypressCommand> { };

    if (RetainPtr context = inputContext())
        [context handleEventByKeyboardLayout:event];
    else
        [m_view.get() interpretKeyEvents:@[event]];

    auto commands = WTFMove(*m_collectedKeypressCommands);
    m_collectedKeypressCommands = std::nullopt;

    if (RetainPtr<NSMenu> menu = NSApp.mainMenu; event.modifierFlags & NSEventModifierFlagFunction
        && [menu respondsToSelector:@selector(_containsItemMatchingEvent:includingDisabledItems:)] && [menu _containsItemMatchingEvent:event includingDisabledItems:YES]) {
        commands.removeAllMatching([](auto& command) {
            return command.commandName == "insertText:"_s;
        });
    }

    return commands;
}

void WebViewImpl::interpretKeyEvent(NSEvent *event, void(^completionHandler)(BOOL handled, const Vector<WebCore::KeypressCommand>& commands))
{
    if (!inputContext()) {
        auto commands = collectKeyboardLayoutCommandsForEvent(event);
        completionHandler(NO, commands);
        return;
    }

#if PLATFORM(MAC)
    if (m_page->editorState().inputMethodUsesCorrectKeyEventOrder) {
        if (m_collectedKeypressCommands) {
            m_interpretKeyEventHoldingTank.append([weakThis = WeakPtr { *this }, capturedEvent = retainPtr(event), capturedBlock = makeBlockPtr(completionHandler)] {
                CheckedPtr checkedThis = weakThis.get();
                if (!checkedThis)
                    capturedBlock(NO, { });
                else
                    checkedThis->interpretKeyEvent(capturedEvent.get(), capturedBlock.get());
            });
            return;
        }

        m_collectedKeypressCommands = Vector<WebCore::KeypressCommand> { };
    }
#endif

    LOG(TextInput, "-> handleEventByInputMethod:%p %@", event, event);
    RetainPtr inputContext { WebViewImpl::inputContext() };
    [inputContext.get() handleEventByInputMethod:event completionHandler:[weakThis = WeakPtr { *this }, capturedEvent = retainPtr(event), capturedBlock = makeBlockPtr(completionHandler)](BOOL handled) mutable {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis) {
            capturedBlock(NO, { });
            return;
        }

        Vector<WebCore::KeypressCommand> commands;
#if PLATFORM(MAC)
        if (checkedThis->m_page->editorState().inputMethodUsesCorrectKeyEventOrder) {
            commands = WTFMove(*checkedThis->m_collectedKeypressCommands);
            checkedThis->m_collectedKeypressCommands = std::nullopt;
            checkedThis->m_stagedMarkedRange = std::nullopt;
        }
#endif

        bool hasInsertText = false;
        for (auto& command : commands) {
            if (command.commandName == "insertText:"_s)
                hasInsertText = true;
        }

        if (hasInsertText)
            handled = NO;

        LOG(TextInput, "... handleEventByInputMethod%s handled", handled ? "" : " not");
        if (handled) {
            capturedBlock(YES, WTFMove(commands));
            auto holdingTank = WTFMove(checkedThis->m_interpretKeyEventHoldingTank);
            for (auto& function : holdingTank)
                function();
            return;
        }

        auto additionalCommands = checkedThis->collectKeyboardLayoutCommandsForEvent(capturedEvent.get());
        commands.appendVector(additionalCommands);
        capturedBlock(NO, commands);
#if PLATFORM(MAC)
        ASSERT(checkedThis->m_page->editorState().inputMethodUsesCorrectKeyEventOrder || checkedThis->m_interpretKeyEventHoldingTank.isEmpty());
#endif
        auto holdingTank = WTFMove(checkedThis->m_interpretKeyEventHoldingTank);
        for (auto& function : holdingTank)
            function();
    }];
}

void WebViewImpl::doCommandBySelector(SEL selector)
{
    LOG(TextInput, "doCommandBySelector:\"%s\"", sel_getName(selector));

    if (m_collectedKeypressCommands) {
        WebCore::KeypressCommand command(NSStringFromSelector(selector));
        m_collectedKeypressCommands->append(command);
        LOG(TextInput, "...stored");
        m_page->registerKeypressCommandName(command.commandName);
    } else {
        // FIXME: Send the command to Editor synchronously and only send it along the
        // responder chain if it's a selector that does not correspond to an editing command.
        [m_view.get() _web_superDoCommandBySelector:selector];
    }
}

void WebViewImpl::insertText(id string)
{
    // Unlike an NSTextInputClient variant with replacementRange, this NSResponder method is called when there is no input context,
    // so text input processing isn't performed. We are not going to actually insert any text in that case, but saving an insertText
    // command ensures that a keypress event is dispatched as appropriate.
    insertText(string, NSMakeRange(NSNotFound, 0));
}

void WebViewImpl::insertText(id string, NSRange replacementRange)
{
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
    ASSERT(isAttributedString || [string isKindOfClass:[NSString class]]);

    if (replacementRange.location != NSNotFound)
        LOG(TextInput, "insertText:\"%@\" replacementRange:(%zu, %zu)", isAttributedString ? [string string] : string, replacementRange.location, replacementRange.length);
    else
        LOG(TextInput, "insertText:\"%@\"", isAttributedString ? [string string] : string);

    RetainPtr<NSString> text;
    Vector<WebCore::TextAlternativeWithRange> dictationAlternatives;

    bool registerUndoGroup = false;
    if (isAttributedString) {
        WebCore::collectDictationTextAlternatives(string, dictationAlternatives);
        registerUndoGroup = WebCore::shouldRegisterInsertionUndoGroup(string);
        // FIXME: We ignore most attributes from the string, so for example inserting from Character Palette loses font and glyph variation data.
        text = [string string];
    } else
        text = string;

    m_isTextInsertionReplacingSoftSpace = false;
    if (m_softSpaceRange.location != NSNotFound && (replacementRange.location == NSMaxRange(m_softSpaceRange) || replacementRange.location == NSNotFound) && !replacementRange.length && [[NSSpellChecker sharedSpellChecker] deletesAutospaceBeforeString:text.get() language:nil]) {
        replacementRange = m_softSpaceRange;
        m_isTextInsertionReplacingSoftSpace = true;
    }
    m_softSpaceRange = NSMakeRange(NSNotFound, 0);

    // insertText can be called for several reasons:
    // - If it's from normal key event processing (including key bindings), we save the action to perform it later.
    // - If it's from an input method, then we should insert the text now.
    // - If it's sent outside of keyboard event processing (e.g. from Character Viewer, or when confirming an inline input area with a mouse),
    // then we also execute it immediately, as there will be no other chance.
    if (m_collectedKeypressCommands && !m_isTextInsertionReplacingSoftSpace) {
        ASSERT(replacementRange.location == NSNotFound);
        WebCore::KeypressCommand command("insertText:"_s, text.get());
        m_collectedKeypressCommands->append(command);
        LOG(TextInput, "...stored");
        m_page->registerKeypressCommandName(command.commandName);
        return;
    }

    String eventText = makeStringByReplacingAll(text.get(), NSBackTabCharacter, NSTabCharacter); // same thing is done in KeyEventMac.mm in WebCore
    if (!dictationAlternatives.isEmpty()) {
        InsertTextOptions options;
        options.registerUndoGroup = registerUndoGroup;
        m_page->insertDictatedTextAsync(eventText, replacementRange, dictationAlternatives, WTFMove(options));
    } else {
        InsertTextOptions options;
        options.registerUndoGroup = registerUndoGroup;
        options.editingRangeIsRelativeTo = m_isTextInsertionReplacingSoftSpace ? EditingRangeIsRelativeTo::Paragraph : EditingRangeIsRelativeTo::EditableRoot;
        options.suppressSelectionUpdate = m_isTextInsertionReplacingSoftSpace;

        m_page->insertTextAsync(eventText, replacementRange, WTFMove(options));
    }
}

void WebViewImpl::selectedRangeWithCompletionHandler(void(^completionHandlerPtr)(NSRange selectedRange))
{
    auto completionHandler = adoptNS([completionHandlerPtr copy]);

    LOG(TextInput, "selectedRange");
    m_page->getSelectedRangeAsync([completionHandler, stagedSelectedRange = m_stagedMarkedRange](const EditingRange& editingRangeResult, const EditingRange& compositionRange) {
        void (^completionHandlerBlock)(NSRange) = (void (^)(NSRange))completionHandler.get();

        if (stagedSelectedRange) {
            completionHandlerBlock(NSRange { compositionRange.location + stagedSelectedRange->location, stagedSelectedRange->length });
            return;
        }

        NSRange result = editingRangeResult;
        if (result.location == NSNotFound)
            LOG(TextInput, "    -> selectedRange returned (NSNotFound, %zu)", result.length);
        else
            LOG(TextInput, "    -> selectedRange returned (%zu, %zu)", result.location, result.length);
        completionHandlerBlock(result);
    });
}

void WebViewImpl::markedRangeWithCompletionHandler(void(^completionHandlerPtr)(NSRange markedRange))
{
    auto completionHandler = adoptNS([completionHandlerPtr copy]);

    LOG(TextInput, "markedRange");
    m_page->getMarkedRangeAsync([completionHandler](const EditingRange& editingRangeResult) {
        void (^completionHandlerBlock)(NSRange) = (void (^)(NSRange))completionHandler.get();
        NSRange result = editingRangeResult;
        if (result.location == NSNotFound)
            LOG(TextInput, "    -> markedRange returned (NSNotFound, %zu)", result.length);
        else
            LOG(TextInput, "    -> markedRange returned (%zu, %zu)", result.location, result.length);
        completionHandlerBlock(result);
    });
}

void WebViewImpl::hasMarkedTextWithCompletionHandler(void(^completionHandler)(BOOL hasMarkedText))
{
    LOG(TextInput, "hasMarkedText");
    m_page->hasMarkedText([completionHandler = makeBlockPtr(completionHandler)] (bool result) {
        completionHandler(result);
        LOG(TextInput, "    -> hasMarkedText returned %u", result);
    });
}

void WebViewImpl::attributedSubstringForProposedRange(NSRange proposedRange, void(^completionHandler)(NSAttributedString *attrString, NSRange actualRange))
{
    LOG(TextInput, "attributedSubstringFromRange:(%zu, %zu)", proposedRange.location, proposedRange.length);
    m_page->attributedSubstringForCharacterRangeAsync(proposedRange, [completionHandler = makeBlockPtr(completionHandler)](const WebCore::AttributedString& string, const EditingRange& actualRange) {
        auto attributedString = string.nsAttributedString();
        LOG(TextInput, "    -> attributedSubstringFromRange returned %@", attributedString.get());
        completionHandler(attributedString.get(), actualRange);
    });
}

void WebViewImpl::firstRectForCharacterRange(NSRange range, void(^completionHandler)(NSRect firstRect, NSRange actualRange))
{
    LOG(TextInput, "firstRectForCharacterRange:(%zu, %zu)", range.location, range.length);

    // Just to match NSTextView's behavior. Regression tests cannot detect this;
    // to reproduce, use a test application from http://bugs.webkit.org/show_bug.cgi?id=4682
    // (type something; try ranges (1, -1) and (2, -1).
    if ((range.location + range.length < range.location) && (range.location + range.length != 0))
        range.length = 0;

    if (range.location == NSNotFound) {
        LOG(TextInput, "    -> NSZeroRect");
        completionHandler(NSZeroRect, range);
        return;
    }

    m_page->firstRectForCharacterRangeAsync(range, [weakThis = WeakPtr { *this }, completionHandler = makeBlockPtr(completionHandler)](const WebCore::IntRect& rect, const EditingRange& actualRange) {
        if (!weakThis) {
            LOG(TextInput, "    ...firstRectForCharacterRange failed (WebViewImpl was destroyed).");
            completionHandler(NSZeroRect, NSMakeRange(NSNotFound, 0));
            return;
        }

        RetainPtr view = weakThis->m_view.get();
        NSRect resultRect = [view convertRect:rect toView:nil];
        resultRect = [retainPtr([view window]) convertRectToScreen:resultRect];

        LOG(TextInput, "    -> firstRectForCharacterRange returned (%f, %f, %f, %f)", resultRect.origin.x, resultRect.origin.y, resultRect.size.width, resultRect.size.height);
        completionHandler(resultRect, actualRange);
    });
}

void WebViewImpl::characterIndexForPoint(NSPoint point, void(^completionHandler)(NSUInteger))
{
    LOG(TextInput, "characterIndexForPoint:(%f, %f)", point.x, point.y);

    RetainPtr window = [m_view.get() window];
    if (window)
        point = [window convertPointFromScreen:point];
    point = [m_view.get() convertPoint:point fromView:nil]; // the point is relative to the main frame

    m_page->characterIndexForPointAsync(WebCore::IntPoint(point), [completionHandler = makeBlockPtr(completionHandler)](uint64_t result) {
        if (result == notFound)
            result = NSNotFound;
        LOG(TextInput, "    -> characterIndexForPoint returned %llu", result);
        completionHandler(result);
    });
}

NSTextInputContext *WebViewImpl::inputContext()
{
    // Disable text input machinery when in non-editable content. An invisible inline input area affects performance, and can prevent Expose from working.
    if (!m_page->editorState().isContentEditable)
        return nil;

    return [m_view.get() _web_superInputContext];
}

void WebViewImpl::unmarkText()
{
    LOG(TextInput, "unmarkText");

    m_page->confirmCompositionAsync();
}

#if HAVE(REDESIGNED_TEXT_CURSOR)
static BOOL shouldUseHighlightsForMarkedText(NSAttributedString *string)
{
    __block BOOL result = NO;

    [string enumerateAttributesInRange:NSMakeRange(0, string.length) options:0 usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange, BOOL *stop) {
        BOOL hasUnderlineStyle = !![attributes objectForKey:NSUnderlineStyleAttributeName];
        BOOL hasUnderlineColor = !![attributes objectForKey:NSUnderlineColorAttributeName];

        BOOL hasBackgroundColor = !![attributes objectForKey:NSBackgroundColorAttributeName];
        BOOL hasForegroundColor = !![attributes objectForKey:NSForegroundColorAttributeName];

        // Marked text may be represented either as an underline or a highlight; this mode is dictated
        // by the attributes it has, and therefore having both types of attributes is not allowed.
        ASSERT(!((hasUnderlineStyle || hasUnderlineColor) && (hasBackgroundColor || hasForegroundColor)));

        if (hasUnderlineStyle || hasUnderlineColor) {
            result = NO;
            *stop = YES;
        } else if (hasBackgroundColor || hasForegroundColor) {
            result = YES;
            *stop = YES;
        }
    }];

    return result;
}

static Vector<WebCore::CompositionHighlight> compositionHighlights(NSAttributedString *string)
{
    if (!string.length)
        return { };

    Vector<WebCore::CompositionHighlight> highlights;
    [string enumerateAttributesInRange:NSMakeRange(0, string.length) options:0 usingBlock:[&highlights](NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        std::optional<WebCore::Color> backgroundHighlightColor;
        if (RetainPtr<WebCore::CocoaColor> backgroundColor = attributes[NSBackgroundColorAttributeName])
            backgroundHighlightColor = WebCore::colorFromCocoaColor(backgroundColor.get());

        std::optional<WebCore::Color> foregroundHighlightColor;
        if (RetainPtr<WebCore::CocoaColor> foregroundColor = attributes[NSForegroundColorAttributeName])
            foregroundHighlightColor = WebCore::colorFromCocoaColor(foregroundColor.get());

        highlights.append({ static_cast<unsigned>(range.location), static_cast<unsigned>(NSMaxRange(range)), backgroundHighlightColor, foregroundHighlightColor });
    }];

    std::ranges::sort(highlights);

    Vector<WebCore::CompositionHighlight> mergedHighlights;
    mergedHighlights.reserveInitialCapacity(highlights.size());
    for (auto& highlight : highlights) {
        if (mergedHighlights.isEmpty() || mergedHighlights.last().backgroundColor != highlight.backgroundColor || mergedHighlights.last().foregroundColor != highlight.foregroundColor)
            mergedHighlights.append(highlight);
        else
            mergedHighlights.last().endOffset = highlight.endOffset;
    }

    mergedHighlights.shrinkToFit();
    return mergedHighlights;
}
#endif

static Vector<WebCore::CompositionUnderline> compositionUnderlines(NSAttributedString *string)
{
    if (!string.length)
        return { };

    Vector<WebCore::CompositionUnderline> mergedUnderlines;

#if HAVE(INLINE_PREDICTIONS) || HAVE(REDESIGNED_TEXT_CURSOR)
    Vector<WebCore::CompositionUnderline> underlines;

    [string enumerateAttributesInRange:NSMakeRange(0, string.length) options:0 usingBlock:[&underlines](NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        RetainPtr<NSNumber> style = [attributes objectForKey:NSUnderlineStyleAttributeName];
        if (!style)
            return;

        RetainPtr<NSColor> underlineColor = attributes[NSUnderlineColorAttributeName];
        bool isClear = [underlineColor isEqual:NSColor.clearColor];

        if (!isClear)
            underlines.append({ static_cast<unsigned>(range.location), static_cast<unsigned>(NSMaxRange(range)), WebCore::CompositionUnderlineColor::GivenColor, WebCore::Color::black, style.get().intValue > 1 });
    }];

    std::ranges::sort(underlines);

    if (!underlines.isEmpty())
        mergedUnderlines.append({ underlines.first().startOffset, underlines.last().endOffset, WebCore::CompositionUnderlineColor::GivenColor, WebCore::Color::black, false });

    for (auto& underline : underlines) {
        if (underline.thick)
            mergedUnderlines.append(underline);
    }

    if (mergedUnderlines.size())
        return mergedUnderlines;
#endif

    int length = string.string.length;

    for (int i = 0; i < length;) {
        NSRange range;
        RetainPtr<NSDictionary> attrs = [string attributesAtIndex:i longestEffectiveRange:&range inRange:NSMakeRange(i, length - i)];

        if (RetainPtr<NSNumber> style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
            WebCore::Color color = WebCore::Color::black;
            WebCore::CompositionUnderlineColor compositionUnderlineColor = WebCore::CompositionUnderlineColor::TextColor;
            if (RetainPtr<NSColor> colorAttribute = [attrs objectForKey:NSUnderlineColorAttributeName]) {
                color = WebCore::colorFromCocoaColor(colorAttribute.get());
                compositionUnderlineColor = WebCore::CompositionUnderlineColor::GivenColor;
            }
            mergedUnderlines.append(WebCore::CompositionUnderline(range.location, NSMaxRange(range), compositionUnderlineColor, color, style.get().intValue > 1));
        }

        i = range.location + range.length;
    }

    return mergedUnderlines;
}

void WebViewImpl::setMarkedText(id string, NSRange selectedRange, NSRange replacementRange)
{
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
    ASSERT(isAttributedString || [string isKindOfClass:[NSString class]]);

    LOG(TextInput, "setMarkedText:\"%@\" selectedRange:(%zu, %zu) replacementRange:(%zu, %zu)", string, selectedRange.location, selectedRange.length, replacementRange.location, replacementRange.length);

#if HAVE(INLINE_PREDICTIONS)
    if (RetainPtr attributedString = dynamic_objc_cast<NSAttributedString>(string)) {
        BOOL hasTextCompletion = [&] {
            __block BOOL result = NO;

            [attributedString enumerateAttribute:NSTextCompletionAttributeName inRange:NSMakeRange(0, [attributedString length]) options:0 usingBlock:^(id value, NSRange range, BOOL *stop) {
                if ([value respondsToSelector:@selector(boolValue)] && [value boolValue]) {
                    result = YES;
                    *stop = YES;
                }
            }];

            return result;
        }();

        if (hasTextCompletion || m_isHandlingAcceptedCandidate) {
            m_isHandlingAcceptedCandidate = hasTextCompletion;
            m_page->setWritingSuggestion([attributedString string], selectedRange);
            return;
        }
    }
#endif

    Vector<WebCore::CompositionUnderline> underlines;
    Vector<WebCore::CompositionHighlight> highlights;
    RetainPtr<NSString> text;

    if (isAttributedString) {
        // FIXME: We ignore most attributes from the string, so an input method cannot specify e.g. a font or a glyph variation.
        text = [string string];
#if HAVE(REDESIGNED_TEXT_CURSOR)
        if (shouldUseHighlightsForMarkedText(string))
            highlights = compositionHighlights(string);
        else
#endif
            underlines = compositionUnderlines(string);
    } else {
        text = string;
        underlines.append(WebCore::CompositionUnderline(0, [text length], WebCore::CompositionUnderlineColor::TextColor, WebCore::Color::black, false));
    }

    if (inSecureInputState()) {
        // In password fields, we only allow ASCII dead keys, and don't allow inline input, matching NSSecureTextInputField.
        // Allowing ASCII dead keys is necessary to enable full Roman input when using a Vietnamese keyboard.
        ASSERT(!m_page->editorState().hasComposition);
        notifyInputContextAboutDiscardedComposition();
        // FIXME: We should store the command to handle it after DOM event processing, as it's regular keyboard input now, not a composition.
        if ([text length] == 1 && isASCII([text characterAtIndex:0]))
            m_page->insertTextAsync(text.get(), replacementRange, { });
        else
            NSBeep();
        return;
    }

#if PLATFORM(MAC)
    if (m_page->editorState().inputMethodUsesCorrectKeyEventOrder && m_collectedKeypressCommands) {
        WebCore::KeypressCommand command("setMarkedText:"_s, text.get(), WTFMove(underlines), WTFMove(highlights),
            EditingRange { selectedRange }.toCharacterRange(), EditingRange { replacementRange }.toCharacterRange());
        m_collectedKeypressCommands->append(command);
        m_stagedMarkedRange = selectedRange;
        LOG(TextInput, "...stored");
        m_page->registerKeypressCommandName(command.commandName);
        return;
    }
#endif

    m_page->setCompositionAsync(text.get(), WTFMove(underlines), WTFMove(highlights), { }, selectedRange, replacementRange);
}

#if HAVE(INLINE_PREDICTIONS)
bool WebViewImpl::allowsInlinePredictions() const
{
    auto& editorState = m_page->editorState();

    if (editorState.hasPostLayoutData() && editorState.postLayoutData->canEnableWritingSuggestions)
        return NSSpellChecker.isAutomaticInlineCompletionEnabled;

    return editorState.isContentEditable && inlinePredictionsEnabled() && NSSpellChecker.isAutomaticInlineCompletionEnabled;
}

void WebViewImpl::showInlinePredictionsForCandidate(NSTextCheckingResult *candidate, NSRange absoluteSelectedRange, NSRange oldRelativeSelectedRange)
{
    if (!candidate)
        return;

    auto postLayoutData = postLayoutDataForContentEditable();
    if (!postLayoutData)
        return;

    if (m_lastStringForCandidateRequest != postLayoutData->stringForCandidateRequest)
        return;

    NSRange relativeSelectedRange = NSMakeRange(postLayoutData->candidateRequestStartPosition, postLayoutData->selectedTextLength);
    if (absoluteSelectedRange.location < relativeSelectedRange.location || absoluteSelectedRange.length != relativeSelectedRange.length)
        return;

    // Make sure the selected range didn’t change while we were making an asynchronous call to get it.
    if (!NSEqualRanges(oldRelativeSelectedRange, relativeSelectedRange))
        return;

    RetainPtr paragraphContextForCandidateRequest = postLayoutData->paragraphContextForCandidateRequest.createNSString();
    auto offsetSelectionRect = postLayoutData->selectionBoundingRect;
    offsetSelectionRect.move(0, offsetSelectionRect.height());

    NSUInteger offset = absoluteSelectedRange.location - relativeSelectedRange.location;
    RetainPtr adjustedCandidate = [candidate resultByAdjustingRangesWithOffset:offset];

    RetainPtr spellChecker = [NSSpellChecker sharedSpellChecker];

    if (![spellChecker respondsToSelector:@selector(showCompletionForCandidate:selectedRange:offset:inString:rect:view:completionHandler:)])
        return;

    WeakPtr weakThis { *this };
    [spellChecker
        showCompletionForCandidate:adjustedCandidate.get()
        selectedRange:absoluteSelectedRange
        offset:offset
        inString:paragraphContextForCandidateRequest.get()
        rect:offsetSelectionRect
        view:m_view.get().get()
        completionHandler:[weakThis](NSDictionary<NSString *, id> *resultDictionary) {
        if (!weakThis)
            return;

        // FIXME: rdar://105809280 Adopt NSTextCheckingSoftSpaceRangeKey once it is in more builds.
        RetainPtr<NSValue> softSpaceRangeValue = resultDictionary[@"SoftSpaceRange"];
        if (!softSpaceRangeValue)
            return;

        NSRange absoluteSoftSpaceRange = softSpaceRangeValue.get().rangeValue;
        weakThis->selectedRangeWithCompletionHandler([weakThis, absoluteSoftSpaceRange](NSRange absoluteSelectedRange) {
            if (!weakThis)
                return;

            auto postLayoutData = weakThis->postLayoutDataForContentEditable();
            if (!postLayoutData)
                return;

            NSRange relativeSelectedRange = NSMakeRange(postLayoutData->candidateRequestStartPosition, postLayoutData->selectedTextLength);

            if (absoluteSoftSpaceRange.location + absoluteSoftSpaceRange.length != absoluteSelectedRange.location)
                return;

            NSUInteger offset = absoluteSelectedRange.location - relativeSelectedRange.location;
            NSRange relativeSoftSpaceRange = NSMakeRange(absoluteSoftSpaceRange.location - offset, absoluteSoftSpaceRange.length);

            weakThis->m_softSpaceRange = relativeSoftSpaceRange;
        });
    }];
}

void WebViewImpl::showInlinePredictionsForCandidates(NSArray<NSTextCheckingResult *> *candidates)
{
    auto postLayoutData = postLayoutDataForContentEditable();
    if (!postLayoutData)
        return;

    if (m_lastStringForCandidateRequest != postLayoutData->stringForCandidateRequest)
        return;

    RetainPtr spellChecker = [NSSpellChecker sharedSpellChecker];

    RetainPtr candidate = [spellChecker completionCandidateFromCandidates:candidates];
    if (!candidate)
        return;

    NSRange relativeSelectedRange = NSMakeRange(postLayoutData->candidateRequestStartPosition, postLayoutData->selectedTextLength);

    WeakPtr weakThis { *this };
    selectedRangeWithCompletionHandler([weakThis, candidate, relativeSelectedRange](NSRange absoluteSelectedRange) {
        if (!weakThis)
            return;

        weakThis->showInlinePredictionsForCandidate(candidate.get(), absoluteSelectedRange, relativeSelectedRange);
    });
}
#endif

// Synchronous NSTextInputClient is still implemented to catch spurious sync calls. Remove when that is no longer needed.

NSRange WebViewImpl::selectedRange()
{
    // FIXME: (rdar://123703512) Re-add the `ASSERT_NOT_REACHED` assertion when possible.
    return NSMakeRange(NSNotFound, 0);
}

bool WebViewImpl::hasMarkedText()
{
    ASSERT_NOT_REACHED();
    return NO;
}

NSRange WebViewImpl::markedRange()
{
    ASSERT_NOT_REACHED();
    return NSMakeRange(NSNotFound, 0);
}

NSAttributedString *WebViewImpl::attributedSubstringForProposedRange(NSRange nsRange, NSRangePointer actualRange)
{
    ASSERT_NOT_REACHED();
    return nil;
}

NSUInteger WebViewImpl::characterIndexForPoint(NSPoint point)
{
    ASSERT_NOT_REACHED();
    return 0;
}

NSRect WebViewImpl::firstRectForCharacterRange(NSRange range, NSRangePointer actualRange)
{
    ASSERT_NOT_REACHED();
    return NSZeroRect;
}

bool WebViewImpl::performKeyEquivalent(NSEvent *event)
{
    if (ignoresNonWheelEvents())
        return NO;

    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    retainPtr(event).autorelease();

    // We get Esc key here after processing either Esc or Cmd+period. The former starts as a keyDown, and the latter starts as a key equivalent,
    // but both get transformed to a cancelOperation: command, executing which passes an Esc key event to -performKeyEquivalent:.
    // Don't interpret this event again, avoiding re-entrancy and infinite loops.
    if ([[event charactersIgnoringModifiers] isEqualToString:@"\e"] && !([event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask))
        return [m_view.get() _web_superPerformKeyEquivalent:event];

    if (m_keyDownEventBeingResent) {
        // WebCore has already seen the event, no need for custom processing.
        // Note that we can get multiple events for each event being re-sent. For example, for Cmd+'=' AppKit
        // first performs the original key equivalent, and if that isn't handled, it dispatches a synthetic Cmd+'+'.
        return [m_view.get() _web_superPerformKeyEquivalent:event];
    }

    // Pass key combos through WebCore if there is a key binding available for
    // this event. This lets webpages have a crack at intercepting key-modified keypresses.
    // FIXME: Why is the firstResponder check needed?
    if (m_view.getAutoreleased() == [m_view.get() window].firstResponder) {
        interpretKeyEvent(event, [weakThis = WeakPtr { *this }, capturedEvent = retainPtr(event)](BOOL handledByInputMethod, const Vector<WebCore::KeypressCommand>& commands) {
            if (weakThis)
                weakThis->m_page->handleKeyboardEvent(NativeWebKeyboardEvent(capturedEvent.get(), handledByInputMethod, false, commands));
        });
        return YES;
    }

    return [m_view.get() _web_superPerformKeyEquivalent:event];
}

void WebViewImpl::keyUp(NSEvent *event)
{
    if (ignoresNonWheelEvents())
        return;

    LOG(TextInput, "keyUp:%p %@", event, event);

    m_isTextInsertionReplacingSoftSpace = false;
    interpretKeyEvent(event, [weakThis = WeakPtr { *this }, capturedEvent = retainPtr(event)](BOOL handledByInputMethod, const Vector<WebCore::KeypressCommand>& commands) {
        if (weakThis)
            weakThis->m_page->handleKeyboardEvent(NativeWebKeyboardEvent(capturedEvent.get(), handledByInputMethod, weakThis->m_isTextInsertionReplacingSoftSpace, commands));
    });
}

void WebViewImpl::keyDown(NSEvent *event)
{
    if (ignoresNonWheelEvents())
        return;

    LOG(TextInput, "keyDown:%p %@%s", event, event, (event == m_keyDownEventBeingResent) ? " (re-sent)" : "");

    // We could be receiving a key down from AppKit if we have re-sent an event
    // that maps to an action that is currently unavailable (for example a copy when
    // there is no range selection).
    // If this is the case we should ignore the key down.
    if (m_keyDownEventBeingResent == event) {
        [m_view.get() _web_superKeyDown:event];
        return;
    }

    m_isTextInsertionReplacingSoftSpace = false;
    interpretKeyEvent(event, [weakThis = WeakPtr { *this }, capturedEvent = retainPtr(event)](BOOL handledByInputMethod, const Vector<WebCore::KeypressCommand>& commands) {
        if (weakThis)
            weakThis->m_page->handleKeyboardEvent(NativeWebKeyboardEvent(capturedEvent.get(), handledByInputMethod, weakThis->m_isTextInsertionReplacingSoftSpace, commands));
    });
}

void WebViewImpl::flagsChanged(NSEvent *event)
{
    if (ignoresNonWheelEvents())
        return;

    LOG(TextInput, "flagsChanged:%p %@", event, event);

    // Don't make an event from the num lock and function keys
    if (eventKeyCodeIsZeroOrNumLockOrFn(event))
        return;

    interpretKeyEvent(event, [weakThis = WeakPtr { *this }, capturedEvent = retainPtr(event)](BOOL handledByInputMethod, const Vector<WebCore::KeypressCommand>& commands) {
        if (weakThis)
            weakThis->m_page->handleKeyboardEvent(NativeWebKeyboardEvent(capturedEvent.get(), handledByInputMethod, false, commands));
    });
}

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, NSEventType eventType)
{
    switch (eventType) {
    case NSEventTypeLeftMouseDown: ts << "NSEventTypeLeftMouseDown"_s; break;
    case NSEventTypeLeftMouseUp: ts << "NSEventTypeLeftMouseUp"_s; break;
    case NSEventTypeRightMouseDown: ts << "NSEventTypeRightMouseDown"_s; break;
    case NSEventTypeRightMouseUp: ts << "NSEventTypeRightMouseUp"_s; break;
    case NSEventTypeMouseMoved: ts << "NSEventTypeMouseMoved"_s; break;
    case NSEventTypeLeftMouseDragged: ts << "NSEventTypeLeftMouseDragged"_s; break;
    case NSEventTypeRightMouseDragged: ts << "NSEventTypeRightMouseDragged"_s; break;
    case NSEventTypeMouseEntered: ts << "NSEventTypeMouseEntered"_s; break;
    case NSEventTypeMouseExited: ts << "NSEventTypeMouseExited"_s; break;
    case NSEventTypeKeyDown: ts << "NSEventTypeKeyDown"_s; break;
    case NSEventTypeKeyUp: ts << "NSEventTypeKeyUp"_s; break;
    case NSEventTypeScrollWheel: ts << "NSEventTypeScrollWheel"_s; break;
    case NSEventTypeOtherMouseDown: ts << "NSEventTypeOtherMouseDown"_s; break;
    case NSEventTypeOtherMouseUp: ts << "NSEventTypeOtherMouseUp"_s; break;
    case NSEventTypeOtherMouseDragged: ts << "NSEventTypeOtherMouseDragged"_s; break;
    default:
        ts << "Other"_s;
    }

    return ts;
}
#endif

void WebViewImpl::nativeMouseEventHandler(NSEvent *event)
{
    if (m_ignoresNonWheelEvents) {
        RELEASE_LOG(MouseHandling, "[pageProxyID=%lld] WebViewImpl::nativeMouseEventHandler: ignored event", m_page->identifier().toUInt64());
        return;
    }

    if (RetainPtr context = [m_view.get() inputContext]) {
        WeakPtr weakThis { *this };
        RetainPtr<NSEvent> retainedEvent = event;
        [context handleEvent:event completionHandler:[weakThis, retainedEvent] (BOOL handled) {
            if (!weakThis)
                return;
            if (handled)
                LOG_WITH_STREAM(TextInput, stream << "Event " << [retainedEvent type] << " was handled by text input context");
            else {
                NativeWebMouseEvent webEvent(retainedEvent.get(), weakThis->m_lastPressureEvent.get(), weakThis->m_view.getAutoreleased());
                weakThis->m_page->handleMouseEvent(webEvent);
            }
        }];
        return;
    }
    NativeWebMouseEvent webEvent(event, m_lastPressureEvent.get(), m_view.get().get());
    m_page->handleMouseEvent(webEvent);
}

void WebViewImpl::nativeMouseEventHandlerInternal(NSEvent *event)
{
    if (m_warningView)
        return;
#if ENABLE(SCREEN_TIME)
    if ([[m_view _screenTimeWebpageController] URLIsBlocked])
        return;
#endif

    nativeMouseEventHandler(event);
}

void WebViewImpl::createFlagsChangedEventMonitor()
{
    if (m_flagsChangedEventMonitor)
        return;

    WeakPtr weakThis { *this };
    m_flagsChangedEventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskFlagsChanged handler:[weakThis] (NSEvent *flagsChangedEvent) {
        if (weakThis)
            weakThis->scheduleMouseDidMoveOverElement(flagsChangedEvent);
        return flagsChangedEvent;
    }];
}

void WebViewImpl::removeFlagsChangedEventMonitor()
{
    if (!m_flagsChangedEventMonitor)
        return;

    RetainPtr flagsChangedEventMonitor = m_flagsChangedEventMonitor;
    [NSEvent removeMonitor:flagsChangedEventMonitor.get()];
    m_flagsChangedEventMonitor = nil;
}

bool WebViewImpl::hasFlagsChangedEventMonitor()
{
    return m_flagsChangedEventMonitor;
}

void WebViewImpl::mouseEntered(NSEvent *event)
{
    if (m_ignoresMouseMoveEvents)
        return;

    if (event.trackingArea == m_flagsChangedEventMonitorTrackingArea.get()) {
        createFlagsChangedEventMonitor();
        return;
    }

    nativeMouseEventHandler(event);
}

void WebViewImpl::mouseExited(NSEvent *event)
{
    if (m_ignoresMouseMoveEvents)
        return;

    if (event.trackingArea == m_flagsChangedEventMonitorTrackingArea.get()) {
        removeFlagsChangedEventMonitor();
        return;
    }

    nativeMouseEventHandler(event);
}

void WebViewImpl::otherMouseDown(NSEvent *event)
{
    nativeMouseEventHandler(event);
}

void WebViewImpl::otherMouseDragged(NSEvent *event)
{
    nativeMouseEventHandler(event);
}

void WebViewImpl::otherMouseUp(NSEvent *event)
{
    nativeMouseEventHandler(event);
}

void WebViewImpl::rightMouseDown(NSEvent *event)
{
    nativeMouseEventHandler(event);
}

void WebViewImpl::rightMouseDragged(NSEvent *event)
{
    nativeMouseEventHandler(event);
}

void WebViewImpl::rightMouseUp(NSEvent *event)
{
    nativeMouseEventHandler(event);
}

void WebViewImpl::mouseMovedInternal(NSEvent *event)
{
    nativeMouseEventHandlerInternal(event);
}

void WebViewImpl::mouseDownInternal(NSEvent *event)
{
    nativeMouseEventHandlerInternal(event);
}

void WebViewImpl::mouseUpInternal(NSEvent *event)
{
    nativeMouseEventHandlerInternal(event);
}

void WebViewImpl::mouseDraggedInternal(NSEvent *event)
{
    nativeMouseEventHandlerInternal(event);
}

void WebViewImpl::mouseMoved(NSEvent *event)
{
    if (m_ignoresNonWheelEvents || m_ignoresMouseMoveEvents)
        return;

    for (auto& hud : _pdfHUDViews.values())
        [hud mouseMoved:event];

    // When a view is first responder, it gets mouse moved events even when the mouse is outside its visible rect.
    RetainPtr view = m_view.get();
    if (view == [view window].firstResponder && !NSPointInRect([view convertPoint:[event locationInWindow] fromView:nil], [view visibleRect]))
        return;

    mouseMovedInternal(event);
}

static _WKRectEdge toWKRectEdge(WebCore::RectEdges<bool> edges)
{
    _WKRectEdge result = _WKRectEdgeNone;

    if (edges.left())
        result |= _WKRectEdgeLeft;

    if (edges.right())
        result |= _WKRectEdgeRight;

    if (edges.top())
        result |= _WKRectEdgeTop;

    if (edges.bottom())
        result |= _WKRectEdgeBottom;

    return result;
}

static WebCore::RectEdges<bool> toRectEdges(_WKRectEdge edges)
{
    return {
        static_cast<bool>(edges & _WKRectEdgeTop),
        static_cast<bool>(edges & _WKRectEdgeRight),
        static_cast<bool>(edges & _WKRectEdgeBottom),
        static_cast<bool>(edges & _WKRectEdgeLeft),
    };
}

_WKRectEdge WebViewImpl::pinnedState()
{
    return toWKRectEdge(m_page->pinnedState());
}

_WKRectEdge WebViewImpl::rubberBandingEnabled()
{
    return toWKRectEdge(m_page->rubberBandableEdges());
}

void WebViewImpl::setRubberBandingEnabled(_WKRectEdge state)
{
    m_page->setRubberBandableEdges(toRectEdges(state));
}

bool WebViewImpl::alwaysBounceVertical()
{
    return m_page->alwaysBounceVertical();
}

void WebViewImpl::setAlwaysBounceVertical(bool value)
{
    m_page->setAlwaysBounceVertical(value);
}

bool WebViewImpl::alwaysBounceHorizontal()
{
    return m_page->alwaysBounceHorizontal();
}

void WebViewImpl::setAlwaysBounceHorizontal(bool value)
{
    m_page->setAlwaysBounceHorizontal(value);
}

void WebViewImpl::mouseDown(NSEvent *event)
{
    if (m_ignoresNonWheelEvents)
        return;

    setLastMouseDownEvent(event);
    setIgnoresMouseDraggedEvents(false);

    for (auto& hud : _pdfHUDViews.values()) {
        if ([hud handleMouseDown:event])
            return;
    }

    mouseDownInternal(event);
}

void WebViewImpl::mouseUp(NSEvent *event)
{
    if (m_ignoresNonWheelEvents)
        return;

    setLastMouseDownEvent(nil);

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    fulfillDeferredImageAnalysisOverlayViewHierarchyTask();
#endif

    for (auto& hud : _pdfHUDViews.values()) {
        if ([hud handleMouseUp:event])
            return;
    }

    mouseUpInternal(event);
}

void WebViewImpl::mouseDragged(NSEvent *event)
{
    if (m_ignoresNonWheelEvents)
        return;
    if (ignoresMouseDraggedEvents())
        return;

    mouseDraggedInternal(event);
}

bool WebViewImpl::windowIsFrontWindowUnderMouse(NSEvent *event)
{
    RetainPtr window = WebViewImpl::window();
    NSRect eventScreenPosition = [window convertRectToScreen:NSMakeRect(event.locationInWindow.x, event.locationInWindow.y, 0, 0)];
    NSInteger eventWindowNumber = [NSWindow windowNumberAtPoint:eventScreenPosition.origin belowWindowWithWindowNumber:0];

    return window.get().windowNumber != eventWindowNumber;
}

static WebCore::UserInterfaceLayoutDirection toUserInterfaceLayoutDirection(NSUserInterfaceLayoutDirection direction)
{
    switch (direction) {
    case NSUserInterfaceLayoutDirectionLeftToRight:
        return WebCore::UserInterfaceLayoutDirection::LTR;
    case NSUserInterfaceLayoutDirectionRightToLeft:
        return WebCore::UserInterfaceLayoutDirection::RTL;
    }

    ASSERT_NOT_REACHED();
    return WebCore::UserInterfaceLayoutDirection::LTR;
}

WebCore::UserInterfaceLayoutDirection WebViewImpl::userInterfaceLayoutDirection()
{
    return toUserInterfaceLayoutDirection([m_view.get() userInterfaceLayoutDirection]);
}

void WebViewImpl::setUserInterfaceLayoutDirection(NSUserInterfaceLayoutDirection direction)
{
    m_page->setUserInterfaceLayoutDirection(toUserInterfaceLayoutDirection(direction));
}

bool WebViewImpl::beginBackSwipeForTesting()
{
    if (!m_allowsBackForwardNavigationGestures)
        return false;

    return ensureProtectedGestureController()->beginSimulatedSwipeInDirectionForTesting(ViewGestureController::SwipeDirection::Back);
}

bool WebViewImpl::completeBackSwipeForTesting()
{
    RefPtr gestureController = m_gestureController;
    return gestureController && gestureController->completeSimulatedSwipeInDirectionForTesting(ViewGestureController::SwipeDirection::Back);
}

bool WebViewImpl::didCallEndSwipeGestureForTesting() const
{
    RefPtr gestureController = m_gestureController;
    return gestureController && gestureController->didCallEndSwipeGesture();
}

void WebViewImpl::effectiveAppearanceDidChange()
{
    m_page->effectiveAppearanceDidChange();

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    updateTopScrollPocketCaptureColor();
#endif
}

bool WebViewImpl::effectiveAppearanceIsDark()
{
    RetainPtr appearance = [retainPtr([m_view.get() effectiveAppearance]) bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    return [appearance isEqualToString:NSAppearanceNameDarkAqua];
}

bool WebViewImpl::effectiveUserInterfaceLevelIsElevated()
{
    return false;
}

bool WebViewImpl::useFormSemanticContext() const
{
    return [m_view.get() _semanticContext] == NSViewSemanticContextForm;
}

void WebViewImpl::semanticContextDidChange()
{
    m_page->semanticContextDidChange();
}

#if HAVE(TOUCH_BAR)

NSTouchBar *WebViewImpl::makeTouchBar()
{
    if (!m_canCreateTouchBars) {
        m_canCreateTouchBars = true;
        updateTouchBar();
    }
    return m_currentTouchBar.get();
}

bool WebViewImpl::requiresUserActionForEditingControlsManager() const
{
    return m_page->configuration().requiresUserActionForEditingControlsManager();
}

void WebViewImpl::updateTouchBar()
{
    if (!m_canCreateTouchBars)
        return;

    RetainPtr<NSTouchBar> touchBar;
    bool userActionRequirementsHaveBeenMet = !requiresUserActionForEditingControlsManager() || m_page->hasFocusedElementWithUserInteraction();
    if (m_page->editorState().isContentEditable && !m_page->isTouchBarUpdateSuppressedForHiddenContentEditable()) {
        updateTextTouchBar();
        if (userActionRequirementsHaveBeenMet)
            touchBar = textTouchBar();
    }
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    else if (m_page->hasActiveVideoForControlsManager()) {
        updateMediaTouchBar();
        // If useMediaPlaybackControlsView() is true, then we are relying on the API client to display a popover version
        // of the media timeline in their own function bar. If it is false, then we will display the media timeline in our
        // function bar.
        if (!useMediaPlaybackControlsView())
            touchBar = [m_mediaTouchBarProvider respondsToSelector:@selector(touchBar)] ? [(id)m_mediaTouchBarProvider.get() touchBar] : [(id)m_mediaTouchBarProvider.get() touchBar];
    } else if ([m_mediaTouchBarProvider playbackControlsController]) {
        if (m_clientWantsMediaPlaybackControlsView) {
            if ([m_view.get() respondsToSelector:@selector(_web_didRemoveMediaControlsManager)] && m_view.getAutoreleased() == protectedWindow().get().firstResponder)
                [m_view.get() _web_didRemoveMediaControlsManager];
        }
        [m_mediaTouchBarProvider setPlaybackControlsController:nil];
        [m_mediaPlaybackControlsView setPlaybackControlsController:nil];
    }
#endif

    if (touchBar.get() == m_currentTouchBar)
        return;

    // If m_editableElementIsFocused is true, then we may have a non-editable selection right now just because
    // the user is clicking or tabbing between editable fields.
    if (m_editableElementIsFocused && touchBar.get() != textTouchBar())
        return;

    m_currentTouchBar = touchBar.get();
    [m_view.get() willChangeValueForKey:@"touchBar"];
    [m_view.get() setTouchBar:m_currentTouchBar.get()];
    [m_view.get() didChangeValueForKey:@"touchBar"];
}

NSCandidateListTouchBarItem *WebViewImpl::candidateListTouchBarItem() const
{
    if (m_page->editorState().isInPasswordField)
        return m_passwordTextCandidateListTouchBarItem.get();
    return isRichlyEditableForTouchBar() ? m_richTextCandidateListTouchBarItem.get() : m_plainTextCandidateListTouchBarItem.get();
}

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
AVTouchBarScrubber *WebViewImpl::mediaPlaybackControlsView() const
{
    if (m_page->hasActiveVideoForControlsManager())
        return m_mediaPlaybackControlsView.get();
    return nil;
}
#endif

bool WebViewImpl::useMediaPlaybackControlsView() const
{
#if ENABLE(FULLSCREEN_API)
    if (hasFullScreenWindowController())
        return ![m_fullScreenWindowController isFullScreen];
#endif
    return m_clientWantsMediaPlaybackControlsView;
}

void WebViewImpl::dismissTextTouchBarPopoverItemWithIdentifier(NSString *identifier)
{
    NSTouchBarItem *foundItem = nil;
    for (NSTouchBarItem *item in RetainPtr { textTouchBar() }.get().items) {
        if ([item.identifier isEqualToString:identifier]) {
            foundItem = item;
            break;
        }

        if ([item.identifier isEqualToString:NSTouchBarItemIdentifierTextFormat]) {
            for (NSTouchBarItem *childItem in checked_objc_cast<NSGroupTouchBarItem>(item).groupTouchBar.items) {
                if ([childItem.identifier isEqualToString:identifier]) {
                    foundItem = childItem;
                    break;
                }
            }
            break;
        }
    }

    if (RetainPtr touchBarItem = dynamic_objc_cast<NSPopoverTouchBarItem>(foundItem))
        [touchBarItem dismissPopover:nil];
}

void WebViewImpl::updateTouchBarAndRefreshTextBarIdentifiers()
{
    if (m_richTextTouchBar)
        setUpTextTouchBar(m_richTextTouchBar.get());

    if (m_plainTextTouchBar)
        setUpTextTouchBar(m_plainTextTouchBar.get());

    if (m_passwordTextTouchBar)
        setUpTextTouchBar(m_passwordTextTouchBar.get());

    updateTouchBar();
}

void WebViewImpl::setUpTextTouchBar(NSTouchBar *touchBar)
{
    RetainPtr<NSSet<NSTouchBarItem *>> templateItems;
    RetainPtr<NSArray<NSTouchBarItemIdentifier>> defaultItemIdentifiers;
    RetainPtr<NSArray<NSTouchBarItemIdentifier>> customizationAllowedItemIdentifiers;

    if (touchBar == m_passwordTextTouchBar.get()) {
        templateItems = [NSMutableSet setWithObject:m_passwordTextCandidateListTouchBarItem.get()];
        defaultItemIdentifiers = passwordTextTouchBarDefaultItemIdentifiers();
    } else if (touchBar == m_richTextTouchBar.get()) {
        templateItems = [NSMutableSet setWithObject:m_richTextCandidateListTouchBarItem.get()];
        defaultItemIdentifiers = richTextTouchBarDefaultItemIdentifiers();
        customizationAllowedItemIdentifiers = textTouchBarCustomizationAllowedIdentifiers();
    } else if (touchBar == m_plainTextTouchBar.get()) {
        templateItems = [NSMutableSet setWithObject:m_plainTextCandidateListTouchBarItem.get()];
        defaultItemIdentifiers = plainTextTouchBarDefaultItemIdentifiers();
        customizationAllowedItemIdentifiers = textTouchBarCustomizationAllowedIdentifiers();
    }

    [touchBar setDelegate:m_textTouchBarItemController.get()];
    [touchBar setTemplateItems:templateItems.get()];
    [touchBar setDefaultItemIdentifiers:defaultItemIdentifiers.get()];
    [touchBar setCustomizationAllowedItemIdentifiers:customizationAllowedItemIdentifiers.get()];

    if (RetainPtr textFormatItem = checked_objc_cast<NSGroupTouchBarItem>([touchBar itemForIdentifier:NSTouchBarItemIdentifierTextFormat]))
        textFormatItem.get().groupTouchBar.customizationIdentifier = @"WKTextFormatTouchBar";
}

bool WebViewImpl::isRichlyEditableForTouchBar() const
{
    return m_page->editorState().isContentRichlyEditable && !m_page->isNeverRichlyEditableForTouchBar();
}

NSTouchBar *WebViewImpl::textTouchBar() const
{
    if (m_page->editorState().isInPasswordField)
        return m_passwordTextTouchBar.get();

    return isRichlyEditableForTouchBar() ? m_richTextTouchBar.get() : m_plainTextTouchBar.get();
}

static NSTextAlignment nsTextAlignmentFromTextAlignment(TextAlignment textAlignment)
{
    switch (textAlignment) {
    case TextAlignment::Natural:
        return NSTextAlignmentNatural;
    case TextAlignment::Left:
        return NSTextAlignmentLeft;
    case TextAlignment::Right:
        return NSTextAlignmentRight;
    case TextAlignment::Center:
        return NSTextAlignmentCenter;
    case TextAlignment::Justified:
        return NSTextAlignmentJustified;
    }
    ASSERT_NOT_REACHED();
    return NSTextAlignmentNatural;
}

void WebViewImpl::updateTextTouchBar()
{
    if (!m_page->editorState().isContentEditable)
        return;

    if (m_isUpdatingTextTouchBar)
        return;

    SetForScope isUpdatingTextFunctionBar(m_isUpdatingTextTouchBar, true);

    if (!m_textTouchBarItemController)
        m_textTouchBarItemController = adoptNS([[WKTextTouchBarItemController alloc] initWithWebViewImpl:this]);

    if (!m_startedListeningToCustomizationEvents) {
        [[NSNotificationCenter defaultCenter] addObserver:m_textTouchBarItemController.get() selector:@selector(touchBarDidExitCustomization:) name:NSTouchBarDidExitCustomization object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:m_textTouchBarItemController.get() selector:@selector(touchBarWillEnterCustomization:) name:NSTouchBarWillEnterCustomization object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:m_textTouchBarItemController.get() selector:@selector(didChangeAutomaticTextCompletion:) name:NSSpellCheckerDidChangeAutomaticTextCompletionNotification object:nil];

        m_startedListeningToCustomizationEvents = true;
    }

    if (!m_richTextCandidateListTouchBarItem || !m_plainTextCandidateListTouchBarItem || !m_passwordTextCandidateListTouchBarItem) {
        m_richTextCandidateListTouchBarItem = adoptNS([[NSCandidateListTouchBarItem alloc] initWithIdentifier:NSTouchBarItemIdentifierCandidateList]);
        [m_richTextCandidateListTouchBarItem setDelegate:m_textTouchBarItemController.get()];
        m_plainTextCandidateListTouchBarItem = adoptNS([[NSCandidateListTouchBarItem alloc] initWithIdentifier:NSTouchBarItemIdentifierCandidateList]);
        [m_plainTextCandidateListTouchBarItem setDelegate:m_textTouchBarItemController.get()];
        m_passwordTextCandidateListTouchBarItem = adoptNS([[NSCandidateListTouchBarItem alloc] initWithIdentifier:NSTouchBarItemIdentifierCandidateList]);
        [m_passwordTextCandidateListTouchBarItem setDelegate:m_textTouchBarItemController.get()];
        requestCandidatesForSelectionIfNeeded();
    }

    if (!m_richTextTouchBar) {
        m_richTextTouchBar = adoptNS([[NSTouchBar alloc] init]);
        setUpTextTouchBar(m_richTextTouchBar.get());
        [m_richTextTouchBar setCustomizationIdentifier:@"WKRichTextTouchBar"];
    }

    if (!m_plainTextTouchBar) {
        m_plainTextTouchBar = adoptNS([[NSTouchBar alloc] init]);
        setUpTextTouchBar(m_plainTextTouchBar.get());
        [m_plainTextTouchBar setCustomizationIdentifier:@"WKPlainTextTouchBar"];
    }

    if ([NSSpellChecker isAutomaticTextCompletionEnabled] && !m_isCustomizingTouchBar) {
        BOOL showCandidatesList = !m_page->editorState().selectionIsRange || m_isHandlingAcceptedCandidate;
        RetainPtr candidateListTouchBarItem = WebViewImpl::candidateListTouchBarItem();
        [candidateListTouchBarItem updateWithInsertionPointVisibility:showCandidatesList];
        [m_view.get() _didUpdateCandidateListVisibility:showCandidatesList];
    }

    if (m_page->editorState().isInPasswordField) {
        if (!m_passwordTextTouchBar) {
            m_passwordTextTouchBar = adoptNS([[NSTouchBar alloc] init]);
            setUpTextTouchBar(m_passwordTextTouchBar.get());
        }
        [m_passwordTextCandidateListTouchBarItem setCandidates:@[ ] forSelectedRange:NSMakeRange(0, 0) inString:nil];
    }

    RetainPtr textTouchBar = this->textTouchBar();
    BOOL isShowingCombinedTextFormatItem = [retainPtr(textTouchBar.get().defaultItemIdentifiers) containsObject:NSTouchBarItemIdentifierTextFormat];
    [textTouchBar setPrincipalItemIdentifier:isShowingCombinedTextFormatItem ? NSTouchBarItemIdentifierTextFormat : nil];

    // Set current typing attributes for rich text. This will ensure that the buttons reflect the state of
    // the text when changing selection throughout the document.
    if (isRichlyEditableForTouchBar()) {
        const EditorState& editorState = m_page->editorState();
        if (editorState.hasPostLayoutData()) {
            [m_textTouchBarItemController setTextIsBold:(m_page->editorState().postLayoutData->typingAttributes.contains(TypingAttribute::Bold))];
            [m_textTouchBarItemController setTextIsItalic:(m_page->editorState().postLayoutData->typingAttributes.contains(TypingAttribute::Italics))];
            [m_textTouchBarItemController setTextIsUnderlined:(m_page->editorState().postLayoutData->typingAttributes.contains(TypingAttribute::Underline))];
            [m_textTouchBarItemController setTextColor:cocoaColor(editorState.postLayoutData->textColor).get()];
            [[m_textTouchBarItemController textListTouchBarViewController] setCurrentListType:(ListType)m_page->editorState().postLayoutData->enclosingListType];
            [m_textTouchBarItemController setCurrentTextAlignment:nsTextAlignmentFromTextAlignment((TextAlignment)editorState.postLayoutData->textAlignment)];
        }
        BOOL isShowingCandidateListItem = [retainPtr(textTouchBar.get().defaultItemIdentifiers) containsObject:NSTouchBarItemIdentifierCandidateList] && [NSSpellChecker isAutomaticTextReplacementEnabled];
        [m_textTouchBarItemController setUsesNarrowTextStyleItem:isShowingCombinedTextFormatItem && isShowingCandidateListItem];
    }
}

bool WebViewImpl::isContentRichlyEditable() const
{
    return m_page->editorState().isContentRichlyEditable;
}

#if ENABLE(MULTI_REPRESENTATION_HEIC)
void WebViewImpl::insertMultiRepresentationHEIC(NSData *data, NSString *altText)
{
    m_page->insertMultiRepresentationHEIC(data, altText);
}
#endif

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

bool WebViewImpl::isPictureInPictureActive()
{
    return [m_playbackControlsManager isPictureInPictureActive];
}

void WebViewImpl::togglePictureInPicture()
{
    [m_playbackControlsManager togglePictureInPicture];
}


PlatformPlaybackSessionInterface* WebViewImpl::playbackSessionInterface() const
{
    if (RefPtr manager = m_page->playbackSessionManager())
        return manager->controlsManagerInterface().unsafeGet();

    return nullptr;
}

bool WebViewImpl::isInWindowFullscreenActive() const
{
    if (RefPtr interface = playbackSessionInterface())
        return interface->isInWindowFullscreenActive();

    return false;
}

void WebViewImpl::enterInWindowFullscreen()
{
    if (RefPtr interface = playbackSessionInterface())
        return interface->enterInWindowFullscreen();
}

void WebViewImpl::exitInWindowFullscreen()
{
    if (RefPtr interface = playbackSessionInterface())
        return interface->exitInWindowFullscreen();
}

void WebViewImpl::updateMediaPlaybackControlsManager()
{
    if (!m_page->hasActiveVideoForControlsManager())
        return;

    if (!m_playbackControlsManager) {
        m_playbackControlsManager = adoptNS([[WebPlaybackControlsManager alloc] init]);
        [m_playbackControlsManager setAllowsPictureInPicturePlayback:m_page->protectedPreferences()->allowsPictureInPictureMediaPlayback()];
        [m_playbackControlsManager setCanTogglePictureInPicture:NO];
    }

    if (RefPtr interface = playbackSessionInterface()) {
        [m_playbackControlsManager setPlaybackSessionInterfaceMac:interface.get()];
        interface->updatePlaybackControlsManagerCanTogglePictureInPicture();
    }
}

#endif // ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

void WebViewImpl::nowPlayingMediaTitleAndArtist(void(^completionHandler)(NSString *, NSString *))
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    if (!m_page->hasActiveVideoForControlsManager()) {
        completionHandler(nil, nil);
        return;
    }

    m_page->requestActiveNowPlayingSessionInfo([completionHandler = makeBlockPtr(completionHandler)] (bool registeredAsNowPlayingApplication, WebCore::NowPlayingInfo&& nowPlayingInfo) {
        completionHandler(nowPlayingInfo.metadata.title.createNSString().get(), nowPlayingInfo.metadata.artist.createNSString().get());
    });
#else
    completionHandler(nil, nil);
#endif
}

void WebViewImpl::updateMediaTouchBar()
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER) && ENABLE(VIDEO_PRESENTATION_MODE)
    if (!m_mediaTouchBarProvider) {
        m_mediaTouchBarProvider = adoptNS([allocAVTouchBarPlaybackControlsProviderInstance() init]);
    }

    if (!m_mediaPlaybackControlsView) {
        m_mediaPlaybackControlsView = adoptNS([allocAVTouchBarScrubberInstance() init]);
        // FIXME: Remove this once setCanShowMediaSelectionButton: is declared in an SDK used by Apple's buildbot.
        if ([m_mediaPlaybackControlsView respondsToSelector:@selector(setCanShowMediaSelectionButton:)])
            [m_mediaPlaybackControlsView setCanShowMediaSelectionButton:YES];
    }

    updateMediaPlaybackControlsManager();

    [m_mediaTouchBarProvider setPlaybackControlsController:m_playbackControlsManager.get()];
    [m_mediaPlaybackControlsView setPlaybackControlsController:m_playbackControlsManager.get()];

    if (!useMediaPlaybackControlsView()) {
#if ENABLE(FULLSCREEN_API)
        // If we can't have a media popover function bar item, it might be because we are in full screen.
        // If so, customize the escape key.
        RetainPtr touchBar = [m_mediaTouchBarProvider respondsToSelector:@selector(touchBar)] ? [(id)m_mediaTouchBarProvider.get() touchBar] : [(id)m_mediaTouchBarProvider.get() touchBar];
        if (hasFullScreenWindowController() && [m_fullScreenWindowController isFullScreen]) {
            if (!m_exitFullScreenButton) {
                m_exitFullScreenButton = adoptNS([[NSCustomTouchBarItem alloc] initWithIdentifier:WKMediaExitFullScreenItem]);

                RetainPtr image = [NSImage imageNamed:NSImageNameTouchBarExitFullScreenTemplate];
                [image setTemplate:YES];

                RetainPtr exitFullScreenButton = [NSButton buttonWithTitle:image ? @"" : @"Exit" image:image.get() target:m_fullScreenWindowController.get() action:@selector(requestExitFullScreen)];
                [exitFullScreenButton setAccessibilityTitle:WebCore::exitFullScreenButtonAccessibilityTitle().createNSString().get()];

                [[retainPtr(exitFullScreenButton.get().widthAnchor) constraintLessThanOrEqualToConstant:exitFullScreenButtonWidth] setActive:YES];
                [m_exitFullScreenButton setView:exitFullScreenButton.get()];
            }
            touchBar.get().escapeKeyReplacementItem = m_exitFullScreenButton.get();
        } else
            touchBar.get().escapeKeyReplacementItem = nil;
#endif
        // The rest of the work to update the media function bar only applies to the popover version, so return early.
        return;
    }

    if (m_playbackControlsManager && m_view.getAutoreleased() == protectedWindow().get().firstResponder && [m_view.get() respondsToSelector:@selector(_web_didAddMediaControlsManager:)])
        [m_view.get() _web_didAddMediaControlsManager:m_mediaPlaybackControlsView.get()];
#endif
}

bool WebViewImpl::canTogglePictureInPicture()
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    return [m_playbackControlsManager canTogglePictureInPicture];
#else
    return NO;
#endif
}

void WebViewImpl::forceRequestCandidatesForTesting()
{
    m_canCreateTouchBars = true;
    updateTouchBar();
}

bool WebViewImpl::shouldRequestCandidates() const
{
    if (m_page->editorState().isInPasswordField)
        return false;

    RetainPtr candidateListTouchBarItem = WebViewImpl::candidateListTouchBarItem();
    if (candidateListTouchBarItem.get().candidateListVisible)
        return true;

#if HAVE(INLINE_PREDICTIONS)
    if (allowsInlinePredictions())
        return true;
#endif

    return false;
}

void WebViewImpl::setEditableElementIsFocused(bool editableElementIsFocused)
{
    m_editableElementIsFocused = editableElementIsFocused;

    // If the editable elements have blurred, then we might need to get rid of the editing function bar.
    if (!m_editableElementIsFocused)
        updateTouchBar();
}

#else // !HAVE(TOUCH_BAR)

void WebViewImpl::forceRequestCandidatesForTesting()
{
}

bool WebViewImpl::shouldRequestCandidates() const
{
    return false;
}

void WebViewImpl::setEditableElementIsFocused(bool editableElementIsFocused)
{
    m_editableElementIsFocused = editableElementIsFocused;
}

#endif // HAVE(TOUCH_BAR)

#if HAVE(REDESIGNED_TEXT_CURSOR)
void WebViewImpl::updateCursorAccessoryPlacement()
{
    const EditorState& editorState = m_page->editorState();
    if (!editorState.hasPostLayoutData())
        return;

    auto& postLayoutData = *editorState.postLayoutData;

    RetainPtr context = [m_view.get() _web_superInputContext];
    if (!context)
        return;

    if ([m_textInputNotifications caretType] == WebCore::CaretAnimatorType::Dictation) {
        // The dictation cursor accessory should always be visible no matter what, since it is
        // the only prominent way a user can tell if dictation is active.
        context.get().showsCursorAccessories = YES;
        return;
    }

    // Otherwise, the cursor accessory should be hidden if it will not show up in the correct position.
    context.get().showsCursorAccessories = !postLayoutData.selectionIsTransparentOrFullyClipped;
}
#endif

#if HAVE(REDESIGNED_TEXT_CURSOR) && PLATFORM(MAC)
static RetainPtr<_WKWebViewTextInputNotifications> subscribeToTextInputNotifications(WebViewImpl* webView)
{
    if (!WebCore::redesignedTextCursorEnabled())
        return nullptr;

    auto textInputNotifications = adoptNS([[_WKWebViewTextInputNotifications alloc] initWithWebView:webView]);

    [[NSNotificationCenter defaultCenter] addObserver:textInputNotifications.get() selector:@selector(dictationDidStart) name:@"_NSTextInputContextDictationDidStartNotification" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:textInputNotifications.get() selector:@selector(dictationDidEnd) name:@"_NSTextInputContextDictationDidEndNotification" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:textInputNotifications.get() selector:@selector(dictationDidPause) name:@"_NSTextInputContextDictationDidPauseNotification" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:textInputNotifications.get() selector:@selector(dictationDidResume) name:@"_NSTextInputContextDictationDidResumeNotification" object:nil];

    return textInputNotifications;
}
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
void WebViewImpl::setMediaSessionCoordinatorForTesting(MediaSessionCoordinatorProxyPrivate* coordinator)
{
    m_coordinatorForTesting = coordinator;
}
#endif

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)

bool WebViewImpl::canHandleContextMenuTranslation() const
{
    return PAL::isTranslationUIServicesFrameworkAvailable() && [PAL::getLTUITranslationViewControllerClassSingleton() isAvailable];
}

void WebViewImpl::handleContextMenuTranslation(const WebCore::TranslationContextMenuInfo& info)
{
    if (!canHandleContextMenuTranslation()) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto view = m_view.get();
    auto translationViewController = adoptNS([PAL::allocLTUITranslationViewControllerInstance() init]);
    [translationViewController setText:adoptNS([[NSAttributedString alloc] initWithString:info.text.createNSString().get()]).get()];
    if (info.mode == WebCore::TranslationContextMenuMode::Editable) {
        [translationViewController setIsSourceEditable:YES];
        [translationViewController setReplacementHandler:[weakThis = WeakPtr { *this }](NSAttributedString *string) {
            if (CheckedPtr checkedThis = weakThis.get())
                checkedThis->insertText(retainPtr(string.string).get());
        }];
    }

    auto sourceMetadata = adoptNS([PAL::allocLTUISourceMetaInstance() init]);
    [sourceMetadata setOrigin:info.source == WebCore::TranslationContextMenuSource::Image ? LTUISourceMetaOriginImage : LTUISourceMetaOriginUnspecified];
    [translationViewController setSourceMeta:sourceMetadata.get()];

    if (NSEqualSizes([translationViewController preferredContentSize], NSZeroSize))
        [translationViewController setPreferredContentSize:NSMakeSize(400, 400)];

    auto popover = adoptNS([[NSPopover alloc] init]);
    [popover setBehavior:NSPopoverBehaviorTransient];
    [popover setAppearance:[view effectiveAppearance]];
    [popover setAnimates:YES];
    [popover setContentViewController:translationViewController.get()];
    [popover setContentSize:[translationViewController preferredContentSize]];

    NSRectEdge preferredEdge;
    auto aim = info.locationInRootView.x();
    auto highlight = info.selectionBoundsInRootView.center().x();
    if (aim == highlight)
        preferredEdge = [view userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft ? NSRectEdgeMinX : NSRectEdgeMaxX;
    else
        preferredEdge = aim > highlight ? NSRectEdgeMaxX : NSRectEdgeMinX;

    m_lastContextMenuTranslationPopover = popover.get();
    [popover showRelativeToRect:info.selectionBoundsInRootView ofView:view.get() preferredEdge:preferredEdge];
}

#endif // HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)

#if ENABLE(WRITING_TOOLS) && ENABLE(CONTEXT_MENUS)

bool WebViewImpl::canHandleContextMenuWritingTools() const
{
    return PAL::isWritingToolsUIFrameworkAvailable() && [PAL::getWTWritingToolsViewControllerClassSingleton() isAvailable] && m_page->writingToolsBehavior() != WebCore::WritingTools::Behavior::None;
}

#endif

bool WebViewImpl::acceptsPreviewPanelControl(QLPreviewPanel *)
{
#if ENABLE(IMAGE_ANALYSIS)
    return !!m_page->quickLookPreviewController();
#else
    return false;
#endif
}

void WebViewImpl::beginPreviewPanelControl(QLPreviewPanel *panel)
{
    m_page->beginPreviewPanelControl(panel);
}

void WebViewImpl::endPreviewPanelControl(QLPreviewPanel *panel)
{
    m_page->endPreviewPanelControl(panel);
}

#if ENABLE(DATA_DETECTION)

void WebViewImpl::handleClickForDataDetectionResult(const WebCore::DataDetectorElementInfo& info, const WebCore::IntPoint& clickLocation)
{
#if ENABLE(REVEAL)
    m_revealItemPresenter = adoptNS([[WKRevealItemPresenter alloc] initWithWebViewImpl:*this item:adoptNS([PAL::allocRVItemInstance() initWithDDResult:info.result.get()]).get() frame:info.elementBounds menuLocation:clickLocation]);
    [m_revealItemPresenter setShouldUseDefaultHighlight:NO];
    [m_revealItemPresenter showContextMenu];
#else
    UNUSED_PARAM(info);
    UNUSED_PARAM(clickLocation);
#endif
}

#endif // ENABLE(DATA_DETECTION)

#if ENABLE(REVEAL)

void WebViewImpl::didFinishPresentation(WKRevealItemPresenter *presenter)
{
    if (presenter == m_revealItemPresenter)
        m_revealItemPresenter = nil;
}

#endif // ENABLE(REVEAL)


#if ENABLE(IMAGE_ANALYSIS)

CocoaImageAnalyzer* WebViewImpl::ensureImageAnalyzer()
{
    if (!m_imageAnalyzer) {
        lazyInitialize(m_imageAnalyzerQueue, WorkQueue::create("WebKit image analyzer queue"_s));
        lazyInitialize(m_imageAnalyzer, createImageAnalyzer());
        [m_imageAnalyzer setCallbackQueue:m_imageAnalyzerQueue->protectedDispatchQueue().get()];
    }
    return m_imageAnalyzer.get();
}

RetainPtr<CocoaImageAnalyzer> WebViewImpl::ensureProtectedImageAnalyzer()
{
    return ensureImageAnalyzer();
}

int32_t WebViewImpl::processImageAnalyzerRequest(CocoaImageAnalyzerRequest *request, CompletionHandler<void(RetainPtr<CocoaImageAnalysis>&&, NSError *)>&& completion)
{
    return [ensureProtectedImageAnalyzer() processRequest:request progressHandler:nil completionHandler:makeBlockPtr([completion = WTFMove(completion)](CocoaImageAnalysis *result, NSError *error) mutable {
        callOnMainRunLoop([completion = WTFMove(completion), result = RetainPtr { result }, error = RetainPtr { error }] mutable {
            completion(WTFMove(result), error.get());
        });
    }).get()];
}

static RetainPtr<CocoaImageAnalyzerRequest> createImageAnalyzerRequest(CGImageRef image, const URL& imageURL, const URL& pageURL, VKAnalysisTypes types)
{
    auto request = createImageAnalyzerRequest(image, types);
    [request setImageURL:imageURL.createNSURL().get()];
    [request setPageURL:pageURL.createNSURL().get()];
    return request;
}

void WebViewImpl::requestTextRecognition(const URL& imageURL, ShareableBitmap::Handle&& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(WebCore::TextRecognitionResult&&)>&& completion)
{
    if (!isLiveTextAvailableAndEnabled()) {
        completion({ });
        return;
    }

    auto imageBitmap = ShareableBitmap::create(WTFMove(imageData));
    if (!imageBitmap) {
        completion({ });
        return;
    }

    RetainPtr cgImage = imageBitmap->createPlatformImage(DontCopyBackingStore);

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if (!targetLanguageIdentifier.isEmpty())
        return requestVisualTranslation(ensureProtectedImageAnalyzer().get(), imageURL.createNSURL().get(), sourceLanguageIdentifier, targetLanguageIdentifier, cgImage.get(), WTFMove(completion));
#else
    UNUSED_PARAM(sourceLanguageIdentifier);
    UNUSED_PARAM(targetLanguageIdentifier);
#endif

    auto request = createImageAnalyzerRequest(cgImage.get(), imageURL, [NSURL _web_URLWithWTFString:m_page->currentURL()], VKAnalysisTypeText);
    auto startTime = MonotonicTime::now();
    processImageAnalyzerRequest(request.get(), [completion = WTFMove(completion), startTime](RetainPtr<CocoaImageAnalysis>&& analysis, NSError *) mutable {
        auto result = makeTextRecognitionResult(analysis.get());
        RELEASE_LOG(ImageAnalysis, "Image analysis completed in %.0f ms (found text? %d)", (MonotonicTime::now() - startTime).milliseconds(), !result.isEmpty());
        completion(WTFMove(result));
    });
}

void WebViewImpl::computeHasVisualSearchResults(const URL& imageURL, ShareableBitmap& imageBitmap, CompletionHandler<void(bool)>&& completion)
{
    if (!isLiveTextAvailableAndEnabled()) {
        completion(false);
        return;
    }

    RetainPtr cgImage = imageBitmap.createPlatformImage(DontCopyBackingStore);
    auto request = createImageAnalyzerRequest(cgImage.get(), imageURL, [NSURL _web_URLWithWTFString:m_page->currentURL()], VKAnalysisTypeVisualSearch);
    auto startTime = MonotonicTime::now();
    [ensureProtectedImageAnalyzer() processRequest:request.get() progressHandler:nil completionHandler:makeBlockPtr([completion = WTFMove(completion), startTime] (CocoaImageAnalysis *analysis, NSError *) mutable {
        BOOL result = [analysis hasResultsForAnalysisTypes:VKAnalysisTypeVisualSearch];
        RetainPtr loop = CFRunLoopGetMain();
        CFRunLoopPerformBlock(loop.get(), RetainPtr { bridge_cast(NSEventTrackingRunLoopMode) }.get(), makeBlockPtr([completion = WTFMove(completion), result, startTime] () mutable {
            RELEASE_LOG(ImageAnalysis, "Image analysis completed in %.0f ms (found visual search results? %d)", (MonotonicTime::now() - startTime).milliseconds(), result);
            completion(result);
        }).get());
        CFRunLoopWakeUp(loop.get());
    }).get()];
}

#endif // ENABLE(IMAGE_ANALYSIS)

bool WebViewImpl::imageAnalysisOverlayViewHasCursorAtPoint(NSPoint locationInView) const
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    return [m_imageAnalysisOverlayView interactableItemExistsAtPoint:locationInView];
#else
    UNUSED_PARAM(locationInView);
    return false;
#endif
}

void WebViewImpl::beginTextRecognitionForVideoInElementFullscreen(ShareableBitmap::Handle&& bitmapHandle, WebCore::FloatRect bounds)
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    auto imageBitmap = ShareableBitmap::create(WTFMove(bitmapHandle));
    if (!imageBitmap)
        return;

    RetainPtr image = imageBitmap->createPlatformImage(DontCopyBackingStore);
    if (!image)
        return;

    auto request = WebKit::createImageAnalyzerRequest(image.get(), VKAnalysisTypeText);
    m_currentImageAnalysisRequestID = processImageAnalyzerRequest(request.get(), [weakThis = WeakPtr { *this }, bounds](RetainPtr<CocoaImageAnalysis>&& result, NSError *error) {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis || !checkedThis->m_currentImageAnalysisRequestID)
            return;

        checkedThis->m_currentImageAnalysisRequestID = 0;
        if (error || !result)
            return;

        checkedThis->m_imageAnalysisInteractionBounds = bounds;
        checkedThis->installImageAnalysisOverlayView(WTFMove(result));
    });
#else
    UNUSED_PARAM(bitmapHandle);
    UNUSED_PARAM(bounds);
#endif
}

void WebViewImpl::cancelTextRecognitionForVideoInElementFullscreen()
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if (auto identifier = std::exchange(m_currentImageAnalysisRequestID, 0))
        [m_imageAnalyzer cancelRequestID:identifier];
    uninstallImageAnalysisOverlayView();
#endif
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

void WebViewImpl::installImageAnalysisOverlayView(RetainPtr<VKCImageAnalysis>&& analysis)
{
    auto installTask = [weakThis = WeakPtr { *this }, analysis = WTFMove(analysis)] {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis)
            return;

        if (!checkedThis->m_imageAnalysisOverlayView) {
            checkedThis->m_imageAnalysisOverlayView = adoptNS([PAL::allocVKCImageAnalysisOverlayViewInstance() initWithFrame:[checkedThis->m_view.get() bounds]]);
            checkedThis->m_imageAnalysisOverlayViewDelegate = adoptNS([[WKImageAnalysisOverlayViewDelegate alloc] initWithWebViewImpl:*checkedThis.get()]);
            [checkedThis->m_imageAnalysisOverlayView setDelegate:checkedThis->m_imageAnalysisOverlayViewDelegate.get()];
            prepareImageAnalysisForOverlayView(checkedThis->m_imageAnalysisOverlayView.get());
            RELEASE_LOG(ImageAnalysis, "Installing image analysis overlay view at {{ %.0f, %.0f }, { %.0f, %.0f }}",
                checkedThis->m_imageAnalysisInteractionBounds.x(), checkedThis->m_imageAnalysisInteractionBounds.y(), checkedThis->m_imageAnalysisInteractionBounds.width(), checkedThis->m_imageAnalysisInteractionBounds.height());
        }

        [checkedThis->m_imageAnalysisOverlayView setAnalysis:analysis.get()];
        [checkedThis->m_view.get() addSubview:checkedThis->m_imageAnalysisOverlayView.get()];
    };

    performOrDeferImageAnalysisOverlayViewHierarchyTask(WTFMove(installTask));
}

void WebViewImpl::uninstallImageAnalysisOverlayView()
{
    auto uninstallTask = [weakThis = WeakPtr { *this }] {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis || !checkedThis->m_imageAnalysisOverlayView)
            return;

        RELEASE_LOG(ImageAnalysis, "Uninstalling image analysis overlay view");
        [checkedThis->m_imageAnalysisOverlayView removeFromSuperview];
        checkedThis->m_imageAnalysisOverlayViewDelegate = nil;
        checkedThis->m_imageAnalysisOverlayView = nil;
        checkedThis->m_imageAnalysisInteractionBounds = { };
    };

    performOrDeferImageAnalysisOverlayViewHierarchyTask(WTFMove(uninstallTask));
}

void WebViewImpl::performOrDeferImageAnalysisOverlayViewHierarchyTask(std::function<void()>&& task)
{
    if (m_lastMouseDownEvent)
        m_imageAnalysisOverlayViewHierarchyDeferredTask = WTFMove(task);
    else
        task();
}

void WebViewImpl::fulfillDeferredImageAnalysisOverlayViewHierarchyTask()
{
    if (auto&& task = std::exchange(m_imageAnalysisOverlayViewHierarchyDeferredTask, nullptr))
        task();
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)

void WebViewImpl::setClientImplicitlyRequestedTopScrollPocket()
{
    if (m_clientImplicitlyRequestedTopScrollPocket)
        return;

    m_clientImplicitlyRequestedTopScrollPocket = true;
    updateScrollPocket();
}

void WebViewImpl::updatePrefersSolidColorHardPocket()
{
    static bool canSetPrefersSolidColorHardPocket = [NSScrollPocket instancesRespondToSelector:@selector(setPrefersSolidColorHardPocket:)];
    if (!canSetPrefersSolidColorHardPocket)
        return;

    RetainPtr view = m_view.get();
    if (!view)
        return;

    [m_topScrollPocket setPrefersSolidColorHardPocket:^{
        if ([view _hasVisibleColorExtensionView:BoxSide::Top])
            return YES;

        if (m_pageIsScrolledToTop)
            return YES;

        if (view->_preferSolidColorHardPocketReasons)
            return YES;

        return NO;
    }()];
}

void WebViewImpl::updateScrollPocket()
{
    if (m_windowIsEnteringOrExitingFullScreen)
        return;

    Ref page = m_page.get();
    RetainPtr view = m_view.get();
    CGFloat topContentInset = obscuredContentInsets().top();
    CGFloat additionalHeight = page->overflowHeightForTopScrollEdgeEffect();
    bool needsTopView = page->preferences().contentInsetBackgroundFillEnabled()
        && view
        && !view->_reasonsToHideTopScrollPocket
        && (m_clientImplicitlyRequestedTopScrollPocket || automaticallyAdjustsContentInsets())
        && (topContentInset > 0 || additionalHeight > 0);

    RetainPtr topScrollPocketSelector = NSStringFromSelector(@selector(_topScrollPocket));
    if (!needsTopView) {
        if (m_topScrollPocket) {
            [view willChangeValueForKey:topScrollPocketSelector.get()];
            RetainPtr scrollPocket = std::exchange(m_topScrollPocket, { });
            [view didChangeValueForKey:topScrollPocketSelector.get()];
            [[scrollPocket captureView] removeFromSuperview];
            [scrollPocket removeFromSuperview];
        }
        return;
    }

    RetainPtr<NSView> captureView;
    if (!m_topScrollPocket) {
        [view willChangeValueForKey:topScrollPocketSelector.get()];
        m_topScrollPocket = adoptNS([NSScrollPocket new]);
        [view didChangeValueForKey:topScrollPocketSelector.get()];
        updateTopScrollPocketStyle();
        [m_topScrollPocket setEdge:NSScrollPocketEdgeTop];
        [m_topScrollPocket layout];
        captureView = [m_topScrollPocket captureView];
        [m_layerHostingView addSubview:captureView.get() positioned:NSWindowBelow relativeTo:nil];
        [captureView layer].zPosition = std::numeric_limits<float>::lowest();
        [view addSubview:m_topScrollPocket.get()];
        for (NSView *pocketContainer in m_viewsAboveScrollPocket.get())
            [m_topScrollPocket addElementContainer:pocketContainer];
        updateScrollPocketVisibilityWhenScrolledToTop();
        updatePrefersSolidColorHardPocket();
    } else
        captureView = [m_topScrollPocket captureView];

    auto bounds = [view bounds];
    if (RetainPtr attachedInspectorView = [view _horizontallyAttachedInspectorWebView])
        bounds = NSUnionRect(bounds, [attachedInspectorView convertRect:[attachedInspectorView bounds] toView:view.get()]);

    auto topInsetFrame = NSMakeRect(NSMinX(bounds), NSMinY(bounds) - additionalHeight, NSWidth(bounds), additionalHeight + std::min<CGFloat>(topContentInset, NSHeight(bounds)));

    if ([m_view _usesAutomaticContentInsetBackgroundFill]) {
        for (NSView *pocketContainer in m_viewsAboveScrollPocket.get())
            topInsetFrame = NSUnionRect(topInsetFrame, [view convertRect:pocketContainer.bounds fromView:pocketContainer]);
    }

    topInsetFrame = [m_topScrollPocket frameForAlignmentRect:topInsetFrame];

    if (!NSEqualRects([m_topScrollPocket frame], topInsetFrame)) {
        [m_topScrollPocket setFrame:topInsetFrame];
        [captureView setFrame:topInsetFrame];
    }

    updateTopScrollPocketCaptureColor();
}

void WebViewImpl::updateTopScrollPocketStyle()
{
    [m_topScrollPocket setStyle:[m_view _usesAutomaticContentInsetBackgroundFill] ? NSScrollPocketStyleAutomatic : NSScrollPocketStyleHard];
}

void WebViewImpl::registerViewAboveScrollPocket(NSView *containerView)
{
    if (!containerView) {
        ASSERT_NOT_REACHED();
        return;
    }

    if ([m_viewsAboveScrollPocket containsObject:containerView])
        return;

    if (!m_viewsAboveScrollPocket)
        m_viewsAboveScrollPocket = [NSHashTable<NSView *> weakObjectsHashTable];

    [m_viewsAboveScrollPocket addObject:containerView];
    [m_topScrollPocket addElementContainer:containerView];
}

void WebViewImpl::unregisterViewAboveScrollPocket(NSView *containerView)
{
    if (!containerView) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (![m_viewsAboveScrollPocket containsObject:containerView])
        return;

    [m_viewsAboveScrollPocket removeObject:containerView];
    [m_topScrollPocket removeElementContainer:containerView];
}

#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)

#if ENABLE(VIDEO)
void WebViewImpl::showCaptionDisplaySettings(HTMLMediaElementIdentifier, const WebCore::ResolvedCaptionDisplaySettingsOptions& options, CompletionHandler<void(Expected<void, WebCore::ExceptionData>&&)>&& completionHandler)
{
    RetainPtr controller = [WKCaptionStyleMenuController menuController];
    NSMenu *menu = [controller captionStyleMenu];
    auto menuSize = FloatSize { [menu size] };

    auto menuLocationFromOptions = [&] () -> NSPoint {
        if (!options.anchorBounds)
            return NSMakePoint(0, 0);

        // Default to presenting the menu so that the bottom-center point is centered in the anchorBounds
        if (!options.xPositionArea && !options.yPositionArea) {
            FloatPoint centerPoint = options.anchorBounds->center();
            centerPoint.move(-menuSize.width() / 2, -menuSize.height());
            return centerPoint;
        }

        auto calculateXLocation = [] (auto& anchorBounds, auto& xPositionArea, const FloatSize& menuSize) {
            using XPositionArea = WebCore::ResolvedCaptionDisplaySettingsOptions::XPositionArea;
            if (!xPositionArea || *xPositionArea == XPositionArea::Center)
                return anchorBounds.center().x() - menuSize.width() / 2;
            if (*xPositionArea == XPositionArea::Left)
                return anchorBounds.x() - menuSize.width();
            return anchorBounds.maxX();
        };

        auto calculateYLocation = [] (auto& anchorBounds, auto& yPositionArea, const FloatSize& menuSize) {
            using YPositionArea = WebCore::ResolvedCaptionDisplaySettingsOptions::YPositionArea;
            if (!yPositionArea || *yPositionArea == YPositionArea::Center)
                return anchorBounds.center().y() - menuSize.height() / 2;
            if (*yPositionArea == YPositionArea::Top)
                return anchorBounds.y() - menuSize.height();
            return anchorBounds.maxY();
        };

        return NSMakePoint(calculateXLocation(*options.anchorBounds, options.xPositionArea, menuSize), calculateYLocation(*options.anchorBounds, options.yPositionArea, menuSize));

    };
    auto menuLocation = menuLocationFromOptions();
    [menu popUpMenuPositioningItem:nil atLocation:menuLocation inView:m_view.get().get()];
    completionHandler({ });
}
#endif

} // namespace WebKit


#endif // PLATFORM(MAC)
