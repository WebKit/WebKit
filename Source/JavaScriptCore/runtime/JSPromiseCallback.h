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

#ifndef JSPromiseCallback_h
#define JSPromiseCallback_h

#include "InternalFunction.h"

namespace JSC {

class JSPromiseResolver;

class JSPromiseCallback : public InternalFunction {
public:
    typedef InternalFunction Base;

    enum Algorithm {
        Fulfill,
        Resolve,
        Reject,
    };

    static JSPromiseCallback* create(ExecState*, JSGlobalObject*, Structure*, JSPromiseResolver*, Algorithm);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

private:
    JSPromiseCallback(JSGlobalObject*, Structure*, Algorithm);
    void finishCreation(ExecState*, JSPromiseResolver*);
    static const unsigned StructureFlags = OverridesVisitChildren | JSObject::StructureFlags;

    static EncodedJSValue JSC_HOST_CALL callPromiseCallback(ExecState*);
    static CallType getCallData(JSCell*, CallData&);

    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<JSPromiseResolver> m_resolver;
    Algorithm m_algorithm;
};

class JSPromiseWrapperCallback : public InternalFunction {
public:
    typedef InternalFunction Base;

    static JSPromiseWrapperCallback* create(ExecState*, JSGlobalObject*, Structure*, JSPromiseResolver*, JSValue callback);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

private:
    JSPromiseWrapperCallback(JSGlobalObject*, Structure*);
    void finishCreation(ExecState*, JSPromiseResolver*, JSValue callback);
    static const unsigned StructureFlags = OverridesVisitChildren | JSObject::StructureFlags;

    static EncodedJSValue JSC_HOST_CALL callPromiseWrapperCallback(ExecState*);
    static CallType getCallData(JSCell*, CallData&);

    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<JSPromiseResolver> m_resolver;
    WriteBarrier<Unknown> m_callback;
};

} // namespace JSC

#endif // JSPromiseCallback_h
