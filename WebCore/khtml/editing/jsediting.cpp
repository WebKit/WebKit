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

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#else
#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)
#define LOG(channel, formatAndArgs...) ((void)0)
#define ERROR(formatAndArgs...) ((void)0)
#endif

using khtml::ApplyStyleCommand;
using khtml::TypingCommand;

namespace DOM {

class DocumentImpl;

namespace {

enum CommandState { no, yes, partial };

struct CommandImp {
    bool (*execFn)(KHTMLPart *part, bool userInterface, const DOMString &value);
    bool (*enabledFn)(KHTMLPart *part);
    CommandState (*stateFn)(KHTMLPart *part);
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
    ASSERT(cmd->enabledFn);
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
    return cmd->stateFn(part) == partial;
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
    return cmd->stateFn(part) != no;
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
    // FIXME: This should share code with WebCoreBridge applyStyle: -- maybe a method on KHTMLPart?
    switch (part->selection().state()) {
        case Selection::NONE:
            // do nothing
            break;
        case Selection::CARET:
            part->setTypingStyle(style);
            break;
        case Selection::RANGE:
            ApplyStyleCommand(part->xmlDocImpl(), style).apply();
            break;
    }
    style->deref();
    return true;
}

bool execStyleChange(KHTMLPart *part, int propertyID, const char *propertyValue)
{
    return execStyleChange(part, propertyID, DOMString(propertyValue));
}

CommandState stateStyle(KHTMLPart *part, int propertyID, const char *desiredValue)
{
    // FIXME: Needs implementation.
    return no;
}

// =============================================================================================
//
// execCommand implementations
//
// EDIT FIXME: All these responses should be tested against the behavior
// of Microsoft browsers to ensure we are as compatible with their
// behavior as is sensible.

bool execBackColor(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    return execStyleChange(part, CSS_PROP_BACKGROUND_COLOR, value);
}

bool execBold(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    bool isBold = false; // FIXME: Needs implementation.
    return execStyleChange(part, CSS_PROP_FONT_WEIGHT, isBold ? "normal" : "bold");
}

bool execCopy(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    // FIXME: Should have a non-KWQ-specific way to do this.
    KWQ(part)->issueCopyCommand();
    return true;
}

bool execCut(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    // FIXME: Should have a non-KWQ-specific way to do this.
    KWQ(part)->issueCutCommand();
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
    bool isItalic = false; // FIXME: Needs implementation.
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
    // FIXME: Should have a non-KWQ-specific way to do this.
    KWQ(part)->issuePasteCommand();
    return true;
}

#endif

bool execPrint(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    // FIXME: Implement.
    return false;
}

bool execRedo(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    // FIXME: Should have a non-KWQ-specific way to do this.
    KWQ(part)->issueRedoCommand();
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
    // FIXME: Should have a non-KWQ-specific way to do this.
    KWQ(part)->issueUndoCommand();
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
// EDIT FIXME: All these responses should be tested against the behavior
// of Microsoft browsers to ensure we are as compatible with their
// behavior as is sensible. For now, the returned values are just my best
// guesses.
//
// It's a bit confusing to get a clear notion of the difference between
// "supported" and "enabled" from reading the Microsoft documentation, but
// what little I could glean from that seems to make some sense.
//     Supported = The command is supported by this object.
//     Enabled =   The command is available and enabled.
//
// With this definition, the different commands return true or false
// based on simple, and for now incomplete, checks on the part and
// selection.

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
    // FIXME: Should also check if there is something on the pasteboard to paste
    return part->selection().notEmpty();
}

#endif

bool enabledRangeSelection(KHTMLPart *part)
{
    return part->selection().state() == Selection::RANGE;
}

bool enabledRedo(KHTMLPart *part)
{
    // EDIT FIXME: Should check if the undo manager has something to undo
    return true;
}

bool enabledUndo(KHTMLPart *part)
{
    // EDIT FIXME: Should check if the undo manager has something to redo
    return true;
}

// =============================================================================================
//
// queryCommandIndeterm implementations
//
// EDIT FIXME: All these responses should be tested against the behavior
// of Microsoft browsers to ensure we are as compatible with their
// behavior as is sensible. For now, the returned values are just my best
// guesses.
//
// It's a bit confusing to get a clear notion of what this method is supposed
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
// all the text is either all bold or not-bold and and "yes" for partially-bold
// text.
//
// Note that, for now, the returned values are just place-holders.

// =============================================================================================
//
// queryCommandState implementations
//
// EDIT FIXME: All these responses should be tested against the behavior
// of Microsoft browsers to ensure we are as compatible with their
// behavior as is sensible. For now, the returned values are just my best
// guesses.
//
// It's a bit confusing to get a clear notion of what this method is supposed
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
// all the text is either all bold or not-bold and and "yes" for partially-bold
// text.
//
// Note that, for now, the returned values are just place-holders.

CommandState stateNo(KHTMLPart *part)
{
    return no;
}

CommandState stateBold(KHTMLPart *part)
{
    return stateStyle(part, CSS_PROP_FONT_WEIGHT, "bold");
}

CommandState stateItalic(KHTMLPart *part)
{
    return stateStyle(part, CSS_PROP_FONT_STYLE, "italic");
}

CommandState stateSubscript(KHTMLPart *part)
{
    return stateStyle(part, CSS_PROP_VERTICAL_ALIGN, "sub");
}

CommandState stateSuperscript(KHTMLPart *part)
{
    return stateStyle(part, CSS_PROP_VERTICAL_ALIGN, "super");
}

// =============================================================================================
//
// queryCommandValue implementations
//
// EDIT FIXME: All these responses should be tested against the behavior
// of Microsoft browsers to ensure we are as compatible with their
// behavior as is sensible. For now, the returned values are just place-holders.

DOMString valueNull(KHTMLPart *part)
{
    return DOMString();
}

DOMString valueBackColor(KHTMLPart *part)
{
    // FIXME: Implement.
    return DOMString();
}

DOMString valueFontName(KHTMLPart *part)
{
    // FIXME: Implement.
    return DOMString();
}

DOMString valueFontSize(KHTMLPart *part)
{
    // FIXME: Implement.
    return DOMString();
}

DOMString valueForeColor(KHTMLPart *part)
{
    // FIXME: Implement.
    return DOMString();
}

// =============================================================================================

QDict<CommandImp> createCommandDictionary()
{
    //
    // All commands are listed with a "supported" or "not supported" label.
    //
    // The "supported" commands need to have all their functions implemented. These bugs
    // correspond to this work.
    // <rdar://problem/3675867>: "Make execCommand work as specified in the Javascript execCommand Compatibility Plan"
    // <rdar://problem/3675898>: "Make queryCommandEnabled work as specified in the Javascript execCommand Compatibility Plan"
    // <rdar://problem/3675899>: "Make queryCommandIndeterm work as specified in the Javascript execCommand Compatibility Plan"
    // <rdar://problem/3675901>: "Make queryCommandState work as specified in the Javascript execCommand Compatibility Plan"
    // <rdar://problem/3675903>: "Make queryCommandSupported work as specified in the Javascript execCommand Compatibility Plan"
    // <rdar://problem/3675904>: "Make queryCommandValue work as specified in the Javascript execCommand Compatibility Plan"
    //
    // The "unsupported" commands are listed here since they appear in the Microsoft
    // documentation used as the basis for the list.
    //

    struct EditorCommand { const char *name; CommandImp imp; };

    static const EditorCommand commands[] = {

        { "backColor", { execBackColor, enabled, stateNo, valueBackColor } },
        { "bold", { execBold, enabledAnySelection, stateBold, valueNull } },
        { "copy", { execCopy, enabledRangeSelection, stateNo, valueNull } },
        { "cut", { execCut, enabledRangeSelection, stateNo, valueNull } },
        { "delete", { execDelete, enabledAnySelection, stateNo, valueNull } },
        { "fontName", { execFontName, enabledAnySelection, stateNo, valueFontName } },
        { "fontSize", { execFontSize, enabledAnySelection, stateNo, valueFontSize } },
        { "foreColor", { execForeColor, enabledAnySelection, stateNo, valueForeColor } },
        { "indent", { execIndent, enabledAnySelection, stateNo, valueNull } },
        { "insertParagraph", { execInsertParagraph, enabledAnySelection, stateNo, valueNull } },
        { "insertText", { execInsertText, enabledAnySelection, stateNo, valueNull } },
        { "italic", { execItalic, enabledAnySelection, stateItalic, valueNull } },
        { "justifyCenter", { execJustifyCenter, enabledAnySelection, stateNo, valueNull } },
        { "justifyFull", { execJustifyFull, enabledAnySelection, stateNo, valueNull } },
        { "justifyLeft", { execJustifyLeft, enabledAnySelection, stateNo, valueNull } },
        { "justifyNone", { execJustifyLeft, enabledAnySelection, stateNo, valueNull } },
        { "justifyRight", { execJustifyRight, enabledAnySelection, stateNo, valueNull } },
        { "outdent", { execOutdent, enabledAnySelection, stateNo, valueNull } },
#if SUPPORT_PASTE
        { "paste", { execPaste, enabledPaste, stateNo, valueNull } },
#endif
        { "print", { execPrint, enabled, stateNo, valueNull } },
        { "redo", { execRedo, enabledRedo, stateNo, valueNull } },
        { "selectAll", { execSelectAll, enabled, stateNo, valueNull } },
        { "subscript", { execSubscript, enabledAnySelection, stateSubscript, valueNull } },
        { "superscript", { execSuperscript, enabledAnySelection, stateSuperscript, valueNull } },
        { "undo", { execUndo, enabledUndo, stateNo, valueNull } },
        { "unselect", { execUnselect, enabledAnySelection, stateNo, valueNull } }

        // 2d-position (not supported)
        // absoluteposition (not supported)
        // blockdirltr (not supported)
        // blockdirrtl (not supported)
        // browsemode (not supported)
        // clearauthenticationcache (not supported)
        // createbookmark (not supported)
        // createlink (not supported)
        // dirltr (not supported)
        // dirrtl (not supported)
        // editmode (not supported)
        // formatblock (not supported)
        // inlinedirltr (not supported)
        // inlinedirrtl (not supported)
        // insertbutton (not supported)
        // insertfieldset (not supported)
        // inserthorizontalrule (not supported)
        // insertiframe (not supported)
        // insertimage (not supported)
        // insertinputbutton (not supported)
        // insertinputcheckbox (not supported)
        // insertinputfileupload (not supported)
        // insertinputhidden (not supported)
        // insertinputimage (not supported)
        // insertinputpassword (not supported)
        // insertinputradio (not supported)
        // insertinputreset (not supported)
        // insertinputsubmit (not supported)
        // insertinputtext (not supported)
        // insertmarquee (not supported)
        // insertorderedlist (not supported)
        // insertselectdropdown (not supported)
        // insertselectlistbox (not supported)
        // inserttextarea (not supported)
        // insertunorderedlist (not supported)
        // liveresize (not supported)
        // multipleselection (not supported)
        // open (not supported)
        // overwrite (not supported)
        // playimage (not supported)
        // refresh (not supported)
        // removeformat (not supported)
        // removeparaformat (not supported)
        // saveas (not supported)
        // sizetocontrol (not supported)
        // sizetocontrolheight (not supported)
        // sizetocontrolwidth (not supported)
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
