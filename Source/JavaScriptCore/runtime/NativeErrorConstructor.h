/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef NativeErrorConstructor_h
#define NativeErrorConstructor_h

#include "InternalFunction.h"

namespace JSC {

    class ErrorInstance;
    class FunctionPrototype;
    class NativeErrorPrototype;

    class NativeErrorConstructor : public InternalFunction {
    public:
        NativeErrorConstructor(ExecState*, JSGlobalObject*, NonNullPassRefPtr<Structure> structure, NonNullPassRefPtr<Structure> prototypeStructure, const UString&);

        static const ClassInfo info;

        Structure* errorStructure() { return m_errorStructure.get(); }

    private:
        virtual ConstructType getConstructData(ConstructData&);
        virtual CallType getCallData(CallData&);

        virtual const ClassInfo* classInfo() const { return &info; }

        RefPtr<Structure> m_errorStructure;
    };

} // namespace JSC

#endif // NativeErrorConstructor_h
