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

#include <wtf/Noncopyable.h>

#include "PlatformString.h"

#if PLATFORM(MAC)
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
        ContextMenuItemBaseApplicationTag = 1000
    };

    enum ContextMenuItemType {
        ActionType,
        SeparatorType,
        SubmenuType
    };

    class ContextMenuItem : Noncopyable {
    public:
        ContextMenuItem(PlatformMenuItemDescription, ContextMenu*);

        ContextMenuItem(ContextMenu* menu = 0)
            : m_menu(menu)
            , m_platformDescription(0)
            , m_type(SeparatorType)
            , m_action(ContextMenuItemTagNoAction)
            , m_title(String())
        {
        }

        ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* menu = 0)
            : m_menu(menu)
            , m_platformDescription(0)
            , m_type(type)
            , m_action(action)
            , m_title(title)
        {
        }

        ~ContextMenuItem();

        ContextMenu* menu() const { return m_menu; }
        const PlatformMenuItemDescription platformDescription() const { return m_platformDescription; }
        ContextMenuItemType type() const { return m_type; }
        ContextMenuAction action() const { return m_action; }
        String title() const { return m_title; }

        // FIXME: Need to support submenus (perhaps a Vector<ContextMenuItem>*?)
        // FIXME: Do we need a keyboard accelerator here?

    private:
        ContextMenu* m_menu;
        PlatformMenuItemDescription m_platformDescription;
        ContextMenuItemType m_type;
        ContextMenuAction m_action;
        String m_title;
    };

}

#endif // ContextMenuItem_h
