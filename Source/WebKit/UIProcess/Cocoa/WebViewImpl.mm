/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#import "AttributedString.h"
#import "ColorSpaceData.h"
#import "FullscreenClient.h"
#import "GenericCallback.h"
#import "Logging.h"
#import "NativeWebGestureEvent.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebMouseEvent.h"
#import "NativeWebWheelEvent.h"
#import "PageClient.h"
#import "PageClientImplMac.h"
#import "PasteboardTypes.h"
#import "PlaybackSessionManagerProxy.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "StringUtilities.h"
#import "TextChecker.h"
#import "TextCheckerState.h"
#import "TiledCoreAnimationDrawingAreaProxy.h"
#import "UIGamepadProvider.h"
#import "UndoOrRedo.h"
#import "ViewGestureController.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKErrorInternal.h"
#import "WKFullScreenWindowController.h"
#import "WKImmediateActionController.h"
#import "WKPrintingView.h"
#import "WKTextInputWindowController.h"
#import "WKViewLayoutStrategy.h"
#import "WKWebViewPrivate.h"
#import "WebBackForwardList.h"
#import "WebEditCommandProxy.h"
#import "WebEventFactory.h"
#import "WebInspectorProxy.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import "_WKThumbnailViewInternal.h"
#import <HIToolbox/CarbonEventsCore.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/ActivityState.h>
#import <WebCore/ColorMac.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DragData.h>
#import <WebCore/DragItem.h>
#import <WebCore/Editor.h>
#import <WebCore/FileSystem.h>
#import <WebCore/FontAttributeChanges.h>
#import <WebCore/KeypressCommand.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/PromisedAttachmentInfo.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/TextUndoInsertionMarkupMac.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <WebCore/WebCoreFullScreenPlaceholderView.h>
#import <WebCore/WebCoreFullScreenWindow.h>
#import <WebCore/WebCoreNSFontManagerExtras.h>
#import <WebCore/WebPlaybackControlsManager.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/cocoa/NSTouchBarSPI.h>
#import <pal/spi/mac/DataDetectorsSPI.h>
#import <pal/spi/mac/LookupSPI.h>
#import <pal/spi/mac/NSAccessibilitySPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>
#import <pal/spi/mac/NSTextFinderSPI.h>
#import <pal/spi/mac/NSWindowSPI.h>
#import <sys/stat.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SetForScope.h>
#import <wtf/SoftLinking.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/text/StringConcatenate.h>

#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
SOFT_LINK_FRAMEWORK(AVKit)
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
SOFT_LINK_CLASS(AVKit, AVTouchBarPlaybackControlsProvider)
SOFT_LINK_CLASS(AVKit, AVTouchBarScrubber)
#else
SOFT_LINK_CLASS(AVKit, AVFunctionBarPlaybackControlsProvider)
SOFT_LINK_CLASS(AVKit, AVFunctionBarScrubber)
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300

static NSString * const WKMediaExitFullScreenItem = @"WKMediaExitFullScreenItem";
#endif // HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

SOFT_LINK_CONSTANT_MAY_FAIL(Lookup, LUNotificationPopoverWillClose, NSString *)

WTF_DECLARE_CF_TYPE_TRAIT(CGImage);

@interface NSApplication ()
- (BOOL)isSpeaking;
- (void)speakString:(NSString *)string;
- (void)stopSpeaking:(id)sender;
@end

// FIXME: Move to an SPI header.
@interface NSTextInputContext (WKNSTextInputContextDetails)
- (void)handleEvent:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler;
- (void)handleEventByInputMethod:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler;
- (BOOL)handleEventByKeyboardLayout:(NSEvent *)event;
@end

#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
// FIXME: Remove this once -setCanShowMediaSelectionButton: is declared in an SDK used by Apple's buildbot.
@interface AVTouchBarScrubber ()
- (void)setCanShowMediaSelectionButton:(BOOL)canShowMediaSelectionButton;
@end
#endif

@interface WKAccessibilitySettingsObserver : NSObject {
    WebKit::WebViewImpl *_impl;
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

    NSNotificationCenter* workspaceNotificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
    [workspaceNotificationCenter addObserver:self selector:@selector(_settingsDidChange:) name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:nil];

    return self;
}

- (void)dealloc
{
    NSNotificationCenter *workspaceNotificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
    [workspaceNotificationCenter removeObserver:self name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:nil];

    [super dealloc];
}

- (void)_settingsDidChange:(NSNotification *)notification
{
    _impl->accessibilitySettingsDidChange();
}

@end

@interface WKWindowVisibilityObserver : NSObject {
    NSView *_view;
    WebKit::WebViewImpl *_impl;

    BOOL _didRegisterForLookupPopoverCloseNotifications;
}

- (instancetype)initWithView:(NSView *)view impl:(WebKit::WebViewImpl&)impl;
- (void)startObserving:(NSWindow *)window;
- (void)stopObserving:(NSWindow *)window;
- (void)startObservingFontPanel;
- (void)startObservingLookupDismissalIfNeeded;
@end

@implementation WKWindowVisibilityObserver

- (instancetype)initWithView:(NSView *)view impl:(WebKit::WebViewImpl&)impl
{
    self = [super init];
    if (!self)
        return nil;

    _view = view;
    _impl = &impl;

    NSNotificationCenter* workspaceNotificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
    [workspaceNotificationCenter addObserver:self selector:@selector(_activeSpaceDidChange:) name:NSWorkspaceActiveSpaceDidChangeNotification object:nil];

    return self;
}

- (void)dealloc
{
    if (_didRegisterForLookupPopoverCloseNotifications && canLoadLUNotificationPopoverWillClose())
        [[NSNotificationCenter defaultCenter] removeObserver:self name:getLUNotificationPopoverWillClose() object:nil];

    NSNotificationCenter *workspaceNotificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
    [workspaceNotificationCenter removeObserver:self name:NSWorkspaceActiveSpaceDidChangeNotification object:nil];

    [super dealloc];
}

static void* keyValueObservingContext = &keyValueObservingContext;

- (void)startObserving:(NSWindow *)window
{
    if (!window)
        return;

    NSNotificationCenter *defaultNotificationCenter = [NSNotificationCenter defaultCenter];

    // An NSView derived object such as WKView cannot observe these notifications, because NSView itself observes them.
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidOrderOffScreen:) name:@"NSWindowDidOrderOffScreenNotification" object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidOrderOnScreen:) name:@"_NSWindowDidBecomeVisible" object:window];

    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:nil];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidResignKey:) name:NSWindowDidResignKeyNotification object:nil];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidMiniaturize:) name:NSWindowDidMiniaturizeNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidDeminiaturize:) name:NSWindowDidDeminiaturizeNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidMove:) name:NSWindowDidMoveNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidResize:) name:NSWindowDidResizeNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeBackingProperties:) name:NSWindowDidChangeBackingPropertiesNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeScreen:) name:NSWindowDidChangeScreenNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeLayerHosting:) name:@"_NSWindowDidChangeContentsHostedInLayerSurfaceNotification" object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeOcclusionState:) name:NSWindowDidChangeOcclusionStateNotification object:window];

    [window addObserver:self forKeyPath:@"contentLayoutRect" options:NSKeyValueObservingOptionInitial context:keyValueObservingContext];
    [window addObserver:self forKeyPath:@"titlebarAppearsTransparent" options:NSKeyValueObservingOptionInitial context:keyValueObservingContext];
}

- (void)stopObserving:(NSWindow *)window
{
    if (!window)
        return;

    NSNotificationCenter *defaultNotificationCenter = [NSNotificationCenter defaultCenter];

    [defaultNotificationCenter removeObserver:self name:@"NSWindowDidOrderOffScreenNotification" object:window];
    [defaultNotificationCenter removeObserver:self name:@"_NSWindowDidBecomeVisible" object:window];

    [defaultNotificationCenter removeObserver:self name:NSWindowDidBecomeKeyNotification object:nil];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidResignKeyNotification object:nil];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidMiniaturizeNotification object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidDeminiaturizeNotification object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidMoveNotification object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidResizeNotification object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidChangeBackingPropertiesNotification object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidChangeScreenNotification object:window];
    [defaultNotificationCenter removeObserver:self name:@"_NSWindowDidChangeContentsHostedInLayerSurfaceNotification" object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidChangeOcclusionStateNotification object:window];

    if (_impl->isEditable())
        [[NSFontPanel sharedFontPanel] removeObserver:self forKeyPath:@"visible" context:keyValueObservingContext];
    [window removeObserver:self forKeyPath:@"contentLayoutRect" context:keyValueObservingContext];
    [window removeObserver:self forKeyPath:@"titlebarAppearsTransparent" context:keyValueObservingContext];
}

- (void)startObservingFontPanel
{
    [[NSFontPanel sharedFontPanel] addObserver:self forKeyPath:@"visible" options:0 context:keyValueObservingContext];
}

- (void)startObservingLookupDismissalIfNeeded
{
    if (_didRegisterForLookupPopoverCloseNotifications)
        return;

    _didRegisterForLookupPopoverCloseNotifications = YES;

    if (canLoadLUNotificationPopoverWillClose())
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_dictionaryLookupPopoverWillClose:) name:getLUNotificationPopoverWillClose() object:nil];
}

- (void)_windowDidOrderOnScreen:(NSNotification *)notification
{
    _impl->windowDidOrderOnScreen();
}

- (void)_windowDidOrderOffScreen:(NSNotification *)notification
{
    _impl->windowDidOrderOffScreen();
}

- (void)_windowDidBecomeKey:(NSNotification *)notification
{
    _impl->windowDidBecomeKey([notification object]);
}

- (void)_windowDidResignKey:(NSNotification *)notification
{
    _impl->windowDidResignKey([notification object]);
}

- (void)_windowDidMiniaturize:(NSNotification *)notification
{
    _impl->windowDidMiniaturize();
}

- (void)_windowDidDeminiaturize:(NSNotification *)notification
{
    _impl->windowDidDeminiaturize();
}

- (void)_windowDidMove:(NSNotification *)notification
{
    _impl->windowDidMove();
}

- (void)_windowDidResize:(NSNotification *)notification
{
    _impl->windowDidResize();
}

- (void)_windowDidChangeBackingProperties:(NSNotification *)notification
{
    CGFloat oldBackingScaleFactor = [[notification.userInfo objectForKey:NSBackingPropertyOldScaleFactorKey] doubleValue];
    _impl->windowDidChangeBackingProperties(oldBackingScaleFactor);
}

- (void)_windowDidChangeScreen:(NSNotification *)notification
{
    _impl->windowDidChangeScreen();
}

- (void)_windowDidChangeLayerHosting:(NSNotification *)notification
{
    _impl->windowDidChangeLayerHosting();
}

- (void)_windowDidChangeOcclusionState:(NSNotification *)notification
{
    _impl->windowDidChangeOcclusionState();
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context != keyValueObservingContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }

    if ([keyPath isEqualToString:@"visible"] && [NSFontPanel sharedFontPanelExists] && object == [NSFontPanel sharedFontPanel]) {
        _impl->updateFontPanelIfNeeded();
        return;
    }
    if ([keyPath isEqualToString:@"contentLayoutRect"] || [keyPath isEqualToString:@"titlebarAppearsTransparent"])
        _impl->updateContentInsetsIfAutomatic();
}

- (void)_dictionaryLookupPopoverWillClose:(NSNotification *)notification
{
    _impl->clearTextIndicatorWithAnimation(WebCore::TextIndicatorWindowDismissalAnimation::None);
}

- (void)_activeSpaceDidChange:(NSNotification *)notification
{
    _impl->activeSpaceDidChange();
}

@end

@interface WKEditCommandObjC : NSObject {
    RefPtr<WebKit::WebEditCommandProxy> m_command;
}
- (id)initWithWebEditCommandProxy:(RefPtr<WebKit::WebEditCommandProxy>)command;
- (WebKit::WebEditCommandProxy*)command;
@end

@interface WKEditorUndoTargetObjC : NSObject
- (void)undoEditing:(id)sender;
- (void)redoEditing:(id)sender;
@end

@implementation WKEditCommandObjC

- (id)initWithWebEditCommandProxy:(RefPtr<WebKit::WebEditCommandProxy>)command
{
    self = [super init];
    if (!self)
        return nil;

    m_command = command;
    return self;
}

- (WebKit::WebEditCommandProxy*)command
{
    return m_command.get();
}

@end

@implementation WKEditorUndoTargetObjC

- (void)undoEditing:(id)sender
{
    ASSERT([sender isKindOfClass:[WKEditCommandObjC class]]);
    [sender command]->unapply();
}

- (void)redoEditing:(id)sender
{
    ASSERT([sender isKindOfClass:[WKEditCommandObjC class]]);
    [sender command]->reapply();
}

@end

@interface WKFlippedView : NSView
@end

@implementation WKFlippedView

- (BOOL)isFlipped
{
    return YES;
}

@end

@interface WKResponderChainSink : NSResponder {
    NSResponder *_lastResponderInChain;
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
    _lastResponderInChain = chain;
    while (NSResponder *next = [_lastResponderInChain nextResponder])
        _lastResponderInChain = next;
    [_lastResponderInChain setNextResponder:self];
    return self;
}

- (void)detach
{
    // This assumes that the responder chain was either unmodified since
    // -initWithResponderChain: was called, or was modified in such a way
    // that _lastResponderInChain is still in the chain, and self was not
    // moved earlier in the chain than _lastResponderInChain.
    NSResponder *responderBeforeSelf = _lastResponderInChain;
    NSResponder *next = [responderBeforeSelf nextResponder];
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

using namespace WebKit;

#if HAVE(TOUCH_BAR)

@interface WKTextListTouchBarViewController : NSViewController {
@private
    WebViewImpl* _webViewImpl;
    ListType _currentListType;
}

@property (nonatomic) ListType currentListType;

- (instancetype)initWithWebViewImpl:(WebViewImpl*)webViewImpl;

@end

@implementation WKTextListTouchBarViewController

@synthesize currentListType=_currentListType;

static const CGFloat listControlSegmentWidth = 67;
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER) && ENABLE(FULLSCREEN_API)
static const CGFloat exitFullScreenButtonWidth = 64;
#endif

static const NSUInteger noListSegment = 0;
static const NSUInteger unorderedListSegment = 1;
static const NSUInteger orderedListSegment = 2;

- (instancetype)initWithWebViewImpl:(WebViewImpl*)webViewImpl
{
    if (!(self = [super init]))
        return nil;

    _webViewImpl = webViewImpl;

    NSSegmentedControl *insertListControl = [NSSegmentedControl segmentedControlWithLabels:@[ WebCore::insertListTypeNone(), WebCore::insertListTypeBulleted(), WebCore::insertListTypeNumbered() ] trackingMode:NSSegmentSwitchTrackingSelectOne target:self action:@selector(_selectList:)];
    [insertListControl setWidth:listControlSegmentWidth forSegment:noListSegment];
    [insertListControl setWidth:listControlSegmentWidth forSegment:unorderedListSegment];
    [insertListControl setWidth:listControlSegmentWidth forSegment:orderedListSegment];
    insertListControl.font = [NSFont systemFontOfSize:15];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    id segmentElement = NSAccessibilityUnignoredDescendant(insertListControl);
    NSArray *segments = [segmentElement accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
    ASSERT(segments.count == 3);
    [segments[noListSegment] accessibilitySetOverrideValue:WebCore::insertListTypeNone() forAttribute:NSAccessibilityDescriptionAttribute];
    [segments[unorderedListSegment] accessibilitySetOverrideValue:WebCore::insertListTypeBulletedAccessibilityTitle() forAttribute:NSAccessibilityDescriptionAttribute];
    [segments[orderedListSegment] accessibilitySetOverrideValue:WebCore::insertListTypeNumberedAccessibilityTitle() forAttribute:NSAccessibilityDescriptionAttribute];
    ALLOW_DEPRECATED_DECLARATIONS_END

    self.view = insertListControl;

    return self;
}

- (void)didDestroyView
{
    _webViewImpl = nullptr;
}

- (void)_selectList:(id)sender
{
    if (!_webViewImpl)
        return;

    NSSegmentedControl *insertListControl = (NSSegmentedControl *)self.view;
    switch (insertListControl.selectedSegment) {
    case noListSegment:
        // There is no "remove list" edit command, but InsertOrderedList and InsertUnorderedList both
        // behave as toggles, so we can invoke the appropriate edit command depending on our _currentListType
        // to remove an existing list. We don't have to do anything if _currentListType is NoList.
        if (_currentListType == OrderedList)
            _webViewImpl->page().executeEditCommand(@"InsertOrderedList", @"");
        else if (_currentListType == UnorderedList)
            _webViewImpl->page().executeEditCommand(@"InsertUnorderedList", @"");
        break;
    case unorderedListSegment:
        _webViewImpl->page().executeEditCommand(@"InsertUnorderedList", @"");
        break;
    case orderedListSegment:
        _webViewImpl->page().executeEditCommand(@"InsertOrderedList", @"");
        break;
    }

    _webViewImpl->dismissTextTouchBarPopoverItemWithIdentifier(NSTouchBarItemIdentifierTextList);
}

- (void)setCurrentListType:(ListType)listType
{
    NSSegmentedControl *insertListControl = (NSSegmentedControl *)self.view;
    switch (listType) {
    case NoList:
        [insertListControl setSelected:YES forSegment:noListSegment];
        break;
    case OrderedList:
        [insertListControl setSelected:YES forSegment:orderedListSegment];
        break;
    case UnorderedList:
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
    WebViewImpl* _webViewImpl;
}

@property (nonatomic) BOOL textIsBold;
@property (nonatomic) BOOL textIsItalic;
@property (nonatomic) BOOL textIsUnderlined;
@property (nonatomic) NSTextAlignment currentTextAlignment;
@property (nonatomic, retain, readwrite) NSColor *textColor;

- (instancetype)initWithWebViewImpl:(WebViewImpl*)webViewImpl;
@end

@implementation WKTextTouchBarItemController

@synthesize textIsBold=_textIsBold;
@synthesize textIsItalic=_textIsItalic;
@synthesize textIsUnderlined=_textIsUnderlined;
@synthesize currentTextAlignment=_currentTextAlignment;

- (instancetype)initWithWebViewImpl:(WebViewImpl*)webViewImpl
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
    NSTouchBarItem *item = [super itemForIdentifier:identifier];
    BOOL isTextFormatItem = [identifier isEqualToString:NSTouchBarItemIdentifierTextFormat];

    if (isTextFormatItem || [identifier isEqualToString:NSTouchBarItemIdentifierTextStyle])
        self.textStyle.action = @selector(_wkChangeTextStyle:);

    if (isTextFormatItem || [identifier isEqualToString:NSTouchBarItemIdentifierTextAlignment])
        self.textAlignments.action = @selector(_wkChangeTextAlignment:);

    NSColorPickerTouchBarItem *colorPickerItem = nil;
    if ([identifier isEqualToString:NSTouchBarItemIdentifierTextColorPicker] && [item isKindOfClass:[NSColorPickerTouchBarItem class]])
        colorPickerItem = (NSColorPickerTouchBarItem *)item;
    if (isTextFormatItem)
        colorPickerItem = self.colorPickerItem;
    if (colorPickerItem) {
        colorPickerItem.target = self;
        colorPickerItem.action = @selector(_wkChangeColor:);
        colorPickerItem.showsAlpha = NO;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
        colorPickerItem.allowedColorSpaces = @[ [NSColorSpace sRGBColorSpace] ];
#endif
    }

    return item;
}

#pragma mark NSCandidateListTouchBarItemDelegate

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem *)anItem endSelectingCandidateAtIndex:(NSInteger)index
{
    if (index == NSNotFound)
        return;

    if (!_webViewImpl)
        return;

    NSArray *candidates = anItem.candidates;
    if ((NSUInteger)index >= candidates.count)
        return;

    NSTextCheckingResult *candidate = candidates[index];
    ASSERT([candidate isKindOfClass:[NSTextCheckingResult class]]);

    _webViewImpl->handleAcceptedCandidate(candidate);
}

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem *)anItem changedCandidateListVisibility:(BOOL)isVisible
{
    if (!_webViewImpl)
        return;

    if (isVisible)
        _webViewImpl->requestCandidatesForSelectionIfNeeded();

    _webViewImpl->updateTouchBar();
}

#pragma mark NSNotificationCenter observers

- (void)touchBarDidExitCustomization:(NSNotification *)notification
{
    if (!_webViewImpl)
        return;

    _webViewImpl->setIsCustomizingTouchBar(false);
    _webViewImpl->updateTouchBar();
}

- (void)touchBarWillEnterCustomization:(NSNotification *)notification
{
    if (!_webViewImpl)
        return;

    _webViewImpl->setIsCustomizingTouchBar(true);
}

- (void)didChangeAutomaticTextCompletion:(NSNotification *)notification
{
    if (!_webViewImpl)
        return;

    _webViewImpl->updateTouchBarAndRefreshTextBarIdentifiers();
}


#pragma mark NSTextTouchBarItemController

- (WKTextListTouchBarViewController *)textListTouchBarViewController
{
    return (WKTextListTouchBarViewController *)self.textListViewController;
}

- (void)setTextIsBold:(BOOL)bold
{
    _textIsBold = bold;
    if ([self.textStyle isSelectedForSegment:0] != _textIsBold)
        [self.textStyle setSelected:_textIsBold forSegment:0];
}

- (void)setTextIsItalic:(BOOL)italic
{
    _textIsItalic = italic;
    if ([self.textStyle isSelectedForSegment:1] != _textIsItalic)
        [self.textStyle setSelected:_textIsItalic forSegment:1];
}

- (void)setTextIsUnderlined:(BOOL)underlined
{
    _textIsUnderlined = underlined;
    if ([self.textStyle isSelectedForSegment:2] != _textIsUnderlined)
        [self.textStyle setSelected:_textIsUnderlined forSegment:2];
}

- (void)_wkChangeTextStyle:(id)sender
{
    if (!_webViewImpl)
        return;

    if ([self.textStyle isSelectedForSegment:0] != _textIsBold) {
        _textIsBold = !_textIsBold;
        _webViewImpl->page().executeEditCommand(@"ToggleBold", @"");
    }

    if ([self.textStyle isSelectedForSegment:1] != _textIsItalic) {
        _textIsItalic = !_textIsItalic;
        _webViewImpl->page().executeEditCommand("ToggleItalic", @"");
    }

    if ([self.textStyle isSelectedForSegment:2] != _textIsUnderlined) {
        _textIsUnderlined = !_textIsUnderlined;
        _webViewImpl->page().executeEditCommand("ToggleUnderline", @"");
    }
}

- (void)setCurrentTextAlignment:(NSTextAlignment)alignment
{
    _currentTextAlignment = alignment;
    [self.textAlignments selectSegmentWithTag:_currentTextAlignment];
}

- (void)_wkChangeTextAlignment:(id)sender
{
    if (!_webViewImpl)
        return;

    NSTextAlignment alignment = (NSTextAlignment)[self.textAlignments.cell tagForSegment:self.textAlignments.selectedSegment];
    switch (alignment) {
    case NSTextAlignmentLeft:
        _currentTextAlignment = NSTextAlignmentLeft;
        _webViewImpl->page().executeEditCommand("AlignLeft", @"");
        break;
    case NSTextAlignmentRight:
        _currentTextAlignment = NSTextAlignmentRight;
        _webViewImpl->page().executeEditCommand("AlignRight", @"");
        break;
    case NSTextAlignmentCenter:
        _currentTextAlignment = NSTextAlignmentCenter;
        _webViewImpl->page().executeEditCommand("AlignCenter", @"");
        break;
    case NSTextAlignmentJustified:
        _currentTextAlignment = NSTextAlignmentJustified;
        _webViewImpl->page().executeEditCommand("AlignJustified", @"");
        break;
    default:
        break;
    }

    _webViewImpl->dismissTextTouchBarPopoverItemWithIdentifier(NSTouchBarItemIdentifierTextAlignment);
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
    _webViewImpl->page().executeEditCommand("ForeColor", WebCore::colorFromNSColor(_textColor.get()).serialized());
}

- (NSViewController *)textListViewController
{
    if (!_textListTouchBarViewController)
        _textListTouchBarViewController = adoptNS([[WKTextListTouchBarViewController alloc] initWithWebViewImpl:_webViewImpl]);
    return _textListTouchBarViewController.get();
}

@end

@interface WKPromisedAttachmentContext : NSObject {
@private
    RetainPtr<NSURL> _blobURL;
    RetainPtr<NSString> _fileName;
    RetainPtr<NSString> _attachmentIdentifier;
}

- (instancetype)initWithIdentifier:(NSString *)identifier blobURL:(NSURL *)url fileName:(NSString *)fileName;

@property (nonatomic, readonly) NSURL *blobURL;
@property (nonatomic, readonly) NSString *fileName;
@property (nonatomic, readonly) NSString *attachmentIdentifier;

@end

@implementation WKPromisedAttachmentContext

- (instancetype)initWithIdentifier:(NSString *)identifier blobURL:(NSURL *)blobURL fileName:(NSString *)fileName
{
    if (!(self = [super init]))
        return nil;

    _blobURL = blobURL;
    _fileName = fileName;
    _attachmentIdentifier = identifier;
    return self;
}

- (NSURL *)blobURL
{
    return _blobURL.get();
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

namespace WebKit {

NSTouchBar *WebViewImpl::makeTouchBar()
{
    if (!m_canCreateTouchBars) {
        m_canCreateTouchBars = true;
        updateTouchBar();
    }
    return m_currentTouchBar.get();
}

void WebViewImpl::updateTouchBar()
{
    if (!m_canCreateTouchBars)
        return;

    NSTouchBar *touchBar = nil;
    bool userActionRequirementsHaveBeenMet = m_requiresUserActionForEditingControlsManager ? m_page->hasHadSelectionChangesFromUserInteraction() : true;
    if (m_page->editorState().isContentEditable && !m_page->needsHiddenContentEditableQuirk()) {
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
            if ([m_view respondsToSelector:@selector(_web_didRemoveMediaControlsManager)] && m_view.getAutoreleased() == [m_view window].firstResponder)
                [m_view _web_didRemoveMediaControlsManager];
        }
        [m_mediaTouchBarProvider setPlaybackControlsController:nil];
        [m_mediaPlaybackControlsView setPlaybackControlsController:nil];
    }
#endif

    if (touchBar == m_currentTouchBar)
        return;

    // If m_editableElementIsFocused is true, then we may have a non-editable selection right now just because
    // the user is clicking or tabbing between editable fields.
    if (m_editableElementIsFocused && touchBar != textTouchBar())
        return;

    m_currentTouchBar = touchBar;
    [m_view willChangeValueForKey:@"touchBar"];
    [m_view setTouchBar:m_currentTouchBar.get()];
    [m_view didChangeValueForKey:@"touchBar"];
}

NSCandidateListTouchBarItem *WebViewImpl::candidateListTouchBarItem() const
{
    if (m_page->editorState().isInPasswordField)
        return m_passwordTextCandidateListTouchBarItem.get();
    return isRichlyEditable() ? m_richTextCandidateListTouchBarItem.get() : m_plainTextCandidateListTouchBarItem.get();
}

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
AVTouchBarScrubber *WebViewImpl::mediaPlaybackControlsView() const
#else
AVFunctionBarScrubber *WebViewImpl::mediaPlaybackControlsView() const
#endif
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
    for (NSTouchBarItem *item in textTouchBar().items) {
        if ([item.identifier isEqualToString:identifier]) {
            foundItem = item;
            break;
        }

        if ([item.identifier isEqualToString:NSTouchBarItemIdentifierTextFormat]) {
            for (NSTouchBarItem *childItem in ((NSGroupTouchBarItem *)item).groupTouchBar.items) {
                if ([childItem.identifier isEqualToString:identifier]) {
                    foundItem = childItem;
                    break;
                }
            }
            break;
        }
    }

    if ([foundItem isKindOfClass:[NSPopoverTouchBarItem class]])
        [(NSPopoverTouchBarItem *)foundItem dismissPopover:nil];
}

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
    NSSet<NSTouchBarItem *> *templateItems = nil;
    NSArray<NSTouchBarItemIdentifier> *defaultItemIdentifiers = nil;
    NSArray<NSTouchBarItemIdentifier> *customizationAllowedItemIdentifiers = nil;

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
    [touchBar setTemplateItems:templateItems];
    [touchBar setDefaultItemIdentifiers:defaultItemIdentifiers];
    [touchBar setCustomizationAllowedItemIdentifiers:customizationAllowedItemIdentifiers];

    if (NSGroupTouchBarItem *textFormatItem = (NSGroupTouchBarItem *)[touchBar itemForIdentifier:NSTouchBarItemIdentifierTextFormat])
        textFormatItem.groupTouchBar.customizationIdentifier = @"WKTextFormatTouchBar";
}

bool WebViewImpl::isRichlyEditable() const
{
    return m_page->editorState().isContentRichlyEditable && !m_page->needsPlainTextQuirk();
}

NSTouchBar *WebViewImpl::textTouchBar() const
{
    if (m_page->editorState().isInPasswordField)
        return m_passwordTextTouchBar.get();
    return isRichlyEditable() ? m_richTextTouchBar.get() : m_plainTextTouchBar.get();
}

static NSTextAlignment nsTextAlignmentFromTextAlignment(TextAlignment textAlignment)
{
    NSTextAlignment nsTextAlignment;
    switch (textAlignment) {
    case NoAlignment:
        nsTextAlignment = NSTextAlignmentNatural;
        break;
    case LeftAlignment:
        nsTextAlignment = NSTextAlignmentLeft;
        break;
    case RightAlignment:
        nsTextAlignment = NSTextAlignmentRight;
        break;
    case CenterAlignment:
        nsTextAlignment = NSTextAlignmentCenter;
        break;
    case JustifiedAlignment:
        nsTextAlignment = NSTextAlignmentJustified;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
    return nsTextAlignment;
}

void WebViewImpl::updateTextTouchBar()
{
    if (!m_page->editorState().isContentEditable)
        return;

    if (m_isUpdatingTextTouchBar)
        return;

    SetForScope<bool> isUpdatingTextFunctionBar(m_isUpdatingTextTouchBar, true);

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
        [candidateListTouchBarItem() updateWithInsertionPointVisibility:showCandidatesList];
        [m_view _didUpdateCandidateListVisibility:showCandidatesList];
    }

    if (m_page->editorState().isInPasswordField) {
        if (!m_passwordTextTouchBar) {
            m_passwordTextTouchBar = adoptNS([[NSTouchBar alloc] init]);
            setUpTextTouchBar(m_passwordTextTouchBar.get());
        }
        [m_passwordTextCandidateListTouchBarItem setCandidates:@[ ] forSelectedRange:NSMakeRange(0, 0) inString:nil];
    }

    NSTouchBar *textTouchBar = this->textTouchBar();
    BOOL isShowingCombinedTextFormatItem = [textTouchBar.defaultItemIdentifiers containsObject:NSTouchBarItemIdentifierTextFormat];
    [textTouchBar setPrincipalItemIdentifier:isShowingCombinedTextFormatItem ? NSTouchBarItemIdentifierTextFormat : nil];

    // Set current typing attributes for rich text. This will ensure that the buttons reflect the state of
    // the text when changing selection throughout the document.
    if (isRichlyEditable()) {
        const EditorState& editorState = m_page->editorState();
        if (!editorState.isMissingPostLayoutData) {
            [m_textTouchBarItemController setTextIsBold:(bool)(m_page->editorState().postLayoutData().typingAttributes & AttributeBold)];
            [m_textTouchBarItemController setTextIsItalic:(bool)(m_page->editorState().postLayoutData().typingAttributes & AttributeItalics)];
            [m_textTouchBarItemController setTextIsUnderlined:(bool)(m_page->editorState().postLayoutData().typingAttributes & AttributeUnderline)];
            [m_textTouchBarItemController setTextColor:nsColor(editorState.postLayoutData().textColor)];
            [[m_textTouchBarItemController textListTouchBarViewController] setCurrentListType:(ListType)m_page->editorState().postLayoutData().enclosingListType];
            [m_textTouchBarItemController setCurrentTextAlignment:nsTextAlignmentFromTextAlignment((TextAlignment)editorState.postLayoutData().textAlignment)];
        }
        BOOL isShowingCandidateListItem = [textTouchBar.defaultItemIdentifiers containsObject:NSTouchBarItemIdentifierCandidateList] && [NSSpellChecker isAutomaticTextReplacementEnabled];
        [m_textTouchBarItemController setUsesNarrowTextStyleItem:isShowingCombinedTextFormatItem && isShowingCandidateListItem];
    }
}

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

bool WebViewImpl::isPictureInPictureActive()
{
    return [m_playbackControlsManager isPictureInPictureActive];
}

void WebViewImpl::togglePictureInPicture()
{
    [m_playbackControlsManager togglePictureInPicture];
}

void WebViewImpl::updateMediaPlaybackControlsManager()
{
    if (!m_page->hasActiveVideoForControlsManager())
        return;

    if (!m_playbackControlsManager) {
        m_playbackControlsManager = adoptNS([[WebPlaybackControlsManager alloc] init]);
        [m_playbackControlsManager setAllowsPictureInPicturePlayback:m_page->preferences().allowsPictureInPictureMediaPlayback()];
        [m_playbackControlsManager setCanTogglePictureInPicture:NO];
    }

    if (PlatformPlaybackSessionInterface* interface = m_page->playbackSessionManager()->controlsManagerInterface()) {
        [m_playbackControlsManager setPlaybackSessionInterfaceMac:interface];
        interface->updatePlaybackControlsManagerCanTogglePictureInPicture();
    }
}

#endif // ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

void WebViewImpl::updateMediaTouchBar()
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER) && ENABLE(VIDEO_PRESENTATION_MODE)
    if (!m_mediaTouchBarProvider) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
        m_mediaTouchBarProvider = adoptNS([allocAVTouchBarPlaybackControlsProviderInstance() init]);
#else
        m_mediaTouchBarProvider = adoptNS([allocAVFunctionBarPlaybackControlsProviderInstance() init]);
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    }

    if (!m_mediaPlaybackControlsView) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
        m_mediaPlaybackControlsView = adoptNS([allocAVTouchBarScrubberInstance() init]);
        // FIXME: Remove this once setCanShowMediaSelectionButton: is declared in an SDK used by Apple's buildbot.
        if ([m_mediaPlaybackControlsView respondsToSelector:@selector(setCanShowMediaSelectionButton:)])
            [m_mediaPlaybackControlsView setCanShowMediaSelectionButton:YES];
#else
        m_mediaPlaybackControlsView = adoptNS([allocAVFunctionBarScrubberInstance() init]);
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    }

    updateMediaPlaybackControlsManager();

    [m_mediaTouchBarProvider setPlaybackControlsController:m_playbackControlsManager.get()];
    [m_mediaPlaybackControlsView setPlaybackControlsController:m_playbackControlsManager.get()];

    if (!useMediaPlaybackControlsView()) {
#if ENABLE(FULLSCREEN_API)
        // If we can't have a media popover function bar item, it might be because we are in full screen.
        // If so, customize the escape key.
        NSTouchBar *touchBar = [m_mediaTouchBarProvider respondsToSelector:@selector(touchBar)] ? [(id)m_mediaTouchBarProvider.get() touchBar] : [(id)m_mediaTouchBarProvider.get() touchBar];
        if (hasFullScreenWindowController() && [m_fullScreenWindowController isFullScreen]) {
            if (!m_exitFullScreenButton) {
                m_exitFullScreenButton = adoptNS([[NSCustomTouchBarItem alloc] initWithIdentifier:WKMediaExitFullScreenItem]);

                NSImage *image = [NSImage imageNamed:NSImageNameTouchBarExitFullScreenTemplate];
                [image setTemplate:YES];

                NSButton *exitFullScreenButton = [NSButton buttonWithTitle:image ? @"" : @"Exit" image:image target:m_fullScreenWindowController.get() action:@selector(requestExitFullScreen)];
                [exitFullScreenButton setAccessibilityTitle:WebCore::exitFullScreenButtonAccessibilityTitle()];

                [[exitFullScreenButton.widthAnchor constraintLessThanOrEqualToConstant:exitFullScreenButtonWidth] setActive:YES];
                [m_exitFullScreenButton setView:exitFullScreenButton];
            }
            touchBar.escapeKeyReplacementItem = m_exitFullScreenButton.get();
        } else
            touchBar.escapeKeyReplacementItem = nil;
#endif
        // The rest of the work to update the media function bar only applies to the popover version, so return early.
        return;
    }

    if (m_playbackControlsManager && m_view.getAutoreleased() == [m_view window].firstResponder && [m_view respondsToSelector:@selector(_web_didAddMediaControlsManager:)])
        [m_view _web_didAddMediaControlsManager:m_mediaPlaybackControlsView.get()];
#endif
}

#if HAVE(TOUCH_BAR)

bool WebViewImpl::canTogglePictureInPicture()
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    return [m_playbackControlsManager canTogglePictureInPicture];
#else
    return NO;
#endif
}

#endif // HAVE(TOUCH_BAR)

void WebViewImpl::forceRequestCandidatesForTesting()
{
    m_canCreateTouchBars = true;
#if HAVE(TOUCH_BAR)
    updateTouchBar();
#endif
}

bool WebViewImpl::shouldRequestCandidates() const
{
    return !m_page->editorState().isInPasswordField && candidateListTouchBarItem().candidateListVisible;
}

void WebViewImpl::setEditableElementIsFocused(bool editableElementIsFocused)
{
    m_editableElementIsFocused = editableElementIsFocused;

#if HAVE(TOUCH_BAR)
    // If the editable elements have blurred, then we might need to get rid of the editing function bar.
    if (!m_editableElementIsFocused)
        updateTouchBar();
#endif
}

} // namespace WebKit
#else // !HAVE(TOUCH_BAR)
namespace WebKit {

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

} // namespace WebKit
#endif // HAVE(TOUCH_BAR)

namespace WebKit {

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

WebViewImpl::WebViewImpl(NSView <WebViewImplDelegate> *view, WKWebView *outerWebView, WebProcessPool& processPool, Ref<API::PageConfiguration>&& configuration)
    : m_view(view)
    , m_pageClient(std::make_unique<PageClientImpl>(view, outerWebView))
    , m_page(processPool.createWebPage(*m_pageClient, WTFMove(configuration)))
    , m_needsViewFrameInWindowCoordinates(m_page->preferences().pluginsEnabled())
    , m_intrinsicContentSize(CGSizeMake(NSViewNoIntrinsicMetric, NSViewNoIntrinsicMetric))
    , m_layoutStrategy([WKViewLayoutStrategy layoutStrategyWithPage:m_page view:view viewImpl:*this mode:kWKLayoutModeViewSize])
    , m_undoTarget(adoptNS([[WKEditorUndoTargetObjC alloc] init]))
    , m_windowVisibilityObserver(adoptNS([[WKWindowVisibilityObserver alloc] initWithView:view impl:*this]))
    , m_accessibilitySettingsObserver(adoptNS([[WKAccessibilitySettingsObserver alloc] initWithImpl:*this]))
    , m_primaryTrackingArea(adoptNS([[NSTrackingArea alloc] initWithRect:view.frame options:trackingAreaOptions() owner:view userInfo:nil]))
{
    static_cast<PageClientImpl&>(*m_pageClient).setImpl(*this);

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    [NSApp registerServicesMenuSendTypes:PasteboardTypes::forSelection() returnTypes:PasteboardTypes::forEditing()];

    [view addTrackingArea:m_primaryTrackingArea.get()];

    m_page->setIntrinsicDeviceScaleFactor(intrinsicDeviceScaleFactor());

    if (Class gestureClass = NSClassFromString(@"NSImmediateActionGestureRecognizer")) {
        m_immediateActionGestureRecognizer = adoptNS([(NSImmediateActionGestureRecognizer *)[gestureClass alloc] init]);
        m_immediateActionController = adoptNS([[WKImmediateActionController alloc] initWithPage:m_page view:view viewImpl:*this recognizer:m_immediateActionGestureRecognizer.get()]);
        [m_immediateActionGestureRecognizer setDelegate:m_immediateActionController.get()];
        [m_immediateActionGestureRecognizer setDelaysPrimaryMouseButtonEvents:NO];
    }

    m_page->setAddsVisitedLinks(processPool.historyClient().addsVisitedLinks());

    m_page->initializeWebPage();

    registerDraggedTypes();

    view.wantsLayer = YES;

    // Explicitly set the layer contents placement so AppKit will make sure that our layer has masksToBounds set to YES.
    view.layerContentsPlacement = NSViewLayerContentsPlacementTopLeft;

#if ENABLE(FULLSCREEN_API) && WK_API_ENABLED
    m_page->setFullscreenClient(std::make_unique<WebKit::FullscreenClient>(view));
#endif

    WebProcessPool::statistics().wkViewCount++;
}

WebViewImpl::~WebViewImpl()
{
#if WK_API_ENABLED
    if (m_remoteObjectRegistry) {
        m_page->process().processPool().removeMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), m_page->pageID());
        [m_remoteObjectRegistry _invalidate];
        m_remoteObjectRegistry = nil;
    }
#endif

    ASSERT(!m_inSecureInputState);

#if WK_API_ENABLED
    ASSERT(!m_thumbnailView);
#endif

    [m_layoutStrategy invalidate];

    [m_immediateActionController willDestroyView:m_view.getAutoreleased()];

#if HAVE(TOUCH_BAR)
    [m_textTouchBarItemController didDestroyView];
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [m_mediaTouchBarProvider setPlaybackControlsController:nil];
    [m_mediaPlaybackControlsView setPlaybackControlsController:nil];
#endif
#endif

    m_page->close();

    WebProcessPool::statistics().wkViewCount--;

}

NSWindow *WebViewImpl::window()
{
    return [m_view window];
}

void WebViewImpl::processDidExit()
{
    dismissContentRelativeChildWindowsWithAnimation(true);

    notifyInputContextAboutDiscardedComposition();

    updateRemoteAccessibilityRegistration(false);
    flushPendingMouseEventCallbacks();

    m_gestureController = nullptr;
}

void WebViewImpl::pageClosed()
{
    updateRemoteAccessibilityRegistration(false);
}

void WebViewImpl::didRelaunchProcess()
{
    accessibilityRegisterUIProcessTokens();
}

void WebViewImpl::setDrawsBackground(bool drawsBackground)
{
    m_page->setDrawsBackground(drawsBackground);

    // Make sure updateLayer gets called on the web view.
    [m_view setNeedsDisplay:YES];
}

bool WebViewImpl::drawsBackground() const
{
    return m_page->drawsBackground();
}

void WebViewImpl::setBackgroundColor(NSColor *backgroundColor)
{
    m_backgroundColor = backgroundColor;

    // Make sure updateLayer gets called on the web view.
    [m_view setNeedsDisplay:YES];
}

NSColor *WebViewImpl::backgroundColor() const
{
    if (!m_backgroundColor)
        return [NSColor whiteColor];
    return m_backgroundColor.get();
}

bool WebViewImpl::isOpaque() const
{
    return m_page->drawsBackground();
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

    NSSelectionDirection direction = [[m_view window] keyViewSelectionDirection];

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
        NSEvent *event = [NSApp currentEvent];
        NSEvent *keyboardEvent = nil;
        if ([event type] == NSEventTypeKeyDown || [event type] == NSEventTypeKeyUp)
            keyboardEvent = event;
        m_page->setInitialFocus(direction == NSSelectingNext, keyboardEvent != nil, NativeWebKeyboardEvent(keyboardEvent, false, false, { }), [](WebKit::CallbackBase::Error) { });
    }
    return true;
}

bool WebViewImpl::resignFirstResponder()
{
#if WK_API_ENABLED
    // Predict the case where we are losing first responder status only to
    // gain it back again. We want resignFirstResponder to do nothing in that case.
    id nextResponder = [[m_view window] _newFirstResponderAfterResigning];

    // FIXME: This will probably need to change once WKWebView doesn't contain a WKView.
    if ([nextResponder isKindOfClass:[WKWebView class]] && [m_view superview] == nextResponder) {
        m_willBecomeFirstResponderAgain = true;
        return true;
    }
#endif

    m_willBecomeFirstResponderAgain = false;
    m_inResignFirstResponder = true;

    m_page->confirmCompositionAsync();

    notifyInputContextAboutDiscardedComposition();

    resetSecureInputState();

    if (!m_page->maintainsInactiveSelection())
        m_page->clearSelection();

    m_page->activityStateDidChange(WebCore::ActivityState::IsFocused);
    
    m_inResignFirstResponder = false;
    
    return true;
}

bool WebViewImpl::isFocused() const
{
    if (m_inBecomeFirstResponder)
        return true;
    if (m_inResignFirstResponder)
        return false;
    return [m_view window].firstResponder == m_view.getAutoreleased();
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

void WebViewImpl::renewGState()
{
    if (m_textIndicatorWindow)
        dismissContentRelativeChildWindowsWithAnimation(false);

    // Update the view frame.
    if ([m_view window])
        updateWindowAndViewFrames();

    updateContentInsetsIfAutomatic();
}

void WebViewImpl::setFrameSize(CGSize)
{
    [m_layoutStrategy didChangeFrameSize];
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

    [m_view frame] = NSRectFromCGRect(frame);
}

void WebViewImpl::updateWindowAndViewFrames()
{
    if (clipsToVisibleRect())
        updateViewExposedRect();

    if (m_didScheduleWindowAndViewFrameUpdate)
        return;

    m_didScheduleWindowAndViewFrameUpdate = true;

    auto weakThis = makeWeakPtr(*this);
    dispatch_async(dispatch_get_main_queue(), [weakThis] {
        if (!weakThis)
            return;

        weakThis->m_didScheduleWindowAndViewFrameUpdate = false;

        NSRect viewFrameInWindowCoordinates = NSZeroRect;
        NSPoint accessibilityPosition = NSZeroPoint;

        if (weakThis->m_needsViewFrameInWindowCoordinates)
            viewFrameInWindowCoordinates = [weakThis->m_view convertRect:[weakThis->m_view frame] toView:nil];

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (WebCore::AXObjectCache::accessibilityEnabled())
            accessibilityPosition = [[weakThis->m_view accessibilityAttributeValue:NSAccessibilityPositionAttribute] pointValue];
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

std::unique_ptr<WebKit::DrawingAreaProxy> WebViewImpl::createDrawingAreaProxy()
{
    if ([[[NSUserDefaults standardUserDefaults] objectForKey:@"WebKit2UseRemoteLayerTreeDrawingArea"] boolValue])
        return std::make_unique<RemoteLayerTreeDrawingAreaProxy>(m_page);

    return std::make_unique<TiledCoreAnimationDrawingAreaProxy>(m_page);
}

bool WebViewImpl::isUsingUISideCompositing() const
{
    auto* drawingArea = m_page->drawingArea();
    return drawingArea && drawingArea->type() == DrawingAreaTypeRemoteLayerTree;
}

void WebViewImpl::setDrawingAreaSize(CGSize size)
{
    if (!m_page->drawingArea())
        return;

    m_page->drawingArea()->setSize(WebCore::IntSize(size), WebCore::IntSize(m_scrollOffsetAdjustment));
    m_scrollOffsetAdjustment = CGSizeZero;
}

void WebViewImpl::updateLayer()
{
    bool draws = drawsBackground();
    if (!draws || !m_backgroundColor)
        [m_view layer].backgroundColor = CGColorGetConstantColor(draws ? kCGColorWhite : kCGColorClear);
    else
        [m_view layer].backgroundColor = [m_backgroundColor CGColor];
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

NSPrintOperation *WebViewImpl::printOperationWithPrintInfo(NSPrintInfo *printInfo, WebFrameProxy& frame)
{
    LOG(Printing, "Creating an NSPrintOperation for frame '%s'", frame.url().string().utf8().data());

    // FIXME: If the frame cannot be printed (e.g. if it contains an encrypted PDF that disallows
    // printing), this function should return nil.
    RetainPtr<WKPrintingView> printingView = adoptNS([[WKPrintingView alloc] initWithFrameProxy:frame view:m_view.getAutoreleased()]);
    // NSPrintOperation takes ownership of the view.
    NSPrintOperation *printOperation = [NSPrintOperation printOperationWithView:printingView.get() printInfo:printInfo];
    [printOperation setCanSpawnSeparateThread:YES];
    [printOperation setJobTitle:frame.title()];
    printingView->_printOperation = printOperation;
    return printOperation;
}

void WebViewImpl::setAutomaticallyAdjustsContentInsets(bool automaticallyAdjustsContentInsets)
{
    m_automaticallyAdjustsContentInsets = automaticallyAdjustsContentInsets;
    updateContentInsetsIfAutomatic();
}

void WebViewImpl::updateContentInsetsIfAutomatic()
{
    if (!m_automaticallyAdjustsContentInsets)
        return;

    NSWindow *window = [m_view window];
    if ((window.styleMask & NSWindowStyleMaskFullSizeContentView) && !window.titlebarAppearsTransparent && ![m_view enclosingScrollView]) {
        NSRect contentLayoutRect = [m_view convertRect:window.contentLayoutRect fromView:nil];
        CGFloat newTopContentInset = NSMaxY(contentLayoutRect) - NSHeight(contentLayoutRect);
        if (m_page->topContentInset() != newTopContentInset)
            setTopContentInset(newTopContentInset);
    } else
        setTopContentInset(0);
}

CGFloat WebViewImpl::topContentInset() const
{
    if (m_didScheduleSetTopContentInset)
        return m_pendingTopContentInset;
    return m_page->topContentInset();
}

void WebViewImpl::setTopContentInset(CGFloat contentInset)
{
    m_pendingTopContentInset = contentInset;

    if (m_didScheduleSetTopContentInset)
        return;

    m_didScheduleSetTopContentInset = true;

    auto weakThis = makeWeakPtr(*this);
    dispatch_async(dispatch_get_main_queue(), [weakThis] {
        if (!weakThis)
            return;
        weakThis->dispatchSetTopContentInset();
    });
}

void WebViewImpl::dispatchSetTopContentInset()
{
    if (!m_didScheduleSetTopContentInset)
        return;

    m_didScheduleSetTopContentInset = false;
    m_page->setTopContentInset(m_pendingTopContentInset);
}

void WebViewImpl::prepareContentInRect(CGRect rect)
{
    m_contentPreparationRect = rect;
    m_useContentPreparationRectForVisibleRect = true;

    updateViewExposedRect();
}

void WebViewImpl::updateViewExposedRect()
{
    CGRect exposedRect = NSRectToCGRect([m_view visibleRect]);

    if (m_useContentPreparationRectForVisibleRect)
        exposedRect = CGRectUnion(m_contentPreparationRect, exposedRect);

    if (auto drawingArea = m_page->drawingArea())
        drawingArea->setViewExposedRect(m_clipsToVisibleRect ? std::optional<WebCore::FloatRect>(exposedRect) : std::nullopt);
}

void WebViewImpl::setClipsToVisibleRect(bool clipsToVisibleRect)
{
    m_clipsToVisibleRect = clipsToVisibleRect;
    updateViewExposedRect();
}

void WebViewImpl::setMinimumSizeForAutoLayout(CGSize minimumSizeForAutoLayout)
{
    bool expandsToFit = minimumSizeForAutoLayout.width > 0;

    m_page->setViewLayoutSize(WebCore::IntSize(minimumSizeForAutoLayout));
    m_page->setMainFrameIsScrollable(!expandsToFit);

    setClipsToVisibleRect(expandsToFit);
}

CGSize WebViewImpl::minimumSizeForAutoLayout() const
{
    return m_page->viewLayoutSize();
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

    CGSize intrinsicContentSizeAcknowledgingFlexibleWidth = intrinsicContentSize;
    if (intrinsicContentSize.width < m_page->viewLayoutSize().width())
        intrinsicContentSizeAcknowledgingFlexibleWidth.width = NSViewNoIntrinsicMetric;

    m_intrinsicContentSize = intrinsicContentSizeAcknowledgingFlexibleWidth;
    [m_view invalidateIntrinsicContentSize];
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

    if (viewScale <= 0 || isnan(viewScale) || isinf(viewScale))
        [NSException raise:NSInvalidArgumentException format:@"View scale should be a positive number"];

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
    m_layoutStrategy = [WKViewLayoutStrategy layoutStrategyWithPage:m_page view:m_view.getAutoreleased() viewImpl:*this mode:layoutMode];
}

bool WebViewImpl::supportsArbitraryLayoutModes() const
{
    if ([m_fullScreenWindowController isFullScreen])
        return false;

    WebFrameProxy* frame = m_page->mainFrame();
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

void WebViewImpl::setOverrideDeviceScaleFactor(CGFloat deviceScaleFactor)
{
    m_overrideDeviceScaleFactor = deviceScaleFactor;
    m_page->setIntrinsicDeviceScaleFactor(intrinsicDeviceScaleFactor());
}

float WebViewImpl::intrinsicDeviceScaleFactor() const
{
    if (m_overrideDeviceScaleFactor)
        return m_overrideDeviceScaleFactor;
    if (m_targetWindowForMovePreparation)
        return m_targetWindowForMovePreparation.backingScaleFactor;
    if (NSWindow *window = [m_view window])
        return window.backingScaleFactor;
    return [NSScreen mainScreen].backingScaleFactor;
}

void WebViewImpl::windowDidOrderOffScreen()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) windowDidOrderOffScreen", this, m_page->pageID());
    m_page->activityStateDidChange({ WebCore::ActivityState::IsVisible, WebCore::ActivityState::WindowIsActive });
}

void WebViewImpl::windowDidOrderOnScreen()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) windowDidOrderOnScreen", this, m_page->pageID());
    m_page->activityStateDidChange({ WebCore::ActivityState::IsVisible, WebCore::ActivityState::WindowIsActive });
}

void WebViewImpl::windowDidBecomeKey(NSWindow *keyWindow)
{
    if (keyWindow == [m_view window] || keyWindow == [m_view window].attachedSheet) {
#if ENABLE(GAMEPAD)
        UIGamepadProvider::singleton().viewBecameActive(m_page.get());
#endif
        updateSecureInputState();
        m_page->activityStateDidChange(WebCore::ActivityState::WindowIsActive);
    }
}

void WebViewImpl::windowDidResignKey(NSWindow *formerKeyWindow)
{
    if (formerKeyWindow == [m_view window] || formerKeyWindow == [m_view window].attachedSheet) {
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

void WebViewImpl::windowDidChangeBackingProperties(CGFloat oldBackingScaleFactor)
{
    CGFloat newBackingScaleFactor = intrinsicDeviceScaleFactor();
    if (oldBackingScaleFactor == newBackingScaleFactor)
        return;

    m_page->setIntrinsicDeviceScaleFactor(newBackingScaleFactor);
}

void WebViewImpl::windowDidChangeScreen()
{
    NSWindow *window = m_targetWindowForMovePreparation ? m_targetWindowForMovePreparation : [m_view window];
    m_page->windowScreenDidChange([[[[window screen] deviceDescription] objectForKey:@"NSScreenNumber"] intValue]);
}

void WebViewImpl::windowDidChangeLayerHosting()
{
    m_page->layerHostingModeDidChange();
}

void WebViewImpl::windowDidChangeOcclusionState()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) windowDidChangeOcclusionState", this, m_page->pageID());
    m_page->activityStateDidChange(WebCore::ActivityState::IsVisible);
}

bool WebViewImpl::mightBeginDragWhileInactive()
{
    if ([m_view window].isKeyWindow)
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
    CFRetain((__bridge CFTypeRef)event);
    CFAutorelease((__bridge CFTypeRef)event);

    if (![m_view hitTest:event.locationInWindow])
        return false;

    setLastMouseDownEvent(event);
    bool result = m_page->acceptsFirstMouse(event.eventNumber, WebEventFactory::createWebMouseEvent(event, m_lastPressureEvent.get(), m_view.getAutoreleased()));
    setLastMouseDownEvent(nil);
    return result;
}

bool WebViewImpl::shouldDelayWindowOrderingForEvent(NSEvent *event)
{
    if (!mightBeginDragWhileInactive())
        return false;

    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    CFRetain((__bridge CFTypeRef)event);
    CFAutorelease((__bridge CFTypeRef)event);

    if (![m_view hitTest:event.locationInWindow])
        return false;

    setLastMouseDownEvent(event);
    bool result = m_page->shouldDelayWindowOrderingForEvent(WebEventFactory::createWebMouseEvent(event, m_lastPressureEvent.get(), m_view.getAutoreleased()));
    setLastMouseDownEvent(nil);
    return result;
}

bool WebViewImpl::windowResizeMouseLocationIsInVisibleScrollerThumb(CGPoint point)
{
    NSPoint localPoint = [m_view convertPoint:NSPointFromCGPoint(point) fromView:nil];
    NSRect visibleThumbRect = NSRect(m_page->visibleScrollerThumbRect());
    return NSMouseInRect(localPoint, visibleThumbRect, [m_view isFlipped]);
}

void WebViewImpl::viewWillMoveToWindow(NSWindow *window)
{
    // If we're in the middle of preparing to move to a window, we should only be moved to that window.
    ASSERT(!m_targetWindowForMovePreparation || (m_targetWindowForMovePreparation == window));

    NSWindow *currentWindow = [m_view window];
    if (window == currentWindow)
        return;

    clearAllEditCommands();

    NSWindow *stopObservingWindow = m_targetWindowForMovePreparation ? m_targetWindowForMovePreparation : [m_view window];
    [m_windowVisibilityObserver stopObserving:stopObservingWindow];
    [m_windowVisibilityObserver startObserving:window];
}

void WebViewImpl::viewDidMoveToWindow()
{
    NSWindow *window = m_targetWindowForMovePreparation ? m_targetWindowForMovePreparation : [m_view window];

    LOG(ActivityState, "WebViewImpl %p viewDidMoveToWindow %p", this, window);

    if (window) {
        windowDidChangeScreen();

        OptionSet<WebCore::ActivityState::Flag> activityStateChanges { WebCore::ActivityState::WindowIsActive, WebCore::ActivityState::IsVisible };
        if (m_shouldDeferViewInWindowChanges)
            m_viewInWindowChangeWasDeferred = true;
        else
            activityStateChanges.add(WebCore::ActivityState::IsInWindow);
        m_page->activityStateDidChange(activityStateChanges);

        updateWindowAndViewFrames();

        // FIXME(135509) This call becomes unnecessary once 135509 is fixed; remove.
        m_page->layerHostingModeDidChange();

        if (!m_flagsChangedEventMonitor) {
            auto weakThis = makeWeakPtr(*this);
            m_flagsChangedEventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskFlagsChanged handler:[weakThis] (NSEvent *flagsChangedEvent) {
                if (weakThis)
                    weakThis->postFakeMouseMovedEventForFlagsChangedEvent(flagsChangedEvent);
                return flagsChangedEvent;
            }];
        }

        accessibilityRegisterUIProcessTokens();

        if (m_immediateActionGestureRecognizer && ![[m_view gestureRecognizers] containsObject:m_immediateActionGestureRecognizer.get()] && !m_ignoresNonWheelEvents && m_allowsLinkPreview)
            [m_view addGestureRecognizer:m_immediateActionGestureRecognizer.get()];
    } else {
        OptionSet<WebCore::ActivityState::Flag> activityStateChanges { WebCore::ActivityState::WindowIsActive, WebCore::ActivityState::IsVisible };
        if (m_shouldDeferViewInWindowChanges)
            m_viewInWindowChangeWasDeferred = true;
        else
            activityStateChanges.add(WebCore::ActivityState::IsInWindow);
        m_page->activityStateDidChange(activityStateChanges);

        [NSEvent removeMonitor:m_flagsChangedEventMonitor];
        m_flagsChangedEventMonitor = nil;

        dismissContentRelativeChildWindowsWithAnimation(false);

        if (m_immediateActionGestureRecognizer) {
            // Work around <rdar://problem/22646404> by explicitly cancelling the animation.
            cancelImmediateActionAnimation();
            [m_view removeGestureRecognizer:m_immediateActionGestureRecognizer.get()];
        }
    }

    m_page->setIntrinsicDeviceScaleFactor(intrinsicDeviceScaleFactor());
}

void WebViewImpl::viewDidChangeBackingProperties()
{
    NSColorSpace *colorSpace = [m_view window].colorSpace;
    if ([colorSpace isEqualTo:m_colorSpace.get()])
        return;

    m_colorSpace = nullptr;
    if (DrawingAreaProxy *drawingArea = m_page->drawingArea())
        drawingArea->colorSpaceDidChange();
}

void WebViewImpl::viewDidHide()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) viewDidHide", this, m_page->pageID());
    m_page->activityStateDidChange(WebCore::ActivityState::IsVisible);
}

void WebViewImpl::viewDidUnhide()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) viewDidUnhide", this, m_page->pageID());
    m_page->activityStateDidChange(WebCore::ActivityState::IsVisible);
}

void WebViewImpl::activeSpaceDidChange()
{
    LOG(ActivityState, "WebViewImpl %p (page %llu) activeSpaceDidChange", this, m_page->pageID());
    m_page->activityStateDidChange(WebCore::ActivityState::IsVisible);
}

NSView *WebViewImpl::hitTest(CGPoint point)
{
    NSView *hitView = [m_view _web_superHitTest:NSPointFromCGPoint(point)];
    if (hitView && hitView == m_layerHostingView)
        hitView = m_view.getAutoreleased();

    return hitView;
}

void WebViewImpl::postFakeMouseMovedEventForFlagsChangedEvent(NSEvent *flagsChangedEvent)
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSEventTypeMouseMoved location:flagsChangedEvent.window.mouseLocationOutsideOfEventStream
        modifierFlags:flagsChangedEvent.modifierFlags timestamp:flagsChangedEvent.timestamp windowNumber:flagsChangedEvent.windowNumber
        context:nullptr eventNumber:0 clickCount:0 pressure:0];
    NativeWebMouseEvent webEvent(fakeEvent, m_lastPressureEvent.get(), m_view.getAutoreleased());
    m_page->handleMouseEvent(webEvent);
}

ColorSpaceData WebViewImpl::colorSpace()
{
    if (!m_colorSpace) {
        if (m_targetWindowForMovePreparation)
            m_colorSpace = m_targetWindowForMovePreparation.colorSpace;
        else if (NSWindow *window = [m_view window])
            m_colorSpace = window.colorSpace;
        else
            m_colorSpace = [NSScreen mainScreen].colorSpace;
    }

    ColorSpaceData colorSpaceData;
    colorSpaceData.cgColorSpace = [m_colorSpace CGColorSpace];
    
    return colorSpaceData;
}

void WebViewImpl::setUnderlayColor(NSColor *underlayColor)
{
    m_page->setUnderlayColor(WebCore::colorFromNSColor(underlayColor));
}

NSColor *WebViewImpl::underlayColor() const
{
    WebCore::Color webColor = m_page->underlayColor();
    if (!webColor.isValid())
        return nil;

    return WebCore::nsColor(webColor);
}

NSColor *WebViewImpl::pageExtendedBackgroundColor() const
{
    WebCore::Color color = m_page->pageExtendedBackgroundColor();
    if (!color.isValid())
        return nil;

    return WebCore::nsColor(color);
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
        dispatchSetTopContentInset();
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
        dispatchSetTopContentInset();
        m_page->activityStateDidChange(WebCore::ActivityState::IsInWindow);
        m_viewInWindowChangeWasDeferred = false;
    }
}

void WebViewImpl::prepareForMoveToWindow(NSWindow *targetWindow, WTF::Function<void()>&& completionHandler)
{
    m_shouldDeferViewInWindowChanges = true;
    viewWillMoveToWindow(targetWindow);
    m_targetWindowForMovePreparation = targetWindow;
    viewDidMoveToWindow();

    m_shouldDeferViewInWindowChanges = false;

    auto weakThis = makeWeakPtr(*this);
    m_page->installActivityStateChangeCompletionHandler([weakThis, completionHandler = WTFMove(completionHandler)]() {
        completionHandler();

        if (!weakThis)
            return;

        ASSERT([weakThis->m_view window] == weakThis->m_targetWindowForMovePreparation);
        weakThis->m_targetWindowForMovePreparation = nil;
    });

    dispatchSetTopContentInset();
    m_page->activityStateDidChange(WebCore::ActivityState::IsInWindow, false, WebPageProxy::ActivityStateChangeDispatchMode::Immediate);
    m_viewInWindowChangeWasDeferred = false;
}

void WebViewImpl::updateSecureInputState()
{
    if (![[m_view window] isKeyWindow] || !isFocused()) {
        if (m_inSecureInputState) {
            DisableSecureEventInput();
            m_inSecureInputState = false;
        }
        return;
    }
    // WKView has a single input context for all editable areas (except for plug-ins).
    NSTextInputContext *context = [m_view _web_superInputContext];
    bool isInPasswordField = m_page->editorState().isInPasswordField;

    if (isInPasswordField) {
        if (!m_inSecureInputState)
            EnableSecureEventInput();
        static NSArray *romanInputSources = [[NSArray alloc] initWithObjects:&NSAllRomanInputSourcesLocaleIdentifier count:1];
        LOG(TextInput, "-> setAllowedInputSourceLocales:romanInputSources");
        [context setAllowedInputSourceLocales:romanInputSources];
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
    if (![[m_view window] isKeyWindow] || m_view.getAutoreleased() != [[m_view window] firstResponder])
        return;

    LOG(TextInput, "-> discardMarkedText");

    [[m_view _web_superInputContext] discardMarkedText]; // Inform the input method that we won't have an inline input area despite having been asked to.
}

void WebViewImpl::setPluginComplexTextInputState(PluginComplexTextInputState pluginComplexTextInputState)
{
    m_pluginComplexTextInputState = pluginComplexTextInputState;

    if (m_pluginComplexTextInputState != PluginComplexTextInputDisabled)
        return;

    // Send back an empty string to the plug-in. This will disable text input.
    m_page->sendComplexTextInputToPlugin(m_pluginComplexTextInputIdentifier, String());
}

void WebViewImpl::setPluginComplexTextInputStateAndIdentifier(PluginComplexTextInputState pluginComplexTextInputState, uint64_t pluginComplexTextInputIdentifier)
{
    if (pluginComplexTextInputIdentifier != m_pluginComplexTextInputIdentifier) {
        // We're asked to update the state for a plug-in that doesn't have focus.
        return;
    }

    setPluginComplexTextInputState(pluginComplexTextInputState);
}

void WebViewImpl::disableComplexTextInputIfNecessary()
{
    if (!m_pluginComplexTextInputIdentifier)
        return;

    if (m_pluginComplexTextInputState != PluginComplexTextInputEnabled)
        return;

    // Check if the text input window has been dismissed.
    if (![[WKTextInputWindowController sharedTextInputWindowController] hasMarkedText])
        setPluginComplexTextInputState(PluginComplexTextInputDisabled);
}

bool WebViewImpl::handlePluginComplexTextInputKeyDown(NSEvent *event)
{
    ASSERT(m_pluginComplexTextInputIdentifier);
    ASSERT(m_pluginComplexTextInputState != PluginComplexTextInputDisabled);

    BOOL usingLegacyCocoaTextInput = m_pluginComplexTextInputState == PluginComplexTextInputEnabledLegacy;

    NSString *string = nil;
    BOOL didHandleEvent = [[WKTextInputWindowController sharedTextInputWindowController] interpretKeyEvent:event usingLegacyCocoaTextInput:usingLegacyCocoaTextInput string:&string];

    if (string) {
        m_page->sendComplexTextInputToPlugin(m_pluginComplexTextInputIdentifier, string);

        if (!usingLegacyCocoaTextInput)
            m_pluginComplexTextInputState = PluginComplexTextInputDisabled;
    }

    return didHandleEvent;
}

bool WebViewImpl::tryHandlePluginComplexTextInputKeyDown(NSEvent *event)
{
    if (!m_pluginComplexTextInputIdentifier || m_pluginComplexTextInputState == PluginComplexTextInputDisabled)
        return NO;

    // Check if the text input window has been dismissed and let the plug-in process know.
    // This is only valid with the updated Cocoa text input spec.
    disableComplexTextInputIfNecessary();

    // Try feeding the keyboard event directly to the plug-in.
    if (m_pluginComplexTextInputState == PluginComplexTextInputEnabledLegacy)
        return handlePluginComplexTextInputKeyDown(event);

    return NO;
}

void WebViewImpl::pluginFocusOrWindowFocusChanged(bool pluginHasFocusAndWindowHasFocus, uint64_t pluginComplexTextInputIdentifier)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    BOOL inputSourceChanged = m_pluginComplexTextInputIdentifier;

    if (pluginHasFocusAndWindowHasFocus) {
        // Check if we're already allowing text input for this plug-in.
        if (pluginComplexTextInputIdentifier == m_pluginComplexTextInputIdentifier)
            return;

        m_pluginComplexTextInputIdentifier = pluginComplexTextInputIdentifier;

    } else {
        // Check if we got a request to unfocus a plug-in that isn't focused.
        if (pluginComplexTextInputIdentifier != m_pluginComplexTextInputIdentifier)
            return;

        m_pluginComplexTextInputIdentifier = 0;
    }

    if (inputSourceChanged) {
        // The input source changed; discard any entered text.
        [[WKTextInputWindowController sharedTextInputWindowController] unmarkText];
    }
    
    // This will force the current input context to be updated to its correct value.
    [NSApp updateWindows];
}

bool WebViewImpl::tryPostProcessPluginComplexTextInputKeyDown(NSEvent *event)
{
    if (!m_pluginComplexTextInputIdentifier || m_pluginComplexTextInputState == PluginComplexTextInputDisabled)
        return NO;

    // In the legacy text input model, the event has already been sent to the input method.
    if (m_pluginComplexTextInputState == PluginComplexTextInputEnabledLegacy)
        return NO;

    return handlePluginComplexTextInputKeyDown(event);
}

void WebViewImpl::handleAcceptedAlternativeText(const String& acceptedAlternative)
{
    m_page->handleAlternativeTextUIResult(acceptedAlternative);
}


NSInteger WebViewImpl::spellCheckerDocumentTag()
{
    if (!m_spellCheckerDocumentTag)
        m_spellCheckerDocumentTag = [NSSpellChecker uniqueSpellDocumentTag];
    return m_spellCheckerDocumentTag.value();
}

void WebViewImpl::pressureChangeWithEvent(NSEvent *event)
{
#if defined(__LP64__)
    if (event == m_lastPressureEvent)
        return;

    if (m_ignoresNonWheelEvents)
        return;

    if (event.phase != NSEventPhaseChanged && event.phase != NSEventPhaseBegan && event.phase != NSEventPhaseEnded)
        return;

    NativeWebMouseEvent webEvent(event, m_lastPressureEvent.get(), m_view.getAutoreleased());
    m_page->handleMouseEvent(webEvent);

    m_lastPressureEvent = event;
#endif
}

#if ENABLE(FULLSCREEN_API)
bool WebViewImpl::hasFullScreenWindowController() const
{
    return !!m_fullScreenWindowController;
}

WKFullScreenWindowController *WebViewImpl::fullScreenWindowController()
{
    if (!m_fullScreenWindowController)
        m_fullScreenWindowController = adoptNS([[WKFullScreenWindowController alloc] initWithWindow:fullScreenWindow() webView:m_view.getAutoreleased() page:m_page]);

    return m_fullScreenWindowController.get();
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
    return [[[WebCoreFullScreenWindow alloc] initWithContentRect:[[NSScreen mainScreen] frame] styleMask:(NSWindowStyleMaskBorderless | NSWindowStyleMaskResizable) backing:NSBackingStoreBuffered defer:NO] autorelease];
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
#if WK_API_ENABLED
        { @selector(_pasteAsQuotation:), "PasteAsQuotation"_s },
#endif
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
    const char* selectorName = sel_getName(selector);
    size_t selectorNameLength = strlen(selectorName);
    if (selectorNameLength < 2 || selectorName[selectorNameLength - 1] != ':')
        return String();
    return String(selectorName, selectorNameLength - 1);
}

bool WebViewImpl::executeSavedCommandBySelector(SEL selector)
{
    LOG(TextInput, "Executing previously saved command %s", sel_getName(selector));
    // The sink does two things: 1) Tells us if the responder went unhandled, and
    // 2) prevents any NSBeep; we don't ever want to beep here.
    RetainPtr<WKResponderChainSink> sink = adoptNS([[WKResponderChainSink alloc] initWithResponderChain:m_view.getAutoreleased()]);
    [m_view _web_superDoCommandBySelector:selector];
    [sink detach];
    return ![sink didReceiveUnhandledCommand];
}

void WebViewImpl::executeEditCommandForSelector(SEL selector, const String& argument)
{
    m_page->executeEditCommand(commandNameForSelector(selector), argument);
}

void WebViewImpl::registerEditCommand(Ref<WebEditCommandProxy>&& command, UndoOrRedo undoOrRedo)
{
    RetainPtr<WKEditCommandObjC> commandObjC = adoptNS([[WKEditCommandObjC alloc] initWithWebEditCommandProxy:command.ptr()]);
    String actionName = WebEditCommandProxy::nameForEditAction(command->editAction());

    NSUndoManager *undoManager = [m_view undoManager];
    [undoManager registerUndoWithTarget:m_undoTarget.get() selector:((undoOrRedo == UndoOrRedo::Undo) ? @selector(undoEditing:) : @selector(redoEditing:)) object:commandObjC.get()];
    if (!actionName.isEmpty())
        [undoManager setActionName:(NSString *)actionName];
}

void WebViewImpl::clearAllEditCommands()
{
    [[m_view undoManager] removeAllActionsWithTarget:m_undoTarget.get()];
}

bool WebViewImpl::writeSelectionToPasteboard(NSPasteboard *pasteboard, NSArray *types)
{
    size_t numTypes = types.count;
    [pasteboard declareTypes:types owner:nil];
    for (size_t i = 0; i < numTypes; ++i) {
        if ([[types objectAtIndex:i] isEqualTo:WebCore::legacyStringPasteboardType()])
            [pasteboard setString:m_page->stringSelectionForPasteboard() forType:WebCore::legacyStringPasteboardType()];
        else {
            RefPtr<WebCore::SharedBuffer> buffer = m_page->dataSelectionForPasteboard([types objectAtIndex:i]);
            [pasteboard setData:buffer ? buffer->createNSData().get() : nil forType:[types objectAtIndex:i]];
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
    bool isValidSendType = false;

    if (sendType && !editorState.selectionIsNone) {
        if (editorState.isInPlugin)
            isValidSendType = [sendType isEqualToString:WebCore::legacyStringPasteboardType()];
        else
            isValidSendType = [PasteboardTypes::forSelection() containsObject:sendType];
    }

    bool isValidReturnType = false;
    if (!returnType)
        isValidReturnType = true;
    else if ([PasteboardTypes::forEditing() containsObject:returnType] && editorState.isContentEditable) {
        // We can insert strings in any editable context.  We can insert other types, like images, only in rich edit contexts.
        isValidReturnType = editorState.isContentRichlyEditable || [returnType isEqualToString:WebCore::legacyStringPasteboardType()];
    }
    if (isValidSendType && isValidReturnType)
        return m_view.getAutoreleased();
    return [[m_view nextResponder] validRequestorForSendType:sendType returnType:returnType];
}

void WebViewImpl::centerSelectionInVisibleArea()
{
    m_page->centerSelectionInVisibleArea();
}

void WebViewImpl::selectionDidChange()
{
    updateFontPanelIfNeeded();
    if (!m_isHandlingAcceptedCandidate)
        m_softSpaceRange = NSMakeRange(NSNotFound, 0);
#if HAVE(TOUCH_BAR)
    updateTouchBar();
    if (!m_page->editorState().isMissingPostLayoutData)
        requestCandidatesForSelectionIfNeeded();
#endif

    [m_view _web_editorStateDidChange];
}

#if WK_API_ENABLED
void WebViewImpl::showShareSheet(const WebCore::ShareDataWithParsedURL& data, WTF::CompletionHandler<void(bool)>&& completionHandler, WKWebView *view)
{
    ASSERT(!_shareSheet);
    if (_shareSheet)
        return;
    
    ASSERT([view respondsToSelector:@selector(shareSheetDidDismiss:)]);
    _shareSheet = adoptNS([[WKShareSheet alloc] initWithView:view]);
    [_shareSheet setDelegate:view];
    
    [_shareSheet presentWithParameters:data completionHandler:WTFMove(completionHandler)];
}
    
void WebViewImpl::shareSheetDidDismiss(WKShareSheet *shareSheet)
{
    ASSERT(_shareSheet == shareSheet);
    
    [_shareSheet setDelegate:nil];
    _shareSheet = nil;
}
#endif

void WebViewImpl::didBecomeEditable()
{
    [m_windowVisibilityObserver startObservingFontPanel];

    dispatch_async(dispatch_get_main_queue(), [] {
        [[NSSpellChecker sharedSpellChecker] _preflightChosenSpellServer];
    });
}

void WebViewImpl::updateFontPanelIfNeeded()
{
    const EditorState& editorState = m_page->editorState();
    if (editorState.selectionIsNone || !editorState.isContentEditable)
        return;
    if ([NSFontPanel sharedFontPanelExists] && [[NSFontPanel sharedFontPanel] isVisible]) {
        m_page->fontAtSelection([](const String& fontName, double fontSize, bool selectionHasMultipleFonts, WebKit::CallbackBase::Error error) {
            NSFont *font = [NSFont fontWithName:fontName size:fontSize];
            if (font)
                [[NSFontManager sharedFontManager] setSelectedFont:font isMultiple:selectionHasMultipleFonts];
        });
    }
}

void WebViewImpl::changeFontColorFromSender(id sender)
{
    if (![sender respondsToSelector:@selector(color)])
        return;

    id color = [sender color];
    if (![color isKindOfClass:NSColor.class])
        return;

    auto& editorState = m_page->editorState();
    if (!editorState.isContentEditable || editorState.selectionIsNone)
        return;

    WebCore::FontAttributeChanges changes;
    changes.setForegroundColor(WebCore::colorFromNSColor((NSColor *)color));
    m_page->changeFontAttributes(WTFMove(changes));
}

void WebViewImpl::changeFontAttributesFromSender(id sender)
{
    auto& editorState = m_page->editorState();
    if (!editorState.isContentEditable || editorState.selectionIsNone)
        return;

    m_page->changeFontAttributes(WebCore::computedFontAttributeChanges(NSFontManager.sharedFontManager, sender));
    updateFontPanelIfNeeded();
}

void WebViewImpl::changeFontFromFontManager()
{
    auto& editorState = m_page->editorState();
    if (!editorState.isContentEditable || editorState.selectionIsNone)
        return;

    m_page->changeFont(WebCore::computedFontChanges(NSFontManager.sharedFontManager));
    updateFontPanelIfNeeded();
}

static NSMenuItem *menuItem(id <NSValidatedUserInterfaceItem> item)
{
    if (![(NSObject *)item isKindOfClass:[NSMenuItem class]])
        return nil;
    return (NSMenuItem *)item;
}

static NSToolbarItem *toolbarItem(id <NSValidatedUserInterfaceItem> item)
{
    if (![(NSObject *)item isKindOfClass:[NSToolbarItem class]])
        return nil;
    return (NSToolbarItem *)item;
}

bool WebViewImpl::validateUserInterfaceItem(id <NSValidatedUserInterfaceItem> item)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    SEL action = [item action];

    if (action == @selector(showGuessPanel:)) {
        if (NSMenuItem *menuItem = WebKit::menuItem(item))
            [menuItem setTitle:WebCore::contextMenuItemTagShowSpellingPanel(![[[NSSpellChecker sharedSpellChecker] spellingPanel] isVisible])];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(checkSpelling:) || action == @selector(changeSpelling:))
        return m_page->editorState().isContentEditable;

    if (action == @selector(toggleContinuousSpellChecking:)) {
        bool enabled = TextChecker::isContinuousSpellCheckingAllowed();
        bool checked = enabled && TextChecker::state().isContinuousSpellCheckingEnabled;
        [menuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return enabled;
    }

    if (action == @selector(toggleGrammarChecking:)) {
        bool checked = TextChecker::state().isGrammarCheckingEnabled;
        [menuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return true;
    }

    if (action == @selector(toggleAutomaticSpellingCorrection:)) {
        bool checked = TextChecker::state().isAutomaticSpellingCorrectionEnabled;
        [menuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(orderFrontSubstitutionsPanel:)) {
        if (NSMenuItem *menuItem = WebKit::menuItem(item))
            [menuItem setTitle:WebCore::contextMenuItemTagShowSubstitutions(![[[NSSpellChecker sharedSpellChecker] substitutionsPanel] isVisible])];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(toggleSmartInsertDelete:)) {
        bool checked = m_page->isSmartInsertDeleteEnabled();
        [menuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticQuoteSubstitution:)) {
        bool checked = TextChecker::state().isAutomaticQuoteSubstitutionEnabled;
        [menuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticDashSubstitution:)) {
        bool checked = TextChecker::state().isAutomaticDashSubstitutionEnabled;
        [menuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticLinkDetection:)) {
        bool checked = TextChecker::state().isAutomaticLinkDetectionEnabled;
        [menuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticTextReplacement:)) {
        bool checked = TextChecker::state().isAutomaticTextReplacementEnabled;
        [menuItem(item) setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
        return m_page->editorState().isContentEditable;
    }

    if (action == @selector(uppercaseWord:) || action == @selector(lowercaseWord:) || action == @selector(capitalizeWord:))
        return m_page->editorState().selectionIsRange && m_page->editorState().isContentEditable;

    if (action == @selector(stopSpeaking:))
        return [NSApp isSpeaking];

    // The centerSelectionInVisibleArea: selector is enabled if there's a selection range or if there's an insertion point in an editable area.
    if (action == @selector(centerSelectionInVisibleArea:))
        return m_page->editorState().selectionIsRange || (m_page->editorState().isContentEditable && !m_page->editorState().selectionIsNone);

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
        auto weakThis = makeWeakPtr(*this);
        m_page->validateCommand(commandName, [weakThis](const String& commandName, bool isEnabled, int32_t state, WebKit::CallbackBase::Error error) {
            if (!weakThis)
                return;

            // If the process exits before the command can be validated, we'll be called back with an error.
            if (error != WebKit::CallbackBase::Error::None)
                return;

            weakThis->setUserInterfaceItemState(commandName, isEnabled, state);
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
        [menuItem(item.get()) setState:state];
        [menuItem(item.get()) setEnabled:enabled];
        [toolbarItem(item.get()) setEnabled:enabled];
        // FIXME <rdar://problem/8803392>: If the item is neither a menu nor toolbar item, it will be left enabled.
    }
}

void WebViewImpl::startSpeaking()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    m_page->getSelectionOrContentsAsString([](const String& string, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None)
            return;
        if (!string)
            return;

        [NSApp speakString:string];
    });
}

void WebViewImpl::stopSpeaking(id sender)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    [NSApp stopSpeaking:sender];
}

void WebViewImpl::showGuessPanel(id sender)
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    NSPanel *spellingPanel = [checker spellingPanel];
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
    NSString *word = [[sender selectedCell] stringValue];

    m_page->changeSpellingToWord(word);
}

void WebViewImpl::toggleContinuousSpellChecking()
{
    bool spellCheckingEnabled = !TextChecker::state().isContinuousSpellCheckingEnabled;
    TextChecker::setContinuousSpellCheckingEnabled(spellCheckingEnabled);

    m_page->process().updateTextCheckerState();
}

bool WebViewImpl::isGrammarCheckingEnabled()
{
    return TextChecker::state().isGrammarCheckingEnabled;
}

void WebViewImpl::setGrammarCheckingEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().isGrammarCheckingEnabled)
        return;

    TextChecker::setGrammarCheckingEnabled(flag);
    m_page->process().updateTextCheckerState();
}

void WebViewImpl::toggleGrammarChecking()
{
    bool grammarCheckingEnabled = !TextChecker::state().isGrammarCheckingEnabled;
    TextChecker::setGrammarCheckingEnabled(grammarCheckingEnabled);

    m_page->process().updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticSpellingCorrection()
{
    TextChecker::setAutomaticSpellingCorrectionEnabled(!TextChecker::state().isAutomaticSpellingCorrectionEnabled);

    m_page->process().updateTextCheckerState();
}

void WebViewImpl::orderFrontSubstitutionsPanel(id sender)
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    NSPanel *substitutionsPanel = [checker substitutionsPanel];
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
    return TextChecker::state().isAutomaticQuoteSubstitutionEnabled;
}

void WebViewImpl::setAutomaticQuoteSubstitutionEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticQuoteSubstitutionEnabled)
        return;

    TextChecker::setAutomaticQuoteSubstitutionEnabled(flag);
    m_page->process().updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticQuoteSubstitution()
{
    TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().isAutomaticQuoteSubstitutionEnabled);
    m_page->process().updateTextCheckerState();
}

bool WebViewImpl::isAutomaticDashSubstitutionEnabled()
{
    return TextChecker::state().isAutomaticDashSubstitutionEnabled;
}

void WebViewImpl::setAutomaticDashSubstitutionEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticDashSubstitutionEnabled)
        return;

    TextChecker::setAutomaticDashSubstitutionEnabled(flag);
    m_page->process().updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticDashSubstitution()
{
    TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().isAutomaticDashSubstitutionEnabled);
    m_page->process().updateTextCheckerState();
}

bool WebViewImpl::isAutomaticLinkDetectionEnabled()
{
    return TextChecker::state().isAutomaticLinkDetectionEnabled;
}

void WebViewImpl::setAutomaticLinkDetectionEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticLinkDetectionEnabled)
        return;

    TextChecker::setAutomaticLinkDetectionEnabled(flag);
    m_page->process().updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticLinkDetection()
{
    TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().isAutomaticLinkDetectionEnabled);
    m_page->process().updateTextCheckerState();
}

bool WebViewImpl::isAutomaticTextReplacementEnabled()
{
    return TextChecker::state().isAutomaticTextReplacementEnabled;
}

void WebViewImpl::setAutomaticTextReplacementEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticTextReplacementEnabled)
        return;
    
    TextChecker::setAutomaticTextReplacementEnabled(flag);
    m_page->process().updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticTextReplacement()
{
    TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().isAutomaticTextReplacementEnabled);
    m_page->process().updateTextCheckerState();
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

void WebViewImpl::requestCandidatesForSelectionIfNeeded()
{
    if (!shouldRequestCandidates())
        return;

    const EditorState& editorState = m_page->editorState();
    if (!editorState.isContentEditable)
        return;

    if (editorState.isMissingPostLayoutData)
        return;

    auto& postLayoutData = editorState.postLayoutData();
    m_lastStringForCandidateRequest = postLayoutData.stringForCandidateRequest;

#if HAVE(ADVANCED_SPELL_CHECKING)
    NSRange selectedRange = NSMakeRange(postLayoutData.candidateRequestStartPosition, postLayoutData.selectedTextLength);
    NSTextCheckingTypes checkingTypes = NSTextCheckingTypeSpelling | NSTextCheckingTypeReplacement | NSTextCheckingTypeCorrection;
    auto weakThis = makeWeakPtr(*this);
    m_lastCandidateRequestSequenceNumber = [[NSSpellChecker sharedSpellChecker] requestCandidatesForSelectedRange:selectedRange inString:postLayoutData.paragraphContextForCandidateRequest types:checkingTypes options:nil inSpellDocumentWithTag:spellCheckerDocumentTag() completionHandler:[weakThis](NSInteger sequenceNumber, NSArray<NSTextCheckingResult *> *candidates) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!weakThis)
                return;
            weakThis->handleRequestedCandidates(sequenceNumber, candidates);
        });
    }];
#endif // HAVE(ADVANCED_SPELL_CHECKING)
}

void WebViewImpl::handleRequestedCandidates(NSInteger sequenceNumber, NSArray<NSTextCheckingResult *> *candidates)
{
    if (!shouldRequestCandidates())
        return;

    if (m_lastCandidateRequestSequenceNumber != sequenceNumber)
        return;

    const EditorState& editorState = m_page->editorState();
    if (!editorState.isContentEditable)
        return;

    // FIXME: It's pretty lame that we have to depend on the most recent EditorState having post layout data,
    // and that we just bail if it is missing.
    if (editorState.isMissingPostLayoutData)
        return;

    auto& postLayoutData = editorState.postLayoutData();
    if (m_lastStringForCandidateRequest != postLayoutData.stringForCandidateRequest)
        return;

#if HAVE(TOUCH_BAR)
    NSRange selectedRange = NSMakeRange(postLayoutData.candidateRequestStartPosition, postLayoutData.selectedTextLength);
    WebCore::IntRect offsetSelectionRect = postLayoutData.selectionClipRect;
    offsetSelectionRect.move(0, offsetSelectionRect.height());

    [candidateListTouchBarItem() setCandidates:candidates forSelectedRange:selectedRange inString:postLayoutData.paragraphContextForCandidateRequest rect:offsetSelectionRect view:m_view.getAutoreleased() completionHandler:nil];
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
    NSRange resultRange = [nsResult range];

    WebCore::TextCheckingResult result;
    result.type = coreTextCheckingType(nsResult.resultType);
    result.location = resultRange.location;
    result.length = resultRange.length;
    result.replacement = nsResult.replacementString;
    return result;
}

void WebViewImpl::handleAcceptedCandidate(NSTextCheckingResult *acceptedCandidate)
{
    const EditorState& editorState = m_page->editorState();
    if (!editorState.isContentEditable)
        return;

    // FIXME: It's pretty lame that we have to depend on the most recent EditorState having post layout data,
    // and that we just bail if it is missing.
    if (editorState.isMissingPostLayoutData)
        return;

    auto& postLayoutData = editorState.postLayoutData();
    if (m_lastStringForCandidateRequest != postLayoutData.stringForCandidateRequest)
        return;

    m_isHandlingAcceptedCandidate = true;
    NSRange range = [acceptedCandidate range];
    if (acceptedCandidate.replacementString && [acceptedCandidate.replacementString length] > 0) {
        NSRange replacedRange = NSMakeRange(range.location, [acceptedCandidate.replacementString length]);
        NSRange softSpaceRange = NSMakeRange(NSMaxRange(replacedRange) - 1, 1);
        if ([acceptedCandidate.replacementString hasSuffix:@" "])
            m_softSpaceRange = softSpaceRange;
    }

    m_page->handleAcceptedCandidate(textCheckingResultFromNSTextCheckingResult(acceptedCandidate));
}

void WebViewImpl::doAfterProcessingAllPendingMouseEvents(dispatch_block_t action)
{
    if (!m_page->isProcessingMouseEvents()) {
        action();
        return;
    }

    m_callbackHandlersAfterProcessingPendingMouseEvents.append(makeBlockPtr(action));
}

void WebViewImpl::didFinishProcessingAllPendingMouseEvents()
{
    flushPendingMouseEventCallbacks();
}

void WebViewImpl::flushPendingMouseEventCallbacks()
{
    for (auto& callback : m_callbackHandlersAfterProcessingPendingMouseEvents)
        callback();

    m_callbackHandlersAfterProcessingPendingMouseEvents.clear();
}

void WebViewImpl::preferencesDidChange()
{
    BOOL needsViewFrameInWindowCoordinates = m_page->preferences().pluginsEnabled();

    if (!!needsViewFrameInWindowCoordinates == !!m_needsViewFrameInWindowCoordinates)
        return;

    m_needsViewFrameInWindowCoordinates = needsViewFrameInWindowCoordinates;
    if ([m_view window])
        updateWindowAndViewFrames();
}

void WebViewImpl::setTextIndicator(WebCore::TextIndicator& textIndicator, WebCore::TextIndicatorWindowLifetime lifetime)
{
    if (!m_textIndicatorWindow)
        m_textIndicatorWindow = std::make_unique<WebCore::TextIndicatorWindow>(m_view.getAutoreleased());

    NSRect textBoundingRectInScreenCoordinates = [[m_view window] convertRectToScreen:[m_view convertRect:textIndicator.textBoundingRectInRootViewCoordinates() toView:nil]];
    m_textIndicatorWindow->setTextIndicator(textIndicator, NSRectToCGRect(textBoundingRectInScreenCoordinates), lifetime);
}

void WebViewImpl::clearTextIndicatorWithAnimation(WebCore::TextIndicatorWindowDismissalAnimation animation)
{
    if (m_textIndicatorWindow)
        m_textIndicatorWindow->clearTextIndicator(animation);
    m_textIndicatorWindow = nullptr;
}

void WebViewImpl::setTextIndicatorAnimationProgress(float progress)
{
    if (m_textIndicatorWindow)
        m_textIndicatorWindow->setAnimationProgress(progress);
}

void WebViewImpl::dismissContentRelativeChildWindowsWithAnimation(bool animate)
{
    [m_view _web_dismissContentRelativeChildWindowsWithAnimation:animate];
}

void WebViewImpl::dismissContentRelativeChildWindowsWithAnimationFromViewOnly(bool animate)
{
    // Calling _clearTextIndicatorWithAnimation here will win out over the animated clear in dismissContentRelativeChildWindowsFromViewOnly.
    // We can't invert these because clients can override (and have overridden) _dismissContentRelativeChildWindows, so it needs to be called.
    // For this same reason, this can't be moved to WebViewImpl without care.
    clearTextIndicatorWithAnimation(animate ? WebCore::TextIndicatorWindowDismissalAnimation::FadeOut : WebCore::TextIndicatorWindowDismissalAnimation::None);
    [m_view _web_dismissContentRelativeChildWindows];
}

void WebViewImpl::dismissContentRelativeChildWindowsFromViewOnly()
{
    bool hasActiveImmediateAction = false;
    hasActiveImmediateAction = [m_immediateActionController hasActiveImmediateAction];

    // FIXME: We don't know which panel we are dismissing, it may not even be in the current page (see <rdar://problem/13875766>).
    if ([m_view window].isKeyWindow || hasActiveImmediateAction) {
        WebCore::DictionaryLookup::hidePopup();

        if (DataDetectorsLibrary())
            [[getDDActionsManagerClass() sharedManager] requestBubbleClosureUnanchorOnFailure:YES];
    }

    clearTextIndicatorWithAnimation(WebCore::TextIndicatorWindowDismissalAnimation::FadeOut);

    [m_immediateActionController dismissContentRelativeChildWindows];

    m_pageClient->dismissCorrectionPanel(WebCore::ReasonForDismissingAlternativeTextIgnored);
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
        [m_view _web_superQuickLookWithEvent:event];
        return;
    }

    NSPoint locationInViewCoordinates = [m_view convertPoint:[event locationInWindow] fromView:nil];
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
        [m_view removeGestureRecognizer:m_immediateActionGestureRecognizer.get()];
    else if (NSGestureRecognizer *immediateActionRecognizer = m_immediateActionGestureRecognizer.get())
        [m_view addGestureRecognizer:immediateActionRecognizer];
}

NSObject *WebViewImpl::immediateActionAnimationControllerForHitTestResult(API::HitTestResult* hitTestResult, uint32_t type, API::Object* userData)
{
    return [m_view _web_immediateActionAnimationControllerForHitTestResultInternal:hitTestResult withType:type userData:userData];
}

void WebViewImpl::didPerformImmediateActionHitTest(const WebHitTestResultData& result, bool contentPreventsDefault, API::Object* userData)
{
    [m_immediateActionController didPerformImmediateActionHitTest:result contentPreventsDefault:contentPreventsDefault userData:userData];
}

void WebViewImpl::prepareForImmediateActionAnimation()
{
    [m_view _web_prepareForImmediateActionAnimation];
}

void WebViewImpl::cancelImmediateActionAnimation()
{
    [m_view _web_cancelImmediateActionAnimation];
}

void WebViewImpl::completeImmediateActionAnimation()
{
    [m_view _web_completeImmediateActionAnimation];
}

void WebViewImpl::didChangeContentSize(CGSize newSize)
{
    [m_view _web_didChangeContentSize:NSSizeFromCGSize(newSize)];
}

void WebViewImpl::didHandleAcceptedCandidate()
{
    m_isHandlingAcceptedCandidate = false;

    [m_view _didHandleAcceptedCandidate];
}

void WebViewImpl::videoControlsManagerDidChange()
{
#if HAVE(TOUCH_BAR)
    updateTouchBar();
#endif

#if ENABLE(FULLSCREEN_API)
    if (hasFullScreenWindowController())
        [fullScreenWindowController() videoControlsManagerDidChange];
#endif
}

void WebViewImpl::setIgnoresNonWheelEvents(bool ignoresNonWheelEvents)
{
    if (m_ignoresNonWheelEvents == ignoresNonWheelEvents)
        return;

    m_ignoresNonWheelEvents = ignoresNonWheelEvents;
    m_page->setShouldDispatchFakeMouseMoveEvents(!ignoresNonWheelEvents);

    if (ignoresNonWheelEvents)
        [m_view removeGestureRecognizer:m_immediateActionGestureRecognizer.get()];
    else if (NSGestureRecognizer *immediateActionRecognizer = m_immediateActionGestureRecognizer.get()) {
        if (m_allowsLinkPreview)
            [m_view addGestureRecognizer:immediateActionRecognizer];
    }
}

void WebViewImpl::setIgnoresAllEvents(bool ignoresAllEvents)
{
    m_ignoresAllEvents = ignoresAllEvents;
    setIgnoresNonWheelEvents(ignoresAllEvents);
}

void WebViewImpl::setIgnoresMouseDraggedEvents(bool ignoresMouseDraggedEvents)
{
    m_ignoresMouseDraggedEvents = ignoresMouseDraggedEvents;
}

void WebViewImpl::setAccessibilityWebProcessToken(NSData *data)
{
    m_remoteAccessibilityChild = data.length ? adoptNS([[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:data]) : nil;
    updateRemoteAccessibilityRegistration(true);
}

void WebViewImpl::updateRemoteAccessibilityRegistration(bool registerProcess)
{
    // When the tree is connected/disconnected, the remote accessibility registration
    // needs to be updated with the pid of the remote process. If the process is going
    // away, that information is not present in WebProcess
    pid_t pid = 0;
    if (registerProcess)
        pid = m_page->process().processIdentifier();
    else if (!registerProcess) {
        pid = [m_remoteAccessibilityChild processIdentifier];
        m_remoteAccessibilityChild = nil;
    }
    if (!pid)
        return;

    if (registerProcess)
        [NSAccessibilityRemoteUIElement registerRemoteUIProcessIdentifier:pid];
    else
        [NSAccessibilityRemoteUIElement unregisterRemoteUIProcessIdentifier:pid];
}

void WebViewImpl::accessibilityRegisterUIProcessTokens()
{
    // Initialize remote accessibility when the window connection has been established.
    NSData *remoteElementToken = [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:m_view.getAutoreleased()];
    NSData *remoteWindowToken = [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:[m_view window]];
    IPC::DataReference elementToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteElementToken bytes]), [remoteElementToken length]);
    IPC::DataReference windowToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteWindowToken bytes]), [remoteWindowToken length]);
    m_page->registerUIProcessAccessibilityTokens(elementToken, windowToken);
}

id WebViewImpl::accessibilityFocusedUIElement()
{
    enableAccessibilityIfNecessary();
    return m_remoteAccessibilityChild.get();
}

id WebViewImpl::accessibilityHitTest(CGPoint)
{
    return accessibilityFocusedUIElement();
}

void WebViewImpl::enableAccessibilityIfNecessary()
{
    if (WebCore::AXObjectCache::accessibilityEnabled())
        return;

    // After enabling accessibility update the window frame on the web process so that the
    // correct accessibility position is transmitted (when AX is off, that position is not calculated).
    WebCore::AXObjectCache::enableAccessibility();
    updateWindowAndViewFrames();
}

id WebViewImpl::accessibilityAttributeValue(NSString *attribute)
{
    enableAccessibilityIfNecessary();

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {

        id child = nil;
        if (m_remoteAccessibilityChild)
            child = m_remoteAccessibilityChild.get();

            if (!child)
                return nil;
        return [NSArray arrayWithObject:child];
    }
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return NSAccessibilityGroupRole;
    if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
        return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, nil);
        if ([attribute isEqualToString:NSAccessibilityParentAttribute])
            return NSAccessibilityUnignoredAncestor([m_view superview]);
            if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
                return @YES;
    
    return [m_view _web_superAccessibilityAttributeValue:attribute];
}

void WebViewImpl::setPrimaryTrackingArea(NSTrackingArea *trackingArea)
{
    [m_view removeTrackingArea:m_primaryTrackingArea.get()];
    m_primaryTrackingArea = trackingArea;
    [m_view addTrackingArea:trackingArea];
}

// Any non-zero value will do, but using something recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

NSTrackingRectTag WebViewImpl::addTrackingRect(CGRect, id owner, void* userData, bool assumeInside)
{
    ASSERT(m_trackingRectOwner == nil);
    m_trackingRectOwner = owner;
    m_trackingRectUserData = userData;
    return TRACKING_RECT_TAG;
}

NSTrackingRectTag WebViewImpl::addTrackingRectWithTrackingNum(CGRect, id owner, void* userData, bool assumeInside, int tag)
{
    ASSERT(tag == 0 || tag == TRACKING_RECT_TAG);
    ASSERT(m_trackingRectOwner == nil);
    m_trackingRectOwner = owner;
    m_trackingRectUserData = userData;
    return TRACKING_RECT_TAG;
}

void WebViewImpl::addTrackingRectsWithTrackingNums(CGRect*, id owner, void** userDataList, bool assumeInside, NSTrackingRectTag *trackingNums, int count)
{
    ASSERT(count == 1);
    ASSERT(trackingNums[0] == 0 || trackingNums[0] == TRACKING_RECT_TAG);
    ASSERT(m_trackingRectOwner == nil);
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
        [m_view _web_superRemoveTrackingRect:tag];
        m_lastToolTipTag = 0;
        return;
    }

    // If any other tracking rect is being removed, we don't know how it was created
    // and it's possible there's a leak involved (see 3500217)
    ASSERT_NOT_REACHED();
}

void WebViewImpl::removeTrackingRects(NSTrackingRectTag *tags, int count)
{
    for (int i = 0; i < count; ++i) {
        int tag = tags[i];
        if (tag == 0)
            continue;
        ASSERT(tag == TRACKING_RECT_TAG);
        m_trackingRectOwner = nil;
    }
}

void WebViewImpl::sendToolTipMouseExited()
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSEventTypeMouseExited
                                                location:NSMakePoint(0, 0)
                                           modifierFlags:0
                                               timestamp:0
                                            windowNumber:[m_view window].windowNumber
                                                 context:NULL
                                             eventNumber:0
                                          trackingNumber:TRACKING_RECT_TAG
                                                userData:m_trackingRectUserData];
    [m_trackingRectOwner mouseExited:fakeEvent];
}

void WebViewImpl::sendToolTipMouseEntered()
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSEventTypeMouseEntered
                                                location:NSMakePoint(0, 0)
                                           modifierFlags:0
                                               timestamp:0
                                            windowNumber:[m_view window].windowNumber
                                                 context:NULL
                                             eventNumber:0
                                          trackingNumber:TRACKING_RECT_TAG
                                                userData:m_trackingRectUserData];
    [m_trackingRectOwner mouseEntered:fakeEvent];
}

NSString *WebViewImpl::stringForToolTip(NSToolTipTag tag)
{
    return nsStringFromWebCoreString(m_page->toolTip());
}

void WebViewImpl::toolTipChanged(const String& oldToolTip, const String& newToolTip)
{
    if (!oldToolTip.isNull())
        sendToolTipMouseExited();
    
    if (!newToolTip.isEmpty()) {
        // See radar 3500217 for why we remove all tooltips rather than just the single one we created.
        [m_view removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        m_lastToolTipTag = [m_view addToolTipRect:wideOpenRect owner:m_view.getAutoreleased() userData:NULL];
        sendToolTipMouseEntered();
    }
}

void WebViewImpl::setAcceleratedCompositingRootLayer(CALayer *rootLayer)
{
    [rootLayer web_disableAllActions];

    m_rootLayer = rootLayer;

#if WK_API_ENABLED
    if (m_thumbnailView) {
        updateThumbnailViewLayer();
        return;
    }
#endif

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    if (rootLayer) {
        if (!m_layerHostingView) {
            // Create an NSView that will host our layer tree.
            m_layerHostingView = adoptNS([[WKFlippedView alloc] initWithFrame:[m_view bounds]]);
            [m_layerHostingView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

            [m_view addSubview:m_layerHostingView.get() positioned:NSWindowBelow relativeTo:nil];

            // Create a root layer that will back the NSView.
            RetainPtr<CALayer> layer = adoptNS([[CALayer alloc] init]);
            [layer setDelegate:[WebActionDisablingCALayerDelegate shared]];
#ifndef NDEBUG
            [layer setName:@"Hosting root layer"];
#endif

            [m_layerHostingView setLayer:layer.get()];
            [m_layerHostingView setWantsLayer:YES];
        }

        [m_layerHostingView layer].sublayers = [NSArray arrayWithObject:rootLayer];
    } else if (m_layerHostingView) {
        [m_layerHostingView removeFromSuperview];
        [m_layerHostingView setLayer:nil];
        [m_layerHostingView setWantsLayer:NO];

        m_layerHostingView = nullptr;
    }

    [CATransaction commit];
}

#if WK_API_ENABLED
void WebViewImpl::setThumbnailView(_WKThumbnailView *thumbnailView)
{
    ASSERT(!m_thumbnailView || !thumbnailView);

    m_thumbnailView = thumbnailView;

    if (thumbnailView)
        updateThumbnailViewLayer();
    else
        setAcceleratedCompositingRootLayer(m_rootLayer.get());
}

void WebViewImpl::reparentLayerTreeInThumbnailView()
{
    m_thumbnailView._thumbnailLayer = m_rootLayer.get();
}

void WebViewImpl::updateThumbnailViewLayer()
{
    _WKThumbnailView *thumbnailView = m_thumbnailView;
    ASSERT(thumbnailView);

    if (thumbnailView._waitingForSnapshot && [m_view window])
        reparentLayerTreeInThumbnailView();
}

void WebViewImpl::setInspectorAttachmentView(NSView *newView)
{
    NSView *oldView = m_inspectorAttachmentView.get();
    if (oldView == newView)
        return;

    m_inspectorAttachmentView = newView;
    m_page->inspector()->attachmentViewDidChange(oldView ? oldView : m_view.getAutoreleased(), newView ? newView : m_view.getAutoreleased());
}

NSView *WebViewImpl::inspectorAttachmentView()
{
    NSView *attachmentView = m_inspectorAttachmentView.get();
    return attachmentView ? attachmentView : m_view.getAutoreleased();
}

_WKRemoteObjectRegistry *WebViewImpl::remoteObjectRegistry()
{
    if (!m_remoteObjectRegistry) {
        m_remoteObjectRegistry = adoptNS([[_WKRemoteObjectRegistry alloc] _initWithWebPageProxy:m_page]);
        m_page->process().processPool().addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), m_page->pageID(), [m_remoteObjectRegistry remoteObjectRegistry]);
    }

    return m_remoteObjectRegistry.get();
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
WKBrowsingContextController *WebViewImpl::browsingContextController()
{
    if (!m_browsingContextController)
        m_browsingContextController = adoptNS([[WKBrowsingContextController alloc] _initWithPageRef:toAPI(m_page.ptr())]);

    return m_browsingContextController.get();
}
ALLOW_DEPRECATED_DECLARATIONS_END
#endif // WK_API_ENABLED

#if ENABLE(DRAG_SUPPORT)
void WebViewImpl::draggedImage(NSImage *, CGPoint endPoint, NSDragOperation operation)
{
    sendDragEndToPage(endPoint, operation);
}

void WebViewImpl::sendDragEndToPage(CGPoint endPoint, NSDragOperation operation)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSPoint windowImageLoc = [[m_view window] convertScreenToBase:NSPointFromCGPoint(endPoint)];
    ALLOW_DEPRECATED_DECLARATIONS_END
    NSPoint windowMouseLoc = windowImageLoc;

    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    m_ignoresMouseDraggedEvents = true;

    m_page->dragEnded(WebCore::IntPoint(windowMouseLoc), WebCore::IntPoint(WebCore::globalPoint(windowMouseLoc, [m_view window])), operation);
}

static WebCore::DragApplicationFlags applicationFlagsForDrag(NSView *view, id <NSDraggingInfo> draggingInfo)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    uint32_t flags = 0;
    if ([NSApp modalWindow])
        flags = WebCore::DragApplicationIsModal;
    if (view.window.attachedSheet)
        flags |= WebCore::DragApplicationHasAttachedSheet;
    if (draggingInfo.draggingSource == view)
        flags |= WebCore::DragApplicationIsSource;
    if ([NSApp currentEvent].modifierFlags & NSEventModifierFlagOption)
        flags |= WebCore::DragApplicationIsCopyKeyDown;
    return static_cast<WebCore::DragApplicationFlags>(flags);

}

NSDragOperation WebViewImpl::draggingEntered(id <NSDraggingInfo> draggingInfo)
{
    WebCore::IntPoint client([m_view convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, [m_view window]));
#if WK_API_ENABLED
    auto dragDestinationAction = static_cast<WebCore::DragDestinationAction>([m_view _web_dragDestinationActionForDraggingInfo:draggingInfo]);
#else
    auto dragDestinationAction = WebCore::DragDestinationActionAny;
#endif
    WebCore::DragData dragData(draggingInfo, client, global, static_cast<WebCore::DragOperation>(draggingInfo.draggingSourceOperationMask), applicationFlagsForDrag(m_view.getAutoreleased(), draggingInfo), dragDestinationAction);

    m_page->resetCurrentDragInformation();
    m_page->dragEntered(dragData, draggingInfo.draggingPasteboard.name);
    m_initialNumberOfValidItemsForDrop = draggingInfo.numberOfValidItemsForDrop;
    return NSDragOperationCopy;
}

NSDragOperation WebViewImpl::draggingUpdated(id <NSDraggingInfo> draggingInfo)
{
    WebCore::IntPoint client([m_view convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, [m_view window]));
#if WK_API_ENABLED
    auto dragDestinationAction = static_cast<WebCore::DragDestinationAction>([m_view _web_dragDestinationActionForDraggingInfo:draggingInfo]);
#else
    auto dragDestinationAction = WebCore::DragDestinationActionAny;
#endif
    WebCore::DragData dragData(draggingInfo, client, global, static_cast<WebCore::DragOperation>(draggingInfo.draggingSourceOperationMask), applicationFlagsForDrag(m_view.getAutoreleased(), draggingInfo), dragDestinationAction);
    m_page->dragUpdated(dragData, draggingInfo.draggingPasteboard.name);

    NSInteger numberOfValidItemsForDrop = m_page->currentDragNumberOfFilesToBeAccepted();

    if (m_page->currentDragOperation() == WebCore::DragOperationNone)
        numberOfValidItemsForDrop = m_initialNumberOfValidItemsForDrop;

    NSDraggingFormation draggingFormation = NSDraggingFormationNone;
    if (m_page->currentDragIsOverFileInput() && numberOfValidItemsForDrop > 0)
        draggingFormation = NSDraggingFormationList;

    if (draggingInfo.numberOfValidItemsForDrop != numberOfValidItemsForDrop)
        [draggingInfo setNumberOfValidItemsForDrop:numberOfValidItemsForDrop];
    if (draggingInfo.draggingFormation != draggingFormation)
        [draggingInfo setDraggingFormation:draggingFormation];

    return m_page->currentDragOperation();
}

void WebViewImpl::draggingExited(id <NSDraggingInfo> draggingInfo)
{
    WebCore::IntPoint client([m_view convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, [m_view window]));
    WebCore::DragData dragData(draggingInfo, client, global, static_cast<WebCore::DragOperation>(draggingInfo.draggingSourceOperationMask), applicationFlagsForDrag(m_view.getAutoreleased(), draggingInfo));
    m_page->dragExited(dragData, draggingInfo.draggingPasteboard.name);
    m_page->resetCurrentDragInformation();
    draggingInfo.numberOfValidItemsForDrop = m_initialNumberOfValidItemsForDrop;
    m_initialNumberOfValidItemsForDrop = 0;
}

bool WebViewImpl::prepareForDragOperation(id <NSDraggingInfo>)
{
    return true;
}

bool WebViewImpl::performDragOperation(id <NSDraggingInfo> draggingInfo)
{
    WebCore::IntPoint client([m_view convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, [m_view window]));
    WebCore::DragData *dragData = new WebCore::DragData(draggingInfo, client, global, static_cast<WebCore::DragOperation>(draggingInfo.draggingSourceOperationMask), applicationFlagsForDrag(m_view.getAutoreleased(), draggingInfo));

    NSArray *types = draggingInfo.draggingPasteboard.types;
    SandboxExtension::Handle sandboxExtensionHandle;
    SandboxExtension::HandleArray sandboxExtensionForUpload;

    if (![types containsObject:PasteboardTypes::WebArchivePboardType] && [types containsObject:WebCore::legacyFilesPromisePasteboardType()]) {
        
        // FIXME: legacyFilesPromisePasteboardType() contains UTIs, not path names. Also, it's not
        // guaranteed that the count of UTIs equals the count of files, since some clients only write
        // unique UTIs.
        NSArray *files = [draggingInfo.draggingPasteboard propertyListForType:WebCore::legacyFilesPromisePasteboardType()];
        if (![files isKindOfClass:[NSArray class]]) {
            delete dragData;
            return false;
        }

        NSString *dropDestinationPath = WebCore::FileSystem::createTemporaryDirectory(@"WebKitDropDestination");
        if (!dropDestinationPath) {
            delete dragData;
            return false;
        }

        size_t fileCount = files.count;
        Vector<String> *fileNames = new Vector<String>;
        NSURL *dropDestination = [NSURL fileURLWithPath:dropDestinationPath isDirectory:YES];
        String pasteboardName = draggingInfo.draggingPasteboard.name;
        [draggingInfo enumerateDraggingItemsWithOptions:0 forView:m_view.getAutoreleased() classes:@[[NSFilePromiseReceiver class]] searchOptions:@{ } usingBlock:^(NSDraggingItem *draggingItem, NSInteger idx, BOOL *stop) {
            NSFilePromiseReceiver *item = draggingItem.item;
            NSDictionary *options = @{ };

            RetainPtr<NSOperationQueue> queue = adoptNS([NSOperationQueue new]);
            [item receivePromisedFilesAtDestination:dropDestination options:options operationQueue:queue.get() reader:^(NSURL *fileURL, NSError *errorOrNil) {
                if (errorOrNil)
                    return;

                dispatch_async(dispatch_get_main_queue(), [this, path = RetainPtr<NSString>(fileURL.path), fileNames, fileCount, dragData, pasteboardName] {
                    fileNames->append(path.get());
                    if (fileNames->size() == fileCount) {
                        SandboxExtension::Handle sandboxExtensionHandle;
                        SandboxExtension::HandleArray sandboxExtensionForUpload;

                        m_page->createSandboxExtensionsIfNeeded(*fileNames, sandboxExtensionHandle, sandboxExtensionForUpload);
                        dragData->setFileNames(*fileNames);
                        m_page->performDragOperation(*dragData, pasteboardName, WTFMove(sandboxExtensionHandle), WTFMove(sandboxExtensionForUpload));
                        delete dragData;
                        delete fileNames;
                    }
                });
            }];
        }];

        return true;
    }

    if ([types containsObject:WebCore::legacyFilenamesPasteboardType()]) {
        NSArray *files = [draggingInfo.draggingPasteboard propertyListForType:WebCore::legacyFilenamesPasteboardType()];
        if (![files isKindOfClass:[NSArray class]]) {
            delete dragData;
            return false;
        }
        
        Vector<String> fileNames;
        
        for (NSString *file in files)
            fileNames.append(file);
        m_page->createSandboxExtensionsIfNeeded(fileNames, sandboxExtensionHandle, sandboxExtensionForUpload);
    }

    m_page->performDragOperation(*dragData, draggingInfo.draggingPasteboard.name, WTFMove(sandboxExtensionHandle), WTFMove(sandboxExtensionForUpload));
    delete dragData;

    return true;
}

NSView *WebViewImpl::hitTestForDragTypes(CGPoint point, NSSet *types)
{
    // This code is needed to support drag and drop when the drag types cannot be matched.
    // This is the case for elements that do not place content
    // in the drag pasteboard automatically when the drag start (i.e. dragging a DIV element).
    if ([[m_view superview] mouse:NSPointFromCGPoint(point) inRect:[m_view frame]])
        return m_view.getAutoreleased();
    return nil;
}

void WebViewImpl::registerDraggedTypes()
{
    auto types = adoptNS([[NSMutableSet alloc] initWithArray:PasteboardTypes::forEditing()]);
    [types addObjectsFromArray:PasteboardTypes::forURL()];
    [types addObject:PasteboardTypes::WebDummyPboardType];
    [m_view registerForDraggedTypes:[types allObjects]];
}

NSString *WebViewImpl::fileNameForFilePromiseProvider(NSFilePromiseProvider *provider, NSString *)
{
    id userInfo = provider.userInfo;
    if (![userInfo isKindOfClass:[WKPromisedAttachmentContext class]])
        return nil;

    return [(WKPromisedAttachmentContext *)userInfo fileName];
}

static NSError *webKitUnknownError()
{
#if WK_API_ENABLED
    return [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil];
#else
    return [NSError errorWithDomain:@"WKErrorDomain" code:1 userInfo:nil];
#endif
}

void WebViewImpl::didPerformDragOperation(bool handled)
{
#if WK_API_ENABLED
    [m_view _web_didPerformDragOperation:handled];
#else
    UNUSED_PARAM(handled);
#endif
}

void WebViewImpl::writeToURLForFilePromiseProvider(NSFilePromiseProvider *provider, NSURL *fileURL, void(^completionHandler)(NSError *))
{
    id userInfo = provider.userInfo;
    if (![userInfo isKindOfClass:[WKPromisedAttachmentContext class]]) {
        completionHandler(webKitUnknownError());
        return;
    }

    WKPromisedAttachmentContext *info = (WKPromisedAttachmentContext *)userInfo;
    auto attachment = m_page->attachmentForIdentifier(info.attachmentIdentifier);
    if (NSFileWrapper *fileWrapper = attachment ? attachment->fileWrapper() : nil) {
        NSError *attachmentWritingError = nil;
        if ([fileWrapper writeToURL:fileURL options:0 originalContentsURL:nil error:&attachmentWritingError])
            completionHandler(nil);
        else
            completionHandler(attachmentWritingError);
        return;
    }

    WebCore::URL blobURL { info.blobURL };
    if (blobURL.isEmpty()) {
        completionHandler(webKitUnknownError());
        return;
    }

    m_page->writeBlobToFilePath(blobURL, fileURL.path, [protectedCompletionHandler = makeBlockPtr(completionHandler)] (bool success) {
        protectedCompletionHandler(success ? nil : webKitUnknownError());
    });
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
    [[m_view window] performWindowDragWithEvent:m_lastMouseDownEvent.get()];
}

void WebViewImpl::startDrag(const WebCore::DragItem& item, const ShareableBitmap::Handle& dragImageHandle)
{
    auto dragImageAsBitmap = ShareableBitmap::create(dragImageHandle);
    if (!dragImageAsBitmap) {
        m_page->dragCancelled();
        return;
    }

    auto dragCGImage = dragImageAsBitmap->makeCGImage();
    auto dragNSImage = adoptNS([[NSImage alloc] initWithCGImage:dragCGImage.get() size:dragImageAsBitmap->size()]);

    WebCore::IntSize size([dragNSImage size]);
    size.scale(1.0 / m_page->deviceScaleFactor());
    [dragNSImage setSize:size];

    // The call below could release the view.
    auto protector = m_view.get();
    auto clientDragLocation = item.dragLocationInWindowCoordinates;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (auto& info = item.promisedAttachmentInfo) {
        NSString *utiType = info.contentType;
        NSString *fileName = info.fileName;
        if (auto attachment = m_page->attachmentForIdentifier(info.attachmentIdentifier)) {
            utiType = attachment->utiType();
            fileName = attachment->fileName();
        }

        auto provider = adoptNS([[NSFilePromiseProvider alloc] initWithFileType:utiType delegate:(id <NSFilePromiseProviderDelegate>)m_view.getAutoreleased()]);
        auto context = adoptNS([[WKPromisedAttachmentContext alloc] initWithIdentifier:info.attachmentIdentifier blobURL:info.blobURL fileName:fileName]);
        [provider setUserInfo:context.get()];
        auto draggingItem = adoptNS([[NSDraggingItem alloc] initWithPasteboardWriter:provider.get()]);
        [draggingItem setDraggingFrame:NSMakeRect(clientDragLocation.x(), clientDragLocation.y() - size.height(), size.width(), size.height()) contents:dragNSImage.get()];
        [m_view beginDraggingSessionWithItems:@[draggingItem.get()] event:m_lastMouseDownEvent.get() source:(id <NSDraggingSource>)m_view.getAutoreleased()];

        ASSERT(info.additionalTypes.size() == info.additionalData.size());
        if (info.additionalTypes.size() == info.additionalData.size()) {
            for (size_t index = 0; index < info.additionalTypes.size(); ++index) {
                auto nsData = info.additionalData[index]->createNSData();
                [pasteboard setData:nsData.get() forType:info.additionalTypes[index]];
            }
        }
        m_page->didStartDrag();
        return;
    }

    [pasteboard setString:@"" forType:PasteboardTypes::WebDummyPboardType];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [m_view dragImage:dragNSImage.get() at:NSPointFromCGPoint(clientDragLocation) offset:NSZeroSize event:m_lastMouseDownEvent.get() pasteboard:pasteboard source:m_view.getAutoreleased() slideBack:YES];
    ALLOW_DEPRECATED_DECLARATIONS_END
    m_page->didStartDrag();
}

static bool matchesExtensionOrEquivalent(const String& filename, const String& extension)
{
    return filename.endsWithIgnoringASCIICase("." + extension)
        || (equalLettersIgnoringASCIICase(extension, "jpeg") && filename.endsWithIgnoringASCIICase(".jpg"));
}

void WebViewImpl::setFileAndURLTypes(NSString *filename, NSString *extension, NSString *title, NSString *url, NSString *visibleURL, NSPasteboard *pasteboard)
{
    if (!matchesExtensionOrEquivalent(filename, extension))
        filename = [[filename stringByAppendingString:@"."] stringByAppendingString:extension];

    [pasteboard setString:visibleURL forType:WebCore::legacyStringPasteboardType()];
    [pasteboard setString:visibleURL forType:PasteboardTypes::WebURLPboardType];
    [pasteboard setString:title forType:PasteboardTypes::WebURLNamePboardType];
    [pasteboard setPropertyList:@[@[visibleURL], @[title]] forType:PasteboardTypes::WebURLsWithTitlesPboardType];
    [pasteboard setPropertyList:@[extension] forType:WebCore::legacyFilesPromisePasteboardType()];
    m_promisedFilename = filename;
    m_promisedURL = url;
}

void WebViewImpl::setPromisedDataForImage(WebCore::Image* image, NSString *filename, NSString *extension, NSString *title, NSString *url, NSString *visibleURL, WebCore::SharedBuffer* archiveBuffer, NSString *pasteboardName)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:pasteboardName];
    RetainPtr<NSMutableArray> types = adoptNS([[NSMutableArray alloc] initWithObjects:WebCore::legacyFilesPromisePasteboardType(), nil]);

    [types addObjectsFromArray:archiveBuffer ? PasteboardTypes::forImagesWithArchive() : PasteboardTypes::forImages()];
    [pasteboard declareTypes:types.get() owner:m_view.getAutoreleased()];
    setFileAndURLTypes(filename, extension, title, url, visibleURL, pasteboard);

    if (archiveBuffer)
        [pasteboard setData:archiveBuffer->createNSData().get() forType:PasteboardTypes::WebArchivePboardType];

    m_promisedImage = image;
}

void WebViewImpl::pasteboardChangedOwner(NSPasteboard *pasteboard)
{
    m_promisedImage = nullptr;
    m_promisedFilename = emptyString();
    m_promisedURL = emptyString();
}

void WebViewImpl::provideDataForPasteboard(NSPasteboard *pasteboard, NSString *type)
{
    // FIXME: need to support NSRTFDPboardType

    if ([type isEqual:WebCore::legacyTIFFPasteboardType()] && m_promisedImage) {
        [pasteboard setData:(__bridge NSData *)m_promisedImage->tiffRepresentation() forType:WebCore::legacyTIFFPasteboardType()];
        m_promisedImage = nullptr;
    }
}

static BOOL fileExists(NSString *path)
{
    struct stat statBuffer;
    return !lstat([path fileSystemRepresentation], &statBuffer);
}

static NSString *pathWithUniqueFilenameForPath(NSString *path)
{
    // "Fix" the filename of the path.
    NSString *filename = filenameByFixingIllegalCharacters([path lastPathComponent]);
    path = [[path stringByDeletingLastPathComponent] stringByAppendingPathComponent:filename];
    
    if (fileExists(path)) {
        // Don't overwrite existing file by appending "-n", "-n.ext" or "-n.ext.ext" to the filename.
        NSString *extensions = nil;
        NSString *pathWithoutExtensions;
        NSString *lastPathComponent = [path lastPathComponent];
        NSRange periodRange = [lastPathComponent rangeOfString:@"."];
        
        if (periodRange.location == NSNotFound) {
            pathWithoutExtensions = path;
        } else {
            extensions = [lastPathComponent substringFromIndex:periodRange.location + 1];
            lastPathComponent = [lastPathComponent substringToIndex:periodRange.location];
            pathWithoutExtensions = [[path stringByDeletingLastPathComponent] stringByAppendingPathComponent:lastPathComponent];
        }
        
        for (unsigned i = 1; ; i++) {
            NSString *pathWithAppendedNumber = [NSString stringWithFormat:@"%@-%d", pathWithoutExtensions, i];
            path = [extensions length] ? [pathWithAppendedNumber stringByAppendingPathExtension:extensions] : pathWithAppendedNumber;
            if (!fileExists(path))
                break;
        }
    }
    
    return path;
}

NSArray *WebViewImpl::namesOfPromisedFilesDroppedAtDestination(NSURL *dropDestination)
{
    RetainPtr<NSFileWrapper> wrapper;
    RetainPtr<NSData> data;
    
    if (m_promisedImage) {
        data = m_promisedImage->data()->createNSData();
        wrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data.get()]);
    } else
        wrapper = adoptNS([[NSFileWrapper alloc] initWithURL:[NSURL URLWithString:m_promisedURL] options:NSFileWrapperReadingImmediate error:nil]);
    
    if (wrapper)
        [wrapper setPreferredFilename:m_promisedFilename];
    else {
        LOG_ERROR("Failed to create image file.");
        return nil;
    }
    
    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[wrapper preferredFilename]];
    path = pathWithUniqueFilenameForPath(path);
    if (![wrapper writeToURL:[NSURL fileURLWithPath:path isDirectory:NO] options:NSFileWrapperWritingWithNameUpdating originalContentsURL:nil error:nullptr])
        LOG_ERROR("Failed to create image file via -[NSFileWrapper writeToURL:options:originalContentsURL:error:]");

    if (!m_promisedURL.isEmpty())
        WebCore::FileSystem::setMetadataURL(String(path), m_promisedURL);
    
    return [NSArray arrayWithObject:[path lastPathComponent]];
}

static RetainPtr<CGImageRef> takeWindowSnapshot(CGSWindowID windowID, bool captureAtNominalResolution)
{
    CGSWindowCaptureOptions options = kCGSCaptureIgnoreGlobalClipShape;
    if (captureAtNominalResolution)
        options |= kCGSWindowCaptureNominalResolution;
    RetainPtr<CFArrayRef> windowSnapshotImages = adoptCF(CGSHWCaptureWindowList(CGSMainConnectionID(), &windowID, 1, options));

    if (windowSnapshotImages && CFArrayGetCount(windowSnapshotImages.get()))
        return checked_cf_cast<CGImageRef>(CFArrayGetValueAtIndex(windowSnapshotImages.get(), 0));

    // Fall back to the non-hardware capture path if we didn't get a snapshot
    // (which usually happens if the window is fully off-screen).
    CGWindowImageOption imageOptions = kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque;
    if (captureAtNominalResolution)
        imageOptions |= kCGWindowImageNominalResolution;
    return adoptCF(CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, windowID, imageOptions));
}

RefPtr<ViewSnapshot> WebViewImpl::takeViewSnapshot()
{
    NSWindow *window = [m_view window];

    CGSWindowID windowID = (CGSWindowID)window.windowNumber;
    if (!windowID || !window.isVisible)
        return nullptr;

    RetainPtr<CGImageRef> windowSnapshotImage = takeWindowSnapshot(windowID, false);
    if (!windowSnapshotImage)
        return nullptr;

    // Work around <rdar://problem/17084993>; re-request the snapshot at kCGWindowImageNominalResolution if it was captured at the wrong scale.
    CGFloat desiredSnapshotWidth = window.frame.size.width * window.screen.backingScaleFactor;
    if (CGImageGetWidth(windowSnapshotImage.get()) != desiredSnapshotWidth)
        windowSnapshotImage = takeWindowSnapshot(windowID, true);

    if (!windowSnapshotImage)
        return nullptr;

    ViewGestureController& gestureController = ensureGestureController();

    NSRect windowCaptureRect;
    WebCore::FloatRect boundsForCustomSwipeViews = gestureController.windowRelativeBoundsForCustomSwipeViews();
    if (!boundsForCustomSwipeViews.isEmpty())
        windowCaptureRect = boundsForCustomSwipeViews;
    else {
        NSRect unobscuredBounds = [m_view bounds];
        float topContentInset = m_page->topContentInset();
        unobscuredBounds.origin.y += topContentInset;
        unobscuredBounds.size.height -= topContentInset;
        windowCaptureRect = [m_view convertRect:unobscuredBounds toView:nil];
    }

    NSRect windowCaptureScreenRect = [window convertRectToScreen:windowCaptureRect];
    CGRect windowScreenRect;
    CGSGetScreenRectForWindow(CGSMainConnectionID(), (CGSWindowID)[window windowNumber], &windowScreenRect);

    NSRect croppedImageRect = windowCaptureRect;
    croppedImageRect.origin.y = windowScreenRect.size.height - windowCaptureScreenRect.size.height - NSMinY(windowCaptureRect);

    auto croppedSnapshotImage = adoptCF(CGImageCreateWithImageInRect(windowSnapshotImage.get(), NSRectToCGRect([window convertRectToBacking:croppedImageRect])));

    auto surface = WebCore::IOSurface::createFromImage(croppedSnapshotImage.get());
    if (!surface)
        return nullptr;

    RefPtr<ViewSnapshot> snapshot = ViewSnapshot::create(WTFMove(surface));
    snapshot->setVolatile(true);

    return snapshot;
}

void WebViewImpl::saveBackForwardSnapshotForCurrentItem()
{
    if (WebBackForwardListItem* item = m_page->backForwardList().currentItem())
        m_page->recordNavigationSnapshot(*item);
}

void WebViewImpl::saveBackForwardSnapshotForItem(WebBackForwardListItem& item)
{
    m_page->recordNavigationSnapshot(item);
}

ViewGestureController& WebViewImpl::ensureGestureController()
{
    if (!m_gestureController)
        m_gestureController = std::make_unique<ViewGestureController>(m_page);
    return *m_gestureController;
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

    m_page->scalePageInViewCoordinates(magnification, WebCore::roundedIntPoint(centerPoint));
}

void WebViewImpl::setMagnification(double magnification)
{
    if (magnification <= 0 || isnan(magnification) || isinf(magnification))
        [NSException raise:NSInvalidArgumentException format:@"Magnification should be a positive number"];

    dismissContentRelativeChildWindowsWithAnimation(false);

    WebCore::FloatPoint viewCenter(NSMidX([m_view bounds]), NSMidY([m_view bounds]));
    m_page->scalePageInViewCoordinates(magnification, roundedIntPoint(viewCenter));
}

double WebViewImpl::magnification() const
{
    if (m_gestureController)
        return m_gestureController->magnification();
    return m_page->pageScaleFactor();
}

void WebViewImpl::setCustomSwipeViews(NSArray *customSwipeViews)
{
    if (!customSwipeViews.count && !m_gestureController)
        return;

    Vector<RetainPtr<NSView>> views;
    views.reserveInitialCapacity(customSwipeViews.count);
    for (NSView *view in customSwipeViews)
        views.uncheckedAppend(view);

    ensureGestureController().setCustomSwipeViews(views);
}

void WebViewImpl::setCustomSwipeViewsTopContentInset(float topContentInset)
{
    ensureGestureController().setCustomSwipeViewsTopContentInset(topContentInset);
}

bool WebViewImpl::tryToSwipeWithEvent(NSEvent *event, bool ignoringPinnedState)
{
    if (!m_allowsBackForwardNavigationGestures)
        return false;

    auto& gestureController = ensureGestureController();

    bool wasIgnoringPinnedState = gestureController.shouldIgnorePinnedState();
    gestureController.setShouldIgnorePinnedState(ignoringPinnedState);

    bool handledEvent = gestureController.handleScrollWheelEvent(event);

    gestureController.setShouldIgnorePinnedState(wasIgnoringPinnedState);
    
    return handledEvent;
}

void WebViewImpl::setDidMoveSwipeSnapshotCallback(BlockPtr<void (CGRect)>&& callback)
{
    if (!m_allowsBackForwardNavigationGestures)
        return;

    ensureGestureController().setDidMoveSwipeSnapshotCallback(WTFMove(callback));
}

void WebViewImpl::scrollWheel(NSEvent *event)
{
    if (m_ignoresAllEvents)
        return;

    if (event.phase == NSEventPhaseBegan)
        dismissContentRelativeChildWindowsWithAnimation(false);

    if (m_allowsBackForwardNavigationGestures && ensureGestureController().handleScrollWheelEvent(event))
        return;

    NativeWebWheelEvent webEvent = NativeWebWheelEvent(event, m_view.getAutoreleased());
    m_page->handleWheelEvent(webEvent);
}

void WebViewImpl::swipeWithEvent(NSEvent *event)
{
    if (m_ignoresNonWheelEvents)
        return;

    if (!m_allowsBackForwardNavigationGestures) {
        [m_view _web_superSwipeWithEvent:event];
        return;
    }

    if (event.deltaX > 0.0)
        m_page->goBack();
    else if (event.deltaX < 0.0)
        m_page->goForward();
    else
        [m_view _web_superSwipeWithEvent:event];
}

void WebViewImpl::magnifyWithEvent(NSEvent *event)
{
    if (!m_allowsMagnification) {
#if ENABLE(MAC_GESTURE_EVENTS)
        NativeWebGestureEvent webEvent = NativeWebGestureEvent(event, m_view.getAutoreleased());
        m_page->handleGestureEvent(webEvent);
#endif
        [m_view _web_superMagnifyWithEvent:event];
        return;
    }

    dismissContentRelativeChildWindowsWithAnimation(false);

    auto& gestureController = ensureGestureController();

#if ENABLE(MAC_GESTURE_EVENTS)
    if (gestureController.hasActiveMagnificationGesture()) {
        gestureController.handleMagnificationGestureEvent(event, [m_view convertPoint:event.locationInWindow fromView:nil]);
        return;
    }

    NativeWebGestureEvent webEvent = NativeWebGestureEvent(event, m_view.getAutoreleased());
    m_page->handleGestureEvent(webEvent);
#else
    gestureController.handleMagnificationGestureEvent(event, [m_view convertPoint:event.locationInWindow fromView:nil]);
#endif
}

void WebViewImpl::smartMagnifyWithEvent(NSEvent *event)
{
    if (!m_allowsMagnification) {
        [m_view _web_superSmartMagnifyWithEvent:event];
        return;
    }

    dismissContentRelativeChildWindowsWithAnimation(false);

    ensureGestureController().handleSmartMagnificationGesture([m_view convertPoint:event.locationInWindow fromView:nil]);
}

void WebViewImpl::setLastMouseDownEvent(NSEvent *event)
{
    ASSERT(!event || event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown || event.type == NSEventTypeOtherMouseDown);

    if (event == m_lastMouseDownEvent.get())
        return;

    m_lastMouseDownEvent = event;
}

#if ENABLE(MAC_GESTURE_EVENTS)
void WebViewImpl::rotateWithEvent(NSEvent *event)
{
    NativeWebGestureEvent webEvent = NativeWebGestureEvent(event, m_view.getAutoreleased());
    m_page->handleGestureEvent(webEvent);
}
#endif

void WebViewImpl::gestureEventWasNotHandledByWebCore(NSEvent *event)
{
    [m_view _web_gestureEventWasNotHandledByWebCore:event];
}

void WebViewImpl::gestureEventWasNotHandledByWebCoreFromViewOnly(NSEvent *event)
{
#if ENABLE(MAC_GESTURE_EVENTS)
    if (m_allowsMagnification && m_gestureController)
        m_gestureController->gestureEventWasNotHandledByWebCore(event, [m_view convertPoint:event.locationInWindow fromView:nil]);
#endif
}

void WebViewImpl::didRestoreScrollPosition()
{
    if (m_gestureController)
        m_gestureController->didRestoreScrollPosition();
}

void WebViewImpl::doneWithKeyEvent(NSEvent *event, bool eventWasHandled)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    if ([event type] != NSEventTypeKeyDown)
        return;

    if (tryPostProcessPluginComplexTextInputKeyDown(event))
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

NSArray *WebViewImpl::validAttributesForMarkedText()
{
    static NSArray *validAttributes;
    if (!validAttributes) {
        validAttributes = @[ NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName, NSMarkedClauseSegmentAttributeName,
#if USE(DICTATION_ALTERNATIVES)
            NSTextAlternativesAttributeName,
#endif
#if USE(INSERTION_UNDO_GROUPING)
            NSTextInsertionUndoableAttributeName,
#endif
        ];
        // NSText also supports the following attributes, but it's
        // hard to tell which are really required for text input to
        // work well; I have not seen any input method make use of them yet.
        //     NSFontAttributeName, NSForegroundColorAttributeName,
        //     NSBackgroundColorAttributeName, NSLanguageAttributeName.
        CFRetain(validAttributes);
    }
    LOG(TextInput, "validAttributesForMarkedText -> (...)");
    return validAttributes;
}

static Vector<WebCore::CompositionUnderline> extractUnderlines(NSAttributedString *string)
{
    Vector<WebCore::CompositionUnderline> result;
    int length = string.string.length;

    for (int i = 0; i < length;) {
        NSRange range;
        NSDictionary *attrs = [string attributesAtIndex:i longestEffectiveRange:&range inRange:NSMakeRange(i, length - i)];

        if (NSNumber *style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
            WebCore::Color color = WebCore::Color::black;
            WebCore::CompositionUnderlineColor compositionUnderlineColor = WebCore::CompositionUnderlineColor::TextColor;
            if (NSColor *colorAttr = [attrs objectForKey:NSUnderlineColorAttributeName]) {
                color = WebCore::colorFromNSColor(colorAttr);
                compositionUnderlineColor = WebCore::CompositionUnderlineColor::GivenColor;
            }
            result.append(WebCore::CompositionUnderline(range.location, NSMaxRange(range), compositionUnderlineColor, color, style.intValue > 1));
        }
        
        i = range.location + range.length;
    }

    return result;
}

static bool eventKeyCodeIsZeroOrNumLockOrFn(NSEvent *event)
{
    unsigned short keyCode = [event keyCode];
    return !keyCode || keyCode == 10 || keyCode == 63;
}

Vector<WebCore::KeypressCommand> WebViewImpl::collectKeyboardLayoutCommandsForEvent(NSEvent *event)
{
    Vector<WebCore::KeypressCommand> commands;

    if ([event type] != NSEventTypeKeyDown)
        return commands;

    ASSERT(!m_collectedKeypressCommands);
    m_collectedKeypressCommands = &commands;

    if (NSTextInputContext *context = inputContext())
        [context handleEventByKeyboardLayout:event];
    else
        [m_view interpretKeyEvents:[NSArray arrayWithObject:event]];

    m_collectedKeypressCommands = nullptr;

    return commands;
}

void WebViewImpl::interpretKeyEvent(NSEvent *event, void(^completionHandler)(BOOL handled, const Vector<WebCore::KeypressCommand>& commands))
{
    // For regular Web content, input methods run before passing a keydown to DOM, but plug-ins get an opportunity to handle the event first.
    // There is no need to collect commands, as the plug-in cannot execute them.
    if (pluginComplexTextInputIdentifier()) {
        completionHandler(NO, { });
        return;
    }

    if (!inputContext()) {
        auto commands = collectKeyboardLayoutCommandsForEvent(event);
        completionHandler(NO, commands);
        return;
    }

    LOG(TextInput, "-> handleEventByInputMethod:%p %@", event, event);
    [inputContext() handleEventByInputMethod:event completionHandler:[weakThis = makeWeakPtr(*this), capturedEvent = retainPtr(event), capturedBlock = makeBlockPtr(completionHandler)](BOOL handled) {
        if (!weakThis) {
            capturedBlock(NO, { });
            return;
        }

        LOG(TextInput, "... handleEventByInputMethod%s handled", handled ? "" : " not");
        if (handled) {
            capturedBlock(YES, { });
            return;
        }

        auto commands = weakThis->collectKeyboardLayoutCommandsForEvent(capturedEvent.get());
        capturedBlock(NO, commands);
    }];
}

void WebViewImpl::doCommandBySelector(SEL selector)
{
    LOG(TextInput, "doCommandBySelector:\"%s\"", sel_getName(selector));

    if (auto* keypressCommands = m_collectedKeypressCommands) {
        WebCore::KeypressCommand command(NSStringFromSelector(selector));
        keypressCommands->append(command);
        LOG(TextInput, "...stored");
        m_page->registerKeypressCommandName(command.commandName);
    } else {
        // FIXME: Send the command to Editor synchronously and only send it along the
        // responder chain if it's a selector that does not correspond to an editing command.
        [m_view _web_superDoCommandBySelector:selector];
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
        LOG(TextInput, "insertText:\"%@\" replacementRange:(%u, %u)", isAttributedString ? [string string] : string, replacementRange.location, replacementRange.length);
    else
        LOG(TextInput, "insertText:\"%@\"", isAttributedString ? [string string] : string);

    NSString *text;
    Vector<WebCore::TextAlternativeWithRange> dictationAlternatives;

    bool registerUndoGroup = false;
    if (isAttributedString) {
#if USE(DICTATION_ALTERNATIVES)
        WebCore::collectDictationTextAlternatives(string, dictationAlternatives);
#endif
#if USE(INSERTION_UNDO_GROUPING)
        registerUndoGroup = WebCore::shouldRegisterInsertionUndoGroup(string);
#endif
        // FIXME: We ignore most attributes from the string, so for example inserting from Character Palette loses font and glyph variation data.
        text = [string string];
    } else
        text = string;

    m_isTextInsertionReplacingSoftSpace = false;
#if HAVE(ADVANCED_SPELL_CHECKING)
    if (m_softSpaceRange.location != NSNotFound && (replacementRange.location == NSMaxRange(m_softSpaceRange) || replacementRange.location == NSNotFound) && replacementRange.length == 0 && [[NSSpellChecker sharedSpellChecker] deletesAutospaceBeforeString:text language:nil]) {
        replacementRange = m_softSpaceRange;
        m_isTextInsertionReplacingSoftSpace = true;
    }
#endif
    m_softSpaceRange = NSMakeRange(NSNotFound, 0);

    // insertText can be called for several reasons:
    // - If it's from normal key event processing (including key bindings), we save the action to perform it later.
    // - If it's from an input method, then we should insert the text now.
    // - If it's sent outside of keyboard event processing (e.g. from Character Viewer, or when confirming an inline input area with a mouse),
    // then we also execute it immediately, as there will be no other chance.
    Vector<WebCore::KeypressCommand>* keypressCommands = m_collectedKeypressCommands;
    if (keypressCommands && !m_isTextInsertionReplacingSoftSpace) {
        ASSERT(replacementRange.location == NSNotFound);
        WebCore::KeypressCommand command("insertText:", text);
        keypressCommands->append(command);
        LOG(TextInput, "...stored");
        m_page->registerKeypressCommandName(command.commandName);
        return;
    }

    String eventText = text;
    eventText.replace(NSBackTabCharacter, NSTabCharacter); // same thing is done in KeyEventMac.mm in WebCore
    if (!dictationAlternatives.isEmpty())
        m_page->insertDictatedTextAsync(eventText, replacementRange, dictationAlternatives, registerUndoGroup);
    else
        m_page->insertTextAsync(eventText, replacementRange, registerUndoGroup, m_isTextInsertionReplacingSoftSpace ? EditingRangeIsRelativeTo::Paragraph : EditingRangeIsRelativeTo::EditableRoot, m_isTextInsertionReplacingSoftSpace);
}

void WebViewImpl::selectedRangeWithCompletionHandler(void(^completionHandlerPtr)(NSRange selectedRange))
{
    auto completionHandler = adoptNS([completionHandlerPtr copy]);

    LOG(TextInput, "selectedRange");
    m_page->getSelectedRangeAsync([completionHandler](const EditingRange& editingRangeResult, WebKit::CallbackBase::Error error) {
        void (^completionHandlerBlock)(NSRange) = (void (^)(NSRange))completionHandler.get();
        if (error != WebKit::CallbackBase::Error::None) {
            LOG(TextInput, "    ...selectedRange failed.");
            completionHandlerBlock(NSMakeRange(NSNotFound, 0));
            return;
        }
        NSRange result = editingRangeResult;
        if (result.location == NSNotFound)
            LOG(TextInput, "    -> selectedRange returned (NSNotFound, %llu)", result.length);
        else
            LOG(TextInput, "    -> selectedRange returned (%llu, %llu)", result.location, result.length);
        completionHandlerBlock(result);
    });
}

void WebViewImpl::markedRangeWithCompletionHandler(void(^completionHandlerPtr)(NSRange markedRange))
{
    auto completionHandler = adoptNS([completionHandlerPtr copy]);

    LOG(TextInput, "markedRange");
    m_page->getMarkedRangeAsync([completionHandler](const EditingRange& editingRangeResult, WebKit::CallbackBase::Error error) {
        void (^completionHandlerBlock)(NSRange) = (void (^)(NSRange))completionHandler.get();
        if (error != WebKit::CallbackBase::Error::None) {
            LOG(TextInput, "    ...markedRange failed.");
            completionHandlerBlock(NSMakeRange(NSNotFound, 0));
            return;
        }
        NSRange result = editingRangeResult;
        if (result.location == NSNotFound)
            LOG(TextInput, "    -> markedRange returned (NSNotFound, %llu)", result.length);
        else
            LOG(TextInput, "    -> markedRange returned (%llu, %llu)", result.location, result.length);
        completionHandlerBlock(result);
    });
}

void WebViewImpl::hasMarkedTextWithCompletionHandler(void(^completionHandlerPtr)(BOOL hasMarkedText))
{
    auto completionHandler = adoptNS([completionHandlerPtr copy]);

    LOG(TextInput, "hasMarkedText");
    m_page->getMarkedRangeAsync([completionHandler](const EditingRange& editingRangeResult, WebKit::CallbackBase::Error error) {
        void (^completionHandlerBlock)(BOOL) = (void (^)(BOOL))completionHandler.get();
        if (error != WebKit::CallbackBase::Error::None) {
            LOG(TextInput, "    ...hasMarkedText failed.");
            completionHandlerBlock(NO);
            return;
        }
        BOOL hasMarkedText = editingRangeResult.location != notFound;
        LOG(TextInput, "    -> hasMarkedText returned %u", hasMarkedText);
        completionHandlerBlock(hasMarkedText);
    });
}

void WebViewImpl::attributedSubstringForProposedRange(NSRange proposedRange, void(^completionHandlerPtr)(NSAttributedString *attrString, NSRange actualRange))
{
    auto completionHandler = adoptNS([completionHandlerPtr copy]);

    LOG(TextInput, "attributedSubstringFromRange:(%u, %u)", proposedRange.location, proposedRange.length);
    m_page->attributedSubstringForCharacterRangeAsync(proposedRange, [completionHandler](const AttributedString& string, const EditingRange& actualRange, WebKit::CallbackBase::Error error) {
        void (^completionHandlerBlock)(NSAttributedString *, NSRange) = (void (^)(NSAttributedString *, NSRange))completionHandler.get();
        if (error != WebKit::CallbackBase::Error::None) {
            LOG(TextInput, "    ...attributedSubstringFromRange failed.");
            completionHandlerBlock(0, NSMakeRange(NSNotFound, 0));
            return;
        }
        LOG(TextInput, "    -> attributedSubstringFromRange returned %@", [string.string.get() string]);
        completionHandlerBlock([[string.string.get() retain] autorelease], actualRange);
    });
}

void WebViewImpl::firstRectForCharacterRange(NSRange range, void(^completionHandlerPtr)(NSRect firstRect, NSRange actualRange))
{
    auto completionHandler = adoptNS([completionHandlerPtr copy]);

    LOG(TextInput, "firstRectForCharacterRange:(%u, %u)", range.location, range.length);

    // Just to match NSTextView's behavior. Regression tests cannot detect this;
    // to reproduce, use a test application from http://bugs.webkit.org/show_bug.cgi?id=4682
    // (type something; try ranges (1, -1) and (2, -1).
    if ((range.location + range.length < range.location) && (range.location + range.length != 0))
        range.length = 0;

    if (range.location == NSNotFound) {
        LOG(TextInput, "    -> NSZeroRect");
        completionHandlerPtr(NSZeroRect, range);
        return;
    }

    auto weakThis = makeWeakPtr(*this);
    m_page->firstRectForCharacterRangeAsync(range, [weakThis, completionHandler](const WebCore::IntRect& rect, const EditingRange& actualRange, WebKit::CallbackBase::Error error) {
        auto completionHandlerBlock = (void (^)(NSRect, NSRange))completionHandler.get();
        if (!weakThis) {
            LOG(TextInput, "    ...firstRectForCharacterRange failed (WebViewImpl was destroyed).");
            completionHandlerBlock(NSZeroRect, NSMakeRange(NSNotFound, 0));
            return;
        }

        if (error != WebKit::CallbackBase::Error::None) {
            LOG(TextInput, "    ...firstRectForCharacterRange failed.");
            completionHandlerBlock(NSZeroRect, NSMakeRange(NSNotFound, 0));
            return;
        }

        NSRect resultRect = [weakThis->m_view convertRect:rect toView:nil];
        resultRect = [[weakThis->m_view window] convertRectToScreen:resultRect];

        LOG(TextInput, "    -> firstRectForCharacterRange returned (%f, %f, %f, %f)", resultRect.origin.x, resultRect.origin.y, resultRect.size.width, resultRect.size.height);
        completionHandlerBlock(resultRect, actualRange);
    });
}

void WebViewImpl::characterIndexForPoint(NSPoint point, void(^completionHandlerPtr)(NSUInteger))
{
    auto completionHandler = adoptNS([completionHandlerPtr copy]);

    LOG(TextInput, "characterIndexForPoint:(%f, %f)", point.x, point.y);

    NSWindow *window = [m_view window];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (window)
        point = [window convertScreenToBase:point];
    ALLOW_DEPRECATED_DECLARATIONS_END
    point = [m_view convertPoint:point fromView:nil];  // the point is relative to the main frame

    m_page->characterIndexForPointAsync(WebCore::IntPoint(point), [completionHandler](uint64_t result, WebKit::CallbackBase::Error error) {
        void (^completionHandlerBlock)(NSUInteger) = (void (^)(NSUInteger))completionHandler.get();
        if (error != WebKit::CallbackBase::Error::None) {
            LOG(TextInput, "    ...characterIndexForPoint failed.");
            completionHandlerBlock(0);
            return;
        }
        if (result == notFound)
            result = NSNotFound;
        LOG(TextInput, "    -> characterIndexForPoint returned %lu", result);
        completionHandlerBlock(result);
    });
}

NSTextInputContext *WebViewImpl::inputContext()
{
    if (pluginComplexTextInputIdentifier()) {
        ASSERT(!m_collectedKeypressCommands); // Should not get here from -_interpretKeyEvent:completionHandler:, we only use WKTextInputWindowController after giving the plug-in a chance to handle keydown natively.
        return [[WKTextInputWindowController sharedTextInputWindowController] inputContext];
    }

    // Disable text input machinery when in non-editable content. An invisible inline input area affects performance, and can prevent Expose from working.
    if (!m_page->editorState().isContentEditable)
        return nil;

    return [m_view _web_superInputContext];
}

void WebViewImpl::unmarkText()
{
    LOG(TextInput, "unmarkText");

    m_page->confirmCompositionAsync();
}

void WebViewImpl::setMarkedText(id string, NSRange selectedRange, NSRange replacementRange)
{
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
    ASSERT(isAttributedString || [string isKindOfClass:[NSString class]]);

    LOG(TextInput, "setMarkedText:\"%@\" selectedRange:(%u, %u) replacementRange:(%u, %u)", isAttributedString ? [string string] : string, selectedRange.location, selectedRange.length, replacementRange.location, replacementRange.length);

    Vector<WebCore::CompositionUnderline> underlines;
    NSString *text;

    if (isAttributedString) {
        // FIXME: We ignore most attributes from the string, so an input method cannot specify e.g. a font or a glyph variation.
        text = [string string];
        underlines = extractUnderlines(string);
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
            m_page->insertTextAsync(text, replacementRange);
        else
            NSBeep();
        return;
    }

    m_page->setCompositionAsync(text, underlines, selectedRange, replacementRange);
}

// Synchronous NSTextInputClient is still implemented to catch spurious sync calls. Remove when that is no longer needed.

NSRange WebViewImpl::selectedRange()
{
    ASSERT_NOT_REACHED();
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
    CFRetain((__bridge CFTypeRef)event);
    CFAutorelease((__bridge CFTypeRef)event);

    // We get Esc key here after processing either Esc or Cmd+period. The former starts as a keyDown, and the latter starts as a key equivalent,
    // but both get transformed to a cancelOperation: command, executing which passes an Esc key event to -performKeyEquivalent:.
    // Don't interpret this event again, avoiding re-entrancy and infinite loops.
    if ([[event charactersIgnoringModifiers] isEqualToString:@"\e"] && !([event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask))
        return [m_view _web_superPerformKeyEquivalent:event];

    if (m_keyDownEventBeingResent) {
        // WebCore has already seen the event, no need for custom processing.
        // Note that we can get multiple events for each event being re-sent. For example, for Cmd+'=' AppKit
        // first performs the original key equivalent, and if that isn't handled, it dispatches a synthetic Cmd+'+'.
        return [m_view _web_superPerformKeyEquivalent:event];
    }

    disableComplexTextInputIfNecessary();

    // Pass key combos through WebCore if there is a key binding available for
    // this event. This lets webpages have a crack at intercepting key-modified keypresses.
    // FIXME: Why is the firstResponder check needed?
    if (m_view.getAutoreleased() == [m_view window].firstResponder) {
        interpretKeyEvent(event, [weakThis = makeWeakPtr(*this), capturedEvent = retainPtr(event)](BOOL handledByInputMethod, const Vector<WebCore::KeypressCommand>& commands) {
            if (weakThis)
                weakThis->m_page->handleKeyboardEvent(NativeWebKeyboardEvent(capturedEvent.get(), handledByInputMethod, false, commands));
        });
        return YES;
    }
    
    return [m_view _web_superPerformKeyEquivalent:event];
}

void WebViewImpl::keyUp(NSEvent *event)
{
    if (ignoresNonWheelEvents())
        return;

    LOG(TextInput, "keyUp:%p %@", event, event);

    m_isTextInsertionReplacingSoftSpace = false;
    interpretKeyEvent(event, [weakThis = makeWeakPtr(*this), capturedEvent = retainPtr(event)](BOOL handledByInputMethod, const Vector<WebCore::KeypressCommand>& commands) {
        ASSERT(!handledByInputMethod || commands.isEmpty());
        if (weakThis)
            weakThis->m_page->handleKeyboardEvent(NativeWebKeyboardEvent(capturedEvent.get(), handledByInputMethod, weakThis->m_isTextInsertionReplacingSoftSpace, commands));
    });
}

void WebViewImpl::keyDown(NSEvent *event)
{
    if (ignoresNonWheelEvents())
        return;

    LOG(TextInput, "keyDown:%p %@%s", event, event, (event == m_keyDownEventBeingResent) ? " (re-sent)" : "");

    if (tryHandlePluginComplexTextInputKeyDown(event)) {
        LOG(TextInput, "...handled by plug-in");
        return;
    }

    // We could be receiving a key down from AppKit if we have re-sent an event
    // that maps to an action that is currently unavailable (for example a copy when
    // there is no range selection).
    // If this is the case we should ignore the key down.
    if (m_keyDownEventBeingResent == event) {
        [m_view _web_superKeyDown:event];
        return;
    }

    m_isTextInsertionReplacingSoftSpace = false;
    interpretKeyEvent(event, [weakThis = makeWeakPtr(*this), capturedEvent = retainPtr(event)](BOOL handledByInputMethod, const Vector<WebCore::KeypressCommand>& commands) {
        ASSERT(!handledByInputMethod || commands.isEmpty());
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

    interpretKeyEvent(event, ^(BOOL handledByInputMethod, const Vector<WebCore::KeypressCommand>& commands) {
        m_page->handleKeyboardEvent(NativeWebKeyboardEvent(event, handledByInputMethod, false, commands));
    });
}

#define NATIVE_MOUSE_EVENT_HANDLER(EventName) \
    void WebViewImpl::EventName(NSEvent *event) \
    { \
        if (m_ignoresNonWheelEvents) \
            return; \
        if (NSTextInputContext *context = [m_view inputContext]) { \
            auto weakThis = makeWeakPtr(*this); \
            RetainPtr<NSEvent> retainedEvent = event; \
            [context handleEvent:event completionHandler:[weakThis, retainedEvent] (BOOL handled) { \
                if (!weakThis) \
                    return; \
                if (handled) \
                    LOG(TextInput, "%s was handled by text input context", String(#EventName).substring(0, String(#EventName).find("Internal")).ascii().data()); \
                else { \
                    NativeWebMouseEvent webEvent(retainedEvent.get(), weakThis->m_lastPressureEvent.get(), weakThis->m_view.getAutoreleased()); \
                    weakThis->m_page->handleMouseEvent(webEvent); \
                } \
            }]; \
            return; \
        } \
        NativeWebMouseEvent webEvent(event, m_lastPressureEvent.get(), m_view.getAutoreleased()); \
        m_page->handleMouseEvent(webEvent); \
    }
#define NATIVE_MOUSE_EVENT_HANDLER_INTERNAL(EventName) \
    void WebViewImpl::EventName(NSEvent *event) \
    { \
        if (m_ignoresNonWheelEvents) \
            return; \
        if (NSTextInputContext *context = [m_view inputContext]) { \
            auto weakThis = makeWeakPtr(*this); \
            RetainPtr<NSEvent> retainedEvent = event; \
            [context handleEvent:event completionHandler:[weakThis, retainedEvent] (BOOL handled) { \
                if (!weakThis) \
                    return; \
                if (handled) \
                    LOG(TextInput, "%s was handled by text input context", String(#EventName).substring(0, String(#EventName).find("Internal")).ascii().data()); \
                else { \
                    NativeWebMouseEvent webEvent(retainedEvent.get(), weakThis->m_lastPressureEvent.get(), weakThis->m_view.getAutoreleased()); \
                    weakThis->m_page->handleMouseEvent(webEvent); \
                } \
            }]; \
            return; \
        } \
        NativeWebMouseEvent webEvent(event, m_lastPressureEvent.get(), m_view.getAutoreleased()); \
        m_page->handleMouseEvent(webEvent); \
    }

NATIVE_MOUSE_EVENT_HANDLER(mouseEntered)
NATIVE_MOUSE_EVENT_HANDLER(mouseExited)
NATIVE_MOUSE_EVENT_HANDLER(otherMouseDown)
NATIVE_MOUSE_EVENT_HANDLER(otherMouseDragged)
NATIVE_MOUSE_EVENT_HANDLER(otherMouseUp)
NATIVE_MOUSE_EVENT_HANDLER(rightMouseDown)
NATIVE_MOUSE_EVENT_HANDLER(rightMouseDragged)
NATIVE_MOUSE_EVENT_HANDLER(rightMouseUp)

NATIVE_MOUSE_EVENT_HANDLER_INTERNAL(mouseMovedInternal)
NATIVE_MOUSE_EVENT_HANDLER_INTERNAL(mouseDownInternal)
NATIVE_MOUSE_EVENT_HANDLER_INTERNAL(mouseUpInternal)
NATIVE_MOUSE_EVENT_HANDLER_INTERNAL(mouseDraggedInternal)

#undef NATIVE_MOUSE_EVENT_HANDLER
#undef NATIVE_MOUSE_EVENT_HANDLER_INTERNAL

void WebViewImpl::mouseMoved(NSEvent *event)
{
    if (m_ignoresNonWheelEvents)
        return;

    // When a view is first responder, it gets mouse moved events even when the mouse is outside its visible rect.
    if (m_view.getAutoreleased() == [m_view window].firstResponder && !NSPointInRect([m_view convertPoint:[event locationInWindow] fromView:nil], [m_view visibleRect]))
        return;

    mouseMovedInternal(event);
}

_WKRectEdge WebViewImpl::pinnedState()
{
#if WK_API_ENABLED
    _WKRectEdge state = _WKRectEdgeNone;
    if (m_page->isPinnedToLeftSide())
        state |= _WKRectEdgeLeft;
    if (m_page->isPinnedToRightSide())
        state |= _WKRectEdgeRight;
    if (m_page->isPinnedToTopSide())
        state |= _WKRectEdgeTop;
    if (m_page->isPinnedToBottomSide())
        state |= _WKRectEdgeBottom;
    return state;
#else
    return 0;
#endif
}

_WKRectEdge WebViewImpl::rubberBandingEnabled()
{
#if WK_API_ENABLED
    _WKRectEdge state = _WKRectEdgeNone;
    if (m_page->rubberBandsAtLeft())
        state |= _WKRectEdgeLeft;
    if (m_page->rubberBandsAtRight())
        state |= _WKRectEdgeRight;
    if (m_page->rubberBandsAtTop())
        state |= _WKRectEdgeTop;
    if (m_page->rubberBandsAtBottom())
        state |= _WKRectEdgeBottom;
    return state;
#else
    return 0;
#endif
}

void WebViewImpl::setRubberBandingEnabled(_WKRectEdge state)
{
#if WK_API_ENABLED
    m_page->setRubberBandsAtLeft(state & _WKRectEdgeLeft);
    m_page->setRubberBandsAtRight(state & _WKRectEdgeRight);
    m_page->setRubberBandsAtTop(state & _WKRectEdgeTop);
    m_page->setRubberBandsAtBottom(state & _WKRectEdgeBottom);
#else
    UNUSED_PARAM(state);
#endif
}

void WebViewImpl::mouseDown(NSEvent *event)
{
    if (m_ignoresNonWheelEvents)
        return;

    setLastMouseDownEvent(event);
    setIgnoresMouseDraggedEvents(false);

    mouseDownInternal(event);
}

void WebViewImpl::mouseUp(NSEvent *event)
{
    if (m_ignoresNonWheelEvents)
        return;

    setLastMouseDownEvent(nil);
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
    NSRect eventScreenPosition = [[m_view window] convertRectToScreen:NSMakeRect(event.locationInWindow.x, event.locationInWindow.y, 0, 0)];
    NSInteger eventWindowNumber = [NSWindow windowNumberAtPoint:eventScreenPosition.origin belowWindowWithWindowNumber:0];
        
    return [m_view window].windowNumber != eventWindowNumber;
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
    return toUserInterfaceLayoutDirection([m_view userInterfaceLayoutDirection]);
}

void WebViewImpl::setUserInterfaceLayoutDirection(NSUserInterfaceLayoutDirection direction)
{
    m_page->setUserInterfaceLayoutDirection(toUserInterfaceLayoutDirection(direction));
}

bool WebViewImpl::beginBackSwipeForTesting()
{
    if (!m_gestureController)
        return false;
    return m_gestureController->beginSimulatedSwipeInDirectionForTesting(ViewGestureController::SwipeDirection::Back);
}

bool WebViewImpl::completeBackSwipeForTesting()
{
    if (!m_gestureController)
        return false;
    return m_gestureController->completeSimulatedSwipeInDirectionForTesting(ViewGestureController::SwipeDirection::Back);
}
    
void WebViewImpl::setUseSystemAppearance(bool useSystemAppearance)
{
    m_page->setUseSystemAppearance(useSystemAppearance);
}

bool WebViewImpl::useSystemAppearance()
{
    return m_page->useSystemAppearance();
}

void WebViewImpl::effectiveAppearanceDidChange()
{
    m_page->effectiveAppearanceDidChange();
}

bool WebViewImpl::effectiveAppearanceIsDark()
{
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    NSAppearanceName appearance = [[m_view effectiveAppearance] bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    return [appearance isEqualToString:NSAppearanceNameDarkAqua];
#else
    return false;
#endif
}

} // namespace WebKit

#endif // PLATFORM(MAC)
