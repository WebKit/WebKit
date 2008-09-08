/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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

#ifndef Arguments_h
#define Arguments_h

#include "IndexToNameMap.h"
#include "JSObject.h"

namespace JSC {

    class JSActivation;

    class Arguments : public JSObject {
    public:
        Arguments(ExecState*, JSFunction*, const ArgList&, JSActivation*);

        virtual void mark();

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual void put(ExecState*, const Identifier& propertyName, JSValue*, PutPropertySlot&);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

    private:
        static JSValue* mappedIndexGetter(ExecState*, const Identifier&, const PropertySlot& slot);

        struct ArgumentsData {
            ArgumentsData(JSActivation* activation_, JSFunction* function_, const ArgList& args_)
                : activation(activation_)
                , indexToNameMap(function_, args_)
            {
            }

            JSActivation* activation;
            mutable IndexToNameMap indexToNameMap;
        };
        
        OwnPtr<ArgumentsData> d;
    };

} // namespace JSC

#endif // Arguments_h
