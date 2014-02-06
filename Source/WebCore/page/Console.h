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

#include "DOMWindowProperty.h"
#include "ScriptProfile.h"
#include "ScriptState.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace Inspector {
class ScriptArguments;
}

namespace WebCore {

class Frame;
class Page;

typedef Vector<RefPtr<ScriptProfile>> ProfilesArray;

class Console : public ScriptWrappable, public RefCounted<Console>, public DOMWindowProperty {
public:
    static PassRefPtr<Console> create(Frame* frame) { return adoptRef(new Console(frame)); }
    virtual ~Console();

    void debug(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void error(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void info(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void log(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void clear(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void warn(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void dir(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void dirxml(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void table(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void trace(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void assertCondition(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>, bool condition);
    void count(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    const ProfilesArray& profiles() const { return m_profiles; }
    void profile(JSC::ExecState*, const String& = String());
    void profileEnd(JSC::ExecState*, const String& = String());
    void time(const String&);
    void timeEnd(JSC::ExecState*, const String&);
    void timeStamp(PassRefPtr<Inspector::ScriptArguments>);
    void group(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void groupCollapsed(JSC::ExecState*, PassRefPtr<Inspector::ScriptArguments>);
    void groupEnd();

private:
    inline Page* page() const;

    explicit Console(Frame*);

    ProfilesArray m_profiles;
};

} // namespace WebCore

#endif // Console_h
