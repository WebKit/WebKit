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

#define NoExec (0)
#define NoEnabled (0)
#define NoIndeterm (0)
#define NoState (0)
#define NoSupported (0)
#define NoValue (0)

using khtml::TypingCommand;

namespace DOM {

class DocumentImpl;

static bool execCommandNotImplemented()
{
    ERROR("unimplemented");
    return false;
}

static bool queryBoolNotImplemented()
{
    ERROR("unimplemented");
    return false;
}

static DOMString queryValueNotImplemented()
{
    ERROR("unimplemented");
    return DOMString();
}

QDict<JSEditor::CommandIdentifier> &JSEditor::commandDict()
{
    static QDict<JSEditor::CommandIdentifier> dict;
    return dict;
}

JSEditor::JSEditor(DocumentImpl *doc) : m_doc(doc) 
{
    initDict();
 }

JSEditor::CommandIdentifier *JSEditor::commandIdentifier(const DOMString &command)
{
    return commandDict().find(command.string().lower());
}

void JSEditor::addCommand(const QString &command, execCommandFn exec, queryBoolFn enabled, queryBoolFn indeterm, queryBoolFn state, queryBoolFn supported, queryValueFn value)
{
    commandDict().insert(command.lower(), new CommandIdentifier(exec, enabled, indeterm, state, supported, value));
}

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
    
    // 2d-position command (not supported)
    addCommand("2d-position", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // absoluteposition command (not supported)
    addCommand("absoluteposition", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // backcolor command (supported)
    addCommand(
        "backcolor", 
        NoExec, 
        &JSEditor::enabledIfPartNotNull, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // blockdirltr command (not supported)
    addCommand("blockdirltr", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // blockdirrtl command (not supported)
    addCommand("blockdirrtl", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // bold command (supported)
    addCommand(
        "bold", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // browsemode command (not supported)
    addCommand("browsemode", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // clearauthenticationcache command (not supported)
    addCommand("clearauthenticationcache", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // copy command (supported)
    addCommand(
        "copy", 
        &JSEditor::execCommandCopy, 
        &JSEditor::enabledIfSelectionIsRange, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // createbookmark command (not supported)
    addCommand("createbookmark", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // createlink command (not supported)
    addCommand("createlink", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // cut command (supported)
    addCommand(
        "cut", 
        &JSEditor::execCommandCut, 
        &JSEditor::enabledIfSelectionIsRange, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // delete command (supported)
    addCommand(
        "delete", 
        &JSEditor::execCommandDelete, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // dirltr command (not supported)
    addCommand("dirltr", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // dirrtl command (not supported)
    addCommand("dirrtl", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // editmode command (not supported)
    addCommand("editmode", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // fontname command (supported)
    addCommand(
        "fontname", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // fontsize command (supported)
    addCommand(
        "fontsize", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // forecolor command (supported)
    addCommand(
        "forecolor", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // formatblock command (not supported)
    addCommand("formatblock", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // indent command (supported)
    addCommand(
        "indent", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // inlinedirltr command (not supported)
    addCommand("inlinedirltr", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // inlinedirrtl command (not supported)
    addCommand("inlinedirrtl", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertbutton command (not supported)
    addCommand("insertbutton", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertfieldset command (not supported)
    addCommand("insertfieldset", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // inserthorizontalrule command (not supported)
    addCommand("inserthorizontalrule", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertiframe command (not supported)
    addCommand("insertiframe", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertimage command (not supported)
    addCommand("insertimage", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertinputbutton command (not supported)
    addCommand("insertinputbutton", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertinputcheckbox command (not supported)
    addCommand("insertinputcheckbox", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertinputfileupload command (not supported)
    addCommand("insertinputfileupload", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertinputhidden command (not supported)
    addCommand("insertinputhidden", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertinputimage command (not supported)
    addCommand("insertinputimage", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertinputpassword command (not supported)
    addCommand("insertinputpassword", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertinputradio command (not supported)
    addCommand("insertinputradio", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertinputreset command (not supported)
    addCommand("insertinputreset", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertinputsubmit command (not supported)
    addCommand("insertinputsubmit", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertinputtext command (not supported)
    addCommand("insertinputtext", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertmarquee command (not supported)
    addCommand("insertmarquee", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertorderedlist command (not supported)
    addCommand("insertorderedlist", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertparagraph command (supported)
    addCommand(
        "insertparagraph", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // insertselectdropdown command (not supported)
    addCommand("insertselectdropdown", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertselectlistbox command (not supported)
    addCommand("insertselectlistbox", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // inserttext command (supported)
    addCommand(
        "inserttext", 
        &JSEditor::execCommandInsertText, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // inserttextarea command (not supported)
    addCommand("inserttextarea", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // insertunorderedlist command (not supported)
    addCommand("insertunorderedlist", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // italic command (supported)
    addCommand(
        "italic", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // justifycenter command (supported)
    addCommand(
        "justifycenter", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // justifyfull command (supported)
    addCommand(
        "justifyfull", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // justifyleft command (supported)
    addCommand(
        "justifyleft", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // justifynone command (supported)
    addCommand(
        "justifynone", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // justifyright command (supported)
    addCommand(
        "justifyright", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // liveresize command (not supported)
    addCommand("liveresize", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // multipleselection command (not supported)
    addCommand("multipleselection", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // open command (not supported)
    addCommand("open", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // outdent command (supported)
    addCommand(
        "outdent", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // overwrite command (not supported)
    addCommand("overwrite", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // paste command (supported)
    addCommand(
        "paste", 
        &JSEditor::execCommandPaste, 
        // EDIT FIXME: Should check if there is something on the pasteboard to paste
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // playimage command (not supported)
    addCommand("playimage", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // print command (supported)
    addCommand(
        "print", 
        NoExec, 
        &JSEditor::enabledIfPartNotNull, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // redo command (supported)
    addCommand(
        "redo", 
        &JSEditor::execCommandRedo, 
        // EDIT FIXME: Should check if the undo manager has something to redo
        &JSEditor::enabledIfPartNotNull, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // refresh command (not supported)
    addCommand("refresh", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // removeformat command (not supported)
    addCommand("removeformat", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // removeparaformat command (not supported)
    addCommand("removeparaformat", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // saveas command (not supported)
    addCommand("saveas", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // selectall command (supported)
    addCommand(
        "selectAll", 
        &JSEditor::execCommandSelectAll, 
        &JSEditor::enabledIfPartNotNull, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // sizetocontrol command (not supported)
    addCommand("sizetocontrol", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // sizetocontrolheight command (not supported)
    addCommand("sizetocontrolheight", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // sizetocontrolwidth command (not supported)
    addCommand("sizetocontrolwidth", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // stop command (not supported)
    addCommand("stop", NoExec, NoEnabled, NoIndeterm, NoState, &JSEditor::commandSupported, NoValue);

    // stopimage command (not supported)
    addCommand("stopimage", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // strikethrough command (not supported)
    addCommand("strikethrough", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // subscript command (supported)
    addCommand(
        "subscript", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // superscript command (supported)
    addCommand(
        "superscript", 
        NoExec, 
        &JSEditor::enabledIfSelectionNotEmpty, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // unbookmark command (not supported)
    addCommand("unbookmark", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // underline command (not supported)
    addCommand("underline", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // undo command (supported)
    addCommand(
        "undo", 
        &JSEditor::execCommandUndo, 
        // EDIT FIXME: Should check if the undo manager has something to undo
        &JSEditor::enabledIfPartNotNull, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );

    // unlink command (not supported)
    addCommand("unlink", NoExec, NoEnabled, NoIndeterm, NoState, NoSupported, NoValue);

    // unselect command (supported)
    addCommand(
        "unselect", 
        NoExec, 
        &JSEditor::enabledIfSelectionIsRange, 
        NoIndeterm, 
        NoState, 
        &JSEditor::commandSupported, 
        NoValue
    );
}

bool JSEditor::execCommand(const DOMString &command, bool userInterface, const DOMString &value) 
{ 
    CommandIdentifier *cmd = commandIdentifier(command);
    if (!cmd || !cmd->execFn)
        return execCommandNotImplemented();
        
    ASSERT(document());
    document()->updateLayout();
    return (this->*(cmd->execFn))(userInterface, value);
}

bool JSEditor::queryCommandEnabled(const DOMString &command)
{ 
    CommandIdentifier *cmd = commandIdentifier(command);
    if (!cmd || !cmd->enabledFn)
        return queryBoolNotImplemented();
        
    ASSERT(document());
    document()->updateLayout();
    return (this->*(cmd->enabledFn))();
}

bool JSEditor::queryCommandIndeterm(const DOMString &command)
{
    CommandIdentifier *cmd = commandIdentifier(command);
    if (!cmd || !cmd->indetermFn)
        return queryBoolNotImplemented();
        
    ASSERT(document());
    document()->updateLayout();
    return (this->*(cmd->indetermFn))();
}

bool JSEditor::queryCommandState(const DOMString &command)
{
    CommandIdentifier *cmd = commandIdentifier(command);
    if (!cmd || !cmd->stateFn)
        return queryBoolNotImplemented();
        
    ASSERT(document());
    document()->updateLayout();
    return (this->*(cmd->stateFn))();
}

bool JSEditor::queryCommandSupported(const DOMString &command)
{
    CommandIdentifier *cmd = commandIdentifier(command);
    if (!cmd || !cmd->supportedFn)
        return queryBoolNotImplemented();
        
    ASSERT(document());
    document()->updateLayout();
    return (this->*(cmd->supportedFn))();
}

DOMString JSEditor::queryCommandValue(const DOMString &command)
{ 
    CommandIdentifier *cmd = commandIdentifier(command);
    if (!cmd || !cmd->valueFn)
        return queryValueNotImplemented();
    
    ASSERT(document());
    document()->updateLayout();
    return (this->*(cmd->valueFn))();
}

// =============================================================================================
//
// execCommand implementations
//
// EDIT FIXME: All these responses should be tested against the behavior
// of Microsoft browsers to ensure we are as compatible with their
// behavior as is sensible.

bool JSEditor::execCommandCopy(bool userInterface, const DOMString &value)
{
    KHTMLPart *part = document()->part();
    if (!part || part->selection().state() != Selection::RANGE)
        return false;
    KWQ(part)->issueCopyCommand();
    return true;
}

bool JSEditor::execCommandCut(bool userInterface, const DOMString &value)
{
    if (!part() || part()->selection().state() != Selection::RANGE)
        return false;
    KWQ(part())->issueCutCommand();
    return true;
}

bool JSEditor::execCommandDelete(bool userInterface, const DOMString &value)
{
    KHTMLPart *part = document()->part();
    if (!part || part->selection().isEmpty())
        return false;
    TypingCommand::deleteKeyPressed(document());
    return true;
}

bool JSEditor::execCommandInsertText(bool userInterface, const DOMString &value)
{
    KHTMLPart *part = document()->part();
    if (!part || part->selection().isEmpty())
        return false;
    TypingCommand::insertText(document(), value);
    return true;
}

bool JSEditor::execCommandPaste(bool userInterface, const DOMString &value)
{
    KHTMLPart *part = document()->part();
    if (!part || part->selection().isEmpty())
        return false;
    KWQ(part)->issuePasteCommand();
    return true;
}

bool JSEditor::execCommandRedo(bool userInterface, const DOMString &value)
{
    KHTMLPart *part = document()->part();
    if (!part || part->selection().state() != Selection::RANGE)
        return false;
    KWQ(part)->issueRedoCommand();
    return true;
}

bool JSEditor::execCommandSelectAll(bool userInterface, const DOMString &value)
{
    KHTMLPart *part = document()->part();
    if (!part)
        return false;
    part->selectAll();
    return true;
}

bool JSEditor::execCommandUndo(bool userInterface, const DOMString &value)
{
    KHTMLPart *part = document()->part();
    if (!part || part->selection().state() != Selection::RANGE)
        return false;
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

bool JSEditor::enabledIfPartNotNull()
{
    return part() != 0;
}

bool JSEditor::enabledIfSelectionNotEmpty()
{
    return part() && part()->selection().notEmpty();
}

bool JSEditor::enabledIfSelectionIsRange()
{
    return part() && part()->selection().state() == Selection::RANGE;
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
// queryCommandSupported implementations
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
// With this definition, all the commands we support return true unconditionally.

bool JSEditor::commandSupported()
{
    return true;
}

// =============================================================================================
//
// queryCommandValue implementations
//
// EDIT FIXME: All these responses should be tested against the behavior
// of Microsoft browsers to ensure we are as compatible with their
// behavior as is sensible. For now, the returned values are just place-holders.

// =============================================================================================

} // namespace khtml
