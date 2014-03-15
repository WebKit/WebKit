/*
 * Copyright (C) 2007, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef InjectedScriptManager_h
#define InjectedScriptManager_h

#if ENABLE(INSPECTOR)

#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorEnvironment.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace Deprecated {
class ScriptObject;
}

namespace JSC {
class ExecState;
}

namespace Inspector {

class JS_EXPORT_PRIVATE InjectedScriptManager {
    WTF_MAKE_NONCOPYABLE(InjectedScriptManager); WTF_MAKE_FAST_ALLOCATED;
public:
    InjectedScriptManager(InspectorEnvironment&, PassRefPtr<InjectedScriptHost>);
    virtual ~InjectedScriptManager();

    virtual void disconnect();

    InjectedScriptHost* injectedScriptHost();
    InspectorEnvironment& inspectorEnvironment() const { return m_environment; }

    InjectedScript injectedScriptFor(JSC::ExecState*);
    InjectedScript injectedScriptForId(int);
    int injectedScriptIdFor(JSC::ExecState*);
    InjectedScript injectedScriptForObjectId(const String& objectId);
    void discardInjectedScripts();
    void releaseObjectGroup(const String& objectGroup);

protected:
    virtual void didCreateInjectedScript(InjectedScript);

    HashMap<int, InjectedScript> m_idToInjectedScript;
    HashMap<JSC::ExecState*, int> m_scriptStateToId;

private:
    String injectedScriptSource();
    Deprecated::ScriptObject createInjectedScript(const String& source, JSC::ExecState*, int id);

    InspectorEnvironment& m_environment;
    RefPtr<InjectedScriptHost> m_injectedScriptHost;
    int m_nextInjectedScriptId;
};

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // !defined(InjectedScriptManager_h)
