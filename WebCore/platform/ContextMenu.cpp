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

void ContextMenu::populate()
{
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
    // FIXME: Add PDF action items

    ContextMenuItem SeparatorItem(SeparatorType, ContextMenuItemTagNoAction, String());
    HitTestResult result = hitTestResult();

    if (!result.isContentEditable()) {
        KURL linkURL = result.absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (true) { // FIXME: if FrameLoaderClient can handle the request
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
            if (imageURL.isLocalFile())
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
                FrameLoader* loader = result.innerNonSharedNode()->document()->frame()->loader();
                if (loader->canGoBackOrForward(-1))
                    appendItem(GoBackItem);

                if (loader->canGoBackOrForward(1))
                    appendItem(GoForwardItem);
                
                if (loader->isLoading())
                    appendItem(StopItem);
                else
                    appendItem(ReloadItem);

                if (result.innerNonSharedNode()->document()->frame() != result.innerNonSharedNode()->document()->frame()->page()->mainFrame())
                    appendItem(OpenFrameInNewWindowItem);
            }
        }
    } else { // Make an editing context menu
        SelectionController* selectionController = result.innerNonSharedNode()->document()->frame()->selectionController();
        bool inPasswordField = selectionController->isInPasswordField();

        // Add spelling-related context menu items.
        if (true) { // FIXME: Should be (selectionController->isSelectionMisspelled() && !inPasswordField)
            // FIXME: Add spelling guesses here
            appendItem(NoGuessesFoundItem);

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
        appendItem(SeparatorItem);

        // FIXME: Add "Spelling [and Grammar, on Leopard]", "Font", "Speech", "Writing Direction" submenus here.
    }
}

}
