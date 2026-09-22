/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/ClassInfo.h>
#include <JavaScriptCore/Concurrency.h>
#include <JavaScriptCore/ConcurrentJSLock.h>
#include <JavaScriptCore/IndexingType.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSCast.h>
#include <JavaScriptCore/JSTypeInfo.h>
#include <JavaScriptCore/PropertyName.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/PropertyOffset.h>
#include <JavaScriptCore/PutPropertySlot.h>
#include <JavaScriptCore/StructureRareData.h>
#include <JavaScriptCore/StructureTransitionTable.h>
#include <JavaScriptCore/TypeInfoBlob.h>
#include <JavaScriptCore/Watchpoint.h>
#include <wtf/Atomics.h>
#include <wtf/CompactPointerTuple.h>
#include <wtf/CompactPtr.h>
#include <wtf/CompactRefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WTF {

class UniquedStringImpl;

} // namespace WTF

namespace JSC {

class DeferGC;
class DeferredStructureTransitionWatchpointFire;
class LLIntOffsetsExtractor;
class PropertyNameArrayBuilder;
class PropertyNameArray;
class PropertyTable;
class StructureChain;
class StructureShape;
class JSString;
struct DumpContext;
struct HashTable;
struct HashTableValue;

namespace Integrity {
class Analyzer;
}

class DeferredStructureTransitionWatchpointFire final : public DeferredWatchpointFire {
    WTF_MAKE_NONCOPYABLE(DeferredStructureTransitionWatchpointFire);
public:
    DeferredStructureTransitionWatchpointFire(VM& vm, Structure* structure)
        : DeferredWatchpointFire()
        , m_vm(vm)
        , m_structure(structure)
    {
    }

    ~DeferredStructureTransitionWatchpointFire()
    {
        if (watchpointsToFire().state() == IsWatched)
            fireAllSlow();
    }

    const Structure* structure() const { return m_structure; }


private:
    JS_EXPORT_PRIVATE void fireAllSlow();

    VM& m_vm;
    const Structure* m_structure;
};

// The out-of-line property storage capacity to use when first allocating out-of-line
// storage. Note that all objects start out without having any out-of-line storage;
// this comes into play only on the first property store that exhausts inline storage.
static constexpr unsigned initialOutOfLineCapacity = 4;

// The factor by which to grow out-of-line storage when it is exhausted, after the
// initial allocation.
static constexpr unsigned outOfLineGrowthFactor = 2;

class PropertyTableEntry;
class CompactPropertyTableEntry {
public:
    CompactPropertyTableEntry()
        : m_data(nullptr, 0)
    {
    }

    CompactPropertyTableEntry(UniquedStringImpl* key, PropertyOffset offset, unsigned attributes)
        : m_data(key, ((offset << 8) | attributes))
    {
        ASSERT(this->attributes() == attributes);
        ASSERT(this->offset() == offset);
    }

    CompactPropertyTableEntry(const PropertyTableEntry&);

    UniquedStringImpl* key() const { return m_data.pointer(); }
    void setKey(UniquedStringImpl* key) { m_data.setPointer(key); }
    PropertyOffset offset() const { return m_data.type() >> 8; }
    void setOffset(PropertyOffset offset)
    {
        m_data.setType((m_data.type() & 0x00ffU) | (offset << 8));
        ASSERT(this->offset() == offset);
    }
    uint8_t attributes() const { return m_data.type(); }
    void setAttributes(uint8_t attributes)
    {
        m_data.setType((m_data.type() & 0xff00U) | attributes);
        ASSERT(this->attributes() == attributes);
    }

private:
    CompactPointerTuple<UniquedStringImpl*, uint16_t> m_data;
};

class PropertyTableEntry {
public:
    PropertyTableEntry() = default;

    PropertyTableEntry(UniquedStringImpl* key, PropertyOffset offset, unsigned attributes)
        : m_key(key)
        , m_offset(offset)
        , m_attributes(attributes)
    {
        ASSERT(this->attributes() == attributes);
    }

    PropertyTableEntry(const CompactPropertyTableEntry& entry)
        : m_key(entry.key())
        , m_offset(entry.offset())
        , m_attributes(entry.attributes())
    {
    }

    UniquedStringImpl* key() const { return m_key; }
    void setKey(UniquedStringImpl* key) { m_key = key; }
    PropertyOffset offset() const { return m_offset; }
    void setOffset(PropertyOffset offset) { m_offset = offset; }
    uint8_t attributes() const { return m_attributes; }
    void setAttributes(uint8_t attributes) { m_attributes = attributes; }

private:
    UniquedStringImpl* m_key { nullptr };
    PropertyOffset m_offset { 0 };
    uint8_t m_attributes { 0 };
};


inline CompactPropertyTableEntry::CompactPropertyTableEntry(const PropertyTableEntry& entry)
    : m_data(entry.key(), ((entry.offset() << 8) | entry.attributes()))
{
}

// One field-type record: the field at `offset` of an object whose structure is `owner` holds a cell whose
// structure is `expected`. JSC's equivalent of a V8 DescriptorArray field type, reduced to None / Class / Any,
// where a null `expected` is Any. Refcounted because the table is appended to on the mutator thread at every
// property creation while compiler threads read it. FieldTypeClaimIndex encodes fieldTypeClaimIndex() below.


namespace FieldTypeClaimIndex {
static constexpr uint16_t noEntryYet = 0;
static constexpr uint16_t entryWithoutClaim = 1;
static constexpr uint16_t offsetWasReused = 2;
static constexpr uint16_t firstClaim = 3;
}

// The property name at `offset` of `owner`, by walking its property table. DIAGNOSTIC ONLY -- it allocates a
// utf8() copy -- so every caller must be inside a logFieldTypes() or validateFieldTypes() guard.
JS_EXPORT_PRIVATE CString fieldTypeFieldName(Structure* owner, PropertyOffset);

class FieldTypeRecord : public ThreadSafeRefCounted<FieldTypeRecord> {
    WTF_MAKE_NONCOPYABLE(FieldTypeRecord);
public:
    static Ref<FieldTypeRecord> create(StructureID owner, PropertyOffset offset, StructureID expected)
    {
        return adoptRef(*new FieldTypeRecord(owner, offset, expected));
    }

    StructureID owner() const { return m_owner; }
    PropertyOffset offset() const { return m_offset; }

    // Safe from any thread. Monotonic: a non-null expected only ever becomes null, so a stale non-null read
    // on a compiler thread is an assumption DesiredWatchpoints revalidates at plan finalisation.
    StructureID expected() const { return m_expected.load(std::memory_order_acquire); }

    // Address of the claim, which never moves for the record's life. Generated code bakes this and loads through
    // it, so it sees the current claim rather than one frozen at compile time, as V8's store handler does.
    const void* addressOfExpected() const { return &m_expected; }

    // Three states: a live claim (expected() != 0), unclaimed (expected() == 0 && !isGeneralized(), a claim may
    // still be established, which happens when a store site created the record before any object of the shape
    // existed), and generalized (permanent, no claim may ever form again).
    bool hasClaim() const { return !!expected(); }

    // Mutator thread only. Establishes the first claim for a still-unclaimed field.
    void establish(StructureID observed)
    {
        ASSERT(observed);
        if (isGeneralized())
            return;
        StructureID empty;
        m_expected.compareExchangeStrong(empty, observed);
    }
    bool isGeneralized() const { return m_generalized.load(std::memory_order_acquire); }

    WatchpointSet& watchpoints() { return m_watchpoints.get(); }

    // Mutator thread only. Idempotent. Fires the watchpoint, jettisoning every compilation that narrowed here.
    void generalize(VM& vm)
    {
        // Mark permanent first, so an unclaimed record can never be claimed afterwards.
        m_generalized.store(true, std::memory_order_release);
        StructureID claimedWas = m_expected.loadRelaxed();
        if (!claimedWas)
            return;
        m_expected.store(StructureID(), std::memory_order_release);
        // Withdraw the shape-carried copy after the table, never before: the table is the source of truth
        // and the Structure word is a cache of it. A stale-SET word is merely conservative (the creation
        // goes to the table and finds nothing); a stale-CLEAR word lets a creation skip maintenance of a
        // live claim, which is unsound. Out of line because Structure is incomplete here.
        clearOwnerShapeClaimCache();
        if (Options::logFieldTypes()) [[unlikely]]
            reportWithdrawalWithName(true);
        if (Options::logFieldTypes()) [[unlikely]] {
            static std::atomic<unsigned> reported { 0 };
            if (reported++ < 12) {
                dataLogLn("\n" "[fieldtype] ABOUT TO FIRE owner=", m_owner.bits(), " offset=", m_offset,
                    " watchpointsEmpty=", !m_watchpoints->isBeingWatched());
                WTFReportBacktrace();
            }
        }
        if (m_watchpoints->isStillValid()) {
            // Uncapped population counter for claim-withdrawal invalidation. The "ABOUT TO FIRE" log above is
            // capped at 12 by a static counter and both benchmarks that motivated it burn all 12 during startup,
            // so it can support no steady-state claim (F6). watched=1 means this fire actually invalidates
            // dependent code, i.e. it is a jettison event rather than a fire nobody was listening to.
            if (Options::logFieldTypes()) [[unlikely]] {
                dataLogLn("[fieldtype] FIRE-CLAIM owner=", m_owner.bits(), " offset=", m_offset,
                    " watched=", m_watchpoints->isBeingWatched(),
                    " creationsSeen=", creationsSeen(), " dependents=", dependents());
                // Was OUTSIDE this guard, so a default build printed to stdout, walked the owner's property
                // table and allocated a utf8() copy of the field name on every withdrawal that had dependents:
                // 19 times on typescript-lib, 8 on babel-wtb, 3 on raytrace. Too few to cost measurable time,
                // which is why it survived -- but it is unconditional output on a shipping path.
                if (dependents()) [[unlikely]]
                    reportExpensiveWithdrawal(claimedWas);
            }
            m_watchpoints->fireAll(vm, "Field type generalized by a store");
        }
    }

    // The GC found the claimed structure dead. Clears the claim WITHOUT firing the watchpoint: every
    // compilation that adopted it registered the claimed structure as a DFG weak reference, so the collection
    // that proved it dead already jettisons that code. Mutator thread, world stopped, end of marking.
    void clearExpectedForDeadStructure()
    {
        // Permanent rather than merely unclaimed, unlike V8, which installs a re-derived field type on a NEW map
        // so objects predating it are not described by it. JSC generalises in place on a structure every object
        // of the shape shares, so re-establishing from one later observation would retroactively describe live
        // objects already holding contradicting values. Costs claim yield, which is the safe direction.
        m_generalized.store(true, std::memory_order_release);
        m_expected.store(StructureID(), std::memory_order_release);
    }

    // The GC found the OWNER dead, so pruneAfterMarking is about to drop this record's table entry. The record
    // outlives the entry (DFGPlan and DFGCommonData hold Refs), so a plan finishing later could still call
    // generalize() and reach clearOwnerShapeClaimCache(), writing through a dead StructureID possibly already
    // recycled for a live Structure. Clearing m_expected makes generalize() return first. World stopped.
    void markTerminalForDeadOwner()
    {
        m_generalized.store(true, std::memory_order_release);
        m_expected.store(StructureID(), std::memory_order_release);
    }

    // Clears the owner Structure's cached copy of this claim; pruneAfterMarking must clear it too when weak
    // clearing drops a claim. Out of line because Structure is incomplete here; exported for dynbench, which
    // links the framework and inlines generalize().
    JS_EXPORT_PRIVATE void clearOwnerShapeClaimCache();

    // Provenance for validateFieldTypes' failure report: `claimSite=creation-on-brand-new-entry creationsSeen=1
    // funnelWrites=0` says the claim came from one observed creation and nothing has written the field through
    // C++ since, which points at a cached JIT path rather than a missing store hook.
    void noteFunnelWrite(bool violating)
    {
        m_funnelWrites.exchangeAdd(1);
        if (violating)
            m_funnelViolations.exchangeAdd(1);
    }
    uint32_t funnelWrites() const { return m_funnelWrites.load(); }
    uint32_t funnelViolations() const { return m_funnelViolations.load(); }

    // An atomic read-modify-write that ran on EVERY creation reaching a record, for a counter read only by
    // the FIRE-CLAIM log and validateFieldTypes' failure report. Measured: recordAtCreation and its tail are
    // 39 of chai-wtb's 45 ns per call, over 59,599 calls; this was part of it and buys nothing in production.
    void noteCreation()
    {
        if (Options::validateFieldTypes() || Options::logFieldTypes()) [[unlikely]]
            m_creationsSeen.exchangeAdd(1);
    }
    // How many compilations registered a dependency on this claim. The discriminator the campaign has been unable
    // to find with counts: delta-blue withdraws 45 claims and jettisons NOTHING while raytrace withdraws 47 and
    // destroys six hot functions, so the difference must be whether anything had depended on the claim yet. This
    // counts exactly that, at all four registration sites.
    // EXPERIMENT 2 (verified/16 6): would V8's is_stable() have refused this claim by the time anything depended
    // on it? V8 clears is_stable when a map is transitioned FROM, so the question is not "was the claimed structure
    // a leaf at claim time" (verified/14 8 measured that snapshot and it blocks only 7% of harm) but "has it stopped
    // being one by now". JSC's analogue is transitionWatchpointSetIsStillValid(), fired by
    // didTransitionFromThisStructure. Reported at DEPENDENCY REGISTRATION, which is the moment a claim becomes
    // capable of costing anything.
    void reportStabilityAtDependency() const;

    void noteDependent()
    {
        unsigned n = m_dependents.exchangeAdd(1) + 1;
        if (Options::logFieldTypes()) [[unlikely]]
            reportStabilityAtDependency();
        // The blast-radius census: every dependency registration, with the running count. Post-processing takes the
        // max per (owner, offset), which is what a cap would act on. Needed because delta-blue's VALUABLE claims are
        // never withdrawn, so their dependent counts cannot be read off the withdrawal log.
        // creationsSeen AT THIS MOMENT is the admission question's discriminator: it is how much evidence existed
        // when a compilation committed to the claim. A claim depended upon at creationsSeen=1 was speculated from a
        // single observation, and that is the only kind a confirmation threshold could refuse.
        if (Options::logFieldTypes()) [[unlikely]]
            dataLogLn("[fieldtype] DEP owner=", m_owner.bits(), " offset=", m_offset, " n=", n,
                " creationsSeen=", creationsSeen());
    }
    // Names the field behind an EXPENSIVE withdrawal (one with dependents). The harmful population is 3-4 records
    // per benchmark, so naming them individually is now tractable where counting thousands never was.
    // Takes the claimed StructureID explicitly: generalize() zeroes m_expected BEFORE this runs, so reading the
    // member here always reported <cleared>. The claimed value's JSType is the one fact that decides whether V8's
    // Object::OptimalType gate (is_stable() && IsJSReceiverMap(), which refuses strings outright) would have
    // prevented the claim at all.
    JS_EXPORT_PRIVATE void reportExpensiveWithdrawal(StructureID claimedWas) const;
    // Names the field behind EVERY withdrawal, expensive or not. Diagnostic for the admission question: a claim
    // is keyed on (adding structure, offset), so the same PROPERTY on a second shape path gets a fresh key and
    // re-learns a lesson the first key already paid for. acorn-wtb contradicts 4,515 of 5,762 distinct keys.
    // Whether those keys share property NAMES is what decides if a name-keyed admission policy can predict them.
    JS_EXPORT_PRIVATE void reportWithdrawalWithName(bool hadClaim) const;
    uint32_t dependents() const { return m_dependents.load(); }
    uint32_t creationsSeen() const { return m_creationsSeen.load(); }

    // PAY FOR WHAT YOU USE. A narrowing is only worth its dependency if it actually FOLDS a structure check, and
    // that is measurable per claim rather than predictable at claim time -- which is why this succeeds where six
    // admission gates failed (README 0.6): it asks about the OUTCOME, not about a property of the claim.
    //
    // Measured on the five hottest narrowed functions of each test, comparing DFG CheckStructure counts across
    // compilations with equal compilation counts:
    //
    //   delta-blue  3,992 narrowings -> CheckStructure 139 -> 61  (-56%, ~20 checks folded per 1,000 narrowings)
    //   acorn-wtb   4,498 narrowings -> CheckStructure 169 -> 167 (-1%,  ~0.4 checks folded per 1,000)
    //
    // A 50x difference in benefit density, and acorn pays 36 CodeBlock jettisons for it (verified/17). So after
    // enough narrowings with nothing folded, stop narrowing on this claim. DECLINING TO NARROW IS SOUND
    // UNCONDITIONALLY -- it forgoes an optimisation and registers no dependency -- so a mis-attributed fold count
    // can only mis-tune, never break. That is the whole reason this is a fold COUNTER and not a deferred
    // registration: deferring would need proof that the narrowing has no other consumer in the AI.
    void noteNarrowing() { m_narrowings.exchangeAdd(1); }
    void noteFold() { m_folds.exchangeAdd(1); }
    uint32_t narrowings() const { return m_narrowings.load(); }
    uint32_t folds() const { return m_folds.load(); }
    bool narrowingHasProvedWorthless() const
    {
        return m_folds.load() == 0
            && m_narrowings.load() >= Options::fieldTypeFoldlessNarrowingBudget();
    }

    // How many times a store site has been refused a metadata cache because of this claim, returning the new
    // count. A refused LLInt replace site is left uncached for the rest of the run, so every store there
    // re-enters C++: measured 24511 re-entries over 700 claims on babel-wtb against 122 over 13 on delta-blue.
    // This is what lets the refusal be spent on cold fields and surrendered on hot ones.
    uint32_t noteCacheRefusal() { return m_cacheRefusals.exchangeAdd(1) + 1; }
    uint32_t cacheRefusals() const { return m_cacheRefusals.load(); }
    // A string literal naming the code path that established the live claim.
    // Provenance for the same two diagnostics, so it is gated with them rather than stored on every claim.
    void setClaimSite(const char* site)
    {
        if (Options::validateFieldTypes() || Options::logFieldTypes()) [[unlikely]]
            m_claimSite = site;
    }
    const char* claimSite() const { return m_claimSite ? m_claimSite : "<none>"; }

private:
    FieldTypeRecord(StructureID owner, PropertyOffset offset, StructureID expected)
        : m_owner(owner)
        , m_offset(offset)
        , m_watchpoints(WatchpointSet::create(IsWatched))
    {
        m_expected.storeRelaxed(expected);
    }

    const StructureID m_owner;
    const PropertyOffset m_offset;
    // StructureID's bits constructor is private, so this is Atomic<StructureID>, not an atomic word.
    Atomic<StructureID> m_expected;
    Atomic<bool> m_generalized { false };
    const Ref<WatchpointSet> m_watchpoints;
    Atomic<uint32_t> m_funnelWrites { 0 };
    Atomic<uint32_t> m_funnelViolations { 0 };
    Atomic<uint32_t> m_creationsSeen { 0 };
    Atomic<uint32_t> m_dependents { 0 };
    // See noteNarrowing(): the pay-for-what-you-use counters. Compiler threads write these, so they are relaxed
    // atomics like every other counter here; an exact count is not needed, only the zero/non-zero distinction.
    Atomic<uint32_t> m_narrowings { 0 };
    Atomic<uint32_t> m_folds { 0 };
    Atomic<uint32_t> m_cacheRefusals { 0 };
    const char* m_claimSite { nullptr };
};

// A live claim forces the inline caches away from their cheap handlers: a bare inline self-replace stub and the
// pre-compiled shared put thunks have nowhere to bake the field's expected StructureID, so a claimed field must
// be routed to a generated handler that can check. Five sites need that decision -- ById Replace and Transition,
// ByVal Replace and Transition, and the inline stub in Repatch -- and they take it here so the measurement gate
// and its counter cannot drift apart between them.
//
// Returning false when the gate is off is UNSOUND on its own: the shared thunk then stores unchecked while the
// claim stays live, so a narrowed load can read the wrong layout. Measure it combined with
// useFieldTypeNarrowing=0, where no consumer trusts a claim and the leg becomes sound.
inline bool fieldTypeClaimForcesGeneratedHandler(FieldTypeRecord* record, ASCIILiteral site)
{
    if (!record || !record->expected())
        return false;
    if (!Options::useFieldTypeICHandlerDowngrade()) [[unlikely]]
        return false;
    if (Options::logFieldTypes()) [[unlikely]]
        dataLogLn("[fieldtype] IC-DOWNGRADE site=", site, " owner=", record->owner().bits(), " offset=", record->offset());
    return true;
}

// Records, per (offset owner structure, property offset), the single structure that field holds, so that a
// CheckStructure on a value loaded out of it can be eliminated. V8 keeps this in the Map's DescriptorArray; JSC
// lost the equivalent when InferredType was removed in 2019, and unlike InferredType this is keyed per
// (owner, offset) rather than per property NAME, which conflated unrelated structures sharing a name.
//
// A side table rather than the property-table entry because CompactPropertyTableEntry is exactly 8 bytes with
// its 16-bit field fully consumed, and widening it would cost property-table memory engine-wide. No
// per-structure summary bit either: it would answer "does this structure's transition subtree have some record"
// when a store needs "does THIS offset have one", and the two diverge 97.6% of the time.
class FieldTypeWatchpointTable {
    WTF_MAKE_TZONE_ALLOCATED(FieldTypeWatchpointTable);
    WTF_MAKE_NONCOPYABLE(FieldTypeWatchpointTable);
public:
    FieldTypeWatchpointTable() = default;

    // THREE states, and the middle one is new. A claim does not need a record to be ENFORCED: enforcement reads the
    // owner shape's claim word plus the interned m_claimedStructureIDBits, never the record. The record is needed
    // only by the compiler (to bake expected() or register the WatchpointSet) and by withdrawal (to fire it). So a
    // claim no compiler has looked at can carry its StructureID inline and skip both allocations.
    //
    //   claimed == 0, record == null   permanently generalised -- no claim can ever form
    //   claimed != 0, record == null   LAZY claim: live and enforced, never yet consulted by a compiler
    //   claimed == 0, record != null   materialised -- the record owns expected()
    //
    // Measured ceiling for the laziness (fieldTypeCreationDummyWork=4, which prices exactly this): chai-wtb 2.33 of
    // its 3.21 points, mobx-startup all of 2.13, proxy-mobx all of 1.20, babylonjs-startup-es6 ~2.5 of 2.76. Free
    // for the beneficiary by construction -- delta-blue narrows 4.69 times per claim, so its records materialise
    // immediately and it pays one branch.
    struct Entry {
        StructureID owner;
        RefPtr<FieldTypeRecord> record;
        StructureID claimed;
    };

    // WHICH CELL KINDS MAY CARRY A CLAIM, as a BITMASK of claimable buckets.
    //
    // This replaced an ordered enum (0 all cells, 1 skip strings, 2 skip strings+arrays, 3 final objects only,
    // 4 never) whose levels forced independent decisions onto one axis. That mattered: the measured reason to
    // exclude anything is ARRAYS -- a structure-IDENTITY claim cannot cover an array's legitimate lifecycle, and
    // 71% of babel-wtb's contradictions are Array -> Array with identical class, indexing type and maxOffset --
    // yet the only level that excluded arrays also excluded STRINGS as collateral, because 2 > 1. Strings were
    // never implicated: "kinds=1 is not enough, it leaves sha256's narrowing count unchanged, so it is arrays
    // specifically, not strings". And the collateral was not free: isolating it (kinds=0 -> 1) costs
    // **cdjs -1.83% (p<0.0001)** while measuring n.s. on sha256, chai-wtb and delta-blue -- and cdjs -1.35% is on
    // record as the ONLY loss from landing the array exclusion. That loss was the string exclusion all along.
    //
    // A skipped kind takes the same path as a non-cell: a claimless entry, permanently generalised. That is the
    // conservative direction, and it keeps `01`'s born-false class closed because nothing can claim the field after.
    enum ClaimableType : unsigned {
        FinalObjectClaim = 1u << 0,   // the case the mechanism exists for: a known layout makes the next load a slot read
        CallableClaim    = 1u << 1,   // functions and friends: prototype-method loads, where delta-blue's Function claims live
        StringClaim      = 1u << 2,   // proves SpecString, folds a Check:String. Cheap to prove, nearly free to lose
        ArrayClaim       = 1u << 3,   // THE ONE WITH EVIDENCE AGAINST IT (see above)
        OtherCellClaim   = 1u << 4,   // RegExp, Set, Map, typed arrays, GetterSetter, ...
        AllClaimable     = FinalObjectClaim | CallableClaim | StringClaim | ArrayClaim | OtherCellClaim,
    };

    static ALWAYS_INLINE unsigned claimableBucketFor(JSType type)
    {
        switch (type) {
        case FinalObjectType:
            return FinalObjectClaim;
        case StringType:
            return StringClaim;
        case ArrayType:
        case DerivedArrayType:
            return ArrayClaim;
        case JSFunctionType:
            return CallableClaim;
        default:
            return OtherCellClaim;
        }
    }

    // The legacy ordered enum is still accepted so that every recorded measurement leg keeps working -- most of
    // this campaign's ablations pass `fieldTypeClaimableKinds=4` for NeverClaim. 5 means "not set, use the mask".
    static ALWAYS_INLINE unsigned claimableMask()
    {
        switch (Options::fieldTypeClaimableKinds()) {
        case 0: return AllClaimable;
        case 1: return AllClaimable & ~StringClaim;
        case 2: return AllClaimable & ~(StringClaim | ArrayClaim);
        case 3: return FinalObjectClaim;
        case 4: return 0;
        default: return Options::fieldTypeClaimableTypeMask() & AllClaimable;
        }
    }

    static ALWAYS_INLINE bool valueIsWorthClaiming(JSValue value)
    {
        if (!value.isCell())
            return false;
        // V8 PARITY. Object::OptimalType returns Class(map) only when `map->is_stable() && IsJSReceiverMap(*map)`,
        // so V8 never claims a string, symbol or bigint. JSC::JSCell::isObject() is `type() >= ObjectType`, the
        // same predicate. This matters beyond parity: string REPRESENTATION is not stable in JSC -- atomic, rope
        // and 8/16-bit strings are distinct Structures -- so a string claim is contradicted by the next store of a
        // differently represented string, which is a claim that can never hold.
        if (Options::useFieldTypeJSReceiverOnlyClaims() && !value.asCell()->isObject()) [[unlikely]]
            return false;
        return claimableMask() & claimableBucketFor(value.asCell()->type());
    }

    // Records at property creation, V8-style, from every C++ property-creation path, where the structure being
    // created IS the offset owner by definition, so this is owner-keyed with no ancestor walk. Recording at
    // birth means no object can predate the record, which removes retroactive verification: the establishment
    // heap walk it replaces cost ~10% on delta-blue and armed zero claims. Mutator thread only.
    // MEASUREMENT ONLY (fieldTypeCreationDummyWork=4). Exactly recordAtCreation's non-cell path -- take the
    // lock, hash the key, add a permanently-generalised entry -- and nothing else. It exists to separate the
    // two remaining candidates for the recorder's cost, which everything else has been eliminated from: the
    // locked hash insert, versus the per-entry FieldTypeRecord heap allocation that follows it. chai-wtb does
    // 29,590 inserts and allocates ~28,700 records to get 160 fires, so the two differ by a lot in what a fix
    // would look like: a cheaper table, or lazily materialising the record.
    void insertGeneralizedForMeasurement(StructureID owner, PropertyOffset offset)
    {
        uint64_t key = keyFor(owner, offset);
        if (!key || key == std::numeric_limits<uint64_t>::max())
            return;
        Locker locker { m_lock };
        auto result = m_records.add(key, Entry { owner, nullptr, StructureID() });
        if (result.isNewEntry)
            m_recordCount.store(m_records.size(), std::memory_order_relaxed);
    }

    // Clears the owner shape's cached claim word for a LAZY withdrawal, where there is no record to do it through.
    // Out of line because Structure is incomplete here.
    JS_EXPORT_PRIVATE static void clearShapeClaimCacheFor(StructureID owner);

    // THE LAZY TABLE ENTRY's support. Reads the owner shape's claim word. Out of line because Structure is
    // incomplete here, like clearShapeClaimCacheFor above.
    // Takes the offset and CHECKS IT. The claim word describes exactly ONE field -- the one this structure adds
    // (recordAtCreation asserts owner->transitionOffset() == offset). But seven ancestor walks call expectedFor()
    // and recordFor() with an arbitrary offset while stepping previousID(), so without this check a candidate that
    // adds a DIFFERENT property would hand back that other field's claim as if it were the claim for `offset` --
    // a wrong expected structure delivered to store maintenance and to the compiler, i.e. type confusion.
    // Returns offsetWasReused ("this shape carries no claim for this field in its word") when they disagree.
    JS_EXPORT_PRIVATE static uint16_t claimWordFor(StructureID owner, PropertyOffset offset);

    // Names a contradiction: the field, and the CLASS plus property count of both the claimed and the observed
    // structure. The question it answers is the one the raw StructureID bits cannot: when JSC withdraws a claim
    // that V8 keeps, are the two structures the same class -- i.e. is JSC splitting into two Structures what V8
    // represents with one Map? Out of line because Structure is incomplete here.
    JS_EXPORT_PRIVATE static void reportContradiction(StructureID owner, PropertyOffset, StructureID had, StructureID got);

    // The StructureID bits a word-only claim names, or 0 for "this shape carries no claim in its word". Callers
    // must hold m_lock: it indexes m_claimedStructureIDBits, which allocateClaimIndex appends to.
    uint32_t wordOnlyClaimBits(StructureID owner, PropertyOffset offset) WTF_REQUIRES_LOCK(m_lock)
    {
        uint16_t word = claimWordFor(owner, offset);
        if (word < FieldTypeClaimIndex::firstClaim)
            return 0;
        size_t slot = word - FieldTypeClaimIndex::firstClaim;
        if (slot >= m_claimedStructureIDBits.size()) [[unlikely]]
            return 0;
        return m_claimedStructureIDBits[slot];
    }

    // An offset at which some compilation stores with no check at all, so no claim there can survive.
    // Graph::poisonAllFieldTypesAtOffset withdraws the claims it can ENUMERATE, which is every materialised one;
    // a word-only claim is invisible to that walk, so this set is what keeps it from ever being narrowed on.
    // Refusing a word-only claim is always safe -- it falls back to an eager entry, i.e. today's behaviour -- so
    // an offset beyond the array reads as poisoned rather than clean.
    static constexpr size_t poisonedOffsetSlots = 1024;
    void notePoisonedOffset(PropertyOffset offset)
    {
        if (offset >= 0 && static_cast<size_t>(offset) < poisonedOffsetSlots)
            m_poisonedOffsets[offset].store(1, std::memory_order_relaxed);
    }
    bool offsetIsPoisoned(PropertyOffset offset) const
    {
        if (offset < 0 || static_cast<size_t>(offset) >= poisonedOffsetSlots) [[unlikely]]
            return true;
        return m_poisonedOffsets[offset].load(std::memory_order_relaxed);
    }

    uint64_t wordOnlyClaims() const { return m_wordOnlyClaims.load(std::memory_order_relaxed); }
    uint64_t wordOnlyWithdrawals() const { return m_wordOnlyWithdrawals.load(std::memory_order_relaxed); }
    uint64_t wordOnlyMaterialisations() const { return m_wordOnlyMaterialisations.load(std::memory_order_relaxed); }

    uint64_t lazyClaims() const { return m_lazyClaims.load(std::memory_order_relaxed); }
    uint64_t lazyMaterialisations() const { return m_lazyMaterialisations.load(std::memory_order_relaxed); }
    uint64_t lockWaitNanos() const { return m_lockWaitNanos.load(std::memory_order_relaxed); }
    uint64_t lockAcquisitions() const { return m_lockAcquisitions.load(std::memory_order_relaxed); }

    // DIAGNOSTIC ONLY (logFieldTypes). An EXACT per-(owner, offset) creation census that counts every creation,
    // including the ones fieldTypeCreationNeedsNoWork skips before this table is ever consulted.
    //
    // FieldTypeRecord::creationsSeen() cannot answer the admission question and it is important to know why: the
    // creation fast path returns early both for a claimless shape (`entryWithoutClaim`) and for a live claim the
    // value MATCHES, so a key reaches the table at birth and then at most once more -- the visit that contradicts
    // it. That counter is therefore structurally incapable of exceeding 2, and measuring it produces exactly that
    // (13,566 keys at 1 and 118 at 2, none higher, on acorn-wtb). Any statement of the form "contradictions arrive
    // by the Nth creation" read off creationsSeen is an artefact of the fast path, not a fact about the workload.
    //
    // What a confirmation threshold needs instead is how many objects of the shape are created before the claim is
    // committed to, which is what this counts.
    struct CreationCensus {
        uint32_t creations { 0 };
        uint32_t firstObservedBits { 0 };
        // The 1-based creation index whose value disagreed with the first; 0 means never contradicted.
        uint32_t contradictedAtCreation { 0 };
        // THE PATTERN-HUNT FEATURES. Every one must be computable AT CLAIM TIME, or a rule built on it could not
        // be implemented -- that constraint is what killed three earlier admission axes (README.md 0.6).
        //
        // storeHits is the analogue of V8's PropertyConstness: V8 decides kConst vs kMutable per field via
        // CanStayConst at IC time and gives it a whole dependency group (kFieldConstGroup), while a JSC claim is
        // made identically whether the field is assigned once in a constructor or reassigned every iteration.
        uint32_t storeHits { 0 };
        uint8_t claimedType { 0 };   // the claimed VALUE's JSType: object vs function vs array vs string
        uint8_t ownerIsPrototype { 0 };
        uint8_t claimedIsLeaf { 0 };   // V8's is_stable() on the CLAIMED value's structure
        // Kept so the dump can resolve the field name and owner class; StructureID's bits constructor is
        // private, so the packed key cannot be turned back into one. Same pattern as Entry::owner.
        StructureID owner;
    };
    void noteCreationForCensus(StructureID owner, PropertyOffset offset, JSValue value, bool ownerIsPrototype = false, bool claimedIsLeaf = false)
    {
        // Its own lock, never m_lock: taking m_lock here would serialise the fast path this instrument exists to
        // observe, and would change the very cost the campaign is measuring.
        StructureID observed = valueIsWorthClaiming(value) ? value.asCell()->structureID() : StructureID();
        uint8_t observedType = value.isCell() ? static_cast<uint8_t>(value.asCell()->type()) : 0;
        Locker locker { m_censusLock };
        auto& c = m_creationCensus.add(keyFor(owner, offset), CreationCensus { }).iterator->value;
        ++c.creations;
        if (c.creations == 1) {
            c.firstObservedBits = observed.bits();
            c.claimedType = observedType;
            c.ownerIsPrototype = ownerIsPrototype;
            c.claimedIsLeaf = claimedIsLeaf;
            c.owner = owner;
        }
        else if (!c.contradictedAtCreation && c.firstObservedBits != observed.bits())
            c.contradictedAtCreation = c.creations;
    }
    // A replace store landing on this field. THE candidate discriminator: a field assigned once at construction
    // and never written again is what delta-blue's valuable claims look like, and a field the program keeps
    // reassigning is what every harmful claim found so far looks like.
    void noteStoreForCensus(StructureID owner, PropertyOffset offset)
    {
        Locker locker { m_censusLock };
        auto iter = m_creationCensus.find(keyFor(owner, offset));
        if (iter != m_creationCensus.end())
            ++iter->value.storeHits;
    }
    JS_EXPORT_PRIVATE void dumpCreationCensus(VM&);

    // `bornClaim` is diagnostic. `outClaimBits` reports the claim live for (owner, offset) after this call; the
    // caller writes it into the owner Structure, where later creations of this shape read it without touching
    // this table (done by the caller because Structure is incomplete here).
    void recordAtCreation(VM& vm, StructureID owner, PropertyOffset offset, JSValue value, uint32_t* outClaimBits, uint16_t* outClaimIndex = nullptr, StructureID* bornClaim = nullptr, const char** bornSite = nullptr)
    {
        StructureID observed = valueIsWorthClaiming(value) ? value.asCell()->structureID() : StructureID();

        if (Options::logFieldTypes()) [[unlikely]]
            dataLogLn("[fieldtype] CREATE owner=", owner.bits(), " offset=", offset, " observed=", observed.bits());

        // Every path below leaves an entry in the table, so the caller may mark the shape unconditionally,
        // which is what lets a later creation of this shape skip the table entirely.
        *outClaimBits = 0;

        RefPtr<FieldTypeRecord> existing;
        bool lazyWithdrawal = false;
        {
            // MEASUREMENT: mode 3 times ONLY the lock acquisition, so contention is separated from the work under
            // it. The stage timer says the table section is 789 of 863 ns per call in-suite against ~20 ns
            // isolated, and both of the sections that blew up (this one and the ancestor withdrawal loop) are lock
            // operations. The earlier dismissal of contention used --useConcurrentJIT=0, which changes what is
            // compiled at all (F2's failure mode); this measures the wait directly instead.
            if (Options::fieldTypeTimeRecorder() == 3) [[unlikely]] {
                MonotonicTime lockStart = MonotonicTime::now();
                Locker probe { m_lock };
                m_lockWaitNanos.fetch_add(static_cast<uint64_t>((MonotonicTime::now() - lockStart).nanoseconds()),
                    std::memory_order_relaxed);
                m_lockAcquisitions.fetch_add(1, std::memory_order_relaxed);
            }
            Locker locker { m_lock };
            // Sentinel safety: m_records is an UncheckedKeyHashMap, whose empty value is 0 and whose deleted
            // value is ~0 for an integral key. A key equal to either corrupts the table silently.
            uint64_t key = keyFor(owner, offset);
            RELEASE_ASSERT(key, owner.bits(), offset);
            RELEASE_ASSERT(key != std::numeric_limits<uint64_t>::max(), owner.bits(), offset);

            // THE LAZY TABLE ENTRY. Try to answer with the shape's claim word alone, inserting nothing. A `find`
            // rather than an `add`: the add is the cost verified/09 priced at +2.24% on chai-wtb, +2.25% on
            // babylonjs-startup-es6, +1.88% on jsdom-d3-startup and +1.40% on babylonjs-scene-es6, and
            // verified/12 measures that 99.35% of the entries it creates are never consulted by a compiler.
            if (Options::useFieldTypeLazyEntries() && m_records.find(key) == m_records.end()) [[unlikely]] {
                uint16_t word = claimWordFor(owner, offset);
                if (word >= FieldTypeClaimIndex::firstClaim) {
                    // A LIVE WORD-ONLY CLAIM. Enforce it by comparing bits, exactly as the creation fast path
                    // does, and never touch the table.
                    size_t slot = word - FieldTypeClaimIndex::firstClaim;
                    uint32_t claimedBits = slot < m_claimedStructureIDBits.size() ? m_claimedStructureIDBits[slot] : 0;
                    if (claimedBits && claimedBits == observed.bits()) {
                        *outClaimBits = claimedBits;
                        if (outClaimIndex)
                            *outClaimIndex = word;
                        return;
                    }
                    // CONTRADICTION. No watchpoint to fire and nothing to jettison: every path that could have
                    // created a dependency materialises an entry first, so a claim with no entry has ZERO
                    // dependents by construction. That invariant is the whole soundness argument for this design.
                    m_wordOnlyWithdrawals.fetch_add(1, std::memory_order_relaxed);
                    lazyWithdrawal = true;
                } else if (word != FieldTypeClaimIndex::offsetWasReused) {
                    // A reused offset cannot be described by one word, so those shapes keep an eager entry.
                    if (!observed) {
                        // Permanently generalised, expressed in the word alone: *outClaimBits stays 0, so the
                        // caller writes entryWithoutClaim, and the creation fast path then skips this shape
                        // forever. Same outcome as the null entry, without the entry.
                        return;
                    }
                    if (!offsetIsPoisoned(offset)) {
                        uint16_t index = allocateClaimIndex(vm, observed);
                        if (index >= FieldTypeClaimIndex::firstClaim) [[likely]] {
                            *outClaimBits = observed.bits();
                            if (outClaimIndex)
                                *outClaimIndex = index;
                            if (bornClaim) {
                                *bornClaim = observed;
                                if (bornSite)
                                    *bornSite = "creation-word-only-claim";
                            }
                            m_wordOnlyClaims.fetch_add(1, std::memory_order_relaxed);
                            if (Options::logFieldTypes()) [[unlikely]]
                                dataLogLn("[fieldtype] WORD-ONLY-CLAIM owner=", owner.bits(), " offset=", offset, " claimed=", observed.bits());
                            return;
                        }
                        // Claim-index exhaustion returns noEntryYet; fall through to an eager entry, which works.
                    }
                }
            }

            if (!lazyWithdrawal) {
            auto result = m_records.add(key, Entry { owner, nullptr, StructureID() });
            if (result.isNewEntry)
                m_recordCount.store(m_records.size(), std::memory_order_relaxed);
            if (result.isNewEntry) {
                // A non-cell can never satisfy a structure claim, so leave the record null: permanently
                // generalised, no allocation, matching V8's Class-only-for-kHeapObject rule.
                if (!observed)
                    return;
                // THE LAZY CLAIM. Nothing has consulted this field yet, so the claim carries its StructureID in
                // the entry and skips both allocations (the record and its WatchpointSet). It is live and enforced
                // from here: the caller publishes the claim index on the owner shape below, and every runtime
                // enforcement path reads that word plus m_claimedStructureIDBits. recordFor() materialises on the
                // first compiler consultation, which is the first moment a record is needed.
                if (Options::useFieldTypeLazyRecords()) [[likely]] {
                    result.iterator->value.claimed = observed;
                    m_lazyClaims.fetch_add(1, std::memory_order_relaxed);
                } else {
                    auto record = FieldTypeRecord::create(owner, offset, observed);
                    record->setClaimSite("creation-on-brand-new-entry");
                    record->noteCreation();
                    result.iterator->value.record = record.ptr();
                }
                *outClaimBits = observed.bits();
                if (outClaimIndex)
                    // Called with m_lock already held by the enclosing Locker.
                    *outClaimIndex = allocateClaimIndex(vm, observed);
                if (bornClaim) {
                    *bornClaim = observed;
                    if (bornSite)
                        *bornSite = "creation-on-brand-new-entry";
                }
                return;
            }
            existing = result.iterator->value.record;
            if (!existing && result.iterator->value.claimed) {
                // A LAZY claim, maintained without materialising -- which is the point: chai-wtb creates 28,936
                // claims and 147 narrowings, so the common case is a claim that is re-observed thousands of times
                // and never consulted. Matching costs a compare; contradiction costs clearing one word, and needs
                // no watchpoint fire because nothing can depend on a claim no compiler has seen.
                StructureID lazyClaim = result.iterator->value.claimed;
                if (lazyClaim == observed) {
                    *outClaimBits = observed.bits();
                    if (outClaimIndex)
                        *outClaimIndex = allocateClaimIndex(vm, observed);
                    return;
                }
                result.iterator->value.claimed = StructureID();
                lazyWithdrawal = true;
            }
            }
        }

        if (lazyWithdrawal) [[unlikely]] {
            // Outside the lock, like every other withdrawal. The shape's cached claim word is a cache of the table
            // and must be cleared after it, never before: a stale-SET word is merely conservative (the creation
            // consults the table and finds nothing) while a stale-CLEAR word would let a creation skip maintenance
            // of a live claim, which is unsound.
            clearShapeClaimCacheFor(owner);
            if (Options::logFieldTypes()) [[unlikely]]
                dataLogLn("[fieldtype] GENERALIZE-at-creation-LAZY owner=", owner.bits(), " offset=", offset);
            return;
        }

        // A null record means the field is already permanently generalised: nothing to maintain.
        if (!existing)
            return;
        existing->noteCreation();

        // An unclaimed record was created by a store site before any object of this shape existed. Establishing
        // the claim here makes that site's already-compiled code match, because it loads rather than bakes it.
        //
        // NOT the source of raytrace's harmful claims: gating this path left NARROW at 2271 -> 2280, i.e. the
        // must-move counter did not move, so the claim-provenance hypothesis is refuted here (todo/24).
        if (!existing->hasClaim()) {
            if (!existing->isGeneralized()) {
                if (observed) {
                    existing->establish(observed);
                    if (existing->expected() == observed) {
                        existing->setClaimSite("creation-establishing-on-store-site-record");
                        *outClaimBits = observed.bits();
                        if (bornClaim) {
                            *bornClaim = observed;
                            if (bornSite)
                                *bornSite = "creation-establishing-on-store-site-record";
                        }
                    }
                } else
                    existing->generalize(vm); // a non-cell can never satisfy a structure claim
            }
            return;
        }

        // Report the live claim even when this creation merely MATCHED a pre-existing one: otherwise the caller
        // caches "no claim" on the shape, the fast path reads that as "skip", and maintenance stops.
        if (outClaimBits && existing->hasClaim() && existing->expected() == observed)
            *outClaimBits = observed.bits();

        // generalize() fires a watchpoint, so it must run with m_lock released: firing reenters arbitrary VM
        // code (code-block jettison) and could otherwise deadlock.
        if (existing->expected() != observed) {
            if (Options::logFieldTypes()) [[unlikely]]
                dataLogLn("[fieldtype]   GENERALIZE-at-creation owner=", owner.bits(), " offset=", offset, " had=", existing->expected().bits(), " got=", observed.bits());
            if (Options::logFieldTypes()) [[unlikely]]
                reportContradiction(owner, offset, existing->expected(), observed);
            existing->generalize(vm);
        }
    }

    // Safe from any thread, including compiler threads. Returns null when the field has no record at all; the
    // caller must additionally treat a generalised record as "no information".
    //
    // MATERIALISES a lazy claim. That is not an optimisation detail, it is the soundness contract: a null return
    // means "no claim, nothing to enforce", and every store-side caller is written against it -- a site that sees
    // null emits no check. If a lazy claim returned null here, that site would store unchecked while the claim
    // stayed live and while narrowed code had deleted its CheckStructure, which is the unsound-store class that
    // cost 110 stress failures when it was tried deliberately. So laziness ends at the first consultation that
    // could lead to a bake or a store check, which is also the first moment a record is actually needed.
    RefPtr<FieldTypeRecord> recordFor(StructureID owner, PropertyOffset offset)
    {
        Locker locker { m_lock };
        auto iter = m_records.find(keyFor(owner, offset));
        if (iter == m_records.end()) {
            // MATERIALISE a word-only claim: this is a compiler consultation, i.e. the first moment a record is
            // actually needed, and the moment the zero-dependents invariant would otherwise be broken.
            if (Options::useFieldTypeLazyEntries()) [[unlikely]] {
                if (uint32_t bits = wordOnlyClaimBits(owner, offset))
                    return materialiseWordOnlyLocked(owner, offset, bits);
            }
            return nullptr;
        }
        if (!iter->value.record && iter->value.claimed) [[unlikely]]
            return materialiseLocked(iter->value, owner, offset);
        return iter->value.record;
    }

    // Gives a lazy claim its record. Called with m_lock held. Idempotent by construction: `claimed` is cleared, so
    // the entry moves permanently into the materialised state and every later reader takes the fast path.
    // Gives a WORD-ONLY claim its table entry and record. Called with m_lock held, on the first consultation
    // that could lead to a dependency or a store check -- which is exactly where laziness has to end.
    RefPtr<FieldTypeRecord> materialiseWordOnlyLocked(StructureID owner, PropertyOffset offset, uint32_t claimedBits) WTF_REQUIRES_LOCK(m_lock)
    {
        auto result = m_records.add(keyFor(owner, offset), Entry { owner, nullptr, StructureID() });
        if (result.isNewEntry)
            m_recordCount.store(m_records.size(), std::memory_order_relaxed);
        if (!result.iterator->value.record) {
            result.iterator->value.claimed = structureIDFromClaimBits(claimedBits);
            materialiseLocked(result.iterator->value, owner, offset);
            m_wordOnlyMaterialisations.fetch_add(1, std::memory_order_relaxed);
        }
        return result.iterator->value.record;
    }

    RefPtr<FieldTypeRecord> materialiseLocked(Entry& entry, StructureID owner, PropertyOffset offset)
    {
        auto record = FieldTypeRecord::create(owner, offset, entry.claimed);
        record->setClaimSite("materialised-from-lazy-claim");
        entry.record = record.ptr();
        entry.claimed = StructureID();
        m_lazyMaterialisations.fetch_add(1, std::memory_order_relaxed);
        return entry.record;
    }

    // Does an entry exist for (owner, offset), whether or not it still carries a claim? A generalised field
    // keeps its entry with a null record, so recordFor cannot tell that from "never recorded". Diagnostic only.
    bool hasEntryFor(StructureID owner, PropertyOffset offset)
    {
        Locker locker { m_lock };
        return m_records.contains(keyFor(owner, offset));
    }

    // The expected structure for (owner, offset), or null if there is no claim or it is generalised. STAYS LAZY:
    // this is the C++ store and maintenance path, which only ever COMPARES structures, so it needs no record and
    // must not force one -- forcing here would materialise every claim on the hot path and forfeit the whole fix.
    StructureID expectedFor(StructureID owner, PropertyOffset offset)
    {
        Locker locker { m_lock };
        auto iter = m_records.find(keyFor(owner, offset));
        if (iter == m_records.end()) {
            // A WORD-ONLY claim has no entry, and answering "no claim" here would tell the store maintenance path
            // and the heap verifier that there is nothing to maintain -- the unsound direction. Compare-only, so
            // it still forces no record: the word plus the interned bits are the whole claim.
            if (Options::useFieldTypeLazyEntries()) [[unlikely]] {
                if (uint32_t bits = wordOnlyClaimBits(owner, offset))
                    return structureIDFromClaimBits(bits);
            }
            return StructureID();
        }
        if (iter->value.record)
            return iter->value.record->expected();
        return iter->value.claimed;
    }

    // Called from the store paths when a value of unexpected structure is written. Mutator thread only.
    void generalize(VM& vm, StructureID owner, PropertyOffset offset)
    {
        RefPtr record = recordFor(owner, offset);
        if (Options::logFieldTypes()) [[unlikely]] {
            // Inside the null check, and reporting whether a claim actually existed: a withdrawal is only
            // real when hadClaim=1, since the non-cell branch of recording calls here unconditionally.
            dataLogLn("[fieldtype] GENERALIZE-by-store owner=", owner.bits(), " offset=", offset,
                " present=", !!record, " hadClaim=", record && record->hasClaim());
        }
        if (record)
            record->generalize(vm);
    }

    unsigned size() const
    {
        Locker locker { m_lock };
        // Word-only claims are added in, for the reason spelled out on sizeRelaxed() below: with lazy entries a
        // claim is LIVE with no table entry, so m_records.size() alone reports "nothing to maintain" while claims
        // exist. Heap.cpp's verifier guards on this, and a verifier that silently skips is worse than none.
        return m_records.size() + liveWordOnlyClaimsForSizing();
    }

    // Lock-free version of size() for the hot gates. size() itself takes m_lock, and the paths that guard on it
    // -- every C++ replace store, every megamorphic put slow path -- were therefore paying a contended lock
    // acquire per store, on a table the mutator and the compiler threads share. It also never returned early,
    // because a record is inserted for every store site.
    //
    // A stale read is safe in the only direction that matters. Claims are created by establish(), which is
    // mutator-only, and every insert adds a CLAIMLESS entry, so a mutator missing another thread's increment
    // can only miss an entry that has nothing to maintain. Reading high is merely conservative: the caller
    // proceeds to the locked slow path and finds nothing.
    // THE LAZY-ENTRY CORRECTION. m_recordCount is only ever m_records.size(), so with useFieldTypeLazyEntries a
    // claim established in the owner shape's word alone leaves this at ZERO while the claim is live. Six callers
    // treat zero as "no field types exist, skip everything", and five of them are soundness-critical:
    //
    //   JSObjectInlines.h:775  maintainFieldTypeRecord    -- every C++ replace store's maintenance
    //   JSObjectInlines.h:682  ancestor withdrawal at creation
    //   JSObject.cpp:512       delete/attribute-change maintenance
    //   JITOperations.cpp:1112 megamorphic put slow path
    //   Heap.cpp:3519          the heap verifier itself
    //
    // Bailing there under a live word-only claim lets a store write a contradicting value with NOTHING withdrawing
    // the claim, while narrowed code has already deleted its CheckStructure. That is the type-confusion class, and
    // it is what killed prettier-wtb with EXC_BREAKPOINT inside JIT code (field-types/verified/13 5b).
    //
    // The word-only count is monotonic, so once any word-only claim has been made this reads non-zero for the rest
    // of the process. That is the conservative direction -- callers proceed to the slow path and find nothing --
    // and it is the same behaviour these gates had before lazy entries, when every store site inserted an entry.
    unsigned sizeRelaxed() const
    {
        return m_recordCount.load(std::memory_order_relaxed) + liveWordOnlyClaimsForSizing();
    }
    unsigned liveWordOnlyClaimsForSizing() const
    {
        uint64_t words = m_wordOnlyClaims.load(std::memory_order_relaxed);
        return static_cast<unsigned>(std::min<uint64_t>(words, std::numeric_limits<unsigned>::max() / 2));
    }
    std::atomic<unsigned> m_recordCount { 0 };

    // Returns a real record for (owner, offset), creating an as-yet-unclaimed one if needed. Nothing is poisoned:
    // a CheckFieldType node loads the claim, so a store site compiled before any claim existed exits until a
    // baseline store records one, after which the SAME code matches -- which is what lets a JIT-emitted creating
    // store participate in recording. Callable from compiler threads: m_records is lock-guarded, no GC objects.
    RefPtr<FieldTypeRecord> ensureRecordForStoreSite(StructureID owner, PropertyOffset offset)
    {
        // MEASUREMENT SWITCH. Distinct from recordForStoreSite and far more expensive: this one ALLOCATES a
        // FieldTypeRecord (with its WatchpointSet) on a new entry, and both parse-time hoists call it for every
        // put site in every compilation. That is unconditional work concentrated in the first compiles, i.e.
        // exactly where the systematic First-Score penalty lives (23 of 73 tests lose First significantly).
        //
        // Sound combined with fieldTypeClaimableKinds=4: no claim can form, so an unclaimed record has nothing to
        // hold, and the hoist's next step (isGeneralized) already declines to emit under NeverClaim.
        if (!Options::useFieldTypeStoreSiteRecordCreation()) [[unlikely]]
            return nullptr;
        if (Options::logFieldTypes()) [[unlikely]]
            dataLogLn("[fieldtype] ENSURE-STORE-SITE owner=", owner.bits(), " offset=", offset);
        Locker locker { m_lock };
        auto result = m_records.add(keyFor(owner, offset), Entry { owner, nullptr, StructureID() });
        if (result.isNewEntry)
            m_recordCount.store(m_records.size(), std::memory_order_relaxed);
        // A brand-new entry here may still be covering a LIVE word-only claim. Answering with an unclaimed
        // record would tell this store site "nothing to enforce", which is the hole that cost 90 stress configs.
        if (result.isNewEntry && Options::useFieldTypeLazyEntries()) [[unlikely]] {
            if (uint32_t bits = wordOnlyClaimBits(owner, offset)) {
                result.iterator->value.claimed = structureIDFromClaimBits(bits);
                m_wordOnlyMaterialisations.fetch_add(1, std::memory_order_relaxed);
                return materialiseLocked(result.iterator->value, owner, offset);
            }
        }
        if (!result.isNewEntry) {
            // MATERIALISES a lazy claim, for the same soundness reason as recordForStoreSiteImpl: this store site
            // needs a record to load or bake the claim from, and answering null would let it store unchecked.
            if (!result.iterator->value.record && result.iterator->value.claimed) [[unlikely]]
                return materialiseLocked(result.iterator->value, owner, offset);
            // Must not resurrect a null (permanently generalised) entry into a fresh unclaimed one: a claim
            // re-established from one later observation is contradicted by every object created while the field
            // was generalised. Measured: resurrecting manufactured eleven false claims across three benchmarks.
            return result.iterator->value.record;
        }

        // Brand new, so no creation and no other store site has touched this field: nothing of this shape can
        // predate the record, and a claim established later is safe.
        result.iterator->value.record = FieldTypeRecord::create(owner, offset, StructureID());
        return result.iterator->value.record;
    }

    // The fail-safe default for a store site about to be compiled or cached with no inline field-type check.
    // Answering "no record" is unsound: a claim established after that code was compiled would never be
    // maintained and would stay false for the rest of the run. So a brand-new field is permanently generalised
    // here. V8 needs no equivalent because its store handler loads the field type at runtime.
    // Per-call-site accounting for the poisoning. Five mechanism guesses in a row were wrong about which caller
    // costs raytrace its 5.5 points, while turning the whole machinery off recovers all of it -- so the split has
    // to be counted, not reasoned about (todo/11 F12). `poisons` counts new keys permanently generalised here;
    // `claimHits` counts consultations that found a LIVE claim.
    struct StoreSiteStat { const char* site { nullptr }; std::atomic<unsigned> poisons { 0 }; std::atomic<unsigned> claimHits { 0 }; };
    std::array<StoreSiteStat, 24> m_storeSiteStats { };
    // Interned on the literal's pointer, so the scan is over a handful of entries and needs no hashing. Called
    // only under logFieldTypes, so it is off any measured path.
    StoreSiteStat* statFor(const char* site)
    {
        for (auto& stat : m_storeSiteStats) {
            if (stat.site == site)
                return &stat;
            if (!stat.site) { stat.site = site; return &stat; }
        }
        return nullptr;
    }
    void dumpStoreSiteStats()
    {
        for (auto& stat : m_storeSiteStats) {
            if (!stat.site)
                break;
            dataLogLn("[fieldtype] STORE-SITE-STAT ", stat.site,
                " poisons=", stat.poisons.load(std::memory_order_relaxed),
                " claimHits=", stat.claimHits.load(std::memory_order_relaxed));
        }
    }

    // Direct timing, because ablation cannot resolve this: every sub-gate of the store-site machinery reads
    // inert on raytrace (llint bake -5.43%, IC checks -5.44%, IC downgrade -5.30%, DFG poisoning -5.36% vs
    // CONTROL -5.46%) while turning the machinery OFF recovers +5.75 points. That is the chai-wtb situation
    // again, and there only the timer found it. Mode 2 times an empty section for the floor.
    RefPtr<FieldTypeRecord> recordForStoreSite(StructureID owner, PropertyOffset offset, const char* site = "unlabelled")
    {
        if (Options::fieldTypeTimeStoreSite()) [[unlikely]] {
            MonotonicTime start = MonotonicTime::now();
            RefPtr<FieldTypeRecord> r;
            if (Options::fieldTypeTimeStoreSite() == 1)
                r = recordForStoreSiteImpl(owner, offset, site);
            uint64_t elapsed = static_cast<uint64_t>((MonotonicTime::now() - start).nanoseconds());
            m_storeSiteNanos.fetch_add(elapsed, std::memory_order_relaxed);
            m_storeSiteCalls.fetch_add(1, std::memory_order_relaxed);
            return r;
        }
        return recordForStoreSiteImpl(owner, offset, site);
    }

    std::atomic<uint64_t> m_storeSiteNanos { 0 };
    std::atomic<uint64_t> m_storeSiteCalls { 0 };
    void dumpStoreSiteTime()
    {
        uint64_t calls = m_storeSiteCalls.load(std::memory_order_relaxed);
        if (calls)
            dataLogLn("[fieldtype] STORE-SITE-TIME mode=", Options::fieldTypeTimeStoreSite(), " calls=", calls,
                " totalNanos=", m_storeSiteNanos.load(std::memory_order_relaxed),
                " nsPerCall=", m_storeSiteNanos.load(std::memory_order_relaxed) / calls);
    }

    RefPtr<FieldTypeRecord> recordForStoreSiteImpl(StructureID owner, PropertyOffset offset, const char* site)
    {
        // MEASUREMENT SWITCH. This function takes m_lock and does a hash add on EVERY call, including when the
        // entry already exists, and it is reached on first execution of every store site (LLInt cache install, IC
        // handler generation, DFG compile). Populations are per-SITE and large: 256k on FlightPlanner, 244k on
        // jsdom-d3-startup, 153k on json-parse-inspector. That makes it the leading candidate for the systematic
        // First-Score penalty (23 of 73 tests lose First significantly, only 2 gain it), since iteration 1 is
        // where every store site runs for the first time.
        //
        // UNSOUND alone -- answering "no record" lets a claim established later go unmaintained -- and sound
        // combined with fieldTypeClaimableKinds=4, where no claim can ever form so there is nothing to poison.
        if (!Options::useFieldTypeStoreSiteRecording()) [[unlikely]]
            return nullptr;
        Locker locker { m_lock };
        auto result = m_records.add(keyFor(owner, offset), Entry { owner, nullptr, StructureID() });
        if (result.isNewEntry)
            m_recordCount.store(m_records.size(), std::memory_order_relaxed);
        // Same hole as ensureRecordForStoreSite: a new entry can be covering a live word-only claim, and the
        // permanently-generalised null below would answer "no claim, emit no check" underneath it.
        if (result.isNewEntry && Options::useFieldTypeLazyEntries()) [[unlikely]] {
            if (uint32_t bits = wordOnlyClaimBits(owner, offset)) {
                result.iterator->value.claimed = structureIDFromClaimBits(bits);
                m_wordOnlyMaterialisations.fetch_add(1, std::memory_order_relaxed);
                materialiseLocked(result.iterator->value, owner, offset);
                if (result.iterator->value.record) [[likely]]
                    result.iterator->value.record->noteCacheRefusal();
                return result.iterator->value.record;
            }
        }
        if (result.isNewEntry) {
            if (Options::logFieldTypes()) [[unlikely]] {
                dataLogLn("[fieldtype] POISON-UNCHECKED-STORE-SITE owner=", owner.bits(), " offset=", offset);
                if (StoreSiteStat* stat = statFor(site))
                    stat->poisons.fetch_add(1, std::memory_order_relaxed);
            }
            // A null record IS the permanently-generalised state, so this costs one hash entry and no allocation.
            // Every unchecked store site lands here, and allocating a record plus WatchpointSet each measured
            // ~200 MB across JetStream3, enough to reach the OOM killer.
            return nullptr;
        }
        // MATERIALISES a lazy claim, and this is load-bearing rather than tidy: THE HEAP VERIFIER CAUGHT ITS
        // ABSENCE. Returning null here for a live lazy claim tells the store site "no claim, emit no check", so the
        // site then writes a contradicting structure with nothing withdrawing the claim -- 90 stress configs and
        // `expected=16828032 observed=16796416` from field-type-claim-word-state-machine.js. Every entry point that
        // can lead to a store or a bake must materialise; only expectedFor(), which merely compares, may stay lazy.
        if (!result.iterator->value.record && result.iterator->value.claimed) [[unlikely]]
            materialiseLocked(result.iterator->value, owner, offset);

        // Every store-site family -- LLInt metadata, ById IC, ByVal IC, and the DFG -- reaches a live claim
        // through here, so this is the one place that can count how often store sites consult one. Counting only:
        // this runs on compiler threads too (DFGGraph.cpp), where firing a watchpoint is not safe, so the
        // surrender decision is taken by the mutator-thread caller.
        if (result.iterator->value.record) [[likely]]
            result.iterator->value.record->noteCacheRefusal();
        // Every decline family -- LLInt metadata, ById IC, ByVal IC -- reaches a live claim through here, so this
        // is the one place that counts how often a store site is refused its cache.
        if (Options::logFieldTypes()) [[unlikely]] {
            if (result.iterator->value.record && result.iterator->value.record->expected()) {
                dataLogLn("[fieldtype] STORE-SITE-CLAIM-HIT owner=", owner.bits(), " offset=", offset, " site=", site);
                if (StoreSiteStat* stat = statFor(site))
                    stat->claimHits.fetch_add(1, std::memory_order_relaxed);
                static std::atomic<unsigned> reported { 0 };
                if (reported++ < 6)
                    WTFReportBacktrace();
            }
        }
        return result.iterator->value.record;
    }


    // Every live claim naming this offset, whoever owns it, for a store site whose base structures are unknown.
    Vector<RefPtr<FieldTypeRecord>> recordsAtOffset(PropertyOffset offset)
    {
        Vector<RefPtr<FieldTypeRecord>> result;
        Locker locker { m_lock };
        for (auto& entry : m_records) {
            if (static_cast<PropertyOffset>(static_cast<uint32_t>(entry.key)) != offset)
                continue;
            // MATERIALISES. This is the fallback for a store site whose base structures are unknown, so it must see
            // every live claim at the offset or that site stores unchecked under one. A lazy claim cannot be handed
            // back as "nothing here", and it cannot be withdrawn from here either -- the caller defers withdrawal
            // through Plan::addFieldTypeToGeneralize, which needs a real record to hold.
            if (!entry.value.record && entry.value.claimed) [[unlikely]]
                materialiseLocked(entry.value, entry.value.owner, offset);
            if (entry.value.record && entry.value.record->expected())
                result.append(entry.value.record);
        }
        return result;
    }


    // Interned: one slot per distinct claimed StructureID rather than one per claimed field, which is what makes
    // a 16-bit index workable. Uninterned, a full JetStream3 run consumed all 65533 slots to hold 990 distinct
    // values. A slot is never recycled, so an index means the same bits for the VM's life: handing a slot to a
    // different structure would make owners still pointing at it match the wrong claim. Reclamation comes free
    // from StructureID recycling -- a dead structure's entry stays, so a recycled ID claimed again lands on a
    // slot already holding those bits (map[b] == k implies bits[k] == b).
    UncheckedKeyHashMap<uint32_t, uint16_t> m_slotForClaimedStructure;

    // StructureID's bits constructor is private, so the interned bits alone cannot be turned back into a
    // StructureID. m_claimedStructures is the parallel vector that can, kept in lockstep by allocateClaimIndex.
    // Bounded by the number of DISTINCT claimed structures, not by claims -- slots are interned by value.
    StructureID structureIDFromClaimBits(uint32_t bits) WTF_REQUIRES_LOCK(m_lock)
    {
        auto iter = m_slotForClaimedStructure.find(bits);
        if (iter == m_slotForClaimedStructure.end()) [[unlikely]]
            return StructureID();
        size_t slot = iter->value - FieldTypeClaimIndex::firstClaim;
        return slot < m_claimedStructures.size() ? m_claimedStructures[slot] : StructureID();
    }

    uint16_t allocateClaimIndex(VM& vm, StructureID claimed)
    {
        uint32_t structureIDBits = claimed.bits();
        // One hash lookup on a path that runs once per claimed field; a hit is the common case once warm.
        auto existing = m_slotForClaimedStructure.find(structureIDBits);
        if (existing != m_slotForClaimedStructure.end()) [[likely]]
            return existing->value;

        size_t next = m_claimedStructureIDBits.size();
        if (next > static_cast<size_t>(UINT16_MAX) - FieldTypeClaimIndex::firstClaim) [[unlikely]] {
            // On exhaustion return noEntryYet, meaning "consult the table" -- never entryWithoutClaim, which
            // means "skip, nothing to maintain": publishing "skip" while the table held a live claim stopped
            // maintenance and produced 336429 heap-verifier violations. The safe direction for any miss in this
            // word is "go ask the table", so these fields still narrow. Exhaustion is permanent; report once.
            static std::once_flag onceFlag;
            std::call_once(onceFlag, [] {
                dataLogLn("[fieldtype] CLAIM INDEX EXHAUSTED: all ", UINT16_MAX - FieldTypeClaimIndex::firstClaim,
                    " slots consumed. Fields claimed from here on keep working, but pay the table lookup on "
                    "creation instead of the shape-carried fast path.");
            });
            return FieldTypeClaimIndex::noEntryYet;
        }
        m_claimedStructureIDBits.append(structureIDBits);
        m_claimedStructures.append(claimed);
        // Republish the base: append may have reallocated, and the creation fast path reads the buffer
        // directly off VM. This is the only place it can move.
        vm.m_fieldTypeClaimBits = m_claimedStructureIDBits.span().data();
        uint16_t allocated = static_cast<uint16_t>(next + FieldTypeClaimIndex::firstClaim);
        m_slotForClaimedStructure.add(structureIDBits, allocated);
        return allocated;
    }

    // How many distinct claimed-StructureID values the slots hold; far below claimIndexCount() means the index
    // space is going on duplicates.
    size_t distinctClaimedStructureCount() const
    {
        UncheckedKeyHashSet<uint32_t> seen;
        for (uint32_t bits : m_claimedStructureIDBits) {
            if (bits)
                seen.add(bits);
        }
        return seen.size();
    }

    // Slots handed out; never reclaimed, so this only grows. Readable as $vm.fieldTypeClaimIndexCount().
    size_t claimIndexCount() const { return m_claimedStructureIDBits.size(); }

    void pruneAfterMarking(VM&);

    // THE FIX from verified/03: memoise Structure::findOffsetOwner for the C++ replace-store path.
    //
    // That walk scans the transition chain one previousID() dereference per link, and it runs on EVERY replace
    // store as soon as the table is non-empty, because the claim-word short-circuit is only reachable once the
    // owner is known. Measured populations: 299,630 calls on typescript-lib, 151,540 on jsdom-d3-startup,
    // 129,520 on babylonjs-scene-es6 -- and removing the walk entirely is worth 1.08%, 0.26% and 0.96%
    // respectively (gbemu 1.35%).
    //
    // The answer is IMMUTABLE for a live (structure, offset): a structure's chain above it never changes, and
    // findOffsetOwner is a pure function of that chain. So it can be cached with no invalidation protocol beyond
    // structure death.
    //
    // Safe against StructureID recycling, which is the hazard that matters here: the memo is cleared at the end
    // of every marking (see pruneAfterMarking), and a structure cannot die and have its identity reused without a
    // collection running first. So every entry present was inserted after the last clear, when its structure was
    // live. Storing a raw Structure* is safe for the same reason plus one more -- the owner is an ANCESTOR of the
    // memoised structure, so it is reachable through previousID() for as long as the key is.
    //
    // Mutator-only: maintainFieldTypeRecord runs on the mutator, and the compiler threads keep calling
    // Structure::findOffsetOwner directly. That is what makes an unlocked direct-mapped array correct here.
    JS_EXPORT_PRIVATE Structure* findOffsetOwnerMemoised(Structure*, PropertyOffset);
    void clearOffsetOwnerMemo()
    {
        // Only the live prefix: the array is allocated at the maximum so one build can sweep sizes, but a run only
        // ever uses `offsetOwnerMemoLiveSize()` of it, and clearing the rest would price a capacity nobody chose.
        for (auto& entry : std::span { m_offsetOwnerMemo }.first(offsetOwnerMemoLiveSize()))
            entry = { };
    }

    // Hit/miss accounting for the memo, so its capacity is chosen from a measured hit rate rather than assumed. The
    // original 512 entries were sized against "every distinct (shape, offset) pair these benchmarks touch in steady
    // state" -- measured per test. In-suite the working set is far larger: 1,082,267 maintenance calls against a
    // table that reaches 169,778 entries, so conflict misses are the expected failure and each one is a chain walk.
    uint64_t offsetOwnerMemoHits() const { return m_offsetOwnerMemoHits.load(std::memory_order_relaxed); }
    uint64_t offsetOwnerMemoMisses() const { return m_offsetOwnerMemoMisses.load(std::memory_order_relaxed); }

private:
    struct OffsetOwnerMemoEntry {
        Structure* structure { nullptr };
        Structure* owner { nullptr };
        PropertyOffset offset { invalidOffset };
    };
    // Direct-mapped, mutator-only, cleared at end of marking. Allocated at the maximum and masked down to the
    // option's value so a single build can price several capacities; 65536 entries is ~1.5 MB per VM, against a
    // heap that peaks at 9 GB on this suite and a field-type table that already holds 169,778 entries.
    static constexpr unsigned offsetOwnerMemoMaxSize = 65536;
    static ALWAYS_INLINE unsigned offsetOwnerMemoLiveSize()
    {
        unsigned requested = Options::fieldTypeOwnerMemoSize();
        if (!requested || requested > offsetOwnerMemoMaxSize)
            return offsetOwnerMemoMaxSize;
        // Round down to a power of two so the index is a single AND.
        return 1u << (31 - WTF::clz(requested));
    }
    std::array<OffsetOwnerMemoEntry, offsetOwnerMemoMaxSize> m_offsetOwnerMemo { };
    std::atomic<uint64_t> m_offsetOwnerMemoHits { 0 };
    std::atomic<uint64_t> m_offsetOwnerMemoMisses { 0 };

    // Packs (owner, offset) into one key. A live StructureID is never 0, so the key is never 0 and cannot
    // collide with UncheckedKeyHashMap's empty-value sentinel.
    static uint64_t keyFor(StructureID owner, PropertyOffset offset)
    {
        return (static_cast<uint64_t>(owner.bits()) << 32) | static_cast<uint32_t>(offset);
    }

    // m_lock guards m_records only; the records are refcounted and independently thread-safe, so a reader can
    // drop the lock and keep using its Ref. A HashMap, not a Vector: delta-blue alone performs 631k property
    // creations. The owner is kept alongside the record so pruneAfterMarking can check its liveness even for a
    // generalised entry, whose record is null, and because StructureID's bits constructor is private.
    Vector<uint32_t> m_claimedStructureIDBits;
    // Parallel to m_claimedStructureIDBits so a word-only claim can be turned back into a StructureID.
    Vector<StructureID> m_claimedStructures;
    mutable Lock m_lock;
    UncheckedKeyHashMap<uint64_t, Entry> m_records;

    std::array<std::atomic<uint8_t>, poisonedOffsetSlots> m_poisonedOffsets { };
    std::atomic<uint64_t> m_wordOnlyClaims { 0 };
    std::atomic<uint64_t> m_wordOnlyWithdrawals { 0 };
    std::atomic<uint64_t> m_wordOnlyMaterialisations { 0 };

    mutable Lock m_censusLock;
    UncheckedKeyHashMap<uint64_t, CreationCensus> m_creationCensus;

    std::atomic<uint64_t> m_lazyClaims { 0 };
    std::atomic<uint64_t> m_lazyMaterialisations { 0 };
    std::atomic<uint64_t> m_lockWaitNanos { 0 };
    std::atomic<uint64_t> m_lockAcquisitions { 0 };

};

class StructureFireDetail final : public FireDetail {
public:
    inline StructureFireDetail(const Structure*); // Defined in StructureInlines.h.

    void dump(PrintStream& out) const final;

private:
    explicit StructureFireDetail(ClangVTableWorkaroundTag);

    const Structure* m_structure;
};

class Structure : public JSCell {
    static constexpr uint16_t shortInvalidOffset = std::numeric_limits<uint16_t>::max() - 1;
    static constexpr uint16_t useRareDataFlag = std::numeric_limits<uint16_t>::max();
public:
    friend class StructureTransitionTable;

    typedef JSCell Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;
    static constexpr uint8_t numberOfLowerTierPreciseCells = 0;

    static_assert(JSCell::atomSize >= MarkedBlock::atomSize);

    static constexpr int s_maxTransitionLength = 128;
    static constexpr int s_maxTransitionLengthForNonEvalPutById = 512;
    static constexpr int s_maxTransitionLengthForRemove = 4096; // Picked from benchmarking measurement.

    using SeenProperties = TinyBloomFilter<CompactPtr<UniquedStringImpl>::StorageType>;

    enum PolyProtoTag { PolyProto };
    inline static Structure* create(VM&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType = NonArray, unsigned inlineCapacity = 0); // Defined in StructureInlines.h
    static Structure* create(PolyProtoTag, VM&, JSGlobalObject*, JSObject* prototype, const TypeInfo&, const ClassInfo*, IndexingType = NonArray, unsigned inlineCapacity = 0);

    ~Structure();
    
    template<typename CellType, SubspaceAccess>
    inline static GCClient::IsoSubspace* subspaceFor(VM&); // Defined in StructureInlines.h

    JS_EXPORT_PRIVATE static bool isValidPrototype(JSValue);

protected:
    inline void finishCreation(VM& vm, const Structure* previous, DeferredStructureTransitionWatchpointFire* deferred); // Defined in StructureInlines.h

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(m_prototype.get().isEmpty() || isValidPrototype(m_prototype.get()));
    }

private:
    inline void finishCreation(VM&, CreatingEarlyCellTag); // Defined in StructureInlines.h

    void NODELETE validateFlags();

public:
    StructureID id() const { return StructureID::encode(this); }

    uint32_t typeInfoBlob() const { return m_blob.blob(); }

    bool isProxy() const
    {
        JSType type = m_blob.type();
        return type == GlobalProxyType || type == ProxyObjectType;
    }

    static void dumpStatistics();

    inline bool shouldDoCacheableDictionaryTransitionForAdd(PutPropertySlot::Context context)
    {
        int maxTransitionLength;
        if (context == PutPropertySlot::PutById)
            maxTransitionLength = s_maxTransitionLengthForNonEvalPutById;
        else
            maxTransitionLength = s_maxTransitionLength;
        return transitionCountEstimate() > maxTransitionLength;
    }

    inline bool shouldDoCacheableDictionaryTransitionForRemoveAndAttributeChange()
    {
        return transitionCountEstimate() > s_maxTransitionLengthForRemove || transitionCountHasOverflowed();
    }

    ALWAYS_INLINE bool transitionCountHasOverflowed() const
    {
        int transitionCount = 0;
        for (auto* structure = this; structure; structure = structure->previousID()) {
            if (++transitionCount > s_maxTransitionLength)
                return true;
        }

        return false;
    }

    Structure* trySingleTransition() { return m_transitionTable.trySingleTransition(); }
    // Has any shape ever been derived from this one? The negation is V8's Map::is_stable(), which is the gate
    // Object::OptimalType applies before it will create a Class field type at all -- the admission rule this
    // patch has no analogue for. See field-types/README.md 4c.
    bool hasBeenTransitionedFrom() const { return m_transitionTable.hasAnyTransition(); }

    JS_EXPORT_PRIVATE static Structure* addPropertyTransition(VM&, Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* addNewPropertyTransition(VM&, Structure*, PropertyName, unsigned attributes, PropertyOffset&, PutPropertySlot::Context = PutPropertySlot::UnknownContext, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* addPropertyTransitionToExistingStructureConcurrently(Structure*, UniquedStringImpl* uid, unsigned attributes, PropertyOffset&);
    static Structure* addPropertyTransitionToExistingStructure(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    static Structure* removeNewPropertyTransition(VM&, Structure*, PropertyName, PropertyOffset&, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* removePropertyTransition(VM&, Structure*, PropertyName, PropertyOffset&, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* removePropertyTransitionFromExistingStructure(Structure*, PropertyName, PropertyOffset&);
    static Structure* removePropertyTransitionFromExistingStructureConcurrently(Structure*, PropertyName, PropertyOffset&);
    static Structure* changePrototypeTransition(VM&, Structure*, JSValue prototype, DeferredStructureTransitionWatchpointFire&);
    static Structure* changeGlobalProxyTargetTransition(VM&, Structure*, JSGlobalObject*, DeferredStructureTransitionWatchpointFire&);
    JS_EXPORT_PRIVATE static Structure* attributeChangeTransition(VM&, Structure*, PropertyName, unsigned attributes, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* attributeChangeTransitionToExistingStructureConcurrently(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* attributeChangeTransitionToExistingStructure(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* toCacheableDictionaryTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* toUncacheableDictionaryTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    JS_EXPORT_PRIVATE static Structure* sealTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    JS_EXPORT_PRIVATE static Structure* freezeTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* preventExtensionsTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* nonPropertyTransition(VM&, Structure*, TransitionKind, DeferredStructureTransitionWatchpointFire*);
    static Structure* setBrandTransitionFromExistingStructureConcurrently(Structure*, UniquedStringImpl*);
    static Structure* setBrandTransition(VM&, Structure*, Symbol* brand, DeferredStructureTransitionWatchpointFire* = nullptr);
    JS_EXPORT_PRIVATE static Structure* becomePrototypeTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);

    JS_EXPORT_PRIVATE bool isSealed(VM&);
    JS_EXPORT_PRIVATE bool isFrozen(VM&);
    bool isStructureExtensible() const { return !didPreventExtensions(); }

    JS_EXPORT_PRIVATE Structure* flattenDictionaryStructure(VM&, JSObject*);

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    // Versions that take a func will call it after making the change but while still holding
    // the lock. The callback is not called if there is no change being made, like if you call
    // removePropertyWithoutTransition() and the property is not found.
    template<typename Func>
    PropertyOffset addPropertyWithoutTransition(VM&, PropertyName, unsigned attributes, const Func&);
    template<typename Func>
    PropertyOffset removePropertyWithoutTransition(VM&, PropertyName, const Func&);
    template<typename Func>
    PropertyOffset attributeChangeWithoutTransition(VM&, PropertyName, unsigned attributes, const Func&);
    template<typename Func>
    auto addOrReplacePropertyWithoutTransition(VM&, PropertyName, unsigned attributes, const Func&) -> decltype(auto);
    void setPrototypeWithoutTransition(VM&, JSValue prototype);
        
    bool isDictionary() const { return dictionaryKind() != NoneDictionaryKind; }
    bool isUncacheableDictionary() const { return dictionaryKind() == UncachedDictionaryKind; }
    bool isCacheableDictionary() const { return dictionaryKind() == CachedDictionaryKind; }
  
    bool prototypeQueriesAreCacheable()
    {
        return !typeInfo().prohibitsPropertyCaching();
    }
    
    bool propertyAccessesAreCacheable()
    {
        return dictionaryKind() != UncachedDictionaryKind
            && prototypeQueriesAreCacheable()
            && !(typeInfo().getOwnPropertySlotIsImpure() && !typeInfo().newImpurePropertyFiresWatchpoints());
    }

    bool propertyAccessesAreCacheableForAbsence()
    {
        // FIXME: dictionaries cannot be cached for absence, so check for dictionaries here instead
        // of at all call sites.
        return !typeInfo().getOwnPropertySlotIsImpureForPropertyAbsence();
    }

    bool needImpurePropertyWatchpoint()
    {
        return propertyAccessesAreCacheable()
            && typeInfo().getOwnPropertySlotIsImpure()
            && typeInfo().newImpurePropertyFiresWatchpoints();
    }

    bool isImmutablePrototypeExoticObject()
    {
        return typeInfo().isImmutablePrototypeExoticObject();
    }

    // We use SlowPath in GetByStatus for structures that may get new impure properties later to prevent
    // DFG from inlining property accesses since structures don't transition when a new impure property appears.
    bool takesSlowPathInDFGForImpureProperty()
    {
        return typeInfo().getOwnPropertySlotIsImpure();
    }

    bool hasNonReifiedStaticProperties() const
    {
        return typeInfo().hasStaticPropertyTable() && !staticPropertiesReified();
    }

    bool isNonExtensibleOrHasNonConfigurableProperties() const
    {
        return didPreventExtensions() || hasNonConfigurableProperties();
    }

    bool hasAnyOfBitFieldFlags(unsigned flags) const
    {
        return m_bitField & flags;
    }

    // Type accessors.
    TypeInfo typeInfo() const { return m_blob.typeInfo(m_outOfLineTypeFlags); }
    bool isObject() const { return typeInfo().isObject(); }
    const ClassInfo* classInfoForCells() const { return m_classInfo; }
    CellState typeInfoDefaultCellState() const { return m_blob.defaultCellState(); }
protected:
    // You probably want typeInfo().type()
    JSType type() { return JSCell::type(); }
    // You probably want classInfoForCell()
    const ClassInfo* classInfo() const = delete;
public:

    IndexingType indexingType() const { return m_blob.indexingModeIncludingHistory() & AllWritableArrayTypes; }
    IndexingType indexingMode() const  { return m_blob.indexingModeIncludingHistory() & AllArrayTypes; }
    Dependency fencedIndexingMode(IndexingType& indexingType)
    {
        Dependency dependency = m_blob.fencedIndexingModeIncludingHistory(indexingType);
        indexingType &= AllArrayTypes;
        return dependency;
    }
    IndexingType indexingModeIncludingHistory() const { return m_blob.indexingModeIncludingHistory(); }
        
    inline bool mayInterceptIndexedAccesses() const;
    
    inline bool holesMustForwardToPrototype(JSObject*) const;
        
    JSGlobalObject* realm() const LIFETIME_BOUND { return m_realm.get(); }

    // NOTE: This method should only be called during the creation of structures, since the realm
    // of a structure is presumed to be immutable in a bunch of places.
    void setRealm(VM&, JSGlobalObject*);

    ALWAYS_INLINE bool hasMonoProto() const
    {
        return !m_prototype.get().isEmpty();
    }
    ALWAYS_INLINE bool hasPolyProto() const
    {
        return !hasMonoProto();
    }
    ALWAYS_INLINE JSValue storedPrototype() const
    {
        ASSERT(hasMonoProto());
        return m_prototype.get();
    }
    JSValue storedPrototype(const JSObject*) const;
    JSObject* storedPrototypeObject(const JSObject*) const;
    Structure* storedPrototypeStructure(const JSObject*) const;

    JSObject* storedPrototypeObject() const;
    Structure* storedPrototypeStructure() const;
    JSValue prototypeForLookup(JSGlobalObject*) const;
    JSValue prototypeForLookup(JSGlobalObject*, JSCell* base) const;
    StructureChain* prototypeChain(VM&, JSGlobalObject*, JSObject* base) const;
    DECLARE_VISIT_CHILDREN;
    
    // A Structure is cheap to mark during GC if doing so would only add a small and bounded amount
    // to our heap footprint. For example, if the structure refers to a global object that is not
    // yet marked, then as far as we know, the decision to mark this Structure would lead to a large
    // increase in footprint because no other object refers to that global object. This method
    // returns true if all user-controlled (and hence unbounded in size) objects referenced from the
    // Structure are already marked.
    template<typename Visitor> bool isCheapDuringGC(Visitor&);
    
    // Returns true if this structure is now marked.
    template<typename Visitor> bool markIfCheap(Visitor&);
    
    bool hasRareData() const
    {
        return isRareData(m_previousOrRareData.get());
    }

    StructureRareData* rareData()
    {
        ASSERT(hasRareData());
        return static_cast<StructureRareData*>(m_previousOrRareData.get());
    }

    StructureRareData* tryRareData()
    {
        JSCell* value = m_previousOrRareData.get();
        WTF::dependentLoadLoadFence();
        if (isRareData(value))
            return static_cast<StructureRareData*>(value);
        return nullptr;
    }

    const StructureRareData* rareData() const
    {
        ASSERT(hasRareData());
        return static_cast<const StructureRareData*>(m_previousOrRareData.get());
    }

    const StructureRareData* rareDataConcurrently() const
    {
        JSCell* cell = m_previousOrRareData.get();
        if (isRareData(cell))
            return static_cast<StructureRareData*>(cell);
        return nullptr;
    }

    StructureRareData* ensureRareData(VM& vm)
    {
        if (!hasRareData())
            allocateRareData(vm);
        return rareData();
    }
    
    inline Structure* previousID() const; // Defined below
    inline bool transitivelyTransitionedFrom(Structure* structureToFind); // Defined below

    inline PropertyOffset maxOffset() const; // Defined below

    inline void setMaxOffset(VM&, PropertyOffset); // Defined below

    inline PropertyOffset transitionOffset() const; // Defined below

    inline void setTransitionOffset(VM&, PropertyOffset); // Defined below

    inline Structure* findOffsetOwner(PropertyOffset); // Defined below

    // The claim for the field this structure adds, read as one load off this cache line rather than a locked
    // hash lookup, as V8 reads descriptors->GetFieldType(i) off the map's own DescriptorArray. 16 bits cannot
    // hold a StructureID, so this is an index into FieldTypeWatchpointTable's claimed-structure array:
    //   0     nothing recorded -- go to the table
    //   1     recorded but permanently generalised -- skip, nothing to maintain
    //   2     an ancestor may own this offset -- go to the table
    //   >= 3  a live claim: index (value - 3) into the claimed-structure array
    // States 0 and 1 must stay distinguishable: collapsing them cost json-parse-inspector 13.8%, because 3.29M
    // of 6.22M property creations stopped skipping and went to the table instead.
    static constexpr uint16_t noFieldTypeEntryYet = FieldTypeClaimIndex::noEntryYet;
    static constexpr uint16_t fieldTypeEntryWithoutClaim = FieldTypeClaimIndex::entryWithoutClaim;
    static constexpr uint16_t fieldTypeOffsetWasReused = FieldTypeClaimIndex::offsetWasReused;
    uint16_t fieldTypeClaimIndex() const { return m_fieldTypeClaimIndex; }
    void setFieldTypeClaimIndex(uint16_t index) { m_fieldTypeClaimIndex = index; }

    static unsigned outOfLineCapacity(PropertyOffset maxOffset)
    {
        unsigned outOfLineSize = Structure::outOfLineSize(maxOffset);

        // This algorithm completely determines the out-of-line property storage growth algorithm.
        // The JSObject code will only trigger a resize if the value returned by this algorithm
        // changed between the new and old structure. So, it's important to keep this simple because
        // it's on a fast path.
        
        if (!outOfLineSize)
            return 0;

        if (outOfLineSize <= initialOutOfLineCapacity)
            return initialOutOfLineCapacity;

        ASSERT(outOfLineSize > initialOutOfLineCapacity);
        static_assert(outOfLineGrowthFactor == 2);
        return roundUpToPowerOfTwo(outOfLineSize);
    }
    
    static unsigned outOfLineSize(PropertyOffset maxOffset)
    {
        return numberOfOutOfLineSlotsForMaxOffset(maxOffset);
    }

    unsigned outOfLineCapacity() const
    {
        return outOfLineCapacity(maxOffset());
    }
    unsigned outOfLineSize() const
    {
        return outOfLineSize(maxOffset());
    }
    bool hasInlineStorage() const
    {
        return !!m_inlineCapacity;
    }
    unsigned inlineCapacity() const
    {
        return m_inlineCapacity;
    }
    unsigned inlineSize() const
    {
        return std::min<unsigned>(maxOffset() + 1, m_inlineCapacity);
    }
    unsigned totalStorageCapacity() const
    {
        ASSERT(structure()->classInfoForCells() == info());
        return outOfLineCapacity() + inlineCapacity();
    }

    bool isValidOffset(PropertyOffset offset) const
    {
        return JSC::isValidOffset(offset)
            && offset <= maxOffset()
            && (offset < m_inlineCapacity || offset >= firstOutOfLineOffset);
    }

    bool hijacksIndexingHeader() const
    {
        return isTypedView(m_blob.type());
    }
    
    bool couldHaveIndexingHeader() const
    {
        return hasIndexedProperties(indexingType())
            || hijacksIndexingHeader();
    }
    
    bool hasIndexingHeader(const JSCell*) const;    
    bool masqueradesAsUndefined(JSGlobalObject* lexicalGlobalObject)
    {
        return typeInfo().masqueradesAsUndefined() && realm() == lexicalGlobalObject;
    }

    PropertyOffset get(VM&, PropertyName);
    PropertyOffset get(VM&, PropertyName, unsigned& attributes);

    inline bool canPerformFastPropertyEnumerationCommon() const; // Defined below

    inline bool canPerformFastPropertyEnumeration() const; // Defined below

    // This is a somewhat internalish method. It will call your functor while possibly holding the
    // Structure's lock. There is no guarantee whether the lock is held or not in any particular
    // call. So, you have to assume the worst. Also, the functor returns true if it wishes for you
    // to continue or false if it's done.
    template<typename Functor>
    void forEachPropertyConcurrently(const Functor&);

    template<typename Functor>
    void forEachProperty(VM&, const Functor&);

    IGNORE_RETURN_TYPE_WARNINGS_BEGIN
    inline PropertyOffset get(VM&, Concurrency, UniquedStringImpl* uid, unsigned& attributes); // Defined in StructureInlines.h
    IGNORE_RETURN_TYPE_WARNINGS_END

    IGNORE_RETURN_TYPE_WARNINGS_BEGIN
    inline PropertyOffset get(VM&, Concurrency, UniquedStringImpl* uid); // Defined in StructureInlines.h
    IGNORE_RETURN_TYPE_WARNINGS_END

    inline PropertyOffset getConcurrently(UniquedStringImpl* uid); // Defined in StructureInlines.h
    PropertyOffset getConcurrently(UniquedStringImpl* uid, unsigned& attributes);
    
    Vector<PropertyTableEntry> getPropertiesConcurrently();
    
    void setHasAnyKindOfGetterSetterPropertiesWithProtoCheck(bool is__proto__)
    {
        setHasAnyKindOfGetterSetterProperties(true);
        if (!is__proto__)
            setHasReadOnlyOrGetterSetterPropertiesExcludingProto(true);
    }
    
    void setContainsReadOnlyProperties() { setHasReadOnlyOrGetterSetterPropertiesExcludingProto(true); }
    
    void setCachedPropertyNameEnumerator(VM&, JSPropertyNameEnumerator*, StructureChain*);
    JSPropertyNameEnumerator* NODELETE cachedPropertyNameEnumerator() const;
    uintptr_t NODELETE cachedPropertyNameEnumeratorAndFlag() const;
    bool NODELETE canCachePropertyNameEnumerator(VM&) const;
    bool NODELETE canAccessPropertiesQuicklyForEnumeration() const;

    inline JSCellButterfly* cachedPropertyNames(CachedPropertyNamesKind kind) const; // Defined in StructureInlines.h
    inline JSCellButterfly* cachedPropertyNamesIgnoringSentinel(CachedPropertyNamesKind kind) const; // Defined in StructureInlines.h
    void setCachedPropertyNames(VM&, CachedPropertyNamesKind, JSCellButterfly*);
    bool canCacheOwnPropertyNames() const
    {
        if (isDictionary())
            return false;
        if (hasIndexedProperties(indexingType()))
            return false;
        if (typeInfo().overridesAnyFormOfGetOwnPropertyNames())
            return false;
        return true;
    }

    void getPropertyNamesFromStructure(VM&, PropertyNameArrayBuilder&, DontEnumPropertiesMode);

    inline JSValue cachedSpecialProperty(CachedSpecialPropertyKey key); // Defined in StructureInlines.h
    void cacheSpecialProperty(JSGlobalObject*, VM&, JSValue, CachedSpecialPropertyKey, const PropertySlot&);

    inline JSString* defaultToPrimitiveFastAndNonObservable(VM&);

    static constexpr ptrdiff_t prototypeOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_prototype);
    }

    static constexpr ptrdiff_t realmOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_realm);
    }

    static constexpr ptrdiff_t classInfoOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_classInfo);
    }

    static constexpr ptrdiff_t outOfLineTypeFlagsOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_outOfLineTypeFlags);
    }

    static constexpr ptrdiff_t indexingModeIncludingHistoryOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_blob) + TypeInfoBlob::indexingModeIncludingHistoryOffset();
    }
    
    static constexpr ptrdiff_t propertyTableUnsafeOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_propertyTableUnsafe);
    }

    static constexpr ptrdiff_t inlineCapacityOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_inlineCapacity);
    }

    static constexpr ptrdiff_t previousOrRareDataOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_previousOrRareData);
    }

    static constexpr ptrdiff_t bitFieldOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_bitField);
    }

    static constexpr ptrdiff_t propertyHashOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_propertyHash);
    }

    static constexpr ptrdiff_t seenPropertiesOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_seenProperties) + SeenProperties::offsetOfBits();
    }

    static Structure* createStructure(VM&);
        
    bool transitionWatchpointSetHasBeenInvalidated() const
    {
        return m_transitionWatchpointSet.hasBeenInvalidated();
    }
        
    bool transitionWatchpointSetIsStillValid() const
    {
        return m_transitionWatchpointSet.isStillValid();
    }
    
    bool dfgMayWatchIfPossible() const
    {
        // FIXME: We would like to not watch things that are unprofitable to watch, like
        // dictionaries. Unfortunately, we can't do such things: a dictionary could get flattened,
        // in which case it will start to appear watchable and so the DFG will think that it is
        // watching it. We should come up with a comprehensive story for not watching things that
        // aren't profitable to watch.
        // https://bugs.webkit.org/show_bug.cgi?id=133625
        
        // - We don't watch Structures that either decided not to be watched, or whose predecessors
        //   decided not to be watched. This happens when a transition is fired while being watched.
        if (transitionWatchpointIsLikelyToBeFired())
            return false;

        // - Don't watch Structures that had been dictionaries.
        if (hasBeenDictionary())
            return false;
        
        return true;
    }
    
    bool dfgMayWatch() const
    {
        return dfgMayWatchIfPossible() && transitionWatchpointSetIsStillValid();
    }

    bool propertyNameEnumeratorMayWatch() const
    {
        return dfgMayWatch() && !hasPolyProto();
    }
        
    inline void addTransitionWatchpoint(Watchpoint* watchpoint) const; // Defined in StructureInlinesLight.h
    
    void NODELETE didTransitionFromThisStructureWithoutFiringWatchpoint() const;
    void fireStructureTransitionWatchpoint(DeferredStructureTransitionWatchpointFire*) const;

    InlineWatchpointSet& transitionWatchpointSet() const
    {
        return m_transitionWatchpointSet;
    }
    
    JS_EXPORT_PRIVATE WatchpointSet* ensurePropertyReplacementWatchpointSet(VM&, PropertyOffset);
    inline void startWatchingPropertyForReplacements(VM& vm, PropertyOffset offset); // Defined in StructureInlines.h
    void startWatchingPropertyForReplacements(VM&, PropertyName);
    WatchpointSet* propertyReplacementWatchpointSet(PropertyOffset);
    WatchpointSet* firePropertyReplacementWatchpointSet(VM&, PropertyOffset, const char* reason);

    void didReplaceProperty(PropertyOffset offset)
    {
        if (!isWatchingReplacement()) [[likely]]
            return;
        didReplacePropertySlow(offset);
    }
    void didCachePropertyReplacement(VM&, PropertyOffset);
    
    inline void startWatchingInternalPropertiesIfNecessary(VM& vm); // Defined in StructureInlines.h
    
    Ref<StructureShape> toStructureShape(JSValue, bool& sawPolyProtoStructure);
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dumpBrief(PrintStream&, const ASCIICString&) const;
    
    static void dumpContextHeader(PrintStream&);
    
    ConcurrentJSLock& lock() LIFETIME_BOUND { return m_lock; }

    unsigned propertyHash() const { return m_propertyHash; }
    SeenProperties seenProperties() const { return m_seenProperties; }

    static bool shouldConvertToPolyProto(const Structure* a, const Structure* b);

    UniquedStringImpl* transitionPropertyName() const { return m_transitionPropertyName.get(); }

    struct PropertyHashEntry {
        const HashTable* table;
        const HashTableValue* value;
    };
    std::optional<PropertyHashEntry> findPropertyHashEntry(PropertyName) const;
    
    DECLARE_EXPORT_INFO;

private:
    JS_EXPORT_PRIVATE void didReplacePropertySlow(PropertyOffset);

    typedef enum {
        NoneDictionaryKind = 0,
        CachedDictionaryKind = 1,
        UncachedDictionaryKind = 2
    } DictionaryKind;

public:
    enum class DefinitelyNonThenableState : uint8_t {
        NotComputed = 0,
        NonThenable = 1, // Cached `true`. Sound only while the realm's promiseThenWatchpointSet is intact.
        MaybeThenable = 2, // Cached `false`. Always safe (a stale `false` only loses the optimization).
        Uncacheable = 3, // Prototype chain isn't covered by the watchpoint; always recompute.
    };

#define DEFINE_BITFIELD(type, lowerName, upperName, width, offset) \
    static constexpr uint32_t s_##lowerName##Shift = offset;\
    static constexpr uint32_t s_##lowerName##Mask = ((1 << (width - 1)) | ((1 << (width - 1)) - 1));\
    static constexpr uint32_t s_##lowerName##Bits = s_##lowerName##Mask << s_##lowerName##Shift;\
    static constexpr uint32_t s_bitWidthOf##upperName = width;\
    type lowerName() const { return static_cast<type>((m_bitField >> offset) & s_##lowerName##Mask); }\
    void set##upperName(type newValue) \
    {\
        m_bitField &= ~(s_##lowerName##Mask << offset);\
        m_bitField |= (static_cast<uint32_t>(newValue) & s_##lowerName##Mask) << offset;\
    }

    DEFINE_BITFIELD(DictionaryKind, dictionaryKind, DictionaryKind, 2, 0);
    DEFINE_BITFIELD(bool, isPinnedPropertyTable, IsPinnedPropertyTable, 1, 2);
    DEFINE_BITFIELD(bool, hasAnyKindOfGetterSetterProperties, HasAnyKindOfGetterSetterProperties, 1, 3);
    DEFINE_BITFIELD(bool, hasReadOnlyOrGetterSetterPropertiesExcludingProto, HasReadOnlyOrGetterSetterPropertiesExcludingProto, 1, 4);
    DEFINE_BITFIELD(bool, isQuickPropertyAccessAllowedForEnumeration, IsQuickPropertyAccessAllowedForEnumeration, 1, 5);
    DEFINE_BITFIELD(bool, hasNonEnumerableProperties, HasNonEnumerableProperties, 1, 6);
    DEFINE_BITFIELD(bool, hasSpecialProperties, HasSpecialProperties, 1, 7);
    DEFINE_BITFIELD(DefinitelyNonThenableState, definitelyNonThenableState, DefinitelyNonThenableState, 2, 8); // This flag can be flipped on the main thread at any timing.
    DEFINE_BITFIELD(TransitionKind, transitionKind, TransitionKind, 5, 13);
    DEFINE_BITFIELD(bool, isWatchingReplacement, IsWatchingReplacement, 1, 18); // This flag can be fliped on the main thread at any timing.
    DEFINE_BITFIELD(bool, mayBePrototype, MayBePrototype, 1, 19);
    DEFINE_BITFIELD(bool, didPreventExtensions, DidPreventExtensions, 1, 20);
    DEFINE_BITFIELD(bool, didTransition, DidTransition, 1, 21);
    DEFINE_BITFIELD(bool, staticPropertiesReified, StaticPropertiesReified, 1, 22);
    DEFINE_BITFIELD(bool, hasBeenFlattenedBefore, HasBeenFlattenedBefore, 1, 23);
    DEFINE_BITFIELD(bool, didWatchInternalProperties, DidWatchInternalProperties, 1, 24);
    DEFINE_BITFIELD(bool, transitionWatchpointIsLikelyToBeFired, TransitionWatchpointIsLikelyToBeFired, 1, 25);
    DEFINE_BITFIELD(bool, hasBeenDictionary, HasBeenDictionary, 1, 26);
    DEFINE_BITFIELD(bool, protectPropertyTableWhileTransitioning, ProtectPropertyTableWhileTransitioning, 1, 27);
    DEFINE_BITFIELD(bool, hasUnderscoreProtoPropertyExcludingOriginalProto, HasUnderscoreProtoPropertyExcludingOriginalProto, 1, 28);
    DEFINE_BITFIELD(bool, hasNonConfigurableProperties, HasNonConfigurableProperties, 1, 29);
    DEFINE_BITFIELD(bool, hasNonConfigurableReadOnlyOrGetterSetterProperties, HasNonConfigurableReadOnlyOrGetterSetterProperties, 1, 30);

    enum class StructureVariant : uint8_t {
        Normal,
        Branded,
        WebAssemblyGC,
    };

    StructureVariant variant() const { return m_structureVariant; }
    bool isBrandedStructure() { return variant() == StructureVariant::Branded; }

    static_assert(s_bitWidthOfTransitionKind <= sizeof(TransitionKind) * 8);

    static bool bitFieldFlagsCantBeChangedWithoutTransition(unsigned flags)
    {
        return flags == (flags & (
            s_didPreventExtensionsBits
            | s_isQuickPropertyAccessAllowedForEnumerationBits
            | s_hasNonEnumerablePropertiesBits
            | s_hasSpecialPropertiesBits
            | s_hasAnyKindOfGetterSetterPropertiesBits
            | s_hasReadOnlyOrGetterSetterPropertiesExcludingProtoBits
            | s_hasUnderscoreProtoPropertyExcludingOriginalProtoBits
            | s_hasNonConfigurablePropertiesBits
            | s_hasNonConfigurableReadOnlyOrGetterSetterPropertiesBits
        ));
    }

    TransitionPropertyAttributes transitionPropertyAttributes() const { return m_transitionPropertyAttributes; }
    void setTransitionPropertyAttributes(TransitionPropertyAttributes transitionPropertyAttributes) { m_transitionPropertyAttributes = transitionPropertyAttributes; }

    int transitionCountEstimate() const
    {
        // Since the number of transitions is often the same as the last offset (except if there are deletes)
        // we keep the size of Structure down by not storing both.
        return numberOfSlotsForMaxOffset(maxOffset(), m_inlineCapacity);
    }

    void reconcileWeakReferencesAtGCEnd(VM&, CollectionScope);

protected:
    Structure(VM&, StructureVariant, Structure* previous); // Branded/Normal only
    Structure(VM&, StructureVariant, JSGlobalObject*, const TypeInfo&, const ClassInfo*); // WebAssemblyGC only

private:
    friend class LLIntOffsetsExtractor;

    JS_EXPORT_PRIVATE Structure(VM&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType, unsigned inlineCapacity);
    Structure(VM&, CreatingEarlyCellTag);

    static Structure* create(VM&, Structure*, DeferredStructureTransitionWatchpointFire*);

    static Structure* addPropertyTransitionToExistingStructureImpl(Structure*, UniquedStringImpl* uid, unsigned attributes, PropertyOffset&);
    ALWAYS_INLINE static Structure* attributeChangeTransitionToExistingStructureImpl(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    static Structure* removePropertyTransitionFromExistingStructureImpl(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    static Structure* setBrandTransitionFromExistingStructureImpl(Structure*, UniquedStringImpl*);

    JS_EXPORT_PRIVATE static Structure* nonPropertyTransitionSlow(VM&, Structure*, TransitionKind, DeferredStructureTransitionWatchpointFire*);

    // This function does the both didTransitionFromThisStructureWithoutFiringWatchpoint and fireStructureTransitionWatchpoint.
    void didTransitionFromThisStructure(DeferredStructureTransitionWatchpointFire*) const;

    // This will return the structure that has a usable property table, that property table,
    // and the list of structures that we visited before we got to it. If it returns a
    // non-null structure, it will also lock the structure that it returns; it is your job
    // to unlock it.
    bool findStructuresAndMapForMaterialization(Vector<Structure*, 8>& structures, Structure*& structure, PropertyTable*&) WTF_ACQUIRES_LOCK_IF(true, structure->m_lock);
    
    static Structure* toDictionaryTransition(VM&, Structure*, DictionaryKind, DeferredStructureTransitionWatchpointFire* = nullptr);

    enum class ShouldPin : bool { No, Yes };
    template<ShouldPin, typename Func>
    PropertyOffset add(VM&, PropertyName, unsigned attributes, const Func&);
    PropertyOffset add(VM&, PropertyName, unsigned attributes);
    template<ShouldPin, typename Func>
    PropertyOffset remove(VM&, PropertyName, const Func&);
    PropertyOffset remove(VM&, PropertyName);
    template<ShouldPin, typename Func>
    PropertyOffset attributeChange(VM&, PropertyName, unsigned attributes, const Func&);
    PropertyOffset attributeChange(VM&, PropertyName, unsigned attributes);

#if ASSERT_ENABLED
    JS_EXPORT_PRIVATE void checkConsistency();
#else
    ALWAYS_INLINE void checkConsistency() { }
#endif

    // This may grab the lock, or not. Do not call when holding the Structure's lock.
    PropertyTable* ensurePropertyTableIfNotEmpty(VM& vm)
    {
        if (PropertyTable* result = m_propertyTableUnsafe.get())
            return result;
        if (!previousID())
            return nullptr;
        return materializePropertyTable(vm);
    }
    
    // This may grab the lock, or not. Do not call when holding the Structure's lock.
    PropertyTable* ensurePropertyTable(VM& vm)
    {
        if (PropertyTable* result = m_propertyTableUnsafe.get())
            return result;
        return materializePropertyTable(vm);
    }
    
    PropertyTable* propertyTableOrNull() const
    {
        return m_propertyTableUnsafe.get();
    }
    
    // This will grab the lock. Do not call when holding the Structure's lock.
    JS_EXPORT_PRIVATE PropertyTable* materializePropertyTable(VM&, bool setPropertyTable = true);
    
    void setPropertyTable(VM& vm, PropertyTable* table);
    
    PropertyTable* takePropertyTableOrCloneIfPinned(VM&);
    PropertyTable* copyPropertyTableForPinning(VM&);

    void setPreviousID(VM&, Structure*);

    void clearPreviousID()
    {
        if (hasRareData())
            rareData()->clearPreviousID();
        else
            m_previousOrRareData.clear();
    }

    bool isValid(JSGlobalObject*, StructureChain* cachedPrototypeChain, JSObject* base) const;

    // You have to hold the structure lock to do these.
    // Keep them inlined function since they are used in the critical path of Dictionary JSObject modification.
    void pin(const AbstractLocker&, VM&, PropertyTable*);
    void pinForCaching(const AbstractLocker&, VM&, PropertyTable*);
    
    static bool isRareData(JSCell* cell)
    {
        return cell && cell->type() != StructureType;
    }

    JS_EXPORT_PRIVATE void allocateRareData(VM&);

    template<typename DetailsFunc>
    void checkOffsetConsistency(PropertyTable*, const DetailsFunc&) const;
    void checkOffsetConsistency() const;

    void startWatchingInternalProperties(VM&);

    inline void clearCachedPrototypeChain(); // Defined in StructureInlines.h

    bool NODELETE holesMustForwardToPrototypeSlow(JSObject*) const;

    // These need to be properly aligned at the beginning of the 'Structure'
    // part of the object.
    TypeInfoBlob m_blob;
    TypeInfo::OutOfLineTypeFlags m_outOfLineTypeFlags;

    uint8_t m_inlineCapacity;

    ConcurrentJSLock m_lock;

    uint32_t m_bitField;
    TransitionPropertyAttributes m_transitionPropertyAttributes { 0 };

    // FIXME: We should probably have a brandedStructureStructure/webAssemblyGCStructureStructure instead of this.
    StructureVariant m_structureVariant { StructureVariant::Normal };

    uint16_t m_transitionOffset;
    uint16_t m_maxOffset;

    // Sixteen bits and in this exact slot, deliberately: this consumes padding Structure already had at +26,
    // whereas a 32-bit field holding the StructureID had to follow m_propertyHash, pushing the 8-byte-aligned
    // m_seenProperties from +32 to +40 and growing every Structure from 112 to 120 bytes.
    uint16_t m_fieldTypeClaimIndex { 0 };

    uint32_t m_propertyHash;


    SeenProperties m_seenProperties;


    WriteBarrier<JSGlobalObject> m_realm;
    WriteBarrier<Unknown> m_prototype;
    mutable WriteBarrier<StructureChain> m_cachedPrototypeChain;

    WriteBarrier<JSCell> m_previousOrRareData;

    CompactRefPtr<UniquedStringImpl> m_transitionPropertyName;

    const ClassInfo* m_classInfo;

    StructureTransitionTable m_transitionTable;

    // Should be accessed through ensurePropertyTable(). During GC, it may be set to 0 by another thread.
    // During a Heap Snapshot GC we avoid clearing the table so it is safe to use.
    WriteBarrier<PropertyTable> m_propertyTableUnsafe;

    mutable InlineWatchpointSet m_transitionWatchpointSet;

    static_assert(firstOutOfLineOffset < 256);

    friend class VMInspector;
    friend class JSDollarVMHelper;
    friend class Integrity::Analyzer;
};

// If this fires, the field-type claim index has stopped fitting in Structure's pre-existing padding and
// every Structure in the process just grew; re-examine the layout before shipping that.
static_assert(sizeof(Structure) == 112, "Structure must not grow: the field-type claim index belongs in existing padding");
JS_EXPORT_PRIVATE void dumpTransitionKind(PrintStream&, TransitionKind);
MAKE_PRINT_ADAPTOR(TransitionKindDump, TransitionKind, dumpTransitionKind);

// Defined here rather than in JSCell.h because it needs Structure to be complete.
inline const ClassInfo* JSCell::classInfo() const
{
    // If the mutator is currently sweeping, then accessing the structure is not safe since the
    // structure may have been swept already (and we're probably being called from this object's
    // destructor). This can only be verified for the mutator thread since other threads might be
    // querying JSCells that are not being swept by the mutator.
    // validateIsNotSweeping() is out-of-line to avoid pulling vm() into this header.
    ASSERT(validateIsNotSweeping());
    return structure()->classInfoForCells();
}

inline bool JSCell::inherits(const ClassInfo* info) const
{
    return classInfo()->isSubClassOf(info);
}

template<typename Target>
inline bool JSCell::inherits() const
{
    return JSCastingHelpers::inherits<Target>(this);
}

inline Structure* Structure::previousID() const
{
    ASSERT(structure()->classInfoForCells() == info());
    // This is so written because it's used concurrently. We only load from m_previousOrRareData
    // once, and this load is guaranteed atomic.
    JSCell* cell = m_previousOrRareData.get();
    if (isRareData(cell))
        return static_cast<StructureRareData*>(cell)->previousID();
    return static_cast<Structure*>(cell);
}

inline bool Structure::transitivelyTransitionedFrom(Structure* structureToFind)
{
    for (Structure* current = this; current; current = current->previousID()) {
        if (current == structureToFind)
            return true;
    }
    return false;
}

// Walks up the transition chain for the structure that ADDED `offset`, which is the structure a field-type
// record is keyed on: every descendant inherits the offset, so anything keyed on an offset must be keyed on the
// introducing structure or it misses accesses made through descendants. Safe from a compiler thread.
//
// Returns null both when there is no owner and when the chain was SEVERED -- Structure::pin calls
// clearPreviousID and every dictionary conversion pins -- so a caller must treat null as "unknown, be
// conservative" rather than "no record". There is deliberately no assertion, because the obvious
// `ASSERT(!isPinnedPropertyTable() || !previousID())` is false: pinForCaching sets the same bit and leaves
// previousID() intact, so no flag tells you whether the chain survived. Dictionaries are rejected outright:
// one adds many properties to a single structure, so no transitionOffset identifies a field.
inline Structure* Structure::findOffsetOwner(PropertyOffset offset)
{
    if (offset == invalidOffset || isDictionary())
        return nullptr;

    for (Structure* current = this; current; current = current->previousID()) {
        if (current->transitionKind() == TransitionKind::PropertyAddition && current->transitionOffset() == offset)
            return current;

    }
    return nullptr;
}

inline PropertyOffset Structure::maxOffset() const
{
    uint16_t maxOffset = m_maxOffset;
    if (maxOffset == shortInvalidOffset)
        return invalidOffset;
    if (maxOffset == useRareDataFlag)
        return rareData()->m_maxOffset;
    return maxOffset;
}

inline void Structure::setMaxOffset(VM& vm, PropertyOffset offset)
{
    if (offset == invalidOffset)
        m_maxOffset = shortInvalidOffset;
    else if (offset < useRareDataFlag && offset < shortInvalidOffset)
        m_maxOffset = offset;
    else if (m_maxOffset == useRareDataFlag)
        rareData()->m_maxOffset = offset;
    else {
        ensureRareData(vm)->m_maxOffset = offset;
        WTF::storeStoreFence();
        m_maxOffset = useRareDataFlag;
    }
}

inline PropertyOffset Structure::transitionOffset() const
{
    uint16_t transitionOffset = m_transitionOffset;
    if (transitionOffset == shortInvalidOffset)
        return invalidOffset;
    if (transitionOffset == useRareDataFlag)
        return rareData()->m_transitionOffset;
    return transitionOffset;
}

inline void Structure::setTransitionOffset(VM& vm, PropertyOffset offset)
{
    if (offset == invalidOffset)
        m_transitionOffset = shortInvalidOffset;
    else if (offset < useRareDataFlag && offset < shortInvalidOffset)
        m_transitionOffset = offset;
    else if (m_transitionOffset == useRareDataFlag)
        rareData()->m_transitionOffset = offset;
    else {
        ensureRareData(vm)->m_transitionOffset = offset;
        WTF::storeStoreFence();
        m_transitionOffset = useRareDataFlag;
    }
}

inline bool Structure::canPerformFastPropertyEnumerationCommon() const
{
    if (typeInfo().overridesGetOwnPropertySlot())
        return false;
    if (typeInfo().overridesAnyFormOfGetOwnPropertyNames())
        return false;
    if (hasAnyKindOfGetterSetterProperties())
        return false;
    if (isUncacheableDictionary())
        return false;
    // Cannot perform fast [[Put]] to |target| if the property names of the |source| contain "__proto__".
    if (hasUnderscoreProtoPropertyExcludingOriginalProto())
        return false;
    return true;
}

inline bool Structure::canPerformFastPropertyEnumeration() const
{
    if (!canPerformFastPropertyEnumerationCommon())
        return false;
    // FIXME: Indexed properties can be handled.
    // https://bugs.webkit.org/show_bug.cgi?id=185358
    if (hasIndexedProperties(indexingType()))
        return false;
    return true;
}

} // namespace JSC
