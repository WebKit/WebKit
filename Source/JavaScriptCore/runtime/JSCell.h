/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "ConstructData.h"
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
class JSArrayBufferView;
class JSDestructibleObject;
class JSGlobalObject;
class LLIntOffsetsExtractor;
class PropertyDescriptor;
class PropertyNameArray;
class Structure;

enum EnumerationMode {
    ExcludeDontEnumProperties,
    IncludeDontEnumProperties
};

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
    static const bool hasImmortalStructure = false;

    enum CreatingEarlyCellTag { CreatingEarlyCell };
    JSCell(CreatingEarlyCellTag);

protected:
    JSCell(VM&, Structure*);
    JS_EXPORT_PRIVATE static void destroy(JSCell*);

public:
    // Querying the type.
    bool isString() const;
    bool isObject() const;
    bool isGetterSetter() const;
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

    const char* className();

    // Extracting the value.
    JS_EXPORT_PRIVATE bool getString(ExecState*, String&) const;
    JS_EXPORT_PRIVATE String getString(ExecState*) const; // null string if not a string
    JS_EXPORT_PRIVATE JSObject* getObject(); // NULL if not an object
    const JSObject* getObject() const; // NULL if not an object
        
    JS_EXPORT_PRIVATE static CallType getCallData(JSCell*, CallData&);
    JS_EXPORT_PRIVATE static ConstructType getConstructData(JSCell*, ConstructData&);

    // Basic conversions.
    JS_EXPORT_PRIVATE JSValue toPrimitive(ExecState*, PreferredPrimitiveType) const;
    bool getPrimitiveNumber(ExecState*, double& number, JSValue&) const;
    bool toBoolean(ExecState*) const;
    TriState pureToBoolean() const;
    JS_EXPORT_PRIVATE double toNumber(ExecState*) const;
    JS_EXPORT_PRIVATE JSObject* toObject(ExecState*, JSGlobalObject*) const;

    static void visitChildren(JSCell*, SlotVisitor&);
    JS_EXPORT_PRIVATE static void copyBackingStore(JSCell*, CopyVisitor&, CopyToken);

    // Object operations, with the toObject operation included.
    const ClassInfo* classInfo() const;
    const MethodTable* methodTable() const;
    const MethodTable* methodTable(VM&) const;
    const MethodTable* methodTableForDestruction() const;
    static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
    static void putByIndex(JSCell*, ExecState*, unsigned propertyName, JSValue, bool shouldThrow);
        
    static bool deleteProperty(JSCell*, ExecState*, PropertyName);
    static bool deletePropertyByIndex(JSCell*, ExecState*, unsigned propertyName);

    static JSValue toThis(JSCell*, ExecState*, ECMAMode);

    void zap() { *reinterpret_cast<uintptr_t**>(this) = 0; }
    bool isZapped() const { return !*reinterpret_cast<uintptr_t* const*>(this); }

    JSValue fastGetOwnProperty(VM&, const String&);

    enum GCData : uint8_t {
        Marked = 0,
        NotMarked = 1,
        MarkedAndRemembered = 2,
    };

    void setMarked() { m_gcData = Marked; }
    void setRemembered(bool remembered)
    {
        ASSERT(m_gcData == remembered ? Marked : MarkedAndRemembered);
        m_gcData = remembered ? MarkedAndRemembered : Marked; 
    }
    bool isMarked() const
    {
        switch (m_gcData) {
        case Marked:
        case MarkedAndRemembered:
            return true;
        case NotMarked:
            return false;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
    bool isRemembered() const { return m_gcData == MarkedAndRemembered; }

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

    static ptrdiff_t gcDataOffset()
    {
        return OBJECT_OFFSETOF(JSCell, m_gcData);
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
    uint8_t m_gcData;
};

template<typename To, typename From>
inline To jsCast(From* from)
{
    ASSERT(!from || from->JSCell::inherits(std::remove_pointer<To>::type::info()));
    return static_cast<To>(from);
}
    
template<typename To>
inline To jsCast(JSValue from)
{
    ASSERT(from.isCell() && from.asCell()->JSCell::inherits(std::remove_pointer<To>::type::info()));
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
