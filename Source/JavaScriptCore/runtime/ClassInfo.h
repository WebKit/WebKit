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

#include "CallFrame.h"
#include "ConstructData.h"
#include "JSCast.h"

#if CPU(ARM64E)
#include <ptrauth.h>
#endif

namespace WTF {
class PrintStream;
};

namespace JSC {

class HeapSnapshotBuilder;
class JSArrayBufferView;
class Snippet;
struct HashTable;

#if CPU(ARM64E)
#define WTF_METHOD_TABLE_ENTRY(method) \
    __ptrauth(ptrauth_key_process_independent_code, true, ptrauth_string_discriminator("MethodTable." #method)) method
#else
#define WTF_METHOD_TABLE_ENTRY(method) method
#endif

struct MethodTable {
    using DestroyFunctionPtr = void (*)(JSCell*);
    DestroyFunctionPtr WTF_METHOD_TABLE_ENTRY(destroy);

    using VisitChildrenFunctionPtr = void (*)(JSCell*, SlotVisitor&);
    VisitChildrenFunctionPtr WTF_METHOD_TABLE_ENTRY(visitChildren);

    using GetCallDataFunctionPtr = CallType (*)(JSCell*, CallData&);
    GetCallDataFunctionPtr WTF_METHOD_TABLE_ENTRY(getCallData);

    using GetConstructDataFunctionPtr = ConstructType (*)(JSCell*, ConstructData&);
    GetConstructDataFunctionPtr WTF_METHOD_TABLE_ENTRY(getConstructData);

    using PutFunctionPtr = bool (*)(JSCell*, ExecState*, PropertyName propertyName, JSValue, PutPropertySlot&);
    PutFunctionPtr WTF_METHOD_TABLE_ENTRY(put);

    using PutByIndexFunctionPtr = bool (*)(JSCell*, ExecState*, unsigned propertyName, JSValue, bool shouldThrow);
    PutByIndexFunctionPtr WTF_METHOD_TABLE_ENTRY(putByIndex);

    using DeletePropertyFunctionPtr = bool (*)(JSCell*, ExecState*, PropertyName);
    DeletePropertyFunctionPtr WTF_METHOD_TABLE_ENTRY(deleteProperty);

    using DeletePropertyByIndexFunctionPtr = bool (*)(JSCell*, ExecState*, unsigned);
    DeletePropertyByIndexFunctionPtr WTF_METHOD_TABLE_ENTRY(deletePropertyByIndex);

    using GetOwnPropertySlotFunctionPtr = bool (*)(JSObject*, ExecState*, PropertyName, PropertySlot&);
    GetOwnPropertySlotFunctionPtr WTF_METHOD_TABLE_ENTRY(getOwnPropertySlot);

    using GetOwnPropertySlotByIndexFunctionPtr = bool (*)(JSObject*, ExecState*, unsigned, PropertySlot&);
    GetOwnPropertySlotByIndexFunctionPtr WTF_METHOD_TABLE_ENTRY(getOwnPropertySlotByIndex);

    using DoPutPropertySecurityCheckFunctionPtr = void (*)(JSObject*, ExecState*, PropertyName, PutPropertySlot&);
    DoPutPropertySecurityCheckFunctionPtr WTF_METHOD_TABLE_ENTRY(doPutPropertySecurityCheck);

    using ToThisFunctionPtr = JSValue (*)(JSCell*, ExecState*, ECMAMode);
    ToThisFunctionPtr WTF_METHOD_TABLE_ENTRY(toThis);

    using DefaultValueFunctionPtr = JSValue (*)(const JSObject*, ExecState*, PreferredPrimitiveType);
    DefaultValueFunctionPtr WTF_METHOD_TABLE_ENTRY(defaultValue);

    using GetOwnPropertyNamesFunctionPtr = void (*)(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    GetOwnPropertyNamesFunctionPtr WTF_METHOD_TABLE_ENTRY(getOwnPropertyNames);

    using GetOwnNonIndexPropertyNamesFunctionPtr = void (*)(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    GetOwnNonIndexPropertyNamesFunctionPtr WTF_METHOD_TABLE_ENTRY(getOwnNonIndexPropertyNames);

    using GetPropertyNamesFunctionPtr = void (*)(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    GetPropertyNamesFunctionPtr WTF_METHOD_TABLE_ENTRY(getPropertyNames);

    using GetEnumerableLengthFunctionPtr = uint32_t (*)(ExecState*, JSObject*);
    GetEnumerableLengthFunctionPtr WTF_METHOD_TABLE_ENTRY(getEnumerableLength);

    GetPropertyNamesFunctionPtr WTF_METHOD_TABLE_ENTRY(getStructurePropertyNames);
    GetPropertyNamesFunctionPtr WTF_METHOD_TABLE_ENTRY(getGenericPropertyNames);

    using ClassNameFunctionPtr = String (*)(const JSObject*, VM&);
    ClassNameFunctionPtr WTF_METHOD_TABLE_ENTRY(className);

    using ToStringNameFunctionPtr = String (*)(const JSObject*, ExecState*);
    ToStringNameFunctionPtr WTF_METHOD_TABLE_ENTRY(toStringName);

    using CustomHasInstanceFunctionPtr = bool (*)(JSObject*, ExecState*, JSValue);
    CustomHasInstanceFunctionPtr WTF_METHOD_TABLE_ENTRY(customHasInstance);

    using DefineOwnPropertyFunctionPtr = bool (*)(JSObject*, ExecState*, PropertyName, const PropertyDescriptor&, bool);
    DefineOwnPropertyFunctionPtr WTF_METHOD_TABLE_ENTRY(defineOwnProperty);

    using PreventExtensionsFunctionPtr = bool (*)(JSObject*, ExecState*);
    PreventExtensionsFunctionPtr WTF_METHOD_TABLE_ENTRY(preventExtensions);

    using IsExtensibleFunctionPtr = bool (*)(JSObject*, ExecState*);
    IsExtensibleFunctionPtr WTF_METHOD_TABLE_ENTRY(isExtensible);

    using SetPrototypeFunctionPtr = bool (*)(JSObject*, ExecState*, JSValue, bool shouldThrowIfCantSet);
    SetPrototypeFunctionPtr WTF_METHOD_TABLE_ENTRY(setPrototype);

    using GetPrototypeFunctionPtr = JSValue (*)(JSObject*, ExecState*);
    GetPrototypeFunctionPtr WTF_METHOD_TABLE_ENTRY(getPrototype);

    using DumpToStreamFunctionPtr = void (*)(const JSCell*, PrintStream&);
    DumpToStreamFunctionPtr WTF_METHOD_TABLE_ENTRY(dumpToStream);

    using HeapSnapshotFunctionPtr = void (*)(JSCell*, HeapSnapshotBuilder&);
    HeapSnapshotFunctionPtr WTF_METHOD_TABLE_ENTRY(heapSnapshot);

    using EstimatedSizeFunctionPtr = size_t (*)(JSCell*, VM&);
    EstimatedSizeFunctionPtr WTF_METHOD_TABLE_ENTRY(estimatedSize);

    using VisitOutputConstraintsPtr = void (*)(JSCell*, SlotVisitor&);
    VisitOutputConstraintsPtr WTF_METHOD_TABLE_ENTRY(visitOutputConstraints);
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

#define CREATE_METHOD_TABLE(ClassName) { \
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
        &ClassName::heapSnapshot, \
        &ClassName::estimatedSize, \
        &ClassName::visitOutputConstraints, \
    }, \
    ClassName::TypedArrayStorageType

struct ClassInfo {
    // A string denoting the class name. Example: "Window".
    const char* className;

    // Pointer to the class information of the base class.
    // nullptrif there is none.
    const ClassInfo* parentClass;

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

    const HashTable* staticPropHashTable;

    using CheckSubClassSnippetFunctionPtr = Ref<Snippet> (*)(void);
    CheckSubClassSnippetFunctionPtr checkSubClassSnippet;

    MethodTable methodTable;

    TypedArrayType typedArrayStorageType;
};

} // namespace JSC
