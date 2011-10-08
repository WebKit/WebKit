/*
 *  Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef RegExpMatchesArray_h
#define RegExpMatchesArray_h

#include "JSArray.h"

namespace JSC {

    class RegExpMatchesArray : public JSArray {
    private:
        RegExpMatchesArray(ExecState*);

    public:
        typedef JSArray Base;

        static RegExpMatchesArray* create(ExecState* exec, RegExpConstructorPrivate* ctorPrivate)
        {
            RegExpMatchesArray* regExp = new (allocateCell<RegExpMatchesArray>(*exec->heap())) RegExpMatchesArray(exec);
            regExp->finishCreation(exec->globalData(), ctorPrivate);
            return regExp;
        }
        virtual ~RegExpMatchesArray();

    protected:
        void finishCreation(JSGlobalData&, RegExpConstructorPrivate* data);

    private:
        virtual bool getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
        {
            return getOwnPropertySlot(this, exec, propertyName, slot);
        }

        static bool getOwnPropertySlot(JSCell* cell, ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
        {
            RegExpMatchesArray* thisObject = static_cast<RegExpMatchesArray*>(cell);
            if (thisObject->subclassData())
                thisObject->fillArrayInstance(exec);
            return JSArray::getOwnPropertySlot(thisObject, exec, propertyName, slot);
        }

        virtual bool getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
        {
            return getOwnPropertySlot(this, exec, propertyName, slot);
        }

        static bool getOwnPropertySlot(JSCell* cell, ExecState* exec, unsigned propertyName, PropertySlot& slot)
        {
            RegExpMatchesArray* thisObject = static_cast<RegExpMatchesArray*>(cell);
            if (thisObject->subclassData())
                thisObject->fillArrayInstance(exec);
            return JSArray::getOwnPropertySlot(thisObject, exec, propertyName, slot);
        }

        virtual bool getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
        {
            if (subclassData())
                fillArrayInstance(exec);
            return JSArray::getOwnPropertyDescriptor(exec, propertyName, descriptor);
        }

        virtual void put(ExecState* exec, const Identifier& propertyName, JSValue v, PutPropertySlot& slot)
        {
            put(this, exec, propertyName, v, slot);
        }

        static void put(JSCell* cell, ExecState* exec, const Identifier& propertyName, JSValue v, PutPropertySlot& slot)
        {
            RegExpMatchesArray* thisObject = static_cast<RegExpMatchesArray*>(cell);
            if (thisObject->subclassData())
                thisObject->fillArrayInstance(exec);
            JSArray::put(thisObject, exec, propertyName, v, slot);
        }
        
        virtual void put(ExecState* exec, unsigned propertyName, JSValue v)
        {
            put(this, exec, propertyName, v);
        }
        
        static void put(JSCell* cell, ExecState* exec, unsigned propertyName, JSValue v)
        {
            RegExpMatchesArray* thisObject = static_cast<RegExpMatchesArray*>(cell);
            if (thisObject->subclassData())
                thisObject->fillArrayInstance(exec);
            JSArray::put(thisObject, exec, propertyName, v);
        }

        virtual bool deleteProperty(ExecState* exec, const Identifier& propertyName)
        {
            return deleteProperty(this, exec, propertyName);
        }

        static bool deleteProperty(JSCell* cell, ExecState* exec, const Identifier& propertyName)
        {
            RegExpMatchesArray* thisObject = static_cast<RegExpMatchesArray*>(cell);
            if (thisObject->subclassData())
                thisObject->fillArrayInstance(exec);
            return JSArray::deleteProperty(thisObject, exec, propertyName);
        }

        virtual bool deleteProperty(ExecState* exec, unsigned propertyName)
        {
            return deleteProperty(this, exec, propertyName);
        }

        static bool deleteProperty(JSCell* cell, ExecState* exec, unsigned propertyName)
        {
            RegExpMatchesArray* thisObject = static_cast<RegExpMatchesArray*>(cell);
            if (thisObject->subclassData())
                thisObject->fillArrayInstance(exec);
            return JSArray::deleteProperty(thisObject, exec, propertyName);
        }

        virtual void getOwnPropertyNames(ExecState* exec, PropertyNameArray& arr, EnumerationMode mode = ExcludeDontEnumProperties)
        {
            if (subclassData())
                fillArrayInstance(exec);
            JSArray::getOwnPropertyNames(exec, arr, mode);
        }

        void fillArrayInstance(ExecState*);
};

}

#endif // RegExpMatchesArray_h
