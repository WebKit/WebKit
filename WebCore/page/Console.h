/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef Console_h
#define Console_h

#include "MemoryInfo.h"
#include "PlatformString.h"
#include "ScriptProfile.h"
#include "ScriptState.h"

#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class ScriptArguments;

#if ENABLE(JAVASCRIPT_DEBUGGER)
typedef Vector<RefPtr<ScriptProfile> > ProfilesArray;
#endif

class Frame;
class Page;
class ScriptCallStack;

// Keep in sync with inspector/front-end/Console.js
enum MessageSource {
    HTMLMessageSource,
    WMLMessageSource,
    XMLMessageSource,
    JSMessageSource,
    CSSMessageSource,
    OtherMessageSource
};

enum MessageType {
    LogMessageType,
    ObjectMessageType,
    TraceMessageType,
    StartGroupMessageType,
    StartGroupCollapsedMessageType,
    EndGroupMessageType,
    AssertMessageType,
    UncaughtExceptionMessageType
};

enum MessageLevel {
    TipMessageLevel,
    LogMessageLevel,
    WarningMessageLevel,
    ErrorMessageLevel,
    DebugMessageLevel
};

class Console : public RefCounted<Console> {
public:
    static PassRefPtr<Console> create(Frame* frame) { return adoptRef(new Console(frame)); }

    Frame* frame() const;
    void disconnectFrame();

    void addMessage(MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String& sourceURL);
    void addMessage(MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String& sourceURL, PassOwnPtr<ScriptCallStack> callStack);

    void debug(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void error(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void info(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void log(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void warn(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void dir(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void dirxml(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void trace(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void assertCondition(bool condition, PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void count(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void markTimeline(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
#if ENABLE(WML)
    String lastWMLErrorMessage() const;
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER)
    const ProfilesArray& profiles() const { return m_profiles; }
    void profile(const String&, ScriptState*, PassOwnPtr<ScriptCallStack>);
    void profileEnd(const String&, ScriptState*, PassOwnPtr<ScriptCallStack>);
#endif
    void time(const String&);
    void timeEnd(const String&, PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void group(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void groupCollapsed(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>);
    void groupEnd();

    bool shouldCaptureFullStackTrace() const;

    static bool shouldPrintExceptions();
    static void setShouldPrintExceptions(bool);

    MemoryInfo* memory() const;

private:
    inline Page* page() const;
    void addMessage(MessageType, MessageLevel, PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack>, bool acceptNoArguments = false);

    Console(Frame*);

    Frame* m_frame;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    ProfilesArray m_profiles;
#endif
    mutable RefPtr<MemoryInfo> m_memory;
};

} // namespace WebCore

#endif // Console_h
