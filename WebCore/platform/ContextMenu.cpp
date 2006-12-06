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
    ContextMenu fontMenu(result);
    fontMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagShowFonts, "Show Fonts"));
    fontMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagBold, "Bold"));
    fontMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagItalic, "Italic"));
    fontMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagUnderline, "Underline"));
    fontMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagOutline, "Outline"));
    fontMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagStyles, "Styles..."));
    fontMenu.appendItem(ContextMenuItem(SeparatorType, ContextMenuItemTagNoAction, String()));
    fontMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagShowColors, "Show Colors"));
    fontMenuItem.setSubMenu(&fontMenu);
}

#ifndef BUILDING_ON_TIGER
static void createSpellingAndGrammarSubMenu(const HitTestResult& result, ContextMenuItem& spellingAndGrammarMenuItem)
{
    ContextMenu spellingAndGrammarMenu(result);
    spellingAndGrammarMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagShowSpellingAndGrammar,
        "Show Spelling and Grammar"));
    spellingAndGrammarMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagCheckDocumentNow, "Check Document Now"));
    spellingAndGrammarMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagCheckSpellingWhileTyping,
        "Check Spelling While Typing"));
    spellingAndGrammarMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagCheckGrammarWithSpelling,
        "Check Grammar With Spelling"));
    spellingAndGrammarMenuItem.setSubMenu(&spellingAndGrammarMenu);
}
#else
static void createSpellingSubMenu(const HitTestResult& result, const ContextMenuItem& spellingMenuItem)
{
    ContextMenu spellingMenu(result);
    spellingMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagSpellingMenuItem, "Spelling..."));
    spellingMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagCheckSpelling, "Check Spelling"));
    spellingMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagCheckSpellingWhileTyping,
        "Check Spelling as You Type"));
    spellingMenuItem.setSubMenu(spellingMenu);
}
#endif

#if PLATFORM(MAC)
static void createSpeechSubMenu(const HitTestResult& result, ContextMenuItem& speechMenuItem)
{
    ContextMenu speechMenu(result);
    speechMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagStartSpeaking, "Start Speaking"));
    speechMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagStartSpeaking, "Stop Speaking"));
    speechMenuItem.setSubMenu(&speechMenu);
}
#endif

static void createWritingDirectionSubMenu(const HitTestResult& result, ContextMenuItem& writingDirectionMenuItem)
{
    ContextMenu writingDirectionMenu(result);
    writingDirectionMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagDefaultDirection, "Default"));
    writingDirectionMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagLeftToRight, "Left to Right"));
    writingDirectionMenu.appendItem(ContextMenuItem(ActionType, ContextMenuItemTagRightToLeft, "Right to Left"));
    writingDirectionMenuItem.setSubMenu(&writingDirectionMenu);
}

void ContextMenu::populate()
{
    HitTestResult result = hitTestResult();
    
    Node* node = m_hitTestResult.innerNonSharedNode();
    if (!node)
        return;
    Frame* frame = node->document()->frame();
    if (!frame)
        return;

    ContextMenuItem SeparatorItem(SeparatorType, ContextMenuItemTagNoAction, String());

    if (!result.isContentEditable()) {
        FrameLoader* loader = frame->loader();
        KURL linkURL = result.absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (loader->canHandleRequest(ResourceRequest(linkURL))) {
                appendItem(ContextMenuItem(ActionType, ContextMenuItemTagOpenLink, "Open Link"));
                appendItem(ContextMenuItem(ActionType, ContextMenuItemTagOpenLinkInNewWindow, "Open Link in New Window"));
                appendItem(ContextMenuItem(ActionType, ContextMenuItemTagDownloadLinkToDisk, "Download Linked File"));
            }
            appendItem(ContextMenuItem(ActionType, ContextMenuItemTagCopyLinkToClipboard, "Copy Link"));
        }

        KURL imageURL = result.absoluteImageURL();
        if (!imageURL.isEmpty()) {
            if (!linkURL.isEmpty())
                appendItem(SeparatorItem);

            appendItem(ContextMenuItem(ActionType, ContextMenuItemTagOpenImageInNewWindow, "Open Image in New Window"));
            appendItem(ContextMenuItem(ActionType, ContextMenuItemTagDownloadImageToDisk, "Download Image"));
            if (imageURL.isLocalFile()) // FIXME: Should be checking if the image is local or we have a file wrapper for it
                appendItem(ContextMenuItem(ActionType, ContextMenuItemTagCopyImageToClipboard, "Copy Image"));
        }

        if (imageURL.isEmpty() && linkURL.isEmpty()) {
            if (result.isSelected()) {
#if PLATFORM(MAC)
                appendItem(ContextMenuItem(ActionType, ContextMenuItemTagSearchInSpotlight, "Search in Spotlight"));
#endif
                appendItem(ContextMenuItem(ActionType, ContextMenuItemTagSearchWeb, "Search in Google"));
                appendItem(SeparatorItem);
                appendItem(ContextMenuItem(ActionType, ContextMenuItemTagLookUpInDictionary, "Look Up in Dictionary"));
                appendItem(SeparatorItem);
                appendItem(ContextMenuItem(ActionType, ContextMenuItemTagCopy, "Copy"));
            } else {
                if (loader->canGoBackOrForward(-1))
                    appendItem(ContextMenuItem(ActionType, ContextMenuItemTagGoBack, "Back"));

                if (loader->canGoBackOrForward(1))
                    appendItem(ContextMenuItem(ActionType, ContextMenuItemTagGoForward,  "Forward"));

                if (loader->isLoading())
                    appendItem(ContextMenuItem(ActionType, ContextMenuItemTagStop, "Stop"));
                else
                    appendItem(ContextMenuItem(ActionType, ContextMenuItemTagReload, "Reload"));

                if (frame->page() && frame != frame->page()->mainFrame())
                    appendItem(ContextMenuItem(ActionType, ContextMenuItemTagOpenFrameInNewWindow, "Open Frame in New Window"));
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
                Vector<String> guesses = misspelling ? frame->editor()->guessesForMisspelledSelection() : frame->editor()->guessesForUngrammaticalSelection();
                size_t size = guesses.size();
                if (size == 0) {
                    // If there's bad grammar but no suggestions (e.g., repeated word), just leave off the suggestions
                    // list and trailing separator rather than adding a "No Guesses Found" item (matches AppKit)
                    if (misspelling) {
                        appendItem(ContextMenuItem(ActionType, ContextMenuItemTagNoGuessesFound, "No Guesses Found"));
                        appendItem(SeparatorItem);
                    }
                } else {
                    for (unsigned i = 0; i < size; i++) {
                        const String &guess = guesses[i];
                        if (!guess.isEmpty())
                            appendItem(ContextMenuItem(ActionType, ContextMenuItemTagSpellingGuess, guess));
                    }
                    appendItem(SeparatorItem);                    
                }
                
                if (misspelling) {
                    appendItem(ContextMenuItem(ActionType, ContextMenuItemTagIgnoreSpelling, "Ignore Spelling"));
                    appendItem(ContextMenuItem(ActionType, ContextMenuItemTagLearnSpelling, "Learn Spelling"));
                } else
                    appendItem(ContextMenuItem(ActionType, ContextMenuItemTagIgnoreGrammar, "Ignore Grammar"));
                appendItem(SeparatorItem);
            }
        }

        if (result.isSelected() && !inPasswordField) {
#if PLATFORM(MAC)
            appendItem(ContextMenuItem(ActionType, ContextMenuItemTagSearchInSpotlight, "Search in Spotlight"));
#endif
            appendItem(ContextMenuItem(ActionType, ContextMenuItemTagSearchWeb, "Search in Google"));
            appendItem(SeparatorItem);
     
            appendItem(ContextMenuItem(ActionType, ContextMenuItemTagLookUpInDictionary, "Look Up in Dictionary"));
            appendItem(SeparatorItem);
        }

        appendItem(ContextMenuItem(ActionType, ContextMenuItemTagCut, "Cut"));
        appendItem(ContextMenuItem(ActionType, ContextMenuItemTagCopy, "Copy"));
        appendItem(ContextMenuItem(ActionType, ContextMenuItemTagPaste, "Paste"));

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
