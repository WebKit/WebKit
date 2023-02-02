/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Igalia S.L
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "LocalizedStrings.h"

#include "IntSize.h"
#include "NotImplemented.h"
#include <wtf/MathExtras.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>

#if USE(CF)
#if PLATFORM(WIN)
#include "WebCoreBundleWin.h"
#endif
#include <wtf/RetainPtr.h>
#endif

#if USE(GLIB)
#include <wtf/glib/GUniquePtr.h>
#endif

namespace WebCore {

#if USE(CF) && !PLATFORM(WIN)
String formatLocalizedString(CFStringRef format, ...)
{
    va_list arguments;
    va_start(arguments, format);

    auto localizedFormat = copyLocalizedString(format);
ALLOW_NONLITERAL_FORMAT_BEGIN
    // The 'format' parameter is already checked for correct placeholders and parameters.
    auto result = adoptCF(CFStringCreateWithFormatAndArguments(0, 0, localizedFormat.get(), arguments));
ALLOW_NONLITERAL_FORMAT_END

    va_end(arguments);
    return result.get();
}
#else
// Because |format| is used as the second parameter to va_start, it cannot be a reference
// type according to section 18.7/3 of the C++ N1905 standard.
String formatLocalizedString(const char* format, ...)
{
#if USE(CF) && PLATFORM(WIN)
    auto cfFormat = adoptCF(CFStringCreateWithCStringNoCopy(nullptr, format, kCFStringEncodingUTF8, kCFAllocatorNull));
    va_list arguments;
    va_start(arguments, format);
    auto localizedFormat = copyLocalizedString(cfFormat.get());
    auto result = adoptCF(CFStringCreateWithFormatAndArguments(0, 0, localizedFormat.get(), arguments));
    va_end(arguments);
    return result.get();
#elif USE(GLIB)
    va_list arguments;
    va_start(arguments, format);
    GUniquePtr<gchar> result(g_strdup_vprintf(format, arguments));
    va_end(arguments);
    return String::fromUTF8(result.get());
#else
    notImplemented();
    return String::fromUTF8(format);
#endif
}
#endif

#if USE(CF)
#if !PLATFORM(WIN)
static CFBundleRef webCoreBundle()
{
    static NeverDestroyed<RetainPtr<CFBundleRef>> bundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebCore"));
    ASSERT(bundle.get());
    return bundle.get().get();
}
#endif

RetainPtr<CFStringRef> copyLocalizedString(CFStringRef key)
{
#if !PLATFORM(IOS_FAMILY)
    // Can be called on a dispatch queue when initializing strings on iOS.
    // See LoadWebLocalizedStrings and <rdar://problem/7902473>.
    ASSERT(isMainThread());
#endif

    static CFStringRef notFound = CFSTR("localized string not found");

#if PLATFORM(WIN)
    CFBundleRef bundle = webKitBundle();
#else
    CFBundleRef bundle = webCoreBundle();
#endif
    auto result = adoptCF(CFBundleCopyLocalizedString(bundle, key, notFound, nullptr));

#if ASSERT_ENABLED
    if (result.get() == notFound) {
        char keyCString[256];
        CFStringGetCString(key, keyCString, sizeof(keyCString), kCFStringEncodingUTF8);
        ASSERT_WITH_MESSAGE(result.get() != notFound, "Could not find localizable string '%s' in bundle", keyCString);
    }
#endif

    return result;
}
#endif

#if USE(CF) && !PLATFORM(WIN)
String localizedString(CFStringRef key)
{
    return copyLocalizedString(key).get();
}
#else
String localizedString(const char* key)
{
#if USE(CF)
    auto keyString = adoptCF(CFStringCreateWithCStringNoCopy(nullptr, key, kCFStringEncodingUTF8, kCFAllocatorNull));
    return copyLocalizedString(keyString.get()).get();
#else
    return String::fromUTF8(key, strlen(key));
#endif
}
#endif

#if ENABLE(CONTEXT_MENUS)

static String truncatedStringForMenuItem(const String& original)
{
    // Truncate the string if it's too long. This number is roughly the same as the one used by AppKit.
    unsigned maxNumberOfGraphemeClustersInLookupMenuItem = 24;

    String trimmed = original.stripWhiteSpace();
    unsigned numberOfCharacters = numCodeUnitsInGraphemeClusters(trimmed, maxNumberOfGraphemeClustersInLookupMenuItem);
    return numberOfCharacters == trimmed.length() ? trimmed : makeString(trimmed.left(numberOfCharacters), horizontalEllipsis);
}

#endif

String inputElementAltText()
{
    return WEB_UI_STRING_KEY("Submit", "Submit (input element)", "alt text for <input> elements with no alt, title, or value");
}

String resetButtonDefaultLabel()
{
    return WEB_UI_STRING("Reset", "default label for Reset buttons in forms on web pages");
}

String searchableIndexIntroduction()
{
    return WEB_UI_STRING("This is a searchable index. Enter search keywords: ",
                         "text that appears at the start of nearly-obsolete web pages in the form of a 'searchable index'");
}

String submitButtonDefaultLabel()
{
    return WEB_UI_STRING("Submit", "default label for Submit buttons in forms on web pages");
}

String fileButtonChooseFileLabel()
{
    return WEB_UI_STRING("Choose File", "title for a single file chooser button used in HTML forms");
}

String fileButtonChooseMultipleFilesLabel()
{
    return WEB_UI_STRING("Choose Files", "title for a multiple file chooser button used in HTML forms. This title should be as short as possible.");
}

String fileButtonNoFileSelectedLabel()
{
    return WEB_UI_STRING("no file selected", "text to display in file button used in HTML forms when no file is selected");
}

String fileButtonNoFilesSelectedLabel()
{
    return WEB_UI_STRING("no files selected", "text to display in file button used in HTML forms when no files are selected and the button allows multiple files to be selected");
}

String defaultDetailsSummaryText()
{
    return WEB_UI_STRING("Details", "text to display in <details> tag when it has no <summary> child");
}

#if ENABLE(CONTEXT_MENUS)

String contextMenuItemTagOpenLinkInNewWindow()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Link in New Window", "Open Link in New _Window", "Open in New Window context menu item");
}

String contextMenuItemTagDownloadLinkToDisk()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Download Linked File", "_Download Linked File", "Download Linked File context menu item");
}

#if !PLATFORM(GTK)
String contextMenuItemTagCopyLinkToClipboard()
{
    return WEB_UI_STRING("Copy Link", "Copy Link context menu item");
}
#endif

String contextMenuItemTagOpenImageInNewWindow()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Image in New Window", "Open _Image in New Window", "Open Image in New Window context menu item");
}

#if !PLATFORM(GTK)
String contextMenuItemTagDownloadImageToDisk()
{
    return WEB_UI_STRING("Download Image", "Download Image context menu item");
}
#endif

String contextMenuItemTagCopyImageToClipboard()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Copy Image", "Cop_y Image", "Copy Image context menu item");
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Frame in New Window", "Open _Frame in New Window", "Open Frame in New Window context menu item");
}

String contextMenuItemTagCopy()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Copy", "_Copy", "Copy context menu item");
}

String contextMenuItemTagGoBack()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Back", "_Back", "Back context menu item");
}

String contextMenuItemTagGoForward()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Forward", "_Forward", "Forward context menu item");
}

String contextMenuItemTagStop()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Stop", "_Stop", "Stop context menu item");
}

String contextMenuItemTagReload()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Reload", "_Reload", "Reload context menu item");
}

String contextMenuItemTagCut()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Cut", "Cu_t", "Cut context menu item");
}

String contextMenuItemTagPaste()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Paste", "_Paste", "Paste context menu item");
}

String contextMenuItemTagNoGuessesFound()
{
    return WEB_UI_STRING("No Guesses Found", "No Guesses Found context menu item");
}

String contextMenuItemTagIgnoreSpelling()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Ignore Spelling", "_Ignore Spelling", "Ignore Spelling context menu item");
}

String contextMenuItemTagLearnSpelling()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Learn Spelling", "_Learn Spelling", "Learn Spelling context menu item");
}

#if !PLATFORM(COCOA)
String contextMenuItemTagSearchWeb()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Search the Web", "_Search the Web", "Search the Web context menu item");
}
#endif

String contextMenuItemTagLookUpInDictionary(const String& selectedString)
{
#if USE(CF)
    auto selectedCFString = truncatedStringForMenuItem(selectedString).createCFString();
    return WEB_UI_FORMAT_CFSTRING("Look Up “%@”", "Look Up context menu item with selected word", selectedCFString.get());
#elif USE(GLIB)
    return WEB_UI_FORMAT_STRING("Look Up “%s”", "Look Up context menu item with selected word", truncatedStringForMenuItem(selectedString).utf8().data());
#else
    return makeStringByReplacingAll(WEB_UI_STRING("Look Up “<selection>”", "Look Up context menu item with selected word"), "<selection>"_s, truncatedStringForMenuItem(selectedString));
#endif
}

String contextMenuItemTagOpenLink()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Link", "_Open Link", "Open Link context menu item");
}

String contextMenuItemTagIgnoreGrammar()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Ignore Grammar", "Ignore _Grammar", "Ignore Grammar context menu item");
}

String contextMenuItemTagSpellingMenu()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Spelling and Grammar", "Spelling and _Grammar", "Spelling and Grammar context sub-menu item");
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    if (show)
        return WEB_UI_STRING_WITH_MNEMONIC("Show Spelling and Grammar", "_Show Spelling and Grammar", "menu item title");
    return WEB_UI_STRING_WITH_MNEMONIC("Hide Spelling and Grammar", "_Hide Spelling and Grammar", "menu item title");
}

String contextMenuItemTagCheckSpelling()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Check Document Now", "_Check Document Now", "Check spelling context menu item");
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Check Spelling While Typing", "Check Spelling While _Typing", "Check spelling while typing context menu item");
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Check Grammar With Spelling", "Check _Grammar With Spelling", "Check grammar with spelling context menu item");
}

String contextMenuItemTagFontMenu()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Font", "_Font", "Font context sub-menu item");
}

String contextMenuItemTagBold()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Bold", "_Bold", "Bold context menu item");
}

String contextMenuItemTagItalic()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Italic", "_Italic", "Italic context menu item");
}

String contextMenuItemTagUnderline()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Underline", "_Underline", "Underline context menu item");
}

String contextMenuItemTagOutline()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Outline", "_Outline", "Outline context menu item");
}

#if !PLATFORM(GTK)
String contextMenuItemTagWritingDirectionMenu()
{
    return WEB_UI_STRING("Paragraph Direction", "Paragraph direction context sub-menu item");
}

String contextMenuItemTagTextDirectionMenu()
{
    return WEB_UI_STRING("Selection Direction", "Selection direction context sub-menu item");
}

String contextMenuItemTagDefaultDirection()
{
    return WEB_UI_STRING("Default", "Default writing direction context menu item");
}

String contextMenuItemTagLeftToRight()
{
    return WEB_UI_STRING("Left to Right", "Left to Right context menu item");
}

String contextMenuItemTagRightToLeft()
{
    return WEB_UI_STRING("Right to Left", "Right to Left context menu item");
}
#endif

String contextMenuItemTagOpenVideoInNewWindow()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Video in New Window", "Open _Video in New Window", "Open Video in New Window context menu item");
}

String contextMenuItemTagOpenAudioInNewWindow()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Audio in New Window", "Open _Audio in New Window", "Open Audio in New Window context menu item");
}

String contextMenuItemTagDownloadVideoToDisk()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Download Video", "Download _Video", "Download Video To Disk context menu item");
}

String contextMenuItemTagDownloadAudioToDisk()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Download Audio", "Download _Audio", "Download Audio To Disk context menu item");
}

#if !PLATFORM(GTK)
String contextMenuItemTagCopyVideoLinkToClipboard()
{
    return WEB_UI_STRING("Copy Video Address", "Copy Video Address Location context menu item");
}

String contextMenuItemTagCopyAudioLinkToClipboard()
{
    return WEB_UI_STRING("Copy Audio Address", "Copy Audio Address Location context menu item");
}

String contextMenuItemTagToggleMediaControls()
{
    return WEB_UI_STRING("Controls", "Media Controls context menu item");
}

String contextMenuItemTagShowMediaControls()
{
    return WEB_UI_STRING("Show Controls", "Show Media Controls context menu item");
}

String contextMenuItemTagHideMediaControls()
{
    return WEB_UI_STRING("Hide Controls", "Hide Media Controls context menu item");
}

String contextMenuItemTagToggleMediaLoop()
{
    return WEB_UI_STRING("Loop", "Media Loop context menu item");
}

String contextMenuItemTagEnterVideoFullscreen()
{
    return WEB_UI_STRING("Enter Full Screen", "Video Enter Full Screen context menu item");
}

String contextMenuItemTagExitVideoFullscreen()
{
    return WEB_UI_STRING_KEY("Exit Full Screen", "Exit Full Screen (context menu)", "Video Exit Full Screen context menu item");
}
#endif

String contextMenuItemTagMediaPlay()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Play", "_Play", "Media Play context menu item");
}

String contextMenuItemTagMediaPause()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Pause", "_Pause", "Media Pause context menu item");
}

String contextMenuItemTagMediaMute()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Mute", "_Mute", "Media Mute context menu item");
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
String contextMenuItemTagPlayAllAnimations()
{
    return WEB_UI_STRING("Play All Animations", "Play All Animations context menu item");
}

String contextMenuItemTagPauseAllAnimations()
{
    return WEB_UI_STRING("Pause All Animations", "Pause All Animations context menu item");
}

String contextMenuItemTagPlayAnimation()
{
    return WEB_UI_STRING("Play Animation", "Play animation context menu item");
}

String contextMenuItemTagPauseAnimation()
{
    return WEB_UI_STRING("Pause Animation", "Pause animation context menu item");
}
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

String contextMenuItemTagInspectElement()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Inspect Element", "Inspect _Element", "Inspect Element context menu item");
}

#if HAVE(TRANSLATION_UI_SERVICES)
String contextMenuItemTagTranslate(const String& selectedString)
{
    auto selectedCFString = truncatedStringForMenuItem(selectedString).createCFString();
    return WEB_UI_FORMAT_CFSTRING("Translate “%@”", "Translate context menu item with selected word", selectedCFString.get());
}
#endif

#if ENABLE(PDFJS)
String contextMenuItemPDFAutoSize()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Automatically Resize", "_Automatically Resize", "Automatically Resize context menu item");
}

String contextMenuItemPDFZoomIn()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Zoom In", "_Zoom In", "Zoom In Continuous context menu item");
}

String contextMenuItemPDFZoomOut()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Zoom Out", "_Zoom Out", "Zoom Out context menu item");
}

String contextMenuItemPDFActualSize()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Actual Size", "_Actual Size", "Actual Size context menu item");
}

String contextMenuItemPDFSinglePage()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Single Page", "_Single Page", "Single Page context menu item");
}

String contextMenuItemPDFSinglePageContinuous()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Single Page Continuous", "_Single Page Continuous", "Single Page Continuous context menu item");
}

String contextMenuItemPDFTwoPages()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Two Pages", "_Two Pages", "Two Pages context menu item");
}

String contextMenuItemPDFTwoPagesContinuous()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Two Pages Continuous", "_Two Pages Continuous", "Two Pages Continuous context menu item");
}

String contextMenuItemPDFNextPage()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Next Page", "_Next Page", "Next Page context menu item");
}

String contextMenuItemPDFPreviousPage()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Previous Page", "_Previous Page", "Previous Page context menu item");
}
#endif
#endif // ENABLE(CONTEXT_MENUS)

#if !PLATFORM(COCOA)
String pdfDocumentTypeDescription()
{
    // Also exposed to DOM.
    return WEB_UI_STRING("Portable Document Format", "Description of the primary type supported by the PDF pseudo plug-in.");
}
#endif // !PLATFORM(COCOA)

#if !PLATFORM(IOS_FAMILY)

String searchMenuNoRecentSearchesText()
{
    return WEB_UI_STRING("No recent searches", "Label for only item in menu that appears when clicking on the search field image, when no searches have been performed");
}

String searchMenuRecentSearchesText()
{
    return WEB_UI_STRING("Recent Searches", "label for first item in the menu that appears when clicking on the search field image, used as embedded menu title");
}

String searchMenuClearRecentSearchesText()
{
    return WEB_UI_STRING("Clear Recent Searches", "menu item in Recent Searches menu that empties menu's contents");
}

#endif // !PLATFORM(IOS_FAMILY)

String AXWebAreaText()
{
    return WEB_UI_STRING("HTML content", "accessibility role description for web area");
}

String AXLinkText()
{
    return WEB_UI_STRING("link", "accessibility role description for link");
}

String AXListMarkerText()
{
    return WEB_UI_STRING("list marker", "accessibility role description for list marker");
}

String AXImageMapText()
{
    return WEB_UI_STRING("image map", "accessibility role description for image map");
}

String AXHeadingText()
{
    return WEB_UI_STRING("heading", "accessibility role description for headings");
}

String AXColorWellText()
{
    return WEB_UI_STRING("color well", "accessibility role description for a color well");
}

String AXDefinitionText()
{
    return WEB_UI_STRING("definition", "role description of ARIA definition role");
}

String AXDescriptionListText()
{
    return WEB_UI_STRING("description list", "accessibility role description of a description list");
}

String AXDescriptionListTermText()
{
    return WEB_UI_STRING("term", "term word of a description list");
}

String AXDescriptionListDetailText()
{
    return WEB_UI_STRING("description", "description detail");
}

String AXDetailsText()
{
    return WEB_UI_STRING("details", "accessibility role description for a details element");
}

String AXSummaryText()
{
    return WEB_UI_STRING("summary", "accessibility role description for a summary element");
}

String AXFooterRoleDescriptionText()
{
    return WEB_UI_STRING("footer", "accessibility role description for a footer");
}

String AXSuggestionRoleDescriptionText()
{
    return WEB_UI_STRING("suggestion", "accessibility role description for a suggestion element");
}

String AXFileUploadButtonText()
{
    return WEB_UI_STRING("file upload button", "accessibility role description for a file upload button");
}

String AXOutputText()
{
    return WEB_UI_STRING("output", "accessibility role description for an output element");
}

String AXAttachmentRoleText()
{
    return WEB_UI_STRING("attachment", "accessibility role description for an attachment element");
}
    
String AXSearchFieldCancelButtonText()
{
    return WEB_UI_STRING("cancel", "accessibility description for a search field cancel button");
}

String AXFeedText()
{
    return WEB_UI_STRING("feed", "accessibility role description for a group containing a scrollable list of articles.");
}

String AXFigureText()
{
    return WEB_UI_STRING("figure", "accessibility role description for a figure element.");
}

String AXEmailFieldText()
{
    return WEB_UI_STRING("email field", "accessibility role description for an email field.");
}

String AXTelephoneFieldText()
{
    return WEB_UI_STRING("telephone number field", "accessibility role description for a telephone number field.");
}

String AXURLFieldText()
{
    return WEB_UI_STRING("URL field", "accessibility role description for a URL field.");
}

String AXDateFieldText()
{
    return WEB_UI_STRING("date field", "accessibility role description for a date field.");
}

String AXTimeFieldText()
{
    return WEB_UI_STRING("time field", "accessibility role description for a time field.");
}

String AXDateTimeFieldText()
{
    return WEB_UI_STRING("date and time field", "accessibility role description for a date and time field.");
}

String AXMonthFieldText()
{
    return WEB_UI_STRING("month and year field", "accessibility role description for a month field.");
}

String AXNumberFieldText()
{
    return WEB_UI_STRING("number field", "accessibility role description for a number field.");
}

String AXWeekFieldText()
{
    return WEB_UI_STRING("week and year field", "accessibility role description for a time field.");
}

String AXARIAContentGroupText(StringView ariaType)
{
    if (ariaType == "ARIAApplicationAlert"_s)
        return WEB_UI_STRING("alert", "An ARIA accessibility group that acts as an alert.");
    if (ariaType == "ARIAApplicationAlertDialog"_s)
        return WEB_UI_STRING("web alert dialog", "An ARIA accessibility group that acts as an alert dialog.");
    if (ariaType == "ARIAApplicationDialog"_s)
        return WEB_UI_STRING("web dialog", "An ARIA accessibility group that acts as an dialog.");
    if (ariaType == "ARIAApplicationLog"_s)
        return WEB_UI_STRING("log", "An ARIA accessibility group that acts as a console log.");
    if (ariaType == "ARIAApplicationMarquee"_s)
        return WEB_UI_STRING("marquee", "An ARIA accessibility group that acts as a marquee.");
    if (ariaType == "ARIAApplicationStatus"_s)
        return WEB_UI_STRING("application status", "An ARIA accessibility group that acts as a status update.");
    if (ariaType == "ARIAApplicationTimer"_s)
        return WEB_UI_STRING("timer", "An ARIA accessibility group that acts as an updating timer.");
    if (ariaType == "ARIADocument"_s)
        return WEB_UI_STRING("document", "An ARIA accessibility group that acts as a document.");
    if (ariaType == "ARIADocumentArticle"_s)
        return WEB_UI_STRING("article", "An ARIA accessibility group that acts as an article.");
    if (ariaType == "ARIADocumentNote"_s)
        return WEB_UI_STRING("note", "An ARIA accessibility group that acts as a note in a document.");
    if (ariaType == "ARIAWebApplication"_s)
        return WEB_UI_STRING("web application", "An ARIA accessibility group that acts as an application.");
    if (ariaType == "ARIALandmarkBanner"_s)
        return WEB_UI_STRING("banner", "An ARIA accessibility group that acts as a banner.");
    if (ariaType == "ARIALandmarkComplementary"_s)
        return WEB_UI_STRING("complementary", "An ARIA accessibility group that acts as a region of complementary information.");
    if (ariaType == "ARIALandmarkContentInfo"_s)
        return WEB_UI_STRING("content information", "An ARIA accessibility group that contains content.");
    if (ariaType == "ARIALandmarkMain"_s)
        return WEB_UI_STRING("main", "An ARIA accessibility group that is the main portion of the website.");
    if (ariaType == "ARIALandmarkNavigation"_s)
        return WEB_UI_STRING("navigation", "An ARIA accessibility group that contains the main navigation elements of a website.");
    if (ariaType == "ARIALandmarkRegion"_s)
        return WEB_UI_STRING("region", "An ARIA accessibility group that acts as a distinct region in a document.");
    if (ariaType == "ARIALandmarkSearch"_s)
        return WEB_UI_STRING("search", "An ARIA accessibility group that contains a search feature of a website.");
    if (ariaType == "ARIAUserInterfaceTooltip"_s)
        return WEB_UI_STRING("tooltip", "An ARIA accessibility group that acts as a tooltip.");
    if (ariaType == "ARIATabPanel"_s)
        return WEB_UI_STRING("tab panel", "An ARIA accessibility group that contains the content of a tab.");
    if (ariaType == "ARIADocumentMath"_s)
        return WEB_UI_STRING("math", "An ARIA accessibility group that contains mathematical symbols.");
    return String();
}

String AXHorizontalRuleDescriptionText()
{
    return WEB_UI_STRING("separator", "accessibility role description for a horizontal rule [<hr>]");
}

String AXMarkText()
{
    return WEB_UI_STRING("highlighted", "accessibility role description for a mark element");
}

String AXButtonActionVerb()
{
    return WEB_UI_STRING("press", "Verb stating the action that will occur when a button is pressed, as used by accessibility");
}

String AXRadioButtonActionVerb()
{
    return WEB_UI_STRING("select", "Verb stating the action that will occur when a radio button is clicked, as used by accessibility");
}

String AXTextFieldActionVerb()
{
    return WEB_UI_STRING("activate", "Verb stating the action that will occur when a text field is selected, as used by accessibility");
}

String AXCheckedCheckBoxActionVerb()
{
    return WEB_UI_STRING("uncheck", "Verb stating the action that will occur when a checked checkbox is clicked, as used by accessibility");
}

String AXUncheckedCheckBoxActionVerb()
{
    return WEB_UI_STRING("check", "Verb stating the action that will occur when an unchecked checkbox is clicked, as used by accessibility");
}

String AXLinkActionVerb()
{
    return WEB_UI_STRING("jump", "Verb stating the action that will occur when a link is clicked, as used by accessibility");
}

String AXMenuListPopupActionVerb()
{
    notImplemented();
    return "select"_s;
}

String AXMenuListActionVerb()
{
    notImplemented();
    return "select"_s;
}

String AXListItemActionVerb()
{
    notImplemented();
    return "select"_s;
}

#if ENABLE(APPLE_PAY)
String AXApplePayPlainLabel()
{
    return WEB_UI_STRING("Apple Pay", "Label for the plain Apple Pay button.");
}

String AXApplePayBuyLabel()
{
    return WEB_UI_STRING("Buy with Apple Pay", "Label for the buy with Apple Pay button.");
}

String AXApplePaySetupLabel()
{
    return WEB_UI_STRING("Set up with Apple Pay", "Label for the set up with Apple Pay button.");
}

String AXApplePayDonateLabel()
{
    return WEB_UI_STRING("Donate with Apple Pay", "Label for the donate with Apple Pay button.");
}

String AXApplePayCheckOutLabel()
{
    return WEB_UI_STRING("Check out with Apple Pay", "Label for the check out with Apple Pay button.");
}

String AXApplePayBookLabel()
{
    return WEB_UI_STRING("Book with Apple Pay", "Label for the book with Apple Pay button.");
}

String AXApplePaySubscribeLabel()
{
    return WEB_UI_STRING("Subscribe with Apple Pay", "Label for the subcribe with Apple Pay button.");
}

#if ENABLE(APPLE_PAY_NEW_BUTTON_TYPES)
String AXApplePayReloadLabel()
{
    return WEB_UI_STRING("Reload with Apple Pay", "Label for the reload with Apple Pay button.");
}
String AXApplePayAddMoneyLabel()
{
    return WEB_UI_STRING("Add money with Apple Pay", "Label for the add money with Apple Pay button.");
}
String AXApplePayTopUpLabel()
{
    return WEB_UI_STRING("Top up with Apple Pay", "Label for the top up with Apple Pay button.");
}
String AXApplePayOrderLabel()
{
    return WEB_UI_STRING("Order with Apple Pay", "Label for the order with Apple Pay button.");
}
String AXApplePayRentLabel()
{
    return WEB_UI_STRING("Rent with Apple Pay", "Label for the rent with Apple Pay button.");
}
String AXApplePaySupportLabel()
{
    return WEB_UI_STRING("Support with Apple Pay", "Label for the support with Apple Pay button.");
}
String AXApplePayContributeLabel()
{
    return WEB_UI_STRING("Contribute with Apple Pay", "Label for the contribute with Apple Pay button.");
}
String AXApplePayTipLabel()
{
    return WEB_UI_STRING("Tip with Apple Pay", "Label for the tip with Apple Pay button.");
}
#endif
#endif

String AXAutoFillCredentialsLabel()
{
    return WEB_UI_STRING("password AutoFill", "Label for the AutoFill credentials button inside a text field.");
}

String AXAutoFillContactsLabel()
{
    return WEB_UI_STRING("contact info AutoFill", "Label for the AutoFill contacts button inside a text field.");
}

String AXAutoFillStrongPasswordLabel()
{
    return WEB_UI_STRING("strong password AutoFill", "Label for the strong password AutoFill button inside a text field.");
}

String AXAutoFillCreditCardLabel()
{
    return WEB_UI_STRING("credit card AutoFill", "Label for the credit card AutoFill button inside a text field.");
}

String AXAutoFillLoadingLabel()
{
    return WEB_UI_STRING("loading AutoFill", "Label for the loading AutoFill button inside a text field.");
}

String autoFillStrongPasswordLabel()
{
    return WEB_UI_STRING("Strong Password", "Label for strong password.");
}

String missingPluginText()
{
    return WEB_UI_STRING("Missing Plug-in", "Label text to be used when a plugin is missing");
}

String crashedPluginText()
{
    return WEB_UI_STRING("Plug-in Failure", "Label text to be used if plugin host process has crashed");
}

String blockedPluginByContentSecurityPolicyText()
{
    return WEB_UI_STRING_KEY("Blocked Plug-in", "Blocked Plug-In (Blocked by page's Content Security Policy)", "Label text to be used if plugin is blocked by a page's Content Security Policy");
}

String insecurePluginVersionText()
{
    return WEB_UI_STRING_KEY("Blocked Plug-in", "Blocked Plug-In (Insecure plug-in)", "Label text to be used when an insecure plug-in version was blocked from loading");
}

String unsupportedPluginText()
{
    return WEB_UI_STRING_KEY("Unsupported Plug-in", "Unsupported Plug-In", "Label text to be used when an unsupported plug-in was blocked from loading");
}

String pluginTooSmallText()
{
    return WEB_UI_STRING_KEY("Plug-In too small", "Plug-In too small", "Label text to be used when a plug-in was blocked from loading because it was too small");
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    return WEB_UI_FORMAT_STRING("%d files", "Label to describe the number of files selected in a file upload control that allows multiple files", numberOfFiles);
}

String unknownFileSizeText()
{
    return WEB_UI_STRING_KEY("Unknown", "Unknown (filesize)", "Unknown filesize FTP directory listing item");
}

String imageTitle(const String& filename, const IntSize& size)
{
#if USE(CF)
    auto locale = adoptCF(CFLocaleCopyCurrent());
    auto formatter = adoptCF(CFNumberFormatterCreate(0, locale.get(), kCFNumberFormatterDecimalStyle));

    int widthInt = size.width();
    auto width = adoptCF(CFNumberCreate(0, kCFNumberIntType, &widthInt));
    auto widthString = adoptCF(CFNumberFormatterCreateStringWithNumber(0, formatter.get(), width.get()));

    int heightInt = size.height();
    auto height = adoptCF(CFNumberCreate(0, kCFNumberIntType, &heightInt));
    auto heightString = adoptCF(CFNumberFormatterCreateStringWithNumber(0, formatter.get(), height.get()));

    return WEB_UI_FORMAT_CFSTRING("%@ %@×%@ pixels", "window title for a standalone image (uses multiplication symbol, not x)", filename.createCFString().get(), widthString.get(), heightString.get());
#elif USE(GLIB)
    return WEB_UI_FORMAT_STRING("%s %d×%d pixels", "window title for a standalone image (uses multiplication symbol, not x)", filename.utf8().data(), size.width(), size.height());
#else
    return makeStringByReplacingAll(WEB_UI_FORMAT_STRING("<filename> %d×%d pixels", "window title for a standalone image (uses multiplication symbol, not x)", size.width(), size.height()), "<filename>"_s, filename);
#endif
}

String mediaElementLoadingStateText()
{
    return WEB_UI_STRING("Loading…", "Media controller status message when the media is loading");
}

String mediaElementLiveBroadcastStateText()
{
    return WEB_UI_STRING("Live Broadcast", "Media controller status message when watching a live broadcast");
}

String localizedMediaControlElementString(const String& name)
{
    if (name == "AudioElement"_s)
        return WEB_UI_STRING("audio playback", "accessibility label for audio element controller");
    if (name == "VideoElement"_s)
        return WEB_UI_STRING("video playback", "accessibility label for video element controller");
    if (name == "MuteButton"_s)
        return WEB_UI_STRING("mute", "accessibility label for mute button");
    if (name == "UnMuteButton"_s)
        return WEB_UI_STRING("unmute", "accessibility label for turn mute off button");
    if (name == "PlayButton"_s)
        return WEB_UI_STRING("play", "accessibility label for play button");
    if (name == "PauseButton"_s)
        return WEB_UI_STRING("pause", "accessibility label for pause button");
    if (name == "Slider"_s)
        return WEB_UI_STRING("movie time", "accessibility label for timeline slider");
    if (name == "SliderThumb"_s)
        return WEB_UI_STRING("timeline slider thumb", "accessibility label for timeline thumb");
    if (name == "RewindButton"_s)
        return WEB_UI_STRING("back 30 seconds", "accessibility label for seek back 30 seconds button");
    if (name == "ReturnToRealtimeButton"_s)
        return WEB_UI_STRING("return to real time", "accessibility label for return to real time button");
    if (name == "CurrentTimeDisplay"_s)
        return WEB_UI_STRING("elapsed time", "accessibility label for elapsed time display");
    if (name == "TimeRemainingDisplay"_s)
        return WEB_UI_STRING("remaining time", "accessibility label for time remaining display");
    if (name == "StatusDisplay"_s)
        return WEB_UI_STRING("status", "accessibility label for movie status");
    if (name == "EnterFullscreenButton"_s)
        return WEB_UI_STRING("enter full screen", "accessibility label for enter full screen button");
    if (name == "ExitFullscreenButton"_s)
        return WEB_UI_STRING("exit full screen", "accessibility label for exit full screen button");
    if (name == "SeekForwardButton"_s)
        return WEB_UI_STRING("fast forward", "accessibility label for fast forward button");
    if (name == "SeekBackButton"_s)
        return WEB_UI_STRING("fast reverse", "accessibility label for fast reverse button");
    if (name == "ShowClosedCaptionsButton"_s)
        return WEB_UI_STRING("show closed captions", "accessibility label for show closed captions button");
    if (name == "HideClosedCaptionsButton"_s)
        return WEB_UI_STRING("hide closed captions", "accessibility label for hide closed captions button");

    // FIXME: the ControlsPanel container should never be visible in the accessibility hierarchy.
    if (name == "ControlsPanel"_s)
        return String();

    ASSERT_NOT_REACHED();
    return String();
}

String localizedMediaControlElementHelpText(const String& name)
{
    if (name == "AudioElement"_s)
        return WEB_UI_STRING("audio element playback controls and status display", "accessibility help text for audio element controller");
    if (name == "VideoElement"_s)
        return WEB_UI_STRING("video element playback controls and status display", "accessibility help text for video element controller");
    if (name == "MuteButton"_s)
        return WEB_UI_STRING("mute audio tracks", "accessibility help text for mute button");
    if (name == "UnMuteButton"_s)
        return WEB_UI_STRING("unmute audio tracks", "accessibility help text for un mute button");
    if (name == "PlayButton"_s)
        return WEB_UI_STRING("begin playback", "accessibility help text for play button");
    if (name == "PauseButton"_s)
        return WEB_UI_STRING("pause playback", "accessibility help text for pause button");
    if (name == "Slider"_s)
        return WEB_UI_STRING("movie time scrubber", "accessibility help text for timeline slider");
    if (name == "SliderThumb"_s)
        return WEB_UI_STRING("movie time scrubber thumb", "accessibility help text for timeline slider thumb");
    if (name == "RewindButton"_s)
        return WEB_UI_STRING("seek movie back 30 seconds", "accessibility help text for jump back 30 seconds button");
    if (name == "ReturnToRealtimeButton"_s)
        return WEB_UI_STRING("resume real time streaming", "accessibility help text for return streaming movie to real time button");
    if (name == "CurrentTimeDisplay"_s)
        return WEB_UI_STRING("current movie time in seconds", "accessibility help text for elapsed time display");
    if (name == "TimeRemainingDisplay"_s)
        return WEB_UI_STRING("number of seconds of movie remaining", "accessibility help text for remaining time display");
    if (name == "StatusDisplay"_s)
        return WEB_UI_STRING("current movie status", "accessibility help text for movie status display");
    if (name == "SeekBackButton"_s)
        return WEB_UI_STRING("seek quickly back", "accessibility help text for fast rewind button");
    if (name == "SeekForwardButton"_s)
        return WEB_UI_STRING("seek quickly forward", "accessibility help text for fast forward button");
    if (name == "FullscreenButton"_s)
        return WEB_UI_STRING("Play movie in full screen mode", "accessibility help text for enter full screen button");
    if (name == "ShowClosedCaptionsButton"_s)
        return WEB_UI_STRING("start displaying closed captions", "accessibility help text for show closed captions button");
    if (name == "HideClosedCaptionsButton"_s)
        return WEB_UI_STRING("stop displaying closed captions", "accessibility help text for hide closed captions button");

    // The description of this button is descriptive enough that it doesn't require help text.
    if (name == "EnterFullscreenButton"_s)
        return String();
    
    ASSERT_NOT_REACHED();
    return String();
}

String localizedMediaTimeDescription(float time)
{
    if (!std::isfinite(time))
        return WEB_UI_STRING("indefinite time", "accessibility help text for an indefinite media controller time value");

    int seconds = static_cast<int>(fabsf(time));
    int days = seconds / (60 * 60 * 24);
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (days)
        return WEB_UI_FORMAT_STRING("%1$d days %2$d hours %3$d minutes %4$d seconds", "accessibility help text for media controller time value >= 1 day", days, hours, minutes, seconds);
    if (hours)
        return WEB_UI_FORMAT_STRING("%1$d hours %2$d minutes %3$d seconds", "accessibility help text for media controller time value >= 60 minutes", hours, minutes, seconds);
    if (minutes)
        return WEB_UI_FORMAT_STRING("%1$d minutes %2$d seconds", "accessibility help text for media controller time value >= 60 seconds", minutes, seconds);
    return WEB_UI_FORMAT_STRING("%1$d seconds", "accessibility help text for media controller time value < 60 seconds", seconds);
}

String validationMessageValueMissingText()
{
    return WEB_UI_STRING("Fill out this field", "Validation message for required form control elements that have no value");
}

String validationMessageValueMissingForCheckboxText()
{
    return WEB_UI_STRING("Select this checkbox", "Validation message for required checkboxes that have not be selected");
}

String validationMessageValueMissingForFileText()
{
    return WEB_UI_STRING("Select a file", "Validation message for required file inputs that have no value");
}

String validationMessageValueMissingForMultipleFileText()
{
    return validationMessageValueMissingForFileText();
}

String validationMessageValueMissingForRadioText()
{
    return WEB_UI_STRING("Select one of these options", "Validation message for required radio boxes that have no selection");
}

String validationMessageValueMissingForSelectText()
{
    return WEB_UI_STRING("Select an item in the list", "Validation message for required menu list controls that have no selection");
}

String validationMessageTypeMismatchText()
{
    return WEB_UI_STRING("Invalid value", "Validation message for input form controls with a value not matching type");
}

String validationMessageTypeMismatchForEmailText()
{
    return WEB_UI_STRING("Enter an email address", "Validation message for input form controls of type 'email' that have an invalid value");
}

String validationMessageTypeMismatchForMultipleEmailText()
{
    return validationMessageTypeMismatchForEmailText();
}

String validationMessageTypeMismatchForURLText()
{
    return WEB_UI_STRING("Enter a URL", "Validation message for input form controls of type 'url' that have an invalid value");
}

String validationMessagePatternMismatchText()
{
    return WEB_UI_STRING("Match the requested format", "Validation message for input form controls requiring a constrained value according to pattern");
}

#if !PLATFORM(GTK)
String validationMessageTooShortText(int, int minLength)
{
    return WEB_UI_FORMAT_STRING("Use at least %d characters", "Validation message for form control elements with a value shorter than minimum allowed length", minLength);
}

#if !PLATFORM(COCOA)
String validationMessageTooLongText(int, int maxLength)
{
    return WEB_UI_FORMAT_STRING("Use no more than %d characters", "Validation message for form control elements with a value shorter than maximum allowed length", maxLength);
}
#endif
#endif

String validationMessageRangeUnderflowText(const String& minimum)
{
#if USE(CF)
    return WEB_UI_FORMAT_CFSTRING("Value must be greater than or equal to %@", "Validation message for input form controls with value lower than allowed minimum", minimum.createCFString().get());
#elif USE(GLIB)
    return WEB_UI_FORMAT_STRING("Value must be greater than or equal to %s", "Validation message for input form controls with value lower than allowed minimum", minimum.utf8().data());
#else
    UNUSED_PARAM(minimum);
    return WEB_UI_STRING("range underflow", "Validation message for input form controls with value lower than allowed minimum");
#endif
}

String validationMessageRangeOverflowText(const String& maximum)
{
#if USE(CF)
    return WEB_UI_FORMAT_CFSTRING("Value must be less than or equal to %@", "Validation message for input form controls with value higher than allowed maximum", maximum.createCFString().get());
#elif USE(GLIB)
    return WEB_UI_FORMAT_STRING("Value must be less than or equal to %s", "Validation message for input form controls with value higher than allowed maximum", maximum.utf8().data());
#else
    UNUSED_PARAM(maximum);
    return WEB_UI_STRING("range overflow", "Validation message for input form controls with value higher than allowed maximum");
#endif
}

String validationMessageStepMismatchText(const String&, const String&)
{
    return WEB_UI_STRING("Enter a valid value", "Validation message for input form controls with value not respecting the step attribute");
}

String validationMessageBadInputForNumberText()
{
    return WEB_UI_STRING("Enter a number", "Validation message for number fields where the user entered a non-number string");
}

String clickToExitFullScreenText()
{
    return WEB_UI_STRING("Click to Exit Full Screen", "Message to display in browser window when in webkit full screen mode.");
}

#if ENABLE(VIDEO)

String trackNoLabelText()
{
    return WEB_UI_STRING_KEY("Unknown", "Unknown (audio/text track)", "Menu item label for a audio/text track that has no other name.");
}

String textTrackOffMenuItemText()
{
    return WEB_UI_STRING_KEY("Off", "Off (text track)", "Menu item label for the track that represents disabling closed captions.");
}

String textTrackAutomaticMenuItemText()
{
    return WEB_UI_STRING_KEY("Auto (Recommended)", "Auto (Recommended) (text track)", "Menu item label for automatic track selection behavior.");
}

#if USE(CF)

String addTrackLabelAsSuffix(const String& text, const String& label)
{
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ (%@)", "%@ (%@) (audio/text track)", "Audio/Text track display name format that includes the label and language of the track, in the form of 'Language (Label)'.", text.createCFString().get(), label.createCFString().get());
}

String textTrackKindClosedCaptionsDisplayName()
{
    return WEB_UI_CFSTRING_KEY("CC", "CC (text track)", "Display name for closed captions text tracks.");
}

String addTextTrackKindClosedCaptionsSuffix(const String& text)
{
    if (text.isEmpty())
        return textTrackKindClosedCaptionsDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ CC", "%@ CC (text track)", "Closed captions text track display name format that includes the language and/or locale (e.g. 'English CC').", text.createCFString().get());
}

String textTrackKindCaptionsDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Captions", "Captions (text track)", "Display name for text track kind 'captions'.");
}

String addTextTrackKindCaptionsSuffix(const String& text)
{
    if (text.isEmpty())
        return textTrackKindCaptionsDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Captions", "%@ Captions (text track)", "Captions text track display name format that includes the language and/or locale (e.g. 'English Captions').", text.createCFString().get());
}

String textTrackKindDescriptionsDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Descriptions", "Descriptions (text track)", "Display name for text track kind 'descriptions'.");
}

String addTextTrackKindDescriptionsSuffix(const String& text)
{
    if (text.isEmpty())
        return textTrackKindDescriptionsDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Descriptions", "%@ Descriptions (text track)", "Descriptions text track display name format that includes the language and/or locale (e.g. 'English Descriptions').", text.createCFString().get());
}

String textTrackKindChaptersDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Chapters", "Chapters (text track)", "Display name for text track kind 'chapters'.");
}

String addTextTrackKindChaptersSuffix(const String& text)
{
    if (text.isEmpty())
        return textTrackKindChaptersDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Chapters", "%@ Chapters (text track)", "Chapters text track display name format that includes the language and/or locale (e.g. 'English Chapters').", text.createCFString().get());
}

String textTrackKindMetadataDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Metadata", "Metadata (text track)", "Display name for text track kind 'metadata'.");
}

String addTextTrackKindMetadataSuffix(const String& text)
{
    if (text.isEmpty())
        return textTrackKindMetadataDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Metadata", "%@ Metadata (text track)", "Metadata text track display name format that includes the language and/or locale (e.g. 'English Metadata').", text.createCFString().get());
}

String textTrackKindSDHDisplayName()
{
    return WEB_UI_CFSTRING_KEY("SDH", "SDH (text track)", "Display name for SDH (i.e. deaf and/or hard of hearing) text tracks.");
}

String addTextTrackKindSDHSuffix(const String& text)
{
    if (text.isEmpty())
        return textTrackKindSDHDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ SDH", "%@ SDH (text track)", "SDH (i.e. deaf and/or hard of hearing) text track display name format that includes the language and/or locale (e.g. 'English SDH').", text.createCFString().get());
}

String textTrackKindEasyReaderDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Easy Reader", "Easy Reader (text track)", "Display name for easy reader (i.e. 3rd-grade level) text tracks.");
}

String addTextTrackKindEasyReaderSuffix(const String& text)
{
    if (text.isEmpty())
        return textTrackKindEasyReaderDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Easy Reader", "%@ Easy Reader (text track)", "Easy Reader (i.e. 3rd-grade level) text track display name format that includes the language and/or locale (e.g. 'English Easy Reader').", text.createCFString().get());
}

String textTrackKindForcedDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Forced", "Forced (text track)", "Display name for text track kind 'forced'.");
}

String addTextTrackKindForcedSuffix(const String& text)
{
    if (text.isEmpty())
        return textTrackKindForcedDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Forced", "%@ Forced (text track)", "Forced text track display name format that includes the language and/or locale (e.g. 'English Forced').", text.createCFString().get());
}

String audioTrackKindDescriptionsDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Descriptions", "Descriptions (audio track)", "Display name for audio track kind 'descriptions'.");
}

String addAudioTrackKindDescriptionsSuffix(const String& text)
{
    if (text.isEmpty())
        return audioTrackKindDescriptionsDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Descriptions", "%@ Descriptions (audio track)", "Descriptions audio track display name format that includes the language and/or locale (e.g. 'English Descriptions').", text.createCFString().get());
}

String audioTrackKindCommentaryDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Commentary", "Commentary (audio track)", "Display name for audio track kind 'commentary'.");
}

String addAudioTrackKindCommentarySuffix(const String& text)
{
    if (text.isEmpty())
        return audioTrackKindCommentaryDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Commentary", "%@ Commentary (audio track)", "Commentary audio track display name format that includes the language and/or locale (e.g. 'English Commentary').", text.createCFString().get());
}

#endif // USE(CF)

String contextMenuItemTagShowMediaStats()
{
    return WEB_UI_STRING("Show Media Stats", "Media stats context menu item");
}

#endif // ENABLE(VIDEO)

String snapshottedPlugInLabelTitle()
{
    return WEB_UI_STRING("Snapshotted Plug-In", "Title of the label to show on a snapshotted plug-in");
}

String snapshottedPlugInLabelSubtitle()
{
    return WEB_UI_STRING("Click to restart", "Subtitle of the label to show on a snapshotted plug-in");
}

String useBlockedPlugInContextMenuTitle()
{
    return WEB_UI_STRING("Show in blocked plug-in", "Title of the context menu item to show when PDFPlugin was used instead of a blocked plugin");
}

#if ENABLE(WEB_CRYPTO)

String webCryptoMasterKeyKeychainLabel(const String& localizedApplicationName)
{
#if USE(CF)
    return WEB_UI_FORMAT_CFSTRING("%@ WebCrypto Master Key", "Name of application's single WebCrypto master key in Keychain", localizedApplicationName.createCFString().get());
#elif USE(GLIB)
    return WEB_UI_FORMAT_STRING("%s WebCrypto Master Key", "Name of application's single WebCrypto master key in Keychain", localizedApplicationName.utf8().data());
#else
    return makeStringByReplacingAll(WEB_UI_STRING("<application> WebCrypto Master Key", "Name of application's single WebCrypto master key in Keychain"), "<application>"_s, localizedApplicationName);
#endif
}

String webCryptoMasterKeyKeychainComment()
{
    return WEB_UI_STRING("Used to encrypt WebCrypto keys in persistent storage, such as IndexedDB", "Description of WebCrypto master keys in Keychain");
}

#endif

#if PLATFORM(WATCHOS)

String numberPadOKButtonTitle()
{
    return WEB_UI_STRING_KEY("OK", "OK (OK button title in extra zoomed number pad)", "Title of the OK button for the number pad in zoomed form controls.");
}

String formControlCancelButtonTitle()
{
    return WEB_UI_STRING("Cancel", "Cancel");
}

String formControlHideButtonTitle()
{
    return WEB_UI_STRING("Hide", "Title of the Hide button for zoomed form controls.");
}

String formControlGoButtonTitle()
{
    return WEB_UI_STRING("Go", "Title of the Go button for zoomed form controls.");
}

String formControlSearchButtonTitle()
{
    return WEB_UI_STRING("Search", "Title of the Search button for zoomed form controls.");
}

String datePickerSetButtonTitle()
{
    return WEB_UI_STRING_KEY("Set", "Set (Button below date picker for extra zoom mode)", "Set button below date picker");
}

String datePickerDayLabelTitle()
{
    return WEB_UI_STRING_KEY("DAY", "DAY (Date picker for extra zoom mode)", "Day label in date picker");
}

String datePickerMonthLabelTitle()
{
    return WEB_UI_STRING_KEY("MONTH", "MONTH (Date picker for extra zoom mode)", "Month label in date picker");
}

String datePickerYearLabelTitle()
{
    return WEB_UI_STRING_KEY("YEAR", "YEAR (Date picker for extra zoom mode)", "Year label in date picker");
}

#endif

#if USE(SOUP)
String unacceptableTLSCertificate()
{
    return WEB_UI_STRING("Unacceptable TLS certificate", "Unacceptable TLS certificate error");
}
#endif

#if ENABLE(WEB_AUTHN)
// On macOS, Touch ID prompt is not guaranteed to show on top of the UI client, and therefore additional
// information is provided to help users to make decisions.
String makeCredentialTouchIDPromptTitle(const String& bundleName, const String& domain)
{
    return WEB_UI_FORMAT_CFSTRING("“%@” would like to use Touch ID for “%@”.", "Allow the specified bundle to use Touch ID to sign in to the specified website on this device", bundleName.createCFString().get(), domain.createCFString().get());
}

String getAssertionTouchIDPromptTitle(const String& bundleName, const String& domain)
{
    return WEB_UI_FORMAT_CFSTRING("“%@” would like to sign in to “%@”.", "Allow the specified bundle to sign in to the specified website", bundleName.createCFString().get(), domain.createCFString().get());
}

String genericTouchIDPromptTitle()
{
    return WEB_UI_STRING("Continue with Touch ID.", "Continue with Touch ID.");
}
#endif // ENABLE(WEB_AUTHN)

} // namespace WebCore
