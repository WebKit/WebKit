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
    auto *representedObject = dynamic_objc_cast<NSDictionary>(item.representedObject);
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

    NSArray *proposedMenuItems = [[PAL::getDDActionsManagerClass() sharedManager] menuItemsForValue:(NSString *)telephoneNumber type:PAL::get_DataDetectorsCore_DDBinderPhoneNumberKey() service:nil context:actionContext.get()];
    for (NSMenuItem *item in proposedMenuItems) {
        auto action = actionForMenuItem(item);
        if ([action.actionUTI hasPrefix:@"com.apple.dial"]) {
            item.title = formattedPhoneNumberString(telephoneNumber);
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
    [urlComponents setPath:telephoneNumber];
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

NSString *symbolNameForAction(const WebCore::ContextMenuAction action, bool useAlternateImage)
{
    // FIXME: <rdar://142002509> The chosen images need to be reviewed.
    if (![NSMenuItem respondsToSelector:@selector(_systemImageNameForAction:)])
        return nil;

    switch (action) {
    case WebCore::ContextMenuItemBaseApplicationTag:
    case WebCore::ContextMenuItemBaseCustomTag:
    case WebCore::ContextMenuItemLastCustomTag:
    case WebCore::ContextMenuItemTagDictationAlternative:
    case WebCore::ContextMenuItemTagFontMenu:
    case WebCore::ContextMenuItemTagNoAction:
    case WebCore::ContextMenuItemTagNoGuessesFound:
    case WebCore::ContextMenuItemTagOther:
    case WebCore::ContextMenuItemTagSpellingGuess:
    case WebCore::ContextMenuItemTagSpeechMenu:
    case WebCore::ContextMenuItemTagSpellingMenu:
    case WebCore::ContextMenuItemTagSubstitutionsMenu:
    case WebCore::ContextMenuItemTagTextDirectionMenu:
    case WebCore::ContextMenuItemTagTransformationsMenu:
    case WebCore::ContextMenuItemTagWritingDirectionMenu:
    case WebCore::ContextMenuItemTagWritingTools:
        return nil;
    case WebCore::ContextMenuItemPDFAutoSize:
        return @"sparkle.magnifyingglass";
    case WebCore::ContextMenuItemPDFActualSize:
        return @"text.magnifyingglass";
    case WebCore::ContextMenuItemPDFContinuous:
    case WebCore::ContextMenuItemPDFSinglePageContinuous:
    case WebCore::ContextMenuItemTagPDFSinglePageScrolling:
        return @"rectangle.stack";
    case WebCore::ContextMenuItemPDFFacingPages:
    case WebCore::ContextMenuItemPDFTwoPages:
        return @"rectangle.split.2x1";
    case WebCore::ContextMenuItemPDFNextPage:
        return @"chevron.right";
    case WebCore::ContextMenuItemPDFPreviousPage:
        return @"chevron.left";
    case WebCore::ContextMenuItemPDFSinglePage:
        return @"rectangle.portrait";
    case WebCore::ContextMenuItemPDFTwoPagesContinuous:
    case WebCore::ContextMenuItemTagPDFFacingPagesScrolling:
        return @"rectangle.stack.fill";
    case WebCore::ContextMenuItemPDFZoomIn:
        return @"plus.magnifyingglass";
    case WebCore::ContextMenuItemPDFZoomOut:
        return @"minus.magnifyingglass";
    case WebCore::ContextMenuItemTagAddHighlightToCurrentQuickNote:
    case WebCore::ContextMenuItemTagAddHighlightToNewQuickNote:
        return @"append.page";
    case WebCore::ContextMenuItemTagBold:
        return @"bold";
    case WebCore::ContextMenuItemTagCapitalize:
        return @"textformat.characters";
    case WebCore::ContextMenuItemTagChangeBack:
        return @"arrow.uturn.backward.circle";
    case WebCore::ContextMenuItemTagCheckGrammarWithSpelling:
        return @"character.magnify";
    case WebCore::ContextMenuItemTagCheckSpelling:
        return @"text.page.badge.magnifyingglass";
    case WebCore::ContextMenuItemTagCheckSpellingWhileTyping:
        return @"character.cursor.ibeam";
    case WebCore::ContextMenuItemTagCopy:
    case WebCore::ContextMenuItemTagCopyImageToClipboard:
    case WebCore::ContextMenuItemTagCopyLinkToClipboard:
    case WebCore::ContextMenuItemTagCopyMediaLinkToClipboard:
        return [NSMenuItem _systemImageNameForAction:@selector(copy:)];
    case WebCore::ContextMenuItemTagCopyLinkWithHighlight:
        return @"text.quote";
    case WebCore::ContextMenuItemTagCopySubject:
        return @"person.fill.viewfinder";
    case WebCore::ContextMenuItemTagCorrectSpellingAutomatically:
        return @"keyboard.badge.eye";
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
        return @"doc.badge.gearshape";
    case WebCore::ContextMenuItemTagItalic:
        return @"italic";
    case WebCore::ContextMenuItemTagLearnSpelling:
        return @"text.book.closed";
    case WebCore::ContextMenuItemTagLeftToRight:
        return [NSMenuItem _systemImageNameForAction:@selector(makeTextWritingDirectionLeftToRight:)];
    case WebCore::ContextMenuItemTagLookUpImage:
    case WebCore::ContextMenuItemTagSearchWeb:
        return @"magnifyingglass";
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
    case WebCore::ContextMenuItemTagOutline:
        return @"character.circle";
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
    case WebCore::ContextMenuItemTagShareMenu:
        return @"square.and.arrow.up";
    case WebCore::ContextMenuItemTagShowColors:
        return @"paintpalette";
    case WebCore::ContextMenuItemTagShowFonts:
        return @"text.and.command.macwindow";
    case WebCore::ContextMenuItemTagShowMediaStats:
        return @"info";
    case WebCore::ContextMenuItemTagShowSpellingPanel:
    case WebCore::ContextMenuItemTagShowSubstitutions:
        return useAlternateImage ? @"eye.slash" : @"text.and.command.macwindow";
    case WebCore::ContextMenuItemTagSmartCopyPaste:
        return @"list.clipboard";
    case WebCore::ContextMenuItemTagSmartDashes:
        return @"arrowtriangle.right.and.line.vertical.and.arrowtriangle.left";
    case WebCore::ContextMenuItemTagSmartLinks:
        return @"link";
    case WebCore::ContextMenuItemTagSmartQuotes:
        return @"quote.closing";
    case WebCore::ContextMenuItemTagStartSpeaking:
        return @"play.fill";
    case WebCore::ContextMenuItemTagStop:
    case WebCore::ContextMenuItemTagStopSpeaking:
        return @"stop.fill";
    case WebCore::ContextMenuItemTagStyles:
        return @"bold.italic.underline";
    case WebCore::ContextMenuItemTagTextDirectionLeftToRight:
        return @"arrow.left.to.line";
    case WebCore::ContextMenuItemTagTextDirectionRightToLeft:
        return @"arrow.right.to.line";
    case WebCore::ContextMenuItemTagTextReplacement:
        return @"text.page.badge.magnifyingglass";
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
        return @"underline";
    }

    return nil;
}

#endif

} // namespace WebKit

#endif
