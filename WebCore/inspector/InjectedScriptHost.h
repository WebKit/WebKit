/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef InjectedScriptHost_h
#define InjectedScriptHost_h

#include "Console.h"
#include "InspectorController.h"
#include "PlatformString.h"

#include <wtf/RefCounted.h>

namespace WebCore {

class Database;
class InspectorDOMAgent;
class InspectorFrontend;
class JavaScriptCallFrame;
class Node;
class Storage;

class InjectedScriptHost : public RefCounted<InjectedScriptHost>
{
public:
    static PassRefPtr<InjectedScriptHost> create(InspectorController* inspectorController)
    {
        return adoptRef(new InjectedScriptHost(inspectorController));
    }

    ~InjectedScriptHost();

    InspectorController* inspectorController() { return m_inspectorController; }
    void disconnectController() { m_inspectorController = 0; }

    void copyText(const String& text);
    Node* nodeForId(long nodeId);
    ScriptValue wrapObject(const ScriptValue& object, const String& objectGroup);
    ScriptValue unwrapObject(const String& objectId);
    long pushNodePathToFrontend(Node* node, bool selectInUI);

    void addNodesToSearchResult(const String& nodeIds);
    long pushNodeByPathToFrontend(const String& path);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    JavaScriptCallFrame* currentCallFrame() const;
#endif
#if ENABLE(DATABASE)
    Database* databaseForId(long databaseId);
    void selectDatabase(Database* database);
#endif
#if ENABLE(DOM_STORAGE)
    void selectDOMStorage(Storage* storage);
#endif
    void reportDidDispatchOnInjectedScript(long callId, const String& result, bool isException);

private:
    InjectedScriptHost(InspectorController* inspectorController);
    InspectorDOMAgent* inspectorDOMAgent();
    InspectorFrontend* inspectorFrontend();

    InspectorController* m_inspectorController;
};

} // namespace WebCore

#endif // !defined(InjectedScriptHost_h)
