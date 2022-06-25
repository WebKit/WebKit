/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)

#include <wtf/Vector.h>

namespace JSC {

class JITPlan;
class Scannable;
class VM;

class Safepoint {
public:
    class Result {
    public:
        Result()
            : m_didGetCancelled(false)
            , m_wasChecked(true)
        {
        }
        
        ~Result();
        
        bool didGetCancelled();
        
    private:
        friend class Safepoint;
        
        bool m_didGetCancelled;
        bool m_wasChecked;
    };
    
    Safepoint(JITPlan&, Result&);
    ~Safepoint();
    
    void add(Scannable*);
    
    void begin();

    template<typename Visitor> void checkLivenessAndVisitChildren(Visitor&);
    template<typename Visitor> bool isKnownToBeLiveDuringGC(Visitor&);
    bool isKnownToBeLiveAfterGC();
    void cancel();
    
    VM* vm() const; // May return null if we've been cancelled.

private:
    VM* m_vm;
    JITPlan& m_plan;
    Vector<Scannable*> m_scannables;
    bool m_didCallBegin;
    Result& m_result;
};

} // namespace JSC

#endif // ENABLE(JIT)
