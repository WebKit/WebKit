/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
#include "VMInspector.h"

#include "CodeBlock.h"
#include "CodeBlockSet.h"
#include "HeapInlines.h"
#include "HeapIterationScope.h"
#include "JSCInlines.h"
#include "JSWebAssemblyModule.h"
#include "MarkedSpaceInlines.h"
#include "StackVisitor.h"
#include "VMEntryRecord.h"
#include <mutex>
#include <wtf/Expected.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(VMInspector);

VM* VMInspector::m_recentVM { nullptr };

VMInspector& VMInspector::instance()
{
    static VMInspector* manager;
    static std::once_flag once;
    std::call_once(once, [] {
        manager = new VMInspector();
    });
    return *manager;
}

void VMInspector::add(VM* vm)
{
    Locker locker { m_lock };
    m_recentVM = vm;
    m_vmList.append(vm);
}

void VMInspector::remove(VM* vm)
{
    Locker locker { m_lock };
    if (m_recentVM == vm)
        m_recentVM = nullptr;
    m_vmList.remove(vm);
}

#if ENABLE(JIT)
static bool ensureIsSafeToLock(Lock& lock) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    static constexpr unsigned maxRetries = 2;
    unsigned tryCount = 0;
    while (tryCount++ <= maxRetries) {
        if (lock.tryLock())
            return true;
    }
    return false;
}
#endif // ENABLE(JIT)

bool VMInspector::isValidVMSlow(VM* vm)
{
    bool found = false;
    forEachVM([&] (VM& nextVM) {
        if (vm == &nextVM) {
            m_recentVM = vm;
            found = true;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return found;
}

void VMInspector::dumpVMs()
{
    unsigned i = 0;
    WTFLogAlways("Registered VMs:");
    forEachVM([&] (VM& nextVM) {
        WTFLogAlways("  [%u] VM %p", i++, &nextVM);
        return IterationStatus::Continue;
    });
}

void VMInspector::forEachVM(Function<IterationStatus(VM&)>&& func)
{
    VMInspector& inspector = instance();
    Locker lock { inspector.getLock() };
    inspector.iterate(func);
}

// Returns null if the callFrame doesn't actually correspond to any active VM.
VM* VMInspector::vmForCallFrame(CallFrame* callFrame)
{
    VMInspector& inspector = instance();
    Locker lock { inspector.getLock() };

    auto isOnVMStack = [] (VM& vm, CallFrame* callFrame) -> bool {
        void* stackBottom = vm.stackPointerAtVMEntry(); // high memory
        void* stackTop = vm.stackLimit(); // low memory
        return stackBottom > callFrame && callFrame > stackTop;
    };

    if (m_recentVM && isOnVMStack(*m_recentVM, callFrame))
        return m_recentVM;

    VM* ownerVM = nullptr;
    inspector.iterate([&] (VM& vm) {
        if (isOnVMStack(vm, callFrame)) {
            ownerVM = &vm;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return ownerVM;
}

WTF_IGNORES_THREAD_SAFETY_ANALYSIS auto VMInspector::isValidExecutableMemory(void* machinePC) -> Expected<bool, Error>
{
#if ENABLE(JIT)
    auto& allocator = ExecutableAllocator::singleton();
    auto& lock = allocator.getLock();

    bool isSafeToLock = ensureIsSafeToLock(lock);
    if (!isSafeToLock)
        return makeUnexpected(Error::TimedOut);

    Locker executableAllocatorLocker { AdoptLock, lock };
    if (allocator.isValidExecutableMemory(executableAllocatorLocker, machinePC))
        return true;

    return false;
#else
    UNUSED_PARAM(machinePC);
    return false;
#endif
}

auto VMInspector::codeBlockForMachinePC(void* machinePC) -> Expected<CodeBlock*, Error>
{
#if ENABLE(JIT)
    CodeBlock* codeBlock = nullptr;
    bool hasTimeout = false;
    iterate([&] (VM& vm) WTF_IGNORES_THREAD_SAFETY_ANALYSIS {
        if (!vm.isInService())
            return IterationStatus::Continue;

        if (!vm.currentThreadIsHoldingAPILock())
            return IterationStatus::Continue;

        // It is safe to call Heap::forEachCodeBlockIgnoringJITPlans here because:
        // 1. CodeBlocks are added to the CodeBlockSet from the main thread before
        //    they are handed to the JIT plans. Those codeBlocks will have a null jitCode,
        //    but we check for that in our lambda functor.
        // 2. We will acquire the CodeBlockSet lock before iterating.
        //    This ensures that a CodeBlock won't be GCed while we're iterating.
        // 3. We do a tryLock on the CodeBlockSet's lock first to ensure that it is
        //    safe for the current thread to lock it before calling
        //    Heap::forEachCodeBlockIgnoringJITPlans(). Hence, there's no risk of
        //    re-entering the lock and deadlocking on it.

        auto& codeBlockSetLock = vm.heap.codeBlockSet().getLock();
        bool isSafeToLock = ensureIsSafeToLock(codeBlockSetLock);
        if (!isSafeToLock) {
            hasTimeout = true;
            return IterationStatus::Continue; // Skip this VM.
        }

        Locker locker { AdoptLock, codeBlockSetLock };
        vm.heap.forEachCodeBlockIgnoringJITPlans(locker, [&] (CodeBlock* cb) {
            JITCode* jitCode = cb->jitCode().get();
            if (!jitCode) {
                // If the codeBlock is a replacement codeBlock which is in the process of being
                // compiled, its jitCode will be null, and we can disregard it as a match for
                // the machinePC we're searching for.
                return;
            }

            if (!JITCode::isJIT(jitCode->jitType()))
                return;

            if (jitCode->contains(machinePC)) {
                codeBlock = cb;
                return;
            }
        });
        if (codeBlock)
            return IterationStatus::Done;
        return IterationStatus::Continue;
    });

    if (!codeBlock && hasTimeout)
        return makeUnexpected(Error::TimedOut);
    return codeBlock;
#else
    UNUSED_PARAM(machinePC);
    return nullptr;
#endif
}

bool VMInspector::currentThreadOwnsJSLock(VM* vm)
{
    return vm->currentThreadIsHoldingAPILock();
}

static bool ensureCurrentThreadOwnsJSLock(VM* vm)
{
    if (VMInspector::currentThreadOwnsJSLock(vm))
        return true;
    dataLog("ERROR: current thread does not own the JSLock\n");
    return false;
}

void VMInspector::gc(VM* vm)
{
    if (!ensureCurrentThreadOwnsJSLock(vm))
        return;
    vm->heap.collectNow(Sync, CollectionScope::Full);
}

void VMInspector::edenGC(VM* vm)
{
    if (!ensureCurrentThreadOwnsJSLock(vm))
        return;
    vm->heap.collectSync(CollectionScope::Eden);
}

bool VMInspector::isInHeap(Heap* heap, void* ptr)
{
    MarkedBlock* candidate = MarkedBlock::blockFor(ptr);
    if (heap->objectSpace().blocks().set().contains(candidate))
        return true;
    for (PreciseAllocation* allocation : heap->objectSpace().preciseAllocations()) {
        if (allocation->contains(ptr))
            return true;
    }
    return false;
}

struct CellAddressCheckFunctor : MarkedBlock::CountFunctor {
    CellAddressCheckFunctor(JSCell* candidate)
        : candidate(candidate)
    {
    }

    IterationStatus operator()(HeapCell* cell, HeapCell::Kind) const
    {
        if (cell == candidate) {
            found = true;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

    JSCell* candidate;
    mutable bool found { false };
};

bool VMInspector::isValidCell(Heap* heap, JSCell* candidate)
{
    HeapIterationScope iterationScope(*heap);
    CellAddressCheckFunctor functor(candidate);
    heap->objectSpace().forEachLiveCell(iterationScope, functor);
    return functor.found;
}

bool VMInspector::isValidCodeBlock(VM* vm, CodeBlock* candidate)
{
    if (!ensureCurrentThreadOwnsJSLock(vm))
        return false;

    struct CodeBlockValidationFunctor {
        CodeBlockValidationFunctor(CodeBlock* candidate)
            : candidate(candidate)
        {
        }

        void operator()(CodeBlock* codeBlock) const
        {
            if (codeBlock == candidate)
                found = true;
        }

        CodeBlock* candidate;
        mutable bool found { false };
    };

    CodeBlockValidationFunctor functor(candidate);
    vm->heap.forEachCodeBlock(functor);
    return functor.found;
}

CodeBlock* VMInspector::codeBlockForFrame(VM* vm, CallFrame* topCallFrame, unsigned frameNumber)
{
    if (!ensureCurrentThreadOwnsJSLock(vm))
        return nullptr;

    if (!topCallFrame)
        return nullptr;

    struct FetchCodeBlockFunctor {
    public:
        FetchCodeBlockFunctor(unsigned targetFrameNumber)
            : targetFrame(targetFrameNumber)
        {
        }

        IterationStatus operator()(StackVisitor& visitor) const
        {
            auto currentFrame = nextFrame++;
            if (currentFrame == targetFrame) {
                codeBlock = visitor->codeBlock();
                return IterationStatus::Done;
            }
            return IterationStatus::Continue;
        }

        unsigned targetFrame;
        mutable unsigned nextFrame { 0 };
        mutable CodeBlock* codeBlock { nullptr };
    };

    FetchCodeBlockFunctor functor(frameNumber);
    StackVisitor::visit(topCallFrame, *vm, functor);
    return functor.codeBlock;
}

class DumpFrameFunctor {
public:
    enum Action {
        DumpOne,
        DumpAll
    };

    DumpFrameFunctor(Action action, unsigned framesToSkip)
        : m_action(action)
        , m_framesToSkip(framesToSkip)
    {
    }

    IterationStatus operator()(StackVisitor& visitor) const
    {
        m_currentFrame++;
        if (m_currentFrame > m_framesToSkip) {
            visitor->dump(WTF::dataFile(), Indenter(2), [&] (PrintStream& out) {
                out.print("[", (m_currentFrame - m_framesToSkip - 1), "] ");
            });
        }
        if (m_action == DumpOne && m_currentFrame > m_framesToSkip)
            return IterationStatus::Done;
        return IterationStatus::Continue;
    }

private:
    Action m_action;
    unsigned m_framesToSkip;
    mutable unsigned m_currentFrame { 0 };
};

void VMInspector::dumpCallFrame(VM* vm, CallFrame* callFrame, unsigned framesToSkip)
{
    if (!ensureCurrentThreadOwnsJSLock(vm))
        return;
    DumpFrameFunctor functor(DumpFrameFunctor::DumpOne, framesToSkip);
    StackVisitor::visit(callFrame, *vm, functor);
}

SUPPRESS_ASAN void VMInspector::dumpRegisters(CallFrame* callFrame)
{
    VM* vmPtr = vmForCallFrame(callFrame);
    if (!vmPtr) {
        dataLogLn("Cannot find callFrame on any VM stack.");
        return;
    }

    VM& vm = *vmPtr;

    auto valueAsString = [&] (JSValue v) -> CString {
        if (!v.isCell() || VMInspector::isValidCell(&vm.heap, reinterpret_cast<JSCell*>(JSValue::encode(v))))
            return toCString(v);
        return "";
    };

    CallFrame* topCallFrame = vm.topCallFrame;
    CallFrame* nextCallFrame = nullptr;
    EntryFrame* entryFrame = nullptr;

    // Check if frame is an entryFrame.
    entryFrame = vm.topEntryFrame;
    while (entryFrame) {
        if (entryFrame == bitwise_cast<EntryFrame*>(callFrame)) {
            dataLogLn("CallFrame ", RawPointer(callFrame), " is an EntryFrame.");
            auto* entryRecord = vmEntryRecord(entryFrame);
            dataLogLn("    previous entryFrame: ", RawPointer(entryRecord->prevTopEntryFrame()));
            dataLogLn("    previous topCallFrame: ", RawPointer(entryRecord->prevTopCallFrame()));
            return;
        }
        auto* entryRecord = vmEntryRecord(entryFrame);
        entryFrame = entryRecord->prevTopEntryFrame();
    }

    // Find entryFrame and next frame.
    if (topCallFrame) {
        StackVisitor::visit(topCallFrame, vm, [&] (StackVisitor& visitor) {
            if (callFrame == visitor->callFrame()) {
                entryFrame = visitor->entryFrame();
                return IterationStatus::Done;
            }
            nextCallFrame = visitor->callFrame();
            return IterationStatus::Continue;
        });
    }

    // Dumping from low memory to high memory.
    JSCell* owner = callFrame->codeOwnerCell();
    CodeBlock* codeBlock = jsDynamicCast<CodeBlock*>(owner);
    unsigned numCalleeLocals = codeBlock ? codeBlock->numCalleeLocals() : 0;
    unsigned numVars = codeBlock ? codeBlock->numVars() : 0;
    bool isWasm = false;
#if ENABLE(WEBASSEMBLY)
    isWasm = owner->inherits<JSWebAssemblyModule>();
#endif

    const Register* it;
    const Register* callFrameTop = callFrame->registers();
    const Register* startOfLocals = callFrameTop - numCalleeLocals;
    const Register* startOfVars = callFrameTop - numVars;

    if (nextCallFrame)
        it = nextCallFrame->registers() + static_cast<int>(CallFrameSlot::thisArgument);
    else
        it = startOfLocals;

    int registerNumber = it - callFrame->registers();
    const char* frameType = isWasm ? "Wasm" : codeBlock ? "JS" : "native";

    dataLogF("Registers for %s frame 0x%llx (entryFrame ", frameType, (long long)callFrame);
    if (entryFrame)
        dataLogF("0x%llx):\n", (long long)entryFrame);
    else
        dataLogF("unknown):\n");
    dataLogF("-----------------------------------------------------------------------------\n");
    dataLogF("   VirtualRegister     : address      value\n");

    if (codeBlock) {
        dataLogF("---------------------------------------------------- Outgoing Args + Misc ---\n");
        
        while (it < startOfVars) {
            JSValue v = it->jsValue();
            String name = codeBlock->nameForRegister(VirtualRegister(registerNumber));
            dataLogF("% 4d  %-16s : %10p  0x%llx %s\n", registerNumber++, name.ascii().data(), it++, (long long)JSValue::encode(v), valueAsString(v).data());
        }
        
        dataLogF("--------------------------------------------------------------- Variables ---\n");
        
        
        size_t numberOfCalleeSaveSlots = CodeBlock::calleeSaveSpaceAsVirtualRegisters(*codeBlock->jitCode()->calleeSaveRegisters());
        const Register* endOfCalleeSaves = callFrameTop - numberOfCalleeSaveSlots;
        
        while (it < endOfCalleeSaves) {
            JSValue v = it->jsValue();
            String name = codeBlock->nameForRegister(VirtualRegister(registerNumber));
            dataLogF("% 4d  %-16s : %10p  0x%llx %s\n", registerNumber++, name.ascii().data(), it++, (long long)JSValue::encode(v), valueAsString(v).data());
        }
        
        dataLogF("------------------------------------------------------------ Callee Saves ---\n");
        
        while (it != callFrameTop) {
            JSValue v = it->jsValue();
            dataLogF("% 4d  %-16s : %10p  0x%llx %s\n", registerNumber++, "CalleeSaveReg", it++, (long long)JSValue::encode(v), valueAsString(v).data());
        }
    }

    dataLogF("-------------------------------------------------------- CallFrame Header ---\n");
    
    dataLogF("% 4d  CallerFrame      : %10p  %p \n", registerNumber++, it++, callFrame->callerFrame());
    if constexpr (isARM64E())
        dataLogF("% 4d  ReturnPC         : %10p  %p (pac signed %p) \n", registerNumber++, it++, callFrame->returnPCForInspection(), callFrame->rawReturnPCForInspection());
    else
        dataLogF("% 4d  ReturnPC         : %10p  %p \n", registerNumber++, it++, callFrame->returnPCForInspection());
    dataLogF("% 4d  CodeBlock        : %10p  0x%llx ", registerNumber++, it++, (long long)codeBlock);
    dataLogLn(codeBlock);
    long long calleeBits = (long long)callFrame->callee().rawPtr();
    auto calleeString = valueAsString(it->jsValue()).data();
    dataLogF("% 4d  Callee           : %10p  0x%llx %s\n", registerNumber++, it++, calleeBits, calleeString);
    
    StackVisitor::visit(callFrame, vm, [&] (StackVisitor& visitor) {
        if (visitor->callFrame() == callFrame) {
            auto lineColumn = visitor->computeLineAndColumn();
            dataLogF("% 2d.1  ReturnVPC        : %10p  %d (line %d)\n", registerNumber, it, visitor->bytecodeIndex().offset(), lineColumn.line);
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    dataLogF("% 2d.2  ArgumentCount    : %10p  %lu \n", registerNumber++, it++, (unsigned long) callFrame->argumentCount());
    
    dataLogF("--------------------------------------------------------------- Arguments ---\n");
    
    const Register* bottom = callFrame->registers() + (CallFrameSlot::thisArgument + callFrame->argumentCount());
    while (it <= bottom) {
        JSValue v = it->jsValue();
        String name = codeBlock ? codeBlock->nameForRegister(VirtualRegister(registerNumber)) : emptyString();
        dataLogF("% 4d  %-16s : %10p  0x%llx %s\n", registerNumber++, name.ascii().data(), it++, (long long)JSValue::encode(v), valueAsString(v).data());
    }
    
    dataLogF("--------------------------------------------------------------------- End ---\n");
}

void VMInspector::dumpStack(VM* vm, CallFrame* topCallFrame, unsigned framesToSkip)
{
    if (!ensureCurrentThreadOwnsJSLock(vm))
        return;
    if (!topCallFrame)
        return;
    DumpFrameFunctor functor(DumpFrameFunctor::DumpAll, framesToSkip);
    StackVisitor::visit(topCallFrame, *vm, functor);
}

void VMInspector::dumpValue(JSValue value)
{
    dataLogLn(value);
}

void VMInspector::dumpCellMemory(JSCell* cell)
{
    dumpCellMemoryToStream(cell, WTF::dataFile());
}

class IndentationScope {
public:
    IndentationScope(unsigned& indentation)
        : m_indentation(indentation)
    {
        ++m_indentation;
    }

    ~IndentationScope()
    {
        --m_indentation;
    }

private:
    unsigned& m_indentation;
};

void VMInspector::dumpCellMemoryToStream(JSCell* cell, PrintStream& out)
{
    StructureID structureID = cell->structureID();
    Structure* structure = cell->structure();
    IndexingType indexingTypeAndMisc = cell->indexingTypeAndMisc();
    IndexingType indexingType = structure->indexingType();
    IndexingType indexingMode = structure->indexingMode();
    JSType type = cell->type();
    TypeInfo::InlineTypeFlags inlineTypeFlags = cell->inlineTypeFlags();
    CellState cellState = cell->cellState();
    size_t cellSize = cell->cellSize();
    size_t slotCount = cellSize / sizeof(EncodedJSValue);

    EncodedJSValue* slots = bitwise_cast<EncodedJSValue*>(cell);
    unsigned indentation = 0;

    auto indent = [&] {
        for (unsigned i = 0 ; i < indentation; ++i)
            out.print("  ");
    };

#define INDENT indent(),
    
    auto dumpSlot = [&] (EncodedJSValue* slots, unsigned index, const char* label = nullptr) {
        out.print("[", index, "] ", format("%p : 0x%016" PRIx64, &slots[index], slots[index]));
        if (label)
            out.print(" ", label);
        out.print("\n");
    };

    out.printf("<%p, %s>\n", cell, cell->className().characters());
    IndentationScope scope(indentation);

    INDENT dumpSlot(slots, 0, "header");
    {
        IndentationScope scope(indentation);
        INDENT out.println("structureID ", format("%d 0x%" PRIx32, structureID, structureID), " structure ", RawPointer(structure));
        INDENT out.println("indexingTypeAndMisc ", format("%d 0x%" PRIx8, indexingTypeAndMisc, indexingTypeAndMisc), " ", IndexingTypeDump(indexingMode));
        INDENT out.println("type ", format("%d 0x%" PRIx8, type, type));
        INDENT out.println("flags ", format("%d 0x%" PRIx8, inlineTypeFlags, inlineTypeFlags));
        INDENT out.println("cellState ", format("%d", cellState));
    }

    unsigned slotIndex = 1;
    if (cell->isObject()) {
        JSObject* obj = static_cast<JSObject*>(const_cast<JSCell*>(cell));
        Butterfly* butterfly = obj->butterfly();
        size_t butterflySize = obj->butterflyTotalSize();

        INDENT dumpSlot(slots, slotIndex, "butterfly");
        slotIndex++;

        if (butterfly) {
            IndentationScope scope(indentation);

            bool hasIndexingHeader = structure->hasIndexingHeader(cell);
            bool hasAnyArrayStorage = JSC::hasAnyArrayStorage(indexingType);

            size_t preCapacity = obj->butterflyPreCapacity();
            size_t propertyCapacity = structure->outOfLineCapacity();

            void* base = hasIndexingHeader
                ? butterfly->base(preCapacity, propertyCapacity)
                : butterfly->base(structure);

            unsigned publicLength = butterfly->publicLength();
            unsigned vectorLength = butterfly->vectorLength();
            size_t butterflyCellSize = MarkedSpace::optimalSizeFor(butterflySize);

            size_t endOfIndexedPropertiesIndex = butterflySize / sizeof(EncodedJSValue);
            size_t endOfButterflyIndex = butterflyCellSize / sizeof(EncodedJSValue);

            INDENT out.println("base ", RawPointer(base));
            INDENT out.println("hasIndexingHeader ", (hasIndexingHeader ? "YES" : "NO"), " hasAnyArrayStorage ", (hasAnyArrayStorage ? "YES" : "NO"));
            if (hasIndexingHeader) {
                INDENT out.print("publicLength ", publicLength, " vectorLength ", vectorLength);
                if (hasAnyArrayStorage)
                    out.print(" indexBias ", butterfly->arrayStorage()->m_indexBias);
                out.print("\n");
            }
            INDENT out.println("preCapacity ", preCapacity, " propertyCapacity ", propertyCapacity);

            unsigned index = 0;
            EncodedJSValue* slots = reinterpret_cast<EncodedJSValue*>(base);

            auto asVoidPtr = [] (void* p) {
                return p;
            };

            auto dumpSectionHeader = [&] (const char* name) {
                out.println("<--- ", name);
            };

            auto dumpSection = [&] (unsigned startIndex, unsigned endIndex, const char* name) -> unsigned {
                for (unsigned index = startIndex; index < endIndex; ++index) {
                    if (name && index == startIndex)
                        INDENT dumpSectionHeader(name);
                    INDENT dumpSlot(slots, index);
                }
                return endIndex;
            };

            {
                IndentationScope scope(indentation);

                index = dumpSection(index, preCapacity, "preCapacity");
                index = dumpSection(index, preCapacity + propertyCapacity, "propertyCapacity");

                if (hasIndexingHeader)
                    index = dumpSection(index, index + 1, "indexingHeader");

                INDENT dumpSectionHeader("butterfly");
                if (hasAnyArrayStorage) {
                    RELEASE_ASSERT(asVoidPtr(butterfly->arrayStorage()) == asVoidPtr(&slots[index]));
                    RELEASE_ASSERT(ArrayStorage::vectorOffset() == 2 * sizeof(EncodedJSValue));
                    index = dumpSection(index, index + 2, "arrayStorage");
                }

                index = dumpSection(index, endOfIndexedPropertiesIndex, "indexedProperties");
                index = dumpSection(index, endOfButterflyIndex, "unallocated capacity");
            }
        }
    }

    for (; slotIndex < slotCount; ++slotIndex)
        INDENT dumpSlot(slots, slotIndex);

#undef INDENT
}

void VMInspector::dumpSubspaceHashes(VM* vm)
{
    unsigned count = 0;
    vm->heap.objectSpace().forEachSubspace([&] (const Subspace& subspace) -> IterationStatus {
        const char* name = subspace.name();
        unsigned hash = SuperFastHash::computeHash(name);
        void* hashAsPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(hash));
        dataLogLn("    [", count++, "] ", name, " Hash:", RawPointer(hashAsPtr));
        return IterationStatus::Continue;
    });
    dataLogLn();
}

} // namespace JSC
