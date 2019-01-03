/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebEditCommandProxy.h"

#include "UndoOrRedo.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/LocalizedStrings.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

WebEditCommandProxy::WebEditCommandProxy(WebUndoStepID commandID, WebCore::EditAction editAction, WebPageProxy* page)
    : m_commandID(commandID)
    , m_editAction(editAction)
    , m_page(page)
{
    m_page->addEditCommand(this);
}

WebEditCommandProxy::~WebEditCommandProxy()
{
    if (m_page)
        m_page->removeEditCommand(this);
}

void WebEditCommandProxy::unapply()
{
    if (!m_page || !m_page->isValid())
        return;

    m_page->process().send(Messages::WebPage::UnapplyEditCommand(m_commandID), m_page->pageID(), IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    m_page->registerEditCommand(*this, UndoOrRedo::Redo);
}

void WebEditCommandProxy::reapply()
{
    if (!m_page || !m_page->isValid())
        return;

    m_page->process().send(Messages::WebPage::ReapplyEditCommand(m_commandID), m_page->pageID(), IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    m_page->registerEditCommand(*this, UndoOrRedo::Undo);
}

String WebEditCommandProxy::nameForEditAction(EditAction editAction)
{
    // FIXME: This is identical to code in WebKit's WebEditorClient class; would be nice to share the strings instead of having two copies.
    switch (editAction) {
    case EditAction::Unspecified:
    case EditAction::Insert:
    case EditAction::InsertReplacement:
    case EditAction::InsertFromDrop:
        return String();
    case EditAction::SetColor:
        return WEB_UI_STRING_KEY("Set Color", "Set Color (Undo action name)", "Undo action name");
    case EditAction::SetBackgroundColor:
        return WEB_UI_STRING_KEY("Set Background Color", "Set Background Color (Undo action name)", "Undo action name");
    case EditAction::TurnOffKerning:
        return WEB_UI_STRING_KEY("Turn Off Kerning", "Turn Off Kerning (Undo action name)", "Undo action name");
    case EditAction::TightenKerning:
        return WEB_UI_STRING_KEY("Tighten Kerning", "Tighten Kerning (Undo action name)", "Undo action name");
    case EditAction::LoosenKerning:
        return WEB_UI_STRING_KEY("Loosen Kerning", "Loosen Kerning (Undo action name)", "Undo action name");
    case EditAction::UseStandardKerning:
        return WEB_UI_STRING_KEY("Use Standard Kerning", "Use Standard Kerning (Undo action name)", "Undo action name");
    case EditAction::TurnOffLigatures:
        return WEB_UI_STRING_KEY("Turn Off Ligatures", "Turn Off Ligatures (Undo action name)", "Undo action name");
    case EditAction::UseStandardLigatures:
        return WEB_UI_STRING_KEY("Use Standard Ligatures", "Use Standard Ligatures (Undo action name)", "Undo action name");
    case EditAction::UseAllLigatures:
        return WEB_UI_STRING_KEY("Use All Ligatures", "Use All Ligatures (Undo action name)", "Undo action name");
    case EditAction::RaiseBaseline:
        return WEB_UI_STRING_KEY("Raise Baseline", "Raise Baseline (Undo action name)", "Undo action name");
    case EditAction::LowerBaseline:
        return WEB_UI_STRING_KEY("Lower Baseline", "Lower Baseline (Undo action name)", "Undo action name");
    case EditAction::SetTraditionalCharacterShape:
        return WEB_UI_STRING_KEY("Set Traditional Character Shape", "Set Traditional Character Shape (Undo action name)", "Undo action name");
    case EditAction::SetFont:
        return WEB_UI_STRING_KEY("Set Font", "Set Font (Undo action name)", "Undo action name");
    case EditAction::ChangeAttributes:
        return WEB_UI_STRING_KEY("Change Attributes", "Change Attributes (Undo action name)", "Undo action name");
    case EditAction::AlignLeft:
        return WEB_UI_STRING_KEY("Align Left", "Align Left (Undo action name)", "Undo action name");
    case EditAction::AlignRight:
        return WEB_UI_STRING_KEY("Align Right", "Align Right (Undo action name)", "Undo action name");
    case EditAction::Center:
        return WEB_UI_STRING_KEY("Center", "Center (Undo action name)", "Undo action name");
    case EditAction::Justify:
        return WEB_UI_STRING_KEY("Justify", "Justify (Undo action name)", "Undo action name");
    case EditAction::SetWritingDirection:
        return WEB_UI_STRING_KEY("Set Writing Direction", "Set Writing Direction (Undo action name)", "Undo action name");
    case EditAction::Subscript:
        return WEB_UI_STRING_KEY("Subscript", "Subscript (Undo action name)", "Undo action name");
    case EditAction::Superscript:
        return WEB_UI_STRING_KEY("Superscript", "Superscript (Undo action name)", "Undo action name");
    case EditAction::Underline:
        return WEB_UI_STRING_KEY("Underline", "Underline (Undo action name)", "Undo action name");
    case EditAction::Outline:
        return WEB_UI_STRING_KEY("Outline", "Outline (Undo action name)", "Undo action name");
    case EditAction::Unscript:
        return WEB_UI_STRING_KEY("Unscript", "Unscript (Undo action name)", "Undo action name");
    case EditAction::DeleteByDrag:
        return WEB_UI_STRING_KEY("Drag", "Drag (Undo action name)", "Undo action name");
    case EditAction::Cut:
        return WEB_UI_STRING_KEY("Cut", "Cut (Undo action name)", "Undo action name");
    case EditAction::Bold:
        return WEB_UI_STRING_KEY("Bold", "Bold (Undo action name)", "Undo action name");
    case EditAction::Italics:
        return WEB_UI_STRING_KEY("Italics", "Italics (Undo action name)", "Undo action name");
    case EditAction::Delete:
        return WEB_UI_STRING_KEY("Delete", "Delete (Undo action name)", "Undo action name");
    case EditAction::Dictation:
        return WEB_UI_STRING_KEY("Dictation", "Dictation (Undo action name)", "Undo action name");
    case EditAction::Paste:
        return WEB_UI_STRING_KEY("Paste", "Paste (Undo action name)", "Undo action name");
    case EditAction::PasteFont:
        return WEB_UI_STRING_KEY("Paste Font", "Paste Font (Undo action name)", "Undo action name");
    case EditAction::PasteRuler:
        return WEB_UI_STRING_KEY("Paste Ruler", "Paste Ruler (Undo action name)", "Undo action name");
    case EditAction::TypingDeleteSelection:
    case EditAction::TypingDeleteBackward:
    case EditAction::TypingDeleteForward:
    case EditAction::TypingDeleteWordBackward:
    case EditAction::TypingDeleteWordForward:
    case EditAction::TypingDeleteLineBackward:
    case EditAction::TypingDeleteLineForward:
    case EditAction::TypingDeletePendingComposition:
    case EditAction::TypingDeleteFinalComposition:
    case EditAction::TypingInsertText:
    case EditAction::TypingInsertLineBreak:
    case EditAction::TypingInsertParagraph:
    case EditAction::TypingInsertPendingComposition:
    case EditAction::TypingInsertFinalComposition:
        return WEB_UI_STRING_KEY("Typing", "Typing (Undo action name)", "Undo action name");
    case EditAction::CreateLink:
        return WEB_UI_STRING_KEY("Create Link", "Create Link (Undo action name)", "Undo action name");
    case EditAction::Unlink:
        return WEB_UI_STRING_KEY("Unlink", "Unlink (Undo action name)", "Undo action name");
    case EditAction::InsertUnorderedList:
    case EditAction::InsertOrderedList:
        return WEB_UI_STRING_KEY("Insert List", "Insert List (Undo action name)", "Undo action name");
    case EditAction::FormatBlock:
        return WEB_UI_STRING_KEY("Formatting", "Format Block (Undo action name)", "Undo action name");
    case EditAction::Indent:
        return WEB_UI_STRING_KEY("Indent", "Indent (Undo action name)", "Undo action name");
    case EditAction::Outdent:
        return WEB_UI_STRING_KEY("Outdent", "Outdent (Undo action name)", "Undo action name");
    // FIXME: We should give internal clients a way to override these undo names. For instance, Mail refers to ordered and unordered lists as "numbered" and "bulleted" lists, respectively,
    // despite the fact that ordered and unordered lists are not necessarily displayed using bullets and numerals.
    case EditAction::ConvertToOrderedList:
        return WEB_UI_STRING_KEY("Convert to Ordered List", "Convert to Ordered List (Undo action name)", "Undo action name");
    case EditAction::ConvertToUnorderedList:
        return WEB_UI_STRING_KEY("Convert to Unordered List", "Convert to Unordered List (Undo action name)", "Undo action name");
    case EditAction::InsertEditableImage:
        return WEB_UI_STRING_KEY("Insert Drawing", "Insert Drawing (Undo action name)", "Undo action name");
    }
    return String();
}

} // namespace WebKit
