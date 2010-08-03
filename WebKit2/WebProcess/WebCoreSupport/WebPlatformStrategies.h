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

#ifndef WebPlatformStrategies_h
#define WebPlatformStrategies_h

#if USE(PLATFORM_STRATEGIES)

#include <WebCore/PlatformStrategies.h>
#include <WebCore/PluginStrategy.h>
#include <WebCore/LocalizationStrategy.h>
#include <WebCore/VisitedLinkStrategy.h>

namespace WebKit {

class WebPlatformStrategies : public WebCore::PlatformStrategies, private WebCore::PluginStrategy, private WebCore::LocalizationStrategy, private WebCore::VisitedLinkStrategy {
public:
    static void initialize();
    
private:
    WebPlatformStrategies();
    
    // WebCore::PlatformStrategies
    virtual WebCore::PluginStrategy* createPluginStrategy();
    virtual WebCore::LocalizationStrategy* createLocalizationStrategy();
    virtual WebCore::VisitedLinkStrategy* createVisitedLinkStrategy();

    // WebCore::PluginStrategy
    virtual void refreshPlugins();
    virtual void getPluginInfo(Vector<WebCore::PluginInfo>&);

    // WebCore::LocalizationStrategy    
    virtual WebCore::String inputElementAltText();
    virtual WebCore::String resetButtonDefaultLabel();
    virtual WebCore::String searchableIndexIntroduction();
    virtual WebCore::String submitButtonDefaultLabel();
    virtual WebCore::String fileButtonChooseFileLabel();
    virtual WebCore::String fileButtonNoFileSelectedLabel();
    virtual WebCore::String copyImageUnknownFileLabel();
#if ENABLE(CONTEXT_MENUS)
    virtual WebCore::String contextMenuItemTagOpenLinkInNewWindow();
    virtual WebCore::String contextMenuItemTagDownloadLinkToDisk();
    virtual WebCore::String contextMenuItemTagCopyLinkToClipboard();
    virtual WebCore::String contextMenuItemTagOpenImageInNewWindow();
    virtual WebCore::String contextMenuItemTagDownloadImageToDisk();
    virtual WebCore::String contextMenuItemTagCopyImageToClipboard();
    virtual WebCore::String contextMenuItemTagOpenFrameInNewWindow();
    virtual WebCore::String contextMenuItemTagCopy();
    virtual WebCore::String contextMenuItemTagGoBack();
    virtual WebCore::String contextMenuItemTagGoForward();
    virtual WebCore::String contextMenuItemTagStop();
    virtual WebCore::String contextMenuItemTagReload();
    virtual WebCore::String contextMenuItemTagCut();
    virtual WebCore::String contextMenuItemTagPaste();
#if PLATFORM(GTK)
    virtual WebCore::String contextMenuItemTagDelete();
    virtual WebCore::String contextMenuItemTagSelectAll();
    virtual WebCore::String contextMenuItemTagInputMethods();
    virtual WebCore::String contextMenuItemTagUnicode();
#endif
    virtual WebCore::String contextMenuItemTagNoGuessesFound();
    virtual WebCore::String contextMenuItemTagIgnoreSpelling();
    virtual WebCore::String contextMenuItemTagLearnSpelling();
    virtual WebCore::String contextMenuItemTagSearchWeb();
    virtual WebCore::String contextMenuItemTagLookUpInDictionary();
    virtual WebCore::String contextMenuItemTagOpenLink();
    virtual WebCore::String contextMenuItemTagIgnoreGrammar();
    virtual WebCore::String contextMenuItemTagSpellingMenu();
    virtual WebCore::String contextMenuItemTagShowSpellingPanel(bool show);
    virtual WebCore::String contextMenuItemTagCheckSpelling();
    virtual WebCore::String contextMenuItemTagCheckSpellingWhileTyping();
    virtual WebCore::String contextMenuItemTagCheckGrammarWithSpelling();
    virtual WebCore::String contextMenuItemTagFontMenu();
    virtual WebCore::String contextMenuItemTagBold();
    virtual WebCore::String contextMenuItemTagItalic();
    virtual WebCore::String contextMenuItemTagUnderline();
    virtual WebCore::String contextMenuItemTagOutline();
    virtual WebCore::String contextMenuItemTagWritingDirectionMenu();
    virtual WebCore::String contextMenuItemTagTextDirectionMenu();
    virtual WebCore::String contextMenuItemTagDefaultDirection();
    virtual WebCore::String contextMenuItemTagLeftToRight();
    virtual WebCore::String contextMenuItemTagRightToLeft();
#if PLATFORM(MAC)
    virtual WebCore::String contextMenuItemTagSearchInSpotlight();
    virtual WebCore::String contextMenuItemTagShowFonts();
    virtual WebCore::String contextMenuItemTagStyles();
    virtual WebCore::String contextMenuItemTagShowColors();
    virtual WebCore::String contextMenuItemTagSpeechMenu();
    virtual WebCore::String contextMenuItemTagStartSpeaking();
    virtual WebCore::String contextMenuItemTagStopSpeaking();
    virtual WebCore::String contextMenuItemTagCorrectSpellingAutomatically();
    virtual WebCore::String contextMenuItemTagSubstitutionsMenu();
    virtual WebCore::String contextMenuItemTagShowSubstitutions(bool show);
    virtual WebCore::String contextMenuItemTagSmartCopyPaste();
    virtual WebCore::String contextMenuItemTagSmartQuotes();
    virtual WebCore::String contextMenuItemTagSmartDashes();
    virtual WebCore::String contextMenuItemTagSmartLinks();
    virtual WebCore::String contextMenuItemTagTextReplacement();
    virtual WebCore::String contextMenuItemTagTransformationsMenu();
    virtual WebCore::String contextMenuItemTagMakeUpperCase();
    virtual WebCore::String contextMenuItemTagMakeLowerCase();
    virtual WebCore::String contextMenuItemTagCapitalize();
    virtual WebCore::String contextMenuItemTagChangeBack(const WebCore::String& replacedString);
#endif
    virtual WebCore::String contextMenuItemTagInspectElement();
#endif // ENABLE(CONTEXT_MENUS)
    virtual WebCore::String searchMenuNoRecentSearchesText();
    virtual WebCore::String searchMenuRecentSearchesText();
    virtual WebCore::String searchMenuClearRecentSearchesText();
    virtual WebCore::String AXWebAreaText();
    virtual WebCore::String AXLinkText();
    virtual WebCore::String AXListMarkerText();
    virtual WebCore::String AXImageMapText();
    virtual WebCore::String AXHeadingText();
    virtual WebCore::String AXDefinitionListTermText();
    virtual WebCore::String AXDefinitionListDefinitionText();
    virtual WebCore::String AXARIAContentGroupText(const WebCore::String& ariaType);
    virtual WebCore::String AXButtonActionVerb();
    virtual WebCore::String AXRadioButtonActionVerb();
    virtual WebCore::String AXTextFieldActionVerb();
    virtual WebCore::String AXCheckedCheckBoxActionVerb();
    virtual WebCore::String AXUncheckedCheckBoxActionVerb();
    virtual WebCore::String AXMenuListActionVerb();
    virtual WebCore::String AXMenuListPopupActionVerb();
    virtual WebCore::String AXLinkActionVerb();
    virtual WebCore::String missingPluginText();
    virtual WebCore::String crashedPluginText();
    virtual WebCore::String multipleFileUploadText(unsigned numberOfFiles);
    virtual WebCore::String unknownFileSizeText();
#if PLATFORM(WIN)
    virtual WebCore::String uploadFileText();
    virtual WebCore::String allFilesText();
#endif
    virtual WebCore::String imageTitle(const WebCore::String& filename, const WebCore::IntSize& size);
    virtual WebCore::String mediaElementLoadingStateText();
    virtual WebCore::String mediaElementLiveBroadcastStateText();
    virtual WebCore::String localizedMediaControlElementString(const WebCore::String&);
    virtual WebCore::String localizedMediaControlElementHelpText(const WebCore::String&);
    virtual WebCore::String localizedMediaTimeDescription(float);
    virtual WebCore::String validationMessageValueMissingText();
    virtual WebCore::String validationMessageTypeMismatchText();
    virtual WebCore::String validationMessagePatternMismatchText();
    virtual WebCore::String validationMessageTooLongText();
    virtual WebCore::String validationMessageRangeUnderflowText();
    virtual WebCore::String validationMessageRangeOverflowText();
    virtual WebCore::String validationMessageStepMismatchText();

    void populatePluginCache();

    bool m_pluginCacheIsPopulated;
    bool m_shouldRefreshPlugins;
    Vector<WebCore::PluginInfo> m_cachedPlugins;

    // WebCore::VisitedLinkStrategy
    virtual bool isLinkVisited(WebCore::Page*, WebCore::LinkHash);
    virtual void addVisitedLink(WebCore::Page*, WebCore::LinkHash);
};

} // namespace WebKit

#endif // USE(PLATFORM_STRATEGIES)

#endif // WebPlatformStrategies_h
