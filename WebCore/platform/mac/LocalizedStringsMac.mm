/*
 * Copyright (C) 2003, 2006, 2009 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "LocalizedStrings.h"

#import "BlockExceptions.h"
#import "IntSize.h"
#import "PlatformString.h"
#import "WebCoreViewFactory.h"

namespace WebCore {

String inputElementAltText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] inputElementAltText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String resetButtonDefaultLabel()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] resetButtonDefaultLabel];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String searchableIndexIntroduction()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] searchableIndexIntroduction];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String submitButtonDefaultLabel()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] submitButtonDefaultLabel];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String fileButtonChooseFileLabel()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] fileButtonChooseFileLabel];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String fileButtonNoFileSelectedLabel()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] fileButtonNoFileSelectedLabel];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String copyImageUnknownFileLabel()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] copyImageUnknownFileLabel];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

#if ENABLE(CONTEXT_MENUS)
String contextMenuItemTagOpenLinkInNewWindow()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagOpenLinkInNewWindow];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagDownloadLinkToDisk()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagDownloadLinkToDisk];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagCopyLinkToClipboard()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagCopyLinkToClipboard];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagOpenImageInNewWindow()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagOpenImageInNewWindow];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagDownloadImageToDisk()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagDownloadImageToDisk];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagCopyImageToClipboard()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagCopyImageToClipboard];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagOpenFrameInNewWindow];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagCopy()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagCopy];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagGoBack()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagGoBack];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagGoForward()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagGoForward];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagStop()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagStop];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagReload()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagReload];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagCut()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagCut];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagPaste()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagPaste];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagNoGuessesFound()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagNoGuessesFound];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagIgnoreSpelling()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagIgnoreSpelling];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagLearnSpelling()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagLearnSpelling];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagSearchInSpotlight()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagSearchInSpotlight];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagSearchWeb()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagSearchWeb];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagLookUpInDictionary()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagLookUpInDictionary];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagOpenLink()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagOpenLink];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagIgnoreGrammar()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagIgnoreGrammar];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagSpellingMenu()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagSpellingMenu];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagShowSpellingPanel:show];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagCheckSpelling()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagCheckSpelling];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagCheckSpellingWhileTyping];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagCheckGrammarWithSpelling];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagFontMenu()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagFontMenu];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagShowFonts()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagShowFonts];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagBold()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagBold];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagItalic()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagItalic];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagUnderline()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagUnderline];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagOutline()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagOutline];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagStyles()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagStyles];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagShowColors()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagShowColors];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagSpeechMenu()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagSpeechMenu];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagStartSpeaking()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagStartSpeaking];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagStopSpeaking()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagStopSpeaking];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagWritingDirectionMenu()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagWritingDirectionMenu];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagTextDirectionMenu()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagTextDirectionMenu];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagDefaultDirection()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagDefaultDirection];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagLeftToRight()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagLeftToRight];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagRightToLeft()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagRightToLeft];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagCorrectSpellingAutomatically()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagCorrectSpellingAutomatically];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagSubstitutionsMenu()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagSubstitutionsMenu];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagShowSubstitutions(bool show)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagShowSubstitutions:show];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagSmartCopyPaste()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagSmartCopyPaste];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagSmartQuotes()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagSmartQuotes];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagSmartDashes()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagSmartDashes];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagSmartLinks()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagSmartLinks];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagTextReplacement()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagTextReplacement];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagTransformationsMenu()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagTransformationsMenu];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagMakeUpperCase()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagMakeUpperCase];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagMakeLowerCase()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagMakeLowerCase];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagCapitalize()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagCapitalize];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String contextMenuItemTagChangeBack(const String& replacedString)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagChangeBack:replacedString];
    END_BLOCK_OBJC_EXCEPTIONS;
    return replacedString;
}
    
String contextMenuItemTagInspectElement()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] contextMenuItemTagInspectElement];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}
#endif // ENABLE(CONTEXT_MENUS)

String searchMenuNoRecentSearchesText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] searchMenuNoRecentSearchesText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String searchMenuRecentSearchesText ()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] searchMenuRecentSearchesText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String searchMenuClearRecentSearchesText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] searchMenuClearRecentSearchesText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXWebAreaText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXWebAreaText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXLinkText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXLinkText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXListMarkerText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXListMarkerText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXImageMapText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXImageMapText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXHeadingText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXHeadingText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXDefinitionListTermText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXDefinitionListTermText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}
    
String AXDefinitionListDefinitionText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXDefinitionListDefinitionText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}
    
String AXARIAContentGroupText(const String& ariaType)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXARIAContentGroupText:ariaType];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();        
}
    
String AXButtonActionVerb()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXButtonActionVerb];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXRadioButtonActionVerb()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXRadioButtonActionVerb];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXTextFieldActionVerb()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXTextFieldActionVerb];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXCheckedCheckBoxActionVerb()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXCheckedCheckBoxActionVerb];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXUncheckedCheckBoxActionVerb()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXUncheckedCheckBoxActionVerb];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXLinkActionVerb()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXLinkActionVerb];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXMenuListPopupActionVerb()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXMenuListPopupActionVerb];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String AXMenuListActionVerb()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] AXMenuListActionVerb];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String missingPluginText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] missingPluginText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String crashedPluginText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] crashedPluginText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] multipleFileUploadTextForNumberOfFiles:numberOfFiles];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String unknownFileSizeText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] unknownFileSizeText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String imageTitle(const String& filename, const IntSize& size)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] imageTitleForFilename:filename width:size.width() height:size.height()];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String mediaElementLoadingStateText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] mediaElementLoadingStateText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String mediaElementLiveBroadcastStateText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] mediaElementLiveBroadcastStateText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String localizedMediaControlElementString(const String& controlName)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] localizedMediaControlElementString:controlName];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String localizedMediaControlElementHelpText(const String& controlName)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] localizedMediaControlElementHelpText:controlName];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String localizedMediaTimeDescription(float time)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] localizedMediaTimeDescription:time];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String validationMessageValueMissingText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] validationMessageValueMissingText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String validationMessageTypeMismatchText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] validationMessageTypeMismatchText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String validationMessagePatternMismatchText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] validationMessagePatternMismatchText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String validationMessageTooLongText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] validationMessageTooLongText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String validationMessageRangeUnderflowText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] validationMessageRangeUnderflowText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String validationMessageRangeOverflowText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] validationMessageRangeOverflowText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

String validationMessageStepMismatchText()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] validationMessageStepMismatchText];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

}
