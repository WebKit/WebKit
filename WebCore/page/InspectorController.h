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

#include "JavaScriptDebugListener.h"

#include "Console.h"
#include "PlatformString.h"
#include "StringHash.h"
#include <JavaScriptCore/JSContextRef.h>
#include <profiler/Profiler.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace KJS {
    class Profile;
    class UString;
}

namespace WebCore {

class Database;
class DocumentLoader;
class GraphicsContext;
class InspectorClient;
class JavaScriptCallFrame;
class Node;
class Page;
class ResourceResponse;
class ResourceError;
class SharedBuffer;

struct ConsoleMessage;
struct InspectorDatabaseResource;
struct InspectorResource;
class ResourceRequest;

class InspectorController : JavaScriptDebugListener, public KJS::ProfilerClient {
public:
    typedef HashMap<long long, RefPtr<InspectorResource> > ResourcesMap;
    typedef HashMap<RefPtr<Frame>, ResourcesMap*> FrameResourcesMap;
    typedef HashSet<RefPtr<InspectorDatabaseResource> > DatabaseResourcesSet;

    typedef enum {
        CurrentPanel,
        ConsolePanel,
        DatabasesPanel,
        ElementsPanel,
        ProfilesPanel,
        ResourcesPanel,
        ScriptsPanel
    } SpecialPanels;

    InspectorController(Page*, InspectorClient*);
    ~InspectorController();

    void inspectedPageDestroyed();
    void pageDestroyed() { m_page = 0; }

    bool enabled() const;

    Page* inspectedPage() const { return m_inspectedPage; }

    String localizedStringsURL();

    void inspect(Node*);
    void highlight(Node*);
    void hideHighlight();

    void show();
    void showPanel(SpecialPanels);
    void close();

    bool isRecordingUserInitiatedProfile() const { return m_recordingUserInitiatedProfile; }
    void startUserInitiatedProfiling();
    void stopUserInitiatedProfiling();
    void finishedProfiling(PassRefPtr<KJS::Profile>);

    bool windowVisible();
    void setWindowVisible(bool visible = true);

    void addMessageToConsole(MessageSource, MessageLevel, KJS::ExecState*, const KJS::ArgList& arguments, unsigned lineNumber, const String& sourceID);
    void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID);

    void addProfile(PassRefPtr<KJS::Profile>);
    void addScriptProfile(KJS::Profile* profile);
    const Vector<RefPtr<KJS::Profile> >& profiles() const { return m_profiles; }

    void attachWindow();
    void detachWindow();

    JSContextRef scriptContext() const { return m_scriptContext; };
    void setScriptContext(JSContextRef context) { m_scriptContext = context; };

    void windowScriptObjectAvailable();

    void scriptObjectReady();

    void populateScriptObjects();
    void resetScriptObjects();

    void didCommitLoad(DocumentLoader*);
    void frameDetachedFromParent(Frame*);

    void didLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length);

    void identifierForInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&);
    void willSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
    void didReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    void didReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived);
    void didFinishLoading(DocumentLoader*, unsigned long identifier);
    void didFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&);
    void resourceRetrievedByXMLHttpRequest(unsigned long identifier, KJS::UString& sourceString);

#if ENABLE(DATABASE)
    void didOpenDatabase(Database*, const String& domain, const String& name, const String& version);
#endif

    const ResourcesMap& resources() const { return m_resources; }

    void moveWindowBy(float x, float y) const;

    void startDebuggingAndReloadInspectedPage();
    void stopDebugging();
    bool debuggerAttached() const { return m_debuggerAttached; }

    JavaScriptCallFrame* currentCallFrame() const;

    void addBreakpoint(int sourceID, unsigned lineNumber);
    void removeBreakpoint(int sourceID, unsigned lineNumber);

    bool pauseOnExceptions();
    void setPauseOnExceptions(bool pause);

    void pauseInDebugger();
    void resumeDebugger();

    void stepOverStatementInDebugger();
    void stepIntoStatementInDebugger();
    void stepOutOfFunctionInDebugger();

    void drawNodeHighlight(GraphicsContext&) const;

private:
    void focusNode();

    void addConsoleMessage(ConsoleMessage*);
    void addScriptConsoleMessage(const ConsoleMessage*);

    void addResource(InspectorResource*);
    void removeResource(InspectorResource*);

    JSObjectRef addScriptResource(InspectorResource*);
    void removeScriptResource(InspectorResource*);

    JSObjectRef addAndUpdateScriptResource(InspectorResource*);
    void updateScriptResourceRequest(InspectorResource*);
    void updateScriptResourceResponse(InspectorResource*);
    void updateScriptResourceType(InspectorResource*);
    void updateScriptResource(InspectorResource*, int length);
    void updateScriptResource(InspectorResource*, bool finished, bool failed = false);
    void updateScriptResource(InspectorResource*, double startTime, double responseReceivedTime, double endTime);

    void pruneResources(ResourcesMap*, DocumentLoader* loaderToKeep = 0);
    void removeAllResources(ResourcesMap* map) { pruneResources(map); }

#if ENABLE(DATABASE)
    JSObjectRef addDatabaseScriptResource(InspectorDatabaseResource*);
    void removeDatabaseScriptResource(InspectorDatabaseResource*);
#endif

    JSValueRef callSimpleFunction(JSContextRef, JSObjectRef thisObject, const char* functionName) const;
    JSValueRef callFunction(JSContextRef, JSObjectRef thisObject, const char* functionName, size_t argumentCount, const JSValueRef arguments[], JSValueRef& exception) const;

    bool handleException(JSContextRef, JSValueRef exception, unsigned lineNumber) const;

    void showWindow();
    void closeWindow();

    virtual void didParseSource(KJS::ExecState*, const KJS::SourceProvider& source, int startingLineNumber, const KJS::UString& sourceURL, int sourceID);
    virtual void failedToParseSource(KJS::ExecState*, const KJS::SourceProvider& source, int startingLineNumber, const KJS::UString& sourceURL, int errorLine, const KJS::UString& errorMessage);
    virtual void didPause();

    Page* m_inspectedPage;
    InspectorClient* m_client;
    Page* m_page;
    RefPtr<Node> m_nodeToFocus;
    RefPtr<InspectorResource> m_mainResource;
    ResourcesMap m_resources;
    HashSet<String> m_knownResources;
    FrameResourcesMap m_frameResources;
    Vector<ConsoleMessage*> m_consoleMessages;
    Vector<RefPtr<KJS::Profile> > m_profiles;
#if ENABLE(DATABASE)
    DatabaseResourcesSet m_databaseResources;
#endif
    JSObjectRef m_scriptObject;
    JSObjectRef m_controllerScriptObject;
    JSContextRef m_scriptContext;
    bool m_windowVisible;
    bool m_debuggerAttached;
    bool m_attachDebuggerWhenShown;
    bool m_recordingUserInitiatedProfile;
    SpecialPanels m_showAfterVisible;
    long long m_nextIdentifier;
    RefPtr<Node> m_highlightedNode;
};

} // namespace WebCore

#endif // !defined(InspectorController_h)
