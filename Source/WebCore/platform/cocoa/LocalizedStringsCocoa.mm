/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

NSString *localizedNSString(NSString *key)
{
#if !PLATFORM(IOS_FAMILY)
    // Can be called on a dispatch queue when initializing strings on iOS.
    // See LoadWebLocalizedStrings and <rdar://problem/7902473>.
    ASSERT(isMainThread());
#endif

    static NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebCore"];
    return [bundle localizedStringForKey:key value:@"localized string not found" table:nullptr];
}

String localizedString(const char* key)
{
    RetainPtr<CFStringRef> keyString = adoptCF(CFStringCreateWithCStringNoCopy(0, key, kCFStringEncodingUTF8, kCFAllocatorNull));
    return localizedNSString((__bridge NSString *)keyString.get());
}

String copyImageUnknownFileLabel()
{
    return WEB_UI_STRING("unknown", "Unknown filename");
}

#if ENABLE(CONTEXT_MENUS)
String contextMenuItemTagSearchInSpotlight()
{
    return WEB_UI_STRING("Search in Spotlight", "Search in Spotlight context menu item");
}

String contextMenuItemTagSearchWeb()
{
    auto searchProviderName = PAL::defaultSearchProviderDisplayName();
    return formatLocalizedString(WEB_UI_STRING("Search with %@", "Search with search provider context menu item with provider name inserted"), searchProviderName.get());
}

String contextMenuItemTagShowFonts()
{
    return WEB_UI_STRING("Show Fonts", "Show fonts context menu item");
}

String contextMenuItemTagStyles()
{
    return WEB_UI_STRING("Styles…", "Styles context menu item");
}

String contextMenuItemTagShowColors()
{
    return WEB_UI_STRING("Show Colors", "Show colors context menu item");
}

String contextMenuItemTagSpeechMenu()
{
    return WEB_UI_STRING("Speech", "Speech context sub-menu item");
}

String contextMenuItemTagStartSpeaking()
{
    return WEB_UI_STRING("Start Speaking", "Start speaking context menu item");
}

String contextMenuItemTagStopSpeaking()
{
    return WEB_UI_STRING("Stop Speaking", "Stop speaking context menu item");
}

String contextMenuItemTagCorrectSpellingAutomatically()
{
    return WEB_UI_STRING("Correct Spelling Automatically", "Correct Spelling Automatically context menu item");
}

String contextMenuItemTagSubstitutionsMenu()
{
    return WEB_UI_STRING("Substitutions", "Substitutions context sub-menu item");
}

String contextMenuItemTagShowSubstitutions(bool show)
{
    if (show)
        return WEB_UI_STRING("Show Substitutions", "menu item title");
    return WEB_UI_STRING("Hide Substitutions", "menu item title");
}

String contextMenuItemTagSmartCopyPaste()
{
    return WEB_UI_STRING("Smart Copy/Paste", "Smart Copy/Paste context menu item");
}

String contextMenuItemTagSmartQuotes()
{
    return WEB_UI_STRING("Smart Quotes", "Smart Quotes context menu item");
}

String contextMenuItemTagSmartDashes()
{
    return WEB_UI_STRING("Smart Dashes", "Smart Dashes context menu item");
}

String contextMenuItemTagSmartLinks()
{
    return WEB_UI_STRING("Smart Links", "Smart Links context menu item");
}

String contextMenuItemTagTextReplacement()
{
    return WEB_UI_STRING("Text Replacement", "Text Replacement context menu item");
}

String contextMenuItemTagTransformationsMenu()
{
    return WEB_UI_STRING("Transformations", "Transformations context sub-menu item");
}

String contextMenuItemTagMakeUpperCase()
{
    return WEB_UI_STRING("Make Upper Case", "Make Upper Case context menu item");
}

String contextMenuItemTagMakeLowerCase()
{
    return WEB_UI_STRING("Make Lower Case", "Make Lower Case context menu item");
}

String contextMenuItemTagCapitalize()
{
    return WEB_UI_STRING("Capitalize", "Capitalize context menu item");
}

String contextMenuItemTagChangeBack(const String& replacedString)
{
    notImplemented();
    return replacedString;
}

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
String contextMenuItemTagEnterVideoEnhancedFullscreen()
{
    return WEB_UI_STRING("Enter Picture in Picture", "menu item");
}

String contextMenuItemTagExitVideoEnhancedFullscreen()
{
    return WEB_UI_STRING("Exit Picture in Picture", "menu item");
}
#endif
#endif // ENABLE(CONTEXT_MENUS)

String AXMeterGaugeRegionOptimumText()
{
    return WEB_UI_STRING("optimal value", "The optimum value description for a meter element.");
}

String AXMeterGaugeRegionSuboptimalText()
{
    return WEB_UI_STRING("suboptimal value", "The suboptimal value description for a meter element.");
}

String AXMeterGaugeRegionLessGoodText()
{
    return WEB_UI_STRING("critical value", "The less good value description for a meter element.");
}

String builtInPDFPluginName()
{
    // Also exposed to DOM.
    return WEB_UI_STRING("WebKit built-in PDF", "Pseudo plug-in name, visible in the Installed Plug-ins page in Safari.");
}

String pdfDocumentTypeDescription()
{
    // Also exposed to DOM.
    return WEB_UI_STRING("Portable Document Format", "Description of the primary type supported by the PDF pseudo plug-in. Visible in the Installed Plug-ins page in Safari.");
}

String postScriptDocumentTypeDescription()
{
    // Also exposed to DOM.
    return WEB_UI_STRING("PostScript", "Description of the PostScript type supported by the PDF pseudo plug-in. Visible in the Installed Plug-ins page in Safari.");
}

String keygenMenuItem2048()
{
    return WEB_UI_STRING("2048 (High Grade)", "Menu item title for KEYGEN pop-up menu");
}

String keygenKeychainItemName(const String& host)
{
    return formatLocalizedString(WEB_UI_STRING("Key from %@", "Name of keychain key generated by the KEYGEN tag"), host.createCFString().get());
}

#if PLATFORM(IOS_FAMILY)
String htmlSelectMultipleItems(size_t count)
{
    switch (count) {
    case 0:
        return WEB_UI_STRING("0 Items", "Present the element <select multiple> when no <option> items are selected (iOS only)");
    case 1:
        return WEB_UI_STRING("1 Item", "Present the element <select multiple> when a single <option> is selected (iOS only)");
    default:
        return formatLocalizedString(WEB_UI_STRING("%zu Items", "Present the number of selected <option> items in a <select multiple> element (iOS only)"), count);
    }
}

String fileButtonChooseMediaFileLabel()
{
    return WEB_UI_STRING("Choose Media (Single)", "Title for file button used in HTML forms for media files");
}

String fileButtonChooseMultipleMediaFilesLabel()
{
    return WEB_UI_STRING("Choose Media (Multiple)", "Title for file button used in HTML5 forms for multiple media files");
}

String fileButtonNoMediaFileSelectedLabel()
{
    return WEB_UI_STRING("no media selected (single)", "Text to display in file button used in HTML forms for media files when no media file is selected");
}

String fileButtonNoMediaFilesSelectedLabel()
{
    return WEB_UI_STRING("no media selected (multiple)", "Text to display in file button used in HTML forms for media files when no media files are selected and the button allows multiple files to be selected");
}
#endif

String validationMessageTooLongText(int, int maxLength)
{
    return [NSString localizedStringWithFormat:WEB_UI_NSSTRING(@"Use no more than %d character(s)", @"Validation message for form control elements with a value shorter than maximum allowed length"), maxLength];
}

#if PLATFORM(MAC)
String insertListTypeNone()
{
    return WEB_UI_STRING("None", "Option in segmented control for choosing list type in text editing");
}

String insertListTypeBulleted()
{
    return WEB_UI_STRING("•", "Option in segmented control for choosing list type in text editing");
}

String insertListTypeBulletedAccessibilityTitle()
{
    return WEB_UI_STRING("Bulleted list", "Option in segmented control for inserting a bulleted list in text editing");
}

String insertListTypeNumbered()
{
    return WEB_UI_STRING("1. 2. 3.", "Option in segmented control for choosing list type in text editing");
}

String insertListTypeNumberedAccessibilityTitle()
{
    return WEB_UI_STRING("Numbered list", "Option in segmented control for inserting a numbered list in text editing");
}

String exitFullScreenButtonAccessibilityTitle()
{
    return WEB_UI_STRING("Exit Full Screen", "Button for exiting full screen when in full screen media playback");
}
#endif // PLATFORM(MAC)

} // namespace WebCore
