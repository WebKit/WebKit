/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2010 Igalia S.L
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include <wtf/text/WTFString.h>

namespace WebCore {

class ContextMenu;
class Image;

enum ContextMenuAction {
    ContextMenuItemTagNoAction,
    ContextMenuItemTagOpenLinkInNewWindow,
    ContextMenuItemTagDownloadLinkToDisk,
    ContextMenuItemTagCopyLinkToClipboard,
    ContextMenuItemTagOpenImageInNewWindow,
    ContextMenuItemTagDownloadImageToDisk,
    ContextMenuItemTagCopyImageToClipboard,
#if PLATFORM(GTK)
    ContextMenuItemTagCopyImageUrlToClipboard,
#endif
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
    ContextMenuItemTagUnicodeInsertLRMMark,
    ContextMenuItemTagUnicodeInsertRLMMark,
    ContextMenuItemTagUnicodeInsertLREMark,
    ContextMenuItemTagUnicodeInsertRLEMark,
    ContextMenuItemTagUnicodeInsertLROMark,
    ContextMenuItemTagUnicodeInsertRLOMark,
    ContextMenuItemTagUnicodeInsertPDFMark,
    ContextMenuItemTagUnicodeInsertZWSMark,
    ContextMenuItemTagUnicodeInsertZWJMark,
    ContextMenuItemTagUnicodeInsertZWNJMark,
    ContextMenuItemTagInsertEmoji,
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
    ContextMenuItemTagOpenLink,
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
    ContextMenuItemTagInspectElement,
    ContextMenuItemTagTextDirectionMenu, // Text Direction sub-menu
    ContextMenuItemTagTextDirectionDefault,
    ContextMenuItemTagTextDirectionLeftToRight,
    ContextMenuItemTagTextDirectionRightToLeft,
#if PLATFORM(COCOA)
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
    ContextMenuItemTagOpenMediaInNewWindow,
    ContextMenuItemTagDownloadMediaToDisk,
    ContextMenuItemTagCopyMediaLinkToClipboard,
    ContextMenuItemTagToggleMediaControls,
    ContextMenuItemTagToggleMediaLoop,
    ContextMenuItemTagEnterVideoFullscreen,
    ContextMenuItemTagMediaPlayPause,
    ContextMenuItemTagMediaMute,
    ContextMenuItemTagDictationAlternative,
    ContextMenuItemTagToggleVideoFullscreen,
    ContextMenuItemTagShareMenu,
    ContextMenuItemTagToggleVideoEnhancedFullscreen,
    ContextMenuItemBaseCustomTag = 5000,
    ContextMenuItemLastCustomTag = 5999,
    ContextMenuItemBaseApplicationTag = 10000
};

enum ContextMenuItemType {
    ActionType,
    CheckableActionType,
    SeparatorType,
    SubmenuType
};

class ContextMenuItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT ContextMenuItem(ContextMenuItemType, ContextMenuAction, const String&, ContextMenu* subMenu = 0);
    WEBCORE_EXPORT ContextMenuItem(ContextMenuItemType, ContextMenuAction, const String&, bool enabled, bool checked);

    WEBCORE_EXPORT ~ContextMenuItem();

    void setType(ContextMenuItemType);
    WEBCORE_EXPORT ContextMenuItemType type() const;

    void setAction(ContextMenuAction);
    WEBCORE_EXPORT ContextMenuAction action() const;

    void setChecked(bool = true);
    WEBCORE_EXPORT bool checked() const;

    void setEnabled(bool = true);
    WEBCORE_EXPORT bool enabled() const;

    void setSubMenu(ContextMenu*);

    WEBCORE_EXPORT ContextMenuItem(ContextMenuAction, const String&, bool enabled, bool checked, const Vector<ContextMenuItem>& subMenuItems);
    ContextMenuItem();

    bool isNull() const;

    void setTitle(const String& title) { m_title = title; }
    const String& title() const { return m_title; }

    const Vector<ContextMenuItem>& subMenuItems() const { return m_subMenuItems; }
private:
    ContextMenuItemType m_type;
    ContextMenuAction m_action;
    String m_title;
    bool m_enabled;
    bool m_checked;
    Vector<ContextMenuItem> m_subMenuItems;
};

}

#endif // ContextMenuItem_h
