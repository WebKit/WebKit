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

#include "cssproperties.h"
#include "dom_selection.h"
#include "htmlediting.h"
#include "khtml_part.h"
#include "KWQKHTMLPart.h"
#include "qstring.h"

using khtml::TypingCommand;

namespace DOM {

class DocumentImpl;

namespace {

struct CommandImp {
    bool (*execFn)(KHTMLPart *part, bool userInterface, const DOMString &value);
    bool (*enabledFn)(KHTMLPart *part);
    KHTMLPart::TriState (*stateFn)(KHTMLPart *part);
    DOMString (*valueFn)(KHTMLPart *part);
};

QDict<CommandImp> createCommandDictionary();

const CommandImp *commandImp(const DOMString &command)
{
    static QDict<CommandImp> commandDictionary = createCommandDictionary();
    return commandDictionary.find(command.string());
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

// =============================================================================================

// Private stuff, all inside an anonymous namespace.

namespace {

bool execStyleChange(KHTMLPart *part, int propertyID, const DOMString &propertyValue)
{
    CSSStyleDeclarationImpl *style = new CSSStyleDeclarationImpl(0);
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
    CSSStyleDeclarationImpl *style = new CSSStyleDeclarationImpl(0);
    style->setProperty(propertyID, desiredValue);
    style->ref();
    KHTMLPart::TriState state = part->selectionHasStyle(style);
    style->deref();
    return state;
}

bool selectionStartHasStyle(KHTMLPart *part, int propertyID, const char *desiredValue)
{
    CSSStyleDeclarationImpl *style = new CSSStyleDeclarationImpl(0);
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
    TypingCommand::deleteKeyPressed(part->xmlDocImpl());
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

bool execForeColor(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_COLOR, value);
}

bool execIndent(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    // FIXME: Implement.
    return false;
}

bool execInsertNewline(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    TypingCommand::insertNewline(part->xmlDocImpl());
    return true;
}

bool execInsertParagraph(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    // FIXME: Implement.
    return false;
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

#if SUPPORT_PASTE

bool execPaste(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->pasteFromPasteboard();
    return true;
}

#endif

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
    return part->selection().notEmpty();
}

#if SUPPORT_PASTE

bool enabledPaste(KHTMLPart *part)
{
    return part->canPaste();
}

#endif

bool enabledRangeSelection(KHTMLPart *part)
{
    return part->selection().state() == Selection::RANGE;
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

DOMString valueForeColor(KHTMLPart *part)
{
    return valueStyle(part, CSS_PROP_COLOR);
}

// =============================================================================================

QDict<CommandImp> createCommandDictionary()
{
    struct EditorCommand { const char *name; CommandImp imp; };

    static const EditorCommand commands[] = {

        { "backColor", { execBackColor, enabled, stateNone, valueBackColor } },
        { "bold", { execBold, enabledAnySelection, stateBold, valueNull } },
        { "copy", { execCopy, enabledRangeSelection, stateNone, valueNull } },
        { "cut", { execCut, enabledRangeSelection, stateNone, valueNull } },
        { "delete", { execDelete, enabledAnySelection, stateNone, valueNull } },
        { "fontName", { execFontName, enabledAnySelection, stateNone, valueFontName } },
        { "fontSize", { execFontSize, enabledAnySelection, stateNone, valueFontSize } },
        { "foreColor", { execForeColor, enabledAnySelection, stateNone, valueForeColor } },
        { "indent", { execIndent, enabledAnySelection, stateNone, valueNull } },
        { "insertNewline", { execInsertNewline, enabledAnySelection, stateNone, valueNull } },
        { "insertParagraph", { execInsertParagraph, enabledAnySelection, stateNone, valueNull } },
        { "insertText", { execInsertText, enabledAnySelection, stateNone, valueNull } },
        { "italic", { execItalic, enabledAnySelection, stateItalic, valueNull } },
        { "justifyCenter", { execJustifyCenter, enabledAnySelection, stateNone, valueNull } },
        { "justifyFull", { execJustifyFull, enabledAnySelection, stateNone, valueNull } },
        { "justifyLeft", { execJustifyLeft, enabledAnySelection, stateNone, valueNull } },
        { "justifyNone", { execJustifyLeft, enabledAnySelection, stateNone, valueNull } },
        { "justifyRight", { execJustifyRight, enabledAnySelection, stateNone, valueNull } },
        { "outdent", { execOutdent, enabledAnySelection, stateNone, valueNull } },
#if SUPPORT_PASTE
        { "paste", { execPaste, enabledPaste, stateNone, valueNull } },
#endif
        { "print", { execPrint, enabled, stateNone, valueNull } },
        { "redo", { execRedo, enabledRedo, stateNone, valueNull } },
        { "selectAll", { execSelectAll, enabled, stateNone, valueNull } },
        { "subscript", { execSubscript, enabledAnySelection, stateSubscript, valueNull } },
        { "superscript", { execSuperscript, enabledAnySelection, stateSuperscript, valueNull } },
        { "undo", { execUndo, enabledUndo, stateNone, valueNull } },
        { "unselect", { execUnselect, enabledAnySelection, stateNone, valueNull } }

        //
        // The "unsupported" commands are listed here since they appear in the Microsoft
        // documentation used as the basis for the list.
        //

        // 2d-position (not supported)
        // absolutePosition (not supported)
        // blockDirLTR (not supported)
        // blockDirRTL (not supported)
        // browseMode (not supported)
        // clearAuthenticationCache (not supported)
        // createBookmark (not supported)
        // createLink (not supported)
        // dirLTR (not supported)
        // dirRTL (not supported)
        // editMode (not supported)
        // formatBlock (not supported)
        // inlineDirLTR (not supported)
        // inlineDirRTL (not supported)
        // insertButton (not supported)
        // insertFieldSet (not supported)
        // insertHorizontalRule (not supported)
        // insertIFrame (not supported)
        // insertImage (not supported)
        // insertInputButton (not supported)
        // insertInputCheckbox (not supported)
        // insertInputFileUpload (not supported)
        // insertInputHidden (not supported)
        // insertInputImage (not supported)
        // insertInputPassword (not supported)
        // insertInputRadio (not supported)
        // insertInputReset (not supported)
        // insertInputSubmit (not supported)
        // insertInputText (not supported)
        // insertMarquee (not supported)
        // insertOrderedList (not supported)
        // insertSelectDropDown (not supported)
        // insertSelectListBox (not supported)
        // insertTextArea (not supported)
        // insertUnorderedList (not supported)
        // liveResize (not supported)
        // multipleSelection (not supported)
        // open (not supported)
        // overwrite (not supported)
        // playImage (not supported)
        // refresh (not supported)
        // removeFormat (not supported)
        // removeParaFormat (not supported)
        // saveAs (not supported)
        // sizeToControl (not supported)
        // sizeToControlHeight (not supported)
        // sizeToControlWidth (not supported)
        // stop (not supported)
        // stopimage (not supported)
        // strikethrough (not supported)
        // unbookmark (not supported)
        // underline (not supported)
        // unlink (not supported)
    };

    const int numCommands = sizeof(commands) / sizeof(commands[0]);
    QDict<CommandImp> commandDictionary(numCommands, false); // case-insensitive dictionary
    for (int i = 0; i < numCommands; ++i) {
        commandDictionary.insert(commands[i].name, &commands[i].imp);
    }
    return commandDictionary;
}

} // anonymous namespace

} // namespace DOM
