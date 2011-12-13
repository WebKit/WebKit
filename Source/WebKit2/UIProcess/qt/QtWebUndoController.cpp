/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "QtWebUndoController.h"

#include "WebEditCommandProxy.h"
#include <qglobal.h>
#include <wtf/RefPtr.h>

using namespace WebKit;

class QtWebUndoCommand : public QUndoCommand {
public:
    QtWebUndoCommand(PassRefPtr<WebEditCommandProxy>, QUndoCommand* parent = 0);
    ~QtWebUndoCommand();

    void redo();
    void undo();

    bool inUndoRedo() const { return m_inUndoRedo; };

private:
    RefPtr<WebEditCommandProxy> m_command;
    bool m_first;
    bool m_inUndoRedo;
};

QtWebUndoCommand::QtWebUndoCommand(PassRefPtr<WebEditCommandProxy> command, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_command(command)
    , m_first(true)
    , m_inUndoRedo(false)
{
}

QtWebUndoCommand::~QtWebUndoCommand()
{
}

void QtWebUndoCommand::redo()
{
    m_inUndoRedo = true;

    // Ignore the first redo called from QUndoStack::push().
    if (m_first) {
        m_first = false;
        m_inUndoRedo = false;
        return;
    }
    if (m_command)
        m_command->reapply();

    m_inUndoRedo = false;
}

void QtWebUndoCommand::undo()
{
    m_inUndoRedo = true;

    if (m_command)
        m_command->unapply();

    m_inUndoRedo = false;
}

QtWebUndoController::QtWebUndoController()
{
}

void QtWebUndoController::registerEditCommand(PassRefPtr<WebEditCommandProxy> command, WebPageProxy::UndoOrRedo undoOrRedo)
{
    if (undoOrRedo == WebPageProxy::Undo) {
        const QtWebUndoCommand* webUndoCommand = static_cast<const QtWebUndoCommand*>(m_undoStack.command(m_undoStack.index()));
        if (webUndoCommand && webUndoCommand->inUndoRedo())
            return;
        m_undoStack.push(new QtWebUndoCommand(command));
    }
}

void QtWebUndoController::clearAllEditCommands()
{
    m_undoStack.clear();
}

bool QtWebUndoController::canUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    if (undoOrRedo == WebPageProxy::Undo)
        return m_undoStack.canUndo();
    return m_undoStack.canRedo();
}

void QtWebUndoController::executeUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    if (undoOrRedo == WebPageProxy::Undo)
        m_undoStack.undo();
    else
        m_undoStack.redo();
}
