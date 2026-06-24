/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "AsyncSequence.h"
#include "IDLTypes.h"
#include "JSDOMConvertBase.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObject.h"
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/IteratorOperations.h>
#include <JavaScriptCore/TopExceptionScope.h>
#include <wtf/Expected.h>

namespace WebCore {

namespace Detail {

// Mirrors GenericSequenceInnerConverter in JSDOMConvertSequences.h: converts a single yielded
// value to the element type, leaving any exception on the scope and returning std::nullopt.
template<typename IDL>
struct AsyncSequenceElementConverter {
    static std::optional<typename IDL::SequenceStorageType> convert(JSC::TopExceptionScope& scope, JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        auto converted = WebCore::convert<IDL>(lexicalGlobalObject, value);
        if (converted.hasException(scope)) [[unlikely]]
            return std::nullopt;
        return typename IDL::SequenceStorageType { converted.releaseReturnValue() };
    }
};

template<typename T>
struct AsyncSequenceElementConverter<IDLInterface<T>> {
    static std::optional<Ref<T>> convert(JSC::TopExceptionScope& scope, JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        auto converted = WebCore::convert<IDLInterface<T>>(lexicalGlobalObject, value);
        if (converted.hasException(scope)) [[unlikely]]
            return std::nullopt;
        return Ref<T> { converted.releaseReturnValue() };
    }
};

} // namespace Detail

// Native representation of a WebIDL async_sequence<T> argument. Wraps the type-erased
// AsyncSequence (which drives the JS async-iterator protocol) and converts each yielded value to
// the element type T as it arrives, per https://webidl.spec.whatwg.org/#js-async-iterable.
template<typename T> class AsyncSequenceValue : public RefCounted<AsyncSequenceValue<T>> {
public:
    using StorageType = typename T::SequenceStorageType;

    // On success the optional holds the converted element, or std::nullopt when the sequence is
    // done. On failure the unexpected JSValue carries the rejection / conversion error reason.
    using NextResult = Expected<std::optional<StorageType>, JSC::JSValue>;
    using NextCallback = Function<void(JSDOMGlobalObject*, NextResult&&)>;
    using ReturnCallback = Function<void(JSDOMGlobalObject*, bool isOK, JSC::JSValue)>;

    static Ref<AsyncSequenceValue> create(Ref<AsyncSequence>&& base) { return adoptRef(*new AsyncSequenceValue(WTF::move(base))); }

    void callNext(NextCallback&&);
    void callReturn(JSC::JSValue reason, ReturnCallback&& callback) { m_base->callReturn(reason, WTF::move(callback)); }

private:
    explicit AsyncSequenceValue(Ref<AsyncSequence>&& base)
        : m_base(WTF::move(base))
    {
    }

    const Ref<AsyncSequence> m_base;
};

template<typename T>
void AsyncSequenceValue<T>::callNext(NextCallback&& callback)
{
    m_base->callNext([callback = WTF::move(callback)](JSDOMGlobalObject* globalObject, bool isOK, JSC::JSValue resultRecord) mutable {
        if (!globalObject) {
            callback(nullptr, makeUnexpected(JSC::JSValue { }));
            return;
        }

        // The underlying next() promise rejected; forward the reason verbatim.
        if (!isOK) {
            callback(globalObject, makeUnexpected(resultRecord));
            return;
        }

        Ref vm = globalObject->vm();
        auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

        if (!resultRecord.isObject()) {
            callback(globalObject, makeUnexpected(JSC::JSValue { JSC::createTypeError(globalObject, "Iterator result is not an object"_s) }));
            return;
        }

        // https://webidl.spec.whatwg.org/#js-async-iterable : "get the next iteration result".
        bool done = JSC::iteratorCompleteExported(globalObject, resultRecord);
        if (auto* exception = scope.exception()) [[unlikely]] {
            scope.clearException();
            callback(globalObject, makeUnexpected(exception->value()));
            return;
        }
        if (done) {
            callback(globalObject, NextResult { std::optional<StorageType> { std::nullopt } });
            return;
        }

        JSC::JSValue value = JSC::iteratorValue(globalObject, resultRecord);
        if (auto* exception = scope.exception()) [[unlikely]] {
            scope.clearException();
            callback(globalObject, makeUnexpected(exception->value()));
            return;
        }

        // Convert the yielded value to the element type T (may throw a TypeError, etc.).
        auto converted = Detail::AsyncSequenceElementConverter<T>::convert(scope, *globalObject, value);
        if (auto* exception = scope.exception()) [[unlikely]] {
            scope.clearException();
            callback(globalObject, makeUnexpected(exception->value()));
            return;
        }

        callback(globalObject, NextResult { std::optional<StorageType> { WTF::move(converted) } });
    });
}

template<typename T> struct Converter<IDLAsyncSequence<T>> : DefaultConverter<IDLAsyncSequence<T>> {
    using Result = ConversionResult<IDLAsyncSequence<T>>;

    // https://webidl.spec.whatwg.org/#js-async-iterable : the conversion runs the "get the async
    // iterator" steps; per-element conversion happens lazily as values are read (see callNext).
    template<typename ExceptionThrower = DefaultExceptionThrower>
    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        UNUSED_PARAM(exceptionThrower);

        auto& vm = lexicalGlobalObject.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        auto* globalObject = dynamicDowncast<JSDOMGlobalObject>(&lexicalGlobalObject);
        RELEASE_ASSERT(globalObject);

        auto baseOrException = AsyncSequence::create(*globalObject, value);
        if (baseOrException.hasException()) {
            propagateException(lexicalGlobalObject, scope, baseOrException.releaseException());
            return Result::exception();
        }

        return Ref<AsyncSequenceValue<T>> { AsyncSequenceValue<T>::create(baseOrException.releaseReturnValue()) };
    }
};

// async_sequence is an argument-only type in WebIDL, so there is intentionally no
// JSConverter<IDLAsyncSequence<T>> (no native-to-JS / output direction).

} // namespace WebCore
