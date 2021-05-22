/*
 * Copyright (C) 2005-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 David Smith (catfish.man@gmail.com)
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

#import "WebViewInternal.h"
#import "WebViewData.h"

#import "BackForwardList.h"
#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "PageStorageSessionProvider.h"
#import "StorageThread.h"
#import "WebAlternativeTextClient.h"
#import "WebApplicationCacheInternal.h"
#import "WebArchive.h"
#import "WebBackForwardListInternal.h"
#import "WebBaseNetscapePluginView.h"
#import "WebCache.h"
#import "WebChromeClient.h"
#import "WebDOMOperationsPrivate.h"
#import "WebDataSourceInternal.h"
#import "WebDatabaseManagerPrivate.h"
#import "WebDatabaseProvider.h"
#import "WebDefaultEditingDelegate.h"
#import "WebDefaultPolicyDelegate.h"
#import "WebDefaultUIDelegate.h"
#import "WebDelegateImplementationCaching.h"
#import "WebDeviceOrientationClient.h"
#import "WebDeviceOrientationProvider.h"
#import "WebDocument.h"
#import "WebDocumentInternal.h"
#import "WebDownload.h"
#import "WebDragClient.h"
#import "WebDynamicScrollBarsViewInternal.h"
#import "WebEditingDelegate.h"
#import "WebEditorClient.h"
#import "WebFormDelegatePrivate.h"
#import "WebFrameInternal.h"
#import "WebFrameLoadDelegatePrivate.h"
#import "WebFrameLoaderClient.h"
#import "WebFrameNetworkingContext.h"
#import "WebFrameViewInternal.h"
#import "WebGeolocationClient.h"
#import "WebGeolocationPositionInternal.h"
#import "WebHTMLRepresentation.h"
#import "WebHTMLViewInternal.h"
#import "WebHistoryDelegate.h"
#import "WebHistoryItemInternal.h"
#import "WebIconDatabase.h"
#import "WebInspector.h"
#import "WebInspectorClient.h"
#import "WebKitErrors.h"
#import "WebKitFullScreenListener.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitStatisticsPrivate.h"
#import "WebKitVersionChecks.h"
#import "WebLocalizableStrings.h"
#import "WebMediaKeySystemClient.h"
#import "WebNSDataExtras.h"
#import "WebNSDataExtrasPrivate.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNSViewExtras.h"
#import "WebNodeHighlight.h"
#import "WebNotificationClient.h"
#import "WebPDFView.h"
#import "WebPaymentCoordinatorClient.h"
#import "WebPlatformStrategies.h"
#import "WebPluginDatabase.h"
#import "WebPluginInfoProvider.h"
#import "WebPolicyDelegate.h"
#import "WebPreferenceKeysPrivate.h"
#import "WebPreferencesPrivate.h"
#import "WebProgressTrackerClient.h"
#import "WebResourceLoadDelegate.h"
#import "WebResourceLoadDelegatePrivate.h"
#import "WebResourceLoadScheduler.h"
#import "WebScriptDebugDelegate.h"
#import "WebScriptWorldInternal.h"
#import "WebSelectionServiceController.h"
#import "WebStorageManagerInternal.h"
#import "WebStorageNamespaceProvider.h"
#import "WebTextCompletionController.h"
#import "WebTextIterator.h"
#import "WebUIDelegatePrivate.h"
#import "WebValidationMessageClient.h"
#import "WebViewGroup.h"
#import "WebVisitedLinkStore.h"
#import <CoreFoundation/CFSet.h>
#import <Foundation/NSURLConnection.h>
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/ArrayPrototype.h>
#import <JavaScriptCore/CatchScope.h>
#import <JavaScriptCore/DateInstance.h>
#import <JavaScriptCore/Exception.h>
#import <JavaScriptCore/InitializeThreading.h>
#import <JavaScriptCore/JSCJSValue.h>
#import <JavaScriptCore/JSGlobalObjectInlines.h>
#import <JavaScriptCore/JSLock.h>
#import <JavaScriptCore/JSValueRef.h>
#import <WebCore/AlternativeTextUIController.h>
#import <WebCore/ApplicationCacheStorage.h>
#import <WebCore/BackForwardCache.h>
#import <WebCore/BackForwardController.h>
#import <WebCore/CacheStorageProvider.h>
#import <WebCore/Chrome.h>
#import <WebCore/ColorMac.h>
#import <WebCore/ColorSerialization.h>
#import <WebCore/CookieJar.h>
#import <WebCore/DatabaseManager.h>
#import <WebCore/DeprecatedGlobalSettings.h>
#import <WebCore/DictationAlternative.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/DragController.h>
#import <WebCore/DragData.h>
#import <WebCore/DragItem.h>
#import <WebCore/DummySpeechRecognitionProvider.h>
#import <WebCore/Editing.h>
#import <WebCore/Editor.h>
#import <WebCore/Event.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FocusController.h>
#import <WebCore/FontAttributes.h>
#import <WebCore/FontCache.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameSelection.h>
#import <WebCore/FrameTree.h>
#import <WebCore/FrameView.h>
#import <WebCore/FullscreenManager.h>
#import <WebCore/GCController.h>
#import <WebCore/GameControllerGamepadProvider.h>
#import <WebCore/GeolocationController.h>
#import <WebCore/GeolocationError.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLOListElement.h>
#import <WebCore/HTMLUListElement.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/HistoryController.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/JSCSSStyleDeclaration.h>
#import <WebCore/JSDocument.h>
#import <WebCore/JSElement.h>
#import <WebCore/JSNodeList.h>
#import <WebCore/JSNotification.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/LegacySchemeRegistry.h>
#import <WebCore/LibWebRTCProvider.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/LogInitialization.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/MediaRecorderProvider.h>
#import <WebCore/MemoryCache.h>
#import <WebCore/MemoryRelease.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NodeList.h>
#import <WebCore/Notification.h>
#import <WebCore/NotificationController.h>
#import <WebCore/Page.h>
#import <WebCore/PageConfiguration.h>
#import <WebCore/PathUtilities.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/ProgressTracker.h>
#import <WebCore/Range.h>
#import <WebCore/RenderTheme.h>
#import <WebCore/RenderView.h>
#import <WebCore/RenderWidget.h>
#import <WebCore/ResourceHandle.h>
#import <WebCore/ResourceLoadObserver.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/RuntimeEnabledFeatures.h>
#import <WebCore/ScriptController.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SecurityPolicy.h>
#import <WebCore/Settings.h>
#import <WebCore/ShouldTreatAsContinuingLoad.h>
#import <WebCore/SocketProvider.h>
#import <WebCore/SocketStreamHandleImpl.h>
#import <WebCore/StringUtilities.h>
#import <WebCore/StyleProperties.h>
#import <WebCore/TextResourceDecoder.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/TranslationContextMenuInfo.h>
#import <WebCore/UTIRegistry.h>
#import <WebCore/UserAgent.h>
#import <WebCore/UserContentController.h>
#import <WebCore/UserGestureIndicator.h>
#import <WebCore/UserScript.h>
#import <WebCore/UserStyleSheet.h>
#import <WebCore/ValidationBubble.h>
#import <WebCore/WebCoreJITOperations.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebCoreView.h>
#import <WebCore/WebViewVisualIdentificationOverlay.h>
#import <WebCore/Widget.h>
#import <WebKitLegacy/DOM.h>
#import <WebKitLegacy/DOMExtensions.h>
#import <WebKitLegacy/DOMPrivate.h>
#import <mach-o/dyld.h>
#import <objc/runtime.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/NSTouchBarSPI.h>
#import <pal/spi/cocoa/NSURLDownloadSPI.h>
#import <pal/spi/cocoa/NSURLFileTypeMappingsSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/mac/NSResponderSPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>
#import <pal/spi/mac/NSViewSPI.h>
#import <pal/spi/mac/NSWindowSPI.h>
#import <wtf/Assertions.h>
#import <wtf/BlockPtr.h>
#import <wtf/FileSystem.h>
#import <wtf/HashTraits.h>
#import <wtf/Language.h>
#import <wtf/MainThread.h>
#import <wtf/MathExtras.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RAMSize.h>
#import <wtf/RefCountedLeakCounter.h>
#import <wtf/RefPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/SetForScope.h>
#import <wtf/SoftLinking.h>
#import <wtf/StdLibExtras.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/darwin/dyldSPI.h>

#if !PLATFORM(IOS_FAMILY)
#import "WebContextMenuClient.h"
#import "WebFullScreenController.h"
#import "WebImmediateActionController.h"
#import "WebNSEventExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSPasteboardExtras.h"
#import "WebNSPrintOperationExtras.h"
#import "WebPDFView.h"
#import "WebSwitchingGPUClient.h"
#import "WebVideoFullscreenController.h"
#import <WebCore/TextIndicator.h>
#import <WebCore/TextIndicatorWindow.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/mac/LookupSPI.h>
#import <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#else
#import "WebCaretChangeListener.h"
#import "WebChromeClientIOS.h"
#import "WebDefaultFormDelegate.h"
#import "WebDefaultFrameLoadDelegate.h"
#import "WebDefaultResourceLoadDelegate.h"
#import "WebDefaultUIKitDelegate.h"
#import "WebFixedPositionContent.h"
#import "WebMailDelegate.h"
#import "WebNSUserDefaultsExtras.h"
#import "WebPDFViewIOS.h"
#import "WebPlainWhiteView.h"
#import "WebPluginController.h"
#import "WebPolicyDelegatePrivate.h"
#import "WebStorageManagerPrivate.h"
#import "WebUIKitSupport.h"
#import "WebVisiblePosition.h"
#import <WebCore/EventNames.h>
#import <WebCore/FontCache.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/LegacyTileCache.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/ResourceLoadStatistics.h>
#import <WebCore/SQLiteDatabaseTracker.h>
#import <WebCore/SmartReplace.h>
#import <WebCore/TileControllerMemoryHandlerIOS.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WKView.h>
#import <WebCore/WebCoreThread.h>
#import <WebCore/WebCoreThreadMessage.h>
#import <WebCore/WebCoreThreadRun.h>
#import <WebCore/WebEvent.h>
#import <WebCore/WebSQLiteDatabaseTrackerClient.h>
#import <WebCore/WebVideoFullscreenControllerAVKit.h>
#import <libkern/OSAtomic.h>
#import <pal/spi/ios/ManagedConfigurationSPI.h>
#import <pal/spi/ios/MobileGestaltSPI.h>
#import <wtf/FastMalloc.h>
#endif

#if ENABLE(REMOTE_INSPECTOR)
#import <JavaScriptCore/RemoteInspector.h>
#if PLATFORM(IOS_FAMILY)
#import "WebIndicateLayer.h"
#endif
#endif

#if USE(QUICK_LOOK)
#import <WebCore/QuickLook.h>
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
#import <WebCore/WebEventRegion.h>
#endif

#if ENABLE(GAMEPAD)
#import <WebCore/HIDGamepadProvider.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
#import "WebMediaPlaybackTargetPicker.h"
#import <WebCore/WebMediaSessionManagerMac.h>
#endif


#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
#import <WebCore/PlaybackSessionInterfaceMac.h>
#import <WebCore/PlaybackSessionModelMediaElement.h>
#endif

#if HAVE(TRANSLATION_UI_SERVICES)
#import <TranslationUIServices/LTUITranslationViewController.h>

@interface LTUITranslationViewController (Staging_77660675)
@property (nonatomic, copy) void(^replacementHandler)(NSAttributedString *);
@end

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(TranslationUIServices)
SOFT_LINK_CLASS_OPTIONAL(TranslationUIServices, LTUITranslationViewController)
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIColor.h>
#import <UIKit/UIImage.h>
#import <pal/ios/UIKitSoftLink.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/ManagedConfigurationSoftLink.h>
#endif

#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVTouchBarPlaybackControlsProvider)
SOFT_LINK_CLASS(AVKit, AVTouchBarScrubber)
#endif

#if !PLATFORM(IOS_FAMILY)

@interface NSSpellChecker (WebNSSpellCheckerDetails)
- (void)_preflightChosenSpellServer;
@end

@interface NSView (WebNSViewDetails)
- (NSView *)_hitTest:(NSPoint *)aPoint dragTypes:(NSSet *)types;
- (void)_autoscrollForDraggingInfo:(id)dragInfo timeDelta:(NSTimeInterval)repeatDelta;
- (BOOL)_shouldAutoscrollForDraggingInfo:(id)dragInfo;
- (void)_windowChangedKeyState;
@end

@interface NSWindow (WebNSWindowDetails)
- (void)_enableScreenUpdatesIfNeeded;
- (BOOL)_wrapsCarbonWindow;
- (BOOL)_hasKeyAppearance;
@end

#endif

#define FOR_EACH_RESPONDER_SELECTOR(macro) \
macro(alignCenter) \
macro(alignJustified) \
macro(alignLeft) \
macro(alignRight) \
macro(capitalizeWord) \
macro(centerSelectionInVisibleArea) \
macro(changeAttributes) \
_Pragma("clang diagnostic push") \
_Pragma("clang diagnostic ignored \"-Wundeclared-selector\"") \
macro(changeBaseWritingDirection) \
macro(changeBaseWritingDirectionToLTR) \
macro(changeBaseWritingDirectionToRTL) \
_Pragma("clang diagnostic pop") \
macro(changeColor) \
macro(changeDocumentBackgroundColor) \
macro(changeFont) \
macro(changeSpelling) \
macro(checkSpelling) \
macro(complete) \
macro(copy) \
macro(copyFont) \
macro(cut) \
macro(delete) \
macro(deleteBackward) \
macro(deleteBackwardByDecomposingPreviousCharacter) \
macro(deleteForward) \
macro(deleteToBeginningOfLine) \
macro(deleteToBeginningOfParagraph) \
macro(deleteToEndOfLine) \
macro(deleteToEndOfParagraph) \
macro(deleteToMark) \
macro(deleteWordBackward) \
macro(deleteWordForward) \
macro(ignoreSpelling) \
macro(indent) \
macro(insertBacktab) \
macro(insertLineBreak) \
macro(insertNewline) \
macro(insertNewlineIgnoringFieldEditor) \
macro(insertParagraphSeparator) \
macro(insertTab) \
macro(insertTabIgnoringFieldEditor) \
macro(lowercaseWord) \
macro(makeBaseWritingDirectionLeftToRight) \
macro(makeBaseWritingDirectionRightToLeft) \
macro(makeTextWritingDirectionLeftToRight) \
macro(makeTextWritingDirectionNatural) \
macro(makeTextWritingDirectionRightToLeft) \
macro(moveBackward) \
macro(moveBackwardAndModifySelection) \
macro(moveDown) \
macro(moveDownAndModifySelection) \
macro(moveForward) \
macro(moveForwardAndModifySelection) \
macro(moveLeft) \
macro(moveLeftAndModifySelection) \
macro(moveParagraphBackwardAndModifySelection) \
macro(moveParagraphForwardAndModifySelection) \
macro(moveRight) \
macro(moveRightAndModifySelection) \
macro(moveToBeginningOfDocument) \
macro(moveToBeginningOfDocumentAndModifySelection) \
macro(moveToBeginningOfLine) \
macro(moveToBeginningOfLineAndModifySelection) \
macro(moveToBeginningOfParagraph) \
macro(moveToBeginningOfParagraphAndModifySelection) \
macro(moveToBeginningOfSentence) \
macro(moveToBeginningOfSentenceAndModifySelection) \
macro(moveToEndOfDocument) \
macro(moveToEndOfDocumentAndModifySelection) \
macro(moveToEndOfLine) \
macro(moveToEndOfLineAndModifySelection) \
macro(moveToEndOfParagraph) \
macro(moveToEndOfParagraphAndModifySelection) \
macro(moveToEndOfSentence) \
macro(moveToEndOfSentenceAndModifySelection) \
macro(moveToLeftEndOfLine) \
macro(moveToLeftEndOfLineAndModifySelection) \
macro(moveToRightEndOfLine) \
macro(moveToRightEndOfLineAndModifySelection) \
macro(moveUp) \
macro(moveUpAndModifySelection) \
macro(moveWordBackward) \
macro(moveWordBackwardAndModifySelection) \
macro(moveWordForward) \
macro(moveWordForwardAndModifySelection) \
macro(moveWordLeft) \
macro(moveWordLeftAndModifySelection) \
macro(moveWordRight) \
macro(moveWordRightAndModifySelection) \
macro(orderFrontSubstitutionsPanel) \
macro(outdent) \
macro(overWrite) \
macro(pageDown) \
macro(pageDownAndModifySelection) \
macro(pageUp) \
macro(pageUpAndModifySelection) \
macro(paste) \
macro(pasteAsPlainText) \
macro(pasteAsRichText) \
macro(pasteFont) \
macro(performFindPanelAction) \
macro(scrollLineDown) \
macro(scrollLineUp) \
macro(scrollPageDown) \
macro(scrollPageUp) \
macro(scrollToBeginningOfDocument) \
macro(scrollToEndOfDocument) \
macro(selectAll) \
macro(selectLine) \
macro(selectParagraph) \
macro(selectSentence) \
macro(selectToMark) \
macro(selectWord) \
macro(setMark) \
macro(showGuessPanel) \
macro(startSpeaking) \
macro(stopSpeaking) \
macro(subscript) \
macro(superscript) \
macro(swapWithMark) \
_Pragma("clang diagnostic push") \
_Pragma("clang diagnostic ignored \"-Wundeclared-selector\"") \
macro(takeFindStringFromSelection) \
_Pragma("clang diagnostic pop") \
macro(toggleBaseWritingDirection) \
macro(transpose) \
macro(underline) \
macro(unscript) \
macro(uppercaseWord) \
macro(yank) \
_Pragma("clang diagnostic push") \
_Pragma("clang diagnostic ignored \"-Wundeclared-selector\"") \
macro(yankAndSelect) \
_Pragma("clang diagnostic pop") \

#define WebKitOriginalTopPrintingMarginKey @"WebKitOriginalTopMargin"
#define WebKitOriginalBottomPrintingMarginKey @"WebKitOriginalBottomMargin"

#define KeyboardUIModeDidChangeNotification @"com.apple.KeyboardUIModeDidChange"
#define AppleKeyboardUIMode CFSTR("AppleKeyboardUIMode")

static BOOL s_didSetCacheModel;
static WebCacheModel s_cacheModel = WebCacheModelDocumentViewer;

#if PLATFORM(IOS_FAMILY)
static Class s_pdfRepresentationClass;
static Class s_pdfViewClass;
#endif

#ifndef NDEBUG
static const char webViewIsOpen[] = "At least one WebView is still open.";
#endif

#if PLATFORM(IOS_FAMILY)
@interface WebView(WebViewPrivate)
- (void)_preferencesChanged:(WebPreferences *)preferences;
- (void)_updateScreenScaleFromWindow;
@end
#endif

#if !PLATFORM(IOS_FAMILY)
@interface NSObject (WebValidateWithoutDelegate)
- (BOOL)validateUserInterfaceItemWithoutDelegate:(id <NSValidatedUserInterfaceItem>)item;
@end
#endif

#if PLATFORM(IOS_FAMILY)
@class _WebSafeForwarder;

@interface _WebSafeAsyncForwarder : NSObject {
    __weak _WebSafeForwarder *_forwarder;
}
- (instancetype)initWithForwarder:(_WebSafeForwarder *)forwarder;
@end
#endif

@interface _WebSafeForwarder : NSObject
{
    // Do not not change _target and _defaultTarget to __weak. See <rdar://problem/62624078>.
    __unsafe_unretained id _target;
    __unsafe_unretained id _defaultTarget;
#if PLATFORM(IOS_FAMILY)
    _WebSafeAsyncForwarder *_asyncForwarder;
#endif
}
- (instancetype)initWithTarget:(id)target defaultTarget:(id)defaultTarget;
#if PLATFORM(IOS_FAMILY)
@property (nonatomic, readonly, strong) id asyncForwarder;
- (void)clearTarget;
#endif
@end

#if ENABLE(DRAG_SUPPORT)
static OptionSet<WebCore::DragDestinationAction> coreDragDestinationActionMask(WebDragDestinationAction actionMask)
{
    OptionSet<WebCore::DragDestinationAction> result;
    if (actionMask & WebDragDestinationActionDHTML)
        result.add(WebCore::DragDestinationAction::DHTML);
    if (actionMask & WebDragDestinationActionEdit)
        result.add(WebCore::DragDestinationAction::Edit);
    if (actionMask & WebDragDestinationActionLoad)
        result.add(WebCore::DragDestinationAction::Load);
    return result;
}

#if !USE(APPKIT)
// See <UIKit/UIDragging_Private.h>.
typedef NS_OPTIONS(NSUInteger, _UIDragOperation) {
    _UIDragOperationNone = 0,
    _UIDragOperationCopy = 1,
    _UIDragOperationMove = 16,
};
#endif

OptionSet<WebCore::DragOperation> coreDragOperationMask(CocoaDragOperation operation)
{
    OptionSet<WebCore::DragOperation> result;

#if USE(APPKIT)
    if (operation & NSDragOperationCopy)
        result.add(WebCore::DragOperation::Copy);
    if (operation & NSDragOperationLink)
        result.add(WebCore::DragOperation::Link);
    if (operation & NSDragOperationGeneric)
        result.add(WebCore::DragOperation::Generic);
    if (operation & NSDragOperationPrivate)
        result.add(WebCore::DragOperation::Private);
    if (operation & NSDragOperationMove)
        result.add(WebCore::DragOperation::Move);
    if (operation & NSDragOperationDelete)
        result.add(WebCore::DragOperation::Delete);
#else
    if (operation & _UIDragOperationCopy)
        result.add(WebCore::DragOperation::Copy);
    if (operation & _UIDragOperationMove)
        result.add(WebCore::DragOperation::Move);
#endif // USE(APPKIT)

    return result;
}

#if USE(APPKIT)
static NSDragOperation kit(Optional<WebCore::DragOperation> dragOperation)
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
#else
static _UIDragOperation kit(Optional<WebCore::DragOperation> dragOperation)
{
    if (!dragOperation)
        return _UIDragOperationNone;

    switch (*dragOperation) {
    case WebCore::DragOperation::Copy:
        return _UIDragOperationCopy;
    case WebCore::DragOperation::Link:
        return _UIDragOperationNone;
    case WebCore::DragOperation::Generic:
        return _UIDragOperationMove;
    case WebCore::DragOperation::Private:
        return _UIDragOperationNone;
    case WebCore::DragOperation::Move:
        return _UIDragOperationMove;
    case WebCore::DragOperation::Delete:
        return _UIDragOperationNone;
    }

    ASSERT_NOT_REACHED();
    return _UIDragOperationNone;
}
#endif // USE(APPKIT)

WebDragSourceAction kit(Optional<WebCore::DragSourceAction> action)
{
    if (!action)
        return WebDragSourceActionNone;

    switch (*action) {
    case WebCore::DragSourceAction::DHTML:
        return WebDragSourceActionDHTML;
    case WebCore::DragSourceAction::Image:
        return WebDragSourceActionImage;
    case WebCore::DragSourceAction::Link:
        return WebDragSourceActionLink;
    case WebCore::DragSourceAction::Selection:
        return WebDragSourceActionSelection;
#if ENABLE(ATTACHMENT_ELEMENT)
    case WebCore::DragSourceAction::Attachment:
        break;
#endif
#if ENABLE(INPUT_TYPE_COLOR)
    case WebCore::DragSourceAction::Color:
        break;
#endif
    }

    ASSERT_NOT_REACHED();
    return WebDragSourceActionNone;
}
#endif // ENABLE(DRAG_SUPPORT)

WebCore::FindOptions coreOptions(WebFindOptions options)
{
    WebCore::FindOptions findOptions;
    if (options & WebFindOptionsCaseInsensitive)
        findOptions.add(WebCore::CaseInsensitive);
    if (options & WebFindOptionsAtWordStarts)
        findOptions.add(WebCore::AtWordStarts);
    if (options & WebFindOptionsTreatMedialCapitalAsWordStart)
        findOptions.add(WebCore::TreatMedialCapitalAsWordStart);
    if (options & WebFindOptionsBackwards)
        findOptions.add(WebCore::Backwards);
    if (options & WebFindOptionsWrapAround)
        findOptions.add(WebCore::WrapAround);
    if (options & WebFindOptionsStartInSelection)
        findOptions.add(WebCore::StartInSelection);
    return findOptions;
}

OptionSet<WebCore::LayoutMilestone> coreLayoutMilestones(WebLayoutMilestones milestones)
{
    OptionSet<WebCore::LayoutMilestone> layoutMilestone;
    if (milestones & WebDidFirstLayout)
        layoutMilestone.add(WebCore::DidFirstLayout);
    if (milestones & WebDidFirstVisuallyNonEmptyLayout)
        layoutMilestone.add(WebCore::DidFirstVisuallyNonEmptyLayout);
    if (milestones & WebDidHitRelevantRepaintedObjectsAreaThreshold)
        layoutMilestone.add(WebCore::DidHitRelevantRepaintedObjectsAreaThreshold);
    return layoutMilestone;
}

WebLayoutMilestones kitLayoutMilestones(OptionSet<WebCore::LayoutMilestone> milestones)
{
    return (milestones & WebCore::DidFirstLayout ? WebDidFirstLayout : 0)
        | (milestones & WebCore::DidFirstVisuallyNonEmptyLayout ? WebDidFirstVisuallyNonEmptyLayout : 0)
        | (milestones & WebCore::DidHitRelevantRepaintedObjectsAreaThreshold ? WebDidHitRelevantRepaintedObjectsAreaThreshold : 0);
}

static WebPageVisibilityState kit(WebCore::VisibilityState visibilityState)
{
    switch (visibilityState) {
    case WebCore::VisibilityState::Visible:
        return WebPageVisibilityStateVisible;
    case WebCore::VisibilityState::Hidden:
        return WebPageVisibilityStateHidden;
    }

    ASSERT_NOT_REACHED();
    return WebPageVisibilityStateVisible;
}

static WebCore::StorageBlockingPolicy core(WebStorageBlockingPolicy storageBlockingPolicy)
{
    switch (storageBlockingPolicy) {
    case WebAllowAllStorage:
        return WebCore::StorageBlockingPolicy::AllowAll;
    case WebBlockThirdPartyStorage:
        return WebCore::StorageBlockingPolicy::BlockThirdParty;
    case WebBlockAllStorage:
        return WebCore::StorageBlockingPolicy::BlockAll;
    default:
        // If an invalid value was set (as can be done via NSUserDefaults), fall back to
        // the default value, WebCore::StorageBlockingPolicy::AllowAll.
        return WebCore::StorageBlockingPolicy::AllowAll;
    }
}

namespace WebKit {

class DeferredPageDestructor {
public:
    static void createDeferredPageDestructor(std::unique_ptr<WebCore::Page> page)
    {
        new DeferredPageDestructor(WTFMove(page));
    }

private:
    DeferredPageDestructor(std::unique_ptr<WebCore::Page> page)
        : m_page(WTFMove(page))
    {
        tryDestruction();
    }

    void tryDestruction()
    {
        if (m_page->insideNestedRunLoop()) {
            m_page->whenUnnested([this] { tryDestruction(); });
            return;
        }

        m_page = nullptr;
        delete this;
    }

    std::unique_ptr<WebCore::Page> m_page;
};

} // namespace WebKit

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)

@implementation WebUITextIndicatorData

@synthesize dataInteractionImage = _dataInteractionImage;
@synthesize selectionRectInRootViewCoordinates = _selectionRectInRootViewCoordinates;
@synthesize textBoundingRectInRootViewCoordinates = _textBoundingRectInRootViewCoordinates;
@synthesize textRectsInBoundingRectCoordinates = _textRectsInBoundingRectCoordinates;
@synthesize contentImageWithHighlight = _contentImageWithHighlight;
@synthesize contentImageWithoutSelection = _contentImageWithoutSelection;
@synthesize contentImageWithoutSelectionRectInRootViewCoordinates = _contentImageWithoutSelectionRectInRootViewCoordinates;
@synthesize contentImage = _contentImage;
@synthesize estimatedBackgroundColor = _estimatedBackgroundColor;

- (void)dealloc
{
    [_dataInteractionImage release];
    [_textRectsInBoundingRectCoordinates release];
    [_contentImageWithHighlight release];
    [_contentImageWithoutSelection release];
    [_contentImage release];
    [_estimatedBackgroundColor release];

    [super dealloc];
}

@end

@implementation WebUITextIndicatorData (WebUITextIndicatorInternal)

- (WebUITextIndicatorData *)initWithImage:(CGImageRef)image textIndicatorData:(const WebCore::TextIndicatorData&)indicatorData scale:(CGFloat)scale
{
    if (!(self = [super init]))
        return nil;

    _dataInteractionImage = [PAL::allocUIImageInstance() initWithCGImage:image scale:scale orientation:UIImageOrientationDownMirrored];
    _selectionRectInRootViewCoordinates = indicatorData.selectionRectInRootViewCoordinates;
    _textBoundingRectInRootViewCoordinates = indicatorData.textBoundingRectInRootViewCoordinates;
    _textRectsInBoundingRectCoordinates = createNSArray(indicatorData.textRectsInBoundingRectCoordinates).leakRef();
    _contentImageScaleFactor = indicatorData.contentImageScaleFactor;
    if (indicatorData.contentImageWithHighlight)
        _contentImageWithHighlight = [PAL::allocUIImageInstance() initWithCGImage:indicatorData.contentImageWithHighlight.get()->nativeImage()->platformImage().get() scale:scale orientation:UIImageOrientationDownMirrored];
    if (indicatorData.contentImage)
        _contentImage = [PAL::allocUIImageInstance() initWithCGImage:indicatorData.contentImage.get()->nativeImage()->platformImage().get() scale:scale orientation:UIImageOrientationUp];

    if (indicatorData.contentImageWithoutSelection) {
        auto nativeImage = indicatorData.contentImageWithoutSelection.get()->nativeImage();
        if (nativeImage) {
            _contentImageWithoutSelection = [PAL::allocUIImageInstance() initWithCGImage:nativeImage->platformImage().get() scale:scale orientation:UIImageOrientationUp];
            _contentImageWithoutSelectionRectInRootViewCoordinates = indicatorData.contentImageWithoutSelectionRectInRootViewCoordinates;
        }
    }

    if (indicatorData.options.contains(WebCore::TextIndicatorOption::ComputeEstimatedBackgroundColor))
        _estimatedBackgroundColor = [PAL::allocUIColorInstance() initWithCGColor:cachedCGColor(indicatorData.estimatedBackgroundColor)];

    return self;
}

- (WebUITextIndicatorData *)initWithImage:(CGImageRef)image scale:(CGFloat)scale
{
    if (!(self = [super init]))
        return nil;

    _dataInteractionImage = [PAL::allocUIImageInstance() initWithCGImage:image scale:scale orientation:UIImageOrientationDownMirrored];

    return self;
}

@end
#elif !PLATFORM(MAC)
@implementation WebUITextIndicatorData
@end
#endif

NSString *WebElementDOMNodeKey =            @"WebElementDOMNode";
NSString *WebElementFrameKey =              @"WebElementFrame";
NSString *WebElementImageKey =              @"WebElementImage";
NSString *WebElementImageAltStringKey =     @"WebElementImageAltString";
NSString *WebElementImageRectKey =          @"WebElementImageRect";
NSString *WebElementImageURLKey =           @"WebElementImageURL";
NSString *WebElementIsSelectedKey =         @"WebElementIsSelected";
NSString *WebElementLinkLabelKey =          @"WebElementLinkLabel";
NSString *WebElementLinkTargetFrameKey =    @"WebElementTargetFrame";
NSString *WebElementLinkTitleKey =          @"WebElementLinkTitle";
NSString *WebElementLinkURLKey =            @"WebElementLinkURL";
NSString *WebElementMediaURLKey =           @"WebElementMediaURL";
NSString *WebElementSpellingToolTipKey =    @"WebElementSpellingToolTip";
NSString *WebElementTitleKey =              @"WebElementTitle";
NSString *WebElementLinkIsLiveKey =         @"WebElementLinkIsLive";
NSString *WebElementIsInScrollBarKey =      @"WebElementIsInScrollBar";
NSString *WebElementIsContentEditableKey =  @"WebElementIsContentEditableKey";

NSString *WebViewProgressStartedNotification =          @"WebProgressStartedNotification";
NSString *WebViewProgressEstimateChangedNotification =  @"WebProgressEstimateChangedNotification";
#if !PLATFORM(IOS_FAMILY)
NSString *WebViewProgressFinishedNotification =         @"WebProgressFinishedNotification";
#else
NSString * const WebViewProgressEstimatedProgressKey = @"WebProgressEstimatedProgressKey";
NSString * const WebViewProgressBackgroundColorKey =   @"WebProgressBackgroundColorKey";
#endif

NSString * const WebViewDidBeginEditingNotification =         @"WebViewDidBeginEditingNotification";
NSString * const WebViewDidChangeNotification =               @"WebViewDidChangeNotification";
NSString * const WebViewDidEndEditingNotification =           @"WebViewDidEndEditingNotification";
NSString * const WebViewDidChangeTypingStyleNotification =    @"WebViewDidChangeTypingStyleNotification";
NSString * const WebViewDidChangeSelectionNotification =      @"WebViewDidChangeSelectionNotification";

enum { WebViewVersion = 4 };

#define timedLayoutSize 4096

static RetainPtr<NSMutableSet>& schemesWithRepresentationsSet()
{
    static NeverDestroyed<RetainPtr<NSMutableSet>> schemesWithRepresentationsSet;
    return schemesWithRepresentationsSet;
}

#if !PLATFORM(IOS_FAMILY)
NSString *_WebCanGoBackKey =            @"canGoBack";
NSString *_WebCanGoForwardKey =         @"canGoForward";
NSString *_WebEstimatedProgressKey =    @"estimatedProgress";
NSString *_WebIsLoadingKey =            @"isLoading";
NSString *_WebMainFrameIconKey =        @"mainFrameIcon";
NSString *_WebMainFrameTitleKey =       @"mainFrameTitle";
NSString *_WebMainFrameURLKey =         @"mainFrameURL";
NSString *_WebMainFrameDocumentKey =    @"mainFrameDocument";
#endif

#if PLATFORM(IOS_FAMILY)
NSString *WebQuickLookFileNameKey = @"WebQuickLookFileNameKey";
NSString *WebQuickLookUTIKey      = @"WebQuickLookUTIKey";
#endif

NSString *_WebViewDidStartAcceleratedCompositingNotification = @"_WebViewDidStartAcceleratedCompositing";
NSString * const WebViewWillCloseNotification = @"WebViewWillCloseNotification";

#if ENABLE(REMOTE_INSPECTOR)
// FIXME: Legacy, remove this, switch to something from JavaScriptCore Inspector::RemoteInspectorServer.
NSString *_WebViewRemoteInspectorHasSessionChangedNotification = @"_WebViewRemoteInspectorHasSessionChangedNotification";
#endif

@interface WebProgressItem : NSObject
{
@public
    long long bytesReceived;
    long long estimatedLength;
}
@end

@implementation WebProgressItem
@end

static BOOL continuousSpellCheckingEnabled;
static BOOL iconLoadingEnabled = YES;
#if !PLATFORM(IOS_FAMILY)
static BOOL grammarCheckingEnabled;
static BOOL automaticQuoteSubstitutionEnabled;
static BOOL automaticLinkDetectionEnabled;
static BOOL automaticDashSubstitutionEnabled;
static BOOL automaticTextReplacementEnabled;
static BOOL automaticSpellingCorrectionEnabled;
#endif

#if HAVE(TOUCH_BAR)

enum class WebListType {
    None = 0,
    Ordered,
    Unordered
};

@interface WebTextListTouchBarViewController : NSViewController {
@private
    WebListType _currentListType;
    WebView *_webView;
}

@property (nonatomic) WebListType currentListType;

- (instancetype)initWithWebView:(WebView *)webView;

@end

@implementation WebTextListTouchBarViewController

@synthesize currentListType = _currentListType;

static const CGFloat listControlSegmentWidth = 67.0;

static const NSUInteger noListSegment = 0;
static const NSUInteger unorderedListSegment = 1;
static const NSUInteger orderedListSegment = 2;

- (instancetype)initWithWebView:(WebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;

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

- (void)_selectList:(id)sender
{
    NSView *documentView = [[[_webView _selectedOrMainFrame] frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]])
        return;

    WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
    NSSegmentedControl *insertListControl = (NSSegmentedControl *)self.view;
    switch (insertListControl.selectedSegment) {
    case noListSegment:
        // There is no "remove list" edit command, but _insertOrderedList and _insertUnorderedList both
        // behave as toggles, so we can invoke the appropriate method depending on our _currentListType
        // to remove an existing list. We don't have to do anything if _currentListType is WebListType::None.
        if (_currentListType == WebListType::Ordered)
            [webHTMLView _insertOrderedList];
        else if (_currentListType == WebListType::Unordered)
            [webHTMLView _insertUnorderedList];
        break;
    case unorderedListSegment:
        [webHTMLView _insertUnorderedList];
        break;
    case orderedListSegment:
        [webHTMLView _insertOrderedList];
        break;
    }

    [_webView _dismissTextTouchBarPopoverItemWithIdentifier:NSTouchBarItemIdentifierTextList];
}

- (void)setCurrentListType:(WebListType)listType
{
    NSSegmentedControl *insertListControl = (NSSegmentedControl *)self.view;
    switch (listType) {
    case WebListType::None:
        [insertListControl setSelected:YES forSegment:noListSegment];
        break;
    case WebListType::Ordered:
        [insertListControl setSelected:YES forSegment:orderedListSegment];
        break;
    case WebListType::Unordered:
        [insertListControl setSelected:YES forSegment:unorderedListSegment];
        break;
    }

    _currentListType = listType;
}

@end

@interface WebTextTouchBarItemController : NSTextTouchBarItemController {
@private
    BOOL _textIsBold;
    BOOL _textIsItalic;
    BOOL _textIsUnderlined;
    NSTextAlignment _currentTextAlignment;
    RetainPtr<NSColor> _textColor;
    RetainPtr<WebTextListTouchBarViewController> _textListTouchBarViewController;
    WebView *_webView;
}

@property (nonatomic) BOOL textIsBold;
@property (nonatomic) BOOL textIsItalic;
@property (nonatomic) BOOL textIsUnderlined;
@property (nonatomic) NSTextAlignment currentTextAlignment;
@property (nonatomic, retain, readwrite) NSColor *textColor;

- (instancetype)initWithWebView:(WebView *)webView;
@end

@implementation WebTextTouchBarItemController

@synthesize textIsBold = _textIsBold;
@synthesize textIsItalic = _textIsItalic;
@synthesize textIsUnderlined = _textIsUnderlined;
@synthesize currentTextAlignment = _currentTextAlignment;

- (instancetype)initWithWebView:(WebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;

    return self;
}

- (NSTouchBarItem *)itemForIdentifier:(NSString *)identifier
{
    NSTouchBarItem *item = [super itemForIdentifier:identifier];
    BOOL isTextFormatItem = [identifier isEqualToString:NSTouchBarItemIdentifierTextFormat];

    if (isTextFormatItem || [identifier isEqualToString:NSTouchBarItemIdentifierTextStyle])
        self.textStyle.action = @selector(_webChangeTextStyle:);

    if (isTextFormatItem || [identifier isEqualToString:NSTouchBarItemIdentifierTextAlignment])
        self.textAlignments.action = @selector(_webChangeTextAlignment:);

    NSColorPickerTouchBarItem *colorPickerItem = nil;
    if ([identifier isEqualToString:NSTouchBarItemIdentifierTextColorPicker] && [item isKindOfClass:[NSColorPickerTouchBarItem class]])
        colorPickerItem = (NSColorPickerTouchBarItem *)item;
    if (isTextFormatItem)
        colorPickerItem = self.colorPickerItem;
    if (colorPickerItem) {
        colorPickerItem.target = self;
        colorPickerItem.action = @selector(_webChangeColor:);
        colorPickerItem.showsAlpha = NO;
    }

    return item;
}

- (WebTextListTouchBarViewController *)webTextListTouchBarViewController
{
    return (WebTextListTouchBarViewController *)self.textListViewController;
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

- (void)_webChangeTextStyle:(id)sender
{
    if ([self.textStyle isSelectedForSegment:0] != _textIsBold) {
        _textIsBold = !_textIsBold;
        [_webView _executeCoreCommandByName:@"ToggleBold" value:@""];
    }

    if ([self.textStyle isSelectedForSegment:1] != _textIsItalic) {
        _textIsItalic = !_textIsItalic;
        [_webView _executeCoreCommandByName:@"ToggleItalic" value:@""];
    }

    if ([self.textStyle isSelectedForSegment:2] != _textIsUnderlined) {
        _textIsUnderlined = !_textIsUnderlined;
        [_webView _executeCoreCommandByName:@"ToggleUnderline" value:@""];
    }
}

- (void)setCurrentTextAlignment:(NSTextAlignment)alignment
{
    _currentTextAlignment = alignment;
    [self.textAlignments selectSegmentWithTag:_currentTextAlignment];
}

- (void)_webChangeTextAlignment:(id)sender
{
    NSTextAlignment alignment = (NSTextAlignment)[self.textAlignments.cell tagForSegment:self.textAlignments.selectedSegment];
    switch (alignment) {
    case NSTextAlignmentLeft:
        _currentTextAlignment = NSTextAlignmentLeft;
        [_webView alignLeft:sender];
        break;
    case NSTextAlignmentRight:
        _currentTextAlignment = NSTextAlignmentRight;
        [_webView alignRight:sender];
        break;
    case NSTextAlignmentCenter:
        _currentTextAlignment = NSTextAlignmentCenter;
        [_webView alignCenter:sender];
        break;
    case NSTextAlignmentJustified:
        _currentTextAlignment = NSTextAlignmentJustified;
        [_webView alignJustified:sender];
        break;
    default:
        break;
    }

    [_webView _dismissTextTouchBarPopoverItemWithIdentifier:NSTouchBarItemIdentifierTextAlignment];
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

- (void)_webChangeColor:(id)sender
{
    _textColor = self.colorPickerItem.color;
    [_webView _executeCoreCommandByName:@"ForeColor" value: WebCore::serializationForHTML(WebCore::colorFromNSColor(_textColor.get()))];
}

- (NSViewController *)textListViewController
{
    if (!_textListTouchBarViewController)
        _textListTouchBarViewController = adoptNS([[WebTextListTouchBarViewController alloc] initWithWebView:_webView]);
    return _textListTouchBarViewController.get();
}

@end

#endif // HAVE(TOUCH_BAR)

@interface WebView ()
#if PLATFORM(IOS_FAMILY)
- (void)_wakWindowScreenScaleChanged:(NSNotification *)notification;
- (void)_wakWindowVisibilityChanged:(NSNotification *)notification;
#else
- (float)_deviceScaleFactor;
#endif
@end

@implementation WebView (AllWebViews)

static CFSetCallBacks NonRetainingSetCallbacks = {
    0,
    NULL,
    NULL,
    CFCopyDescription,
    CFEqual,
    CFHash
};

static RetainPtr<CFMutableSetRef>& allWebViewsSet()
{
    static NeverDestroyed<RetainPtr<CFMutableSetRef>> allWebViewsSet;
    return allWebViewsSet;
}

+ (void)_makeAllWebViewsPerformSelector:(SEL)selector
{
    if (!allWebViewsSet())
        return;

    [(__bridge NSMutableSet *)allWebViewsSet().get() makeObjectsPerformSelector:selector];
}

- (void)_removeFromAllWebViewsSet
{
    if (allWebViewsSet())
        CFSetRemoveValue(allWebViewsSet().get(), (__bridge CFTypeRef)self);
}

- (void)_addToAllWebViewsSet
{
    if (!allWebViewsSet())
        allWebViewsSet() = adoptCF(CFSetCreateMutable(NULL, 0, &NonRetainingSetCallbacks));

    CFSetSetValue(allWebViewsSet().get(), (__bridge CFTypeRef)self);
}

@end

@implementation WebView (WebPrivate)

+ (NSString *)_standardUserAgentWithApplicationName:(NSString *)applicationName
{
    return WebCore::standardUserAgentWithApplicationName(applicationName);
}

#if PLATFORM(IOS_FAMILY)
- (void)_setBrowserUserAgentProductVersion:(NSString *)productVersion buildVersion:(NSString *)buildVersion bundleVersion:(NSString *)bundleVersion
{
    // The web-visible build and bundle versions are frozen to remove a fingerprinting surface
    UNUSED_PARAM(buildVersion);
    [self setApplicationNameForUserAgent:[NSString stringWithFormat:@"Version/%@ Mobile/15E148 Safari/%@", productVersion, bundleVersion]];
}

- (void)_setUIWebViewUserAgentWithBuildVersion:(NSString *)buildVersion
{
    UNUSED_PARAM(buildVersion);
    [self setApplicationNameForUserAgent:@"Mobile/15E148"];
}
#endif // PLATFORM(IOS_FAMILY)

+ (void)_reportException:(JSValueRef)exception inContext:(JSContextRef)context
{
    if (!exception || !context)
        return;

    JSC::JSGlobalObject* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject);

    // Make sure the context has a DOMWindow global object, otherwise this context didn't originate from a WebView.
    if (!WebCore::toJSDOMWindow(globalObject->vm(), globalObject))
        return;

    WebCore::reportException(globalObject, toJS(globalObject, exception));
}

- (void)_dispatchPendingLoadRequests
{
    webResourceLoadScheduler().servePendingRequests();
}

#if !PLATFORM(IOS_FAMILY)

- (void)_registerDraggedTypes
{
    NSArray *editableTypes = [WebHTMLView _insertablePasteboardTypes];
    NSArray *URLTypes = [NSPasteboard _web_dragTypesForURL];
    auto types = adoptNS([[NSMutableSet alloc] initWithArray:editableTypes]);
    [types addObjectsFromArray:URLTypes];
    [types addObject:[WebHTMLView _dummyPasteboardType]];
    [self registerForDraggedTypes:[types allObjects]];
}

static bool needsOutlookQuirksScript()
{
    static bool isOutlookNeedingQuirksScript = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_HTML5_PARSER)
        && WebCore::MacApplication::isMicrosoftOutlook();
    return isOutlookNeedingQuirksScript;
}

static RetainPtr<NSString> createOutlookQuirksUserScriptContents()
{
    NSString *scriptPath = [[NSBundle bundleForClass:[WebView class]] pathForResource:@"OutlookQuirksUserScript" ofType:@"js"];
    NSStringEncoding encoding;
    return adoptNS([[NSString alloc] initWithContentsOfFile:scriptPath usedEncoding:&encoding error:0]);
}

-(void)_injectOutlookQuirksScript
{
    static NeverDestroyed<RetainPtr<NSString>> outlookQuirksScriptContents = createOutlookQuirksUserScriptContents();
    _private->group->userContentController().addUserScript(*core([WebScriptWorld world]), makeUnique<WebCore::UserScript>(outlookQuirksScriptContents.get().get(), URL(), Vector<String>(), Vector<String>(), WebCore::UserScriptInjectionTime::DocumentEnd, WebCore::UserContentInjectedFrames::InjectInAllFrames, WebCore::WaitForNotificationBeforeInjecting::No));

}
#endif

#if PLATFORM(IOS)
static bool needsLaBanquePostaleQuirks()
{
    static bool needsQuirks = WebCore::IOSApplication::isLaBanquePostale() && dyld_get_program_sdk_version() < DYLD_IOS_VERSION_14_0;
    return needsQuirks;
}

static RetainPtr<NSString> createLaBanquePostaleQuirksScript()
{
    NSURL *scriptURL = [[NSBundle bundleForClass:WebView.class] URLForResource:@"LaBanquePostaleQuirks" withExtension:@"js"];
    NSStringEncoding encoding;
    return adoptNS([[NSString alloc] initWithContentsOfURL:scriptURL usedEncoding:&encoding error:nullptr]);
}

- (void)_injectLaBanquePostaleQuirks
{
    ASSERT(needsLaBanquePostaleQuirks());
    static NeverDestroyed<RetainPtr<NSString>> quirksScript = createLaBanquePostaleQuirksScript();

    using namespace WebCore;
    auto userScript = makeUnique<UserScript>(quirksScript.get().get(), URL(), Vector<String>(), Vector<String>(), UserScriptInjectionTime::DocumentEnd, UserContentInjectedFrames::InjectInAllFrames, WaitForNotificationBeforeInjecting::No);
    _private->group->userContentController().addUserScript(*core(WebScriptWorld.world), WTFMove(userScript));
}
#endif

#if PLATFORM(IOS_FAMILY)
static bool isInternalInstall()
{
    static bool isInternal = MGGetBoolAnswer(kMGQAppleInternalInstallCapability);
    return isInternal;
}

static bool didOneTimeInitialization = false;
#endif

#if ENABLE(GAMEPAD)
static void WebKitInitializeGamepadProviderIfNecessary()
{
    static bool initialized = false;
    if (initialized)
        return;

#if PLATFORM(MAC)
    WebCore::GamepadProvider::singleton().setSharedProvider(WebCore::HIDGamepadProvider::singleton());
#else
    WebCore::GamepadProvider::singleton().setSharedProvider(WebCore::GameControllerGamepadProvider::singleton());
#endif

    initialized = true;
}
#endif

- (void)_commonInitializationWithFrameName:(NSString *)frameName groupName:(NSString *)groupName
{
    WebCoreThreadViolationCheckRoundTwo();

#ifndef NDEBUG
    WTF::RefCountedLeakCounter::suppressMessages(webViewIsOpen);
#endif

    WebPreferences *standardPreferences = [WebPreferences standardPreferences];
    [standardPreferences willAddToWebView];

    _private->preferences = standardPreferences;
    _private->mainFrameDocumentReady = NO;
    _private->drawsBackground = YES;
#if !PLATFORM(IOS_FAMILY)
    _private->backgroundColor = [NSColor colorWithDeviceWhite:1 alpha:1];
#else
    _private->backgroundColor = WebCore::cachedCGColor(WebCore::Color::white);
#endif

#if PLATFORM(MAC)
    _private->windowVisibilityObserver = adoptNS([[WebWindowVisibilityObserver alloc] initWithView:self]);
#endif

    NSRect f = [self frame];
    auto frameView = adoptNS([[WebFrameView alloc] initWithFrame: NSMakeRect(0,0,f.size.width,f.size.height)]);
    [frameView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview:frameView.get()];

#if PLATFORM(MAC)
    if (Class gestureClass = NSClassFromString(@"NSImmediateActionGestureRecognizer")) {
        RetainPtr<NSImmediateActionGestureRecognizer> recognizer = adoptNS([(NSImmediateActionGestureRecognizer *)[gestureClass alloc] init]);
        _private->immediateActionController = adoptNS([[WebImmediateActionController alloc] initWithWebView:self recognizer:recognizer.get()]);
        [recognizer setDelegate:_private->immediateActionController.get()];
        [recognizer setDelaysPrimaryMouseButtonEvents:NO];
    }
#endif

    [self updateTouchBar];

#if !PLATFORM(IOS_FAMILY)
    static bool didOneTimeInitialization = false;
#endif
    if (!didOneTimeInitialization) {
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
        WebKitInitializeLogChannelsIfNecessary();
        WebCore::initializeLogChannelsIfNecessary();
#endif

        // Initialize our platform strategies first before invoking the rest
        // of the initialization code which may depend on the strategies.
        WebPlatformStrategies::initializeIfNecessary();

        initializeDOMWrapperHooks();

#if PLATFORM(IOS_FAMILY)
        // Set the WebSQLiteDatabaseTrackerClient.
        WebCore::SQLiteDatabaseTracker::setClient(&WebCore::WebSQLiteDatabaseTrackerClient::sharedWebSQLiteDatabaseTrackerClient());

        if ([standardPreferences databasesEnabled])
#endif
        [WebDatabaseManager sharedWebDatabaseManager];

#if PLATFORM(IOS_FAMILY)
        if ([standardPreferences storageTrackerEnabled])
#endif
        WebKitInitializeStorageIfNecessary();

#if ENABLE(GAMEPAD)
        WebKitInitializeGamepadProviderIfNecessary();
#endif
#if PLATFORM(MAC)
        WebCore::SwitchingGPUClient::setSingleton(WebKit::WebSwitchingGPUClient::singleton());
#endif

#if PLATFORM(IOS_FAMILY)
        if (WebCore::IOSApplication::isMobileSafari())
            WebCore::DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(true);
#endif

#if ENABLE(VIDEO)
        WebCore::HTMLMediaElement::setMediaCacheDirectory(FileSystem::pathByAppendingComponent(NSTemporaryDirectory(), "MediaCache/"_s));
#endif
        didOneTimeInitialization = true;
    }

    _private->group = WebViewGroup::getOrCreate(groupName, [_private->preferences _localStorageDatabasePath]);
    _private->group->addWebView(self);

    auto storageProvider = PageStorageSessionProvider::create();
    WebCore::PageConfiguration pageConfiguration(
        [[self preferences] privateBrowsingEnabled] ? PAL::SessionID::legacyPrivateSessionID() : PAL::SessionID::defaultSessionID(),
        makeUniqueRef<WebEditorClient>(self),
        WebCore::SocketProvider::create(),
        WebCore::LibWebRTCProvider::create(),
        WebCore::CacheStorageProvider::create(),
        _private->group->userContentController(),
        BackForwardList::create(self),
        WebCore::CookieJar::create(storageProvider.copyRef()),
        makeUniqueRef<WebProgressTrackerClient>(self),
        makeUniqueRef<WebFrameLoaderClient>(),
        makeUniqueRef<WebCore::DummySpeechRecognitionProvider>(),
        makeUniqueRef<WebCore::MediaRecorderProvider>()
    );
#if !PLATFORM(IOS_FAMILY)
    pageConfiguration.chromeClient = new WebChromeClient(self);
    pageConfiguration.contextMenuClient = new WebContextMenuClient(self);
    // FIXME: We should enable this on iOS as well.
    pageConfiguration.validationMessageClient = makeUnique<WebValidationMessageClient>(self);
    pageConfiguration.inspectorClient = new WebInspectorClient(self);
#else
    pageConfiguration.chromeClient = new WebChromeClientIOS(self);
    pageConfiguration.inspectorClient = new WebInspectorClient(self);
#endif

#if ENABLE(DRAG_SUPPORT)
    pageConfiguration.dragClient = makeUnique<WebDragClient>(self);
#endif

#if ENABLE(APPLE_PAY)
    pageConfiguration.paymentCoordinatorClient = new WebPaymentCoordinatorClient();
#endif

    pageConfiguration.alternativeTextClient = makeUnique<WebAlternativeTextClient>(self);
    pageConfiguration.applicationCacheStorage = &webApplicationCacheStorage();
    pageConfiguration.databaseProvider = &WebDatabaseProvider::singleton();
    pageConfiguration.pluginInfoProvider = &WebPluginInfoProvider::singleton();
    pageConfiguration.storageNamespaceProvider = &_private->group->storageNamespaceProvider();
    pageConfiguration.visitedLinkStore = &_private->group->visitedLinkStore();
    _private->page = new WebCore::Page(WTFMove(pageConfiguration));
    storageProvider->setPage(*_private->page);

    _private->page->setGroupName(groupName);

#if ENABLE(GEOLOCATION)
    WebCore::provideGeolocationTo(_private->page, *new WebGeolocationClient(self));
#endif
#if ENABLE(NOTIFICATIONS)
    WebCore::provideNotification(_private->page, new WebNotificationClient(self));
#endif
#if ENABLE(DEVICE_ORIENTATION) && !PLATFORM(IOS_FAMILY)
    WebCore::provideDeviceOrientationTo(*_private->page, *new WebDeviceOrientationClient(self));
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    WebCore::provideMediaKeySystemTo(*_private->page, *new WebMediaKeySystemClient());
#endif

#if ENABLE(REMOTE_INSPECTOR)
    _private->page->setRemoteInspectionAllowed(true);
#endif

    _private->page->setCanStartMedia([self window]);
    _private->page->settings().setLocalStorageDatabasePath([[self preferences] _localStorageDatabasePath]);

#if !PLATFORM(IOS_FAMILY)
    if (needsOutlookQuirksScript()) {
        _private->page->settings().setShouldInjectUserScriptsInInitialEmptyDocument(true);
        [self _injectOutlookQuirksScript];
    }
#endif

#if PLATFORM(IOS)
    if (needsLaBanquePostaleQuirks())
        [self _injectLaBanquePostaleQuirks];
#endif

#if PLATFORM(IOS_FAMILY)
    // Preserve the behavior we had before <rdar://problem/7580867>
    // by enforcing a 5MB limit for session storage.
    _private->page->settings().setSessionStorageQuota(5 * 1024 * 1024);

    [self _updateScreenScaleFromWindow];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_wakWindowScreenScaleChanged:) name:WAKWindowScreenScaleDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_wakWindowVisibilityChanged:) name:WAKWindowVisibilityDidChangeNotification object:nil];
    _private->_fixedPositionContent = adoptNS([[WebFixedPositionContent alloc] initWithWebView:self]);
#if ENABLE(ORIENTATION_EVENTS)
    _private->deviceOrientation = [[self _UIKitDelegateForwarder] deviceOrientation];
#endif
#endif

    if ([[NSUserDefaults standardUserDefaults] objectForKey:WebSmartInsertDeleteEnabled])
        [self setSmartInsertDeleteEnabled:[[NSUserDefaults standardUserDefaults] boolForKey:WebSmartInsertDeleteEnabled]];

    [WebFrame _createMainFrameWithPage:_private->page frameName:frameName frameView:frameView.get()];

#if PLATFORM(IOS_FAMILY)
    NSRunLoop *runLoop = WebThreadNSRunLoop();
#else
    NSRunLoop *runLoop = [NSRunLoop mainRunLoop];
#endif

    if (WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_LOADING_DURING_COMMON_RUNLOOP_MODES))
        [self scheduleInRunLoop:runLoop forMode:(NSString *)kCFRunLoopCommonModes];
    else
        [self scheduleInRunLoop:runLoop forMode:NSDefaultRunLoopMode];

    [self _addToAllWebViewsSet];

    // If there's already a next key view (e.g., from a nib), wire it up to our
    // contained frame view. In any case, wire our next key view up to the our
    // contained frame view. This works together with our becomeFirstResponder
    // and setNextKeyView overrides.
    NSView *nextKeyView = [self nextKeyView];
    if (nextKeyView && nextKeyView != frameView)
        [frameView setNextKeyView:nextKeyView];
    [super setNextKeyView:frameView.get()];

    if ([[self class] shouldIncludeInWebKitStatistics])
        ++WebViewCount;

#if !PLATFORM(IOS_FAMILY)
    [self _registerDraggedTypes];
#endif

    [self _setIsVisible:[self _isViewVisible]];

    WebPreferences *prefs = [self preferences];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                 name:WebPreferencesChangedInternalNotification object:prefs];

#if !PLATFORM(IOS_FAMILY)
    [self _preferencesChanged:[self preferences]];
    [[self preferences] _postPreferencesChangedAPINotification];
#else
    // do this on the current thread on iOS, since the web thread could be blocked on the main thread,
    // and prefs need to be changed synchronously <rdar://problem/5841558>
    [self _preferencesChanged:prefs];
    _private->page->settings().setFontFallbackPrefersPictographs(true);
#endif

    WebInstallMemoryPressureHandler();

#if PLATFORM(MAC)
    if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_LOCAL_RESOURCE_SECURITY_RESTRICTION)) {
        // Originally, we allowed all local loads.
        WebCore::SecurityPolicy::setLocalLoadPolicy(WebCore::SecurityPolicy::AllowLocalLoadsForAll);
    } else if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_MORE_STRICT_LOCAL_RESOURCE_SECURITY_RESTRICTION)) {
        // Later, we allowed local loads for local URLs and documents loaded
        // with substitute data.
        WebCore::SecurityPolicy::setLocalLoadPolicy(WebCore::SecurityPolicy::AllowLocalLoadsForLocalAndSubstituteData);
    }

    if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_CONTENT_SNIFFING_FOR_FILE_URLS))
        WebCore::ResourceHandle::forceContentSniffing();

    _private->page->setDeviceScaleFactor([self _deviceScaleFactor]);

#if HAVE(OS_DARK_MODE_SUPPORT)
    _private->page->effectiveAppearanceDidChange(self._effectiveAppearanceIsDark, self._effectiveUserInterfaceLevelIsElevated);
#endif

    [WebViewVisualIdentificationOverlay installForWebViewIfNeeded:self kind:@"WebView" deprecated:YES];

    WTF::listenForLanguageChangeNotifications();
#endif // PLATFORM(MAC)
}

- (id)_initWithFrame:(NSRect)f frameName:(NSString *)frameName groupName:(NSString *)groupName
{
    self = [super initWithFrame:f];
    if (!self)
        return nil;

#ifdef ENABLE_WEBKIT_UNSET_DYLD_FRAMEWORK_PATH
    // DYLD_FRAMEWORK_PATH is used so Safari will load the development version of WebKit, which
    // may not work with other WebKit applications.  Unsetting DYLD_FRAMEWORK_PATH removes the
    // need for Safari to unset it to prevent it from being passed to applications it launches.
    // Unsetting it when a WebView is first created is as good a place as any.
    // See <http://bugs.webkit.org/show_bug.cgi?id=4286> for more details.
    if (getenv("WEBKIT_UNSET_DYLD_FRAMEWORK_PATH")) {
        unsetenv("DYLD_FRAMEWORK_PATH");
        unsetenv("WEBKIT_UNSET_DYLD_FRAMEWORK_PATH");
    }
#endif

    _private = [[WebViewPrivate alloc] init];
    [self _commonInitializationWithFrameName:frameName groupName:groupName];
    [self setMaintainsBackForwardList: YES];
    return self;
}

- (void)_updateRendering
{
#if PLATFORM(IOS_FAMILY)
    // Ensure fixed positions layers are where they should be.
    [self _synchronizeCustomFixedPositionLayoutRect];
#endif

    if (_private->page) {
        _private->page->updateRendering();
        _private->page->finalizeRenderingUpdate({ });
    }
}

+ (NSArray *)_supportedMIMETypes
{
    // Load the plug-in DB allowing plug-ins to install types.
    [WebPluginDatabase sharedDatabase];
    return [[WebFrameView _viewTypesAllowImageTypeOmission:NO] allKeys];
}

#if !PLATFORM(IOS_FAMILY)
+ (NSArray *)_supportedFileExtensions
{
    auto extensions = adoptNS([[NSMutableSet alloc] init]);
    NSArray *MIMETypes = [self _supportedMIMETypes];
    NSEnumerator *enumerator = [MIMETypes objectEnumerator];
    NSString *MIMEType;
    while ((MIMEType = [enumerator nextObject]) != nil) {
        NSArray *extensionsForType = [[NSURLFileTypeMappings sharedMappings] extensionsForMIMEType:MIMEType];
        if (extensionsForType) {
            [extensions addObjectsFromArray:extensionsForType];
        }
    }
    return [extensions allObjects];
}
#endif

#if PLATFORM(IOS_FAMILY)
+ (void)enableWebThread
{
    static BOOL isWebThreadEnabled = NO;
    if (!isWebThreadEnabled) {
        WebCoreObjCDeallocOnWebThread([DOMObject class]);
        WebCoreObjCDeallocOnWebThread([WebBasePluginPackage class]);
        WebCoreObjCDeallocOnWebThread([WebDataSource class]);
        WebCoreObjCDeallocOnWebThread([WebFrame class]);
        WebCoreObjCDeallocOnWebThread([WebHTMLView class]);
        WebCoreObjCDeallocOnWebThread([WebHistoryItem class]);
        WebCoreObjCDeallocOnWebThread([WebPlainWhiteView class]);
        WebCoreObjCDeallocOnWebThread([WebPolicyDecisionListener class]);
        WebCoreObjCDeallocOnWebThread([WebView class]);
        WebCoreObjCDeallocOnWebThread([WebVisiblePosition class]);
        WebThreadEnable();
        isWebThreadEnabled = YES;
    }
}

- (id)initSimpleHTMLDocumentWithStyle:(NSString *)style frame:(CGRect)frame preferences:(WebPreferences *)preferences groupName:(NSString *)groupName
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    _private = [[WebViewPrivate alloc] init];

#ifndef NDEBUG
    WTF::RefCountedLeakCounter::suppressMessages(webViewIsOpen);
#endif

    if (!preferences)
        preferences = [WebPreferences standardPreferences];
    [preferences willAddToWebView];

    _private->preferences = preferences;
    _private->mainFrameDocumentReady = NO;
    _private->drawsBackground = YES;
    _private->backgroundColor = WebCore::cachedCGColor(WebCore::Color::white);

    auto frameView = adoptNS([[WebFrameView alloc] initWithFrame: CGRectMake(0,0,frame.size.width,frame.size.height)]);
    [frameView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview:frameView.get()];

    _private->group = WebViewGroup::getOrCreate(groupName, [_private->preferences _localStorageDatabasePath]);
    _private->group->addWebView(self);

    auto storageProvider = PageStorageSessionProvider::create();
    WebCore::PageConfiguration pageConfiguration(
        [[self preferences] privateBrowsingEnabled] ? PAL::SessionID::legacyPrivateSessionID() : PAL::SessionID::defaultSessionID(),
        makeUniqueRef<WebEditorClient>(self),
        WebCore::SocketProvider::create(),
        WebCore::LibWebRTCProvider::create(),
        WebCore::CacheStorageProvider::create(),
        _private->group->userContentController(),
        BackForwardList::create(self),
        WebCore::CookieJar::create(storageProvider.copyRef()),
        makeUniqueRef<WebProgressTrackerClient>(self),
        makeUniqueRef<WebFrameLoaderClient>(),
        makeUniqueRef<WebCore::DummySpeechRecognitionProvider>(),
        makeUniqueRef<WebCore::MediaRecorderProvider>()
    );
    pageConfiguration.chromeClient = new WebChromeClientIOS(self);
#if ENABLE(DRAG_SUPPORT)
    pageConfiguration.dragClient = makeUnique<WebDragClient>(self);
#endif

#if ENABLE(APPLE_PAY)
    pageConfiguration.paymentCoordinatorClient = new WebPaymentCoordinatorClient();
#endif

    pageConfiguration.inspectorClient = new WebInspectorClient(self);
    pageConfiguration.applicationCacheStorage = &webApplicationCacheStorage();
    pageConfiguration.databaseProvider = &WebDatabaseProvider::singleton();
    pageConfiguration.storageNamespaceProvider = &_private->group->storageNamespaceProvider();
    pageConfiguration.visitedLinkStore = &_private->group->visitedLinkStore();
    pageConfiguration.pluginInfoProvider = &WebPluginInfoProvider::singleton();

    _private->page = new WebCore::Page(WTFMove(pageConfiguration));
    storageProvider->setPage(*_private->page);

    [self setSmartInsertDeleteEnabled:YES];

    // FIXME: <rdar://problem/6851451> Should respect preferences in fast path WebView initialization
    // We are ignoring the preferences object on fast path and just using Settings defaults (everything fancy off).
    // This matches how UIKit sets up the preferences. We need to set  default values for fonts though, <rdar://problem/6850611>.
    // This should be revisited later. There is some risk involved, _preferencesChanged used to show up badly in Shark.
    _private->page->settings().setMinimumLogicalFontSize(9);
    _private->page->settings().setDefaultFontSize([_private->preferences defaultFontSize]);
    _private->page->settings().setDefaultFixedFontSize(13);
    _private->page->settings().setAcceleratedDrawingEnabled([preferences acceleratedDrawingEnabled]);
    _private->page->settings().setDisplayListDrawingEnabled([preferences displayListDrawingEnabled]);

    _private->page->settings().setFontFallbackPrefersPictographs(true);
    _private->page->settings().setPictographFontFamily("AppleColorEmoji");

    _private->page->settings().setScriptMarkupEnabled(false);
    _private->page->settings().setScriptEnabled(true);

    // FIXME: this is a workaround for <rdar://problem/11518688> REGRESSION: Quoted text font changes when replying to certain email
    _private->page->settings().setStandardFontFamily([_private->preferences standardFontFamily]);

    // FIXME: this is a workaround for <rdar://problem/11820090> Quoted text changes in size when replying to certain email
    _private->page->settings().setMinimumFontSize([_private->preferences minimumFontSize]);

    // This is a workaround for <rdar://problem/21309911>.
    _private->page->settings().setHttpEquivEnabled([_private->preferences httpEquivEnabled]);

    _private->page->setGroupName(groupName);

#if ENABLE(REMOTE_INSPECTOR)
    _private->page->setRemoteInspectionAllowed(isInternalInstall());
#endif

    [self _updateScreenScaleFromWindow];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_wakWindowScreenScaleChanged:) name:WAKWindowScreenScaleDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_wakWindowVisibilityChanged:) name:WAKWindowVisibilityDidChangeNotification object:nil];

    [WebFrame _createMainFrameWithSimpleHTMLDocumentWithPage:_private->page frameView:frameView.get() style:style];

    [self _addToAllWebViewsSet];

    ++WebViewCount;

    WebCore::SecurityPolicy::setLocalLoadPolicy(WebCore::SecurityPolicy::AllowLocalLoadsForLocalAndSubstituteData);

    WebCore::RuntimeEnabledFeatures::sharedFeatures().setAttachmentElementEnabled(self.preferences.attachmentElementEnabled);

    return self;
}

+ (void)_releaseMemoryNow
{
    WebThreadRun(^{
        WebCore::releaseMemory(Critical::Yes, Synchronous::Yes);
    });
}

- (void)_replaceCurrentHistoryItem:(WebHistoryItem *)item
{
    auto* frame = [self _mainCoreFrame];
    if (frame)
        frame->loader().history().replaceCurrentItem(core(item));
}

+ (void)willEnterBackgroundWithCompletionHandler:(void(^)(void))handler
{
    WebThreadRun(^{
        [WebView _releaseMemoryNow];
        RunLoop::main().dispatch([handler = makeBlockPtr(handler)] {
            handler();
        });
    });
}

+ (BOOL)isCharacterSmartReplaceExempt:(unichar)character isPreviousCharacter:(BOOL)b
{
    return WebCore::isCharacterSmartReplaceExempt(character, b);
}

- (void)updateLayoutIgnorePendingStyleSheets
{
    WebThreadRun(^{
        for (auto* frame = [self _mainCoreFrame]; frame; frame = frame->tree().traverseNext()) {
            auto *document = frame->document();
            if (document)
                document->updateLayoutIgnorePendingStylesheets();
        }
    });
}
#endif

#if PLATFORM(IOS_FAMILY)

#if ENABLE(DRAG_SUPPORT)

- (BOOL)_requestStartDataInteraction:(CGPoint)clientPosition globalPosition:(CGPoint)globalPosition
{
    WebThreadLock();
    return _private->page->mainFrame().eventHandler().tryToBeginDragAtPoint(WebCore::IntPoint(clientPosition), WebCore::IntPoint(globalPosition));
}

- (void)_startDrag:(const WebCore::DragItem&)dragItem
{
    auto& dragImage = dragItem.image;
    auto image = dragImage.get().get();
    auto indicatorData = dragImage.indicatorData();

    if (indicatorData)
        _private->textIndicatorData = adoptNS([[WebUITextIndicatorData alloc] initWithImage:image textIndicatorData:indicatorData.value() scale:_private->page->deviceScaleFactor()]);
    else
        _private->textIndicatorData = adoptNS([[WebUITextIndicatorData alloc] initWithImage:image scale:_private->page->deviceScaleFactor()]);
    _private->draggedLinkURL = dragItem.url.isEmpty() ? nil : (NSURL *)dragItem.url;
    _private->draggedLinkTitle = dragItem.title.isEmpty() ? nil : (NSString *)dragItem.title;
    _private->dragPreviewFrameInRootViewCoordinates = dragItem.dragPreviewFrameInRootViewCoordinates;
    _private->dragSourceAction = kit(dragItem.sourceAction);
}

- (CGRect)_dataInteractionCaretRect
{
    WebThreadLock();
    if (auto* page = _private->page)
        return page->dragCaretController().caretRectInRootViewCoordinates();

    return { };
}

- (WebUITextIndicatorData *)_dataOperationTextIndicator
{
    return _private->dataOperationTextIndicator.get();
}

- (WebDragSourceAction)_dragSourceAction
{
    return _private->dragSourceAction;
}

- (NSString *)_draggedLinkTitle
{
    return _private->draggedLinkTitle.get();
}

- (NSURL *)_draggedLinkURL
{
    return _private->draggedLinkURL.get();
}

- (CGRect)_draggedElementBounds
{
    return _private->dragPreviewFrameInRootViewCoordinates;
}

- (WebUITextIndicatorData *)_getDataInteractionData
{
    return _private->textIndicatorData.get();
}

- (WebDragDestinationAction)dragDestinationActionMaskForSession:(id <UIDropSession>)session
{
    return [self._UIDelegateForwarder webView:self dragDestinationActionMaskForSession:session];
}

- (WebCore::DragData)dragDataForSession:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
    auto dragOperationMask = coreDragOperationMask(operation);
    auto dragDestinationActionMask = coreDragDestinationActionMask([self dragDestinationActionMaskForSession:session]);
    return { session, WebCore::roundedIntPoint(clientPosition), WebCore::roundedIntPoint(globalPosition), dragOperationMask, { }, dragDestinationActionMask };
}

- (uint64_t)_enteredDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
    WebThreadLock();
    auto dragData = [self dragDataForSession:session client:clientPosition global:globalPosition operation:operation];
    return kit(_private->page->dragController().dragEntered(dragData));
}

- (uint64_t)_updatedDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
    WebThreadLock();
    auto dragData = [self dragDataForSession:session client:clientPosition global:globalPosition operation:operation];
    return kit(_private->page->dragController().dragUpdated(dragData));
}

- (void)_exitedDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
    WebThreadLock();
    auto dragData = [self dragDataForSession:session client:clientPosition global:globalPosition operation:operation];
    _private->page->dragController().dragExited(dragData);
}

- (void)_performDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
    [self _tryToPerformDataInteraction:session client:clientPosition global:globalPosition operation:operation];
}

- (BOOL)_tryToPerformDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
    WebThreadLock();
    auto dragData = [self dragDataForSession:session client:clientPosition global:globalPosition operation:operation];
    return _private->page->dragController().performDragOperation(dragData);
}

- (void)_endedDataInteraction:(CGPoint)clientPosition global:(CGPoint)globalPosition
{
    WebThreadLock();
    _private->page->dragController().dragEnded();
    _private->dataOperationTextIndicator = nullptr;
    _private->dragPreviewFrameInRootViewCoordinates = CGRectNull;
    _private->dragSourceAction = WebDragSourceActionNone;
    _private->draggedLinkTitle = nil;
    _private->draggedLinkURL = nil;
}

- (void)_didConcludeEditDrag
{
    _private->dataOperationTextIndicator = nullptr;
    auto* page = _private->page;
    if (!page)
        return;

    constexpr OptionSet<WebCore::TextIndicatorOption> defaultEditDragTextIndicatorOptions {
        WebCore::TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection,
        WebCore::TextIndicatorOption::ExpandClipBeyondVisibleRect,
        WebCore::TextIndicatorOption::PaintAllContent,
        WebCore::TextIndicatorOption::IncludeMarginIfRangeMatchesSelection,
        WebCore::TextIndicatorOption::PaintBackgrounds,
        WebCore::TextIndicatorOption::UseSelectionRectForSizing,
        WebCore::TextIndicatorOption::IncludeSnapshotWithSelectionHighlight,
        WebCore::TextIndicatorOption::RespectTextColor
    };
    auto& frame = page->focusController().focusedOrMainFrame();
    if (auto range = frame.selection().selection().toNormalizedRange()) {
        if (auto textIndicator = WebCore::TextIndicator::createWithRange(*range, defaultEditDragTextIndicatorOptions, WebCore::TextIndicatorPresentationTransition::None, WebCore::FloatSize()))
            _private->dataOperationTextIndicator = adoptNS([[WebUITextIndicatorData alloc] initWithImage:nil textIndicatorData:textIndicator->data() scale:page->deviceScaleFactor()]);
    }
}

#elif PLATFORM(IOS)

- (BOOL)_requestStartDataInteraction:(CGPoint)clientPosition globalPosition:(CGPoint)globalPosition
{
    return NO;
}

- (WebUITextIndicatorData *)_getDataInteractionData
{
    return nil;
}

- (WebUITextIndicatorData *)_dataOperationTextIndicator
{
    return nil;
}

- (NSUInteger)_dragSourceAction
{
    return 0;
}

- (NSString *)_draggedLinkTitle
{
    return nil;
}

- (NSURL *)_draggedLinkURL
{
    return nil;
}

- (CGRect)_draggedElementBounds
{
    return CGRectNull;
}

- (uint64_t)_enteredDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
    return 0;
}

- (uint64_t)_updatedDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
    return 0;
}

- (void)_exitedDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
}

- (void)_performDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
}

- (BOOL)_tryToPerformDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation
{
    return NO;
}

- (void)_endedDataInteraction:(CGPoint)clientPosition global:(CGPoint)globalPosition
{
}

- (CGRect)_dataInteractionCaretRect
{
    return CGRectNull;
}

#endif

#endif // PLATFORM(IOS_FAMILY)

static NSMutableSet *knownPluginMIMETypes()
{
    static NSMutableSet *mimeTypes = [[NSMutableSet alloc] init];

    return mimeTypes;
}

+ (void)_registerPluginMIMEType:(NSString *)MIMEType
{
    [WebView registerViewClass:[WebHTMLView class] representationClass:[WebHTMLRepresentation class] forMIMEType:MIMEType];
    [knownPluginMIMETypes() addObject:MIMEType];
}

+ (void)_unregisterPluginMIMEType:(NSString *)MIMEType
{
    [self _unregisterViewClassAndRepresentationClassForMIMEType:MIMEType];
    [knownPluginMIMETypes() removeObject:MIMEType];
}

+ (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType allowingPlugins:(BOOL)allowPlugins
{
    MIMEType = [MIMEType lowercaseString];
    Class viewClass = [[WebFrameView _viewTypesAllowImageTypeOmission:YES] _webkit_objectForMIMEType:MIMEType];
    Class repClass = [[WebDataSource _repTypesAllowImageTypeOmission:YES] _webkit_objectForMIMEType:MIMEType];

#if PLATFORM(IOS_FAMILY)
#define WebPDFView ([WebView _getPDFViewClass])
#endif
    if (!viewClass || !repClass || [[WebPDFView supportedMIMETypes] containsObject:MIMEType]) {
#if PLATFORM(IOS_FAMILY)
#undef WebPDFView
#endif
        // Our optimization to avoid loading the plug-in DB and image types for the HTML case failed.

        if (allowPlugins) {
            // Load the plug-in DB allowing plug-ins to install types.
            [WebPluginDatabase sharedDatabase];
        }

        // Load the image types and get the view class and rep class. This should be the fullest picture of all handled types.
        viewClass = [[WebFrameView _viewTypesAllowImageTypeOmission:NO] _webkit_objectForMIMEType:MIMEType];
        repClass = [[WebDataSource _repTypesAllowImageTypeOmission:NO] _webkit_objectForMIMEType:MIMEType];
    }

    if (viewClass && repClass) {
        if (viewClass == [WebHTMLView class] && repClass == [WebHTMLRepresentation class]) {
            // Special-case WebHTMLView for text types that shouldn't be shown.
            if ([[WebHTMLView unsupportedTextMIMETypes] containsObject:MIMEType])
                return NO;

            // If the MIME type is a known plug-in we might not want to load it.
            if (!allowPlugins && [knownPluginMIMETypes() containsObject:MIMEType]) {
                BOOL isSupportedByWebKit = [[WebHTMLView supportedNonImageMIMETypes] containsObject:MIMEType] ||
                    [[WebHTMLView supportedMIMETypes] containsObject:MIMEType];

                // If this is a known plug-in MIME type and WebKit can't show it natively, we don't want to show it.
                if (!isSupportedByWebKit)
                    return NO;
            }
        }
        if (vClass)
            *vClass = viewClass;
        if (rClass)
            *rClass = repClass;
        return YES;
    }

    return NO;
}

- (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType
{
    if ([[self class] _viewClass:vClass andRepresentationClass:rClass forMIMEType:MIMEType allowingPlugins:[_private->preferences arePlugInsEnabled]])
        return YES;
#if !PLATFORM(IOS_FAMILY)
    if (_private->pluginDatabase) {
        WebBasePluginPackage *pluginPackage = [_private->pluginDatabase pluginForMIMEType:MIMEType];
        if (pluginPackage) {
            if (vClass)
                *vClass = [WebHTMLView class];
            if (rClass)
                *rClass = [WebHTMLRepresentation class];
            return YES;
        }
    }
#endif
    return NO;
}

+ (void)_setAlwaysUsesComplexTextCodePath:(BOOL)f
{
    WebCore::FontCascade::setCodePath(f ? WebCore::FontCascade::CodePath::Complex : WebCore::FontCascade::CodePath::Auto);
}

+ (BOOL)canCloseAllWebViews
{
    return WebCore::DOMWindow::dispatchAllPendingBeforeUnloadEvents();
}

+ (void)closeAllWebViews
{
    WebCore::DOMWindow::dispatchAllPendingUnloadEvents();

    // This will close the WebViews in a random order. Change this if close order is important.
    for (WebView *webView in [(__bridge NSSet *)allWebViewsSet().get() allObjects])
        [webView close];
}

+ (BOOL)canShowFile:(NSString *)path
{
    return [[self class] canShowMIMEType:[WebView _MIMETypeForFile:path]];
}

#if !PLATFORM(IOS_FAMILY)
+ (NSString *)suggestedFileExtensionForMIMEType:(NSString *)type
{
    return [[NSURLFileTypeMappings sharedMappings] preferredExtensionForMIMEType:type];
}
#endif

- (BOOL)_isClosed
{
    return !_private || _private->closed;
}

#if PLATFORM(IOS_FAMILY)

- (void)_dispatchUnloadEvent
{
    WebThreadRun(^{
        WebFrame *mainFrame = [self mainFrame];
        auto* coreMainFrame = core(mainFrame);
        if (coreMainFrame) {
            auto* document = coreMainFrame->document();
            if (document)
                document->dispatchWindowEvent(WebCore::Event::create(WebCore::eventNames().unloadEvent, WebCore::Event::CanBubble::No, WebCore::Event::IsCancelable::No));
        }
    });
}

- (DOMCSSStyleDeclaration *)styleAtSelectionStart
{
    auto* mainFrame = [self _mainCoreFrame];
    if (!mainFrame)
        return nil;
    RefPtr<WebCore::EditingStyle> editingStyle = WebCore::EditingStyle::styleAtSelectionStart(mainFrame->selection().selection());
    if (!editingStyle)
        return nil;
    auto* style = editingStyle->style();
    if (!style)
        return nil;
    return kit(&style->ensureCSSStyleDeclaration());
}

- (NSUInteger)_renderTreeSize
{
    if (!_private->page)
        return 0;
    return _private->page->renderTreeSize();
}

- (void)_dispatchTileDidDraw:(CALayer*)tile
{
    id mailDelegate = [self _webMailDelegate];
    if ([mailDelegate respondsToSelector:@selector(_webthread_webView:tileDidDraw:)]) {
        [mailDelegate _webthread_webView:self tileDidDraw:tile];
        return;
    }

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (!OSAtomicCompareAndSwap32(0, 1, &_private->didDrawTiles))
        return;
    ALLOW_DEPRECATED_DECLARATIONS_END

    WebThreadLock();

    [[[self _UIKitDelegateForwarder] asyncForwarder] webViewDidDrawTiles:self];
}

- (void)_willStartScrollingOrZooming
{
    // Pause timers during top level interaction
    if (_private->mainViewIsScrollingOrZooming)
        return;
    _private->mainViewIsScrollingOrZooming = YES;

    [self hideFormValidationMessage];

    // This suspends active DOM objects like timers, but not media.
    [[self mainFrame] setTimeoutsPaused:YES];

    // This defers loading callbacks only.
    // WARNING: This behavior could change if Bug 49401 lands in open source WebKit again.
    [self setDefersCallbacks:YES];
}

- (void)_didFinishScrollingOrZooming
{
    if (!_private->mainViewIsScrollingOrZooming)
        return;
    _private->mainViewIsScrollingOrZooming = NO;
    [self setDefersCallbacks:NO];
    [[self mainFrame] setTimeoutsPaused:NO];
    if (auto* view = [self _mainCoreFrame]->view())
        view->resumeVisibleImageAnimationsIncludingSubframes();
}

- (void)_setResourceLoadSchedulerSuspended:(BOOL)suspend
{
    if (suspend)
        webResourceLoadScheduler().suspendPendingRequests();
    else
        webResourceLoadScheduler().resumePendingRequests();
}

+ (BOOL)_isUnderMemoryPressure
{
    return MemoryPressureHandler::singleton().isUnderMemoryPressure();
}

#endif // PLATFORM(IOS_FAMILY)

- (void)_closePluginDatabases
{
    pluginDatabaseClientCount--;

    // Close both sets of plug-in databases because plug-ins need an opportunity to clean up files, etc.

#if !PLATFORM(IOS_FAMILY)
    // Unload the WebView local plug-in database.
    if (_private->pluginDatabase) {
        [_private->pluginDatabase destroyAllPluginInstanceViews];
        [_private->pluginDatabase close];
        _private->pluginDatabase = nil;
    }
#endif

    // Keep the global plug-in database active until the app terminates to avoid having to reload plug-in bundles.
    if (!pluginDatabaseClientCount && applicationIsTerminating)
        [WebPluginDatabase closeSharedDatabase];
}

- (void)_closeWithFastTeardown
{
#ifndef NDEBUG
    WTF::RefCountedLeakCounter::suppressMessages("At least one WebView was closed with fast teardown.");
#endif

#if !PLATFORM(IOS_FAMILY)
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
#endif
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [self _closePluginDatabases];
}

static bool fastDocumentTeardownEnabled()
{
#ifdef NDEBUG
    static bool enabled = ![[NSUserDefaults standardUserDefaults] boolForKey:WebKitEnableFullDocumentTeardownPreferenceKey];
#else
    static bool initialized = false;
    static bool enabled = false;
    if (!initialized) {
        // This allows debug builds to default to not have fast teardown, so leak checking still works.
        // But still allow the WebKitEnableFullDocumentTeardown default to override it if present.
        NSNumber *setting = [[NSUserDefaults standardUserDefaults] objectForKey:WebKitEnableFullDocumentTeardownPreferenceKey];
        if (setting)
            enabled = ![setting boolValue];
        initialized = true;
    }
#endif
    return enabled;
}

// _close is here only for backward compatibility; clients and subclasses should use
// public method -close instead.
- (void)_close
{
#if PLATFORM(IOS_FAMILY)
    // Always clear delegates on calling thread, becasue caller can deallocate delegates right after calling -close
    // (and could in fact already be in delegate's -dealloc method, as seen in <rdar://problem/11540972>).

    [self _clearDelegates];

    // Fix for problems such as <rdar://problem/5774587> Crash closing tab in WebCore::Frame::page() from -[WebCoreFrameBridge pauseTimeouts]
    WebThreadRun(^{
#endif

    if (!_private || _private->closed)
        return;

    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewWillCloseNotification object:self];

    _private->closed = YES;
    [self _removeFromAllWebViewsSet];

#ifndef NDEBUG
    WTF::RefCountedLeakCounter::cancelMessageSuppression(webViewIsOpen);
#endif

    // To quit the apps fast we skip document teardown, except plugins
    // need to be destroyed and unloaded.
    if (applicationIsTerminating && fastDocumentTeardownEnabled()) {
        [self _closeWithFastTeardown];
        return;
    }

#if ENABLE(VIDEO_PRESENTATION_MODE) && !PLATFORM(IOS_FAMILY)
    [self _exitVideoFullscreen];
#endif

#if PLATFORM(IOS_FAMILY)
    _private->closing = YES;
#endif

    if (auto* mainFrame = [self _mainCoreFrame])
        mainFrame->loader().detachFromParent();

    [self setHostWindow:nil];

#if !PLATFORM(IOS_FAMILY)
    [self setDownloadDelegate:nil];
    [self setEditingDelegate:nil];
    [self setFrameLoadDelegate:nil];
    [self setPolicyDelegate:nil];
    [self setResourceLoadDelegate:nil];
    [self setScriptDebugDelegate:nil];
    [self setUIDelegate:nil];

    [_private->inspector inspectedWebViewClosed];
#endif
#if PLATFORM(MAC)
    [_private->immediateActionController webViewClosed];
#endif

#if !PLATFORM(IOS_FAMILY)
    // To avoid leaks, call removeDragCaret in case it wasn't called after moveDragCaretToPoint.
    [self removeDragCaret];
#endif

    _private->group->removeWebView(self);

    // Deleteing the WebCore::Page will clear the back/forward cache so we call destroy on
    // all the plug-ins in the back/forward cache to break any retain cycles.
    // See comment in HistoryItem::releaseAllPendingPageCaches() for more information.
    auto* page = _private->page;
    _private->page = nullptr;
    WebKit::DeferredPageDestructor::createDeferredPageDestructor(std::unique_ptr<WebCore::Page>(page));

#if !PLATFORM(IOS_FAMILY)
    if (_private->hasSpellCheckerDocumentTag) {
        [[NSSpellChecker sharedSpellChecker] closeSpellDocumentWithTag:_private->spellCheckerDocumentTag];
        _private->hasSpellCheckerDocumentTag = NO;
    }
#endif

    if (_private->layerFlushController) {
        _private->layerFlushController->invalidate();
        _private->layerFlushController = nullptr;
    }

    [[self _notificationProvider] unregisterWebView:self];

#if !PLATFORM(IOS_FAMILY)
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
#endif
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [WebPreferences _removeReferenceForIdentifier:[self preferencesIdentifier]];

    auto preferences = std::exchange(_private->preferences, nil);
    [preferences didRemoveFromWebView];

    [self _closePluginDatabases];

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    if (_private->m_playbackTargetPicker) {
        _private->m_playbackTargetPicker->invalidate();
        _private->m_playbackTargetPicker = nullptr;
    }
#endif

#ifndef NDEBUG
    // Need this to make leak messages accurate.
    if (applicationIsTerminating) {
        WebCore::GCController::singleton().garbageCollectNow();
        [WebCache setDisabled:YES];
    }
#endif
#if PLATFORM(IOS_FAMILY)
    // Fix for problems such as <rdar://problem/5774587> Crash closing tab in WebCore::Frame::page() from -[WebCoreFrameBridge pauseTimeouts]
    });
#endif
}

// Indicates if the WebView is in the midst of a user gesture.
- (BOOL)_isProcessingUserGesture
{
    return WebCore::UserGestureIndicator::processingUserGesture();
}

+ (NSString *)_MIMETypeForFile:(NSString *)path
{
#if !PLATFORM(IOS_FAMILY)
    NSString *extension = [path pathExtension];
#endif
    NSString *MIMEType = nil;

#if !PLATFORM(IOS_FAMILY)
    // Get the MIME type from the extension.
    if ([extension length] != 0) {
        MIMEType = [[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
    }
#endif

    // If we can't get a known MIME type from the extension, sniff.
    if ([MIMEType length] == 0 || [MIMEType isEqualToString:@"application/octet-stream"]) {
        NSFileHandle *handle = [NSFileHandle fileHandleForReadingAtPath:path];
        NSData *data = [handle readDataOfLength:WEB_GUESS_MIME_TYPE_PEEK_LENGTH];
        [handle closeFile];
        if ([data length] != 0) {
            MIMEType = [data _webkit_guessedMIMEType];
        }
        if ([MIMEType length] == 0) {
            MIMEType = @"application/octet-stream";
        }
    }

    return MIMEType;
}

- (WebDownload *)_downloadURL:(NSURL *)URL
{
    ASSERT(URL);

    auto request = adoptNS([[NSURLRequest alloc] initWithURL:URL]);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [WebDownload _downloadWithRequest:request.get() delegate:_private->downloadDelegate directory:nil];
ALLOW_DEPRECATED_DECLARATIONS_END
}

- (WebView *)_openNewWindowWithRequest:(NSURLRequest *)request
{
    WebView *newWindowWebView = [[self _UIDelegateForwarder] webView:self createWebViewWithRequest:nil windowFeatures:@{ }];
    if (!newWindowWebView)
        return nil;

    CallUIDelegate(newWindowWebView, @selector(webViewShow:));
    return newWindowWebView;
}

- (BOOL)_useDarkAppearance
{
    if (!_private || !_private->page)
        return NO;
    return _private->page->useDarkAppearance();
}

- (void)_setUseDarkAppearance:(BOOL)useDarkAppearance
{
    if (!_private || !_private->page)
        return;
    [self _setUseDarkAppearance:useDarkAppearance useElevatedUserInterfaceLevel:_private->page->useElevatedUserInterfaceLevel()];
}

- (BOOL)_useElevatedUserInterfaceLevel
{
    if (!_private || !_private->page)
        return NO;
    return _private->page->useElevatedUserInterfaceLevel();
}

- (void)_setUseElevatedUserInterfaceLevel:(BOOL)useElevatedUserInterfaceLevel
{
    if (!_private || !_private->page)
        return;
    [self _setUseDarkAppearance:_private->page->useDarkAppearance() useElevatedUserInterfaceLevel:useElevatedUserInterfaceLevel];
}

- (void)_setUseDarkAppearance:(BOOL)useDarkAppearance useInactiveAppearance:(BOOL)useInactiveAppearance
{
    // FIXME: Remove once UIWebView has moved off this old method.
    [self _setUseDarkAppearance:useDarkAppearance useElevatedUserInterfaceLevel:!useInactiveAppearance];
}

- (void)_setUseDarkAppearance:(BOOL)useDarkAppearance useElevatedUserInterfaceLevel:(BOOL)useElevatedUserInterfaceLevel
{
    if (!_private || !_private->page)
        return;
    _private->page->effectiveAppearanceDidChange(useDarkAppearance, useElevatedUserInterfaceLevel);
}

+ (void)_setIconLoadingEnabled:(BOOL)enabled
{
    iconLoadingEnabled = enabled;
}

+ (BOOL)_isIconLoadingEnabled
{
    return iconLoadingEnabled;
}

- (WebInspector *)inspector
{
    if (!_private->inspector)
        _private->inspector = adoptNS([[WebInspector alloc] initWithInspectedWebView:self]);
    return _private->inspector.get();
}

#if ENABLE(REMOTE_INSPECTOR)
+ (void)_enableRemoteInspector
{
    Inspector::RemoteInspector::singleton().start();
}

+ (void)_disableRemoteInspector
{
    Inspector::RemoteInspector::singleton().stop();
}

+ (void)_disableAutoStartRemoteInspector
{
    Inspector::RemoteInspector::startDisabled();
}

+ (BOOL)_isRemoteInspectorEnabled
{
    return Inspector::RemoteInspector::singleton().enabled();
}

+ (BOOL)_hasRemoteInspectorSession
{
    return Inspector::RemoteInspector::singleton().hasActiveDebugSession();
}

- (BOOL)allowsRemoteInspection
{
    return _private->page->remoteInspectionAllowed();
}

- (void)setAllowsRemoteInspection:(BOOL)allow
{
    _private->page->setRemoteInspectionAllowed(allow);
}

- (void)setShowingInspectorIndication:(BOOL)showing
{
#if PLATFORM(IOS_FAMILY)
    ASSERT(WebThreadIsLocked());

    if (showing) {
        if (!_private->indicateLayer) {
            _private->indicateLayer = adoptNS([[WebIndicateLayer alloc] initWithWebView:self]);
            [_private->indicateLayer setNeedsLayout];
            [[[self window] hostLayer] addSublayer:_private->indicateLayer.get()];
        }
    } else {
        [_private->indicateLayer removeFromSuperlayer];
        _private->indicateLayer = nil;
    }
#else
    // Implemented in WebCore::InspectorOverlay.
#endif
}

#if PLATFORM(IOS_FAMILY)
- (void)_setHostApplicationProcessIdentifier:(pid_t)pid auditToken:(audit_token_t)auditToken
{
    RetainPtr<CFDataRef> auditData = adoptCF(CFDataCreate(nullptr, (const UInt8*)&auditToken, sizeof(auditToken)));
    Inspector::RemoteInspector::singleton().setParentProcessInformation(pid, auditData);
}
#endif // PLATFORM(IOS_FAMILY)
#endif // ENABLE(REMOTE_INSPECTOR)

- (NakedPtr<WebCore::Page>)page
{
    return _private->page;
}

#if !PLATFORM(IOS_FAMILY)
- (NSMenu *)_menuForElement:(NSDictionary *)element defaultItems:(NSArray *)items
{
    NSArray *defaultMenuItems = [[WebDefaultUIDelegate sharedUIDelegate] webView:self contextMenuItemsForElement:element defaultMenuItems:items];
    NSArray *menuItems = defaultMenuItems;

    // CallUIDelegate returns nil if UIDelegate is nil or doesn't respond to the selector. So we need to check that here
    // to distinguish between using defaultMenuItems or the delegate really returning nil to say "no context menu".
    SEL selector = @selector(webView:contextMenuItemsForElement:defaultMenuItems:);
    if (_private->UIDelegate && [_private->UIDelegate respondsToSelector:selector]) {
        menuItems = CallUIDelegate(self, selector, element, defaultMenuItems);
        if (!menuItems)
            return nil;
    }

    unsigned count = [menuItems count];
    if (!count)
        return nil;

    auto menu = adoptNS([[NSMenu alloc] init]);
    for (unsigned i = 0; i < count; i++)
        [menu addItem:[menuItems objectAtIndex:i]];

    return menu.autorelease();
}
#endif

- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(NSUInteger)modifierFlags
{
    // We originally intended to call this delegate method sometimes with a nil dictionary, but due to
    // a bug dating back to WebKit 1.0 this delegate was never called with nil! Unfortunately we can't
    // start calling this with nil since it will break Adobe Help Viewer, and possibly other clients.
    if (!dictionary)
        return;
    CallUIDelegate(self, @selector(webView:mouseDidMoveOverElement:modifierFlags:), dictionary, modifierFlags);
}

- (void)_loadBackForwardListFromOtherView:(WebView *)otherView
{
    if (!_private->page)
        return;

    if (!otherView->_private->page)
        return;

    // It turns out the right combination of behavior is done with the back/forward load
    // type.  (See behavior matrix at the top of WebFramePrivate.)  So we copy all the items
    // in the back forward list, and go to the current one.

    auto& backForward = _private->page->backForward();
    ASSERT(!backForward.currentItem()); // destination list should be empty

    auto& otherBackForward = otherView->_private->page->backForward();
    if (!otherBackForward.currentItem())
        return; // empty back forward list, bail

    WebCore::HistoryItem* newItemToGoTo = nullptr;

    int lastItemIndex = otherBackForward.forwardCount();
    for (int i = -otherBackForward.backCount(); i <= lastItemIndex; ++i) {
        if (i == 0) {
            // If this item is showing , save away its current scroll and form state,
            // since that might have changed since loading and it is normally not saved
            // until we leave that page.
            otherView->_private->page->mainFrame().loader().history().saveDocumentAndScrollState();
        }
        Ref<WebCore::HistoryItem> newItem = otherBackForward.itemAtIndex(i)->copy();
        if (i == 0)
            newItemToGoTo = newItem.ptr();
        backForward.client().addItem(WTFMove(newItem));
    }

    ASSERT(newItemToGoTo);
    _private->page->goToItem(*newItemToGoTo, WebCore::FrameLoadType::IndexedBackForward, WebCore::ShouldTreatAsContinuingLoad::No);
}

- (void)_setFormDelegate: (id<WebFormDelegate>)delegate
{
    _private->formDelegate = delegate;
#if PLATFORM(IOS_FAMILY)
    [_private->formDelegateForwarder clearTarget];
    _private->formDelegateForwarder = nil;
#endif
}

- (id<WebFormDelegate>)_formDelegate
{
    return _private->formDelegate;
}

#if PLATFORM(IOS_FAMILY)
- (id)_formDelegateForwarder
{
    if (_private->closing)
        return nil;

    if (!_private->formDelegateForwarder)
        _private->formDelegateForwarder = adoptNS([[_WebSafeForwarder alloc] initWithTarget:[self _formDelegate] defaultTarget:[WebDefaultFormDelegate sharedFormDelegate]]);
    return _private->formDelegateForwarder.get();
}

- (id)_formDelegateForSelector:(SEL)selector
{
    id delegate = self->_private->formDelegate;
    if ([delegate respondsToSelector:selector])
        return delegate;

    delegate = [WebDefaultFormDelegate sharedFormDelegate];
    if ([delegate respondsToSelector:selector])
        return delegate;

    return nil;
}
#endif

#if PLATFORM(MAC)
- (BOOL)_needsFrameLoadDelegateRetainQuirk
{
    static BOOL needsQuirk = _CFAppVersionCheckLessThan(CFSTR("com.equinux.iSale5"), -1, 5.6);
    return needsQuirk;
}

static bool needsSelfRetainWhileLoadingQuirk()
{
    static bool needsQuirk = WebCore::MacApplication::isAperture();
    return needsQuirk;
}
#endif

- (void)_preferencesChangedNotification:(NSNotification *)notification
{
#if PLATFORM(IOS_FAMILY)
    // For WebViews that load synchronously, preference changes need to be propagated
    // down to WebCore synchronously so that loads in that WebView have the correct
    // up-to-date settings.
    if (!WebThreadIsLocked())
        WebThreadLock();
    if ([[self mainFrame] _loadsSynchronously]) {
#endif

    WebPreferences *preferences = (WebPreferences *)[notification object];
    [self _preferencesChanged:preferences];

#if PLATFORM(IOS_FAMILY)
    } else {
        WebThreadRun(^{
            // It is possible that the prefs object has already changed before the invocation could be called
            // on the web thread. This is not possible on TOT which is why they have a simple ASSERT.
            WebPreferences *preferences = (WebPreferences *)[notification object];
            if (preferences != [self preferences])
                return;
            [self _preferencesChanged:preferences];
        });
    }
#endif
}

- (void)_preferencesChanged:(WebPreferences *)preferences
{
    using namespace WebCore;
    ASSERT(preferences == [self preferences]);
    if (!_private->userAgentOverridden)
        [self _invalidateUserAgentCache];

    // Update corresponding WebCore Settings object.
    if (!_private->page)
        return;

#if PLATFORM(IOS_FAMILY)
    // iOS waits to call [WebDatabaseManager sharedWebDatabaseManager] until
    // didOneTimeInitialization is true so that we don't initialize databases
    // until the first WebView has been created. This is because database
    // initialization current requires disk access to populate the origins
    // quota map and we want to do this lazily by waiting until WebKit is
    // used to display web content in a WebView. The possible cases are:
    //   - on one time initialize (the first WebView creation) if databases are enabled (default)
    //   - when databases are dynamically enabled later, and they were disabled at startup (this case)
    if (didOneTimeInitialization) {
        if ([preferences databasesEnabled])
            [WebDatabaseManager sharedWebDatabaseManager];
        if ([preferences storageTrackerEnabled])
            WebKitInitializeStorageIfNecessary();
    }
#endif

    WebCore::setAdditionalSupportedImageTypes(makeVector<String>([preferences additionalSupportedImageTypes]));

    [self _preferencesChangedGenerated:preferences];

    auto& settings = _private->page->settings();
    
    // FIXME: These should switch to using WebPreferences for storage and adopt autogeneration.
    settings.setInteractiveFormValidationEnabled([self interactiveFormValidationEnabled]);
    settings.setValidationMessageTimerMagnification([self validationMessageTimerMagnification]);

    // FIXME: Autogeneration should be smart enough to deal with core/kit conversions and validation of non-primitive types like enums, URLs and Seconds.
    settings.setStorageBlockingPolicy(core([preferences storageBlockingPolicy]));
    settings.setEditableLinkBehavior(core([preferences editableLinkBehavior]));
    settings.setJavaScriptRuntimeFlags(JSC::RuntimeFlags([preferences javaScriptRuntimeFlags]));
    settings.setFrameFlattening((const WebCore::FrameFlattening)[preferences frameFlattening]);
    settings.setTextDirectionSubmenuInclusionBehavior(core([preferences textDirectionSubmenuInclusionBehavior]));
    settings.setBackForwardCacheExpirationInterval(Seconds { [preferences _backForwardCacheExpirationInterval] });
    settings.setPitchCorrectionAlgorithm(static_cast<WebCore::MediaPlayerEnums::PitchCorrectionAlgorithm>([preferences _pitchCorrectionAlgorithm]));

    // FIXME: Add a way to have a preference check multiple different keys.
    settings.setDeveloperExtrasEnabled([preferences developerExtrasEnabled]);

    BOOL mediaPlaybackRequiresUserGesture = [preferences mediaPlaybackRequiresUserGesture];
    settings.setVideoPlaybackRequiresUserGesture(mediaPlaybackRequiresUserGesture || [preferences videoPlaybackRequiresUserGesture]);
    settings.setAudioPlaybackRequiresUserGesture(mediaPlaybackRequiresUserGesture || [preferences audioPlaybackRequiresUserGesture]);

    RuntimeEnabledFeatures::sharedFeatures().setWebSQLDisabled(![preferences webSQLEnabled]);
    DatabaseManager::singleton().setIsAvailable([preferences databasesEnabled]);
    settings.setLocalStorageDatabasePath([preferences _localStorageDatabasePath]);
    _private->page->setSessionID([preferences privateBrowsingEnabled] ? PAL::SessionID::legacyPrivateSessionID() : PAL::SessionID::defaultSessionID());
    _private->group->storageNamespaceProvider().setSessionIDForTesting([preferences privateBrowsingEnabled] ? PAL::SessionID::legacyPrivateSessionID() : PAL::SessionID::defaultSessionID());

#if PLATFORM(MAC)
    // This parses the user stylesheet synchronously so anything that may affect it should be done first.
    if ([preferences userStyleSheetEnabled]) {
        NSString* location = [[preferences userStyleSheetLocation] _web_originalDataAsString];
        settings.setUserStyleSheetLocation([NSURL URLWithString:(location ? location : @"")]);
    } else
        settings.setUserStyleSheetLocation([NSURL URLWithString:@""]);
#endif

#if PLATFORM(IOS_FAMILY)
    WebCore::DeprecatedGlobalSettings::setAudioSessionCategoryOverride([preferences audioSessionCategoryOverride]);
    WebCore::DeprecatedGlobalSettings::setNetworkDataUsageTrackingEnabled([preferences networkDataUsageTrackingEnabled]);
    WebCore::DeprecatedGlobalSettings::setNetworkInterfaceName([preferences networkInterfaceName]);
    ASSERT_WITH_MESSAGE(settings.backForwardCacheSupportsPlugins(), "BackForwardCacheSupportsPlugins should be enabled on iOS.");
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    settings.setMediaKeysStorageDirectory([preferences mediaKeysStorageDirectory]);
#endif

    // FIXME: Is this relevent to WebKitLegacy? If not, we should remove it.
    WebCore::DeprecatedGlobalSettings::setResourceLoadStatisticsEnabled([preferences resourceLoadStatisticsEnabled]);

    // Application Cache Preferences are stored on the global cache storage manager, not in Settings.
    [WebApplicationCache setDefaultOriginQuota:[preferences applicationCacheDefaultOriginQuota]];

    BOOL zoomsTextOnly = [preferences zoomsTextOnly];
    if (_private->zoomsTextOnly != zoomsTextOnly)
        [self _setZoomMultiplier:_private->zoomMultiplier isTextOnly:zoomsTextOnly];

#if PLATFORM(IOS_FAMILY)
    if (auto tileCache = self.window.tileCache) {
        tileCache->setTileBordersVisible(preferences.showDebugBorders);
        tileCache->setTilePaintCountersVisible(preferences.showRepaintCounter);
        tileCache->setAcceleratedDrawingEnabled(preferences.acceleratedDrawingEnabled);
    }
    [WAKView _setInterpolationQuality:[preferences _interpolationQuality]];
#endif
}

static inline IMP getMethod(id o, SEL s)
{
    return [o respondsToSelector:s] ? [o methodForSelector:s] : 0;
}

#if PLATFORM(IOS_FAMILY)
- (id)_UIKitDelegateForwarder
{
    if (_private->closing)
        return nil;

    if (!_private->UIKitDelegateForwarder)
        _private->UIKitDelegateForwarder = adoptNS([[_WebSafeForwarder alloc] initWithTarget:_private->UIKitDelegate defaultTarget:[WebDefaultUIKitDelegate sharedUIKitDelegate]]);
    return _private->UIKitDelegateForwarder.get();
}
#endif

- (void)_cacheResourceLoadDelegateImplementations
{
    WebResourceDelegateImplementationCache *cache = &_private->resourceLoadDelegateImplementations;
    id delegate = _private->resourceProgressDelegate;

    if (!delegate) {
        bzero(cache, sizeof(WebResourceDelegateImplementationCache));
        return;
    }

    cache->didFailLoadingWithErrorFromDataSourceFunc = getMethod(delegate, @selector(webView:resource:didFailLoadingWithError:fromDataSource:));
    cache->didFinishLoadingFromDataSourceFunc = getMethod(delegate, @selector(webView:resource:didFinishLoadingFromDataSource:));
    cache->didLoadResourceFromMemoryCacheFunc = getMethod(delegate, @selector(webView:didLoadResourceFromMemoryCache:response:length:fromDataSource:));
    cache->didReceiveAuthenticationChallengeFunc = getMethod(delegate, @selector(webView:resource:didReceiveAuthenticationChallenge:fromDataSource:));
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    cache->canAuthenticateAgainstProtectionSpaceFunc = getMethod(delegate, @selector(webView:resource:canAuthenticateAgainstProtectionSpace:forDataSource:));
#endif

#if PLATFORM(IOS_FAMILY)
    cache->connectionPropertiesFunc = getMethod(delegate, @selector(webView:connectionPropertiesForResource:dataSource:));
    cache->webThreadDidFinishLoadingFromDataSourceFunc = getMethod(delegate, @selector(webThreadWebView:resource:didFinishLoadingFromDataSource:));
    cache->webThreadDidFailLoadingWithErrorFromDataSourceFunc = getMethod(delegate, @selector(webThreadWebView:resource:didFailLoadingWithError:fromDataSource:));
    cache->webThreadIdentifierForRequestFunc = getMethod(delegate, @selector(webThreadWebView:identifierForInitialRequest:fromDataSource:));
    cache->webThreadDidLoadResourceFromMemoryCacheFunc = getMethod(delegate, @selector(webThreadWebView:didLoadResourceFromMemoryCache:response:length:fromDataSource:));
    cache->webThreadWillSendRequestFunc = getMethod(delegate, @selector(webThreadWebView:resource:willSendRequest:redirectResponse:fromDataSource:));
    cache->webThreadDidReceiveResponseFunc = getMethod(delegate, @selector(webThreadWebView:resource:didReceiveResponse:fromDataSource:));
    cache->webThreadDidReceiveContentLengthFunc = getMethod(delegate, @selector(webThreadWebView:resource:didReceiveContentLength:fromDataSource:));
    cache->webThreadWillCacheResponseFunc = getMethod(delegate, @selector(webThreadWebView:resource:willCacheResponse:fromDataSource:));
#endif

    cache->didReceiveContentLengthFunc = getMethod(delegate, @selector(webView:resource:didReceiveContentLength:fromDataSource:));
    cache->didReceiveResponseFunc = getMethod(delegate, @selector(webView:resource:didReceiveResponse:fromDataSource:));
    cache->identifierForRequestFunc = getMethod(delegate, @selector(webView:identifierForInitialRequest:fromDataSource:));
    cache->plugInFailedWithErrorFunc = getMethod(delegate, @selector(webView:plugInFailedWithError:dataSource:));
    cache->willCacheResponseFunc = getMethod(delegate, @selector(webView:resource:willCacheResponse:fromDataSource:));
    cache->willSendRequestFunc = getMethod(delegate, @selector(webView:resource:willSendRequest:redirectResponse:fromDataSource:));
    cache->shouldUseCredentialStorageFunc = getMethod(delegate, @selector(webView:resource:shouldUseCredentialStorageForDataSource:));
    cache->shouldPaintBrokenImageForURLFunc = getMethod(delegate, @selector(webView:shouldPaintBrokenImageForURL:));
}

- (void)_cacheFrameLoadDelegateImplementations
{
    WebFrameLoadDelegateImplementationCache *cache = &_private->frameLoadDelegateImplementations;
    id delegate = _private->frameLoadDelegate;

    if (!delegate) {
        bzero(cache, sizeof(WebFrameLoadDelegateImplementationCache));
        return;
    }

    cache->didCancelClientRedirectForFrameFunc = getMethod(delegate, @selector(webView:didCancelClientRedirectForFrame:));
    cache->didChangeLocationWithinPageForFrameFunc = getMethod(delegate, @selector(webView:didChangeLocationWithinPageForFrame:));
    cache->didPushStateWithinPageForFrameFunc = getMethod(delegate, @selector(webView:didPushStateWithinPageForFrame:));
    cache->didReplaceStateWithinPageForFrameFunc = getMethod(delegate, @selector(webView:didReplaceStateWithinPageForFrame:));
    cache->didPopStateWithinPageForFrameFunc = getMethod(delegate, @selector(webView:didPopStateWithinPageForFrame:));
#if JSC_OBJC_API_ENABLED
    cache->didCreateJavaScriptContextForFrameFunc = getMethod(delegate, @selector(webView:didCreateJavaScriptContext:forFrame:));
#endif
    cache->didClearWindowObjectForFrameFunc = getMethod(delegate, @selector(webView:didClearWindowObject:forFrame:));
    cache->didClearWindowObjectForFrameInScriptWorldFunc = getMethod(delegate, @selector(webView:didClearWindowObjectForFrame:inScriptWorld:));
    cache->didClearInspectorWindowObjectForFrameFunc = getMethod(delegate, @selector(webView:didClearInspectorWindowObject:forFrame:));
    cache->didCommitLoadForFrameFunc = getMethod(delegate, @selector(webView:didCommitLoadForFrame:));
    cache->didFailLoadWithErrorForFrameFunc = getMethod(delegate, @selector(webView:didFailLoadWithError:forFrame:));
    cache->didFailProvisionalLoadWithErrorForFrameFunc = getMethod(delegate, @selector(webView:didFailProvisionalLoadWithError:forFrame:));
    cache->didFinishDocumentLoadForFrameFunc = getMethod(delegate, @selector(webView:didFinishDocumentLoadForFrame:));
    cache->didFinishLoadForFrameFunc = getMethod(delegate, @selector(webView:didFinishLoadForFrame:));
    cache->didFirstLayoutInFrameFunc = getMethod(delegate, @selector(webView:didFirstLayoutInFrame:));
    cache->didFirstVisuallyNonEmptyLayoutInFrameFunc = getMethod(delegate, @selector(webView:didFirstVisuallyNonEmptyLayoutInFrame:));
    cache->didLayoutFunc = getMethod(delegate, @selector(webView:didLayout:));
    cache->didHandleOnloadEventsForFrameFunc = getMethod(delegate, @selector(webView:didHandleOnloadEventsForFrame:));
#if PLATFORM(MAC)
    cache->didReceiveIconForFrameFunc = getMethod(delegate, @selector(webView:didReceiveIcon:forFrame:));
#endif
    cache->didReceiveServerRedirectForProvisionalLoadForFrameFunc = getMethod(delegate, @selector(webView:didReceiveServerRedirectForProvisionalLoadForFrame:));
    cache->didReceiveTitleForFrameFunc = getMethod(delegate, @selector(webView:didReceiveTitle:forFrame:));
    cache->didStartProvisionalLoadForFrameFunc = getMethod(delegate, @selector(webView:didStartProvisionalLoadForFrame:));
    cache->willCloseFrameFunc = getMethod(delegate, @selector(webView:willCloseFrame:));
    cache->willPerformClientRedirectToURLDelayFireDateForFrameFunc = getMethod(delegate, @selector(webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:));
    cache->windowScriptObjectAvailableFunc = getMethod(delegate, @selector(webView:windowScriptObjectAvailable:));
    cache->didDisplayInsecureContentFunc = getMethod(delegate, @selector(webViewDidDisplayInsecureContent:));
    cache->didRunInsecureContentFunc = getMethod(delegate, @selector(webView:didRunInsecureContent:));
    cache->didDetectXSSFunc = getMethod(delegate, @selector(webView:didDetectXSS:));
    cache->didRemoveFrameFromHierarchyFunc = getMethod(delegate, @selector(webView:didRemoveFrameFromHierarchy:));
#if PLATFORM(IOS_FAMILY)
    cache->webThreadDidLayoutFunc = getMethod(delegate, @selector(webThreadWebView:didLayout:));
#endif

    // It would be nice to get rid of this code and transition all clients to using didLayout instead of
    // didFirstLayoutInFrame and didFirstVisuallyNonEmptyLayoutInFrame. In the meantime, this is required
    // for backwards compatibility.
    auto* page = core(self);
    if (page) {
        OptionSet<WebCore::LayoutMilestone> milestones { WebCore::DidFirstLayout };
#if PLATFORM(IOS_FAMILY)
        milestones.add(WebCore::DidFirstVisuallyNonEmptyLayout);
#else
        if (cache->didFirstVisuallyNonEmptyLayoutInFrameFunc)
            milestones.add(WebCore::DidFirstVisuallyNonEmptyLayout);
#endif
        page->addLayoutMilestones(milestones);
    }
}

- (void)_cacheScriptDebugDelegateImplementations
{
    WebScriptDebugDelegateImplementationCache *cache = &_private->scriptDebugDelegateImplementations;
    id delegate = _private->scriptDebugDelegate;

    if (!delegate) {
        bzero(cache, sizeof(WebScriptDebugDelegateImplementationCache));
        return;
    }

    cache->didParseSourceFunc = getMethod(delegate, @selector(webView:didParseSource:baseLineNumber:fromURL:sourceId:forWebFrame:));
    if (cache->didParseSourceFunc)
        cache->didParseSourceExpectsBaseLineNumber = YES;
    else {
        cache->didParseSourceExpectsBaseLineNumber = NO;
        cache->didParseSourceFunc = getMethod(delegate, @selector(webView:didParseSource:fromURL:sourceId:forWebFrame:));
    }

    cache->failedToParseSourceFunc = getMethod(delegate, @selector(webView:failedToParseSource:baseLineNumber:fromURL:withError:forWebFrame:));

    cache->exceptionWasRaisedFunc = getMethod(delegate, @selector(webView:exceptionWasRaised:hasHandler:sourceId:line:forWebFrame:));
    if (cache->exceptionWasRaisedFunc)
        cache->exceptionWasRaisedExpectsHasHandlerFlag = YES;
    else {
        cache->exceptionWasRaisedExpectsHasHandlerFlag = NO;
        cache->exceptionWasRaisedFunc = getMethod(delegate, @selector(webView:exceptionWasRaised:sourceId:line:forWebFrame:));
    }
}

- (void)_cacheHistoryDelegateImplementations
{
    WebHistoryDelegateImplementationCache *cache = &_private->historyDelegateImplementations;
    id delegate = _private->historyDelegate;

    if (!delegate) {
        bzero(cache, sizeof(WebHistoryDelegateImplementationCache));
        return;
    }

    cache->navigatedFunc = getMethod(delegate, @selector(webView:didNavigateWithNavigationData:inFrame:));
    cache->clientRedirectFunc = getMethod(delegate, @selector(webView:didPerformClientRedirectFromURL:toURL:inFrame:));
    cache->serverRedirectFunc = getMethod(delegate, @selector(webView:didPerformServerRedirectFromURL:toURL:inFrame:));
IGNORE_WARNINGS_BEGIN("undeclared-selector")
    cache->deprecatedSetTitleFunc = getMethod(delegate, @selector(webView:updateHistoryTitle:forURL:));
IGNORE_WARNINGS_END
    cache->setTitleFunc = getMethod(delegate, @selector(webView:updateHistoryTitle:forURL:inFrame:));
    cache->populateVisitedLinksFunc = getMethod(delegate, @selector(populateVisitedLinksForWebView:));
}

- (id)_policyDelegateForwarder
{
#if PLATFORM(IOS_FAMILY)
    if (_private->closing)
        return nil;
#endif
    if (!_private->policyDelegateForwarder)
        _private->policyDelegateForwarder = adoptNS([[_WebSafeForwarder alloc] initWithTarget:_private->policyDelegate defaultTarget:[WebDefaultPolicyDelegate sharedPolicyDelegate]]);
    return _private->policyDelegateForwarder.get();
}

- (id)_UIDelegateForwarder
{
#if PLATFORM(IOS_FAMILY)
    if (_private->closing)
        return nil;
#endif
    if (!_private->UIDelegateForwarder)
        _private->UIDelegateForwarder = adoptNS([[_WebSafeForwarder alloc] initWithTarget:_private->UIDelegate defaultTarget:[WebDefaultUIDelegate sharedUIDelegate]]);
    return _private->UIDelegateForwarder.get();
}

#if PLATFORM(IOS_FAMILY)
- (id)_UIDelegateForSelector:(SEL)selector
{
    id delegate = self->_private->UIDelegate;
    if ([delegate respondsToSelector:selector])
        return delegate;

    delegate = [WebDefaultUIDelegate sharedUIDelegate];
    if ([delegate respondsToSelector:selector])
        return delegate;

    return nil;
}
#endif

- (id)_editingDelegateForwarder
{
#if PLATFORM(IOS_FAMILY)
    if (_private->closing)
        return nil;
#endif
    // This can be called during window deallocation by QTMovieView in the QuickTime Cocoa Plug-in.
    // Not sure if that is a bug or not.
    if (!_private)
        return nil;

    if (!_private->editingDelegateForwarder)
        _private->editingDelegateForwarder = adoptNS([[_WebSafeForwarder alloc] initWithTarget:_private->editingDelegate defaultTarget:[WebDefaultEditingDelegate sharedEditingDelegate]]);
    return _private->editingDelegateForwarder.get();
}

+ (void)_unregisterViewClassAndRepresentationClassForMIMEType:(NSString *)MIMEType
{
    [[WebFrameView _viewTypesAllowImageTypeOmission:NO] removeObjectForKey:MIMEType];
    [[WebDataSource _repTypesAllowImageTypeOmission:NO] removeObjectForKey:MIMEType];

    // FIXME: We also need to maintain MIMEType registrations (which can be dynamically changed)
    // in the WebCore MIMEType registry.  For now we're doing this in a safe, limited manner
    // to fix <rdar://problem/5372989> - a future revamping of the entire system is neccesary for future robustness
    WebCore::MIMETypeRegistry::supportedNonImageMIMETypes().remove(MIMEType);
}

+ (void)_registerViewClass:(Class)viewClass representationClass:(Class)representationClass forURLScheme:(NSString *)URLScheme
{
    NSString *MIMEType = [self _generatedMIMETypeForURLScheme:URLScheme];
    [self registerViewClass:viewClass representationClass:representationClass forMIMEType:MIMEType];

    // FIXME: We also need to maintain MIMEType registrations (which can be dynamically changed)
    // in the WebCore MIMEType registry.  For now we're doing this in a safe, limited manner
    // to fix <rdar://problem/5372989> - a future revamping of the entire system is neccesary for future robustness
    if ([viewClass class] == [WebHTMLView class])
        WebCore::MIMETypeRegistry::supportedNonImageMIMETypes().add(MIMEType);

    // This is used to make _representationExistsForURLScheme faster.
    // Without this set, we'd have to create the MIME type each time.
    auto& schemes = schemesWithRepresentationsSet();
    if (!schemes)
        schemes = adoptNS([[NSMutableSet alloc] init]);
    [schemes addObject:adoptNS([[URLScheme lowercaseString] copy]).get()];
}

+ (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme
{
    return [@"x-apple-web-kit/" stringByAppendingString:[URLScheme lowercaseString]];
}

+ (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme
{
    return [schemesWithRepresentationsSet() containsObject:[URLScheme lowercaseString]];
}

+ (BOOL)_canHandleRequest:(NSURLRequest *)request forMainFrame:(BOOL)forMainFrame
{
    // FIXME: If <rdar://problem/5217309> gets fixed, this check can be removed.
    if (!request)
        return NO;

    if ([NSURLConnection canHandleRequest:request])
        return YES;

    NSString *scheme = [[request URL] scheme];

    // Representations for URL schemes work at the top level.
    if (forMainFrame && [self _representationExistsForURLScheme:scheme])
        return YES;

    if ([scheme _webkit_isCaseInsensitiveEqualToString:@"applewebdata"])
        return YES;

    if ([scheme _webkit_isCaseInsensitiveEqualToString:@"blob"])
        return YES;

    return NO;
}

+ (BOOL)_canHandleRequest:(NSURLRequest *)request
{
    return [self _canHandleRequest:request forMainFrame:YES];
}

+ (NSString *)_decodeData:(NSData *)data
{
    WebCore::HTMLNames::init(); // this method is used for importing bookmarks at startup, so HTMLNames are likely to be uninitialized yet
    return WebCore::TextResourceDecoder::create("text/html")->decodeAndFlush(static_cast<const char*>([data bytes]), [data length]); // bookmark files are HTML
}

- (void)_pushPerformingProgrammaticFocus
{
    _private->programmaticFocusCount++;
}

- (void)_popPerformingProgrammaticFocus
{
    _private->programmaticFocusCount--;
}

- (BOOL)_isPerformingProgrammaticFocus
{
    return _private->programmaticFocusCount != 0;
}

#if !PLATFORM(IOS_FAMILY)
- (void)_didChangeValueForKey: (NSString *)key
{
    LOG (Bindings, "calling didChangeValueForKey: %@", key);
    [self didChangeValueForKey: key];
}

- (void)_willChangeValueForKey: (NSString *)key
{
    LOG (Bindings, "calling willChangeValueForKey: %@", key);
    [self willChangeValueForKey: key];
}

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
    static NeverDestroyed<RetainPtr<NSSet>> manualNotifyKeys = adoptNS([[NSSet alloc] initWithObjects:_WebMainFrameURLKey, _WebIsLoadingKey, _WebEstimatedProgressKey,
        _WebCanGoBackKey, _WebCanGoForwardKey, _WebMainFrameTitleKey, _WebMainFrameIconKey, _WebMainFrameDocumentKey, nil]);
    if ([manualNotifyKeys.get() containsObject:key])
        return NO;
    return YES;
}

- (NSArray *)_declaredKeys {
    static NeverDestroyed<RetainPtr<NSArray>> declaredKeys = @[
        _WebMainFrameURLKey, _WebIsLoadingKey, _WebEstimatedProgressKey,
        _WebCanGoBackKey, _WebCanGoForwardKey, _WebMainFrameTitleKey,
        _WebMainFrameIconKey, _WebMainFrameDocumentKey
    ];
    return declaredKeys.get().get();
}

- (void)_willChangeBackForwardKeys
{
    [self _willChangeValueForKey: _WebCanGoBackKey];
    [self _willChangeValueForKey: _WebCanGoForwardKey];
}

- (void)_didChangeBackForwardKeys
{
    [self _didChangeValueForKey: _WebCanGoBackKey];
    [self _didChangeValueForKey: _WebCanGoForwardKey];
}

- (void)_didStartProvisionalLoadForFrame:(WebFrame *)frame
{
    if (needsSelfRetainWhileLoadingQuirk())
        [self retain];

    [self _willChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];

        [self _willChangeValueForKey: _WebMainFrameURLKey];

        [self hideFormValidationMessage];
    }

    [NSApp setWindowsNeedUpdate:YES];

#if ENABLE(FULLSCREEN_API)
    auto* document = core([frame DOMDocument]);
    if (auto* element = document ? document->fullscreenManager().currentFullscreenElement() : 0) {
        SEL selector = @selector(webView:closeFullScreenWithListener:);
        if ([_private->UIDelegate respondsToSelector:selector]) {
            auto listener = adoptNS([[WebKitFullScreenListener alloc] initWithElement:element]);
            CallUIDelegate(self, selector, listener.get());
        } else if (_private->newFullscreenController && [_private->newFullscreenController isFullScreen]) {
            [_private->newFullscreenController close];
        }
    }
#endif
}

- (void)_checkDidPerformFirstNavigation
{
    if (_private->_didPerformFirstNavigation)
        return;

    auto* page = _private->page;
    if (!page)
        return;

    auto& backForwardController = page->backForward();

    if (!backForwardController.backItem())
        return;

    _private->_didPerformFirstNavigation = YES;

    if ([_private->preferences automaticallyDetectsCacheModel] && [_private->preferences cacheModel] < WebCacheModelDocumentBrowser)
        [_private->preferences setCacheModel:WebCacheModelDocumentBrowser];
}

- (void)_didCommitLoadForFrame:(WebFrame *)frame
{
    if (frame == [self mainFrame])
        [self _didChangeValueForKey: _WebMainFrameURLKey];

    [self _checkDidPerformFirstNavigation];

    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFinishLoadForFrame:(WebFrame *)frame
{
    if (needsSelfRetainWhileLoadingQuirk())
        [self performSelector:@selector(release) withObject:nil afterDelay:0];

    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    if (needsSelfRetainWhileLoadingQuirk())
        [self performSelector:@selector(release) withObject:nil afterDelay:0];

    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    if (needsSelfRetainWhileLoadingQuirk())
        [self performSelector:@selector(release) withObject:nil afterDelay:0];

    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];

        [self _didChangeValueForKey: _WebMainFrameURLKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (NSCachedURLResponse *)_cachedResponseForURL:(NSURL *)URL
{
    RetainPtr<NSMutableURLRequest> request = adoptNS([[NSMutableURLRequest alloc] initWithURL:URL]);
    [request _web_setHTTPUserAgent:[self userAgentForURL:URL]];
    NSCachedURLResponse *cachedResponse;

    if (!_private->page)
        return nil;

    if (auto storageSession = _private->page->mainFrame().loader().networkingContext()->storageSession()->platformSession())
        cachedResponse = WebCore::cachedResponseForRequest(storageSession, request.get());
    else
        cachedResponse = [[NSURLCache sharedURLCache] cachedResponseForRequest:request.get()];

    return cachedResponse;
}

- (void)_writeImageForElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    NSURL *linkURL = [element objectForKey:WebElementLinkURLKey];
    DOMElement *domElement = [element objectForKey:WebElementDOMNodeKey];
    [pasteboard _web_writeImage:(NSImage *)(domElement ? nil : [element objectForKey:WebElementImageKey])
                        element:domElement
                            URL:linkURL ? linkURL : (NSURL *)[element objectForKey:WebElementImageURLKey]
                          title:[element objectForKey:WebElementImageAltStringKey]
                        archive:[[element objectForKey:WebElementDOMNodeKey] webArchive]
                          types:types
                         source:nil];
}

- (void)_writeLinkElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    [pasteboard _web_writeURL:[element objectForKey:WebElementLinkURLKey]
                     andTitle:[element objectForKey:WebElementLinkLabelKey]
                        types:types];
}

#if ENABLE(DRAG_SUPPORT)
- (void)_setInitiatedDrag:(BOOL)initiatedDrag
{
    if (!_private->page)
        return;
    _private->page->dragController().setDidInitiateDrag(initiatedDrag);
}
#endif

#else

- (void)_didCommitLoadForFrame:(WebFrame *)frame
{
    if (frame == [self mainFrame])
        _private->didDrawTiles = 0;
}

#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(DASHBOARD_SUPPORT)

// FIXME: Remove these once it is verified no one is dependent on it.

- (void)_addScrollerDashboardRegions:(NSMutableDictionary *)regions
{
}

- (NSDictionary *)_dashboardRegions
{
    return nil;
}

- (void)_setDashboardBehavior:(WebDashboardBehavior)behavior to:(BOOL)flag
{
}

- (BOOL)_dashboardBehavior:(WebDashboardBehavior)behavior
{
    return NO;
}

#endif /* ENABLE(DASHBOARD_SUPPORT) */

+ (void)_setShouldUseFontSmoothing:(BOOL)f
{
    WebCore::FontCascade::setShouldUseSmoothing(f);
}

+ (BOOL)_shouldUseFontSmoothing
{
    return WebCore::FontCascade::shouldUseSmoothing();
}

#if !PLATFORM(IOS_FAMILY)
+ (void)_setUsesTestModeFocusRingColor:(BOOL)f
{
    WebCore::setUsesTestModeFocusRingColor(f);
}

+ (BOOL)_usesTestModeFocusRingColor
{
    return WebCore::usesTestModeFocusRingColor();
}
#endif

#if PLATFORM(IOS_FAMILY)
- (void)_setUIKitDelegate:(id)delegate
{
    _private->UIKitDelegate = delegate;
    [_private->UIKitDelegateForwarder clearTarget];
    _private->UIKitDelegateForwarder = nil;
}

- (id)_UIKitDelegate
{
    return _private->UIKitDelegate;
}

- (void)setWebMailDelegate:(id)delegate
{
    _private->WebMailDelegate = delegate;
}

- (id)_webMailDelegate
{
    return _private->WebMailDelegate;
}

- (id <WebCaretChangeListener>)caretChangeListener
{
    return _private->_caretChangeListener;
}

- (void)setCaretChangeListener:(id <WebCaretChangeListener>)listener
{
    _private->_caretChangeListener = listener;
}

- (NSSet *)caretChangeListeners
{
    return _private->_caretChangeListeners.get();
}

- (void)addCaretChangeListener:(id <WebCaretChangeListener>)listener
{
    if (_private->_caretChangeListeners == nil) {
        _private->_caretChangeListeners = adoptNS([[NSMutableSet alloc] init]);
    }

    [_private->_caretChangeListeners addObject:listener];
}

- (void)removeCaretChangeListener:(id <WebCaretChangeListener>)listener
{
    [_private->_caretChangeListeners removeObject:listener];
}

- (void)removeAllCaretChangeListeners
{
    [_private->_caretChangeListeners removeAllObjects];
}

- (void)caretChanged
{
    [_private->_caretChangeListener caretChanged];
    auto copy = adoptNS([_private->_caretChangeListeners copy]);
    for (id <WebCaretChangeListener> listener in copy.get()) {
        [listener caretChanged];
    }
}

- (void)_clearDelegates
{
    ASSERT(WebThreadIsLocked() || !WebThreadIsEnabled());

    [self _setFormDelegate:nil];
    [self _setUIKitDelegate:nil];
    [self setCaretChangeListener:nil];
    [self removeAllCaretChangeListeners];
    [self setWebMailDelegate:nil];

    [self setDownloadDelegate:nil];
    [self setEditingDelegate:nil];
    [self setFrameLoadDelegate:nil];
    [self setPolicyDelegate:nil];
    [self setResourceLoadDelegate:nil];
    [self setScriptDebugDelegate:nil];
    [self setUIDelegate:nil];
}

- (NSURL *)_displayURL
{
    WebThreadLock();
    WebFrame *frame = [self mainFrame];
    // FIXME: <rdar://problem/6362369> We used to get provisionalDataSource here if in provisional state; how do we tell that now? Is this right?
    WebDataSource *dataSource = [frame provisionalDataSource];
    if (!dataSource)
        dataSource = [frame dataSource];
    auto unreachableURL = retainPtr([dataSource unreachableURL]);
    auto url = unreachableURL != nil ? unreachableURL : retainPtr([[dataSource request] URL]);
    return url.autorelease();
}
#endif // PLATFORM(IOS_FAMILY)

#if !PLATFORM(IOS_FAMILY)
- (void)setAlwaysShowVerticalScroller:(BOOL)flag
{
    WebDynamicScrollBarsView *scrollview = [[[self mainFrame] frameView] _scrollView];
    if (flag) {
        [scrollview setVerticalScrollingMode:WebCore::ScrollbarAlwaysOn andLock:YES];
    } else {
        [scrollview setVerticalScrollingModeLocked:NO];
        [scrollview setVerticalScrollingMode:WebCore::ScrollbarAuto andLock:NO];
    }
}

- (BOOL)alwaysShowVerticalScroller
{
    WebDynamicScrollBarsView *scrollview = [[[self mainFrame] frameView] _scrollView];
    return [scrollview verticalScrollingModeLocked] && [scrollview verticalScrollingMode] == WebCore::ScrollbarAlwaysOn;
}

- (void)setAlwaysShowHorizontalScroller:(BOOL)flag
{
    WebDynamicScrollBarsView *scrollview = [[[self mainFrame] frameView] _scrollView];
    if (flag) {
        [scrollview setHorizontalScrollingMode:WebCore::ScrollbarAlwaysOn andLock:YES];
    } else {
        [scrollview setHorizontalScrollingModeLocked:NO];
        [scrollview setHorizontalScrollingMode:WebCore::ScrollbarAuto andLock:NO];
    }
}

- (void)setProhibitsMainFrameScrolling:(BOOL)prohibits
{
    if (auto* mainFrame = [self _mainCoreFrame])
        mainFrame->view()->setProhibitsScrolling(prohibits);
}

- (BOOL)alwaysShowHorizontalScroller
{
    WebDynamicScrollBarsView *scrollview = [[[self mainFrame] frameView] _scrollView];
    return [scrollview horizontalScrollingModeLocked] && [scrollview horizontalScrollingMode] == WebCore::ScrollbarAlwaysOn;
}
#endif // !PLATFORM(IOS_FAMILY)

- (void)_setUseFastImageScalingMode:(BOOL)flag
{
    if (_private->page && _private->page->inLowQualityImageInterpolationMode() != flag) {
        _private->page->setInLowQualityImageInterpolationMode(flag);
        [self setNeedsDisplay:YES];
    }
}

- (BOOL)_inFastImageScalingMode
{
    if (_private->page)
        return _private->page->inLowQualityImageInterpolationMode();
    return NO;
}

- (BOOL)_cookieEnabled
{
    if (_private->page)
        return _private->page->settings().cookieEnabled();
    return YES;
}

- (void)_setCookieEnabled:(BOOL)enable
{
    if (_private->page)
        _private->page->settings().setCookieEnabled(enable);
}

#if !PLATFORM(IOS_FAMILY)
- (void)_setAdditionalWebPlugInPaths:(NSArray *)newPaths
{
    if (!_private->pluginDatabase)
        _private->pluginDatabase = adoptNS([[WebPluginDatabase alloc] init]);

    [_private->pluginDatabase setPlugInPaths:newPaths];
    [_private->pluginDatabase refresh];
}
#endif

#if PLATFORM(IOS_FAMILY)
- (BOOL)_locked_plugInsAreRunningInFrame:(WebFrame *)frame
{
    // Ask plug-ins in this frame
    id <WebDocumentView> documentView = [[frame frameView] documentView];
    if (documentView && [documentView isKindOfClass:[WebHTMLView class]]) {
        if ([[(WebHTMLView *)documentView _pluginController] plugInsAreRunning])
            return YES;
    }

    // Ask plug-ins in child frames
    NSArray *childFrames = [frame childFrames];
    unsigned childCount = [childFrames count];
    unsigned childIndex;
    for (childIndex = 0; childIndex < childCount; childIndex++) {
        if ([self _locked_plugInsAreRunningInFrame:[childFrames objectAtIndex:childIndex]])
            return YES;
    }

    return NO;
}

- (BOOL)_pluginsAreRunning
{
    WebThreadLock();
    return [self _locked_plugInsAreRunningInFrame:[self mainFrame]];
}

- (void)_locked_recursivelyPerformPlugInSelector:(SEL)selector inFrame:(WebFrame *)frame
{
    // Send the message to plug-ins in this frame
    id <WebDocumentView> documentView = [[frame frameView] documentView];
    if (documentView && [documentView isKindOfClass:[WebHTMLView class]])
        [[(WebHTMLView *)documentView _pluginController] performSelector:selector];

    // Send the message to plug-ins in child frames
    NSArray *childFrames = [frame childFrames];
    unsigned childCount = [childFrames count];
    unsigned childIndex;
    for (childIndex = 0; childIndex < childCount; childIndex++) {
        [self _locked_recursivelyPerformPlugInSelector:selector inFrame:[childFrames objectAtIndex:childIndex]];
    }
}

- (void)_destroyAllPlugIns
{
    WebThreadLock();
    [self _locked_recursivelyPerformPlugInSelector:@selector(destroyAllPlugins) inFrame:[self mainFrame]];
}

- (void)_startAllPlugIns
{
    WebThreadLock();
    [self _locked_recursivelyPerformPlugInSelector:@selector(startAllPlugins) inFrame:[self mainFrame]];
}

- (void)_stopAllPlugIns
{
    WebThreadLock();
    [self _locked_recursivelyPerformPlugInSelector:@selector(stopAllPlugins) inFrame:[self mainFrame]];
}

- (void)_stopAllPlugInsForPageCache
{
    WebThreadLock();
    [self _locked_recursivelyPerformPlugInSelector:@selector(stopPluginsForPageCache) inFrame:[self mainFrame]];
}

- (void)_restorePlugInsFromCache
{
    WebThreadLock();
    [self _locked_recursivelyPerformPlugInSelector:@selector(restorePluginsFromCache) inFrame:[self mainFrame]];
}

- (BOOL)_setMediaLayer:(CALayer*)layer forPluginView:(NSView*)pluginView
{
    WebThreadLock();

    auto* mainCoreFrame = [self _mainCoreFrame];
    for (auto* frame = mainCoreFrame; frame; frame = frame->tree().traverseNext()) {
        auto* coreView = frame ? frame->view() : 0;
        if (!coreView)
            continue;

        // Get the GraphicsLayer for this widget.
        WebCore::GraphicsLayer* layerForWidget = coreView->graphicsLayerForPlatformWidget(pluginView);
        if (!layerForWidget)
            continue;

        if (layerForWidget->contentsLayerForMedia() != layer) {
            layerForWidget->setContentsToPlatformLayer(layer, WebCore::GraphicsLayer::ContentsLayerPurpose::Media);
            // We need to make sure the layer hierarchy change is applied immediately.
            if (mainCoreFrame->view())
                mainCoreFrame->view()->flushCompositingStateIncludingSubframes();
            return YES;
        }
    }

    return NO;
}

- (void)_setNeedsUnrestrictedGetMatchedCSSRules:(BOOL)flag
{
    if (_private->page)
        _private->page->settings().setCrossOriginCheckInGetMatchedCSSRulesDisabled(flag);
}
#endif // PLATFORM(IOS_FAMILY)

- (void)_attachScriptDebuggerToAllFrames
{
    for (auto* frame = [self _mainCoreFrame]; frame; frame = frame->tree().traverseNext())
        [kit(frame) _attachScriptDebugger];
}

- (void)_detachScriptDebuggerFromAllFrames
{
    for (auto* frame = [self _mainCoreFrame]; frame; frame = frame->tree().traverseNext())
        [kit(frame) _detachScriptDebugger];
}

#if !PLATFORM(IOS_FAMILY)
- (void)setBackgroundColor:(NSColor *)backgroundColor
{
    if ([_private->backgroundColor isEqual:backgroundColor])
        return;

    _private->backgroundColor = backgroundColor;

    [[self mainFrame] _updateBackgroundAndUpdatesWhileOffscreen];
}

- (NSColor *)backgroundColor
{
    return _private->backgroundColor.get();
}
#else
- (void)setBackgroundColor:(CGColorRef)backgroundColor
{
    if (!backgroundColor || CFEqual(_private->backgroundColor.get(), backgroundColor))
        return;

    _private->backgroundColor = backgroundColor;

    [[self mainFrame] _updateBackgroundAndUpdatesWhileOffscreen];
}

- (CGColorRef)backgroundColor
{
    return _private->backgroundColor.get();
}
#endif

- (BOOL)defersCallbacks
{
    if (!_private->page)
        return NO;
    return _private->page->defersLoading();
}

- (void)setDefersCallbacks:(BOOL)defer
{
    if (!_private->page)
        return;
    return _private->page->setDefersLoading(defer);
}

#if PLATFORM(IOS_FAMILY)
- (NSDictionary *)quickLookContentForURL:(NSURL *)url
{
    return nil;
}

- (BOOL)_isStopping
{
    return _private->isStopping;
}

- (BOOL)_isClosing
{
    return _private->closing;
}

#if ENABLE(ORIENTATION_EVENTS)
- (void)_setDeviceOrientation:(NSUInteger)orientation
{
    _private->deviceOrientation = orientation;
}

- (NSUInteger)_deviceOrientation
{
    return _private->deviceOrientation;
}
#endif

+ (NSArray *)_productivityDocumentMIMETypes
{
#if USE(QUICK_LOOK)
    return [WebCore::QLPreviewGetSupportedMIMETypesSet() allObjects];
#else
    return nil;
#endif
}

- (void)_setAllowsMessaging:(BOOL)aFlag
{
    _private->allowsMessaging = aFlag;
}

- (BOOL)_allowsMessaging
{
    return _private->allowsMessaging;
}

- (void)_setFixedLayoutSize:(CGSize)size
{
    ASSERT(WebThreadIsLocked());
    _private->fixedLayoutSize = size;
    if (auto* mainFrame = core([self mainFrame])) {
        WebCore::IntSize newSize(size);
        mainFrame->view()->setFixedLayoutSize(newSize);
        mainFrame->view()->setUseFixedLayout(!newSize.isEmpty());
        [self setNeedsDisplay:YES];
    }
}

- (CGSize)_fixedLayoutSize
{
    return _private->fixedLayoutSize;
}

- (void)_synchronizeCustomFixedPositionLayoutRect
{
    ASSERT(WebThreadIsLocked());

    WebCore::IntRect newRect;
    {
        LockHolder locker(_private->pendingFixedPositionLayoutRectMutex);
        if (CGRectIsNull(_private->pendingFixedPositionLayoutRect))
            return;
        newRect = WebCore::enclosingIntRect(_private->pendingFixedPositionLayoutRect);
        _private->pendingFixedPositionLayoutRect = CGRectNull;
    }

    if (auto* mainFrame = core([self mainFrame]))
        mainFrame->view()->setCustomFixedPositionLayoutRect(newRect);
}

- (void)_setCustomFixedPositionLayoutRectInWebThread:(CGRect)rect synchronize:(BOOL)synchronize
{
    {
        LockHolder locker(_private->pendingFixedPositionLayoutRectMutex);
        _private->pendingFixedPositionLayoutRect = rect;
    }
    if (!synchronize)
        return;
    WebThreadRun(^{
        [self _synchronizeCustomFixedPositionLayoutRect];
    });
}

- (void)_setCustomFixedPositionLayoutRect:(CGRect)rect
{
    ASSERT(WebThreadIsLocked());
    {
        LockHolder locker(_private->pendingFixedPositionLayoutRectMutex);
        _private->pendingFixedPositionLayoutRect = rect;
    }
    [self _synchronizeCustomFixedPositionLayoutRect];
}

- (BOOL)_fetchCustomFixedPositionLayoutRect:(NSRect*)rect
{
    LockHolder locker(_private->pendingFixedPositionLayoutRectMutex);
    if (CGRectIsNull(_private->pendingFixedPositionLayoutRect))
        return false;

    *rect = _private->pendingFixedPositionLayoutRect;
    _private->pendingFixedPositionLayoutRect = CGRectNull;
    return true;
}

- (void)_viewGeometryDidChange
{
    ASSERT(WebThreadIsLocked());

    if (auto* coreFrame = [self _mainCoreFrame])
        coreFrame->viewportOffsetChanged(WebCore::Frame::IncrementalScrollOffset);
}

- (void)_overflowScrollPositionChangedTo:(CGPoint)offset forNode:(DOMNode *)domNode isUserScroll:(BOOL)userScroll
{
    // Find the frame
    auto* node = core(domNode);
    auto* frame = node->document().frame();
    if (!frame)
        return;

    frame->overflowScrollPositionChangedForNode(WebCore::roundedIntPoint(offset), node, userScroll);
}

+ (void)_doNotStartObservingNetworkReachability
{
    WebCore::DeprecatedGlobalSettings::setShouldOptOutOfNetworkStateObservation(true);
}
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(TOUCH_EVENTS)

- (NSArray *)_touchEventRegions
{
    auto* frame = [self _mainCoreFrame];
    if (!frame)
        return nil;

    auto* document = frame->document();
    if (!document)
        return nil;

    Vector<WebCore::IntRect> rects;
    document->getTouchRects(rects);

    if (rects.isEmpty())
        return nil;

    NSView <WebDocumentView> *documentView = [[[self mainFrame] frameView] documentView];
    return createNSArray(rects, [&] (auto& rect) -> RetainPtr<WebEventRegion> {
        if (rect.isEmpty())
            return nil;

        // The touch rectangles are in the coordinate system of the document (inside the WebHTMLView), which is not
        // the same as the coordinate system of the WebView. UIWebView currently expects view coordinates, so we'll
        // convert them here now.
        auto viewRect = [documentView convertRect:rect toView:self];

        // The event region wants this points in this order:
        //  p2------p3
        //  |       |
        //  p1------p4

        auto p1 = CGPointMake(CGRectGetMinX(viewRect), CGRectGetMaxY(viewRect));
        auto p2 = CGPointMake(CGRectGetMinX(viewRect), CGRectGetMinY(viewRect));
        auto p3 = CGPointMake(CGRectGetMaxX(viewRect), CGRectGetMinY(viewRect));
        auto p4 = CGPointMake(CGRectGetMaxX(viewRect), CGRectGetMaxY(viewRect));

        return adoptNS([[WebEventRegion alloc] initWithPoints:p1 :p2 :p3 :p4]);
    }).autorelease();
}

#endif // ENABLE(TOUCH_EVENTS)

// For backwards compatibility with the WebBackForwardList API, we honor both
// a per-WebView and a per-preferences setting for whether to use the back/forward cache.

- (BOOL)usesPageCache
{
    return _private->usesPageCache && [[self preferences] usesPageCache];
}

- (void)setUsesPageCache:(BOOL)usesPageCache
{
    _private->usesPageCache = usesPageCache;

    // Update our own settings and post the public notification only
    [self _preferencesChanged:[self preferences]];
    [[self preferences] _postPreferencesChangedAPINotification];
}

- (WebTextIterator *)textIteratorForRect:(NSRect)rect
{
    auto* coreFrame = [self _mainCoreFrame];
    if (!coreFrame)
        return nil;

    auto intRect = WebCore::enclosingIntRect(rect);
    auto range = WebCore::VisibleSelection(coreFrame->visiblePositionForPoint(intRect.minXMinYCorner()),
        coreFrame->visiblePositionForPoint(intRect.maxXMaxYCorner())).toNormalizedRange();
    return adoptNS([[WebTextIterator alloc] initWithRange:kit(range)]).autorelease();
}

#if !PLATFORM(IOS_FAMILY)
- (void)_clearUndoRedoOperations
{
    if (!_private->page)
        return;
    _private->page->clearUndoRedoOperations();
}
#endif

- (void)_executeCoreCommandByName:(NSString *)name value:(NSString *)value
{
    if (auto* page = _private->page)
        page->focusController().focusedOrMainFrame().editor().command(name).execute(value);
}

- (void)_clearMainFrameName
{
    _private->page->mainFrame().tree().clearName();
}

- (void)setSelectTrailingWhitespaceEnabled:(BOOL)flag
{
    if (_private->page->settings().selectTrailingWhitespaceEnabled() != flag) {
        _private->page->settings().setSelectTrailingWhitespaceEnabled(flag);
        [self setSmartInsertDeleteEnabled:!flag];
    }
}

- (BOOL)isSelectTrailingWhitespaceEnabled
{
    return _private->page->settings().selectTrailingWhitespaceEnabled();
}

- (void)setMemoryCacheDelegateCallsEnabled:(BOOL)enabled
{
    _private->page->setMemoryCacheClientCallsEnabled(enabled);
}

- (BOOL)areMemoryCacheDelegateCallsEnabled
{
    return _private->page->areMemoryCacheClientCallsEnabled();
}

- (BOOL)_postsAcceleratedCompositingNotifications
{
    return _private->postsAcceleratedCompositingNotifications;
}
- (void)_setPostsAcceleratedCompositingNotifications:(BOOL)flag
{
    _private->postsAcceleratedCompositingNotifications = flag;
}

- (BOOL)_isUsingAcceleratedCompositing
{
    auto* coreFrame = [self _mainCoreFrame];
    for (auto* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        NSView *documentView = [[kit(frame) frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]] && [(WebHTMLView *)documentView _isUsingAcceleratedCompositing])
            return YES;
    }

    return NO;
}

- (void)_setBaseCTM:(CGAffineTransform)transform forContext:(CGContextRef)context
{
    CGContextSetBaseCTM(context, transform);
}

- (BOOL)interactiveFormValidationEnabled
{
    return _private->interactiveFormValidationEnabled;
}

- (void)setInteractiveFormValidationEnabled:(BOOL)enabled
{
    _private->interactiveFormValidationEnabled = enabled;
}

- (int)validationMessageTimerMagnification
{
    return _private->validationMessageTimerMagnification;
}

- (void)setValidationMessageTimerMagnification:(int)newValue
{
    _private->validationMessageTimerMagnification = newValue;
}

- (NSDictionary *)_contentsOfUserInterfaceItem:(NSString *)userInterfaceItem
{
    if ([userInterfaceItem isEqualToString:@"validationBubble"]) {
        auto* validationBubble = _private->formValidationBubble.get();
        String message = validationBubble ? validationBubble->message() : emptyString();
        double fontSize = validationBubble ? validationBubble->fontSize() : 0;
        return @{ userInterfaceItem: @{ @"message": (NSString *)message, @"fontSize": @(fontSize) } };
    }

    return nil;
}

- (BOOL)_isSoftwareRenderable
{
    auto* coreFrame = [self _mainCoreFrame];
    for (auto* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        if (auto* view = frame->view()) {
            if (!view->isSoftwareRenderable())
                return NO;
        }
    }

    return YES;
}

- (void)setTracksRepaints:(BOOL)flag
{
    auto* coreFrame = [self _mainCoreFrame];
    if (auto* view = coreFrame->view())
        view->setTracksRepaints(flag);
}

- (BOOL)isTrackingRepaints
{
    auto* coreFrame = [self _mainCoreFrame];
    if (auto* view = coreFrame->view())
        return view->isTrackingRepaints();

    return NO;
}

- (void)resetTrackedRepaints
{
    auto* coreFrame = [self _mainCoreFrame];
    if (auto* view = coreFrame->view())
        view->resetTrackedRepaints();
}

- (NSArray *)trackedRepaintRects
{
    auto view = self._mainCoreFrame->view();
    if (!view || !view->isTrackingRepaints())
        return nil;
    return createNSArray(view->trackedRepaintRects(), [] (auto& rect) {
        return [NSValue valueWithRect:snappedIntRect(WebCore::LayoutRect { rect })];
    }).autorelease();
}

#if !PLATFORM(IOS_FAMILY)
- (NSPasteboard *)_insertionPasteboard
{
    return _private ? _private->insertionPasteboard : nil;
}
#endif

+ (void)_addOriginAccessAllowListEntryWithSourceOrigin:(NSString *)sourceOrigin destinationProtocol:(NSString *)destinationProtocol destinationHost:(NSString *)destinationHost allowDestinationSubdomains:(BOOL)allowDestinationSubdomains
{
    WebCore::SecurityPolicy::addOriginAccessAllowlistEntry(WebCore::SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

+ (void)_removeOriginAccessAllowListEntryWithSourceOrigin:(NSString *)sourceOrigin destinationProtocol:(NSString *)destinationProtocol destinationHost:(NSString *)destinationHost allowDestinationSubdomains:(BOOL)allowDestinationSubdomains
{
    WebCore::SecurityPolicy::removeOriginAccessAllowlistEntry(WebCore::SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

+ (void)_resetOriginAccessAllowLists
{
    WebCore::SecurityPolicy::resetOriginAccessAllowlists();
}

- (BOOL)_isViewVisible
{
    NSWindow *window = [self window];
    if (!window)
        return false;

    if (![window isVisible])
        return false;

    if ([self isHiddenOrHasHiddenAncestor])
        return false;

#if !PLATFORM(IOS_FAMILY)
    if (_private->windowOcclusionDetectionEnabled && (window.occlusionState & NSWindowOcclusionStateVisible) != NSWindowOcclusionStateVisible)
        return false;
#endif

    return true;
}

- (void)_updateVisibilityState
{
    if (_private && _private->page)
        [self _setIsVisible:[self _isViewVisible]];
}

- (void)_updateActiveState
{
    if (_private && _private->page)
#if PLATFORM(IOS_FAMILY)
        _private->page->focusController().setActive([[self window] isKeyWindow]);
#else
        _private->page->focusController().setActive([[self window] _hasKeyAppearance]);
#endif
}

+ (void)_addUserScriptToGroup:(NSString *)groupName world:(WebScriptWorld *)world source:(NSString *)source url:(NSURL *)url includeMatchPatternStrings:(NSArray *)includeMatchPatternStrings excludeMatchPatternStrings:(NSArray *)excludeMatchPatternStrings injectionTime:(WebUserScriptInjectionTime)injectionTime injectedFrames:(WebUserContentInjectedFrames)injectedFrames
{
    String group(groupName);
    if (group.isEmpty())
        return;

    auto viewGroup = WebViewGroup::getOrCreate(groupName, String());

    if (!world)
        return;

    auto userScript = makeUnique<WebCore::UserScript>(source, url, makeVector<String>(includeMatchPatternStrings), makeVector<String>(excludeMatchPatternStrings), injectionTime == WebInjectAtDocumentStart ? WebCore::UserScriptInjectionTime::DocumentStart : WebCore::UserScriptInjectionTime::DocumentEnd, injectedFrames == WebInjectInAllFrames ? WebCore::UserContentInjectedFrames::InjectInAllFrames : WebCore::UserContentInjectedFrames::InjectInTopFrameOnly, WebCore::WaitForNotificationBeforeInjecting::No);
    viewGroup->userContentController().addUserScript(*core(world), WTFMove(userScript));
}

+ (void)_addUserStyleSheetToGroup:(NSString *)groupName world:(WebScriptWorld *)world source:(NSString *)source url:(NSURL *)url includeMatchPatternStrings:(NSArray *)includeMatchPatternStrings excludeMatchPatternStrings:(NSArray *)excludeMatchPatternStrings injectedFrames:(WebUserContentInjectedFrames)injectedFrames
{
    String group(groupName);
    if (group.isEmpty())
        return;

    auto viewGroup = WebViewGroup::getOrCreate(groupName, String());

    if (!world)
        return;

    auto styleSheet = makeUnique<WebCore::UserStyleSheet>(source, url, makeVector<String>(includeMatchPatternStrings), makeVector<String>(excludeMatchPatternStrings), injectedFrames == WebInjectInAllFrames ? WebCore::UserContentInjectedFrames::InjectInAllFrames : WebCore::UserContentInjectedFrames::InjectInTopFrameOnly, WebCore::UserStyleUserLevel);
    viewGroup->userContentController().addUserStyleSheet(*core(world), WTFMove(styleSheet), WebCore::InjectInExistingDocuments);
}

+ (void)_removeUserScriptFromGroup:(NSString *)groupName world:(WebScriptWorld *)world url:(NSURL *)url
{
    String group(groupName);
    if (group.isEmpty())
        return;

    auto* viewGroup = WebViewGroup::get(group);
    if (!viewGroup)
        return;

    if (!world)
        return;

    viewGroup->userContentController().removeUserScript(*core(world), url);
}

+ (void)_removeUserStyleSheetFromGroup:(NSString *)groupName world:(WebScriptWorld *)world url:(NSURL *)url
{
    String group(groupName);
    if (group.isEmpty())
        return;

    auto* viewGroup = WebViewGroup::get(group);
    if (!viewGroup)
        return;

    if (!world)
        return;

    viewGroup->userContentController().removeUserStyleSheet(*core(world), url);
}

+ (void)_removeUserScriptsFromGroup:(NSString *)groupName world:(WebScriptWorld *)world
{
    String group(groupName);
    if (group.isEmpty())
        return;

    auto* viewGroup = WebViewGroup::get(group);
    if (!viewGroup)
        return;

    if (!world)
        return;

    viewGroup->userContentController().removeUserScripts(*core(world));
}

+ (void)_removeUserStyleSheetsFromGroup:(NSString *)groupName world:(WebScriptWorld *)world
{
    String group(groupName);
    if (group.isEmpty())
        return;

    auto* viewGroup = WebViewGroup::get(group);
    if (!viewGroup)
        return;

    if (!world)
        return;

    viewGroup->userContentController().removeUserStyleSheets(*core(world));
}

+ (void)_removeAllUserContentFromGroup:(NSString *)groupName
{
    String group(groupName);
    if (group.isEmpty())
        return;

    auto* viewGroup = WebViewGroup::get(group);
    if (!viewGroup)
        return;

    viewGroup->userContentController().removeAllUserContent();
}

- (void)_forceRepaintForTesting
{
    [self _updateRendering];
    [CATransaction flush];
    [CATransaction synchronize];
}

+ (void)_setDomainRelaxationForbidden:(BOOL)forbidden forURLScheme:(NSString *)scheme
{
    WebCore::LegacySchemeRegistry::setDomainRelaxationForbiddenForURLScheme(forbidden, scheme);
}

+ (void)_registerURLSchemeAsSecure:(NSString *)scheme
{
    WebCore::LegacySchemeRegistry::registerURLSchemeAsSecure(scheme);
}

+ (void)_registerURLSchemeAsAllowingLocalStorageAccessInPrivateBrowsing:(NSString *)scheme
{
}

+ (void)_registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsing:(NSString *)scheme
{
    WebCore::LegacySchemeRegistry::registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsing(scheme);
}

- (void)_scaleWebView:(float)scale atOrigin:(NSPoint)origin
{
    [self hideFormValidationMessage];

    _private->page->setPageScaleFactor(scale, WebCore::IntPoint(origin));
}

- (float)_viewScaleFactor
{
    return _private->page->pageScaleFactor();
}

- (void)_setUseFixedLayout:(BOOL)fixed
{
    auto* coreFrame = [self _mainCoreFrame];
    if (!coreFrame)
        return;

    auto* view = coreFrame->view();
    if (!view)
        return;

    view->setUseFixedLayout(fixed);
    if (!fixed)
        view->setFixedLayoutSize(WebCore::IntSize());
}

#if !PLATFORM(IOS_FAMILY)
- (void)_setFixedLayoutSize:(NSSize)size
{
    auto* coreFrame = [self _mainCoreFrame];
    if (!coreFrame)
        return;

    auto* view = coreFrame->view();
    if (!view)
        return;

    view->setFixedLayoutSize(WebCore::IntSize(size));
    view->forceLayout();
}
#endif

- (BOOL)_useFixedLayout
{
    auto* coreFrame = [self _mainCoreFrame];
    if (!coreFrame)
        return NO;

    auto* view = coreFrame->view();
    if (!view)
        return NO;

    return view->useFixedLayout();
}

#if !PLATFORM(IOS_FAMILY)
- (NSSize)_fixedLayoutSize
{
    auto* coreFrame = [self _mainCoreFrame];
    if (!coreFrame)
        return WebCore::IntSize();

    auto* view = coreFrame->view();
    if (!view)
        return WebCore::IntSize();

    return view->fixedLayoutSize();
}
#endif

- (void)_setPaginationMode:(WebPaginationMode)paginationMode
{
    auto* page = core(self);
    if (!page)
        return;

    auto pagination = page->pagination();
    switch (paginationMode) {
    case WebPaginationModeUnpaginated:
        pagination.mode = WebCore::Pagination::Unpaginated;
        break;
    case WebPaginationModeLeftToRight:
        pagination.mode = WebCore::Pagination::LeftToRightPaginated;
        break;
    case WebPaginationModeRightToLeft:
        pagination.mode = WebCore::Pagination::RightToLeftPaginated;
        break;
    case WebPaginationModeTopToBottom:
        pagination.mode = WebCore::Pagination::TopToBottomPaginated;
        break;
    case WebPaginationModeBottomToTop:
        pagination.mode = WebCore::Pagination::BottomToTopPaginated;
        break;
    default:
        return;
    }

    page->setPagination(pagination);
}

- (WebPaginationMode)_paginationMode
{
    auto* page = core(self);
    if (!page)
        return WebPaginationModeUnpaginated;

    switch (page->pagination().mode) {
    case WebCore::Pagination::Unpaginated:
        return WebPaginationModeUnpaginated;
    case WebCore::Pagination::LeftToRightPaginated:
        return WebPaginationModeLeftToRight;
    case WebCore::Pagination::RightToLeftPaginated:
        return WebPaginationModeRightToLeft;
    case WebCore::Pagination::TopToBottomPaginated:
        return WebPaginationModeTopToBottom;
    case WebCore::Pagination::BottomToTopPaginated:
        return WebPaginationModeBottomToTop;
    }

    ASSERT_NOT_REACHED();
    return WebPaginationModeUnpaginated;
}

- (void)_listenForLayoutMilestones:(WebLayoutMilestones)layoutMilestones
{
    auto* page = core(self);
    if (!page)
        return;

    page->addLayoutMilestones(coreLayoutMilestones(layoutMilestones));
}

- (WebLayoutMilestones)_layoutMilestones
{
    auto* page = core(self);
    if (!page)
        return 0;

    return kitLayoutMilestones(page->requestedLayoutMilestones());
}

- (WebPageVisibilityState)_visibilityState
{
    if (_private->page)
        return kit(_private->page->visibilityState());
    return WebPageVisibilityStateVisible;
}

- (void)_setIsVisible:(BOOL)isVisible
{
    if (_private->page)
        _private->page->setIsVisible(isVisible);
}

- (void)_setVisibilityState:(WebPageVisibilityState)visibilityState isInitialState:(BOOL)isInitialState
{
    UNUSED_PARAM(isInitialState);

    if (_private->page) {
        _private->page->setIsVisible(visibilityState == WebPageVisibilityStateVisible);
        if (visibilityState == WebPageVisibilityStatePrerender)
            _private->page->setIsPrerender();
    }
}

#if !PLATFORM(IOS_FAMILY)
- (BOOL)windowOcclusionDetectionEnabled
{
    return _private->windowOcclusionDetectionEnabled;
}

- (void)setWindowOcclusionDetectionEnabled:(BOOL)flag
{
    _private->windowOcclusionDetectionEnabled = flag;
}
#endif

- (void)_setPaginationBehavesLikeColumns:(BOOL)behavesLikeColumns
{
    auto* page = core(self);
    if (!page)
        return;

    auto pagination = page->pagination();
    pagination.behavesLikeColumns = behavesLikeColumns;

    page->setPagination(pagination);
}

- (BOOL)_paginationBehavesLikeColumns
{
    auto* page = core(self);
    if (!page)
        return NO;

    return page->pagination().behavesLikeColumns;
}

- (void)_setPageLength:(CGFloat)pageLength
{
    auto* page = core(self);
    if (!page)
        return;

    auto pagination = page->pagination();
    pagination.pageLength = pageLength;

    page->setPagination(pagination);
}

- (CGFloat)_pageLength
{
    auto* page = core(self);
    if (!page)
        return 1;

    return page->pagination().pageLength;
}

- (void)_setGapBetweenPages:(CGFloat)pageGap
{
    auto* page = core(self);
    if (!page)
        return;

    auto pagination = page->pagination();
    pagination.gap = pageGap;
    page->setPagination(pagination);
}

- (CGFloat)_gapBetweenPages
{
    auto* page = core(self);
    if (!page)
        return 0;

    return page->pagination().gap;
}

- (void)_setPaginationLineGridEnabled:(BOOL)lineGridEnabled
{
    auto* page = core(self);
    if (!page)
        return;

    page->setPaginationLineGridEnabled(lineGridEnabled);
}

- (BOOL)_paginationLineGridEnabled
{
    auto* page = core(self);
    if (!page)
        return NO;

    return page->paginationLineGridEnabled();
}

- (NSUInteger)_pageCount
{
    auto* page = core(self);
    if (!page)
        return 0;

    return page->pageCount();
}

#if !PLATFORM(IOS_FAMILY)
- (CGFloat)_backingScaleFactor
{
    return [self _deviceScaleFactor];
}

- (void)_setCustomBackingScaleFactor:(CGFloat)customScaleFactor
{
    float oldScaleFactor = [self _deviceScaleFactor];

    _private->customDeviceScaleFactor = customScaleFactor;

    if (_private->page && oldScaleFactor != [self _deviceScaleFactor])
        _private->page->setDeviceScaleFactor([self _deviceScaleFactor]);
}
#endif

- (NSUInteger)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag highlight:(BOOL)highlight limit:(NSUInteger)limit
{
    return [self countMatchesForText:string options:(caseFlag ? 0 : WebFindOptionsCaseInsensitive) highlight:highlight limit:limit markMatches:YES];
}

- (NSUInteger)countMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag highlight:(BOOL)highlight limit:(NSUInteger)limit markMatches:(BOOL)markMatches
{
    return [self countMatchesForText:string options:(caseFlag ? 0 : WebFindOptionsCaseInsensitive) highlight:highlight limit:limit markMatches:markMatches];
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag startInSelection:(BOOL)startInSelection
{
    return [self findString:string options:((forward ? 0 : WebFindOptionsBackwards) | (caseFlag ? 0 : WebFindOptionsCaseInsensitive) | (wrapFlag ? WebFindOptionsWrapAround : 0) | (startInSelection ? WebFindOptionsStartInSelection : 0))];
}

+ (void)_setLoadResourcesSerially:(BOOL)serialize
{
    WebPlatformStrategies::initializeIfNecessary();

    webResourceLoadScheduler().setSerialLoadingEnabled(serialize);
}

+ (BOOL)_HTTPPipeliningEnabled
{
    return WebCore::ResourceRequest::httpPipeliningEnabled();
}

+ (void)_setHTTPPipeliningEnabled:(BOOL)enabled
{
    WebCore::ResourceRequest::setHTTPPipeliningEnabled(enabled);
}

- (void)_didScrollDocumentInFrameView:(WebFrameView *)frameView
{
    [self hideFormValidationMessage];
    [[self _UIDelegateForwarder] webView:self didScrollDocumentInFrameView:frameView];
}

#if PLATFORM(IOS_FAMILY)
- (WebFixedPositionContent*)_fixedPositionContent
{
    return _private ? _private->_fixedPositionContent.get() : nil;
}

- (void)_documentScaleChanged
{
    if (WebNodeHighlight *currentHighlight = [self currentNodeHighlight])
        [currentHighlight setNeedsDisplay];

    if (_private->indicateLayer) {
        [_private->indicateLayer setNeedsLayout];
        [_private->indicateLayer setNeedsDisplay];
    }
}

- (BOOL)_wantsTelephoneNumberParsing
{
    if (!_private->page)
        return NO;
    return _private->page->settings().telephoneNumberParsingEnabled();
}

- (void)_setWantsTelephoneNumberParsing:(BOOL)flag
{
    if (_private->page)
        _private->page->settings().setTelephoneNumberParsingEnabled(flag);
}

- (BOOL)_webGLEnabled
{
    if (!_private->page)
        return NO;
    return _private->page->settings().webGLEnabled();
}

- (void)_setWebGLEnabled:(BOOL)enabled
{
    if (_private->page)
        _private->page->settings().setWebGLEnabled(enabled);
}

+ (void)_setTileCacheLayerPoolCapacity:(unsigned)capacity
{
    LegacyTileCache::setLayerPoolCapacity(capacity);
}
#endif // PLATFORM(IOS_FAMILY)

- (void)_setUnobscuredSafeAreaInsets:(WebEdgeInsets)insets
{
    if (auto page = _private->page)
        page->setUnobscuredSafeAreaInsets(WebCore::FloatBoxExtent(insets.top, insets.right, insets.bottom, insets.left));
}

- (WebEdgeInsets)_unobscuredSafeAreaInsets
{
    WebEdgeInsets insets({ 0, 0, 0, 0 });

    if (auto page = _private->page) {
        auto unobscuredSafeAreaInsets = page->unobscuredSafeAreaInsets();
        insets.top = unobscuredSafeAreaInsets.top();
        insets.left = unobscuredSafeAreaInsets.left();
        insets.bottom = unobscuredSafeAreaInsets.bottom();
        insets.right = unobscuredSafeAreaInsets.right();
    }

    return insets;
}

#if HAVE(OS_DARK_MODE_SUPPORT) && PLATFORM(MAC)
- (bool)_effectiveAppearanceIsDark
{
    NSAppearanceName appearance = [[self effectiveAppearance] bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    return [appearance isEqualToString:NSAppearanceNameDarkAqua];
}

- (bool)_effectiveUserInterfaceLevelIsElevated
{
    return false;
}
#endif

- (void)_setUseSystemAppearance:(BOOL)useSystemAppearance
{
    if (_private && _private->page)
        _private->page->setUseSystemAppearance(useSystemAppearance);
}

- (BOOL)_useSystemAppearance
{
    if (!_private->page)
        return NO;

    return _private->page->useSystemAppearance();
}

#if HAVE(OS_DARK_MODE_SUPPORT) && PLATFORM(MAC)
- (void)viewDidChangeEffectiveAppearance
{
    // This can be called during [super initWithCoder:] and [super initWithFrame:].
    // That is before _private is ready to be used, so check. <rdar://problem/39611236>
    if (!_private || !_private->page)
        return;

    _private->page->effectiveAppearanceDidChange(self._effectiveAppearanceIsDark, self._effectiveUserInterfaceLevelIsElevated);
}
#endif

- (void)_setSourceApplicationAuditData:(NSData *)sourceApplicationAuditData
{
    if (_private->sourceApplicationAuditData == sourceApplicationAuditData)
        return;

    _private->sourceApplicationAuditData = adoptNS([sourceApplicationAuditData copy]);
}

- (NSData *)_sourceApplicationAuditData
{
    return _private->sourceApplicationAuditData.get();
}

- (void)_setFontFallbackPrefersPictographs:(BOOL)flag
{
    if (_private->page)
        _private->page->settings().setFontFallbackPrefersPictographs(flag);
}

#if HAVE(TOUCH_BAR)

- (void)showCandidates:(NSArray<NSTextCheckingResult *> *)candidates forString:(NSString *)string inRect:(NSRect)rectOfTypedString forSelectedRange:(NSRange)range view:(NSView *)view completionHandler:(void (^)(NSTextCheckingResult *acceptedCandidate))completionBlock
{
    [self.candidateList setCandidates:candidates forSelectedRange:range inString:string rect:rectOfTypedString view:view completionHandler:completionBlock];
}

- (BOOL)shouldRequestCandidates
{
    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return NO;

    return !coreFrame->selection().selection().isInPasswordField() && self.candidateList.candidateListVisible;
}

- (void)forceRequestCandidatesForTesting
{
    _private->_canCreateTouchBars = YES;
    [self updateTouchBar];
}

#else

- (void)showCandidates:(NSArray *)candidates forString:(NSString *)string inRect:(NSRect)rectOfTypedString forSelectedRange:(NSRange)range view:(NSView *)view completionHandler:(void (^)(NSTextCheckingResult *acceptedCandidate))completionBlock
{
}

- (void)forceRequestCandidatesForTesting
{
}

- (BOOL)shouldRequestCandidates
{
    return NO;
}

#endif // HAVE(TOUCH_BAR)

@end

@implementation _WebSafeForwarder

// Used to send messages to delegates that implement informal protocols.

- (instancetype)initWithTarget:(id)target defaultTarget:(id)defaultTarget
{
    if (!(self = [super init]))
        return nil;
    _target = target;
    _defaultTarget = defaultTarget;
#if PLATFORM(IOS_FAMILY)
    _asyncForwarder = [[_WebSafeAsyncForwarder alloc] initWithForwarder:self];
#endif
    return self;
}

#if PLATFORM(IOS_FAMILY)
@synthesize asyncForwarder = _asyncForwarder;

- (void)dealloc
{
    _target = nil;
    _defaultTarget = nil;
    [_asyncForwarder release];
    _asyncForwarder = nil;

    [super dealloc];
}

- (void)clearTarget
{
    _target = nil;
}
#endif

- (void)forwardInvocation:(NSInvocation *)invocation
{
#if PLATFORM(IOS_FAMILY)
    if (WebThreadIsCurrent()) {
        [invocation retainArguments];
        WebThreadCallDelegate(invocation);
        return;
    }
#endif
    if ([_target respondsToSelector:invocation.selector]) {
        @try {
            [invocation invokeWithTarget:_target];
        } @catch(id exception) {
            ReportDiscardedDelegateException(invocation.selector, exception);
        }
        return;
    }

    if ([_defaultTarget respondsToSelector:invocation.selector])
        [invocation invokeWithTarget:_defaultTarget];

    // Do nothing quietly if method not implemented.
}

#if PLATFORM(IOS_FAMILY)
- (BOOL)respondsToSelector:(SEL)aSelector
{
    return [_defaultTarget respondsToSelector:aSelector];
}
#endif

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    return [_defaultTarget methodSignatureForSelector:aSelector];
}

@end

#if PLATFORM(IOS_FAMILY)
@implementation _WebSafeAsyncForwarder

- (instancetype)initWithForwarder:(_WebSafeForwarder *)forwarder
{
    if (!(self = [super init]))
        return nil;
    _forwarder = forwarder;
    return self;
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
    if (WebThreadIsCurrent()) {
        [invocation retainArguments];
        RunLoop::main().dispatch([forwarder = retainPtr(_forwarder), invocation = retainPtr(invocation)] {
            [forwarder forwardInvocation:invocation.get()];
        });
    } else
        [_forwarder forwardInvocation:invocation];
}

- (BOOL)respondsToSelector:(SEL)aSelector
{
    return [_forwarder respondsToSelector:aSelector];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    return [_forwarder methodSignatureForSelector:aSelector];
}

@end
#endif

#if HAVE(TOUCH_BAR)
@interface WebView () <NSCandidateListTouchBarItemDelegate, NSTouchBarDelegate, NSTouchBarProvider>
@end
#endif

@implementation WebView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (initialized)
        return;
    initialized = YES;

#if !PLATFORM(IOS_FAMILY)
    JSC::initialize();
    WTF::initializeMainThread();
    WebCore::populateJITOperations();
#endif

    WTF::RefCountedBase::enableThreadingChecksGlobally();

    WTF::setProcessPrivileges(allPrivileges());
    WebCore::NetworkStorageSession::permitProcessToUseCookieAPI(true);

#if !PLATFORM(IOS_FAMILY)
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillTerminate) name:NSApplicationWillTerminateNotification object:NSApp];
#endif
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_cacheModelChangedNotification:) name:WebPreferencesCacheModelChangedInternalNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesRemovedNotification:) name:WebPreferencesRemovedNotification object:nil];

#if PLATFORM(IOS_FAMILY)
    continuousSpellCheckingEnabled = NO;

#else

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    continuousSpellCheckingEnabled = [defaults boolForKey:WebContinuousSpellCheckingEnabled];
    grammarCheckingEnabled = [defaults boolForKey:WebGrammarCheckingEnabled];

    automaticQuoteSubstitutionEnabled = [self _shouldAutomaticQuoteSubstitutionBeEnabled];
    automaticLinkDetectionEnabled = [defaults boolForKey:WebAutomaticLinkDetectionEnabled];
    automaticDashSubstitutionEnabled = [self _shouldAutomaticDashSubstitutionBeEnabled];
    automaticTextReplacementEnabled = [self _shouldAutomaticTextReplacementBeEnabled];
    automaticSpellingCorrectionEnabled = [self _shouldAutomaticSpellingCorrectionBeEnabled];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_didChangeAutomaticTextReplacementEnabled:)
        name:NSSpellCheckerDidChangeAutomaticTextReplacementNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_didChangeAutomaticSpellingCorrectionEnabled:)
        name:NSSpellCheckerDidChangeAutomaticSpellingCorrectionNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_didChangeAutomaticQuoteSubstitutionEnabled:)
        name:NSSpellCheckerDidChangeAutomaticQuoteSubstitutionNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_didChangeAutomaticDashSubstitutionEnabled:)
        name:NSSpellCheckerDidChangeAutomaticDashSubstitutionNotification object:nil];
#endif
}

#if !PLATFORM(IOS_FAMILY)
+ (BOOL)_shouldAutomaticTextReplacementBeEnabled
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:WebAutomaticTextReplacementEnabled])
        return [NSSpellChecker isAutomaticTextReplacementEnabled];
    return [defaults boolForKey:WebAutomaticTextReplacementEnabled];
}

+ (void)_didChangeAutomaticTextReplacementEnabled:(NSNotification *)notification
{
    automaticTextReplacementEnabled = [self _shouldAutomaticTextReplacementBeEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

+ (BOOL)_shouldAutomaticSpellingCorrectionBeEnabled
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:WebAutomaticSpellingCorrectionEnabled])
        return [NSSpellChecker isAutomaticTextReplacementEnabled];
    return [defaults boolForKey:WebAutomaticSpellingCorrectionEnabled];
}

+ (void)_didChangeAutomaticSpellingCorrectionEnabled:(NSNotification *)notification
{
    automaticSpellingCorrectionEnabled = [self _shouldAutomaticSpellingCorrectionBeEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

+ (BOOL)_shouldAutomaticQuoteSubstitutionBeEnabled
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:WebAutomaticQuoteSubstitutionEnabled])
        return [NSSpellChecker isAutomaticQuoteSubstitutionEnabled];

    return [defaults boolForKey:WebAutomaticQuoteSubstitutionEnabled];
}

+ (BOOL)_shouldAutomaticDashSubstitutionBeEnabled
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:WebAutomaticDashSubstitutionEnabled])
        return [NSSpellChecker isAutomaticDashSubstitutionEnabled];

    return [defaults boolForKey:WebAutomaticDashSubstitutionEnabled];
}

+ (void)_didChangeAutomaticQuoteSubstitutionEnabled:(NSNotification *)notification
{
    automaticQuoteSubstitutionEnabled = [self _shouldAutomaticQuoteSubstitutionBeEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

+ (void)_didChangeAutomaticDashSubstitutionEnabled:(NSNotification *)notification
{
    automaticDashSubstitutionEnabled = [self _shouldAutomaticDashSubstitutionBeEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

+ (void)_applicationWillTerminate
{
    applicationIsTerminating = YES;

    if (fastDocumentTeardownEnabled())
        [self closeAllWebViews];

    if (!pluginDatabaseClientCount)
        [WebPluginDatabase closeSharedDatabase];

    WebKit::WebStorageNamespaceProvider::closeLocalStorage();
}
#endif // !PLATFORM(IOS_FAMILY)

+ (BOOL)_canShowMIMEType:(NSString *)MIMEType allowingPlugins:(BOOL)allowPlugins
{
    return [self _viewClass:nil andRepresentationClass:nil forMIMEType:MIMEType allowingPlugins:allowPlugins];
}

+ (BOOL)canShowMIMEType:(NSString *)MIMEType
{
    return [self _canShowMIMEType:MIMEType allowingPlugins:YES];
}

- (BOOL)_canShowMIMEType:(NSString *)MIMEType
{
    return [[self class] _canShowMIMEType:MIMEType allowingPlugins:[_private->preferences arePlugInsEnabled]];
}

- (WebBasePluginPackage *)_pluginForMIMEType:(NSString *)MIMEType
{
    if (![_private->preferences arePlugInsEnabled])
        return nil;

    WebBasePluginPackage *pluginPackage = [[WebPluginDatabase sharedDatabase] pluginForMIMEType:MIMEType];
    if (pluginPackage)
        return pluginPackage;

#if !PLATFORM(IOS_FAMILY)
    if (_private->pluginDatabase)
        return [_private->pluginDatabase pluginForMIMEType:MIMEType];
#endif

    return nil;
}

- (WebBasePluginPackage *)_pluginForExtension:(NSString *)extension
{
    if (![_private->preferences arePlugInsEnabled])
        return nil;

    WebBasePluginPackage *pluginPackage = [[WebPluginDatabase sharedDatabase] pluginForExtension:extension];
    if (pluginPackage)
        return pluginPackage;

#if !PLATFORM(IOS_FAMILY)
    if (_private->pluginDatabase)
        return [_private->pluginDatabase pluginForExtension:extension];
#endif

    return nil;
}

#if !PLATFORM(IOS_FAMILY)
- (void)addPluginInstanceView:(NSView *)view
{
    if (!_private->pluginDatabase)
        _private->pluginDatabase = adoptNS([[WebPluginDatabase alloc] init]);
    [_private->pluginDatabase addPluginInstanceView:view];
}

- (void)removePluginInstanceView:(NSView *)view
{
    if (_private->pluginDatabase)
        [_private->pluginDatabase removePluginInstanceView:view];
}

- (void)removePluginInstanceViewsFor:(WebFrame*)webFrame
{
    if (_private->pluginDatabase)
        [_private->pluginDatabase removePluginInstanceViewsFor:webFrame];
}
#endif

- (BOOL)_isMIMETypeRegisteredAsPlugin:(NSString *)MIMEType
{
    if (![_private->preferences arePlugInsEnabled])
        return NO;

    if ([[WebPluginDatabase sharedDatabase] isMIMETypeRegistered:MIMEType])
        return YES;

#if !PLATFORM(IOS_FAMILY)
    if (_private->pluginDatabase && [_private->pluginDatabase isMIMETypeRegistered:MIMEType])
        return YES;
#endif

    return NO;
}

+ (BOOL)canShowMIMETypeAsHTML:(NSString *)MIMEType
{
#if PLATFORM(IOS_FAMILY)
    // FIXME: <rdar://problem/7961656> +[WebView canShowMIMETypeAsHTML:] regressed significantly in iOS 4.0
    // Fast path for the common case to avoid creating the MIME type registry.
    if ([MIMEType isEqualToString:@"text/html"])
        return YES;
#endif
    return [WebFrameView _canShowMIMETypeAsHTML:MIMEType];
}

+ (NSArray *)MIMETypesShownAsHTML
{
    NSMutableDictionary *viewTypes = [WebFrameView _viewTypesAllowImageTypeOmission:YES];
    NSEnumerator *enumerator = [viewTypes keyEnumerator];
    id key;
    auto array = adoptNS([[NSMutableArray alloc] init]);

    while ((key = [enumerator nextObject])) {
        if ([viewTypes objectForKey:key] == [WebHTMLView class])
            [array addObject:key];
    }

    return array.autorelease();
}

+ (void)setMIMETypesShownAsHTML:(NSArray *)MIMETypes
{
    auto viewTypes = adoptNS([[WebFrameView _viewTypesAllowImageTypeOmission:YES] copy]);
    NSEnumerator *enumerator = [viewTypes keyEnumerator];
    id key;
    while ((key = [enumerator nextObject])) {
        if ([viewTypes objectForKey:key] == [WebHTMLView class])
            [WebView _unregisterViewClassAndRepresentationClassForMIMEType:key];
    }

    int i, count = [MIMETypes count];
    for (i = 0; i < count; i++) {
        [WebView registerViewClass:[WebHTMLView class]
                representationClass:[WebHTMLRepresentation class]
                forMIMEType:[MIMETypes objectAtIndex:i]];
    }
}

#if !PLATFORM(IOS_FAMILY)
+ (NSURL *)URLFromPasteboard:(NSPasteboard *)pasteboard
{
    return [pasteboard _web_bestURL];
}

+ (NSString *)URLTitleFromPasteboard:(NSPasteboard *)pasteboard
{
    return [pasteboard stringForType:WebURLNamePboardType];
}
#endif

+ (void)registerURLSchemeAsLocal:(NSString *)protocol
{
    WebCore::LegacySchemeRegistry::registerURLSchemeAsLocal(protocol);
}

- (id)_initWithArguments:(NSDictionary *) arguments
{
#if !PLATFORM(IOS_FAMILY)
    NSCoder *decoder = [arguments objectForKey:@"decoder"];
    if (decoder) {
        self = [self initWithCoder:decoder];
    } else {
#endif
        ASSERT([arguments objectForKey:@"frame"]);
        NSValue *frameValue = [arguments objectForKey:@"frame"];
        NSRect frame = (frameValue ? [frameValue rectValue] : NSZeroRect);
        NSString *frameName = [arguments objectForKey:@"frameName"];
        NSString *groupName = [arguments objectForKey:@"groupName"];
        self = [self initWithFrame:frame frameName:frameName groupName:groupName];
#if !PLATFORM(IOS_FAMILY)
    }
#endif

    return self;
}

#if !PLATFORM(IOS_FAMILY)
static bool clientNeedsWebViewInitThreadWorkaround()
{
    if (WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_WEBVIEW_INIT_THREAD_WORKAROUND))
        return false;

    NSString *bundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];

    // Installer.
    if ([bundleIdentifier _webkit_isCaseInsensitiveEqualToString:@"com.apple.installer"])
        return true;

    // Automator.
    if ([bundleIdentifier _webkit_isCaseInsensitiveEqualToString:@"com.apple.Automator"])
        return true;

    // Automator Runner.
    if ([bundleIdentifier _webkit_isCaseInsensitiveEqualToString:@"com.apple.AutomatorRunner"])
        return true;

    // Automator workflows.
    if ([bundleIdentifier _webkit_hasCaseInsensitivePrefix:@"com.apple.Automator."])
        return true;

    return false;
}

static bool needsWebViewInitThreadWorkaround()
{
    static bool isOldClient = clientNeedsWebViewInitThreadWorkaround();
    return isOldClient && !pthread_main_np();
}
#endif // !PLATFORM(IOS_FAMILY)

- (instancetype)initWithFrame:(NSRect)f
{
    return [self initWithFrame:f frameName:nil groupName:nil];
}

- (instancetype)initWithFrame:(NSRect)f frameName:(NSString *)frameName groupName:(NSString *)groupName
{
#if !PLATFORM(IOS_FAMILY)
    if (needsWebViewInitThreadWorkaround())
        return [[self _webkit_invokeOnMainThread] initWithFrame:f frameName:frameName groupName:groupName];
#endif

    WebCoreThreadViolationCheckRoundTwo();
    return [self _initWithFrame:f frameName:frameName groupName:groupName];
}

#if !PLATFORM(IOS_FAMILY)
- (instancetype)initWithCoder:(NSCoder *)decoder
{
    if (needsWebViewInitThreadWorkaround())
        return [[self _webkit_invokeOnMainThread] initWithCoder:decoder];

    WebCoreThreadViolationCheckRoundTwo();
    WebView *result = nil;

    @try {
        NSString *frameName;
        NSString *groupName;
        WebPreferences *preferences;
        BOOL useBackForwardList = NO;
        BOOL allowsUndo = YES;

        result = [super initWithCoder:decoder];
        result->_private = [[WebViewPrivate alloc] init];

        // We don't want any of the archived subviews. The subviews will always
        // be created in _commonInitializationFrameName:groupName:.
        [[result subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];

        if ([decoder allowsKeyedCoding]) {
            frameName = [decoder decodeObjectForKey:@"FrameName"];
            groupName = [decoder decodeObjectForKey:@"GroupName"];
            preferences = [decoder decodeObjectForKey:@"Preferences"];
            useBackForwardList = [decoder decodeBoolForKey:@"UseBackForwardList"];
            if ([decoder containsValueForKey:@"AllowsUndo"])
                allowsUndo = [decoder decodeBoolForKey:@"AllowsUndo"];
        } else {
            int version;
            [decoder decodeValueOfObjCType:@encode(int) at:&version];
            frameName = [decoder decodeObject];
            groupName = [decoder decodeObject];
            preferences = [decoder decodeObject];
            if (version > 1)
                [decoder decodeValuesOfObjCTypes:"c", &useBackForwardList];
            // The allowsUndo field is no longer written out in encodeWithCoder, but since there are
            // version 3 NIBs that have this field encoded, we still need to read it in.
            if (version == 3)
                [decoder decodeValuesOfObjCTypes:"c", &allowsUndo];
        }

        if (![frameName isKindOfClass:[NSString class]])
            frameName = nil;
        if (![groupName isKindOfClass:[NSString class]])
            groupName = nil;
        if (![preferences isKindOfClass:[WebPreferences class]])
            preferences = nil;

        LOG(Encoding, "FrameName = %@, GroupName = %@, useBackForwardList = %d\n", frameName, groupName, (int)useBackForwardList);
        [result _commonInitializationWithFrameName:frameName groupName:groupName];
        static_cast<BackForwardList&>([result page]->backForward().client()).setEnabled(useBackForwardList);
        result->_private->allowsUndo = allowsUndo;
        if (preferences)
            [result setPreferences:preferences];
    } @catch (NSException *localException) {
        result = nil;
        [self release];
    }

    return result;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    // Set asside the subviews before we archive. We don't want to archive any subviews.
    // The subviews will always be created in _commonInitializationFrameName:groupName:.
    id originalSubviews = self._subviewsIvar;
    self._subviewsIvar = nil;

    [super encodeWithCoder:encoder];

    // Restore the subviews we set aside.
    self._subviewsIvar = originalSubviews;

    BOOL useBackForwardList = _private->page && static_cast<BackForwardList&>(_private->page->backForward().client()).enabled();
    if ([encoder allowsKeyedCoding]) {
        [encoder encodeObject:[[self mainFrame] name] forKey:@"FrameName"];
        [encoder encodeObject:[self groupName] forKey:@"GroupName"];
        [encoder encodeObject:[self preferences] forKey:@"Preferences"];
        [encoder encodeBool:useBackForwardList forKey:@"UseBackForwardList"];
        [encoder encodeBool:_private->allowsUndo forKey:@"AllowsUndo"];
    } else {
        int version = WebViewVersion;
        [encoder encodeValueOfObjCType:@encode(int) at:&version];
        [encoder encodeObject:[[self mainFrame] name]];
        [encoder encodeObject:[self groupName]];
        [encoder encodeObject:[self preferences]];
        [encoder encodeValuesOfObjCTypes:"c", &useBackForwardList];
        // DO NOT encode any new fields here, doing so will break older WebKit releases.
    }

    LOG(Encoding, "FrameName = %@, GroupName = %@, useBackForwardList = %d\n", [[self mainFrame] name], [self groupName], (int)useBackForwardList);
}
#endif // !PLATFORM(IOS_FAMILY)

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebView class], self))
        return;

    // Because the machinations of the view's shutdown may cause self to be added to
    // active autorelease pool, we capture any such releases here to ensure they are
    // carried out before we are dealloc'd.
    @autoreleasepool {

#if PLATFORM(IOS_FAMILY)
        if (_private)
            [_private->_geolocationProvider stopTrackingWebView:self];
#endif

        [[NSNotificationCenter defaultCenter] removeObserver:self];

        // call close to ensure we tear-down completely
        // this maintains our old behavior for existing applications
        [self close];

        if ([[self class] shouldIncludeInWebKitStatistics])
            --WebViewCount;

#if PLATFORM(MAC)
        if ([self _needsFrameLoadDelegateRetainQuirk])
            [_private->frameLoadDelegate release];
#endif

        [_private release];
        // [super dealloc] can end up dispatching against _private (3466082)
        _private = nil;
    }

    [super dealloc];
}

- (void)close
{
    // _close existed first, and some clients might be calling or overriding it, so call through.
    [self _close];

#if PLATFORM(IOS_FAMILY)
    if (_private->layerFlushController) {
        _private->layerFlushController->invalidate();
        _private->layerFlushController = nullptr;
    }
#endif
}

- (void)setShouldCloseWithWindow:(BOOL)close
{
    _private->shouldCloseWithWindow = close;
}

- (BOOL)shouldCloseWithWindow
{
    return _private->shouldCloseWithWindow;
}

#if !PLATFORM(IOS_FAMILY)
// FIXME: Use AppKit constants for these when they are available.
static NSString * const windowDidChangeBackingPropertiesNotification = @"NSWindowDidChangeBackingPropertiesNotification";
static NSString * const backingPropertyOldScaleFactorKey = @"NSBackingPropertyOldScaleFactorKey";

- (void)addWindowObserversForWindow:(NSWindow *)window
{
    if (window) {
        NSNotificationCenter *defaultNotificationCenter = [NSNotificationCenter defaultCenter];

        [defaultNotificationCenter addObserver:self selector:@selector(windowKeyStateChanged:)
            name:NSWindowDidBecomeKeyNotification object:window];
        [defaultNotificationCenter addObserver:self selector:@selector(windowKeyStateChanged:)
            name:NSWindowDidResignKeyNotification object:window];
        [defaultNotificationCenter addObserver:self selector:@selector(_windowWillOrderOnScreen:)
            name:NSWindowWillOrderOnScreenNotification object:window];
        [defaultNotificationCenter addObserver:self selector:@selector(_windowWillOrderOffScreen:)
            name:NSWindowWillOrderOffScreenNotification object:window];
        [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeBackingProperties:)
            name:windowDidChangeBackingPropertiesNotification object:window];
        [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeScreen:)
            name:NSWindowDidChangeScreenNotification object:window];
        [defaultNotificationCenter addObserver:self selector:@selector(_windowVisibilityChanged:)
            name:NSWindowDidMiniaturizeNotification object:window];
        [defaultNotificationCenter addObserver:self selector:@selector(_windowVisibilityChanged:)
            name:NSWindowDidDeminiaturizeNotification object:window];
        [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeOcclusionState:)
            name:NSWindowDidChangeOcclusionStateNotification object:window];
        [_private->windowVisibilityObserver startObserving:window];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        NSNotificationCenter *defaultNotificationCenter = [NSNotificationCenter defaultCenter];

        [defaultNotificationCenter removeObserver:self
            name:NSWindowDidBecomeKeyNotification object:window];
        [defaultNotificationCenter removeObserver:self
            name:NSWindowDidResignKeyNotification object:window];
        [defaultNotificationCenter removeObserver:self
            name:NSWindowWillOrderOnScreenNotification object:window];
        [defaultNotificationCenter removeObserver:self
            name:NSWindowWillOrderOffScreenNotification object:window];
        [defaultNotificationCenter removeObserver:self
            name:windowDidChangeBackingPropertiesNotification object:window];
        [defaultNotificationCenter removeObserver:self
            name:NSWindowDidChangeScreenNotification object:window];
        [defaultNotificationCenter removeObserver:self
            name:NSWindowDidMiniaturizeNotification object:window];
        [defaultNotificationCenter removeObserver:self
            name:NSWindowDidDeminiaturizeNotification object:window];
        [defaultNotificationCenter removeObserver:self
            name:NSWindowDidChangeOcclusionStateNotification object:window];
        [_private->windowVisibilityObserver stopObserving:window];
    }
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    // Don't do anything if the WebView isn't initialized.
    // This happens when decoding a WebView in a nib.
    // FIXME: What sets up the observer of NSWindowWillCloseNotification in this case?
    if (!_private)
        return;

    if ([self window] && [self window] != [self hostWindow])
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillCloseNotification object:[self window]];

    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowWillClose:) name:NSWindowWillCloseNotification object:window];

        // Ensure that we will receive the events that WebHTMLView (at least) needs.
        // The following are expensive enough that we don't want to call them over
        // and over, so do them when we move into a window.
        [window setAcceptsMouseMovedEvents:YES];
        [window _setShouldPostEventNotifications:YES];
    } else if (!_private->closed) {
        _private->page->setCanStartMedia(false);
        _private->page->setIsInWindow(false);
    }

    if (window != [self window]) {
        [self removeWindowObservers];
        [self addWindowObserversForWindow:window];
    }
}
#endif // !PLATFORM(IOS_FAMILY)

- (void)viewDidMoveToWindow
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (!_private || _private->closed)
        return;

    if ([self window]) {
        _private->page->setCanStartMedia(true);
        _private->page->setIsInWindow(true);

#if PLATFORM(IOS_FAMILY)
        auto preferences = self.preferences;
        if (auto tileCache = self.window.tileCache) {
            tileCache->setTileBordersVisible(preferences.showDebugBorders);
            tileCache->setTilePaintCountersVisible(preferences.showRepaintCounter);
            tileCache->setAcceleratedDrawingEnabled(preferences.acceleratedDrawingEnabled);
        }
#endif
    }
#if PLATFORM(IOS_FAMILY)
    else
        [_private->fullscreenController requestHideAndExitFullscreen];
#endif

#if PLATFORM(MAC)
    _private->page->setDeviceScaleFactor([self _deviceScaleFactor]);

    if (_private->immediateActionController) {
        NSImmediateActionGestureRecognizer *recognizer = [_private->immediateActionController immediateActionRecognizer];
        if ([self window]) {
            if (![[self gestureRecognizers] containsObject:recognizer])
                [self addGestureRecognizer:recognizer];
        } else
            [self removeGestureRecognizer:recognizer];
    }
#endif

    [self _updateActiveState];
    [self _updateVisibilityState];
}

#if !PLATFORM(IOS_FAMILY)
- (void)doWindowDidChangeScreen
{
    if (_private && _private->page)
        _private->page->chrome().windowScreenDidChange(WebCore::displayID(self.window.screen), WTF::nullopt);
}

- (void)_windowChangedKeyState
{
    [self _updateActiveState];
    [super _windowChangedKeyState];
}

- (void)windowKeyStateChanged:(NSNotification *)notification
{
    [self _updateActiveState];
}

- (void)viewDidHide
{
    [self _updateVisibilityState];
}

- (void)viewDidUnhide
{
    [self _updateVisibilityState];
}

- (void)_windowWillOrderOnScreen:(NSNotification *)notification
{
    if (![self shouldUpdateWhileOffscreen])
        [self setNeedsDisplay:YES];

    // Send a change screen to make sure the initial displayID is set
    [self doWindowDidChangeScreen];

    if (_private && _private->page) {
        _private->page->resumeScriptedAnimations();
        _private->page->setIsVisible(true);
    }
}

- (void)_windowDidChangeScreen:(NSNotification *)notification
{
    [self doWindowDidChangeScreen];
}

- (void)_windowWillOrderOffScreen:(NSNotification *)notification
{
    if (_private && _private->page) {
        _private->page->suspendScriptedAnimations();
        _private->page->setIsVisible(false);
    }
}

- (void)_windowWillClose:(NSNotification *)notification
{
    if ([self shouldCloseWithWindow] && ([self window] == [self hostWindow] || ([self window] && ![self hostWindow]) || (![self window] && [self hostWindow])))
        [self close];
}

- (void)_windowDidChangeBackingProperties:(NSNotification *)notification
{
    CGFloat oldBackingScaleFactor = [[notification.userInfo objectForKey:backingPropertyOldScaleFactorKey] doubleValue];
    CGFloat newBackingScaleFactor = [self _deviceScaleFactor];
    if (oldBackingScaleFactor == newBackingScaleFactor)
        return;

    _private->page->setDeviceScaleFactor(newBackingScaleFactor);
}

- (void)_windowDidChangeOcclusionState:(NSNotification *)notification
{
    [self _updateVisibilityState];
}

#else
- (void)_wakWindowScreenScaleChanged:(NSNotification *)notification
{
    [self _updateScreenScaleFromWindow];
}

- (void)_wakWindowVisibilityChanged:(NSNotification *)notification
{
    if ([notification object] == [self window])
        [self _updateVisibilityState];
}

- (void)_updateScreenScaleFromWindow
{
    float scaleFactor = 1.0f;
    if (WAKWindow *window = [self window])
        scaleFactor = [window screenScale];
    else
        scaleFactor = WebCore::screenScaleFactor();

    _private->page->setDeviceScaleFactor(scaleFactor);
}
#endif // PLATFORM(IOS_FAMILY)

- (void)setPreferences:(WebPreferences *)prefs
{
    if (!prefs)
        prefs = [WebPreferences standardPreferences];

    if (_private->preferences == prefs)
        return;

    [prefs willAddToWebView];

    auto oldPrefs = std::exchange(_private->preferences, nullptr);

    [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedInternalNotification object:[self preferences]];
    [WebPreferences _removeReferenceForIdentifier:[oldPrefs identifier]];

    _private->preferences = prefs;

    // After registering for the notification, post it so the WebCore settings update.
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
        name:WebPreferencesChangedInternalNotification object:[self preferences]];
    [self _preferencesChanged:[self preferences]];
    [[self preferences] _postPreferencesChangedAPINotification];

    [oldPrefs didRemoveFromWebView];
}

- (WebPreferences *)preferences
{
    return _private->preferences.get();
}

- (void)setPreferencesIdentifier:(NSString *)anIdentifier
{
    if (!_private->closed && ![anIdentifier isEqual:[[self preferences] identifier]]) {
        auto prefs = adoptNS([[WebPreferences alloc] initWithIdentifier:anIdentifier]);
        [self setPreferences:prefs.get()];
    }
}

- (NSString *)preferencesIdentifier
{
    return [[self preferences] identifier];
}

- (void)setUIDelegate:delegate
{
    _private->UIDelegate = delegate;
#if PLATFORM(IOS_FAMILY)
    [_private->UIDelegateForwarder clearTarget];
#endif
    _private->UIDelegateForwarder = nil;
}

- (id)UIDelegate
{
    return _private->UIDelegate;
}

#if PLATFORM(IOS_FAMILY)
- (id)_resourceLoadDelegateForwarder
{
    if (_private->closing)
        return nil;

    if (!_private->resourceProgressDelegateForwarder)
        _private->resourceProgressDelegateForwarder = adoptNS([[_WebSafeForwarder alloc] initWithTarget:[self resourceLoadDelegate] defaultTarget:[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate]]);
    return _private->resourceProgressDelegateForwarder.get();
}
#endif

- (void)setResourceLoadDelegate: delegate
{
#if PLATFORM(IOS_FAMILY)
    [_private->resourceProgressDelegateForwarder clearTarget];
    _private->resourceProgressDelegateForwarder = nil;
#endif
    _private->resourceProgressDelegate = delegate;
    [self _cacheResourceLoadDelegateImplementations];
}

- (id)resourceLoadDelegate
{
    return _private->resourceProgressDelegate;
}

- (void)setDownloadDelegate: delegate
{
    _private->downloadDelegate = delegate;
}


- (id)downloadDelegate
{
    return _private->downloadDelegate;
}

- (void)setPolicyDelegate:delegate
{
    _private->policyDelegate = delegate;
#if PLATFORM(IOS_FAMILY)
    [_private->policyDelegateForwarder clearTarget];
#endif
    _private->policyDelegateForwarder = nil;
}

- (id)policyDelegate
{
    return _private->policyDelegate;
}

#if PLATFORM(IOS_FAMILY)
- (id)_frameLoadDelegateForwarder
{
    if (_private->closing)
        return nil;

    if (!_private->frameLoadDelegateForwarder)
        _private->frameLoadDelegateForwarder = adoptNS([[_WebSafeForwarder alloc] initWithTarget:[self frameLoadDelegate] defaultTarget:[WebDefaultFrameLoadDelegate sharedFrameLoadDelegate]]);
    return _private->frameLoadDelegateForwarder.get();
}
#endif

- (void)setFrameLoadDelegate:delegate
{
    // <rdar://problem/6950660> - Due to some subtle WebKit changes - presumably to delegate callback behavior - we've
    // unconvered a latent bug in at least one WebKit app where the delegate wasn't properly retained by the app and
    // was dealloc'ed before being cleared.
    // This is an effort to keep such apps working for now.
#if PLATFORM(MAC)
    if ([self _needsFrameLoadDelegateRetainQuirk]) {
        [delegate retain];
        [_private->frameLoadDelegate release];
    }
#else
    [_private->frameLoadDelegateForwarder clearTarget];
    _private->frameLoadDelegateForwarder = nil;
#endif

    _private->frameLoadDelegate = delegate;
    [self _cacheFrameLoadDelegateImplementations];
}

- (id)frameLoadDelegate
{
    return _private->frameLoadDelegate;
}

- (WebFrame *)mainFrame
{
    // This can be called in initialization, before _private has been set up (3465613)
    if (!_private || !_private->page)
        return nil;
    return kit(&_private->page->mainFrame());
}

- (WebFrame *)selectedFrame
{
    // If the first responder is a view in our tree, we get the frame containing the first responder.
    // This is faster than searching the frame hierarchy, and will give us a result even in the case
    // where the focused frame doesn't actually contain a selection.
    WebFrame *focusedFrame = [self _focusedFrame];
    if (focusedFrame)
        return focusedFrame;

    // If the first responder is outside of our view tree, we search for a frame containing a selection.
    // There should be at most only one of these.
    return [[self mainFrame] _findFrameWithSelection];
}

- (WebBackForwardList *)backForwardList
{
    if (!_private->page)
        return nil;
    BackForwardList& list = static_cast<BackForwardList&>(_private->page->backForward().client());
    if (!list.enabled())
        return nil;
    return kit(&list);
}

- (void)setMaintainsBackForwardList:(BOOL)flag
{
    if (!_private->page)
        return;
    static_cast<BackForwardList&>(_private->page->backForward().client()).setEnabled(flag);
}

- (BOOL)goBack
{
    if (!_private->page)
        return NO;

#if PLATFORM(IOS_FAMILY)
    if (WebThreadIsCurrent() || !WebThreadIsEnabled())
#endif
    return _private->page->backForward().goBack();
#if PLATFORM(IOS_FAMILY)
    WebThreadRun(^{
        _private->page->backForward().goBack();
    });
    // FIXME: <rdar://problem/9157572> -[WebView goBack] and -goForward always return YES when called from the main thread
    return YES;
#endif
}

- (BOOL)goForward
{
    if (!_private->page)
        return NO;

#if PLATFORM(IOS_FAMILY)
    if (WebThreadIsCurrent() || !WebThreadIsEnabled())
#endif
    return _private->page->backForward().goForward();
#if PLATFORM(IOS_FAMILY)
    WebThreadRun(^{
        _private->page->backForward().goForward();
    });
    // FIXME: <rdar://problem/9157572> -[WebView goBack] and -goForward always return YES when called from the main thread
    return YES;
#endif
}

- (BOOL)goToBackForwardItem:(WebHistoryItem *)item
{
    if (!_private->page)
        return NO;

    ASSERT(item);
    _private->page->goToItem(*core(item), WebCore::FrameLoadType::IndexedBackForward, WebCore::ShouldTreatAsContinuingLoad::No);
    return YES;
}

- (void)setTextSizeMultiplier:(float)m
{
    [self _setZoomMultiplier:m isTextOnly:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (float)textSizeMultiplier
{
    return [self _realZoomMultiplierIsTextOnly] ? _private->zoomMultiplier : 1.0f;
}

- (void)_setZoomMultiplier:(float)multiplier isTextOnly:(BOOL)isTextOnly
{
    // NOTE: This has no visible effect when viewing a PDF (see <rdar://problem/4737380>)
    _private->zoomMultiplier = multiplier;
    _private->zoomsTextOnly = isTextOnly;

    [self hideFormValidationMessage];

    // FIXME: It might be nice to rework this code so that _private->zoomMultiplier doesn't exist
    // and instead the zoom factors stored in Frame are used.
    auto* coreFrame = [self _mainCoreFrame];
    if (coreFrame) {
        if (_private->zoomsTextOnly)
            coreFrame->setPageAndTextZoomFactors(1, multiplier);
        else
            coreFrame->setPageAndTextZoomFactors(multiplier, 1);
    }
}

- (float)_zoomMultiplier:(BOOL)isTextOnly
{
    if (isTextOnly != [self _realZoomMultiplierIsTextOnly])
        return 1.0f;
    return _private->zoomMultiplier;
}

- (float)_realZoomMultiplier
{
    return _private->zoomMultiplier;
}

- (BOOL)_realZoomMultiplierIsTextOnly
{
    if (!_private->page)
        return NO;

    return _private->zoomsTextOnly;
}

#define MinimumZoomMultiplier       0.5f
#define MaximumZoomMultiplier       3.0f
#define ZoomMultiplierRatio         1.2f

- (BOOL)_canZoomOut:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _canZoomOut];
    }
    return [self _zoomMultiplier:isTextOnly] / ZoomMultiplierRatio > MinimumZoomMultiplier;
}


- (BOOL)_canZoomIn:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _canZoomIn];
    }
    return [self _zoomMultiplier:isTextOnly] * ZoomMultiplierRatio < MaximumZoomMultiplier;
}

- (IBAction)_zoomOut:(id)sender isTextOnly:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _zoomOut:sender];
    }
    float newScale = [self _zoomMultiplier:isTextOnly] / ZoomMultiplierRatio;
    if (newScale > MinimumZoomMultiplier)
        [self _setZoomMultiplier:newScale isTextOnly:isTextOnly];
}

- (IBAction)_zoomIn:(id)sender isTextOnly:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _zoomIn:sender];
    }
    float newScale = [self _zoomMultiplier:isTextOnly] * ZoomMultiplierRatio;
    if (newScale < MaximumZoomMultiplier)
        [self _setZoomMultiplier:newScale isTextOnly:isTextOnly];
}

- (BOOL)_canResetZoom:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _canResetZoom];
    }
    return [self _zoomMultiplier:isTextOnly] != 1.0f;
}

- (IBAction)_resetZoom:(id)sender isTextOnly:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _resetZoom:sender];
    }
    if ([self _zoomMultiplier:isTextOnly] != 1.0f)
        [self _setZoomMultiplier:1.0f isTextOnly:isTextOnly];
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationName
{
    _private->applicationNameForUserAgent = adoptNS([applicationName copy]);
    if (!_private->userAgentOverridden)
        [self _invalidateUserAgentCache];
}

- (void)_invalidateUserAgentCache
{
    if (_private->userAgent.isNull())
        return;

    _private->userAgent = String();
    if (_private->page)
        _private->page->userAgentChanged();
}

- (NSString *)applicationNameForUserAgent
{
    auto applicationNameForUserAgentCopy = _private->applicationNameForUserAgent;
    return applicationNameForUserAgentCopy.autorelease();
}

- (void)setCustomUserAgent:(NSString *)userAgentString
{
    [self _invalidateUserAgentCache];
    _private->userAgent = userAgentString;
    _private->userAgentOverridden = userAgentString != nil;
}

- (NSString *)customUserAgent
{
    if (!_private->userAgentOverridden)
        return nil;
    return _private->userAgent;
}

- (void)setMediaStyle:(NSString *)mediaStyle
{
    if (_private->mediaStyle != mediaStyle) {
        _private->mediaStyle = adoptNS([mediaStyle copy]);
    }
}

- (NSString *)mediaStyle
{
    return _private->mediaStyle.get();
}

- (BOOL)supportsTextEncoding
{
    id documentView = [[[self mainFrame] frameView] documentView];
    return [documentView conformsToProtocol:@protocol(WebDocumentText)]
        && [documentView supportsTextEncoding];
}

- (void)setCustomTextEncodingName:(NSString *)encoding
{
    WebCoreThreadViolationCheckRoundThree();

    NSString *oldEncoding = [self customTextEncodingName];
    if (encoding == oldEncoding || [encoding isEqualToString:oldEncoding])
        return;
    if (auto* mainFrame = [self _mainCoreFrame])
        mainFrame->loader().reloadWithOverrideEncoding(encoding);
}

- (NSString *)_mainFrameOverrideEncoding
{
    WebDataSource *dataSource = [[self mainFrame] provisionalDataSource];
    if (dataSource == nil)
        dataSource = [[self mainFrame] _dataSource];
    if (dataSource == nil)
        return nil;
    return nsStringNilIfEmpty([dataSource _documentLoader]->overrideEncoding());
}

- (NSString *)customTextEncodingName
{
    return [self _mainFrameOverrideEncoding];
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script
{
    WebCoreThreadViolationCheckRoundThree();

#if !PLATFORM(IOS_FAMILY)
    // Return statements are only valid in a function but some applications pass in scripts
    // prefixed with return (<rdar://problems/5103720&4616860>) since older WebKit versions
    // silently ignored the return. If the application is linked against an earlier version
    // of WebKit we will strip the return so the script wont fail.
    if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_JAVASCRIPT_RETURN_QUIRK)) {
        NSRange returnStringRange = [script rangeOfString:@"return "];
        if (returnStringRange.length && !returnStringRange.location)
            script = [script substringFromIndex:returnStringRange.location + returnStringRange.length];
    }
#endif

    NSString *result = [[self mainFrame] _stringByEvaluatingJavaScriptFromString:script];
    // The only way stringByEvaluatingJavaScriptFromString can return nil is if the frame was removed by the script
    // Since there's no way to get rid of the main frame, result will never ever be nil here.
    ASSERT(result);

    return result;
}

- (WebScriptObject *)windowScriptObject
{
    WebCoreThreadViolationCheckRoundThree();

    auto* coreFrame = [self _mainCoreFrame];
    if (!coreFrame)
        return nil;
    return coreFrame->script().windowScriptObject();
}

- (String)_userAgentString
{
    if (_private->userAgent.isNull())
        _private->userAgent = [[self class] _standardUserAgentWithApplicationName:_private->applicationNameForUserAgent.get()];

    return _private->userAgent;
}

// Get the appropriate user-agent string for a particular URL.
- (NSString *)userAgentForURL:(NSURL *)url
{
    return [self _userAgentString];
}

- (void)setHostWindow:(NSWindow *)hostWindow
{
    if (_private->closed && hostWindow)
        return;
    if (hostWindow == _private->hostWindow)
        return;

    auto* coreFrame = [self _mainCoreFrame];
#if !PLATFORM(IOS_FAMILY)
    for (auto* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame))
        [[[kit(frame) frameView] documentView] viewWillMoveToHostWindow:hostWindow];
    if (_private->hostWindow && [self window] != _private->hostWindow)
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillCloseNotification object:_private->hostWindow.get()];
    if (hostWindow)
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowWillClose:) name:NSWindowWillCloseNotification object:hostWindow];
#endif
    _private->hostWindow = hostWindow;
    for (auto* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame))
        [[[kit(frame) frameView] documentView] viewDidMoveToHostWindow];
#if !PLATFORM(IOS_FAMILY)
    _private->page->setDeviceScaleFactor([self _deviceScaleFactor]);
#endif
}

- (NSWindow *)hostWindow
{
    // -[WebView hostWindow] can sometimes be called from the WebView's [super dealloc] method
    // so we check here to make sure it's not null.
    if (!_private)
        return nil;

    return _private->hostWindow.get();
}

- (NSView <WebDocumentView> *)documentViewAtWindowPoint:(NSPoint)point
{
    return [[self _frameViewAtWindowPoint:point] documentView];
}

- (NSDictionary *)_elementAtWindowPoint:(NSPoint)windowPoint
{
    WebFrameView *frameView = [self _frameViewAtWindowPoint:windowPoint];
    if (!frameView)
        return nil;
    NSView <WebDocumentView> *documentView = [frameView documentView];
    if ([documentView conformsToProtocol:@protocol(WebDocumentElement)]) {
        NSPoint point = [documentView convertPoint:windowPoint fromView:nil];
        return [(NSView <WebDocumentElement> *)documentView elementAtPoint:point];
    }
    return @{ WebElementFrameKey: [frameView webFrame] };
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    return [self _elementAtWindowPoint:[self convertPoint:point toView:nil]];
}

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
// The following 2 internal NSView methods are called on the drag destination to make scrolling while dragging work.
// Scrolling while dragging will only work if the drag destination is in a scroll view. The WebView is the drag destination.
// When dragging to a WebView, the document subview should scroll, but it doesn't because it is not the drag destination.
// Forward these calls to the document subview to make its scroll view scroll.
- (void)_autoscrollForDraggingInfo:(id)draggingInfo timeDelta:(NSTimeInterval)repeatDelta
{
    NSView <WebDocumentView> *documentView = [self documentViewAtWindowPoint:[draggingInfo draggingLocation]];
    [documentView _autoscrollForDraggingInfo:draggingInfo timeDelta:repeatDelta];
}

- (BOOL)_shouldAutoscrollForDraggingInfo:(id)draggingInfo
{
    NSView <WebDocumentView> *documentView = [self documentViewAtWindowPoint:[draggingInfo draggingLocation]];
    return [documentView _shouldAutoscrollForDraggingInfo:draggingInfo];
}

- (OptionSet<WebCore::DragApplicationFlags>)_applicationFlagsForDrag:(id <NSDraggingInfo>)draggingInfo
{
    OptionSet<WebCore::DragApplicationFlags> flags;
    if ([NSApp modalWindow])
        flags.add(WebCore::DragApplicationFlags::IsModal);
    if ([[self window] attachedSheet])
        flags.add(WebCore::DragApplicationFlags::HasAttachedSheet);
    if ([draggingInfo draggingSource] == self)
        flags.add(WebCore::DragApplicationFlags::IsSource);
    if ([[NSApp currentEvent] modifierFlags] & NSEventModifierFlagOption)
        flags.add(WebCore::DragApplicationFlags::IsCopyKeyDown);
    return flags;
}

- (OptionSet<WebCore::DragDestinationAction>)actionMaskForDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    return coreDragDestinationActionMask([[self _UIDelegateForwarder] webView:self dragDestinationActionMaskForDraggingInfo:draggingInfo]);
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)draggingInfo
{
    WebCore::IntPoint client([draggingInfo draggingLocation]);
    WebCore::IntPoint global(WebCore::globalPoint([draggingInfo draggingLocation], [self window]));

    WebCore::DragData dragData(draggingInfo, client, global, coreDragOperationMask([draggingInfo draggingSourceOperationMask]), [self _applicationFlagsForDrag:draggingInfo], [self actionMaskForDraggingInfo:draggingInfo]);
    return kit(core(self)->dragController().dragEntered(dragData));
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)draggingInfo
{
    auto* page = core(self);
    if (!page)
        return NSDragOperationNone;

    WebCore::IntPoint client([draggingInfo draggingLocation]);
    WebCore::IntPoint global(WebCore::globalPoint([draggingInfo draggingLocation], [self window]));

    WebCore::DragData dragData(draggingInfo, client, global, coreDragOperationMask([draggingInfo draggingSourceOperationMask]), [self _applicationFlagsForDrag:draggingInfo], [self actionMaskForDraggingInfo:draggingInfo]);
    return kit(page->dragController().dragUpdated(dragData));
}

- (void)draggingExited:(id <NSDraggingInfo>)draggingInfo
{
    auto* page = core(self);
    if (!page)
        return;

    WebCore::IntPoint client([draggingInfo draggingLocation]);
    WebCore::IntPoint global(WebCore::globalPoint([draggingInfo draggingLocation], [self window]));
    WebCore::DragData dragData(draggingInfo, client, global, coreDragOperationMask([draggingInfo draggingSourceOperationMask]), [self _applicationFlagsForDrag:draggingInfo]);
    page->dragController().dragExited(dragData);
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    WebCore::IntPoint client([draggingInfo draggingLocation]);
    WebCore::IntPoint global(WebCore::globalPoint([draggingInfo draggingLocation], [self window]));
    auto* dragData = new WebCore::DragData(draggingInfo, client, global, coreDragOperationMask([draggingInfo draggingSourceOperationMask]), [self _applicationFlagsForDrag:draggingInfo]);

    NSArray* types = draggingInfo.draggingPasteboard.types;
    if (![types containsObject:WebArchivePboardType] && [types containsObject:WebCore::legacyFilesPromisePasteboardType()]) {

        // FIXME: legacyFilesPromisePasteboardType() contains UTIs, not path names. Also, it's not
        // guaranteed that the count of UTIs equals the count of files, since some clients only write
        // unique UTIs.
        NSArray *files = [draggingInfo.draggingPasteboard propertyListForType:WebCore::legacyFilesPromisePasteboardType()];
        if (![files isKindOfClass:[NSArray class]]) {
            delete dragData;
            return false;
        }

        NSString *dropDestinationPath = FileSystem::createTemporaryDirectory(@"WebKitDropDestination");
        if (!dropDestinationPath) {
            delete dragData;
            return false;
        }

        size_t fileCount = files.count;
        Vector<String> *fileNames = new Vector<String>;
        NSURL *dropDestination = [NSURL fileURLWithPath:dropDestinationPath isDirectory:YES];
        [draggingInfo enumerateDraggingItemsWithOptions:0 forView:self classes:@[[NSFilePromiseReceiver class]] searchOptions:@{ } usingBlock:^(NSDraggingItem * __nonnull draggingItem, NSInteger idx, BOOL * __nonnull stop) {
            NSFilePromiseReceiver *item = draggingItem.item;
            NSDictionary *options = @{ };

            RetainPtr<NSOperationQueue> queue = adoptNS([NSOperationQueue new]);
            [item receivePromisedFilesAtDestination:dropDestination options:options operationQueue:queue.get() reader:^(NSURL * _Nonnull fileURL, NSError * _Nullable errorOrNil) {
                if (errorOrNil)
                    return;

                RunLoop::main().dispatch([self, path = RetainPtr<NSString>(fileURL.path), fileNames, fileCount, dragData] {
                    fileNames->append(path.get());
                    if (fileNames->size() == fileCount) {
                        dragData->setFileNames(*fileNames);
                        core(self)->dragController().performDragOperation(*dragData);
                        delete dragData;
                        delete fileNames;
                    }
                });
            }];
        }];

        return true;
    }
    bool returnValue = core(self)->dragController().performDragOperation(*dragData);
    delete dragData;

    return returnValue;
}

- (NSView *)_hitTest:(NSPoint *)point dragTypes:(NSSet *)types
{
    NSView *hitView = [super _hitTest:point dragTypes:types];
    if (!hitView && [[self superview] mouse:*point inRect:[self frame]])
        return self;
    return hitView;
}
#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)

- (BOOL)acceptsFirstResponder
{
    return [[[self mainFrame] frameView] acceptsFirstResponder];
}

- (BOOL)becomeFirstResponder
{
    if (_private->becomingFirstResponder) {
        // Fix for unrepro infinite recursion reported in Radar 4448181. If we hit this assert on
        // a debug build, we should figure out what causes the problem and do a better fix.
        ASSERT_NOT_REACHED();
        return NO;
    }

    // This works together with setNextKeyView to splice the WebView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebFrameView has very similar code.
#if !PLATFORM(IOS_FAMILY)
    NSWindow *window = [self window];
#endif
    WebFrameView *mainFrameView = [[self mainFrame] frameView];

#if !PLATFORM(IOS_FAMILY)
    NSResponder *previousFirstResponder = [[self window] _oldFirstResponderBeforeBecoming];
    BOOL fromOutside = ![previousFirstResponder isKindOfClass:[NSView class]] || (![(NSView *)previousFirstResponder isDescendantOf:self] && previousFirstResponder != self);

    if ([window keyViewSelectionDirection] == NSSelectingPrevious) {
        NSView *previousValidKeyView = [self previousValidKeyView];
        if (previousValidKeyView != self && previousValidKeyView != mainFrameView) {
            _private->becomingFirstResponder = YES;
            _private->becomingFirstResponderFromOutside = fromOutside;
            [window makeFirstResponder:previousValidKeyView];
            _private->becomingFirstResponderFromOutside = NO;
            _private->becomingFirstResponder = NO;
            return YES;
        }
        return NO;
    }
#endif

    if ([mainFrameView acceptsFirstResponder]) {
#if !PLATFORM(IOS_FAMILY)
        _private->becomingFirstResponder = YES;
        _private->becomingFirstResponderFromOutside = fromOutside;
        [window makeFirstResponder:mainFrameView];
        _private->becomingFirstResponderFromOutside = NO;
        _private->becomingFirstResponder = NO;
#endif
        return YES;
    }

    return NO;
}

- (NSView *)_webcore_effectiveFirstResponder
{
    if (WebFrameView *frameView = [[self mainFrame] frameView])
        return [frameView _webcore_effectiveFirstResponder];

    return [super _webcore_effectiveFirstResponder];
}

- (void)setNextKeyView:(NSView *)view
{
    // This works together with becomeFirstResponder to splice the WebView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebFrameView has similar code.
    if (WebFrameView *mainFrameView = [[self mainFrame] frameView]) {
        [mainFrameView setNextKeyView:view];
        return;
    }

    [super setNextKeyView:view];
}

static WebFrame *incrementFrame(WebFrame *frame, WebFindOptions options = 0)
{
    auto* coreFrame = core(frame);
    WebCore::CanWrap canWrap = options & WebFindOptionsWrapAround ? WebCore::CanWrap::Yes : WebCore::CanWrap::No;
    return kit((options & WebFindOptionsBackwards)
        ? coreFrame->tree().traversePrevious(canWrap)
        : coreFrame->tree().traverseNext(canWrap));
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    return [self searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapFlag startInSelection:NO];
}

+ (void)registerViewClass:(Class)viewClass representationClass:(Class)representationClass forMIMEType:(NSString *)MIMEType
{
    [[WebFrameView _viewTypesAllowImageTypeOmission:YES] setObject:viewClass forKey:MIMEType];
    [[WebDataSource _repTypesAllowImageTypeOmission:YES] setObject:representationClass forKey:MIMEType];

    // FIXME: We also need to maintain MIMEType registrations (which can be dynamically changed)
    // in the WebCore MIMEType registry.  For now we're doing this in a safe, limited manner
    // to fix <rdar://problem/5372989> - a future revamping of the entire system is neccesary for future robustness
    if ([viewClass class] == [WebHTMLView class])
        WebCore::MIMETypeRegistry::supportedNonImageMIMETypes().add(MIMEType);
}

- (void)setGroupName:(NSString *)groupName
{
    WebCoreThreadViolationCheckRoundThree();

    if (_private->group)
        _private->group->removeWebView(self);

    _private->group = WebViewGroup::getOrCreate(groupName, [_private->preferences _localStorageDatabasePath]);
    _private->group->addWebView(self);

    if (!_private->page)
        return;

    _private->page->setUserContentProvider(_private->group->userContentController());
    _private->page->setVisitedLinkStore(_private->group->visitedLinkStore());
    _private->page->setGroupName(groupName);
}

- (NSString *)groupName
{
    if (!_private->page)
        return nil;
    return _private->page->groupName();
}

- (double)estimatedProgress
{
    if (!_private->page)
        return 0.0;
    return _private->page->progress().estimatedProgress();
}

#if !PLATFORM(IOS_FAMILY)
- (NSArray *)pasteboardTypesForSelection
{
    NSView <WebDocumentView> *documentView = [[[self _selectedOrMainFrame] frameView] documentView];
    if ([documentView conformsToProtocol:@protocol(WebDocumentSelection)]) {
        return [(NSView <WebDocumentSelection> *)documentView pasteboardTypesForSelection];
    }
    return @[];
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    WebFrame *frame = [self _selectedOrMainFrame];
    if (frame && [frame _hasSelection]) {
        NSView <WebDocumentView> *documentView = [[frame frameView] documentView];
        if ([documentView conformsToProtocol:@protocol(WebDocumentSelection)])
            [(NSView <WebDocumentSelection> *)documentView writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
    }
}

- (NSArray *)pasteboardTypesForElement:(NSDictionary *)element
{
    if ([element objectForKey:WebElementImageURLKey] != nil) {
        return [NSPasteboard _web_writableTypesForImageIncludingArchive:([element objectForKey:WebElementDOMNodeKey] != nil)];
    } else if ([element objectForKey:WebElementLinkURLKey] != nil) {
        return [NSPasteboard _web_writableTypesForURL];
    } else if ([[element objectForKey:WebElementIsSelectedKey] boolValue]) {
        return [self pasteboardTypesForSelection];
    }
    return @[];
}

- (void)writeElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    if ([element objectForKey:WebElementImageURLKey] != nil) {
        [self _writeImageForElement:element withPasteboardTypes:types toPasteboard:pasteboard];
    } else if ([element objectForKey:WebElementLinkURLKey] != nil) {
        [self _writeLinkElement:element withPasteboardTypes:types toPasteboard:pasteboard];
    } else if ([[element objectForKey:WebElementIsSelectedKey] boolValue]) {
        [self writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
    }
}

- (void)moveDragCaretToPoint:(NSPoint)point
{
#if ENABLE(DRAG_SUPPORT)
    if (auto* page = core(self))
        page->dragController().placeDragCaret(WebCore::IntPoint([self convertPoint:point toView:nil]));
#endif
}

- (void)removeDragCaret
{
#if ENABLE(DRAG_SUPPORT)
    if (auto* page = core(self))
        page->dragController().dragEnded();
#endif
}
#endif // !PLATFORM(IOS_FAMILY)

- (void)setMainFrameURL:(NSString *)URLString
{
    WebCoreThreadViolationCheckRoundThree();

    NSURL *url;
    if ([URLString hasPrefix:@"/"])
        url = [NSURL fileURLWithPath:URLString isDirectory:NO];
    else
        url = [NSURL _web_URLWithDataAsString:URLString];

    [[self mainFrame] loadRequest:[NSURLRequest requestWithURL:url]];
}

- (NSString *)mainFrameURL
{
    WebDataSource *ds;
    ds = [[self mainFrame] provisionalDataSource];
    if (!ds)
        ds = [[self mainFrame] _dataSource];
    return [[[ds request] URL] _web_originalDataAsString];
}

- (BOOL)isLoading
{
    LOG (Bindings, "isLoading = %d", (int)[self _isLoading]);
    return [self _isLoading];
}

- (NSString *)mainFrameTitle
{
    WebCoreThreadViolationCheckRoundThree();

    NSString *mainFrameTitle = [[[self mainFrame] _dataSource] pageTitle];
    return (mainFrameTitle != nil) ? mainFrameTitle : (NSString *)@"";
}

#if !PLATFORM(IOS_FAMILY)
- (NSImage *)mainFrameIcon
{
    WebCoreThreadViolationCheckRoundThree();

    if (auto *icon = _private->_mainFrameIcon.get())
        return icon;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [[WebIconDatabase sharedIconDatabase] defaultIconWithSize:WebIconSmallSize];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (void)_setMainFrameIcon:(NSImage *)icon
{
    if (_private->_mainFrameIcon.get() == icon)
        return;

    [self _willChangeValueForKey:_WebMainFrameIconKey];

    _private->_mainFrameIcon = icon;

    WebFrameLoadDelegateImplementationCache* cache = &_private->frameLoadDelegateImplementations;
    if (icon && cache->didReceiveIconForFrameFunc)
        CallFrameLoadDelegate(cache->didReceiveIconForFrameFunc, self, @selector(webView:didReceiveIcon:forFrame:), icon, [self mainFrame]);

    [self _didChangeValueForKey:_WebMainFrameIconKey];
}
#else
- (NSURL *)mainFrameIconURL
{
    WebFrame *mainFrame = [self mainFrame];
    auto* coreMainFrame = core(mainFrame);
    if (!coreMainFrame)
        return nil;

    auto* documentLoader = coreMainFrame->loader().documentLoader();
    if (!documentLoader)
        return nil;

    auto& linkIcons = documentLoader->linkIcons();
    if (linkIcons.isEmpty())
        return nil;

    // We arbitrarily choose the first icon in the list if there is more than one.
    return (NSURL *)linkIcons[0].url;
}
#endif

- (DOMDocument *)mainFrameDocument
{
    // only return the actual value if the state we're in gives NSTreeController
    // enough time to release its observers on the old model
    if (_private->mainFrameDocumentReady)
        return [[self mainFrame] DOMDocument];
    return nil;
}

- (void)setDrawsBackground:(BOOL)drawsBackground
{
    WebCoreThreadViolationCheckRoundThree();

    if (_private->drawsBackground == drawsBackground)
        return;
    _private->drawsBackground = drawsBackground;
    [[self mainFrame] _updateBackgroundAndUpdatesWhileOffscreen];
}

- (BOOL)drawsBackground
{
    // This method can be called beneath -[NSView dealloc] after we have cleared _private,
    // indirectly via -[WebFrameView viewDidMoveToWindow].
    return !_private || _private->drawsBackground;
}

- (void)setShouldUpdateWhileOffscreen:(BOOL)updateWhileOffscreen
{
    WebCoreThreadViolationCheckRoundThree();

    if (_private->shouldUpdateWhileOffscreen == updateWhileOffscreen)
        return;
    _private->shouldUpdateWhileOffscreen = updateWhileOffscreen;
    [[self mainFrame] _updateBackgroundAndUpdatesWhileOffscreen];
}

- (BOOL)shouldUpdateWhileOffscreen
{
    return _private->shouldUpdateWhileOffscreen;
}

- (void)setCurrentNodeHighlight:(WebNodeHighlight *)nodeHighlight
{
    _private->currentNodeHighlight = nodeHighlight;
}

- (WebNodeHighlight *)currentNodeHighlight
{
    return _private->currentNodeHighlight.get();
}

- (NSView *)previousValidKeyView
{
    NSView *result = [super previousValidKeyView];

    // Work around AppKit bug 6905484. If the result is a view that's inside this one, it's
    // possible it is the wrong answer, because the fact that it's a descendant causes the
    // code that implements key view redirection to fail; this means we won't redirect to
    // the toolbar, for example, when we hit the edge of a window. Since the bug is specific
    // to cases where the receiver of previousValidKeyView is an ancestor of the last valid
    // key view in the loop, we can sidestep it by walking along previous key views until
    // we find one that is not a superview, then using that to call previousValidKeyView.

    if (![result isDescendantOf:self])
        return result;

    // Use a visited set so we don't loop indefinitely when walking crazy key loops.
    // AppKit uses such sets internally and we want our loop to be as robust as its loops.
    RetainPtr<CFMutableSetRef> visitedViews = adoptCF(CFSetCreateMutable(0, 0, 0));
    CFSetAddValue(visitedViews.get(), result);

    NSView *previousView = self;
    do {
        CFSetAddValue(visitedViews.get(), previousView);
        previousView = [previousView previousKeyView];
        if (!previousView || CFSetGetValue(visitedViews.get(), previousView))
            return result;
    } while ([result isDescendantOf:previousView]);
    return [previousView previousValidKeyView];
}

#if HAVE(TOUCH_BAR)

@dynamic touchBar;

- (NSTouchBar *)makeTouchBar
{
    if (!_private->_canCreateTouchBars) {
        _private->_canCreateTouchBars = YES;
        [self updateTouchBar];
    }
    return _private->_currentTouchBar.get();
}

- (NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar makeItemForIdentifier:(NSString *)identifier
{
    if (touchBar == _private->_richTextTouchBar || touchBar == _private->_plainTextTouchBar)
        return [_private->_textTouchBarItemController itemForIdentifier:identifier];

    return nil;
}

constexpr WebCore::TextCheckingType coreTextCheckingType(NSTextCheckingType type)
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

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem *)anItem endSelectingCandidateAtIndex:(NSInteger)index
{
    if (index == NSNotFound)
        return;

    if (anItem != self.candidateList)
        return;

    NSArray *candidates = anItem.candidates;
    if ((NSUInteger)index >= candidates.count)
        return;

    id candidate = candidates[index];
    ASSERT([candidate isKindOfClass:[NSTextCheckingResult class]]);

    if (auto* coreFrame = core(self._selectedOrMainFrame))
        coreFrame->editor().client()->handleAcceptedCandidateWithSoftSpaces(textCheckingResultFromNSTextCheckingResult((NSTextCheckingResult *)candidate));
}

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem *)anItem changedCandidateListVisibility:(BOOL)isVisible
{
    if (anItem != self.candidateList)
        return;

    if (isVisible) {
        if (auto* coreFrame = core([self _selectedOrMainFrame]))
            coreFrame->editor().client()->requestCandidatesForSelection(coreFrame->selection().selection());
    }

    [self updateTouchBar];
}

#endif // HAVE(TOUCH_BAR)

static WebFrameView *containingFrameView(NSView *view)
{
    while (view && ![view isKindOfClass:[WebFrameView class]])
        view = [view superview];
    return (WebFrameView *)view;
}

#if !PLATFORM(IOS_FAMILY)
- (float)_deviceScaleFactor
{
    if (_private->customDeviceScaleFactor != 0)
        return _private->customDeviceScaleFactor;

    NSWindow *window = [self window];
    NSWindow *hostWindow = [self hostWindow];
    if (window)
        return [window backingScaleFactor];
    if (hostWindow)
        return [hostWindow backingScaleFactor];
    return [[NSScreen mainScreen] backingScaleFactor];
}
#endif

+ (BOOL)_didSetCacheModel
{
    return s_didSetCacheModel;
}

+ (WebCacheModel)_maxCacheModelInAnyInstance
{
    WebCacheModel cacheModel = WebCacheModelDocumentViewer;
    NSEnumerator *enumerator = [(NSMutableSet *)allWebViewsSet().get() objectEnumerator];
    while (WebPreferences *preferences = [[enumerator nextObject] preferences])
        cacheModel = std::max(cacheModel, [preferences cacheModel]);
    return cacheModel;
}

+ (void)_cacheModelChangedNotification:(NSNotification *)notification
{
#if PLATFORM(IOS_FAMILY)
    // This needs to happen on the Web Thread
    WebThreadRun(^{
#endif
    WebPreferences *preferences = (WebPreferences *)[notification object];
    ASSERT([preferences isKindOfClass:[WebPreferences class]]);

    WebCacheModel cacheModel = [preferences cacheModel];
    if (![self _didSetCacheModel] || cacheModel > [self _cacheModel])
        [self _setCacheModel:cacheModel];
    else if (cacheModel < [self _cacheModel])
        [self _setCacheModel:std::max([[WebPreferences standardPreferences] cacheModel], [self _maxCacheModelInAnyInstance])];
#if PLATFORM(IOS_FAMILY)
    });
#endif
}

+ (void)_preferencesRemovedNotification:(NSNotification *)notification
{
    WebPreferences *preferences = (WebPreferences *)[notification object];
    ASSERT([preferences isKindOfClass:[WebPreferences class]]);

    if ([preferences cacheModel] == [self _cacheModel])
        [self _setCacheModel:std::max([[WebPreferences standardPreferences] cacheModel], [self _maxCacheModelInAnyInstance])];
}

- (WebFrame *)_focusedFrame
{
    NSResponder *resp = [[self window] firstResponder];
    if (resp && [resp isKindOfClass:[NSView class]] && [(NSView *)resp isDescendantOf:[[self mainFrame] frameView]]) {
        WebFrameView *frameView = containingFrameView((NSView *)resp);
        ASSERT(frameView != nil);
        return [frameView webFrame];
    }

    return nil;
}

- (BOOL)_isLoading
{
    WebFrame *mainFrame = [self mainFrame];
    return [[mainFrame _dataSource] isLoading]
        || [[mainFrame provisionalDataSource] isLoading];
}

- (WebFrameView *)_frameViewAtWindowPoint:(NSPoint)point
{
    if (_private->closed)
        return nil;
#if !PLATFORM(IOS_FAMILY)
    NSView *view = [self hitTest:[[self superview] convertPoint:point fromView:nil]];
#else
    //[WebView superview] on iOS is nil, don't do a convertPoint
    NSView *view = [self hitTest:point];
#endif
    if (![view isDescendantOf:[[self mainFrame] frameView]])
        return nil;
    WebFrameView *frameView = containingFrameView(view);
    ASSERT(frameView);
    return frameView;
}

+ (void)_preflightSpellCheckerNow:(id)sender
{
#if !PLATFORM(IOS_FAMILY)
    [[NSSpellChecker sharedSpellChecker] _preflightChosenSpellServer];
#endif
}

+ (void)_preflightSpellChecker
{
#if !PLATFORM(IOS_FAMILY)
    // As AppKit does, we wish to delay tickling the shared spellchecker into existence on application launch.
    if ([NSSpellChecker sharedSpellCheckerExists]) {
        [self _preflightSpellCheckerNow:self];
    } else {
        [self performSelector:@selector(_preflightSpellCheckerNow:) withObject:self afterDelay:2.0];
    }
#endif
}

- (BOOL)_continuousCheckingAllowed
{
    static BOOL allowContinuousSpellChecking = YES;
    static BOOL readAllowContinuousSpellCheckingDefault = NO;
    if (!readAllowContinuousSpellCheckingDefault) {
        if ([[NSUserDefaults standardUserDefaults] objectForKey:@"NSAllowContinuousSpellChecking"]) {
            allowContinuousSpellChecking = [[NSUserDefaults standardUserDefaults] boolForKey:@"NSAllowContinuousSpellChecking"];
        }
        readAllowContinuousSpellCheckingDefault = YES;
    }
    return allowContinuousSpellChecking;
}

- (NSResponder *)_responderForResponderOperations
{
    NSResponder *responder = [[self window] firstResponder];
    WebFrameView *mainFrameView = [[self mainFrame] frameView];

    // If the current responder is outside of the webview, use our main frameView or its
    // document view. We also do this for subviews of self that are siblings of the main
    // frameView since clients might insert non-webview-related views there (see 4552713).
    if (responder != self && ![mainFrameView _web_firstResponderIsSelfOrDescendantView]) {
        responder = [mainFrameView documentView];
        if (!responder)
            responder = mainFrameView;
    }
    return responder;
}

@end

@implementation WebView (WebIBActions)

- (IBAction)takeStringURLFrom: sender
{
    NSString *URLString = [sender stringValue];

    [[self mainFrame] loadRequest: [NSURLRequest requestWithURL: [NSURL _web_URLWithDataAsString: URLString]]];
}

- (BOOL)canGoBack
{
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
    if (!_private->page)
#else
    if (!_private->page || _private->page->defersLoading())
#endif
        return NO;

    return _private->page->backForward().canGoBackOrForward(-1);
}

- (BOOL)canGoForward
{
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
    if (!_private->page)
#else
    if (!_private->page || _private->page->defersLoading())
#endif
        return NO;

    return !!_private->page->backForward().canGoBackOrForward(1);
}

- (IBAction)goBack:(id)sender
{
    [self goBack];
}

- (IBAction)goForward:(id)sender
{
    [self goForward];
}

- (IBAction)stopLoading:(id)sender
{
#if PLATFORM(IOS_FAMILY)
    if (WebThreadNotCurrent()) {
        _private->isStopping = true;
        WebThreadSetShouldYield();
    }
    WebThreadRun(^{
        _private->isStopping = false;
#endif
    [[self mainFrame] stopLoading];
#if PLATFORM(IOS_FAMILY)
    });
#endif
}

#if PLATFORM(IOS_FAMILY)
- (void)stopLoadingAndClear
{
    if (WebThreadNotCurrent() && !WebThreadIsLocked()) {
        _private->isStopping = true;
        WebThreadSetShouldYield();
    }
    WebThreadRun(^{
        _private->isStopping = false;

        WebFrame *frame = [self mainFrame];
        [frame stopLoading];
        core(frame)->document()->loader()->writer().end(); // End to finish parsing and display immediately

        WebFrameView *mainFrameView = [frame frameView];
        float scale = [[mainFrameView documentView] scale];
        auto plainWhiteView = adoptNS([[WebPlainWhiteView alloc] initWithFrame:NSZeroRect]);
        [plainWhiteView setScale:scale];
        [plainWhiteView setFrame:[mainFrameView bounds]];
        [mainFrameView _setDocumentView:plainWhiteView.get()];
        [plainWhiteView setNeedsDisplay:YES];
    });
}
#endif

- (IBAction)reload:(id)sender
{
#if PLATFORM(IOS_FAMILY)
    WebThreadRun(^{
#endif
    [[self mainFrame] reload];
#if PLATFORM(IOS_FAMILY)
    });
#endif
}

- (IBAction)reloadFromOrigin:(id)sender
{
    [[self mainFrame] reloadFromOrigin];
}

// FIXME: This code should move into WebCore so that it is not duplicated in each WebKit.
// (This includes canMakeTextSmaller/Larger, makeTextSmaller/Larger, and canMakeTextStandardSize/makeTextStandardSize)
- (BOOL)canMakeTextSmaller
{
    return [self _canZoomOut:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (IBAction)makeTextSmaller:(id)sender
{
    return [self _zoomOut:sender isTextOnly:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (BOOL)canMakeTextLarger
{
    return [self _canZoomIn:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (IBAction)makeTextLarger:(id)sender
{
    return [self _zoomIn:sender isTextOnly:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (BOOL)canMakeTextStandardSize
{
    return [self _canResetZoom:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (IBAction)makeTextStandardSize:(id)sender
{
   return [self _resetZoom:sender isTextOnly:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (IBAction)toggleContinuousSpellChecking:(id)sender
{
    [self setContinuousSpellCheckingEnabled:![self isContinuousSpellCheckingEnabled]];
}

#if !PLATFORM(IOS_FAMILY)
- (IBAction)toggleSmartInsertDelete:(id)sender
{
    [self setSmartInsertDeleteEnabled:![self smartInsertDeleteEnabled]];
}

- (BOOL)_responderValidateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    id responder = [self _responderForResponderOperations];
    if (responder != self && [responder respondsToSelector:[item action]]) {
        if ([responder respondsToSelector:@selector(validateUserInterfaceItemWithoutDelegate:)])
            return [responder validateUserInterfaceItemWithoutDelegate:item];
        if ([responder respondsToSelector:@selector(validateUserInterfaceItem:)])
            return [responder validateUserInterfaceItem:item];
        return YES;
    }
    return NO;
}

#define VALIDATE(name) \
    else if (action == @selector(name:)) { return [self _responderValidateUserInterfaceItem:item]; }

- (BOOL)validateUserInterfaceItemWithoutDelegate:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(goBack:)) {
        return [self canGoBack];
    } else if (action == @selector(goForward:)) {
        return [self canGoForward];
    } else if (action == @selector(makeTextLarger:)) {
        return [self canMakeTextLarger];
    } else if (action == @selector(makeTextSmaller:)) {
        return [self canMakeTextSmaller];
    } else if (action == @selector(makeTextStandardSize:)) {
        return [self canMakeTextStandardSize];
    } else if (action == @selector(reload:)) {
        return [[self mainFrame] _dataSource] != nil;
    } else if (action == @selector(stopLoading:)) {
        return [self _isLoading];
    } else if (action == @selector(toggleContinuousSpellChecking:)) {
        BOOL checkMark = NO;
        BOOL retVal = NO;
        if ([self _continuousCheckingAllowed]) {
            checkMark = [self isContinuousSpellCheckingEnabled];
            retVal = YES;
        }
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSControlStateValueOn : NSControlStateValueOff];
        }
        return retVal;
    } else if (action == @selector(toggleSmartInsertDelete:)) {
        BOOL checkMark = [self smartInsertDeleteEnabled];
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSControlStateValueOn : NSControlStateValueOff];
        }
        return YES;
    } else if (action == @selector(toggleGrammarChecking:)) {
        BOOL checkMark = [self isGrammarCheckingEnabled];
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSControlStateValueOn : NSControlStateValueOff];
        }
        return YES;
    } else if (action == @selector(toggleAutomaticQuoteSubstitution:)) {
        BOOL checkMark = [self isAutomaticQuoteSubstitutionEnabled];
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSControlStateValueOn : NSControlStateValueOff];
        }
        return YES;
    } else if (action == @selector(toggleAutomaticLinkDetection:)) {
        BOOL checkMark = [self isAutomaticLinkDetectionEnabled];
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSControlStateValueOn : NSControlStateValueOff];
        }
        return YES;
    } else if (action == @selector(toggleAutomaticDashSubstitution:)) {
        BOOL checkMark = [self isAutomaticDashSubstitutionEnabled];
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSControlStateValueOn : NSControlStateValueOff];
        }
        return YES;
    } else if (action == @selector(toggleAutomaticTextReplacement:)) {
        BOOL checkMark = [self isAutomaticTextReplacementEnabled];
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSControlStateValueOn : NSControlStateValueOff];
        }
        return YES;
    } else if (action == @selector(toggleAutomaticSpellingCorrection:)) {
        BOOL checkMark = [self isAutomaticSpellingCorrectionEnabled];
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSControlStateValueOn : NSControlStateValueOff];
        }
        return YES;
    }
    FOR_EACH_RESPONDER_SELECTOR(VALIDATE)

    return YES;
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    BOOL result = [self validateUserInterfaceItemWithoutDelegate:item];
    return CallUIDelegateReturningBoolean(result, self, @selector(webView:validateUserInterfaceItem:defaultValidation:), item, result);
}
#endif // !PLATFORM(IOS_FAMILY)

@end

@implementation WebView (WebPendingPublic)

- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode
{
    if (runLoop && mode)
        core(self)->addSchedulePair(SchedulePair::create(runLoop, (CFStringRef)mode));
}

- (void)unscheduleFromRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode
{
    if (runLoop && mode)
        core(self)->removeSchedulePair(SchedulePair::create(runLoop, (CFStringRef)mode));
}

static BOOL findString(NSView <WebDocumentSearching> *searchView, NSString *string, WebFindOptions options)
{
    if ([searchView conformsToProtocol:@protocol(WebDocumentOptionsSearching)])
        return [(NSView <WebDocumentOptionsSearching> *)searchView _findString:string options:options];
    if ([searchView conformsToProtocol:@protocol(WebDocumentIncrementalSearching)])
        return [(NSView <WebDocumentIncrementalSearching> *)searchView searchFor:string direction:!(options & WebFindOptionsBackwards) caseSensitive:!(options & WebFindOptionsCaseInsensitive) wrap:!!(options & WebFindOptionsWrapAround) startInSelection:!!(options & WebFindOptionsStartInSelection)];
    return [searchView searchFor:string direction:!(options & WebFindOptionsBackwards) caseSensitive:!(options & WebFindOptionsCaseInsensitive) wrap:!!(options & WebFindOptionsWrapAround)];
}

- (BOOL)findString:(NSString *)string options:(WebFindOptions)options
{
    if (_private->closed)
        return NO;

    // Get the frame holding the selection, or start with the main frame
    WebFrame *startFrame = [self _selectedOrMainFrame];

    // Search the first frame, then all the other frames, in order
    NSView <WebDocumentSearching> *startSearchView = nil;
    WebFrame *frame = startFrame;
    do {
        WebFrame *nextFrame = incrementFrame(frame, options);

        BOOL onlyOneFrame = (frame == nextFrame);
        ASSERT(!onlyOneFrame || frame == startFrame);

        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebDocumentSearching)]) {
            NSView <WebDocumentSearching> *searchView = (NSView <WebDocumentSearching> *)view;

            if (frame == startFrame)
                startSearchView = searchView;

            // In some cases we have to search some content twice; see comment later in this method.
            // We can avoid ever doing this in the common one-frame case by passing the wrap option through
            // here, and then bailing out before we get to the code that would search again in the
            // same content.
            WebFindOptions optionsForThisPass = onlyOneFrame ? options : (options & ~WebFindOptionsWrapAround);

            if (findString(searchView, string, optionsForThisPass)) {
                if (frame != startFrame)
                    [startFrame _clearSelection];
                [[self window] makeFirstResponder:searchView];
                return YES;
            }

            if (onlyOneFrame)
                return NO;
        }
        frame = nextFrame;
    } while (frame && frame != startFrame);

    // If there are multiple frames and WebFindOptionsWrapAround is set and we've visited each one without finding a result, we still need to search in the
    // first-searched frame up to the selection. However, the API doesn't provide a way to search only up to a particular point. The only
    // way to make sure the entire frame is searched is to pass WebFindOptionsWrapAround. When there are no matches, this will search
    // some content that we already searched on the first pass. In the worst case, we could search the entire contents of this frame twice.
    // To fix this, we'd need to add a mechanism to specify a range in which to search.
    if ((options & WebFindOptionsWrapAround) && startSearchView) {
        if (findString(startSearchView, string, options)) {
            [[self window] makeFirstResponder:startSearchView];
            return YES;
        }
    }
    return NO;
}

- (DOMRange *)DOMRangeOfString:(NSString *)string relativeTo:(DOMRange *)previousRange options:(WebFindOptions)options
{
    if (!_private->page)
        return nil;
    return kit(_private->page->rangeOfString(string, makeSimpleRange(core(previousRange)), coreOptions(options)));
}

- (void)setMainFrameDocumentReady:(BOOL)mainFrameDocumentReady
{
    // by setting this to NO, calls to mainFrameDocument are forced to return nil
    // setting this to YES lets it return the actual DOMDocument value
    // we use this to tell NSTreeController to reset its observers and clear its state
    if (_private->mainFrameDocumentReady == mainFrameDocumentReady)
        return;
#if !PLATFORM(IOS_FAMILY)
    [self _willChangeValueForKey:_WebMainFrameDocumentKey];
    _private->mainFrameDocumentReady = mainFrameDocumentReady;
    [self _didChangeValueForKey:_WebMainFrameDocumentKey];
    // this will cause observers to call mainFrameDocument where this flag will be checked
#endif
}

- (void)setTabKeyCyclesThroughElements:(BOOL)cyclesElements
{
    _private->tabKeyCyclesThroughElementsChanged = YES;
    if (_private->page)
        _private->page->setTabKeyCyclesThroughElements(cyclesElements);
}

- (BOOL)tabKeyCyclesThroughElements
{
    return _private->page && _private->page->tabKeyCyclesThroughElements();
}

- (void)setScriptDebugDelegate:(id)delegate
{
    _private->scriptDebugDelegate = delegate;
    [self _cacheScriptDebugDelegateImplementations];

    if (delegate)
        [self _attachScriptDebuggerToAllFrames];
    else
        [self _detachScriptDebuggerFromAllFrames];
}

- (id)scriptDebugDelegate
{
    return _private->scriptDebugDelegate;
}

- (void)setHistoryDelegate:(id)delegate
{
    _private->historyDelegate = delegate;
    [self _cacheHistoryDelegateImplementations];
}

- (id)historyDelegate
{
    return _private->historyDelegate;
}

- (BOOL)shouldClose
{
    auto* coreFrame = [self _mainCoreFrame];
    if (!coreFrame)
        return YES;
    return coreFrame->loader().shouldClose();
}

#if !PLATFORM(IOS_FAMILY)
static NSAppleEventDescriptor* aeDescFromJSValue(JSC::JSGlobalObject* lexicalGlobalObject, JSC::JSValue jsValue)
{
    using namespace JSC;
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    NSAppleEventDescriptor* aeDesc = 0;
    if (jsValue.isBoolean())
        return [NSAppleEventDescriptor descriptorWithBoolean:jsValue.asBoolean()];
    if (jsValue.isString())
        return [NSAppleEventDescriptor descriptorWithString:asString(jsValue)->value(lexicalGlobalObject)];
    if (jsValue.isNumber()) {
        double value = jsValue.asNumber();
        int intValue = value;
        if (value == intValue)
            return [NSAppleEventDescriptor descriptorWithDescriptorType:typeSInt32 bytes:&intValue length:sizeof(intValue)];
        return [NSAppleEventDescriptor descriptorWithDescriptorType:typeIEEE64BitFloatingPoint bytes:&value length:sizeof(value)];
    }
    if (jsValue.isObject()) {
        JSObject* object = jsValue.getObject();
        if (object->inherits<DateInstance>(vm)) {
            DateInstance* date = static_cast<DateInstance*>(object);
            double ms = date->internalNumber();
            if (!std::isnan(ms)) {
                CFAbsoluteTime utcSeconds = ms / 1000 - kCFAbsoluteTimeIntervalSince1970;
                LongDateTime ldt;
                if (noErr == UCConvertCFAbsoluteTimeToLongDateTime(utcSeconds, &ldt))
                    return [NSAppleEventDescriptor descriptorWithDescriptorType:typeLongDateTime bytes:&ldt length:sizeof(ldt)];
            }
        } else if (object->inherits<JSArray>(vm)) {
            static NeverDestroyed<HashSet<JSObject*>> visitedElems;
            if (visitedElems.get().add(object).isNewEntry) {
                JSArray* array = static_cast<JSArray*>(object);
                aeDesc = [NSAppleEventDescriptor listDescriptor];
                unsigned numItems = array->length();
                for (unsigned i = 0; i < numItems; ++i)
                    [aeDesc insertDescriptor:aeDescFromJSValue(lexicalGlobalObject, array->get(lexicalGlobalObject, i)) atIndex:0];
                visitedElems.get().remove(object);
                return aeDesc;
            }
        }
        JSC::JSValue primitive = object->toPrimitive(lexicalGlobalObject);
        if (UNLIKELY(scope.exception())) {
            scope.clearException();
            return [NSAppleEventDescriptor nullDescriptor];
        }
        return aeDescFromJSValue(lexicalGlobalObject, primitive);
    }
    if (jsValue.isUndefined())
        return [NSAppleEventDescriptor descriptorWithTypeCode:cMissingValue];
    ASSERT(jsValue.isNull());
    return [NSAppleEventDescriptor nullDescriptor];
}

- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)script
{
    auto* coreFrame = [self _mainCoreFrame];
    if (!coreFrame)
        return nil;
    if (!coreFrame->document())
        return nil;
    JSC::JSValue result = coreFrame->script().executeScriptIgnoringException(script, true);
    if (!result) // FIXME: pass errors
        return 0;
    JSC::JSLockHolder lock(coreFrame->script().globalObject(WebCore::mainThreadNormalWorld()));
    return aeDescFromJSValue(coreFrame->script().globalObject(WebCore::mainThreadNormalWorld()), result);
}
#endif

- (BOOL)canMarkAllTextMatches
{
    if (_private->closed)
        return NO;

    WebFrame *frame = [self mainFrame];
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if (view && ![view conformsToProtocol:@protocol(WebMultipleTextMatches)])
            return NO;

        frame = incrementFrame(frame);
    } while (frame);

    return YES;
}

- (NSUInteger)countMatchesForText:(NSString *)string options:(WebFindOptions)options highlight:(BOOL)highlight limit:(NSUInteger)limit markMatches:(BOOL)markMatches
{
    return [self countMatchesForText:string inDOMRange:nil options:options highlight:highlight limit:limit markMatches:markMatches];
}

- (NSUInteger)countMatchesForText:(NSString *)string inDOMRange:(DOMRange *)range options:(WebFindOptions)options highlight:(BOOL)highlight limit:(NSUInteger)limit markMatches:(BOOL)markMatches
{
    if (_private->closed)
        return 0;

    WebFrame *frame = [self mainFrame];
    unsigned matchCount = 0;
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebMultipleTextMatches)]) {
            if (markMatches)
                [(NSView <WebMultipleTextMatches>*)view setMarkedTextMatchesAreHighlighted:highlight];

            ASSERT(limit == 0 || matchCount < limit);
            matchCount += [(NSView <WebMultipleTextMatches>*)view countMatchesForText:string inDOMRange:range options:options limit:(limit == 0 ? 0 : limit - matchCount) markMatches:markMatches];

            // Stop looking if we've reached the limit. A limit of 0 means no limit.
            if (limit > 0 && matchCount >= limit)
                break;
        }

        frame = incrementFrame(frame);
    } while (frame);

    return matchCount;
}

- (void)unmarkAllTextMatches
{
    if (_private->closed)
        return;

    WebFrame *frame = [self mainFrame];
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebMultipleTextMatches)])
            [(NSView <WebMultipleTextMatches>*)view unmarkAllTextMatches];

        frame = incrementFrame(frame);
    } while (frame);
}

- (NSArray *)rectsForTextMatches
{
    if (_private->closed)
        return @[];

    NSMutableArray *result = [NSMutableArray array];
    WebFrame *frame = [self mainFrame];
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebMultipleTextMatches)]) {
            NSView <WebMultipleTextMatches> *documentView = (NSView <WebMultipleTextMatches> *)view;
            NSRect documentViewVisibleRect = [documentView visibleRect];
            for (NSValue *rect in [documentView rectsForTextMatches]) {
                NSRect r = [rect rectValue];
                // Clip rect to document view's visible rect so rect is confined to subframe
                r = NSIntersectionRect(r, documentViewVisibleRect);
                if (NSIsEmptyRect(r))
                    continue;

                @autoreleasepool {
                    // Convert rect to our coordinate system
                    r = [documentView convertRect:r toView:self];
                    [result addObject:[NSValue valueWithRect:r]];
                }
            }
        }

        frame = incrementFrame(frame);
    } while (frame);

    return result;
}

- (void)scrollDOMRangeToVisible:(DOMRange *)range
{
    [[[[range startContainer] ownerDocument] webFrame] _scrollDOMRangeToVisible:range];
}

#if PLATFORM(IOS_FAMILY)
- (void)scrollDOMRangeToVisible:(DOMRange *)range withInset:(CGFloat)inset
{
    [[[[range startContainer] ownerDocument] webFrame] _scrollDOMRangeToVisible:range withInset:inset];
}
#endif

- (BOOL)allowsUndo
{
    return _private->allowsUndo;
}

- (void)setAllowsUndo:(BOOL)flag
{
    _private->allowsUndo = flag;
}

- (void)setPageSizeMultiplier:(float)m
{
    [self _setZoomMultiplier:m isTextOnly:NO];
}

- (float)pageSizeMultiplier
{
    return ![self _realZoomMultiplierIsTextOnly] ? _private->zoomMultiplier : 1.0f;
}

- (BOOL)canZoomPageIn
{
    return [self _canZoomIn:NO];
}

- (IBAction)zoomPageIn:(id)sender
{
    return [self _zoomIn:sender isTextOnly:NO];
}

- (BOOL)canZoomPageOut
{
    return [self _canZoomOut:NO];
}

- (IBAction)zoomPageOut:(id)sender
{
    return [self _zoomOut:sender isTextOnly:NO];
}

- (BOOL)canResetPageZoom
{
    return [self _canResetZoom:NO];
}

- (IBAction)resetPageZoom:(id)sender
{
    return [self _resetZoom:sender isTextOnly:NO];
}

- (void)setMediaVolume:(float)volume
{
    if (_private->page)
        _private->page->setMediaVolume(volume);
}

- (float)mediaVolume
{
    if (!_private->page)
        return 0;

    return _private->page->mediaVolume();
}

- (void)suspendAllMediaPlayback
{
    if (_private->page)
        _private->page->suspendAllMediaPlayback();
}

- (void)resumeAllMediaPlayback
{
    if (_private->page)
        _private->page->resumeAllMediaPlayback();
}

#if PLATFORM(MAC)
- (BOOL)_allowsLinkPreview
{
    if (WebImmediateActionController *immediateActionController = _private->immediateActionController.get())
        return immediateActionController.enabled;
    return NO;
}

- (void)_setAllowsLinkPreview:(BOOL)allowsLinkPreview
{
    if (WebImmediateActionController *immediateActionController = _private->immediateActionController.get())
        immediateActionController.enabled = allowsLinkPreview;
}
#endif

- (void)addVisitedLinks:(NSArray *)visitedLinks
{
    WebVisitedLinkStore& visitedLinkStore = _private->group->visitedLinkStore();
    for (NSString *urlString in visitedLinks)
        visitedLinkStore.addVisitedLink(urlString);
}

#if PLATFORM(IOS_FAMILY)
- (void)removeVisitedLink:(NSURL *)url
{
    _private->group->visitedLinkStore().removeVisitedLink(URL(url).string());
}
#endif

@end

#if !PLATFORM(IOS_FAMILY)
@implementation WebView (WebViewPrintingPrivate)

- (float)_headerHeight
{
    return CallUIDelegateReturningFloat(self, @selector(webViewHeaderHeight:));
}

- (float)_footerHeight
{
    return CallUIDelegateReturningFloat(self, @selector(webViewFooterHeight:));
}

- (void)_drawHeaderInRect:(NSRect)rect
{
#ifdef DEBUG_HEADER_AND_FOOTER
    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];
    [[NSColor yellowColor] set];
    NSRectFill(rect);
    [currentContext restoreGraphicsState];
#endif

    SEL selector = @selector(webView:drawHeaderInRect:);
    if (![_private->UIDelegate respondsToSelector:selector])
        return;

    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];

    NSRectClip(rect);
    CallUIDelegate(self, selector, rect);

    [currentContext restoreGraphicsState];
}

- (void)_drawFooterInRect:(NSRect)rect
{
#ifdef DEBUG_HEADER_AND_FOOTER
    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];
    [[NSColor cyanColor] set];
    NSRectFill(rect);
    [currentContext restoreGraphicsState];
#endif

    SEL selector = @selector(webView:drawFooterInRect:);
    if (![_private->UIDelegate respondsToSelector:selector])
        return;

    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];

    NSRectClip(rect);
    CallUIDelegate(self, selector, rect);

    [currentContext restoreGraphicsState];
}

- (void)_adjustPrintingMarginsForHeaderAndFooter
{
    NSPrintOperation *op = [NSPrintOperation currentOperation];
    NSPrintInfo *info = [op printInfo];
    NSMutableDictionary *infoDictionary = [info dictionary];

    // We need to modify the top and bottom margins in the NSPrintInfo to account for the space needed by the
    // header and footer. Because this method can be called more than once on the same NSPrintInfo (see 5038087),
    // we stash away the unmodified top and bottom margins the first time this method is called, and we read from
    // those stashed-away values on subsequent calls.
    float originalTopMargin;
    float originalBottomMargin;
    NSNumber *originalTopMarginNumber = [infoDictionary objectForKey:WebKitOriginalTopPrintingMarginKey];
    if (!originalTopMarginNumber) {
        ASSERT(![infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey]);
        originalTopMargin = [info topMargin];
        originalBottomMargin = [info bottomMargin];
        [infoDictionary setObject:[NSNumber numberWithFloat:originalTopMargin] forKey:WebKitOriginalTopPrintingMarginKey];
        [infoDictionary setObject:[NSNumber numberWithFloat:originalBottomMargin] forKey:WebKitOriginalBottomPrintingMarginKey];
    } else {
        ASSERT([originalTopMarginNumber isKindOfClass:[NSNumber class]]);
        ASSERT([[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] isKindOfClass:[NSNumber class]]);
        originalTopMargin = [originalTopMarginNumber floatValue];
        originalBottomMargin = [[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] floatValue];
    }

    float scale = [op _web_pageSetupScaleFactor];
    [info setTopMargin:originalTopMargin + [self _headerHeight] * scale];
    [info setBottomMargin:originalBottomMargin + [self _footerHeight] * scale];
}

- (void)_drawHeaderAndFooter
{
    // The header and footer rect height scales with the page, but the width is always
    // all the way across the printed page (inset by printing margins).
    NSPrintOperation *op = [NSPrintOperation currentOperation];
    float scale = [op _web_pageSetupScaleFactor];
    NSPrintInfo *printInfo = [op printInfo];
    NSSize paperSize = [printInfo paperSize];
    float headerFooterLeft = [printInfo leftMargin]/scale;
    float headerFooterWidth = (paperSize.width - ([printInfo leftMargin] + [printInfo rightMargin]))/scale;
    NSRect footerRect = NSMakeRect(headerFooterLeft, [printInfo bottomMargin]/scale - [self _footerHeight] ,
                                   headerFooterWidth, [self _footerHeight]);
    NSRect headerRect = NSMakeRect(headerFooterLeft, (paperSize.height - [printInfo topMargin])/scale,
                                   headerFooterWidth, [self _headerHeight]);

    [self _drawHeaderInRect:headerRect];
    [self _drawFooterInRect:footerRect];
}
@end

@implementation WebView (WebDebugBinding)

- (void)addObserver:(NSObject *)anObserver forKeyPath:(NSString *)keyPath options:(NSKeyValueObservingOptions)options context:(void *)context
{
    LOG (Bindings, "addObserver:%p forKeyPath:%@ options:%x context:%p", anObserver, keyPath, options, context);
    [super addObserver:anObserver forKeyPath:keyPath options:options context:context];
}

- (void)removeObserver:(NSObject *)anObserver forKeyPath:(NSString *)keyPath
{
    LOG (Bindings, "removeObserver:%p forKeyPath:%@", anObserver, keyPath);
    [super removeObserver:anObserver forKeyPath:keyPath];
}

@end

#endif // !PLATFORM(IOS_FAMILY)

//==========================================================================================
// Editing

@implementation WebView (WebViewCSS)

- (DOMCSSStyleDeclaration *)computedStyleForElement:(DOMElement *)element pseudoElement:(NSString *)pseudoElement
{
    // FIXME: is this the best level for this conversion?
    if (pseudoElement == nil)
        pseudoElement = @"";

    return [[element ownerDocument] getComputedStyle:element pseudoElement:pseudoElement];
}

@end

@implementation WebView (WebViewEditing)

- (DOMRange *)editableDOMRangeForPoint:(NSPoint)point
{
    auto* page = core(self);
    if (!page)
        return nil;
    return kit(page->mainFrame().editor().rangeForPoint(WebCore::IntPoint([self convertPoint:point toView:nil])));
}

- (BOOL)_shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag
{
#if !PLATFORM(IOS_FAMILY)
    // FIXME: This quirk is needed due to <rdar://problem/4985321> - We can phase it out once Aperture can adopt the new behavior on their end
    if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_APERTURE_QUIRK) && [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.Aperture"])
        return YES;
#endif
    return [[self _editingDelegateForwarder] webView:self shouldChangeSelectedDOMRange:currentRange toDOMRange:proposedRange affinity:selectionAffinity stillSelecting:flag];
}

- (void)_setMaintainsInactiveSelection:(BOOL)shouldMaintainInactiveSelection
{
    _private->shouldMaintainInactiveSelection = shouldMaintainInactiveSelection;
}

- (BOOL)maintainsInactiveSelection
{
    return _private->shouldMaintainInactiveSelection;
}

- (void)setSelectedDOMRange:(DOMRange *)range affinity:(NSSelectionAffinity)selectionAffinity
{
    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return;

    if (range == nil)
        coreFrame->selection().clear();
    else {
        // Derive the frame to use from the range passed in.
        // Using _selectedOrMainFrame could give us a different document than
        // the one the range uses.
        coreFrame = core([range startContainer])->document().frame();
        if (!coreFrame)
            return;

        coreFrame->selection().setSelectedRange(makeSimpleRange(*core(range)), core(selectionAffinity), WebCore::FrameSelection::ShouldCloseTyping::Yes);
    }
}

- (DOMRange *)selectedDOMRange
{
    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->selection().selection().toNormalizedRange());
}

- (NSSelectionAffinity)selectionAffinity
{
    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return NSSelectionAffinityDownstream;
    return kit(coreFrame->selection().selection().affinity());
}

- (void)setEditable:(BOOL)flag
{
    if ([self isEditable] != flag && _private->page) {
        _private->page->setEditable(flag);
        if (!_private->tabKeyCyclesThroughElementsChanged)
            _private->page->setTabKeyCyclesThroughElements(!flag);
#if PLATFORM(MAC)
        if (flag) {
            RunLoop::main().dispatch([] {
                [[NSSpellChecker sharedSpellChecker] _preflightChosenSpellServer];
            });
        }
#endif
        auto* mainFrame = [self _mainCoreFrame];
        if (mainFrame) {
            if (flag) {
                mainFrame->editor().applyEditingStyleToBodyElement();
                // If the WebView is made editable and the selection is empty, set it to something.
                if (![self selectedDOMRange])
                    mainFrame->selection().setSelectionFromNone();
            }
        }
    }
}

- (BOOL)isEditable
{
    return _private->page && _private->page->isEditable();
}

- (void)setTypingStyle:(DOMCSSStyleDeclaration *)style
{
    // We don't know enough at thls level to pass in a relevant WebUndoAction; we'd have to
    // change the API to allow this.
    [[self _selectedOrMainFrame] _setTypingStyle:style withUndoAction:WebCore::EditAction::Unspecified];
}

- (DOMCSSStyleDeclaration *)typingStyle
{
    return [[self _selectedOrMainFrame] _typingStyle];
}

- (void)setSmartInsertDeleteEnabled:(BOOL)flag
{
    if (_private->page->settings().smartInsertDeleteEnabled() != flag) {
        _private->page->settings().setSmartInsertDeleteEnabled(flag);
        [[NSUserDefaults standardUserDefaults] setBool:_private->page->settings().smartInsertDeleteEnabled() forKey:WebSmartInsertDeleteEnabled];
        [self setSelectTrailingWhitespaceEnabled:!flag];
    }
}

- (BOOL)smartInsertDeleteEnabled
{
    return _private->page->settings().smartInsertDeleteEnabled();
}

- (void)setContinuousSpellCheckingEnabled:(BOOL)flag
{
    if (continuousSpellCheckingEnabled == flag)
        return;

    continuousSpellCheckingEnabled = flag;
#if !PLATFORM(IOS_FAMILY)
    [[NSUserDefaults standardUserDefaults] setBool:continuousSpellCheckingEnabled forKey:WebContinuousSpellCheckingEnabled];
#endif
    if ([self isContinuousSpellCheckingEnabled])
        [[self class] _preflightSpellChecker];
    else
        [[self mainFrame] _unmarkAllMisspellings];
}

- (BOOL)isContinuousSpellCheckingEnabled
{
    return (continuousSpellCheckingEnabled && [self _continuousCheckingAllowed]);
}

#if !PLATFORM(IOS_FAMILY)
- (NSInteger)spellCheckerDocumentTag
{
    if (!_private->hasSpellCheckerDocumentTag) {
        _private->spellCheckerDocumentTag = [NSSpellChecker uniqueSpellDocumentTag];
        _private->hasSpellCheckerDocumentTag = YES;
    }
    return _private->spellCheckerDocumentTag;
}
#endif

- (NSUndoManager *)undoManager
{
    if (!_private->allowsUndo)
        return nil;

#if !PLATFORM(IOS_FAMILY)
    NSUndoManager *undoManager = [[self _editingDelegateForwarder] undoManagerForWebView:self];
    if (undoManager)
        return undoManager;

    return [super undoManager];
#else
    return [[self _editingDelegateForwarder] undoManagerForWebView:self];
#endif
}

- (void)registerForEditingDelegateNotification:(NSString *)name selector:(SEL)selector
{
    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    if ([_private->editingDelegate respondsToSelector:selector])
        [defaultCenter addObserver:_private->editingDelegate selector:selector name:name object:self];
}

- (void)setEditingDelegate:(id)delegate
{
    if (_private->editingDelegate == delegate)
        return;

    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];

    // remove notifications from current delegate
    [defaultCenter removeObserver:_private->editingDelegate name:WebViewDidBeginEditingNotification object:self];
    [defaultCenter removeObserver:_private->editingDelegate name:WebViewDidChangeNotification object:self];
    [defaultCenter removeObserver:_private->editingDelegate name:WebViewDidEndEditingNotification object:self];
    [defaultCenter removeObserver:_private->editingDelegate name:WebViewDidChangeTypingStyleNotification object:self];
    [defaultCenter removeObserver:_private->editingDelegate name:WebViewDidChangeSelectionNotification object:self];

    _private->editingDelegate = delegate;
    _private->editingDelegateForwarder = nil;

    // add notifications for new delegate
    [self registerForEditingDelegateNotification:WebViewDidBeginEditingNotification selector:@selector(webViewDidBeginEditing:)];
    [self registerForEditingDelegateNotification:WebViewDidChangeNotification selector:@selector(webViewDidChange:)];
    [self registerForEditingDelegateNotification:WebViewDidEndEditingNotification selector:@selector(webViewDidEndEditing:)];
    [self registerForEditingDelegateNotification:WebViewDidChangeTypingStyleNotification selector:@selector(webViewDidChangeTypingStyle:)];
    [self registerForEditingDelegateNotification:WebViewDidChangeSelectionNotification selector:@selector(webViewDidChangeSelection:)];
}

- (id)editingDelegate
{
    return _private->editingDelegate;
}

- (DOMCSSStyleDeclaration *)styleDeclarationWithText:(NSString *)text
{
    // FIXME: Should this really be attached to the document with the current selection?
    DOMCSSStyleDeclaration *decl = [[[self _selectedOrMainFrame] DOMDocument] createCSSStyleDeclaration];
    [decl setCssText:text];
    return decl;
}

@end

#if !PLATFORM(IOS_FAMILY)
@implementation WebView (WebViewGrammarChecking)

// FIXME: This method should be merged into WebViewEditing when we're not in API freeze
- (BOOL)isGrammarCheckingEnabled
{
    return grammarCheckingEnabled;
}

// FIXME: This method should be merged into WebViewEditing when we're not in API freeze
- (void)setGrammarCheckingEnabled:(BOOL)flag
{
    if (grammarCheckingEnabled == flag)
        return;

    grammarCheckingEnabled = flag;
    [[NSUserDefaults standardUserDefaults] setBool:grammarCheckingEnabled forKey:WebGrammarCheckingEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];

    // We call _preflightSpellChecker when turning continuous spell checking on, but we don't need to do that here
    // because grammar checking only occurs on code paths that already preflight spell checking appropriately.

    if (![self isGrammarCheckingEnabled])
        [[self mainFrame] _unmarkAllBadGrammar];
}

// FIXME: This method should be merged into WebIBActions when we're not in API freeze
- (void)toggleGrammarChecking:(id)sender
{
    [self setGrammarCheckingEnabled:![self isGrammarCheckingEnabled]];
}

@end
#endif

@implementation WebView (WebViewTextChecking)

- (BOOL)isAutomaticQuoteSubstitutionEnabled
{
#if PLATFORM(IOS_FAMILY)
    return NO;
#else
    return automaticQuoteSubstitutionEnabled;
#endif
}

- (BOOL)isAutomaticLinkDetectionEnabled
{
#if PLATFORM(IOS_FAMILY)
    return NO;
#else
    return automaticLinkDetectionEnabled;
#endif
}

- (BOOL)isAutomaticDashSubstitutionEnabled
{
#if PLATFORM(IOS_FAMILY)
    return NO;
#else
    return automaticDashSubstitutionEnabled;
#endif
}

- (BOOL)isAutomaticTextReplacementEnabled
{
#if PLATFORM(IOS_FAMILY)
    return NO;
#else
    return automaticTextReplacementEnabled;
#endif
}

- (BOOL)isAutomaticSpellingCorrectionEnabled
{
#if PLATFORM(IOS_FAMILY)
    return NO;
#else
    return automaticSpellingCorrectionEnabled;
#endif
}

#if !PLATFORM(IOS_FAMILY)

- (void)setAutomaticQuoteSubstitutionEnabled:(BOOL)flag
{
    if (automaticQuoteSubstitutionEnabled == flag)
        return;
    automaticQuoteSubstitutionEnabled = flag;
    [[NSUserDefaults standardUserDefaults] setBool:automaticQuoteSubstitutionEnabled forKey:WebAutomaticQuoteSubstitutionEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

- (void)toggleAutomaticQuoteSubstitution:(id)sender
{
    [self setAutomaticQuoteSubstitutionEnabled:![self isAutomaticQuoteSubstitutionEnabled]];
}

- (void)setAutomaticLinkDetectionEnabled:(BOOL)flag
{
    if (automaticLinkDetectionEnabled == flag)
        return;
    automaticLinkDetectionEnabled = flag;
    [[NSUserDefaults standardUserDefaults] setBool:automaticLinkDetectionEnabled forKey:WebAutomaticLinkDetectionEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

- (void)toggleAutomaticLinkDetection:(id)sender
{
    [self setAutomaticLinkDetectionEnabled:![self isAutomaticLinkDetectionEnabled]];
}

- (void)setAutomaticDashSubstitutionEnabled:(BOOL)flag
{
    if (automaticDashSubstitutionEnabled == flag)
        return;
    automaticDashSubstitutionEnabled = flag;
    [[NSUserDefaults standardUserDefaults] setBool:automaticDashSubstitutionEnabled forKey:WebAutomaticDashSubstitutionEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

- (void)toggleAutomaticDashSubstitution:(id)sender
{
    [self setAutomaticDashSubstitutionEnabled:![self isAutomaticDashSubstitutionEnabled]];
}

- (void)setAutomaticTextReplacementEnabled:(BOOL)flag
{
    if (automaticTextReplacementEnabled == flag)
        return;
    automaticTextReplacementEnabled = flag;
    [[NSUserDefaults standardUserDefaults] setBool:automaticTextReplacementEnabled forKey:WebAutomaticTextReplacementEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

- (void)toggleAutomaticTextReplacement:(id)sender
{
    [self setAutomaticTextReplacementEnabled:![self isAutomaticTextReplacementEnabled]];
}

- (void)setAutomaticSpellingCorrectionEnabled:(BOOL)flag
{
    if (automaticSpellingCorrectionEnabled == flag)
        return;
    automaticSpellingCorrectionEnabled = flag;
    [[NSUserDefaults standardUserDefaults] setBool:automaticSpellingCorrectionEnabled forKey:WebAutomaticSpellingCorrectionEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

- (void)toggleAutomaticSpellingCorrection:(id)sender
{
    [self setAutomaticSpellingCorrectionEnabled:![self isAutomaticSpellingCorrectionEnabled]];
}

#endif // !PLATFORM(IOS_FAMILY)

@end

@implementation WebView (WebViewUndoableEditing)

- (void)replaceSelectionWithNode:(DOMNode *)node
{
    [[self _selectedOrMainFrame] _replaceSelectionWithNode:node selectReplacement:YES smartReplace:NO matchStyle:NO];
}

- (void)replaceSelectionWithText:(NSString *)text
{
    [[self _selectedOrMainFrame] _replaceSelectionWithText:text selectReplacement:YES smartReplace:NO];
}

- (void)replaceSelectionWithMarkupString:(NSString *)markupString
{
    [[self _selectedOrMainFrame] _replaceSelectionWithMarkupString:markupString baseURLString:nil selectReplacement:YES smartReplace:NO];
}

- (void)replaceSelectionWithArchive:(WebArchive *)archive
{
    [[[self _selectedOrMainFrame] _dataSource] _replaceSelectionWithArchive:archive selectReplacement:YES];
}

- (void)deleteSelection
{
    WebFrame *webFrame = [self _selectedOrMainFrame];
    auto* coreFrame = core(webFrame);
    if (coreFrame)
        coreFrame->editor().deleteSelectionWithSmartDelete([(WebHTMLView *)[[webFrame frameView] documentView] _canSmartCopyOrDelete]);
}

- (void)applyStyle:(DOMCSSStyleDeclaration *)style
{
    // We don't know enough at thls level to pass in a relevant WebUndoAction; we'd have to
    // change the API to allow this.
    WebFrame *webFrame = [self _selectedOrMainFrame];
    if (auto* coreFrame = core(webFrame)) {
        // FIXME: We shouldn't have to make a copy here.
        Ref<WebCore::MutableStyleProperties> properties(core(style)->copyProperties());
        coreFrame->editor().applyStyle(properties.ptr());
    }
}

@end

@implementation WebView (WebViewEditingActions)

- (void)_performResponderOperation:(SEL)selector with:(id)parameter
{
    static BOOL reentered = NO;
    if (reentered) {
        [[self nextResponder] tryToPerform:selector with:parameter];
        return;
    }

    // There are two possibilities here.
    //
    // One is that WebView has been called in its role as part of the responder chain.
    // In that case, it's fine to call the first responder and end up calling down the
    // responder chain again. Later we will return here with reentered = YES and continue
    // past the WebView.
    //
    // The other is that we are being called directly, in which case we want to pass the
    // selector down to the view inside us that can handle it, and continue down the
    // responder chain as usual.

    // Pass this selector down to the first responder.
    NSResponder *responder = [self _responderForResponderOperations];
    reentered = YES;
    [responder tryToPerform:selector with:parameter];
    reentered = NO;
}

#define FORWARD(name) \
    - (void)name:(id)sender { [self _performResponderOperation:_cmd with:sender]; }

FOR_EACH_RESPONDER_SELECTOR(FORWARD)

#if PLATFORM(IOS_FAMILY)
FORWARD(clearText)
FORWARD(toggleBold)
FORWARD(toggleItalic)
FORWARD(toggleUnderline)

- (void)insertDictationPhrases:(NSArray *)dictationPhrases metadata:(id)metadata
{
    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return;

    coreFrame->editor().insertDictationPhrases(vectorForDictationPhrasesArray(dictationPhrases), metadata);
}
#endif

- (void)insertText:(NSString *)text
{
    [self _performResponderOperation:_cmd with:text];
}

- (NSDictionary *)typingAttributes
{
    if (auto* coreFrame = core([self _selectedOrMainFrame]))
        return coreFrame->editor().fontAttributesAtSelectionStart().createDictionary().autorelease();

    return nil;
}

@end

@implementation WebView (WebViewEditingInMail)

- (void)_insertNewlineInQuotedContent
{
    [[self _selectedOrMainFrame] _insertParagraphSeparatorInQuotedContent];
}

- (void)_replaceSelectionWithNode:(DOMNode *)node matchStyle:(BOOL)matchStyle
{
    [[self _selectedOrMainFrame] _replaceSelectionWithNode:node selectReplacement:YES smartReplace:NO matchStyle:matchStyle];
}

- (BOOL)_selectionIsCaret
{
    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return NO;
    return coreFrame->selection().isCaret();
}

- (BOOL)_selectionIsAll
{
    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return NO;
    return coreFrame->selection().isAll(WebCore::CanCrossEditingBoundary);
}

- (void)_simplifyMarkup:(DOMNode *)startNode endNode:(DOMNode *)endNode
{
    auto* coreFrame = core([self mainFrame]);
    if (!coreFrame || !startNode)
        return;
    auto* coreStartNode= core(startNode);
    if (&coreStartNode->document() != coreFrame->document())
        return;
    return coreFrame->editor().simplifyMarkup(coreStartNode, core(endNode));
}

+ (void)_setCacheModel:(WebCacheModel)cacheModel
{
    if (s_didSetCacheModel && cacheModel == s_cacheModel)
        return;

    auto nsurlCacheDirectory = adoptCF(_CFURLCacheCopyCacheDirectory([[NSURLCache sharedURLCache] _CFURLCache]));
    if (!nsurlCacheDirectory)
        nsurlCacheDirectory = (__bridge CFStringRef)NSHomeDirectory();

    static uint64_t memSize = ramSize() / 1024 / 1024;

    NSDictionary *fileSystemAttributesDictionary = [[NSFileManager defaultManager] attributesOfFileSystemForPath:(__bridge NSString *)nsurlCacheDirectory.get() error:nullptr];
    unsigned long long diskFreeSize = [[fileSystemAttributesDictionary objectForKey:NSFileSystemFreeSize] unsignedLongLongValue] / 1024 / 1000;

    NSURLCache *nsurlCache = [NSURLCache sharedURLCache];

    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    Seconds deadDecodedDataDeletionInterval;

    unsigned pageCacheSize = 0;

    NSUInteger nsurlCacheMemoryCapacity = 0;
    NSUInteger nsurlCacheDiskCapacity = 0;
#if PLATFORM(IOS_FAMILY)
    unsigned tileLayerPoolCapacity = 0;
#endif

    switch (cacheModel) {
    case WebCacheModelDocumentViewer: {
        // Back/forward cache capacity (in pages)
        pageCacheSize = 0;

        // Object cache capacities (in bytes)
        if (memSize >= 4096)
            cacheTotalCapacity = 128 * 1024 * 1024;
        else if (memSize >= 2048)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 32 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 16 * 1024 * 1024;
#if PLATFORM(IOS_FAMILY)
        else
            cacheTotalCapacity = 4 * 1024 * 1024;
#endif

        cacheMinDeadCapacity = 0;
        cacheMaxDeadCapacity = 0;

        // Foundation memory cache capacity (in bytes)
        nsurlCacheMemoryCapacity = 0;

        // Foundation disk cache capacity (in bytes)
        nsurlCacheDiskCapacity = [nsurlCache diskCapacity];

#if PLATFORM(IOS_FAMILY)
        // TileCache layer pool capacity, in bytes.
        if (memSize >= 1024)
            tileLayerPoolCapacity = 24 * 1024 * 1024;
        else
            tileLayerPoolCapacity = 12 * 1024 * 1024;
#endif
        break;
    }
    case WebCacheModelDocumentBrowser: {
        // Back/forward cache capacity (in pages)
        if (memSize >= 512)
            pageCacheSize = 2;
        else if (memSize >= 256)
            pageCacheSize = 1;
        else
            pageCacheSize = 0;

        // Object cache capacities (in bytes)
        if (memSize >= 4096)
            cacheTotalCapacity = 128 * 1024 * 1024;
        else if (memSize >= 2048)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 32 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 16 * 1024 * 1024;

        cacheMinDeadCapacity = cacheTotalCapacity / 8;
        cacheMaxDeadCapacity = cacheTotalCapacity / 4;

        // Foundation memory cache capacity (in bytes)
        if (memSize >= 2048)
            nsurlCacheMemoryCapacity = 4 * 1024 * 1024;
        else if (memSize >= 1024)
            nsurlCacheMemoryCapacity = 2 * 1024 * 1024;
        else if (memSize >= 512)
            nsurlCacheMemoryCapacity = 1 * 1024 * 1024;
        else
            nsurlCacheMemoryCapacity =      512 * 1024;

        // Foundation disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            nsurlCacheDiskCapacity = 50 * 1024 * 1024;
        else if (diskFreeSize >= 8192)
            nsurlCacheDiskCapacity = 40 * 1024 * 1024;
        else if (diskFreeSize >= 4096)
            nsurlCacheDiskCapacity = 30 * 1024 * 1024;
        else
            nsurlCacheDiskCapacity = 20 * 1024 * 1024;

#if PLATFORM(IOS_FAMILY)
        // TileCache layer pool capacity, in bytes.
        if (memSize >= 1024)
            tileLayerPoolCapacity = 24 * 1024 * 1024;
        else
            tileLayerPoolCapacity = 12 * 1024 * 1024;
#endif
        break;
    }
    case WebCacheModelPrimaryWebBrowser: {
        // Back/forward cache capacity (in pages)
        if (memSize >= 512)
            pageCacheSize = 2;
        else if (memSize >= 256)
            pageCacheSize = 1;
        else
            pageCacheSize = 0;

        // Object cache capacities (in bytes)
        // (Testing indicates that value / MB depends heavily on content and
        // browsing pattern. Even growth above 128MB can have substantial
        // value / MB for some content / browsing patterns.)
        if (memSize >= 4096)
            cacheTotalCapacity = 192 * 1024 * 1024;
        else if (memSize >= 2048)
            cacheTotalCapacity = 128 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 32 * 1024 * 1024;

        cacheMinDeadCapacity = cacheTotalCapacity / 4;
        cacheMaxDeadCapacity = cacheTotalCapacity / 2;

        // This code is here to avoid a PLT regression. We can remove it if we
        // can prove that the overall system gain would justify the regression.
        cacheMaxDeadCapacity = std::max<unsigned>(24, cacheMaxDeadCapacity);

        deadDecodedDataDeletionInterval = 60_s;

#if PLATFORM(IOS_FAMILY)
        if (memSize >= 1024)
            nsurlCacheMemoryCapacity = 16 * 1024 * 1024;
        else
            nsurlCacheMemoryCapacity = 8 * 1024 * 1024;
#else
        // Foundation memory cache capacity (in bytes)
        // (These values are small because WebCore does most caching itself.)
        if (memSize >= 1024)
            nsurlCacheMemoryCapacity = 4 * 1024 * 1024;
        else if (memSize >= 512)
            nsurlCacheMemoryCapacity = 2 * 1024 * 1024;
        else if (memSize >= 256)
            nsurlCacheMemoryCapacity = 1 * 1024 * 1024;
        else
            nsurlCacheMemoryCapacity =      512 * 1024;
#endif

        // Foundation disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            nsurlCacheDiskCapacity = 175 * 1024 * 1024;
        else if (diskFreeSize >= 8192)
            nsurlCacheDiskCapacity = 150 * 1024 * 1024;
        else if (diskFreeSize >= 4096)
            nsurlCacheDiskCapacity = 125 * 1024 * 1024;
        else if (diskFreeSize >= 2048)
            nsurlCacheDiskCapacity = 100 * 1024 * 1024;
        else if (diskFreeSize >= 1024)
            nsurlCacheDiskCapacity = 75 * 1024 * 1024;
        else
            nsurlCacheDiskCapacity = 50 * 1024 * 1024;

#if PLATFORM(IOS_FAMILY)
        // TileCache layer pool capacity, in bytes.
        if (memSize >= 1024)
            tileLayerPoolCapacity = 48 * 1024 * 1024;
        else
            tileLayerPoolCapacity = 24 * 1024 * 1024;
#endif
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    };


    // Don't shrink a big disk cache, since that would cause churn.
    nsurlCacheDiskCapacity = std::max(nsurlCacheDiskCapacity, [nsurlCache diskCapacity]);

    auto& memoryCache = WebCore::MemoryCache::singleton();
    memoryCache.setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    memoryCache.setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);

    auto& pageCache = WebCore::BackForwardCache::singleton();
    pageCache.setMaxSize(pageCacheSize);
#if PLATFORM(IOS_FAMILY)
    nsurlCacheMemoryCapacity = std::max(nsurlCacheMemoryCapacity, [nsurlCache memoryCapacity]);
    CFURLCacheRef cfCache;
    if ((cfCache = [nsurlCache _CFURLCache]))
        CFURLCacheSetMemoryCapacity(cfCache, nsurlCacheMemoryCapacity);
    else
        [nsurlCache setMemoryCapacity:nsurlCacheMemoryCapacity];
#else
    [nsurlCache setMemoryCapacity:nsurlCacheMemoryCapacity];
#endif
    [nsurlCache setDiskCapacity:nsurlCacheDiskCapacity];

#if PLATFORM(IOS_FAMILY)
    [WebView _setTileCacheLayerPoolCapacity:tileLayerPoolCapacity];
#endif

    s_cacheModel = cacheModel;
    s_didSetCacheModel = YES;
}

+ (WebCacheModel)_cacheModel
{
    return s_cacheModel;
}

#if !PLATFORM(IOS_FAMILY)
- (void)_openFrameInNewWindowFromMenu:(NSMenuItem *)sender
{
    ASSERT_ARG(sender, [sender isKindOfClass:[NSMenuItem class]]);

    NSDictionary *element = [sender representedObject];
    ASSERT([element isKindOfClass:[NSDictionary class]]);

    WebDataSource *dataSource = [(WebFrame *)[element objectForKey:WebElementFrameKey] dataSource];
    auto request = adoptNS([[dataSource request] copy]);
    ASSERT(request);

    [self _openNewWindowWithRequest:request.get()];
}

- (void)_searchWithGoogleFromMenu:(id)sender
{
    id documentView = [[[self selectedFrame] frameView] documentView];
    if (![documentView conformsToProtocol:@protocol(WebDocumentText)]) {
        return;
    }

    NSString *selectedString = [(id <WebDocumentText>)documentView selectedString];
    if ([selectedString length] == 0) {
        return;
    }

    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[WebCore::legacyStringPasteboardType()] owner:nil];
    auto s = adoptNS([selectedString mutableCopy]);
    const unichar nonBreakingSpaceCharacter = 0xA0;
    NSString *nonBreakingSpaceString = [NSString stringWithCharacters:&nonBreakingSpaceCharacter length:1];
    [s replaceOccurrencesOfString:nonBreakingSpaceString withString:@" " options:0 range:NSMakeRange(0, [s length])];
    [pasteboard setString:s.get() forType:WebCore::legacyStringPasteboardType()];

    // FIXME: seems fragile to use the service by name, but this is what AppKit does
    NSPerformService(@"Search With Google", pasteboard);
}

- (void)_searchWithSpotlightFromMenu:(id)sender
{
    id documentView = [[[self selectedFrame] frameView] documentView];
    if (![documentView conformsToProtocol:@protocol(WebDocumentText)])
        return;

    NSString *selectedString = [(id <WebDocumentText>)documentView selectedString];
    if (![selectedString length])
        return;

    [[NSWorkspace sharedWorkspace] showSearchResultsForQueryString:selectedString];
}
#endif // !PLATFORM(IOS_FAMILY)

@end

@implementation WebView (WebViewInternal)

+ (BOOL)shouldIncludeInWebKitStatistics
{
    return NO;
}

- (BOOL)_becomingFirstResponderFromOutside
{
    return _private->becomingFirstResponderFromOutside;
}

- (void)_addObject:(id)object forIdentifier:(unsigned long)identifier
{
    ASSERT(!_private->identifierMap.contains(identifier));

    // If the identifier map is initially empty it means we're starting a load
    // of something. The semantic is that the web view should be around as long
    // as something is loading. Because of that we retain the web view.
    if (_private->identifierMap.isEmpty())
        CFRetain(self);

    _private->identifierMap.set(identifier, object);
}

- (id)_objectForIdentifier:(unsigned long)identifier
{
    return _private->identifierMap.get(identifier).get();
}

- (void)_removeObjectForIdentifier:(unsigned long)identifier
{
    ASSERT(_private->identifierMap.contains(identifier));
    _private->identifierMap.remove(identifier);

    // If the identifier map is now empty it means we're no longer loading anything
    // and we should release the web view. Autorelease rather than release in order to
    // avoid re-entering this method beneath -dealloc with the same identifier. <rdar://problem/10523721>
    if (_private->identifierMap.isEmpty())
        [self autorelease];
}

- (void)_retrieveKeyboardUIModeFromPreferences:(NSNotification *)notification
{
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);

    Boolean keyExistsAndHasValidFormat;
    int mode = CFPreferencesGetAppIntegerValue(AppleKeyboardUIMode, kCFPreferencesCurrentApplication, &keyExistsAndHasValidFormat);

    // The keyboard access mode has two bits:
    // Bit 0 is set if user can set the focus to menus, the dock, and various windows using the keyboard.
    // Bit 1 is set if controls other than text fields are included in the tab order (WebKit also always includes lists).
    _private->_keyboardUIMode = (mode & 0x2) ? WebCore::KeyboardAccessFull : WebCore::KeyboardAccessDefault;
#if !PLATFORM(IOS_FAMILY)
    // check for tabbing to links
    if ([_private->preferences tabsToLinks])
        _private->_keyboardUIMode = (WebCore::KeyboardUIMode)(_private->_keyboardUIMode | WebCore::KeyboardAccessTabsToLinks);
#endif
}

- (WebCore::KeyboardUIMode)_keyboardUIMode
{
    if (!_private->_keyboardUIModeAccessed) {
        _private->_keyboardUIModeAccessed = YES;

        [self _retrieveKeyboardUIModeFromPreferences:nil];

#if !PLATFORM(IOS_FAMILY)
        [[NSDistributedNotificationCenter defaultCenter]
            addObserver:self selector:@selector(_retrieveKeyboardUIModeFromPreferences:)
            name:KeyboardUIModeDidChangeNotification object:nil];
#endif

        [[NSNotificationCenter defaultCenter]
            addObserver:self selector:@selector(_retrieveKeyboardUIModeFromPreferences:)
            name:WebPreferencesChangedInternalNotification object:nil];
    }
    return _private->_keyboardUIMode;
}

#if !PLATFORM(IOS_FAMILY)
- (void)_setInsertionPasteboard:(NSPasteboard *)pasteboard
{
    _private->insertionPasteboard = pasteboard;
}
#endif

- (WebCore::Frame*)_mainCoreFrame
{
    return (_private && _private->page) ? &_private->page->mainFrame() : 0;
}

- (WebFrame *)_selectedOrMainFrame
{
    WebFrame *result = [self selectedFrame];
    if (result == nil)
        result = [self mainFrame];
    return result;
}

- (void)_clearCredentials
{
    auto* frame = [self _mainCoreFrame];
    if (!frame)
        return;

    auto* networkingContext = frame->loader().networkingContext();
    if (!networkingContext)
        return;

    networkingContext->storageSession()->credentialStorage().clearCredentials();
}

- (BOOL)_needsOneShotDrawingSynchronization
{
    return _private->needsOneShotDrawingSynchronization;
}

- (void)_setNeedsOneShotDrawingSynchronization:(BOOL)needsSynchronization
{
    _private->needsOneShotDrawingSynchronization = needsSynchronization;
}

/*
    The order of events with compositing updates is this:

   Start of runloop                                        End of runloop
        |                                                       |
      --|-------------------------------------------------------|--
           ^         ^                                        ^
           |         |                                        |
    NSWindow update, |                                     CA commit
     NSView drawing  |
        flush        |
                layerSyncRunLoopObserverCallBack

    To avoid flashing, we have to ensure that compositing changes (rendered via
    the CoreAnimation rendering display link) appear on screen at the same time
    as content painted into the window via the normal WebCore rendering path.

    CoreAnimation will commit any layer changes at the end of the runloop via
    its "CA commit" observer. Those changes can then appear onscreen at any time
    when the display link fires, which can result in unsynchronized rendering.

    To fix this, the GraphicsLayerCA code in WebCore does not change the CA
    layer tree during style changes and layout; it stores up all changes and
    commits them via flushCompositingState(). There are then two situations in
    which we can call flushCompositingState():

    1. When painting. FrameView::paintContents() makes a call to flushCompositingState().

    2. When style changes/layout have made changes to the layer tree which do not
       result in painting. In this case we need a run loop observer to do a
       flushCompositingState() at an appropriate time. The observer will keep firing
       until the time is right (essentially when there are no more pending layouts).

*/
bool LayerFlushController::flushLayers()
{
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
#endif

#if PLATFORM(MAC)
    NSWindow *window = [m_webView window];
#endif // PLATFORM(MAC)

    [m_webView _updateRendering];

#if PLATFORM(MAC)
    // AppKit may have disabled screen updates, thinking an upcoming window flush will re-enable them.
    // In case setNeedsDisplayInRect() has prevented the window from needing to be flushed, re-enable screen
    // updates here.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (![window isFlushWindowDisabled])
        [window _enableScreenUpdatesIfNeeded];
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif

    return true;
}

- (void)_scheduleUpdateRendering
{
#if PLATFORM(IOS_FAMILY)
    if (_private->closing)
        return;
#endif

    if (!_private->layerFlushController)
        _private->layerFlushController = LayerFlushController::create(self);
    _private->layerFlushController->scheduleLayerFlush();
}

- (BOOL)_flushCompositingChanges
{
    auto* frame = [self _mainCoreFrame];
    if (frame && frame->view())
        return frame->view()->flushCompositingStateIncludingSubframes();

    return YES;
}

#if PLATFORM(IOS_FAMILY)
- (void)_scheduleRenderingUpdateForPendingTileCacheRepaint
{
    if (auto* page = _private->page)
        page->scheduleRenderingUpdate(WebCore::RenderingUpdateStep::LayerFlush);
}
#endif

#if ENABLE(VIDEO)

#if ENABLE(VIDEO_PRESENTATION_MODE)

- (void)_enterVideoFullscreenForVideoElement:(NakedPtr<WebCore::HTMLVideoElement>)videoElement mode:(WebCore::HTMLMediaElementEnums::VideoFullscreenMode)mode
{
    if (_private->fullscreenController) {
        if ([_private->fullscreenController videoElement] == videoElement) {
            // The backend may just warn us that the underlaying plaftormMovie()
            // has changed. Just force an update.
            [_private->fullscreenController setVideoElement:videoElement];
            return; // No more to do.
        }

        // First exit Fullscreen for the old videoElement.
        [_private->fullscreenController videoElement]->exitFullscreen();
        _private->fullscreenControllersExiting.append(std::exchange(_private->fullscreenController, nil));
    }

    if (!_private->fullscreenController) {
        _private->fullscreenController = adoptNS([[WebVideoFullscreenController alloc] init]);
        [_private->fullscreenController setVideoElement:videoElement];
#if PLATFORM(IOS_FAMILY)
        [_private->fullscreenController enterFullscreen:(UIView *)[[[self window] hostLayer] delegate] mode:mode];
#else
        [_private->fullscreenController enterFullscreen:[[self window] screen]];
#endif
    }
    else
        [_private->fullscreenController setVideoElement:videoElement];
}

- (void)_exitVideoFullscreen
{
    if (!_private->fullscreenController && _private->fullscreenControllersExiting.isEmpty())
        return;

    if (!_private->fullscreenControllersExiting.isEmpty()) {
        auto controller = _private->fullscreenControllersExiting.first();
        _private->fullscreenControllersExiting.remove(0);

        [controller exitFullscreen];
        return;
    }

    auto fullscreenController = _private->fullscreenController;
    _private->fullscreenController = nil;

    [fullscreenController exitFullscreen];
}

#if PLATFORM(MAC)
- (BOOL)_hasActiveVideoForControlsInterface
{
    if (!_private->playbackSessionModel)
        return false;

    auto* mediaElement = _private->playbackSessionModel->mediaElement();
    if (!mediaElement)
        return false;

    return mediaElement->hasAudio() || mediaElement->hasVideo();
}

- (void)_setUpPlaybackControlsManagerForMediaElement:(NakedRef<WebCore::HTMLMediaElement>)mediaElement
{
    if (_private->playbackSessionModel && _private->playbackSessionModel->mediaElement() == mediaElement.ptr())
        return;

    if (!_private->playbackSessionModel)
        _private->playbackSessionModel = WebCore::PlaybackSessionModelMediaElement::create();
    _private->playbackSessionModel->setMediaElement(mediaElement.ptr());

    if (!_private->playbackSessionInterface)
        _private->playbackSessionInterface = WebCore::PlaybackSessionInterfaceMac::create(*_private->playbackSessionModel);

    [self updateTouchBar];
}

- (void)_clearPlaybackControlsManager
{
    if (!_private->playbackSessionModel || !_private->playbackSessionModel->mediaElement())
        return;

    _private->playbackSessionModel->setMediaElement(nullptr);
    _private->playbackSessionInterface->invalidate();

    _private->playbackSessionModel = nullptr;
    _private->playbackSessionInterface = nullptr;
    [self updateTouchBar];
}

- (void)_playbackControlsMediaEngineChanged
{
    if (!_private->playbackSessionModel)
        return;

    _private->playbackSessionModel->mediaEngineChanged();
}

#endif // PLATFORM(MAC)

#endif // ENABLE(VIDEO_PRESENTATION_MODE)

#endif // ENABLE(VIDEO)

#if ENABLE(FULLSCREEN_API) && !PLATFORM(IOS_FAMILY)
- (BOOL)_supportsFullScreenForElement:(NakedPtr<const WebCore::Element>)element withKeyboard:(BOOL)withKeyboard
{
    if (![[self preferences] fullScreenEnabled])
        return NO;

    return true;
}

- (void)_enterFullScreenForElement:(NakedPtr<WebCore::Element>)element
{
    if (!_private->newFullscreenController)
        _private->newFullscreenController = adoptNS([[WebFullScreenController alloc] init]);

    [_private->newFullscreenController setElement:element.get()];
    [_private->newFullscreenController setWebView:self];
    [_private->newFullscreenController enterFullScreen:[[self window] screen]];
}

- (void)_exitFullScreenForElement:(NakedPtr<WebCore::Element>)element
{
    if (!_private->newFullscreenController)
        return;
    [_private->newFullscreenController exitFullScreen];
}
#endif

#if USE(AUTOCORRECTION_PANEL)
- (void)handleAcceptedAlternativeText:(NSString*)text
{
    WebFrame *webFrame = [self _selectedOrMainFrame];
    auto* coreFrame = core(webFrame);
    if (coreFrame)
        coreFrame->editor().handleAlternativeTextUIResult(text);
}
#endif

- (void)_getWebCoreDictationAlternatives:(Vector<WebCore::DictationAlternative>&)alternatives fromTextAlternatives:(const Vector<WebCore::TextAlternativeWithRange>&)alternativesWithRange
{
    for (auto& alternativeWithRange : alternativesWithRange) {
        if (auto dictationContext = _private->m_alternativeTextUIController->addAlternatives(alternativeWithRange.alternatives.get()))
            alternatives.append({ alternativeWithRange.range, dictationContext });
    }
}

- (void)_showDictationAlternativeUI:(const WebCore::FloatRect&)boundingBoxOfDictatedText forDictationContext:(WebCore::DictationContext)dictationContext
{
#if USE(AUTOCORRECTION_PANEL)
    _private->m_alternativeTextUIController->showAlternatives(self, [self _convertRectFromRootView:boundingBoxOfDictatedText], dictationContext, ^(NSString* acceptedAlternative) {
        [self handleAcceptedAlternativeText:acceptedAlternative];
    });
#endif
}

- (void)_removeDictationAlternatives:(WebCore::DictationContext)dictationContext
{
    _private->m_alternativeTextUIController->removeAlternatives(dictationContext);
}

- (Vector<String>)_dictationAlternatives:(WebCore::DictationContext)dictationContext
{
    return makeVector<String>(_private->m_alternativeTextUIController->alternativesForContext(dictationContext).alternativeStrings);
}

#if ENABLE(SERVICE_CONTROLS)
- (WebSelectionServiceController&)_selectionServiceController
{
    if (!_private->_selectionServiceController)
        _private->_selectionServiceController = makeUnique<WebSelectionServiceController>(self);
    return *_private->_selectionServiceController;
}
#endif

- (NSPoint)_convertPointFromRootView:(NSPoint)point
{
    return NSMakePoint(point.x, [self bounds].size.height - point.y);
}

- (NSRect)_convertRectFromRootView:(NSRect)rect
{
#if PLATFORM(MAC)
    if (self.isFlipped)
        return rect;
#endif
    return NSMakeRect(rect.origin.x, [self bounds].size.height - rect.origin.y - rect.size.height, rect.size.width, rect.size.height);
}

#if PLATFORM(MAC)
- (WebImmediateActionController *)_immediateActionController
{
    return _private->immediateActionController.get();
}

- (id)_animationControllerForDictionaryLookupPopupInfo:(const WebCore::DictionaryPopupInfo&)dictionaryPopupInfo
{
    if (!dictionaryPopupInfo.attributedString)
        return nil;

    [self _prepareForDictionaryLookup];

    return WebCore::DictionaryLookup::animationControllerForPopup(dictionaryPopupInfo, self, [self](WebCore::TextIndicator& textIndicator) {
        [self _setTextIndicator:textIndicator withLifetime:WebCore::TextIndicatorWindowLifetime::Permanent];
    }, [self](WebCore::FloatRect rectInRootViewCoordinates) {
        return [self _convertRectFromRootView:rectInRootViewCoordinates];
    }, [self]() {
        [self _clearTextIndicatorWithAnimation:WebCore::TextIndicatorWindowDismissalAnimation::FadeOut];
    });
}

- (NSEvent *)_pressureEvent
{
    return _private->pressureEvent.get();
}

- (void)_setPressureEvent:(NSEvent *)event
{
    _private->pressureEvent = event;
}

- (void)_setTextIndicator:(WebCore::TextIndicator&)textIndicator
{
    [self _setTextIndicator:textIndicator withLifetime:WebCore::TextIndicatorWindowLifetime::Permanent];
}

- (void)_setTextIndicator:(WebCore::TextIndicator&)textIndicator withLifetime:(WebCore::TextIndicatorWindowLifetime)lifetime
{
    if (!_private->textIndicatorWindow)
        _private->textIndicatorWindow = makeUnique<WebCore::TextIndicatorWindow>(self);

    NSRect textBoundingRectInWindowCoordinates = [self convertRect:[self _convertRectFromRootView:textIndicator.textBoundingRectInRootViewCoordinates()] toView:nil];
    NSRect textBoundingRectInScreenCoordinates = [self.window convertRectToScreen:textBoundingRectInWindowCoordinates];
    _private->textIndicatorWindow->setTextIndicator(textIndicator, NSRectToCGRect(textBoundingRectInScreenCoordinates), lifetime);
}

- (void)_clearTextIndicatorWithAnimation:(WebCore::TextIndicatorWindowDismissalAnimation)animation
{
    if (_private->textIndicatorWindow)
        _private->textIndicatorWindow->clearTextIndicator(WebCore::TextIndicatorWindowDismissalAnimation::FadeOut);
    _private->textIndicatorWindow = nullptr;
}

- (void)_setTextIndicatorAnimationProgress:(float)progress
{
    if (_private->textIndicatorWindow)
        _private->textIndicatorWindow->setAnimationProgress(progress);
}

- (void)_prepareForDictionaryLookup
{
    if (_private->hasInitializedLookupObserver)
        return;

    _private->hasInitializedLookupObserver = YES;

#if !ENABLE(REVEAL)
    if (PAL::canLoad_Lookup_LUNotificationPopoverWillClose())
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_dictionaryLookupPopoverWillClose:) name:PAL::get_Lookup_LUNotificationPopoverWillClose() object:nil];
#endif // !ENABLE(REVEAL)

}

- (void)_showDictionaryLookupPopup:(const WebCore::DictionaryPopupInfo&)dictionaryPopupInfo
{
    if (!dictionaryPopupInfo.attributedString)
        return;

    [self _prepareForDictionaryLookup];

    WebCore::DictionaryLookup::showPopup(dictionaryPopupInfo, self, [self](WebCore::TextIndicator& textIndicator) {
        [self _setTextIndicator:textIndicator withLifetime:WebCore::TextIndicatorWindowLifetime::Permanent];
    }, [self](WebCore::FloatRect rectInRootViewCoordinates) {
        return [self _convertRectFromRootView:rectInRootViewCoordinates];
    }, [weakSelf = WeakObjCPtr<WebView>(self)]() {
        [weakSelf.get() _clearTextIndicatorWithAnimation:WebCore::TextIndicatorWindowDismissalAnimation::FadeOut];
    });
}

#if !ENABLE(REVEAL)
- (void)_dictionaryLookupPopoverWillClose:(NSNotification *)notification
{
    [self _clearTextIndicatorWithAnimation:WebCore::TextIndicatorWindowDismissalAnimation::FadeOut];
}
#endif // ENABLE(REVEAL)

#endif // PLATFORM(MAC)

- (void)showFormValidationMessage:(NSString *)message withAnchorRect:(NSRect)anchorRect
{
    // FIXME: We should enable this on iOS as well.
#if PLATFORM(MAC)
    double minimumFontSize = _private->page ? _private->page->settings().minimumFontSize() : 0;
    _private->formValidationBubble = WebCore::ValidationBubble::create(self, message, { minimumFontSize });
    _private->formValidationBubble->showRelativeTo(WebCore::enclosingIntRect([self _convertRectFromRootView:anchorRect]));
#else
    UNUSED_PARAM(message);
    UNUSED_PARAM(anchorRect);
#endif
}

- (void)hideFormValidationMessage
{
    _private->formValidationBubble = nullptr;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
- (WebMediaPlaybackTargetPicker *) _devicePicker
{
    if (!_private->m_playbackTargetPicker)
        _private->m_playbackTargetPicker = WebMediaPlaybackTargetPicker::create(self, *_private->page);

    return _private->m_playbackTargetPicker.get();
}

- (void)_addPlaybackTargetPickerClient:(WebCore::PlaybackTargetClientContextIdentifier)contextId
{
    [self _devicePicker]->addPlaybackTargetPickerClient(contextId);
}

- (void)_removePlaybackTargetPickerClient:(WebCore::PlaybackTargetClientContextIdentifier)contextId
{
    [self _devicePicker]->removePlaybackTargetPickerClient(contextId);
}

- (void)_showPlaybackTargetPicker:(WebCore::PlaybackTargetClientContextIdentifier)contextId location:(const WebCore::IntPoint&)location hasVideo:(BOOL)hasVideo
{
    if (!_private->page)
        return;

    NSRect rectInScreenCoordinates = [self.window convertRectToScreen:NSMakeRect(location.x(), location.y(), 0, 0)];
    [self _devicePicker]->showPlaybackTargetPicker(contextId, rectInScreenCoordinates, hasVideo);
}

- (void)_playbackTargetPickerClientStateDidChange:(WebCore::PlaybackTargetClientContextIdentifier)contextId state:(WebCore::MediaProducer::MediaStateFlags)state
{
    [self _devicePicker]->playbackTargetPickerClientStateDidChange(contextId, state);
}

- (void)_setMockMediaPlaybackTargetPickerEnabled:(bool)enabled
{
    [self _devicePicker]->setMockMediaPlaybackTargetPickerEnabled(enabled);
}

- (void)_setMockMediaPlaybackTargetPickerName:(NSString *)name state:(WebCore::MediaPlaybackTargetContext::MockState)state
{
    [self _devicePicker]->setMockMediaPlaybackTargetPickerState(name, state);
}

- (void)_mockMediaPlaybackTargetPickerDismissPopup
{
    [self _devicePicker]->mockMediaPlaybackTargetPickerDismissPopup();
}
#endif

#if HAVE(TOUCH_BAR)
- (void)_dismissTextTouchBarPopoverItemWithIdentifier:(NSString *)identifier
{
    NSTouchBarItem *foundItem = nil;
    for (NSTouchBarItem *item in self.textTouchBar.items) {
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

- (NSArray<NSString *> *)_textTouchBarCustomizationAllowedIdentifiers
{
    return @[ NSTouchBarItemIdentifierCharacterPicker, NSTouchBarItemIdentifierTextColorPicker, NSTouchBarItemIdentifierTextStyle, NSTouchBarItemIdentifierTextAlignment, NSTouchBarItemIdentifierTextList, NSTouchBarItemIdentifierFlexibleSpace ];
}

- (NSArray<NSString *> *)_plainTextTouchBarDefaultItemIdentifiers
{
    return @[ NSTouchBarItemIdentifierCharacterPicker, NSTouchBarItemIdentifierCandidateList ];
}

- (NSArray<NSString *> *)_richTextTouchBarDefaultItemIdentifiers
{
    return @[ NSTouchBarItemIdentifierCharacterPicker, NSTouchBarItemIdentifierTextFormat, NSTouchBarItemIdentifierCandidateList ];
}

- (NSArray<NSString *> *)_passwordTextTouchBarDefaultItemIdentifiers
{
    return @[ NSTouchBarItemIdentifierCandidateList ];
}

- (void)touchBarDidExitCustomization:(NSNotification *)notification
{
    _private->_isCustomizingTouchBar = NO;
    [self updateTouchBar];
}

- (void)touchBarWillEnterCustomization:(NSNotification *)notification
{
    _private->_isCustomizingTouchBar = YES;
}

- (void)didChangeAutomaticTextCompletion:(NSNotification *)notification
{
    if (_private->_richTextTouchBar)
        [self setUpTextTouchBar:_private->_richTextTouchBar.get()];

    if (_private->_plainTextTouchBar)
        [self setUpTextTouchBar:_private->_plainTextTouchBar.get()];

    if (_private->_passwordTextTouchBar)
        [self setUpTextTouchBar:_private->_passwordTextTouchBar.get()];

    [self updateTouchBar];
}

- (void)setUpTextTouchBar:(NSTouchBar *)textTouchBar
{
    NSSet<NSTouchBarItem *> *templateItems = nil;
    NSArray<NSTouchBarItemIdentifier> *defaultItemIdentifiers = nil;
    NSArray<NSTouchBarItemIdentifier> *customizationAllowedItemIdentifiers = nil;

    if (textTouchBar == _private->_passwordTextTouchBar) {
        templateItems = [NSMutableSet setWithObject:_private->_passwordTextCandidateListTouchBarItem.get()];
        defaultItemIdentifiers = [self _passwordTextTouchBarDefaultItemIdentifiers];
    } else if (textTouchBar == _private->_richTextTouchBar) {
        templateItems = [NSMutableSet setWithObject:_private->_richTextCandidateListTouchBarItem.get()];
        defaultItemIdentifiers = [self _richTextTouchBarDefaultItemIdentifiers];
        customizationAllowedItemIdentifiers = [self _textTouchBarCustomizationAllowedIdentifiers];
    } else if (textTouchBar == _private->_plainTextTouchBar) {
        templateItems = [NSMutableSet setWithObject:_private->_plainTextCandidateListTouchBarItem.get()];
        defaultItemIdentifiers = [self _plainTextTouchBarDefaultItemIdentifiers];
        customizationAllowedItemIdentifiers = [self _textTouchBarCustomizationAllowedIdentifiers];
    }

    [textTouchBar setDelegate:self];
    [textTouchBar setTemplateItems:templateItems];
    [textTouchBar setDefaultItemIdentifiers:defaultItemIdentifiers];
    [textTouchBar setCustomizationAllowedItemIdentifiers:customizationAllowedItemIdentifiers];

    if (NSGroupTouchBarItem *textFormatItem = (NSGroupTouchBarItem *)[textTouchBar itemForIdentifier:NSTouchBarItemIdentifierTextFormat])
        textFormatItem.groupTouchBar.customizationIdentifier = @"WebTextFormatTouchBar";
}

- (BOOL)_isRichlyEditable
{
    NSView *documentView = self._selectedOrMainFrame.frameView.documentView;
    if (![documentView isKindOfClass:[WebHTMLView class]])
        return NO;

    WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
    return webHTMLView._isEditable && webHTMLView._canEditRichly;
}

- (NSTouchBar *)textTouchBar
{
    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return nil;

    if (coreFrame->selection().selection().isInPasswordField())
        return _private->_passwordTextTouchBar.get();

    return self._isRichlyEditable ? _private->_richTextTouchBar.get() : _private->_plainTextTouchBar.get();
}

static NSTextAlignment nsTextAlignmentFromRenderStyle(const WebCore::RenderStyle* style)
{
    NSTextAlignment textAlignment;
    switch (style->textAlign()) {
    case WebCore::TextAlignMode::Right:
    case WebCore::TextAlignMode::WebKitRight:
        textAlignment = NSTextAlignmentRight;
        break;
    case WebCore::TextAlignMode::Left:
    case WebCore::TextAlignMode::WebKitLeft:
        textAlignment = NSTextAlignmentLeft;
        break;
    case WebCore::TextAlignMode::Center:
    case WebCore::TextAlignMode::WebKitCenter:
        textAlignment = NSTextAlignmentCenter;
        break;
    case WebCore::TextAlignMode::Justify:
        textAlignment = NSTextAlignmentJustified;
        break;
    case WebCore::TextAlignMode::Start:
        textAlignment = style->isLeftToRightDirection() ? NSTextAlignmentLeft : NSTextAlignmentRight;
        break;
    case WebCore::TextAlignMode::End:
        textAlignment = style->isLeftToRightDirection() ? NSTextAlignmentRight : NSTextAlignmentLeft;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return textAlignment;
}

- (void)updateTextTouchBar
{
    using namespace WebCore;
    BOOL touchBarsRequireInitialization = !_private->_richTextTouchBar || !_private->_plainTextTouchBar;
    if (_private->_isDeferringTextTouchBarUpdates && !touchBarsRequireInitialization) {
        _private->_needsDeferredTextTouchBarUpdate = YES;
        return;
    }

    NSView *documentView = [[[self _selectedOrMainFrame] frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]])
        return;

    WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
    if (![webHTMLView _isEditable])
        return;

    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return;

    if (_private->_isUpdatingTextTouchBar)
        return;

    SetForScope<BOOL> isUpdatingTextTouchBar(_private->_isUpdatingTextTouchBar, YES);

    if (!_private->_textTouchBarItemController)
        _private->_textTouchBarItemController = adoptNS([[WebTextTouchBarItemController alloc] initWithWebView:self]);

    if (!_private->_startedListeningToCustomizationEvents) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(touchBarDidExitCustomization:) name:NSTouchBarDidExitCustomization object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(touchBarWillEnterCustomization:) name:NSTouchBarWillEnterCustomization object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didChangeAutomaticTextCompletion:) name:NSSpellCheckerDidChangeAutomaticTextCompletionNotification object:nil];
        _private->_startedListeningToCustomizationEvents = YES;
    }

    if (!_private->_plainTextCandidateListTouchBarItem || !_private->_richTextCandidateListTouchBarItem || !_private->_passwordTextCandidateListTouchBarItem) {
        _private->_plainTextCandidateListTouchBarItem = adoptNS([[NSCandidateListTouchBarItem alloc] initWithIdentifier:NSTouchBarItemIdentifierCandidateList]);
        [_private->_plainTextCandidateListTouchBarItem setDelegate:self];
        _private->_richTextCandidateListTouchBarItem = adoptNS([[NSCandidateListTouchBarItem alloc] initWithIdentifier:NSTouchBarItemIdentifierCandidateList]);
        [_private->_richTextCandidateListTouchBarItem setDelegate:self];
        _private->_passwordTextCandidateListTouchBarItem = adoptNS([[NSCandidateListTouchBarItem alloc] initWithIdentifier:NSTouchBarItemIdentifierCandidateList]);
        [_private->_passwordTextCandidateListTouchBarItem setDelegate:self];

        coreFrame->editor().client()->requestCandidatesForSelection(coreFrame->selection().selection());
    }

    if (!_private->_richTextTouchBar) {
        _private->_richTextTouchBar = adoptNS([[NSTouchBar alloc] init]);
        [self setUpTextTouchBar:_private->_richTextTouchBar.get()];
        [_private->_richTextTouchBar setCustomizationIdentifier:@"WebRichTextTouchBar"];
    }

    if (!_private->_plainTextTouchBar) {
        _private->_plainTextTouchBar = adoptNS([[NSTouchBar alloc] init]);
        [self setUpTextTouchBar:_private->_plainTextTouchBar.get()];
        [_private->_plainTextTouchBar setCustomizationIdentifier:@"WebPlainTextTouchBar"];
    }

    if ([NSSpellChecker isAutomaticTextCompletionEnabled] && !_private->_isCustomizingTouchBar && !coreFrame->editor().ignoreSelectionChanges())
        [self.candidateList updateWithInsertionPointVisibility:!coreFrame->selection().selection().isRange()];

    if (coreFrame->selection().selection().isInPasswordField()) {
        // We don't request candidates for password fields. If the user was previously in a non-password field, then the
        // old candidates will still show by default, so we clear them here by setting an empty array of candidates.
        if (!_private->_passwordTextTouchBar) {
            _private->_passwordTextTouchBar = adoptNS([[NSTouchBar alloc] init]);
            [self setUpTextTouchBar:_private->_passwordTextTouchBar.get()];
        }
        [_private->_passwordTextCandidateListTouchBarItem setCandidates:@[ ] forSelectedRange:NSMakeRange(0, 0) inString:nil];
    }

    NSTouchBar *textTouchBar = self.textTouchBar;
    NSArray<NSString *> *itemIdentifiers = textTouchBar.defaultItemIdentifiers;
    BOOL isShowingCombinedTextFormatItem = [itemIdentifiers containsObject:NSTouchBarItemIdentifierTextFormat];
    [textTouchBar setPrincipalItemIdentifier:isShowingCombinedTextFormatItem ? NSTouchBarItemIdentifierTextFormat : nil];

    // Set current typing attributes for rich text. This will ensure that the buttons reflect the state of
    // the text when changing selection throughout the document.
    if (webHTMLView._canEditRichly) {
        const VisibleSelection& selection = coreFrame->selection().selection();
        if (!selection.isNone()) {
            RefPtr<Node> nodeToRemove;
            if (auto* style = coreFrame->editor().styleForSelectionStart(nodeToRemove)) {
                [_private->_textTouchBarItemController setTextIsBold:isFontWeightBold(style->fontCascade().weight())];
                [_private->_textTouchBarItemController setTextIsItalic:isItalic(style->fontCascade().italic())];

                RefPtr<EditingStyle> typingStyle = coreFrame->selection().typingStyle();
                if (typingStyle && typingStyle->style()) {
                    String value = typingStyle->style()->getPropertyValue(CSSPropertyWebkitTextDecorationsInEffect);
                    [_private->_textTouchBarItemController setTextIsUnderlined:value.contains("underline")];
                } else
                    [_private->_textTouchBarItemController setTextIsUnderlined:style->textDecorationsInEffect().contains(TextDecoration::Underline)];

                Color textColor = style->visitedDependentColor(CSSPropertyColor);
                if (textColor.isValid())
                    [_private->_textTouchBarItemController setTextColor:nsColor(textColor)];

                [_private->_textTouchBarItemController setCurrentTextAlignment:nsTextAlignmentFromRenderStyle(style)];

                auto enclosingListElement = makeRefPtr(enclosingList(selection.start().deprecatedNode()));
                if (enclosingListElement) {
                    if (is<HTMLUListElement>(*enclosingListElement))
                        [[_private->_textTouchBarItemController webTextListTouchBarViewController] setCurrentListType:WebListType::Unordered];
                    else if (is<HTMLOListElement>(*enclosingListElement))
                        [[_private->_textTouchBarItemController webTextListTouchBarViewController] setCurrentListType:WebListType::Ordered];
                    else
                        ASSERT_NOT_REACHED();
                } else
                    [[_private->_textTouchBarItemController webTextListTouchBarViewController] setCurrentListType:WebListType::None];

                if (nodeToRemove)
                    nodeToRemove->remove();
            }
        }
        BOOL isShowingCandidateListItem = [itemIdentifiers containsObject:NSTouchBarItemIdentifierCandidateList] && [NSSpellChecker isAutomaticTextCompletionEnabled];
        [_private->_textTouchBarItemController setUsesNarrowTextStyleItem:isShowingCombinedTextFormatItem && isShowingCandidateListItem];
    }
}

- (void)updateMediaTouchBar
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER) && ENABLE(VIDEO_PRESENTATION_MODE)
    if (!_private->mediaTouchBarProvider)
        _private->mediaTouchBarProvider = adoptNS([allocAVTouchBarPlaybackControlsProviderInstance() init]);

    if (![_private->mediaTouchBarProvider playbackControlsController]) {
        ASSERT(_private->playbackSessionInterface);
        WebPlaybackControlsManager *manager = _private->playbackSessionInterface->playBackControlsManager();
        [_private->mediaTouchBarProvider setPlaybackControlsController:(id <AVTouchBarPlaybackControlsControlling>)manager];
        [_private->mediaPlaybackControlsView setPlaybackControlsController:(id <AVTouchBarPlaybackControlsControlling>)manager];
    }
#endif
}

- (void)updateTouchBar
{
    if (!_private->_canCreateTouchBars)
        return;

    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return;

    NSTouchBar *touchBar = nil;
    NSView *documentView = [[[self _selectedOrMainFrame] frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]]) {
        WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
        if ([webHTMLView _isEditable]) {
            [self updateTextTouchBar];
            touchBar = [self textTouchBar];
        }
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
        else if ([self _hasActiveVideoForControlsInterface]) {
            [self updateMediaTouchBar];
            touchBar = [_private->mediaTouchBarProvider respondsToSelector:@selector(touchBar)] ? [(id)_private->mediaTouchBarProvider touchBar] : [(id)_private->mediaTouchBarProvider touchBar];
        } else if ([_private->mediaTouchBarProvider playbackControlsController]) {
            [_private->mediaTouchBarProvider setPlaybackControlsController:nil];
            [_private->mediaPlaybackControlsView setPlaybackControlsController:nil];
        }
#endif
    }

    if (touchBar == _private->_currentTouchBar)
        return;

    _private->_currentTouchBar = touchBar;
    [self willChangeValueForKey:@"touchBar"];
    [self setTouchBar:_private->_currentTouchBar.get()];
    [self didChangeValueForKey:@"touchBar"];
}

- (void)prepareForMouseDown
{
    _private->_needsDeferredTextTouchBarUpdate = NO;
    _private->_isDeferringTextTouchBarUpdates = YES;
}

- (void)prepareForMouseUp
{
    if (!_private->_isDeferringTextTouchBarUpdates)
        return;

    _private->_isDeferringTextTouchBarUpdates = NO;
    if (_private->_needsDeferredTextTouchBarUpdate) {
        // Only trigger another update if we attempted and bailed from an update during the deferral.
        [self updateTouchBar];
    }
}

- (NSCandidateListTouchBarItem *)candidateList
{
    auto* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return nil;

    if (coreFrame->selection().selection().isInPasswordField())
        return _private->_passwordTextCandidateListTouchBarItem.get();

    return self._isRichlyEditable ? _private->_richTextCandidateListTouchBarItem.get() : _private->_plainTextCandidateListTouchBarItem.get();
}
#else

- (void)updateTouchBar
{
}

- (void)prepareForMouseDown
{
}

- (void)prepareForMouseUp
{
}

- (id)candidateList
{
    return nil;
}

#endif

- (void)_windowVisibilityChanged:(NSNotification *)notification
{
    [self _updateVisibilityState];
}

- (void)_closeWindow
{
    [[self _UIDelegateForwarder] webViewClose:self];
}

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)

+ (BOOL)_canHandleContextMenuTranslation
{
    return TranslationUIServicesLibrary() && [getLTUITranslationViewControllerClass() isAvailable];
}

- (void)_handleContextMenuTranslation:(const WebCore::TranslationContextMenuInfo&)info
{
    if (!WebView._canHandleContextMenuTranslation) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto translationViewController = adoptNS([allocLTUITranslationViewControllerInstance() init]);
    [translationViewController setText:adoptNS([[NSAttributedString alloc] initWithString:info.text]).get()];
    if (info.mode == WebCore::TranslationContextMenuMode::Editable && [translationViewController respondsToSelector:@selector(setReplacementHandler:)]) {
        [translationViewController setIsSourceEditable:YES];
        [translationViewController setReplacementHandler:[weakSelf = WeakObjCPtr<WebView>(self)](NSAttributedString *string) {
            auto strongSelf = weakSelf.get();
            [strongSelf insertText:string.string];
        }];
    }

    auto convertedSelectionBounds = [self _convertRectFromRootView:info.selectionBoundsInRootView];
    auto convertedMenuLocation = [self _convertPointFromRootView:info.locationInRootView];

    auto popover = adoptNS([[NSPopover alloc] init]);
    [popover setBehavior:NSPopoverBehaviorTransient];
    [popover setAppearance:self.effectiveAppearance];
    [popover setAnimates:YES];
    [popover setContentViewController:translationViewController.get()];
    [popover setContentSize:[translationViewController preferredContentSize]];

    NSRectEdge preferredEdge;
    auto aim = convertedMenuLocation.x;
    auto highlight = NSMidX(convertedSelectionBounds);
    if (WTF::areEssentiallyEqual<CGFloat>(aim, highlight))
        preferredEdge = self.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft ? NSRectEdgeMinX : NSRectEdgeMaxX;
    else
        preferredEdge = aim > highlight ? NSRectEdgeMaxX : NSRectEdgeMinX;

    [popover showRelativeToRect:convertedSelectionBounds ofView:self preferredEdge:preferredEdge];
}

#endif // HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)

@end

@implementation WebView (WebViewDeviceOrientation)

- (void)_setDeviceOrientationProvider:(id<WebDeviceOrientationProvider>)deviceOrientationProvider
{
    if (_private)
        _private->m_deviceOrientationProvider = deviceOrientationProvider;
}

- (id<WebDeviceOrientationProvider>)_deviceOrientationProvider
{
    if (_private)
        return _private->m_deviceOrientationProvider;
    return nil;
}

@end

@implementation WebView (WebViewGeolocation)

- (void)_setGeolocationProvider:(id<WebGeolocationProvider>)geolocationProvider
{
    if (_private)
        _private->_geolocationProvider = geolocationProvider;
}

- (id<WebGeolocationProvider>)_geolocationProvider
{
    if (_private)
        return _private->_geolocationProvider;
    return nil;
}

- (void)_geolocationDidChangePosition:(WebGeolocationPosition *)position
{
#if ENABLE(GEOLOCATION)
    if (_private && _private->page)
        WebCore::GeolocationController::from(_private->page)->positionChanged(core(position));
#endif // ENABLE(GEOLOCATION)
}

- (void)_geolocationDidFailWithMessage:(NSString *)errorMessage
{
#if ENABLE(GEOLOCATION)
    if (_private && _private->page) {
        auto geolocatioError = WebCore::GeolocationError::create(WebCore::GeolocationError::PositionUnavailable, errorMessage);
        WebCore::GeolocationController::from(_private->page)->errorOccurred(geolocatioError.get());
    }
#endif // ENABLE(GEOLOCATION)
}

#if PLATFORM(IOS_FAMILY)
- (void)_resetAllGeolocationPermission
{
#if ENABLE(GEOLOCATION)
    auto* frame = [self _mainCoreFrame];
    if (frame)
        frame->resetAllGeolocationPermission();
#endif
}
#endif

@end

@implementation WebView (WebViewNotification)
- (void)_setNotificationProvider:(id<WebNotificationProvider>)notificationProvider
{
    if (_private && !_private->_notificationProvider) {
        _private->_notificationProvider = notificationProvider;
        [_private->_notificationProvider registerWebView:self];
    }
}

- (id<WebNotificationProvider>)_notificationProvider
{
    if (_private)
        return _private->_notificationProvider;
    return nil;
}

- (void)_notificationDidShow:(uint64_t)notificationID
{
    [[self _notificationProvider] webView:self didShowNotification:notificationID];
}

- (void)_notificationDidClick:(uint64_t)notificationID
{
    [[self _notificationProvider] webView:self didClickNotification:notificationID];
}

- (void)_notificationsDidClose:(NSArray *)notificationIDs
{
    [[self _notificationProvider] webView:self didCloseNotifications:notificationIDs];
}

- (uint64_t)_notificationIDForTesting:(JSValueRef)jsNotification
{
#if ENABLE(NOTIFICATIONS)
    auto* page = _private->page;
    if (!page)
        return 0;
    JSContextRef context = [[self mainFrame] globalContext];
    auto* notification = WebCore::JSNotification::toWrapped(toJS(context)->vm(), toJS(toJS(context), jsNotification));
    return static_cast<WebNotificationClient*>(WebCore::NotificationController::clientFrom(*page))->notificationIDForTesting(notification);
#else
    return 0;
#endif
}
@end

@implementation WebView (WebViewFontSelection)

+ (void)_setFontAllowList:(NSArray *)allowList
{
#if !PLATFORM(MAC)
    UNUSED_PARAM(allowList);
#else
    WebCore::FontCache::setFontAllowlist(makeVector<String>(allowList));
#endif
}

@end

#if PLATFORM(IOS_FAMILY)

@implementation WebView (WebViewIOSPDF)

+ (Class)_getPDFRepresentationClass
{
    if (s_pdfRepresentationClass)
        return s_pdfRepresentationClass;
    return [WebPDFView class]; // This is WebPDFRepresentation for PLATFORM(MAC).
}

+ (void)_setPDFRepresentationClass:(Class)pdfRepresentationClass
{
    s_pdfRepresentationClass = pdfRepresentationClass;
}

+ (Class)_getPDFViewClass
{
    if (s_pdfViewClass)
        return s_pdfViewClass;
    return [WebPDFView class];
}

+ (void)_setPDFViewClass:(Class)pdfViewClass
{
    s_pdfViewClass = pdfViewClass;
}

@end

@implementation WebView (WebViewIOSAdditions)

- (NSArray<DOMElement *> *)_editableElementsInRect:(CGRect)rect
{
    auto* page = core(self);
    if (!page)
        return @[];
    return createNSArray(page->editableElementsInRect(rect), [] (auto& coreElement) {
        return kit(coreElement.ptr());
    }).autorelease();
}

- (void)revealCurrentSelection
{
    if (auto* page = core(self))
        page->revealCurrentSelection();
}

- (void)_installVisualIdentificationOverlayForViewIfNeeded:(id)view kind:(NSString *)kind
{
    [WebViewVisualIdentificationOverlay installForWebViewIfNeeded:static_cast<UIView *>(view) kind:kind deprecated:YES];
}

@end

#endif

@implementation WebView (WebViewFullScreen)

- (NSView*)fullScreenPlaceholderView
{
#if ENABLE(FULLSCREEN_API) && !PLATFORM(IOS_FAMILY)
    if (_private->newFullscreenController && [_private->newFullscreenController isFullScreen])
        return [_private->newFullscreenController webViewPlaceholder];
#endif
    return nil;
}

@end

void WebInstallMemoryPressureHandler(void)
{
    if (![[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitSuppressMemoryPressureHandler"]) {
        WebCore::registerMemoryReleaseNotifyCallbacks();

        static std::once_flag onceFlag;
        std::call_once(onceFlag, [] {
            auto& memoryPressureHandler = MemoryPressureHandler::singleton();
            memoryPressureHandler.setLowMemoryHandler([] (Critical critical, Synchronous synchronous) {
#if PLATFORM(IOS_FAMILY)
                WebThreadRun(^{
#endif
                WebCore::releaseMemory(critical, synchronous);
#if PLATFORM(IOS_FAMILY)
                });
#endif
            });
            memoryPressureHandler.install();
        });
    }
}
