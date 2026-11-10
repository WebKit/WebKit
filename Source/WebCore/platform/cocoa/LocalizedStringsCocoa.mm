/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
#import "LocalizedStrings.h"

#import "NotImplemented.h"
#import <pal/system/mac/DefaultSearchProvider.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

static RetainPtr<NSString> localizedPercentage(double percent)
{
    RetainPtr numberFormatter = adoptNS([[NSNumberFormatter alloc] init]);
    [numberFormatter setLocale:[NSLocale currentLocale]];
    [numberFormatter setNumberStyle:NSNumberFormatterPercentStyle];
    [numberFormatter setMinimumFractionDigits:0];
    [numberFormatter setMaximumFractionDigits:0];
    return [numberFormatter stringFromNumber:@(percent)];
}

RefPtr<StringImpl> AXProcessingPage(double percent)
{
    return WEB_UI_FORMAT_STRING("Processing page %@", "Title for the webarea while the accessibility tree is being built.", localizedPercentage(percent).get());
}

RefPtr<StringImpl> copyImageUnknownFileLabel()
{
    return WEB_UI_STRING("unknown", "Unknown filename");
}

#if ENABLE(APP_HIGHLIGHTS)
RefPtr<StringImpl> contextMenuItemTagAddHighlightToCurrentQuickNote()
{
    return StringImpl::create(protect(WEB_UI_NSSTRING(@"Add to Quick Note", "Add to Quick Note context menu item.")).get());
}

RefPtr<StringImpl> contextMenuItemTagAddHighlightToNewQuickNote()
{
    return StringImpl::create(protect(WEB_UI_NSSTRING(@"New Quick Note", "New Quick Note context menu item.")).get());
}
#endif

#if ENABLE(CONTEXT_MENUS)
RefPtr<StringImpl> contextMenuItemTagSearchWeb()
{
    auto searchProviderName = PAL::defaultSearchProviderDisplayName();
    return WEB_UI_FORMAT_CFSTRING("Search with %@", "Search with search provider context menu item with provider name inserted", searchProviderName.get());
}

RefPtr<StringImpl> contextMenuItemTagShowFonts()
{
    return WEB_UI_STRING("Show Fonts", "Show fonts context menu item");
}

RefPtr<StringImpl> contextMenuItemTagStyles()
{
    return WEB_UI_STRING("Styles…", "Styles context menu item");
}

RefPtr<StringImpl> contextMenuItemTagShowColors()
{
    return WEB_UI_STRING("Show Colors", "Show colors context menu item");
}

RefPtr<StringImpl> contextMenuItemTagSpeechMenu()
{
    return WEB_UI_STRING("Speech", "Speech context sub-menu item");
}

RefPtr<StringImpl> contextMenuItemTagStartSpeaking()
{
    return WEB_UI_STRING("Start Speaking", "Start speaking context menu item");
}

RefPtr<StringImpl> contextMenuItemTagStopSpeaking()
{
    return WEB_UI_STRING("Stop Speaking", "Stop speaking context menu item");
}

RefPtr<StringImpl> contextMenuItemTagCorrectSpellingAutomatically()
{
    return WEB_UI_STRING("Correct Spelling Automatically", "Correct Spelling Automatically context menu item");
}

RefPtr<StringImpl> contextMenuItemTagSubstitutionsMenu()
{
    return WEB_UI_STRING("Substitutions", "Substitutions context sub-menu item");
}

RefPtr<StringImpl> contextMenuItemTagShowSubstitutions(bool show)
{
    if (show)
        return WEB_UI_STRING("Show Substitutions", "menu item title");
    return WEB_UI_STRING("Hide Substitutions", "menu item title");
}

RefPtr<StringImpl> contextMenuItemTagSmartCopyPaste()
{
    return WEB_UI_STRING("Smart Copy/Paste", "Smart Copy/Paste context menu item");
}

RefPtr<StringImpl> contextMenuItemTagSmartQuotes()
{
    return WEB_UI_STRING("Smart Quotes", "Smart Quotes context menu item");
}

RefPtr<StringImpl> contextMenuItemTagSmartDashes()
{
    return WEB_UI_STRING("Smart Dashes", "Smart Dashes context menu item");
}

RefPtr<StringImpl> contextMenuItemTagSmartLinks()
{
    return WEB_UI_STRING("Smart Links", "Smart Links context menu item");
}

RefPtr<StringImpl> contextMenuItemTagSmartLists()
{
    return WEB_UI_STRING("Smart Lists", "Smart Lists context menu item");
}

RefPtr<StringImpl> contextMenuItemTagTextReplacement()
{
    return WEB_UI_STRING("Text Replacement", "Text Replacement context menu item");
}

RefPtr<StringImpl> contextMenuItemTagTransformationsMenu()
{
    return WEB_UI_STRING("Transformations", "Transformations context sub-menu item");
}

RefPtr<StringImpl> contextMenuItemTagMakeUpperCase()
{
    return WEB_UI_STRING("Make Upper Case", "Make Upper Case context menu item");
}

RefPtr<StringImpl> contextMenuItemTagMakeLowerCase()
{
    return WEB_UI_STRING("Make Lower Case", "Make Lower Case context menu item");
}

RefPtr<StringImpl> contextMenuItemTagCapitalize()
{
    return WEB_UI_STRING("Capitalize", "Capitalize context menu item");
}

RefPtr<StringImpl> contextMenuItemTagConvertToTraditionalChinese()
{
    return WEB_UI_STRING("Convert to Traditional Chinese", "Convert to Traditional Chinese context menu item");
}

RefPtr<StringImpl> contextMenuItemTagConvertToSimplifiedChinese()
{
    return WEB_UI_STRING("Convert to Simplified Chinese", "Convert to Simplified Chinese context menu item");
}

RefPtr<StringImpl> contextMenuItemTagChangeBack(StringImpl* replacedString)
{
    notImplemented();
    return replacedString;
}

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
RefPtr<StringImpl> contextMenuItemTagEnterPictureInPicture()
{
    return WEB_UI_STRING("Enter Picture in Picture", "menu item");
}

RefPtr<StringImpl> contextMenuItemTagExitPictureInPicture()
{
    return WEB_UI_STRING("Exit Picture in Picture", "menu item");
}

RefPtr<StringImpl> contextMenuItemTagEnterVideoViewer()
{
    return WEB_UI_STRING("Enter Viewer", "Enter Video Viewer context menu item");
}

RefPtr<StringImpl> contextMenuItemTagExitVideoViewer()
{
    return WEB_UI_STRING("Exit Viewer", "Exit Video Viewer context menu item");
}
#endif
#endif // ENABLE(CONTEXT_MENUS)

RefPtr<StringImpl> AXMeterGaugeRegionOptimumText()
{
    return WEB_UI_STRING("optimal value", "The optimum value description for a meter element.");
}

RefPtr<StringImpl> AXMeterGaugeRegionSuboptimalText()
{
    return WEB_UI_STRING("suboptimal value", "The suboptimal value description for a meter element.");
}

RefPtr<StringImpl> AXMeterGaugeRegionLessGoodText()
{
    return WEB_UI_STRING("critical value", "The less good value description for a meter element.");
}

RefPtr<StringImpl> pdfDocumentTypeDescription()
{
    // Also exposed to DOM.
    return WEB_UI_STRING_KEY("Portable Document Format", "Portable Document Format (Safari)", "Description of the primary type supported by the PDF pseudo plug-in. Visible in the Installed Plug-ins page in Safari.");
}

#if PLATFORM(IOS_FAMILY)
RefPtr<StringImpl> fileButtonChooseMediaFileLabel()
{
    return WEB_UI_STRING("Choose Media (Single)", "Title for file button used in HTML forms for media files");
}

RefPtr<StringImpl> fileButtonChooseMultipleMediaFilesLabel()
{
    return WEB_UI_STRING("Choose Media (Multiple)", "Title for file button used in HTML5 forms for multiple media files");
}

RefPtr<StringImpl> fileButtonNoMediaFileSelectedLabel()
{
    return WEB_UI_STRING("no media selected (single)", "Text to display in file button used in HTML forms for media files when no media file is selected");
}

RefPtr<StringImpl> fileButtonNoMediaFilesSelectedLabel()
{
    return WEB_UI_STRING("no media selected (multiple)", "Text to display in file button used in HTML forms for media files when no media files are selected and the button allows multiple files to be selected");
}


RefPtr<StringImpl> formControlDoneButtonTitle()
{
    return WEB_UI_STRING("Done", "Title of the Done button for form controls.");
}
#endif

RefPtr<StringImpl> validationMessageTooLongText(int, int maxLength)
{
    return WEB_UI_FORMAT_CFSTRING("Use no more than %d character(s)", "Validation message for form control elements with a value shorter than maximum allowed length", maxLength);
}

#if PLATFORM(MAC)
RefPtr<StringImpl> insertListTypeNone()
{
    return WEB_UI_STRING("None", "Option in segmented control for choosing list type in text editing");
}

RefPtr<StringImpl> insertListTypeBulleted()
{
    return WEB_UI_STRING("•", "Option in segmented control for choosing list type in text editing");
}

RefPtr<StringImpl> insertListTypeBulletedAccessibilityTitle()
{
    return WEB_UI_STRING("Bulleted list", "Option in segmented control for inserting a bulleted list in text editing");
}

RefPtr<StringImpl> insertListTypeNumbered()
{
    return WEB_UI_STRING("1. 2. 3.", "Option in segmented control for choosing list type in text editing");
}

RefPtr<StringImpl> insertListTypeNumberedAccessibilityTitle()
{
    return WEB_UI_STRING("Numbered list", "Option in segmented control for inserting a numbered list in text editing");
}

RefPtr<StringImpl> exitFullScreenButtonAccessibilityTitle()
{
    return WEB_UI_STRING("Exit Full Screen", "Button for exiting full screen when in full screen media playback");
}
#endif // PLATFORM(MAC)

#if ENABLE(IMAGE_ANALYSIS)

RefPtr<StringImpl> contextMenuItemTagLookUpImage()
{
    return WEB_UI_STRING("Look Up", "Title for Look Up action button");
}

#endif

#if ENABLE(IMAGE_ANALYSIS)

RefPtr<StringImpl> contextMenuItemTagCopySubject()
{
    return WEB_UI_STRING("Copy Subject", "Title for Copy Subject");
}

RefPtr<StringImpl> contextMenuItemTitleRemoveBackground()
{
    return WEB_UI_STRING("Remove Background", "Remove Background menu item");
}

#endif // ENABLE(IMAGE_ANALYSIS)

} // namespace WebCore
