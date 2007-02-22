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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef EDITCOMMANDQT_H
#define EDITCOMMANDQT_H


#include <QUndoCommand>
#include <EditCommand.h>

class EditCommandQt : public QUndoCommand {
    public:
        EditCommandQt(WTF::RefPtr<WebCore::EditCommand> cmd, QUndoCommand *parent = 0);
        ~EditCommandQt();

        void redo();
        void undo();

    private:
        WTF::RefPtr<WebCore::EditCommand> _cmd;
        bool _first;
};

#endif

// vim: ts=4 sw=4 et
