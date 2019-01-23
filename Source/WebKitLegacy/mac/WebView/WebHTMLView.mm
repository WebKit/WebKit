/*
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
 *           (C) 2006, 2007 Graham Dennis (graham.dennis@gmail.com)
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

#import "WebHTMLView.h"

#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMDocumentFragmentInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebArchive.h"
#import "WebClipView.h"
#import "WebContextMenuClient.h"
#import "WebDOMOperationsInternal.h"
#import "WebDataSourceInternal.h"
#import "WebDefaultUIDelegate.h"
#import "WebDelegateImplementationCaching.h"
#import "WebDocumentInternal.h"
#import "WebDynamicScrollBarsViewInternal.h"
#import "WebEditingDelegate.h"
#import "WebElementDictionary.h"
#import "WebFrameInternal.h"
#import "WebFramePrivate.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentationPrivate.h"
#import "WebHTMLViewInternal.h"
#import "WebImmediateActionController.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitVersionChecks.h"
#import "WebLocalizableStringsInternal.h"
#import "WebNSFileManagerExtras.h"
#import "WebNSImageExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSPrintOperationExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSViewExtras.h"
#import "WebNetscapePluginView.h"
#import "WebNodeHighlight.h"
#import "WebPluginController.h"
#import "WebPreferences.h"
#import "WebPreferencesPrivate.h"
#import "WebResourcePrivate.h"
#import "WebSharingServicePickerController.h"
#import "WebTextCompletionController.h"
#import "WebTypesInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/CSSStyleDeclaration.h>
#import <WebCore/CachedImage.h>
#import <WebCore/CachedResourceClient.h>
#import <WebCore/CachedResourceLoader.h>
#import <WebCore/Chrome.h>
#import <WebCore/ColorMac.h>
#import <WebCore/ContextMenu.h>
#import <WebCore/ContextMenuController.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentFragment.h>
#import <WebCore/DocumentMarkerController.h>
#import <WebCore/DragController.h>
#import <WebCore/DragImage.h>
#import <WebCore/Editor.h>
#import <WebCore/EditorDeleteAction.h>
#import <WebCore/Element.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FloatRect.h>
#import <WebCore/FocusController.h>
#import <WebCore/Font.h>
#import <WebCore/FontAttributeChanges.h>
#import <WebCore/FontAttributes.h>
#import <WebCore/FontCache.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameSelection.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/Image.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/Page.h>
#import <WebCore/PrintContext.h>
#import <WebCore/Range.h>
#import <WebCore/RenderView.h>
#import <WebCore/RenderWidget.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/RuntimeEnabledFeatures.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/StyleProperties.h>
#import <WebCore/StyleScope.h>
#import <WebCore/Text.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TextUndoInsertionMarkupMac.h>
#import <WebCore/WebCoreNSFontManagerExtras.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebNSAttributedStringExtras.h>
#import <WebCore/markup.h>
#import <WebKitLegacy/DOM.h>
#import <WebKitLegacy/DOMExtensions.h>
#import <WebKitLegacy/DOMPrivate.h>
#import <dlfcn.h>
#import <limits>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <pal/spi/cocoa/NSURLFileTypeMappingsSPI.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>
#import <pal/spi/mac/NSViewSPI.h>
#import <pal/spi/mac/NSWindowSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/MainThread.h>
#import <wtf/MathExtras.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/RunLoop.h>
#import <wtf/SystemTracing.h>

#if PLATFORM(MAC)
#import "WebNSEventExtras.h"
#import "WebNSPasteboardExtras.h"
#import <AppKit/NSAccessibility.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <pal/spi/mac/NSMenuSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "WebUIKitDelegate.h"
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/KeyEventCodesIOS.h>
#import <WebCore/PlatformEventFactoryIOS.h>
#import <WebCore/WAKClipView.h>
#import <WebCore/WAKScrollView.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WKGraphics.h>
#import <WebCore/WebCoreThreadRun.h>
#import <WebCore/WebEvent.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#endif

using namespace WebCore;
using namespace HTMLNames;
using namespace WTF;

#if PLATFORM(IOS_FAMILY)

@interface NSObject (Accessibility)
- (id)accessibilityHitTest:(NSPoint)point;
- (id)accessibilityFocusedUIElement;
@end

#endif

#if PLATFORM(MAC)

@class NSTextInputContext;

@interface NSApplication ()
- (BOOL)isSpeaking;
- (void)speakString:(NSString *)string;
- (void)stopSpeaking:(id)sender;
@end

@interface NSAttributedString ()
- (DOMDocumentFragment *)_documentFromRange:(NSRange)range document:(DOMDocument *)document documentAttributes:(NSDictionary *)dict subresources:(NSArray **)subresources;
@end

@interface NSObject ()
- (BOOL)handleMouseEvent:(NSEvent *)event;
- (BOOL)wantsToHandleMouseEvents;
@end

@interface NSResponder ()
- (NSTextInputContext *)inputContext;
@end

@interface NSView ()
- (BOOL)_drawnByAncestor;
- (void)_invalidateGStatesForTree;
- (void)_windowChangedKeyState;
@end

@interface NSWindow ()
- (id)_newFirstResponderAfterResigning;
@end

@interface NSWindow (WebBorderViewAccess)
- (NSView *)_web_borderView;
@end

using WebEvent = NSEvent;
const auto WebEventMouseDown = NSEventTypeLeftMouseDown;

@interface WebMenuTarget : NSObject {
    WebCore::ContextMenuController* _menuController;
}
+ (WebMenuTarget*)sharedMenuTarget;
- (WebCore::ContextMenuController*)menuController;
- (void)setMenuController:(WebCore::ContextMenuController*)menuController;
- (void)forwardContextMenuAction:(id)sender;
@end

static Optional<ContextMenuAction> toAction(NSInteger tag)
{
    if (tag >= ContextMenuItemBaseCustomTag && tag <= ContextMenuItemLastCustomTag) {
        // Just pass these through.
        return static_cast<ContextMenuAction>(tag);
    }

    switch (tag) {
    case WebMenuItemTagOpenLinkInNewWindow:
        return ContextMenuItemTagOpenLinkInNewWindow;
    case WebMenuItemTagDownloadLinkToDisk:
        return ContextMenuItemTagDownloadLinkToDisk;
    case WebMenuItemTagCopyLinkToClipboard:
        return ContextMenuItemTagCopyLinkToClipboard;
    case WebMenuItemTagOpenImageInNewWindow:
        return ContextMenuItemTagOpenImageInNewWindow;
    case WebMenuItemTagDownloadImageToDisk:
        return ContextMenuItemTagDownloadImageToDisk;
    case WebMenuItemTagCopyImageToClipboard:
        return ContextMenuItemTagCopyImageToClipboard;
    case WebMenuItemTagOpenFrameInNewWindow:
        return ContextMenuItemTagOpenFrameInNewWindow;
    case WebMenuItemTagCopy:
        return ContextMenuItemTagCopy;
    case WebMenuItemTagGoBack:
        return ContextMenuItemTagGoBack;
    case WebMenuItemTagGoForward:
        return ContextMenuItemTagGoForward;
    case WebMenuItemTagStop:
        return ContextMenuItemTagStop;
    case WebMenuItemTagReload:
        return ContextMenuItemTagReload;
    case WebMenuItemTagCut:
        return ContextMenuItemTagCut;
    case WebMenuItemTagPaste:
        return ContextMenuItemTagPaste;
    case WebMenuItemTagSpellingGuess:
        return ContextMenuItemTagSpellingGuess;
    case WebMenuItemTagNoGuessesFound:
        return ContextMenuItemTagNoGuessesFound;
    case WebMenuItemTagIgnoreSpelling:
        return ContextMenuItemTagIgnoreSpelling;
    case WebMenuItemTagLearnSpelling:
        return ContextMenuItemTagLearnSpelling;
    case WebMenuItemTagOther:
        return ContextMenuItemTagOther;
    case WebMenuItemTagSearchInSpotlight:
        return ContextMenuItemTagSearchInSpotlight;
    case WebMenuItemTagSearchWeb:
        return ContextMenuItemTagSearchWeb;
    case WebMenuItemTagLookUpInDictionary:
        return ContextMenuItemTagLookUpInDictionary;
    case WebMenuItemTagOpenWithDefaultApplication:
        return ContextMenuItemTagOpenWithDefaultApplication;
    case WebMenuItemPDFActualSize:
        return ContextMenuItemPDFActualSize;
    case WebMenuItemPDFZoomIn:
        return ContextMenuItemPDFZoomIn;
    case WebMenuItemPDFZoomOut:
        return ContextMenuItemPDFZoomOut;
    case WebMenuItemPDFAutoSize:
        return ContextMenuItemPDFAutoSize;
    case WebMenuItemPDFSinglePage:
        return ContextMenuItemPDFSinglePage;
    case WebMenuItemPDFFacingPages:
        return ContextMenuItemPDFFacingPages;
    case WebMenuItemPDFContinuous:
        return ContextMenuItemPDFContinuous;
    case WebMenuItemPDFNextPage:
        return ContextMenuItemPDFNextPage;
    case WebMenuItemPDFPreviousPage:
        return ContextMenuItemPDFPreviousPage;
    case WebMenuItemTagOpenLink:
        return ContextMenuItemTagOpenLink;
    case WebMenuItemTagIgnoreGrammar:
        return ContextMenuItemTagIgnoreGrammar;
    case WebMenuItemTagSpellingMenu:
        return ContextMenuItemTagSpellingMenu;
    case WebMenuItemTagShowSpellingPanel:
        return ContextMenuItemTagShowSpellingPanel;
    case WebMenuItemTagCheckSpelling:
        return ContextMenuItemTagCheckSpelling;
    case WebMenuItemTagCheckSpellingWhileTyping:
        return ContextMenuItemTagCheckSpellingWhileTyping;
    case WebMenuItemTagCheckGrammarWithSpelling:
        return ContextMenuItemTagCheckGrammarWithSpelling;
    case WebMenuItemTagFontMenu:
        return ContextMenuItemTagFontMenu;
    case WebMenuItemTagShowFonts:
        return ContextMenuItemTagShowFonts;
    case WebMenuItemTagBold:
        return ContextMenuItemTagBold;
    case WebMenuItemTagItalic:
        return ContextMenuItemTagItalic;
    case WebMenuItemTagUnderline:
        return ContextMenuItemTagUnderline;
    case WebMenuItemTagOutline:
        return ContextMenuItemTagOutline;
    case WebMenuItemTagStyles:
        return ContextMenuItemTagStyles;
    case WebMenuItemTagShowColors:
        return ContextMenuItemTagShowColors;
    case WebMenuItemTagSpeechMenu:
        return ContextMenuItemTagSpeechMenu;
    case WebMenuItemTagStartSpeaking:
        return ContextMenuItemTagStartSpeaking;
    case WebMenuItemTagStopSpeaking:
        return ContextMenuItemTagStopSpeaking;
    case WebMenuItemTagWritingDirectionMenu:
        return ContextMenuItemTagWritingDirectionMenu;
    case WebMenuItemTagDefaultDirection:
        return ContextMenuItemTagDefaultDirection;
    case WebMenuItemTagLeftToRight:
        return ContextMenuItemTagLeftToRight;
    case WebMenuItemTagRightToLeft:
        return ContextMenuItemTagRightToLeft;
    case WebMenuItemPDFSinglePageScrolling:
        return ContextMenuItemTagPDFSinglePageScrolling;
    case WebMenuItemPDFFacingPagesScrolling:
        return ContextMenuItemTagPDFFacingPagesScrolling;
    case WebMenuItemTagInspectElement:
        return ContextMenuItemTagInspectElement;
    case WebMenuItemTagTextDirectionMenu:
        return ContextMenuItemTagTextDirectionMenu;
    case WebMenuItemTagTextDirectionDefault:
        return ContextMenuItemTagTextDirectionDefault;
    case WebMenuItemTagTextDirectionLeftToRight:
        return ContextMenuItemTagTextDirectionLeftToRight;
    case WebMenuItemTagTextDirectionRightToLeft:
        return ContextMenuItemTagTextDirectionRightToLeft;
    case WebMenuItemTagCorrectSpellingAutomatically:
        return ContextMenuItemTagCorrectSpellingAutomatically;
    case WebMenuItemTagSubstitutionsMenu:
        return ContextMenuItemTagSubstitutionsMenu;
    case WebMenuItemTagShowSubstitutions:
        return ContextMenuItemTagShowSubstitutions;
    case WebMenuItemTagSmartCopyPaste:
        return ContextMenuItemTagSmartCopyPaste;
    case WebMenuItemTagSmartQuotes:
        return ContextMenuItemTagSmartQuotes;
    case WebMenuItemTagSmartDashes:
        return ContextMenuItemTagSmartDashes;
    case WebMenuItemTagSmartLinks:
        return ContextMenuItemTagSmartLinks;
    case WebMenuItemTagTextReplacement:
        return ContextMenuItemTagTextReplacement;
    case WebMenuItemTagTransformationsMenu:
        return ContextMenuItemTagTransformationsMenu;
    case WebMenuItemTagMakeUpperCase:
        return ContextMenuItemTagMakeUpperCase;
    case WebMenuItemTagMakeLowerCase:
        return ContextMenuItemTagMakeLowerCase;
    case WebMenuItemTagCapitalize:
        return ContextMenuItemTagCapitalize;
    case WebMenuItemTagChangeBack:
        return ContextMenuItemTagChangeBack;
    case WebMenuItemTagOpenMediaInNewWindow:
        return ContextMenuItemTagOpenMediaInNewWindow;
    case WebMenuItemTagCopyMediaLinkToClipboard:
        return ContextMenuItemTagCopyMediaLinkToClipboard;
    case WebMenuItemTagToggleMediaControls:
        return ContextMenuItemTagToggleMediaControls;
    case WebMenuItemTagToggleMediaLoop:
        return ContextMenuItemTagToggleMediaLoop;
    case WebMenuItemTagEnterVideoFullscreen:
        return ContextMenuItemTagEnterVideoFullscreen;
    case WebMenuItemTagToggleVideoEnhancedFullscreen:
        return ContextMenuItemTagToggleVideoEnhancedFullscreen;
    case WebMenuItemTagMediaPlayPause:
        return ContextMenuItemTagMediaPlayPause;
    case WebMenuItemTagMediaMute:
        return ContextMenuItemTagMediaMute;
    case WebMenuItemTagDictationAlternative:
        return ContextMenuItemTagDictationAlternative;
    }
    return WTF::nullopt;
}

static Optional<NSInteger> toTag(ContextMenuAction action)
{
    switch (action) {
    case ContextMenuItemTagNoAction:
        return WTF::nullopt;

    case ContextMenuItemTagOpenLinkInNewWindow:
        return WebMenuItemTagOpenLinkInNewWindow;
    case ContextMenuItemTagDownloadLinkToDisk:
        return WebMenuItemTagDownloadLinkToDisk;
    case ContextMenuItemTagCopyLinkToClipboard:
        return WebMenuItemTagCopyLinkToClipboard;
    case ContextMenuItemTagOpenImageInNewWindow:
        return WebMenuItemTagOpenImageInNewWindow;
    case ContextMenuItemTagDownloadImageToDisk:
        return WebMenuItemTagDownloadImageToDisk;
    case ContextMenuItemTagCopyImageToClipboard:
        return WebMenuItemTagCopyImageToClipboard;
    case ContextMenuItemTagOpenFrameInNewWindow:
        return WebMenuItemTagOpenFrameInNewWindow;
    case ContextMenuItemTagCopy:
        return WebMenuItemTagCopy;
    case ContextMenuItemTagGoBack:
        return WebMenuItemTagGoBack;
    case ContextMenuItemTagGoForward:
        return WebMenuItemTagGoForward;
    case ContextMenuItemTagStop:
        return WebMenuItemTagStop;
    case ContextMenuItemTagReload:
        return WebMenuItemTagReload;
    case ContextMenuItemTagCut:
        return WebMenuItemTagCut;
    case ContextMenuItemTagPaste:
        return WebMenuItemTagPaste;
    case ContextMenuItemTagSpellingGuess:
        return WebMenuItemTagSpellingGuess;
    case ContextMenuItemTagNoGuessesFound:
        return WebMenuItemTagNoGuessesFound;
    case ContextMenuItemTagIgnoreSpelling:
        return WebMenuItemTagIgnoreSpelling;
    case ContextMenuItemTagLearnSpelling:
        return WebMenuItemTagLearnSpelling;
    case ContextMenuItemTagOther:
        return WebMenuItemTagOther;
    case ContextMenuItemTagSearchInSpotlight:
        return WebMenuItemTagSearchInSpotlight;
    case ContextMenuItemTagSearchWeb:
        return WebMenuItemTagSearchWeb;
    case ContextMenuItemTagLookUpInDictionary:
        return WebMenuItemTagLookUpInDictionary;
    case ContextMenuItemTagOpenWithDefaultApplication:
        return WebMenuItemTagOpenWithDefaultApplication;
    case ContextMenuItemPDFActualSize:
        return WebMenuItemPDFActualSize;
    case ContextMenuItemPDFZoomIn:
        return WebMenuItemPDFZoomIn;
    case ContextMenuItemPDFZoomOut:
        return WebMenuItemPDFZoomOut;
    case ContextMenuItemPDFAutoSize:
        return WebMenuItemPDFAutoSize;
    case ContextMenuItemPDFSinglePage:
        return WebMenuItemPDFSinglePage;
    case ContextMenuItemPDFFacingPages:
        return WebMenuItemPDFFacingPages;
    case ContextMenuItemPDFContinuous:
        return WebMenuItemPDFContinuous;
    case ContextMenuItemPDFNextPage:
        return WebMenuItemPDFNextPage;
    case ContextMenuItemPDFPreviousPage:
        return WebMenuItemPDFPreviousPage;
    case ContextMenuItemTagOpenLink:
        return WebMenuItemTagOpenLink;
    case ContextMenuItemTagIgnoreGrammar:
        return WebMenuItemTagIgnoreGrammar;
    case ContextMenuItemTagSpellingMenu:
        return WebMenuItemTagSpellingMenu;
    case ContextMenuItemTagShowSpellingPanel:
        return WebMenuItemTagShowSpellingPanel;
    case ContextMenuItemTagCheckSpelling:
        return WebMenuItemTagCheckSpelling;
    case ContextMenuItemTagCheckSpellingWhileTyping:
        return WebMenuItemTagCheckSpellingWhileTyping;
    case ContextMenuItemTagCheckGrammarWithSpelling:
        return WebMenuItemTagCheckGrammarWithSpelling;
    case ContextMenuItemTagFontMenu:
        return WebMenuItemTagFontMenu;
    case ContextMenuItemTagShowFonts:
        return WebMenuItemTagShowFonts;
    case ContextMenuItemTagBold:
        return WebMenuItemTagBold;
    case ContextMenuItemTagItalic:
        return WebMenuItemTagItalic;
    case ContextMenuItemTagUnderline:
        return WebMenuItemTagUnderline;
    case ContextMenuItemTagOutline:
        return WebMenuItemTagOutline;
    case ContextMenuItemTagStyles:
        return WebMenuItemTagStyles;
    case ContextMenuItemTagShowColors:
        return WebMenuItemTagShowColors;
    case ContextMenuItemTagSpeechMenu:
        return WebMenuItemTagSpeechMenu;
    case ContextMenuItemTagStartSpeaking:
        return WebMenuItemTagStartSpeaking;
    case ContextMenuItemTagStopSpeaking:
        return WebMenuItemTagStopSpeaking;
    case ContextMenuItemTagWritingDirectionMenu:
        return WebMenuItemTagWritingDirectionMenu;
    case ContextMenuItemTagDefaultDirection:
        return WebMenuItemTagDefaultDirection;
    case ContextMenuItemTagLeftToRight:
        return WebMenuItemTagLeftToRight;
    case ContextMenuItemTagRightToLeft:
        return WebMenuItemTagRightToLeft;
    case ContextMenuItemTagPDFSinglePageScrolling:
        return WebMenuItemPDFSinglePageScrolling;
    case ContextMenuItemTagPDFFacingPagesScrolling:
        return WebMenuItemPDFFacingPagesScrolling;
    case ContextMenuItemTagInspectElement:
        return WebMenuItemTagInspectElement;
    case ContextMenuItemTagTextDirectionMenu:
        return WebMenuItemTagTextDirectionMenu;
    case ContextMenuItemTagTextDirectionDefault:
        return WebMenuItemTagTextDirectionDefault;
    case ContextMenuItemTagTextDirectionLeftToRight:
        return WebMenuItemTagTextDirectionLeftToRight;
    case ContextMenuItemTagTextDirectionRightToLeft:
        return WebMenuItemTagTextDirectionRightToLeft;
    case ContextMenuItemTagCorrectSpellingAutomatically:
        return WebMenuItemTagCorrectSpellingAutomatically;
    case ContextMenuItemTagSubstitutionsMenu:
        return WebMenuItemTagSubstitutionsMenu;
    case ContextMenuItemTagShowSubstitutions:
        return WebMenuItemTagShowSubstitutions;
    case ContextMenuItemTagSmartCopyPaste:
        return WebMenuItemTagSmartCopyPaste;
    case ContextMenuItemTagSmartQuotes:
        return WebMenuItemTagSmartQuotes;
    case ContextMenuItemTagSmartDashes:
        return WebMenuItemTagSmartDashes;
    case ContextMenuItemTagSmartLinks:
        return WebMenuItemTagSmartLinks;
    case ContextMenuItemTagTextReplacement:
        return WebMenuItemTagTextReplacement;
    case ContextMenuItemTagTransformationsMenu:
        return WebMenuItemTagTransformationsMenu;
    case ContextMenuItemTagMakeUpperCase:
        return WebMenuItemTagMakeUpperCase;
    case ContextMenuItemTagMakeLowerCase:
        return WebMenuItemTagMakeLowerCase;
    case ContextMenuItemTagCapitalize:
        return WebMenuItemTagCapitalize;
    case ContextMenuItemTagChangeBack:
        return WebMenuItemTagChangeBack;
    case ContextMenuItemTagOpenMediaInNewWindow:
        return WebMenuItemTagOpenMediaInNewWindow;
    case ContextMenuItemTagDownloadMediaToDisk:
        return WebMenuItemTagDownloadMediaToDisk;
    case ContextMenuItemTagCopyMediaLinkToClipboard:
        return WebMenuItemTagCopyMediaLinkToClipboard;
    case ContextMenuItemTagToggleMediaControls:
        return WebMenuItemTagToggleMediaControls;
    case ContextMenuItemTagToggleMediaLoop:
        return WebMenuItemTagToggleMediaLoop;
    case ContextMenuItemTagEnterVideoFullscreen:
        return WebMenuItemTagEnterVideoFullscreen;
    case ContextMenuItemTagMediaPlayPause:
        return WebMenuItemTagMediaPlayPause;
    case ContextMenuItemTagMediaMute:
        return WebMenuItemTagMediaMute;
    case ContextMenuItemTagDictationAlternative:
        return WebMenuItemTagDictationAlternative;
    case ContextMenuItemTagToggleVideoFullscreen:
        return WebMenuItemTagToggleVideoFullscreen;
    case ContextMenuItemTagShareMenu:
        return WebMenuItemTagShareMenu;
    case ContextMenuItemTagToggleVideoEnhancedFullscreen:
        return WebMenuItemTagToggleVideoEnhancedFullscreen;

    case ContextMenuItemBaseCustomTag ... ContextMenuItemLastCustomTag:
        // We just pass these through.
        return static_cast<NSInteger>(action);

    case ContextMenuItemBaseApplicationTag:
        ASSERT_NOT_REACHED();
    }

    return WTF::nullopt;
}

@implementation WebMenuTarget

+ (WebMenuTarget *)sharedMenuTarget
{
    static WebMenuTarget *target = [[WebMenuTarget alloc] init];
    return target;
}

- (WebCore::ContextMenuController*)menuController
{
    return _menuController;
}

- (void)setMenuController:(WebCore::ContextMenuController*)menuController
{
    _menuController = menuController;
}

- (void)forwardContextMenuAction:(id)sender
{
    if (auto action = toAction([sender tag]))
        _menuController->contextMenuItemSelected(*action, [sender title]);
}

@end

@implementation NSWindow (WebBorderViewAccess)

- (NSView *)_web_borderView
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return _borderView;
    ALLOW_DEPRECATED_DECLARATIONS_END
}

@end

@interface WebResponderChainSink : NSResponder {
    NSResponder* _lastResponderInChain;
    BOOL _receivedUnhandledCommand;
}
- (id)initWithResponderChain:(NSResponder *)chain;
- (void)detach;
- (BOOL)receivedUnhandledCommand;
@end

@interface WebLayerHostingFlippedView : NSView
@end

@implementation WebLayerHostingFlippedView

- (BOOL)isFlipped
{
    return YES;
}

@end

@interface WebRootLayer : CALayer
@end

@implementation WebRootLayer

- (void)renderInContext:(CGContextRef)graphicsContext
{
    // AppKit calls -[CALayer renderInContext:] to render layer-backed views
    // into bitmap contexts, but renderInContext: doesn't capture mask layers
    // (<rdar://problem/9539526>), so we can't rely on it. Since our layer
    // contents will have already been rendered by drawRect:, we can safely make
    // this a NOOP.
}

@end

// if YES, do the standard NSView hit test (which can't give the right result when HTML overlaps a view)
static BOOL forceNSViewHitTest;

// if YES, do the "top WebHTMLView" hit test (which we'd like to do all the time but can't because of Java requirements [see bug 4349721])
static BOOL forceWebHTMLViewHitTest;

static WebHTMLView *lastHitView;

static bool needsCursorRectsSupportAtPoint(NSWindow* window, NSPoint point)
{
    forceNSViewHitTest = YES;
    NSView* view = [[window _web_borderView] hitTest:point];
    forceNSViewHitTest = NO;

    // WebHTMLView doesn't use cursor rects.
    if ([view isKindOfClass:[WebHTMLView class]])
        return false;

#if ENABLE(NETSCAPE_PLUGIN_API)
    // Neither do NPAPI plug-ins.
    if ([view isKindOfClass:[WebBaseNetscapePluginView class]])
        return false;
#endif

    // Non-Web content, WebPDFView, and WebKit plug-ins use normal cursor handling.
    return true;
}

static IMP oldSetCursorForMouseLocationIMP;

// Overriding an internal method is a hack; <rdar://problem/7662987> tracks finding a better solution.
static void setCursor(NSWindow *self, SEL cmd, NSPoint point)
{
    if (needsCursorRectsSupportAtPoint(self, point))
        wtfCallIMP<id>(oldSetCursorForMouseLocationIMP, self, cmd, point);
}

// FIXME: Get this from <AppKit/NSTextInputContext_Private.h> using a NSTextInputContextSPI.h header instead of defining it here.
extern "C" NSString *NSTextInputReplacementRangeAttributeName;

#endif // PLATFORM(MAC)

@interface NSView ()
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView;
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect;
#if PLATFORM(MAC)
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
- (void)_recursive:(BOOL)recursive displayRectIgnoringOpacity:(NSRect)displayRect inContext:(NSGraphicsContext *)graphicsContext shouldChangeFontReferenceColor:(BOOL)shouldChangeFontReferenceColor stopAtLayerBackedViews:(BOOL)stopAtLayerBackedViews;
#elif __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
- (void)_recursive:(BOOL)recurse displayRectIgnoringOpacity:(NSRect)displayRect inContext:(NSGraphicsContext *)context shouldChangeFontReferenceColor:(BOOL)shouldChangeFontReferenceColor;
- (void)_recursive:(BOOL)recurseX displayRectIgnoringOpacity:(NSRect)displayRect inGraphicsContext:(NSGraphicsContext *)graphicsContext shouldChangeFontReferenceColor:(BOOL)shouldChangeFontReferenceColor;
#else
- (void)_recursive:(BOOL)recurse displayRectIgnoringOpacity:(NSRect)displayRect inContext:(NSGraphicsContext *)context topView:(BOOL)topView;
- (void)_recursive:(BOOL)recurseX displayRectIgnoringOpacity:(NSRect)displayRect inGraphicsContext:(NSGraphicsContext *)graphicsContext CGContext:(CGContextRef)ctx topView:(BOOL)isTopView shouldChangeFontReferenceColor:(BOOL)shouldChangeFontReferenceColor;
#endif
#endif
- (void)_setDrawsOwnDescendants:(BOOL)drawsOwnDescendants;
#if PLATFORM(IOS_FAMILY)
- (void)centerSelectionInVisibleArea:(id)sender;
#endif
@end

#if PLATFORM(MAC)

@interface NSView (WebSetNeedsDisplayInRect)
- (void)_web_setNeedsDisplayInRect:(NSRect)invalidRect;
@end

@implementation NSView (WebSetNeedsDisplayInRect)

- (void)_web_setNeedsDisplayInRect:(NSRect)invalidRect
{
    // Note that we call method_exchangeImplementations below, so any calls
    // to _web_setNeedsDisplayInRect: will actually call -[NSView setNeedsDisplayInRect:].

    if (![NSThread isMainThread] || ![self _drawnByAncestor]) {
        [self _web_setNeedsDisplayInRect:invalidRect];
        return;
    }

    static Class webFrameViewClass = [WebFrameView class];
    WebFrameView *enclosingWebFrameView = (WebFrameView *)self;
    while (enclosingWebFrameView && ![enclosingWebFrameView isKindOfClass:webFrameViewClass])
        enclosingWebFrameView = (WebFrameView *)[enclosingWebFrameView superview];

    if (!enclosingWebFrameView) {
        [self _web_setNeedsDisplayInRect:invalidRect];
        return;
    }

    Frame* coreFrame = core([enclosingWebFrameView webFrame]);
    FrameView* frameView = coreFrame ? coreFrame->view() : 0;
    if (!frameView || !frameView->isEnclosedInCompositingLayer()) {
        [self _web_setNeedsDisplayInRect:invalidRect];
        return;
    }

    NSRect invalidRectInWebFrameViewCoordinates = [enclosingWebFrameView convertRect:invalidRect fromView:self];
    IntRect invalidRectInFrameViewCoordinates(invalidRectInWebFrameViewCoordinates);
    if (![enclosingWebFrameView isFlipped])
        invalidRectInFrameViewCoordinates.setY(frameView->frameRect().size().height() - invalidRectInFrameViewCoordinates.maxY());

    frameView->invalidateRect(invalidRectInFrameViewCoordinates);
}

@end

#endif // PLATFORM(MAC)

const float _WebHTMLViewPrintingMinimumShrinkFactor = PrintContext::minimumShrinkFactor();
const float _WebHTMLViewPrintingMaximumShrinkFactor = PrintContext::maximumShrinkFactor();

// Any non-zero value will do, but using something recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

// FIXME: From AppKit's _NXSmartPaste constant. Get with an SPI header instead?
#define WebSmartPastePboardType @"NeXT smart paste pasteboard type"

#define STANDARD_WEIGHT 5
#define MIN_BOLD_WEIGHT 7
#define STANDARD_BOLD_WEIGHT 9

#if PLATFORM(MAC)

// <rdar://problem/4985524> References to WebCoreScrollView as a subview of a WebHTMLView may be present
// in some NIB files, so NSUnarchiver must be still able to look up this now-unused class.
@interface WebCoreScrollView : NSScrollView
@end

@implementation WebCoreScrollView
@end

// We need this to be able to safely reference the CachedImage for the promised drag data
static CachedImageClient& promisedDataClient()
{
    static NeverDestroyed<CachedImageClient> staticCachedResourceClient;
    return staticCachedResourceClient.get();
}

#endif

#if PLATFORM(IOS_FAMILY)
static NSString * const WebMarkedTextUpdatedNotification = @"WebMarkedTextUpdated";

static void hardwareKeyboardAvailabilityChangedCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    ASSERT(observer);
    WebThreadRun(^{
        WebHTMLView *webView = (__bridge WebHTMLView *)observer;
        if (Frame* coreFrame = core([webView _frame]))
            coreFrame->eventHandler().capsLockStateMayHaveChanged();
    });
}
#endif

@interface WebHTMLView (WebHTMLViewFileInternal)
#if PLATFORM(MAC)
- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard inContext:(DOMRange *)context allowPlainText:(BOOL)allowPlainText;
- (NSString *)_plainTextFromPasteboard:(NSPasteboard *)pasteboard;
- (void)_pasteWithPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText;
- (void)_pasteAsPlainTextWithPasteboard:(NSPasteboard *)pasteboard;
- (void)_postFakeMouseMovedEventForFlagsChangedEvent:(NSEvent *)flagsChangedEvent;
- (void)_removeSuperviewObservers;
- (void)_removeWindowObservers;
#endif
- (BOOL)_shouldInsertFragment:(DOMDocumentFragment *)fragment replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action;
- (BOOL)_shouldInsertText:(NSString *)text replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action;
- (BOOL)_shouldReplaceSelectionWithText:(NSString *)text givenAction:(WebViewInsertAction)action;
- (DOMRange *)_selectedRange;
#if PLATFORM(MAC)
- (NSView *)_hitViewForEvent:(NSEvent *)event;
- (void)_writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard cachedAttributedString:(NSAttributedString *)attributedString;
#endif
- (DOMRange *)_documentRange;
- (void)_setMouseDownEvent:(WebEvent *)event;
- (WebHTMLView *)_topHTMLView;
- (BOOL)_isTopHTMLView;
#if PLATFORM(MAC)
- (void)_web_setPrintingModeRecursive;
- (void)_web_setPrintingModeRecursiveAndAdjustViewSize;
- (void)_web_clearPrintingModeRecursive;
#endif
#if ENABLE(NETSCAPE_PLUGIN_API)
- (void)_web_makePluginSubviewsPerformSelector:(SEL)selector withObject:(id)object;
#endif
@end

#if PLATFORM(MAC)

@interface WebHTMLView (WebHTMLViewTextCheckingInternal)
- (void)orderFrontSubstitutionsPanel:(id)sender;
- (BOOL)smartInsertDeleteEnabled;
- (void)setSmartInsertDeleteEnabled:(BOOL)flag;
- (void)toggleSmartInsertDelete:(id)sender;
- (BOOL)isAutomaticQuoteSubstitutionEnabled;
- (void)setAutomaticQuoteSubstitutionEnabled:(BOOL)flag;
- (void)toggleAutomaticQuoteSubstitution:(id)sender;
- (BOOL)isAutomaticLinkDetectionEnabled;
- (void)setAutomaticLinkDetectionEnabled:(BOOL)flag;
- (void)toggleAutomaticLinkDetection:(id)sender;
- (BOOL)isAutomaticDashSubstitutionEnabled;
- (void)setAutomaticDashSubstitutionEnabled:(BOOL)flag;
- (void)toggleAutomaticDashSubstitution:(id)sender;
- (BOOL)isAutomaticTextReplacementEnabled;
- (void)setAutomaticTextReplacementEnabled:(BOOL)flag;
- (void)toggleAutomaticTextReplacement:(id)sender;
- (BOOL)isAutomaticSpellingCorrectionEnabled;
- (void)setAutomaticSpellingCorrectionEnabled:(BOOL)flag;
- (void)toggleAutomaticSpellingCorrection:(id)sender;
@end

#endif

@interface WebHTMLView (WebForwardDeclaration) // FIXME: Put this in the WebFileInternal category instead of doing the forward declaration trick.
- (void)_setPrinting:(BOOL)printing minimumPageLogicalWidth:(float)minPageWidth logicalHeight:(float)minPageHeight originalPageWidth:(float)pageLogicalWidth originalPageHeight:(float)pageLogicalHeight maximumShrinkRatio:(float)maximumShrinkRatio adjustViewSize:(BOOL)adjustViewSize paginateScreenContent:(BOOL)paginateScreenContent;
@end

#if PLATFORM(MAC)
@interface WebHTMLView (WebNSTextInputSupport) <NSTextInput>
#else
@interface WebHTMLView (WebNSTextInputSupport)
#endif
#if PLATFORM(MAC)
- (void)_updateSecureInputState;
- (void)_updateSelectionForInputManager;
#endif
#if PLATFORM(IOS_FAMILY)
- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange;
- (void)doCommandBySelector:(SEL)selector;
#endif
@end

@interface NSView (WebHTMLViewFileInternal)
- (void)_web_addDescendentWebHTMLViewsToArray:(NSMutableArray *) array;
@end

struct WebHTMLViewInterpretKeyEventsParameters {
    KeyboardEvent* event;
    bool eventInterpretationHadSideEffects;
    bool shouldSaveCommands;
    bool consumedByIM;
    bool executingSavedKeypressCommands;
};

@interface WebHTMLViewPrivate : NSObject {
@public
    BOOL closed;
    BOOL ignoringMouseDraggedEvents;
    BOOL printing;
    BOOL paginateScreenContent;

#if PLATFORM(MAC)
    BOOL observingSuperviewNotifications;
    BOOL observingWindowNotifications;

    id savedSubviews;
    BOOL subviewsSetAside;
#endif

    NSView *layerHostingView;

#if PLATFORM(MAC)
    BOOL drawingIntoLayer;
    BOOL drawingIntoAcceleratedLayer;
#endif

    RetainPtr<WebEvent> mouseDownEvent; // Kept after handling the event.
    BOOL handlingMouseDownEvent;
    RetainPtr<WebEvent> keyDownEvent; // Kept after handling the event.

    // A WebHTMLView has a single input context, but we return nil when in non-editable content to avoid making input methods do their work.
    // This state is saved each time selection changes, because computing it causes style recalc, which is not always safe to do.
    BOOL exposeInputContext;

#if PLATFORM(MAC)
    // Track whether the view has set a secure input state.
    BOOL isInSecureInputState;

    BOOL _forceUpdateSecureInputState;
#endif

    NSPoint lastScrollPosition;
    BOOL inScrollPositionChanged;

    RetainPtr<WebPluginController> pluginController;
    
#if PLATFORM(MAC)
    RetainPtr<NSString> toolTip;
    NSToolTipTag lastToolTipTag;

    id trackingRectOwner;
    void* trackingRectUserData;
    
    RetainPtr<NSTimer> autoscrollTimer;
    RetainPtr<NSEvent> autoscrollTriggerEvent;
#endif

    RetainPtr<NSArray> pageRects;

#if PLATFORM(MAC)
    RetainPtr<WebTextCompletionController> completionController;

    BOOL transparentBackground;
#endif

    WebHTMLViewInterpretKeyEventsParameters* interpretKeyEventsParameters;
    
    RetainPtr<WebDataSource> dataSource;

#if PLATFORM(MAC)
    WebCore::CachedImage* promisedDragTIFFDataSource;
#endif

    SEL selectorForDoCommandBySelector;

#if PLATFORM(MAC)
    BOOL installedTrackingArea;
    id flagsChangedEventMonitor;
    NSRange softSpaceRange;
#endif

#if ENABLE(SERVICE_CONTROLS)
    RetainPtr<WebSharingServicePickerController> currentSharingServicePickerController;
#endif
}
- (void)clear;
@end

#if PLATFORM(MAC)

static NSControlStateValue kit(TriState state)
{
    switch (state) {
        case FalseTriState:
            return NSControlStateValueOff;
        case TrueTriState:
            return NSControlStateValueOn;
        case MixedTriState:
            return NSControlStateValueMixed;
    }
    ASSERT_NOT_REACHED();
    return NSControlStateValueOff;
}

#endif

@implementation WebHTMLViewPrivate

#if PLATFORM(MAC)

+ (void)initialize
{
    // FIXME: Shouldn't all of this move into +[WebHTMLView initialize]?
    // And some of this work is likely redundant since +[WebHTMLView initialize] is guaranteed to run first.

    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
    RunLoop::initializeMainRunLoop();

    if (!oldSetCursorForMouseLocationIMP) {
        Method setCursorMethod = class_getInstanceMethod([NSWindow class], @selector(_setCursorForMouseLocation:));
        ASSERT(setCursorMethod);
        oldSetCursorForMouseLocationIMP = method_setImplementation(setCursorMethod, (IMP)setCursor);
        ASSERT(oldSetCursorForMouseLocationIMP);
    }

    method_exchangeImplementations(class_getInstanceMethod([NSView class], @selector(setNeedsDisplayInRect:)), class_getInstanceMethod([NSView class], @selector(_web_setNeedsDisplayInRect:)));
}

#endif

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebHTMLViewPrivate class], self))
        return;

#if PLATFORM(MAC)
    ASSERT(!autoscrollTimer);
    ASSERT(!autoscrollTriggerEvent);
#endif

#if PLATFORM(MAC)
    if (promisedDragTIFFDataSource)
        promisedDragTIFFDataSource->removeClient(promisedDataClient());

    if (flagsChangedEventMonitor) {
        [NSEvent removeMonitor:flagsChangedEventMonitor];
        flagsChangedEventMonitor = nil;
    }
#endif

    [super dealloc];
}

- (void)clear
{
#if PLATFORM(MAC)
    if (promisedDragTIFFDataSource)
        promisedDragTIFFDataSource->removeClient(promisedDataClient());
#endif

    mouseDownEvent = nil;
    keyDownEvent = nil;
    pluginController = nil;
#if PLATFORM(MAC)
    toolTip = nil;
    completionController = nil;
#endif
    dataSource = nil;
#if PLATFORM(MAC)
    promisedDragTIFFDataSource = nullptr;
#endif

    layerHostingView = nil;
}

@end

@implementation WebHTMLView (WebHTMLViewFileInternal)

- (DOMRange *)_documentRange
{
    return [[[self _frame] DOMDocument] _documentRange];
}

- (WebDataSource *)_dataSource
{
    return _private->dataSource.get();
}

- (WebView *)_webView
{
    return [_private->dataSource _webView];
}

- (WebFrameView *)_frameView
{
    return [[_private->dataSource webFrame] frameView];
}

#if PLATFORM(MAC)

- (DOMDocumentFragment *)_documentFragmentWithPaths:(NSArray *)paths
{
    auto textNodes = adoptNS([[NSMutableArray alloc] init]);

    for (NSString *path in paths) {
        // Non-image file types; _web_userVisibleString is appropriate here because this will
        // be pasted as visible text.
        NSString *url = [[[NSURL fileURLWithPath:path] _webkit_canonicalize] _web_userVisibleString];
        [textNodes addObject:[[[self _frame] DOMDocument] createTextNode:url]];
    }

    DOMDocumentFragment *fragment = [[self _frame] _documentFragmentWithNodesAsParagraphs:textNodes.get()];
    return [fragment firstChild] != nil ? fragment : nil;
}

+ (NSArray *)_excludedElementsForAttributedStringConversion
{
    NSMutableArray *elements = [[NSMutableArray alloc] initWithObjects:
        // Omit style since we want style to be inline so the fragment can be easily inserted.
        @"style",
        // Omit xml so the result is not XHTML.
        @"xml",
        // Omit tags that will get stripped when converted to a fragment anyway.
        @"doctype", @"html", @"head", @"body",
        // Omit deprecated tags.
        @"applet", @"basefont", @"center", @"dir", @"font", @"menu", @"s", @"strike", @"u",
        // Omit object so no file attachments are part of the fragment.
#if !ENABLE(ATTACHMENT_ELEMENT)
        // Omit object so no file attachments are part of the fragment.
        @"object",
#endif
        nil];

#if ENABLE(ATTACHMENT_ELEMENT)
    if (!RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled())
        [elements addObject:@"object"];
#endif

    return [elements autorelease];
}

- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard inContext:(DOMRange *)context allowPlainText:(BOOL)allowPlainText
{
    NSArray *types = [pasteboard types];
    DOMDocumentFragment *fragment = nil;

    if ([types containsObject:WebArchivePboardType] && (fragment = [self _documentFragmentFromPasteboard:pasteboard forType:WebArchivePboardType inContext:context subresources:0]))
        return fragment;

    if ([types containsObject:legacyFilenamesPasteboardType()] && (fragment = [self _documentFragmentFromPasteboard:pasteboard forType:legacyFilenamesPasteboardType() inContext:context subresources:0]))
        return fragment;
    
    if ([types containsObject:legacyHTMLPasteboardType()] && (fragment = [self _documentFragmentFromPasteboard:pasteboard forType:legacyHTMLPasteboardType() inContext:context subresources:0]))
        return fragment;
    
    if ([types containsObject:legacyRTFDPasteboardType()] && (fragment = [self _documentFragmentFromPasteboard:pasteboard forType:legacyRTFDPasteboardType() inContext:context subresources:0]))
        return fragment;
    
    if ([types containsObject:legacyRTFPasteboardType()] && (fragment = [self _documentFragmentFromPasteboard:pasteboard forType:legacyRTFPasteboardType() inContext:context subresources:0]))
        return fragment;

    if ([types containsObject:legacyTIFFPasteboardType()] && (fragment = [self _documentFragmentFromPasteboard:pasteboard forType:legacyTIFFPasteboardType() inContext:context subresources:0]))
        return fragment;

    if ([types containsObject:legacyPDFPasteboardType()] && (fragment = [self _documentFragmentFromPasteboard:pasteboard forType:legacyPDFPasteboardType() inContext:context subresources:0]))
        return fragment;

    if ([types containsObject:(NSString *)kUTTypePNG] && (fragment = [self _documentFragmentFromPasteboard:pasteboard forType:(NSString *)kUTTypePNG inContext:context subresources:0]))
        return fragment;

    if ([types containsObject:legacyURLPasteboardType()] && (fragment = [self _documentFragmentFromPasteboard:pasteboard forType:legacyURLPasteboardType() inContext:context subresources:0]))
        return fragment;

    if (allowPlainText && [types containsObject:legacyStringPasteboardType()] && (fragment = [self _documentFragmentFromPasteboard:pasteboard forType:legacyStringPasteboardType() inContext:context subresources:0]))
        return fragment;
    
    return nil;
}

- (NSString *)_plainTextFromPasteboard:(NSPasteboard *)pasteboard
{
    NSArray *types = [pasteboard types];
    
    if ([types containsObject:legacyStringPasteboardType()])
        return [[pasteboard stringForType:legacyStringPasteboardType()] precomposedStringWithCanonicalMapping];

    RetainPtr<NSAttributedString> attributedString;
    if ([types containsObject:legacyRTFDPasteboardType()])
        attributedString = adoptNS([[NSAttributedString alloc] initWithRTFD:[pasteboard dataForType:legacyRTFDPasteboardType()] documentAttributes:NULL]);
    if (attributedString == nil && [types containsObject:legacyRTFPasteboardType()])
        attributedString = adoptNS([[NSAttributedString alloc] initWithRTF:[pasteboard dataForType:legacyRTFPasteboardType()] documentAttributes:NULL]);
    if (attributedString)
        return [[[attributedString string] copy] autorelease];

    if ([types containsObject:legacyFilenamesPasteboardType()]) {
        if (NSString *string = [[pasteboard propertyListForType:legacyFilenamesPasteboardType()] componentsJoinedByString:@"\n"])
            return string;
    }

    if (NSURL *URL = [NSURL URLFromPasteboard:pasteboard]) {
        NSString *string = [URL _web_userVisibleString];
        if ([string length])
            return string;
    }

    return nil;
}

- (void)_pasteWithPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText
{
    auto webView = retainPtr([self _webView]);
    [webView _setInsertionPasteboard:pasteboard];

    DOMRange *range = [self _selectedRange];
    Frame* coreFrame = core([self _frame]);

    DOMDocumentFragment *fragment = [self _documentFragmentFromPasteboard:pasteboard inContext:range allowPlainText:allowPlainText];
    if (fragment && [self _shouldInsertFragment:fragment replacingDOMRange:range givenAction:WebViewInsertActionPasted])
        coreFrame->editor().pasteAsFragment(*core(fragment), [self _canSmartReplaceWithPasteboard:pasteboard], false);

    [webView _setInsertionPasteboard:nil];
}

- (void)_pasteAsPlainTextWithPasteboard:(NSPasteboard *)pasteboard 
{ 
    auto webView = retainPtr([self _webView]);
    [webView _setInsertionPasteboard:pasteboard];

    NSString *text = [self _plainTextFromPasteboard:pasteboard]; 
    if ([self _shouldReplaceSelectionWithText:text givenAction:WebViewInsertActionPasted]) 
        [[self _frame] _replaceSelectionWithText:text selectReplacement:NO smartReplace:[self _canSmartReplaceWithPasteboard:pasteboard]]; 

    [webView _setInsertionPasteboard:nil]; 
}

- (void)_postFakeMouseMovedEventForFlagsChangedEvent:(NSEvent *)flagsChangedEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSEventTypeMouseMoved location:flagsChangedEvent.window.mouseLocationOutsideOfEventStream
        modifierFlags:flagsChangedEvent.modifierFlags timestamp:flagsChangedEvent.timestamp windowNumber:flagsChangedEvent.windowNumber
        context:nullptr eventNumber:0 clickCount:0 pressure:0];
    [self mouseMoved:fakeEvent];
}

// This method is needed to support macOS services.
- (BOOL)readSelectionFromPasteboard:(NSPasteboard *)pasteboard 
{ 
    Frame* coreFrame = core([self _frame]); 
    if (!coreFrame) 
        return NO; 
    if (coreFrame->selection().selection().isContentRichlyEditable())
        [self _pasteWithPasteboard:pasteboard allowPlainText:YES]; 
    else 
        [self _pasteAsPlainTextWithPasteboard:pasteboard]; 
    return YES; 
}

- (void)_removeSuperviewObservers
{
    if (!_private || !_private->observingSuperviewNotifications)
        return;

    NSView *superview = [self superview];
    if (!superview || ![self window])
        return;

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:NSViewFrameDidChangeNotification object:superview];
    [notificationCenter removeObserver:self name:NSViewBoundsDidChangeNotification object:superview];

    _private->observingSuperviewNotifications = false;
}

- (void)_removeWindowObservers
{
    if (!_private->observingWindowNotifications)
        return;

    NSWindow *window = [self window];
    if (!window)
        return;

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:NSWindowDidBecomeKeyNotification object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidResignKeyNotification object:nil];
    [notificationCenter removeObserver:self name:NSWindowWillCloseNotification object:window];

    _private->observingWindowNotifications = false;
}

#endif // PLATFORM(MAC)

- (BOOL)_shouldInsertFragment:(DOMDocumentFragment *)fragment replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    WebView *webView = [self _webView];
    DOMNode *child = [fragment firstChild];
    if ([fragment lastChild] == child && [child isKindOfClass:[DOMCharacterData class]])
        return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:[(DOMCharacterData *)child data] replacingDOMRange:range givenAction:action];
    return [[webView _editingDelegateForwarder] webView:webView shouldInsertNode:fragment replacingDOMRange:range givenAction:action];
}

- (BOOL)_shouldInsertText:(NSString *)text replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    WebView *webView = [self _webView];
    return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:text replacingDOMRange:range givenAction:action];
}

- (BOOL)_shouldReplaceSelectionWithText:(NSString *)text givenAction:(WebViewInsertAction)action
{
    return [self _shouldInsertText:text replacingDOMRange:[self _selectedRange] givenAction:action];
}

- (DOMRange *)_selectedRange
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->selection().toNormalizedRange().get()) : nil;
}

#if PLATFORM(MAC)

- (NSView *)_hitViewForEvent:(NSEvent *)event
{
    // Usually, we hack AK's hitTest method to catch all events at the topmost WebHTMLView.  
    // Callers of this method, however, want to query the deepest view instead.
    forceNSViewHitTest = YES;
    NSView *hitView = [(NSView *)[[self window] contentView] hitTest:[event locationInWindow]];
    forceNSViewHitTest = NO;    
    return hitView;
}

- (void)_writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard cachedAttributedString:(NSAttributedString *)attributedString
{
    // Put HTML on the pasteboard.
    if ([types containsObject:WebArchivePboardType]) {
        if (auto coreArchive = LegacyWebArchive::createFromSelection(core([self _frame]))) {
            if (RetainPtr<CFDataRef> data = coreArchive ? coreArchive->rawDataRepresentation() : 0)
                [pasteboard setData:(__bridge NSData *)data.get() forType:WebArchivePboardType];
        }
    }

    // Put the attributed string on the pasteboard (RTF/RTFD format).
    if ([types containsObject:legacyRTFDPasteboardType()]) {
        if (attributedString == nil) {
            attributedString = [self selectedAttributedString];
        }        
        NSData *RTFDData = [attributedString RTFDFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:@{ }];
        [pasteboard setData:RTFDData forType:legacyRTFDPasteboardType()];
    }        
    if ([types containsObject:legacyRTFPasteboardType()]) {
        if (!attributedString)
            attributedString = [self selectedAttributedString];
        if ([attributedString containsAttachments])
            attributedString = attributedStringByStrippingAttachmentCharacters(attributedString);
        NSData *RTFData = [attributedString RTFFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:@{ }];
        [pasteboard setData:RTFData forType:legacyRTFPasteboardType()];
    }

    // Put plain string on the pasteboard.
    if ([types containsObject:legacyStringPasteboardType()]) {
        // Map &nbsp; to a plain old space because this is better for source code, other browsers do it, and
        // because HTML forces content creators and editors to use this character any time they want two spaces in a row.
        [pasteboard setString:[[self selectedString] stringByReplacingOccurrencesOfString:@"\u00A0" withString:@" "] forType:legacyStringPasteboardType()];
    }

    if ([self _canSmartCopyOrDelete] && [types containsObject:WebSmartPastePboardType])
        [pasteboard setData:nil forType:WebSmartPastePboardType];
}

#endif // PLATFORM(MAC)

- (void)_setMouseDownEvent:(WebEvent *)event
{
#if PLATFORM(MAC)
    ASSERT(!event || [event type] == NSEventTypeLeftMouseDown || [event type] == NSEventTypeRightMouseDown || [event type] == NSEventTypeOtherMouseDown);
#else
    ASSERT(!event || event.type == WebEventMouseDown);
#endif

    _private->mouseDownEvent = event;
}

- (WebHTMLView *)_topHTMLView
{
    // FIXME: this can fail if the dataSource is nil, which happens when the WebView is tearing down from the window closing.
    WebHTMLView *view = (WebHTMLView *)[[[[_private->dataSource _webView] mainFrame] frameView] documentView];
    ASSERT(!view || [view isKindOfClass:[WebHTMLView class]]);
    return view;
}

- (BOOL)_isTopHTMLView
{
    // FIXME: this should be a cached boolean that doesn't rely on _topHTMLView since that can fail (see _topHTMLView).
    return self == [self _topHTMLView];
}

#if PLATFORM(MAC)

- (void)_web_setPrintingModeRecursive:(BOOL)printing adjustViewSize:(BOOL)adjustViewSize
{
    auto array = adoptNS([[NSMutableArray alloc] initWithObjects:self, nil]);
    [self _web_addDescendentWebHTMLViewsToArray:array.get()];
    for (WebHTMLView *view in array.get())
        [view _setPrinting:printing minimumPageLogicalWidth:0 logicalHeight:0 originalPageWidth:0 originalPageHeight:0 maximumShrinkRatio:0 adjustViewSize:adjustViewSize paginateScreenContent:[self _isInScreenPaginationMode]];
}

- (void)_web_setPrintingModeRecursive
{
    [self _web_setPrintingModeRecursive:YES adjustViewSize:NO];
}

- (void)_web_clearPrintingModeRecursive
{
    [self _web_setPrintingModeRecursive:NO adjustViewSize:NO];
}

- (void)_web_setPrintingModeRecursiveAndAdjustViewSize
{
    [self _web_setPrintingModeRecursive:YES adjustViewSize:YES];
}

#endif // PLATFORM(MAC)

#if ENABLE(NETSCAPE_PLUGIN_API)

- (void)_web_makePluginSubviewsPerformSelector:(SEL)selector withObject:(id)object
{
    // Copy subviews because [self subviews] returns the view's mutable internal array,
    // and we must avoid mutating the array while enumerating it.
    auto subviewsCopy = adoptNS([self.subviews copy]);
    for (NSView *view in subviewsCopy.get()) {
        if ([view isKindOfClass:[WebBaseNetscapePluginView class]])
            [view performSelector:selector withObject:object];
    }
}

#endif

@end

@implementation WebHTMLView (WebPrivate)

+ (NSArray *)supportedMIMETypes
{
    return [WebHTMLRepresentation supportedMIMETypes];
}

+ (NSArray *)supportedMediaMIMETypes
{
    return [WebHTMLRepresentation supportedMediaMIMETypes];
}

+ (NSArray *)supportedImageMIMETypes
{
    return [WebHTMLRepresentation supportedImageMIMETypes];
}

+ (NSArray *)supportedNonImageMIMETypes
{
    return [WebHTMLRepresentation supportedNonImageMIMETypes];
}

+ (NSArray *)unsupportedTextMIMETypes
{
    return [WebHTMLRepresentation unsupportedTextMIMETypes];
}

#if PLATFORM(IOS_FAMILY)

- (void)mouseMoved:(WebEvent *)event
{
    if (auto* frame = core([self _frame]))
        frame->eventHandler().mouseMoved(event);
}

#endif

#if PLATFORM(MAC)

+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent
{
    // This is obsolete SPI needed only for older versions of Safari
}

- (id)_bridge
{
    // This method exists to maintain compatibility with Leopard's Dictionary.app, since it
    // calls _bridge to get access to convertNSRangeToDOMRange: and convertDOMRangeToNSRange:.
    // Return the WebFrame, which implements the compatibility methods. <rdar://problem/6002160>
    return [self _frame];
}

- (void)_updateMouseoverWithFakeEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
        location:[[self window]
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        convertScreenToBase:[NSEvent mouseLocation]]
ALLOW_DEPRECATED_DECLARATIONS_END
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:nullptr
        eventNumber:0 clickCount:0 pressure:0];

    [self _updateMouseoverWithEvent:fakeEvent];
}

#endif // PLATFORM(MAC)

- (void)_frameOrBoundsChanged
{
    WebView *webView = [self _webView];
    WebDynamicScrollBarsView *scrollView = [[[webView mainFrame] frameView] _scrollView];

    NSPoint origin = [[self superview] bounds].origin;
    if (!NSEqualPoints(_private->lastScrollPosition, origin) && ![scrollView inProgrammaticScroll]) {
        if (Frame* coreFrame = core([self _frame])) {
            if (FrameView* coreView = coreFrame->view()) {
                _private->inScrollPositionChanged = YES;
                coreView->scrollOffsetChangedViaPlatformWidget(IntPoint(_private->lastScrollPosition), IntPoint(origin));
                _private->inScrollPositionChanged = NO;
            }
        }
    
#if PLATFORM(MAC)
        [_private->completionController endRevertingChange:NO moveLeft:NO];
#endif
        
        [webView _didScrollDocumentInFrameView:[self _frameView]];
    }
    _private->lastScrollPosition = origin;
}

- (void)_setAsideSubviews
{
#if PLATFORM(MAC)
    ASSERT(!_private->subviewsSetAside);
    ASSERT(_private->savedSubviews == nil);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    _private->savedSubviews = _subviews;
    ALLOW_DEPRECATED_DECLARATIONS_END
    // We need to keep the layer-hosting view in the subviews, otherwise the layers flash.
    if (_private->layerHostingView) {
        NSArray* newSubviews = [[NSArray alloc] initWithObjects:_private->layerHostingView, nil];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        _subviews = newSubviews;
    } else
        _subviews = nil;
    ALLOW_DEPRECATED_DECLARATIONS_END
    _private->subviewsSetAside = YES;
#endif
 }
 
 - (void)_restoreSubviews
 {
#if PLATFORM(MAC)
    ASSERT(_private->subviewsSetAside);
    if (_private->layerHostingView) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [_subviews release];
        _subviews = _private->savedSubviews;
    } else {
        ASSERT(_subviews == nil);
        _subviews = _private->savedSubviews;
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
    _private->savedSubviews = nil;
    _private->subviewsSetAside = NO;
#endif
}

- (void)viewWillDraw
{
    // On window close we will be called when the data source is nil, then hit an assert in _topHTMLView
    // So check if the data source is nil before calling [self _isTopHTMLView], this can be removed
    // once the FIXME in _isTopHTMLView is fixed.
    if (_private->dataSource && [self _isTopHTMLView]) {
        [self _web_updateLayoutAndStyleIfNeededRecursive];
        [[self _webView] _flushCompositingChanges];
    }

    [super viewWillDraw];
}

#if PLATFORM(MAC)

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];
    if (isPrinting) {
        if (!wasInPrintingMode)
            [self _web_setPrintingModeRecursive];
        else
            [self _web_updateLayoutAndStyleIfNeededRecursive];
    } else if (wasInPrintingMode)
        [self _web_clearPrintingModeRecursive];

    // There are known cases where -viewWillDraw is not called on all views being drawn.
    // See <rdar://problem/6964278> for example. Performing layout at this point prevents us from
    // trying to paint without layout (which WebCore now refuses to do, instead bailing out without
    // drawing at all), but we may still fail to update any regions dirtied by the layout which are
    // not already dirty. 
    if ([self _needsLayout]) {
        NSInteger rectCount;
        [self getRectsBeingDrawn:0 count:&rectCount];
        if (rectCount) {
            LOG_ERROR("View needs layout. Either -viewWillDraw wasn't called or layout was invalidated during the display operation. Performing layout now.");
            [self _web_updateLayoutAndStyleIfNeededRecursive];
        }
    }

    [self _setAsideSubviews];
    [super _recursiveDisplayRectIfNeededIgnoringOpacity:rect isVisibleRect:isVisibleRect rectIsVisibleRectForView:visibleView topView:topView];
    [self _restoreSubviews];

    if (wasInPrintingMode != isPrinting) {
        if (wasInPrintingMode)
            [self _web_setPrintingModeRecursive];
        else
            [self _web_clearPrintingModeRecursive];
    }
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect
{
    BOOL needToSetAsideSubviews = !_private->subviewsSetAside;

    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];

    if (needToSetAsideSubviews) {
        // This helps when we print as part of a larger print process.
        // If the WebHTMLView itself is what we're printing, then we will never have to do this.
        if (isPrinting) {
            if (!wasInPrintingMode)
                [self _web_setPrintingModeRecursive];
            else
                [self _web_updateLayoutAndStyleIfNeededRecursive];
        } else if (wasInPrintingMode)
            [self _web_clearPrintingModeRecursive];


        [self _setAsideSubviews];
    }

    [super _recursiveDisplayAllDirtyWithLockFocus:needsLockFocus visRect:visRect];

    if (needToSetAsideSubviews) {
        if (wasInPrintingMode != isPrinting) {
            if (wasInPrintingMode)
                [self _web_setPrintingModeRecursive];
            else
                [self _web_clearPrintingModeRecursive];
        }

        [self _restoreSubviews];
    }
}

// Don't let AppKit even draw subviews. We take care of that.
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
- (void)_recursive:(BOOL)recursive displayRectIgnoringOpacity:(NSRect)displayRect inContext:(NSGraphicsContext *)graphicsContext shouldChangeFontReferenceColor:(BOOL)shouldChangeFontReferenceColor stopAtLayerBackedViews:(BOOL)stopAtLayerBackedViews
#elif __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
- (void)_recursive:(BOOL)recurse displayRectIgnoringOpacity:(NSRect)displayRect inContext:(NSGraphicsContext *)context shouldChangeFontReferenceColor:(BOOL)shouldChangeFontReferenceColor
#else
- (void)_recursive:(BOOL)recurse displayRectIgnoringOpacity:(NSRect)displayRect inContext:(NSGraphicsContext *)context topView:(BOOL)topView
#endif
{
    [self _setAsideSubviews];
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    [super _recursive:recursive displayRectIgnoringOpacity:displayRect inContext:graphicsContext shouldChangeFontReferenceColor:shouldChangeFontReferenceColor stopAtLayerBackedViews:stopAtLayerBackedViews];
#elif __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    [super _recursive:recurse displayRectIgnoringOpacity:displayRect inContext:context shouldChangeFontReferenceColor:shouldChangeFontReferenceColor];
#else
    [super _recursive:recurse displayRectIgnoringOpacity:displayRect inContext:context topView:topView];
#endif
    [self _restoreSubviews];
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101400
// Don't let AppKit even draw subviews. We take care of that.
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
- (void)_recursive:(BOOL)recurseX displayRectIgnoringOpacity:(NSRect)displayRect inGraphicsContext:(NSGraphicsContext *)graphicsContext shouldChangeFontReferenceColor:(BOOL)shouldChangeFontReferenceColor
#else
- (void)_recursive:(BOOL)recurseX displayRectIgnoringOpacity:(NSRect)displayRect inGraphicsContext:(NSGraphicsContext *)graphicsContext CGContext:(CGContextRef)ctx topView:(BOOL)isTopView shouldChangeFontReferenceColor:(BOOL)shouldChangeFontReferenceColor
#endif
{
    BOOL didSetAsideSubviews = NO;

    if (!_private->subviewsSetAside) {
        [self _setAsideSubviews];
        didSetAsideSubviews = YES;
    }

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    [super _recursive:recurseX displayRectIgnoringOpacity:displayRect inGraphicsContext:graphicsContext shouldChangeFontReferenceColor:shouldChangeFontReferenceColor];
#else
    [super _recursive:recurseX displayRectIgnoringOpacity:displayRect inGraphicsContext:graphicsContext CGContext:ctx topView:isTopView shouldChangeFontReferenceColor:shouldChangeFontReferenceColor];
#endif

    if (didSetAsideSubviews)
        [self _restoreSubviews];
}
#endif

static BOOL isQuickLookEvent(NSEvent *event)
{
    const int kCGSEventSystemSubtypeHotKeyCombinationReleased = 9;
    return [event type] == NSEventTypeSystemDefined && [event subtype] == kCGSEventSystemSubtypeHotKeyCombinationReleased && [event data1] == 'lkup';
}

#endif

- (NSView *)hitTest:(NSPoint)point
{
    // WebHTMLView objects handle all events for objects inside them.
    // To get those events, we prevent hit testing from AppKit.

    // But there are three exceptions to this:
    //   1) For right mouse clicks and control clicks we don't yet have an implementation
    //      that works for nested views, so we let the hit testing go through the
    //      standard NSView code path (needs to be fixed, see bug 4361618).
    //   2) Java depends on doing a hit test inside it's mouse moved handling,
    //      so we let the hit testing go through the standard NSView code path
    //      when the current event is a mouse move (except when we are calling
    //      from _updateMouseoverWithEvent, so we have to use a global,
    //      forceWebHTMLViewHitTest, for that)
    //   3) The acceptsFirstMouse: and shouldDelayWindowOrderingForEvent: methods
    //      both need to figure out which view to check with inside the WebHTMLView.
    //      They use a global to change the behavior of hitTest: so they can get the
    //      right view. The global is forceNSViewHitTest and the method they use to
    //      do the hit testing is _hitViewForEvent:. (But this does not work correctly
    //      when there is HTML overlapping the view, see bug 4361626)
    //   4) NSAccessibilityHitTest relies on this for checking the cursor position.
    //      Our check for that is whether the event is NSFlagsChanged.  This works
    //      for VoiceOver's Control-Option-F5 command (move focus to item under cursor)
    //      and Dictionary's Command-Control-D (open dictionary popup for item under cursor).
    //      This is of course a hack.

    if (_private->closed)
        return nil;

#if !PLATFORM(IOS_FAMILY)
    BOOL captureHitsOnSubviews;
    if (forceNSViewHitTest)
        captureHitsOnSubviews = NO;
    else if (forceWebHTMLViewHitTest)
        captureHitsOnSubviews = YES;
    else {
        // FIXME: Why doesn't this include mouse entered/exited events, or other mouse button events?
        NSEvent *event = [[self window] currentEvent];
        captureHitsOnSubviews = !([event type] == NSEventTypeMouseMoved
            || [event type] == NSEventTypeRightMouseDown
            || ([event type] == NSEventTypeLeftMouseDown && [event modifierFlags] & NSEventModifierFlagControl)
            || [event type] == NSEventTypeFlagsChanged
            || isQuickLookEvent(event));
    }

    if (!captureHitsOnSubviews) {
        NSView* hitView = [super hitTest:point];
        if (_private && hitView == _private->layerHostingView)
            hitView = self;
        return hitView;
    }
#endif // !PLATFORM(IOS_FAMILY)

    if ([[self superview] mouse:point inRect:[self frame]])
        return self;
    return nil;
}

#if PLATFORM(MAC)

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    ASSERT(tag == 0 || tag == TRACKING_RECT_TAG);
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (void)_addTrackingRects:(NSRect *)rects owner:(id)owner userDataList:(void **)userDataList assumeInsideList:(BOOL *)assumeInsideList trackingNums:(NSTrackingRectTag *)trackingNums count:(int)count
{
    ASSERT(count == 1);
    ASSERT(trackingNums[0] == 0 || trackingNums[0] == TRACKING_RECT_TAG);
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = userDataList[0];
    trackingNums[0] = TRACKING_RECT_TAG;
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    if (tag == 0)
        return;
    
    if (_private && (tag == TRACKING_RECT_TAG)) {
        _private->trackingRectOwner = nil;
        return;
    }
    
    if (_private && (tag == _private->lastToolTipTag)) {
        [super removeTrackingRect:tag];
        _private->lastToolTipTag = 0;
        return;
    }
    
    // If any other tracking rect is being removed, we don't know how it was created
    // and it's possible there's a leak involved (see 3500217)
    ASSERT_NOT_REACHED();
}

- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count
{
    int i;
    for (i = 0; i < count; ++i) {
        int tag = tags[i];
        if (tag == 0)
            continue;
        ASSERT(tag == TRACKING_RECT_TAG);
        if (_private != nil) {
            _private->trackingRectOwner = nil;
        }
    }
}

- (void)_sendToolTipMouseExited
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSEventTypeMouseExited
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseExited:fakeEvent];
}

- (void)_sendToolTipMouseEntered
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSEventTypeMouseEntered
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseEntered:fakeEvent];
}

#endif // PLATFORM(MAC)

- (void)_setToolTip:(NSString *)string
{
#if PLATFORM(MAC)
    NSString *toolTip = [string length] == 0 ? nil : string;
    NSString *oldToolTip = _private->toolTip.get();
    if (toolTip == oldToolTip || [toolTip isEqualToString:oldToolTip])
        return;
    if (oldToolTip)
        [self _sendToolTipMouseExited];
    _private->toolTip = adoptNS([toolTip copy]);
    if (toolTip) {
        // See radar 3500217 for why we remove all tooltips rather than just the single one we created.
        [self removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        _private->lastToolTipTag = [self addToolTipRect:wideOpenRect owner:self userData:NULL];
        [self _sendToolTipMouseEntered];
    }
#endif
}

#if PLATFORM(MAC)

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return [[_private->toolTip copy] autorelease];
}

static bool mouseEventIsPartOfClickOrDrag(NSEvent *event)
{
    switch ([event type]) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
        return true;
    default:
        return false;
    }
}

- (void)_updateMouseoverWithEvent:(NSEvent *)event
{
    if (_private->closed)
        return;

    NSView *contentView = [[event window] contentView];
    NSPoint locationForHitTest = [[contentView superview] convertPoint:[event locationInWindow] fromView:nil];
    
    forceWebHTMLViewHitTest = YES;
    NSView *hitView = [contentView hitTest:locationForHitTest];
    forceWebHTMLViewHitTest = NO;
    
    WebHTMLView *view = nil;
    if ([hitView isKindOfClass:[WebHTMLView class]])
        view = (WebHTMLView *)hitView;    

    if (view)
        [view retain];

    if (lastHitView != view && lastHitView && [lastHitView _frame]) {
        // If we are moving out of a view (or frame), let's pretend the mouse moved
        // all the way out of that view. But we have to account for scrolling, because
        // WebCore doesn't understand our clipping.
        NSRect visibleRect = [[[[lastHitView _frame] frameView] _scrollView] documentVisibleRect];
        float yScroll = visibleRect.origin.y;
        float xScroll = visibleRect.origin.x;

        NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
            location:NSMakePoint(-1 - xScroll, -1 - yScroll)
            modifierFlags:[[NSApp currentEvent] modifierFlags]
            timestamp:[NSDate timeIntervalSinceReferenceDate]
            windowNumber:[[view window] windowNumber]
            context:nullptr
            eventNumber:0 clickCount:0 pressure:0];

        if (Frame* lastHitCoreFrame = core([lastHitView _frame]))
            lastHitCoreFrame->eventHandler().mouseMoved(event, [[self _webView] _pressureEvent]);
    }

    lastHitView = view;

    if (view) {
        if (Frame* coreFrame = core([view _frame])) {
            // We need to do a full, normal hit test during this mouse event if the page is active or if a mouse
            // button is currently pressed. It is possible that neither of those things will be true on Lion and
            // newer when legacy scrollbars are enabled, because then WebKit receives mouse events all the time. 
            // If it is one of those cases where the page is not active and the mouse is not pressed, then we can
            // fire a much more restricted and efficient scrollbars-only version of the event.

            if ([[self window] isKeyWindow] 
                || mouseEventIsPartOfClickOrDrag(event)
#if ENABLE(DASHBOARD_SUPPORT)
                || [[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows]
#endif
                ) {
                coreFrame->eventHandler().mouseMoved(event, [[self _webView] _pressureEvent]);
            } else {
                [self removeAllToolTips];
                coreFrame->eventHandler().passMouseMovedEventToScrollbars(event, [[self _webView] _pressureEvent]);
            }
        }

        [view release];
    }
}

+ (NSString *)_dummyPasteboardType
{
    return @"Apple WebKit dummy pasteboard type";
}

+ (NSArray *)_insertablePasteboardTypes
{
    static NSArray *types = nil;
    if (!types) {
        types = [[NSArray alloc] initWithObjects:WebArchivePboardType, legacyHTMLPasteboardType(), legacyFilenamesPasteboardType(), legacyTIFFPasteboardType(), legacyPDFPasteboardType(),
            legacyURLPasteboardType(), legacyRTFDPasteboardType(), legacyRTFPasteboardType(), legacyStringPasteboardType(), legacyColorPasteboardType(), kUTTypePNG, nil];
        CFRetain(types);
    }
    return types;
}

+ (NSArray *)_selectionPasteboardTypes
{
    // FIXME: We should put data for NSHTMLPboardType on the pasteboard but Microsoft Excel doesn't like our format of HTML (3640423).
    return [NSArray arrayWithObjects:WebArchivePboardType, legacyRTFDPasteboardType(), legacyRTFPasteboardType(), legacyStringPasteboardType(), nil];
}

- (void)pasteboardChangedOwner:(NSPasteboard *)pasteboard
{
    [self setPromisedDragTIFFDataSource:nullptr];
}

- (void)pasteboard:(NSPasteboard *)pasteboard provideDataForType:(NSString *)type
{
    if ([type isEqualToString:legacyRTFDPasteboardType()] && [[pasteboard types] containsObject:WebArchivePboardType]) {
        auto archive = adoptNS([[WebArchive alloc] initWithData:[pasteboard dataForType:WebArchivePboardType]]);
        [pasteboard _web_writePromisedRTFDFromArchive:archive.get() containsImage:[[pasteboard types] containsObject:legacyTIFFPasteboardType()]];
    } else if ([type isEqualToString:legacyTIFFPasteboardType()] && _private->promisedDragTIFFDataSource) {
        if (auto* image = _private->promisedDragTIFFDataSource->image())
            [pasteboard setData:(__bridge NSData *)image->tiffRepresentation() forType:legacyTIFFPasteboardType()];
        [self setPromisedDragTIFFDataSource:nullptr];
    }
}

- (void)_handleAutoscrollForMouseDragged:(NSEvent *)event 
{ 
    [self autoscroll:event]; 
    [self _startAutoscrollTimer:event]; 
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

// WAKView override.
- (void)layoutIfNeeded
{
    [self _layoutIfNeeded];
}

// WAKView override.
- (void)setScale:(float)scale
{
    [super setScale:scale];
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    if (Page* page = coreFrame->page())
        page->setPageScaleFactor(scale, IntPoint());

    [[self _webView] _documentScaleChanged];
}

#endif

#if PLATFORM(MAC)

- (void)_layoutForPrinting
{
    // Set printing mode temporarily so we can adjust the size of the view. This will allow
    // AppKit's pagination code to use the correct height for the page content. Leaving printing
    // mode on indefinitely would interfere with Mail's printing mechanism (at least), so we just
    // turn it off again after adjusting the size.
    [self _web_setPrintingModeRecursiveAndAdjustViewSize];
    [self _web_clearPrintingModeRecursive];
}

- (void)_smartInsertForString:(NSString *)pasteString replacingRange:(DOMRange *)rangeToReplace beforeString:(NSString **)beforeString afterString:(NSString **)afterString
{
    if (!pasteString || !rangeToReplace || ![[self _webView] smartInsertDeleteEnabled]) {
        if (beforeString)
            *beforeString = nil;
        if (afterString)
            *afterString = nil;
        return;
    }
    
    [[self _frame] _smartInsertForString:pasteString replacingRange:rangeToReplace beforeString:beforeString afterString:afterString];
}

- (BOOL)_canSmartReplaceWithPasteboard:(NSPasteboard *)pasteboard
{
    return [[self _webView] smartInsertDeleteEnabled] && [[pasteboard types] containsObject:WebSmartPastePboardType];
}

#endif

// FIXME: _selectionRect is deprecated in favor of selectionRect, which is in protocol WebDocumentSelection.
// We can't remove this yet because it's still in use by Mail.
- (NSRect)_selectionRect
{
    return [self selectionRect];
}

#if PLATFORM(MAC)

- (void)_autoscroll
{
    // Guarantee that the autoscroll timer is invalidated, even if we don't receive
    // a mouse up event.
    BOOL isStillDown = CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft);
    if (!isStillDown){
        [self _stopAutoscrollTimer];
        return;
    }

    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDragged
        location:[[self window]
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        convertScreenToBase:[NSEvent mouseLocation]]
ALLOW_DEPRECATED_DECLARATIONS_END
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:nullptr
        eventNumber:0 clickCount:0 pressure:0];

    [self mouseDragged:fakeEvent];
}

#endif

- (BOOL)_canEdit
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor().canEdit();
}

- (BOOL)_canEditRichly
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor().canEditRichly();
}

- (BOOL)_canAlterCurrentSelection
{
    return [self _hasSelectionOrInsertionPoint] && [self _isEditable];
}

- (BOOL)_hasSelection
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selection().selection().isRange();
}

- (BOOL)_hasSelectionOrInsertionPoint
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selection().selection().isCaretOrRange();
}

- (BOOL)_hasInsertionPoint
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selection().selection().isCaret();
}

- (BOOL)_isEditable
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selection().selection().isContentEditable();
}

#if PLATFORM(MAC)

- (BOOL)_transparentBackground
{
    return _private->transparentBackground;
}

- (void)_setTransparentBackground:(BOOL)f
{
    _private->transparentBackground = f;
}

- (NSImage *)_selectionDraggingImage
{
    if (![self _hasSelection])
        return nil;

    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return nil;

    TextIndicatorData textIndicator;
    auto dragImage = createDragImageForSelection(*coreFrame, textIndicator);
    [dragImage _web_dissolveToFraction:WebDragImageAlpha];

    return dragImage.autorelease();
}

- (NSRect)_selectionDraggingRect
{
    // Mail currently calls this method. We can eliminate it when Mail no longer calls it.
    return [self selectionRect];
}

#endif

- (DOMNode *)_insertOrderedList
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->editor().insertOrderedList().get()) : nil;
}

- (DOMNode *)_insertUnorderedList
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->editor().insertUnorderedList().get()) : nil;
}

- (BOOL)_canIncreaseSelectionListLevel
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor().canIncreaseSelectionListLevel();
}

- (BOOL)_canDecreaseSelectionListLevel
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor().canDecreaseSelectionListLevel();
}

- (DOMNode *)_increaseSelectionListLevel
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->editor().increaseSelectionListLevel().get()) : nil;
}

- (DOMNode *)_increaseSelectionListLevelOrdered
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->editor().increaseSelectionListLevelOrdered().get()) : nil;
}

- (DOMNode *)_increaseSelectionListLevelUnordered
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->editor().increaseSelectionListLevelUnordered().get()) : nil;
}

- (void)_decreaseSelectionListLevel
{
    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        coreFrame->editor().decreaseSelectionListLevel();
}

#if PLATFORM(MAC)

- (void)_writeSelectionToPasteboard:(NSPasteboard *)pasteboard
{
    ASSERT([self _hasSelection]);
    NSArray *types = [self pasteboardTypesForSelection];

    // Don't write RTFD to the pasteboard when the copied attributed string has no attachments.
    NSAttributedString *attributedString = [self selectedAttributedString];
    RetainPtr<NSMutableArray> mutableTypes;
    if (![attributedString containsAttachments]) {
        mutableTypes = adoptNS([types mutableCopy]);
        [mutableTypes removeObject:legacyRTFDPasteboardType()];
        types = mutableTypes.get();
    }

    [pasteboard declareTypes:types owner:[self _topHTMLView]];
    [self _writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard cachedAttributedString:attributedString];
}

#endif

- (void)close
{
    // Check for a nil _private here in case we were created with initWithCoder. In that case, the WebView is just throwing
    // out the archived WebHTMLView and recreating a new one if needed. So close doesn't need to do anything in that case.
    if (!_private || _private->closed)
        return;

    _private->closed = YES;

#if PLATFORM(MAC)
    if (lastHitView == self)
        lastHitView = nil;

    [self _removeWindowObservers];
    [self _removeSuperviewObservers];
#endif

    [_private->pluginController destroyAllPlugins];
    [_private->pluginController setDataSource:nil];

#if PLATFORM(MAC)
    // remove tooltips before clearing _private so removeTrackingRect: will work correctly
    [self removeAllToolTips];

    if (_private->isInSecureInputState) {
        DisableSecureEventInput();
        _private->isInSecureInputState = NO;
    }
#endif

    [_private clear];
}

#if PLATFORM(MAC)

- (DOMDocumentFragment *)_web_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard pasteboardType:(NSString *)pasteboardType imageMIMEType:(NSString *)imageMIMEType
{
    auto filename = [imageMIMEType stringByReplacingOccurrencesOfString:@"/" withString:@"."];
    auto resource = adoptNS([[WebResource alloc] initWithData:[pasteboard dataForType:pasteboardType]
        URL:URL::fakeURLWithRelativePart(filename) MIMEType:imageMIMEType textEncodingName:nil frameName:nil]);
    return [[self _dataSource] _documentFragmentWithImageResource:resource.get()];
}

- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard
                                                 forType:(NSString *)pboardType
                                               inContext:(DOMRange *)context
                                            subresources:(NSArray **)subresources
{
    if ([pboardType isEqualToString:WebArchivePboardType]) {
        auto archive = adoptNS([[WebArchive alloc] initWithData:[pasteboard dataForType:WebArchivePboardType]]);
        if (subresources)
            *subresources = [archive subresources];
        return [[self _dataSource] _documentFragmentWithArchive:archive.get()];
    }

    if ([pboardType isEqualToString:legacyFilenamesPasteboardType()])
        return [self _documentFragmentWithPaths:[pasteboard propertyListForType:legacyFilenamesPasteboardType()]];

    if ([pboardType isEqualToString:legacyHTMLPasteboardType()]) {
        NSString *HTMLString = [pasteboard stringForType:legacyHTMLPasteboardType()];
        // This is a hack to make Microsoft's HTML pasteboard data work. See 3778785.
        if ([HTMLString hasPrefix:@"Version:"]) {
            NSRange range = [HTMLString rangeOfString:@"<html" options:NSCaseInsensitiveSearch];
            if (range.location != NSNotFound)
                HTMLString = [HTMLString substringFromIndex:range.location];
        }
        if ([HTMLString length] == 0)
            return nil;
        return [[self _frame] _documentFragmentWithMarkupString:HTMLString baseURLString:nil];
    }

    if ([pboardType isEqualToString:legacyRTFPasteboardType()] || [pboardType isEqualToString:legacyRTFDPasteboardType()]) {
        RetainPtr<NSAttributedString> string;
        if ([pboardType isEqualToString:legacyRTFDPasteboardType()])
            string = adoptNS([[NSAttributedString alloc] initWithRTFD:[pasteboard dataForType:legacyRTFDPasteboardType()] documentAttributes:NULL]);
        if (!string)
            string = adoptNS([[NSAttributedString alloc] initWithRTF:[pasteboard dataForType:legacyRTFPasteboardType()] documentAttributes:NULL]);
        if (!string)
            return nil;

        auto documentAttributes = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:
            [[self class] _excludedElementsForAttributedStringConversion], NSExcludedElementsDocumentAttribute,
            nil]);

        BOOL wasDeferringCallbacks = [[self _webView] defersCallbacks];
        if (!wasDeferringCallbacks)
            [[self _webView] setDefersCallbacks:YES];

        NSArray *localSubresources;
        DOMDocumentFragment *fragment = [string _documentFromRange:NSMakeRange(0, [string length]) document:[[self _frame] DOMDocument]
            documentAttributes:documentAttributes.get() subresources:&localSubresources];

        if (subresources)
            *subresources = localSubresources;

        for (WebResource *resource in localSubresources)
            [[self _dataSource] addSubresource:resource];

        if (!wasDeferringCallbacks)
            [[self _webView] setDefersCallbacks:NO];

        return fragment;
    }

    if ([pboardType isEqualToString:legacyTIFFPasteboardType()])
        return [self _web_documentFragmentFromPasteboard:pasteboard pasteboardType:legacyTIFFPasteboardType() imageMIMEType:@"image/tiff"];
    if ([pboardType isEqualToString:legacyPDFPasteboardType()])
        return [self _web_documentFragmentFromPasteboard:pasteboard pasteboardType:legacyPDFPasteboardType() imageMIMEType:@"application/pdf"];
    if ([pboardType isEqualToString:(NSString *)kUTTypePNG])
        return [self _web_documentFragmentFromPasteboard:pasteboard pasteboardType:(NSString *)kUTTypePNG imageMIMEType:@"image/png"];

    if ([pboardType isEqualToString:legacyURLPasteboardType()]) {
        NSURL *URL = [NSURL URLFromPasteboard:pasteboard];
        DOMDocument* document = [[self _frame] DOMDocument];
        ASSERT(document);
        if (!document)
            return nil;
        DOMHTMLAnchorElement *anchor = (DOMHTMLAnchorElement *)[document createElement:@"a"];
        NSString *URLString = [URL _web_originalDataAsString]; // Original data is ASCII-only, so there is no need to precompose.
        if ([URLString length] == 0)
            return nil;
        NSString *URLTitleString = [[pasteboard stringForType:WebURLNamePboardType] precomposedStringWithCanonicalMapping];
        DOMText *text = [document createTextNode:URLTitleString];
        [anchor setHref:URLString];
        [anchor appendChild:text];
        DOMDocumentFragment *fragment = [document createDocumentFragment];
        [fragment appendChild:anchor];
        return fragment;
    }

    if ([pboardType isEqualToString:legacyStringPasteboardType()]) {
        if (!context)
            return nil;
        return kit(createFragmentFromText(*core(context), [[pasteboard stringForType:legacyStringPasteboardType()] precomposedStringWithCanonicalMapping]).ptr());
    }

    return nil;
}

#endif // PLATFORM(MAC)

#if ENABLE(NETSCAPE_PLUGIN_API) 

- (void)_pauseNullEventsForAllNetscapePlugins 
{
    [self _web_makePluginSubviewsPerformSelector:@selector(stopTimers) withObject:nil];
}

- (void)_resumeNullEventsForAllNetscapePlugins
{
    [self _web_makePluginSubviewsPerformSelector:@selector(restartTimers) withObject:nil];
}

#endif 

- (BOOL)_isUsingAcceleratedCompositing
{
    return _private->layerHostingView != nil;
}

- (NSView *)_compositingLayersHostingView
{
    return _private->layerHostingView;
}

- (BOOL)_isInPrintMode
{
    return _private->printing;
}

- (BOOL)_beginPrintModeWithMinimumPageWidth:(CGFloat)minimumPageWidth height:(CGFloat)minimumPageHeight maximumPageWidth:(CGFloat)maximumPageWidth
{
    Frame* frame = core([self _frame]);
    if (!frame)
        return NO;

    if (frame->document() && frame->document()->isFrameSet()) {
        minimumPageWidth = 0;
        minimumPageHeight = 0;
    }

    float maximumShrinkRatio = 0;
    if (minimumPageWidth > 0.0)
        maximumShrinkRatio = maximumPageWidth / minimumPageWidth;

    [self _setPrinting:YES minimumPageLogicalWidth:minimumPageWidth logicalHeight:minimumPageHeight originalPageWidth:minimumPageWidth originalPageHeight:minimumPageHeight maximumShrinkRatio:maximumShrinkRatio adjustViewSize:YES paginateScreenContent:[self _isInScreenPaginationMode]];
    return YES;
}

- (BOOL)_beginPrintModeWithPageWidth:(float)pageWidth height:(float)pageHeight shrinkToFit:(BOOL)shrinkToFit
{
    Frame* frame = core([self _frame]);
    if (!frame)
        return NO;

    Document* document = frame->document();
    bool isHorizontal = !document || !document->renderView() || document->renderView()->style().isHorizontalWritingMode();

    float pageLogicalWidth = isHorizontal ? pageWidth : pageHeight;
    float pageLogicalHeight = isHorizontal ? pageHeight : pageWidth;
    FloatSize minLayoutSize(pageLogicalWidth, pageLogicalHeight);
    float maximumShrinkRatio = 1;

    // If we are a frameset just print with the layout we have onscreen, otherwise relayout
    // according to the page width.
    if (shrinkToFit && (!frame->document() || !frame->document()->isFrameSet())) {
        minLayoutSize = frame->resizePageRectsKeepingRatio(FloatSize(pageLogicalWidth, pageLogicalHeight), FloatSize(pageLogicalWidth * _WebHTMLViewPrintingMinimumShrinkFactor, pageLogicalHeight * _WebHTMLViewPrintingMinimumShrinkFactor));
        maximumShrinkRatio = _WebHTMLViewPrintingMaximumShrinkFactor / _WebHTMLViewPrintingMinimumShrinkFactor;
    }

    [self _setPrinting:YES minimumPageLogicalWidth:minLayoutSize.width() logicalHeight:minLayoutSize.height() originalPageWidth:pageLogicalWidth originalPageHeight:pageLogicalHeight maximumShrinkRatio:maximumShrinkRatio adjustViewSize:YES paginateScreenContent:[self _isInScreenPaginationMode]];

    return YES;
}

- (void)_endPrintMode
{
    [self _setPrinting:NO minimumPageLogicalWidth:0 logicalHeight:0 originalPageWidth:0 originalPageHeight:0 maximumShrinkRatio:0 adjustViewSize:YES paginateScreenContent:[self _isInScreenPaginationMode]];
}

- (BOOL)_isInScreenPaginationMode
{
    return _private->paginateScreenContent;
}

- (BOOL)_beginScreenPaginationModeWithPageSize:(CGSize)pageSize shrinkToFit:(BOOL)shrinkToFit
{
    Frame* frame = core([self _frame]);
    if (!frame)
        return NO;

    Document* document = frame->document();
    bool isHorizontal = !document || !document->renderView() || document->renderView()->style().isHorizontalWritingMode();

    float pageLogicalWidth = isHorizontal ? pageSize.width : pageSize.height;
    float pageLogicalHeight = isHorizontal ? pageSize.height : pageSize.width;
    FloatSize minLayoutSize(pageLogicalWidth, pageLogicalHeight);
    float maximumShrinkRatio = 1;

    // If we are a frameset just print with the layout we have onscreen, otherwise relayout
    // according to the page width.
    if (shrinkToFit && (!frame->document() || !frame->document()->isFrameSet())) {
        minLayoutSize = frame->resizePageRectsKeepingRatio(FloatSize(pageLogicalWidth, pageLogicalHeight), FloatSize(pageLogicalWidth * _WebHTMLViewPrintingMinimumShrinkFactor, pageLogicalHeight * _WebHTMLViewPrintingMinimumShrinkFactor));
        maximumShrinkRatio = _WebHTMLViewPrintingMaximumShrinkFactor / _WebHTMLViewPrintingMinimumShrinkFactor;
    }

    [self _setPrinting:[self _isInPrintMode] minimumPageLogicalWidth:minLayoutSize.width() logicalHeight:minLayoutSize.height() originalPageWidth:pageLogicalWidth originalPageHeight:pageLogicalHeight maximumShrinkRatio:maximumShrinkRatio adjustViewSize:YES paginateScreenContent:[self _isInScreenPaginationMode]];

    return YES;
}

- (void)_endScreenPaginationMode
{
    [self _setPrinting:[self _isInPrintMode] minimumPageLogicalWidth:0 logicalHeight:0 originalPageWidth:0 originalPageHeight:0 maximumShrinkRatio:0 adjustViewSize:YES paginateScreenContent:NO];
}

- (CGFloat)_adjustedBottomOfPageWithTop:(CGFloat)top bottom:(CGFloat)bottom limit:(CGFloat)bottomLimit
{
    Frame* frame = core([self _frame]);
    if (!frame)
        return bottom;

    FrameView* view = frame->view();
    if (!view)
        return bottom;

    float newBottom;
    view->adjustPageHeightDeprecated(&newBottom, top, bottom, bottomLimit);

#ifdef __LP64__
    // If the new bottom is equal to the old bottom (when both are treated as floats), we just return the original
    // bottom. This prevents rounding errors that can occur when converting newBottom to a double.
    if (WTF::areEssentiallyEqual(static_cast<float>(bottom), newBottom))
        return bottom;
    else
#endif
        return newBottom;
}

#if PLATFORM(IOS_FAMILY)

- (id)accessibilityRootElement
{
    return [[self _frame] accessibilityRoot];
}

#endif

@end

@implementation NSView (WebHTMLViewFileInternal)

- (void)_web_addDescendentWebHTMLViewsToArray:(NSMutableArray *)array
{
    for (NSView *child in [self subviews]) {
        if ([child isKindOfClass:[WebHTMLView class]])
            [array addObject:child];
        [child _web_addDescendentWebHTMLViewsToArray:array];
    }
}

@end

@implementation WebHTMLView

#if PLATFORM(MAC)

+ (void)initialize
{
    [NSApp registerServicesMenuSendTypes:[[self class] _selectionPasteboardTypes] returnTypes:[[self class] _insertablePasteboardTypes]];

    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
    RunLoop::initializeMainRunLoop();
}

#endif

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;
    
#if PLATFORM(MAC)
    [self setFocusRingType:NSFocusRingTypeNone];
#endif
    
    // Make all drawing go through us instead of subviews.
    [self _setDrawsOwnDescendants:YES];
    
    _private = [[WebHTMLViewPrivate alloc] init];

    _private->pluginController = adoptNS([[WebPluginController alloc] initWithDocumentView:self]);

#if PLATFORM(IOS_FAMILY)
    [[NSNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(markedTextUpdate:) 
                   name:WebMarkedTextUpdatedNotification object:nil];
    CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(), (__bridge const void *)(self), hardwareKeyboardAvailabilityChangedCallback, (CFStringRef)[NSString stringWithUTF8String:kGSEventHardwareKeyboardAvailabilityChangedNotification], nullptr, CFNotificationSuspensionBehaviorCoalesce);
#endif

#if PLATFORM(MAC)
    _private->softSpaceRange = NSMakeRange(NSNotFound, 0);
#endif
    
    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebHTMLView class], self))
        return;

#if PLATFORM(IOS_FAMILY)
    [[NSNotificationCenter defaultCenter] removeObserver:self name:WebMarkedTextUpdatedNotification object:nil];
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetDarwinNotifyCenter(), (__bridge const void *)(self), (CFStringRef)[NSString stringWithUTF8String:kGSEventHardwareKeyboardAvailabilityChangedNotification], nullptr);
#endif

    // We can't assert that close has already been called because
    // this view can be removed from it's superview, even though
    // it could be needed later, so close if needed.
    [self close];
    [_private release];
    _private = nil;

    [super dealloc];
}

// Returns YES if the delegate returns YES (so we should do no more work).
- (BOOL)callDelegateDoCommandBySelectorIfNeeded:(SEL)selector
{
    BOOL callerAlreadyCalledDelegate = _private->selectorForDoCommandBySelector == selector;
    _private->selectorForDoCommandBySelector = 0;
    if (callerAlreadyCalledDelegate)
        return NO;
    WebView *webView = [self _webView];
    return [[webView _editingDelegateForwarder] webView:webView doCommandBySelector:selector];
}

typedef HashMap<SEL, String> SelectorNameMap;

// Map selectors into Editor command names.
// This is not needed for any selectors that have the same name as the Editor command.
static const SelectorNameMap* createSelectorExceptionMap()
{
    SelectorNameMap* map = new HashMap<SEL, String>;

    map->add(@selector(insertNewlineIgnoringFieldEditor:), "InsertNewline");
    map->add(@selector(insertParagraphSeparator:), "InsertNewline");
    map->add(@selector(insertTabIgnoringFieldEditor:), "InsertTab");
    map->add(@selector(pageDown:), "MovePageDown");
    map->add(@selector(pageDownAndModifySelection:), "MovePageDownAndModifySelection");
    map->add(@selector(pageUp:), "MovePageUp");
    map->add(@selector(pageUpAndModifySelection:), "MovePageUpAndModifySelection");

    return map;
}

static String commandNameForSelector(SEL selector)
{
    // Check the exception map first.
    static const SelectorNameMap* exceptionMap = createSelectorExceptionMap();
    SelectorNameMap::const_iterator it = exceptionMap->find(selector);
    if (it != exceptionMap->end())
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

- (Editor::Command)coreCommandBySelector:(SEL)selector
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return Editor::Command();
    return coreFrame->editor().command(commandNameForSelector(selector));
}

- (Editor::Command)coreCommandByName:(const char*)name
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return Editor::Command();
    return coreFrame->editor().command(name);
}

- (void)executeCoreCommandBySelector:(SEL)selector
{
    if ([self callDelegateDoCommandBySelectorIfNeeded:selector])
        return;
    [self coreCommandBySelector:selector].execute();
}

- (void)executeCoreCommandByName:(const char*)name
{
    [self coreCommandByName:name].execute();
}

// These commands are forwarded to the Editor object in WebCore.
// Ideally we'd do this for all editing commands; more of the code
// should be moved from here to there, and more commands should be
// added to this list.

// FIXME: Maybe we should set things up so that all these share a single method implementation function.
// The functions are identical.

#define WEBCORE_COMMAND(command) - (void)command:(id)sender { [self executeCoreCommandBySelector:_cmd]; }

WEBCORE_COMMAND(alignCenter)
WEBCORE_COMMAND(alignJustified)
WEBCORE_COMMAND(alignLeft)
WEBCORE_COMMAND(alignRight)
WEBCORE_COMMAND(copy)
WEBCORE_COMMAND(cut)
WEBCORE_COMMAND(paste)
WEBCORE_COMMAND(delete)
WEBCORE_COMMAND(deleteBackward)
WEBCORE_COMMAND(deleteBackwardByDecomposingPreviousCharacter)
WEBCORE_COMMAND(deleteForward)
WEBCORE_COMMAND(deleteToBeginningOfLine)
WEBCORE_COMMAND(deleteToBeginningOfParagraph)
WEBCORE_COMMAND(deleteToEndOfLine)
WEBCORE_COMMAND(deleteToEndOfParagraph)
WEBCORE_COMMAND(deleteToMark)
WEBCORE_COMMAND(deleteWordBackward)
WEBCORE_COMMAND(deleteWordForward)
WEBCORE_COMMAND(ignoreSpelling)
WEBCORE_COMMAND(indent)
WEBCORE_COMMAND(insertBacktab)
WEBCORE_COMMAND(insertLineBreak)
WEBCORE_COMMAND(insertNewline)
WEBCORE_COMMAND(insertNewlineIgnoringFieldEditor)
WEBCORE_COMMAND(insertParagraphSeparator)
WEBCORE_COMMAND(insertTab)
WEBCORE_COMMAND(insertTabIgnoringFieldEditor)
WEBCORE_COMMAND(makeTextWritingDirectionLeftToRight)
WEBCORE_COMMAND(makeTextWritingDirectionNatural)
WEBCORE_COMMAND(makeTextWritingDirectionRightToLeft)
WEBCORE_COMMAND(moveBackward)
WEBCORE_COMMAND(moveBackwardAndModifySelection)
WEBCORE_COMMAND(moveDown)
WEBCORE_COMMAND(moveDownAndModifySelection)
WEBCORE_COMMAND(moveForward)
WEBCORE_COMMAND(moveForwardAndModifySelection)
WEBCORE_COMMAND(moveLeft)
WEBCORE_COMMAND(moveLeftAndModifySelection)
WEBCORE_COMMAND(moveParagraphBackwardAndModifySelection)
WEBCORE_COMMAND(moveParagraphForwardAndModifySelection)
WEBCORE_COMMAND(moveRight)
WEBCORE_COMMAND(moveRightAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfDocument)
WEBCORE_COMMAND(moveToBeginningOfDocumentAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfLine)
WEBCORE_COMMAND(moveToBeginningOfLineAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfParagraph)
WEBCORE_COMMAND(moveToBeginningOfParagraphAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfSentence)
WEBCORE_COMMAND(moveToBeginningOfSentenceAndModifySelection)
WEBCORE_COMMAND(moveToEndOfDocument)
WEBCORE_COMMAND(moveToEndOfDocumentAndModifySelection)
WEBCORE_COMMAND(moveToEndOfLine)
WEBCORE_COMMAND(moveToEndOfLineAndModifySelection)
WEBCORE_COMMAND(moveToEndOfParagraph)
WEBCORE_COMMAND(moveToEndOfParagraphAndModifySelection)
WEBCORE_COMMAND(moveToEndOfSentence)
WEBCORE_COMMAND(moveToEndOfSentenceAndModifySelection)
WEBCORE_COMMAND(moveToLeftEndOfLine)
WEBCORE_COMMAND(moveToLeftEndOfLineAndModifySelection)
WEBCORE_COMMAND(moveToRightEndOfLine)
WEBCORE_COMMAND(moveToRightEndOfLineAndModifySelection)
WEBCORE_COMMAND(moveUp)
WEBCORE_COMMAND(moveUpAndModifySelection)
WEBCORE_COMMAND(moveWordBackward)
WEBCORE_COMMAND(moveWordBackwardAndModifySelection)
WEBCORE_COMMAND(moveWordForward)
WEBCORE_COMMAND(moveWordForwardAndModifySelection)
WEBCORE_COMMAND(moveWordLeft)
WEBCORE_COMMAND(moveWordLeftAndModifySelection)
WEBCORE_COMMAND(moveWordRight)
WEBCORE_COMMAND(moveWordRightAndModifySelection)
WEBCORE_COMMAND(outdent)
WEBCORE_COMMAND(overWrite)
WEBCORE_COMMAND(pageDown)
WEBCORE_COMMAND(pageDownAndModifySelection)
WEBCORE_COMMAND(pageUp)
WEBCORE_COMMAND(pageUpAndModifySelection)
WEBCORE_COMMAND(pasteAsPlainText)
WEBCORE_COMMAND(selectAll)
WEBCORE_COMMAND(selectLine)
WEBCORE_COMMAND(selectParagraph)
WEBCORE_COMMAND(selectSentence)
WEBCORE_COMMAND(selectToMark)
WEBCORE_COMMAND(selectWord)
WEBCORE_COMMAND(setMark)
WEBCORE_COMMAND(subscript)
WEBCORE_COMMAND(superscript)
WEBCORE_COMMAND(swapWithMark)
WEBCORE_COMMAND(transpose)
WEBCORE_COMMAND(underline)
WEBCORE_COMMAND(unscript)
WEBCORE_COMMAND(yank)
WEBCORE_COMMAND(yankAndSelect)

#if PLATFORM(IOS_FAMILY)
WEBCORE_COMMAND(clearText)
WEBCORE_COMMAND(toggleBold)
WEBCORE_COMMAND(toggleItalic)
WEBCORE_COMMAND(toggleUnderline)
#endif

#undef WEBCORE_COMMAND

#define COMMAND_PROLOGUE if ([self callDelegateDoCommandBySelectorIfNeeded:_cmd]) return;

#if PLATFORM(MAC)

- (IBAction)takeFindStringFromSelection:(id)sender
{
    COMMAND_PROLOGUE

    if (![self _hasSelection]) {
        NSBeep();
        return;
    }

    [NSPasteboard _web_setFindPasteboardString:[self selectedString] withOwner:self];
}

// This method is needed to support macOS services.
- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    [pasteboard declareTypes:types owner:[self _topHTMLView]];
    [self writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
    return YES;
}

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
    BOOL isSendTypeOK = !sendType || ([[self pasteboardTypesForSelection] containsObject:sendType] && [self _hasSelection]);
    BOOL isReturnTypeOK = NO;
    if (!returnType)
        isReturnTypeOK = YES;
    else if ([[[self class] _insertablePasteboardTypes] containsObject:returnType] && [self _isEditable]) {
        // We can insert strings in any editable context.  We can insert other types, like images, only in rich edit contexts.
        isReturnTypeOK = [returnType isEqualToString:legacyStringPasteboardType()] || [self _canEditRichly];
    }
    if (isSendTypeOK && isReturnTypeOK)
        return self;
    return [[self nextResponder] validRequestorForSendType:sendType returnType:returnType];
}

#endif

// jumpToSelection is the old name for what AppKit now calls centerSelectionInVisibleArea. Safari
// was using the old jumpToSelection selector in its menu. Newer versions of Safari will use the
// selector centerSelectionInVisibleArea. We'll leave the old selector in place for two reasons:
// (1) Compatibility between older Safari and newer WebKit; (2) other WebKit-based applications
// might be using the selector, and we don't want to break them.
- (void)jumpToSelection:(id)sender
{
    COMMAND_PROLOGUE

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->selection().revealSelection(SelectionRevealMode::Reveal, ScrollAlignment::alignCenterAlways);
}

#if PLATFORM(MAC)

- (BOOL)validateUserInterfaceItemWithoutDelegate:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];
    RefPtr<Frame> frame = core([self _frame]);

    if (!frame)
        return NO;

    if (Document* doc = frame->document()) {
        if (doc->isPluginDocument())
            return NO;
        if (doc->isImageDocument()) {            
            if (action == @selector(copy:))
                return frame->loader().isComplete();
            return NO;
        }
    }
    if (action == @selector(changeSpelling:)
IGNORE_WARNINGS_BEGIN("undeclared-selector")
            || action == @selector(_changeSpellingFromMenu:)
IGNORE_WARNINGS_END
            || action == @selector(checkSpelling:)
            || action == @selector(complete:)
            || action == @selector(pasteFont:))
        return [self _canEdit];

    if (action == @selector(showGuessPanel:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            BOOL panelShowing = [[[NSSpellChecker sharedSpellChecker] spellingPanel] isVisible];
            [menuItem setTitle:panelShowing
                ? UI_STRING_INTERNAL("Hide Spelling and Grammar", "menu item title")
                : UI_STRING_INTERNAL("Show Spelling and Grammar", "menu item title")];
        }
        return [self _canEdit];
    }
    
    if (action == @selector(changeBaseWritingDirection:)
            || action == @selector(makeBaseWritingDirectionLeftToRight:)
            || action == @selector(makeBaseWritingDirectionRightToLeft:)) {
        NSWritingDirection writingDirection;

        if (action == @selector(changeBaseWritingDirection:)) {
            writingDirection = static_cast<NSWritingDirection>([item tag]);
            if (writingDirection == NSWritingDirectionNatural)
                return NO;
        } else if (action == @selector(makeBaseWritingDirectionLeftToRight:))
            writingDirection = NSWritingDirectionLeftToRight;
        else
            writingDirection = NSWritingDirectionRightToLeft;

        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            String direction = writingDirection == NSWritingDirectionLeftToRight ? "ltr" : "rtl";
            [menuItem setState:frame->editor().selectionHasStyle(CSSPropertyDirection, direction)];
        }
        return [self _canEdit];
    }

    if (action == @selector(makeBaseWritingDirectionNatural:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:NSControlStateValueOff];
        return NO;
    }

    if (action == @selector(toggleBaseWritingDirection:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            // Take control of the title of the menu item instead of just checking/unchecking it because
            // a check would be ambiguous.
            [menuItem setTitle:frame->editor().selectionHasStyle(CSSPropertyDirection, "rtl")
                ? UI_STRING_INTERNAL("Left to Right", "Left to Right context menu item")
                : UI_STRING_INTERNAL("Right to Left", "Right to Left context menu item")];
        }
        return [self _canEdit];
    } 

    if (action == @selector(changeAttributes:)
            || action == @selector(changeColor:)        
            || action == @selector(changeFont:))
        return [self _canEditRichly];

    if (action == @selector(capitalizeWord:)
               || action == @selector(lowercaseWord:)
               || action == @selector(uppercaseWord:))
        return [self _hasSelection] && [self _isEditable];

    if (action == @selector(centerSelectionInVisibleArea:)
               || action == @selector(jumpToSelection:)
               || action == @selector(copyFont:))
        return [self _hasSelection] || ([self _isEditable] && [self _hasInsertionPoint]);

    if (action == @selector(changeDocumentBackgroundColor:))
        return [[self _webView] isEditable] && [self _canEditRichly];

IGNORE_WARNINGS_BEGIN("undeclared-selector")
    if (action == @selector(_ignoreSpellingFromMenu:)
            || action == @selector(_learnSpellingFromMenu:)
IGNORE_WARNINGS_END
            || action == @selector(takeFindStringFromSelection:))
        return [self _hasSelection];

    if (action == @selector(paste:) || action == @selector(pasteAsPlainText:))
        return frame && (frame->editor().canDHTMLPaste() || frame->editor().canPaste());

    if (action == @selector(pasteAsRichText:))
        return frame && (frame->editor().canDHTMLPaste()
            || (frame->editor().canPaste() && frame->selection().selection().isContentRichlyEditable()));

    if (action == @selector(performFindPanelAction:))
        return NO;

    if (action == @selector(_lookUpInDictionaryFromMenu:))
        return [self _hasSelection];

    if (action == @selector(stopSpeaking:))
        return [NSApp isSpeaking];

    if (action == @selector(toggleGrammarChecking:)) {
        // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must validate 
        // the selector here because we implement it here, and we must implement it here because the AppKit 
        // code checks the first responder.
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isGrammarCheckingEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
        return YES;
    }

    if (action == @selector(orderFrontSubstitutionsPanel:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            BOOL panelShowing = [[[NSSpellChecker sharedSpellChecker] substitutionsPanel] isVisible];
            [menuItem setTitle:panelShowing
                ? UI_STRING_INTERNAL("Hide Substitutions", "menu item title")
                : UI_STRING_INTERNAL("Show Substitutions", "menu item title")];
        }
        return [self _canEdit];
    }

    // FIXME 4799134: WebView is the bottleneck for this logic, but we must validate 
    // the selector here because we implement it here, and we must implement it here because the AppKit 
    // code checks the first responder.
    if (action == @selector(toggleSmartInsertDelete:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self smartInsertDeleteEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
        return [self _canEdit];
    }

    if (action == @selector(toggleAutomaticQuoteSubstitution:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isAutomaticQuoteSubstitutionEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
        return [self _canEdit];
    }

    if (action == @selector(toggleAutomaticLinkDetection:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isAutomaticLinkDetectionEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
        return [self _canEdit];
    }

    if (action == @selector(toggleAutomaticDashSubstitution:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isAutomaticDashSubstitutionEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
        return [self _canEdit];
    }

    if (action == @selector(toggleAutomaticTextReplacement:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isAutomaticTextReplacementEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
        return [self _canEdit];
    }

    if (action == @selector(toggleAutomaticSpellingCorrection:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isAutomaticSpellingCorrectionEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
        return [self _canEdit];
    }

    auto command = [self coreCommandBySelector:action];
    if (command.isSupported()) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:kit(command.state())];
        return command.isEnabled();
    }

    return YES;
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    // This can be called during teardown when _webView is nil. Return NO when this happens, because CallUIDelegateReturningBoolean
    // assumes the WebVIew is non-nil.
    if (![self _webView])
        return NO;
    BOOL result = [self validateUserInterfaceItemWithoutDelegate:item];
    return CallUIDelegateReturningBoolean(result, [self _webView], @selector(webView:validateUserInterfaceItem:defaultValidation:), item, result);
}

#endif // PLATFORM(MAC)

- (BOOL)acceptsFirstResponder
{
    // Don't accept first responder when we first click on this view.
    // We have to pass the event down through WebCore first to be sure we don't hit a subview.
    // Do accept first responder at any other time, for example from keyboard events,
    // or from calls back from WebCore once we begin mouse-down event handling.
#if PLATFORM(MAC)
    WebEvent *event = [NSApp currentEvent];
#else
    WebEvent *event = [WAKWindow currentEvent];
#endif
    if (event && event.type == WebEventMouseDown
            && !_private->handlingMouseDownEvent
            && NSPointInRect([event locationInWindow], [self convertRect:[self visibleRect] toView:nil]))
        return NO;
    return YES;
}

- (BOOL)maintainsInactiveSelection
{
#if USE(UIKIT_EDITING)
    // We want to maintain an inactive selection, when in editable content.
    if ([[self _webView] maintainsInactiveSelection])
        return YES;

    if ([[self window] _newFirstResponderAfterResigning] == self)
        return YES;
    
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selection().selection().isContentEditable();
#else
    // This method helps to determine whether the WebHTMLView should maintain
    // an inactive selection when it's not first responder.
    // Traditionally, these views have not maintained such selections,
    // clearing them when the view was not first responder. However,
    // to fix bugs like this one:
    // <rdar://problem/3672088>: "Editable WebViews should maintain a selection even 
    //                            when they're not firstResponder"
    // it was decided to add a switch to act more like an NSTextView.

    if ([[self _webView] maintainsInactiveSelection])
        return YES;

    // Predict the case where we are losing first responder status only to
    // gain it back again. Want to keep the selection in that case.
    id nextResponder = [[self window] _newFirstResponderAfterResigning];
    if ([nextResponder isKindOfClass:[NSScrollView class]]) {
        id contentView = [nextResponder contentView];
        if (contentView)
            nextResponder = contentView;
    }
    if ([nextResponder isKindOfClass:[NSClipView class]]) {
        id documentView = [nextResponder documentView];
        if (documentView)
            nextResponder = documentView;
    }
    if (nextResponder == self)
        return YES;

    Frame* coreFrame = core([self _frame]);
    bool selectionIsEditable = coreFrame && coreFrame->selection().selection().isContentEditable();
    bool nextResponderIsInWebView = [nextResponder isKindOfClass:[NSView class]]
        && [nextResponder isDescendantOf:[[[self _webView] mainFrame] frameView]];

    return selectionIsEditable && nextResponderIsInWebView;
#endif
}

#if PLATFORM(MAC)

- (void)addSuperviewObservers
{
    if (_private->observingSuperviewNotifications)
        return;

    NSView *superview = [self superview];
    if (!superview || ![self window])
        return;
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self selector:@selector(_frameOrBoundsChanged) name:NSViewFrameDidChangeNotification object:superview];
    [notificationCenter addObserver:self selector:@selector(_frameOrBoundsChanged) name:NSViewBoundsDidChangeNotification object:superview];
    
    // In addition to registering for frame/bounds change notifications, call -_frameOrBoundsChanged.
    // It will check the current scroll against the previous layout's scroll.  We need to
    // do this here to catch the case where the WebView is laid out at one size, removed from its
    // window, resized, and inserted into another window.  Our frame/bounds changed notifications
    // will not be sent in that situation, since we only watch for changes while in the view hierarchy.
    [self _frameOrBoundsChanged];
    
    _private->observingSuperviewNotifications = true;
}

- (void)addWindowObservers
{
    if (_private->observingWindowNotifications)
        return;
    
    NSWindow *window = [self window];
    if (!window)
        return;
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self selector:@selector(windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:nil];
    [notificationCenter addObserver:self selector:@selector(windowDidResignKey:) name:NSWindowDidResignKeyNotification object:nil];
    [notificationCenter addObserver:self selector:@selector(windowWillClose:) name:NSWindowWillCloseNotification object:window];
    
    _private->observingWindowNotifications = true;
}

- (void)viewWillMoveToSuperview:(NSView *)newSuperview
{
    [self _removeSuperviewObservers];
}

- (void)viewDidMoveToSuperview
{
    if ([self superview] != nil)
        [self addSuperviewObservers];

    if ([self superview] && [self _isUsingAcceleratedCompositing]) {
        WebView *webView = [self _webView];
        if ([webView _postsAcceleratedCompositingNotifications])
            [[NSNotificationCenter defaultCenter] postNotificationName:_WebViewDidStartAcceleratedCompositingNotification object:webView userInfo:nil];
    }
}

#endif // PLATFORM(MAC)

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (!_private)
        return;

#if PLATFORM(MAC)
    // FIXME: Some of these calls may not work because this view may be already removed from it's superview.
    [self _removeWindowObservers];
    [self _removeSuperviewObservers];
#endif

    // FIXME: This accomplishes the same thing as the call to setCanStartMedia(false) in
    // WebView. It would be nice to have a single mechanism instead of two.
    [[self _pluginController] stopAllPlugins];
}

- (void)viewDidMoveToWindow
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (!_private || _private->closed)
        return;
        
    [self _stopAutoscrollTimer];
    if ([self window]) {
        _private->lastScrollPosition = [[self superview] bounds].origin;
#if PLATFORM(MAC)
        [self addWindowObservers];
        [self addSuperviewObservers];
#endif

        // FIXME: This accomplishes the same thing as the call to setCanStartMedia(true) in
        // WebView. It would be nice to have a single mechanism instead of two.
        [[self _pluginController] startAllPlugins];

        _private->lastScrollPosition = NSZeroPoint;

#if PLATFORM(MAC)
        if (!_private->flagsChangedEventMonitor) {
            __block WebHTMLView *weakSelf = self;
            _private->flagsChangedEventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskFlagsChanged handler:^(NSEvent *flagsChangedEvent) {
                [weakSelf _postFakeMouseMovedEventForFlagsChangedEvent:flagsChangedEvent];
                return flagsChangedEvent;
            }];
        }
    } else {
        [NSEvent removeMonitor:_private->flagsChangedEventMonitor];
        _private->flagsChangedEventMonitor = nil;
#endif
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    [self _web_makePluginSubviewsPerformSelector:@selector(viewWillMoveToHostWindow:) withObject:hostWindow];
#endif
}

- (void)viewDidMoveToHostWindow
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    [self _web_makePluginSubviewsPerformSelector:@selector(viewDidMoveToHostWindow) withObject:nil];
#endif
}

- (void)addSubview:(NSView *)view
{
    [super addSubview:view];

    if ([WebPluginController isPlugInView:view]) {

#if PLATFORM(IOS_FAMILY)
        WebView *webView = [self _webView];
        [[webView _UIKitDelegateForwarder] webView:webView willAddPlugInView:view];
#endif

        [[self _pluginController] addPlugin:view];
    }
}

- (void)willRemoveSubview:(NSView *)subview
{
    if ([WebPluginController isPlugInView:subview])
        [[self _pluginController] destroyPlugin:subview];

    [super willRemoveSubview:subview];
}

- (void)reapplyStyles
{
#ifdef LOG_TIMES
    double start = CFAbsoluteTimeGetCurrent();
#endif

    if (Frame* coreFrame = core([self _frame])) {
        coreFrame->document()->styleScope().didChangeStyleSheetEnvironment();
        coreFrame->document()->updateStyleIfNeeded();
    }

#ifdef LOG_TIMES
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s apply style seconds = %f", [self URL], thisTime);
#endif
}

// Do a layout, but set up a new fixed width for the purposes of doing printing layout.
// minPageWidth==0 implies a non-printing layout
- (void)layoutToMinimumPageWidth:(float)minPageLogicalWidth height:(float)minPageLogicalHeight originalPageWidth:(float)originalPageWidth originalPageHeight:(float)originalPageHeight maximumShrinkRatio:(float)maximumShrinkRatio adjustingViewSize:(BOOL)adjustViewSize
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    if (coreFrame->document()) {
        if (coreFrame->document()->pageCacheState() != Document::NotInPageCache)
            return;
        coreFrame->document()->updateStyleIfNeeded();
    }

    if (![self _needsLayout])
        return;

#ifdef LOG_TIMES        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    LOG(View, "%@ doing layout", self);

    if (FrameView* coreView = coreFrame->view()) {
        if (minPageLogicalWidth > 0.0) {
            FloatSize pageSize(minPageLogicalWidth, minPageLogicalHeight);
            FloatSize originalPageSize(originalPageWidth, originalPageHeight);
            if (coreFrame->document() && coreFrame->document()->renderView() && !coreFrame->document()->renderView()->style().isHorizontalWritingMode()) {
                pageSize = FloatSize(minPageLogicalHeight, minPageLogicalWidth);
                originalPageSize = FloatSize(originalPageHeight, originalPageWidth);
            }
            coreView->forceLayoutForPagination(pageSize, originalPageSize, maximumShrinkRatio, adjustViewSize ? AdjustViewSize : DoNotAdjustViewSize);
        } else {
            coreView->forceLayout(!adjustViewSize);
            if (adjustViewSize)
                coreView->adjustViewSize();
        }
    }
    
#ifdef LOG_TIMES        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s layout seconds = %f", [self URL], thisTime);
#endif
}

- (void)layout
{
    [self layoutToMinimumPageWidth:0 height:0 originalPageWidth:0 originalPageHeight:0 maximumShrinkRatio:0 adjustingViewSize:NO];
}

#if PLATFORM(MAC)

// Deliver mouseup events to the DOM for button 2.
- (void)rightMouseUp:(NSEvent *)event
{
    // There's a chance that if we run a nested event loop the event will be released.
    // Retaining and then autoreleasing prevents that from causing a problem later here or
    // inside AppKit code.
    [[event retain] autorelease];

    [super rightMouseUp:event];

    if (Frame* coreframe = core([self _frame]))
        coreframe->eventHandler().mouseUp(event, [[self _webView] _pressureEvent]);
}

static BOOL isPreVersion3Client(void)
{
    static BOOL preVersion3Client = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_3_0_CONTEXT_MENU_TAGS);
    return preVersion3Client;
}

static BOOL isPreInspectElementTagClient(void)
{
    static BOOL preInspectElementTagClient = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_INSPECT_ELEMENT_MENU_TAG);
    return preInspectElementTagClient;
}

enum {
    // The next three values were used in WebKit 2.0 for SPI. In WebKit 3.0 these are API, with different values.
    OldWebMenuItemTagSearchInSpotlight = 1000,
    OldWebMenuItemTagSearchWeb,
    OldWebMenuItemTagLookUpInDictionary,
};

static RetainPtr<NSArray> fixMenusToSendToOldClients(NSMutableArray *defaultMenuItems)
{
    auto savedItems = adoptNS([[NSMutableArray alloc] init]);

    unsigned defaultItemsCount = [defaultMenuItems count];

    if (isPreInspectElementTagClient() && defaultItemsCount >= 2) {
        NSMenuItem *secondToLastItem = [defaultMenuItems objectAtIndex:defaultItemsCount - 2];
        NSMenuItem *lastItem = [defaultMenuItems objectAtIndex:defaultItemsCount - 1];

        if ([secondToLastItem isSeparatorItem] && [lastItem tag] == WebMenuItemTagInspectElement) {
            savedItems = adoptNS([[NSMutableArray alloc] initWithCapacity:2]);
            [savedItems addObject:secondToLastItem];
            [savedItems addObject:lastItem];

            [defaultMenuItems removeObject:secondToLastItem];
            [defaultMenuItems removeObject:lastItem];
            defaultItemsCount -= 2;
        }
    }

    BOOL preVersion3Client = isPreVersion3Client();
    if (!preVersion3Client)
        return savedItems;

    for (NSMenuItem *item in defaultMenuItems) {
        int tag = item.tag;
        int oldStyleTag = tag;

        if (tag >= WEBMENUITEMTAG_WEBKIT_3_0_SPI_START) {
            // Change all editing-related SPI tags listed in WebUIDelegatePrivate.h to WebMenuItemTagOther
            // to match our old WebKit context menu behavior.
            oldStyleTag = WebMenuItemTagOther;
        } else {
            // All items are expected to have useful tags coming into this method.
            ASSERT(tag != WebMenuItemTagOther);
            
            // Use the pre-3.0 tags for the few items that changed tags as they moved from SPI to API. We
            // do this only for old clients; new Mail already expects the new symbols in this case.
            if (preVersion3Client) {
                switch (tag) {
                case WebMenuItemTagSearchInSpotlight:
                    oldStyleTag = OldWebMenuItemTagSearchInSpotlight;
                    break;
                case WebMenuItemTagSearchWeb:
                    oldStyleTag = OldWebMenuItemTagSearchWeb;
                    break;
                case WebMenuItemTagLookUpInDictionary:
                    oldStyleTag = OldWebMenuItemTagLookUpInDictionary;
                    break;
                default:
                    break;
                }
            }
        }

        item.tag = oldStyleTag;
    }

    return savedItems;
}

static RetainPtr<NSArray> fixMenusReceivedFromOldClients(NSArray *delegateSuppliedItems, NSArray *savedItems)
{
    auto newMenuItems = adoptNS([delegateSuppliedItems mutableCopy]);

    if (savedItems)
        [newMenuItems addObjectsFromArray:savedItems];

    BOOL preVersion3Client = isPreVersion3Client();
    if (!preVersion3Client)
        return newMenuItems;
    
    // Restore the modern tags to the menu items whose tags we altered in fixMenusToSendToOldClients. 
    for (NSMenuItem *item in newMenuItems.get()) {
        int tag = [item tag];
        int modernTag = tag;
        
        if (tag == WebMenuItemTagOther) {
            // Restore the specific tag for items on which we temporarily set WebMenuItemTagOther to match old behavior.
            NSString *title = [item title];
            if ([title isEqualToString:contextMenuItemTagOpenLink()])
                modernTag = WebMenuItemTagOpenLink;
            else if ([title isEqualToString:contextMenuItemTagIgnoreGrammar()])
                modernTag = WebMenuItemTagIgnoreGrammar;
            else if ([title isEqualToString:contextMenuItemTagSpellingMenu()])
                modernTag = WebMenuItemTagSpellingMenu;
            else if ([title isEqualToString:contextMenuItemTagShowSpellingPanel(true)] || [title isEqualToString:contextMenuItemTagShowSpellingPanel(false)])
                modernTag = WebMenuItemTagShowSpellingPanel;
            else if ([title isEqualToString:contextMenuItemTagCheckSpelling()])
                modernTag = WebMenuItemTagCheckSpelling;
            else if ([title isEqualToString:contextMenuItemTagCheckSpellingWhileTyping()])
                modernTag = WebMenuItemTagCheckSpellingWhileTyping;
            else if ([title isEqualToString:contextMenuItemTagCheckGrammarWithSpelling()])
                modernTag = WebMenuItemTagCheckGrammarWithSpelling;
            else if ([title isEqualToString:contextMenuItemTagFontMenu()])
                modernTag = WebMenuItemTagFontMenu;
            else if ([title isEqualToString:contextMenuItemTagShowFonts()])
                modernTag = WebMenuItemTagShowFonts;
            else if ([title isEqualToString:contextMenuItemTagBold()])
                modernTag = WebMenuItemTagBold;
            else if ([title isEqualToString:contextMenuItemTagItalic()])
                modernTag = WebMenuItemTagItalic;
            else if ([title isEqualToString:contextMenuItemTagUnderline()])
                modernTag = WebMenuItemTagUnderline;
            else if ([title isEqualToString:contextMenuItemTagOutline()])
                modernTag = WebMenuItemTagOutline;
            else if ([title isEqualToString:contextMenuItemTagStyles()])
                modernTag = WebMenuItemTagStyles;
            else if ([title isEqualToString:contextMenuItemTagShowColors()])
                modernTag = WebMenuItemTagShowColors;
            else if ([title isEqualToString:contextMenuItemTagSpeechMenu()])
                modernTag = WebMenuItemTagSpeechMenu;
            else if ([title isEqualToString:contextMenuItemTagStartSpeaking()])
                modernTag = WebMenuItemTagStartSpeaking;
            else if ([title isEqualToString:contextMenuItemTagStopSpeaking()])
                modernTag = WebMenuItemTagStopSpeaking;
            else if ([title isEqualToString:contextMenuItemTagWritingDirectionMenu()])
                modernTag = WebMenuItemTagWritingDirectionMenu;
            else if ([title isEqualToString:contextMenuItemTagDefaultDirection()])
                modernTag = WebMenuItemTagDefaultDirection;
            else if ([title isEqualToString:contextMenuItemTagLeftToRight()])
                modernTag = WebMenuItemTagLeftToRight;
            else if ([title isEqualToString:contextMenuItemTagRightToLeft()])
                modernTag = WebMenuItemTagRightToLeft;
            else if ([title isEqualToString:contextMenuItemTagInspectElement()])
                modernTag = WebMenuItemTagInspectElement;
            else if ([title isEqualToString:contextMenuItemTagCorrectSpellingAutomatically()])
                modernTag = WebMenuItemTagCorrectSpellingAutomatically;
            else if ([title isEqualToString:contextMenuItemTagSubstitutionsMenu()])
                modernTag = WebMenuItemTagSubstitutionsMenu;
            else if ([title isEqualToString:contextMenuItemTagShowSubstitutions(true)] || [title isEqualToString:contextMenuItemTagShowSubstitutions(false)])
                modernTag = WebMenuItemTagShowSubstitutions;
            else if ([title isEqualToString:contextMenuItemTagSmartCopyPaste()])
                modernTag = WebMenuItemTagSmartCopyPaste;
            else if ([title isEqualToString:contextMenuItemTagSmartQuotes()])
                modernTag = WebMenuItemTagSmartQuotes;
            else if ([title isEqualToString:contextMenuItemTagSmartDashes()])
                modernTag = WebMenuItemTagSmartDashes;
            else if ([title isEqualToString:contextMenuItemTagSmartLinks()])
                modernTag = WebMenuItemTagSmartLinks;
            else if ([title isEqualToString:contextMenuItemTagTextReplacement()])
                modernTag = WebMenuItemTagTextReplacement;
            else if ([title isEqualToString:contextMenuItemTagTransformationsMenu()])
                modernTag = WebMenuItemTagTransformationsMenu;
            else if ([title isEqualToString:contextMenuItemTagMakeUpperCase()])
                modernTag = WebMenuItemTagMakeUpperCase;
            else if ([title isEqualToString:contextMenuItemTagMakeLowerCase()])
                modernTag = WebMenuItemTagMakeLowerCase;
            else if ([title isEqualToString:contextMenuItemTagCapitalize()])
                modernTag = WebMenuItemTagCapitalize;
            else {
            // We don't expect WebMenuItemTagOther for any items other than the ones we explicitly handle.
            // There's nothing to prevent an app from applying this tag, but they are supposed to only
            // use tags in the range starting with WebMenuItemBaseApplicationTag=10000
                ASSERT_NOT_REACHED();
            }
        } else if (preVersion3Client) {
            // Restore the new API tag for items on which we temporarily set the old SPI tag. The old SPI tag was
            // needed to avoid confusing clients linked against earlier WebKits; the new API tag is needed for
            // WebCore to handle the menu items appropriately (without needing to know about the old SPI tags).
            switch (tag) {
            case OldWebMenuItemTagSearchInSpotlight:
                modernTag = WebMenuItemTagSearchInSpotlight;
                break;
            case OldWebMenuItemTagSearchWeb:
                modernTag = WebMenuItemTagSearchWeb;
                break;
            case OldWebMenuItemTagLookUpInDictionary:
                modernTag = WebMenuItemTagLookUpInDictionary;
                break;
            default:
                break;
            }
        }
        
        if (modernTag != tag)
            [item setTag:modernTag];        
    }

    return newMenuItems;
}

static RetainPtr<NSMenuItem> createShareMenuItem(const HitTestResult& hitTestResult)
{
    auto items = adoptNS([[NSMutableArray alloc] init]);

    if (!hitTestResult.absoluteLinkURL().isEmpty()) {
        NSURL *absoluteLinkURL = hitTestResult.absoluteLinkURL();
        [items addObject:absoluteLinkURL];
    }

    if (!hitTestResult.absoluteMediaURL().isEmpty() && hitTestResult.isDownloadableMedia()) {
        NSURL *downloadableMediaURL = hitTestResult.absoluteMediaURL();
        [items addObject:downloadableMediaURL];
    }

    if (Image* image = hitTestResult.image()) {
        if (RefPtr<SharedBuffer> buffer = image->data())
            [items addObject:adoptNS([[NSImage alloc] initWithData:[NSData dataWithBytes:buffer->data() length:buffer->size()]]).get()];
    }

    if (!hitTestResult.selectedText().isEmpty()) {
        NSString *selectedText = hitTestResult.selectedText();
        [items addObject:selectedText];
    }

    if (![items count])
        return nil;

    return [NSMenuItem standardShareMenuItemForItems:items.get()];
}

static RetainPtr<NSMutableArray> createMenuItems(const HitTestResult&, const Vector<ContextMenuItem>&);

static RetainPtr<NSMenuItem> createMenuItem(const HitTestResult& hitTestResult, const ContextMenuItem& item)
{
    if (item.action() == ContextMenuItemTagShareMenu)
        return createShareMenuItem(hitTestResult);

    switch (item.type()) {
    case WebCore::ActionType:
    case WebCore::CheckableActionType: {
        auto menuItem = adoptNS([[NSMenuItem alloc] initWithTitle:item.title() action:@selector(forwardContextMenuAction:) keyEquivalent:@""]);

        if (auto tag = toTag(item.action()))
            [menuItem setTag:*tag];
        [menuItem setEnabled:item.enabled()];
        [menuItem setState:item.checked() ? NSControlStateValueOn : NSControlStateValueOff];
        [menuItem setTarget:[WebMenuTarget sharedMenuTarget]];

        return menuItem;
    }

    case SeparatorType:
        return [NSMenuItem separatorItem];

    case SubmenuType: {
        auto menu = adoptNS([[NSMenu alloc] init]);

        auto submenuItems = createMenuItems(hitTestResult, item.subMenuItems());
        for (NSMenuItem *menuItem in submenuItems.get())
            [menu addItem:menuItem];

        auto menuItem = adoptNS([[NSMenuItem alloc] initWithTitle:item.title() action:nullptr keyEquivalent:@""]);

        if (auto tag = toTag(item.action()))
            [menuItem setTag:*tag];
        [menuItem setEnabled:item.enabled()];
        [menuItem setSubmenu:menu.get()];

        return menuItem;
    }
    }
}

static RetainPtr<NSMutableArray> createMenuItems(const HitTestResult& hitTestResult, const Vector<ContextMenuItem>& items)
{
    auto menuItems = adoptNS([[NSMutableArray alloc] init]);

    for (auto& item : items) {
        if (auto menuItem = createMenuItem(hitTestResult, item))
            [menuItems addObject:menuItem.get()];
    }

    return menuItems;
}

static RetainPtr<NSArray> customMenuFromDefaultItems(WebView *webView, const ContextMenu& defaultMenu)
{
    const auto& hitTestResult = webView.page->contextMenuController().hitTestResult();
    auto defaultMenuItems = createMenuItems(hitTestResult, defaultMenu.items());

    id delegate = [webView UIDelegate];
    SEL selector = @selector(webView:contextMenuItemsForElement:defaultMenuItems:);
    if (![delegate respondsToSelector:selector])
        return defaultMenuItems;

    auto element = adoptNS([[WebElementDictionary alloc] initWithHitTestResult:hitTestResult]);

    BOOL preVersion3Client = isPreVersion3Client();
    if (preVersion3Client) {
        DOMNode *node = [element objectForKey:WebElementDOMNodeKey];
        if ([node isKindOfClass:[DOMHTMLInputElement class]] && [(DOMHTMLInputElement *)node _isTextField])
            return defaultMenuItems;
        if ([node isKindOfClass:[DOMHTMLTextAreaElement class]])
            return defaultMenuItems;
    }

    for (NSMenuItem *menuItem in defaultMenuItems.get()) {
        if (!menuItem.representedObject)
            menuItem.representedObject = element.get();
    }

    auto savedItems = fixMenusToSendToOldClients(defaultMenuItems.get());

    NSArray *delegateSuppliedItems = CallUIDelegate(webView, selector, element.get(), defaultMenuItems.get());

    return fixMenusReceivedFromOldClients(delegateSuppliedItems, savedItems.get());
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
    // There's a chance that if we run a nested event loop the event will be released.
    // Retaining and then autoreleasing prevents that from causing a problem later here or
    // inside AppKit code.
    [[event retain] autorelease];

    [_private->completionController endRevertingChange:NO moveLeft:NO];

    RefPtr<Frame> coreFrame = core([self _frame]);
    if (!coreFrame)
        return nil;

    Page* page = coreFrame->page();
    if (!page)
        return nil;

    // Match behavior of other browsers by sending a mousedown event for right clicks.
    _private->handlingMouseDownEvent = YES;
    page->contextMenuController().clearContextMenu();
    coreFrame->eventHandler().mouseDown(event, [[self _webView] _pressureEvent]);
    BOOL handledEvent = coreFrame->eventHandler().sendContextMenuEvent(PlatformEventFactory::createPlatformMouseEvent(event, [[self _webView] _pressureEvent], page->chrome().platformPageClient()));
    _private->handlingMouseDownEvent = NO;

    if (!handledEvent)
        return nil;

    // Re-get page, since it might have gone away during event handling.
    page = coreFrame->page();
    if (!page)
        return nil;

    ContextMenu* contextMenu = page->contextMenuController().contextMenu();
    if (!contextMenu)
        return nil;

    auto menuItems = customMenuFromDefaultItems([self _webView], *contextMenu);
    if (![menuItems count])
        return nil;

    auto menu = adoptNS([[NSMenu alloc] init]);

    for (NSMenuItem *item in menuItems.get()) {
        [menu addItem:item];

        if (item.tag == ContextMenuItemTagShareMenu) {
            ASSERT([item.representedObject isKindOfClass:[NSSharingServicePicker class]]);
#if ENABLE(SERVICE_CONTROLS)
            _private->currentSharingServicePickerController = adoptNS([[WebSharingServicePickerController alloc] initWithSharingServicePicker:item.representedObject client:static_cast<WebContextMenuClient&>(page->contextMenuController().client())]);
#endif
        }
    }

    [[WebMenuTarget sharedMenuTarget] setMenuController:&page->contextMenuController()];
    
    return menu.autorelease();
}

#endif // PLATFORM(MAC)

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    return [self searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapFlag startInSelection:NO];
}

- (void)clearFocus
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    Document* document = coreFrame->document();
    if (!document)
        return;
    
    document->setFocusedElement(0);
}

- (BOOL)isOpaque
{
    return [[self _webView] drawsBackground];
}

#if PLATFORM(MAC)

- (void)setLayer:(CALayer *)layer
{
    if (Frame* frame = core([self _frame])) {
        if (FrameView* view = frame->view())
            view->setPaintsEntireContents(layer);
    }

    [super setLayer:layer];
}

#endif

#if !LOG_DISABLED
- (void)setNeedsDisplay:(BOOL)flag
{
    LOG(View, "%@ setNeedsDisplay:%@", self, flag ? @"YES" : @"NO");
    [super setNeedsDisplay:flag];
}
#endif

static BOOL currentScrollIsBlit(NSView *clipView)
{
#if PLATFORM(MAC)
    return [clipView isKindOfClass:[WebClipView class]] && [(WebClipView *)clipView currentScrollIsBlit];
#else
    return NO;
#endif
}

// FIXME: this entire function could be #ifdeffed out on iOS. The below workaround is AppKit-specific.
- (void)setNeedsDisplayInRect:(NSRect)invalidRect
{
    if (_private->inScrollPositionChanged && currentScrollIsBlit([self superview])) {
        // When scrolling, the dirty regions are adjusted for the scroll only
        // after NSViewBoundsDidChangeNotification is sent. Translate the invalid
        // rect to pre-scrolled coordinates in order to get the right dirty region
        // after adjustment. See <rdar://problem/7678927>.
        NSPoint origin = [[self superview] bounds].origin;
        invalidRect.origin.x -= _private->lastScrollPosition.x - origin.x;
        invalidRect.origin.y -= _private->lastScrollPosition.y - origin.y;
    }
    [super setNeedsDisplayInRect:invalidRect];
}

- (void)setNeedsLayout: (BOOL)flag
{
    LOG(View, "%@ setNeedsLayout:%@", self, flag ? @"YES" : @"NO");
    if (!flag)
        return; // There's no way to say you don't need a layout.
    if (Frame* frame = core([self _frame])) {
        if (frame->document() && frame->document()->pageCacheState() != Document::NotInPageCache)
            return;
        if (FrameView* view = frame->view())
            view->setNeedsLayout();
    }
}

- (void)setNeedsToApplyStyles: (BOOL)flag
{
    LOG(View, "%@ setNeedsToApplyStyles:%@", self, flag ? @"YES" : @"NO");
    if (!flag)
        return; // There's no way to say you don't need a style recalc.
    if (Frame* frame = core([self _frame])) {
        if (frame->document() && frame->document()->pageCacheState() != Document::NotInPageCache)
            return;
        frame->document()->scheduleFullStyleRebuild();
    }
}

- (void)drawSingleRect:(NSRect)rect
{
#if PLATFORM(MAC)
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(rect);
        
    ASSERT([[self superview] isKindOfClass:[WebClipView class]]);

    [(WebClipView *)[self superview] setAdditionalClip:rect];

    @try {
        if ([self _transparentBackground]) {
            [[NSColor clearColor] set];
            NSRectFill (rect);
        }
#endif

        [[self _frame] _drawRect:rect contentsOnly:YES];

#if PLATFORM(MAC)
        WebView *webView = [self _webView];

        // This hack is needed for <rdar://problem/5023545>. We can hit a race condition where drawRect will be
        // called after the WebView has closed. If the client did not properly close the WebView and set the 
        // UIDelegate to nil, then the UIDelegate will be stale and this code will crash. 
        static BOOL version3OrLaterClient = WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_QUICKBOOKS_QUIRK);
        if (version3OrLaterClient)
            [[webView _UIDelegateForwarder] webView:webView didDrawRect:[webView convertRect:rect fromView:self]];
        // Clients don't need support for the didDrawRect delegate method above on iOS.
        // Also, a long time ago, when it was supported, it was part of a threading problem.

        if (WebNodeHighlight *currentHighlight = [webView currentNodeHighlight])
            [currentHighlight setNeedsUpdateInTargetViewRect:[self convertRect:rect toView:[currentHighlight targetView]]];

        [(WebClipView *)[self superview] resetAdditionalClip];
        [NSGraphicsContext restoreGraphicsState];
    } @catch (NSException *localException) {
        [(WebClipView *)[self superview] resetAdditionalClip];
        [NSGraphicsContext restoreGraphicsState];

        LOG_ERROR("Exception caught while drawing: %@", localException);
        [localException raise];
    }
#endif
}

- (void)drawRect:(NSRect)rect
{
    LOG(View, "%@ drawing", self);
    
    TraceScope scope(WebHTMLViewPaintStart, WebHTMLViewPaintEnd);

#if PLATFORM(MAC)
    const NSRect *rects;
    NSInteger count;
    [self getRectsBeingDrawn:&rects count:&count];

    BOOL subviewsWereSetAside = _private->subviewsSetAside;
    if (subviewsWereSetAside)
        [self _restoreSubviews];
#endif

#ifdef LOG_TIMES
    double start = CFAbsoluteTimeGetCurrent();
#endif

#if PLATFORM(MAC)
    // If count == 0 here, use the rect passed in for drawing. This is a workaround for: 
    // <rdar://problem/3908282> REGRESSION (Mail): No drag image dragging selected text in Blot and Mail 
    // The reason for the workaround is that this method is called explicitly from the code 
    // to generate a drag image, and at that time, getRectsBeingDrawn:count: will return a zero count. 
    const int cRectThreshold = 10; 
    const float cWastedSpaceThreshold = 0.75f; 
    BOOL useUnionedRect = (count <= 1) || (count > cRectThreshold); 
    if (!useUnionedRect) {
        // Attempt to guess whether or not we should use the unioned rect or the individual rects. 
        // We do this by computing the percentage of "wasted space" in the union.  If that wasted space 
        // is too large, then we will do individual rect painting instead. 
        float unionPixels = (rect.size.width * rect.size.height); 
        float singlePixels = 0; 
        for (int i = 0; i < count; ++i) 
            singlePixels += rects[i].size.width * rects[i].size.height; 
        float wastedSpace = 1 - (singlePixels / unionPixels); 
        if (wastedSpace <= cWastedSpaceThreshold) 
            useUnionedRect = YES; 
    }

    if (useUnionedRect) 
        [self drawSingleRect:rect];
    else {
        for (int i = 0; i < count; ++i)
            [self drawSingleRect:rects[i]];
    }
#else
    [self drawSingleRect:rect];    
#endif

#ifdef LOG_TIMES
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s draw seconds = %f", widget->part()->baseURL().URL().latin1(), thisTime);
#endif

#if PLATFORM(MAC)
    if (subviewsWereSetAside)
        [self _setAsideSubviews];
#endif

    WebView *webView = [self _webView];

#if PLATFORM(MAC)
    // Only do the synchronization dance if we're drawing into the window, otherwise
    // we risk disabling screen updates when no flush is pending.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if ([NSGraphicsContext currentContext] == [[self window] graphicsContext] && [webView _needsOneShotDrawingSynchronization]) {
        ALLOW_DEPRECATED_DECLARATIONS_END
        // Disable screen updates to minimize the chances of the race between the CA
        // display link and AppKit drawing causing flashes.
        [[self window] disableScreenUpdatesUntilFlush];
        
        // Make sure any layer changes that happened as a result of layout
        // via -viewWillDraw are committed.
        [CATransaction flush];
        [webView _setNeedsOneShotDrawingSynchronization:NO];
    }
#endif

    if (webView)
        CallUIDelegate(webView, @selector(webView:didDrawFrame:), [self _frame]);
}

#if PLATFORM(MAC)

// Turn off the additional clip while computing our visibleRect.
- (NSRect)visibleRect
{
    if (!([[self superview] isKindOfClass:[WebClipView class]]))
        return [super visibleRect];

    WebClipView *clipView = (WebClipView *)[self superview];

    if (![clipView hasAdditionalClip])
        return [super visibleRect];

    NSRect additionalClip = [clipView additionalClip];
    [clipView resetAdditionalClip];
    NSRect visibleRect = [super visibleRect];
    [clipView setAdditionalClip:additionalClip];
    return visibleRect;
}

- (void)_invalidateGStatesForTree
{
    // AppKit is in the process of traversing the NSView tree, and is going to send -renewGState to
    // descendants, including plug-in views. This can result in calls out to plug-in code and back into
    // WebCore via JavaScript, which could normally mutate the NSView tree while it is being traversed.
    // Defer those mutations while descendants are being traveresed.
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
    [super _invalidateGStatesForTree];
}

- (BOOL)isFlipped 
{
    return YES;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:notification waitUntilDone:NO];
        return;
    }

    NSWindow *keyWindow = [notification object];

    if (keyWindow == [self window])
        [self _updateSecureInputState];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:notification waitUntilDone:NO];
        return;
    }

    NSWindow *formerKeyWindow = [notification object];

    if (formerKeyWindow == [self window] || formerKeyWindow == [[self window] attachedSheet]) {
        [self _updateSecureInputState];
        [_private->completionController endRevertingChange:NO moveLeft:NO];
    }
}

- (void)windowWillClose:(NSNotification *)notification
{
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:notification waitUntilDone:NO];
        return;
    }

    [_private->completionController endRevertingChange:NO moveLeft:NO];
    [[self _pluginController] destroyAllPlugins];
}

#endif // PLATFORM(MAC)

- (void)scrollWheel:(WebEvent *)event
{
#if PLATFORM(MAC)
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    // FIXME: Why not do this on iOS too?
    [[event retain] autorelease];
#endif

    Frame* frame = core([self _frame]);
    if (!frame || !frame->eventHandler().wheelEvent(event)) {
#if PLATFORM(MAC)
        [super scrollWheel:event];
#endif
    }

#if PLATFORM(MAC)
    [[[self _webView] _immediateActionController] webView:[self _webView] didHandleScrollWheel:event];
#endif
}

- (BOOL)_isSelectionEvent:(WebEvent *)event
{
#if PLATFORM(MAC)
    bool allowShadowContent = true;
#else
    bool allowShadowContent = false; // FIXME: Why does this need to be false on iOS?
#endif

    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    return [[[self elementAtPoint:point allowShadowContent:allowShadowContent] objectForKey:WebElementIsSelectedKey] boolValue];
}

#if PLATFORM(MAC)

- (BOOL)_isScrollBarEvent:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    return [[[self elementAtPoint:point allowShadowContent:YES] objectForKey:WebElementIsInScrollBarKey] boolValue];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    NSView *hitView = [self _hitViewForEvent:event];
    WebHTMLView *hitHTMLView = [hitView isKindOfClass:[self class]] ? (WebHTMLView *)hitView : nil;
    
#if ENABLE(DASHBOARD_SUPPORT)
    if ([[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysAcceptsFirstMouse])
        return YES;
#endif
    
    if (hitHTMLView) {
        bool result = false;
        if (Frame* coreFrame = core([hitHTMLView _frame])) {
            coreFrame->eventHandler().setActivationEventNumber([event eventNumber]);
            [hitHTMLView _setMouseDownEvent:event];
            if ([hitHTMLView _isSelectionEvent:event]) {
#if ENABLE(DRAG_SUPPORT)
                if (Page* page = coreFrame->page())
                    result = coreFrame->eventHandler().eventMayStartDrag(PlatformEventFactory::createPlatformMouseEvent(event, [[self _webView] _pressureEvent], page->chrome().platformPageClient()));
#endif
            } else if ([hitHTMLView _isScrollBarEvent:event])
                result = true;
            [hitHTMLView _setMouseDownEvent:nil];
        }
        return result;
    }
    return [hitView acceptsFirstMouse:event];
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    NSView *hitView = [self _hitViewForEvent:event];
    WebHTMLView *hitHTMLView = [hitView isKindOfClass:[self class]] ? (WebHTMLView *)hitView : nil;
    if (hitHTMLView) {
        bool result = false;
        if ([hitHTMLView _isSelectionEvent:event]) {
            [hitHTMLView _setMouseDownEvent:event];
#if ENABLE(DRAG_SUPPORT)
            if (Frame* coreFrame = core([hitHTMLView _frame])) {
                if (Page* page = coreFrame->page())
                    result = coreFrame->eventHandler().eventMayStartDrag(PlatformEventFactory::createPlatformMouseEvent(event, [[self _webView] _pressureEvent], page->chrome().platformPageClient()));
            }
#endif
            [hitHTMLView _setMouseDownEvent:nil];
        }
        return result;
    }
    return [hitView shouldDelayWindowOrderingForEvent:event];
}

#endif // PLATFORM(MAC)

- (void)mouseDown:(WebEvent *)event
{
    [[self _webView] prepareForMouseDown];

#if PLATFORM(MAC)
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];
#endif

    RetainPtr<WebHTMLView> protector = self;

#if PLATFORM(MAC)
    if ([[self inputContext] wantsToHandleMouseEvents] && [[self inputContext] handleMouseEvent:event])
        return;
#endif

    _private->handlingMouseDownEvent = YES;

    // Record the mouse down position so we can determine drag hysteresis.
    [self _setMouseDownEvent:event];

#if PLATFORM(IOS_FAMILY)
    // TEXTINPUT: if there is marked text and the current input
    // manager wants to handle mouse events, we need to make sure to
    // pass it to them. If not, then we need to notify the input
    // manager when the marked text is abandoned (user clicks outside
    // the marked area)
    _private->ignoringMouseDraggedEvents = NO;

    // Let WebCore get a chance to deal with the event. This will call back to us
    // to start the autoscroll timer if appropriate.
    if (Frame* coreframe = core([self _frame]))
        coreframe->eventHandler().mouseDown(event);
#else
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSInputManager *currentInputManager = [NSInputManager currentInputManager];

    if (![currentInputManager wantsToHandleMouseEvents] || ![currentInputManager handleMouseEvent:event]) {
        ALLOW_DEPRECATED_DECLARATIONS_END
        [_private->completionController endRevertingChange:NO moveLeft:NO];

        // If the web page handles the context menu event and menuForEvent: returns nil, we'll get control click events here.
        // We don't want to pass them along to KHTML a second time.
        if (!([event modifierFlags] & NSEventModifierFlagControl)) {
            _private->ignoringMouseDraggedEvents = NO;

            // Let WebCore get a chance to deal with the event. This will call back to us
            // to start the autoscroll timer if appropriate.
            if (Frame* coreframe = core([self _frame]))
                coreframe->eventHandler().mouseDown(event, [[self _webView] _pressureEvent]);
        }
    }
#endif

    _private->handlingMouseDownEvent = NO;
}

#if ENABLE(TOUCH_EVENTS)

- (void)touch:(WebEvent *)event
{
    RetainPtr<WebHTMLView> protector = self;

    // Let WebCore get a chance to deal with the event. This will call back to us
    // to start the autoscroll timer if appropriate.
    if (Frame* coreframe = core([self _frame]))
        coreframe->eventHandler().touchEvent(event);
}

#endif

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)dragImage:(NSImage *)dragImage
               at:(NSPoint)at
           offset:(NSSize)offset
            event:(NSEvent *)event
       pasteboard:(NSPasteboard *)pasteboard
           source:(id)source
        slideBack:(BOOL)slideBack
IGNORE_WARNINGS_END
{
    ASSERT(self == [self _topHTMLView]);
    [pasteboard setString:@"" forType:[WebHTMLView _dummyPasteboardType]];

    [super dragImage:dragImage at:at offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
}

- (void)mouseDragged:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        return;
    ALLOW_DEPRECATED_DECLARATIONS_END

    [self retain];

    if (!_private->ignoringMouseDraggedEvents) {
        if (Frame* frame = core([self _frame])) {
            if (Page* page = frame->page())
                page->mainFrame().eventHandler().mouseDragged(event, [[self _webView] _pressureEvent]);
        }
    }

    [self release];
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal
IGNORE_WARNINGS_END
{
    ASSERT(![self _webView] || [self _isTopHTMLView]);
    
    Page* page = core([self _webView]);
    if (!page)
        return NSDragOperationNone;

    return (NSDragOperation)page->dragController().sourceDragOperation();
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
IGNORE_WARNINGS_END
{
    ASSERT(![self _webView] || [self _isTopHTMLView]);
    
    NSPoint windowImageLoc = [[self window] convertScreenToBase:aPoint];
    NSPoint windowMouseLoc = windowImageLoc;
    
    if (Page* page = core([self _webView])) {
        windowMouseLoc = NSMakePoint(windowImageLoc.x + page->dragController().dragOffset().x(), windowImageLoc.y + page->dragController().dragOffset().y());
        page->dragController().dragEnded();
    }
    
    [[self _frame] _dragSourceEndedAt:windowMouseLoc operation:operation];
    
    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    _private->ignoringMouseDraggedEvents = YES;
    
    // Once the dragging machinery kicks in, we no longer get mouse drags or the up event.
    // WebCore expects to get balanced down/up's, so we must fake up a mouseup.
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
                                            location:windowMouseLoc
                                       modifierFlags:[[NSApp currentEvent] modifierFlags]
                                           timestamp:[NSDate timeIntervalSinceReferenceDate]
                                        windowNumber:[[self window] windowNumber]
                                             context:nullptr
                                         eventNumber:0 clickCount:0 pressure:0];
    [self mouseUp:fakeEvent]; // This will also update the mouseover state.
}

static bool matchesExtensionOrEquivalent(NSString *filename, NSString *extension)
{
    NSString *extensionAsSuffix = [@"." stringByAppendingString:extension];
    return [filename _webkit_hasCaseInsensitiveSuffix:extensionAsSuffix]
    || ([extension _webkit_isCaseInsensitiveEqualToString:@"jpeg"]
        && [filename _webkit_hasCaseInsensitiveSuffix:@".jpg"]);
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
IGNORE_WARNINGS_END
{
    NSFileWrapper *wrapper = nil;
    NSURL *draggingElementURL = nil;
    
    if (auto* tiffResource = _private->promisedDragTIFFDataSource) {
        if (auto* buffer = tiffResource->resourceBuffer()) {
            NSURLResponse *response = tiffResource->response().nsURLResponse();
            draggingElementURL = [response URL];
            wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:buffer->createNSData().get()] autorelease];
            NSString* filename = [response suggestedFilename];
            NSString* trueExtension(tiffResource->image()->filenameExtension());
            if (!matchesExtensionOrEquivalent(filename, trueExtension))
                filename = [[filename stringByAppendingString:@"."] stringByAppendingString:trueExtension];
            [wrapper setPreferredFilename:filename];
        }
    }
    
    if (!wrapper) {
        ASSERT(![self _webView] || [self _isTopHTMLView]);
        Page* page = core([self _webView]);
        
        //If a load occurs midway through a drag, the view may be detached, which gives
        //us no ability to get to the original Page, so we cannot access any drag state
        //FIXME: is there a way to recover?
        if (!page) 
            return nil; 
        
        const URL& imageURL = page->dragController().draggingImageURL();
        if (!imageURL.isEmpty())
            draggingElementURL = imageURL;

        wrapper = [[self _dataSource] _fileWrapperForURL:draggingElementURL];
    }
    
    if (wrapper == nil) {
        LOG_ERROR("Failed to create image file.");
        return nil;
    }

    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[wrapper preferredFilename]];
    path = [[NSFileManager defaultManager] _webkit_pathWithUniqueFilenameForPath:path];
    if (![wrapper writeToURL:[NSURL fileURLWithPath:path isDirectory:NO] options:NSFileWrapperWritingWithNameUpdating originalContentsURL:nil error:nullptr])
        LOG_ERROR("Failed to create image file via -[NSFileWrapper writeToURL:options:originalContentsURL:error:]");
    
    if (draggingElementURL)
        [[NSFileManager defaultManager] _webkit_setMetadataURL:[draggingElementURL absoluteString] referrer:nil atPath:path];
    
    return [NSArray arrayWithObject:[path lastPathComponent]];
}

// MARK: NSDraggingSource

- (NSDragOperation)draggingSession:(NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
    ASSERT(![self _webView] || [self _isTopHTMLView]);

    Page* page = core([self _webView]);
    if (!page)
        return NSDragOperationNone;

    return (NSDragOperation)page->dragController().sourceDragOperation();
}

- (void)draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)screenPoint operation:(NSDragOperation)operation
{
    ASSERT(![self _webView] || [self _isTopHTMLView]);

    NSPoint windowLocation = [self.window convertRectFromScreen:{ screenPoint, NSZeroSize }].origin;

    if (Page* page = core([self _webView]))
        page->dragController().dragEnded();

    [[self _frame] _dragSourceEndedAt:windowLocation operation:operation];

    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    _private->ignoringMouseDraggedEvents = YES;

    // Once the dragging machinery kicks in, we no longer get mouse drags or the up event.
    // WebCore expects to get balanced down/up's, so we must fake up a mouseup.
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:windowLocation modifierFlags:[NSApp currentEvent].modifierFlags timestamp:[NSDate timeIntervalSinceReferenceDate] windowNumber:self.window. windowNumber context:nullptr eventNumber:0 clickCount:0 pressure:0];

    // This will also update the mouseover state.
    [self mouseUp:fakeEvent];
}

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)

- (void)mouseUp:(WebEvent *)event
{
    [[self _webView] prepareForMouseUp];

#if PLATFORM(MAC)
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];
#endif

    [self _setMouseDownEvent:nil];

#if PLATFORM(MAC)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        return;
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif

    [self retain];

    [self _stopAutoscrollTimer];
    if (Frame* frame = core([self _frame])) {
        if (Page* page = frame->page()) {
#if PLATFORM(IOS_FAMILY)
            page->mainFrame().eventHandler().mouseUp(event);
#else
            page->mainFrame().eventHandler().mouseUp(event, [[self _webView] _pressureEvent]);
#endif
        }
    }

#if PLATFORM(MAC)
    [self _updateMouseoverWithFakeEvent];
#endif

    [self release];
}

#if PLATFORM(MAC)

- (void)mouseMoved:(NSEvent *)event
{
    [self _updateMouseoverWithEvent:event];
}

#endif

- (void)pressureChangeWithEvent:(NSEvent *)event
{
#if PLATFORM(MAC) && defined(__LP64__)
    NSEvent *lastPressureEvent = [[self _webView] _pressureEvent];
    if (event.phase != NSEventPhaseChanged && event.phase != NSEventPhaseBegan && event.phase != NSEventPhaseEnded)
        return;

    RefPtr<Frame> coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    coreFrame->eventHandler().pressureChange(event, lastPressureEvent);
    [[self _webView] _setPressureEvent:event];
#endif
}

#if PLATFORM(MAC)

// returning YES from this method is the way we tell AppKit that it is ok for this view
// to be in the key loop even when "tab to all controls" is not on.
- (BOOL)needsPanelToBecomeKey
{
    return YES;
}

#endif

// Utility function to make sure we don't return anything through the NSTextInput
// API when an editable region is not currently focused.
static BOOL isTextInput(Frame* coreFrame)
{
    if (!coreFrame)
        return NO;
    const VisibleSelection& selection = coreFrame->selection().selection();
    return !selection.isNone() && selection.isContentEditable();
}

#if PLATFORM(MAC)

static BOOL isInPasswordField(Frame* coreFrame)
{
    return coreFrame && coreFrame->selection().selection().isInPasswordField();
}

#endif

static RefPtr<KeyboardEvent> currentKeyboardEvent(Frame* coreFrame)
{
#if PLATFORM(MAC)
    NSEvent *event = [NSApp currentEvent];
    if (!event)
        return nullptr;

    switch ([event type]) {
    case NSEventTypeKeyDown: {
        PlatformKeyboardEvent platformEvent = PlatformEventFactory::createPlatformKeyboardEvent(event);
        platformEvent.disambiguateKeyDownEvent(PlatformEvent::RawKeyDown);
        return KeyboardEvent::create(platformEvent, &coreFrame->windowProxy());
    }
    case NSEventTypeKeyUp:
        return KeyboardEvent::create(PlatformEventFactory::createPlatformKeyboardEvent(event), &coreFrame->windowProxy());
    default:
        return nullptr;
    }
#else
    WebEvent *event = [WAKWindow currentEvent];
    if (!event)
        return nullptr;
    WebEventType type = event.type;
    if (type == WebEventKeyDown || type == WebEventKeyUp) {
        Document* document = coreFrame->document();
        return KeyboardEvent::create(PlatformEventFactory::createPlatformKeyboardEvent(event), document ? document->windowProxy() : 0);
    }
    return nullptr;
#endif
}

- (BOOL)becomeFirstResponder
{
    NSSelectionDirection direction = NSDirectSelection;
    if (![[self _webView] _isPerformingProgrammaticFocus])
        direction = [[self window] keyViewSelectionDirection];

#if PLATFORM(MAC)
    [self _updateFontPanel];
#endif
    
    Frame* frame = core([self _frame]);
    if (!frame)
        return YES;

#if PLATFORM(MAC)
    BOOL exposeInputContext = isTextInput(frame) && !isInPasswordField(frame);
    if (exposeInputContext != _private->exposeInputContext) {
        _private->exposeInputContext = exposeInputContext;
        [NSApp updateWindows];
    }

    _private->_forceUpdateSecureInputState = YES;
    [self _updateSecureInputState];
    _private->_forceUpdateSecureInputState = NO;
#endif

    // FIXME: Kill ring handling is mostly in WebCore, so this call should also be moved there.
    frame->editor().setStartNewKillRingSequence(true);

    Page* page = frame->page();
    if (!page)
        return YES;

    if (![[self _webView] _isPerformingProgrammaticFocus])
        page->focusController().setFocusedFrame(frame);

    page->focusController().setFocused(true);

    if (direction == NSDirectSelection)
        return YES;

    if (Document* document = frame->document())
        document->setFocusedElement(0);
    page->focusController().setInitialFocus(direction == NSSelectingNext ? FocusDirectionForward : FocusDirectionBackward,
                                             currentKeyboardEvent(frame).get());
    return YES;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
#if PLATFORM(MAC)
        if (_private->isInSecureInputState) {
            DisableSecureEventInput();
            _private->isInSecureInputState = NO;
        }
        [_private->completionController endRevertingChange:NO moveLeft:NO];
#endif
        Frame* coreFrame = core([self _frame]);
        if (!coreFrame)
            return resign;

#if PLATFORM(IOS_FAMILY)
        if (Document* document = coreFrame->document()) {
            document->markers().removeMarkers(DocumentMarker::DictationPhraseWithAlternatives);
            document->markers().removeMarkers(DocumentMarker::DictationResult);
        }
#endif

        Page* page = coreFrame->page();
        if (!page)
            return resign;
        if (![self maintainsInactiveSelection]) { 
            [self deselectAll];
            if (![[self _webView] _isPerformingProgrammaticFocus])
                [self clearFocus];
        }
        
        id nextResponder = [[self window] _newFirstResponderAfterResigning];
        bool nextResponderIsInWebView = [nextResponder isKindOfClass:[NSView class]]
            && [nextResponder isDescendantOf:[[[self _webView] mainFrame] frameView]];
        if (!nextResponderIsInWebView)
            page->focusController().setFocused(false);
    }
    return resign;
}

- (void)setDataSource:(WebDataSource *)dataSource 
{
    ASSERT(dataSource);
    if (_private->dataSource == dataSource)
        return;

    ASSERT(!_private->closed);

    _private->dataSource = dataSource;
    [_private->pluginController setDataSource:dataSource];

#if PLATFORM(MAC)
    if (!_private->installedTrackingArea) {
        NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingCursorUpdate;
        if (_NSRecommendedScrollerStyle() == NSScrollerStyleLegacy
#if ENABLE(DASHBOARD_SUPPORT)
            || [[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows]
#endif
            )
            options |= NSTrackingActiveAlways;
        else
            options |= NSTrackingActiveInKeyWindow;

        [self addTrackingArea:adoptNS([[NSTrackingArea alloc] initWithRect:[self frame] options:options owner:self userInfo:nil]).get()];
        _private->installedTrackingArea = YES;
    }
#endif
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

#if PLATFORM(MAC)

// This is an override of an NSControl method that wants to repaint the entire view when the window resigns/becomes key.
// WebHTMLView is an NSControl only because it hosts NSCells that are painted by WebCore's Aqua theme
// renderer (and those cells must be hosted by an enclosing NSControl in order to paint properly).
- (void)updateCell:(NSCell *)cell
{
}

#endif

// Does setNeedsDisplay:NO as a side effect when printing is ending.
// pageWidth != 0 implies we will relayout to a new width
- (void)_setPrinting:(BOOL)printing minimumPageLogicalWidth:(float)minPageLogicalWidth logicalHeight:(float)minPageLogicalHeight originalPageWidth:(float)originalPageWidth originalPageHeight:(float)originalPageHeight maximumShrinkRatio:(float)maximumShrinkRatio adjustViewSize:(BOOL)adjustViewSize paginateScreenContent:(BOOL)paginateScreenContent
{
    if (printing == _private->printing && paginateScreenContent == _private->paginateScreenContent)
        return;

    for (WebFrame *subframe in [[self _frame] childFrames]) {
        WebFrameView *frameView = [subframe frameView];
        if ([[subframe _dataSource] _isDocumentHTML]) {
            [(WebHTMLView *)[frameView documentView] _setPrinting:printing minimumPageLogicalWidth:0 logicalHeight:0 originalPageWidth:0 originalPageHeight:0 maximumShrinkRatio:0 adjustViewSize:adjustViewSize paginateScreenContent:paginateScreenContent];
        }
    }

    _private->pageRects = nil;
    _private->printing = printing;
    _private->paginateScreenContent = paginateScreenContent;
    
    Frame* coreFrame = core([self _frame]);
    if (coreFrame) {
        if (FrameView* coreView = coreFrame->view())
            coreView->setMediaType(_private->printing ? "print" : "screen");
        if (Document* document = coreFrame->document()) {
            // In setting printing, we should not validate resources already cached for the document.
            // See https://bugs.webkit.org/show_bug.cgi?id=43704
            ResourceCacheValidationSuppressor validationSuppressor(document->cachedResourceLoader());

            document->setPaginatedForScreen(_private->paginateScreenContent);
            document->setPrinting(_private->printing);
            document->styleScope().didChangeStyleSheetEnvironment();
        }
    }

    [self setNeedsLayout:YES];
    [self layoutToMinimumPageWidth:minPageLogicalWidth height:minPageLogicalHeight originalPageWidth:originalPageWidth originalPageHeight:originalPageHeight maximumShrinkRatio:maximumShrinkRatio adjustingViewSize:adjustViewSize];
    if (!printing) {
        // Can't do this when starting printing or nested printing won't work, see 3491427.
        [self setNeedsDisplay:NO];
    }
}

#if PLATFORM(MAC)

- (BOOL)canPrintHeadersAndFooters
{
    return YES;
}

// This is needed for the case where the webview is embedded in the view that's being printed.
// It shouldn't be called when the webview is being printed directly.
- (void)adjustPageHeightNew:(CGFloat *)newBottom top:(CGFloat)oldTop bottom:(CGFloat)oldBottom limit:(CGFloat)bottomLimit
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    if (!wasInPrintingMode)
        [self _setPrinting:YES minimumPageLogicalWidth:0 logicalHeight:0 originalPageWidth:0 originalPageHeight:0 maximumShrinkRatio:0 adjustViewSize:NO paginateScreenContent:[self _isInScreenPaginationMode]];

    *newBottom = [self _adjustedBottomOfPageWithTop:oldTop bottom:oldBottom limit:bottomLimit];

    if (!wasInPrintingMode) {
        NSPrintOperation *currenPrintOperation = [NSPrintOperation currentOperation];
        if (currenPrintOperation)
            // delay _setPrinting:NO until back to main loop as this method may get called repeatedly
            [self performSelector:@selector(_delayedEndPrintMode:) withObject:currenPrintOperation afterDelay:0];
        else
            // not sure if this is actually ever invoked, it probably shouldn't be
            [self _setPrinting:NO minimumPageLogicalWidth:0 logicalHeight:0 originalPageWidth:0 originalPageHeight:0 maximumShrinkRatio:0 adjustViewSize:NO paginateScreenContent:[self _isInScreenPaginationMode]];
    }
}

- (float)_scaleFactorForPrintOperation:(NSPrintOperation *)printOperation
{
    bool useViewWidth = true;
    Frame* coreFrame = core([self _frame]);
    if (coreFrame) {
        Document* document = coreFrame->document();
        if (document && document->renderView())
            useViewWidth = document->renderView()->style().isHorizontalWritingMode();
    }

    float viewLogicalWidth = useViewWidth ? NSWidth([self bounds]) : NSHeight([self bounds]);
    if (viewLogicalWidth < 1) {
        LOG_ERROR("%@ has no logical width when printing", self);
        return 1.0f;
    }

    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    float maxShrinkToFitScaleFactor = 1.0f / _WebHTMLViewPrintingMaximumShrinkFactor;
    float shrinkToFitScaleFactor = (useViewWidth ? [printOperation _web_availablePaperWidth] :  [printOperation _web_availablePaperHeight]) / viewLogicalWidth;
    return userScaleFactor * std::max(maxShrinkToFitScaleFactor, shrinkToFitScaleFactor);
}

// FIXME 3491344: This is a secret AppKit-internal method that we need to override in order
// to get our shrink-to-fit to work with a custom pagination scheme. We can do this better
// if AppKit makes it SPI/API.
- (CGFloat)_provideTotalScaleFactorForPrintOperation:(NSPrintOperation *)printOperation 
{
    return [self _scaleFactorForPrintOperation:printOperation];
}

// This is used for Carbon printing. At some point we might want to make this public API.
- (void)setPageWidthForPrinting:(float)pageWidth
{
    [self _setPrinting:NO minimumPageLogicalWidth:0 logicalHeight:0 originalPageWidth:0 originalPageHeight:0 maximumShrinkRatio:0 adjustViewSize:NO paginateScreenContent:[self _isInScreenPaginationMode]];
    [self _setPrinting:YES minimumPageLogicalWidth:pageWidth logicalHeight:0 originalPageWidth:0 originalPageHeight:0 maximumShrinkRatio:1 adjustViewSize:YES paginateScreenContent:[self _isInScreenPaginationMode]];
}

- (void)_endPrintModeAndRestoreWindowAutodisplay
{
    [self _endPrintMode];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[self window] setAutodisplay:YES];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (void)_delayedEndPrintMode:(NSPrintOperation *)initiatingOperation
{
    ASSERT_ARG(initiatingOperation, initiatingOperation != nil);
    NSPrintOperation *currentOperation = [NSPrintOperation currentOperation];
    if (initiatingOperation == currentOperation) {
        // The print operation is still underway. We don't expect this to ever happen, hence the assert, but we're
        // being extra paranoid here since the printing code is so fragile. Delay the cleanup
        // further.
        ASSERT_NOT_REACHED();
        [self performSelector:@selector(_delayedEndPrintMode:) withObject:initiatingOperation afterDelay:0];
    } else if ([currentOperation view] == self) {
        // A new print job has started, but it is printing the same WebHTMLView again. We don't expect
        // this to ever happen, hence the assert, but we're being extra paranoid here since the printing code is so
        // fragile. Do nothing, because we don't want to break the print job currently in progress, and
        // the print job currently in progress is responsible for its own cleanup.
        ASSERT_NOT_REACHED();
    } else {
        // The print job that kicked off this delayed call has finished, and this view is not being
        // printed again. We expect that no other print job has started. Since this delayed call wasn't
        // cancelled, beginDocument and endDocument must not have been called, and we need to clean up
        // the print mode here.
        ASSERT(currentOperation == nil);
        [self _endPrintModeAndRestoreWindowAutodisplay];
    }
}

// Return the number of pages available for printing
- (BOOL)knowsPageRange:(NSRangePointer)range
{
    // Must do this explicit display here, because otherwise the view might redisplay while the print
    // sheet was up, using printer fonts (and looking different).
    [self displayIfNeeded];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[self window] setAutodisplay:NO];
    ALLOW_DEPRECATED_DECLARATIONS_END

    [[self _webView] _adjustPrintingMarginsForHeaderAndFooter];
    NSPrintOperation *printOperation = [NSPrintOperation currentOperation];
    if (![self _beginPrintModeWithPageWidth:[printOperation _web_availablePaperWidth] height:[printOperation _web_availablePaperHeight] shrinkToFit:YES])
        return NO;

    // Certain types of errors, including invalid page ranges, can cause beginDocument and
    // endDocument to be skipped after we've put ourselves in print mode (see 4145905). In those cases
    // we need to get out of print mode without relying on any more callbacks from the printing mechanism.
    // If we get as far as beginDocument without trouble, then this delayed request will be cancelled.
    // If not cancelled, this delayed call will be invoked in the next pass through the main event loop,
    // which is after beginDocument and endDocument would be called.
    [self performSelector:@selector(_delayedEndPrintMode:) withObject:printOperation afterDelay:0];
    
    // There is a theoretical chance that someone could do some drawing between here and endDocument,
    // if something caused setNeedsDisplay after this point. If so, it's not a big tragedy, because
    // you'd simply see the printer fonts on screen. As of this writing, this does not happen with Safari.

    range->location = 1;
    float totalScaleFactor = [self _scaleFactorForPrintOperation:printOperation];
    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    float fullPageWidth = floorf([printOperation _web_availablePaperWidth] / totalScaleFactor);
    float fullPageHeight = floorf([printOperation _web_availablePaperHeight] / totalScaleFactor);
    WebFrame *frame = [self _frame];
    NSArray *newPageRects = [frame _computePageRectsWithPrintScaleFactor:userScaleFactor pageSize:NSMakeSize(fullPageWidth, fullPageHeight)];

    // AppKit gets all messed up if you give it a zero-length page count (see 3576334), so if we
    // hit that case we'll pass along a degenerate 1 pixel square to print. This will print
    // a blank page (with correct-looking header and footer if that option is on), which matches
    // the behavior of IE and Camino at least.
    if ([newPageRects count] == 0)
        newPageRects = [NSArray arrayWithObject:[NSValue valueWithRect:NSMakeRect(0, 0, 1, 1)]];

    _private->pageRects = newPageRects;

    range->length = [_private->pageRects count];

    return YES;
}

// Return the drawing rectangle for a particular page number
- (NSRect)rectForPage:(NSInteger)page
{
    return [[_private->pageRects objectAtIndex:page - 1] rectValue];
}

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[[NSPrintOperation currentOperation] printInfo] paperSize]));    
    [[self _webView] _drawHeaderAndFooter];
}

- (void)beginDocument
{
    @try {
        // From now on we'll get a chance to call _endPrintMode in either beginDocument or
        // endDocument, so we can cancel the "just in case" pending call.
        [NSObject cancelPreviousPerformRequestsWithTarget:self
                                                 selector:@selector(_delayedEndPrintMode:)
                                                   object:[NSPrintOperation currentOperation]];
        [super beginDocument];
    } @catch (NSException *localException) {
        // Exception during [super beginDocument] means that endDocument will not get called,
        // so we need to clean up our "print mode" here.
        [self _endPrintModeAndRestoreWindowAutodisplay];
    }
}

- (void)endDocument
{
    [super endDocument];
    // Note sadly at this point [NSGraphicsContext currentContextDrawingToScreen] is still NO 
    [self _endPrintModeAndRestoreWindowAutodisplay];
}

#endif // PLATFORM(MAC)

- (void)keyDown:(WebEvent *)event
{
#if PLATFORM(MAC)
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];
#endif

    RetainPtr<WebHTMLView> selfProtector = self;
    BOOL eventWasSentToWebCore = (_private->keyDownEvent == event);

    BOOL callSuper = NO;

    _private->keyDownEvent = event;

#if PLATFORM(MAC)
    BOOL completionPopupWasOpen = _private->completionController && [_private->completionController popupWindowIsOpen];
    Frame* coreFrame = core([self _frame]);
    if (!eventWasSentToWebCore && coreFrame && coreFrame->eventHandler().keyEvent(event)) {
        // WebCore processed a key event, bail on any preexisting complete: UI
        if (completionPopupWasOpen)
            [_private->completionController endRevertingChange:YES moveLeft:NO];
    } else if (!_private->completionController || ![_private->completionController filterKeyDown:event]) {
        // Not consumed by complete: popup window
        [_private->completionController endRevertingChange:YES moveLeft:NO];
        callSuper = YES;
    }
#else
    Frame *coreFrame = core([self _frame]);
    if (!eventWasSentToWebCore && coreFrame)
        coreFrame->eventHandler().keyEvent(event);
#endif

    if (callSuper)
        [super keyDown:event];
    else {
#if PLATFORM(MAC)
        [NSCursor setHiddenUntilMouseMoves:YES];
#endif
    }
}

- (void)keyUp:(WebEvent *)event
{
#if PLATFORM(MAC)
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];
#endif

    BOOL eventWasSentToWebCore = (_private->keyDownEvent == event);

    RetainPtr<WebHTMLView> selfProtector = self;
    Frame* coreFrame = core([self _frame]);
    if (coreFrame && !eventWasSentToWebCore)
        coreFrame->eventHandler().keyEvent(event);
    else
        [super keyUp:event];
}

#if PLATFORM(MAC)

- (void)flagsChanged:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    RetainPtr<WebHTMLView> selfProtector = self;

    Frame* coreFrame = core([self _frame]);
    unsigned short keyCode = [event keyCode];

    // Don't make an event from the num lock and function keys.
    if (coreFrame && keyCode != 0 && keyCode != 10 && keyCode != 63) {
        coreFrame->eventHandler().keyEvent(PlatformEventFactory::createPlatformKeyboardEvent(event));
        return;
    }
        
    [super flagsChanged:event];
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (id)accessibilityAttributeValue:(NSString*)attributeName
IGNORE_WARNINGS_END
{
    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        id accTree = [[self _frame] accessibilityRoot];
        if (accTree)
            return [NSArray arrayWithObject:accTree];
        return nil;
    }
    return [super accessibilityAttributeValue:attributeName];
}

#endif

- (id)accessibilityFocusedUIElement
{
    id accTree = [[self _frame] accessibilityRoot];
    if (accTree)
        return [accTree accessibilityFocusedUIElement];
    return self;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    id accTree = [[self _frame] accessibilityRoot];
    if (accTree) {
#if PLATFORM(IOS_FAMILY)
        return [accTree accessibilityHitTest:point];
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        NSPoint windowCoord = [[self window] convertScreenToBase:point];
        ALLOW_DEPRECATED_DECLARATIONS_END
        return [accTree accessibilityHitTest:[self convertPoint:windowCoord fromView:nil]];
#endif
    }
    return self;
}

- (id)_accessibilityParentForSubview:(NSView *)subview
{
    id accTree = [[self _frame] accessibilityRoot];
    if (!accTree)
        return self;
    id parent = [accTree _accessibilityParentForSubview:subview];
    if (!parent)
        return self;
    return parent;
}

- (void)centerSelectionInVisibleArea:(id)sender
{
    COMMAND_PROLOGUE

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->selection().revealSelection(SelectionRevealMode::Reveal, ScrollAlignment::alignCenterAlways);
}

#if PLATFORM(MAC)

- (NSData *)_selectionStartFontAttributesAsRTF
{
    Frame* coreFrame = core([self _frame]);
    auto string = adoptNS([[NSAttributedString alloc] initWithString:@"x"
        attributes:coreFrame ? coreFrame->editor().fontAttributesAtSelectionStart().createDictionary().get() : nil]);
    return [string RTFFromRange:NSMakeRange(0, [string length]) documentAttributes:@{ }];
}

- (NSDictionary *)_fontAttributesFromFontPasteboard
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSPasteboard *fontPasteboard = [NSPasteboard pasteboardWithName:NSFontPboard];
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (fontPasteboard == nil)
        return nil;
    NSData *data = [fontPasteboard dataForType:legacyFontPasteboardType()];
    if (data == nil || [data length] == 0)
        return nil;
    // NSTextView does something more efficient by parsing the attributes only, but that's not available in API.
    NSAttributedString *string = [[[NSAttributedString alloc] initWithRTF:data documentAttributes:NULL] autorelease];
    if (string == nil || [string length] == 0)
        return nil;
    return [string fontAttributesInRange:NSMakeRange(0, 1)];
}

#endif

- (DOMCSSStyleDeclaration *)_emptyStyle
{
    return [[[self _frame] DOMDocument] createCSSStyleDeclaration];
}

#if PLATFORM(MAC)

- (NSString *)_colorAsString:(NSColor *)color
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSColor *rgbColor = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    ALLOW_DEPRECATED_DECLARATIONS_END
    // FIXME: If color is non-nil and rgbColor is nil, that means we got some kind
    // of fancy color that can't be converted to RGB. Changing that to "transparent"
    // might not be great, but it's probably OK.
    if (rgbColor == nil)
        return @"transparent";
    float r = [rgbColor redComponent];
    float g = [rgbColor greenComponent];
    float b = [rgbColor blueComponent];
    float a = [rgbColor alphaComponent];
    if (a == 0)
        return @"transparent";
    if (r == 0 && g == 0 && b == 0 && a == 1)
        return @"black";
    if (r == 1 && g == 1 && b == 1 && a == 1)
        return @"white";
    // FIXME: Lots more named colors. Maybe we could use the table in WebCore?
    if (a == 1)
        return [NSString stringWithFormat:@"rgb(%.0f,%.0f,%.0f)", r * 255, g * 255, b * 255];
    return [NSString stringWithFormat:@"rgba(%.0f,%.0f,%.0f,%f)", r * 255, g * 255, b * 255, a];
}

- (NSString *)_shadowAsString:(NSShadow *)shadow
{
    if (shadow == nil)
        return @"none";
    NSSize offset = [shadow shadowOffset];
    float blurRadius = [shadow shadowBlurRadius];
    if (offset.width == 0 && offset.height == 0 && blurRadius == 0)
        return @"none";
    NSColor *color = [shadow shadowColor];
    if (color == nil)
        return @"none";
    // FIXME: Handle non-integral values here?
    if (blurRadius == 0)
        return [NSString stringWithFormat:@"%@ %.0fpx %.0fpx", [self _colorAsString:color], offset.width, offset.height];
    return [NSString stringWithFormat:@"%@ %.0fpx %.0fpx %.0fpx", [self _colorAsString:color], offset.width, offset.height, blurRadius];
}

- (DOMCSSStyleDeclaration *)_styleFromFontAttributes:(NSDictionary *)dictionary
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    NSColor *color = [dictionary objectForKey:NSBackgroundColorAttributeName];
    [style setBackgroundColor:[self _colorAsString:color]];

    NSFont *font = [dictionary objectForKey:NSFontAttributeName];
    if (!font) {
        [style setFontFamily:@"Helvetica"];
        [style setFontSize:@"12px"];
        [style setFontWeight:@"normal"];
        [style setFontStyle:@"normal"];
    } else {
        NSFontManager *fm = [NSFontManager sharedFontManager];
        // FIXME: Need more sophisticated escaping code if we want to handle family names
        // with characters like single quote or backslash in their names.
        [style setFontFamily:[NSString stringWithFormat:@"'%@'", [font familyName]]];
        [style setFontSize:[NSString stringWithFormat:@"%0.fpx", [font pointSize]]];
        // FIXME: Map to the entire range of CSS weight values.
        if ([fm weightOfFont:font] >= MIN_BOLD_WEIGHT)
            [style setFontWeight:@"bold"];
        else
            [style setFontWeight:@"normal"];
        if ([fm traitsOfFont:font] & NSItalicFontMask)
            [style setFontStyle:@"italic"];
        else
            [style setFontStyle:@"normal"];
    }

    color = [dictionary objectForKey:NSForegroundColorAttributeName];
    [style setColor:color ? [self _colorAsString:color] : (NSString *)@"black"];

    NSShadow *shadow = [dictionary objectForKey:NSShadowAttributeName];
    [style setTextShadow:[self _shadowAsString:shadow]];

    int strikethroughInt = [[dictionary objectForKey:NSStrikethroughStyleAttributeName] intValue];

    int superscriptInt = [[dictionary objectForKey:NSSuperscriptAttributeName] intValue];
    if (superscriptInt > 0)
        [style setVerticalAlign:@"super"];
    else if (superscriptInt < 0)
        [style setVerticalAlign:@"sub"];
    else
        [style setVerticalAlign:@"baseline"];
    int underlineInt = [[dictionary objectForKey:NSUnderlineStyleAttributeName] intValue];
    // FIXME: Underline wins here if we have both (see bug 3790443).
    if (strikethroughInt == NSUnderlineStyleNone && underlineInt == NSUnderlineStyleNone)
        [style setProperty:@"-webkit-text-decorations-in-effect" value:@"none" priority:@""];
    else if (underlineInt == NSUnderlineStyleNone)
        [style setProperty:@"-webkit-text-decorations-in-effect" value:@"line-through" priority:@""];
    else
        [style setProperty:@"-webkit-text-decorations-in-effect" value:@"underline" priority:@""];

    return style;
}

#endif // PLATFORM(MAC)

- (void)_applyStyleToSelection:(DOMCSSStyleDeclaration *)style withUndoAction:(EditAction)undoAction
{
    [self _applyEditingStyleToSelection:EditingStyle::create(core(style)) withUndoAction:undoAction];
}

- (void)_applyEditingStyleToSelection:(Ref<EditingStyle>&&)editingStyle withUndoAction:(EditAction)undoAction
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor().applyStyleToSelection(WTFMove(editingStyle), undoAction, Editor::ColorFilterMode::InvertColor);
}

#if PLATFORM(MAC)

- (BOOL)_handleStyleKeyEquivalent:(NSEvent *)event
{
    WebView *webView = [self _webView];
    if (!webView)
        return NO;

    if (![[webView preferences] respectStandardStyleKeyEquivalents])
        return NO;
    
    if (![self _canEdit])
        return NO;
    
    if (([event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask) != NSEventModifierFlagCommand)
        return NO;

    NSString *string = [event characters];
    if ([string caseInsensitiveCompare:@"b"] == NSOrderedSame) {
        [self executeCoreCommandByName:"ToggleBold"];
        return YES;
    }
    if ([string caseInsensitiveCompare:@"i"] == NSOrderedSame) {
        [self executeCoreCommandByName:"ToggleItalic"];
        return YES;
    }
    
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    BOOL eventWasSentToWebCore = (_private->keyDownEvent == event);
    BOOL ret = NO;

    _private->keyDownEvent = event;
    
    [self retain];

    // Pass command-key combos through WebCore if there is a key binding available for
    // this event. This lets web pages have a crack at intercepting command-modified keypresses.
    // But don't do it if we have already handled the event.
    // Pressing Esc results in a fake event being sent - don't pass it to WebCore.
    if (!eventWasSentToWebCore && event == [NSApp currentEvent] && self == [[self window] firstResponder])
        if (Frame* frame = core([self _frame]))
            ret = frame->eventHandler().keyEvent(event);

    if (ret) {
#if PLATFORM(MAC)
        [NSCursor setHiddenUntilMouseMoves:YES];
#endif
    } else
        ret = [self _handleStyleKeyEquivalent:event] || [super performKeyEquivalent:event];

    [self release];
    
    return ret;
}

- (void)copyFont:(id)sender
{
    COMMAND_PROLOGUE

    // Put RTF with font attributes on the pasteboard.
    // Maybe later we should add a pasteboard type that contains CSS text for "native" copy and paste font.
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSPasteboard *fontPasteboard = [NSPasteboard pasteboardWithName:NSFontPboard];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [fontPasteboard declareTypes:[NSArray arrayWithObject:legacyFontPasteboardType()] owner:nil];
    [fontPasteboard setData:[self _selectionStartFontAttributesAsRTF] forType:legacyFontPasteboardType()];
}

- (void)pasteFont:(id)sender
{
    COMMAND_PROLOGUE

    // Read RTF with font attributes from the pasteboard.
    // Maybe later we should add a pasteboard type that contains CSS text for "native" copy and paste font.
    [self _applyStyleToSelection:[self _styleFromFontAttributes:[self _fontAttributesFromFontPasteboard]] withUndoAction:EditAction::PasteFont];
}

- (void)pasteAsRichText:(id)sender
{
    COMMAND_PROLOGUE

    // Since rich text always beats plain text when both are on the pasteboard, it's not
    // clear how this is different from plain old paste.
    [self _pasteWithPasteboard:[NSPasteboard generalPasteboard] allowPlainText:NO];
}

- (void)changeFont:(id)sender
{
    COMMAND_PROLOGUE

    [self _applyEditingStyleToSelection:computedFontChanges(NSFontManager.sharedFontManager).createEditingStyle() withUndoAction:EditAction::SetFont];
}

- (void)changeAttributes:(id)sender
{
    COMMAND_PROLOGUE

    [self _applyEditingStyleToSelection:computedFontAttributeChanges(NSFontManager.sharedFontManager, sender).createEditingStyle() withUndoAction:EditAction::ChangeAttributes];
}

- (DOMCSSStyleDeclaration *)_styleFromColorPanelWithSelector:(SEL)selector
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    ASSERT([style respondsToSelector:selector]);
    [style performSelector:selector withObject:[self _colorAsString:[[NSColorPanel sharedColorPanel] color]]];
    
    return style;
}

- (EditAction)_undoActionFromColorPanelWithSelector:(SEL)selector
{
    if (selector == @selector(setBackgroundColor:))
        return EditAction::SetBackgroundColor;
    return EditAction::SetColor;
}

- (void)_changeCSSColorUsingSelector:(SEL)selector inRange:(DOMRange *)range
{
    DOMCSSStyleDeclaration *style = [self _styleFromColorPanelWithSelector:selector];
    WebView *webView = [self _webView];
    if ([[webView _editingDelegateForwarder] webView:webView shouldApplyStyle:style toElementsInDOMRange:range]) {
        if (Frame* coreFrame = core([self _frame])) {
            // FIXME: We shouldn't have to make a copy here.
            Ref<MutableStyleProperties> properties(core(style)->copyProperties());
            coreFrame->editor().applyStyle(properties.ptr(), [self _undoActionFromColorPanelWithSelector:selector]);
        }
    }

}

- (void)changeDocumentBackgroundColor:(id)sender
{
    COMMAND_PROLOGUE

    // Mimicking NSTextView, this method sets the background color for the
    // entire document. There is no NSTextView API for setting the background
    // color on the selected range only. Note that this method is currently
    // never called from the UI (see comment in changeColor:).
    // FIXME: this actually has no effect when called, probably due to 3654850. _documentRange seems
    // to do the right thing because it works in startSpeaking:, and I know setBackgroundColor: does the
    // right thing because I tested it with [self _selectedRange].
    // FIXME: This won't actually apply the style to the entire range here, because it ends up calling
    // [frame _applyStyle:], which operates on the current selection. To make this work right, we'll
    // need to save off the selection, temporarily set it to the entire range, make the change, then
    // restore the old selection.
    [self _changeCSSColorUsingSelector:@selector(setBackgroundColor:) inRange:[self _documentRange]];
}

- (void)changeColor:(id)sender
{
    COMMAND_PROLOGUE

    // FIXME: in NSTextView, this method calls changeDocumentBackgroundColor: when a
    // private call has earlier been made by [NSFontFontEffectsBox changeColor:], see 3674493. 
    // AppKit will have to be revised to allow this to work with anything that isn't an 
    // NSTextView. However, this might not be required for Tiger, since the background-color 
    // changing box in the font panel doesn't work in Mail (3674481), though it does in TextEdit.
    [self _applyStyleToSelection:[self _styleFromColorPanelWithSelector:@selector(setColor:)] withUndoAction:EditAction::SetColor];
}

#endif // PLATFORM(MAC)

- (void)_changeWordCaseWithSelector:(SEL)selector
{
    if (![self _canEdit])
        return;

    WebFrame *frame = [self _frame];
    [self selectWord:nil];
    NSString *word = [[frame _selectedString] performSelector:selector];
    // FIXME: Does this need a different action context other than "typed"?
    if ([self _shouldReplaceSelectionWithText:word givenAction:WebViewInsertActionTyped])
        [frame _replaceSelectionWithText:word selectReplacement:NO smartReplace:NO];
}

- (void)uppercaseWord:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeWordCaseWithSelector:@selector(uppercaseString)];
}

- (void)lowercaseWord:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeWordCaseWithSelector:@selector(lowercaseString)];
}

- (void)capitalizeWord:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeWordCaseWithSelector:@selector(capitalizedString)];
}

#if PLATFORM(MAC)

- (void)complete:(id)sender
{
    COMMAND_PROLOGUE

    if (![self _canEdit])
        return;
    if (!_private->completionController)
        _private->completionController = adoptNS([[WebTextCompletionController alloc] initWithWebView:[self _webView] HTMLView:self]);
    [_private->completionController doCompletion];
}

- (void)checkSpelling:(id)sender
{
    COMMAND_PROLOGUE

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor().advanceToNextMisspelling();
}

- (void)showGuessPanel:(id)sender
{
    COMMAND_PROLOGUE

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
    
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor().advanceToNextMisspelling(true);
    [spellingPanel orderFront:sender];
}

- (void)changeSpelling:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeSpellingToWord:[[sender selectedCell] stringValue]];
}

- (void)performFindPanelAction:(id)sender
{
    COMMAND_PROLOGUE

    // Implementing this will probably require copying all of NSFindPanel.h and .m.
    // We need *almost* the same thing as AppKit, but not quite.
    LOG_ERROR("unimplemented");
}

- (void)startSpeaking:(id)sender
{
    COMMAND_PROLOGUE

    WebFrame *frame = [self _frame];
    DOMRange *range = [self _selectedRange];
    if (!range || [range collapsed])
        range = [self _documentRange];
    [NSApp speakString:[frame _stringForRange:range]];
}

- (void)stopSpeaking:(id)sender
{
    COMMAND_PROLOGUE

    [NSApp stopSpeaking:sender];
}

- (void)toggleBaseWritingDirection:(id)sender
{
    COMMAND_PROLOGUE

    if (![self _canEdit])
        return;
    
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    auto direction = WritingDirection::RightToLeft;
    switch (coreFrame->editor().baseWritingDirectionForSelectionStart()) {
    case WritingDirection::LeftToRight:
        break;
    case WritingDirection::RightToLeft:
        direction = WritingDirection::LeftToRight;
        break;
    // The writingDirectionForSelectionStart method will never return "natural". It
    // will always return a concrete direction. So, keep the compiler happy, and assert not reached.
    case WritingDirection::Natural:
        ASSERT_NOT_REACHED();
        break;
    }

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor().setBaseWritingDirection(direction);
}

- (void)changeBaseWritingDirection:(id)sender
{
    COMMAND_PROLOGUE

    if (![self _canEdit])
        return;
    
    NSWritingDirection writingDirection = static_cast<NSWritingDirection>([sender tag]);
    
    // We disable the menu item that performs this action because we can't implement
    // NSWritingDirectionNatural's behavior using CSS.
    ASSERT(writingDirection != NSWritingDirectionNatural);

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor().setBaseWritingDirection(writingDirection == NSWritingDirectionLeftToRight ? WritingDirection::LeftToRight : WritingDirection::RightToLeft);
}

static BOOL writingDirectionKeyBindingsEnabled()
{
    return YES;
}

- (void)_changeBaseWritingDirectionTo:(NSWritingDirection)direction
{
    if (![self _canEdit])
        return;

    static BOOL bindingsEnabled = writingDirectionKeyBindingsEnabled();

    if (!bindingsEnabled) {
        NSBeep();
        return;
    }

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor().setBaseWritingDirection(direction == NSWritingDirectionLeftToRight ? WritingDirection::LeftToRight : WritingDirection::RightToLeft);
}

- (void)makeBaseWritingDirectionLeftToRight:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeBaseWritingDirectionTo:NSWritingDirectionLeftToRight];
}

- (void)makeBaseWritingDirectionRightToLeft:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeBaseWritingDirectionTo:NSWritingDirectionRightToLeft];
}

#endif // PLATFORM(MAC)

- (void)makeBaseWritingDirectionNatural:(id)sender
{
    LOG_ERROR("Sent from %@.", sender);
}

#if 0

// CSS does not have a way to specify an outline font, which may make this difficult to implement.
// Maybe a special case of text-shadow?
- (void)outline:(id)sender;

// This is part of table support, which may be in NSTextView for Tiger.
// It's probably simple to do the equivalent thing for WebKit.
- (void)insertTable:(id)sender;

// This could be important.
- (void)toggleTraditionalCharacterShape:(id)sender;

// I'm not sure what the equivalents of these in the web world are.
- (void)insertLineSeparator:(id)sender;
- (void)insertPageBreak:(id)sender;

// These methods are not implemented in NSTextView yet at the time of this writing.
- (void)changeCaseOfLetter:(id)sender;
- (void)transposeWords:(id)sender;

#endif

// Override this so that AppKit will send us arrow keys as key down events so we can
// support them via the key bindings mechanism.
- (BOOL)_wantsKeyDownForEvent:(NSEvent *)event
{
    bool haveWebCoreFrame = core([self _frame]);

    // If we have a frame, our keyDown method will handle key bindings after sending
    // the event through the DOM, so ask AppKit not to do its early special key binding
    // mapping. If we don't have a frame, just let things work the normal way without
    // a keyDown.
    return haveWebCoreFrame;
}

#if PLATFORM(MAC)

- (BOOL)_automaticFocusRingDisabled
{
    // The default state for _automaticFocusRingDisabled is NO, which prevents focus rings
    // from being painted for search fields. Calling NSSetFocusRingStyle has the side effect
    // of changing this to YES, so just return YES all the time. <rdar://problem/13780122>,
    return YES;
}

- (void)_updateControlTints
{
    Frame* frame = core([self _frame]);
    if (!frame)
        return;
    FrameView* view = frame->view();
    if (!view)
        return;
    view->updateControlTints();
}

// Despite its name, this is called at different times than windowDidBecomeKey is.
// It takes into account all the other factors that determine when NSCell draws
// with different tints, so it's the right call to use for control tints. We'd prefer
// to do this with API. <rdar://problem/5136760>
- (void)_windowChangedKeyState
{
    if (pthread_main_np())
        [self _updateControlTints];
    else
        [self performSelectorOnMainThread:@selector(_updateControlTints) withObject:nil waitUntilDone:NO];

    [super _windowChangedKeyState];
}

- (void)otherMouseDown:(NSEvent *)event
{
    if (event.buttonNumber != 2 || [NSMenu menuTypeForEvent:event] == NSMenuTypeContextMenu) {
        [super otherMouseDown:event];
        return;
    }

    [self mouseDown:event];
}

- (void)otherMouseDragged:(NSEvent *)event
{
    if ([event buttonNumber] == 2)
        [self mouseDragged:event];
    else
        [super otherMouseDragged:event];
}

- (void)otherMouseUp:(NSEvent *)event
{
    if ([event buttonNumber] == 2)
        [self mouseUp:event];
    else
        [super otherMouseUp:event];
}

#endif

#if PLATFORM(IOS_FAMILY)
- (void)markedTextUpdate:(NSNotification *)notification
{
    NSString *text = [notification object];
    NSRange range = NSMakeRange(0, [text length]);
    [self setMarkedText:text selectedRange:range];
}
#endif

@end

@implementation WebHTMLView (WebInternal)

- (void)_selectionChanged
{
#if PLATFORM(MAC)
    [self _updateSelectionForInputManager];
    [self _updateFontPanel];
    if (Frame* coreFrame = core([self _frame])) {
        if (!coreFrame->editor().isHandlingAcceptedCandidate())
            _private->softSpaceRange = NSMakeRange(NSNotFound, 0);
    }
#endif
}

#if PLATFORM(MAC)

- (void)_updateFontPanel
{
    // FIXME: NSTextView bails out if becoming or resigning first responder, for which it has ivar flags. Not
    // sure if we need to do something similar.

    if (![self _canEdit])
        return;

    NSWindow *window = [self window];
    // FIXME: is this first-responder check correct? What happens if a subframe is editable and is first responder?
    if (![window isKeyWindow] || [window firstResponder] != self)
        return;

    bool multipleFonts = false;
    NSFont *font = nil;
    RetainPtr<NSDictionary> attributes;
    if (Frame* coreFrame = core([self _frame])) {
        if (const Font* fd = coreFrame->editor().fontForSelection(multipleFonts))
            font = (NSFont *)fd->platformData().registeredFont();
        attributes = coreFrame->editor().fontAttributesAtSelectionStart().createDictionary();
    }

    // FIXME: for now, return a bogus font that distinguishes the empty selection from the non-empty
    // selection. We should be able to remove this once the rest of this code works properly.
    if (font == nil)
        font = [self _hasSelection] ? [NSFont menuFontOfSize:23] : [NSFont toolTipsFontOfSize:17];
    ASSERT(font != nil);

    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    [fontManager setSelectedFont:font isMultiple:multipleFonts];
    [fontManager setSelectedAttributes:(attributes ? attributes.get() : @{ }) isMultiple:multipleFonts];
}

- (void)_setSoftSpaceRange:(NSRange)range
{
    _private->softSpaceRange = range;
}

#endif // PLATFORM(MAC)

- (BOOL)_canSmartCopyOrDelete
{
    if (![[self _webView] smartInsertDeleteEnabled])
        return NO;
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selection().granularity() == WordGranularity;
}

#if PLATFORM(MAC)

- (NSEvent *)_mouseDownEvent
{
    return _private->mouseDownEvent.get();
}

#endif

- (WebFrame *)_frame
{
    return [_private->dataSource webFrame];
}

- (void)closeIfNotCurrentView
{
    if ([[[self _frame] frameView] documentView] != self)
        [self close];
}

#if PLATFORM(MAC)

- (DOMDocumentFragment*)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard
{
    return [self _documentFragmentFromPasteboard:pasteboard inContext:nil allowPlainText:NO];
}


- (BOOL)isGrammarCheckingEnabled
{
    // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must implement the method here because
    // the AppKit code checks the first responder.
    return [[self _webView] isGrammarCheckingEnabled];
}

- (void)setGrammarCheckingEnabled:(BOOL)flag
{
    // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must implement the method here because
    // the AppKit code checks the first responder.
    [[self _webView] setGrammarCheckingEnabled:flag];
}

- (void)toggleGrammarChecking:(id)sender
{
    // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must implement the method here because
    // the AppKit code checks the first responder.
    [[self _webView] toggleGrammarChecking:sender];
}

- (void)orderFrontSubstitutionsPanel:(id)sender
{
    COMMAND_PROLOGUE

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

// FIXME 4799134: WebView is the bottleneck for this logic, but we must implement these methods here because
// the AppKit code checks the first responder.

- (BOOL)smartInsertDeleteEnabled
{
    return [[self _webView] smartInsertDeleteEnabled];
}

- (void)setSmartInsertDeleteEnabled:(BOOL)flag
{
    [[self _webView] setSmartInsertDeleteEnabled:flag];
}

- (void)toggleSmartInsertDelete:(id)sender
{
    [[self _webView] toggleSmartInsertDelete:sender];
}

- (BOOL)isAutomaticQuoteSubstitutionEnabled
{
    return [[self _webView] isAutomaticQuoteSubstitutionEnabled];
}

- (void)setAutomaticQuoteSubstitutionEnabled:(BOOL)flag
{
    [[self _webView] setAutomaticQuoteSubstitutionEnabled:flag];
}

- (void)toggleAutomaticQuoteSubstitution:(id)sender
{
    [[self _webView] toggleAutomaticQuoteSubstitution:sender];
}

- (BOOL)isAutomaticLinkDetectionEnabled
{
    return [[self _webView] isAutomaticLinkDetectionEnabled];
}

- (void)setAutomaticLinkDetectionEnabled:(BOOL)flag
{
    [[self _webView] setAutomaticLinkDetectionEnabled:flag];
}

- (void)toggleAutomaticLinkDetection:(id)sender
{
    [[self _webView] toggleAutomaticLinkDetection:sender];
}

- (BOOL)isAutomaticDashSubstitutionEnabled
{
    return [[self _webView] isAutomaticDashSubstitutionEnabled];
}

- (void)setAutomaticDashSubstitutionEnabled:(BOOL)flag
{
    [[self _webView] setAutomaticDashSubstitutionEnabled:flag];
}

- (void)toggleAutomaticDashSubstitution:(id)sender
{
    [[self _webView] toggleAutomaticDashSubstitution:sender];
}

- (BOOL)isAutomaticTextReplacementEnabled
{
    return [[self _webView] isAutomaticTextReplacementEnabled];
}

- (void)setAutomaticTextReplacementEnabled:(BOOL)flag
{
    [[self _webView] setAutomaticTextReplacementEnabled:flag];
}

- (void)toggleAutomaticTextReplacement:(id)sender
{
    [[self _webView] toggleAutomaticTextReplacement:sender];
}

- (BOOL)isAutomaticSpellingCorrectionEnabled
{
    return [[self _webView] isAutomaticSpellingCorrectionEnabled];
}

- (void)setAutomaticSpellingCorrectionEnabled:(BOOL)flag
{
    [[self _webView] setAutomaticSpellingCorrectionEnabled:flag];
}

- (void)toggleAutomaticSpellingCorrection:(id)sender
{
    [[self _webView] toggleAutomaticSpellingCorrection:sender];
}

- (void)_lookUpInDictionaryFromMenu:(id)sender
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    RefPtr<Range> selectionRange = coreFrame->selection().selection().firstRange();
    if (!selectionRange)
        return;

    [[self _webView] _showDictionaryLookupPopup:[WebImmediateActionController _dictionaryPopupInfoForRange:*selectionRange inFrame:coreFrame withLookupOptions:nil indicatorOptions:TextIndicatorOptionIncludeSnapshotWithSelectionHighlight transition:TextIndicatorPresentationTransition::BounceAndCrossfade]];
}

- (void)quickLookWithEvent:(NSEvent *)event
{
    [[self _webView] _clearTextIndicatorWithAnimation:TextIndicatorWindowDismissalAnimation::FadeOut];
    [super quickLookWithEvent:event];
}

#endif // PLATFORM(MAC)

#undef COMMAND_PROLOGUE

- (void)_executeSavedKeypressCommands
{
    auto* parameters = _private->interpretKeyEventsParameters;
    if (!parameters || parameters->event->keypressCommands().isEmpty())
        return;

    // We could be called again if the execution of one command triggers a call to selectedRange.
    // In this case, the state is up to date, and we don't need to execute any more saved commands to return a result
    if (parameters->executingSavedKeypressCommands)
        return;
    
    // Avoid an infinite loop that would occur if executing a command appended it to event->keypressCommands() again.
    bool wasSavingCommands = parameters->shouldSaveCommands;
    parameters->shouldSaveCommands = false;
    parameters->executingSavedKeypressCommands = true;
    
    const Vector<KeypressCommand>& commands = parameters->event->keypressCommands();

    for (size_t i = 0; i < commands.size(); ++i) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (commands[i].commandName == "insertText:")
            [self insertText:commands[i].text];
        else if (commands[i].commandName == "noop:")
            ; // Do nothing. This case can be removed once <rdar://problem/9025012> is fixed.
        else
            [self doCommandBySelector:NSSelectorFromString(commands[i].commandName)];
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
    parameters->event->keypressCommands().clear();
    parameters->shouldSaveCommands = wasSavingCommands;
    parameters->executingSavedKeypressCommands = false;
}

#if PLATFORM(MAC)

- (BOOL)_interpretKeyEvent:(KeyboardEvent*)event savingCommands:(BOOL)savingCommands
{
    ASSERT(core([self _frame]) == downcast<Node>(event->target())->document().frame());
    ASSERT(!savingCommands || event->keypressCommands().isEmpty()); // Save commands once for each event.

    WebHTMLViewInterpretKeyEventsParameters parameters;
    parameters.eventInterpretationHadSideEffects = false;
    parameters.shouldSaveCommands = savingCommands;
    parameters.executingSavedKeypressCommands = false;
    // If we're intercepting the initial IM call we assume that the IM has consumed the event, 
    // and only change this assumption if one of the NSTextInput/Responder callbacks is used.
    // We assume the IM will *not* consume hotkey sequences
    parameters.consumedByIM = savingCommands && !event->metaKey();

    auto* platformEvent = event->underlyingPlatformEvent();
    if (!platformEvent)
        return NO;

    NSEvent *macEvent = platformEvent->macEvent();
    if ([macEvent type] == NSEventTypeKeyDown && [_private->completionController filterKeyDown:macEvent])
        return YES;
    
    if ([macEvent type] == NSEventTypeFlagsChanged)
        return NO;
    
    parameters.event = event;
    _private->interpretKeyEventsParameters = &parameters;
    const Vector<KeypressCommand>& commands = event->keypressCommands();

    if (savingCommands) {
        // AppKit will respond with a series of NSTextInput protocol method calls. There are three groups that we heuristically differentiate:
        // 1. Key Bindings. Only doCommandBySelector: and insertText: calls will be made, which we save in the event for execution
        // after DOM dispatch. This is safe, because neither returns a result, so there is no branching on AppKit side.
        // 2. Plain text input. Here as well, we need to dispatch DOM events prior to inserting text, so we save the insertText: command.
        // 3. Input method processing. An IM can make any NSTextInput calls, and can base its decisions on results it gets, so we must
        // execute the calls immediately. DOM events like keydown are tweaked to have keyCode of 229, and canceling them has no effect.
        // Unfortunately, there is no real difference between plain text input and IM processing - for example, AppKit queries hasMarkedText
        // when typing with U.S. keyboard, and inserts marked text for dead keys.
        [self interpretKeyEvents:[NSArray arrayWithObject:macEvent]];
    } else {
        // Are there commands that could just cause text insertion if executed via Editor?
        // WebKit doesn't have enough information about mode to decide how they should be treated, so we leave it upon WebCore
        // to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        bool haveTextInsertionCommands = false;
        for (size_t i = 0; i < commands.size(); ++i) {
            if ([self coreCommandBySelector:NSSelectorFromString(commands[i].commandName)].isTextInsertion())
                haveTextInsertionCommands = true;
        }
        // If there are no text insertion commands, default keydown handler is the right time to execute the commands.
        // Keypress (Char event) handler is the latest opportunity to execute.
        if (!haveTextInsertionCommands || platformEvent->type() == PlatformEvent::Char)
            [self _executeSavedKeypressCommands];
    }
    _private->interpretKeyEventsParameters = nullptr;

    // An input method may make several actions per keypress. For example, pressing Return with Korean IM both confirms it and sends a newline.
    // IM-like actions are handled immediately (so parameters.eventInterpretationHadSideEffects is true), but there are saved commands that
    // should be handled like normal text input after DOM event dispatch.
    if (!event->keypressCommands().isEmpty())
        return NO;

    // An input method may consume an event and not tell us (e.g. when displaying a candidate window),
    // in which case we should not bubble the event up the DOM.
    if (parameters.consumedByIM)
        return YES;

    // If we have already executed all commands, don't do it again.
    return parameters.eventInterpretationHadSideEffects;
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
    
#define kWebEnterKey         0x0003
#define kWebBackspaceKey     0x0008
#define kWebReturnKey        0x000d
#define kWebDeleteKey        0x007F
    
- (BOOL)_handleEditingKeyEvent:(KeyboardEvent *)wcEvent
{
    // Use the isEditable state to determine whether or not to process tab key events.
    // The idea here is that isEditable will be NO when this WebView is being used
    // in a browser, and we desire the behavior where tab moves to the next element
    // in tab order. If isEditable is YES, it is likely that the WebView is being
    // embedded as the whole view, as in Mail, and tabs should input tabs as expected
    // in a text editor.
    
    if (auto* platformEvent = wcEvent->underlyingPlatformEvent()) {
        WebEvent *event = platformEvent->event();
        if (event.keyboardFlags & WebEventKeyboardInputModifierFlagsChanged)
            return NO;

        WebView *webView = [self _webView];
        if (!webView.isEditable && event.isTabKey)
            return NO;

        bool isCharEvent = platformEvent->type() == PlatformKeyboardEvent::Char;

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000
        if (!isCharEvent && [webView._UIKitDelegateForwarder handleKeyTextCommandForCurrentEvent])
            return YES;
        if (isCharEvent && [webView._UIKitDelegateForwarder handleKeyAppCommandForCurrentEvent])
            return YES;
#endif

        NSString *s = [event characters];
        if (!s.length)
            return NO;
        switch ([s characterAtIndex:0]) {
        case kWebBackspaceKey:
        case kWebDeleteKey:
            [[webView _UIKitDelegateForwarder] deleteFromInputWithFlags:event.keyboardFlags];
            return YES;
        case kWebEnterKey:
        case kWebReturnKey:
            if (isCharEvent) {
                // Map \r from HW keyboard to \n to match the behavior of the soft keyboard.
                [[webView _UIKitDelegateForwarder] addInputString:@"\n" withFlags:0];
                return YES;
            }
            break;
        default:
            if (isCharEvent) {
                [[webView _UIKitDelegateForwarder] addInputString:event.characters withFlags:event.keyboardFlags];
                return YES;
            }
        }
    }
    return NO;
}
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

- (void)setPromisedDragTIFFDataSource:(WebCore::CachedImage*)source
{
    if (source)
        source->addClient(promisedDataClient());
    
    if (_private->promisedDragTIFFDataSource)
        _private->promisedDragTIFFDataSource->removeClient(promisedDataClient());
    _private->promisedDragTIFFDataSource = source;
}

#endif

- (void)_layoutIfNeeded
{
#if PLATFORM(MAC)
    ASSERT(!_private->subviewsSetAside);
#endif

    if ([self _needsLayout])
        [self layout];
}

- (void)_web_updateLayoutAndStyleIfNeededRecursive
{
    WebFrame *webFrame = [self _frame];
    Frame* coreFrame = core(webFrame);
    if (coreFrame && coreFrame->view())
        coreFrame->view()->updateLayoutAndStyleIfNeededRecursive();
}

- (void) _destroyAllWebPlugins
{
    [[self _pluginController] destroyAllPlugins];
}

- (BOOL)_needsLayout
{
    return [[self _frame] _needsLayout];
}

#if PLATFORM(MAC)

- (void)attachRootLayer:(CALayer *)layer
{
    if (!_private->layerHostingView) {
        auto hostingView = adoptNS([[WebLayerHostingFlippedView alloc] initWithFrame:[self bounds]]);
        [hostingView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        [self addSubview:hostingView.get()];
        // hostingView is owned by being a subview of self
        _private->layerHostingView = hostingView.get();
    }

    // Make a container layer, which will get sized/positioned by AppKit and CA.
    CALayer* viewLayer = [WebRootLayer layer];

    if ([self layer]) {
        // If we are in a layer-backed view, we need to manually initialize the geometry for our layer.
        [viewLayer setBounds:NSRectToCGRect([_private->layerHostingView bounds])];
        [viewLayer setAnchorPoint:CGPointMake(0, [self isFlipped] ? 1 : 0)];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CGPoint layerPosition = NSPointToCGPoint([self convertPointToBase:[_private->layerHostingView frame].origin]);
        ALLOW_DEPRECATED_DECLARATIONS_END
        [viewLayer setPosition:layerPosition];
    }
    
    [_private->layerHostingView setLayer:viewLayer];
    [_private->layerHostingView setWantsLayer:YES];
    
    // Parent our root layer in the container layer
    [viewLayer addSublayer:layer];
    
    if ([[self _webView] _postsAcceleratedCompositingNotifications])
        [[NSNotificationCenter defaultCenter] postNotificationName:_WebViewDidStartAcceleratedCompositingNotification object:[self _webView] userInfo:nil];

    if (!_CFExecutableLinkedOnOrAfter(CFSystemVersionMountainLion))
        [viewLayer setGeometryFlipped:YES];
}

- (void)detachRootLayer
{
    if (_private->layerHostingView) {
        [_private->layerHostingView setLayer:nil];
        [_private->layerHostingView setWantsLayer:NO];
        [_private->layerHostingView removeFromSuperview];
        _private->layerHostingView = nil;
    }
}

- (void)drawLayer:(CALayer *)layer inContext:(CGContextRef)ctx
{
    if (_private) {
        ASSERT(!_private->drawingIntoLayer);
        _private->drawingIntoLayer = YES;
        _private->drawingIntoAcceleratedLayer = [layer drawsAsynchronously];
    }

    [super drawLayer:layer inContext:ctx];

    if (_private) {
        _private->drawingIntoLayer = NO;
        _private->drawingIntoAcceleratedLayer = NO;
    }
}

- (BOOL)_web_isDrawingIntoLayer
{
    return _private->drawingIntoLayer;
}

- (BOOL)_web_isDrawingIntoAcceleratedLayer
{
    return _private->drawingIntoAcceleratedLayer;
}

- (void)_changeSpellingToWord:(NSString *)newWord
{
    if (![self _canEdit])
        return;

    if (![NSSpellChecker sharedSpellChecker]) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    // Don't correct to empty string.  (AppKit checked this, we might as well too.)    
    if ([newWord isEqualToString:@""])
        return;

    if ([self _shouldReplaceSelectionWithText:newWord givenAction:WebViewInsertActionPasted])
        [[self _frame] _replaceSelectionWithText:newWord selectReplacement:YES smartReplace:NO];
}

- (void)_startAutoscrollTimer:(NSEvent *)triggerEvent
{
    if (_private->autoscrollTimer)
        return;
    _private->autoscrollTimer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(_autoscroll) userInfo:nil repeats:YES];
    _private->autoscrollTriggerEvent = triggerEvent;
}

#endif

- (void)_stopAutoscrollTimer
{
#if PLATFORM(MAC)
    [_private->autoscrollTimer invalidate];
    _private->autoscrollTimer = nil;
    _private->autoscrollTriggerEvent = nil;
#endif
}

- (WebPluginController *)_pluginController
{
    return _private->pluginController.get();
}

@end

@implementation WebHTMLView (WebNSTextInputSupport)

#if PLATFORM(MAC)

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray *)validAttributesForMarkedText
IGNORE_WARNINGS_END
{
    static NSArray *validAttributes = [[NSArray alloc] initWithObjects:
        NSUnderlineStyleAttributeName,
        NSUnderlineColorAttributeName,
        NSMarkedClauseSegmentAttributeName,
        NSTextInputReplacementRangeAttributeName,
#if USE(DICTATION_ALTERNATIVES)
        NSTextAlternativesAttributeName,
#endif
#if USE(INSERTION_UNDO_GROUPING)
        NSTextInsertionUndoableAttributeName,
#endif
        nil];
    LOG(TextInput, "validAttributesForMarkedText -> (...)");
    return validAttributes;
}

- (NSTextInputContext *)inputContext
{
    return _private->exposeInputContext ? [super inputContext] : nil;
}

- (NSAttributedString *)textStorage
{
    if (!_private->exposeInputContext) {
        LOG(TextInput, "textStorage -> nil");
        return nil;
    }
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSAttributedString *result = [self attributedSubstringFromRange:NSMakeRange(0, UINT_MAX)];
    ALLOW_DEPRECATED_DECLARATIONS_END

    LOG(TextInput, "textStorage -> \"%@\"", result ? [result string] : @"");
    
    // We have to return an empty string rather than null to prevent TSM from calling -string
    return result ? result : [[[NSAttributedString alloc] initWithString:@""] autorelease];
}

#endif // PLATFORM(MAC)

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
IGNORE_WARNINGS_END
{
    [self _executeSavedKeypressCommands];

    NSWindow *window = [self window];
    WebFrame *frame = [self _frame];

    if (window) {
        NSRect screenRect = { thePoint, NSZeroSize };
        thePoint = [window convertRectFromScreen:screenRect].origin;
    }
    thePoint = [self convertPoint:thePoint fromView:nil];

    DOMRange *range = [frame _characterRangeAtPoint:thePoint];
    if (!range) {
        LOG(TextInput, "characterIndexForPoint:(%f, %f) -> NSNotFound", thePoint.x, thePoint.y);
        return NSNotFound;
    }
    
    unsigned result = [frame _convertDOMRangeToNSRange:range].location;
    LOG(TextInput, "characterIndexForPoint:(%f, %f) -> %u", thePoint.x, thePoint.y, result);
    return result;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSRect)firstRectForCharacterRange:(NSRange)theRange
IGNORE_WARNINGS_END
{
    [self _executeSavedKeypressCommands];

    WebFrame *frame = [self _frame];
    
    // Just to match NSTextView's behavior. Regression tests cannot detect this;
    // to reproduce, use a test application from http://bugs.webkit.org/show_bug.cgi?id=4682
    // (type something; try ranges (1, -1) and (2, -1).
    if ((theRange.location + theRange.length < theRange.location) && (theRange.location + theRange.length != 0))
        theRange.length = 0;
    
    DOMRange *range = [frame _convertNSRangeToDOMRange:theRange];
    if (!range) {
        LOG(TextInput, "firstRectForCharacterRange:(%u, %u) -> (0, 0, 0, 0)", theRange.location, theRange.length);
        return NSMakeRect(0, 0, 0, 0);
    }
    
    ASSERT([range startContainer]);
    ASSERT([range endContainer]);
    
    NSRect resultRect = [frame _firstRectForDOMRange:range];
    resultRect = [self convertRect:resultRect toView:nil];

    NSWindow *window = [self window];
    if (window)
        resultRect.origin = [window convertRectToScreen:resultRect].origin;
    
    LOG(TextInput, "firstRectForCharacterRange:(%u, %u) -> (%f, %f, %f, %f)", theRange.location, theRange.length, resultRect.origin.x, resultRect.origin.y, resultRect.size.width, resultRect.size.height);
    return resultRect;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSRange)selectedRange
IGNORE_WARNINGS_END
{
    [self _executeSavedKeypressCommands];

    if (!isTextInput(core([self _frame]))) {
        LOG(TextInput, "selectedRange -> (NSNotFound, 0)");
        return NSMakeRange(NSNotFound, 0);
    }
    NSRange result = [[self _frame] _selectedNSRange];

    LOG(TextInput, "selectedRange -> (%u, %u)", result.location, result.length);
    return result;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSRange)markedRange
IGNORE_WARNINGS_END
{
    [self _executeSavedKeypressCommands];

    WebFrame *webFrame = [self _frame];
    Frame* coreFrame = core(webFrame);
    if (!coreFrame)
        return NSMakeRange(0, 0);
    NSRange result = [webFrame _convertToNSRange:coreFrame->editor().compositionRange().get()];

    LOG(TextInput, "markedRange -> (%u, %u)", result.location, result.length);
    return result;
}

#if PLATFORM(MAC)

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSAttributedString *)attributedSubstringFromRange:(NSRange)nsRange
IGNORE_WARNINGS_END
{
    [self _executeSavedKeypressCommands];

    WebFrame *frame = [self _frame];
    Frame* coreFrame = core(frame);
    if (!isTextInput(coreFrame) || isInPasswordField(coreFrame)) {
        LOG(TextInput, "attributedSubstringFromRange:(%u, %u) -> nil", nsRange.location, nsRange.length);
        return nil;
    }
    RefPtr<Range> range = [frame _convertToDOMRange:nsRange];
    if (!range) {
        LOG(TextInput, "attributedSubstringFromRange:(%u, %u) -> nil", nsRange.location, nsRange.length);
        return nil;
    }

    NSAttributedString *result = editingAttributedStringFromRange(*range);
    
    // WebCore::editingAttributedStringFromRange() insists on inserting a trailing
    // whitespace at the end of the string which breaks the ATOK input method.  <rdar://problem/5400551>
    // To work around this we truncate the resultant string to the correct length.
    if ([result length] > nsRange.length) {
        ASSERT([result length] == nsRange.length + 1);
        ASSERT([[result string] characterAtIndex:nsRange.length] == '\n' || [[result string] characterAtIndex:nsRange.length] == ' ');
        result = [result attributedSubstringFromRange:NSMakeRange(0, nsRange.length)];
    }
    LOG(TextInput, "attributedSubstringFromRange:(%u, %u) -> \"%@\"", nsRange.location, nsRange.length, [result string]);
    return result;
}

#endif

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSInteger)conversationIdentifier
IGNORE_WARNINGS_END
{
    return (NSInteger)self;
}

- (BOOL)hasMarkedText
{
    Frame* coreFrame = core([self _frame]);
    BOOL result = coreFrame && coreFrame->editor().hasComposition();

    if (result) {
        // A saved command can confirm a composition, but it cannot start a new one.
        [self _executeSavedKeypressCommands];
        result = coreFrame->editor().hasComposition();
    }

    LOG(TextInput, "hasMarkedText -> %u", result);
    return result;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)unmarkText
IGNORE_WARNINGS_END
{
    [self _executeSavedKeypressCommands];

    LOG(TextInput, "unmarkText");

    // Use pointer to get parameters passed to us by the caller of interpretKeyEvents.
    auto* parameters = _private->interpretKeyEventsParameters;

    if (parameters) {
        parameters->eventInterpretationHadSideEffects = true;
        parameters->consumedByIM = false;
    }
    
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor().confirmComposition();
}

#if PLATFORM(MAC)

static void extractUnderlines(NSAttributedString *string, Vector<CompositionUnderline>& result)
{
    int length = [[string string] length];

    int i = 0;
    while (i < length) {
        NSRange range;
        NSDictionary *attrs = [string attributesAtIndex:i longestEffectiveRange:&range inRange:NSMakeRange(i, length - i)];

        if (NSNumber *style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
            Color color = Color::black;
            CompositionUnderlineColor compositionUnderlineColor = CompositionUnderlineColor::TextColor;
            if (NSColor *colorAttr = [attrs objectForKey:NSUnderlineColorAttributeName]) {
                color = colorFromNSColor(colorAttr);
                compositionUnderlineColor = CompositionUnderlineColor::GivenColor;
            }
            result.append(CompositionUnderline(range.location, NSMaxRange(range), compositionUnderlineColor, color, [style intValue] > 1));
        }

        i = range.location + range.length;
    }
}

#endif

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange
IGNORE_WARNINGS_END
{
    [self _executeSavedKeypressCommands];

#if PLATFORM(MAC)
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
    ASSERT(isAttributedString || [string isKindOfClass:[NSString class]]);

    LOG(TextInput, "setMarkedText:\"%@\" selectedRange:(%u, %u)", isAttributedString ? [string string] : string, newSelRange.location, newSelRange.length);
#endif

    // Use pointer to get parameters passed to us by the caller of interpretKeyEvents.
    auto* parameters = _private->interpretKeyEventsParameters;

    if (parameters) {
        parameters->eventInterpretationHadSideEffects = true;
        parameters->consumedByIM = false;
    }
    
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    if (![self _isEditable])
        return;

    Vector<CompositionUnderline> underlines;
    NSString *text;
    NSRange replacementRange = { NSNotFound, 0 };

#if PLATFORM(MAC)
    if (isAttributedString) {
        // FIXME: We ignore most attributes from the string, so an input method cannot specify e.g. a font or a glyph variation.
        text = [string string];
        NSString *rangeString = [string attribute:NSTextInputReplacementRangeAttributeName atIndex:0 longestEffectiveRange:0 inRange:NSMakeRange(0, [text length])];
        LOG(TextInput, "    ReplacementRange: %@", rangeString);
        // The AppKit adds a 'secret' property to the string that contains the replacement range.
        // The replacement range is the range of the text that should be replaced with the new string.
        if (rangeString)
            replacementRange = NSRangeFromString(rangeString);

        extractUnderlines(string, underlines);
    } else {
        text = string;
        underlines.append(CompositionUnderline(0, [text length], CompositionUnderlineColor::TextColor, Color::black, false));
    }
#else
    text = string;
#endif

    if (replacementRange.location != NSNotFound)
        [[self _frame] _selectNSRange:replacementRange];

    coreFrame->editor().setComposition(text, underlines, newSelRange.location, NSMaxRange(newSelRange));
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)doCommandBySelector:(SEL)selector
IGNORE_WARNINGS_END
{
    LOG(TextInput, "doCommandBySelector:\"%s\"", sel_getName(selector));

    // Use pointer to get parameters passed to us by the caller of interpretKeyEvents.
    // The same call to interpretKeyEvents can do more than one command.
    auto* parameters = _private->interpretKeyEventsParameters;
    if (parameters)
        parameters->consumedByIM = false;

    KeyboardEvent* event = parameters ? parameters->event : 0;
    bool shouldSaveCommand = parameters && parameters->shouldSaveCommands;

    // As in insertText:, we assume that the call comes from an input method if there is marked text.
    RefPtr<Frame> coreFrame = core([self _frame]);
    bool isFromInputMethod = coreFrame && coreFrame->editor().hasComposition();

    if (event && shouldSaveCommand && !isFromInputMethod)
        event->keypressCommands().append(KeypressCommand(NSStringFromSelector(selector)));
    else {
        // Make sure that only direct calls to doCommandBySelector: see the parameters by setting to 0.
        _private->interpretKeyEventsParameters = 0;

        bool eventWasHandled;

        WebView *webView = [self _webView];
        if ([[webView _editingDelegateForwarder] webView:webView doCommandBySelector:selector])
            eventWasHandled = true;
        else {
            Editor::Command command = [self coreCommandBySelector:selector];
            if (command.isSupported())
                eventWasHandled = command.execute(event);
            else {
#if PLATFORM(MAC)
                // If WebKit does not support this command, we need to pass the selector to super.
                _private->selectorForDoCommandBySelector = selector;

                // The sink does two things: 1) Tells us if the responder went unhandled, and
                // 2) prevents any NSBeep; we don't ever want to beep here.
                auto sink = adoptNS([[WebResponderChainSink alloc] initWithResponderChain:self]);
                [super doCommandBySelector:selector];
                eventWasHandled = ![sink receivedUnhandledCommand];
                [sink detach];

                _private->selectorForDoCommandBySelector = 0;
#else
                eventWasHandled = false;
#endif
            }
        }

        if (parameters)
            parameters->eventInterpretationHadSideEffects |= eventWasHandled;

        _private->interpretKeyEventsParameters = parameters;
    }
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)insertText:(id)string
IGNORE_WARNINGS_END
{
#if PLATFORM(MAC)
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
    ASSERT(isAttributedString || [string isKindOfClass:[NSString class]]);

    LOG(TextInput, "insertText:\"%@\"", isAttributedString ? [string string] : string);
#endif

    auto* parameters = _private->interpretKeyEventsParameters;
    if (parameters)
        parameters->consumedByIM = false;

    RefPtr<Frame> coreFrame = core([self _frame]);
    NSString *text;
    NSRange replacementRange = { NSNotFound, 0 };
    bool isFromInputMethod = coreFrame && coreFrame->editor().hasComposition();
#if USE(INSERTION_UNDO_GROUPING)
    bool registerUndoGroup = false;
#endif

    Vector<DictationAlternative> dictationAlternativeLocations;
#if PLATFORM(MAC)
    if (isAttributedString) {
#if USE(DICTATION_ALTERNATIVES)
        Vector<WebCore::TextAlternativeWithRange> textAlternatives;
        collectDictationTextAlternatives(string, textAlternatives);
        if (!textAlternatives.isEmpty())
            [[self _webView] _getWebCoreDictationAlternatives:dictationAlternativeLocations fromTextAlternatives:textAlternatives];
#endif
#if USE(INSERTION_UNDO_GROUPING)
        registerUndoGroup = shouldRegisterInsertionUndoGroup(string);
#endif
        // FIXME: We ignore most attributes from the string, so for example inserting from Character Palette loses font and glyph variation data.
        // It does not look like any input methods ever use insertText: with attributes other than NSTextInputReplacementRangeAttributeName.
        text = [string string];
        NSString *rangeString = [string attribute:NSTextInputReplacementRangeAttributeName atIndex:0 longestEffectiveRange:0 inRange:NSMakeRange(0, [text length])];
        LOG(TextInput, "    ReplacementRange: %@", rangeString);
        if (rangeString) {
            replacementRange = NSRangeFromString(rangeString);
            isFromInputMethod = true;
        }
    } else
#endif
        text = string;

    KeyboardEvent* event = parameters ? parameters->event : 0;

    // insertText can be called for several reasons:
    // - If it's from normal key event processing (including key bindings), we may need to save the action to perform it later.
    // - If it's from an input method, then we should insert the text now. We assume it's from the input method if we have marked text.
    // FIXME: In theory, this could be wrong for some input methods, so we should try to find another way to determine if the call is from the input method.
    // - If it's sent outside of keyboard event processing (e.g. from Character Viewer, or when confirming an inline input area with a mouse),
    // then we also execute it immediately, as there will be no other chance.
    bool shouldSaveCommand = parameters && parameters->shouldSaveCommands;
    if (event && shouldSaveCommand && !isFromInputMethod) {
        event->keypressCommands().append(KeypressCommand("insertText:", text));
        return;
    }

    if (!coreFrame || !coreFrame->editor().canEdit())
        return;

    BOOL needToRemoveSoftSpace = NO;
#if PLATFORM(MAC)
    if (_private->softSpaceRange.location != NSNotFound && (replacementRange.location == NSMaxRange(_private->softSpaceRange) || replacementRange.location == NSNotFound) && !replacementRange.length && [[NSSpellChecker sharedSpellChecker] deletesAutospaceBeforeString:text language:nil]) {
        replacementRange = _private->softSpaceRange;
        needToRemoveSoftSpace = YES;
    }
    _private->softSpaceRange = NSMakeRange(NSNotFound, 0);
#endif

    bool replacesText = false;
    if (replacementRange.location != NSNotFound) {
        WebRangeIsRelativeTo rangeIsRelativeTo = needToRemoveSoftSpace ? WebRangeIsRelativeTo::Paragraph : WebRangeIsRelativeTo::EditableRoot;
        if (auto domRange = [[self _frame] _convertToDOMRange:replacementRange rangeIsRelativeTo:rangeIsRelativeTo]) {
            coreFrame->selection().setSelection(VisibleSelection(*domRange, SEL_DEFAULT_AFFINITY));
            replacesText = replacementRange.length;
        }
    }

    bool eventHandled = false;
    String eventText = text;
    eventText.replace(NSBackTabCharacter, NSTabCharacter); // same thing is done in KeyEventMac.mm in WebCore
    if (!coreFrame->editor().hasComposition()) {
        // An insertText: might be handled by other responders in the chain if we don't handle it.
        // One example is space bar that results in scrolling down the page.

        if (!dictationAlternativeLocations.isEmpty())
            eventHandled = coreFrame->editor().insertDictatedText(eventText, dictationAlternativeLocations, event);
        else
            eventHandled = coreFrame->editor().insertText(eventText, event, replacesText ? TextEventInputAutocompletion : TextEventInputKeyboard);
        
#if USE(INSERTION_UNDO_GROUPING)
        if (registerUndoGroup)
            registerInsertionUndoGroupingWithUndoManager([[self _webView] undoManager]);
#endif
    } else {
        eventHandled = true;
        coreFrame->editor().confirmComposition(eventText);
    }
    
    if (parameters)
        parameters->eventInterpretationHadSideEffects |= eventHandled;
}

#if PLATFORM(MAC)

- (void)_updateSecureInputState
{
    if (![[self window] isKeyWindow] || ([[self window] firstResponder] != self && !_private->_forceUpdateSecureInputState)) {
        if (_private->isInSecureInputState) {
            DisableSecureEventInput();
            _private->isInSecureInputState = NO;
        }
        return;
    }

    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    if (isInPasswordField(coreFrame)) {
        if (!_private->isInSecureInputState)
            EnableSecureEventInput();
        _private->isInSecureInputState = YES;
        // WebKit substitutes nil for input context when in password field, which corresponds to null TSMDocument. So, there is
        // no need to call TSMGetActiveDocument(), which may return an incorrect result when selection hasn't been yet updated
        // after focusing a node.
        static CFArrayRef inputSources = TISCreateASCIICapableInputSourceList();
        TSMSetDocumentProperty(0, kTSMDocumentEnabledInputSourcesPropertyTag, sizeof(CFArrayRef), &inputSources);
    } else {
        if (_private->isInSecureInputState)
            DisableSecureEventInput();
        _private->isInSecureInputState = NO;
        TSMRemoveDocumentProperty(0, kTSMDocumentEnabledInputSourcesPropertyTag);
    }
}

- (void)_updateSelectionForInputManager
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    BOOL exposeInputContext = isTextInput(coreFrame) && !isInPasswordField(coreFrame);
    if (exposeInputContext != _private->exposeInputContext) {
        _private->exposeInputContext = exposeInputContext;
        // Let AppKit cache a potentially changed input context.
        // WebCore routinely sets the selection to None when editing, and IMs become unhappy when an input context suddenly turns nil, see bug 26009.
        if (!coreFrame->selection().isNone())
            [NSApp updateWindows];
    }

    [self _updateSecureInputState];

    if (!coreFrame->editor().hasComposition() || coreFrame->editor().ignoreSelectionChanges())
        return;

    unsigned start;
    unsigned end;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (coreFrame->editor().getCompositionSelection(start, end))
        [[NSInputManager currentInputManager] markedTextSelectionChanged:NSMakeRange(start, end - start) client:self];
    else {
        coreFrame->editor().cancelComposition();
        [[NSInputManager currentInputManager] markedTextAbandoned:self];
    }
    ALLOW_DEPRECATED_DECLARATIONS_END
}

#endif

#if HAVE(TOUCH_BAR)

- (NSCandidateListTouchBarItem *)candidateListTouchBarItem
{
    return [[self _webView] candidateList];
}

#endif

@end

@implementation WebHTMLView (WebDocumentPrivateProtocols)

- (NSRect)selectionRect
{
    if (![self _hasSelection])
        return NSZeroRect;
    return core([self _frame])->selection().selectionBounds();
}

- (NSArray *)selectionTextRects
{
    if (![self _hasSelection])
        return nil;

    Vector<FloatRect> list;
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->selection().getClippedVisibleTextRectangles(list);

    size_t size = list.size();

    NSMutableArray *result = [NSMutableArray arrayWithCapacity:size];

    for (size_t i = 0; i < size; ++i)
        [result addObject:[NSValue valueWithRect:list[i]]];

    return result;
}

- (NSView *)selectionView
{
    return self;
}

#if PLATFORM(IOS_FAMILY)

static CGImageRef imageFromRect(Frame* frame, CGRect rect)
{
    Page* page = frame->page();
    if (!page)
        return nil;
    WAKView* documentView = frame->view()->documentView();
    if (!documentView)
        return nil;
    if (![documentView isKindOfClass:[WebHTMLView class]])
        return nil;
    
    WebHTMLView *view = (WebHTMLView *)documentView;
    
    OptionSet<PaintBehavior> oldPaintBehavior = frame->view()->paintBehavior();
    frame->view()->setPaintBehavior(oldPaintBehavior | PaintBehavior::FlattenCompositingLayers | PaintBehavior::Snapshotting);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    CGRect bounds = [view bounds];
    
    float scale = page->pageScaleFactor() * page->deviceScaleFactor();
    
    // Round image rect size in window coordinate space to avoid pixel cracks at HiDPI (4622794)
    rect = [view convertRect:rect toView:nil];
    rect.size.height = roundf(rect.size.height);
    rect.size.width = roundf(rect.size.width);
    rect = [view convertRect:rect fromView:nil];
    if (rect.size.width == 0 || rect.size.height == 0)
        return nil;
    
    size_t width = static_cast<size_t>(rect.size.width * scale);
    size_t height = static_cast<size_t>(rect.size.height * scale);
    size_t bitsPerComponent = 8;
    size_t bitsPerPixel = 4 * bitsPerComponent;
    size_t bytesPerRow = ((bitsPerPixel + 7) / 8) * width;
    RetainPtr<CGContextRef> context = adoptCF(CGBitmapContextCreate(NULL, width, height, bitsPerComponent, bytesPerRow, sRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast));
    if (!context)
        return nil;
    
    CGContextRef oldContext = WKGetCurrentGraphicsContext();
    CGContextRef contextRef = context.get();
    WKSetCurrentGraphicsContext(contextRef);
    
    CGContextClearRect(contextRef, CGRectMake(0, 0, width, height));
    CGContextSaveGState(contextRef);
    CGContextScaleCTM(contextRef, scale, scale);
    CGContextSetBaseCTM(contextRef, CGAffineTransformMakeScale(scale, scale));
    CGContextTranslateCTM(contextRef, bounds.origin.x - rect.origin.x,  bounds.origin.y - rect.origin.y);
    
    [view drawSingleRect:rect];
    
    CGContextRestoreGState(contextRef);
    
    RetainPtr<CGImageRef> resultImage = adoptCF(CGBitmapContextCreateImage(contextRef));
    
    WKSetCurrentGraphicsContext(oldContext);
    frame->view()->setPaintBehavior(oldPaintBehavior);
    
    return resultImage.autorelease();
    
    END_BLOCK_OBJC_EXCEPTIONS;

    frame->view()->setPaintBehavior(oldPaintBehavior);
    return nil;
}

static CGImageRef selectionImage(Frame* frame, bool forceBlackText)
{
    ASSERT(!WebThreadIsEnabled() || WebThreadIsLocked());
    frame->view()->setPaintBehavior(PaintBehavior::SelectionOnly | (forceBlackText ? OptionSet<PaintBehavior>(PaintBehavior::ForceBlackText) : OptionSet<PaintBehavior>()));
    frame->document()->updateLayout();
    CGImageRef result = imageFromRect(frame, frame->selection().selectionBounds());
    frame->view()->setPaintBehavior(PaintBehavior::Normal);
    return result;
}

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
- (NSImage *)selectionImageForcingBlackText:(BOOL)forceBlackText
#else
- (CGImageRef)selectionImageForcingBlackText:(BOOL)forceBlackText
#endif
{
    if (![self _hasSelection])
        return nil;

    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return nil;

#if PLATFORM(IOS_FAMILY)
    return selectionImage(coreFrame, forceBlackText);
#else
    TextIndicatorData textIndicator;
    return createDragImageForSelection(*coreFrame, textIndicator, forceBlackText).autorelease();
#endif
}

- (NSRect)selectionImageRect
{
    if (![self _hasSelection])
        return NSZeroRect;
    return core([self _frame])->selection().selectionBounds();
}

#if PLATFORM(MAC)

- (NSArray *)pasteboardTypesForSelection
{
    if ([self _canSmartCopyOrDelete]) {
        NSMutableArray *types = [[[[self class] _selectionPasteboardTypes] mutableCopy] autorelease];
        [types addObject:WebSmartPastePboardType];
        return types;
    }
    return [[self class] _selectionPasteboardTypes];
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    [self _writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard cachedAttributedString:nil];
}

#endif

- (void)selectAll
{
    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        coreFrame->selection().selectAll();
}

- (void)deselectAll
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    coreFrame->selection().clear();
}

- (NSString *)string
{
    return [[self _frame] _stringForRange:[self _documentRange]];
}

#if PLATFORM(MAC)

- (NSAttributedString *)_attributedStringFromDOMRange:(DOMRange *)range
{
    NSAttributedString *attributedString;
#if !LOG_DISABLED
    double start = CFAbsoluteTimeGetCurrent();
#endif
    if (!range)
        return nullptr;
    attributedString = attributedStringFromRange(*core(range));
#if !LOG_DISABLED
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "creating attributed string from selection took %f seconds.", duration);
#endif
    return attributedString;
}

- (NSAttributedString *)_legacyAttributedStringFrom:(DOMNode*)startContainer offset:(int)startOffset to:(DOMNode*)endContainer offset:(int)endOffset
{
    return attributedStringBetweenStartAndEnd(
        Position { core(startContainer), startOffset, Position::PositionIsOffsetInAnchor },
        Position { core(endContainer), endOffset, Position::PositionIsOffsetInAnchor });
}

- (NSAttributedString *)attributedString
{
    DOMDocument *document = [[self _frame] DOMDocument];
    NSAttributedString *attributedString = [self _attributedStringFromDOMRange:[document _documentRange]];
    if (!attributedString) {
        Document* coreDocument = core(document);
        attributedString = editingAttributedStringFromRange(Range::create(*coreDocument, coreDocument, 0, coreDocument, coreDocument->countChildNodes()));
    }
    return attributedString;
}

- (NSAttributedString *)selectedAttributedString
{
    NSAttributedString *attributedString = [self _attributedStringFromDOMRange:[self _selectedRange]];
    if (!attributedString) {
        Frame* coreFrame = core([self _frame]);
        if (coreFrame) {
            RefPtr<Range> range = coreFrame->selection().selection().toNormalizedRange();
            if (range)
                attributedString = editingAttributedStringFromRange(*range);
            else
                attributedString = [[[NSAttributedString alloc] init] autorelease];
        }
    }
    return attributedString;
}

#endif

- (NSString *)selectedString
{
    return [[self _frame] _selectedString];
}

- (BOOL)supportsTextEncoding
{
    return YES;
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag startInSelection:(BOOL)startInSelection
{
    return [self _findString:string options:(forward ? 0 : WebFindOptionsBackwards) | (caseFlag ? 0 : WebFindOptionsCaseInsensitive) | (wrapFlag ? WebFindOptionsWrapAround : 0) | (startInSelection ? WebFindOptionsStartInSelection : 0)];
}

@end

@implementation WebHTMLView (WebDocumentInternalProtocols)

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    return [self elementAtPoint:point allowShadowContent:NO];
}

- (NSDictionary *)elementAtPoint:(NSPoint)point allowShadowContent:(BOOL)allow
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return nil;
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active
        | (allow ? 0 : HitTestRequest::DisallowUserAgentShadowContent);
    return [[[WebElementDictionary alloc] initWithHitTestResult:coreFrame->eventHandler().hitTestResultAtPoint(IntPoint(point), hitType)] autorelease];
}

- (NSUInteger)countMatchesForText:(NSString *)string inDOMRange:(DOMRange *)range options:(WebFindOptions)options limit:(NSUInteger)limit markMatches:(BOOL)markMatches
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return 0;

    return coreFrame->editor().countMatchesForText(string, core(range), coreOptions(options), limit, markMatches, 0);
}

- (void)setMarkedTextMatchesAreHighlighted:(BOOL)newValue
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    coreFrame->editor().setMarkedTextMatchesAreHighlighted(newValue);
}

- (BOOL)markedTextMatchesAreHighlighted
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor().markedTextMatchesAreHighlighted();
}

- (void)unmarkAllTextMatches
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    Document* document = coreFrame->document();
    if (!document)
        return;
    document->markers().removeMarkers(DocumentMarker::TextMatch);
}

- (NSArray *)rectsForTextMatches
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return [NSArray array];
    Document* document = coreFrame->document();
    if (!document)
        return [NSArray array];

    Vector<FloatRect> rects = document->markers().renderedRectsForMarkers(DocumentMarker::TextMatch);
    unsigned count = rects.size();
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:count];
    for (unsigned index = 0; index < count; ++index)
        [result addObject:[NSValue valueWithRect:rects[index]]];    
    return result;
}

- (BOOL)_findString:(NSString *)string options:(WebFindOptions)options
{
    if (![string length])
        return NO;
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor().findString(string, coreOptions(options));
}

@end

// This is used by AppKit/TextKit. It should be possible to remove this once
// -[NSAttributedString _documentFromRange:document:documentAttributes:subresources:] is removed.
@implementation NSURL (WebDataURL)

+ (NSURL *)_web_uniqueWebDataURL
{
    return URL::fakeURLWithRelativePart(emptyString());
}

@end

#if PLATFORM(MAC)

@implementation WebResponderChainSink

- (id)initWithResponderChain:(NSResponder *)chain
{
    self = [super init];
    _lastResponderInChain = chain;
    while (NSResponder *next = [_lastResponderInChain nextResponder])
        _lastResponderInChain = next;
    [_lastResponderInChain setNextResponder:self];
    return self;
}

- (void)detach
{
    [_lastResponderInChain setNextResponder:nil];
    _lastResponderInChain = nil;
}

- (BOOL)receivedUnhandledCommand
{
    return _receivedUnhandledCommand;
}

- (void)noResponderFor:(SEL)selector
{
    _receivedUnhandledCommand = YES;
}

- (void)doCommandBySelector:(SEL)selector
{
    _receivedUnhandledCommand = YES;
}

- (BOOL)tryToPerform:(SEL)action with:(id)object
{
    _receivedUnhandledCommand = YES;
    return YES;
}

@end

#endif
