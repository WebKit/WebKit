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

class JSPromiseReaction;
class JSPromiseConstructor;

class JSPromise : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static JSPromise* create(VM&, JSGlobalObject*, JSPromiseConstructor*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    enum class Status {
        Unresolved,
        HasResolution,
        HasRejection
    };

    Status status() const
    {
        return m_status;
    }

    JSValue result() const
    {
        ASSERT(m_status != Status::Unresolved);
        return m_result.get();
    }

    JSPromiseConstructor* constructor() const
    {
        return m_constructor.get();
    }

    void reject(VM&, JSValue);
    void resolve(VM&, JSValue);

    void appendResolveReaction(VM&, JSPromiseReaction*);
    void appendRejectReaction(VM&, JSPromiseReaction*);

private:
    JSPromise(VM&, Structure*);
    void finishCreation(VM&, JSPromiseConstructor*);
    static const unsigned StructureFlags = OverridesVisitChildren | JSObject::StructureFlags;
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    Status m_status;
    WriteBarrier<Unknown> m_result;
    WriteBarrier<JSPromiseConstructor> m_constructor;
    Vector<WriteBarrier<JSPromiseReaction>> m_resolveReactions;
    Vector<WriteBarrier<JSPromiseReaction>> m_rejectReactions;
};

} // namespace JSC

#endif // ENABLE(PROMISES)

#endif // JSPromise_h
