/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef LocalizedStrings_h
#define LocalizedStrings_h

namespace WebCore {

    class String;
    class IntSize;
    
    String inputElementAltText();
    String resetButtonDefaultLabel();
    String searchableIndexIntroduction();
    String submitButtonDefaultLabel();
    String fileButtonChooseFileLabel();
    String fileButtonNoFileSelectedLabel();
    String copyImageUnknownFileLabel();
    String contextMenuItemTagOpenLinkInNewWindow();
    String contextMenuItemTagDownloadLinkToDisk();
    String contextMenuItemTagCopyLinkToClipboard();
    String contextMenuItemTagOpenImageInNewWindow();
    String contextMenuItemTagDownloadImageToDisk();
    String contextMenuItemTagCopyImageToClipboard();
    String contextMenuItemTagOpenFrameInNewWindow();
    String contextMenuItemTagCopy();
    String contextMenuItemTagGoBack();
    String contextMenuItemTagGoForward();
    String contextMenuItemTagStop();
    String contextMenuItemTagReload();
    String contextMenuItemTagCut();
    String contextMenuItemTagPaste();
#if PLATFORM(GTK)
    String contextMenuItemTagDelete();
    String contextMenuItemTagSelectAll();
    String contextMenuItemTagInputMethods();
    String contextMenuItemTagUnicode();
#endif
    String contextMenuItemTagNoGuessesFound();
    String contextMenuItemTagIgnoreSpelling();
    String contextMenuItemTagLearnSpelling();
    String contextMenuItemTagSearchWeb();
    String contextMenuItemTagLookUpInDictionary();
    String contextMenuItemTagOpenLink();
    String contextMenuItemTagIgnoreGrammar();
    String contextMenuItemTagSpellingMenu();
    String contextMenuItemTagShowSpellingPanel(bool show);
    String contextMenuItemTagCheckSpelling();
    String contextMenuItemTagCheckSpellingWhileTyping();
    String contextMenuItemTagCheckGrammarWithSpelling();
    String contextMenuItemTagFontMenu();
    String contextMenuItemTagBold();
    String contextMenuItemTagItalic();
    String contextMenuItemTagUnderline();
    String contextMenuItemTagOutline();
    String contextMenuItemTagWritingDirectionMenu();
    String contextMenuItemTagTextDirectionMenu();
    String contextMenuItemTagDefaultDirection();
    String contextMenuItemTagLeftToRight();
    String contextMenuItemTagRightToLeft();
#if PLATFORM(MAC)
    String contextMenuItemTagSearchInSpotlight();
    String contextMenuItemTagShowFonts();
    String contextMenuItemTagStyles();
    String contextMenuItemTagShowColors();
    String contextMenuItemTagSpeechMenu();
    String contextMenuItemTagStartSpeaking();
    String contextMenuItemTagStopSpeaking();
    String contextMenuItemTagCorrectSpellingAutomatically();
    String contextMenuItemTagSubstitutionsMenu();
    String contextMenuItemTagShowSubstitutions(bool show);
    String contextMenuItemTagSmartCopyPaste();
    String contextMenuItemTagSmartQuotes();
    String contextMenuItemTagSmartDashes();
    String contextMenuItemTagSmartLinks();
    String contextMenuItemTagTextReplacement();
    String contextMenuItemTagTransformationsMenu();
    String contextMenuItemTagMakeUpperCase();
    String contextMenuItemTagMakeLowerCase();
    String contextMenuItemTagCapitalize();
    String contextMenuItemTagChangeBack(const String& replacedString);
#endif
    String contextMenuItemTagInspectElement();

    String searchMenuNoRecentSearchesText();
    String searchMenuRecentSearchesText();
    String searchMenuClearRecentSearchesText();

    String AXWebAreaText();
    String AXLinkText();
    String AXListMarkerText();
    String AXImageMapText();
    String AXHeadingText();
    String AXDefinitionListTermText();
    String AXDefinitionListDefinitionText();
    
    String AXButtonActionVerb();
    String AXRadioButtonActionVerb();
    String AXTextFieldActionVerb();
    String AXCheckedCheckBoxActionVerb();
    String AXUncheckedCheckBoxActionVerb();
    String AXLinkActionVerb();

    String multipleFileUploadText(unsigned numberOfFiles);
    String unknownFileSizeText();

#if PLATFORM(WIN)
    String uploadFileText();
    String allFilesText();
#endif

    String imageTitle(const String& filename, const IntSize& size);

    String mediaElementLoadingStateText();
    String mediaElementLiveBroadcastStateText();
}

#endif
