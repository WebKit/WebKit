/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All Rights Reserved.
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

#ifndef RegExpObject_h
#define RegExpObject_h

#include "JSObject.h"
#include "RegExp.h"

namespace JSC {

    class RegExpObject : public JSObject {
    public:
        RegExpObject(NonNullPassRefPtr<Structure>, NonNullPassRefPtr<RegExp>);
        virtual ~RegExpObject();

        void setRegExp(PassRefPtr<RegExp> r) { d->regExp = r; }
        RegExp* regExp() const { return d->regExp.get(); }

        void setLastIndex(double lastIndex) { d->lastIndex = lastIndex; }
        double lastIndex() const { return d->lastIndex; }

        JSValue test(ExecState*, const ArgList&);
        JSValue exec(ExecState*, const ArgList&);

        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertyDescriptor(ExecState*, const Identifier&, PropertyDescriptor&);
        virtual void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

        static PassRefPtr<Structure> createStructure(JSValue prototype)
        {
            return Structure::create(prototype, TypeInfo(ObjectType, OverridesGetOwnPropertySlot | HasDefaultGetPropertyNames));
        }

    private:
        bool match(ExecState*, const ArgList&);

        virtual CallType getCallData(CallData&);

        struct RegExpObjectData : FastAllocBase {
            RegExpObjectData(NonNullPassRefPtr<RegExp> regExp, double lastIndex)
                : regExp(regExp)
                , lastIndex(lastIndex)
            {
            }

            RefPtr<RegExp> regExp;
            double lastIndex;
        };

        OwnPtr<RegExpObjectData> d;
    };

    RegExpObject* asRegExpObject(JSValue);

    inline RegExpObject* asRegExpObject(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&RegExpObject::info));
        return static_cast<RegExpObject*>(asObject(value));
    }

} // namespace JSC

#endif // RegExpObject_h
