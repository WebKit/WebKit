/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#ifndef ScriptCallStack_h
#define ScriptCallStack_h

#include "ScriptCallFrame.h"
#include "ScriptState.h"
#include "ScriptValue.h"
#include <wtf/Noncopyable.h>

namespace v8 {
    class Arguments;
}

namespace WebCore {

class ScriptCallStack : public Noncopyable {
public:
    static ScriptCallStack* create(const v8::Arguments&, unsigned skipArgumentCount = 0);
    ~ScriptCallStack();

    static bool callLocation(String* sourceName, int* sourceLineNumber, String* functionName);

    const ScriptCallFrame& at(unsigned) const;
    // FIXME: implement retrieving and storing call stack trace
    unsigned size() const { return 1; }

    ScriptState* state() const { return m_scriptState; }
    ScriptState* globalState() const { return m_scriptState; }

private:
    ScriptCallStack(const v8::Arguments& arguments, unsigned skipArgumentCount, String sourceName, int sourceLineNumber, String funcName);

    // Function for retrieving the source name, line number and function name for the top
    // JavaScript stack frame.
    //
    // It will return true if the caller information was successfully retrieved and written
    // into the function parameters, otherwise the function will return false. It may
    // fail due to a stack overflow in the underlying JavaScript implementation, handling
    // of such exception is up to the caller.
    static bool topStackFrame(String& sourceName, int& lineNumber, String& functionName);

    static void createUtilityContext();

     // Returns a local handle of the utility context.
    static v8::Local<v8::Context> utilityContext()
    {
      if (s_utilityContext.IsEmpty())
          createUtilityContext();
      return v8::Local<v8::Context>::New(s_utilityContext);
    }

    ScriptCallFrame m_lastCaller;
    ScriptState* m_scriptState;

    // Utility context holding JavaScript functions used internally.
    static v8::Persistent<v8::Context> s_utilityContext;
};

} // namespace WebCore

#endif // ScriptCallStack_h
