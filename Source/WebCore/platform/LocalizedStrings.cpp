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

#if ENABLE(MEDIA_SESSION) && ENABLE(MEDIA_SESSION_CALL_TO_ACTION)
#include "CallToActionLabel.h"
#endif
#include "DateComponents.h"
#include "IntSize.h"
#include "NotImplemented.h"
#include <wtf/MathExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

#if USE(GLIB)
#include <wtf/glib/GUniquePtr.h>
#endif

namespace WebCore {

#if PLATFORM(COCOA)
RefPtr<StringImpl> formatLocalizedString(CFStringRef format, ...)
{
    va_list arguments;
    va_start(arguments, format);

    auto localizedFormat = copyLocalizedString(format);
ALLOW_NONLITERAL_FORMAT_BEGIN
    // The 'format' parameter is already checked for correct placeholders and parameters.
    auto result = adoptCF(CFStringCreateWithFormatAndArguments(0, 0, localizedFormat.get(), arguments));
ALLOW_NONLITERAL_FORMAT_END

    va_end(arguments);
    return StringImpl::create(result.get());
}
#elif PLATFORM(WIN)
RefPtr<StringImpl> formatLocalizedString(const wchar_t* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int len = _vscwprintf(format, arguments);
    Vector<wchar_t> buffer(len + 1);
    _vsnwprintf(buffer.mutableSpan().data(), len + 1, format, arguments);
    va_end(arguments);
    return { buffer.span().data() };
}
#else
// Because |format| is used as the second parameter to va_start, it cannot be a reference
// type according to section 18.7/3 of the C++ N1905 standard.
RefPtr<StringImpl> formatLocalizedString(const char* format, ...)
{
#if USE(GLIB)
    va_list arguments;
    va_start(arguments, format);
    GUniquePtr<gchar> result(g_strdup_vprintf(format, arguments));
    va_end(arguments);
    return StringImpl::fromUTF8(result.get());
#else
    notImplemented();
    return StringImpl::fromUTF8(format);
#endif
}
#endif

#if PLATFORM(COCOA)
static CFBundleRef webCoreBundleSingleton()
{
    static NeverDestroyed<RetainPtr<CFBundleRef>> bundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebCore"));
    ASSERT(bundle.get());
    return bundle.get().get();
}

RetainPtr<CFStringRef> copyLocalizedString(CFStringRef key)
{
    static CFStringRef notFound = CFSTR("localized string not found");

    auto result = adoptCF(CFBundleCopyLocalizedString(webCoreBundleSingleton(), key, notFound, nullptr));

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

#if PLATFORM(COCOA)
RefPtr<StringImpl> localizedString(CFStringRef key)
{
    return StringImpl::create(copyLocalizedString(key).get());
}
#elif PLATFORM(WIN)
RefPtr<StringImpl> localizedString(const wchar_t* key)
{
    return key;
}
#else
RefPtr<StringImpl> localizedString(const char* key)
{
    return StringImpl::fromUTF8(key);
}
#endif

#if PLATFORM(COCOA)

RefPtr<StringImpl> truncatedStringForMenuItem(StringImpl* original)
{
    // Truncate the string if it's too long. This number is roughly the same as the one used by AppKit.
    unsigned maxNumberOfGraphemeClustersInLookupMenuItem = 24;

    auto trimmed = trim(original, deprecatedIsSpaceOrNewline);
    unsigned numberOfCharacters = numCodeUnitsInGraphemeClusters(trimmed, maxNumberOfGraphemeClustersInLookupMenuItem);
    return numberOfCharacters == length(trimmed) ? trimmed : makeString(left(trimmed, numberOfCharacters), horizontalEllipsis);
}

#endif

RefPtr<StringImpl> inputElementAltText()
{
    return WEB_UI_STRING_KEY("Submit", "Submit (input element)", "alt text for <input> elements with no alt, title, or value");
}

RefPtr<StringImpl> resetButtonDefaultLabel()
{
    return WEB_UI_STRING("Reset", "default label for Reset buttons in forms on web pages");
}

RefPtr<StringImpl> searchableIndexIntroduction()
{
    return WEB_UI_STRING("This is a searchable index. Enter search keywords: ",
                         "text that appears at the start of nearly-obsolete web pages in the form of a 'searchable index'");
}

RefPtr<StringImpl> submitButtonDefaultLabel()
{
    return WEB_UI_STRING("Submit", "default label for Submit buttons in forms on web pages");
}

RefPtr<StringImpl> fileButtonChooseFileLabel()
{
    return WEB_UI_STRING("Choose File", "title for a single file chooser button used in HTML forms");
}

RefPtr<StringImpl> fileButtonChooseMultipleFilesLabel()
{
    return WEB_UI_STRING("Choose Files", "title for a multiple file chooser button used in HTML forms. This title should be as short as possible.");
}

RefPtr<StringImpl> fileButtonNoFileSelectedLabel()
{
    return WEB_UI_STRING("no file selected", "text to display in file button used in HTML forms when no file is selected");
}

RefPtr<StringImpl> fileButtonNoFilesSelectedLabel()
{
    return WEB_UI_STRING("no files selected", "text to display in file button used in HTML forms when no files are selected and the button allows multiple files to be selected");
}

RefPtr<StringImpl> defaultDetailsSummaryText()
{
    return WEB_UI_STRING("Details", "text to display in <details> tag when it has no <summary> child");
}

#if ENABLE(CONTEXT_MENUS)

RefPtr<StringImpl> contextMenuItemTagOpenLinkInNewWindow()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Link in New Window", "Open Link in New _Window", "Open in New Window context menu item");
}

RefPtr<StringImpl> contextMenuItemTagDownloadLinkToDisk()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Download Linked File", "_Download Linked File", "Download Linked File context menu item");
}

#if !PLATFORM(GTK)
RefPtr<StringImpl> contextMenuItemTagCopyLinkToClipboard()
{
    return WEB_UI_STRING("Copy Link", "Copy Link context menu item");
}
#endif

RefPtr<StringImpl> contextMenuItemTagOpenImageInNewWindow()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Image in New Window", "Open _Image in New Window", "Open Image in New Window context menu item");
}

#if !PLATFORM(GTK)
RefPtr<StringImpl> contextMenuItemTagDownloadImageToDisk()
{
    return WEB_UI_STRING("Download Image", "Download Image context menu item");
}
#endif

RefPtr<StringImpl> contextMenuItemTagCopyImageToClipboard()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Copy Image", "Cop_y Image", "Copy Image context menu item");
}

RefPtr<StringImpl> contextMenuItemTagOpenFrameInNewWindow()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Frame in New Window", "Open _Frame in New Window", "Open Frame in New Window context menu item");
}

RefPtr<StringImpl> contextMenuItemTagCopy()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Copy", "_Copy", "Copy context menu item");
}

RefPtr<StringImpl> contextMenuItemTagGoBack()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Back", "_Back", "Back context menu item");
}

RefPtr<StringImpl> contextMenuItemTagGoForward()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Forward", "_Forward", "Forward context menu item");
}

RefPtr<StringImpl> contextMenuItemTagStop()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Stop", "_Stop", "Stop context menu item");
}

RefPtr<StringImpl> contextMenuItemTagReload()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Reload", "_Reload", "Reload context menu item");
}

RefPtr<StringImpl> contextMenuItemTagCut()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Cut", "Cu_t", "Cut context menu item");
}

RefPtr<StringImpl> contextMenuItemTagPaste()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Paste", "_Paste", "Paste context menu item");
}

RefPtr<StringImpl> contextMenuItemTagNoGuessesFound()
{
    return WEB_UI_STRING("No Guesses Found", "No Guesses Found context menu item");
}

RefPtr<StringImpl> contextMenuItemTagIgnoreSpelling()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Ignore Spelling", "_Ignore Spelling", "Ignore Spelling context menu item");
}

RefPtr<StringImpl> contextMenuItemTagLearnSpelling()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Learn Spelling", "_Learn Spelling", "Learn Spelling context menu item");
}

#if !PLATFORM(COCOA)
RefPtr<StringImpl> contextMenuItemTagSearchWeb()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Search the Web", "_Search the Web", "Search the Web context menu item");
}
#endif

#if PLATFORM(COCOA)
RefPtr<StringImpl> contextMenuItemTagLookUpInDictionary(StringImpl* selectedString)
{
    auto selectedCFString = createCFString(truncatedStringForMenuItem(selectedString));
    return WEB_UI_FORMAT_CFSTRING("Look Up “%@”", "Look Up context menu item with selected word", selectedCFString.get());
}
#endif

RefPtr<StringImpl> contextMenuItemTagOpenLink()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Link", "_Open Link", "Open Link context menu item");
}

RefPtr<StringImpl> contextMenuItemTagIgnoreGrammar()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Ignore Grammar", "Ignore _Grammar", "Ignore Grammar context menu item");
}

RefPtr<StringImpl> contextMenuItemTagSpellingMenu()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Spelling and Grammar", "Spelling and _Grammar", "Spelling and Grammar context sub-menu item");
}

RefPtr<StringImpl> contextMenuItemTagShowSpellingPanel(bool show)
{
    if (show)
        return WEB_UI_STRING_WITH_MNEMONIC("Show Spelling and Grammar", "_Show Spelling and Grammar", "menu item title");
    return WEB_UI_STRING_WITH_MNEMONIC("Hide Spelling and Grammar", "_Hide Spelling and Grammar", "menu item title");
}

RefPtr<StringImpl> contextMenuItemTagCheckSpelling()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Check Document Now", "_Check Document Now", "Check spelling context menu item");
}

RefPtr<StringImpl> contextMenuItemTagCheckSpellingWhileTyping()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Check Spelling While Typing", "Check Spelling While _Typing", "Check spelling while typing context menu item");
}

RefPtr<StringImpl> contextMenuItemTagCheckGrammarWithSpelling()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Check Grammar With Spelling", "Check _Grammar With Spelling", "Check grammar with spelling context menu item");
}

RefPtr<StringImpl> contextMenuItemTagFontMenu()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Font", "_Font", "Font context sub-menu item");
}

RefPtr<StringImpl> contextMenuItemTagBold()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Bold", "_Bold", "Bold context menu item");
}

RefPtr<StringImpl> contextMenuItemTagItalic()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Italic", "_Italic", "Italic context menu item");
}

RefPtr<StringImpl> contextMenuItemTagUnderline()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Underline", "_Underline", "Underline context menu item");
}

RefPtr<StringImpl> contextMenuItemTagOutline()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Outline", "_Outline", "Outline context menu item");
}

#if !PLATFORM(GTK)
RefPtr<StringImpl> contextMenuItemTagWritingDirectionMenu()
{
    return WEB_UI_STRING("Paragraph Direction", "Paragraph direction context sub-menu item");
}

RefPtr<StringImpl> contextMenuItemTagTextDirectionMenu()
{
    return WEB_UI_STRING("Selection Direction", "Selection direction context sub-menu item");
}

RefPtr<StringImpl> contextMenuItemTagDefaultDirection()
{
    return WEB_UI_STRING("Default", "Default writing direction context menu item");
}

RefPtr<StringImpl> contextMenuItemTagLeftToRight()
{
    return WEB_UI_STRING("Left to Right", "Left to Right context menu item");
}

RefPtr<StringImpl> contextMenuItemTagRightToLeft()
{
    return WEB_UI_STRING("Right to Left", "Right to Left context menu item");
}
#endif

RefPtr<StringImpl> contextMenuItemTagOpenVideoInNewWindow()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Video in New Window", "Open _Video in New Window", "Open Video in New Window context menu item");
}

RefPtr<StringImpl> contextMenuItemTagOpenAudioInNewWindow()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Open Audio in New Window", "Open _Audio in New Window", "Open Audio in New Window context menu item");
}

RefPtr<StringImpl> contextMenuItemTagDownloadVideoToDisk()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Download Video", "Download _Video", "Download Video To Disk context menu item");
}

RefPtr<StringImpl> contextMenuItemTagDownloadAudioToDisk()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Download Audio", "Download _Audio", "Download Audio To Disk context menu item");
}

#if !PLATFORM(GTK)
RefPtr<StringImpl> contextMenuItemTagCopyVideoLinkToClipboard()
{
    return WEB_UI_STRING("Copy Video Address", "Copy Video Address Location context menu item");
}

RefPtr<StringImpl> contextMenuItemTagCopyAudioLinkToClipboard()
{
    return WEB_UI_STRING("Copy Audio Address", "Copy Audio Address Location context menu item");
}

RefPtr<StringImpl> contextMenuItemTagToggleMediaControls()
{
    return WEB_UI_STRING("Controls", "Media Controls context menu item");
}

RefPtr<StringImpl> contextMenuItemTagShowMediaControls()
{
    return WEB_UI_STRING("Show Controls", "Show Media Controls context menu item");
}

RefPtr<StringImpl> contextMenuItemTagHideMediaControls()
{
    return WEB_UI_STRING("Hide Controls", "Hide Media Controls context menu item");
}

RefPtr<StringImpl> contextMenuItemTagToggleMediaLoop()
{
    return WEB_UI_STRING("Loop", "Media Loop context menu item");
}

RefPtr<StringImpl> contextMenuItemTagEnterVideoFullscreen()
{
    return WEB_UI_STRING("Enter Full Screen", "Video Enter Full Screen context menu item");
}

RefPtr<StringImpl> contextMenuItemTagExitVideoFullscreen()
{
    return WEB_UI_STRING_KEY("Exit Full Screen", "Exit Full Screen (context menu)", "Video Exit Full Screen context menu item");
}
#endif

RefPtr<StringImpl> contextMenuItemTagMediaPlay()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Play", "_Play", "Media Play context menu item");
}

RefPtr<StringImpl> contextMenuItemTagMediaPause()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Pause", "_Pause", "Media Pause context menu item");
}

RefPtr<StringImpl> contextMenuItemTagMediaMute()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Mute", "_Mute", "Media Mute context menu item");
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
RefPtr<StringImpl> contextMenuItemTagPlayAllAnimations()
{
    return WEB_UI_STRING("Play All Animations", "Play All Animations context menu item");
}

RefPtr<StringImpl> contextMenuItemTagPauseAllAnimations()
{
    return WEB_UI_STRING("Pause All Animations", "Pause All Animations context menu item");
}

RefPtr<StringImpl> contextMenuItemTagPlayAnimation()
{
    return WEB_UI_STRING("Play Animation", "Title for play animation action button or context menu item");
}

RefPtr<StringImpl> contextMenuItemTagPauseAnimation()
{
    return WEB_UI_STRING("Pause Animation", "Title for pause animation action button or context menu item");
}
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

RefPtr<StringImpl> contextMenuItemTagInspectElement()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Inspect Element", "Inspect _Element", "Inspect Element context menu item");
}

#if HAVE(TRANSLATION_UI_SERVICES)
RefPtr<StringImpl> contextMenuItemTagTranslate(StringImpl* selectedString)
{
    auto selectedCFString = createCFString(truncatedStringForMenuItem(selectedString));
    return WEB_UI_FORMAT_CFSTRING("Translate “%@”", "Translate context menu item with selected word", selectedCFString.get());
}
#endif

#if ENABLE(WRITING_TOOLS)
RefPtr<StringImpl> contextMenuItemTagWritingTools()
{
#if ENABLE(TOP_LEVEL_WRITING_TOOLS_CONTEXT_MENU_ITEMS)
    return WEB_UI_STRING("Show Writing Tools", "Writing Tools context menu item");
#else
    return WEB_UI_STRING("Writing Tools", "Writing Tools context menu item");
#endif
}

RefPtr<StringImpl> contextMenuItemTagProofread()
{
    return WEB_UI_STRING("Proofread", "Proofread context menu item");
}

RefPtr<StringImpl> contextMenuItemTagRewrite()
{
    return WEB_UI_STRING("Rewrite", "Rewrite context menu item");
}

RefPtr<StringImpl> contextMenuItemTagSummarize()
{
    return WEB_UI_STRING("Summarize", "Summarize context menu item");
}
#endif

#if ENABLE(UNIFIED_PDF)
RefPtr<StringImpl> contextMenuItemPDFOpenWithDefaultViewer(StringImpl* appName)
{
    return WEB_UI_FORMAT_STRING("Open with %@", "Open with default PDF viewer context menu item", createCFString(appName).get());
}
#endif

#if ENABLE(PDFJS) || ENABLE(UNIFIED_PDF)
RefPtr<StringImpl> contextMenuItemPDFSinglePageContinuous()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Single Page Continuous", "_Single Page Continuous", "Single Page Continuous context menu item");
}

RefPtr<StringImpl> contextMenuItemPDFTwoPagesContinuous()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Two Pages Continuous", "_Two Pages Continuous", "Two Pages Continuous context menu item");
}

RefPtr<StringImpl> contextMenuItemPDFZoomIn()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Zoom In", "_Zoom In", "Zoom In Continuous context menu item");
}

RefPtr<StringImpl> contextMenuItemPDFZoomOut()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Zoom Out", "_Zoom Out", "Zoom Out context menu item");
}

RefPtr<StringImpl> contextMenuItemPDFActualSize()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Actual Size", "_Actual Size", "Actual Size context menu item");
}

RefPtr<StringImpl> contextMenuItemPDFAutoSize()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Automatically Resize", "_Automatically Resize", "Automatically Resize context menu item");
}

RefPtr<StringImpl> contextMenuItemPDFNextPage()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Next Page", "_Next Page", "Next Page context menu item");
}

RefPtr<StringImpl> contextMenuItemPDFPreviousPage()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Previous Page", "_Previous Page", "Previous Page context menu item");
}
#endif

#endif // ENABLE(CONTEXT_MENUS)

#if ENABLE(PDFJS) || ENABLE(UNIFIED_PDF)
RefPtr<StringImpl> contextMenuItemPDFSinglePage()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Single Page", "_Single Page", "Single Page context menu item");
}

RefPtr<StringImpl> contextMenuItemPDFTwoPages()
{
    return WEB_UI_STRING_WITH_MNEMONIC("Two Pages", "_Two Pages", "Two Pages context menu item");
}
#endif

#if !PLATFORM(COCOA)
RefPtr<StringImpl> pdfDocumentTypeDescription()
{
    // Also exposed to DOM.
    return WEB_UI_STRING("Portable Document Format", "Description of the primary type supported by the PDF pseudo plug-in.");
}
#endif // !PLATFORM(COCOA)

#if !PLATFORM(IOS_FAMILY)

RefPtr<StringImpl> searchMenuNoRecentSearchesText()
{
    return WEB_UI_STRING("No recent searches", "Label for only item in menu that appears when clicking on the search field image, when no searches have been performed");
}

RefPtr<StringImpl> searchMenuRecentSearchesText()
{
    return WEB_UI_STRING("Recent Searches", "label for first item in the menu that appears when clicking on the search field image, used as embedded menu title");
}

RefPtr<StringImpl> searchMenuClearRecentSearchesText()
{
    return WEB_UI_STRING("Clear Recent Searches", "menu item in Recent Searches menu that empties menu's contents");
}

#endif // !PLATFORM(IOS_FAMILY)

#if ENABLE(MEDIA_STREAM)
RefPtr<StringImpl> defaultSystemSpeakerLabel()
{
    return WEB_UI_STRING("Default", "label for the default system speaker");
}
#endif

RefPtr<StringImpl> AXWebAreaText()
{
    return WEB_UI_STRING("HTML content", "accessibility role description for web area");
}

RefPtr<StringImpl> AXLinkText()
{
    return WEB_UI_STRING("link", "accessibility role description for link");
}

RefPtr<StringImpl> AXListMarkerText()
{
    return WEB_UI_STRING("list marker", "accessibility role description for list marker");
}

RefPtr<StringImpl> AXImageMapText()
{
    return WEB_UI_STRING("image map", "accessibility role description for image map");
}

RefPtr<StringImpl> AXHeadingText()
{
    return WEB_UI_STRING("heading", "accessibility role description for headings");
}

RefPtr<StringImpl> AXColorWellText()
{
    return WEB_UI_STRING("color well", "accessibility role description for a color well");
}

RefPtr<StringImpl> AXDefinitionText()
{
    return WEB_UI_STRING("definition", "role description of ARIA definition role");
}

RefPtr<StringImpl> AXDescriptionListText()
{
    return WEB_UI_STRING("description list", "accessibility role description of a description list");
}

RefPtr<StringImpl> AXDescriptionListTermText()
{
    return WEB_UI_STRING("term", "term word of a description list");
}

RefPtr<StringImpl> AXDescriptionListDetailText()
{
    return WEB_UI_STRING("description", "description detail");
}

RefPtr<StringImpl> AXDetailsText()
{
    return WEB_UI_STRING("details", "accessibility role description for a details element");
}

RefPtr<StringImpl> AXSummaryText()
{
    return WEB_UI_STRING("summary", "accessibility role description for a summary element");
}

RefPtr<StringImpl> AXFooterRoleDescriptionText()
{
    return WEB_UI_STRING("section footer", "accessibility role description for a footer");
}

RefPtr<StringImpl> AXHeaderRoleDescriptionText()
{
    return WEB_UI_STRING("section header", "accessibility role description for a header");
}

RefPtr<StringImpl> AXSuggestionRoleDescriptionText()
{
    return WEB_UI_STRING("suggestion", "accessibility role description for a suggestion element");
}

RefPtr<StringImpl> AXFileUploadButtonText()
{
    return WEB_UI_STRING("file upload button", "accessibility role description for a file upload button");
}

RefPtr<StringImpl> AXOutputText()
{
    return WEB_UI_STRING("output", "accessibility role description for an output element");
}

RefPtr<StringImpl> AXAttachmentRoleText()
{
    return WEB_UI_STRING("attachment", "accessibility role description for an attachment element");
}

RefPtr<StringImpl> AXSearchFieldCancelButtonText()
{
    return WEB_UI_STRING("cancel", "accessibility description for a search field cancel button");
}

RefPtr<StringImpl> AXFeedText()
{
    return WEB_UI_STRING("feed", "accessibility role description for a group containing a scrollable list of articles.");
}

RefPtr<StringImpl> AXFigureText()
{
    return WEB_UI_STRING("figure", "accessibility role description for a figure element.");
}

RefPtr<StringImpl> AXEmailFieldText()
{
    return WEB_UI_STRING("email field", "accessibility role description for an email field.");
}

RefPtr<StringImpl> AXTelephoneFieldText()
{
    return WEB_UI_STRING("telephone number field", "accessibility role description for a telephone number field.");
}

RefPtr<StringImpl> AXURLFieldText()
{
    return WEB_UI_STRING("URL field", "accessibility role description for a URL field.");
}

RefPtr<StringImpl> AXDateFieldText()
{
    return WEB_UI_STRING("date field", "accessibility role description for a date field.");
}

RefPtr<StringImpl> AXTimeFieldText()
{
    return WEB_UI_STRING("time field", "accessibility role description for a time field.");
}

RefPtr<StringImpl> AXDateFieldMonthText()
{
    return WEB_UI_STRING("month", "accessibility label for a date field month input.");
}

RefPtr<StringImpl> AXDateFieldDayText()
{
    return WEB_UI_STRING("day", "accessibility label for a date field day input.");
}

RefPtr<StringImpl> AXDateFieldYearText()
{
    return WEB_UI_STRING("year", "accessibility label for a date field year input.");
}

RefPtr<StringImpl> AXTimeFieldHourText()
{
    return WEB_UI_STRING("hour", "accessibility label for hour fields.");
}

RefPtr<StringImpl> AXTimeFieldMinuteText()
{
    return WEB_UI_STRING("minutes", "accessibility label for minutes fields.");
}

RefPtr<StringImpl> AXTimeFieldSecondText()
{
    return WEB_UI_STRING("seconds", "accessibility label for seconds fields.");
}

RefPtr<StringImpl> AXTimeFieldMillisecondText()
{
    return WEB_UI_STRING("milliseconds", "accessibility label for milliseconds fields.");
}

RefPtr<StringImpl> AXDateTimeFieldText()
{
    return WEB_UI_STRING("date and time field", "accessibility role description for a date and time field.");
}

RefPtr<StringImpl> AXMonthFieldText()
{
    return WEB_UI_STRING("month and year field", "accessibility role description for a month field.");
}

RefPtr<StringImpl> AXNumberFieldText()
{
    return WEB_UI_STRING("number field", "accessibility role description for a number field.");
}

RefPtr<StringImpl> AXWeekFieldText()
{
    return WEB_UI_STRING("week and year field", "accessibility role description for a time field.");
}

RefPtr<StringImpl> AXARIAContentGroupText(StringView ariaType)
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
    if (ariaType == "ARIALandmarkForm"_s)
        return WEB_UI_STRING("form", "An ARIA accessibility group that acts as a form region.");
    if (ariaType == "ARIALandmarkMain"_s)
        return WEB_UI_STRING("main", "An ARIA accessibility group that is the main portion of the website.");
    if (ariaType == "ARIALandmarkNavigation"_s)
        return WEB_UI_STRING("navigation", "An ARIA accessibility group that contains the main navigation elements of a website.");
    if (ariaType == "ARIALandmarkRegion"_s)
        return WEB_UI_STRING("region", "An ARIA accessibility group that acts as a distinct region in a document.");
    if (ariaType == "ARIALandmarkSearch"_s)
        return WEB_UI_STRING("search", "An ARIA accessibility group that contains a search feature of a website.");
    if (ariaType == "ARIASectionFooter"_s)
        return WEB_UI_STRING("section footer", "An ARIA accessibility group that acts as a footer region.");
    if (ariaType == "ARIASectionHeader"_s)
        return WEB_UI_STRING("section header", "An ARIA accessibility group that acts as a header region.");
    if (ariaType == "ARIAUserInterfaceTooltip"_s)
        return WEB_UI_STRING("tooltip", "An ARIA accessibility group that acts as a tooltip.");
    if (ariaType == "ARIATabPanel"_s)
        return WEB_UI_STRING("tab panel", "An ARIA accessibility group that contains the content of a tab.");
    if (ariaType == "ARIADocumentMath"_s)
        return WEB_UI_STRING("math", "An ARIA accessibility group that contains mathematical symbols.");
    return nullptr;
}

RefPtr<StringImpl> AXHorizontalRuleDescriptionText()
{
    return WEB_UI_STRING("separator", "accessibility role description for a horizontal rule [<hr>]");
}

RefPtr<StringImpl> AXMarkText()
{
    return WEB_UI_STRING("highlighted", "accessibility role description for a mark element");
}

RefPtr<StringImpl> AXButtonActionVerb()
{
    return WEB_UI_STRING("press", "Verb stating the action that will occur when a button is pressed, as used by accessibility");
}

RefPtr<StringImpl> AXRadioButtonActionVerb()
{
    return WEB_UI_STRING("select", "Verb stating the action that will occur when a radio button is clicked, as used by accessibility");
}

RefPtr<StringImpl> AXTextFieldActionVerb()
{
    return WEB_UI_STRING("activate", "Verb stating the action that will occur when a text field is selected, as used by accessibility");
}

RefPtr<StringImpl> AXCheckedCheckboxActionVerb()
{
    return WEB_UI_STRING("uncheck", "Verb stating the action that will occur when a checked checkbox is clicked, as used by accessibility");
}

RefPtr<StringImpl> AXUncheckedCheckboxActionVerb()
{
    return WEB_UI_STRING("check", "Verb stating the action that will occur when an unchecked checkbox is clicked, as used by accessibility");
}

RefPtr<StringImpl> AXLinkActionVerb()
{
    return WEB_UI_STRING("jump", "Verb stating the action that will occur when a link is clicked, as used by accessibility");
}

RefPtr<StringImpl> AXMenuListPopupActionVerb()
{
    notImplemented();
    return "select"_si;
}

RefPtr<StringImpl> AXMenuListActionVerb()
{
    notImplemented();
    return "select"_si;
}

RefPtr<StringImpl> AXShowWritingToolsLabel()
{
    return WEB_UI_STRING("Show Writing Tools", "Label for the AX custom action that brings up the Writing Tools affordance");
}

RefPtr<StringImpl> AXListItemActionVerb()
{
    notImplemented();
    return "select"_si;
}

#if ENABLE(APPLE_PAY)
RefPtr<StringImpl> AXApplePayPlainLabel()
{
    return WEB_UI_STRING("Apple Pay", "Label for the plain Apple Pay button.");
}

RefPtr<StringImpl> AXApplePayBuyLabel()
{
    return WEB_UI_STRING("Buy with Apple Pay", "Label for the buy with Apple Pay button.");
}

RefPtr<StringImpl> AXApplePaySetupLabel()
{
    return WEB_UI_STRING("Set up with Apple Pay", "Label for the set up with Apple Pay button.");
}

RefPtr<StringImpl> AXApplePayDonateLabel()
{
    return WEB_UI_STRING("Donate with Apple Pay", "Label for the donate with Apple Pay button.");
}

RefPtr<StringImpl> AXApplePayCheckOutLabel()
{
    return WEB_UI_STRING("Check out with Apple Pay", "Label for the check out with Apple Pay button.");
}

RefPtr<StringImpl> AXApplePayBookLabel()
{
    return WEB_UI_STRING("Book with Apple Pay", "Label for the book with Apple Pay button.");
}

RefPtr<StringImpl> AXApplePaySubscribeLabel()
{
    return WEB_UI_STRING("Subscribe with Apple Pay", "Label for the subcribe with Apple Pay button.");
}

#if ENABLE(APPLE_PAY_NEW_BUTTON_TYPES)
RefPtr<StringImpl> AXApplePayReloadLabel()
{
    return WEB_UI_STRING("Reload with Apple Pay", "Label for the reload with Apple Pay button.");
}
RefPtr<StringImpl> AXApplePayAddMoneyLabel()
{
    return WEB_UI_STRING("Add money with Apple Pay", "Label for the add money with Apple Pay button.");
}
RefPtr<StringImpl> AXApplePayTopUpLabel()
{
    return WEB_UI_STRING("Top up with Apple Pay", "Label for the top up with Apple Pay button.");
}
RefPtr<StringImpl> AXApplePayOrderLabel()
{
    return WEB_UI_STRING("Order with Apple Pay", "Label for the order with Apple Pay button.");
}
RefPtr<StringImpl> AXApplePayRentLabel()
{
    return WEB_UI_STRING("Rent with Apple Pay", "Label for the rent with Apple Pay button.");
}
RefPtr<StringImpl> AXApplePaySupportLabel()
{
    return WEB_UI_STRING("Support with Apple Pay", "Label for the support with Apple Pay button.");
}
RefPtr<StringImpl> AXApplePayContributeLabel()
{
    return WEB_UI_STRING("Contribute with Apple Pay", "Label for the contribute with Apple Pay button.");
}
RefPtr<StringImpl> AXApplePayTipLabel()
{
    return WEB_UI_STRING("Tip with Apple Pay", "Label for the tip with Apple Pay button.");
}
#endif
#endif

RefPtr<StringImpl> AXAutoFillCredentialsLabel()
{
    return WEB_UI_STRING("password AutoFill", "Label for the AutoFill credentials button inside a text field.");
}

RefPtr<StringImpl> AXAutoFillContactsLabel()
{
    return WEB_UI_STRING("contact info AutoFill", "Label for the AutoFill contacts button inside a text field.");
}

RefPtr<StringImpl> AXAutoFillStrongPasswordLabel()
{
    return WEB_UI_STRING("strong password AutoFill", "Label for the strong password AutoFill button inside a text field.");
}

RefPtr<StringImpl> AXAutoFillCreditCardLabel()
{
    return WEB_UI_STRING("credit card AutoFill", "Label for the credit card AutoFill button inside a text field.");
}

RefPtr<StringImpl> AXAutoFillLoadingLabel()
{
    return WEB_UI_STRING("loading AutoFill", "Label for the loading AutoFill button inside a text field.");
}

RefPtr<StringImpl> autoFillStrongPasswordLabel()
{
    return WEB_UI_STRING("Strong Password", "Label for strong password.");
}

RefPtr<StringImpl> missingPluginText()
{
    return WEB_UI_STRING("Missing Plug-in", "Label text to be used when a plugin is missing");
}

RefPtr<StringImpl> crashedPluginText()
{
    return WEB_UI_STRING("Plug-in Failure", "Label text to be used if plugin host process has crashed");
}

RefPtr<StringImpl> blockedPluginByContentSecurityPolicyText()
{
    return WEB_UI_STRING_KEY("Blocked Plug-in", "Blocked Plug-In (Blocked by page's Content Security Policy)", "Label text to be used if plugin is blocked by a page's Content Security Policy");
}

RefPtr<StringImpl> insecurePluginVersionText()
{
    return WEB_UI_STRING_KEY("Blocked Plug-in", "Blocked Plug-In (Insecure plug-in)", "Label text to be used when an insecure plug-in version was blocked from loading");
}

RefPtr<StringImpl> unsupportedPluginText()
{
    return WEB_UI_STRING_KEY("Unsupported Plug-in", "Unsupported Plug-In", "Label text to be used when an unsupported plug-in was blocked from loading");
}

RefPtr<StringImpl> pluginTooSmallText()
{
    return WEB_UI_STRING_KEY("Plug-In too small", "Plug-In too small", "Label text to be used when a plug-in was blocked from loading because it was too small");
}

RefPtr<StringImpl> multipleFileUploadText(unsigned numberOfFiles)
{
    return WEB_UI_FORMAT_STRING("%d files", "Label to describe the number of files selected in a file upload control that allows multiple files", numberOfFiles);
}

RefPtr<StringImpl> unknownFileSizeText()
{
    return WEB_UI_STRING_KEY("Unknown", "Unknown (filesize)", "Unknown filesize FTP directory listing item");
}

RefPtr<StringImpl> htmlSelectMultipleItems(size_t count)
{
    switch (count) {
    case 0:
        return WEB_UI_STRING("0 Items", "Present the element <select multiple> when no <option> items are selected");
    case 1:
        return WEB_UI_STRING("1 Item", "Present the element <select multiple> when a single <option> is selected");
    default:
        return WEB_UI_FORMAT_STRING("%zu Items", "Present the number of selected <option> items in a <select multiple> element", count);
    }
}

RefPtr<StringImpl> imageTitle(StringImpl* filename, const IntSize& size)
{
#if PLATFORM(COCOA)
    auto locale = adoptCF(CFLocaleCopyCurrent());
    auto formatter = adoptCF(CFNumberFormatterCreate(0, locale.get(), kCFNumberFormatterDecimalStyle));

    int widthInt = size.width();
    auto width = adoptCF(CFNumberCreate(0, kCFNumberIntType, &widthInt));
    auto widthString = adoptCF(CFNumberFormatterCreateStringWithNumber(0, formatter.get(), width.get()));

    int heightInt = size.height();
    auto height = adoptCF(CFNumberCreate(0, kCFNumberIntType, &heightInt));
    auto heightString = adoptCF(CFNumberFormatterCreateStringWithNumber(0, formatter.get(), height.get()));

    return WEB_UI_FORMAT_CFSTRING("%@ %@×%@ pixels", "window title for a standalone image (uses multiplication symbol, not x)", createCFString(filename).get(), widthString.get(), heightString.get());
#elif PLATFORM(WIN)
    return WEB_UI_FORMAT_STRING("%s %d×%d pixels", "window title for a standalone image (uses multiplication symbol, not x)", wideCharacters(filename).span().data(), size.width(), size.height());
#elif USE(GLIB)
    return WEB_UI_FORMAT_STRING("%s %d×%d pixels", "window title for a standalone image (uses multiplication symbol, not x)", utf8(filename), size.width(), size.height());
#else
    return makeStringByReplacingAll(WEB_UI_FORMAT_STRING("<filename> %d×%d pixels", "window title for a standalone image (uses multiplication symbol, not x)", size.width(), size.height()), "<filename>"_s, filename);
#endif
}

RefPtr<StringImpl> mediaElementLoadingStateText()
{
    return WEB_UI_STRING("Loading…", "Media controller status message when the media is loading");
}

RefPtr<StringImpl> mediaElementLiveBroadcastStateText()
{
    return WEB_UI_STRING("Live Broadcast", "Media controller status message when watching a live broadcast");
}

RefPtr<StringImpl> localizedMediaControlElementString(StringImpl* name)
{
    if (equal(name, "AudioElement"_s))
        return WEB_UI_STRING("audio playback", "accessibility label for audio element controller");
    if (equal(name, "VideoElement"_s))
        return WEB_UI_STRING("video playback", "accessibility label for video element controller");
    if (equal(name, "MuteButton"_s))
        return WEB_UI_STRING("mute", "accessibility label for mute button");
    if (equal(name, "UnMuteButton"_s))
        return WEB_UI_STRING("unmute", "accessibility label for turn mute off button");
    if (equal(name, "PlayButton"_s))
        return WEB_UI_STRING("play", "accessibility label for play button");
    if (equal(name, "PauseButton"_s))
        return WEB_UI_STRING("pause", "accessibility label for pause button");
    if (equal(name, "Slider"_s))
        return WEB_UI_STRING("movie time", "accessibility label for timeline slider");
    if (equal(name, "SliderThumb"_s))
        return WEB_UI_STRING("timeline slider thumb", "accessibility label for timeline thumb");
    if (equal(name, "RewindButton"_s))
        return WEB_UI_STRING("back 30 seconds", "accessibility label for seek back 30 seconds button");
    if (equal(name, "ReturnToRealtimeButton"_s))
        return WEB_UI_STRING("return to real time", "accessibility label for return to real time button");
    if (equal(name, "CurrentTimeDisplay"_s))
        return WEB_UI_STRING("elapsed time", "accessibility label for elapsed time display");
    if (equal(name, "TimeRemainingDisplay"_s))
        return WEB_UI_STRING("remaining time", "accessibility label for time remaining display");
    if (equal(name, "StatusDisplay"_s))
        return WEB_UI_STRING("status", "accessibility label for movie status");
    if (equal(name, "EnterFullscreenButton"_s))
        return WEB_UI_STRING("enter full screen", "accessibility label for enter full screen button");
    if (equal(name, "ExitFullscreenButton"_s))
        return WEB_UI_STRING("exit full screen", "accessibility label for exit full screen button");
    if (equal(name, "SeekForwardButton"_s))
        return WEB_UI_STRING("fast forward", "accessibility label for fast forward button");
    if (equal(name, "SeekBackButton"_s))
        return WEB_UI_STRING("fast reverse", "accessibility label for fast reverse button");
    if (equal(name, "ShowClosedCaptionsButton"_s))
        return WEB_UI_STRING("show closed captions", "accessibility label for show closed captions button");
    if (equal(name, "HideClosedCaptionsButton"_s))
        return WEB_UI_STRING("hide closed captions", "accessibility label for hide closed captions button");

    // FIXME: the ControlsPanel container should never be visible in the accessibility hierarchy.
    if (equal(name, "ControlsPanel"_s))
        return nullptr;

    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<StringImpl> localizedMediaControlElementHelpText(StringImpl* name)
{
    if (equal(name, "AudioElement"_s))
        return WEB_UI_STRING("audio element playback controls and status display", "accessibility help text for audio element controller");
    if (equal(name, "VideoElement"_s))
        return WEB_UI_STRING("video element playback controls and status display", "accessibility help text for video element controller");
    if (equal(name, "MuteButton"_s))
        return WEB_UI_STRING("mute audio tracks", "accessibility help text for mute button");
    if (equal(name, "UnMuteButton"_s))
        return WEB_UI_STRING("unmute audio tracks", "accessibility help text for un mute button");
    if (equal(name, "PlayButton"_s))
        return WEB_UI_STRING("begin playback", "accessibility help text for play button");
    if (equal(name, "PauseButton"_s))
        return WEB_UI_STRING("pause playback", "accessibility help text for pause button");
    if (equal(name, "Slider"_s))
        return WEB_UI_STRING("movie time scrubber", "accessibility help text for timeline slider");
    if (equal(name, "SliderThumb"_s))
        return WEB_UI_STRING("movie time scrubber thumb", "accessibility help text for timeline slider thumb");
    if (equal(name, "RewindButton"_s))
        return WEB_UI_STRING("seek movie back 30 seconds", "accessibility help text for jump back 30 seconds button");
    if (equal(name, "ReturnToRealtimeButton"_s))
        return WEB_UI_STRING("resume real time streaming", "accessibility help text for return streaming movie to real time button");
    if (equal(name, "CurrentTimeDisplay"_s))
        return WEB_UI_STRING("current movie time in seconds", "accessibility help text for elapsed time display");
    if (equal(name, "TimeRemainingDisplay"_s))
        return WEB_UI_STRING("number of seconds of movie remaining", "accessibility help text for remaining time display");
    if (equal(name, "StatusDisplay"_s))
        return WEB_UI_STRING("current movie status", "accessibility help text for movie status display");
    if (equal(name, "SeekBackButton"_s))
        return WEB_UI_STRING("seek quickly back", "accessibility help text for fast rewind button");
    if (equal(name, "SeekForwardButton"_s))
        return WEB_UI_STRING("seek quickly forward", "accessibility help text for fast forward button");
    if (equal(name, "FullscreenButton"_s))
        return WEB_UI_STRING("Play movie in full screen mode", "accessibility help text for enter full screen button");
    if (equal(name, "ShowClosedCaptionsButton"_s))
        return WEB_UI_STRING("start displaying closed captions", "accessibility help text for show closed captions button");
    if (equal(name, "HideClosedCaptionsButton"_s))
        return WEB_UI_STRING("stop displaying closed captions", "accessibility help text for hide closed captions button");

    // The description of this button is descriptive enough that it doesn't require help text.
    if (equal(name, "EnterFullscreenButton"_s))
        return nullptr;
    
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<StringImpl> localizedMediaTimeDescription(float time)
{
    if (!std::isfinite(time))
        return WEB_UI_STRING("indefinite time", "accessibility help text for an indefinite media controller time value");

    int seconds = static_cast<int>(std::abs(time));
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

RefPtr<StringImpl> validationMessageValueMissingText()
{
    return WEB_UI_STRING("Fill out this field", "Validation message for required form control elements that have no value");
}

RefPtr<StringImpl> validationMessageValueMissingForCheckboxText()
{
    return WEB_UI_STRING("Select this checkbox", "Validation message for required checkboxes that have not been selected");
}

RefPtr<StringImpl> validationMessageValueMissingForFileText()
{
    return WEB_UI_STRING("Select a file", "Validation message for required file inputs that have no value");
}

RefPtr<StringImpl> validationMessageValueMissingForMultipleFileText()
{
    return validationMessageValueMissingForFileText();
}

RefPtr<StringImpl> validationMessageValueMissingForRadioText()
{
    return WEB_UI_STRING("Select one of these options", "Validation message for required radio boxes that have no selection");
}

RefPtr<StringImpl> validationMessageValueMissingForSelectText()
{
    return WEB_UI_STRING("Select an item in the list", "Validation message for required menu list controls that have no selection");
}

RefPtr<StringImpl> validationMessageValueMissingForSwitchText()
{
    return WEB_UI_STRING("Tap this switch", "Validation message for required switches that are not on");
}

RefPtr<StringImpl> validationMessageTypeMismatchText()
{
    return WEB_UI_STRING("Invalid value", "Validation message for input form controls with a value not matching type");
}

RefPtr<StringImpl> validationMessageTypeMismatchForEmailText()
{
    return WEB_UI_STRING("Enter an email address", "Validation message for input form controls of type 'email' that have an invalid value");
}

RefPtr<StringImpl> validationMessageTypeMismatchForMultipleEmailText()
{
    return validationMessageTypeMismatchForEmailText();
}

RefPtr<StringImpl> validationMessageTypeMismatchForURLText()
{
    return WEB_UI_STRING("Enter a URL", "Validation message for input form controls of type 'url' that have an invalid value");
}

RefPtr<StringImpl> validationMessagePatternMismatchText()
{
    return WEB_UI_STRING("Match the requested format", "Validation message for input form controls requiring a constrained value according to pattern");
}

RefPtr<StringImpl> validationMessagePatternMismatchText(StringImpl* title)
{
#if PLATFORM(COCOA)
    return WEB_UI_FORMAT_CFSTRING("Match the requested format: %@", "Validation message for input form controls requiring a constrained value according to pattern followed by a website-provided description of the pattern", createCFString(title).get());
#elif USE(GLIB)
    return WEB_UI_FORMAT_STRING("Match the requested format: %s", "Validation message for input form controls requiring a constrained value according to pattern followed by a website-provided description of the pattern", utf8(title));
#else
    UNUSED_PARAM(title);
    return validationMessagePatternMismatchText();
#endif
}

#if !PLATFORM(GTK)
RefPtr<StringImpl> validationMessageTooShortText(int, int minLength)
{
    return WEB_UI_FORMAT_STRING("Use at least %d characters", "Validation message for form control elements with a value shorter than minimum allowed length", minLength);
}

#if !PLATFORM(COCOA)
RefPtr<StringImpl> validationMessageTooLongText(int, int maxLength)
{
    return WEB_UI_FORMAT_STRING("Use no more than %d characters", "Validation message for form control elements with a value shorter than maximum allowed length", maxLength);
}
#endif
#endif

RefPtr<StringImpl> validationMessageRangeUnderflowText(StringImpl* minimum)
{
#if PLATFORM(COCOA)
    return WEB_UI_FORMAT_CFSTRING("Value must be greater than or equal to %@", "Validation message for input form controls with value lower than allowed minimum", createCFString(minimum).get());
#elif USE(GLIB)
    return WEB_UI_FORMAT_STRING("Value must be greater than or equal to %s", "Validation message for input form controls with value lower than allowed minimum", utf8(minimum));
#else
    UNUSED_PARAM(minimum);
    return WEB_UI_STRING("range underflow", "Validation message for input form controls with value lower than allowed minimum");
#endif
}

RefPtr<StringImpl> validationMessageRangeOverflowText(StringImpl* maximum)
{
#if PLATFORM(COCOA)
    return WEB_UI_FORMAT_CFSTRING("Value must be less than or equal to %@", "Validation message for input form controls with value higher than allowed maximum", createCFString(maximum).get());
#elif USE(GLIB)
    return WEB_UI_FORMAT_STRING("Value must be less than or equal to %s", "Validation message for input form controls with value higher than allowed maximum", utf8(maximum));
#else
    UNUSED_PARAM(maximum);
    return WEB_UI_STRING("range overflow", "Validation message for input form controls with value higher than allowed maximum");
#endif
}

RefPtr<StringImpl> validationMessageStepMismatchText(StringImpl*, StringImpl*)
{
    return WEB_UI_STRING("Enter a valid value", "Validation message for input form controls with value not respecting the step attribute");
}

RefPtr<StringImpl> validationMessageBadInputForNumberText()
{
    return WEB_UI_STRING("Enter a number", "Validation message for number fields where the user entered a non-number string");
}

RefPtr<StringImpl> clickToExitFullScreenText()
{
    return WEB_UI_STRING("Click to Exit Full Screen", "Message to display in browser window when in webkit full screen mode.");
}

#if ENABLE(VIDEO)

RefPtr<StringImpl> trackNoLabelText()
{
    return WEB_UI_STRING_KEY("Unknown", "Unknown (audio/text track)", "Menu item label for a audio/text track that has no other name.");
}

RefPtr<StringImpl> textTrackOffMenuItemText()
{
    return WEB_UI_STRING_KEY("Off", "Off (text track)", "Menu item label for the track that represents disabling closed captions.");
}

RefPtr<StringImpl> textTrackOnMenuItemText()
{
    return WEB_UI_STRING_KEY("On", "On (text track)", "Menu item label for the track that represents enabling closed captions.");
}

RefPtr<StringImpl> textTrackAutomaticMenuItemText()
{
    return WEB_UI_STRING_KEY("Auto (Recommended)", "Auto (Recommended) (text track)", "Menu item label for automatic track selection behavior.");
}

RefPtr<StringImpl> captionStylePreviewWithProfileName(StringImpl* profileName)
{
    return WEB_UI_FORMAT_STRING("This is the %s subtitle style", "This is the %s subtitle style (Caption User Preferences)", utf8(profileName));
}

RefPtr<StringImpl> captionStylePreview()
{
    return WEB_UI_STRING_KEY("This is a preview style", "This is a preview style (Caption User Preferences)", "Caption Style Preview String");
}

#if PLATFORM(COCOA)

RefPtr<StringImpl> addTrackLabelAsSuffix(StringImpl* text, StringImpl* label)
{
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ (%@)", "%@ (%@) (audio/text track)", "Audio/Text track display name format that includes the label and language of the track, in the form of 'Language (Label)'.", createCFString(text).get(), createCFString(label).get());
}

RefPtr<StringImpl> textTrackKindClosedCaptionsDisplayName()
{
    return WEB_UI_CFSTRING_KEY("CC", "CC (text track)", "Display name for closed captions text tracks.");
}

RefPtr<StringImpl> addTextTrackKindClosedCaptionsSuffix(StringImpl* text)
{
    if (isEmpty(text))
        return textTrackKindClosedCaptionsDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ CC", "%@ CC (text track)", "Closed captions text track display name format that includes the language and/or locale (e.g. 'English CC').", createCFString(text).get());
}

RefPtr<StringImpl> textTrackKindCaptionsDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Captions", "Captions (text track)", "Display name for text track kind 'captions'.");
}

RefPtr<StringImpl> addTextTrackKindCaptionsSuffix(StringImpl* text)
{
    if (isEmpty(text))
        return textTrackKindCaptionsDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Captions", "%@ Captions (text track)", "Captions text track display name format that includes the language and/or locale (e.g. 'English Captions').", createCFString(text).get());
}

RefPtr<StringImpl> textTrackKindDescriptionsDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Descriptions", "Descriptions (text track)", "Display name for text track kind 'descriptions'.");
}

RefPtr<StringImpl> addTextTrackKindDescriptionsSuffix(StringImpl* text)
{
    if (isEmpty(text))
        return textTrackKindDescriptionsDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Descriptions", "%@ Descriptions (text track)", "Descriptions text track display name format that includes the language and/or locale (e.g. 'English Descriptions').", createCFString(text).get());
}

RefPtr<StringImpl> textTrackKindChaptersDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Chapters", "Chapters (text track)", "Display name for text track kind 'chapters'.");
}

RefPtr<StringImpl> addTextTrackKindChaptersSuffix(StringImpl* text)
{
    if (isEmpty(text))
        return textTrackKindChaptersDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Chapters", "%@ Chapters (text track)", "Chapters text track display name format that includes the language and/or locale (e.g. 'English Chapters').", createCFString(text).get());
}

RefPtr<StringImpl> textTrackKindMetadataDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Metadata", "Metadata (text track)", "Display name for text track kind 'metadata'.");
}

RefPtr<StringImpl> addTextTrackKindMetadataSuffix(StringImpl* text)
{
    if (isEmpty(text))
        return textTrackKindMetadataDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Metadata", "%@ Metadata (text track)", "Metadata text track display name format that includes the language and/or locale (e.g. 'English Metadata').", createCFString(text).get());
}

RefPtr<StringImpl> textTrackKindSDHDisplayName()
{
    return WEB_UI_CFSTRING_KEY("SDH", "SDH (text track)", "Display name for SDH (i.e. deaf and/or hard of hearing) text tracks.");
}

RefPtr<StringImpl> addTextTrackKindSDHSuffix(StringImpl* text)
{
    if (isEmpty(text))
        return textTrackKindSDHDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ SDH", "%@ SDH (text track)", "SDH (i.e. deaf and/or hard of hearing) text track display name format that includes the language and/or locale (e.g. 'English SDH').", createCFString(text).get());
}

RefPtr<StringImpl> textTrackKindEasyReaderDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Easy Reader", "Easy Reader (text track)", "Display name for easy reader (i.e. 3rd-grade level) text tracks.");
}

RefPtr<StringImpl> addTextTrackKindEasyReaderSuffix(StringImpl* text)
{
    if (isEmpty(text))
        return textTrackKindEasyReaderDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Easy Reader", "%@ Easy Reader (text track)", "Easy Reader (i.e. 3rd-grade level) text track display name format that includes the language and/or locale (e.g. 'English Easy Reader').", createCFString(text).get());
}

RefPtr<StringImpl> textTrackKindForcedDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Forced", "Forced (text track)", "Display name for text track kind 'forced'.");
}

RefPtr<StringImpl> addTextTrackKindForcedSuffix(StringImpl* text)
{
    if (isEmpty(text))
        return textTrackKindForcedDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Forced", "%@ Forced (text track)", "Forced text track display name format that includes the language and/or locale (e.g. 'English Forced').", createCFString(text).get());
}

RefPtr<StringImpl> textTrackKindGeneratedDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Generated", "Generated (text track)", "Display name for text track kind 'generated'.");
}

RefPtr<StringImpl> addTextTrackKindGeneratedSuffix(StringImpl* text)
{
    if (isEmpty(text))
        return textTrackKindGeneratedDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Generated", "%@ Generated (text track)", "Generated text track display name format that includes the language and/or locale (e.g. 'English Generated').", createCFString(text).get());
}

RefPtr<StringImpl> audioTrackKindDescriptionsDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Descriptions", "Descriptions (audio track)", "Display name for audio track kind 'descriptions'.");
}

RefPtr<StringImpl> addAudioTrackKindDescriptionsSuffix(StringImpl* text)
{
    if (isEmpty(text))
        return audioTrackKindDescriptionsDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Descriptions", "%@ Descriptions (audio track)", "Descriptions audio track display name format that includes the language and/or locale (e.g. 'English Descriptions').", createCFString(text).get());
}

RefPtr<StringImpl> audioTrackKindCommentaryDisplayName()
{
    return WEB_UI_CFSTRING_KEY("Commentary", "Commentary (audio track)", "Display name for audio track kind 'commentary'.");
}

RefPtr<StringImpl> addAudioTrackKindCommentarySuffix(StringImpl* text)
{
    if (isEmpty(text))
        return audioTrackKindCommentaryDisplayName();
    return WEB_UI_FORMAT_CFSTRING_KEY("%@ Commentary", "%@ Commentary (audio track)", "Commentary audio track display name format that includes the language and/or locale (e.g. 'English Commentary').", createCFString(text).get());
}

#endif // PLATFORM(COCOA)

RefPtr<StringImpl> contextMenuItemTagShowMediaStats()
{
    return WEB_UI_STRING("Show Media Statistics", "Media statistics context menu item");
}

#endif // ENABLE(VIDEO)

#if PLATFORM(COCOA)

RefPtr<StringImpl> webCryptoMasterKeyKeychainLabel(StringImpl* localizedApplicationName)
{
    return WEB_UI_FORMAT_CFSTRING("%@ WebCrypto Master Key", "Name of application's single WebCrypto master key in Keychain", createCFString(localizedApplicationName).get());
}

RefPtr<StringImpl> webCryptoMasterKeyKeychainComment()
{
    return WEB_UI_STRING("Used to encrypt WebCrypto keys in persistent storage, such as IndexedDB", "Description of WebCrypto master keys in Keychain");
}

#endif

#if PLATFORM(WATCHOS)

RefPtr<StringImpl> numberPadOKButtonTitle()
{
    return WEB_UI_STRING_KEY("OK", "OK (OK button title in extra zoomed number pad)", "Title of the OK button for the number pad in zoomed form controls.");
}

RefPtr<StringImpl> formControlCancelButtonTitle()
{
    return WEB_UI_STRING("Cancel", "Cancel");
}

RefPtr<StringImpl> formControlHideButtonTitle()
{
    return WEB_UI_STRING("Hide", "Title of the Hide button for zoomed form controls.");
}

RefPtr<StringImpl> formControlGoButtonTitle()
{
    return WEB_UI_STRING("Go", "Title of the Go button for zoomed form controls.");
}

RefPtr<StringImpl> formControlSearchButtonTitle()
{
    return WEB_UI_STRING("Search", "Title of the Search button for zoomed form controls.");
}

RefPtr<StringImpl> datePickerSetButtonTitle()
{
    return WEB_UI_STRING_KEY("Set", "Set (Button below date picker for extra zoom mode)", "Set button below date picker");
}

RefPtr<StringImpl> datePickerDayLabelTitle()
{
    return WEB_UI_STRING_KEY("DAY", "DAY (Date picker for extra zoom mode)", "Day label in date picker");
}

RefPtr<StringImpl> datePickerMonthLabelTitle()
{
    return WEB_UI_STRING_KEY("MONTH", "MONTH (Date picker for extra zoom mode)", "Month label in date picker");
}

RefPtr<StringImpl> datePickerYearLabelTitle()
{
    return WEB_UI_STRING_KEY("YEAR", "YEAR (Date picker for extra zoom mode)", "Year label in date picker");
}

#endif

#if ENABLE(INPUT_TYPE_WEEK_PICKER)

RefPtr<StringImpl> inputWeekLabel(const DateComponents& dateComponents)
{
    int week = dateComponents.week();
    int year = dateComponents.fullYear();
    return WEB_UI_FORMAT_STRING("Week %1$d, %2$d", "Week of year input type label with week number and year", week, year);
}

#endif

#if USE(SOUP)
RefPtr<StringImpl> unacceptableTLSCertificate()
{
    return WEB_UI_STRING("Unacceptable TLS certificate", "Unacceptable TLS certificate error");
}
#endif

#if ENABLE(WEB_AUTHN)
// On macOS, Touch ID prompt is not guaranteed to show on top of the UI client, and therefore additional
// information is provided to help users to make decisions.
RefPtr<StringImpl> makeCredentialTouchIDPromptTitle(StringImpl* bundleName, StringImpl* domain)
{
    return WEB_UI_FORMAT_CFSTRING("“%@” would like to use Touch ID for “%@”.", "Allow the specified bundle to use Touch ID to sign in to the specified website on this device", createCFString(bundleName).get(), createCFString(domain).get());
}

RefPtr<StringImpl> getAssertionTouchIDPromptTitle(StringImpl* bundleName, StringImpl* domain)
{
    return WEB_UI_FORMAT_CFSTRING("“%@” would like to sign in to “%@”.", "Allow the specified bundle to sign in to the specified website", createCFString(bundleName).get(), createCFString(domain).get());
}

RefPtr<StringImpl> genericTouchIDPromptTitle()
{
    return WEB_UI_STRING("Continue with Touch ID.", "Continue with Touch ID.");
}
#endif // ENABLE(WEB_AUTHN)

RefPtr<StringImpl> pdfPasswordFormTitle()
{
    return WEB_UI_STRING("This document is password protected.", "Title when a PDF needs a password to be unlocked");
}

RefPtr<StringImpl> pdfPasswordFormSubtitle()
{
    return WEB_UI_STRING("Please enter the password below.", "Subtitle when a PDF needs a password to be unlocked");
}

RefPtr<StringImpl> pdfPasswordFormInvalidPasswordSubtitle()
{
    return WEB_UI_STRING("Invalid Password", "Message when a PDF fails to unlock with the given password");
}

RefPtr<StringImpl> contextMenuItemTagCopyLinkWithHighlight()
{
    return WEB_UI_STRING("Copy Link with Highlight", "Copy link with highlight context menu item");
}

#if ENABLE(MEDIA_SESSION) && ENABLE(MEDIA_SESSION_CALL_TO_ACTION)
RefPtr<StringImpl> callToActionLabelTitle(CallToActionLabel label)
{
    switch (label) {
    case CallToActionLabel::VisitWebsite:
        return WEB_UI_STRING("Visit Website", "Title for the call to action button in Picture in Picture that visits a website");
    case CallToActionLabel::Download:
        return WEB_UI_STRING("Download", "Title for the call to action button in Picture in Picture that starts a download");
    case CallToActionLabel::VisitAppStore:
        return WEB_UI_STRING("Visit App Store", "Title for the call to action button in Picture in Picture that visits the App Store");
    }
    ASSERT_NOT_REACHED();
    return { };
}
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
RefPtr<StringImpl> fullscreenControllerViewSpatial()
{
    return WEB_UI_STRING("View Spatial", "Title for View Spatial action button while in fullscreen");
}

RefPtr<StringImpl> fullscreenControllerViewImmersive()
{
    return WEB_UI_STRING("View Immersive", "Title for View Immersive action button while in fullscreen");
}
#endif

#if ENABLE(SPATIAL_IMAGE_CONTROLS)
RefPtr<StringImpl> imageControlsLabelSpatial()
{
    return WEB_UI_STRING("SPATIAL", "Label for Spatial Photos with image controls next to spatial glyph");
}

RefPtr<StringImpl> imageControlsLabelPanorama()
{
    return WEB_UI_STRING("PANORAMA", "Label for panorama photos with image controls next to pano glyph");
}
#endif

} // namespace WebCore
