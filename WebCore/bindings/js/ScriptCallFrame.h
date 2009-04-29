/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScriptCallFrame_h
#define ScriptCallFrame_h

#include "KURL.h"
#include <runtime/ArgList.h>
#include "ScriptString.h"
#include "ScriptValue.h"
#include <wtf/Vector.h>

namespace JSC {
    class ExecState;
    class InternalFunction;
}

namespace WebCore {

    // FIXME: Implement retrieving line number and source URL and storing here
    // for all call frames, not just the first one.
    // See <https://bugs.webkit.org/show_bug.cgi?id=22556> and
    // <https://bugs.webkit.org/show_bug.cgi?id=21180>
    class ScriptCallFrame  {
    public:
        ScriptCallFrame(const JSC::UString& functionName, const JSC::UString& urlString, int lineNumber, const JSC::ArgList&, unsigned skipArgumentCount);
        ~ScriptCallFrame();

        const ScriptString& functionName() const { return m_functionName; }
        const KURL& sourceURL() const { return m_sourceURL; }
        unsigned lineNumber() const { return m_lineNumber; }

        // argument retrieval methods
        const ScriptValue& argumentAt(unsigned) const;
        unsigned argumentCount() const { return m_arguments.size(); }

    private:
        ScriptString m_functionName;
        KURL m_sourceURL;
        unsigned m_lineNumber;

        Vector<ScriptValue> m_arguments;
    };

} // namespace WebCore

#endif // ScriptCallFrame_h
