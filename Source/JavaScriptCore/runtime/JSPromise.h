/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef JSPromise_h
#define JSPromise_h

#if ENABLE(PROMISES)

#include "JSDestructibleObject.h"

namespace JSC {

class JSPromiseResolver;
class InternalFunction;
class TaskContext;

class JSPromise : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static JSPromise* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSPromise* createWithResolver(VM&, JSGlobalObject*);

    DECLARE_INFO;

    void setResolver(VM&, JSPromiseResolver*);
    JSPromiseResolver* resolver() const;

    enum State {
        Pending,
        Fulfilled,
        Rejected,
    };

    void setState(State);
    State state() const;

    void setResult(VM&, JSValue);
    JSValue result() const;

    void appendCallbacks(ExecState*, InternalFunction* fulfillCallback, InternalFunction* rejectCallback);

    void queueTaskToProcessFulfillCallbacks(ExecState*);
    void queueTaskToProcessRejectCallbacks(ExecState*);
    void processFulfillCallbacksWithValue(ExecState*, JSValue);
    void processRejectCallbacksWithValue(ExecState*, JSValue);

protected:
    void finishCreation(VM&);
    static const unsigned StructureFlags = OverridesVisitChildren | JSObject::StructureFlags;

private:
    JSPromise(VM&, Structure*);

    static void processFulfillCallbacksForTask(ExecState*, TaskContext*);
    static void processRejectCallbacksForTask(ExecState*, TaskContext*);

    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<JSPromiseResolver> m_resolver;
    WriteBarrier<Unknown> m_result;
    Vector<WriteBarrier<InternalFunction> > m_fulfillCallbacks;
    Vector<WriteBarrier<InternalFunction> > m_rejectCallbacks;
    State m_state;
};

} // namespace JSC

#endif // ENABLE(PROMISES)

#endif // JSPromise_h
