/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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

#ifndef InspectorBackendDispatcher_h
#define InspectorBackendDispatcher_h

#include "InspectorValues.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

class InspectorBackendDispatcher;
class InspectorFrontendChannel;
typedef String ErrorString;

class InspectorSupplementalBackendDispatcher : public RefCounted<InspectorSupplementalBackendDispatcher> {
public:
    InspectorSupplementalBackendDispatcher(InspectorBackendDispatcher* backendDispatcher) : m_backendDispatcher(backendDispatcher) { }
    virtual ~InspectorSupplementalBackendDispatcher() { }
    virtual void dispatch(long callId, const String& method, PassRefPtr<InspectorObject> message) = 0;
protected:
    RefPtr<InspectorBackendDispatcher> m_backendDispatcher;
};

class JS_EXPORT_PRIVATE InspectorBackendDispatcher : public RefCounted<InspectorBackendDispatcher> {
public:
    static PassRefPtr<InspectorBackendDispatcher> create(InspectorFrontendChannel*);

    class JS_EXPORT_PRIVATE CallbackBase : public RefCounted<CallbackBase> {
    public:
        CallbackBase(PassRefPtr<InspectorBackendDispatcher>, int id);

        bool isActive() const;
        void sendFailure(const ErrorString&);
        void disable() { m_alreadySent = true; }

    protected:
        void sendIfActive(PassRefPtr<InspectorObject> partialMessage, const ErrorString& invocationError);

    private:
        RefPtr<InspectorBackendDispatcher> m_backendDispatcher;
        int m_id;
        bool m_alreadySent;
    };

    void clearFrontend() { m_inspectorFrontendChannel = nullptr; }
    bool isActive() const { return !!m_inspectorFrontendChannel; }

    enum CommonErrorCode {
        ParseError = 0,
        InvalidRequest,
        MethodNotFound,
        InvalidParams,
        InternalError,
        ServerError
    };

    void registerDispatcherForDomain(const String& domain, InspectorSupplementalBackendDispatcher*);
    void dispatch(const String& message);
    void sendResponse(long callId, PassRefPtr<InspectorObject> result, const ErrorString& invocationError);
    void reportProtocolError(const long* const callId, CommonErrorCode, const String& errorMessage) const;
    void reportProtocolError(const long* const callId, CommonErrorCode, const String& errorMessage, PassRefPtr<InspectorArray> data) const;

    static int getInt(InspectorObject*, const String& name, bool* valueFound, InspectorArray* protocolErrors);
    static double getDouble(InspectorObject*, const String& name, bool* valueFound, InspectorArray* protocolErrors);
    static String getString(InspectorObject*, const String& name, bool* valueFound, InspectorArray* protocolErrors);
    static bool getBoolean(InspectorObject*, const String& name, bool* valueFound, InspectorArray* protocolErrors);
    static PassRefPtr<InspectorObject> getObject(InspectorObject*, const String& name, bool* valueFound, InspectorArray* protocolErrors);
    static PassRefPtr<InspectorArray> getArray(InspectorObject*, const String& name, bool* valueFound, InspectorArray* protocolErrors);

private:
    InspectorBackendDispatcher(InspectorFrontendChannel* inspectorFrontendChannel) : m_inspectorFrontendChannel(inspectorFrontendChannel) { }

    InspectorFrontendChannel* m_inspectorFrontendChannel;
    HashMap<String, InspectorSupplementalBackendDispatcher*> m_dispatchers;
};

} // namespace Inspector

#endif // !defined(InspectorBackendDispatcher_h)
