/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef ClassInfo_h
#define ClassInfo_h

#include "CallFrame.h"
#include "ConstructData.h"

namespace JSC {

    class HashEntry;
    struct HashTable;

    struct MethodTable {
        typedef void (*VisitChildrenFunctionPtr)(JSCell*, SlotVisitor&);
        VisitChildrenFunctionPtr visitChildren;

        typedef CallType (*GetCallDataFunctionPtr)(JSCell*, CallData&);
        GetCallDataFunctionPtr getCallData;

        typedef ConstructType (*GetConstructDataFunctionPtr)(JSCell*, ConstructData&);
        GetConstructDataFunctionPtr getConstructData;

        typedef void (*PutFunctionPtr)(JSCell*, ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
        PutFunctionPtr put;

        typedef void (*PutByIndexFunctionPtr)(JSCell*, ExecState*, unsigned propertyName, JSValue);
        PutByIndexFunctionPtr putByIndex;
    };

#define CREATE_METHOD_TABLE(ClassName) { \
        &ClassName::visitChildren, \
        &ClassName::getCallData, \
        &ClassName::getConstructData, \
        &ClassName::put, \
        &ClassName::putByIndex, \
    }, \
    sizeof(ClassName)

    struct ClassInfo {
        /**
         * A string denoting the class name. Example: "Window".
         */
        const char* className;

        /**
         * Pointer to the class information of the base class.
         * 0L if there is none.
         */
        const ClassInfo* parentClass;
        /**
         * Static hash-table of properties.
         * For classes that can be used from multiple threads, it is accessed via a getter function that would typically return a pointer to thread-specific value.
         */
        const HashTable* propHashTable(ExecState* exec) const
        {
            if (classPropHashTableGetterFunction)
                return classPropHashTableGetterFunction(exec);
            return staticPropHashTable;
        }
        
        bool isSubClassOf(const ClassInfo* other) const
        {
            for (const ClassInfo* ci = this; ci; ci = ci->parentClass) {
                if (ci == other)
                    return true;
            }
            return false;
        }

        bool hasStaticProperties() const
        {
            for (const ClassInfo* ci = this; ci; ci = ci->parentClass) {
                if (ci->staticPropHashTable || ci->classPropHashTableGetterFunction)
                    return true;
            }
            return false;
        }

        const HashTable* staticPropHashTable;
        typedef const HashTable* (*ClassPropHashTableGetterFunction)(ExecState*);
        const ClassPropHashTableGetterFunction classPropHashTableGetterFunction;

        MethodTable methodTable;

        size_t cellSize;
    };

} // namespace JSC

#endif // ClassInfo_h
