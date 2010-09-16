/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "WebPlatformStrategies.h"

#import "WebLocalizableStrings.h"
#import "WebPluginDatabase.h"
#import "WebPluginPackage.h"
#import <WebCore/BlockExceptions.h>
#import <WebCore/IntSize.h>
#import <WebCore/Page.h>
#import <WebCore/PageGroup.h>
#import <wtf/StdLibExtras.h>

#ifdef BUILDING_ON_TIGER
typedef unsigned NSUInteger;
#endif

using namespace WebCore;

void WebPlatformStrategies::initialize()
{
    DEFINE_STATIC_LOCAL(WebPlatformStrategies, platformStrategies, ());
    setPlatformStrategies(&platformStrategies);
}

WebPlatformStrategies::WebPlatformStrategies()
{
}

// PluginStrategy

PluginStrategy* WebPlatformStrategies::createPluginStrategy()
{
    return this;
}

LocalizationStrategy* WebPlatformStrategies::createLocalizationStrategy()
{
    return this;
}

VisitedLinkStrategy* WebPlatformStrategies::createVisitedLinkStrategy()
{
    return this;
}

void WebPlatformStrategies::refreshPlugins()
{
    [[WebPluginDatabase sharedDatabase] refresh];
}

void WebPlatformStrategies::getPluginInfo(Vector<WebCore::PluginInfo>& plugins)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSArray* pluginsArray = [[WebPluginDatabase sharedDatabase] plugins];
    for (unsigned int i = 0; i < [pluginsArray count]; ++i) {
        WebPluginPackage *plugin = [pluginsArray objectAtIndex:i];

        plugins.append([plugin pluginInfo]);
    }
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

// LocalizationStrategy    

String WebPlatformStrategies::inputElementAltText()
{
    return UI_STRING_KEY("Submit", "Submit (input element)", "alt text for <input> elements with no alt, title, or value");
}

String WebPlatformStrategies::resetButtonDefaultLabel()
{
    return UI_STRING("Reset", "default label for Reset buttons in forms on web pages");
}

String WebPlatformStrategies::searchableIndexIntroduction()
{
    return UI_STRING("This is a searchable index. Enter search keywords: ",
        "text that appears at the start of nearly-obsolete web pages in the form of a 'searchable index'");
}

String WebPlatformStrategies::submitButtonDefaultLabel()
{
    return UI_STRING("Submit", "default label for Submit buttons in forms on web pages");
}

String WebPlatformStrategies::fileButtonChooseFileLabel()
{
    return UI_STRING("Choose File", "title for file button used in HTML forms");
}

String WebPlatformStrategies::fileButtonNoFileSelectedLabel()
{
    return UI_STRING("no file selected", "text to display in file button used in HTML forms when no file is selected");
}

String WebPlatformStrategies::copyImageUnknownFileLabel()
{
    return UI_STRING("unknown", "Unknown filename");
}

#if ENABLE(CONTEXT_MENUS)

String WebPlatformStrategies::contextMenuItemTagOpenLinkInNewWindow()
{
    return UI_STRING("Open Link in New Window", "Open in New Window context menu item");
}

String WebPlatformStrategies::contextMenuItemTagDownloadLinkToDisk()
{
    return UI_STRING("Download Linked File", "Download Linked File context menu item");
}

String WebPlatformStrategies::contextMenuItemTagCopyLinkToClipboard()
{
    return UI_STRING("Copy Link", "Copy Link context menu item");
}

String WebPlatformStrategies::contextMenuItemTagOpenImageInNewWindow()
{
    return UI_STRING("Open Image in New Window", "Open Image in New Window context menu item");
}

String WebPlatformStrategies::contextMenuItemTagDownloadImageToDisk()
{
    return UI_STRING("Download Image", "Download Image context menu item");
}

String WebPlatformStrategies::contextMenuItemTagCopyImageToClipboard()
{
    return UI_STRING("Copy Image", "Copy Image context menu item");
}

String WebPlatformStrategies::contextMenuItemTagOpenFrameInNewWindow()
{
    return UI_STRING("Open Frame in New Window", "Open Frame in New Window context menu item");
}

String WebPlatformStrategies::contextMenuItemTagCopy()
{
    return UI_STRING("Copy", "Copy context menu item");
}

String WebPlatformStrategies::contextMenuItemTagGoBack()
{
    return UI_STRING("Back", "Back context menu item");
}

String WebPlatformStrategies::contextMenuItemTagGoForward()
{
    return UI_STRING("Forward", "Forward context menu item");
}

String WebPlatformStrategies::contextMenuItemTagStop()
{
    return UI_STRING("Stop", "Stop context menu item");
}

String WebPlatformStrategies::contextMenuItemTagReload()
{
    return UI_STRING("Reload", "Reload context menu item");
}

String WebPlatformStrategies::contextMenuItemTagCut()
{
    return UI_STRING("Cut", "Cut context menu item");
}

String WebPlatformStrategies::contextMenuItemTagPaste()
{
    return UI_STRING("Paste", "Paste context menu item");
}

String WebPlatformStrategies::contextMenuItemTagNoGuessesFound()
{
    return UI_STRING("No Guesses Found", "No Guesses Found context menu item");
}

String WebPlatformStrategies::contextMenuItemTagIgnoreSpelling()
{
    return UI_STRING("Ignore Spelling", "Ignore Spelling context menu item");
}

String WebPlatformStrategies::contextMenuItemTagLearnSpelling()
{
    return UI_STRING("Learn Spelling", "Learn Spelling context menu item");
}

String WebPlatformStrategies::contextMenuItemTagSearchWeb()
{
    return UI_STRING("Search in Google", "Search in Google context menu item");
}

String WebPlatformStrategies::contextMenuItemTagLookUpInDictionary()
{
    return UI_STRING("Look Up in Dictionary", "Look Up in Dictionary context menu item");
}

String WebPlatformStrategies::contextMenuItemTagOpenLink()
{
    return UI_STRING("Open Link", "Open Link context menu item");
}

String WebPlatformStrategies::contextMenuItemTagIgnoreGrammar()
{
    return UI_STRING("Ignore Grammar", "Ignore Grammar context menu item");
}

String WebPlatformStrategies::contextMenuItemTagSpellingMenu()
{
#ifndef BUILDING_ON_TIGER
    return UI_STRING("Spelling and Grammar", "Spelling and Grammar context sub-menu item");
#else
    return UI_STRING("Spelling", "Spelling context sub-menu item");
#endif
}

String WebPlatformStrategies::contextMenuItemTagShowSpellingPanel(bool show)
{
#ifndef BUILDING_ON_TIGER
    if (show)
        return UI_STRING("Show Spelling and Grammar", "menu item title");
    return UI_STRING("Hide Spelling and Grammar", "menu item title");
#else
    return UI_STRING("Spelling...", "menu item title");
#endif
}

String WebPlatformStrategies::contextMenuItemTagCheckSpelling()
{
#ifndef BUILDING_ON_TIGER
    return UI_STRING("Check Document Now", "Check spelling context menu item");
#else
    return UI_STRING("Check Spelling", "Check spelling context menu item");
#endif
}

String WebPlatformStrategies::contextMenuItemTagCheckSpellingWhileTyping()
{
#ifndef BUILDING_ON_TIGER
    return UI_STRING("Check Spelling While Typing", "Check spelling while typing context menu item");
#else
    return UI_STRING("Check Spelling as You Type", "Check spelling while typing context menu item");
#endif
}

String WebPlatformStrategies::contextMenuItemTagCheckGrammarWithSpelling()
{
    return UI_STRING("Check Grammar With Spelling", "Check grammar with spelling context menu item");
}

String WebPlatformStrategies::contextMenuItemTagFontMenu()
{
    return UI_STRING("Font", "Font context sub-menu item");
}

String WebPlatformStrategies::contextMenuItemTagBold()
{
    return UI_STRING("Bold", "Bold context menu item");
}

String WebPlatformStrategies::contextMenuItemTagItalic()
{
    return UI_STRING("Italic", "Italic context menu item");
}

String WebPlatformStrategies::contextMenuItemTagUnderline()
{
    return UI_STRING("Underline", "Underline context menu item");
}

String WebPlatformStrategies::contextMenuItemTagOutline()
{
    return UI_STRING("Outline", "Outline context menu item");
}

String WebPlatformStrategies::contextMenuItemTagWritingDirectionMenu()
{
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    return UI_STRING("Paragraph Direction", "Paragraph direction context sub-menu item");
#else
    return UI_STRING("Writing Direction", "Writing direction context sub-menu item");
#endif
}

String WebPlatformStrategies::contextMenuItemTagTextDirectionMenu()
{
    return UI_STRING("Selection Direction", "Selection direction context sub-menu item");
}

String WebPlatformStrategies::contextMenuItemTagDefaultDirection()
{
    return UI_STRING("Default", "Default writing direction context menu item");
}

String WebPlatformStrategies::contextMenuItemTagLeftToRight()
{
    return UI_STRING("Left to Right", "Left to Right context menu item");
}

String WebPlatformStrategies::contextMenuItemTagRightToLeft()
{
    return UI_STRING("Right to Left", "Right to Left context menu item");
}

#if PLATFORM(MAC)

String WebPlatformStrategies::contextMenuItemTagSearchInSpotlight()
{
    return UI_STRING("Search in Spotlight", "Search in Spotlight context menu item");
}

String WebPlatformStrategies::contextMenuItemTagShowFonts()
{
    return UI_STRING("Show Fonts", "Show fonts context menu item");
}

String WebPlatformStrategies::contextMenuItemTagStyles()
{
    return UI_STRING("Styles...", "Styles context menu item");
}

String WebPlatformStrategies::contextMenuItemTagShowColors()
{
    return UI_STRING("Show Colors", "Show colors context menu item");
}

String WebPlatformStrategies::contextMenuItemTagSpeechMenu()
{
    return UI_STRING("Speech", "Speech context sub-menu item");
}

String WebPlatformStrategies::contextMenuItemTagStartSpeaking()
{
    return UI_STRING("Start Speaking", "Start speaking context menu item");
}

String WebPlatformStrategies::contextMenuItemTagStopSpeaking()
{
    return UI_STRING("Stop Speaking", "Stop speaking context menu item");
}

String WebPlatformStrategies::contextMenuItemTagCorrectSpellingAutomatically()
{
    return UI_STRING("Correct Spelling Automatically", "Correct Spelling Automatically context menu item");
}

String WebPlatformStrategies::contextMenuItemTagSubstitutionsMenu()
{
    return UI_STRING("Substitutions", "Substitutions context sub-menu item");
}

String WebPlatformStrategies::contextMenuItemTagShowSubstitutions(bool show)
{
    if (show) 
        return UI_STRING("Show Substitutions", "menu item title");
    return UI_STRING("Hide Substitutions", "menu item title");
}

String WebPlatformStrategies::contextMenuItemTagSmartCopyPaste()
{
    return UI_STRING("Smart Copy/Paste", "Smart Copy/Paste context menu item");
}

String WebPlatformStrategies::contextMenuItemTagSmartQuotes()
{
    return UI_STRING("Smart Quotes", "Smart Quotes context menu item");
}

String WebPlatformStrategies::contextMenuItemTagSmartDashes()
{
    return UI_STRING("Smart Dashes", "Smart Dashes context menu item");
}

String WebPlatformStrategies::contextMenuItemTagSmartLinks()
{
    return UI_STRING("Smart Links", "Smart Links context menu item");
}

String WebPlatformStrategies::contextMenuItemTagTextReplacement()
{
    return UI_STRING("Text Replacement", "Text Replacement context menu item");
}

String WebPlatformStrategies::contextMenuItemTagTransformationsMenu()
{
    return UI_STRING("Transformations", "Transformations context sub-menu item");
}

String WebPlatformStrategies::contextMenuItemTagMakeUpperCase()
{
    return UI_STRING("Make Upper Case", "Make Upper Case context menu item");
}

String WebPlatformStrategies::contextMenuItemTagMakeLowerCase()
{
    return UI_STRING("Make Lower Case", "Make Lower Case context menu item");
}

String WebPlatformStrategies::contextMenuItemTagCapitalize()
{
    return UI_STRING("Capitalize", "Capitalize context menu item");
}

String WebPlatformStrategies::contextMenuItemTagChangeBack(const String& replacedString)
{
    static NSString *formatString = nil;
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    static bool lookedUpString = false;
    if (!lookedUpString) {
        formatString = [[[NSBundle bundleForClass:[NSSpellChecker class]] localizedStringForKey:@"Change Back to \\U201C%@\\U201D" value:nil table:@"MenuCommands"] retain];
        lookedUpString = true;
    }
#endif
    if (!formatString)
        return replacedString;
    return [NSString stringWithFormat:formatString, (NSString *)replacedString];
}

#endif

String WebPlatformStrategies::contextMenuItemTagInspectElement()
{
    return UI_STRING("Inspect Element", "Inspect Element context menu item");
}

String WebPlatformStrategies::contextMenuItemTagOpenMediaInNewWindow()
{
    return UI_STRING("Open Media in New Window", "Open Media in New Window context menu item");
}

String WebPlatformStrategies::contextMenuItemTagCopyMediaLinkToClipboard()
{
    return UI_STRING("Copy Media Address", "Copy Media Address context menu item");
}

String WebPlatformStrategies::contextMenuItemTagToggleMediaControls()
{
    return UI_STRING("Toggle Media Controls", "Toggle Media Controls context menu item");
}

String WebPlatformStrategies::contextMenuItemTagToggleMediaLoop()
{
    return UI_STRING("Toggle Media Loop Playback", "Toggle Media Loop Playback context menu item");
}

String WebPlatformStrategies::contextMenuItemTagEnterVideoFullscreen()
{
    return UI_STRING("Fullscreen", "Fullscreen context menu item");
}

String WebPlatformStrategies::contextMenuItemTagMediaPlay()
{
    return UI_STRING("Play", "Play context menu item");
}

String WebPlatformStrategies::contextMenuItemTagMediaPause()
{
    return UI_STRING("Pause", "Pause context menu item");
}

String WebPlatformStrategies::contextMenuItemTagMediaMute()
{
    return UI_STRING("Mute", "Mute context menu item");
}

String WebPlatformStrategies::contextMenuItemTagMediaUnMute()
{
    return UI_STRING("Unmute", "Unmute context menu item");
}

#endif // ENABLE(CONTEXT_MENUS)

String WebPlatformStrategies::searchMenuNoRecentSearchesText()
{
    return UI_STRING("No recent searches", "Label for only item in menu that appears when clicking on the search field image, when no searches have been performed");
}

String WebPlatformStrategies::searchMenuRecentSearchesText()
{
    return UI_STRING("Recent Searches", "label for first item in the menu that appears when clicking on the search field image, used as embedded menu title");
}

String WebPlatformStrategies::searchMenuClearRecentSearchesText()
{
    return UI_STRING("Clear Recent Searches", "menu item in Recent Searches menu that empties menu's contents");
}

String WebPlatformStrategies::AXWebAreaText()
{
    return UI_STRING("HTML content", "accessibility role description for web area");
}

String WebPlatformStrategies::AXLinkText()
{
    return UI_STRING("link", "accessibility role description for link");
}

String WebPlatformStrategies::AXListMarkerText()
{
    return UI_STRING("list marker", "accessibility role description for list marker");
}

String WebPlatformStrategies::AXImageMapText()
{
    return UI_STRING("image map", "accessibility role description for image map");
}

String WebPlatformStrategies::AXHeadingText()
{
    return UI_STRING("heading", "accessibility role description for headings");
}

String WebPlatformStrategies::AXDefinitionListTermText()
{
    return UI_STRING("term", "term word of a definition");
}

String WebPlatformStrategies::AXDefinitionListDefinitionText()
{
    return UI_STRING("definition", "definition phrase");
}

String WebPlatformStrategies::AXARIAContentGroupText(const String& ariaType)
{
    if (ariaType == "ARIAApplicationAlert")
        return UI_STRING("alert", "An ARIA accessibility group that acts as an alert.");
    if (ariaType == "ARIAApplicationAlertDialog")
        return UI_STRING("alert dialog", "An ARIA accessibility group that acts as an alert dialog.");
    if (ariaType == "ARIAApplicationDialog")
        return UI_STRING("dialog", "An ARIA accessibility group that acts as an dialog.");
    if (ariaType == "ARIAApplicationLog")
        return UI_STRING("log", "An ARIA accessibility group that acts as a console log.");
    if (ariaType == "ARIAApplicationMarquee")
        return UI_STRING("marquee", "An ARIA accessibility group that acts as a marquee.");    
    if (ariaType == "ARIAApplicationStatus")
        return UI_STRING("application status", "An ARIA accessibility group that acts as a status update.");    
    if (ariaType == "ARIAApplicationTimer")
        return UI_STRING("timer", "An ARIA accessibility group that acts as an updating timer.");    
    if (ariaType == "ARIADocument")
        return UI_STRING("document", "An ARIA accessibility group that acts as a document.");    
    if (ariaType == "ARIADocumentArticle")
        return UI_STRING("article", "An ARIA accessibility group that acts as an article.");    
    if (ariaType == "ARIADocumentNote")
        return UI_STRING("note", "An ARIA accessibility group that acts as a note in a document.");    
    if (ariaType == "ARIADocumentRegion")
        return UI_STRING("region", "An ARIA accessibility group that acts as a distinct region in a document.");    
    if (ariaType == "ARIALandmarkApplication")
        return UI_STRING("application", "An ARIA accessibility group that acts as an application.");    
    if (ariaType == "ARIALandmarkBanner")
        return UI_STRING("banner", "An ARIA accessibility group that acts as a banner.");    
    if (ariaType == "ARIALandmarkComplementary")
        return UI_STRING("complementary", "An ARIA accessibility group that acts as a region of complementary information.");    
    if (ariaType == "ARIALandmarkContentInfo")
        return UI_STRING("content", "An ARIA accessibility group that contains content.");    
    if (ariaType == "ARIALandmarkMain")
        return UI_STRING("main", "An ARIA accessibility group that is the main portion of the website.");    
    if (ariaType == "ARIALandmarkNavigation")
        return UI_STRING("navigation", "An ARIA accessibility group that contains the main navigation elements of a website.");    
    if (ariaType == "ARIALandmarkSearch")
        return UI_STRING("search", "An ARIA accessibility group that contains a search feature of a website.");    
    if (ariaType == "ARIAUserInterfaceTooltip")
        return UI_STRING("tooltip", "An ARIA accessibility group that acts as a tooltip.");    
    if (ariaType == "ARIATabPanel")
        return UI_STRING("tab panel", "An ARIA accessibility group that contains the content of a tab.");
    if (ariaType == "ARIADocumentMath")
        return UI_STRING("math", "An ARIA accessibility group that contains mathematical symbols.");
    return String();
}

String WebPlatformStrategies::AXButtonActionVerb()
{
    return UI_STRING("press", "Verb stating the action that will occur when a button is pressed, as used by accessibility");
}

String WebPlatformStrategies::AXRadioButtonActionVerb() 
{
    return UI_STRING("select", "Verb stating the action that will occur when a radio button is clicked, as used by accessibility");
}

String WebPlatformStrategies::AXTextFieldActionVerb()
{
    return UI_STRING("activate", "Verb stating the action that will occur when a text field is selected, as used by accessibility");
}

String WebPlatformStrategies::AXCheckedCheckBoxActionVerb()
{
    return UI_STRING("uncheck", "Verb stating the action that will occur when a checked checkbox is clicked, as used by accessibility");
}

String WebPlatformStrategies::AXUncheckedCheckBoxActionVerb()
{
    return UI_STRING("check", "Verb stating the action that will occur when an unchecked checkbox is clicked, as used by accessibility");
}

String WebPlatformStrategies::AXMenuListActionVerb()
{
    return String();
}

String WebPlatformStrategies::AXMenuListPopupActionVerb()
{
    return String();
}

String WebPlatformStrategies::AXLinkActionVerb()
{
    return UI_STRING("jump", "Verb stating the action that will occur when a link is clicked, as used by accessibility");
}

String WebPlatformStrategies::missingPluginText()
{
    return UI_STRING("Missing Plug-in", "Label text to be used when a plugin is missing");
}

String WebPlatformStrategies::crashedPluginText()
{
    return UI_STRING("Plug-in Failure", "Label text to be used if plugin host process has crashed");
}

String WebPlatformStrategies::multipleFileUploadText(unsigned numberOfFiles)
{
    return [NSString stringWithFormat:UI_STRING("%d files", "Label to describe the number of files selected in a file upload control that allows multiple files"), numberOfFiles];
}

String WebPlatformStrategies::unknownFileSizeText()
{
    return UI_STRING("Unknown", "Unknown filesize FTP directory listing item");
}

String WebPlatformStrategies::imageTitle(const String& filename, const IntSize& size)
{
    return [NSString stringWithFormat:UI_STRING("%@ %d×%d pixels", "window title for a standalone image (uses multiplication symbol, not x)"), (NSString *)filename, size.width(), size.height()];
}

String WebPlatformStrategies::mediaElementLoadingStateText()
{
    return UI_STRING("Loading...", "Media controller status message when the media is loading");
}

String WebPlatformStrategies::mediaElementLiveBroadcastStateText()
{
    return UI_STRING("Live Broadcast", "Media controller status message when watching a live broadcast");
}

String WebPlatformStrategies::localizedMediaControlElementString(const String& name)
{
    if (name == "AudioElement")
        return UI_STRING("audio element controller", "accessibility role description for audio element controller");
    if (name == "VideoElement")
        return UI_STRING("video element controller", "accessibility role description for video element controller");
    if (name == "MuteButton")
        return UI_STRING("mute", "accessibility role description for mute button");
    if (name == "UnMuteButton")
        return UI_STRING("unmute", "accessibility role description for turn mute off button");
    if (name == "PlayButton")
        return UI_STRING("play", "accessibility role description for play button");
    if (name == "PauseButton")
        return UI_STRING("pause", "accessibility role description for pause button");
    if (name == "Slider")
        return UI_STRING("movie time", "accessibility role description for timeline slider");
    if (name == "SliderThumb")
        return UI_STRING("timeline slider thumb", "accessibility role description for timeline thumb");
    if (name == "RewindButton")
        return UI_STRING("back 30 seconds", "accessibility role description for seek back 30 seconds button");
    if (name == "ReturnToRealtimeButton")
        return UI_STRING("return to realtime", "accessibility role description for return to real time button");
    if (name == "CurrentTimeDisplay")
        return UI_STRING("elapsed time", "accessibility role description for elapsed time display");
    if (name == "TimeRemainingDisplay")
        return UI_STRING("remaining time", "accessibility role description for time remaining display");
    if (name == "StatusDisplay")
        return UI_STRING("status", "accessibility role description for movie status");
    if (name == "FullscreenButton")
        return UI_STRING("fullscreen", "accessibility role description for enter fullscreen button");
    if (name == "SeekForwardButton")
        return UI_STRING("fast forward", "accessibility role description for fast forward button");
    if (name == "SeekBackButton")
        return UI_STRING("fast reverse", "accessibility role description for fast reverse button");
    if (name == "ShowClosedCaptionsButton")
        return UI_STRING("show closed captions", "accessibility role description for show closed captions button");
    if (name == "HideClosedCaptionsButton")
        return UI_STRING("hide closed captions", "accessibility role description for hide closed captions button");

    // FIXME: the ControlsPanel container should never be visible in the accessibility hierarchy.
    if (name == "ControlsPanel")
        return String();

    ASSERT_NOT_REACHED();
    return String();
}

String WebPlatformStrategies::localizedMediaControlElementHelpText(const String& name)
{
    if (name == "AudioElement")
        return UI_STRING("audio element playback controls and status display", "accessibility role description for audio element controller");
    if (name == "VideoElement")
        return UI_STRING("video element playback controls and status display", "accessibility role description for video element controller");
    if (name == "MuteButton")
        return UI_STRING("mute audio tracks", "accessibility help text for mute button");
    if (name == "UnMuteButton")
        return UI_STRING("unmute audio tracks", "accessibility help text for un mute button");
    if (name == "PlayButton")
        return UI_STRING("begin playback", "accessibility help text for play button");
    if (name == "PauseButton")
        return UI_STRING("pause playback", "accessibility help text for pause button");
    if (name == "Slider")
        return UI_STRING("movie time scrubber", "accessibility help text for timeline slider");
    if (name == "SliderThumb")
        return UI_STRING("movie time scrubber thumb", "accessibility help text for timeline slider thumb");
    if (name == "RewindButton")
        return UI_STRING("seek movie back 30 seconds", "accessibility help text for jump back 30 seconds button");
    if (name == "ReturnToRealtimeButton")
        return UI_STRING("return streaming movie to real time", "accessibility help text for return streaming movie to real time button");
    if (name == "CurrentTimeDisplay")
        return UI_STRING("current movie time in seconds", "accessibility help text for elapsed time display");
    if (name == "TimeRemainingDisplay")
        return UI_STRING("number of seconds of movie remaining", "accessibility help text for remaining time display");
    if (name == "StatusDisplay")
        return UI_STRING("current movie status", "accessibility help text for movie status display");
    if (name == "SeekBackButton")
        return UI_STRING("seek quickly back", "accessibility help text for fast rewind button");
    if (name == "SeekForwardButton")
        return UI_STRING("seek quickly forward", "accessibility help text for fast forward button");
    if (name == "FullscreenButton")
        return UI_STRING("Play movie in fullscreen mode", "accessibility help text for enter fullscreen button");
    if (name == "ShowClosedCaptionsButton")
        return UI_STRING("start displaying closed captions", "accessibility help text for show closed captions button");
    if (name == "HideClosedCaptionsButton")
        return UI_STRING("stop displaying closed captions", "accessibility help text for hide closed captions button");

    ASSERT_NOT_REACHED();
    return String();
}

String WebPlatformStrategies::localizedMediaTimeDescription(float time)
{
    if (!isfinite(time))
        return UI_STRING("indefinite time", "accessibility help text for an indefinite media controller time value");

    int seconds = (int)fabsf(time); 
    int days = seconds / (60 * 60 * 24);
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (days)
        return [NSString stringWithFormat:UI_STRING("%1$d days %2$d hours %3$d minutes %4$d seconds", "accessibility help text for media controller time value >= 1 day"), days, hours, minutes, seconds];
    else if (hours)
        return [NSString stringWithFormat:UI_STRING("%1$d hours %2$d minutes %3$d seconds", "accessibility help text for media controller time value >= 60 minutes"), hours, minutes, seconds];
    else if (minutes)
        return [NSString stringWithFormat:UI_STRING("%1$d minutes %2$d seconds", "accessibility help text for media controller time value >= 60 seconds"), minutes, seconds];

    return [NSString stringWithFormat:UI_STRING("%1$d seconds", "accessibility help text for media controller time value < 60 seconds"), seconds];
}

String WebPlatformStrategies::validationMessageValueMissingText()
{
    return UI_STRING("value missing", "Validation message for required form control elements that have no value");
}

String WebPlatformStrategies::validationMessageTypeMismatchText()
{
    return UI_STRING("type mismatch", "Validation message for input form controls with a value not matching type");
}

String WebPlatformStrategies::validationMessagePatternMismatchText()
{
    return UI_STRING("pattern mismatch", "Validation message for input form controls requiring a constrained value according to pattern");
}

String WebPlatformStrategies::validationMessageTooLongText()
{
    return UI_STRING("too long", "Validation message for form control elements with a value longer than maximum allowed length");
}

String WebPlatformStrategies::validationMessageRangeUnderflowText()
{
    return UI_STRING("range underflow", "Validation message for input form controls with value lower than allowed minimum");
}

String WebPlatformStrategies::validationMessageRangeOverflowText()
{
    return UI_STRING("range overflow", "Validation message for input form controls with value higher than allowed maximum");
}

String WebPlatformStrategies::validationMessageStepMismatchText()
{
    return UI_STRING("step mismatch", "Validation message for input form controls with value not respecting the step attribute");
}

// VisitedLinkStrategy
bool WebPlatformStrategies::isLinkVisited(Page* page, LinkHash hash)
{
    return page->group().isLinkVisited(hash);
}

void WebPlatformStrategies::addVisitedLink(Page* page, LinkHash hash)
{
    return page->group().addVisitedLinkHash(hash);
}
