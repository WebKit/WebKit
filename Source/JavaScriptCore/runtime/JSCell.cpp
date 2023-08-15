/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include "IntegrityInlines.h"
#include "IsoSubspaceInlines.h"
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "MarkedBlockInlines.h"
#include "SubspaceInlines.h"
#include "Symbol.h"
#include <wtf/LockAlgorithmInlines.h>

namespace JSC {

const ASCIILiteral SymbolCoercionError { "Cannot convert a symbol to a string"_s };

static_assert(sizeof(JSCell) == sizeof(uint64_t), "jscell is eight bytes");
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSCell);

void JSCell::destroy(JSCell* cell)
{
    cell->JSCell::~JSCell();
}

void JSCell::dump(PrintStream& out) const
{
    methodTable()->dumpToStream(this, out);
}

void JSCell::dumpToStream(const JSCell* cell, PrintStream& out)
{
    out.printf("<%p, %s>", cell, cell->className().characters());
}

size_t JSCell::estimatedSizeInBytes(VM& vm) const
{
    return methodTable()->estimatedSize(const_cast<JSCell*>(this), vm);
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
    return isObject() ? asObject(this) : nullptr;
}

const JSObject* JSCell::getObject() const
{
    return isObject() ? static_cast<const JSObject*>(this) : nullptr;
}

CallData JSCell::getCallData(JSCell*)
{
    return { };
}

CallData JSCell::getConstructData(JSCell*)
{
    return { };
}

bool JSCell::isValidCallee() const
{
    return isObject() && asObject(this)->globalObject();
}

bool JSCell::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName identifier, JSValue value, PutPropertySlot& slot)
{
    if (cell->isString() || cell->isSymbol() || cell->isHeapBigInt())
        return JSValue(cell).putToPrimitive(globalObject, identifier, value, slot);

    JSObject* thisObject = cell->toObject(globalObject);
    return thisObject->methodTable()->put(thisObject, globalObject, identifier, value, slot);
}

bool JSCell::putByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned identifier, JSValue value, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    if (cell->isString() || cell->isSymbol() || cell->isHeapBigInt()) {
        PutPropertySlot slot(cell, shouldThrow);
        return JSValue(cell).putToPrimitive(globalObject, Identifier::from(vm, identifier), value, slot);
    }
    JSObject* thisObject = cell->toObject(globalObject);
    return thisObject->methodTable()->putByIndex(thisObject, globalObject, identifier, value, shouldThrow);
}

bool JSCell::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName identifier, DeletePropertySlot& slot)
{
    JSObject* thisObject = cell->toObject(globalObject);
    return thisObject->methodTable()->deleteProperty(thisObject, globalObject, identifier, slot);
}

bool JSCell::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName identifier)
{
    JSObject* thisObject = cell->toObject(globalObject);
    DeletePropertySlot slot;
    return thisObject->methodTable()->deleteProperty(thisObject, globalObject, identifier, slot);
}

bool JSCell::deletePropertyByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned identifier)
{
    JSObject* thisObject = cell->toObject(globalObject);
    return thisObject->methodTable()->deletePropertyByIndex(thisObject, globalObject, identifier);
}

JSValue JSCell::toPrimitive(JSGlobalObject* globalObject, PreferredPrimitiveType preferredType) const
{
    if (isString())
        return static_cast<const JSString*>(this)->toPrimitive(globalObject, preferredType);
    if (isSymbol())
        return static_cast<const Symbol*>(this)->toPrimitive(globalObject, preferredType);
    if (isHeapBigInt())
        return static_cast<const JSBigInt*>(this)->toPrimitive(globalObject, preferredType);
    return static_cast<const JSObject*>(this)->toPrimitive(globalObject, preferredType);
}

double JSCell::toNumber(JSGlobalObject* globalObject) const
{
    if (isString())
        return static_cast<const JSString*>(this)->toNumber(globalObject);
    if (isSymbol())
        return static_cast<const Symbol*>(this)->toNumber(globalObject);
    if (isHeapBigInt())
        return static_cast<const JSBigInt*>(this)->toNumber(globalObject);
    return static_cast<const JSObject*>(this)->toNumber(globalObject);
}

JSObject* JSCell::toObjectSlow(JSGlobalObject* globalObject) const
{
    Integrity::auditStructureID(structureID());
    ASSERT(!isObject());
    if (isString())
        return static_cast<const JSString*>(this)->toObject(globalObject);
    if (isHeapBigInt())
        return static_cast<const JSBigInt*>(this)->toObject(globalObject);
    ASSERT(isSymbol());
    return static_cast<const Symbol*>(this)->toObject(globalObject);
}

void slowValidateCell(JSCell* cell)
{
    ASSERT_GC_OBJECT_LOOKS_VALID(cell);
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

void JSCell::getOwnPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, DontEnumPropertiesMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void JSCell::getOwnSpecialPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, DontEnumPropertiesMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

ASCIILiteral JSCell::className() const
{
    return classInfo()->className;
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

JSString* JSCell::toStringSlowCase(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(isSymbol() || isHeapBigInt());
    auto* emptyString = jsEmptyString(vm);
    if (auto* bigInt = jsDynamicCast<JSBigInt*>(const_cast<JSCell*>(this))) {
        // FIXME: we should rather have two cases here: one-character string vs jsNonTrivialString for everything else.
        auto string = bigInt->toString(globalObject, 10);
        RETURN_IF_EXCEPTION(scope, emptyString);
        JSString* returnString = JSString::create(vm, string.releaseImpl().releaseNonNull());
        RETURN_IF_EXCEPTION(scope, emptyString);
        return returnString;
    }
    ASSERT(isSymbol());
    throwTypeError(globalObject, scope, SymbolCoercionError);
    return emptyString;
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

#if CPU(X86_64)
NEVER_INLINE NO_RETURN_DUE_TO_CRASH NOT_TAIL_CALLED void reportZappedCellAndCrash(Heap& heap, const JSCell* cell)
{
    MarkedBlock::Handle* foundBlockHandle = nullptr;
    uint64_t* cellWords = bitwise_cast<uint64_t*>(cell);

    uintptr_t cellAddress = bitwise_cast<uintptr_t>(cell);
    uint64_t headerWord = cellWords[0];
    uint64_t zapReasonAndMore = cellWords[1];
    unsigned subspaceHash = 0;
    size_t cellSize = 0;

    heap.objectSpace().forEachBlock([&](MarkedBlock::Handle* blockHandle) {
        if (blockHandle->contains(bitwise_cast<JSCell*>(cell))) {
            foundBlockHandle = blockHandle;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });

    uint64_t variousState = 0;
    MarkedBlock* foundBlock = nullptr;
    if (foundBlockHandle) {
        foundBlock = &foundBlockHandle->block();
        subspaceHash = SuperFastHash::computeHash(foundBlockHandle->subspace()->name());
        cellSize = foundBlockHandle->cellSize();

        variousState |= static_cast<uint64_t>(foundBlockHandle->isFreeListed()) << 0;
        variousState |= static_cast<uint64_t>(foundBlockHandle->isAllocated()) << 1;
        variousState |= static_cast<uint64_t>(foundBlockHandle->isEmpty()) << 2;
        variousState |= static_cast<uint64_t>(foundBlockHandle->needsDestruction()) << 3;
        variousState |= static_cast<uint64_t>(foundBlock->isNewlyAllocated(cell)) << 4;

        ptrdiff_t cellOffset = cellAddress - bitwise_cast<uintptr_t>(foundBlockHandle->start());
        bool cellIsProperlyAligned = !(cellOffset % cellSize);
        variousState |= static_cast<uint64_t>(cellIsProperlyAligned) << 5;
    } else {
        bool isFreeListed = false;
        PreciseAllocation* foundPreciseAllocation = nullptr;
        heap.objectSpace().forEachSubspace([&](Subspace& subspace) {
            subspace.forEachPreciseAllocation([&](PreciseAllocation* allocation) {
                if (allocation->contains(cell))
                    foundPreciseAllocation = allocation;
            });
            if (foundPreciseAllocation)
                return IterationStatus::Done;

            if (subspace.isIsoSubspace()) {
                static_cast<IsoSubspace&>(subspace).forEachLowerTierFreeListedPreciseAllocation([&](PreciseAllocation* allocation) {
                    if (allocation->contains(cell)) {
                        foundPreciseAllocation = allocation;
                        isFreeListed = true;
                    }
                });
            }
            if (foundPreciseAllocation)
                return IterationStatus::Done;
            return IterationStatus::Continue;
        });
        if (foundPreciseAllocation) {
            subspaceHash = SuperFastHash::computeHash(foundPreciseAllocation->subspace()->name());
            cellSize = foundPreciseAllocation->cellSize();

            variousState |= static_cast<uint64_t>(isFreeListed) << 0;
            variousState |= static_cast<uint64_t>(!isFreeListed) << 1;
            variousState |= static_cast<uint64_t>(foundPreciseAllocation->subspace()->attributes().destruction == NeedsDestruction) << 3;
            if (!isFreeListed) {
                variousState |= static_cast<uint64_t>(foundPreciseAllocation->isEmpty()) << 2;
                variousState |= static_cast<uint64_t>(foundPreciseAllocation->isNewlyAllocated()) << 4;
            }
            bool cellIsProperlyAligned = foundPreciseAllocation->cell() == cell;
            variousState |= static_cast<uint64_t>(cellIsProperlyAligned) << 5;
        }
    }

    CRASH_WITH_INFO(cellAddress, headerWord, zapReasonAndMore, subspaceHash, cellSize, foundBlock, variousState);
}
#endif // CPU(X86_64)

} // namespace JSC
