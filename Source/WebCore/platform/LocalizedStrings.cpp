/*
 * Copyright (C) 2003, 2006, 2009, 2010 Apple Inc. All rights reserved.
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

#include "DefaultLocalizationStrategy.h"
#include "IntSize.h"
#include "PlatformStrategies.h"
#include "PlatformString.h"

namespace WebCore {

#if USE(PLATFORM_STRATEGIES)

static inline LocalizationStrategy* localizationStrategy()
{
    if (hasPlatformStrategies())
        return platformStrategies()->localizationStrategy();

    return &DefaultLocalizationStrategy::shared();
}

String inputElementAltText()
{
    return localizationStrategy()->inputElementAltText();
}

String resetButtonDefaultLabel()
{
    return localizationStrategy()->resetButtonDefaultLabel();
}

String searchableIndexIntroduction()
{
    return localizationStrategy()->searchableIndexIntroduction();
}

String submitButtonDefaultLabel()
{
    return localizationStrategy()->submitButtonDefaultLabel();
}

String fileButtonChooseFileLabel()
{
    return localizationStrategy()->fileButtonChooseFileLabel();
}

String fileButtonNoFileSelectedLabel()
{
    return localizationStrategy()->fileButtonNoFileSelectedLabel();
}

String defaultDetailsSummaryText()
{
    return localizationStrategy()->defaultDetailsSummaryText();
}

#if PLATFORM(MAC)
String copyImageUnknownFileLabel()
{
    return localizationStrategy()->copyImageUnknownFileLabel();
}
#endif

#if ENABLE(CONTEXT_MENUS)
String contextMenuItemTagOpenLinkInNewWindow()
{
    return localizationStrategy()->contextMenuItemTagOpenLinkInNewWindow();
}

String contextMenuItemTagDownloadLinkToDisk()
{
    return localizationStrategy()->contextMenuItemTagDownloadLinkToDisk();
}

String contextMenuItemTagCopyLinkToClipboard()
{
    return localizationStrategy()->contextMenuItemTagCopyLinkToClipboard();
}

String contextMenuItemTagOpenImageInNewWindow()
{
    return localizationStrategy()->contextMenuItemTagOpenImageInNewWindow();
}

String contextMenuItemTagDownloadImageToDisk()
{
    return localizationStrategy()->contextMenuItemTagDownloadImageToDisk();
}

String contextMenuItemTagCopyImageToClipboard()
{
    return localizationStrategy()->contextMenuItemTagCopyImageToClipboard();
}

#if PLATFORM(QT) || PLATFORM(GTK)
String contextMenuItemTagCopyImageUrlToClipboard()
{
    return localizationStrategy()->contextMenuItemTagCopyImageUrlToClipboard();
}
#endif

String contextMenuItemTagOpenFrameInNewWindow()
{
    return localizationStrategy()->contextMenuItemTagOpenFrameInNewWindow();
}

String contextMenuItemTagCopy()
{
    return localizationStrategy()->contextMenuItemTagCopy();
}

String contextMenuItemTagGoBack()
{
    return localizationStrategy()->contextMenuItemTagGoBack();
}

String contextMenuItemTagGoForward()
{
    return localizationStrategy()->contextMenuItemTagGoForward();
}

String contextMenuItemTagStop()
{
    return localizationStrategy()->contextMenuItemTagStop();
}

String contextMenuItemTagReload()
{
    return localizationStrategy()->contextMenuItemTagReload();
}

String contextMenuItemTagCut()
{
    return localizationStrategy()->contextMenuItemTagCut();
}

String contextMenuItemTagPaste()
{
    return localizationStrategy()->contextMenuItemTagPaste();
}

#if PLATFORM(QT)
String contextMenuItemTagSelectAll()
{
    return localizationStrategy()->contextMenuItemTagSelectAll();
}
#endif

String contextMenuItemTagNoGuessesFound()
{
    return localizationStrategy()->contextMenuItemTagNoGuessesFound();
}

String contextMenuItemTagIgnoreSpelling()
{
    return localizationStrategy()->contextMenuItemTagIgnoreSpelling();
}

String contextMenuItemTagLearnSpelling()
{
    return localizationStrategy()->contextMenuItemTagLearnSpelling();
}

#if PLATFORM(MAC)
String contextMenuItemTagSearchInSpotlight()
{
    return localizationStrategy()->contextMenuItemTagSearchInSpotlight();
}
#endif

String contextMenuItemTagSearchWeb()
{
    return localizationStrategy()->contextMenuItemTagSearchWeb();
}

String contextMenuItemTagLookUpInDictionary(const String& selectedString)
{
    return localizationStrategy()->contextMenuItemTagLookUpInDictionary(selectedString);
}

String contextMenuItemTagOpenLink()
{
    return localizationStrategy()->contextMenuItemTagOpenLink();
}

String contextMenuItemTagIgnoreGrammar()
{
    return localizationStrategy()->contextMenuItemTagIgnoreGrammar();
}

String contextMenuItemTagSpellingMenu()
{
    return localizationStrategy()->contextMenuItemTagSpellingMenu();
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    return localizationStrategy()->contextMenuItemTagShowSpellingPanel(show);
}

String contextMenuItemTagCheckSpelling()
{
    return localizationStrategy()->contextMenuItemTagCheckSpelling();
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    return localizationStrategy()->contextMenuItemTagCheckSpellingWhileTyping();
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    return localizationStrategy()->contextMenuItemTagCheckGrammarWithSpelling();
}

String contextMenuItemTagFontMenu()
{
    return localizationStrategy()->contextMenuItemTagFontMenu();
}

#if PLATFORM(MAC)
String contextMenuItemTagShowFonts()
{
    return localizationStrategy()->contextMenuItemTagShowFonts();
}
#endif

String contextMenuItemTagBold()
{
    return localizationStrategy()->contextMenuItemTagBold();
}

String contextMenuItemTagItalic()
{
    return localizationStrategy()->contextMenuItemTagItalic();
}

String contextMenuItemTagUnderline()
{
    return localizationStrategy()->contextMenuItemTagUnderline();
}

String contextMenuItemTagOutline()
{
    return localizationStrategy()->contextMenuItemTagOutline();
}

#if PLATFORM(MAC)
String contextMenuItemTagStyles()
{
    return localizationStrategy()->contextMenuItemTagStyles();
}

String contextMenuItemTagShowColors()
{
    return localizationStrategy()->contextMenuItemTagShowColors();
}

String contextMenuItemTagSpeechMenu()
{
    return localizationStrategy()->contextMenuItemTagSpeechMenu();
}

String contextMenuItemTagStartSpeaking()
{
    return localizationStrategy()->contextMenuItemTagStartSpeaking();
}

String contextMenuItemTagStopSpeaking()
{
    return localizationStrategy()->contextMenuItemTagStopSpeaking();
}
#endif

String contextMenuItemTagWritingDirectionMenu()
{
    return localizationStrategy()->contextMenuItemTagWritingDirectionMenu();
}

String contextMenuItemTagTextDirectionMenu()
{
    return localizationStrategy()->contextMenuItemTagTextDirectionMenu();
}

String contextMenuItemTagDefaultDirection()
{
    return localizationStrategy()->contextMenuItemTagDefaultDirection();
}

String contextMenuItemTagLeftToRight()
{
    return localizationStrategy()->contextMenuItemTagLeftToRight();
}

String contextMenuItemTagRightToLeft()
{
    return localizationStrategy()->contextMenuItemTagRightToLeft();
}

#if PLATFORM(MAC)

String contextMenuItemTagCorrectSpellingAutomatically()
{
    return localizationStrategy()->contextMenuItemTagCorrectSpellingAutomatically();
}

String contextMenuItemTagSubstitutionsMenu()
{
    return localizationStrategy()->contextMenuItemTagSubstitutionsMenu();
}

String contextMenuItemTagShowSubstitutions(bool show)
{
    return localizationStrategy()->contextMenuItemTagShowSubstitutions(show);
}

String contextMenuItemTagSmartCopyPaste()
{
    return localizationStrategy()->contextMenuItemTagSmartCopyPaste();
}

String contextMenuItemTagSmartQuotes()
{
    return localizationStrategy()->contextMenuItemTagSmartQuotes();
}

String contextMenuItemTagSmartDashes()
{
    return localizationStrategy()->contextMenuItemTagSmartDashes();
}

String contextMenuItemTagSmartLinks()
{
    return localizationStrategy()->contextMenuItemTagSmartLinks();
}

String contextMenuItemTagTextReplacement()
{
    return localizationStrategy()->contextMenuItemTagTextReplacement();
}

String contextMenuItemTagTransformationsMenu()
{
    return localizationStrategy()->contextMenuItemTagTransformationsMenu();
}

String contextMenuItemTagMakeUpperCase()
{
    return localizationStrategy()->contextMenuItemTagMakeUpperCase();
}

String contextMenuItemTagMakeLowerCase()
{
    return localizationStrategy()->contextMenuItemTagMakeLowerCase();
}

String contextMenuItemTagCapitalize()
{
    return localizationStrategy()->contextMenuItemTagCapitalize();
}

String contextMenuItemTagChangeBack(const String& replacedString)
{
    return localizationStrategy()->contextMenuItemTagChangeBack(replacedString);
}

#endif // PLATFORM(MAC)

String contextMenuItemTagOpenVideoInNewWindow()
{
    return localizationStrategy()->contextMenuItemTagOpenVideoInNewWindow();
}

String contextMenuItemTagOpenAudioInNewWindow()
{
    return localizationStrategy()->contextMenuItemTagOpenAudioInNewWindow();
}

String contextMenuItemTagCopyVideoLinkToClipboard()
{
    return localizationStrategy()->contextMenuItemTagCopyVideoLinkToClipboard();
}

String contextMenuItemTagCopyAudioLinkToClipboard()
{
    return localizationStrategy()->contextMenuItemTagCopyAudioLinkToClipboard();
}

String contextMenuItemTagToggleMediaControls()
{
    return localizationStrategy()->contextMenuItemTagToggleMediaControls();
}

String contextMenuItemTagToggleMediaLoop()
{
    return localizationStrategy()->contextMenuItemTagToggleMediaLoop();
}

String contextMenuItemTagEnterVideoFullscreen()
{
    return localizationStrategy()->contextMenuItemTagEnterVideoFullscreen();
}

String contextMenuItemTagMediaPlay()
{
    return localizationStrategy()->contextMenuItemTagMediaPlay();
}

String contextMenuItemTagMediaPause()
{
    return localizationStrategy()->contextMenuItemTagMediaPause();
}

String contextMenuItemTagMediaMute()
{
    return localizationStrategy()->contextMenuItemTagMediaMute();
}
    
String contextMenuItemTagInspectElement()
{
    return localizationStrategy()->contextMenuItemTagInspectElement();
}

#endif // ENABLE(CONTEXT_MENUS)

String searchMenuNoRecentSearchesText()
{
    return localizationStrategy()->searchMenuNoRecentSearchesText();
}

String searchMenuRecentSearchesText ()
{
    return localizationStrategy()->searchMenuRecentSearchesText ();
}

String searchMenuClearRecentSearchesText()
{
    return localizationStrategy()->searchMenuClearRecentSearchesText();
}

String AXWebAreaText()
{
    return localizationStrategy()->AXWebAreaText();
}

String AXLinkText()
{
    return localizationStrategy()->AXLinkText();
}

String AXListMarkerText()
{
    return localizationStrategy()->AXListMarkerText();
}

String AXImageMapText()
{
    return localizationStrategy()->AXImageMapText();
}

String AXHeadingText()
{
    return localizationStrategy()->AXHeadingText();
}

String AXDefinitionListTermText()
{
    return localizationStrategy()->AXDefinitionListTermText();
}

String AXDefinitionListDefinitionText()
{
    return localizationStrategy()->AXDefinitionListDefinitionText();
}

#if PLATFORM(MAC)
String AXARIAContentGroupText(const String& ariaType)
{
    return localizationStrategy()->AXARIAContentGroupText(ariaType);
}
#endif
    
String AXButtonActionVerb()
{
    return localizationStrategy()->AXButtonActionVerb();
}

String AXRadioButtonActionVerb()
{
    return localizationStrategy()->AXRadioButtonActionVerb();
}

String AXTextFieldActionVerb()
{
    return localizationStrategy()->AXTextFieldActionVerb();
}

String AXCheckedCheckBoxActionVerb()
{
    return localizationStrategy()->AXCheckedCheckBoxActionVerb();
}

String AXUncheckedCheckBoxActionVerb()
{
    return localizationStrategy()->AXUncheckedCheckBoxActionVerb();
}

String AXLinkActionVerb()
{
    return localizationStrategy()->AXLinkActionVerb();
}

String AXMenuListPopupActionVerb()
{
    return localizationStrategy()->AXMenuListPopupActionVerb();
}

String AXMenuListActionVerb()
{
    return localizationStrategy()->AXMenuListActionVerb();
}

String missingPluginText()
{
    return localizationStrategy()->missingPluginText();
}

String crashedPluginText()
{
    return localizationStrategy()->crashedPluginText();
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    return localizationStrategy()->multipleFileUploadText(numberOfFiles);
}

String unknownFileSizeText()
{
    return localizationStrategy()->unknownFileSizeText();
}

#if PLATFORM(WIN)
String uploadFileText()
{
    return localizationStrategy()->uploadFileText();
}

String allFilesText()
{
    return localizationStrategy()->allFilesText();
}
#endif

#if PLATFORM(MAC)
String keygenMenuItem512()
{
    return localizationStrategy()->keygenMenuItem512();
}

String keygenMenuItem1024()
{
    return localizationStrategy()->keygenMenuItem1024();
}

String keygenMenuItem2048()
{
    return localizationStrategy()->keygenMenuItem2048();
}

String keygenKeychainItemName(const String& host)
{
    return localizationStrategy()->keygenKeychainItemName(host);
}

#endif

String imageTitle(const String& filename, const IntSize& size)
{
    return localizationStrategy()->imageTitle(filename, size);
}

String mediaElementLoadingStateText()
{
    return localizationStrategy()->mediaElementLoadingStateText();
}

String mediaElementLiveBroadcastStateText()
{
    return localizationStrategy()->mediaElementLiveBroadcastStateText();
}

String localizedMediaControlElementString(const String& controlName)
{
    return localizationStrategy()->localizedMediaControlElementString(controlName);
}

String localizedMediaControlElementHelpText(const String& controlName)
{
    return localizationStrategy()->localizedMediaControlElementHelpText(controlName);
}

String localizedMediaTimeDescription(float time)
{
    return localizationStrategy()->localizedMediaTimeDescription(time);
}

String validationMessageValueMissingText()
{
    return localizationStrategy()->validationMessageValueMissingText();
}

String validationMessageValueMissingForCheckboxText()
{
    return localizationStrategy()->validationMessageValueMissingText();
}

String validationMessageValueMissingForFileText()
{
    return localizationStrategy()->validationMessageValueMissingText();
}

String validationMessageValueMissingForMultipleFileText()
{
    return localizationStrategy()->validationMessageValueMissingText();
}

String validationMessageValueMissingForRadioText()
{
    return localizationStrategy()->validationMessageValueMissingText();
}

String validationMessageValueMissingForSelectText()
{
    return localizationStrategy()->validationMessageValueMissingText();
}

String validationMessageTypeMismatchText()
{
    return localizationStrategy()->validationMessageTypeMismatchText();
}

String validationMessageTypeMismatchForEmailText()
{
    return localizationStrategy()->validationMessageTypeMismatchText();
}

String validationMessageTypeMismatchForMultipleEmailText()
{
    return localizationStrategy()->validationMessageTypeMismatchText();
}

String validationMessageTypeMismatchForURLText()
{
    return localizationStrategy()->validationMessageTypeMismatchText();
}

String validationMessagePatternMismatchText()
{
    return localizationStrategy()->validationMessagePatternMismatchText();
}

String validationMessageTooLongText(int, int)
{
    return localizationStrategy()->validationMessageTooLongText();
}

String validationMessageRangeUnderflowText(const String&)
{
    return localizationStrategy()->validationMessageRangeUnderflowText();
}

String validationMessageRangeOverflowText(const String&)
{
    return localizationStrategy()->validationMessageRangeOverflowText();
}

String validationMessageStepMismatchText(const String&, const String&)
{
    return localizationStrategy()->validationMessageStepMismatchText();
}

#endif // USE(PLATFORM_STRATEGIES)

#if !PLATFORM(MAC) && !PLATFORM(WIN)
String localizedString(const char* key)
{
    return String::fromUTF8(key, strlen(key));
}
#endif

} // namespace WebCore
