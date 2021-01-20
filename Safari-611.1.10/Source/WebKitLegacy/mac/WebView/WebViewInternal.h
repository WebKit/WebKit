/*
 * Copyright (C) 2005-2020 Apple Inc. All rights reserved.
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

// This header contains WebView declarations that can be used anywhere in WebKit, but are neither SPI nor API.

#import "WebPreferences.h"
#import "WebViewPrivate.h"
#import "WebTypesInternal.h"
#import "WebUIDelegate.h"

#ifdef __cplusplus

#import <WebCore/AlternativeTextClient.h>
#import <WebCore/DragActions.h>
#import <WebCore/FindOptions.h>
#import <WebCore/FloatRect.h>
#import <WebCore/HTMLMediaElementEnums.h>
#import <WebCore/LayoutMilestone.h>
#import <WebCore/PlaybackTargetClientContextIdentifier.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TextIndicatorWindow.h>
#import <WebCore/WebCoreKeyboardUIMode.h>
#import <functional>
#import <wtf/Forward.h>
#import <wtf/NakedPtr.h>
#import <wtf/NakedRef.h>
#import <wtf/RetainPtr.h>

namespace WebCore {
class Element;
class Event;
class Frame;
class HTMLMediaElement;
class HTMLVideoElement;
class KeyboardEvent;
class Page;
class RenderBox;
class TextIndicator;
struct DictationAlternative;
struct DictionaryPopupInfo;
}

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
namespace WebCore {
struct DragItem;
}
#endif

class WebMediaPlaybackTargetPicker;
class WebSelectionServiceController;

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
#import <WebCore/MediaPlaybackTargetContext.h>
#import <WebCore/MediaProducer.h>
#endif

#endif

@class NSCandidateListTouchBarItem;
@class NSTextAlternatives;
@class WebBasePluginPackage;
@class WebDownload;
@class WebImmediateActionController;
@class WebNodeHighlight;

#ifdef __cplusplus

#if ENABLE(DRAG_SUPPORT)
#if USE(APPKIT)
using CocoaDragOperation = NSDragOperation;
#else
using CocoaDragOperation = uint64_t;
#endif

OptionSet<WebCore::DragOperation> coreDragOperationMask(CocoaDragOperation);

WebDragSourceAction kit(Optional<WebCore::DragSourceAction>);
#endif // ENABLE(DRAG_SUPPORT)

WebCore::FindOptions coreOptions(WebFindOptions);

OptionSet<WebCore::LayoutMilestone> coreLayoutMilestones(WebLayoutMilestones);
WebLayoutMilestones kitLayoutMilestones(OptionSet<WebCore::LayoutMilestone>);

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT) && defined(__cplusplus)
@interface WebUITextIndicatorData (WebUITextIndicatorInternal)
- (WebUITextIndicatorData *)initWithImage:(CGImageRef)image textIndicatorData:(const WebCore::TextIndicatorData&)indicatorData scale:(CGFloat)scale;
- (WebUITextIndicatorData *)initWithImage:(CGImageRef)image scale:(CGFloat)scale;
@end
#endif

@interface WebView (WebViewEditingExtras)
- (BOOL)_shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag;
@end

@interface WebView (AllWebViews)
+ (void)_makeAllWebViewsPerformSelector:(SEL)selector;
- (void)_removeFromAllWebViewsSet;
- (void)_addToAllWebViewsSet;
@end

@interface WebView (WebViewInternal)

+ (BOOL)shouldIncludeInWebKitStatistics;

- (WebCore::Frame*)_mainCoreFrame;
- (WebFrame *)_selectedOrMainFrame;

- (void)_clearCredentials;

- (WebCore::KeyboardUIMode)_keyboardUIMode;

- (BOOL)_becomingFirstResponderFromOutside;

- (BOOL)_needsOneShotDrawingSynchronization;
- (void)_setNeedsOneShotDrawingSynchronization:(BOOL)needsSynchronization;
- (void)_scheduleUpdateRendering;
- (BOOL)_flushCompositingChanges;

#if USE(AUTOCORRECTION_PANEL)
- (void)handleAcceptedAlternativeText:(NSString*)text;
#endif

- (void)_getWebCoreDictationAlternatives:(Vector<WebCore::DictationAlternative>&)alternatives fromTextAlternatives:(const Vector<WebCore::TextAlternativeWithRange>&)alternativesWithRange;
- (void)_showDictationAlternativeUI:(const WebCore::FloatRect&)boundingBoxOfDictatedText forDictationContext:(WebCore::DictationContext)dictationContext;
- (void)_removeDictationAlternatives:(WebCore::DictationContext)dictationContext;
- (Vector<String>)_dictationAlternatives:(WebCore::DictationContext)dictationContext;

#if ENABLE(SERVICE_CONTROLS)
- (WebSelectionServiceController&)_selectionServiceController;
#endif

- (void)_windowVisibilityChanged:(NSNotification *)notification;

- (void)_closeWindow;

@end

#endif

#if PLATFORM(IOS_FAMILY)
@interface NSObject (WebSafeForwarder)
- (id)asyncForwarder;
@end
#endif

// FIXME: Temporary way to expose methods that are in the wrong category inside WebView.
@interface WebView (WebViewOtherInternal)

+ (void)_setCacheModel:(WebCacheModel)cacheModel;
+ (WebCacheModel)_cacheModel;

#ifdef __cplusplus
- (NakedPtr<WebCore::Page>)page;
- (WTF::String)_userAgentString;
#endif

#if !PLATFORM(IOS_FAMILY)
- (NSMenu *)_menuForElement:(NSDictionary *)element defaultItems:(NSArray *)items;
#endif
- (id)_UIDelegateForwarder;
#if PLATFORM(IOS_FAMILY)
- (id)_UIDelegateForSelector:(SEL)selector;
#endif
- (id)_editingDelegateForwarder;
- (id)_policyDelegateForwarder;
#if PLATFORM(IOS_FAMILY)
- (id)_frameLoadDelegateForwarder;
- (id)_resourceLoadDelegateForwarder;
- (id)_UIKitDelegateForwarder;
#endif
- (void)_pushPerformingProgrammaticFocus;
- (void)_popPerformingProgrammaticFocus;
#if !PLATFORM(IOS_FAMILY)
- (void)_didStartProvisionalLoadForFrame:(WebFrame *)frame;
#endif
+ (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType allowingPlugins:(BOOL)allowPlugins;
- (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType;
+ (void)_registerPluginMIMEType:(NSString *)MIMEType;
+ (void)_unregisterPluginMIMEType:(NSString *)MIMEType;
+ (BOOL)_canShowMIMEType:(NSString *)MIMEType allowingPlugins:(BOOL)allowPlugins;
- (BOOL)_canShowMIMEType:(NSString *)MIMEType;
+ (NSString *)_MIMETypeForFile:(NSString *)path;
- (WebDownload *)_downloadURL:(NSURL *)URL;
+ (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme;
+ (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme;
- (BOOL)_isPerformingProgrammaticFocus;
- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(NSUInteger)modifierFlags;
- (WebView *)_openNewWindowWithRequest:(NSURLRequest *)request;
#if !PLATFORM(IOS_FAMILY)
- (void)_writeImageForElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard;
- (void)_writeLinkElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard;
- (void)_openFrameInNewWindowFromMenu:(NSMenuItem *)sender;
- (void)_searchWithGoogleFromMenu:(id)sender;
- (void)_searchWithSpotlightFromMenu:(id)sender;
#endif
- (void)_progressCompleted:(WebFrame *)frame;
- (void)_didCommitLoadForFrame:(WebFrame *)frame;
#if !PLATFORM(IOS_FAMILY)
- (void)_didFinishLoadForFrame:(WebFrame *)frame;
- (void)_didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
- (void)_didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
#endif
- (void)_willChangeValueForKey:(NSString *)key;
- (void)_didChangeValueForKey:(NSString *)key;
- (WebBasePluginPackage *)_pluginForMIMEType:(NSString *)MIMEType;
- (WebBasePluginPackage *)_pluginForExtension:(NSString *)extension;

- (void)setCurrentNodeHighlight:(WebNodeHighlight *)nodeHighlight;
- (WebNodeHighlight *)currentNodeHighlight;

#if !PLATFORM(IOS_FAMILY)
- (void)addPluginInstanceView:(NSView *)view;
- (void)removePluginInstanceView:(NSView *)view;
- (void)removePluginInstanceViewsFor:(WebFrame*)webFrame;
#endif

- (void)_addObject:(id)object forIdentifier:(unsigned long)identifier;
- (id)_objectForIdentifier:(unsigned long)identifier;
- (void)_removeObjectForIdentifier:(unsigned long)identifier;

- (void)_setZoomMultiplier:(float)multiplier isTextOnly:(BOOL)isTextOnly;
- (float)_zoomMultiplier:(BOOL)isTextOnly;
- (float)_realZoomMultiplier;
- (BOOL)_realZoomMultiplierIsTextOnly;
- (BOOL)_canZoomOut:(BOOL)isTextOnly;
- (BOOL)_canZoomIn:(BOOL)isTextOnly;
- (IBAction)_zoomOut:(id)sender isTextOnly:(BOOL)isTextOnly;
- (IBAction)_zoomIn:(id)sender isTextOnly:(BOOL)isTextOnly;
- (BOOL)_canResetZoom:(BOOL)isTextOnly;
- (IBAction)_resetZoom:(id)sender isTextOnly:(BOOL)isTextOnly;

+ (BOOL)_canHandleRequest:(NSURLRequest *)request forMainFrame:(BOOL)forMainFrame;

#if !PLATFORM(IOS_FAMILY)
- (void)_setInsertionPasteboard:(NSPasteboard *)pasteboard;
#endif

#if PLATFORM(IOS_FAMILY)
- (BOOL)_isStopping;
- (BOOL)_isClosing;

- (void)_documentScaleChanged;
- (BOOL)_fetchCustomFixedPositionLayoutRect:(NSRect*)rect;
#if ENABLE(ORIENTATION_EVENTS)
- (void)_setDeviceOrientation:(NSUInteger)orientation;
- (NSUInteger)_deviceOrientation;
#endif
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT) && defined(__cplusplus)
- (void)_startDrag:(const WebCore::DragItem&)dragItem;
- (void)_didConcludeEditDrag;
#endif

- (void)_preferencesChanged:(WebPreferences *)preferences;
- (void)_invalidateUserAgentCache;

#if ENABLE(VIDEO) && defined(__cplusplus)
#if ENABLE(VIDEO_PRESENTATION_MODE)
- (void)_enterVideoFullscreenForVideoElement:(NakedPtr<WebCore::HTMLVideoElement>)videoElement mode:(WebCore::HTMLMediaElementEnums::VideoFullscreenMode)mode;
- (void)_exitVideoFullscreen;
#if PLATFORM(MAC)
- (BOOL)_hasActiveVideoForControlsInterface;
- (void)_setUpPlaybackControlsManagerForMediaElement:(NakedRef<WebCore::HTMLMediaElement>)mediaElement;
- (void)_clearPlaybackControlsManager;
- (void)_playbackControlsMediaEngineChanged;
#endif
#endif
#endif

#if ENABLE(FULLSCREEN_API) && !PLATFORM(IOS_FAMILY) && defined(__cplusplus)
- (BOOL)_supportsFullScreenForElement:(NakedPtr<WebCore::Element>)element withKeyboard:(BOOL)withKeyboard;
- (void)_enterFullScreenForElement:(NakedPtr<WebCore::Element>)element;
- (void)_exitFullScreenForElement:(NakedPtr<WebCore::Element>)element;
#endif

// Conversion functions between WebCore root view coordinates and web view coordinates.
- (NSPoint)_convertPointFromRootView:(NSPoint)point;
- (NSRect)_convertRectFromRootView:(NSRect)rect;

- (void)_setMaintainsInactiveSelection:(BOOL)shouldMaintainInactiveSelection;

#if PLATFORM(MAC) && defined(__cplusplus)
- (void)_setTextIndicator:(WebCore::TextIndicator&)textIndicator;
- (void)_setTextIndicator:(WebCore::TextIndicator&)textIndicator withLifetime:(WebCore::TextIndicatorWindowLifetime)lifetime;
- (void)_clearTextIndicatorWithAnimation:(WebCore::TextIndicatorWindowDismissalAnimation)animation;
- (void)_setTextIndicatorAnimationProgress:(float)progress;
- (void)_showDictionaryLookupPopup:(const WebCore::DictionaryPopupInfo&)dictionaryPopupInfo;
- (id)_animationControllerForDictionaryLookupPopupInfo:(const WebCore::DictionaryPopupInfo&)dictionaryPopupInfo;
- (WebImmediateActionController *)_immediateActionController;
- (NSEvent *)_pressureEvent;
- (void)_setPressureEvent:(NSEvent *)event;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY) && defined(__cplusplus)
- (WebMediaPlaybackTargetPicker *) _devicePicker;
- (void)_addPlaybackTargetPickerClient:(WebCore::PlaybackTargetClientContextIdentifier)contextId;
- (void)_removePlaybackTargetPickerClient:(WebCore::PlaybackTargetClientContextIdentifier)contextId;
- (void)_showPlaybackTargetPicker:(WebCore::PlaybackTargetClientContextIdentifier)contextId location:(const WebCore::IntPoint&)location hasVideo:(BOOL)hasVideo;
- (void)_playbackTargetPickerClientStateDidChange:(WebCore::PlaybackTargetClientContextIdentifier)contextId state:(WebCore::MediaProducer::MediaStateFlags)state;
- (void)_setMockMediaPlaybackTargetPickerEnabled:(bool)enabled;
- (void)_setMockMediaPlaybackTargetPickerName:(NSString *)name state:(WebCore::MediaPlaybackTargetContext::State)state;
- (void)_mockMediaPlaybackTargetPickerDismissPopup;
#endif

- (void)prepareForMouseUp;
- (void)prepareForMouseDown;
- (void)updateTouchBar;
- (void)_dismissTextTouchBarPopoverItemWithIdentifier:(NSString *)identifier;
- (NSCandidateListTouchBarItem *)candidateList;

- (void)showFormValidationMessage:(NSString *)message withAnchorRect:(NSRect)anchorRect;
- (void)hideFormValidationMessage;

#if !PLATFORM(IOS_FAMILY)
- (void)_setMainFrameIcon:(NSImage *)icon;
#endif
@end

@interface WebView (WebViewInternalPreferencesChangedGenerated)
// Implemented in generated file WebViewPreferencesChangedGenerated.mm
- (void)_preferencesChangedGenerated:(WebPreferences *)preferences;
@end
