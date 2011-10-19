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

#ifndef QtWebUndoCommand_h
#define QtWebUndoCommand_h

#include "WebEditCommandProxy.h"
#include <QUndoCommand>
#include <qglobal.h>
#include <wtf/RefPtr.h>

class QtWebUndoCommand : public QUndoCommand {
public:
    QtWebUndoCommand(PassRefPtr<WebKit::WebEditCommandProxy>, QUndoCommand* parent = 0);
    ~QtWebUndoCommand();

    void redo();
    void undo();

    bool inUndoRedo() const { return m_inUndoRedo; };

private:
    RefPtr<WebKit::WebEditCommandProxy> m_command;
    bool m_first;
    bool m_inUndoRedo;
};

#endif // QtWebUndoCommand_h
