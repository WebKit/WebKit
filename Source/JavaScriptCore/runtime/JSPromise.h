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

#include <JavaScriptCore/JSInternalFieldObjectImpl.h>

namespace JSC {

class JSPromiseConstructor;
class JSPromise : public JSInternalFieldObjectImpl<2> {
public:
    using Base = JSInternalFieldObjectImpl<2>;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.promiseSpace();
    }

    JS_EXPORT_PRIVATE static JSPromise* create(VM&, Structure*);
    static JSPromise* createWithInitialValues(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_EXPORT_INFO;

    enum class Status : unsigned {
        Pending = 0, // Making this as 0, so that, we can change the status from Pending to others without masking.
        Fulfilled = 1,
        Rejected = 2,
    };
    static constexpr uint32_t isHandledFlag = 4;
    static constexpr uint32_t isFirstResolvingFunctionCalledFlag = 8;
    static constexpr uint32_t stateMask = 0b11;

    enum class Field : unsigned {
        Flags = 0,
        ReactionsOrResult = 1,
    };
    static_assert(numberOfInternalFields == 2);

    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNumber(static_cast<unsigned>(Status::Pending)),
            jsUndefined(),
        } };
    }

    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    inline Status status() const
    {
        JSValue value = internalField(Field::Flags).get();
        uint32_t flags = value.asUInt32AsAnyInt();
        return static_cast<Status>(flags & stateMask);
    }

    inline bool isHandled() const
    {
        return flags() & isHandledFlag;
    }

    inline JSValue result() const
    {
        Status status = this->status();
        if (status == Status::Pending)
            return jsUndefined();
        return internalField(Field::ReactionsOrResult).get();
    }


    JS_EXPORT_PRIVATE static JSPromise* resolvedPromise(JSGlobalObject*, JSValue);
    JS_EXPORT_PRIVATE static JSPromise* rejectedPromise(JSGlobalObject*, JSValue);

    JS_EXPORT_PRIVATE void resolve(JSGlobalObject*, JSValue);
    JS_EXPORT_PRIVATE void reject(JSGlobalObject*, JSValue);
    void fulfill(JSGlobalObject*, JSValue);
    JS_EXPORT_PRIVATE void rejectAsHandled(JSGlobalObject*, JSValue);
    JS_EXPORT_PRIVATE void reject(JSGlobalObject*, Exception*);
    JS_EXPORT_PRIVATE void rejectAsHandled(JSGlobalObject*, Exception*);
    JS_EXPORT_PRIVATE void performPromiseThenExported(JSGlobalObject*, JSValue onFulfilled, JSValue onRejected, JSValue, JSValue = jsUndefined());

    JS_EXPORT_PRIVATE JSPromise* rejectWithCaughtException(JSGlobalObject*, ThrowScope&);

    JSValue reactionsOrResult() const { return internalField(Field::ReactionsOrResult).get(); };
    void setReactionsOrResult(VM& vm, JSValue value) { internalField(Field::ReactionsOrResult).set(vm, this, value); };

    // https://webidl.spec.whatwg.org/#mark-a-promise-as-handled
    void markAsHandled()
    {
        uint32_t flags = this->flags();
        internalField(Field::Flags).setWithoutWriteBarrier(jsNumber(flags | isHandledFlag));
    }

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
    void performPromiseThen(JSGlobalObject*, JSValue onFulfilled, JSValue onRejected, JSValue, JSValue = jsUndefined());
    void rejectPromise(JSGlobalObject*, JSValue);
    void fulfillPromise(JSGlobalObject*, JSValue);
    void resolvePromise(JSGlobalObject*, JSValue);

    static void resolveWithoutPromiseForAsyncAwait(JSGlobalObject*, JSValue resolution, JSValue onFulfilled, JSValue onRejected, JSValue context);
    static void resolveWithoutPromise(JSGlobalObject*, JSValue resolution, JSValue onFulfilled, JSValue onRejected, JSValue context);
    static void rejectWithoutPromise(JSGlobalObject*, JSValue argument, JSValue onFulfilled, JSValue onRejected, JSValue context);
    static void fulfillWithoutPromise(JSGlobalObject*, JSValue argument, JSValue onFulfilled, JSValue onRejected, JSValue context);

    bool isThenFastAndNonObservable();

    std::tuple<JSFunction*, JSFunction*> createResolvingFunctions(VM&, JSGlobalObject*);
    std::tuple<JSFunction*, JSFunction*> createFirstResolvingFunctions(VM&, JSGlobalObject*);
    static std::tuple<JSFunction*, JSFunction*> createResolvingFunctionsWithoutPromise(VM&, JSGlobalObject*, JSValue onFulfilled, JSValue onRejected, JSValue context);
    static std::tuple<JSObject*, JSObject*, JSObject*> newPromiseCapability(JSGlobalObject*, JSValue constructor);
    static JSValue createPromiseCapability(VM&, JSGlobalObject*, JSObject* promise, JSObject* resolve, JSObject* reject);
    static JSValue promiseResolve(JSGlobalObject*, JSValue constructor, JSValue);

    JSValue then(JSGlobalObject*, JSValue onFulfilled, JSValue onRejected);

protected:
    JSPromise(VM&, Structure*);
    void finishCreation(VM&);

    static void triggerPromiseReactions(JSGlobalObject*, JSPromise::Status, JSPromiseReaction* head, JSValue argument);

    inline uint32_t flags() const
    {
        JSValue value = internalField(Field::Flags).get();
        return value.asUInt32AsAnyInt();
    }
};

static constexpr PropertyOffset promiseCapabilityResolvePropertyOffset = 0;
static constexpr PropertyOffset promiseCapabilityRejectPropertyOffset = 1;
static constexpr PropertyOffset promiseCapabilityPromisePropertyOffset = 2;

JSC_DECLARE_HOST_FUNCTION(promiseResolvingFunctionResolve);
JSC_DECLARE_HOST_FUNCTION(promiseResolvingFunctionReject);
JSC_DECLARE_HOST_FUNCTION(promiseFirstResolvingFunctionResolve);
JSC_DECLARE_HOST_FUNCTION(promiseFirstResolvingFunctionReject);
JSC_DECLARE_HOST_FUNCTION(promiseResolvingFunctionResolveWithoutPromise);
JSC_DECLARE_HOST_FUNCTION(promiseResolvingFunctionRejectWithoutPromise);
JSC_DECLARE_HOST_FUNCTION(promiseCapabilityExecutor);

JSObject* promiseSpeciesConstructor(JSGlobalObject*, JSObject*);
Structure* createPromiseCapabilityObjectStructure(VM&, JSGlobalObject&);

} // namespace JSC
