/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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

#include "JITOperations.h"
#include "StringObject.h"

namespace JSC {

class ObjectPrototype;
class RegExp;
class RegExpObject;

class StringPrototype final : public StringObject {
public:
    using Base = StringObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    static StringPrototype* create(VM&, JSGlobalObject*, Structure*);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(DerivedStringObjectType, StructureFlags), info());
    }

    DECLARE_INFO;

private:
    StringPrototype(VM&, Structure*);
    void finishCreation(VM&, JSGlobalObject*, JSString*);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(StringPrototype, StringObject);

JSCell* JIT_OPERATION operationStringProtoFuncReplaceGeneric(JSGlobalObject*, EncodedJSValue thisValue, EncodedJSValue searchValue, EncodedJSValue replaceValue);
JSCell* JIT_OPERATION operationStringProtoFuncReplaceRegExpEmptyStr(JSGlobalObject*, JSString* thisValue, RegExpObject* searchValue);
JSCell* JIT_OPERATION operationStringProtoFuncReplaceRegExpString(JSGlobalObject*, JSString* thisValue, RegExpObject* searchValue, JSString* replaceValue);

void substituteBackreferences(StringBuilder& result, const String& replacement, StringView source, const int* ovector, RegExp*);

JSC_DECLARE_HOST_FUNCTION(stringProtoFuncRepeatCharacter);
JSC_DECLARE_HOST_FUNCTION(stringProtoFuncSplitFast);

JSC_DECLARE_HOST_FUNCTION(builtinStringSubstringInternal);
JSC_DECLARE_HOST_FUNCTION(builtinStringIncludesInternal);
JSC_DECLARE_HOST_FUNCTION(builtinStringIndexOfInternal);

} // namespace JSC
