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
#include <profiler/Profile.h>
#include <wtf/RefCounted.h>
#include <wtf/PassRefPtr.h>

namespace JSC {
    class ExecState;
    class ArgList;
}

namespace WebCore {

    typedef Vector<RefPtr<JSC::Profile> > ProfilesArray;

    class Frame;
    class Page;
    class String;

    // Keep in sync with inspector/front-end/Console.js
    enum MessageSource {
        HTMLMessageSource,
        WMLMessageSource,
        XMLMessageSource,
        JSMessageSource,
        CSSMessageSource,
        OtherMessageSource
    };

    enum MessageLevel {
        TipMessageLevel,
        LogMessageLevel,
        WarningMessageLevel,
        ErrorMessageLevel,
        ObjectMessageLevel,
        NodeMessageLevel,
        TraceMessageLevel,
        StartGroupMessageLevel,
        EndGroupMessageLevel
    };

    class Console : public RefCounted<Console> {
    public:
        static PassRefPtr<Console> create(Frame* frame) { return adoptRef(new Console(frame)); }

        void disconnectFrame();

        void addMessage(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceURL);

#if USE(JSC)
        void debug(JSC::ExecState*, const JSC::ArgList&);
        void error(JSC::ExecState*, const JSC::ArgList&);
        void info(JSC::ExecState*, const JSC::ArgList&);
        void log(JSC::ExecState*, const JSC::ArgList&);
        void warn(JSC::ExecState*, const JSC::ArgList&);
        void dir(JSC::ExecState*, const JSC::ArgList&);
        void dirxml(JSC::ExecState*, const JSC::ArgList& arguments);
        void trace(JSC::ExecState*);
        void assertCondition(bool condition, JSC::ExecState*, const JSC::ArgList&);
        void count(JSC::ExecState*, const JSC::ArgList&);
        void profile(JSC::ExecState*, const JSC::ArgList&);
        void profileEnd(JSC::ExecState*, const JSC::ArgList&);
        void time(const JSC::UString& title);
        void timeEnd(JSC::ExecState*, const JSC::ArgList&);
        void group(JSC::ExecState*, const JSC::ArgList&);
        void groupEnd();

        static bool shouldPrintExceptions();
        static void setShouldPrintExceptions(bool);

        const ProfilesArray& profiles() const { return m_profiles; }
#endif

    private:
        inline Page* page() const;

        Console(Frame*);

        Frame* m_frame;
        ProfilesArray m_profiles;
    };

} // namespace WebCore

#endif // Console_h
