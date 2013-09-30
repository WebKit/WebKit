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

#ifndef StringConstructor_h
#define StringConstructor_h

#include "InternalFunction.h"

namespace JSC {

    class StringPrototype;

    class StringConstructor : public InternalFunction {
    public:
        typedef InternalFunction Base;

        static StringConstructor* create(VM& vm, Structure* structure, StringPrototype* stringPrototype)
        {
            StringConstructor* constructor = new (NotNull, allocateCell<StringConstructor>(vm.heap)) StringConstructor(vm, structure);
            constructor->finishCreation(vm, stringPrototype);
            return constructor;
        }

        DECLARE_INFO;

        static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
        {
            return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
        }

    protected:
        static const unsigned StructureFlags = OverridesGetOwnPropertySlot | InternalFunction::StructureFlags;

    private:
        StringConstructor(VM&, Structure*);
        void finishCreation(VM&, StringPrototype*);
        static ConstructType getConstructData(JSCell*, ConstructData&);
        static CallType getCallData(JSCell*, CallData&);

        static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    };
    
    JSCell* JSC_HOST_CALL stringFromCharCode(ExecState*, int32_t);

} // namespace JSC

#endif // StringConstructor_h
