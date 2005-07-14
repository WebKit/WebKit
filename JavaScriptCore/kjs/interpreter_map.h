// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#ifndef _KJS_INTERPRETER_MAP_H_
#define _KJS_INTERPRETER_MAP_H_

namespace KJS {
    class ObjectImp;
    class InterpreterImp;

    class InterpreterMap {
	struct KeyValue {
	    ObjectImp *key;
	    InterpreterImp *value;
	};

    public:
	static InterpreterImp * getInterpreterForGlobalObject(ObjectImp *global);
	static void setInterpreterForGlobalObject(InterpreterImp *interpreter, ObjectImp *global);
	static void removeInterpreterForGlobalObject(ObjectImp *global);

    private:
	static void insert(InterpreterImp *interpreter, ObjectImp *global);
	static void expand();
	static void shrink();
	static void rehash(int newTableSize);
	static unsigned computeHash(ObjectImp *pointer);


	static KeyValue * InterpreterMap::_table;
	static int InterpreterMap::_tableSize;
	static int InterpreterMap::_tableSizeMask;
	static int InterpreterMap::_keyCount;
    };

} // namespace


#endif // _KJS_INTERPRETER_MAP_H_
