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

#include "JSFunction.h"

namespace KJS {

    class FunctionPrototype;
    class RegExpPrototype;
    class RegExp;
    struct RegExpConstructorPrivate;

    class RegExpConstructor : public InternalFunction {
    public:
        enum { Dollar1, Dollar2, Dollar3, Dollar4, Dollar5, Dollar6, Dollar7, Dollar8, Dollar9, 
               Input, Multiline, LastMatch, LastParen, LeftContext, RightContext };

        RegExpConstructor(ExecState*, FunctionPrototype*, RegExpPrototype*);

        virtual void put(ExecState*, const Identifier&, JSValue*);
        void putValueProperty(ExecState*, int token, JSValue*);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;

        static const ClassInfo info;

        void performMatch(RegExp*, const UString&, int startOffset, int& position, int& length, int** ovector = 0);
        JSObject* arrayOfMatches(ExecState*) const;
        const UString& input() const;

    private:
        virtual ConstructType getConstructData(ConstructData&);
        virtual CallType getCallData(CallData&);
        virtual const ClassInfo* classInfo() const { return &info; }

        JSValue* getBackref(ExecState*, unsigned) const;
        JSValue* getLastParen(ExecState*) const;
        JSValue* getLeftContext(ExecState*) const;
        JSValue* getRightContext(ExecState*) const;

        OwnPtr<RegExpConstructorPrivate> d;
    };

} // namespace

#endif
