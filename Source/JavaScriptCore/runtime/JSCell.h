/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2007, 2008, 2009, 2015 Apple Inc. All rights reserved.
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

#ifndef JSCell_h
#define JSCell_h

#include "CallData.h"
#include "CellState.h"
#include "ConstructData.h"
#include "EnumerationMode.h"
#include "Heap.h"
#include "IndexingType.h"
#include "JSLock.h"
#include "JSTypeInfo.h"
#include "SlotVisitor.h"
#include "TypedArrayType.h"
#include "WriteBarrier.h"
#include <wtf/Noncopyable.h>

namespace JSC {

class CopyVisitor;
class ExecState;
class Identifier;
class JSArrayBufferView;
class JSDestructibleObject;
class JSGlobalObject;
class LLIntOffsetsExtractor;
class PropertyDescriptor;
class PropertyNameArray;
class Structure;

template<typename T> void* allocateCell(Heap&);
template<typename T> void* allocateCell(Heap&, size_t);

#define DECLARE_EXPORT_INFO                                             \
    protected:                                                          \
        static JS_EXPORTDATA const ::JSC::ClassInfo s_info;             \
    public:                                                             \
        static const ::JSC::ClassInfo* info() { return &s_info; }

#define DECLARE_INFO                                                    \
    protected:                                                          \
        static const ::JSC::ClassInfo s_info;                           \
    public:                                                             \
        static const ::JSC::ClassInfo* info() { return &s_info; }

class JSCell {
    friend class JSValue;
    friend class MarkedBlock;
    template<typename T> friend void* allocateCell(Heap&);
    template<typename T> friend void* allocateCell(Heap&, size_t);

public:
    static const unsigned StructureFlags = 0;

    static const bool needsDestruction = false;

    static JSCell* seenMultipleCalleeObjects() { return bitwise_cast<JSCell*>(static_cast<uintptr_t>(1)); }

    enum CreatingEarlyCellTag { CreatingEarlyCell };
    JSCell(CreatingEarlyCellTag);

protected:
    JSCell(VM&, Structure*);
    JS_EXPORT_PRIVATE static void destroy(JSCell*);

public:
    // Querying the type.
    bool isString() const;
    bool isSymbol() const;
    bool isObject() const;
    bool isGetterSetter() const;
    bool isCustomGetterSetter() const;
    bool isProxy() const;
    bool inherits(const ClassInfo*) const;
    bool isAPIValueWrapper() const;

    JSType type() const;
    IndexingType indexingType() const;
    StructureID structureID() const { return m_structureID; }
    Structure* structure() const;
    Structure* structure(VM&) const;
    void setStructure(VM&, Structure*);
    void clearStructure() { m_structureID = 0; }

    TypeInfo::InlineTypeFlags inlineTypeFlags() const { return m_flags; }

    const char* className() const;

    VM* vm() const;

    // Extracting the value.
    JS_EXPORT_PRIVATE bool getString(ExecState*, String&) const;
    JS_EXPORT_PRIVATE String getString(ExecState*) const; // null string if not a string
    JS_EXPORT_PRIVATE JSObject* getObject(); // NULL if not an object
    const JSObject* getObject() const; // NULL if not an object
        
    // Returns information about how to call/construct this cell as a function/constructor. May tell
    // you that the cell is not callable or constructor (default is that it's not either). If it
    // says that the function is callable, and the TypeOfShouldCallGetCallData type flag is set, and
    // this is an object, then typeof will return "function" instead of "object". These methods
    // cannot change their minds and must be thread-safe. They are sometimes called from compiler
    // threads.
    JS_EXPORT_PRIVATE static CallType getCallData(JSCell*, CallData&);
    JS_EXPORT_PRIVATE static ConstructType getConstructData(JSCell*, ConstructData&);

    // Basic conversions.
    JS_EXPORT_PRIVATE JSValue toPrimitive(ExecState*, PreferredPrimitiveType) const;
    bool getPrimitiveNumber(ExecState*, double& number, JSValue&) const;
    bool toBoolean(ExecState*) const;
    TriState pureToBoolean() const;
    JS_EXPORT_PRIVATE double toNumber(ExecState*) const;
    JS_EXPORT_PRIVATE JSObject* toObject(ExecState*, JSGlobalObject*) const;

    void dump(PrintStream&) const;
    JS_EXPORT_PRIVATE static void dumpToStream(const JSCell*, PrintStream&);

    size_t estimatedSizeInBytes() const;
    JS_EXPORT_PRIVATE static size_t estimatedSize(JSCell*);

    static void visitChildren(JSCell*, SlotVisitor&);
    JS_EXPORT_PRIVATE static void copyBackingStore(JSCell*, CopyVisitor&, CopyToken);

    // Object operations, with the toObject operation included.
    const ClassInfo* classInfo() const;
    const MethodTable* methodTable() const;
    const MethodTable* methodTable(VM&) const;
    static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
    static void putByIndex(JSCell*, ExecState*, unsigned propertyName, JSValue, bool shouldThrow);
        
    static bool deleteProperty(JSCell*, ExecState*, PropertyName);
    static bool deletePropertyByIndex(JSCell*, ExecState*, unsigned propertyName);

    static JSValue toThis(JSCell*, ExecState*, ECMAMode);

    void zap() { *reinterpret_cast<uintptr_t**>(this) = 0; }
    bool isZapped() const { return !*reinterpret_cast<uintptr_t* const*>(this); }

    static bool canUseFastGetOwnProperty(const Structure&);
    JSValue fastGetOwnProperty(VM&, Structure&, PropertyName);

    // The recommended idiom for using cellState() is to switch on it or perform an == comparison on it
    // directly. We deliberately avoid helpers for this, because we want transparency about how the various
    // CellState values influences our various algorithms. 
    CellState cellState() const { return m_cellState; }
    
    void setCellState(CellState data) const { const_cast<JSCell*>(this)->m_cellState = data; }

    static ptrdiff_t structureIDOffset()
    {
        return OBJECT_OFFSETOF(JSCell, m_structureID);
    }

    static ptrdiff_t typeInfoFlagsOffset()
    {
        return OBJECT_OFFSETOF(JSCell, m_flags);
    }

    static ptrdiff_t typeInfoTypeOffset()
    {
        return OBJECT_OFFSETOF(JSCell, m_type);
    }

    static ptrdiff_t indexingTypeOffset()
    {
        return OBJECT_OFFSETOF(JSCell, m_indexingType);
    }

    static ptrdiff_t cellStateOffset()
    {
        return OBJECT_OFFSETOF(JSCell, m_cellState);
    }

    static const TypedArrayType TypedArrayStorageType = NotTypedArray;
protected:

    void finishCreation(VM&);
    void finishCreation(VM&, Structure*, CreatingEarlyCellTag);

    // Dummy implementations of override-able static functions for classes to put in their MethodTable
    static JSValue defaultValue(const JSObject*, ExecState*, PreferredPrimitiveType);
    static NO_RETURN_DUE_TO_CRASH void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    static NO_RETURN_DUE_TO_CRASH void getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    static NO_RETURN_DUE_TO_CRASH void getPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);

    static uint32_t getEnumerableLength(ExecState*, JSObject*);
    static NO_RETURN_DUE_TO_CRASH void getStructurePropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    static NO_RETURN_DUE_TO_CRASH void getGenericPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    static NO_RETURN_DUE_TO_CRASH bool preventExtensions(JSObject*, ExecState*);
    static NO_RETURN_DUE_TO_CRASH bool isExtensible(JSObject*, ExecState*);
    static NO_RETURN_DUE_TO_CRASH bool setPrototypeOf(JSObject*, ExecState*, JSValue);

    static String className(const JSObject*);
    JS_EXPORT_PRIVATE static bool customHasInstance(JSObject*, ExecState*, JSValue);
    static bool defineOwnProperty(JSObject*, ExecState*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    static bool getOwnPropertySlotByIndex(JSObject*, ExecState*, unsigned propertyName, PropertySlot&);
    JS_EXPORT_PRIVATE static ArrayBuffer* slowDownAndWasteMemory(JSArrayBufferView*);
    JS_EXPORT_PRIVATE static PassRefPtr<ArrayBufferView> getTypedArrayImpl(JSArrayBufferView*);

private:
    friend class LLIntOffsetsExtractor;

    StructureID m_structureID;
    IndexingType m_indexingType;
    JSType m_type;
    TypeInfo::InlineTypeFlags m_flags;
    CellState m_cellState;
};

template<typename To, typename From>
inline To jsCast(From* from)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!from || from->JSCell::inherits(std::remove_pointer<To>::type::info()));
    return static_cast<To>(from);
}
    
template<typename To>
inline To jsCast(JSValue from)
{
    ASSERT_WITH_SECURITY_IMPLICATION(from.isCell() && from.asCell()->JSCell::inherits(std::remove_pointer<To>::type::info()));
    return static_cast<To>(from.asCell());
}

template<typename To, typename From>
inline To jsDynamicCast(From* from)
{
    if (LIKELY(from->inherits(std::remove_pointer<To>::type::info())))
        return static_cast<To>(from);
    return nullptr;
}

template<typename To>
inline To jsDynamicCast(JSValue from)
{
    if (LIKELY(from.isCell() && from.asCell()->inherits(std::remove_pointer<To>::type::info())))
        return static_cast<To>(from.asCell());
    return nullptr;
}

} // namespace JSC

#endif // JSCell_h
