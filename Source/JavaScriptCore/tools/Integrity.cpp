/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Integrity.h"

#include "APICast.h"
#include "CellSize.h"
#include "IntegrityInlines.h"
#include "JSCast.h"
#include "JSCellInlines.h"
#include "JSGlobalObject.h"
#include "Options.h"
#include "VMInspectorInlines.h"
#include <wtf/DataLog.h>
#include <wtf/OSLogPrintStream.h>

namespace JSC {
namespace Integrity {

namespace IntegrityInternal {
static constexpr bool verbose = false;
}

PrintStream& logFile()
{
#if OS(DARWIN)
    static PrintStream* s_file;
    static std::once_flag once;
    std::call_once(once, [] {
        // We want to use OS_LOG_TYPE_ERROR because we want to guarantee that the log makes it into
        // the file system, and is not potentially stuck in some memory buffer. Integrity audit logs
        // are used for debugging error states. So, this is an appropriate use of OS_LOG_TYPE_ERROR.
        s_file = OSLogPrintStream::open("com.apple.JavaScriptCore", "Integrity", OS_LOG_TYPE_ERROR).release();
    });
    return *s_file;
#else
    return WTF::dataFile();
#endif
}

void logF(const char* format, ...)
{
    va_list argList;
    va_start(argList, format);
    logFile().vprintf(format, argList);
    va_end(argList);
}

void logLnF(const char* format, ...)
{
    va_list argList;
    va_start(argList, format);
    logFile().vprintf(format, argList);
    va_end(argList);
    logFile().println();
}

Random::Random(VM& vm)
{
    reloadAndCheckShouldAuditSlow(vm);
}

bool Random::reloadAndCheckShouldAuditSlow(VM& vm)
{
    Locker locker { m_lock };

    if (!Options::randomIntegrityAuditRate()) {
        m_triggerBits = 0; // Never trigger, and don't bother reloading.
        if (IntegrityInternal::verbose)
            dataLogLn("disabled Integrity audits: trigger bits ", RawHex(m_triggerBits));
        return false;
    }

    // Reload the trigger bits.
    m_triggerBits = 1ull << 63;

    uint32_t threshold = UINT_MAX * Options::randomIntegrityAuditRate();
    for (int i = 0; i < numberOfTriggerBits; ++i) {
        bool trigger = vm.random().getUint32() <= threshold;
        m_triggerBits = m_triggerBits | (static_cast<uint64_t>(trigger) << i);
    }
    if (IntegrityInternal::verbose)
        dataLogLn("reloaded Integrity trigger bits ", RawHex(m_triggerBits));
    ASSERT(m_triggerBits >= (1ull << 63));
    return vm.random().getUint32() <= threshold;
}

void auditCellMinimallySlow(VM&, JSCell* cell)
{
    if (Gigacage::contains(cell)) {
        if (cell->type() != JSImmutableButterflyType) {
            if (IntegrityInternal::verbose)
                dataLogLn("Integrity ERROR: Bad cell ", RawPointer(cell), " ", JSValue(cell));
            CRASH();
        }
    }
}

#if USE(JSVALUE64)

JSContextRef doAudit(JSContextRef ctx)
{
    IA_ASSERT(ctx, "NULL JSContextRef");
    toJS(ctx); // toJS will trigger an audit.
    return ctx;
}

JSGlobalContextRef doAudit(JSGlobalContextRef ctx)
{
    IA_ASSERT(ctx, "NULL JSGlobalContextRef");
    toJS(ctx); // toJS will trigger an audit.
    return ctx;
}

JSObjectRef doAudit(JSObjectRef objectRef)
{
    if (!objectRef)
        return objectRef;
    toJS(objectRef); // toJS will trigger an audit.
    return objectRef;
}

JSValueRef doAudit(JSValueRef valueRef)
{
#if CPU(ADDRESS64)
    if (!valueRef)
        return valueRef;
    toJS(valueRef); // toJS will trigger an audit.
#endif
    return valueRef;
}

JSValue doAudit(JSValue value)
{
    if (value.isCell())
        doAudit(value.asCell());
    return value;
}

bool Analyzer::analyzeVM(VM& vm, Analyzer::Action action)
{
    IA_ASSERT_WITH_ACTION(VMInspector::isValidVM(&vm), {
        VMInspector::dumpVMs();
        if (action == Action::LogAndCrash)
            RELEASE_ASSERT(VMInspector::isValidVM(&vm));
        else
            return false;
    }, "Invalid VM %p", &vm);
    return true;
}

#if COMPILER(MSVC) || !VA_OPT_SUPPORTED

#define AUDIT_VERIFY(cond, format, ...) do { \
        IA_ASSERT_WITH_ACTION(cond, { \
            Integrity::logLnF("    cell %p", cell); \
            if (action == Action::LogAndCrash) \
                RELEASE_ASSERT((cond)); \
            else \
                return false; \
        }); \
    } while (false)

#else // not (COMPILER(MSVC) || !VA_OPT_SUPPORTED)

#define AUDIT_VERIFY(cond, format, ...) do { \
        IA_ASSERT_WITH_ACTION(cond, { \
            Integrity::logLnF("    cell %p", cell); \
            if (action == Action::LogAndCrash) \
                RELEASE_ASSERT((cond) __VA_OPT__(,) __VA_ARGS__); \
            else \
                return false; \
        }, format __VA_OPT__(,) __VA_ARGS__); \
    } while (false)

#endif // COMPILER(MSVC) || !VA_OPT_SUPPORTED

bool Analyzer::analyzeCell(VM& vm, JSCell* cell, Analyzer::Action action)
{
    AUDIT_VERIFY(isSanePointer(cell), "cell %p cell.type %d", cell, cell->type());

    size_t allocatorCellSize = 0;
    if (cell->isPreciseAllocation()) {
        PreciseAllocation& preciseAllocation = cell->preciseAllocation();
        AUDIT_VERIFY(&preciseAllocation.vm() == &vm,
            "cell %p cell.type %d preciseAllocation.vm %p vm %p", cell, cell->type(), &preciseAllocation.vm(), &vm);

        bool isValidPreciseAllocation = false;
        for (auto* i : vm.heap.objectSpace().preciseAllocations()) {
            if (i == &preciseAllocation) {
                isValidPreciseAllocation = true;
                break;
            }
        }
        AUDIT_VERIFY(isValidPreciseAllocation, "cell %p cell.type %d", cell, cell->type());

        allocatorCellSize = preciseAllocation.cellSize();
    } else {
        MarkedBlock& block = cell->markedBlock();
        MarkedBlock::Handle& blockHandle = block.handle();
        AUDIT_VERIFY(&block.vm() == &vm,
            "cell %p cell.type %d markedBlock.vm %p vm %p", cell, cell->type(), &block.vm(), &vm);

        uintptr_t blockStartAddress = reinterpret_cast<uintptr_t>(blockHandle.start());
        AUDIT_VERIFY(blockHandle.contains(cell),
            "cell %p cell.type %d markedBlock.start %p markedBlock.end %p", cell, cell->type(), blockHandle.start(), blockHandle.end());

        uintptr_t cellAddress = reinterpret_cast<uintptr_t>(cell);
        uintptr_t cellOffset = cellAddress - blockStartAddress;
        allocatorCellSize = block.cellSize();
        bool cellIsProperlyAligned = !(cellOffset % allocatorCellSize);
        AUDIT_VERIFY(cellIsProperlyAligned,
            "cell %p cell.type %d allocator.cellSize %zu", cell, cell->type(), allocatorCellSize);
    }

    JSType cellType = cell->type();
    if (cell->type() != JSImmutableButterflyType)
        AUDIT_VERIFY(!Gigacage::contains(cell), "cell %p cell.type %d", cell, cellType);

    WeakSet& weakSet = cell->cellContainer().weakSet();
    AUDIT_VERIFY(!weakSet.m_allocator || isSanePointer(weakSet.m_allocator),
        "cell %p cell.type %d weakSet.allocator %p", cell, cell->type(), weakSet.m_allocator);
    AUDIT_VERIFY(!weakSet.m_nextAllocator || isSanePointer(weakSet.m_nextAllocator),
        "cell %p cell.type %d weakSet.allocator %p", cell, cell->type(), weakSet.m_nextAllocator);

    // If we're currently destructing the cell, then we can't rely on its
    // structure being good. Skip the following tests which rely on structure.
    if (vm.currentlyDestructingCallbackObject == cell)
        return true;

    auto structureID = cell->structureID();

    Structure* structure = structureID.tryDecode();
    AUDIT_VERIFY(structure,
        "cell %p cell.type %d structureID.bits 0x%x", cell, cellType, structureID.bits());
    if (action == Analyzer::Action::LogAndCrash) {
        // structure should be pointing to readable memory. Force a read.
        WTF::opaque(*bitwise_cast<uintptr_t*>(structure));
    }

    const ClassInfo* classInfo = structure->classInfoForCells();
    AUDIT_VERIFY(cellType == structure->m_blob.type(),
        "cell %p cell.type %d structureBlob.type %d", cell, cellType, structure->m_blob.type());

    size_t size = cellSize(cell);
    AUDIT_VERIFY(size <= allocatorCellSize,
        "cell %p cell.type %d cell.size %zu allocator.cellSize %zu, classInfo.cellSize %u", cell, cellType, size, allocatorCellSize, classInfo->staticClassSize);
    if (isDynamicallySizedType(cellType)) {
        AUDIT_VERIFY(size >= classInfo->staticClassSize,
            "cell %p cell.type %d cell.size %zu classInfo.cellSize %u", cell, cellType, size, classInfo->staticClassSize);
    }

    if (cell->isObject()) {
        AUDIT_VERIFY(jsDynamicCast<JSObject*>(cell),
            "cell %p cell.type %d", cell, cell->type());

        if (Gigacage::isEnabled(Gigacage::JSValue)) {
            JSObject* object = bitwise_cast<JSObject*>(cell);
            const Butterfly* butterfly = object->butterfly();
            AUDIT_VERIFY(!butterfly || Gigacage::isCaged(Gigacage::JSValue, butterfly),
                "cell %p cell.type %d butterfly %p", cell, cell->type(), butterfly);
        }
    }

    return true;
}

bool Analyzer::analyzeCell(JSCell* cell, Analyzer::Action action)
{
    if (!cell)
        return cell;

    JSValue value = JSValue::decode(static_cast<EncodedJSValue>(bitwise_cast<uintptr_t>(cell)));
    AUDIT_VERIFY(value.isCell(), "Invalid cell address: cell %p", cell);

    VM& vm = cell->vm();
    analyzeVM(vm, action);
    return analyzeCell(vm, cell, action);
}

#undef AUDIT_VERIFY

VM* doAuditSlow(VM* vm)
{
    Analyzer::analyzeVM(*vm, Analyzer::Action::LogAndCrash);
    return vm;
}

JSCell* doAudit(JSCell* cell)
{
    Analyzer::analyzeCell(cell, Analyzer::Action::LogAndCrash);
    return cell;
}

JSCell* doAudit(VM& vm, JSCell* cell)
{
    if (!cell)
        return cell;
    Analyzer::analyzeCell(vm, cell, Analyzer::Action::LogAndCrash);
    return cell;
}

bool verifyCell(JSCell* cell)
{
    bool valid = Analyzer::analyzeCell(cell, Analyzer::Action::LogOnly);
    Integrity::logLnF("Cell %p is %s", cell, valid ? "VALID" : "INVALID");
    return valid;
}

bool verifyCell(VM& vm, JSCell* cell)
{
    bool valid = Analyzer::analyzeCell(vm, cell, Analyzer::Action::LogOnly);
    Integrity::logLnF("Cell %p is %s", cell, valid ? "VALID" : "INVALID");
    return valid;
}

JSObject* doAudit(JSObject* object)
{
    if (!object)
        return object;
    JSCell* cell = doAudit(reinterpret_cast<JSCell*>(object));
    IA_ASSERT(cell->isObject(), "Invalid JSObject %p", object);
    return object;
}

JSGlobalObject* doAudit(JSGlobalObject* globalObject)
{
    doAudit(reinterpret_cast<JSCell*>(globalObject));
    IA_ASSERT(globalObject->isGlobalObject(), "Invalid JSGlobalObject %p", globalObject);
    return globalObject;
}

#endif // USE(JSVALUE64)

} // namespace Integrity
} // namespace JSC
