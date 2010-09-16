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
#include <wtf/OwnPtr.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
@class NSMenuItem;
#else
class NSMenuItem;
#endif
#elif PLATFORM(WIN)
typedef struct tagMENUITEMINFOW* LPMENUITEMINFO;
#elif PLATFORM(GTK)
typedef struct _GtkMenuItem GtkMenuItem;
#elif PLATFORM(QT)
#include <QAction>
#elif PLATFORM(WX)
class wxMenuItem;
#elif PLATFORM(HAIKU)
class BMenuItem;
#endif

namespace WebCore {

    class ContextMenu;

    // This enum needs to be in sync with the WebMenuItemTag enum in WebUIDelegate.h and the
    // extra values in WebUIDelegatePrivate.h
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
#if PLATFORM(GTK)
        ContextMenuItemTagDelete,
        ContextMenuItemTagSelectAll,
        ContextMenuItemTagInputMethods,
        ContextMenuItemTagUnicode,
#endif
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
        ContextMenuItemTagOpenLink = 2000,
        ContextMenuItemTagIgnoreGrammar,
        ContextMenuItemTagSpellingMenu, // Spelling or Spelling/Grammar sub-menu
        ContextMenuItemTagShowSpellingPanel,
        ContextMenuItemTagCheckSpelling,
        ContextMenuItemTagCheckSpellingWhileTyping,
        ContextMenuItemTagCheckGrammarWithSpelling,
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
        ContextMenuItemTagPDFSinglePageScrolling,
        ContextMenuItemTagPDFFacingPagesScrolling,
#if ENABLE(INSPECTOR)
        ContextMenuItemTagInspectElement,
#endif
        ContextMenuItemTagTextDirectionMenu, // Text Direction sub-menu
        ContextMenuItemTagTextDirectionDefault,
        ContextMenuItemTagTextDirectionLeftToRight,
        ContextMenuItemTagTextDirectionRightToLeft,
#if PLATFORM(MAC)
        ContextMenuItemTagCorrectSpellingAutomatically,
        ContextMenuItemTagSubstitutionsMenu,
        ContextMenuItemTagShowSubstitutions,
        ContextMenuItemTagSmartCopyPaste,
        ContextMenuItemTagSmartQuotes,
        ContextMenuItemTagSmartDashes,
        ContextMenuItemTagSmartLinks,
        ContextMenuItemTagTextReplacement,
        ContextMenuItemTagTransformationsMenu,
        ContextMenuItemTagMakeUpperCase,
        ContextMenuItemTagMakeLowerCase,
        ContextMenuItemTagCapitalize,
        ContextMenuItemTagChangeBack,
#endif
        ContextMenuItemBaseCustomTag = 5000,
        ContextMenuItemCustomTagNoAction = 5998,
        ContextMenuItemLastCustomTag = 5999,
        ContextMenuItemBaseApplicationTag = 10000
    };

    enum ContextMenuItemType {
        ActionType,
        CheckableActionType,
        SeparatorType,
        SubmenuType
    };

#if PLATFORM(MAC)
    typedef NSMenuItem* PlatformMenuItemDescription;
#elif PLATFORM(WIN)
    typedef LPMENUITEMINFO PlatformMenuItemDescription;
#elif PLATFORM(QT)
    struct PlatformMenuItemDescription {
        PlatformMenuItemDescription()
            : type(ActionType),
              action(ContextMenuItemTagNoAction),
              checked(false),
              enabled(true)
        {}

        ContextMenuItemType type;
        ContextMenuAction action;
        String title;
        QList<ContextMenuItem> subMenuItems;
        bool checked;
        bool enabled;
    };
#elif PLATFORM(GTK)
    struct PlatformMenuItemDescription {
        PlatformMenuItemDescription()
            : type(ActionType)
            , action(ContextMenuItemTagNoAction)
            , subMenu(0)
            , checked(false)
            , enabled(true)
        {}

        ContextMenuItemType type;
        ContextMenuAction action;
        String title;
        GtkMenu* subMenu;
        bool checked;
        bool enabled;
    };
#elif PLATFORM(WX)
    struct PlatformMenuItemDescription {
        PlatformMenuItemDescription()
            : type(ActionType),
              action(ContextMenuItemTagNoAction),
              checked(false),
              enabled(true)
        {}

        ContextMenuItemType type;
        ContextMenuAction action;
        String title;
        wxMenu * subMenu;
        bool checked;
        bool enabled;
    };
#elif PLATFORM(HAIKU)
    typedef BMenuItem* PlatformMenuItemDescription;
#elif PLATFORM(CHROMIUM)
    struct PlatformMenuItemDescription {
        PlatformMenuItemDescription()
            : type(ActionType)
            , action(ContextMenuItemTagNoAction)
            , checked(false)
            , enabled(true) { }
        ContextMenuItemType type;
        ContextMenuAction action;
        String title;
        bool checked;
        bool enabled;
    };
#elif PLATFORM(EFL)
    struct PlatformMenuItemDescription {
        PlatformMenuItemDescription()
            : type(ActionType)
            , action(ContextMenuItemTagNoAction)
            , title("")
            , subMenu(0)
            , checked(false)
            , enabled(true) { }
        ContextMenuItemType type;
        ContextMenuAction action;
        String title;
        ContextMenu* subMenu;
        bool checked;
        bool enabled;
    };
#else
    typedef void* PlatformMenuItemDescription;
#endif

    class ContextMenuItem : public FastAllocBase {
    public:
        ContextMenuItem(PlatformMenuItemDescription);
        ContextMenuItem(ContextMenu* subMenu = 0);
        ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu = 0);
#if PLATFORM(GTK)
        ContextMenuItem(GtkMenuItem*);
#endif
        ~ContextMenuItem();

        PlatformMenuItemDescription releasePlatformDescription();

        ContextMenuItemType type() const;
        void setType(ContextMenuItemType);

        ContextMenuAction action() const;
        void setAction(ContextMenuAction);

        String title() const;
        void setTitle(const String&);

        PlatformMenuDescription platformSubMenu() const;
        void setSubMenu(ContextMenu*);

        void setChecked(bool = true);
        bool checked() const;

        void setEnabled(bool = true);
        bool enabled() const;

        // FIXME: Do we need a keyboard accelerator here?
#if PLATFORM(GTK)
        static GtkMenuItem* createNativeMenuItem(const PlatformMenuItemDescription&);
#endif

    private:
#if PLATFORM(MAC)
        RetainPtr<NSMenuItem> m_platformDescription;
#else
        PlatformMenuItemDescription m_platformDescription;
#endif
    };

}

#endif // ContextMenuItem_h
