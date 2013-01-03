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

#include "DateTimeFormat.h"
#include "IntSize.h"
#include "NotImplemented.h"

#include <public/Platform.h>
#include <public/WebLocalizedString.h>
#include <public/WebString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

using WebKit::WebLocalizedString;
using WebKit::WebString;

namespace WebCore {

static String query(WebLocalizedString::Name name)
{
    return WebKit::Platform::current()->queryLocalizedString(name);
}

static String query(WebLocalizedString::Name name, const WebString& parameter)
{
    return WebKit::Platform::current()->queryLocalizedString(name, parameter);
}

static String query(WebLocalizedString::Name name, const WebString& parameter1, const WebString& parameter2)
{
    return WebKit::Platform::current()->queryLocalizedString(name, parameter1, parameter2);
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

String fileButtonChooseMultipleFilesLabel()
{
    return query(WebLocalizedString::FileButtonChooseMultipleFilesLabel);
}

String defaultDetailsSummaryText()
{
    return query(WebLocalizedString::DetailsLabel);
}

String fileButtonNoFileSelectedLabel()
{
    return query(WebLocalizedString::FileButtonNoFileSelectedLabel);
}

String fileButtonNoFilesSelectedLabel()
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

String AXFooterRoleDescriptionText()
{
    notImplemented();
    return String("footer");
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
    
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
String AXAMPMFieldText()
{
    return query(WebLocalizedString::AXAMPMFieldText);
}

String AXDayOfMonthFieldText()
{
    return query(WebLocalizedString::AXDayOfMonthFieldText);
}

String AXDateTimeFieldEmptyValueText()
{
    return query(WebLocalizedString::AXDateTimeFieldEmptyValueText);
}

String AXHourFieldText()
{
    return query(WebLocalizedString::AXHourFieldText);
}

String AXMillisecondFieldText()
{
    return query(WebLocalizedString::AXMillisecondFieldText);
}

String AXMinuteFieldText()
{
    return query(WebLocalizedString::AXMinuteFieldText);
}

String AXMonthFieldText()
{
    return query(WebLocalizedString::AXMonthFieldText);
}

String AXSecondFieldText()
{
    return query(WebLocalizedString::AXSecondFieldText);
}

String AXWeekOfYearFieldText()
{
    return query(WebLocalizedString::AXWeekOfYearFieldText);
}

String AXYearFieldText()
{
    return query(WebLocalizedString::AXYearFieldText);
}

String placeholderForDayOfMonthField()
{
    return query(WebLocalizedString::PlaceholderForDayOfMonthField);
}

String placeholderForMonthField()
{
    return query(WebLocalizedString::PlaceholderForMonthField);
}

String placeholderForYearField()
{
    return query(WebLocalizedString::PlaceholderForYearField);
}
#endif

#if ENABLE(INPUT_TYPE_WEEK)
String weekFormatInLDML()
{
    String templ = query(WebLocalizedString::WeekFormatTemplate);
    // Converts a string like "Week $2, $1" to an LDML date format pattern like
    // "'Week 'ww', 'yyyy".
    StringBuilder builder;
    unsigned literalStart = 0;
    unsigned length = templ.length();
    for (unsigned i = 0; i + 1 < length; ++i) {
        if (templ[i] == '$' && (templ[i + 1] == '1' || templ[i + 1] == '2')) {
            if (literalStart < i)
                DateTimeFormat::quoteAndAppendLiteral(templ.substring(literalStart, i - literalStart), builder);
            builder.append(templ[++i] == '1' ? "yyyy" : "ww");
            literalStart = i + 1;
        }
    }
    if (literalStart < length)
        DateTimeFormat::quoteAndAppendLiteral(templ.substring(literalStart, length - literalStart), builder);
    return builder.toString();
}

#endif

String missingPluginText()
{
    return query(WebLocalizedString::MissingPluginText);
}

String crashedPluginText()
{
    notImplemented();
    return String("Plug-in Failure");
}

String blockedPluginByContentSecurityPolicyText()
{
    notImplemented();
    return String();
}

String insecurePluginVersionText()
{
    notImplemented();
    return String();
}

String inactivePluginText()
{
    notImplemented();
    return String();
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    return query(WebLocalizedString::MultipleFileUploadText, String::number(numberOfFiles));
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
    StringBuilder result;
    result.append(filename);
    result.append(" (");
    result.append(String::number(size.width()));
    result.append(static_cast<UChar>(0xD7));  // U+00D7 (multiplication sign)
    result.append(String::number(size.height()));
    result.append(')');
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
String contextMenuItemTagLookUpInDictionary(const String&) { return String(); }
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

#if ENABLE(VIDEO_TRACK)
String textTrackClosedCaptionsText() { return String(); }
String textTrackSubtitlesText() { return String(); }
String textTrackOffText() { return String(); }
String textTrackNoLabelText() { return String(); }
#endif

String localizedMediaControlElementString(const String& name)
{
    if (name == "AudioElement")
        return query(WebLocalizedString::AXMediaAudioElement);
    if (name == "VideoElement")
        return query(WebLocalizedString::AXMediaVideoElement);
    if (name == "MuteButton")
        return query(WebLocalizedString::AXMediaMuteButton);
    if (name == "UnMuteButton")
        return query(WebLocalizedString::AXMediaUnMuteButton);
    if (name == "PlayButton")
        return query(WebLocalizedString::AXMediaPlayButton);
    if (name == "PauseButton")
        return query(WebLocalizedString::AXMediaPauseButton);
    if (name == "Slider")
        return query(WebLocalizedString::AXMediaSlider);
    if (name == "SliderThumb")
        return query(WebLocalizedString::AXMediaSliderThumb);
    if (name == "RewindButton")
        return query(WebLocalizedString::AXMediaRewindButton);
    if (name == "ReturnToRealtimeButton")
        return query(WebLocalizedString::AXMediaReturnToRealTime);
    if (name == "CurrentTimeDisplay")
        return query(WebLocalizedString::AXMediaCurrentTimeDisplay);
    if (name == "TimeRemainingDisplay")
        return query(WebLocalizedString::AXMediaTimeRemainingDisplay);
    if (name == "StatusDisplay")
        return query(WebLocalizedString::AXMediaStatusDisplay);
    if (name == "EnterFullscreenButton")
        return query(WebLocalizedString::AXMediaEnterFullscreenButton);
    if (name == "ExitFullscreenButton")
        return query(WebLocalizedString::AXMediaExitFullscreenButton);
    if (name == "SeekForwardButton")
        return query(WebLocalizedString::AXMediaSeekForwardButton);
    if (name == "SeekBackButton")
        return query(WebLocalizedString::AXMediaSeekBackButton);
    if (name == "ShowClosedCaptionsButton")
        return query(WebLocalizedString::AXMediaShowClosedCaptionsButton);
    if (name == "HideClosedCaptionsButton")
        return query(WebLocalizedString::AXMediaHideClosedCaptionsButton);

    // FIXME: the ControlsPanel container should never be visible in the accessibility hierarchy.
    if (name == "ControlsPanel")
        return query(WebLocalizedString::AXMediaDefault);

    ASSERT_NOT_REACHED();
    return query(WebLocalizedString::AXMediaDefault);
}

String localizedMediaControlElementHelpText(const String& name)
{
    if (name == "AudioElement")
        return query(WebLocalizedString::AXMediaAudioElementHelp);
    if (name == "VideoElement")
        return query(WebLocalizedString::AXMediaVideoElementHelp);
    if (name == "MuteButton")
        return query(WebLocalizedString::AXMediaMuteButtonHelp);
    if (name == "UnMuteButton")
        return query(WebLocalizedString::AXMediaUnMuteButtonHelp);
    if (name == "PlayButton")
        return query(WebLocalizedString::AXMediaPlayButtonHelp);
    if (name == "PauseButton")
        return query(WebLocalizedString::AXMediaPauseButtonHelp);
    if (name == "Slider")
        return query(WebLocalizedString::AXMediaSliderHelp);
    if (name == "SliderThumb")
        return query(WebLocalizedString::AXMediaSliderThumbHelp);
    if (name == "RewindButton")
        return query(WebLocalizedString::AXMediaRewindButtonHelp);
    if (name == "ReturnToRealtimeButton")
        return query(WebLocalizedString::AXMediaReturnToRealTimeHelp);
    if (name == "CurrentTimeDisplay")
        return query(WebLocalizedString::AXMediaCurrentTimeDisplayHelp);
    if (name == "TimeRemainingDisplay")
        return query(WebLocalizedString::AXMediaTimeRemainingDisplayHelp);
    if (name == "StatusDisplay")
        return query(WebLocalizedString::AXMediaStatusDisplayHelp);
    if (name == "EnterFullscreenButton")
        return query(WebLocalizedString::AXMediaEnterFullscreenButtonHelp);
    if (name == "ExitFullscreenButton")
        return query(WebLocalizedString::AXMediaExitFullscreenButtonHelp);
    if (name == "SeekForwardButton")
        return query(WebLocalizedString::AXMediaSeekForwardButtonHelp);
    if (name == "SeekBackButton")
        return query(WebLocalizedString::AXMediaSeekBackButtonHelp);
    if (name == "ShowClosedCaptionsButton")
        return query(WebLocalizedString::AXMediaShowClosedCaptionsButtonHelp);
    if (name == "HideClosedCaptionsButton")
        return query(WebLocalizedString::AXMediaHideClosedCaptionsButtonHelp);

    ASSERT_NOT_REACHED();
    return query(WebLocalizedString::AXMediaDefault);
}

String localizedMediaTimeDescription(float /*time*/)
{
    // FIXME: to be fixed.
    return String();
}

String validationMessageValueMissingText()
{
    return query(WebLocalizedString::ValidationValueMissing);
}

String validationMessageValueMissingForCheckboxText()
{
    return query(WebLocalizedString::ValidationValueMissingForCheckbox);
}

String validationMessageValueMissingForFileText()
{
    return query(WebLocalizedString::ValidationValueMissingForFile);
}

String validationMessageValueMissingForMultipleFileText()
{
    return query(WebLocalizedString::ValidationValueMissingForMultipleFile);
}

String validationMessageValueMissingForRadioText()
{
    return query(WebLocalizedString::ValidationValueMissingForRadio);
}

String validationMessageValueMissingForSelectText()
{
    return query(WebLocalizedString::ValidationValueMissingForSelect);
}

String validationMessageTypeMismatchText()
{
    return query(WebLocalizedString::ValidationTypeMismatch);
}

String validationMessageTypeMismatchForEmailText()
{
    return query(WebLocalizedString::ValidationTypeMismatchForEmail);
}

String validationMessageTypeMismatchForMultipleEmailText()
{
    return query(WebLocalizedString::ValidationTypeMismatchForMultipleEmail);
}

String validationMessageTypeMismatchForURLText()
{
    return query(WebLocalizedString::ValidationTypeMismatchForURL);
}

String validationMessagePatternMismatchText()
{
    return query(WebLocalizedString::ValidationPatternMismatch);
}

String validationMessageTooLongText(int valueLength, int maxLength)
{
    return query(WebLocalizedString::ValidationTooLong, String::number(valueLength), String::number(maxLength));
}

String validationMessageRangeUnderflowText(const String& minimum)
{
    return query(WebLocalizedString::ValidationRangeUnderflow, minimum);
}

String validationMessageRangeOverflowText(const String& maximum)
{
    return query(WebLocalizedString::ValidationRangeOverflow, maximum);
}

String validationMessageStepMismatchText(const String& base, const String& step)
{
    return query(WebLocalizedString::ValidationStepMismatch, base, step);
}

String validationMessageBadInputForNumberText()
{
    return query(WebLocalizedString::ValidationBadInputForNumber);
}

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
String validationMessageBadInputForDateTimeText()
{
    return query(WebLocalizedString::ValidationBadInputForDateTime);
}
#endif

} // namespace WebCore
