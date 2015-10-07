/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef IDBRequestImpl_h
#define IDBRequestImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBOpenDBRequest.h"
#include "IDBResourceIdentifier.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class Event;
class IDBResultData;

namespace IDBClient {

class IDBConnectionToServer;

class IDBRequest : public WebCore::IDBOpenDBRequest, public RefCounted<IDBRequest> {
public:
    const IDBResourceIdentifier& resourceIdentifier() const { return m_resourceIdentifier; }

    virtual RefPtr<IDBAny> result(ExceptionCode&) const override;
    virtual unsigned short errorCode(ExceptionCode&) const override;
    virtual RefPtr<DOMError> error(ExceptionCode&) const override;
    virtual RefPtr<IDBAny> source() const override;
    virtual RefPtr<IDBTransaction> transaction() const override;
    virtual const String& readyState() const override;

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const override;
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted<IDBRequest>::ref;
    using RefCounted<IDBRequest>::deref;

    void enqueueEvent(Ref<Event>&&);
    
protected:
    IDBRequest(IDBConnectionToServer&, ScriptExecutionContext*);

    // ActiveDOMObject.
    virtual const char* activeDOMObjectName() const override final;
    virtual bool canSuspendForPageCache() const override final;
    
    // EventTarget.
    virtual void refEventTarget() override final { RefCounted<IDBRequest>::ref(); }
    virtual void derefEventTarget() override final { RefCounted<IDBRequest>::deref(); }

    IDBResourceIdentifier m_resourceIdentifier;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBRequestImpl_h
