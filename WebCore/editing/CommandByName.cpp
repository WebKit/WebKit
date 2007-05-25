/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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
 
#include "config.h"
#include "CommandByName.h"

#include <wtf/HashMap.h>
#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "Page.h"
#include "PlatformString.h"
#include "SelectionController.h"
#include "Settings.h"
#include "StringHash.h"
#include "TypingCommand.h"

namespace WebCore {

class Frame;

namespace {

struct CommandImp {
    bool (*execFn)(Frame* frame);
    bool (*enabledFn)(Frame* frame);
};

typedef HashMap<StringImpl*, const CommandImp*, CaseInsensitiveHash<StringImpl*> > CommandMap;

CommandMap* createCommandDictionary();

const CommandImp* commandImp(const String& command)
{
    static CommandMap* commandDictionary = createCommandDictionary();
    return commandDictionary->get(command.impl());
}

// ============================================================================
// 
// execCommand function implementations
// 

bool execCopy(Frame* frame)
{
    frame->editor()->copy();
    return true;
}

bool execCut(Frame* frame)
{
    frame->editor()->cut();
    return true;
}

bool execDelete(Frame* frame)
{
    TypingCommand::deleteKeyPressed(frame->document(), frame->selectionGranularity() == WordGranularity);
    return true;
}

bool execForwardDelete(Frame* frame)
{
    TypingCommand::forwardDeleteKeyPressed(frame->document());
    return true;
}

bool execPaste(Frame* frame)
{
    frame->editor()->paste();
    return true;
}

bool execMoveLeft(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::LEFT, CharacterGranularity, true);
    return true;
}

bool execMoveRight(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::RIGHT, CharacterGranularity, true);
    return true;
}

bool execMoveUp(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, LineGranularity, true);
    return true;
}

bool execMoveDown(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, LineGranularity, true);
    return true;
}

bool execSelectAll(Frame* frame)
{
    frame->selectionController()->selectAll();
    return true;
}

bool execSelectLeft(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::LEFT, CharacterGranularity, true);
    return true;
}

bool execSelectRight(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::RIGHT, CharacterGranularity, true);
    return true;
}

bool execSelectUp(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, LineGranularity, true);
    return true;
}

bool execSelectDown(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, LineGranularity, true);
    return true;
}

// ============================================================================
// 
// enabled functions implementations
// 

bool enabled(Frame*)
{
    return true;
}

bool enabledAnySelection(Frame* frame)
{
    return frame->selectionController()->isCaretOrRange();
}

bool enabledAnyEditableSelection(Frame* frame)
{
    return frame->selectionController()->isCaretOrRange() && frame->selectionController()->isContentEditable();
}

bool enabledPaste(Frame* frame)
{
    Settings* settings = frame ? frame->settings() : 0;
    return settings && settings->isDOMPasteAllowed() && frame->editor()->canPaste();
}

bool enabledAnyRangeSelection(Frame* frame)
{
    return frame->selectionController()->isRange();
}

bool enabledAnyEditableRangeSelection(Frame* frame)
{
    return frame->selectionController()->isRange() && frame->selectionController()->isContentEditable();
}

CommandMap* createCommandDictionary()
{
    struct Command { const char* name; CommandImp imp; };

    static const Command commands[] = {
        { "Copy", { execCopy, enabledAnyRangeSelection } },
        { "Cut", { execCut, enabledAnyEditableRangeSelection } },
        { "Delete", { execDelete, enabledAnyEditableSelection } },
        { "ForwardDelete", { execForwardDelete, enabledAnyEditableSelection } },
        { "Paste", { execPaste, enabledPaste } },
        { "SelectAll", { execSelectAll, enabled } },
        { "MoveLeft", { execMoveLeft, enabledAnySelection } },
        { "MoveRight", { execMoveRight, enabledAnySelection } },
        { "MoveUp", { execMoveUp, enabledAnySelection } },
        { "MoveDown", { execMoveDown, enabledAnySelection } },
        { "SelectLeft", { execSelectLeft, enabledAnySelection } },
        { "SelectRight", { execSelectRight, enabledAnySelection } },
        { "SelectUp", { execSelectUp, enabledAnySelection } },
        { "SelectDown", { execSelectDown, enabledAnySelection } }
    };

    CommandMap* commandMap = new CommandMap;

    const int numCommands = sizeof(commands) / sizeof(commands[0]);
    for (int i = 0; i < numCommands; ++i) {
        StringImpl *name = new StringImpl(commands[i].name);
        name->ref();
        commandMap->set(name, &commands[i].imp);
    }

    return commandMap;
}

} // anonymous namespace

CommandByName::CommandByName(Frame* frame)
    : m_frame(frame)
{
}

const bool CommandByName::execCommand(const String& command)
{
    bool handled = false;
    const CommandImp* cmd = commandImp(command);
    if (!cmd)
        return false;
    if (!m_frame)
        return false;
    if (cmd->enabledFn(m_frame)) {
        m_frame->document()->updateLayoutIgnorePendingStylesheets();
        handled = cmd->execFn(m_frame);
    }
    return handled;
}

} // namespace WebCore
