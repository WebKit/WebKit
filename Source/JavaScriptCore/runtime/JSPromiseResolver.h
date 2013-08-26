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

#ifndef JSPromiseResolver_h
#define JSPromiseResolver_h

#include "JSObject.h"

namespace JSC {

class JSPromise;

class JSPromiseResolver : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;

    static JSPromiseResolver* create(VM&, Structure*, JSPromise*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    JSPromise* promise() const;

    void fulfillIfNotResolved(ExecState*, JSValue);
    void resolveIfNotResolved(ExecState*, JSValue);
    void rejectIfNotResolved(ExecState*, JSValue);

    enum ResolverMode {
        ResolveSynchronously,
        ResolveAsynchronously,
    };

    void fulfill(ExecState*, JSValue, ResolverMode = ResolveAsynchronously);
    void resolve(ExecState*, JSValue, ResolverMode = ResolveAsynchronously);
    void reject(ExecState*, JSValue, ResolverMode = ResolveAsynchronously);

protected:
    void finishCreation(VM&, JSPromise*);
    static const unsigned StructureFlags = OverridesVisitChildren | JSObject::StructureFlags;

private:
    JSPromiseResolver(VM&, Structure*);

    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<JSPromise> m_promise;
    bool m_isResolved;
};

} // namespace JSC

#endif // JSPromiseResolver_h
