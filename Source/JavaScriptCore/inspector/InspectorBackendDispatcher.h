/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2011 The Chromium Authors. All rights reserved.
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

#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/InspectorProtocolTypes.h>
#include <functional>
#include <wtf/Function.h>
#include <wtf/RefCounted.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

class BackendDispatcher;

typedef RefPtr<StringImpl> ErrorString;

template<typename T>
using CommandResult = Inspector::Protocol::ErrorStringOr<T>;

template <typename T>
using CommandCallback = Function<void(CommandResult<T>)>;

template <typename... ArgTypes>
using CommandResultOf = Inspector::Protocol::ErrorStringOr<std::tuple<ArgTypes...>>;

template <typename... ArgTypes>
using CommandCallbackOf = Function<void(CommandResultOf<ArgTypes...>)>;

class SupplementalBackendDispatcher : public RefCounted<SupplementalBackendDispatcher> {
public:
    JS_EXPORT_PRIVATE SupplementalBackendDispatcher(BackendDispatcher&);
    JS_EXPORT_PRIVATE virtual ~SupplementalBackendDispatcher();
    virtual void dispatch(long requestId, StringImpl* method, Ref<JSON::Object>&& message) = 0;
protected:
    const Ref<BackendDispatcher> m_backendDispatcher;
};

class BackendDispatcher : public RefCountedAndCanMakeWeakPtr<BackendDispatcher> {
public:
    JS_EXPORT_PRIVATE static Ref<BackendDispatcher> create(Ref<FrontendRouter>&&, BackendDispatcher* fallback = nullptr);

    class CallbackBase : public RefCounted<CallbackBase> {
    public:
        JS_EXPORT_PRIVATE CallbackBase(Ref<BackendDispatcher>&&, long requestId);
        virtual ~CallbackBase() { }

        NODELETE JS_EXPORT_PRIVATE bool isActive() const;
        void disable() { m_alreadySent = true; }

        JS_EXPORT_PRIVATE void sendSuccess(Ref<JSON::Object>&&);
        JS_EXPORT_PRIVATE void sendFailure(const ErrorString&);

    private:
        const Ref<BackendDispatcher> m_backendDispatcher;
        long m_requestId;
        bool m_alreadySent { false };
    };

    bool isActive() const;

    bool hasProtocolErrors() const { return m_protocolErrors.size() > 0; }

    enum CommonErrorCode {
        ParseError = 0,
        InvalidRequest,
        MethodNotFound,
        InvalidParams,
        InternalError,
        ServerError
    };

    JS_EXPORT_PRIVATE void registerDispatcherForDomain(StringImpl* domain, SupplementalBackendDispatcher*);
    JS_EXPORT_PRIVATE void dispatch(StringImpl* message);

    // Note that 'unused' is a workaround so the compiler can pick the right sendResponse based on arity.
    // When <http://webkit.org/b/179847> is fixed or this class is renamed for the JSON::Object case,
    // then this alternate method with a dummy parameter can be removed in favor of the one without it.
    void sendResponse(long requestId, RefPtr<JSON::Object>&& result);
    void sendResponse(long requestId, RefPtr<JSON::Object>&& result, bool unused);
    void sendResponse(long requestId, Ref<JSON::Object>&& result);
    JS_EXPORT_PRIVATE void sendResponse(long requestId, Ref<JSON::Object>&& result, bool unused);
    JS_EXPORT_PRIVATE void sendPendingErrors();
    JS_EXPORT_PRIVATE void sendErrorResponse(long requestId, CommonErrorCode, StringImpl* errorMessage);

    JS_EXPORT_PRIVATE void reportProtocolError(CommonErrorCode, StringImpl* errorMessage);
    JS_EXPORT_PRIVATE void reportProtocolError(std::optional<long> relatedRequestId, CommonErrorCode, StringImpl* errorMessage);

    JS_EXPORT_PRIVATE std::optional<bool> getBoolean(JSON::Object*, StringImpl* name, bool required);
    JS_EXPORT_PRIVATE std::optional<int> getInteger(JSON::Object*, StringImpl* name, bool required);
    JS_EXPORT_PRIVATE std::optional<double> getDouble(JSON::Object*, StringImpl* name, bool required);
    JS_EXPORT_PRIVATE RefPtr<StringImpl> getString(JSON::Object*, StringImpl* name, bool required);
    JS_EXPORT_PRIVATE RefPtr<JSON::Value> getValue(JSON::Object*, StringImpl* name, bool required);
    JS_EXPORT_PRIVATE RefPtr<JSON::Object> getObject(JSON::Object*, StringImpl* name, bool required);
    JS_EXPORT_PRIVATE RefPtr<JSON::Array> getArray(JSON::Object*, StringImpl* name, bool required);

private:
    BackendDispatcher(Ref<FrontendRouter>&&, BackendDispatcher* fallback);

    template<typename T>
    WTF_INTERNAL T getPropertyValue(JSON::Object*, StringImpl* name, bool required, std::function<T(JSON::Value&)> converter, ASCIILiteral typeName);

    const Ref<FrontendRouter> m_frontendRouter;
    UncheckedKeyHashMap<RefPtr<StringImpl>, SupplementalBackendDispatcher*> m_dispatchers;

    // Protocol errors reported for the top-level request being processed. Async responses
    // send their errors with sendErrorResponse() instead, because the request being
    // processed when they complete can be a different one.
    Vector<std::tuple<CommonErrorCode, RefPtr<StringImpl>>> m_protocolErrors;

    // For synchronously handled requests, avoid plumbing requestId through every
    // call that could potentially fail with a protocol error.
    std::optional<long> m_currentRequestId { std::nullopt };

    WeakPtr<BackendDispatcher> m_fallbackDispatcher;
};

} // namespace Inspector
