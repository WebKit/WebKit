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


#ifndef _KJS_PROTECTED_VALUES_H_
#define _KJS_PROTECTED_VALUES_H_

namespace KJS {
    class ValueImp;

    class ProtectedValues {
	struct KeyValue {
	    ValueImp *key;
	    int value;
	};

    public:
	static void increaseProtectCount(ValueImp *key);
	static void decreaseProtectCount(ValueImp *key);

	static int getProtectCount(ValueImp *key);

    private:
	static void insert(ValueImp *key, int value);
	static void expand();
	static void shrink();
	static void rehash(int newTableSize);
	static unsigned computeHash(ValueImp *pointer);

	// let the collector scan the table directly for protected values
	friend class Collector;

	static KeyValue *_table;
	static int _tableSize;
	static int _tableSizeMask;
	static int _keyCount;
    };
}

#endif
