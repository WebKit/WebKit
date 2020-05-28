/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "ConstructData.h"
#include "JSCast.h"
#include <wtf/PtrTag.h>

namespace WTF {
class PrintStream;
};

namespace JSC {

class HeapAnalyzer;
class JSArrayBufferView;
class Snippet;
struct HashTable;

#define METHOD_TABLE_ENTRY(method) \
    WTF_VTBL_FUNCPTR_PTRAUTH_STR("MethodTable." #method) method

struct MethodTable {
    using DestroyFunctionPtr = void (*)(JSCell*);
    DestroyFunctionPtr METHOD_TABLE_ENTRY(destroy);

    using VisitChildrenFunctionPtr = void (*)(JSCell*, SlotVisitor&);
    VisitChildrenFunctionPtr METHOD_TABLE_ENTRY(visitChildren);

    using GetCallDataFunctionPtr = CallData (*)(JSCell*);
    GetCallDataFunctionPtr METHOD_TABLE_ENTRY(getCallData);

    using GetConstructDataFunctionPtr = CallData (*)(JSCell*);
    GetConstructDataFunctionPtr METHOD_TABLE_ENTRY(getConstructData);

    using PutFunctionPtr = bool (*)(JSCell*, JSGlobalObject*, PropertyName propertyName, JSValue, PutPropertySlot&);
    PutFunctionPtr METHOD_TABLE_ENTRY(put);

    using PutByIndexFunctionPtr = bool (*)(JSCell*, JSGlobalObject*, unsigned propertyName, JSValue, bool shouldThrow);
    PutByIndexFunctionPtr METHOD_TABLE_ENTRY(putByIndex);

    using DeletePropertyFunctionPtr = bool (*)(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);
    DeletePropertyFunctionPtr METHOD_TABLE_ENTRY(deleteProperty);

    using DeletePropertyByIndexFunctionPtr = bool (*)(JSCell*, JSGlobalObject*, unsigned);
    DeletePropertyByIndexFunctionPtr METHOD_TABLE_ENTRY(deletePropertyByIndex);

    using GetOwnPropertySlotFunctionPtr = bool (*)(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    GetOwnPropertySlotFunctionPtr METHOD_TABLE_ENTRY(getOwnPropertySlot);

    using GetOwnPropertySlotByIndexFunctionPtr = bool (*)(JSObject*, JSGlobalObject*, unsigned, PropertySlot&);
    GetOwnPropertySlotByIndexFunctionPtr METHOD_TABLE_ENTRY(getOwnPropertySlotByIndex);

    using DoPutPropertySecurityCheckFunctionPtr = void (*)(JSObject*, JSGlobalObject*, PropertyName, PutPropertySlot&);
    DoPutPropertySecurityCheckFunctionPtr METHOD_TABLE_ENTRY(doPutPropertySecurityCheck);

    using ToThisFunctionPtr = JSValue (*)(JSCell*, JSGlobalObject*, ECMAMode);
    ToThisFunctionPtr METHOD_TABLE_ENTRY(toThis);

    using DefaultValueFunctionPtr = JSValue (*)(const JSObject*, JSGlobalObject*, PreferredPrimitiveType);
    DefaultValueFunctionPtr METHOD_TABLE_ENTRY(defaultValue);

    using GetOwnPropertyNamesFunctionPtr = void (*)(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    GetOwnPropertyNamesFunctionPtr METHOD_TABLE_ENTRY(getOwnPropertyNames);

    using GetOwnNonIndexPropertyNamesFunctionPtr = void (*)(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    GetOwnNonIndexPropertyNamesFunctionPtr METHOD_TABLE_ENTRY(getOwnNonIndexPropertyNames);

    using GetPropertyNamesFunctionPtr = void (*)(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    GetPropertyNamesFunctionPtr METHOD_TABLE_ENTRY(getPropertyNames);

    using GetEnumerableLengthFunctionPtr = uint32_t (*)(JSGlobalObject*, JSObject*);
    GetEnumerableLengthFunctionPtr METHOD_TABLE_ENTRY(getEnumerableLength);

    GetPropertyNamesFunctionPtr METHOD_TABLE_ENTRY(getStructurePropertyNames);
    GetPropertyNamesFunctionPtr METHOD_TABLE_ENTRY(getGenericPropertyNames);

    using ClassNameFunctionPtr = String (*)(const JSObject*, VM&);
    ClassNameFunctionPtr METHOD_TABLE_ENTRY(className);

    using ToStringNameFunctionPtr = String (*)(const JSObject*, JSGlobalObject*);
    ToStringNameFunctionPtr METHOD_TABLE_ENTRY(toStringName);

    using CustomHasInstanceFunctionPtr = bool (*)(JSObject*, JSGlobalObject*, JSValue);
    CustomHasInstanceFunctionPtr METHOD_TABLE_ENTRY(customHasInstance);

    using DefineOwnPropertyFunctionPtr = bool (*)(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool);
    DefineOwnPropertyFunctionPtr METHOD_TABLE_ENTRY(defineOwnProperty);

    using PreventExtensionsFunctionPtr = bool (*)(JSObject*, JSGlobalObject*);
    PreventExtensionsFunctionPtr METHOD_TABLE_ENTRY(preventExtensions);

    using IsExtensibleFunctionPtr = bool (*)(JSObject*, JSGlobalObject*);
    IsExtensibleFunctionPtr METHOD_TABLE_ENTRY(isExtensible);

    using SetPrototypeFunctionPtr = bool (*)(JSObject*, JSGlobalObject*, JSValue, bool shouldThrowIfCantSet);
    SetPrototypeFunctionPtr METHOD_TABLE_ENTRY(setPrototype);

    using GetPrototypeFunctionPtr = JSValue (*)(JSObject*, JSGlobalObject*);
    GetPrototypeFunctionPtr METHOD_TABLE_ENTRY(getPrototype);

    using DumpToStreamFunctionPtr = void (*)(const JSCell*, PrintStream&);
    DumpToStreamFunctionPtr METHOD_TABLE_ENTRY(dumpToStream);

    using AnalyzeHeapFunctionPtr = void (*)(JSCell*, HeapAnalyzer&);
    AnalyzeHeapFunctionPtr METHOD_TABLE_ENTRY(analyzeHeap);

    using EstimatedSizeFunctionPtr = size_t (*)(JSCell*, VM&);
    EstimatedSizeFunctionPtr METHOD_TABLE_ENTRY(estimatedSize);

    using VisitOutputConstraintsPtr = void (*)(JSCell*, SlotVisitor&);
    VisitOutputConstraintsPtr METHOD_TABLE_ENTRY(visitOutputConstraints);
};

#define CREATE_MEMBER_CHECKER(member) \
    template <typename T> \
    struct MemberCheck##member { \
        struct Fallback { \
            void member(...); \
        }; \
        struct Derived : T, Fallback { }; \
        template <typename U, U> struct Check; \
        typedef char Yes[2]; \
        typedef char No[1]; \
        template <typename U> \
        static No &func(Check<void (Fallback::*)(...), &U::member>*); \
        template <typename U> \
        static Yes &func(...); \
        enum { has = sizeof(func<Derived>(0)) == sizeof(Yes) }; \
    }

#define HAS_MEMBER_NAMED(klass, name) (MemberCheck##name<klass>::has)

#define CREATE_METHOD_TABLE(ClassName) \
    JSCastingHelpers::InheritsTraits<ClassName>::typeRange, \
    { \
        &ClassName::destroy, \
        &ClassName::visitChildren, \
        &ClassName::getCallData, \
        &ClassName::getConstructData, \
        &ClassName::put, \
        &ClassName::putByIndex, \
        &ClassName::deleteProperty, \
        &ClassName::deletePropertyByIndex, \
        &ClassName::getOwnPropertySlot, \
        &ClassName::getOwnPropertySlotByIndex, \
        &ClassName::doPutPropertySecurityCheck, \
        &ClassName::toThis, \
        &ClassName::defaultValue, \
        &ClassName::getOwnPropertyNames, \
        &ClassName::getOwnNonIndexPropertyNames, \
        &ClassName::getPropertyNames, \
        &ClassName::getEnumerableLength, \
        &ClassName::getStructurePropertyNames, \
        &ClassName::getGenericPropertyNames, \
        &ClassName::className, \
        &ClassName::toStringName, \
        &ClassName::customHasInstance, \
        &ClassName::defineOwnProperty, \
        &ClassName::preventExtensions, \
        &ClassName::isExtensible, \
        &ClassName::setPrototype, \
        &ClassName::getPrototype, \
        &ClassName::dumpToStream, \
        &ClassName::analyzeHeap, \
        &ClassName::estimatedSize, \
        &ClassName::visitOutputConstraints, \
    }, \
    ClassName::TypedArrayStorageType, \
    sizeof(ClassName),

struct ClassInfo {
    using CheckJSCastSnippetFunctionPtr = Ref<Snippet> (*)(void);

    // A string denoting the class name. Example: "Window".
    const char* className;
    // Pointer to the class information of the base class.
    // nullptrif there is none.
    const ClassInfo* parentClass;
    const HashTable* staticPropHashTable;
    CheckJSCastSnippetFunctionPtr checkSubClassSnippet;
    const Optional<JSTypeRange> inheritsJSTypeRange; // This is range of JSTypes for doing inheritance checking. Has the form: [firstJSType, lastJSType] (inclusive).
    MethodTable methodTable;
    const TypedArrayType typedArrayStorageType;
    const unsigned staticClassSize;

    static ptrdiff_t offsetOfParentClass()
    {
        return OBJECT_OFFSETOF(ClassInfo, parentClass);
    }

    bool isSubClassOf(const ClassInfo* other) const
    {
        for (const ClassInfo* ci = this; ci; ci = ci->parentClass) {
            if (ci == other)
                return true;
        }
        return false;
    }

    JS_EXPORT_PRIVATE void dump(PrintStream&) const;

    JS_EXPORT_PRIVATE bool hasStaticSetterOrReadonlyProperties() const;
};

} // namespace JSC
