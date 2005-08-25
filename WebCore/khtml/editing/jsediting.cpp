/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "jsediting.h"

#include "css/css_valueimpl.h"
#include "css/cssproperties.h"

#include "htmlediting.h"
#include "khtml_part.h"
#include "selection.h"
#include "misc/hashmap.h"

#if APPLE_CHANGES
#include "KWQKHTMLPart.h"
#endif

using khtml::TypingCommand;
using khtml::HashMap;
using khtml::CaseInsensitiveHash;

namespace DOM {

class DocumentImpl;

namespace {

bool supportsPasteCommand = false;

struct CommandImp {
    bool (*execFn)(KHTMLPart *part, bool userInterface, const DOMString &value);
    bool (*enabledFn)(KHTMLPart *part);
    KHTMLPart::TriState (*stateFn)(KHTMLPart *part);
    DOMString (*valueFn)(KHTMLPart *part);
};

typedef HashMap<DOMStringImpl *, const CommandImp *, CaseInsensitiveHash> CommandMap;

CommandMap *createCommandDictionary();

const CommandImp *commandImp(const DOMString &command)
{
    static CommandMap *commandDictionary = createCommandDictionary();
    return commandDictionary->get(command.implementation());
}

} // anonymous namespace

bool JSEditor::execCommand(const DOMString &command, bool userInterface, const DOMString &value)
{
    const CommandImp *cmd = commandImp(command);
    if (!cmd)
        return false;
    KHTMLPart *part = m_doc->part();
    if (!part)
        return false;
    m_doc->updateLayout();
    return cmd->enabledFn(part) && cmd->execFn(part, userInterface, value);
}

bool JSEditor::queryCommandEnabled(const DOMString &command)
{
    const CommandImp *cmd = commandImp(command);
    if (!cmd)
        return false;
    KHTMLPart *part = m_doc->part();
    if (!part)
        return false;
    m_doc->updateLayout();
    return cmd->enabledFn(part);
}

bool JSEditor::queryCommandIndeterm(const DOMString &command)
{
    const CommandImp *cmd = commandImp(command);
    if (!cmd)
        return false;
    KHTMLPart *part = m_doc->part();
    if (!part)
        return false;
    m_doc->updateLayout();
    return cmd->stateFn(part) == KHTMLPart::mixedTriState;
}

bool JSEditor::queryCommandState(const DOMString &command)
{
    const CommandImp *cmd = commandImp(command);
    if (!cmd)
        return false;
    KHTMLPart *part = m_doc->part();
    if (!part)
        return false;
    m_doc->updateLayout();
    return cmd->stateFn(part) != KHTMLPart::falseTriState;
}

bool JSEditor::queryCommandSupported(const DOMString &command)
{
    if (!supportsPasteCommand && command.qstring().lower() == "paste")
        return false;
    return commandImp(command) != 0;
}

DOMString JSEditor::queryCommandValue(const DOMString &command)
{
    const CommandImp *cmd = commandImp(command);
    if (!cmd)
        return DOMString();
    KHTMLPart *part = m_doc->part();
    if (!part)
        return DOMString();
    m_doc->updateLayout();
    return cmd->valueFn(part);
}

void JSEditor::setSupportsPasteCommand(bool flag)
{
    supportsPasteCommand = flag;
}

// =============================================================================================

// Private stuff, all inside an anonymous namespace.

namespace {

bool execStyleChange(KHTMLPart *part, int propertyID, const DOMString &propertyValue)
{
    CSSMutableStyleDeclarationImpl *style = new CSSMutableStyleDeclarationImpl;
    style->setProperty(propertyID, propertyValue);
    style->ref();
    part->applyStyle(style);
    style->deref();
    return true;
}

bool execStyleChange(KHTMLPart *part, int propertyID, const char *propertyValue)
{
    return execStyleChange(part, propertyID, DOMString(propertyValue));
}

KHTMLPart::TriState stateStyle(KHTMLPart *part, int propertyID, const char *desiredValue)
{
    CSSMutableStyleDeclarationImpl *style = new CSSMutableStyleDeclarationImpl;
    style->setProperty(propertyID, desiredValue);
    style->ref();
    KHTMLPart::TriState state = part->selectionHasStyle(style);
    style->deref();
    return state;
}

bool selectionStartHasStyle(KHTMLPart *part, int propertyID, const char *desiredValue)
{
    CSSMutableStyleDeclarationImpl *style = new CSSMutableStyleDeclarationImpl;
    style->setProperty(propertyID, desiredValue);
    style->ref();
    bool hasStyle = part->selectionStartHasStyle(style);
    style->deref();
    return hasStyle;
}

DOMString valueStyle(KHTMLPart *part, int propertyID)
{
    return part->selectionStartStylePropertyValue(propertyID);
}

// =============================================================================================
//
// execCommand implementations
//

bool execBackColor(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_BACKGROUND_COLOR, value);
}

bool execBold(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    bool isBold = selectionStartHasStyle(part, CSS_PROP_FONT_WEIGHT, "bold");
    return execStyleChange(part, CSS_PROP_FONT_WEIGHT, isBold ? "normal" : "bold");
}

bool execCopy(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->copyToPasteboard();
    return true;
}

bool execCut(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->cutToPasteboard();
    return true;
}

bool execDelete(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    TypingCommand::deleteKeyPressed(part->xmlDocImpl(), part->selectionGranularity() == khtml::WORD);
    return true;
}

bool execForwardDelete(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    TypingCommand::forwardDeleteKeyPressed(part->xmlDocImpl());
    return true;
}

bool execFontName(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_FONT_FAMILY, value);
}

bool execFontSize(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_FONT_SIZE, value);
}

bool execFontSizeDelta(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP__KHTML_FONT_SIZE_DELTA, value);
}

bool execForeColor(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_COLOR, value);
}

bool execIndent(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    // FIXME: Implement.
    return false;
}

bool execInsertLineBreak(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    TypingCommand::insertLineBreak(part->xmlDocImpl());
    return true;
}

bool execInsertParagraph(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    TypingCommand::insertParagraphSeparator(part->xmlDocImpl());
    return true;
}

bool execInsertText(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    TypingCommand::insertText(part->xmlDocImpl(), value);
    return true;
}

bool execItalic(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    bool isItalic = selectionStartHasStyle(part, CSS_PROP_FONT_STYLE, "italic");
    return execStyleChange(part, CSS_PROP_FONT_STYLE, isItalic ? "normal" : "italic");
}

bool execJustifyCenter(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_TEXT_ALIGN, "center");
}

bool execJustifyFull(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_TEXT_ALIGN, "justify");
}

bool execJustifyLeft(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_TEXT_ALIGN, "left");
}

bool execJustifyRight(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_TEXT_ALIGN, "right");
}

bool execOutdent(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    // FIXME: Implement.
    return false;
}

bool execPaste(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->pasteFromPasteboard();
    return true;
}

bool execPasteAndMatchStyle(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->pasteAndMatchStyle();
    return true;
}

bool execPrint(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->print();
    return true;
}

bool execRedo(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->redo();
    return true;
}

bool execSelectAll(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->selectAll();
    return true;
}

bool execSubscript(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_VERTICAL_ALIGN, "sub");
}

bool execSuperscript(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_VERTICAL_ALIGN, "super");
}

bool execTranspose(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->transpose();
    return true;
}

bool execUnderline(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    bool isUnderlined = selectionStartHasStyle(part, CSS_PROP__KHTML_TEXT_DECORATIONS_IN_EFFECT, "underline");
    return execStyleChange(part, CSS_PROP__KHTML_TEXT_DECORATIONS_IN_EFFECT, isUnderlined ? "none" : "underline");
}

bool execUndo(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->undo();
    return true;
}

bool execUnselect(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->clearSelection();
    return true;
}

// =============================================================================================
//
// queryCommandEnabled implementations
//
// It's a bit difficult to get a clear notion of the difference between
// "supported" and "enabled" from reading the Microsoft documentation, but
// what little I could glean from that seems to make some sense.
//     Supported = The command is supported by this object.
//     Enabled =   The command is available and enabled.

bool enabled(KHTMLPart *part)
{
    return true;
}

bool enabledAnySelection(KHTMLPart *part)
{
    return part->selection().isCaretOrRange();
}

bool enabledPaste(KHTMLPart *part)
{
    return supportsPasteCommand && part->canPaste();
}

bool enabledPasteAndMatchStyle(KHTMLPart *part)
{
    return supportsPasteCommand && part->canPaste();
}

bool enabledRangeSelection(KHTMLPart *part)
{
    return part->selection().isRange();
}

bool enabledRedo(KHTMLPart *part)
{
    return part->canRedo();
}

bool enabledUndo(KHTMLPart *part)
{
    return part->canUndo();
}

// =============================================================================================
//
// queryCommandIndeterm/State implementations
//
// It's a bit difficult to get a clear notion of what these methods are supposed
// to do from reading the Microsoft documentation, but my current guess is this:
//
//     queryCommandState and queryCommandIndeterm work in concert to return
//     the two bits of information that are needed to tell, for instance,
//     if the text of a selection is bold. The answer can be "yes", "no", or
//     "partially".
//
// If this is so, then queryCommandState should return "yes" in the case where
// all the text is bold and "no" for non-bold or partially-bold text.
// Then, queryCommandIndeterm should return "no" in the case where
// all the text is either all bold or not-bold and and "yes" for partially-bold text.

KHTMLPart::TriState stateNone(KHTMLPart *part)
{
    return KHTMLPart::falseTriState;
}

KHTMLPart::TriState stateBold(KHTMLPart *part)
{
    return stateStyle(part, CSS_PROP_FONT_WEIGHT, "bold");
}

KHTMLPart::TriState stateItalic(KHTMLPart *part)
{
    return stateStyle(part, CSS_PROP_FONT_STYLE, "italic");
}

KHTMLPart::TriState stateSubscript(KHTMLPart *part)
{
    return stateStyle(part, CSS_PROP_VERTICAL_ALIGN, "sub");
}

KHTMLPart::TriState stateSuperscript(KHTMLPart *part)
{
    return stateStyle(part, CSS_PROP_VERTICAL_ALIGN, "super");
}

KHTMLPart::TriState stateUnderline(KHTMLPart *part)
{
    return stateStyle(part, CSS_PROP_TEXT_DECORATION, "underline");
}

// =============================================================================================
//
// queryCommandValue implementations
//

DOMString valueNull(KHTMLPart *part)
{
    return DOMString();
}

DOMString valueBackColor(KHTMLPart *part)
{
    return valueStyle(part, CSS_PROP_BACKGROUND_COLOR);
}

DOMString valueFontName(KHTMLPart *part)
{
    return valueStyle(part, CSS_PROP_FONT_FAMILY);
}

DOMString valueFontSize(KHTMLPart *part)
{
    return valueStyle(part, CSS_PROP_FONT_SIZE);
}

DOMString valueFontSizeDelta(KHTMLPart *part)
{
    return valueStyle(part, CSS_PROP__KHTML_FONT_SIZE_DELTA);
}

DOMString valueForeColor(KHTMLPart *part)
{
    return valueStyle(part, CSS_PROP_COLOR);
}

// =============================================================================================

CommandMap *createCommandDictionary()
{
    struct EditorCommand { const char *name; CommandImp imp; };

    static const EditorCommand commands[] = {

        { "BackColor", { execBackColor, enabled, stateNone, valueBackColor } },
        { "Bold", { execBold, enabledAnySelection, stateBold, valueNull } },
        { "Copy", { execCopy, enabledRangeSelection, stateNone, valueNull } },
        { "Cut", { execCut, enabledRangeSelection, stateNone, valueNull } },
        { "Delete", { execDelete, enabledAnySelection, stateNone, valueNull } },
        { "FontName", { execFontName, enabledAnySelection, stateNone, valueFontName } },
        { "FontSize", { execFontSize, enabledAnySelection, stateNone, valueFontSize } },
        { "FontSizeDelta", { execFontSizeDelta, enabledAnySelection, stateNone, valueFontSizeDelta } },
        { "ForeColor", { execForeColor, enabledAnySelection, stateNone, valueForeColor } },
        { "ForwardDelete", { execForwardDelete, enabledAnySelection, stateNone, valueNull } },
        { "Indent", { execIndent, enabledAnySelection, stateNone, valueNull } },
        { "InsertLineBreak", { execInsertLineBreak, enabledAnySelection, stateNone, valueNull } },
        { "InsertParagraph", { execInsertParagraph, enabledAnySelection, stateNone, valueNull } },
        { "InsertText", { execInsertText, enabledAnySelection, stateNone, valueNull } },
        { "Italic", { execItalic, enabledAnySelection, stateItalic, valueNull } },
        { "JustifyCenter", { execJustifyCenter, enabledAnySelection, stateNone, valueNull } },
        { "JustifyFull", { execJustifyFull, enabledAnySelection, stateNone, valueNull } },
        { "JustifyLeft", { execJustifyLeft, enabledAnySelection, stateNone, valueNull } },
        { "JustifyNone", { execJustifyLeft, enabledAnySelection, stateNone, valueNull } },
        { "JustifyRight", { execJustifyRight, enabledAnySelection, stateNone, valueNull } },
        { "Outdent", { execOutdent, enabledAnySelection, stateNone, valueNull } },
        { "Paste", { execPaste, enabledPaste, stateNone, valueNull } },
        { "PasteAndMatchStyle", { execPasteAndMatchStyle, enabledPasteAndMatchStyle, stateNone, valueNull } },
        { "Print", { execPrint, enabled, stateNone, valueNull } },
        { "Redo", { execRedo, enabledRedo, stateNone, valueNull } },
        { "SelectAll", { execSelectAll, enabled, stateNone, valueNull } },
        { "Subscript", { execSubscript, enabledAnySelection, stateSubscript, valueNull } },
        { "Superscript", { execSuperscript, enabledAnySelection, stateSuperscript, valueNull } },
        { "Transpose", { execTranspose, enabled, stateNone, valueNull } },
        { "Underline", { execUnderline, enabledAnySelection, stateUnderline, valueNull } },
        { "Undo", { execUndo, enabledUndo, stateNone, valueNull } },
        { "Unselect", { execUnselect, enabledAnySelection, stateNone, valueNull } }

        //
        // The "unsupported" commands are listed here since they appear in the Microsoft
        // documentation used as the basis for the list.
        //

        // 2D-Position (not supported)
        // AbsolutePosition (not supported)
        // BlockDirLTR (not supported)
        // BlockDirRTL (not supported)
        // BrowseMode (not supported)
        // ClearAuthenticationCache (not supported)
        // CreateBookmark (not supported)
        // CreateLink (not supported)
        // DirLTR (not supported)
        // DirRTL (not supported)
        // EditMode (not supported)
        // FormatBlock (not supported)
        // InlineDirLTR (not supported)
        // InlineDirRTL (not supported)
        // InsertButton (not supported)
        // InsertFieldSet (not supported)
        // InsertHorizontalRule (not supported)
        // InsertIFrame (not supported)
        // InsertImage (not supported)
        // InsertInputButton (not supported)
        // InsertInputCheckbox (not supported)
        // InsertInputFileUpload (not supported)
        // InsertInputHidden (not supported)
        // InsertInputImage (not supported)
        // InsertInputPassword (not supported)
        // InsertInputRadio (not supported)
        // InsertInputReset (not supported)
        // InsertInputSubmit (not supported)
        // InsertInputText (not supported)
        // InsertMarquee (not supported)
        // InsertOrderedList (not supported)
        // InsertSelectDropDown (not supported)
        // InsertSelectListBox (not supported)
        // InsertTextArea (not supported)
        // InsertUnorderedList (not supported)
        // LiveResize (not supported)
        // MultipleSelection (not supported)
        // Open (not supported)
        // Overwrite (not supported)
        // PlayImage (not supported)
        // Refresh (not supported)
        // RemoveFormat (not supported)
        // RemoveParaFormat (not supported)
        // SaveAs (not supported)
        // SizeToControl (not supported)
        // SizeToControlHeight (not supported)
        // SizeToControlWidth (not supported)
        // Stop (not supported)
        // StopImage (not supported)
        // Strikethrough (not supported)
        // Unbookmark (not supported)
        // Unlink (not supported)
    };

    CommandMap *commandMap = new CommandMap;

    const int numCommands = sizeof(commands) / sizeof(commands[0]);
    for (int i = 0; i < numCommands; ++i) {
        DOMStringImpl *name = new DOMStringImpl(commands[i].name);
        name->ref();
        commandMap->insert(name, &commands[i].imp);
    }
#ifndef NDEBUG
    supportsPasteCommand = true;
#endif
    return commandMap;
}

} // anonymous namespace

} // namespace DOM
