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
#include "Frame.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "Node.h"
#include "Page.h"
#include "ResourceRequest.h"
#include "SelectionController.h"

namespace WebCore {

#define MENU_ACTION_ITEM(action, title) static ContextMenuItem action##Item(ActionType, ContextMenuItemTag##action, String(title))

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
    static ContextMenuItem SeparatorItem(SeparatorType, ContextMenuItemTagNoAction, String());

    MENU_ACTION_ITEM(ShowFonts, "Show Fonts");
    MENU_ACTION_ITEM(Bold, "Bold");
    MENU_ACTION_ITEM(Italic, "Italic");
    MENU_ACTION_ITEM(Underline, "Underline");
    MENU_ACTION_ITEM(Outline, "Outline");
    MENU_ACTION_ITEM(Styles, "Styles...");
    MENU_ACTION_ITEM(ShowColors, "Show Colors");
    
    ContextMenu* fontMenu = new ContextMenu(result);
    fontMenu->appendItem(ShowFontsItem);
    fontMenu->appendItem(BoldItem);
    fontMenu->appendItem(ItalicItem);
    fontMenu->appendItem(UnderlineItem);
    fontMenu->appendItem(OutlineItem);
    fontMenu->appendItem(StylesItem);
    fontMenu->appendItem(SeparatorItem);
    fontMenu->appendItem(ShowColorsItem);
    fontMenuItem.setSubMenu(fontMenu);
}

#ifndef BUILDING_ON_TIGER
static void createSpellingAndGrammarSubMenu(const HitTestResult& result, ContextMenuItem& spellingAndGrammarMenuItem)
{
    MENU_ACTION_ITEM(ShowSpellingAndGrammar, "Show Spelling and Grammar");
    MENU_ACTION_ITEM(CheckDocumentNow, "Check Document Now");
    MENU_ACTION_ITEM(CheckSpellingWhileTyping, "Check Spelling While Typing");
    MENU_ACTION_ITEM(CheckGrammarWithSpelling, "Check Grammar With Spelling");

    ContextMenu* spellingAndGrammarMenu = new ContextMenu(result);
    spellingAndGrammarMenu->appendItem(ShowSpellingAndGrammarItem);
    spellingAndGrammarMenu->appendItem(CheckDocumentNowItem);
    spellingAndGrammarMenu->appendItem(CheckSpellingWhileTypingItem);
    spellingAndGrammarMenu->appendItem(CheckGrammarWithSpellingItem);
    spellingAndGrammarMenuItem.setSubMenu(spellingAndGrammarMenu);
}
#else
static void createSpellingSubMenu(const HitTestResult& result, ContextMenuItem& spellingMenuItem)
{
    MENU_ACTION_ITEM(SpellingMenuItem, "Spelling...");
    MENU_ACTION_ITEM(CheckSpelling, "Check Spelling");
    MENU_ACTION_ITEM(CheckSpellingWhileTyping, "Check Spelling as You Type");

    ContextMenu* spellingMenu = new ContextMenu(result);
    spellingMenu->appendItem(SpellingMenuItemItem);
    spellingMenu->appendItem(CheckSpellingItem);
    spellingMenu->appendItem(CheckSpellingWhileTypingItem);
    spellingMenuItem.setSubMenu(spellingMenu);
}
#endif

#if PLATFORM(MAC)
static void createSpeechSubMenu(const HitTestResult& result, ContextMenuItem& speechMenuItem)
{
    MENU_ACTION_ITEM(StartSpeaking, "Start Speaking");
    MENU_ACTION_ITEM(StopSpeaking, "Stop Speaking");

    ContextMenu* speechMenu = new ContextMenu(result);
    speechMenu->appendItem(StartSpeakingItem);
    speechMenu->appendItem(StopSpeakingItem);
    speechMenuItem.setSubMenu(speechMenu);
}
#endif

static void createWritingDirectionSubMenu(const HitTestResult& result, ContextMenuItem& writingDirectionMenuItem)
{
    MENU_ACTION_ITEM(DefaultDirection, "Default");
    MENU_ACTION_ITEM(LeftToRight, "Left to Right");
    MENU_ACTION_ITEM(RightToLeft, "Right to Left");

    ContextMenu* writingDirectionMenu = new ContextMenu(result);
    writingDirectionMenu->appendItem(DefaultDirectionItem);
    writingDirectionMenu->appendItem(LeftToRightItem);
    writingDirectionMenu->appendItem(RightToLeftItem);
    writingDirectionMenuItem.setSubMenu(writingDirectionMenu);
}

void ContextMenu::populate()
{
    static ContextMenuItem SeparatorItem(SeparatorType, ContextMenuItemTagNoAction, String());

    MENU_ACTION_ITEM(OpenLinkInNewWindow, "Open Link in New Window");
    MENU_ACTION_ITEM(DownloadLinkToDisk, "Download Linked File");
    MENU_ACTION_ITEM(CopyLinkToClipboard, "Copy Link");
    MENU_ACTION_ITEM(OpenImageInNewWindow, "Open Image in New Window");
    MENU_ACTION_ITEM(DownloadImageToDisk, "Download Image");
    MENU_ACTION_ITEM(CopyImageToClipboard, "Copy Image");
    MENU_ACTION_ITEM(OpenFrameInNewWindow, "Open Frame in New Window");
    MENU_ACTION_ITEM(Copy, "Copy");
    MENU_ACTION_ITEM(GoBack, "Back");
    MENU_ACTION_ITEM(GoForward, "Forward");
    MENU_ACTION_ITEM(Stop, "Stop");
    MENU_ACTION_ITEM(Reload, "Reload");
    MENU_ACTION_ITEM(Cut, "Cut");
    MENU_ACTION_ITEM(Paste, "Paste");
    MENU_ACTION_ITEM(SpellingGuess, "");
    MENU_ACTION_ITEM(NoGuessesFound, "No Guesses Found");
    MENU_ACTION_ITEM(IgnoreSpelling, "Ignore Spelling");
    MENU_ACTION_ITEM(LearnSpelling, "Learn Spelling");
#if PLATFORM(MAC)
    MENU_ACTION_ITEM(SearchInSpotlight, "Search in Spotlight");
#endif
    MENU_ACTION_ITEM(SearchWeb, "Search in Google");
    MENU_ACTION_ITEM(LookUpInDictionary, "Look Up in Dictionary");
    MENU_ACTION_ITEM(OpenLink, "Open Link");

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
                appendItem(DownloadLinkToDiskItem);
            }
            appendItem(CopyLinkToClipboardItem);
        }

        KURL imageURL = result.absoluteImageURL();
        if (!imageURL.isEmpty()) {
            if (!linkURL.isEmpty())
                appendItem(SeparatorItem);

            appendItem(OpenImageInNewWindowItem);
            appendItem(DownloadImageToDiskItem);
            if (imageURL.isLocalFile()) // FIXME: Should be checking if the image is local or we have a file wrapper for it
                appendItem(CopyImageToClipboardItem);
        }

        if (imageURL.isEmpty() && linkURL.isEmpty()) {
            if (result.isSelected()) {
#if PLATFORM(MAC)
                appendItem(SearchInSpotlightItem);
#endif
                appendItem(SearchWebItem);
                appendItem(SeparatorItem);
                appendItem(LookUpInDictionaryItem);
                appendItem(SeparatorItem);
                appendItem(CopyItem);
            } else {
                if (loader->canGoBackOrForward(-1))
                    appendItem(GoBackItem);

                if (loader->canGoBackOrForward(1))
                    appendItem(GoForwardItem);
                
                if (loader->isLoading())
                    appendItem(StopItem);
                else
                    appendItem(ReloadItem);

                if (frame->page() && frame != frame->page()->mainFrame())
                    appendItem(OpenFrameInNewWindowItem);
            }
        }
    } else { // Make an editing context menu
        SelectionController* selectionController = frame->selectionController();
        bool inPasswordField = selectionController->isInPasswordField();

        // Add spelling-related context menu items.
        if (frame->isSelectionMisspelled() && !inPasswordField) {
            Vector<String> guesses = frame->guessesForMisspelledSelection();
            unsigned size = guesses.size();
            if (size == 0)
                appendItem(NoGuessesFoundItem);
            else {
                for (unsigned i = 0; i < size; i++) {
                    String guess = guesses[i];
                    if (!guess.isNull()) {
                        ContextMenuItem item(ActionType, ContextMenuItemTagSpellingGuess, guess);
                        appendItem(item);
                    }
                }
            }                

            appendItem(SeparatorItem);
            appendItem(IgnoreSpellingItem);
            appendItem(LearnSpellingItem);
            appendItem(SeparatorItem);
        }

        if (result.isSelected() && !inPasswordField) {
#if PLATFORM(MAC)
            appendItem(SearchInSpotlightItem);
#endif
            appendItem(SearchWebItem);
            appendItem(SeparatorItem);
     
            appendItem(LookUpInDictionaryItem);
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
            ContextMenuItem FontMenuItem(SubmenuType, ContextMenuItemTagFontMenu, "Font");
            createFontSubMenu(m_hitTestResult, FontMenuItem);
            appendItem(FontMenuItem);
#if PLATFORM(MAC)
            ContextMenuItem SpeechMenuItem(SubmenuType, ContextMenuItemTagSpeechMenu, "Speech");
            createSpeechSubMenu(m_hitTestResult, SpeechMenuItem);
            appendItem(SpeechMenuItem);
#endif
            ContextMenuItem WritingDirectionMenuItem(SubmenuType, ContextMenuItemTagWritingDirectionMenu,
                "Writing Direction");
            createWritingDirectionSubMenu(m_hitTestResult, WritingDirectionMenuItem);
            appendItem(WritingDirectionMenuItem);
        }
    }
}

}
