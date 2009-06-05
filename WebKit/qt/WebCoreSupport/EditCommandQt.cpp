/*
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
#include <wtf/Platform.h>
#include "EditCommandQt.h"

using namespace WebCore;

EditCommandQt::EditCommandQt(WTF::RefPtr<EditCommand> cmd, QUndoCommand *parent)
:
#ifndef QT_NO_UNDOCOMMAND
    QUndoCommand(parent),
#endif
    _cmd(cmd), _first(true)
{
}


EditCommandQt::~EditCommandQt() {
}


void EditCommandQt::redo() {
    if (_first) {
        _first = false;
        return;
    }
    if (_cmd) {
        _cmd->reapply();
    }
}


void EditCommandQt::undo() {
    if (_cmd) {
        _cmd->unapply();
    }
}


// vim: ts=4 sw=4 et
