/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LocalizedStrings.h"

#include "IntSize.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "StringBuilder.h"

#include "WebKit.h"
#include "WebKitClient.h"
#include "WebLocalizedString.h"
#include "WebString.h"

using WebKit::WebLocalizedString;

namespace WebCore {

static String query(WebLocalizedString::Name name)
{
    return WebKit::webKitClient()->queryLocalizedString(name);
}

static String query(WebLocalizedString::Name name, int numericValue)
{
    return WebKit::webKitClient()->queryLocalizedString(name, numericValue);
}

String searchableIndexIntroduction()
{
    return query(WebLocalizedString::SearchableIndexIntroduction);
}

String submitButtonDefaultLabel()
{
    return query(WebLocalizedString::SubmitButtonDefaultLabel);
}

String inputElementAltText()
{
    return query(WebLocalizedString::InputElementAltText);
}

String resetButtonDefaultLabel()
{
    return query(WebLocalizedString::ResetButtonDefaultLabel);
}

String fileButtonChooseFileLabel()
{
    return query(WebLocalizedString::FileButtonChooseFileLabel);
}

String fileButtonNoFileSelectedLabel()
{
    return query(WebLocalizedString::FileButtonNoFileSelectedLabel);
}

String searchMenuNoRecentSearchesText()
{
    return query(WebLocalizedString::SearchMenuNoRecentSearchesText);
}
String searchMenuRecentSearchesText()
{
    return query(WebLocalizedString::SearchMenuRecentSearchesText);
}
String searchMenuClearRecentSearchesText()
{
    return query(WebLocalizedString::SearchMenuClearRecentSearchesText);
}

String AXWebAreaText()
{
    return query(WebLocalizedString::AXWebAreaText);
}

String AXLinkText()
{
    return query(WebLocalizedString::AXLinkText);
}

String AXListMarkerText()
{
    return query(WebLocalizedString::AXListMarkerText);
}

String AXImageMapText()
{
    return query(WebLocalizedString::AXImageMapText);
}

String AXHeadingText()
{
    return query(WebLocalizedString::AXHeadingText);
}

String AXDefinitionListTermText()
{
    notImplemented();
    return String("term");
}

String AXDefinitionListDefinitionText()
{
    notImplemented();
    return String("definition");
}

String AXButtonActionVerb()
{
    return query(WebLocalizedString::AXButtonActionVerb);
}

String AXRadioButtonActionVerb()
{
    return query(WebLocalizedString::AXRadioButtonActionVerb);
}

String AXTextFieldActionVerb()
{
    return query(WebLocalizedString::AXTextFieldActionVerb);
}

String AXCheckedCheckBoxActionVerb()
{
    return query(WebLocalizedString::AXCheckedCheckBoxActionVerb);
}

String AXUncheckedCheckBoxActionVerb()
{
    return query(WebLocalizedString::AXUncheckedCheckBoxActionVerb);
}

String AXLinkActionVerb()
{
    return query(WebLocalizedString::AXLinkActionVerb);
}

String AXMenuListPopupActionVerb()
{
    return String();
}

String AXMenuListActionVerb()
{
    return String();
}
    
String missingPluginText()
{
    notImplemented();
    return String("Missing Plug-in");
}

String crashedPluginText()
{
    notImplemented();
    return String("Plug-in Failure");
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    return query(WebLocalizedString::MultipleFileUploadText, numberOfFiles);
}

// Used in FTPDirectoryDocument.cpp
String unknownFileSizeText()
{
    return String();
}

// The following two functions are not declared in LocalizedStrings.h.
// They are used by the menu for the HTML keygen tag.
String keygenMenuHighGradeKeySize()
{
    return query(WebLocalizedString::KeygenMenuHighGradeKeySize);
}

String keygenMenuMediumGradeKeySize()
{
    return query(WebLocalizedString::KeygenMenuMediumGradeKeySize);
}

// Used in ImageDocument.cpp as the title for pages when that page is an image.
String imageTitle(const String& filename, const IntSize& size)
{
    // Note that we cannot use String::format because it works for ASCII only.
    StringBuilder result;
    result.append(filename);
    result.append(" (");
    result.append(String::number(size.width()));
    result.append(static_cast<UChar>(0xD7));  // U+00D7 (multiplication sign)
    result.append(String::number(size.height()));
    result.append(")");
    return result.toString();
}

// We don't use these strings, so they return an empty String. We can't just
// make them asserts because webcore still calls them.
String contextMenuItemTagOpenLinkInNewWindow() { return String(); }
String contextMenuItemTagDownloadLinkToDisk() { return String(); }
String contextMenuItemTagCopyLinkToClipboard() { return String(); }
String contextMenuItemTagOpenImageInNewWindow() { return String(); }
String contextMenuItemTagDownloadImageToDisk() { return String(); }
String contextMenuItemTagCopyImageToClipboard() { return String(); }
String contextMenuItemTagOpenFrameInNewWindow() { return String(); }
String contextMenuItemTagCopy() { return String(); }
String contextMenuItemTagGoBack() { return String(); }
String contextMenuItemTagGoForward() { return String(); }
String contextMenuItemTagStop() { return String(); }
String contextMenuItemTagReload() { return String(); }
String contextMenuItemTagCut() { return String(); }
String contextMenuItemTagPaste() { return String(); }
String contextMenuItemTagNoGuessesFound() { return String(); }
String contextMenuItemTagIgnoreSpelling() { return String(); }
String contextMenuItemTagLearnSpelling() { return String(); }
String contextMenuItemTagSearchWeb() { return String(); }
String contextMenuItemTagLookUpInDictionary() { return String(); }
String contextMenuItemTagOpenLink() { return String(); }
String contextMenuItemTagIgnoreGrammar() { return String(); }
String contextMenuItemTagSpellingMenu() { return String(); }
String contextMenuItemTagCheckSpelling() { return String(); }
String contextMenuItemTagCheckSpellingWhileTyping() { return String(); }
String contextMenuItemTagCheckGrammarWithSpelling() { return String(); }
String contextMenuItemTagFontMenu() { return String(); }
String contextMenuItemTagBold() { return String(); }
String contextMenuItemTagItalic() { return String(); }
String contextMenuItemTagUnderline() { return String(); }
String contextMenuItemTagOutline() { return String(); }
String contextMenuItemTagWritingDirectionMenu() { return String(); }
String contextMenuItemTagTextDirectionMenu() { return String(); }
String contextMenuItemTagDefaultDirection() { return String(); }
String contextMenuItemTagLeftToRight() { return String(); }
String contextMenuItemTagRightToLeft() { return String(); }
String contextMenuItemTagInspectElement() { return String(); }
String contextMenuItemTagShowSpellingPanel(bool show) { return String(); }
String mediaElementLiveBroadcastStateText() { return String(); }
String mediaElementLoadingStateText() { return String(); }
String contextMenuItemTagOpenVideoInNewWindow() { return String(); }
String contextMenuItemTagOpenAudioInNewWindow() { return String(); }
String contextMenuItemTagCopyVideoLinkToClipboard() { return String(); }
String contextMenuItemTagCopyAudioLinkToClipboard() { return String(); }
String contextMenuItemTagToggleMediaControls() { return String(); }
String contextMenuItemTagToggleMediaLoop() { return String(); }
String contextMenuItemTagEnterVideoFullscreen() { return String(); }
String contextMenuItemTagMediaPlay() { return String(); }
String contextMenuItemTagMediaPause() { return String(); }
String contextMenuItemTagMediaMute() { return String(); }

String localizedMediaControlElementString(const String& /*name*/)
{
    // FIXME: to be fixed.
    return String();
}

String localizedMediaControlElementHelpText(const String& /*name*/)
{
    // FIXME: to be fixed.
    return String();
}

String localizedMediaTimeDescription(float /*time*/)
{
    // FIXME: to be fixed.
    return String();
}

String validationMessageValueMissingText()
{
    notImplemented();
    return String();
}

String validationMessageTypeMismatchText()
{
    notImplemented();
    return String();
}

String validationMessagePatternMismatchText()
{
    notImplemented();
    return String();
}

String validationMessageTooLongText()
{
    notImplemented();
    return String();
}

String validationMessageRangeUnderflowText()
{
    notImplemented();
    return String();
}

String validationMessageRangeOverflowText()
{
    notImplemented();
    return String();
}

String validationMessageStepMismatchText()
{
    notImplemented();
    return String();
}

} // namespace WebCore
