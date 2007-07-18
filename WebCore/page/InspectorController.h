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

#ifndef InspectorController_h
#define InspectorController_h

#include "Chrome.h"
#include <JavaScriptCore/JSContextRef.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class DocumentLoader;
class InspectorClient;
class Node;
class ResourceResponse;
class ResourceError;

struct ConsoleMessage;
struct InspectorResource;
struct ResourceRequest;

class InspectorController {
public:
    typedef HashMap<long long, RefPtr<InspectorResource> > ResourcesMap;
    typedef HashMap<RefPtr<Frame>, ResourcesMap*> FrameResourcesMap;

    InspectorController(Page*, InspectorClient*);
    ~InspectorController();

    void pageDestroyed() { m_page = 0; }

    bool enabled() const;

    Page* inspectedPage() const { return m_inspectedPage; }

    void inspect(Node*);
    void highlight(Node*);
    void hideHighlight();

    bool windowVisible();
    void setWindowVisible(bool visible = true);

    void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID);

    void attachWindow();
    void detachWindow();

    JSContextRef scriptContext() const { return m_scriptContext; };
    void setScriptContext(JSContextRef context) { m_scriptContext = context; };

    void windowScriptObjectAvailable();

    void scriptObjectReady();
    void windowUnloading();

    void populateScriptResources();
    void clearScriptResources();

    void didCommitLoad(DocumentLoader*);
    void frameDetachedFromParent(Frame*);

    void didLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length);

    void identifierForInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&);
    void willSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
    void didReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    void didReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived);
    void didFinishLoading(DocumentLoader*, unsigned long identifier);
    void didFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&);

    const ResourcesMap& resources() const { return m_resources; }

private:
    void focusNode();

    void addScriptConsoleMessage(const ConsoleMessage*);
    void clearScriptConsoleMessages();

    void clearNetworkTimeline();

    void addResource(InspectorResource*);
    void removeResource(InspectorResource*);

    JSObjectRef addScriptResource(InspectorResource*);
    void removeScriptResource(InspectorResource*);

    JSObjectRef addAndUpdateScriptResource(InspectorResource*);
    void updateScriptResourceRequest(InspectorResource*);
    void updateScriptResourceResponse(InspectorResource*);
    void updateScriptResource(InspectorResource*, int length);
    void updateScriptResource(InspectorResource*, bool finished, bool failed = false);
    void updateScriptResource(InspectorResource*, double startTime, double responseReceivedTime, double endTime);

    void pruneResources(ResourcesMap*, DocumentLoader* loaderToKeep = 0);
    void removeAllResources(ResourcesMap* map) { pruneResources(map); }

    Page* m_inspectedPage;
    InspectorClient* m_client;
    Page* m_page;
    RefPtr<Node> m_nodeToFocus;
    RefPtr<InspectorResource> m_mainResource;
    ResourcesMap m_resources;
    FrameResourcesMap m_frameResources;
    Vector<ConsoleMessage*> m_consoleMessages;
    JSObjectRef m_scriptObject;
    JSObjectRef m_controllerScriptObject;
    JSContextRef m_scriptContext;
    bool m_windowVisible;
    long long m_nextIdentifier;
};

} // namespace WebCore

#endif // !defined(InspectorController_h)
