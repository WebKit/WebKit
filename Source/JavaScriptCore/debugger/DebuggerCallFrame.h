/*
 * Copyright (C) 2008, 2013 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
 
#ifndef DebuggerCallFrame_h
#define DebuggerCallFrame_h

#include "CallFrame.h"
#include "DebuggerPrimitives.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/TextPosition.h>

namespace JSC {

class DebuggerCallFrame : public RefCounted<DebuggerCallFrame> {
public:
    enum Type { ProgramType, FunctionType };

    static PassRefPtr<DebuggerCallFrame> create(CallFrame* callFrame)
    {
        return adoptRef(new DebuggerCallFrame(callFrame));
    }

    JS_EXPORT_PRIVATE explicit DebuggerCallFrame(CallFrame*);

    JS_EXPORT_PRIVATE PassRefPtr<DebuggerCallFrame> callerFrame();
    ExecState* exec() const { return m_callFrame; }
    JS_EXPORT_PRIVATE SourceID sourceID() const;

    // line and column are in base 0 e.g. the first line is line 0.
    int line() const { return m_position.m_line.zeroBasedInt(); }
    int column() const { return m_position.m_column.zeroBasedInt(); }
    JS_EXPORT_PRIVATE const TextPosition& position() const { return m_position; }

    JS_EXPORT_PRIVATE JSGlobalObject* vmEntryGlobalObject() const;
    JS_EXPORT_PRIVATE JSScope* scope() const;
    JS_EXPORT_PRIVATE String functionName() const;
    JS_EXPORT_PRIVATE Type type() const;
    JS_EXPORT_PRIVATE JSValue thisValue() const;
    JS_EXPORT_PRIVATE JSValue evaluate(const String&, JSValue& exception) const;

    bool isValid() const { return !!m_callFrame; }
    JS_EXPORT_PRIVATE void invalidate();

    // The following are only public for the Debugger's use only. They will be
    // made private soon. Other clients should not use these.

    JS_EXPORT_PRIVATE static JSValue evaluateWithCallFrame(CallFrame*, const String& script, JSValue& exception);
    JS_EXPORT_PRIVATE static TextPosition positionForCallFrame(CallFrame*);
    JS_EXPORT_PRIVATE static SourceID sourceIDForCallFrame(CallFrame*);
    static JSValue thisValueForCallFrame(CallFrame*);

private:
    CallFrame* m_callFrame;
    RefPtr<DebuggerCallFrame> m_caller;
    TextPosition m_position;
};

} // namespace JSC

#endif // DebuggerCallFrame_h
