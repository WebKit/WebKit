/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ewk_context_menu_item.h"

#include "ewk_context_menu_item_private.h"
#include "ewk_private.h"
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace WebKit;

EwkContextMenuItem::EwkContextMenuItem(const WebContextMenuItemData& item)
    : m_type(static_cast<Ewk_Context_Menu_Item_Type>(item.type()))
    , m_action(static_cast<Ewk_Context_Menu_Item_Action>(item.action()))
    , m_title(item.title().utf8().data())
    , m_isChecked(item.checked())
    , m_isEnabled(item.enabled())
    , m_parentMenu(0)
    , m_subMenu(0)
{
}

EwkContextMenuItem::EwkContextMenuItem(Ewk_Context_Menu_Item_Type type, Ewk_Context_Menu_Item_Action action, const char* title, Eina_Bool checked, Eina_Bool enabled, Ewk_Context_Menu* subMenu)
    : m_type(type)
    , m_action(action)
    , m_title(title)
    , m_isChecked(checked)
    , m_isEnabled(enabled)
    , m_parentMenu(0)
    , m_subMenu(subMenu)
{
}

Ewk_Context_Menu_Item* ewk_context_menu_item_new(Ewk_Context_Menu_Item_Type type, Ewk_Context_Menu_Item_Action action, const char* title, Eina_Bool checked, Eina_Bool enabled)
{
    return Ewk_Context_Menu_Item::create(type, action, title, checked, enabled, 0).leakPtr();
}

Ewk_Context_Menu_Item* ewk_context_menu_item_new_with_submenu(Ewk_Context_Menu_Item_Type type, Ewk_Context_Menu_Item_Action action, const char* title, Eina_Bool checked, Eina_Bool enabled, Ewk_Context_Menu* subMenu)
{
    return Ewk_Context_Menu_Item::create(type, action, title, checked, enabled, subMenu).leakPtr();
}

Ewk_Context_Menu_Item_Type ewk_context_menu_item_type_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, EWK_ACTION_TYPE);

    return item->type();
}

Eina_Bool ewk_context_menu_item_type_set(Ewk_Context_Menu_Item* item, Ewk_Context_Menu_Item_Type type)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);

    item->setType(type);

    return true;
}

Ewk_Context_Menu_Item_Action ewk_context_menu_item_action_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, EWK_CONTEXT_MENU_ITEM_TAG_NO_ACTION);

    return item->action();
}

Eina_Bool ewk_context_menu_item_action_set(Ewk_Context_Menu_Item* item, Ewk_Context_Menu_Item_Action action)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);

    item->setAction(action);

    return true;
}

const char* ewk_context_menu_item_title_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, 0);

    return item->title();
}

Eina_Bool ewk_context_menu_item_title_set(Ewk_Context_Menu_Item* item, const char* title)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);

    item->setTitle(title);

    return true;
}

Eina_Bool ewk_context_menu_item_checked_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);

    return item->checked();
}

Eina_Bool ewk_context_menu_item_checked_set(Ewk_Context_Menu_Item* item, Eina_Bool checked)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);

    item->setChecked(checked);

    return true;
}

Eina_Bool ewk_context_menu_item_enabled_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);
    
    return item->enabled();
}

Eina_Bool ewk_context_menu_item_enabled_set(Ewk_Context_Menu_Item* item, Eina_Bool enabled)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);

    item->setEnabled(enabled);

    return true;
}

COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_NO_ACTION, WebCore::ContextMenuItemTagNoAction);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_OPEN_LINK_IN_NEW_WINDOW, WebCore::ContextMenuItemTagOpenLinkInNewWindow);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_DOWNLOAD_LINK_TO_DISK, WebCore::ContextMenuItemTagDownloadLinkToDisk);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_COPY_LINK_TO_CLIPBOARD, WebCore::ContextMenuItemTagCopyLinkToClipboard);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_OPEN_IMAGE_IN_NEW_WINDOW, WebCore::ContextMenuItemTagOpenImageInNewWindow);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_DOWNLOAD_IMAGE_TO_DISK, WebCore::ContextMenuItemTagDownloadImageToDisk);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_COPY_IMAGE_TO_CLIPBOARD, WebCore::ContextMenuItemTagCopyImageToClipboard);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_COPY_IMAGE_URL_TO_CLIPBOARD, WebCore::ContextMenuItemTagCopyImageUrlToClipboard);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_OPEN_FRAME_IN_NEW_WINDOW, WebCore::ContextMenuItemTagOpenFrameInNewWindow);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_COPY, WebCore::ContextMenuItemTagCopy);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_GO_BACK, WebCore::ContextMenuItemTagGoBack);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_GO_FORWARD, WebCore::ContextMenuItemTagGoForward);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_STOP, WebCore::ContextMenuItemTagStop);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_RELOAD, WebCore::ContextMenuItemTagReload);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_CUT, WebCore::ContextMenuItemTagCut);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_PASTE, WebCore::ContextMenuItemTagPaste);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_SELECT_ALL, WebCore::ContextMenuItemTagSelectAll);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_SPELLING_GUESS, WebCore::ContextMenuItemTagSpellingGuess);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_NO_GUESSES_FOUND, WebCore::ContextMenuItemTagNoGuessesFound);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_IGNORE_SPELLING, WebCore::ContextMenuItemTagIgnoreSpelling);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_LEARN_SPELLING, WebCore::ContextMenuItemTagLearnSpelling);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_OTHER, WebCore::ContextMenuItemTagOther);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_SEARCH_IN_SPOTLIGHT, WebCore::ContextMenuItemTagSearchInSpotlight);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_SEARCH_WEB, WebCore::ContextMenuItemTagSearchWeb);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_LOOK_UP_IN_DICTIONARY, WebCore::ContextMenuItemTagLookUpInDictionary);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_OPEN_WITH_DEFAULT_APPLICATION, WebCore::ContextMenuItemTagOpenWithDefaultApplication);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_PDFACTUAL_SIZE, WebCore::ContextMenuItemPDFActualSize);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_PDFZOOM_IN, WebCore::ContextMenuItemPDFZoomIn);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_PDFZOOM_OUT, WebCore::ContextMenuItemPDFZoomOut);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_PDFAUTO_SIZE, WebCore::ContextMenuItemPDFAutoSize);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_PDFSINGLE_PAGE, WebCore::ContextMenuItemPDFSinglePage);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_PDFFACING_PAGES, WebCore::ContextMenuItemPDFFacingPages);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_PDFCONTINUOUS, WebCore::ContextMenuItemPDFContinuous);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_PDFNEXT_PAGE, WebCore::ContextMenuItemPDFNextPage);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_PDFPREVIOUS_PAGE, WebCore::ContextMenuItemPDFPreviousPage);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_OPEN_LINK, WebCore::ContextMenuItemTagOpenLink);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_IGNORE_GRAMMAR, WebCore::ContextMenuItemTagIgnoreGrammar);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_SPELLING_MENU, WebCore::ContextMenuItemTagSpellingMenu);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_SHOW_SPELLING_PANEL, WebCore::ContextMenuItemTagShowSpellingPanel);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_CHECK_SPELLING, WebCore::ContextMenuItemTagCheckSpelling);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_CHECK_SPELLING_WHILE_TYPING, WebCore::ContextMenuItemTagCheckSpellingWhileTyping);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_CHECK_GRAMMAR_WITH_SPELLING, WebCore::ContextMenuItemTagCheckGrammarWithSpelling);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_FONT_MENU, WebCore::ContextMenuItemTagFontMenu);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_SHOW_FONTS, WebCore::ContextMenuItemTagShowFonts);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_BOLD, WebCore::ContextMenuItemTagBold);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_ITALIC, WebCore::ContextMenuItemTagItalic);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_UNDERLINE, WebCore::ContextMenuItemTagUnderline);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_OUTLINE, WebCore::ContextMenuItemTagOutline);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_STYLES, WebCore::ContextMenuItemTagStyles);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_SHOW_COLORS, WebCore::ContextMenuItemTagShowColors);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_SPEECH_MENU, WebCore::ContextMenuItemTagSpeechMenu);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_START_SPEAKING, WebCore::ContextMenuItemTagStartSpeaking);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_STOP_SPEAKING, WebCore::ContextMenuItemTagStopSpeaking);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_WRITING_DIRECTION_MENU, WebCore::ContextMenuItemTagWritingDirectionMenu);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_DEFAULT_DIRECTION, WebCore::ContextMenuItemTagDefaultDirection);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_LEFT_TO_RIGHT, WebCore::ContextMenuItemTagLeftToRight);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_RIGHT_TO_LEFT, WebCore::ContextMenuItemTagRightToLeft);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_PDFSINGLE_PAGE_SCROLLING, WebCore::ContextMenuItemTagPDFSinglePageScrolling);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_PDFFACING_PAGES_SCROLLING, WebCore::ContextMenuItemTagPDFFacingPagesScrolling);
#if ENABLE(INSPECTOR)
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_INSPECT_ELEMENT, WebCore::ContextMenuItemTagInspectElement);
#endif
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_MENU, WebCore::ContextMenuItemTagTextDirectionMenu);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_DEFAULT, WebCore::ContextMenuItemTagTextDirectionDefault);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_LEFT_TO_RIGHT, WebCore::ContextMenuItemTagTextDirectionLeftToRight);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_RIGHT_TO_LEFT, WebCore::ContextMenuItemTagTextDirectionRightToLeft);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_OPEN_MEDIA_IN_NEW_WINDOW, WebCore::ContextMenuItemTagOpenMediaInNewWindow);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_COPY_MEDIA_LINK_TO_CLIPBOARD, WebCore::ContextMenuItemTagCopyMediaLinkToClipboard);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_TOGGLE_MEDIA_CONTROLS, WebCore::ContextMenuItemTagToggleMediaControls);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_TOGGLE_MEDIA_LOOP, WebCore::ContextMenuItemTagToggleMediaLoop);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_ENTER_VIDEO_FULLSCREEN, WebCore::ContextMenuItemTagEnterVideoFullscreen);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_MEDIA_PLAY_PAUSE, WebCore::ContextMenuItemTagMediaPlayPause);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_TAG_MEDIA_MUTE, WebCore::ContextMenuItemTagMediaMute);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_BASE_CUSTOM_TAG, WebCore::ContextMenuItemBaseCustomTag);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_CUSTOM_TAG_NO_ACTION, WebCore::ContextMenuItemCustomTagNoAction);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_LAST_CUSTOM_TAG, WebCore::ContextMenuItemLastCustomTag);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CONTEXT_MENU_ITEM_BASE_APPLICATION_TAG, WebCore::ContextMenuItemBaseApplicationTag);
