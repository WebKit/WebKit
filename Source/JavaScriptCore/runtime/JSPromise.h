/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/Microtask.h>
#include <wtf/CompactPointerTuple.h>

namespace JSC {

class JSPromiseConstructor;
class JSPromiseReaction;

// JSPromise stores its state in two machine words:
//   m_packed: CompactPointerTuple<JSCell*, uint16_t>
//       Lower 48 bits: payload cell pointer (may be null)
//       Upper 16 bits: flags (see Flags layout below)
//   m_slot: JSValue (as WriteBarrier<Unknown>)
class JSPromise : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.promiseSpace();
    }

    JS_EXPORT_PRIVATE static JSPromise* create(VM&, Structure*);
    static JSPromise* createWithInitialValues(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static size_t allocationSize(Checked<size_t> inlineCapacity)
    {
        ASSERT_UNUSED(inlineCapacity, !inlineCapacity);
        return sizeof(JSPromise);
    }

    DECLARE_EXPORT_INFO;

    // Flags layout (upper 16 bits of m_packed):
    //   bits 0-1:   Status
    //   bit 2:      isHandled
    //   bit 3:      isFirstResolvingFunctionCalled
    //   bits 4-5:   InlineReactionKind (4 values, 2 bits)
    //   bits 6-13:  InternalMicrotask (only when kind == InternalMicrotask)
    //   bits 14-15: reserved

    enum class Status : uint16_t {
        Pending = 0, // Making this as 0, so that, we can change the status from Pending to others without masking.
        Fulfilled = 1,
        Rejected = 2,
    };

    enum class InlineReactionKind : uint8_t {
        None = 0,
        InternalMicrotask = 1,
        FulfillHandler = 2,
        RejectHandler = 3,
    };

    static constexpr uint16_t stateMask                          = 0b0000000000000011;
    static constexpr uint16_t isHandledFlag                      = 0b0000000000000100;
    static constexpr uint16_t isFirstResolvingFunctionCalledFlag = 0b0000000000001000;
    static constexpr uint16_t inlineReactionKindMask             = 0b0000000000110000;
    static constexpr uint16_t inlineReactionMicrotaskMask        = 0b0011111111000000;
    static constexpr unsigned inlineReactionKindShift = 4;
    static constexpr unsigned inlineReactionMicrotaskShift = 6;

    static constexpr ptrdiff_t offsetOfPacked() { return OBJECT_OFFSETOF(JSPromise, m_packed); }
    static constexpr ptrdiff_t offsetOfSlot() { return OBJECT_OFFSETOF(JSPromise, m_slot); }

    inline uint16_t flags() const { return m_packed.type(); }

    inline Status status() const { return static_cast<Status>(flags() & stateMask); }
    inline bool isHandled() const { return flags() & isHandledFlag; }

    // Value accessors — meaningful only after settlement.
    inline JSValue settlementValue() const
    {
        ASSERT(status() != Status::Pending);
        return m_slot.get();
    }
    inline JSValue result() const
    {
        if (status() == Status::Pending)
            return jsUndefined();
        return m_slot.get();
    }

    JSValue asyncStackTraceContext() const;

    JS_EXPORT_PRIVATE static JSPromise* resolvedPromise(JSGlobalObject*, JSValue);
    JS_EXPORT_PRIVATE static JSPromise* rejectedPromise(JSGlobalObject*, JSValue);

    JS_EXPORT_PRIVATE void resolve(JSGlobalObject*, VM&, JSValue);
    JS_EXPORT_PRIVATE void reject(VM&, JSValue);
    JS_EXPORT_PRIVATE void fulfill(VM&, JSValue);
    // Pipes its settlement to this promise via internal microtask. Otherwise directly
    // fulfills. Never triggers user-observable behavior.
    JS_EXPORT_PRIVATE void pipeFrom(VM&, JSPromise* from);
    JS_EXPORT_PRIVATE void rejectAsHandled(VM&, JSValue);
    JS_EXPORT_PRIVATE void reject(VM&, Exception*);
    JS_EXPORT_PRIVATE void rejectAsHandled(VM&, Exception*);
    JS_EXPORT_PRIVATE void performPromiseThenExported(VM&, JSGlobalObject*, JSValue onFulfilled, JSValue onRejected, JSValue);

    JS_EXPORT_PRIVATE JSPromise* rejectWithCaughtException(VM&, ThrowScope&);

    // https://webidl.spec.whatwg.org/#mark-a-promise-as-handled
    void markAsHandled() { m_packed.setType(flags() | isHandledFlag); }

    struct DeferredData {
        WTF_FORBID_HEAP_ALLOCATION;
    public:
        JSPromise* promise { nullptr };
        JSFunction* resolve { nullptr };
        JSFunction* reject { nullptr };
    };
    static DeferredData createDeferredData(JSGlobalObject*, JSPromiseConstructor*);
    JS_EXPORT_PRIVATE static JSValue createNewPromiseCapability(JSGlobalObject*, JSValue constructor);

    DECLARE_VISIT_CHILDREN;

    // This is abstract operations defined in the spec.
    void performPromiseThen(VM&, JSGlobalObject*, JSValue onFulfilled, JSValue onRejected, JSValue);
    void rejectPromise(VM&, JSValue);
    void fulfillPromise(VM&, JSValue);
    void resolvePromise(JSGlobalObject*, VM&, JSValue);

    static void resolveWithInternalMicrotaskForAsyncAwait(JSGlobalObject*, VM&, JSValue resolution, InternalMicrotask, JSValue context);
    static void resolveWithInternalMicrotask(JSGlobalObject*, VM&, JSValue resolution, InternalMicrotask, JSValue context);
    static void rejectWithInternalMicrotask(VM&, JSGlobalObject*, JSValue argument, InternalMicrotask, JSValue context);
    static void fulfillWithInternalMicrotask(VM&, JSGlobalObject*, JSValue argument, InternalMicrotask, JSValue context);

    void performPromiseThenWithInternalMicrotask(VM&, InternalMicrotask, JSCell*, JSValue context);

    bool isThenFastAndNonObservable();

    std::tuple<JSFunction*, JSFunction*> createResolvingFunctions(VM&, JSGlobalObject*);
    std::tuple<JSFunction*, JSFunction*> createFirstResolvingFunctions(VM&, JSGlobalObject*);
    JSFunction* createFirstResolveFunction(VM&, JSGlobalObject*);
    JSFunction* createFirstRejectFunction(VM&, JSGlobalObject*);
    static std::tuple<JSFunction*, JSFunction*> createResolvingFunctionsWithInternalMicrotask(VM&, JSGlobalObject*, InternalMicrotask, JSValue context);
    static std::tuple<JSObject*, JSObject*, JSObject*> newPromiseCapability(JSGlobalObject*, JSValue constructor);
    static JSValue createPromiseCapability(VM&, JSGlobalObject*, JSObject* promise, JSObject* resolve, JSObject* reject);
    static JSObject* promiseResolve(JSGlobalObject*, JSObject* constructor, JSValue);
    static JSObject* promiseReject(JSGlobalObject*, JSObject* constructor, JSValue);

    JS_EXPORT_PRIVATE JSObject* then(JSGlobalObject*, JSValue onFulfilled, JSValue onRejected);

protected:
    JSPromise(VM&, Structure*);

    DECLARE_DEFAULT_FINISH_CREATION;

private:
    inline bool isFirstResolvingFunctionCalled() const { return flags() & isFirstResolvingFunctionCalledFlag; }

    inline InlineReactionKind inlineReactionKind() const
    {
        return static_cast<InlineReactionKind>((flags() & inlineReactionKindMask) >> inlineReactionKindShift);
    }
    inline bool hasInlineReaction() const { return inlineReactionKind() != InlineReactionKind::None; }
    inline bool hasInlineHandlerReaction() const
    {
        auto kind = inlineReactionKind();
        return kind == InlineReactionKind::FulfillHandler || kind == InlineReactionKind::RejectHandler;
    }

    // Pending-state accessors. The caller is responsible for checking state.
    inline JSValue inlineReactionContext() const
    {
        ASSERT(inlineReactionKind() == InlineReactionKind::InternalMicrotask);
        return m_slot.get();
    }
    inline JSValue inlineHandlerHandler() const
    {
        ASSERT(hasInlineHandlerReaction());
        return m_slot.get();
    }
    inline JSPromise* inlineHandlerResultPromise() const
    {
        ASSERT(hasInlineHandlerReaction());
        return uncheckedDowncast<JSPromise>(payloadCell());
    }
    inline JSCell* payloadCell() const { return m_packed.pointer(); }

    void setFlags(uint16_t newFlags) { m_packed.setType(newFlags); }
    void setPackedCell(VM& vm, uint16_t newFlags, JSCell* cell)
    {
        m_packed = CompactPointerTuple<JSCell*, uint16_t> { cell, newFlags };
        if (cell)
            vm.writeBarrier(this, cell);
    }
    void setSlot(VM& vm, JSValue value) { m_slot.set(vm, this, value); }
    void clearSlot() { m_slot.clear(); }

    void setInlineMicrotaskReaction(VM&, InternalMicrotask, JSCell*, JSValue context);
    void setInlineHandlerReaction(VM&, InlineReactionKind, JSPromise*, JSValue handler);
    JSPromiseReaction* spillInlineReaction(VM&);
    JSPromiseReaction* reactionHead(VM&);
    void settleInlineInternalMicrotask(VM&, JSGlobalObject*, Status, JSValue argument, uint16_t flags);
    void settleInlineHandler(VM&, JSGlobalObject*, Status, JSValue argument, uint16_t flags);
    static void triggerPromiseReactions(VM&, JSGlobalObject*, JSPromise::Status, JSPromiseReaction* head, JSValue argument);

    InternalMicrotask inlineReactionMicrotask() const
    {
        ASSERT(inlineReactionKind() == InlineReactionKind::InternalMicrotask);
        return static_cast<InternalMicrotask>((flags() & inlineReactionMicrotaskMask) >> inlineReactionMicrotaskShift);
    }

    CompactPointerTuple<JSCell*, uint16_t> m_packed;
    WriteBarrier<Unknown> m_slot;
};

static constexpr PropertyOffset promiseCapabilityResolvePropertyOffset = 0;
static constexpr PropertyOffset promiseCapabilityRejectPropertyOffset = 1;
static constexpr PropertyOffset promiseCapabilityPromisePropertyOffset = 2;

JSC_DECLARE_HOST_FUNCTION(promiseResolvingFunctionResolve);
JSC_DECLARE_HOST_FUNCTION(promiseResolvingFunctionReject);
JSC_DECLARE_HOST_FUNCTION(promiseFirstResolvingFunctionResolve);
JSC_DECLARE_HOST_FUNCTION(promiseFirstResolvingFunctionReject);
JSC_DECLARE_HOST_FUNCTION(promiseResolvingFunctionResolveWithInternalMicrotask);
JSC_DECLARE_HOST_FUNCTION(promiseResolvingFunctionRejectWithInternalMicrotask);
JSC_DECLARE_HOST_FUNCTION(promiseCapabilityExecutor);

JSObject* promiseSpeciesConstructor(JSGlobalObject*, JSObject*);
Structure* createPromiseCapabilityObjectStructure(VM&, JSGlobalObject&);
bool isDefinitelyNonThenable(JSObject*, JSGlobalObject*);

} // namespace JSC
