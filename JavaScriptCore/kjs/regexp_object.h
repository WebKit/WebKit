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

#ifndef REGEXP_OBJECT_H_
#define REGEXP_OBJECT_H_

#include "function_object.h"
#include "regexp.h"

namespace KJS {

    struct RegExpObjectImpPrivate;

    class RegExpPrototype : public JSObject {
    public:
        RegExpPrototype(ExecState*, ObjectPrototype*, FunctionPrototype*);

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
    };

    class RegExpImp : public JSObject {
    public:
        enum { Global, IgnoreCase, Multiline, Source, LastIndex };

        RegExpImp(RegExpPrototype*, PassRefPtr<RegExp>);
        virtual ~RegExpImp();

        void setRegExp(PassRefPtr<RegExp> r) { m_regExp = r; }
        RegExp* regExp() const { return m_regExp.get(); }

        JSValue* test(ExecState*, const List& args);
        JSValue* exec(ExecState*, const List& args);

        virtual bool implementsCall() const;
        virtual JSValue* callAsFunction(ExecState*, JSObject*, const List&);
        bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        void put(ExecState*, const Identifier&, JSValue*, int attributes = None);
        void putValueProperty(ExecState*, int token, JSValue*, int attributes);

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

        void setLastIndex(double lastIndex) { m_lastIndex = lastIndex; }

    private:
        bool match(ExecState*, const List& args);

        RefPtr<RegExp> m_regExp;
        double m_lastIndex;
    };

    class RegExpObjectImp : public InternalFunctionImp {
    public:
        enum { Dollar1, Dollar2, Dollar3, Dollar4, Dollar5, Dollar6, Dollar7, Dollar8, Dollar9, 
               Input, Multiline, LastMatch, LastParen, LeftContext, RightContext };

        RegExpObjectImp(ExecState*, FunctionPrototype*, RegExpPrototype*);

        virtual bool implementsConstruct() const;
        virtual JSObject* construct(ExecState*, const List&);
        JSObject* createRegExpImp(ExecState*, PassRefPtr<RegExp>);
        virtual JSValue* callAsFunction(ExecState*, JSObject*, const List&);
        virtual void put(ExecState*, const Identifier&, JSValue*, int attributes = None);
        void putValueProperty(ExecState*, int token, JSValue*, int attributes);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }

        void performMatch(RegExp*, const UString&, int startOffset, int& position, int& length, int** ovector = 0);
        JSObject* arrayOfMatches(ExecState*) const;
        const UString& input() const;

    private:
        JSValue* getBackref(unsigned) const;
        JSValue* getLastParen() const;
        JSValue* getLeftContext() const;
        JSValue* getRightContext() const;

        OwnPtr<RegExpObjectImpPrivate> d;

        static const ClassInfo info;
    };

} // namespace

#endif
