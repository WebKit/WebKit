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

#include "PlatformString.h"

#include "ScriptProfile.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

#if ENABLE(JAVASCRIPT_DEBUGGER)
typedef Vector<RefPtr<ScriptProfile> > ProfilesArray;
#endif

class Frame;
class Page;
class String;
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
    EndGroupMessageType,
    AssertMessageType
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

    void debug(ScriptCallStack*);
    void error(ScriptCallStack*);
    void info(ScriptCallStack*);
    void log(ScriptCallStack*);
    void warn(ScriptCallStack*);
    void dir(ScriptCallStack*);
    void dirxml(ScriptCallStack*);
    void trace(ScriptCallStack*);
    void assertCondition(bool condition, ScriptCallStack*);
    void count(ScriptCallStack*);
    void markTimeline(ScriptCallStack*);
#if ENABLE(WML)
    String lastWMLErrorMessage() const;
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER)
    void profile(const String&, ScriptCallStack*);
    void profileEnd(const String&, ScriptCallStack*);
#endif
    void time(const String&);
    void timeEnd(const String&, ScriptCallStack*);
    void group(ScriptCallStack*);
    void groupEnd();

    static bool shouldPrintExceptions();
    static void setShouldPrintExceptions(bool);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    const ProfilesArray& profiles() const { return m_profiles; }
#endif

private:
    inline Page* page() const;
    void addMessage(MessageType, MessageLevel, ScriptCallStack*, bool acceptNoArguments = false);

    Console(Frame*);

    Frame* m_frame;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    ProfilesArray m_profiles;
#endif
};

} // namespace WebCore

#endif // Console_h
