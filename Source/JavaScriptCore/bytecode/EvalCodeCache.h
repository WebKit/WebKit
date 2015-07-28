/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EvalCodeCache_h
#define EvalCodeCache_h

#include "Executable.h"
#include "JSGlobalObject.h"
#include "Options.h"
#include "SourceCode.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringHash.h>

namespace JSC {

    class SlotVisitor;

    class EvalCodeCache {
    public:
        EvalExecutable* tryGet(bool inStrictContext, const String& evalSource, JSScope* scope)
        {
            if (isCacheable(inStrictContext, evalSource, scope))
                return m_cacheMap.get(evalSource.impl()).get();
            return 0;
        }
        
        EvalExecutable* getSlow(ExecState* exec, ScriptExecutable* owner, bool inStrictContext, ThisTDZMode thisTDZMode, const String& evalSource, JSScope* scope)
        {
            VariableEnvironment variablesUnderTDZ;
            JSScope::collectVariablesUnderTDZ(scope, variablesUnderTDZ);
            EvalExecutable* evalExecutable = EvalExecutable::create(exec, makeSource(evalSource), inStrictContext, thisTDZMode, &variablesUnderTDZ);
            if (!evalExecutable)
                return 0;

            if (isCacheable(inStrictContext, evalSource, scope) && m_cacheMap.size() < maxCacheEntries)
                m_cacheMap.set(evalSource.impl(), WriteBarrier<EvalExecutable>(exec->vm(), owner, evalExecutable));
            
            return evalExecutable;
        }

        bool isEmpty() const { return m_cacheMap.isEmpty(); }

        void visitAggregate(SlotVisitor&);

        void clear()
        {
            m_cacheMap.clear();
        }

    private:
        ALWAYS_INLINE bool isCacheable(bool inStrictContext, const String& evalSource, JSScope* scope) const
        {
            // If eval() is called and it has access to a lexical scope, we can't soundly cache it.
            // If the eval() only has access to the "var" scope, then we can cache it.
            return !inStrictContext 
                && evalSource.length() < Options::maximumEvalCacheableSourceLength() 
                && scope->begin()->isVariableObject()
                && !scope->isLexicalScope()
                && !scope->isCatchScope();
        }
        static const int maxCacheEntries = 64;

        typedef HashMap<RefPtr<StringImpl>, WriteBarrier<EvalExecutable>> EvalCacheMap;
        EvalCacheMap m_cacheMap;
    };

} // namespace JSC

#endif // EvalCodeCache_h
