/*
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 *
 * All rights reserved.
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

#include "NotImplemented.h"
#include "PlatformString.h"


namespace WebCore {
String submitButtonDefaultLabel()
{
    return "Submit";
}

String inputElementAltText()
{
    return String();
}

String resetButtonDefaultLabel()
{
    return "Reset";
}

String defaultDetailsSummaryText()
{
    return "Details";
}

String defaultLanguage()
{
    return "en";
}

String searchableIndexIntroduction()
{
    return "Searchable Index";
}

String fileButtonChooseFileLabel()
{
    return "Choose File";
}

String fileButtonNoFileSelectedLabel()
{
    return "No file selected";
}

String contextMenuItemTagOpenLinkInNewWindow()
{
    return "Open in new tab";
}

String contextMenuItemTagDownloadLinkToDisk()
{
    return "Download link to disk";
}

String contextMenuItemTagCopyLinkToClipboard()
{
    return "Copy link to clipboard";
}

String contextMenuItemTagOpenImageInNewWindow()
{
    return "Open image in new window";
}

String contextMenuItemTagDownloadImageToDisk()
{
    return "Download image to disk";
}

String contextMenuItemTagCopyImageToClipboard()
{
    return "Copy image to clipboard";
}

String contextMenuItemTagOpenVideoInNewWindow()
{
    return "Open video in new window";
}

String contextMenuItemTagOpenAudioInNewWindow()
{
    return "Open audio in new window";
}

String contextMenuItemTagCopyVideoLinkToClipboard()
{
    return "Copy video link location";
}

String contextMenuItemTagCopyAudioLinkToClipboard()
{
    return "Copy audio link location";
}

String contextMenuItemTagToggleMediaControls()
{
    return "Toggle media controls";
}

String contextMenuItemTagToggleMediaLoop()
{
    return "Toggle media loop playback";
}

String contextMenuItemTagEnterVideoFullscreen()
{
    return "Switch video to fullscreen";
}

String contextMenuItemTagMediaPlay()
{
    return "Play";
}

String contextMenuItemTagMediaPause()
{
    return "Pause";
}

String contextMenuItemTagMediaMute()
{
    return "Mute";
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    return "Open frame in new window";
}

String contextMenuItemTagCopy()
{
    return "Copy";
}

String contextMenuItemTagGoBack()
{
    return "Go back";
}

String contextMenuItemTagGoForward()
{
    return "Go forward";
}

String contextMenuItemTagStop()
{
    return "Stop";
}

String contextMenuItemTagReload()
{
    return "Reload";
}

String contextMenuItemTagCut()
{
    return "Cut";
}

String contextMenuItemTagPaste()
{
    return "Paste";
}

String contextMenuItemTagNoGuessesFound()
{
    return "No guesses found";
}

String contextMenuItemTagIgnoreSpelling()
{
    return "Ignore spelling";
}

String contextMenuItemTagLearnSpelling()
{
    return "Learn spelling";
}

String contextMenuItemTagSearchWeb()
{
    return "Search web";
}

String contextMenuItemTagLookUpInDictionary(const String&)
{
    return "Lookup in dictionary";
}

String contextMenuItemTagOpenLink()
{
    return "Open link";
}

String contextMenuItemTagIgnoreGrammar()
{
    return "Ignore grammar";
}

String contextMenuItemTagSpellingMenu()
{
    return "Spelling menu";
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    return "Show spelling panel";
}

String contextMenuItemTagCheckSpelling()
{
    return "Check spelling";
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    return "Check spelling while typing";
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    return "Check for grammar with spelling";
}

String contextMenuItemTagFontMenu()
{
    return "Font menu";
}

String contextMenuItemTagBold()
{
    return "Bold";
}

String contextMenuItemTagItalic()
{
    return "Italic";
}

String contextMenuItemTagUnderline()
{
    return "Underline";
}

String contextMenuItemTagOutline()
{
    return "Outline";
}

String contextMenuItemTagWritingDirectionMenu()
{
    return "Writing direction menu";
}

String contextMenuItemTagDefaultDirection()
{
    return "Default direction";
}

String contextMenuItemTagLeftToRight()
{
    return "Left to right";
}

String contextMenuItemTagRightToLeft()
{
    return "Right to left";
}

String contextMenuItemTagInspectElement()
{
    return "Inspect";
}

String searchMenuNoRecentSearchesText()
{
    return "No recent text searches";
}

String searchMenuRecentSearchesText()
{
    return "Recent text searches";
}

String searchMenuClearRecentSearchesText()
{
    return "Clear recent text searches";
}

String unknownFileSizeText()
{
    return "Unknown";
}

String AXWebAreaText()
{
    return String();
}

String AXLinkText()
{
    return String();
}

String AXListMarkerText()
{
    return String();
}

String AXImageMapText()
{
    return String();
}

String AXHeadingText()
{
    return String();
}

String AXMenuListPopupActionVerb()
{
    return String();
}

String AXMenuListActionVerb()
{
    return String();
}

String imageTitle(const String& filename, const IntSize& size)
{
    return String(filename);
}

String contextMenuItemTagTextDirectionMenu()
{
    return String();
}

String AXButtonActionVerb()
{
    return String();
}

String AXTextFieldActionVerb()
{
    return String();
}

String AXRadioButtonActionVerb()
{
    return String();
}

String AXCheckedCheckBoxActionVerb()
{
    return String();
}

String AXUncheckedCheckBoxActionVerb()
{
    return String();
}

String AXLinkActionVerb()
{
    return String();
}

String AXDefinitionListTermText()
{
    return String();
}

String AXDefinitionListDefinitionText()
{
    return String();
}

String validationMessageValueMissingText()
{
    notImplemented();
    return String();
}

String validationMessageValueMissingForCheckboxText()
{
    notImplemented();
    return validationMessageValueMissingText();
}

String validationMessageValueMissingForFileText()
{
    notImplemented();
    return validationMessageValueMissingText();
}

String validationMessageValueMissingForMultipleFileText()
{
    notImplemented();
    return validationMessageValueMissingText();
}

String validationMessageValueMissingForRadioText()
{
    notImplemented();
    return validationMessageValueMissingText();
}

String validationMessageValueMissingForSelectText()
{
    notImplemented();
    return validationMessageValueMissingText();
}

String validationMessageTypeMismatchText()
{
    notImplemented();
    return String();
}

String validationMessageTypeMismatchForEmailText()
{
    notImplemented();
    return validationMessageTypeMismatchText();
}

String validationMessageTypeMismatchForMultipleEmailText()
{
    notImplemented();
    return validationMessageTypeMismatchText();
}

String validationMessageTypeMismatchForURLText()
{
    notImplemented();
    return validationMessageTypeMismatchText();
}

String validationMessagePatternMismatchText()
{
    notImplemented();
    return String();
}

String validationMessageTooLongText(int, int)
{
    notImplemented();
    return String();
}

String validationMessageRangeUnderflowText(const String&)
{
    notImplemented();
    return String();
}

String validationMessageRangeOverflowText(const String&)
{
    notImplemented();
    return String();
}

String validationMessageStepMismatchText(const String&, const String&)
{
    notImplemented();
    return String();
}

} // namespace WebCore

