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

#ifndef VMEntryScope_h
#define VMEntryScope_h

#include "Interpreter.h"
#include <wtf/StackBounds.h>
#include <wtf/StackStats.h>

namespace JSC {

class JSGlobalObject;
class VM;

class VMEntryScope {
public:
    JS_EXPORT_PRIVATE VMEntryScope(VM&, JSGlobalObject*);
    JS_EXPORT_PRIVATE ~VMEntryScope();

    JSGlobalObject* globalObject() const { return m_globalObject; }

    void setRecompilationNeeded(bool recompileNeeded) { m_recompilationNeeded = recompileNeeded; }

private:
    size_t requiredCapacity() const;

    VM& m_vm;
    StackStats::CheckPoint m_stackCheckPoint;
    StackBounds m_stack;
    JSGlobalObject* m_globalObject;

    // m_prev and m_prevStackLimit may belong to a different thread's stack.
    VMEntryScope* m_prev;
    void* m_prevStackLimit;
    bool m_recompilationNeeded;
};

} // namespace JSC

#endif // VMEntryScope_h

