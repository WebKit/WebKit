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

#if ENABLE(CONTEXT_MENU_IMAGES_ON_MAC)
#import <WebCore/LocaleToScriptMapping.h>
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
using namespace WebCore;

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

#if ENABLE(CONTEXT_MENU_IMAGES_ON_MAC)

enum class SymbolType : bool { Public, Private };

struct SymbolNameWithType {
    SymbolType type;
    String name;
};

static std::optional<SymbolNameWithType> symbolForTransformationItem(String symbolName)
{
    // The images used for the items in the transformation submenu are not all localized
    // for the same scripts, so we must ensure that the images are shown only when a
    // localized version exists for all 3.

    RetainPtr currentLocale = [NSLocale currentLocale];
    RetainPtr scriptCode = [currentLocale scriptCode];
    RetainPtr languageCode = [currentLocale languageCode];

    const auto isoScriptCode = scriptCode ? scriptNameToCode(String(scriptCode.get())) : localeToScriptCode(languageCode.get());

    switch (isoScriptCode) {
    case USCRIPT_CYRILLIC:
    case USCRIPT_GREEK:
    case USCRIPT_LATIN:
        return { { SymbolType::Public, symbolName } };
    default:
        return { };
    }
}

static std::optional<SymbolNameWithType> symbolNameWithTypeForAction(const WebCore::ContextMenuAction action, bool useAlternateImage)
{
    if (![NSMenuItem respondsToSelector:@selector(_systemImageNameForAction:)])
        return { };

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
        return { };
    case WebCore::ContextMenuItemTagWritingTools:
        return { { SymbolType::Public, "apple.writing.tools"_s } };
    case WebCore::ContextMenuItemTagProofread:
        return { { SymbolType::Public, "text.magnifyingglass"_s } };
    case WebCore::ContextMenuItemTagRewrite:
        return { { SymbolType::Private, "pencil.arrow.trianglehead.clockwise"_s } };
    case WebCore::ContextMenuItemTagSummarize:
        return { { SymbolType::Private, "text.line.3.summary"_s } };
    case WebCore::ContextMenuItemPDFAutoSize:
        return { { SymbolType::Public, "sparkle.magnifyingglass"_s } };
    case WebCore::ContextMenuItemPDFActualSize:
        return { { SymbolType::Public, "text.magnifyingglass"_s } };
    case WebCore::ContextMenuItemPDFNextPage:
        return { { SymbolType::Public, "chevron.down"_s } };
    case WebCore::ContextMenuItemPDFPreviousPage:
        return { { SymbolType::Public, "chevron.up"_s } };
    case WebCore::ContextMenuItemPDFZoomIn:
        return { { SymbolType::Public, "plus.magnifyingglass"_s } };
    case WebCore::ContextMenuItemPDFZoomOut:
        return { { SymbolType::Public, "minus.magnifyingglass"_s } };
    case WebCore::ContextMenuItemTagAddHighlightToCurrentQuickNote:
    case WebCore::ContextMenuItemTagAddHighlightToNewQuickNote:
        return { { SymbolType::Private, "quicknote"_s } };
    case WebCore::ContextMenuItemTagBold:
        return { { SymbolType::Public, "bold"_s } };
    case WebCore::ContextMenuItemTagCapitalize:
        return symbolForTransformationItem("textformat.characters"_s);
    case WebCore::ContextMenuItemTagChangeBack:
        return { { SymbolType::Public, "arrow.uturn.backward.circle"_s } };
    case WebCore::ContextMenuItemTagCheckSpelling:
        return { { SymbolType::Public, "text.page.badge.magnifyingglass"_s } };
    case WebCore::ContextMenuItemTagCopy:
    case WebCore::ContextMenuItemTagCopyImageToClipboard:
    case WebCore::ContextMenuItemTagCopyLinkToClipboard:
    case WebCore::ContextMenuItemTagCopyMediaLinkToClipboard:
        return { { SymbolType::Public, [NSMenuItem _systemImageNameForAction:@selector(copy:)] } };
    case WebCore::ContextMenuItemTagCut:
        return { { SymbolType::Public, [NSMenuItem _systemImageNameForAction:@selector(cut:)] } };
    case WebCore::ContextMenuItemTagDefaultDirection:
    case WebCore::ContextMenuItemTagTextDirectionDefault:
        return { { SymbolType::Public, "arrow.left.arrow.right"_s } };
    case WebCore::ContextMenuItemTagDownloadImageToDisk:
    case WebCore::ContextMenuItemTagDownloadLinkToDisk:
    case WebCore::ContextMenuItemTagDownloadMediaToDisk:
        return { { SymbolType::Public, "square.and.arrow.down"_s } };
    case WebCore::ContextMenuItemTagEnterVideoFullscreen:
        return { { SymbolType::Public, "arrow.up.left.and.arrow.down.right"_s } };
    case WebCore::ContextMenuItemTagGoBack:
        return { { SymbolType::Public, "chevron.backward"_s } };
    case WebCore::ContextMenuItemTagGoForward:
        return { { SymbolType::Public, "chevron.forward"_s } };
    case WebCore::ContextMenuItemTagIgnoreGrammar:
    case WebCore::ContextMenuItemTagIgnoreSpelling:
        return { { SymbolType::Public, "checkmark.circle"_s } };
    case WebCore::ContextMenuItemTagInspectElement:
        return { { SymbolType::Public, "wrench.and.screwdriver"_s } };
    case WebCore::ContextMenuItemTagItalic:
        return { { SymbolType::Public, "italic"_s } };
    case WebCore::ContextMenuItemTagLearnSpelling:
        return { { SymbolType::Public, "text.book.closed"_s } };
    case WebCore::ContextMenuItemTagLeftToRight:
    case WebCore::ContextMenuItemTagTextDirectionLeftToRight:
        return { { SymbolType::Public, "arrow.right"_s } };
    case WebCore::ContextMenuItemTagLookUpImage:
        return { { SymbolType::Private, "info.circle.badge.sparkles"_s } };
    case WebCore::ContextMenuItemTagLookUpInDictionary:
        return { { SymbolType::Public, "character.book.closed"_s } };
    case WebCore::ContextMenuItemTagMakeLowerCase:
        return symbolForTransformationItem("characters.lowercase"_s);
    case WebCore::ContextMenuItemTagMakeUpperCase:
        return symbolForTransformationItem("characters.uppercase"_s);
    case WebCore::ContextMenuItemTagMediaMute:
        return { { SymbolType::Public, "speaker.slash"_s } };
    case WebCore::ContextMenuItemTagMediaPlayPause: {
        const auto symbolName = useAlternateImage ? "pause.fill"_s : "play.fill"_s;
        return { { SymbolType::Public, symbolName } };
    }
    case WebCore::ContextMenuItemTagOpenFrameInNewWindow:
    case WebCore::ContextMenuItemTagOpenImageInNewWindow:
    case WebCore::ContextMenuItemTagOpenLinkInNewWindow:
    case WebCore::ContextMenuItemTagOpenMediaInNewWindow:
        return { { SymbolType::Public, "macwindow.badge.plus"_s } };
    case WebCore::ContextMenuItemTagOpenLink:
        return { { SymbolType::Public, "safari"_s } };
    case WebCore::ContextMenuItemTagOpenWithDefaultApplication:
        return { { SymbolType::Public, "arrow.up.forward.app"_s } };
    case WebCore::ContextMenuItemTagPaste:
        return { { SymbolType::Public, [NSMenuItem _systemImageNameForAction:@selector(paste:)] } };
    case WebCore::ContextMenuItemTagPauseAllAnimations:
        return { { SymbolType::Public, "rectangle.stack.badge.minus"_s } };
    case WebCore::ContextMenuItemTagPauseAnimation:
        return { { SymbolType::Public, "pause.rectangle"_s } };
    case WebCore::ContextMenuItemTagPlayAllAnimations:
        return { { SymbolType::Public, "rectangle.stack.badge.play.fill"_s } };
    case WebCore::ContextMenuItemTagPlayAnimation:
        return { { SymbolType::Public, "play.rectangle"_s } };
    case WebCore::ContextMenuItemTagReload:
        return { { SymbolType::Public, "arrow.clockwise"_s } };
    case WebCore::ContextMenuItemTagRightToLeft:
    case WebCore::ContextMenuItemTagTextDirectionRightToLeft:
        return { { SymbolType::Public, "arrow.left"_s } };
    case WebCore::ContextMenuItemTagSearchWeb:
        return { { SymbolType::Public, "magnifyingglass"_s } };
    case WebCore::ContextMenuItemTagShareMenu:
        return { { SymbolType::Public, "square.and.arrow.up"_s } };
    case WebCore::ContextMenuItemTagShowColors:
        return { { SymbolType::Public, "paintpalette"_s } };
    case WebCore::ContextMenuItemTagShowFonts:
        return { { SymbolType::Public, "text.and.command.macwindow"_s } };
    case WebCore::ContextMenuItemTagShowMediaStats:
        return { { SymbolType::Public, "info.circle"_s } };
    case WebCore::ContextMenuItemTagShowSpellingPanel:
    case WebCore::ContextMenuItemTagShowSubstitutions: {
        const auto symbolName = useAlternateImage ? "eye.slash"_s : "text.and.command.macwindow"_s;
        return { { SymbolType::Public, symbolName } };
    }
    case WebCore::ContextMenuItemTagStartSpeaking:
        return { { SymbolType::Public, "play.fill"_s } };
    case WebCore::ContextMenuItemTagStop:
    case WebCore::ContextMenuItemTagStopSpeaking:
        return { { SymbolType::Public, "stop.fill"_s } };
    case WebCore::ContextMenuItemTagToggleMediaControls: {
        const auto symbolName = useAlternateImage ? "eye"_s : "eye.slash"_s;
        return { { SymbolType::Public, symbolName } };
    }
    case WebCore::ContextMenuItemTagToggleMediaLoop:
        return { { SymbolType::Public, "arrow.2.squarepath"_s } };
    case WebCore::ContextMenuItemTagToggleVideoEnhancedFullscreen: {
        const auto symbolName =  useAlternateImage ? "pip.exit"_s : "pip.enter"_s;
        return { { SymbolType::Public, symbolName } };
    }
    case WebCore::ContextMenuItemTagToggleVideoFullscreen: {
        const auto symbolName =  useAlternateImage ? "arrow.down.right.and.arrow.up.left"_s : "arrow.up.backward.and.arrow.down.forward"_s;
        return { { SymbolType::Public, symbolName } };
    }
    case WebCore::ContextMenuItemTagToggleVideoViewer: {
        const auto symbolName =  useAlternateImage ? "rectangle.slash"_s : "rectangle.expand.diagonal"_s;
        return { { SymbolType::Public, symbolName } };
    }
    case WebCore::ContextMenuItemTagTranslate:
        return { { SymbolType::Public, "translate"_s } };
    case WebCore::ContextMenuItemTagUnderline:
        return { { SymbolType::Public, [NSMenuItem _systemImageNameForAction:@selector(underline:)] } };
    }

    return { };
}

void addImageToMenuItem(NSMenuItem *item, const WebCore::ContextMenuAction action, bool useAlternateImage)
{
    if (auto symbolNameWithType = symbolNameWithTypeForAction(action, useAlternateImage)) {
        const auto symbolType = symbolNameWithType.value().type;
        RetainPtr symbolName = symbolNameWithType.value().name.createNSString();

        if (symbolType == SymbolType::Public)
            [item _setActionImage:[NSImage imageWithSystemSymbolName:symbolName.get() accessibilityDescription:nil]];
        else
            [item _setActionImage:[NSImage imageWithPrivateSystemSymbolName:symbolName.get() accessibilityDescription:nil]];
    }
}

#endif

} // namespace WebKit

#endif
