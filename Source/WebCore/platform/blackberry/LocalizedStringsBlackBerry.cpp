/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "LocalizedStrings.h"

#include "IntSize.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include <BlackBerryPlatformClient.h>
#include <LocalizeResource.h>
#include <wtf/Vector.h>

namespace WebCore {

DEFINE_STATIC_LOCAL(BlackBerry::Platform::LocalizeResource, s_resource, ());

String fileButtonChooseFileLabel()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::FILE_CHOOSE_BUTTON_LABEL));
}

String fileButtonChooseMultipleFilesLabel()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::FILE_CHOOSE_MULTIPLE_BUTTON_LABEL));
}

String fileButtonNoFileSelectedLabel()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::FILE_BUTTON_NO_FILE_SELECTED_LABEL));
}

String resetButtonDefaultLabel()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::RESET_BUTTON_LABEL));
}

String submitButtonDefaultLabel()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::SUBMIT_BUTTON_LABEL));
}

String inputElementAltText()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::SUBMIT_BUTTON_LABEL));
}

static String platformLanguage()
{
    String lang = BlackBerry::Platform::Client::get()->getLocale().c_str();
    // getLocale() returns a POSIX locale which uses '_' to separate language and country.
    // However, we use '-' instead of '_' in WebCore (e.g. en_us should read en-us)
    size_t underscorePosition = lang.find('_');
    String replaceWith = "-";
    if (underscorePosition != notFound)
        return lang.replace(underscorePosition, replaceWith.length(), replaceWith);
    return lang;
}

Vector<String> platformUserPreferredLanguages()
{
    Vector<String> userPreferredLanguages;
    userPreferredLanguages.append(platformLanguage());
    return userPreferredLanguages;
}

#if ENABLE(CONTEXT_MENUS)
String contextMenuItemTagBold()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCheckSpelling()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCopyImageToClipboard()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCopyLinkToClipboard()
{
    notImplemented();
    return String();
}

String contextMenuItemTagDefaultDirection()
{
    notImplemented();
    return String();
}

String contextMenuItemTagDownloadImageToDisk()
{
    notImplemented();
    return String();
}

String contextMenuItemTagDownloadLinkToDisk()
{
    notImplemented();
    return String();
}

String contextMenuItemTagFontMenu()
{
    notImplemented();
    return String();
}

String contextMenuItemTagIgnoreGrammar()
{
    notImplemented();
    return String();
}

String contextMenuItemTagIgnoreSpelling()
{
    notImplemented();
    return String();
}

String contextMenuItemTagInspectElement()
{
    notImplemented();
    return String();
}

String contextMenuItemTagItalic()
{
    notImplemented();
    return String();
}

String contextMenuItemTagLearnSpelling()
{
    notImplemented();
    return String();
}

String contextMenuItemTagLeftToRight()
{
    notImplemented();
    return String();
}

String contextMenuItemTagNoGuessesFound()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenImageInNewWindow()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenLinkInNewWindow()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenLink()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOutline()
{
    notImplemented();
    return String();
}

String contextMenuItemTagReload()
{
    notImplemented();
    return String();
}

String contextMenuItemTagRightToLeft()
{
    notImplemented();
    return String();
}

String contextMenuItemTagSearchWeb()
{
    notImplemented();
    return String();
}

String contextMenuItemTagShowSpellingPanel(bool)
{
    notImplemented();
    return String();
}

String contextMenuItemTagSpellingMenu()
{
    notImplemented();
    return String();
}

String contextMenuItemTagTextDirectionMenu()
{
    notImplemented();
    return String();
}

String contextMenuItemTagUnderline()
{
    notImplemented();
    return String();
}

String contextMenuItemTagWritingDirectionMenu()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCopyVideoLinkToClipboard()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenVideoInNewWindow()
{
    notImplemented();
    return String();
}

String contextMenuItemTagToggleMediaControls()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_MEDIA_TOGGLE_CONTROLS));
}

String contextMenuItemTagToggleMediaLoop()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_MEDIA_TOGGLE_LOOP));
}

String contextMenuItemTagEnterVideoFullscreen()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_VIDEO_FULLSCREEN));
}

String contextMenuItemTagMediaPlay()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_MEDIA_PLAY));
}

String contextMenuItemTagMediaPause()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_MEDIA_PAUSE));
}

String contextMenuItemTagMediaMute()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_MEDIA_MUTE));
}

String contextMenuItemTagCopyAudioLinkToClipboard()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenAudioInNewWindow()
{
    notImplemented();
    return String();
}

String contextMenuItemTagGoBack()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_GOBACK));
}

String contextMenuItemTagGoForward()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_GOFORWARD));
}

String contextMenuItemTagStop()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_STOP));
}

String contextMenuItemTagCopy()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_COPY));
}

String contextMenuItemTagCut()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_CUT));
}

String contextMenuItemTagPaste()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::CONTEXT_PASTE));
}

#endif

String searchableIndexIntroduction()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::SEARCHABLE_INDEX_INTRODUCTION));
}

String searchMenuNoRecentSearchesText()
{
    notImplemented();
    return String();
}

String searchMenuRecentSearchesText()
{
    notImplemented();
    return String();
}

String searchMenuClearRecentSearchesText()
{
    notImplemented();
    return String();
}

String imageTitle(String const& filename, IntSize const& size)
{
    return filename + " (" + String::number(size.width()) + "x" + String::number(size.height()) + ")";
}

String AXButtonActionVerb()
{
    notImplemented();
    return String();
}

String AXCheckedCheckBoxActionVerb()
{
    notImplemented();
    return String();
}

String AXDefinitionListDefinitionText()
{
    notImplemented();
    return String();
}

String AXDefinitionListTermText()
{
    notImplemented();
    return String();
}

String AXFooterRoleDescriptionText()
{
    notImplemented();
    return String();
}

String AXLinkActionVerb()
{
    notImplemented();
    return String();
}

String AXRadioButtonActionVerb()
{
    notImplemented();
    return String();
}

String AXTextFieldActionVerb()
{
    notImplemented();
    return String();
}

String AXUncheckedCheckBoxActionVerb()
{
    notImplemented();
    return String();
}

String AXMenuListPopupActionVerb()
{
    notImplemented();
    return String();
}

String AXMenuListActionVerb()
{
    notImplemented();
    return String();
}

String unknownFileSizeText()
{
    notImplemented();
    return String();
}

String validationMessagePatternMismatchText()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::VALIDATION_PATTERN_MISMATCH));
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
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::VALIDATION_STEP_MISMATCH));
}

String validationMessageTypeMismatchText()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::VALIDATION_TYPE_MISMATCH));
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

String validationMessageValueMissingText()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::VALIDATION_VALUE_MISSING));
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

String localizedMediaControlElementString(const String&)
{
    notImplemented();
    return String();
}

String localizedMediaControlElementHelpText(const String&)
{
    notImplemented();
    return String();
}

String localizedMediaTimeDescription(const String&)
{
    notImplemented();
    return String();
}

String localizedMediaTimeDescription(float)
{
    notImplemented();
    return String();
}

String mediaElementLoadingStateText()
{
    notImplemented();
    return String();
}

String mediaElementLiveBroadcastStateText()
{
    notImplemented();
    return String();
}

String missingPluginText()
{
    notImplemented();
    return String();
}

String crashedPluginText()
{
    notImplemented();
    return String();
}

String multipleFileUploadText(unsigned)
{
    return String(", ...");
}

String defaultDetailsSummaryText()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::DETAILS_SUMMARY));
}

String fileButtonNoFilesSelectedLabel()
{
    return String::fromUTF8(s_resource.getString(BlackBerry::Platform::FILE_BUTTON_NO_FILE_SELECTED_LABEL));
}

} // namespace WebCore
