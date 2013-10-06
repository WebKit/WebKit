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
        typedef JSWrapperObject Base;

        static StringObject* create(VM& vm, Structure* structure)
        {
            JSString* string = jsEmptyString(&vm);
            StringObject* object = new (NotNull, allocateCell<StringObject>(vm.heap)) StringObject(vm, structure);
            object->finishCreation(vm, string);
            return object;
        }
        static StringObject* create(VM& vm, Structure* structure, JSString* string)
        {
            StringObject* object = new (NotNull, allocateCell<StringObject>(vm.heap)) StringObject(vm, structure);
            object->finishCreation(vm, string);
            return object;
        }
        static StringObject* create(VM&, JSGlobalObject*, JSString*);

        static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
        static bool getOwnPropertySlotByIndex(JSObject*, ExecState*, unsigned propertyName, PropertySlot&);

        static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
        static void putByIndex(JSCell*, ExecState*, unsigned propertyName, JSValue, bool shouldThrow);

        static bool deleteProperty(JSCell*, ExecState*, PropertyName);
        static bool deletePropertyByIndex(JSCell*, ExecState*, unsigned propertyName);
        static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
        static bool defineOwnProperty(JSObject*, ExecState*, PropertyName, const PropertyDescriptor&, bool shouldThrow);

        DECLARE_EXPORT_INFO;

        JSString* internalValue() const { return asString(JSWrapperObject::internalValue());}

        static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
        {
            return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
        }

    protected:
        JS_EXPORT_PRIVATE void finishCreation(VM&, JSString*);
        static const unsigned StructureFlags = OverridesGetOwnPropertySlot | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero | OverridesGetPropertyNames | JSWrapperObject::StructureFlags;
        JS_EXPORT_PRIVATE StringObject(VM&, Structure*);
    };

    StringObject* asStringObject(JSValue);

    inline StringObject* asStringObject(JSValue value)
    {
        ASSERT(asObject(value)->inherits(StringObject::info()));
        return static_cast<StringObject*>(asObject(value));
    }

    JS_EXPORT_PRIVATE StringObject* constructString(VM&, JSGlobalObject*, JSValue);

} // namespace JSC

#endif // StringObject_h
