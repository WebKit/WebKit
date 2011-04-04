/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorPageAgent_h
#define InspectorPageAgent_h

#if ENABLE(INSPECTOR)

#include "PlatformString.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class DOMWrapperWorld;
class Frame;
class Frontend;
class InjectedScriptManager;
class InspectorArray;
class InspectorFrontend;
class InstrumentingAgents;
class Page;

typedef String ErrorString;

class InspectorPageAgent {
    WTF_MAKE_NONCOPYABLE(InspectorPageAgent);
public:
    static PassOwnPtr<InspectorPageAgent> create(InstrumentingAgents*, Page*, InjectedScriptManager*);

    // Page API for InspectorFrontend
    void addScriptToEvaluateOnLoad(ErrorString*, const String& source);
    void removeAllScriptsToEvaluateOnLoad(ErrorString*);
    void reloadPage(ErrorString*, const bool* const optionalIgnoreCache);
    void openInInspectedWindow(ErrorString*, const String& url);
    void setUserAgentOverride(ErrorString*, const String& userAgent);
    void getCookies(ErrorString*, RefPtr<InspectorArray>* cookies, WTF::String* cookiesString);
    void deleteCookie(ErrorString*, const String& cookieName, const String& domain);

    // InspectorInstrumentation API
    void inspectedURLChanged(const String& url);
    void didCommitLoad(const String& url);
    void didClearWindowObjectInWorld(Frame*, DOMWrapperWorld*);
    void domContentEventFired();
    void loadEventFired();

    void setFrontend(InspectorFrontend*);
    void clearFrontend();
    void restore();
    void applyUserAgentOverride(String* userAgent) const;

private:
    InspectorPageAgent(InstrumentingAgents*, Page*, InjectedScriptManager*);

    InstrumentingAgents* m_instrumentingAgents;
    Page* m_inspectedPage;
    InjectedScriptManager* m_injectedScriptManager;
    InspectorFrontend* m_frontend;
    Vector<String> m_scriptsToEvaluateOnLoad;
    String m_userAgentOverride;
};


} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorPagerAgent_h)
