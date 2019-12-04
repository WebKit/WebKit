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

#include "config.h"
#include "JSCell.h"

#include "ArrayBufferView.h"
#include "JSCInlines.h"
#include "JSCast.h"
#include "JSFunction.h"
#include "JSString.h"
#include "JSObject.h"
#include "NumberObject.h"
#include <wtf/LockAlgorithmInlines.h>
#include <wtf/MathExtras.h>

namespace JSC {

COMPILE_ASSERT(sizeof(JSCell) == sizeof(uint64_t), jscell_is_eight_bytes);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSCell);

void JSCell::destroy(JSCell* cell)
{
    cell->JSCell::~JSCell();
}

void JSCell::dump(PrintStream& out) const
{
    methodTable(vm())->dumpToStream(this, out);
}

void JSCell::dumpToStream(const JSCell* cell, PrintStream& out)
{
    out.printf("<%p, %s>", cell, cell->className(cell->vm()));
}

size_t JSCell::estimatedSizeInBytes(VM& vm) const
{
    return methodTable(vm)->estimatedSize(const_cast<JSCell*>(this), vm);
}

size_t JSCell::estimatedSize(JSCell* cell, VM&)
{
    return cell->cellSize();
}

void JSCell::analyzeHeap(JSCell*, HeapAnalyzer&)
{
}

bool JSCell::getString(JSGlobalObject* globalObject, String& stringValue) const
{
    if (!isString())
        return false;
    stringValue = static_cast<const JSString*>(this)->value(globalObject);
    return true;
}

String JSCell::getString(JSGlobalObject* globalObject) const
{
    return isString() ? static_cast<const JSString*>(this)->value(globalObject) : String();
}

JSObject* JSCell::getObject()
{
    return isObject() ? asObject(this) : 0;
}

const JSObject* JSCell::getObject() const
{
    return isObject() ? static_cast<const JSObject*>(this) : 0;
}

CallType JSCell::getCallData(JSCell*, CallData& callData)
{
    callData.js.functionExecutable = nullptr;
    callData.js.scope = nullptr;
    callData.native.function = nullptr;
    return CallType::None;
}

ConstructType JSCell::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.js.functionExecutable = nullptr;
    constructData.js.scope = nullptr;
    constructData.native.function = nullptr;
    return ConstructType::None;
}

bool JSCell::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName identifier, JSValue value, PutPropertySlot& slot)
{
    if (cell->isString() || cell->isSymbol() || cell->isBigInt())
        return JSValue(cell).putToPrimitive(globalObject, identifier, value, slot);

    JSObject* thisObject = cell->toObject(globalObject);
    return thisObject->methodTable(globalObject->vm())->put(thisObject, globalObject, identifier, value, slot);
}

bool JSCell::putByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned identifier, JSValue value, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    if (cell->isString() || cell->isSymbol() || cell->isBigInt()) {
        PutPropertySlot slot(cell, shouldThrow);
        return JSValue(cell).putToPrimitive(globalObject, Identifier::from(vm, identifier), value, slot);
    }
    JSObject* thisObject = cell->toObject(globalObject);
    return thisObject->methodTable(vm)->putByIndex(thisObject, globalObject, identifier, value, shouldThrow);
}

bool JSCell::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName identifier)
{
    JSObject* thisObject = cell->toObject(globalObject);
    return thisObject->methodTable(globalObject->vm())->deleteProperty(thisObject, globalObject, identifier);
}

bool JSCell::deletePropertyByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned identifier)
{
    JSObject* thisObject = cell->toObject(globalObject);
    return thisObject->methodTable(globalObject->vm())->deletePropertyByIndex(thisObject, globalObject, identifier);
}

JSValue JSCell::toThis(JSCell* cell, JSGlobalObject* globalObject, ECMAMode ecmaMode)
{
    if (ecmaMode == StrictMode)
        return cell;
    return cell->toObject(globalObject);
}

JSValue JSCell::toPrimitive(JSGlobalObject* globalObject, PreferredPrimitiveType preferredType) const
{
    if (isString())
        return static_cast<const JSString*>(this)->toPrimitive(globalObject, preferredType);
    if (isSymbol())
        return static_cast<const Symbol*>(this)->toPrimitive(globalObject, preferredType);
    if (isBigInt())
        return static_cast<const JSBigInt*>(this)->toPrimitive(globalObject, preferredType);
    return static_cast<const JSObject*>(this)->toPrimitive(globalObject, preferredType);
}

bool JSCell::getPrimitiveNumber(JSGlobalObject* globalObject, double& number, JSValue& value) const
{
    if (isString())
        return static_cast<const JSString*>(this)->getPrimitiveNumber(globalObject, number, value);
    if (isSymbol())
        return static_cast<const Symbol*>(this)->getPrimitiveNumber(globalObject, number, value);
    if (isBigInt())
        return static_cast<const JSBigInt*>(this)->getPrimitiveNumber(globalObject, number, value);
    return static_cast<const JSObject*>(this)->getPrimitiveNumber(globalObject, number, value);
}

double JSCell::toNumber(JSGlobalObject* globalObject) const
{
    if (isString())
        return static_cast<const JSString*>(this)->toNumber(globalObject);
    if (isSymbol())
        return static_cast<const Symbol*>(this)->toNumber(globalObject);
    if (isBigInt())
        return static_cast<const JSBigInt*>(this)->toNumber(globalObject);
    return static_cast<const JSObject*>(this)->toNumber(globalObject);
}

JSObject* JSCell::toObjectSlow(JSGlobalObject* globalObject) const
{
    ASSERT(!isObject());
    if (isString())
        return static_cast<const JSString*>(this)->toObject(globalObject);
    if (isBigInt())
        return static_cast<const JSBigInt*>(this)->toObject(globalObject);
    ASSERT(isSymbol());
    return static_cast<const Symbol*>(this)->toObject(globalObject);
}

void slowValidateCell(JSCell* cell)
{
    ASSERT_GC_OBJECT_LOOKS_VALID(cell);
}

JSValue JSCell::defaultValue(const JSObject*, JSGlobalObject*, PreferredPrimitiveType)
{
    RELEASE_ASSERT_NOT_REACHED();
    return jsUndefined();
}

bool JSCell::getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&)
{
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool JSCell::getOwnPropertySlotByIndex(JSObject*, JSGlobalObject*, unsigned, PropertySlot&)
{
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

void JSCell::doPutPropertySecurityCheck(JSObject*, JSGlobalObject*, PropertyName, PutPropertySlot&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void JSCell::getOwnPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void JSCell::getOwnNonIndexPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

String JSCell::className(const JSObject*, VM&)
{
    RELEASE_ASSERT_NOT_REACHED();
    return String();
}

String JSCell::toStringName(const JSObject*, JSGlobalObject*)
{
    RELEASE_ASSERT_NOT_REACHED();
    return String();
}

const char* JSCell::className(VM& vm) const
{
    return classInfo(vm)->className;
}

void JSCell::getPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

bool JSCell::customHasInstance(JSObject*, JSGlobalObject*, JSValue)
{
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool JSCell::defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool)
{
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

uint32_t JSCell::getEnumerableLength(JSGlobalObject*, JSObject*)
{
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

void JSCell::getStructurePropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void JSCell::getGenericPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

bool JSCell::preventExtensions(JSObject*, JSGlobalObject*)
{
    RELEASE_ASSERT_NOT_REACHED();
}

bool JSCell::isExtensible(JSObject*, JSGlobalObject*)
{
    RELEASE_ASSERT_NOT_REACHED();
}

bool JSCell::setPrototype(JSObject*, JSGlobalObject*, JSValue, bool)
{
    RELEASE_ASSERT_NOT_REACHED();
}

JSValue JSCell::getPrototype(JSObject*, JSGlobalObject*)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void JSCellLock::lockSlow()
{
    Atomic<IndexingType>* lock = bitwise_cast<Atomic<IndexingType>*>(&m_indexingTypeAndMisc);
    IndexingTypeLockAlgorithm::lockSlow(*lock);
}

void JSCellLock::unlockSlow()
{
    Atomic<IndexingType>* lock = bitwise_cast<Atomic<IndexingType>*>(&m_indexingTypeAndMisc);
    IndexingTypeLockAlgorithm::unlockSlow(*lock);
}

} // namespace JSC
