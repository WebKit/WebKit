/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008, 2011 Apple Inc. All rights reserved.
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

#ifndef DateConstructor_h
#define DateConstructor_h

#include "InternalFunction.h"

namespace JSC {

    class DatePrototype;

    class DateConstructor : public InternalFunction {
    public:
        typedef InternalFunction Base;

        static DateConstructor* create(VM& vm, Structure* structure, DatePrototype* datePrototype)
        {
            DateConstructor* constructor = new (NotNull, allocateCell<DateConstructor>(vm.heap)) DateConstructor(vm, structure);
            constructor->finishCreation(vm, datePrototype);
            return constructor;
        }

        DECLARE_INFO;

        static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
        {
            return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
        }

    protected:
        void finishCreation(VM&, DatePrototype*);
        static const unsigned StructureFlags = OverridesGetOwnPropertySlot | InternalFunction::StructureFlags;

    private:
        DateConstructor(VM&, Structure*);
        static ConstructType getConstructData(JSCell*, ConstructData&);
        static CallType getCallData(JSCell*, CallData&);

        static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    };

    JSObject* constructDate(ExecState*, JSGlobalObject*, const ArgList&);

} // namespace JSC

#endif // DateConstructor_h
