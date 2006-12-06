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

#ifndef ContextMenuItem_h
#define ContextMenuItem_h

#include "PlatformMenuDescription.h"
#include "PlatformString.h"

#if PLATFORM(MAC)
#include "RetainPtr.h"

#ifdef __OBJC__
@class NSMenuItem;
#else
class NSMenuItem;
#endif
#elif PLATFORM(WIN)
typedef struct tagMENUITEMINFOW* LPMENUITEMINFO;
#elif PLATFORM(QT)
#endif

namespace WebCore {

    class ContextMenu;

#if PLATFORM(MAC)
    typedef NSMenuItem* PlatformMenuItemDescription;
#elif PLATFORM(WIN)
    typedef LPMENUITEMINFO PlatformMenuItemDescription;
#elif PLATFORM(QT)
    typedef void* PlatformMenuItemDescription;
#endif

    // This enum needs to be in sync with WebMenuItemTag, which is defined in WebUIDelegate.h
    enum ContextMenuAction {
        ContextMenuItemTagNoAction=0, // This item is not actually in WebUIDelegate.h
        ContextMenuItemTagOpenLinkInNewWindow=1,
        ContextMenuItemTagDownloadLinkToDisk,
        ContextMenuItemTagCopyLinkToClipboard,
        ContextMenuItemTagOpenImageInNewWindow,
        ContextMenuItemTagDownloadImageToDisk,
        ContextMenuItemTagCopyImageToClipboard,
        ContextMenuItemTagOpenFrameInNewWindow,
        ContextMenuItemTagCopy,
        ContextMenuItemTagGoBack,
        ContextMenuItemTagGoForward,
        ContextMenuItemTagStop,
        ContextMenuItemTagReload,
        ContextMenuItemTagCut,
        ContextMenuItemTagPaste,
        ContextMenuItemTagSpellingGuess,
        ContextMenuItemTagNoGuessesFound,
        ContextMenuItemTagIgnoreSpelling,
        ContextMenuItemTagLearnSpelling,
        ContextMenuItemTagOther,
        ContextMenuItemTagSearchInSpotlight,
        ContextMenuItemTagSearchWeb,
        ContextMenuItemTagLookUpInDictionary,
        ContextMenuItemTagOpenWithDefaultApplication,
        ContextMenuItemPDFActualSize,
        ContextMenuItemPDFZoomIn,
        ContextMenuItemPDFZoomOut,
        ContextMenuItemPDFAutoSize,
        ContextMenuItemPDFSinglePage,
        ContextMenuItemPDFFacingPages,
        ContextMenuItemPDFContinuous,
        ContextMenuItemPDFNextPage,
        ContextMenuItemPDFPreviousPage,
        // These are new tags! Not a part of API!!!!
        ContextMenuItemTagOpenLink = 1000,
        ContextMenuItemTagIgnoreGrammar,
#ifndef BUILDING_ON_TIGER
        ContextMenuItemTagSpellingAndGrammarMenu, // Spelling sub-menu
        ContextMenuItemTagShowSpellingAndGrammar,
        ContextMenuItemTagCheckDocumentNow,
        ContextMenuItemTagCheckSpellingWhileTyping,
        ContextMenuItemTagCheckGrammarWithSpelling,
#else
        ContextMenuItemTagSpellingMenu, // Tiger Spelling sub-menu
        ContextMenuItemTagSpellingMenuItem,
        ContextMenuItemTagCheckSpelling,
        ContextMenuItemTagCheckSpellingWhileTyping,
#endif
        ContextMenuItemTagFontMenu, // Font sub-menu
        ContextMenuItemTagShowFonts,
        ContextMenuItemTagBold,
        ContextMenuItemTagItalic,
        ContextMenuItemTagUnderline,
        ContextMenuItemTagOutline,
        ContextMenuItemTagStyles,
        ContextMenuItemTagShowColors,
        ContextMenuItemTagSpeechMenu, // Speech sub-menu
        ContextMenuItemTagStartSpeaking,
        ContextMenuItemTagStopSpeaking,
        ContextMenuItemTagWritingDirectionMenu, // Writing Direction sub-menu
        ContextMenuItemTagDefaultDirection,
        ContextMenuItemTagLeftToRight,
        ContextMenuItemTagRightToLeft,
        ContextMenuItemBaseApplicationTag = 10000
    };

    enum ContextMenuItemType {
        ActionType,
        SeparatorType,
        SubmenuType
    };

    class ContextMenuItem {
    public:
        ContextMenuItem(PlatformMenuItemDescription, ContextMenu*);
        ContextMenuItem(ContextMenu* parentMenu = 0, ContextMenu* subMenu = 0);
        ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* parentMenu = 0, 
            ContextMenu* subMenu = 0);
        ~ContextMenuItem();

        ContextMenu* parentMenu() const { return m_parentMenu; }
        PlatformMenuItemDescription platformDescription() const;

        ContextMenuItemType type() const { return m_type; }
        void setType(ContextMenuItemType type) { m_type = type; }

        ContextMenuAction action() const;
        void setAction(ContextMenuAction action);

        String title() const;
        void setTitle(String title);

        PlatformMenuDescription platformSubMenu() const;
        void setSubMenu(ContextMenu* subMenu);

        // FIXME: Do we need a keyboard accelerator here?

    private:
        ContextMenu* m_parentMenu;
#if PLATFORM(MAC)
        RetainPtr<NSMenuItem> m_platformDescription;
#else
        PlatformMenuItemDescription m_platformDescription;
#endif
        ContextMenuItemType m_type;
    };

}

#endif // ContextMenuItem_h
