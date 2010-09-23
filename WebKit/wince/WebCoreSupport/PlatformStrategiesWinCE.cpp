/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "config.h"
#include "PlatformStrategiesWinCE.h"

#include "IntSize.h"
#include "MathExtras.h"
#include "Page.h"
#include "PageGroup.h"
#include "PluginDatabase.h"
#include <wtf/text/CString.h>

#define UI_STRING(text, desciprion) text
#define UI_STRING_KEY(text, key, desciprion) text

using namespace WebCore;

void PlatformStrategiesWinCE::initialize()
{
    DEFINE_STATIC_LOCAL(PlatformStrategiesWinCE, platformStrategies, ());
}

PlatformStrategiesWinCE::PlatformStrategiesWinCE()
{
    setPlatformStrategies(this);
}

// PluginStrategy

PluginStrategy* PlatformStrategiesWinCE::createPluginStrategy()
{
    return this;
}

LocalizationStrategy* PlatformStrategiesWinCE::createLocalizationStrategy()
{
    return this;
}

VisitedLinkStrategy* PlatformStrategiesWinCE::createVisitedLinkStrategy()
{
    return this;
}

void PlatformStrategiesWinCE::refreshPlugins()
{
    PluginDatabase::installedPlugins()->refresh();
}

void PlatformStrategiesWinCE::getPluginInfo(const Page*, Vector<PluginInfo>& outPlugins)
{
    const Vector<PluginPackage*>& plugins = PluginDatabase::installedPlugins()->plugins();

    outPlugins.resize(plugins.size());

    for (size_t i = 0; i < plugins.size(); ++i) {
        PluginPackage* package = plugins[i];

        PluginInfo info;
        info.name = package->name();
        info.file = package->fileName();
        info.desc = package->description();

        const MIMEToDescriptionsMap& mimeToDescriptions = package->mimeToDescriptions();

        info.mimes.reserveCapacity(mimeToDescriptions.size());

        MIMEToDescriptionsMap::const_iterator end = mimeToDescriptions.end();
        for (MIMEToDescriptionsMap::const_iterator it = mimeToDescriptions.begin(); it != end; ++it) {
            MimeClassInfo mime;

            mime.type = it->first;
            mime.desc = it->second;
            mime.extensions = package->mimeToExtensions().get(mime.type);

            info.mimes.append(mime);
        }

        outPlugins[i] = info;
    }
}

// LocalizationStrategy    

String PlatformStrategiesWinCE::searchableIndexIntroduction()
{
    return UI_STRING("This is a searchable index. Enter search keywords: ", "text that appears at the start of nearly-obsolete web pages in the form of a 'searchable index'");
}

String PlatformStrategiesWinCE::submitButtonDefaultLabel()
{
    return UI_STRING("Submit", "default label for Submit buttons in forms on web pages");
}

String PlatformStrategiesWinCE::inputElementAltText()
{
    return UI_STRING_KEY("Submit", "Submit (input element)", "alt text for <input> elements with no alt, title, or value");
}

String PlatformStrategiesWinCE::resetButtonDefaultLabel()
{
    return UI_STRING("Reset", "default label for Reset buttons in forms on web pages");
}

String PlatformStrategiesWinCE::fileButtonChooseFileLabel()
{
    return UI_STRING("Choose File", "title for file button used in HTML forms");
}

String PlatformStrategiesWinCE::fileButtonNoFileSelectedLabel()
{
    return UI_STRING("no file selected", "text to display in file button used in HTML forms when no file is selected");
}

String PlatformStrategiesWinCE::contextMenuItemTagOpenLinkInNewWindow()
{
    return UI_STRING("Open Link in New Window", "Open in New Window context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagDownloadLinkToDisk()
{
    return UI_STRING("Download Linked File", "Download Linked File context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagCopyLinkToClipboard()
{
    return UI_STRING("Copy Link", "Copy Link context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagOpenImageInNewWindow()
{
    return UI_STRING("Open Image in New Window", "Open Image in New Window context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagDownloadImageToDisk()
{
    return UI_STRING("Download Image", "Download Image context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagCopyImageToClipboard()
{
    return UI_STRING("Copy Image", "Copy Image context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagOpenVideoInNewWindow()
{
    return UI_STRING("Open Video in New Window", "Open Video in New Window context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagOpenAudioInNewWindow()
{
    return UI_STRING("Open Audio in New Window", "Open Audio in New Window context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagCopyVideoLinkToClipboard()
{
    return UI_STRING("Copy Video Address", "Copy Video Address Location context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagCopyAudioLinkToClipboard()
{
    return UI_STRING("Copy Audio Address", "Copy Audio Address Location context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagToggleMediaControls()
{
    return UI_STRING("Controls", "Media Controls context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagToggleMediaLoop()
{
    return UI_STRING("Loop", "Media Loop context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagEnterVideoFullscreen()
{
    return UI_STRING("Enter Fullscreen", "Video Enter Fullscreen context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagMediaPlay()
{
    return UI_STRING("Play", "Media Play context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagMediaPause()
{
    return UI_STRING("Pause", "Media Pause context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagMediaMute()
{
    return UI_STRING("Mute", "Media Mute context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagOpenFrameInNewWindow()
{
    return UI_STRING("Open Frame in New Window", "Open Frame in New Window context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagCopy()
{
    return UI_STRING("Copy", "Copy context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagGoBack()
{
    return UI_STRING("Back", "Back context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagGoForward()
{
    return UI_STRING("Forward", "Forward context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagStop()
{
    return UI_STRING("Stop", "Stop context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagReload()
{
    return UI_STRING("Reload", "Reload context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagCut()
{
    return UI_STRING("Cut", "Cut context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagPaste()
{
    return UI_STRING("Paste", "Paste context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagNoGuessesFound()
{
    return UI_STRING("No Guesses Found", "No Guesses Found context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagIgnoreSpelling()
{
    return UI_STRING("Ignore Spelling", "Ignore Spelling context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagLearnSpelling()
{
    return UI_STRING("Learn Spelling", "Learn Spelling context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagSearchWeb()
{
    return UI_STRING("Search with Google", "Search in Google context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagLookUpInDictionary()
{
    return UI_STRING("Look Up in Dictionary", "Look Up in Dictionary context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagOpenLink()
{
    return UI_STRING("Open Link", "Open Link context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagIgnoreGrammar()
{
    return UI_STRING("Ignore Grammar", "Ignore Grammar context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagSpellingMenu()
{
    return UI_STRING("Spelling and Grammar", "Spelling and Grammar context sub-menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagCheckSpelling()
{
    return UI_STRING("Check Document Now", "Check spelling context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagCheckSpellingWhileTyping()
{
    return UI_STRING("Check Spelling While Typing", "Check spelling while typing context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagCheckGrammarWithSpelling()
{
    return UI_STRING("Check Grammar With Spelling", "Check grammar with spelling context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagFontMenu()
{
    return UI_STRING("Font", "Font context sub-menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagBold()
{
    return UI_STRING("Bold", "Bold context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagItalic()
{
    return UI_STRING("Italic", "Italic context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagUnderline()
{
    return UI_STRING("Underline", "Underline context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagOutline()
{
    return UI_STRING("Outline", "Outline context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagWritingDirectionMenu()
{
    return UI_STRING("Paragraph Direction", "Paragraph direction context sub-menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagTextDirectionMenu()
{
    return UI_STRING("Selection Direction", "Selection direction context sub-menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagDefaultDirection()
{
    return UI_STRING("Default", "Default writing direction context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagLeftToRight()
{
    return UI_STRING("Left to Right", "Left to Right context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagRightToLeft()
{
    return UI_STRING("Right to Left", "Right to Left context menu item");
}

String PlatformStrategiesWinCE::contextMenuItemTagShowSpellingPanel(bool show)
{
    if (show)
        return UI_STRING("Show Spelling and Grammar", "menu item title");
    return UI_STRING("Hide Spelling and Grammar", "menu item title");
}

String PlatformStrategiesWinCE::contextMenuItemTagInspectElement()
{
    return UI_STRING("Inspect Element", "Inspect Element context menu item");
}

String PlatformStrategiesWinCE::searchMenuNoRecentSearchesText()
{
    return UI_STRING("No recent searches", "Label for only item in menu that appears when clicking on the search field image, when no searches have been performed");
}

String PlatformStrategiesWinCE::searchMenuRecentSearchesText()
{
    return UI_STRING("Recent Searches", "label for first item in the menu that appears when clicking on the search field image, used as embedded menu title");
}

String PlatformStrategiesWinCE::searchMenuClearRecentSearchesText()
{
    return UI_STRING("Clear Recent Searches", "menu item in Recent Searches menu that empties menu's contents");
}

String PlatformStrategiesWinCE::AXWebAreaText()
{
    return UI_STRING("web area", "accessibility role description for web area");
}

String PlatformStrategiesWinCE::AXLinkText()
{
    return UI_STRING("link", "accessibility role description for link");
}

String PlatformStrategiesWinCE::AXListMarkerText()
{
    return UI_STRING("list marker", "accessibility role description for list marker");
}

String PlatformStrategiesWinCE::AXImageMapText()
{
    return UI_STRING("image map", "accessibility role description for image map");
}

String PlatformStrategiesWinCE::AXHeadingText()
{
    return UI_STRING("heading", "accessibility role description for headings");
}

String PlatformStrategiesWinCE::AXDefinitionListTermText()
{
    return UI_STRING("term", "term word of a definition");
}

String PlatformStrategiesWinCE::AXDefinitionListDefinitionText()
{
    return UI_STRING("definition", "definition phrase");
}

String PlatformStrategiesWinCE::AXButtonActionVerb()
{
    return UI_STRING("press", "Verb stating the action that will occur when a button is pressed, as used by accessibility");
}

String PlatformStrategiesWinCE::AXRadioButtonActionVerb()
{
    return UI_STRING("select", "Verb stating the action that will occur when a radio button is clicked, as used by accessibility");
}

String PlatformStrategiesWinCE::AXTextFieldActionVerb()
{
    return UI_STRING("activate", "Verb stating the action that will occur when a text field is selected, as used by accessibility");
}

String PlatformStrategiesWinCE::AXCheckedCheckBoxActionVerb()
{
    return UI_STRING("uncheck", "Verb stating the action that will occur when a checked checkbox is clicked, as used by accessibility");
}

String PlatformStrategiesWinCE::AXUncheckedCheckBoxActionVerb()
{
    return UI_STRING("check", "Verb stating the action that will occur when an unchecked checkbox is clicked, as used by accessibility");
}

String PlatformStrategiesWinCE::AXLinkActionVerb()
{
    return UI_STRING("jump", "Verb stating the action that will occur when a link is clicked, as used by accessibility");
}

String PlatformStrategiesWinCE::AXMenuListActionVerb()
{
    return UI_STRING("open", "Verb stating the action that will occur when a select element is clicked, as used by accessibility");
}

String PlatformStrategiesWinCE::AXMenuListPopupActionVerb()
{
    return UI_STRING_KEY("press", "press (select element)", "Verb stating the action that will occur when a select element's popup list is clicked, as used by accessibility");
}

String PlatformStrategiesWinCE::unknownFileSizeText()
{
    return UI_STRING("Unknown", "Unknown filesize FTP directory listing item");
}

String PlatformStrategiesWinCE::uploadFileText()
{
    return UI_STRING("Upload file", "(Windows) Form submit file upload dialog title");
}

String PlatformStrategiesWinCE::allFilesText()
{
    return UI_STRING("All Files", "(Windows) Form submit file upload all files pop-up");
}

String PlatformStrategiesWinCE::missingPluginText()
{
    return UI_STRING("Missing Plug-in", "Label text to be used when a plugin is missing");
}

String PlatformStrategiesWinCE::crashedPluginText()
{
    return UI_STRING("Plug-in Failure", "Label text to be used if plugin host process has crashed");
}

String PlatformStrategiesWinCE::imageTitle(const String& filename, const IntSize& size) 
{
    CString filenameCString = filename.utf8();
    return String::format(UI_STRING("%s %d\xC3\x97%d pixels", "window title for a standalone image (uses multiplication symbol, not x)"), filenameCString.data(), size.width(), size.height());
}

String PlatformStrategiesWinCE::multipleFileUploadText(unsigned numberOfFiles)
{
    return String::format(UI_STRING("%d files", "Label to describe the number of files selected in a file upload control that allows multiple files"), numberOfFiles);
}

String PlatformStrategiesWinCE::mediaElementLoadingStateText()
{
    return UI_STRING("Loading...", "Media controller status message when the media is loading");
}

String PlatformStrategiesWinCE::mediaElementLiveBroadcastStateText()
{
    return UI_STRING("Live Broadcast", "Media controller status message when watching a live broadcast");
}

String PlatformStrategiesWinCE::localizedMediaControlElementString(const String& name)
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

    ASSERT_NOT_REACHED();
    return String();
}

String PlatformStrategiesWinCE::localizedMediaControlElementHelpText(const String& name)
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

String PlatformStrategiesWinCE::localizedMediaTimeDescription(float time)
{
    if (!isfinite(time))
        return UI_STRING("indefinite time", "accessibility help text for an indefinite media controller time value");

    int seconds = (int)fabsf(time);
    int days = seconds / (60 * 60 * 24);
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (days)
        return String::format(UI_STRING("%1$d days %2$d hours %3$d minutes %4$d seconds", "accessibility help text for media controller time value >= 1 day"), days, hours, minutes, seconds);

    if (hours)
        return String::format(UI_STRING("%1$d hours %2$d minutes %3$d seconds", "accessibility help text for media controller time value >= 60 minutes"), hours, minutes, seconds);

    if (minutes)
        return String::format(UI_STRING("%1$d minutes %2$d seconds", "accessibility help text for media controller time value >= 60 seconds"), minutes, seconds);

    return String::format(UI_STRING("%1$d seconds", "accessibility help text for media controller time value < 60 seconds"), seconds);
}

String PlatformStrategiesWinCE::validationMessageValueMissingText()
{
    return UI_STRING("value missing", "Validation message for required form control elements that have no value");
}

String PlatformStrategiesWinCE::validationMessageTypeMismatchText()
{
    return UI_STRING("type mismatch", "Validation message for input form controls with a value not matching type");
}

String PlatformStrategiesWinCE::validationMessagePatternMismatchText()
{
    return UI_STRING("pattern mismatch", "Validation message for input form controls requiring a constrained value according to pattern");
}

String PlatformStrategiesWinCE::validationMessageTooLongText()
{
    return UI_STRING("too long", "Validation message for form control elements with a value longer than maximum allowed length");
}

String PlatformStrategiesWinCE::validationMessageRangeUnderflowText()
{
    return UI_STRING("range underflow", "Validation message for input form controls with value lower than allowed minimum");
}

String PlatformStrategiesWinCE::validationMessageRangeOverflowText()
{
    return UI_STRING("range overflow", "Validation message for input form controls with value higher than allowed maximum");
}

String PlatformStrategiesWinCE::validationMessageStepMismatchText()
{
    return UI_STRING("step mismatch", "Validation message for input form controls with value not respecting the step attribute");
}

bool PlatformStrategiesWinCE::isLinkVisited(Page* page, LinkHash hash)
{
    return page->group().isLinkVisited(hash);
}

void PlatformStrategiesWinCE::addVisitedLink(Page* page, LinkHash hash)
{
    page->group().addVisitedLinkHash(hash);
}
