/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ContextMenuItem.h"

#include "ContextMenu.h"

#if ENABLE(CONTEXT_MENUS)

namespace WebCore {

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu)
    : m_type(type)
    , m_action(action)
    , m_title(title)
    , m_enabled(true)
    , m_checked(false)
    , m_indentationLevel(0)
{
    if (subMenu)
        setSubMenu(subMenu);
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, bool enabled, bool checked, unsigned indentationLevel)
    : m_type(type)
    , m_action(action)
    , m_title(title)
    , m_enabled(enabled)
    , m_checked(checked)
    , m_indentationLevel(indentationLevel)
{
}
        
ContextMenuItem::ContextMenuItem(ContextMenuAction action, const String& title, bool enabled, bool checked, const Vector<ContextMenuItem>& subMenuItems, unsigned indentationLevel)
    : m_type(SubmenuType)
    , m_action(action)
    , m_title(title)
    , m_enabled(enabled)
    , m_checked(checked)
    , m_indentationLevel(indentationLevel)
    , m_subMenuItems(subMenuItems)
{
}

ContextMenuItem::ContextMenuItem()
    : m_type(SeparatorType)
    , m_action(ContextMenuItemTagNoAction)
    , m_enabled(false)
    , m_checked(false)
    , m_indentationLevel(0)
{
}

ContextMenuItem::~ContextMenuItem() = default;

bool ContextMenuItem::isNull() const
{
    // FIXME: This is a bit of a hack. Cross-platform ContextMenuItem users need a concrete way to track "isNull".
    return m_action == ContextMenuItemTagNoAction && m_title.isNull() && m_subMenuItems.isEmpty();
}

void ContextMenuItem::setSubMenu(ContextMenu* subMenu)
{
    if (subMenu) {
        m_type = SubmenuType;
        m_subMenuItems = subMenu->items();
    } else {
        m_type = ActionType;
        m_subMenuItems.clear();
    }
}

void ContextMenuItem::setType(ContextMenuItemType type)
{
    m_type = type;
}

ContextMenuItemType ContextMenuItem::type() const
{
    return m_type;
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
    m_action = action;
}

ContextMenuAction ContextMenuItem::action() const
{
    return m_action;
}

void ContextMenuItem::setIndentationLevel(unsigned indentationLevel)
{
    m_indentationLevel = indentationLevel;
}

unsigned ContextMenuItem::indentationLevel() const
{
    return m_indentationLevel;
}

void ContextMenuItem::setChecked(bool checked)
{
    m_checked = checked;
}

bool ContextMenuItem::checked() const
{
    return m_checked;
}

void ContextMenuItem::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool ContextMenuItem::enabled() const
{
    return m_enabled;
}

static bool isValidContextMenuAction(WebCore::ContextMenuAction action)
{
    switch (action) {
    case ContextMenuAction::ContextMenuItemTagNoAction:
    case ContextMenuAction::ContextMenuItemTagOpenLinkInNewWindow:
    case ContextMenuAction::ContextMenuItemTagDownloadLinkToDisk:
    case ContextMenuAction::ContextMenuItemTagCopyLinkToClipboard:
    case ContextMenuAction::ContextMenuItemTagOpenImageInNewWindow:
    case ContextMenuAction::ContextMenuItemTagDownloadImageToDisk:
    case ContextMenuAction::ContextMenuItemTagCopyImageToClipboard:
    case ContextMenuAction::ContextMenuItemTagCopySubject:
#if PLATFORM(GTK)
    case ContextMenuAction::ContextMenuItemTagCopyImageUrlToClipboard:
#endif
    case ContextMenuAction::ContextMenuItemTagOpenFrameInNewWindow:
    case ContextMenuAction::ContextMenuItemTagCopy:
    case ContextMenuAction::ContextMenuItemTagGoBack:
    case ContextMenuAction::ContextMenuItemTagGoForward:
    case ContextMenuAction::ContextMenuItemTagStop:
    case ContextMenuAction::ContextMenuItemTagReload:
    case ContextMenuAction::ContextMenuItemTagCut:
    case ContextMenuAction::ContextMenuItemTagPaste:
#if PLATFORM(GTK)
    case ContextMenuAction::ContextMenuItemTagPasteAsPlainText:
    case ContextMenuAction::ContextMenuItemTagDelete:
    case ContextMenuAction::ContextMenuItemTagSelectAll:
    case ContextMenuAction::ContextMenuItemTagInputMethods:
    case ContextMenuAction::ContextMenuItemTagUnicode:
    case ContextMenuAction::ContextMenuItemTagUnicodeInsertLRMMark:
    case ContextMenuAction::ContextMenuItemTagUnicodeInsertRLMMark:
    case ContextMenuAction::ContextMenuItemTagUnicodeInsertLREMark:
    case ContextMenuAction::ContextMenuItemTagUnicodeInsertRLEMark:
    case ContextMenuAction::ContextMenuItemTagUnicodeInsertLROMark:
    case ContextMenuAction::ContextMenuItemTagUnicodeInsertRLOMark:
    case ContextMenuAction::ContextMenuItemTagUnicodeInsertPDFMark:
    case ContextMenuAction::ContextMenuItemTagUnicodeInsertZWSMark:
    case ContextMenuAction::ContextMenuItemTagUnicodeInsertZWJMark:
    case ContextMenuAction::ContextMenuItemTagUnicodeInsertZWNJMark:
    case ContextMenuAction::ContextMenuItemTagInsertEmoji:
#endif
    case ContextMenuAction::ContextMenuItemTagSpellingGuess:
    case ContextMenuAction::ContextMenuItemTagNoGuessesFound:
    case ContextMenuAction::ContextMenuItemTagIgnoreSpelling:
    case ContextMenuAction::ContextMenuItemTagLearnSpelling:
    case ContextMenuAction::ContextMenuItemTagOther:
    case ContextMenuAction::ContextMenuItemTagSearchInSpotlight:
    case ContextMenuAction::ContextMenuItemTagSearchWeb:
    case ContextMenuAction::ContextMenuItemTagLookUpInDictionary:
    case ContextMenuAction::ContextMenuItemTagOpenWithDefaultApplication:
    case ContextMenuAction::ContextMenuItemPDFActualSize:
    case ContextMenuAction::ContextMenuItemPDFZoomIn:
    case ContextMenuAction::ContextMenuItemPDFZoomOut:
    case ContextMenuAction::ContextMenuItemPDFAutoSize:
    case ContextMenuAction::ContextMenuItemPDFSinglePage:
    case ContextMenuAction::ContextMenuItemPDFSinglePageContinuous:
    case ContextMenuAction::ContextMenuItemPDFTwoPages:
    case ContextMenuAction::ContextMenuItemPDFTwoPagesContinuous:
    case ContextMenuAction::ContextMenuItemPDFFacingPages:
    case ContextMenuAction::ContextMenuItemPDFContinuous:
    case ContextMenuAction::ContextMenuItemPDFNextPage:
    case ContextMenuAction::ContextMenuItemPDFPreviousPage:
    case ContextMenuAction::ContextMenuItemTagOpenLink:
    case ContextMenuAction::ContextMenuItemTagIgnoreGrammar:
    case ContextMenuAction::ContextMenuItemTagSpellingMenu:
    case ContextMenuAction::ContextMenuItemTagShowSpellingPanel:
    case ContextMenuAction::ContextMenuItemTagCheckSpelling:
    case ContextMenuAction::ContextMenuItemTagCheckSpellingWhileTyping:
    case ContextMenuAction::ContextMenuItemTagCheckGrammarWithSpelling:
    case ContextMenuAction::ContextMenuItemTagFontMenu:
    case ContextMenuAction::ContextMenuItemTagShowFonts:
    case ContextMenuAction::ContextMenuItemTagBold:
    case ContextMenuAction::ContextMenuItemTagItalic:
    case ContextMenuAction::ContextMenuItemTagUnderline:
    case ContextMenuAction::ContextMenuItemTagOutline:
    case ContextMenuAction::ContextMenuItemTagStyles:
    case ContextMenuAction::ContextMenuItemTagShowColors:
    case ContextMenuAction::ContextMenuItemTagSpeechMenu:
    case ContextMenuAction::ContextMenuItemTagStartSpeaking:
    case ContextMenuAction::ContextMenuItemTagStopSpeaking:
    case ContextMenuAction::ContextMenuItemTagWritingDirectionMenu:
    case ContextMenuAction::ContextMenuItemTagDefaultDirection:
    case ContextMenuAction::ContextMenuItemTagLeftToRight:
    case ContextMenuAction::ContextMenuItemTagRightToLeft:
    case ContextMenuAction::ContextMenuItemTagPDFSinglePageScrolling:
    case ContextMenuAction::ContextMenuItemTagPDFFacingPagesScrolling:
    case ContextMenuAction::ContextMenuItemTagInspectElement:
    case ContextMenuAction::ContextMenuItemTagTextDirectionMenu:
    case ContextMenuAction::ContextMenuItemTagTextDirectionDefault:
    case ContextMenuAction::ContextMenuItemTagTextDirectionLeftToRight:
    case ContextMenuAction::ContextMenuItemTagTextDirectionRightToLeft:
    case ContextMenuAction::ContextMenuItemTagAddHighlightToCurrentQuickNote:
    case ContextMenuAction::ContextMenuItemTagAddHighlightToNewQuickNote:
#if PLATFORM(COCOA)
    case ContextMenuAction::ContextMenuItemTagCorrectSpellingAutomatically:
    case ContextMenuAction::ContextMenuItemTagSubstitutionsMenu:
    case ContextMenuAction::ContextMenuItemTagShowSubstitutions:
    case ContextMenuAction::ContextMenuItemTagSmartCopyPaste:
    case ContextMenuAction::ContextMenuItemTagSmartQuotes:
    case ContextMenuAction::ContextMenuItemTagSmartDashes:
    case ContextMenuAction::ContextMenuItemTagSmartLinks:
    case ContextMenuAction::ContextMenuItemTagTextReplacement:
    case ContextMenuAction::ContextMenuItemTagTransformationsMenu:
    case ContextMenuAction::ContextMenuItemTagMakeUpperCase:
    case ContextMenuAction::ContextMenuItemTagMakeLowerCase:
    case ContextMenuAction::ContextMenuItemTagCapitalize:
    case ContextMenuAction::ContextMenuItemTagChangeBack:
#endif
    case ContextMenuAction::ContextMenuItemTagOpenMediaInNewWindow:
    case ContextMenuAction::ContextMenuItemTagDownloadMediaToDisk:
    case ContextMenuAction::ContextMenuItemTagCopyMediaLinkToClipboard:
    case ContextMenuAction::ContextMenuItemTagToggleMediaControls:
    case ContextMenuAction::ContextMenuItemTagToggleMediaLoop:
    case ContextMenuAction::ContextMenuItemTagShowMediaStats:
    case ContextMenuAction::ContextMenuItemTagEnterVideoFullscreen:
    case ContextMenuAction::ContextMenuItemTagMediaPlayPause:
    case ContextMenuAction::ContextMenuItemTagMediaMute:
    case ContextMenuAction::ContextMenuItemTagDictationAlternative:
    case ContextMenuAction::ContextMenuItemTagToggleVideoFullscreen:
    case ContextMenuAction::ContextMenuItemTagShareMenu:
    case ContextMenuAction::ContextMenuItemTagToggleVideoEnhancedFullscreen:
    case ContextMenuAction::ContextMenuItemTagLookUpImage:
    case ContextMenuAction::ContextMenuItemTagTranslate:
    case ContextMenuAction::ContextMenuItemBaseCustomTag:
    case ContextMenuAction::ContextMenuItemLastCustomTag:
    case ContextMenuAction::ContextMenuItemBaseApplicationTag:
    case ContextMenuAction::ContextMenuItemTagPlayAllAnimations:
    case ContextMenuAction::ContextMenuItemTagPauseAllAnimations:
        return true;
    }

    if (action > ContextMenuAction::ContextMenuItemBaseCustomTag && action < ContextMenuAction::ContextMenuItemLastCustomTag)
        return true;

    if (action > ContextMenuAction::ContextMenuItemBaseApplicationTag)
        return true;

    return false;
}

} // namespace WebCore

namespace WTF {

template<> bool isValidEnum<WebCore::ContextMenuAction, void>(std::underlying_type_t<WebCore::ContextMenuAction> action)
{
    return WebCore::isValidContextMenuAction(static_cast<WebCore::ContextMenuAction>(action));
}

}

#endif // ENABLE(CONTEXT_MENUS)
