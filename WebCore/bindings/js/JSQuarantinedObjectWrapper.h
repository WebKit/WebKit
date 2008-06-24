/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef JSQuarantinedObjectWrapper_h
#define JSQuarantinedObjectWrapper_h

#include <kjs/JSObject.h>

namespace WebCore {

    class JSQuarantinedObjectWrapper : public KJS::JSObject {
    public:
        static JSQuarantinedObjectWrapper* asWrapper(KJS::JSValue*);

        virtual ~JSQuarantinedObjectWrapper();

        KJS::JSObject* unwrappedObject() const { return m_unwrappedObject; }
        KJS::JSGlobalObject* unwrappedGlobalObject() const { return m_unwrappedGlobalObject; };
        KJS::ExecState* unwrappedExecState() const;

        bool allowsUnwrappedAccessFrom(const KJS::ExecState*) const;

        static const KJS::ClassInfo s_info;

    protected:
        JSQuarantinedObjectWrapper(KJS::ExecState* unwrappedExec, KJS::JSObject* unwrappedObject, KJS::JSValue* wrappedPrototype);

        virtual void mark();

    private:
        virtual bool getOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&);
        virtual bool getOwnPropertySlot(KJS::ExecState*, unsigned, KJS::PropertySlot&);

        virtual void put(KJS::ExecState*, const KJS::Identifier&, KJS::JSValue*);
        virtual void put(KJS::ExecState*, unsigned, KJS::JSValue*);

        virtual bool deleteProperty(KJS::ExecState*, const KJS::Identifier&);
        virtual bool deleteProperty(KJS::ExecState*, unsigned);

        virtual KJS::CallType getCallData(KJS::CallData&);
        virtual KJS::ConstructType getConstructData(KJS::ConstructData&);

        virtual bool implementsHasInstance() const;
        virtual bool hasInstance(KJS::ExecState*, KJS::JSValue*);

        virtual void getPropertyNames(KJS::ExecState*, KJS::PropertyNameArray&);

        virtual KJS::UString className() const { return m_unwrappedObject->className(); }

        virtual bool allowsGetProperty() const { return false; }
        virtual bool allowsSetProperty() const { return false; }
        virtual bool allowsDeleteProperty() const { return false; }
        virtual bool allowsConstruct() const { return false; }
        virtual bool allowsHasInstance() const { return false; }
        virtual bool allowsCallAsFunction() const { return false; }
        virtual bool allowsGetPropertyNames() const { return false; }

        virtual KJS::JSValue* prepareIncomingValue(KJS::ExecState* unwrappedExec, KJS::JSValue* unwrappedValue) const = 0;
        virtual KJS::JSValue* wrapOutgoingValue(KJS::ExecState* unwrappedExec, KJS::JSValue* unwrappedValue) const = 0;

        static KJS::JSValue* cachedValueGetter(KJS::ExecState*, const KJS::Identifier&, const KJS::PropertySlot&);

        void transferExceptionToExecState(KJS::ExecState*) const;

        static KJS::JSValue* call(KJS::ExecState*, KJS::JSObject* function, KJS::JSValue* thisValue, const KJS::ArgList&);
        static KJS::JSObject* construct(KJS::ExecState*, KJS::JSObject*, const KJS::ArgList&);

        KJS::JSGlobalObject* m_unwrappedGlobalObject;
        KJS::JSObject* m_unwrappedObject;
    };

} // namespace WebCore

#endif // JSQuarantinedObjectWrapper_h
