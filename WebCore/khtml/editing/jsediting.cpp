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

using khtml::TypingCommand;

namespace DOM {

class DocumentImpl;

const JSEditor::execCommandFn NoExec = 0;
const JSEditor::queryBoolFn NoEnabled = 0;
const JSEditor::queryBoolFn NoIndeterm = 0;
const JSEditor::queryBoolFn NoState = 0;
const JSEditor::queryValueFn NoValue = 0;

QDict<JSEditor::CommandImp> &JSEditor::commandDict()
{
    static QDict<CommandImp> dict;
    return dict;
}

JSEditor::JSEditor(DocumentImpl *doc) : m_doc(doc)
{
    initDict();
}

JSEditor::CommandImp *JSEditor::commandImp(const DOMString &command)
{
    return commandDict().find(command.string().lower());
}

bool JSEditor::execCommand(const DOMString &command, bool userInterface, const DOMString &value)
{
    CommandImp *cmd = commandImp(command);
    if (!cmd || !cmd->execFn)
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
    CommandImp *cmd = commandImp(command);
    if (!cmd || !cmd->enabledFn)
        return false;
    KHTMLPart *part = m_doc->part();
    if (!part)
        return false;
    m_doc->updateLayout();
    return cmd->enabledFn(part);
}

bool JSEditor::queryCommandIndeterm(const DOMString &command)
{
    CommandImp *cmd = commandImp(command);
    if (!cmd || !cmd->indetermFn)
        return false;
    KHTMLPart *part = m_doc->part();
    if (!part)
        return false;
    m_doc->updateLayout();
    return cmd->indetermFn(part);
}

bool JSEditor::queryCommandState(const DOMString &command)
{
    CommandImp *cmd = commandImp(command);
    if (!cmd || !cmd->stateFn)
        return false;
    KHTMLPart *part = m_doc->part();
    if (!part)
        return false;
    m_doc->updateLayout();
    return cmd->stateFn(part);
}

bool JSEditor::queryCommandSupported(const DOMString &command)
{
    return commandImp(command) != 0;
}

DOMString JSEditor::queryCommandValue(const DOMString &command)
{
    CommandImp *cmd = commandImp(command);
    if (!cmd || !cmd->valueFn)
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

// =============================================================================================
//
// execCommand implementations
//
// EDIT FIXME: All these responses should be tested against the behavior
// of Microsoft browsers to ensure we are as compatible with their
// behavior as is sensible.

bool execCommandCopy(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    KWQ(part)->issueCopyCommand();
    return true;
}

bool execCommandCut(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    KWQ(part)->issueCutCommand();
    return true;
}

bool execCommandDelete(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    TypingCommand::deleteKeyPressed(part->xmlDocImpl());
    return true;
}

bool execCommandInsertText(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    TypingCommand::insertText(part->xmlDocImpl(), value);
    return true;
}

bool execCommandPaste(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    KWQ(part)->issuePasteCommand();
    return true;
}

bool execCommandRedo(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    KWQ(part)->issueRedoCommand();
    return true;
}

bool execCommandSelectAll(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    part->selectAll();
    return true;
}

bool execCommandUndo(KHTMLPart *part, bool userInterface, const DOMString &value)
{
    KWQ(part)->issueUndoCommand();
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

bool enabledIfSelectionNotEmpty(KHTMLPart *part)
{
    return part->selection().notEmpty();
}

bool enabledIfSelectionIsRange(KHTMLPart *part)
{
    return part->selection().state() == Selection::RANGE;
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


// =============================================================================================
//
// queryCommandValue implementations
//
// EDIT FIXME: All these responses should be tested against the behavior
// of Microsoft browsers to ensure we are as compatible with their
// behavior as is sensible. For now, the returned values are just place-holders.

}

// =============================================================================================

void JSEditor::initDict()
{
    static bool initFlag = false;
    if (initFlag)
        return;
    initFlag = true;
   
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
    // documentation used as the basis for the list. It seems reasonable that these
    // commands should do something, even if it is to return a default placeholder value.
    //

    struct EditorCommand { const char *name; CommandImp imp; };

    static const EditorCommand commands[] = {

        // 2d-position command (not supported)
        // absoluteposition command (not supported)

        // backcolor command (supported)
        { "backcolor", {
            NoExec,
            enabled,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // blockdirltr command (not supported)
        // blockdirrtl command (not supported)

        // bold command (supported)
        { "bold", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // browsemode command (not supported)
        // clearauthenticationcache command (not supported)

        // copy command (supported)
        { "copy", {
            execCommandCopy,
            enabledIfSelectionIsRange,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // createbookmark command (not supported)
        // createlink command (not supported)

        // cut command (supported)
        { "cut", {
            execCommandCut,
            enabledIfSelectionIsRange,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // delete command (supported)
        { "delete", {
            execCommandDelete,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // dirltr command (not supported)
        // dirrtl command (not supported)
        // editmode command (not supported)

        // fontname command (supported)
        { "fontname", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // fontsize command (supported)
        { "fontsize", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // forecolor command (supported)
        { "forecolor", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // formatblock command (not supported)

        // indent command (supported)
        { "indent", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // inlinedirltr command (not supported)
        // inlinedirrtl command (not supported)
        // insertbutton command (not supported)
        // insertfieldset command (not supported)
        // inserthorizontalrule command (not supported)
        // insertiframe command (not supported)
        // insertimage command (not supported)
        // insertinputbutton command (not supported)
        // insertinputcheckbox command (not supported)
        // insertinputfileupload command (not supported)
        // insertinputhidden command (not supported)
        // insertinputimage command (not supported)
        // insertinputpassword command (not supported)
        // insertinputradio command (not supported)
        // insertinputreset command (not supported)
        // insertinputsubmit command (not supported)
        // insertinputtext command (not supported)
        // insertmarquee command (not supported)
        // insertorderedlist command (not supported)

        // insertparagraph command (supported)
        { "insertparagraph", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // insertselectdropdown command (not supported)
        // insertselectlistbox command (not supported)

        // inserttext command (supported)
        { "inserttext", {
            execCommandInsertText,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // inserttextarea command (not supported)
        // insertunorderedlist command (not supported)

        // italic command (supported)
        { "italic", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // justifycenter command (supported)
        { "justifycenter", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // justifyfull command (supported)
        { "justifyfull", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // justifyleft command (supported)
        { "justifyleft", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // justifynone command (supported)
        { "justifynone", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // justifyright command (supported)
        { "justifyright", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // liveresize command (not supported)
        // multipleselection command (not supported)
        // open command (not supported)

        // outdent command (supported)
        { "outdent", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // overwrite command (not supported)

        // paste command (supported)
        { "paste", {
            execCommandPaste,
            // EDIT FIXME: Should check if there is something on the pasteboard to paste
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // playimage command (not supported)

        // print command (supported)
        { "print", {
            NoExec,
            enabled,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // redo command (supported)
        { "redo", {
            execCommandRedo,
            // EDIT FIXME: Should check if the undo manager has something to redo
            enabled,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // refresh command (not supported)
        // removeformat command (not supported)
        // removeparaformat command (not supported)
        // saveas command (not supported)

        // selectall command (supported)
        { "selectAll", {
            execCommandSelectAll,
            enabled,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // sizetocontrol command (not supported)
        // sizetocontrolheight command (not supported)
        // sizetocontrolwidth command (not supported)
        // stop command (not supported)
        // stopimage command (not supported)
        // strikethrough command (not supported)

        // subscript command (supported)
        { "subscript", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // superscript command (supported)
        { "superscript", {
            NoExec,
            enabledIfSelectionNotEmpty,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // unbookmark command (not supported)
        // underline command (not supported)

        // undo command (supported)
        { "undo", {
            execCommandUndo,
            // EDIT FIXME: Should check if the undo manager has something to undo
            enabled,
            NoIndeterm,
            NoState,
            NoValue
        } },

        // unlink command (not supported)

        // unselect command (supported)
        { "unselect", {
            NoExec,
            enabledIfSelectionIsRange,
            NoIndeterm,
            NoState,
            NoValue
        } }

    };

    QDict<CommandImp> &dict = commandDict();
    const int numCommands = sizeof(commands) / sizeof(commands[0]);
    for (int i = 0; i < numCommands; ++i) {
        dict.insert(commands[i].name, &commands[i].imp);
    }
}

} // namespace khtml
