/*
 * Copyright (C) 2006-2020 Apple Inc.  All rights reserved.
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

#pragma once

#include <wtf/EnumTraits.h>
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
    ContextMenuItemTagPasteAsPlainText,
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

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ContextMenuAction> {
    using values = EnumValues<
        WebCore::ContextMenuAction,
        WebCore::ContextMenuAction::ContextMenuItemTagNoAction,
        WebCore::ContextMenuAction::ContextMenuItemTagOpenLinkInNewWindow,
        WebCore::ContextMenuAction::ContextMenuItemTagDownloadLinkToDisk,
        WebCore::ContextMenuAction::ContextMenuItemTagCopyLinkToClipboard,
        WebCore::ContextMenuAction::ContextMenuItemTagOpenImageInNewWindow,
        WebCore::ContextMenuAction::ContextMenuItemTagDownloadImageToDisk,
        WebCore::ContextMenuAction::ContextMenuItemTagCopyImageToClipboard,
#if PLATFORM(GTK)
        WebCore::ContextMenuAction::ContextMenuItemTagCopyImageUrlToClipboard,
#endif
        WebCore::ContextMenuAction::ContextMenuItemTagOpenFrameInNewWindow,
        WebCore::ContextMenuAction::ContextMenuItemTagCopy,
        WebCore::ContextMenuAction::ContextMenuItemTagGoBack,
        WebCore::ContextMenuAction::ContextMenuItemTagGoForward,
        WebCore::ContextMenuAction::ContextMenuItemTagStop,
        WebCore::ContextMenuAction::ContextMenuItemTagReload,
        WebCore::ContextMenuAction::ContextMenuItemTagCut,
        WebCore::ContextMenuAction::ContextMenuItemTagPaste,
#if PLATFORM(GTK)
        WebCore::ContextMenuAction::ContextMenuItemTagPasteAsPlainText,
        WebCore::ContextMenuAction::ContextMenuItemTagDelete,
        WebCore::ContextMenuAction::ContextMenuItemTagSelectAll,
        WebCore::ContextMenuAction::ContextMenuItemTagInputMethods,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicode,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicodeInsertLRMMark,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicodeInsertRLMMark,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicodeInsertLREMark,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicodeInsertRLEMark,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicodeInsertLROMark,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicodeInsertRLOMark,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicodeInsertPDFMark,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicodeInsertZWSMark,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicodeInsertZWJMark,
        WebCore::ContextMenuAction::ContextMenuItemTagUnicodeInsertZWNJMark,
        WebCore::ContextMenuAction::ContextMenuItemTagInsertEmoji,
#endif
        WebCore::ContextMenuAction::ContextMenuItemTagSpellingGuess,
        WebCore::ContextMenuAction::ContextMenuItemTagNoGuessesFound,
        WebCore::ContextMenuAction::ContextMenuItemTagIgnoreSpelling,
        WebCore::ContextMenuAction::ContextMenuItemTagLearnSpelling,
        WebCore::ContextMenuAction::ContextMenuItemTagOther,
        WebCore::ContextMenuAction::ContextMenuItemTagSearchInSpotlight,
        WebCore::ContextMenuAction::ContextMenuItemTagSearchWeb,
        WebCore::ContextMenuAction::ContextMenuItemTagLookUpInDictionary,
        WebCore::ContextMenuAction::ContextMenuItemTagOpenWithDefaultApplication,
        WebCore::ContextMenuAction::ContextMenuItemPDFActualSize,
        WebCore::ContextMenuAction::ContextMenuItemPDFZoomIn,
        WebCore::ContextMenuAction::ContextMenuItemPDFZoomOut,
        WebCore::ContextMenuAction::ContextMenuItemPDFAutoSize,
        WebCore::ContextMenuAction::ContextMenuItemPDFSinglePage,
        WebCore::ContextMenuAction::ContextMenuItemPDFFacingPages,
        WebCore::ContextMenuAction::ContextMenuItemPDFContinuous,
        WebCore::ContextMenuAction::ContextMenuItemPDFNextPage,
        WebCore::ContextMenuAction::ContextMenuItemPDFPreviousPage,
        WebCore::ContextMenuAction::ContextMenuItemTagOpenLink,
        WebCore::ContextMenuAction::ContextMenuItemTagIgnoreGrammar,
        WebCore::ContextMenuAction::ContextMenuItemTagSpellingMenu,
        WebCore::ContextMenuAction::ContextMenuItemTagShowSpellingPanel,
        WebCore::ContextMenuAction::ContextMenuItemTagCheckSpelling,
        WebCore::ContextMenuAction::ContextMenuItemTagCheckSpellingWhileTyping,
        WebCore::ContextMenuAction::ContextMenuItemTagCheckGrammarWithSpelling,
        WebCore::ContextMenuAction::ContextMenuItemTagFontMenu,
        WebCore::ContextMenuAction::ContextMenuItemTagShowFonts,
        WebCore::ContextMenuAction::ContextMenuItemTagBold,
        WebCore::ContextMenuAction::ContextMenuItemTagItalic,
        WebCore::ContextMenuAction::ContextMenuItemTagUnderline,
        WebCore::ContextMenuAction::ContextMenuItemTagOutline,
        WebCore::ContextMenuAction::ContextMenuItemTagStyles,
        WebCore::ContextMenuAction::ContextMenuItemTagShowColors,
        WebCore::ContextMenuAction::ContextMenuItemTagSpeechMenu,
        WebCore::ContextMenuAction::ContextMenuItemTagStartSpeaking,
        WebCore::ContextMenuAction::ContextMenuItemTagStopSpeaking,
        WebCore::ContextMenuAction::ContextMenuItemTagWritingDirectionMenu,
        WebCore::ContextMenuAction::ContextMenuItemTagDefaultDirection,
        WebCore::ContextMenuAction::ContextMenuItemTagLeftToRight,
        WebCore::ContextMenuAction::ContextMenuItemTagRightToLeft,
        WebCore::ContextMenuAction::ContextMenuItemTagPDFSinglePageScrolling,
        WebCore::ContextMenuAction::ContextMenuItemTagPDFFacingPagesScrolling,
        WebCore::ContextMenuAction::ContextMenuItemTagInspectElement,
        WebCore::ContextMenuAction::ContextMenuItemTagTextDirectionMenu,
        WebCore::ContextMenuAction::ContextMenuItemTagTextDirectionDefault,
        WebCore::ContextMenuAction::ContextMenuItemTagTextDirectionLeftToRight,
        WebCore::ContextMenuAction::ContextMenuItemTagTextDirectionRightToLeft,
#if PLATFORM(COCOA)
        WebCore::ContextMenuAction::ContextMenuItemTagCorrectSpellingAutomatically,
        WebCore::ContextMenuAction::ContextMenuItemTagSubstitutionsMenu,
        WebCore::ContextMenuAction::ContextMenuItemTagShowSubstitutions,
        WebCore::ContextMenuAction::ContextMenuItemTagSmartCopyPaste,
        WebCore::ContextMenuAction::ContextMenuItemTagSmartQuotes,
        WebCore::ContextMenuAction::ContextMenuItemTagSmartDashes,
        WebCore::ContextMenuAction::ContextMenuItemTagSmartLinks,
        WebCore::ContextMenuAction::ContextMenuItemTagTextReplacement,
        WebCore::ContextMenuAction::ContextMenuItemTagTransformationsMenu,
        WebCore::ContextMenuAction::ContextMenuItemTagMakeUpperCase,
        WebCore::ContextMenuAction::ContextMenuItemTagMakeLowerCase,
        WebCore::ContextMenuAction::ContextMenuItemTagCapitalize,
        WebCore::ContextMenuAction::ContextMenuItemTagChangeBack,
#endif
        WebCore::ContextMenuAction::ContextMenuItemTagOpenMediaInNewWindow,
        WebCore::ContextMenuAction::ContextMenuItemTagDownloadMediaToDisk,
        WebCore::ContextMenuAction::ContextMenuItemTagCopyMediaLinkToClipboard,
        WebCore::ContextMenuAction::ContextMenuItemTagToggleMediaControls,
        WebCore::ContextMenuAction::ContextMenuItemTagToggleMediaLoop,
        WebCore::ContextMenuAction::ContextMenuItemTagEnterVideoFullscreen,
        WebCore::ContextMenuAction::ContextMenuItemTagMediaPlayPause,
        WebCore::ContextMenuAction::ContextMenuItemTagMediaMute,
        WebCore::ContextMenuAction::ContextMenuItemTagDictationAlternative,
        WebCore::ContextMenuAction::ContextMenuItemTagToggleVideoFullscreen,
        WebCore::ContextMenuAction::ContextMenuItemTagShareMenu,
        WebCore::ContextMenuAction::ContextMenuItemTagToggleVideoEnhancedFullscreen,
        WebCore::ContextMenuAction::ContextMenuItemBaseCustomTag,
        WebCore::ContextMenuAction::ContextMenuItemLastCustomTag,
        WebCore::ContextMenuAction::ContextMenuItemBaseApplicationTag
    >;
};

template<> struct EnumTraits<WebCore::ContextMenuItemType> {
    using values = EnumValues<
        WebCore::ContextMenuItemType,
        WebCore::ContextMenuItemType::ActionType,
        WebCore::ContextMenuItemType::CheckableActionType,
        WebCore::ContextMenuItemType::SeparatorType,
        WebCore::ContextMenuItemType::SubmenuType
    >;
};

} // namespace WTF
