/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#import "MenuUtilities.h"

#if PLATFORM(MAC)

#import "StringUtilities.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/RevealUtilities.h>

#if ENABLE(CONTEXT_MENU_IMAGES_FOR_INTERNAL_CLIENTS)
#import <pal/spi/mac/NSImageSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
#import <pal/spi/mac/TelephonyUtilitiesSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(TelephonyUtilities)
SOFT_LINK_CLASS(TelephonyUtilities, TUCall)
#endif

#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#import <pal/cocoa/RevealSoftLink.h>
#import <pal/mac/DataDetectorsSoftLink.h>

@interface WKEmptyPresenterHighlightDelegate : NSObject <RVPresenterHighlightDelegate>

- (instancetype)initWithRect:(NSRect)rect;

@property NSRect rect;

@end

@implementation WKEmptyPresenterHighlightDelegate

- (instancetype)initWithRect:(NSRect)rect
{
    if (!(self = [super init]))
        return nil;

    _rect = rect;
    return self;
}


- (NSArray <NSValue *> *)revealContext:(RVPresentingContext *)context rectsForItem:(RVItem *)item
{
    return @[ [NSValue valueWithRect:self.rect] ];
}

- (BOOL)revealContext:(RVPresentingContext *)context shouldUseDefaultHighlightForItem:(RVItem *)item
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(item);
    return NO;
}

@end

namespace WebKit {

#if ENABLE(TELEPHONE_NUMBER_DETECTION)

NSString *menuItemTitleForTelephoneNumberGroup()
{
    return [getTUCallClass() supplementalDialTelephonyCallString];
}

#if HAVE(DATA_DETECTORS_MAC_ACTION)
static DDMacAction *actionForMenuItem(NSMenuItem *item)
#else
static DDAction *actionForMenuItem(NSMenuItem *item)
#endif
{
    auto *representedObject = dynamicObjCDowncast<NSDictionary>(item.representedObject);
    if (!representedObject)
        return nil;

    id action = [representedObject objectForKey:@"DDAction"];

#if HAVE(DATA_DETECTORS_MAC_ACTION)
    if (![action isKindOfClass:PAL::getDDMacActionClass()])
        return nil;
#else
    if (![action isKindOfClass:PAL::getDDActionClass()])
        return nil;
#endif

    return action;
}

NSMenuItem *menuItemForTelephoneNumber(const String& telephoneNumber)
{
    if (!PAL::isDataDetectorsFrameworkAvailable())
        return nil;

    auto actionContext = adoptNS([PAL::allocWKDDActionContextInstance() init]);

    [actionContext setAllowedActionUTIs:@[ @"com.apple.dial" ]];

    RetainPtr<NSArray> proposedMenuItems = [[PAL::getDDActionsManagerClass() sharedManager] menuItemsForValue:telephoneNumber.createNSString().get() type:PAL::get_DataDetectorsCore_DDBinderPhoneNumberKey() service:nil context:actionContext.get()];
    for (NSMenuItem *item in proposedMenuItems.get()) {
        RetainPtr action = actionForMenuItem(item);
        if ([action.get().actionUTI hasPrefix:@"com.apple.dial"]) {
            item.title = formattedPhoneNumberString(telephoneNumber.createNSString().get());
            return item;
        }
    }

    return nil;
}

RetainPtr<NSMenu> menuForTelephoneNumber(const String& telephoneNumber, NSView *webView, const WebCore::IntRect& rect)
{
    if (!PAL::isRevealFrameworkAvailable() || !PAL::isRevealCoreFrameworkAvailable())
        return nil;

    RetainPtr<NSMenu> menu = adoptNS([[NSMenu alloc] init]);
    auto urlComponents = adoptNS([[NSURLComponents alloc] init]);
    [urlComponents setScheme:@"tel"];
    [urlComponents setPath:telephoneNumber.createNSString().get()];
    auto item = adoptNS([PAL::allocRVItemInstance() initWithURL:[urlComponents URL] rangeInContext:NSMakeRange(0, telephoneNumber.length())]);
    auto presenter = adoptNS([PAL::allocRVPresenterInstance() init]);
    auto delegate = adoptNS([[WKEmptyPresenterHighlightDelegate alloc] initWithRect:rect]);
    auto context = WebCore::createRVPresentingContextWithRetainedDelegate(NSZeroPoint, webView, delegate.get());
    NSArray *proposedMenuItems = [presenter menuItemsForItem:item.get() documentContext:nil presentingContext:context.get() options:nil];
    
    [menu setItemArray:proposedMenuItems];

    return menu;
}

#endif

#if ENABLE(CONTEXT_MENU_IMAGES_FOR_INTERNAL_CLIENTS)

static NSString *symbolNameForAction(const WebCore::ContextMenuAction action, bool useAlternateImage)
{
    if (![NSMenuItem respondsToSelector:@selector(_systemImageNameForAction:)])
        return nil;

    switch (action) {
    case WebCore::ContextMenuItemBaseApplicationTag:
    case WebCore::ContextMenuItemBaseCustomTag:
    case WebCore::ContextMenuItemLastCustomTag:
    case WebCore::ContextMenuItemPDFContinuous:
    case WebCore::ContextMenuItemPDFFacingPages:
    case WebCore::ContextMenuItemPDFSinglePage:
    case WebCore::ContextMenuItemPDFSinglePageContinuous:
    case WebCore::ContextMenuItemPDFTwoPages:
    case WebCore::ContextMenuItemPDFTwoPagesContinuous:
    case WebCore::ContextMenuItemTagCheckGrammarWithSpelling:
    case WebCore::ContextMenuItemTagCheckSpellingWhileTyping:
    case WebCore::ContextMenuItemTagCopyLinkWithHighlight:
    case WebCore::ContextMenuItemTagCopySubject:
    case WebCore::ContextMenuItemTagCorrectSpellingAutomatically:
    case WebCore::ContextMenuItemTagDictationAlternative:
    case WebCore::ContextMenuItemTagFontMenu:
    case WebCore::ContextMenuItemTagNoAction:
    case WebCore::ContextMenuItemTagNoGuessesFound:
    case WebCore::ContextMenuItemTagOther:
    case WebCore::ContextMenuItemTagOutline:
    case WebCore::ContextMenuItemTagPDFFacingPagesScrolling:
    case WebCore::ContextMenuItemTagPDFSinglePageScrolling:
    case WebCore::ContextMenuItemTagSmartCopyPaste:
    case WebCore::ContextMenuItemTagSmartDashes:
    case WebCore::ContextMenuItemTagSmartLinks:
    case WebCore::ContextMenuItemTagSmartQuotes:
    case WebCore::ContextMenuItemTagSpeechMenu:
    case WebCore::ContextMenuItemTagSpellingGuess:
    case WebCore::ContextMenuItemTagSpellingMenu:
    case WebCore::ContextMenuItemTagStyles:
    case WebCore::ContextMenuItemTagSubstitutionsMenu:
    case WebCore::ContextMenuItemTagTextDirectionMenu:
    case WebCore::ContextMenuItemTagTextReplacement:
    case WebCore::ContextMenuItemTagTransformationsMenu:
    case WebCore::ContextMenuItemTagWritingDirectionMenu:
        return nil;
    case WebCore::ContextMenuItemTagWritingTools:
        return @"apple.writing.tools";
    case WebCore::ContextMenuItemTagProofread:
        return @"text.magnifyingglass";
    case WebCore::ContextMenuItemTagRewrite:
        return @"pencil.arrow.trianglehead.clockwise";
    case WebCore::ContextMenuItemTagSummarize:
        return @"text.line.3.summary";
    case WebCore::ContextMenuItemPDFAutoSize:
        return @"sparkle.magnifyingglass";
    case WebCore::ContextMenuItemPDFActualSize:
        return @"text.magnifyingglass";
    case WebCore::ContextMenuItemPDFNextPage:
        return @"chevron.down";
    case WebCore::ContextMenuItemPDFPreviousPage:
        return @"chevron.up";
    case WebCore::ContextMenuItemPDFZoomIn:
        return @"plus.magnifyingglass";
    case WebCore::ContextMenuItemPDFZoomOut:
        return @"minus.magnifyingglass";
    case WebCore::ContextMenuItemTagAddHighlightToCurrentQuickNote:
    case WebCore::ContextMenuItemTagAddHighlightToNewQuickNote:
        return @"quicknote";
    case WebCore::ContextMenuItemTagBold:
        return @"bold";
    case WebCore::ContextMenuItemTagCapitalize:
        return @"textformat.characters";
    case WebCore::ContextMenuItemTagChangeBack:
        return @"arrow.uturn.backward.circle";
    case WebCore::ContextMenuItemTagCheckSpelling:
        return @"text.page.badge.magnifyingglass";
    case WebCore::ContextMenuItemTagCopy:
    case WebCore::ContextMenuItemTagCopyImageToClipboard:
    case WebCore::ContextMenuItemTagCopyLinkToClipboard:
    case WebCore::ContextMenuItemTagCopyMediaLinkToClipboard:
        return [NSMenuItem _systemImageNameForAction:@selector(copy:)];
    case WebCore::ContextMenuItemTagCut:
        return [NSMenuItem _systemImageNameForAction:@selector(cut:)];
    case WebCore::ContextMenuItemTagDefaultDirection:
    case WebCore::ContextMenuItemTagTextDirectionDefault:
        return @"arrow.left.arrow.right";
    case WebCore::ContextMenuItemTagDownloadImageToDisk:
    case WebCore::ContextMenuItemTagDownloadLinkToDisk:
    case WebCore::ContextMenuItemTagDownloadMediaToDisk:
        return @"square.and.arrow.down";
    case WebCore::ContextMenuItemTagEnterVideoFullscreen:
        return @"arrow.up.left.and.arrow.down.right";
    case WebCore::ContextMenuItemTagGoBack:
        return @"chevron.backward";
    case WebCore::ContextMenuItemTagGoForward:
        return @"chevron.forward";
    case WebCore::ContextMenuItemTagIgnoreGrammar:
    case WebCore::ContextMenuItemTagIgnoreSpelling:
        return @"checkmark.circle";
    case WebCore::ContextMenuItemTagInspectElement:
        return @"gear";
    case WebCore::ContextMenuItemTagItalic:
        return @"italic";
    case WebCore::ContextMenuItemTagLearnSpelling:
        return @"text.book.closed";
    case WebCore::ContextMenuItemTagLeftToRight:
        return [NSMenuItem _systemImageNameForAction:@selector(makeTextWritingDirectionLeftToRight:)];
    case WebCore::ContextMenuItemTagLookUpImage:
        return @"info.circle.badge.sparkles";
    case WebCore::ContextMenuItemTagLookUpInDictionary:
        return @"character.book.closed";
    case WebCore::ContextMenuItemTagMakeLowerCase:
        return @"characters.lowercase";
    case WebCore::ContextMenuItemTagMakeUpperCase:
        return @"characters.uppercase";
    case WebCore::ContextMenuItemTagMediaMute:
        return @"speaker.slash";
    case WebCore::ContextMenuItemTagMediaPlayPause:
        return useAlternateImage ? @"pause.fill" : @"play.fill";
    case WebCore::ContextMenuItemTagOpenFrameInNewWindow:
    case WebCore::ContextMenuItemTagOpenImageInNewWindow:
    case WebCore::ContextMenuItemTagOpenLinkInNewWindow:
    case WebCore::ContextMenuItemTagOpenMediaInNewWindow:
        return @"macwindow.badge.plus";
    case WebCore::ContextMenuItemTagOpenLink:
        return @"safari";
    case WebCore::ContextMenuItemTagOpenWithDefaultApplication:
        return @"arrow.up.forward.app";
    case WebCore::ContextMenuItemTagPaste:
        return [NSMenuItem _systemImageNameForAction:@selector(paste:)];
    case WebCore::ContextMenuItemTagPauseAllAnimations:
        return @"rectangle.stack.badge.minus";
    case WebCore::ContextMenuItemTagPauseAnimation:
        return @"pause.rectangle";
    case WebCore::ContextMenuItemTagPlayAllAnimations:
        return @"rectangle.stack.badge.play.fill";
    case WebCore::ContextMenuItemTagPlayAnimation:
        return @"play.rectangle";
    case WebCore::ContextMenuItemTagReload:
        return @"arrow.clockwise";
    case WebCore::ContextMenuItemTagRightToLeft:
        return [NSMenuItem _systemImageNameForAction:@selector(makeTextWritingDirectionRightToLeft:)];
    case WebCore::ContextMenuItemTagSearchWeb:
        return @"magnifyingglass";
    case WebCore::ContextMenuItemTagShareMenu:
        return @"square.and.arrow.up";
    case WebCore::ContextMenuItemTagShowColors:
        return @"paintpalette";
    case WebCore::ContextMenuItemTagShowFonts:
        return @"text.and.command.macwindow";
    case WebCore::ContextMenuItemTagShowMediaStats:
        return @"info.circle";
    case WebCore::ContextMenuItemTagShowSpellingPanel:
    case WebCore::ContextMenuItemTagShowSubstitutions:
        return useAlternateImage ? @"eye.slash" : @"text.and.command.macwindow";
    case WebCore::ContextMenuItemTagStartSpeaking:
        return @"play.fill";
    case WebCore::ContextMenuItemTagStop:
    case WebCore::ContextMenuItemTagStopSpeaking:
        return @"stop.fill";
    case WebCore::ContextMenuItemTagTextDirectionLeftToRight:
        return @"arrow.right";
    case WebCore::ContextMenuItemTagTextDirectionRightToLeft:
        return @"arrow.left";
    case WebCore::ContextMenuItemTagToggleMediaControls:
        return useAlternateImage ? @"eye" : @"eye.slash";
    case WebCore::ContextMenuItemTagToggleMediaLoop:
        return @"arrow.2.squarepath";
    case WebCore::ContextMenuItemTagToggleVideoEnhancedFullscreen:
        return useAlternateImage ? @"pip.exit" : @"pip.enter";
    case WebCore::ContextMenuItemTagToggleVideoFullscreen:
        return useAlternateImage ? @"arrow.down.right.and.arrow.up.left" : @"arrow.up.backward.and.arrow.down.forward";
    case WebCore::ContextMenuItemTagToggleVideoViewer:
        return useAlternateImage ? @"rectangle.slash" : @"rectangle.expand.diagonal";
    case WebCore::ContextMenuItemTagTranslate:
        return @"translate";
    case WebCore::ContextMenuItemTagUnderline:
        return [NSMenuItem _systemImageNameForAction:@selector(underline:)];
    }

    return nil;
}

void addImageToMenuItem(NSMenuItem *item, const WebCore::ContextMenuAction action, bool useAlternateImage)
{
    RetainPtr symbolName = symbolNameForAction(action, useAlternateImage);
    auto isPrivate = action == WebCore::ContextMenuItemTagLookUpImage;

    if (isPrivate)
        [item _setActionImage:[NSImage imageWithPrivateSystemSymbolName:symbolName.get() accessibilityDescription:nil]];
    else
        [item _setActionImage:[NSImage imageWithSystemSymbolName:symbolName.get() accessibilityDescription:nil]];
}

#endif

} // namespace WebKit

#endif
