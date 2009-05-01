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

#ifndef RegExpConstructor_h
#define RegExpConstructor_h

#include "InternalFunction.h"
#include <wtf/OwnPtr.h>

namespace JSC {

    class RegExp;
    class RegExpPrototype;
    struct RegExpConstructorPrivate;

    class RegExpConstructor : public InternalFunction {
    public:
        RegExpConstructor(ExecState*, PassRefPtr<Structure>, RegExpPrototype*);

        static PassRefPtr<Structure> createStructure(JSValue prototype)
        {
            return Structure::create(prototype, TypeInfo(ObjectType, ImplementsHasInstance));
        }

        virtual void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);

        static const ClassInfo info;

        void performMatch(RegExp*, const UString&, int startOffset, int& position, int& length, int** ovector = 0);
        JSObject* arrayOfMatches(ExecState*) const;

        void setInput(const UString&);
        const UString& input() const;

        void setMultiline(bool);
        bool multiline() const;

        JSValue getBackref(ExecState*, unsigned) const;
        JSValue getLastParen(ExecState*) const;
        JSValue getLeftContext(ExecState*) const;
        JSValue getRightContext(ExecState*) const;

    private:
        virtual ConstructType getConstructData(ConstructData&);
        virtual CallType getCallData(CallData&);

        virtual const ClassInfo* classInfo() const { return &info; }

        OwnPtr<RegExpConstructorPrivate> d;
    };

    RegExpConstructor* asRegExpConstructor(JSValue);

    JSObject* constructRegExp(ExecState*, const ArgList&);

    inline RegExpConstructor* asRegExpConstructor(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&RegExpConstructor::info));
        return static_cast<RegExpConstructor*>(asObject(value));
    }

} // namespace JSC

#endif // RegExpConstructor_h
