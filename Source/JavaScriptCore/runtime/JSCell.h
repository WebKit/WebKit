/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#include "CallData.h"
#include "CellState.h"
#include "ConstructData.h"
#include "EnumerationMode.h"
#include "Heap.h"
#include "HeapCell.h"
#include "IndexingType.h"
#include "JSLock.h"
#include "JSTypeInfo.h"
#include "SlotVisitor.h"
#include "SubspaceAccess.h"
#include "TypedArrayType.h"
#include "WriteBarrier.h"

namespace JSC {

class CompleteSubspace;
class CopyVisitor;
class GCDeferralContext;
class ExecState;
class Identifier;
class JSArrayBufferView;
class JSDestructibleObject;
class JSGlobalObject;
class LLIntOffsetsExtractor;
class PropertyDescriptor;
class PropertyName;
class PropertyNameArray;
class Structure;
class JSCellLock;

enum class GCDeferralContextArgPresense {
    HasArg,
    DoesNotHaveArg
};

template<typename T> void* allocateCell(Heap&, size_t = sizeof(T));
template<typename T> void* tryAllocateCell(Heap&, size_t = sizeof(T));
template<typename T> void* allocateCell(Heap&, GCDeferralContext*, size_t = sizeof(T));
template<typename T> void* tryAllocateCell(Heap&, GCDeferralContext*, size_t = sizeof(T));

#define DECLARE_EXPORT_INFO                                                  \
    protected:                                                               \
        static JS_EXPORT_PRIVATE const ::JSC::ClassInfo s_info;              \
    public:                                                                  \
        static constexpr const ::JSC::ClassInfo* info() { return &s_info; }

#define DECLARE_INFO                                                         \
    protected:                                                               \
        static const ::JSC::ClassInfo s_info;                                \
    public:                                                                  \
        static constexpr const ::JSC::ClassInfo* info() { return &s_info; }

class JSCell : public HeapCell {
    friend class JSValue;
    friend class MarkedBlock;
    template<typename T>
    friend void* tryAllocateCellHelper(Heap&, size_t, GCDeferralContext*, AllocationFailureMode);

public:
    static const unsigned StructureFlags = 0;

    static const bool needsDestruction = false;

    // Don't call this directly. Call JSC::subspaceFor<Type>(vm) instead.
    // FIXME: Refer to Subspace by reference.
    // https://bugs.webkit.org/show_bug.cgi?id=166988
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM&);

    static JSCell* seenMultipleCalleeObjects() { return bitwise_cast<JSCell*>(static_cast<uintptr_t>(1)); }

    enum CreatingEarlyCellTag { CreatingEarlyCell };
    JSCell(CreatingEarlyCellTag);
    
protected:
    JSCell(VM&, Structure*);
    JS_EXPORT_PRIVATE static void destroy(JSCell*);

public:
    // Querying the type.
    bool isString() const;
    bool isBigInt() const;
    bool isSymbol() const;
    bool isObject() const;
    bool isGetterSetter() const;
    bool isCustomGetterSetter() const;
    bool isProxy() const;
    bool isFunction(VM&);
    bool isCallable(VM&, CallType&, CallData&);
    bool isConstructor(VM&);
    bool isConstructor(VM&, ConstructType&, ConstructData&);
    bool inherits(VM&, const ClassInfo*) const;
    template<typename Target> bool inherits(VM&) const;
    bool isAPIValueWrapper() const;
    
    // Each cell has a built-in lock. Currently it's simply available for use if you need it. It's
    // a full-blown WTF::Lock. Note that this lock is currently used in JSArray and that lock's
    // ordering with the Structure lock is that the Structure lock must be acquired first.

    // We use this abstraction to make it easier to grep for places where we lock cells.
    // to lock a cell you can just do:
    // auto locker = holdLock(cell->cellLocker());
    JSCellLock& cellLock() { return *reinterpret_cast<JSCellLock*>(this); }
    
    JSType type() const;
    IndexingType indexingTypeAndMisc() const;
    IndexingType indexingMode() const;
    IndexingType indexingType() const;
    StructureID structureID() const { return m_structureID; }
    Structure* structure() const;
    Structure* structure(VM&) const;
    void setStructure(VM&, Structure*);
    void setStructureIDDirectly(StructureID id) { m_structureID = id; }
    void clearStructure() { m_structureID = 0; }

    TypeInfo::InlineTypeFlags inlineTypeFlags() const { return m_flags; }
    
    const char* className(VM&) const;

    // Extracting the value.
    JS_EXPORT_PRIVATE bool getString(ExecState*, String&) const;
    JS_EXPORT_PRIVATE String getString(ExecState*) const; // null string if not a string
    JS_EXPORT_PRIVATE JSObject* getObject(); // NULL if not an object
    const JSObject* getObject() const; // NULL if not an object
        
    // Returns information about how to call/construct this cell as a function/constructor. May tell
    // you that the cell is not callable or constructor (default is that it's not either). If it
    // says that the function is callable, and the OverridesGetCallData type flag is set, and
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
    JSObject* toObject(ExecState*, JSGlobalObject*) const;

    void dump(PrintStream&) const;
    JS_EXPORT_PRIVATE static void dumpToStream(const JSCell*, PrintStream&);

    size_t estimatedSizeInBytes(VM&) const;
    JS_EXPORT_PRIVATE static size_t estimatedSize(JSCell*, VM&);

    static void visitChildren(JSCell*, SlotVisitor&);
    static void visitOutputConstraints(JSCell*, SlotVisitor&);

    JS_EXPORT_PRIVATE static void analyzeHeap(JSCell*, HeapAnalyzer&);

    // Object operations, with the toObject operation included.
    const ClassInfo* classInfo(VM&) const;
    const MethodTable* methodTable(VM&) const;
    static bool put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
    static bool putByIndex(JSCell*, ExecState*, unsigned propertyName, JSValue, bool shouldThrow);
    bool putInline(ExecState*, PropertyName, JSValue, PutPropertySlot&);
        
    static bool deleteProperty(JSCell*, ExecState*, PropertyName);
    static bool deletePropertyByIndex(JSCell*, ExecState*, unsigned propertyName);

    static JSValue toThis(JSCell*, ExecState*, ECMAMode);

    static bool canUseFastGetOwnProperty(const Structure&);
    JSValue fastGetOwnProperty(VM&, Structure&, PropertyName);

    // The recommended idiom for using cellState() is to switch on it or perform an == comparison on it
    // directly. We deliberately avoid helpers for this, because we want transparency about how the various
    // CellState values influences our various algorithms. 
    CellState cellState() const { return m_cellState; }
    
    void setCellState(CellState data) const { const_cast<JSCell*>(this)->m_cellState = data; }
    
    bool atomicCompareExchangeCellStateWeakRelaxed(CellState oldState, CellState newState)
    {
        return WTF::atomicCompareExchangeWeakRelaxed(&m_cellState, oldState, newState);
    }

    CellState atomicCompareExchangeCellStateStrong(CellState oldState, CellState newState)
    {
        return WTF::atomicCompareExchangeStrong(&m_cellState, oldState, newState);
    }

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

    // DO NOT store to this field. Always use a CAS loop, since some bits are flipped using CAS
    // from other threads due to the internal lock. One exception: you don't need the CAS if the
    // object has not escaped yet.
    static ptrdiff_t indexingTypeAndMiscOffset()
    {
        return OBJECT_OFFSETOF(JSCell, m_indexingTypeAndMisc);
    }

    static ptrdiff_t cellStateOffset()
    {
        return OBJECT_OFFSETOF(JSCell, m_cellState);
    }
    
    static const TypedArrayType TypedArrayStorageType = NotTypedArray;

    void setPerCellBit(bool);
    bool perCellBit() const;
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
    static NO_RETURN_DUE_TO_CRASH bool setPrototype(JSObject*, ExecState*, JSValue, bool);
    static NO_RETURN_DUE_TO_CRASH JSValue getPrototype(JSObject*, ExecState*);

    static String className(const JSObject*, VM&);
    static String toStringName(const JSObject*, ExecState*);
    JS_EXPORT_PRIVATE static bool customHasInstance(JSObject*, ExecState*, JSValue);
    static bool defineOwnProperty(JSObject*, ExecState*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    static bool getOwnPropertySlotByIndex(JSObject*, ExecState*, unsigned propertyName, PropertySlot&);
    static NO_RETURN_DUE_TO_CRASH void doPutPropertySecurityCheck(JSObject*, ExecState*, PropertyName, PutPropertySlot&);

private:
    friend class LLIntOffsetsExtractor;
    friend class JSCellLock;

    JS_EXPORT_PRIVATE JSObject* toObjectSlow(ExecState*, JSGlobalObject*) const;

    StructureID m_structureID;
    IndexingType m_indexingTypeAndMisc; // DO NOT store to this field. Always CAS.
    JSType m_type;
    TypeInfo::InlineTypeFlags m_flags;
    CellState m_cellState;
};

class JSCellLock : public JSCell {
public:
    void lock();
    bool tryLock();
    void unlock();
    bool isLocked() const;
private:
    JS_EXPORT_PRIVATE void lockSlow();
    JS_EXPORT_PRIVATE void unlockSlow();
};

// FIXME: Refer to Subspace by reference.
// https://bugs.webkit.org/show_bug.cgi?id=166988
template<typename Type>
inline auto subspaceFor(VM& vm)
{
    return Type::template subspaceFor<Type, SubspaceAccess::OnMainThread>(vm);
}

template<typename Type>
inline auto subspaceForConcurrently(VM& vm)
{
    return Type::template subspaceFor<Type, SubspaceAccess::Concurrently>(vm);
}

} // namespace JSC
