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
#include "QtWebUndoCommand.h"

using namespace WebKit;

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
