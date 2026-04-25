/*
 * Copyright (C) 2008-2026 Apple Inc. All rights reserved.
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

#pragma once

#include "CacheableIdentifier.h"
#include "CodeBlock.h"
#include "CodeOrigin.h"
#include "InlineCacheCompiler.h"
#include "Instruction.h"
#include "JITStubRoutine.h"
#include "MacroAssembler.h"
#include "Options.h"
#include "PropertyInlineCacheClearingWatchpoint.h"
#include "PropertyInlineCacheSummary.h"
#include "RegisterSet.h"
#include "Structure.h"
#include "StructureSet.h"
#include <wtf/Box.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>


#if ENABLE(JIT)

namespace JSC {

namespace DFG {
struct UnlinkedPropertyInlineCache;
}

class AccessCase;
class AccessGenerationResult;
class PolymorphicAccess;

#define JSC_FOR_EACH_PROPERTY_INLINE_CACHE_ACCESS_TYPE(macro) \
    macro(GetById) \
    macro(GetByIdWithThis) \
    macro(GetByIdDirect) \
    macro(TryGetById) \
    macro(GetByVal) \
    macro(GetByValWithThis) \
    macro(PutByIdStrict) \
    macro(PutByIdSloppy) \
    macro(PutByIdDirectStrict) \
    macro(PutByIdDirectSloppy) \
    macro(PutByValStrict) \
    macro(PutByValSloppy) \
    macro(PutByValDirectStrict) \
    macro(PutByValDirectSloppy) \
    macro(DefinePrivateNameByVal) \
    macro(DefinePrivateNameById) \
    macro(SetPrivateNameByVal) \
    macro(SetPrivateNameById) \
    macro(InById) \
    macro(InByVal) \
    macro(HasPrivateName) \
    macro(HasPrivateBrand) \
    macro(InstanceOf) \
    macro(DeleteByIdStrict) \
    macro(DeleteByIdSloppy) \
    macro(DeleteByValStrict) \
    macro(DeleteByValSloppy) \
    macro(GetPrivateName) \
    macro(GetPrivateNameById) \
    macro(CheckPrivateBrand) \
    macro(SetPrivateBrand) \


enum class AccessType : int8_t {
#define JSC_DEFINE_ACCESS_TYPE(name) name,
    JSC_FOR_EACH_PROPERTY_INLINE_CACHE_ACCESS_TYPE(JSC_DEFINE_ACCESS_TYPE)
#undef JSC_DEFINE_ACCESS_TYPE
};

#define JSC_INCREMENT_ACCESS_TYPE(name) + 1
static constexpr unsigned numberOfAccessTypes = 0 JSC_FOR_EACH_PROPERTY_INLINE_CACHE_ACCESS_TYPE(JSC_INCREMENT_ACCESS_TYPE);
#undef JSC_INCREMENT_ACCESS_TYPE

// This file defines two distinct inline cache (IC) dispatch strategies used across
// JSC's JIT tiers. Both strategies work the same way conceptually: each IC site is
// guarded by one or more conditions, either runtime checks (e.g. a structureID
// comparison, a property UID check) or Watchpoints on values assumed to be stable.
// When a guard passes, the IC performs the cached access; when it fails, we fall
// through to the next case or the slow path, which may add new cases.
//
// The two strategies differ in how they store and dispatch through those cases:
//
//   HandlerIC: used by Baseline JIT and DFG. The call site never modifies machine
//   code; instead it loads a pointer to the head of a singly-linked list of
//   InlineCacheHandler nodes and dispatches through them at runtime.
//
//   RepatchingIC: used by FTL only. The call site owns a fixed-size
//   slab of inline machine code embedded in the compiled function body. That slab
//   is rewritten at runtime as new cases are learned.
//
// The choice between them is a throughput vs. cost tradeoff. RepatchingIC can generate
// bespoke, case-specific code and, once sufficiently polymorphic, dispatch via a binary switch,
// which is meaningfully faster than walking a chain of indirect branches.
// However, rewriting machine code at runtime is expensive. So we only pay that
// cost in the FTL, where we have substantially more profiling and thus think the code
// is most likely to be in a steady state.

enum class PropertyInlineCacheType : uint8_t { Handler, Repatching };

struct UnlinkedPropertyInlineCache;
struct BaselineUnlinkedPropertyInlineCache;

class HandlerPropertyInlineCache;
class RepatchingPropertyInlineCache;

class PropertyInlineCache {
    WTF_MAKE_NONCOPYABLE(PropertyInlineCache);
    WTF_MAKE_TZONE_ALLOCATED(PropertyInlineCache);
public:

    ~PropertyInlineCache();

    void initGetByIdSelf(const ConcurrentJSLockerBase&, CodeBlock*, Structure* inlineAccessBaseStructure, PropertyOffset);
    void NODELETE initArrayLength(const ConcurrentJSLockerBase&);
    void NODELETE initStringLength(const ConcurrentJSLockerBase&);
    void initPutByIdReplace(const ConcurrentJSLockerBase&, CodeBlock*, Structure* inlineAccessBaseStructure, PropertyOffset);
    void initInByIdSelf(const ConcurrentJSLockerBase&, CodeBlock*, Structure* inlineAccessBaseStructure, PropertyOffset);

    AccessGenerationResult addAccessCase(const GCSafeConcurrentJSLocker&, JSGlobalObject*, CodeBlock*, ECMAMode, CacheableIdentifier, RefPtr<AccessCase>);

    void reset(const ConcurrentJSLockerBase&, CodeBlock*);

    void deref();
    void aboutToDie();

    void NODELETE initializePredefinedRegisters();

    DECLARE_VISIT_AGGREGATE;

    // Check if the stub has weak references that are dead. If it does, then it resets itself,
    // either entirely or just enough to ensure that those dead pointers don't get used anymore.
    void visitWeak(const ConcurrentJSLockerBase&, CodeBlock*);

    // This returns true if it has marked everything that it will ever mark.
    template<typename Visitor> void propagateTransitions(Visitor&);

    PropertyInlineCacheSummary summary(const ConcurrentJSLocker&, VM&) const;

    static PropertyInlineCacheSummary summary(const ConcurrentJSLocker&, VM&, const PropertyInlineCache*);

    CacheableIdentifier identifier() const { return m_identifier; }

    bool NODELETE containsPC(void* pc) const;

    JSValueRegs valueRegs() const
    {
        return JSValueRegs(
#if USE(JSVALUE32_64)
            m_valueTagGPR,
#endif
            m_valueGPR);
    }

    JSValueRegs propertyRegs() const
    {
        return JSValueRegs(
#if USE(JSVALUE32_64)
            propertyTagGPR(),
#endif
            propertyGPR());
    }

    JSValueRegs baseRegs() const
    {
        return JSValueRegs(
#if USE(JSVALUE32_64)
            m_baseTagGPR,
#endif
            m_baseGPR);
    }

    bool thisValueIsInExtraGPR() const { return accessType == AccessType::GetByIdWithThis || accessType == AccessType::GetByValWithThis; }

    bool isHandlerIC() const { return m_icType == PropertyInlineCacheType::Handler; }

#if ASSERT_ENABLED
    void checkConsistency();
#else
    ALWAYS_INLINE void checkConsistency() { }
#endif

    CacheType cacheType() const { return m_cacheType; }

    // Not ByVal and ById case: e.g. instanceof, by-index etc.
    ALWAYS_INLINE bool considerRepatchingCacheGeneric(VM& vm, CodeBlock* codeBlock, Structure* structure)
    {
        // We never cache non-cells.
        if (!structure) {
            sawNonCell = true;
            return false;
        }
        return considerRepatchingCacheImpl(vm, codeBlock, structure, CacheableIdentifier());
    }

    ALWAYS_INLINE bool considerRepatchingCacheBy(VM& vm, CodeBlock* codeBlock, Structure* structure, CacheableIdentifier impl)
    {
        // We never cache non-cells.
        if (!structure) {
            sawNonCell = true;
            return false;
        }
        return considerRepatchingCacheImpl(vm, codeBlock, structure, impl);
    }

    ALWAYS_INLINE bool considerRepatchingCacheMegamorphic(VM& vm)
    {
        return considerRepatchingCacheImpl(vm, nullptr, nullptr, CacheableIdentifier());
    }

    Structure* inlineAccessBaseStructure() const
    {
        return m_inlineAccessBaseStructureID.get();
    }

    CallLinkInfo* callLinkInfoAt(const ConcurrentJSLocker&, unsigned index, const AccessCase&);


    Vector<AccessCase*, 16> listedAccessCases(const AbstractLocker&) const;

private:
    AccessGenerationResult upgradeForPolyProtoIfNecessary(const GCSafeConcurrentJSLocker&, VM&, CodeBlock*, const Vector<AccessCase*, 16>&, AccessCase&);

    ALWAYS_INLINE bool considerRepatchingCacheImpl(VM& vm, CodeBlock* codeBlock, Structure* structure, CacheableIdentifier impl)
    {
        AssertNoGC assertNoGC;


        // This method is called from the Optimize variants of IC slow paths. The first part of this
        // method tries to determine if the Optimize variant should really behave like the
        // non-Optimize variant and leave the IC untouched.
        //
        // If we determine that we should do something to the IC then the next order of business is
        // to determine if this Structure would impact the IC at all. We know that it won't, if we
        // have already buffered something on its behalf. That's what the m_bufferedStructures set is
        // for.

        everConsidered = true;
        if (!countdown) {
            // Check if we have been doing repatching too frequently. If so, then we should cool off
            // for a while.
            WTF::incrementWithSaturation(repatchCount);
            if (repatchCount > Options::repatchCountForCoolDown()) {
                // We've been repatching too much, so don't do it now.
                repatchCount = 0;
                // The amount of time we require for cool-down depends on the number of times we've
                // had to cool down in the past. The relationship is exponential. The max value we
                // allow here is 2^256 - 2, since the slow paths may increment the count to indicate
                // that they'd like to temporarily skip patching just this once.
                countdown = WTF::leftShiftWithSaturation(
                    static_cast<uint8_t>(Options::initialCoolDownCount()),
                    numberOfCoolDowns,
                    static_cast<uint8_t>(std::numeric_limits<uint8_t>::max() - 1));
                WTF::incrementWithSaturation(numberOfCoolDowns);

                // We may still have had something buffered. Trigger generation now.
                bufferingCountdown = 0;
                return true;
            }

            // We don't want to return false due to buffering indefinitely.
            if (!bufferingCountdown) {
                // Note that when this returns true, it's possible that we will not even get an
                // AccessCase because this may cause Repatch.cpp to simply do an in-place
                // repatching.
                return true;
            }

            bufferingCountdown--;

            if (!structure)
                return true;

            // Now protect the IC buffering. We want to proceed only if this is a structure that
            // we don't already have a case buffered for. Note that if this returns true but the
            // bufferingCountdown is not zero then we will buffer the access case for later without
            // immediately generating code for it.
            //
            // NOTE: This will behave oddly for InstanceOf if the user varies the prototype but not
            // the base's structure. That seems unlikely for the canonical use of instanceof, where
            // the prototype is fixed.
            bool isNewlyAdded = false;
            StructureID structureID = structure->id();
            {
                Locker locker { m_bufferedStructuresLock };
                if (std::holds_alternative<std::monostate>(m_bufferedStructures)) {
                    if (m_identifier)
                        m_bufferedStructures = Vector<StructureID>();
                    else
                        m_bufferedStructures = Vector<std::tuple<StructureID, CacheableIdentifier>>();
                }
                WTF::switchOn(m_bufferedStructures,
                    [&](std::monostate) { },
                    [&](Vector<StructureID>& structures) {
                        for (auto bufferedStructureID : structures) {
                            if (bufferedStructureID == structureID)
                                return;
                        }
                        structures.append(structureID);
                        isNewlyAdded = true;
                    },
                    [&](Vector<std::tuple<StructureID, CacheableIdentifier>>& structures) {
                        ASSERT(!m_identifier);
                        for (auto& [bufferedStructureID, bufferedCacheableIdentifier] : structures) {
                            if (bufferedStructureID == structureID && bufferedCacheableIdentifier == impl)
                                return;
                        }
                        structures.append(std::tuple { structureID, impl });
                        isNewlyAdded = true;
                    });
            }
            if (isNewlyAdded)
                vm.writeBarrier(codeBlock);
            return isNewlyAdded;
        }
        countdown--;
        return false;
    }

    void setCacheType(const ConcurrentJSLockerBase&, CacheType);

    void clearBufferedStructures()
    {
        Locker locker { m_bufferedStructuresLock };
        WTF::switchOn(m_bufferedStructures,
            [&](std::monostate) { },
            [&](Vector<StructureID>& structures) {
                structures.shrink(0);
            },
            [&](Vector<std::tuple<StructureID, CacheableIdentifier>>& structures) {
                structures.shrink(0);
            });
    }

protected:
    PropertyInlineCache(PropertyInlineCacheType icType, AccessType accessType, CodeOrigin codeOrigin)
        : codeOrigin(codeOrigin)
        , accessType(accessType)
        , bufferingCountdown(Options::initialRepatchBufferingCountdown())
        , m_icType(icType)
    {
    }

    PropertyInlineCache(PropertyInlineCacheType icType)
        : PropertyInlineCache(icType, AccessType::GetById, { })
    { }

    void initializeWithUnitHandler(CodeBlock*, Ref<InlineCacheHandler>&&);
    void prependHandler(CodeBlock*, Ref<InlineCacheHandler>&&, bool isMegamorphic);
    void rewireStubAsJumpInAccess(CodeBlock*, Ref<InlineCacheHandler>&&);

public:
    static constexpr ptrdiff_t offsetOfByIdSelfOffset() { return OBJECT_OFFSETOF(PropertyInlineCache, byIdSelfOffset); }
    static constexpr ptrdiff_t offsetOfInlineAccessBaseStructureID() { return OBJECT_OFFSETOF(PropertyInlineCache, m_inlineAccessBaseStructureID); }
    static constexpr ptrdiff_t offsetOfInlineHolder() { return OBJECT_OFFSETOF(PropertyInlineCache, m_inlineHolder); }
    static constexpr ptrdiff_t offsetOfDoneLocation() { return OBJECT_OFFSETOF(PropertyInlineCache, doneLocation); }
    static constexpr ptrdiff_t offsetOfCountdown() { return OBJECT_OFFSETOF(PropertyInlineCache, countdown); }
    static constexpr ptrdiff_t offsetOfCallSiteIndex() { return OBJECT_OFFSETOF(PropertyInlineCache, callSiteIndex); }
    static constexpr ptrdiff_t offsetOfSlowPathStartLocation() { return OBJECT_OFFSETOF(PropertyInlineCache, slowPathStartLocation); }
    static constexpr ptrdiff_t offsetOfHandler() { return OBJECT_OFFSETOF(PropertyInlineCache, m_handler); }
    static constexpr ptrdiff_t offsetOfGlobalObject() { return OBJECT_OFFSETOF(PropertyInlineCache, m_globalObject); }

    InlineCacheHandler* firstHandler() const { return m_handler.get(); }

    JSGlobalObject* globalObject() const { return m_globalObject; }

    void resetStubAsJumpInAccess(CodeBlock*);

    GPRReg thisGPR() const { return m_extraGPR; }
    GPRReg prototypeGPR() const { return m_extraGPR; }
    GPRReg brandGPR() const { return m_extraGPR; }
    GPRReg propertyGPR() const
    {
        switch (accessType) {
        case AccessType::GetByValWithThis:
            return m_extra2GPR;
        default:
            return m_extraGPR;
        }
    }

#if USE(JSVALUE32_64)
    GPRReg thisTagGPR() const { return m_extraTagGPR; }
    GPRReg prototypeTagGPR() const { return m_extraTagGPR; }
    GPRReg propertyTagGPR() const
    {
        switch (accessType) {
        case AccessType::GetByValWithThis:
            return m_extra2TagGPR;
        default:
            return m_extraTagGPR;
        }
    }
#endif

    CodeOrigin codeOrigin { };
    PropertyOffset byIdSelfOffset;
    WriteBarrierStructureID m_inlineAccessBaseStructureID;
    JSCell* m_inlineHolder { nullptr };
    CacheableIdentifier m_identifier;
    CodeLocationLabel<JSInternalPtrTag> doneLocation;
    CodeLocationLabel<JITStubRoutinePtrTag> slowPathStartLocation;

    JSGlobalObject* m_globalObject { nullptr };
private:
    // Handler chain: used by both modes. Handler IC uses this as the main dispatch chain
    // (accessed from JIT via offsetOfHandler()). Repatching IC uses it in
    // rewireStubAsJumpInAccess() and initializeWithUnitHandler().
    RefPtr<InlineCacheHandler> m_handler;
    // Represents those structures that already have buffered AccessCases in the PolymorphicAccess.
    // Note that it's always safe to clear this. If we clear it prematurely, then if we see the same
    // structure again during this buffering countdown, we will create an AccessCase object for it.
    // That's not so bad - we'll get rid of the redundant ones once we regenerate.
    Variant<std::monostate, Vector<StructureID>, Vector<std::tuple<StructureID, CacheableIdentifier>>> m_bufferedStructures WTF_GUARDED_BY_LOCK(m_bufferedStructuresLock);
public:

    ScalarRegisterSet usedRegisters;

    CallSiteIndex callSiteIndex;

    // FIXME: These should only be needed by the repatching ICs but it's slightly non-trivial to move them there as different AccessTypes use different pinned registers.
    GPRReg m_baseGPR { InvalidGPRReg };
    GPRReg m_valueGPR { InvalidGPRReg };
    GPRReg m_extraGPR { InvalidGPRReg };
    GPRReg m_extra2GPR { InvalidGPRReg };
    GPRReg m_propertyCacheGPR { InvalidGPRReg };
    GPRReg m_arrayProfileGPR { InvalidGPRReg };
#if USE(JSVALUE32_64)
    GPRReg m_valueTagGPR { InvalidGPRReg };
    // FIXME: [32-bits] Check if PropertyInlineCache::m_baseTagGPR is used somewhere.
    // https://bugs.webkit.org/show_bug.cgi?id=204726
    GPRReg m_baseTagGPR { InvalidGPRReg };
    GPRReg m_extraTagGPR { InvalidGPRReg };
    GPRReg m_extra2TagGPR { InvalidGPRReg };
#endif

    AccessType accessType { AccessType::GetById };
protected:
    CacheType m_cacheType { CacheType::Unset };
public:
    CacheType preconfiguredCacheType { CacheType::Unset };
    // We repatch only when this is zero. If not zero, we decrement.
    // Setting 1 for a totally clear stub, we'll patch it after the first execution.
    uint8_t countdown { 1 };
    uint8_t repatchCount { 0 };
    uint8_t numberOfCoolDowns { 0 };
    uint8_t bufferingCountdown;
private:
    Lock m_bufferedStructuresLock;
public:
    bool resetByGC : 1 { false };
    bool tookSlowPath : 1 { false };
    bool everConsidered : 1 { false };
    bool prototypeIsKnownObject : 1 { false }; // Only relevant for InstanceOf.
    bool sawNonCell : 1 { false };
    bool propertyIsString : 1 { false };
    bool propertyIsInt32 : 1 { false };
    bool propertyIsSymbol : 1 { false };
    bool canBeMegamorphic : 1 { false };
    const PropertyInlineCacheType m_icType : 1;
};

// HandlerPropertyInlineCache
// ==========================
// Implements handler-list dispatch. The call site never modifies machine code;
// instead, as new cases are learned, handler nodes are prepended to a linked list
// that the call site walks at runtime.
//
// Call site layout (Baseline / DFG JIT):
//
//     load  handlerGPR, [PropertyInlineCache + offsetOfHandler]   // head of list
//     call  [handlerGPR + InlineCacheHandler::offsetOfJumpTarget] // enter first handler
//
// Handler chain layout in memory:
//
//   PropertyInlineCache
//   +------------------+
//   | m_handler        |---> InlineCacheHandler #N  (most recently added, checked first)
//   +------------------+     +-------------------+
//                            | structureID        |
//                            | offset / uid       |
//                            | m_next             |---> InlineCacheHandler #N-1
//                            +-------------------+     +-------------------+
//                                                      | ...               |
//                                                      | m_next            |---> (slow-path handler)
//                                                      +-------------------+
//
// Most handler stubs follow a uniform pattern compiled by InlineCacheCompiler::compileHandler():
//
//     emitDataICPrologue()       // x86_64 pushes FP; ARM64E tags return address;
//                                //   other ISAs do nothing. callFrameRegister is NOT updated.
//     check guard                // e.g. do a structure check: load from base, compare against
//                                //   [handlerGPR + offsetOfStructureID]
//     --- on match ---
//     perform access             // load / store, depending on AccessCase kind
//     emitDataICEpilogue()       // minimal inverse of prologue
//     return
//
//     --- on match (JS/C++ call needed, e.g. getter/setter/custom accessor) ---
//     emitDataICPrepareForCall() // lazily save LR/FP now that we know we need a full frame
//     call into JS/C++
//     emitDataICRestoreAfterCall() // restore LR/FP
//     emitDataICEpilogue()
//     return
//
//     --- on miss ---
//     load  handlerGPR, [handlerGPR + offsetOfNext]
//     jump  [handlerGPR + offsetOfJumpTarget]            // offset skips prologue
//
// Not modifying callFrameRegister is important because it means handler stubs execute in the caller's frame
// context, so exception unwinding and CallFrame* access via callFrameRegister need no special
// handling in the handler prologue/epilogue. The lazy save/restore pattern also means that
// simple load/store handlers (the common case) pay no frame-setup cost at all.
//
// The terminal handler is always the slow-path. It calls m_slowOperation
// to fall back to the C++ runtime. The slow path may generate and prepend a new
// InlineCacheHandler to the front of the list (LIFO ordering).
//
// Cached-field fast path (m_inlinedHandler):
//
// For simple access patterns (GetByIdSelf, PutByIdReplace, InByIdSelf, etc.) that match
// preconfiguredCacheType, we also store the handler in m_inlinedHandler and write the
// structure, offset, and holder into the IC's own fields (m_inlineAccessBaseStructureID,
// byIdSelfOffset, m_inlineHolder). The JIT emits a structure check inline at the call
// site (before loading m_handler) that can succeed without touching the chain at all.
// These fields are read from memory at runtime, so no constants are embedded in the
// generated assembly.
//
// Watchpoint invalidation:
//
// Some access cases (e.g. loading a property from a prototype assumed not to change)
// attach a PropertyInlineCacheClearingWatchpoint to the relevant WatchpointSet. When
// the watchpoint fires, PropertyInlineCacheClearingWatchpoint::fireInternal() eventually calls
// resetStubAsJumpInAccess(). That function walks the entire m_handler chain calling
// removeOwner() on each node, then assigns m_handler to a freshly generated slow-path
// handler, dropping the old chain's RefPtr and potentially running every
// InlineCacheHandler destructor in the chain.
//
// N.B. A watchpoint can fire while a handler stub is mid-execution — for example, a
// getter or custom accessor calls JS, that JS mutates a prototype, and the watchpoint
// fires before the getter returns. We rely on two distinct practices to avoid
// use-after-free:
//
//   1. InlineCacheHandler struct: if the chain is reset while a stub is on the
//      stack and no other Ref holds the node, the InlineCacheHandler wrapper and its
//      trailing DataOnlyCallLinkInfo array are freed immediately. This is safe because
//      handler stubs access InlineCacheHandler fields only before making calls: the
//      structure check and the m_next load both occur on the miss path, ahead of any
//      JS call. After the call returns, the result is in a register and the stub
//      performs the epilogue and returns without touching the handler struct again.
//
//   2. Machine code: each InlineCacheHandler holds a
//      Ref<GCAwareJITStubRoutine>. GCAwareJITStubRoutine does not free the routine
//      immediately when its refcount reaches zero; it sets m_isJettisoned = true and
//      defers actual deletion until the GC confirms that the routine is no longer on
//      any call stack. So the code being executed remains valid even if m_handler is
//      cleared under us.
//
// Shared handler thunks:
//
// Many common handler shapes (e.g. getByIdLoadOwnPropertyHandler, putByIdReplaceHandler)
// are pre-compiled as shared thunks stored in VM::m_sharedJITStubs. compileHandler()
// reuses an existing shared stub before generating a new one, so multiple IC sites with
// the same access pattern share the same machine code. This sharing works because
// everything is data only, unlike with repatching ICs.
class HandlerPropertyInlineCache final : public PropertyInlineCache {
    WTF_MAKE_NONCOPYABLE(HandlerPropertyInlineCache);
public:
    HandlerPropertyInlineCache()
        : PropertyInlineCache(PropertyInlineCacheType::Handler)
    { }

    HandlerPropertyInlineCache(AccessType accessType, CodeOrigin codeOrigin)
        : PropertyInlineCache(PropertyInlineCacheType::Handler, accessType, codeOrigin)
    { }

    void initializeFromUnlinkedPropertyInlineCache(VM&, CodeBlock*, const BaselineUnlinkedPropertyInlineCache&);
    void initializeFromDFGUnlinkedPropertyInlineCache(CodeBlock*, const DFG::UnlinkedPropertyInlineCache&);

    void setInlinedHandler(CodeBlock*, Ref<InlineCacheHandler>&&);
    void clearInlinedHandler(CodeBlock*);

    static constexpr ptrdiff_t offsetOfSlowOperation() { return OBJECT_OFFSETOF(HandlerPropertyInlineCache, m_slowOperation); }

    CodePtr<OperationPtrTag> m_slowOperation;
    RefPtr<InlineCacheHandler> m_inlinedHandler;
};

// RepatchingPropertyInlineCache
// =============================
// Implements slab-patching dispatch (used by FTL only). The call site owns a
// fixed-size region of inline machine code embedded in the compiled function body.
// When new cases are learned, we rewrite either that slab in place, or a
// separately-allocated stub it jumps to.
//
// Inline code region layout inside the JIT-compiled function body:
//
//   startLocation --> +--------------------------------------+
//                     |  inline IC code (inlineCodeSize      |
//                     |  bytes; initially: call to slow path)|
//                     +--------------------------------------+
//   doneLocation  --> (next instruction in the function)
//
// The IC evolves through the following states:
//
//   1. [slow path]: initial state; the slab contains a call to operationXyzOptimize.
//
//   2. [inline access]: after the first hit. InlineAccess patches the slab in-place
//      with a monomorphic structure check and access (e.g. a direct load at a known
//      offset). No separate stub is needed yet.
//
//   3. [jump -> stub]: when a second structure is seen, rewireStubAsJumpInAccess()
//      overwrites the slab with a direct jump to a PolymorphicAccess stub. The stub
//      holds all accumulated AccessCases compiled together by InlineCacheCompiler::compile().
//
// The stub uses one of two dispatch strategies, chosen when the stub is (re)generated:
//
//   Cascade (linear, newest-first):
//       case N:   check guard --match--> perform access, jump to doneLocation
//                              --miss --> fall through
//       case N-1: check guard --match--> perform access, jump to doneLocation
//                              --miss --> fall through
//       ...
//       slow path
//
//   BinarySwitch (O(log n)):  used when every case is guarded solely by a structure
//       check (no proxies, no non-structure guards). A balanced binary tree of
//       structureID comparisons is emitted; each leaf performs its access and jumps
//       to doneLocation. We cannot use this form if any case involves a proxy, since
//       proxies require additional checks beyond the structureID.
//
// Because compiling a new stub is expensive, new cases are buffered in the
// PolymorphicAccess case list and the stub is only regenerated when bufferingCountdown
// reaches zero (reset to Options::repatchBufferingCountdown() after each regeneration).
// This batches multiple new cases into a single regeneration pass.
//
// Why only in FTL:
//
// Rewriting machine code at runtime incurs instruction cache flushes and, on some
// platforms, requires toggling memory write permissions. The generated stub code is
// also bespoke (built for exactly the set of cases we have seen), so every new case
// requires a full recompile of the entire stub. The payoff is tight dispatch: no
// indirect branches through a handler chain, and potentially O(log n) dispatch via
// binary switch. This tradeoff is only worthwhile in the FTL, where a function is
// hot enough to amortize the patching cost over many executions.
//
// Watchpoint invalidation:
//
// Cases that depend on stable conditions (prototype-chain stability, equivalence of a
// property value, etc.) register watchpoints on the relevant WatchpointSets. When a
// watchpoint fires, PropertyInlineCacheClearingWatchpoint::fireInternal() eventually calls
// resetStubAsJumpInAccess(). For RepatchingIC that function overwrites the inline slab
// with a jump back to the slow path and drops the reference to the PolymorphicAccess
// stub; the IC then begins accumulating cases from scratch.
//
// If a stub is mid-execution when the watchpoint fires (e.g. a polymorphic accessor
// case calls JS), the machine code is protected by GCAwareJITStubRoutine (as described above),
// this guarantees the code of the stub is still valid. No inline slabs can be at the
// first instruction when this rewrite happens either.
class RepatchingPropertyInlineCache final : public PropertyInlineCache {
    WTF_MAKE_NONCOPYABLE(RepatchingPropertyInlineCache);
public:
    RepatchingPropertyInlineCache()
        : PropertyInlineCache(PropertyInlineCacheType::Repatching)
    { }

    RepatchingPropertyInlineCache(AccessType accessType, CodeOrigin codeOrigin)
        : PropertyInlineCache(PropertyInlineCacheType::Repatching, accessType, codeOrigin)
    { }

    // This is either the start of the inline IC for *byId caches, or the location of patchable jump for 'instanceof' caches.
    CodeLocationLabel<JITStubRoutinePtrTag> startLocation;
    CodeLocationCall<JSInternalPtrTag> m_slowPathCallLocation;
    std::unique_ptr<PolymorphicAccess> m_stub;

    uint32_t inlineCodeSize() const
    {
        int32_t inlineSize = MacroAssembler::differenceBetweenCodePtr(startLocation, doneLocation);
        ASSERT(inlineSize >= 0);
        return inlineSize;
    }
};

inline auto appropriateGetByIdOptimizeFunction(AccessType type) -> decltype(&operationGetByIdOptimize)
{
    switch (type) {
    case AccessType::GetById:
        return operationGetByIdOptimize;
    case AccessType::TryGetById:
        return operationTryGetByIdOptimize;
    case AccessType::GetByIdDirect:
        return operationGetByIdDirectOptimize;
    case AccessType::GetPrivateNameById:
        return operationGetPrivateNameByIdOptimize;
    case AccessType::GetByIdWithThis:
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

inline auto appropriateGetByIdGenericFunction(AccessType type) -> decltype(&operationGetByIdGeneric)
{
    switch (type) {
    case AccessType::GetById:
        return operationGetByIdGeneric;
    case AccessType::TryGetById:
        return operationTryGetByIdGeneric;
    case AccessType::GetByIdDirect:
        return operationGetByIdDirectGeneric;
    case AccessType::GetPrivateNameById:
        return operationGetPrivateNameByIdGeneric;
    case AccessType::GetByIdWithThis:
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

inline auto appropriatePutByIdOptimizeFunction(AccessType type) -> decltype(&operationPutByIdStrictOptimize)
{
    switch (type) {
    case AccessType::PutByIdStrict:
        return operationPutByIdStrictOptimize;
    case AccessType::PutByIdSloppy:
        return operationPutByIdSloppyOptimize;
    case AccessType::PutByIdDirectStrict:
        return operationPutByIdDirectStrictOptimize;
    case AccessType::PutByIdDirectSloppy:
        return operationPutByIdDirectSloppyOptimize;
    case AccessType::DefinePrivateNameById:
        return operationPutByIdDefinePrivateFieldStrictOptimize;
    case AccessType::SetPrivateNameById:
        return operationPutByIdSetPrivateFieldStrictOptimize;
    default:
        break;
    }
    // Make win port compiler happy
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

inline bool hasConstantIdentifier(AccessType accessType)
{
    switch (accessType) {
    case AccessType::DeleteByValStrict:
    case AccessType::DeleteByValSloppy:
    case AccessType::GetByVal:
    case AccessType::GetPrivateName:
    case AccessType::InstanceOf:
    case AccessType::InByVal:
    case AccessType::HasPrivateName:
    case AccessType::HasPrivateBrand:
    case AccessType::GetByValWithThis:
    case AccessType::PutByValStrict:
    case AccessType::PutByValSloppy:
    case AccessType::PutByValDirectStrict:
    case AccessType::PutByValDirectSloppy:
    case AccessType::DefinePrivateNameByVal:
    case AccessType::SetPrivateNameByVal:
    case AccessType::SetPrivateBrand:
    case AccessType::CheckPrivateBrand:
        return false;
    case AccessType::DeleteByIdStrict:
    case AccessType::DeleteByIdSloppy:
    case AccessType::InById:
    case AccessType::TryGetById:
    case AccessType::GetByIdDirect:
    case AccessType::GetById:
    case AccessType::GetPrivateNameById:
    case AccessType::GetByIdWithThis:
    case AccessType::PutByIdStrict:
    case AccessType::PutByIdSloppy:
    case AccessType::PutByIdDirectStrict:
    case AccessType::PutByIdDirectSloppy:
    case AccessType::DefinePrivateNameById:
    case AccessType::SetPrivateNameById:
        return true;
    }
    return false;
}

struct UnlinkedPropertyInlineCache {
    AccessType accessType;
    CacheType preconfiguredCacheType { CacheType::Unset };
    bool propertyIsInt32 : 1 { false };
    bool propertyIsString : 1 { false };
    bool propertyIsSymbol : 1 { false };
    bool prototypeIsKnownObject : 1 { false };
    bool canBeMegamorphic : 1 { false };
    CacheableIdentifier m_identifier; // This only comes from already marked one. Thus, we do not mark it via GC.
    CodeLocationLabel<JSInternalPtrTag> doneLocation;
    CodeLocationLabel<JITStubRoutinePtrTag> slowPathStartLocation;
};

struct BaselineUnlinkedPropertyInlineCache : JSC::UnlinkedPropertyInlineCache {
    BytecodeIndex bytecodeIndex;
};

} // namespace JSC

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::HandlerPropertyInlineCache)
    static bool isType(const JSC::PropertyInlineCache& cache)
    {
        return cache.isHandlerIC();
    }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::RepatchingPropertyInlineCache)
    static bool isType(const JSC::PropertyInlineCache& cache)
    {
        return !cache.isHandlerIC();
    }
SPECIALIZE_TYPE_TRAITS_END()

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::AccessType> : public IntHash<JSC::AccessType> { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::AccessType> : public StrongEnumHashTraits<JSC::AccessType> { };

} // namespace WTF

#endif // ENABLE(JIT)
