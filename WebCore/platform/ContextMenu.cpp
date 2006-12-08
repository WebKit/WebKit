/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "ContextMenu.h"

#include "ContextMenuController.h"
#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "Node.h"
#include "Page.h"
#include "ResourceRequest.h"
#include "SelectionController.h"

namespace WebCore {

ContextMenuController* ContextMenu::controller() const
{
    if (Node* node = m_hitTestResult.innerNonSharedNode())
        if (Frame* frame = node->document()->frame())
            if (Page* page = frame->page())
                return page->contextMenuController();
    return 0;
}

static void createFontSubMenu(const HitTestResult& result, ContextMenuItem& fontMenuItem)
{
    ContextMenuItem showFonts(ActionType, ContextMenuItemTagShowFonts, "Show Fonts");
    ContextMenuItem bold(ActionType, ContextMenuItemTagBold, "Bold");
    ContextMenuItem italic(ActionType, ContextMenuItemTagItalic, "Italic");
    ContextMenuItem underline(ActionType, ContextMenuItemTagUnderline, "Underline");
    ContextMenuItem outline(ActionType, ContextMenuItemTagOutline, "Outline");
    ContextMenuItem styles(ActionType, ContextMenuItemTagStyles, "Styles...");
    ContextMenuItem separator(SeparatorType, ContextMenuItemTagNoAction, String());
    ContextMenuItem showColors(ActionType, ContextMenuItemTagShowColors, "Show Colors");

    ContextMenu* fontMenu = new ContextMenu(result);   
    fontMenu->appendItem(showFonts);
    fontMenu->appendItem(bold);
    fontMenu->appendItem(italic);
    fontMenu->appendItem(underline);
    fontMenu->appendItem(outline);
    fontMenu->appendItem(styles);
    fontMenu->appendItem(separator);
    fontMenu->appendItem(showColors);

    fontMenuItem.setSubMenu(fontMenu);
}

#ifndef BUILDING_ON_TIGER
static void createSpellingAndGrammarSubMenu(const HitTestResult& result, ContextMenuItem& spellingAndGrammarMenuItem)
{
    ContextMenuItem show(ActionType, ContextMenuItemTagShowSpellingAndGrammar, "Show Spelling and Grammar");
    ContextMenuItem checkNow(ActionType, ContextMenuItemTagCheckDocumentNow, "Check Document Now");
    ContextMenuItem checkWhileTyping(ActionType, ContextMenuItemTagCheckSpellingWhileTyping, "Check Spelling While Typing");
    ContextMenuItem grammarWithSpelling(ActionType, ContextMenuItemTagCheckGrammarWithSpelling, "Check Grammar With Spelling");

    ContextMenu* spellingAndGrammarMenu = new ContextMenu(result);
    spellingAndGrammarMenu->appendItem(show);
    spellingAndGrammarMenu->appendItem(checkNow);
    spellingAndGrammarMenu->appendItem(checkWhileTyping);
    spellingAndGrammarMenu->appendItem(grammarWithSpelling);

    spellingAndGrammarMenuItem.setSubMenu(spellingAndGrammarMenu);
}
#else
static void createSpellingSubMenu(const HitTestResult& result, const ContextMenuItem& spellingMenuItem)
{
    ContextMenuItem spelling(ActionType, ContextMenuItemTagSpellingMenuItem, "Spelling...");
    ContextMenuItem checkSpelling(ActionType, ContextMenuItemTagCheckSpelling, "Check Spelling");
    ContextMenuItem checkAsYouType(ActionType, ContextMenuItemTagCheckSpellingWhileTyping, "Check Spelling as You Type");

    ContextMenu* spellingMenu = new ContextMenu(result);
    spellingMenu->appendItem(spelling);
    spellingMenu->appendItem(checkSpelling);
    spellingMenu->appendItem(checkAsYouType);

    spellingMenuItem.setSubMenu(spellingMenu);
}
#endif

#if PLATFORM(MAC)
static void createSpeechSubMenu(const HitTestResult& result, ContextMenuItem& speechMenuItem)
{
    ContextMenuItem start(ActionType, ContextMenuItemTagStartSpeaking, "Start Speaking");
    ContextMenuItem stop(ActionType, ContextMenuItemTagStartSpeaking, "Stop Speaking");

    ContextMenu* speechMenu = new ContextMenu(result);
    speechMenu->appendItem(start);
    speechMenu->appendItem(stop);

    speechMenuItem.setSubMenu(speechMenu);
}
#endif

static void createWritingDirectionSubMenu(const HitTestResult& result, ContextMenuItem& writingDirectionMenuItem)
{
    ContextMenuItem defaultItem(ActionType, ContextMenuItemTagDefaultDirection, "Default");
    ContextMenuItem ltr(ActionType, ContextMenuItemTagLeftToRight, "Left to Right");
    ContextMenuItem rtl(ActionType, ContextMenuItemTagRightToLeft, "Right to Left");

    ContextMenu* writingDirectionMenu = new ContextMenu(result);
    writingDirectionMenu->appendItem(defaultItem);
    writingDirectionMenu->appendItem(ltr);
    writingDirectionMenu->appendItem(rtl);

    writingDirectionMenuItem.setSubMenu(writingDirectionMenu);
}

void ContextMenu::populate()
{
    ContextMenuItem SeparatorItem(SeparatorType, ContextMenuItemTagNoAction, String());
    ContextMenuItem OpenLinkItem(ActionType, ContextMenuItemTagOpenLink, "Open Link");
    ContextMenuItem OpenLinkInNewWindowItem(ActionType, ContextMenuItemTagOpenLinkInNewWindow, "Open Link in New Window");
    ContextMenuItem DownloadFileItem(ActionType, ContextMenuItemTagDownloadLinkToDisk, "Download Linked File");
    ContextMenuItem CopyLinkItem(ActionType, ContextMenuItemTagCopyLinkToClipboard, "Copy Link");
    ContextMenuItem OpenImageInNewWindowItem(ActionType, ContextMenuItemTagOpenImageInNewWindow, "Open Image in New Window");
    ContextMenuItem DownloadImageItem(ActionType, ContextMenuItemTagDownloadImageToDisk, "Download Image");
    ContextMenuItem CopyImageItem(ActionType, ContextMenuItemTagCopyImageToClipboard, "Copy Image");
#if PLATFORM(MAC)
    ContextMenuItem SearchSpotlightItem(ActionType, ContextMenuItemTagSearchInSpotlight, "Search in Spotlight");
#endif
    ContextMenuItem SearchWebItem(ActionType, ContextMenuItemTagSearchWeb, "Search in Google");
    ContextMenuItem LookInDictionaryItem(ActionType, ContextMenuItemTagLookUpInDictionary, "Look Up in Dictionary");
    ContextMenuItem CopyItem(ActionType, ContextMenuItemTagCopy, "Copy");
    ContextMenuItem BackItem(ActionType, ContextMenuItemTagGoBack, "Back");
    ContextMenuItem ForwardItem(ActionType, ContextMenuItemTagGoForward,  "Forward");
    ContextMenuItem StopItem(ActionType, ContextMenuItemTagStop, "Stop");
    ContextMenuItem ReloadItem(ActionType, ContextMenuItemTagReload, "Reload");
    ContextMenuItem OpenFrameItem(ActionType, ContextMenuItemTagOpenFrameInNewWindow, "Open Frame in New Window");
    ContextMenuItem NowGuessesItem(ActionType, ContextMenuItemTagNoGuessesFound, "No Guesses Found");
    ContextMenuItem IgnoreSpellingItem(ActionType, ContextMenuItemTagIgnoreSpelling, "Ignore Spelling");
    ContextMenuItem LearnSpellingItem(ActionType, ContextMenuItemTagLearnSpelling, "Learn Spelling");
    ContextMenuItem IgnoreGrammarItem(ActionType, ContextMenuItemTagIgnoreGrammar, "Ignore Grammar");
    ContextMenuItem CutItem(ActionType, ContextMenuItemTagCut, "Cut");
    ContextMenuItem PasteItem(ActionType, ContextMenuItemTagPaste, "Paste");
    
    HitTestResult result = hitTestResult();
    
    Node* node = m_hitTestResult.innerNonSharedNode();
    if (!node)
        return;
    Frame* frame = node->document()->frame();
    if (!frame)
        return;

    if (!result.isContentEditable()) {
        FrameLoader* loader = frame->loader();
        KURL linkURL = result.absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (loader->canHandleRequest(ResourceRequest(linkURL))) {
                appendItem(OpenLinkItem);
                appendItem(OpenLinkInNewWindowItem);
                appendItem(DownloadFileItem);
            }
            appendItem(CopyLinkItem);
        }

        KURL imageURL = result.absoluteImageURL();
        if (!imageURL.isEmpty()) {
            if (!linkURL.isEmpty())
                appendItem(SeparatorItem);

            appendItem(OpenImageInNewWindowItem);
            appendItem(DownloadImageItem);
            if (imageURL.isLocalFile()) // FIXME: Should be checking if the image is local or we have a file wrapper for it
                appendItem(CopyImageItem);
        }

        if (imageURL.isEmpty() && linkURL.isEmpty()) {
            if (result.isSelected()) {
#if PLATFORM(MAC)
                appendItem(SearchSpotlightItem);
#endif
                appendItem(SearchWebItem);
                appendItem(SeparatorItem);
                appendItem(LookInDictionaryItem);
                appendItem(SeparatorItem);
                appendItem(CopyItem);
            } else {
                if (loader->canGoBackOrForward(-1))
                    appendItem(BackItem);

                if (loader->canGoBackOrForward(1))
                    appendItem(ForwardItem);

                if (loader->isLoading())
                    appendItem(StopItem);
                else
                    appendItem(ReloadItem);

                if (frame->page() && frame != frame->page()->mainFrame())
                    appendItem(OpenFrameItem);
            }
        }
    } else { // Make an editing context menu
        SelectionController* selectionController = frame->selectionController();
        bool inPasswordField = selectionController->isInPasswordField();
        
        if (!inPasswordField) {
            // Consider adding spelling-related or grammar-related context menu items (never both, since a single selected range
            // is never considered a misspelling and bad grammar at the same time)
            bool misspelling = frame->editor()->isSelectionMisspelled();
            bool badGrammar = !misspelling && (frame->editor()->isGrammarCheckingEnabled() && frame->editor()->isSelectionUngrammatical());
            
            if (misspelling || badGrammar) {
                Vector<String> guesses = misspelling ? frame->editor()->guessesForMisspelledSelection()
                    : frame->editor()->guessesForUngrammaticalSelection();
                size_t size = guesses.size();
                if (size == 0) {
                    // If there's bad grammar but no suggestions (e.g., repeated word), just leave off the suggestions
                    // list and trailing separator rather than adding a "No Guesses Found" item (matches AppKit)
                    if (misspelling) {
                        appendItem(NowGuessesItem);
                        appendItem(SeparatorItem);
                    }
                } else {
                    for (unsigned i = 0; i < size; i++) {
                        const String &guess = guesses[i];
                        if (!guess.isEmpty()) {
                            ContextMenuItem item(ActionType, ContextMenuItemTagSpellingGuess, guess);
                            appendItem(item);
                        }
                    }
                    appendItem(SeparatorItem);                    
                }
                
                if (misspelling) {
                    appendItem(IgnoreSpellingItem);
                    appendItem(LearnSpellingItem);
                } else
                    appendItem(IgnoreGrammarItem);
                appendItem(SeparatorItem);
            }
        }

        if (result.isSelected() && !inPasswordField) {
#if PLATFORM(MAC)
            appendItem(SearchSpotlightItem);
#endif
            appendItem(SearchWebItem);
            appendItem(SeparatorItem);
     
            appendItem(LookInDictionaryItem);
            appendItem(SeparatorItem);
        }

        appendItem(CutItem);
        appendItem(CopyItem);
        appendItem(PasteItem);

        if (!inPasswordField) {
            appendItem(SeparatorItem);
#ifndef BUILDING_ON_TIGER
            ContextMenuItem SpellingAndGrammarMenuItem(SubmenuType, ContextMenuItemTagSpellingAndGrammarMenu,
                "Spelling and Grammar");
            createSpellingAndGrammarSubMenu(m_hitTestResult, SpellingAndGrammarMenuItem);
            appendItem(SpellingAndGrammarMenuItem);
#else
            ContextMenuItem SpellingMenuItem(SubmenuType, ContextMenuItemTagSpellingMenu, "Spelling");
            createSpellingSubMenu(m_hitTestResult, SpellingMenuItem);
            appendItem(SpellingMenuItem);
#endif
            ContextMenuItem  FontMenuItem(SubmenuType, ContextMenuItemTagFontMenu, "Font");
            createFontSubMenu(m_hitTestResult, FontMenuItem);
            appendItem(FontMenuItem);
#if PLATFORM(MAC)
            ContextMenuItem SpeechMenuItem(SubmenuType, ContextMenuItemTagSpeechMenu, "Speech");
            createSpeechSubMenu(m_hitTestResult, SpeechMenuItem);
            appendItem(SpeechMenuItem);
#endif
            ContextMenuItem WritingDirectionMenuItem(SubmenuType, ContextMenuItemTagWritingDirectionMenu, "Writing Direction");
            createWritingDirectionSubMenu(m_hitTestResult, WritingDirectionMenuItem);
            appendItem(WritingDirectionMenuItem);
        }
    }
}

}
