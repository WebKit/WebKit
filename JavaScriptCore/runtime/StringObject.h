/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef StringObject_h
#define StringObject_h

#include "JSWrapperObject.h"
#include "JSString.h"

namespace JSC {

    class StringObject : public JSWrapperObject {
    public:
        StringObject(ExecState*, PassRefPtr<Structure>);
        StringObject(ExecState*, PassRefPtr<Structure>, const UString&);

        static StringObject* create(ExecState*, JSString*);

        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        virtual void put(ExecState* exec, const Identifier& propertyName, JSValue, PutPropertySlot&);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual void getPropertyNames(ExecState*, PropertyNameArray&);

        virtual const ClassInfo* classInfo() const { return &info; }
        static const JS_EXPORTDATA ClassInfo info;

        JSString* internalValue() const { return asString(JSWrapperObject::internalValue());}

        static PassRefPtr<Structure> createStructure(JSValue prototype)
        {
            return Structure::create(prototype, TypeInfo(ObjectType));
        }

    protected:
        StringObject(PassRefPtr<Structure>, JSString*);

    private:
        virtual UString toString(ExecState*) const;
        virtual UString toThisString(ExecState*) const;
        virtual JSString* toThisJSString(ExecState*);
  };

    StringObject* asStringObject(JSValue);

    inline StringObject* asStringObject(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&StringObject::info));
        return static_cast<StringObject*>(asObject(value));
    }

} // namespace JSC

#endif // StringObject_h
