/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef GlobalEvalFunction_h
#define GlobalEvalFunction_h

#include "PrototypeFunction.h"

namespace JSC {

    class JSGlobalObject;

    class GlobalEvalFunction : public PrototypeFunction {
    public:
        GlobalEvalFunction(ExecState*, NonNullPassRefPtr<Structure>, int len, const Identifier&, NativeFunction, JSGlobalObject* expectedThisObject);
        JSGlobalObject* cachedGlobalObject() const { return m_cachedGlobalObject; }

        static PassRefPtr<Structure> createStructure(JSValue prototype) 
        { 
            return Structure::create(prototype, TypeInfo(ObjectType, ImplementsHasInstance | HasStandardGetOwnPropertySlot));
        }

    private:
        virtual void markChildren(MarkStack&);

        JSGlobalObject* m_cachedGlobalObject;
    };

} // namespace JSC

#endif // GlobalEvalFunction_h
