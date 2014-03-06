/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef PageConsole_h
#define PageConsole_h

#include <inspector/ScriptCallStack.h>
#include <runtime/ConsoleClient.h>
#include <wtf/Forward.h>
#include <wtf/PassOwnPtr.h>

namespace JSC {
class ExecState;
}

namespace WebCore {

class Document;
class Page;
class ScriptProfile;
typedef Vector<RefPtr<ScriptProfile>> ProfilesArray;

class PageConsole final : public JSC::ConsoleClient {
public:
    explicit PageConsole(Page&);
    virtual ~PageConsole();

    static bool shouldPrintExceptions();
    static void setShouldPrintExceptions(bool);

    static void mute();
    static void unmute();

    void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, PassRefPtr<Inspector::ScriptCallStack> = nullptr, JSC::ExecState* = nullptr, unsigned long requestIdentifier = 0);
    void addMessage(MessageSource, MessageLevel, const String& message, PassRefPtr<Inspector::ScriptCallStack>);
    void addMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier = 0, Document* = nullptr);

    const ProfilesArray& profiles() const { return m_profiles; }
    void clearProfiles();

protected:
    virtual void messageWithTypeAndLevel(MessageType, MessageLevel, JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>) override;
    virtual void count(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>) override;
    virtual void profile(JSC::ExecState*, const String& title) override;
    virtual void profileEnd(JSC::ExecState*, const String& title) override;
    virtual void time(JSC::ExecState*, const String& title) override;
    virtual void timeEnd(JSC::ExecState*, const String& title) override;
    virtual void timeStamp(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>) override;

private:
    Page& m_page;
    ProfilesArray m_profiles;
};

} // namespace WebCore

#endif // PageConsole_h
