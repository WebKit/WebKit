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

#ifndef JSPromiseReaction_h
#define JSPromiseReaction_h

#include "JSCell.h"
#include "Structure.h"

namespace JSC {

class JSPromiseDeferred;
class Microtask;

class JSPromiseReaction : public JSCell {
public:
    typedef JSCell Base;

    static JSPromiseReaction* create(VM&, JSPromiseDeferred*, JSValue);
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CompoundType, StructureFlags), info());
    }

    static const bool hasImmortalStructure = true;

    DECLARE_INFO;

    JSPromiseDeferred* deferred() const { return m_deferred.get(); }
    JSValue handler() const { return m_handler.get(); }

private:
    JSPromiseReaction(VM&);
    void finishCreation(VM&, JSPromiseDeferred*, JSValue);
    static const unsigned StructureFlags = OverridesVisitChildren | Base::StructureFlags;
    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<JSPromiseDeferred> m_deferred;
    WriteBarrier<Unknown> m_handler;
};

PassRefPtr<Microtask> createExecutePromiseReactionMicrotask(VM&, JSPromiseReaction*, JSValue);

} // namespace JSC

#endif // JSPromiseReaction_h
