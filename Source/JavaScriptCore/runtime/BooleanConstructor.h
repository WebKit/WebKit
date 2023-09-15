/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "JSFunction.h"

namespace JSC {

class BooleanPrototype;
class GetterSetter;

class BooleanConstructor final : public JSFunction {
public:
    using Base = JSFunction;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static BooleanConstructor* create(VM&, Structure*, BooleanPrototype*, GetterSetter*);

    DECLARE_INFO;

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

private:
    BooleanConstructor(VM&, NativeExecutable*, JSGlobalObject*, Structure*);
    void finishCreation(VM&, BooleanPrototype*);
};
static_assert(sizeof(BooleanConstructor) == sizeof(JSFunction), "Allocate BooleanConstructor in JSFunction IsoSubspace");

JSObject* constructBooleanFromImmediateBoolean(JSGlobalObject*, JSValue);

} // namespace JSC
