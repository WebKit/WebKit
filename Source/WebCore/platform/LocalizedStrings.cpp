/*
 * Copyright (C) 2003, 2006, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "PlatformString.h"
#include <wtf/MathExtras.h>
#if USE(CF)
#include <wtf/RetainPtr.h>
#endif
#include <wtf/UnusedParam.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(MAC)
#include "WebCoreSystemInterface.h"
#endif

namespace WebCore {

// We can't use String::format for two reasons:
//  1) It doesn't handle non-ASCII characters in the format string.
//  2) It doesn't handle the %2$d syntax.
// Note that because |format| is used as the second parameter to va_start, it cannot be a reference
// type according to section 18.7/3 of the C++ N1905 standard.
static String formatLocalizedString(String format, ...)
{
#if USE(CF)
    va_list arguments;
    va_start(arguments, format);
    RetainPtr<CFStringRef> formatCFString(AdoptCF, format.createCFString());

#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
    RetainPtr<CFStringRef> result(AdoptCF, CFStringCreateWithFormatAndArguments(0, 0, formatCFString.get(), arguments));
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

    va_end(arguments);
    return result.get();
#else
    notImplemented();
    return format;
#endif
}

#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
static String truncatedStringForLookupMenuItem(const String& original)
{
    if (original.isEmpty())
        return original;

    // Truncate the string if it's too long. This is in consistency with AppKit.
    unsigned maxNumberOfGraphemeClustersInLookupMenuItem = 24;
    DEFINE_STATIC_LOCAL(String, ellipsis, (&horizontalEllipsis, 1));

    String trimmed = original.stripWhiteSpace();
    unsigned numberOfCharacters = numCharactersInGraphemeClusters(trimmed, maxNumberOfGraphemeClustersInLookupMenuItem);
    return numberOfCharacters == trimmed.length() ? trimmed : trimmed.left(numberOfCharacters) + ellipsis;
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

#if PLATFORM(MAC)
String copyImageUnknownFileLabel()
{
    return WEB_UI_STRING("unknown", "Unknown filename");
}
#endif

#if ENABLE(CONTEXT_MENUS)
String contextMenuItemTagOpenLinkInNewWindow()
{
    return WEB_UI_STRING("Open Link in New Window", "Open in New Window context menu item");
}

String contextMenuItemTagDownloadLinkToDisk()
{
    return WEB_UI_STRING("Download Linked File", "Download Linked File context menu item");
}

String contextMenuItemTagCopyLinkToClipboard()
{
    return WEB_UI_STRING("Copy Link", "Copy Link context menu item");
}

String contextMenuItemTagOpenImageInNewWindow()
{
    return WEB_UI_STRING("Open Image in New Window", "Open Image in New Window context menu item");
}

String contextMenuItemTagDownloadImageToDisk()
{
    return WEB_UI_STRING("Download Image", "Download Image context menu item");
}

String contextMenuItemTagCopyImageToClipboard()
{
    return WEB_UI_STRING("Copy Image", "Copy Image context menu item");
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    return WEB_UI_STRING("Open Frame in New Window", "Open Frame in New Window context menu item");
}

String contextMenuItemTagCopy()
{
    return WEB_UI_STRING("Copy", "Copy context menu item");
}

String contextMenuItemTagGoBack()
{
    return WEB_UI_STRING("Back", "Back context menu item");
}

String contextMenuItemTagGoForward()
{
    return WEB_UI_STRING("Forward", "Forward context menu item");
}

String contextMenuItemTagStop()
{
    return WEB_UI_STRING("Stop", "Stop context menu item");
}

String contextMenuItemTagReload()
{
    return WEB_UI_STRING("Reload", "Reload context menu item");
}

String contextMenuItemTagCut()
{
    return WEB_UI_STRING("Cut", "Cut context menu item");
}

String contextMenuItemTagPaste()
{
    return WEB_UI_STRING("Paste", "Paste context menu item");
}

#if PLATFORM(QT)
String contextMenuItemTagSelectAll()
{
    notImplemented();
    return "Select All";
}
#endif

String contextMenuItemTagNoGuessesFound()
{
    return WEB_UI_STRING("No Guesses Found", "No Guesses Found context menu item");
}

String contextMenuItemTagIgnoreSpelling()
{
    return WEB_UI_STRING("Ignore Spelling", "Ignore Spelling context menu item");
}

String contextMenuItemTagLearnSpelling()
{
    return WEB_UI_STRING("Learn Spelling", "Learn Spelling context menu item");
}

#if PLATFORM(MAC)
String contextMenuItemTagSearchInSpotlight()
{
    return WEB_UI_STRING("Search in Spotlight", "Search in Spotlight context menu item");
}
#endif

String contextMenuItemTagSearchWeb()
{
#if PLATFORM(MAC) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    RetainPtr<CFStringRef> searchProviderName(AdoptCF, wkCopyDefaultSearchProviderDisplayName());
    return formatLocalizedString(WEB_UI_STRING("Search with %@", "Search with search provider context menu item with provider name inserted"), searchProviderName.get());
#else
    return WEB_UI_STRING("Search with Google", "Search with Google context menu item");
#endif
}

String contextMenuItemTagLookUpInDictionary(const String& selectedString)
{
#if defined(BUILDING_ON_LEOPARD) || defined(BUILDING_ON_SNOW_LEOPARD)
    UNUSED_PARAM(selectedString);
    return WEB_UI_STRING("Look Up in Dictionary", "Look Up in Dictionary context menu item");
#else
#if USE(CF)
    RetainPtr<CFStringRef> selectedCFString(AdoptCF, truncatedStringForLookupMenuItem(selectedString).createCFString());
    return formatLocalizedString(WEB_UI_STRING("Look Up “%@”", "Look Up context menu item with selected word"), selectedCFString.get());
#else
    return WEB_UI_STRING("Look Up “<selection>”", "Look Up context menu item with selected word").replace("<selection>", truncatedStringForLookupMenuItem(selectedString));
#endif
#endif
}

String contextMenuItemTagOpenLink()
{
    return WEB_UI_STRING("Open Link", "Open Link context menu item");
}

String contextMenuItemTagIgnoreGrammar()
{
    return WEB_UI_STRING("Ignore Grammar", "Ignore Grammar context menu item");
}

String contextMenuItemTagSpellingMenu()
{
    return WEB_UI_STRING("Spelling and Grammar", "Spelling and Grammar context sub-menu item");
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    if (show)
        return WEB_UI_STRING("Show Spelling and Grammar", "menu item title");
    return WEB_UI_STRING("Hide Spelling and Grammar", "menu item title");
}

String contextMenuItemTagCheckSpelling()
{
    return WEB_UI_STRING("Check Document Now", "Check spelling context menu item");
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    return WEB_UI_STRING("Check Spelling While Typing", "Check spelling while typing context menu item");
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    return WEB_UI_STRING("Check Grammar With Spelling", "Check grammar with spelling context menu item");
}

String contextMenuItemTagFontMenu()
{
    return WEB_UI_STRING("Font", "Font context sub-menu item");
}

#if PLATFORM(MAC)
String contextMenuItemTagShowFonts()
{
    return WEB_UI_STRING("Show Fonts", "Show fonts context menu item");
}
#endif

String contextMenuItemTagBold()
{
    return WEB_UI_STRING("Bold", "Bold context menu item");
}

String contextMenuItemTagItalic()
{
    return WEB_UI_STRING("Italic", "Italic context menu item");
}

String contextMenuItemTagUnderline()
{
    return WEB_UI_STRING("Underline", "Underline context menu item");
}

String contextMenuItemTagOutline()
{
    return WEB_UI_STRING("Outline", "Outline context menu item");
}

#if PLATFORM(MAC)
String contextMenuItemTagStyles()
{
    return WEB_UI_STRING("Styles...", "Styles context menu item");
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
#endif

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

#if PLATFORM(MAC)

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

#endif // PLATFORM(MAC)

String contextMenuItemTagOpenVideoInNewWindow()
{
    return WEB_UI_STRING("Open Video in New Window", "Open Video in New Window context menu item");
}

String contextMenuItemTagOpenAudioInNewWindow()
{
    return WEB_UI_STRING("Open Audio in New Window", "Open Audio in New Window context menu item");
}

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

String contextMenuItemTagToggleMediaLoop()
{
    return WEB_UI_STRING("Loop", "Media Loop context menu item");
}

String contextMenuItemTagEnterVideoFullscreen()
{
    return WEB_UI_STRING("Enter Fullscreen", "Video Enter Fullscreen context menu item");
}

String contextMenuItemTagMediaPlay()
{
    return WEB_UI_STRING("Play", "Media Play context menu item");
}

String contextMenuItemTagMediaPause()
{
    return WEB_UI_STRING("Pause", "Media Pause context menu item");
}

String contextMenuItemTagMediaMute()
{
    return WEB_UI_STRING("Mute", "Media Mute context menu item");
}
    
String contextMenuItemTagInspectElement()
{
    return WEB_UI_STRING("Inspect Element", "Inspect Element context menu item");
}

#endif // ENABLE(CONTEXT_MENUS)

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

String AXDefinitionListTermText()
{
    return WEB_UI_STRING("term", "term word of a definition");
}

String AXDefinitionListDefinitionText()
{
    return WEB_UI_STRING("definition", "definition phrase");
}

String AXFooterRoleDescriptionText()
{
    return WEB_UI_STRING("footer", "accessibility role description for a footer");
}

#if PLATFORM(MAC)
String AXARIAContentGroupText(const String& ariaType)
{
    if (ariaType == "ARIAApplicationAlert")
        return WEB_UI_STRING("alert", "An ARIA accessibility group that acts as an alert.");
    if (ariaType == "ARIAApplicationAlertDialog")
        return WEB_UI_STRING("alert dialog", "An ARIA accessibility group that acts as an alert dialog.");
    if (ariaType == "ARIAApplicationDialog")
        return WEB_UI_STRING("dialog", "An ARIA accessibility group that acts as an dialog.");
    if (ariaType == "ARIAApplicationLog")
        return WEB_UI_STRING("log", "An ARIA accessibility group that acts as a console log.");
    if (ariaType == "ARIAApplicationMarquee")
        return WEB_UI_STRING("marquee", "An ARIA accessibility group that acts as a marquee.");
    if (ariaType == "ARIAApplicationStatus")
        return WEB_UI_STRING("application status", "An ARIA accessibility group that acts as a status update.");
    if (ariaType == "ARIAApplicationTimer")
        return WEB_UI_STRING("timer", "An ARIA accessibility group that acts as an updating timer.");
    if (ariaType == "ARIADocument")
        return WEB_UI_STRING("document", "An ARIA accessibility group that acts as a document.");
    if (ariaType == "ARIADocumentArticle")
        return WEB_UI_STRING("article", "An ARIA accessibility group that acts as an article.");
    if (ariaType == "ARIADocumentNote")
        return WEB_UI_STRING("note", "An ARIA accessibility group that acts as a note in a document.");
    if (ariaType == "ARIADocumentRegion")
        return WEB_UI_STRING("region", "An ARIA accessibility group that acts as a distinct region in a document.");
    if (ariaType == "ARIALandmarkApplication")
        return WEB_UI_STRING("application", "An ARIA accessibility group that acts as an application.");
    if (ariaType == "ARIALandmarkBanner")
        return WEB_UI_STRING("banner", "An ARIA accessibility group that acts as a banner.");
    if (ariaType == "ARIALandmarkComplementary")
        return WEB_UI_STRING("complementary", "An ARIA accessibility group that acts as a region of complementary information.");
    if (ariaType == "ARIALandmarkContentInfo")
        return WEB_UI_STRING("content information", "An ARIA accessibility group that contains content.");
    if (ariaType == "ARIALandmarkMain")
        return WEB_UI_STRING("main", "An ARIA accessibility group that is the main portion of the website.");
    if (ariaType == "ARIALandmarkNavigation")
        return WEB_UI_STRING("navigation", "An ARIA accessibility group that contains the main navigation elements of a website.");
    if (ariaType == "ARIALandmarkSearch")
        return WEB_UI_STRING("search", "An ARIA accessibility group that contains a search feature of a website.");
    if (ariaType == "ARIAUserInterfaceTooltip")
        return WEB_UI_STRING("tooltip", "An ARIA accessibility group that acts as a tooltip.");
    if (ariaType == "ARIATabPanel")
        return WEB_UI_STRING("tab panel", "An ARIA accessibility group that contains the content of a tab.");
    if (ariaType == "ARIADocumentMath")
        return WEB_UI_STRING("math", "An ARIA accessibility group that contains mathematical symbols.");
    return String();
}
#endif
    
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
    return "select";
}

String AXMenuListActionVerb()
{
    notImplemented();
    return "select";
}

String missingPluginText()
{
    return WEB_UI_STRING("Missing Plug-in", "Label text to be used when a plugin is missing");
}

String crashedPluginText()
{
    return WEB_UI_STRING("Plug-in Failure", "Label text to be used if plugin host process has crashed");
}

String insecurePluginVersionText()
{
    return WEB_UI_STRING("Blocked Plug-in", "Label text to be used when an insecure plug-in version was blocked from loading");
}

String inactivePluginText()
{
    return WEB_UI_STRING("Inactive Plug-in", "Label text to be used when a plugin has not been loaded for some time");
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    return formatLocalizedString(WEB_UI_STRING("%d files", "Label to describe the number of files selected in a file upload control that allows multiple files"), numberOfFiles);
}

String unknownFileSizeText()
{
    return WEB_UI_STRING("Unknown", "Unknown filesize FTP directory listing item");
}

#if PLATFORM(WIN)
String uploadFileText()
{
    notImplemented();
    return "upload";
}

String allFilesText()
{
    notImplemented();
    return "all files";
}
#endif

#if PLATFORM(MAC)
String builtInPDFPluginName()
{
    // Also exposed to DOM.
    return WEB_UI_STRING("WebKit built-in PDF", "Pseudo plug-in name, visible in Installed Plug-ins page in Safari.");
}

String pdfDocumentTypeDescription()
{
    // Also exposed to DOM.
    return WEB_UI_STRING("Portable Document Format", "Description of the (only) type supported by PDF pseudo plug-in. Visible in Installed Plug-ins page in Safari.");
}

String keygenMenuItem512()
{
    return WEB_UI_STRING("512 (Low Grade)", "Menu item title for KEYGEN pop-up menu");
}

String keygenMenuItem1024()
{
    return WEB_UI_STRING("1024 (Medium Grade)", "Menu item title for KEYGEN pop-up menu");
}

String keygenMenuItem2048()
{
    return WEB_UI_STRING("2048 (High Grade)", "Menu item title for KEYGEN pop-up menu");
}

String keygenKeychainItemName(const String& host)
{
    RetainPtr<CFStringRef> hostCFString(AdoptCF, host.createCFString());
    return formatLocalizedString(WEB_UI_STRING("Key from %@", "Name of keychain key generated by the KEYGEN tag"), hostCFString.get());
}

#endif

#if PLATFORM(IOS)
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
#endif

String imageTitle(const String& filename, const IntSize& size)
{
#if USE(CF)
#if !defined(BUILDING_ON_LEOPARD)
    RetainPtr<CFStringRef> filenameCFString(AdoptCF, filename.createCFString());
    RetainPtr<CFLocaleRef> locale(AdoptCF, CFLocaleCopyCurrent());
    RetainPtr<CFNumberFormatterRef> formatter(AdoptCF, CFNumberFormatterCreate(0, locale.get(), kCFNumberFormatterDecimalStyle));

    int widthInt = size.width();
    RetainPtr<CFNumberRef> width(AdoptCF, CFNumberCreate(0, kCFNumberIntType, &widthInt));
    RetainPtr<CFStringRef> widthString(AdoptCF, CFNumberFormatterCreateStringWithNumber(0, formatter.get(), width.get()));

    int heightInt = size.height();
    RetainPtr<CFNumberRef> height(AdoptCF, CFNumberCreate(0, kCFNumberIntType, &heightInt));
    RetainPtr<CFStringRef> heightString(AdoptCF, CFNumberFormatterCreateStringWithNumber(0, formatter.get(), height.get()));

    return formatLocalizedString(WEB_UI_STRING("%@ %@×%@ pixels", "window title for a standalone image (uses multiplication symbol, not x)"), filenameCFString.get(), widthString.get(), heightString.get());
#else
    RetainPtr<CFStringRef> filenameCFString(AdoptCF, filename.createCFString());
    return formatLocalizedString(WEB_UI_STRING("%@ %d×%d pixels", "window title for a standalone image (uses multiplication symbol, not x)"), filenameCFString.get(), size.width(), size.height());
#endif
#else
    return formatLocalizedString(WEB_UI_STRING("<filename> %d×%d pixels", "window title for a standalone image (uses multiplication symbol, not x)"), size.width(), size.height()).replace("<filename>", filename);
#endif
}

String mediaElementLoadingStateText()
{
    return WEB_UI_STRING("Loading...", "Media controller status message when the media is loading");
}

String mediaElementLiveBroadcastStateText()
{
    return WEB_UI_STRING("Live Broadcast", "Media controller status message when watching a live broadcast");
}

String localizedMediaControlElementString(const String& name)
{
    if (name == "AudioElement")
        return WEB_UI_STRING("audio element controller", "accessibility role description for audio element controller");
    if (name == "VideoElement")
        return WEB_UI_STRING("video element controller", "accessibility role description for video element controller");
    if (name == "MuteButton")
        return WEB_UI_STRING("mute", "accessibility role description for mute button");
    if (name == "UnMuteButton")
        return WEB_UI_STRING("unmute", "accessibility role description for turn mute off button");
    if (name == "PlayButton")
        return WEB_UI_STRING("play", "accessibility role description for play button");
    if (name == "PauseButton")
        return WEB_UI_STRING("pause", "accessibility role description for pause button");
    if (name == "Slider")
        return WEB_UI_STRING("movie time", "accessibility role description for timeline slider");
    if (name == "SliderThumb")
        return WEB_UI_STRING("timeline slider thumb", "accessibility role description for timeline thumb");
    if (name == "RewindButton")
        return WEB_UI_STRING("back 30 seconds", "accessibility role description for seek back 30 seconds button");
    if (name == "ReturnToRealtimeButton")
        return WEB_UI_STRING("return to realtime", "accessibility role description for return to real time button");
    if (name == "CurrentTimeDisplay")
        return WEB_UI_STRING("elapsed time", "accessibility role description for elapsed time display");
    if (name == "TimeRemainingDisplay")
        return WEB_UI_STRING("remaining time", "accessibility role description for time remaining display");
    if (name == "StatusDisplay")
        return WEB_UI_STRING("status", "accessibility role description for movie status");
    if (name == "EnterFullscreenButton")
        return WEB_UI_STRING("enter fullscreen", "accessibility role description for enter fullscreen button");
    if (name == "ExitFullscreenButton")
        return WEB_UI_STRING("exit fullscreen", "accessibility role description for exit fullscreen button");
    if (name == "SeekForwardButton")
        return WEB_UI_STRING("fast forward", "accessibility role description for fast forward button");
    if (name == "SeekBackButton")
        return WEB_UI_STRING("fast reverse", "accessibility role description for fast reverse button");
    if (name == "ShowClosedCaptionsButton")
        return WEB_UI_STRING("show closed captions", "accessibility role description for show closed captions button");
    if (name == "HideClosedCaptionsButton")
        return WEB_UI_STRING("hide closed captions", "accessibility role description for hide closed captions button");

    // FIXME: the ControlsPanel container should never be visible in the accessibility hierarchy.
    if (name == "ControlsPanel")
        return String();

    ASSERT_NOT_REACHED();
    return String();
}

String localizedMediaControlElementHelpText(const String& name)
{
    if (name == "AudioElement")
        return WEB_UI_STRING("audio element playback controls and status display", "accessibility role description for audio element controller");
    if (name == "VideoElement")
        return WEB_UI_STRING("video element playback controls and status display", "accessibility role description for video element controller");
    if (name == "MuteButton")
        return WEB_UI_STRING("mute audio tracks", "accessibility help text for mute button");
    if (name == "UnMuteButton")
        return WEB_UI_STRING("unmute audio tracks", "accessibility help text for un mute button");
    if (name == "PlayButton")
        return WEB_UI_STRING("begin playback", "accessibility help text for play button");
    if (name == "PauseButton")
        return WEB_UI_STRING("pause playback", "accessibility help text for pause button");
    if (name == "Slider")
        return WEB_UI_STRING("movie time scrubber", "accessibility help text for timeline slider");
    if (name == "SliderThumb")
        return WEB_UI_STRING("movie time scrubber thumb", "accessibility help text for timeline slider thumb");
    if (name == "RewindButton")
        return WEB_UI_STRING("seek movie back 30 seconds", "accessibility help text for jump back 30 seconds button");
    if (name == "ReturnToRealtimeButton")
        return WEB_UI_STRING("return streaming movie to real time", "accessibility help text for return streaming movie to real time button");
    if (name == "CurrentTimeDisplay")
        return WEB_UI_STRING("current movie time in seconds", "accessibility help text for elapsed time display");
    if (name == "TimeRemainingDisplay")
        return WEB_UI_STRING("number of seconds of movie remaining", "accessibility help text for remaining time display");
    if (name == "StatusDisplay")
        return WEB_UI_STRING("current movie status", "accessibility help text for movie status display");
    if (name == "SeekBackButton")
        return WEB_UI_STRING("seek quickly back", "accessibility help text for fast rewind button");
    if (name == "SeekForwardButton")
        return WEB_UI_STRING("seek quickly forward", "accessibility help text for fast forward button");
    if (name == "FullscreenButton")
        return WEB_UI_STRING("Play movie in fullscreen mode", "accessibility help text for enter fullscreen button");
    if (name == "ShowClosedCaptionsButton")
        return WEB_UI_STRING("start displaying closed captions", "accessibility help text for show closed captions button");
    if (name == "HideClosedCaptionsButton")
        return WEB_UI_STRING("stop displaying closed captions", "accessibility help text for hide closed captions button");

    ASSERT_NOT_REACHED();
    return String();
}

String localizedMediaTimeDescription(float time)
{
    if (!isfinite(time))
        return WEB_UI_STRING("indefinite time", "accessibility help text for an indefinite media controller time value");

    int seconds = static_cast<int>(fabsf(time));
    int days = seconds / (60 * 60 * 24);
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (days)
        return formatLocalizedString(WEB_UI_STRING("%1$d days %2$d hours %3$d minutes %4$d seconds", "accessibility help text for media controller time value >= 1 day"), days, hours, minutes, seconds);
    if (hours)
        return formatLocalizedString(WEB_UI_STRING("%1$d hours %2$d minutes %3$d seconds", "accessibility help text for media controller time value >= 60 minutes"), hours, minutes, seconds);
    if (minutes)
        return formatLocalizedString(WEB_UI_STRING("%1$d minutes %2$d seconds", "accessibility help text for media controller time value >= 60 seconds"), minutes, seconds);
    return formatLocalizedString(WEB_UI_STRING("%1$d seconds", "accessibility help text for media controller time value < 60 seconds"), seconds);
}

String validationMessageValueMissingText()
{
    return WEB_UI_STRING("value missing", "Validation message for required form control elements that have no value");
}

String validationMessageValueMissingForCheckboxText()
{
    return validationMessageValueMissingText();
}

String validationMessageValueMissingForFileText()
{
    return validationMessageValueMissingText();
}

String validationMessageValueMissingForMultipleFileText()
{
    return validationMessageValueMissingText();
}

String validationMessageValueMissingForRadioText()
{
    return validationMessageValueMissingText();
}

String validationMessageValueMissingForSelectText()
{
    return validationMessageValueMissingText();
}

String validationMessageTypeMismatchText()
{
    return WEB_UI_STRING("type mismatch", "Validation message for input form controls with a value not matching type");
}

String validationMessageTypeMismatchForEmailText()
{
    return validationMessageTypeMismatchText();
}

String validationMessageTypeMismatchForMultipleEmailText()
{
    return validationMessageTypeMismatchText();
}

String validationMessageTypeMismatchForURLText()
{
    return validationMessageTypeMismatchText();
}

String validationMessagePatternMismatchText()
{
    return WEB_UI_STRING("pattern mismatch", "Validation message for input form controls requiring a constrained value according to pattern");
}

String validationMessageTooLongText(int, int)
{
    return WEB_UI_STRING("too long", "Validation message for form control elements with a value longer than maximum allowed length");
}

String validationMessageRangeUnderflowText(const String&)
{
    return WEB_UI_STRING("range underflow", "Validation message for input form controls with value lower than allowed minimum");
}

String validationMessageRangeOverflowText(const String&)
{
    return WEB_UI_STRING("range overflow", "Validation message for input form controls with value higher than allowed maximum");
}

String validationMessageStepMismatchText(const String&, const String&)
{
    return WEB_UI_STRING("step mismatch", "Validation message for input form controls with value not respecting the step attribute");
}

} // namespace WebCore
