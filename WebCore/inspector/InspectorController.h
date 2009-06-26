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

#include "Console.h"
#include "PlatformString.h"
#include "ScriptState.h"
#include "StringHash.h"
#include "Timer.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "JavaScriptDebugListener.h"

namespace JSC {
    class Profile;
    class UString;
}
#endif

namespace WebCore {

class CachedResource;
class Database;
class DocumentLoader;
class GraphicsContext;
class HitTestResult;
class InspectorClient;
class InspectorFrontend;
class JavaScriptCallFrame;
class StorageArea;
class KURL;
class Node;
class Page;
struct ResourceRequest;
class ResourceResponse;
class ResourceError;
class ScriptCallStack;
class ScriptObject;
class ScriptString;
class SharedBuffer;

class ConsoleMessage;
class InspectorDatabaseResource;
class InspectorDOMStorageResource;
class InspectorResource;

class InspectorController : public RefCounted<InspectorController>
#if ENABLE(JAVASCRIPT_DEBUGGER)
                          , JavaScriptDebugListener
#endif
                                                    {
public:
    typedef HashMap<long long, RefPtr<InspectorResource> > ResourcesMap;
    typedef HashMap<RefPtr<Frame>, ResourcesMap*> FrameResourcesMap;
    typedef HashSet<RefPtr<InspectorDatabaseResource> > DatabaseResourcesSet;
    typedef HashSet<RefPtr<InspectorDOMStorageResource> > DOMStorageResourcesSet;

    typedef enum {
        CurrentPanel,
        ConsolePanel,
        DatabasesPanel,
        ElementsPanel,
        ProfilesPanel,
        ResourcesPanel,
        ScriptsPanel
    } SpecialPanels;

    struct Setting {
        enum Type {
            NoType, StringType, StringVectorType, DoubleType, IntegerType, BooleanType
        };

        Setting()
            : m_type(NoType)
        {
        }

        explicit Setting(bool value)
            : m_type(BooleanType)
        {
            m_simpleContent.m_boolean = value;
        }

        Type type() const { return m_type; }

        String string() const { ASSERT(m_type == StringType); return m_string; }
        const Vector<String>& stringVector() const { ASSERT(m_type == StringVectorType); return m_stringVector; }
        double doubleValue() const { ASSERT(m_type == DoubleType); return m_simpleContent.m_double; }
        long integerValue() const { ASSERT(m_type == IntegerType); return m_simpleContent.m_integer; }
        bool booleanValue() const { ASSERT(m_type == BooleanType); return m_simpleContent.m_boolean; }

        void set(const String& value) { m_type = StringType; m_string = value; }
        void set(const Vector<String>& value) { m_type = StringVectorType; m_stringVector = value; }
        void set(double value) { m_type = DoubleType; m_simpleContent.m_double = value; }
        void set(long value) { m_type = IntegerType; m_simpleContent.m_integer = value; }
        void set(bool value) { m_type = BooleanType; m_simpleContent.m_boolean = value; }

    private:
        Type m_type;

        String m_string;
        Vector<String> m_stringVector;

        union {
            double m_double;
            long m_integer;
            bool m_boolean;
        } m_simpleContent;
    };

    static PassRefPtr<InspectorController> create(Page* page, InspectorClient* inspectorClient)
    {
        return adoptRef(new InspectorController(page, inspectorClient));
    }

    ~InspectorController();

    void inspectedPageDestroyed();
    void pageDestroyed() { m_page = 0; }

    bool enabled() const;

    Page* inspectedPage() const { return m_inspectedPage; }

    const Setting& setting(const String& key) const;
    void setSetting(const String& key, const Setting&);

    String localizedStringsURL();
    String hiddenPanels();

    void inspect(Node*);
    void highlight(Node*);
    void hideHighlight();

    void show();
    void showPanel(SpecialPanels);
    void close();

    bool windowVisible();
    void setWindowVisible(bool visible = true, bool attached = false);

    void addResourceSourceToFrame(long identifier, Node* frame);
    bool addSourceToFrame(const String& mimeType, const String& source, Node*);
    void addMessageToConsole(MessageSource, MessageLevel, ScriptCallStack*);
    void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID);
    void clearConsoleMessages();

    void attachWindow();
    void detachWindow();

    void setAttachedWindow(bool);
    void setAttachedWindowHeight(unsigned height);

    void toggleSearchForNodeInPage();
    bool searchingForNodeInPage() { return m_searchingForNode; };
    void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);
    void handleMousePressOnNode(Node*);

    void inspectedWindowScriptObjectCleared(Frame*);
    void windowScriptObjectAvailable();

    void scriptObjectReady();
    void setFrontendProxyObject(ScriptState* state, ScriptObject object);

    void populateScriptObjects();
    void resetScriptObjects();

    void didCommitLoad(DocumentLoader*);
    void frameDetachedFromParent(Frame*);

    void didLoadResourceFromMemoryCache(DocumentLoader*, const CachedResource*);

    void identifierForInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&);
    void willSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
    void didReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    void didReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived);
    void didFinishLoading(DocumentLoader*, unsigned long identifier);
    void didFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&);
    void resourceRetrievedByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString);
    void scriptImported(unsigned long identifier, const String& sourceString);

    void enableResourceTracking(bool always = false);
    void disableResourceTracking(bool always = false);
    bool resourceTrackingEnabled() const { return m_resourceTrackingEnabled; }
    void ensureResourceTrackingSettingsLoaded();

#if ENABLE(DATABASE)
    void didOpenDatabase(Database*, const String& domain, const String& name, const String& version);
#endif
#if ENABLE(DOM_STORAGE)
    void didUseDOMStorage(StorageArea* storageArea, bool isLocalStorage, Frame* frame);
#endif

    const ResourcesMap& resources() const { return m_resources; }

    void moveWindowBy(float x, float y) const;
    void closeWindow();

    void drawNodeHighlight(GraphicsContext&) const;

    void count(const String& title, unsigned lineNumber, const String& sourceID);

    void startTiming(const String& title);
    bool stopTiming(const String& title, double& elapsed);

    void startGroup(MessageSource source, ScriptCallStack* callFrame);
    void endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL);

    const String& platform() const;

#if ENABLE(JAVASCRIPT_DEBUGGER)
    void addProfile(PassRefPtr<JSC::Profile>, unsigned lineNumber, const JSC::UString& sourceURL);
    void addProfileFinishedMessageToConsole(PassRefPtr<JSC::Profile>, unsigned lineNumber, const JSC::UString& sourceURL);
    void addStartProfilingMessageToConsole(const JSC::UString& title, unsigned lineNumber, const JSC::UString& sourceURL);
    void addScriptProfile(JSC::Profile*);
    const ProfilesArray& profiles() const { return m_profiles; }

    bool isRecordingUserInitiatedProfile() const { return m_recordingUserInitiatedProfile; }

    JSC::UString getCurrentUserInitiatedProfileName(bool incrementProfileNumber);
    void startUserInitiatedProfilingSoon();
    void startUserInitiatedProfiling(Timer<InspectorController>* = 0);
    void stopUserInitiatedProfiling();
    void toggleRecordButton(bool);

    void enableProfiler(bool always = false, bool skipRecompile = false);
    void disableProfiler(bool always = false);
    bool profilerEnabled() const { return enabled() && m_profilerEnabled; }

    void enableDebuggerFromFrontend(bool always);
    void enableDebugger();
    void disableDebugger(bool always = false);
    bool debuggerEnabled() const { return m_debuggerEnabled; }

    JavaScriptCallFrame* currentCallFrame() const;

    void addBreakpoint(const String& sourceID, unsigned lineNumber);
    void removeBreakpoint(const String& sourceID, unsigned lineNumber);

    bool pauseOnExceptions();
    void setPauseOnExceptions(bool pause);

    void pauseInDebugger();
    void resumeDebugger();

    void stepOverStatementInDebugger();
    void stepIntoStatementInDebugger();
    void stepOutOfFunctionInDebugger();

    virtual void didParseSource(JSC::ExecState*, const JSC::SourceCode&);
    virtual void failedToParseSource(JSC::ExecState*, const JSC::SourceCode&, int errorLine, const JSC::UString& errorMessage);
    virtual void didPause();
    virtual void didContinue();
#endif

private:
    InspectorController(Page*, InspectorClient*);

    void focusNode();

    void addConsoleMessage(ScriptState*, ConsoleMessage*);

    void addResource(InspectorResource*);
    void removeResource(InspectorResource*);
    InspectorResource* getTrackedResource(long long identifier);

    void pruneResources(ResourcesMap*, DocumentLoader* loaderToKeep = 0);
    void removeAllResources(ResourcesMap* map) { pruneResources(map); }

    void showWindow();

    bool isMainResourceLoader(DocumentLoader* loader, const KURL& requestUrl);

    Page* m_inspectedPage;
    InspectorClient* m_client;
    OwnPtr<InspectorFrontend> m_frontend;
    Page* m_page;
    RefPtr<Node> m_nodeToFocus;
    RefPtr<InspectorResource> m_mainResource;
    ResourcesMap m_resources;
    HashSet<String> m_knownResources;
    FrameResourcesMap m_frameResources;
    Vector<ConsoleMessage*> m_consoleMessages;
    HashMap<String, double> m_times;
    HashMap<String, unsigned> m_counts;
#if ENABLE(DATABASE)
    DatabaseResourcesSet m_databaseResources;
#endif
#if ENABLE(DOM_STORAGE)
    DOMStorageResourcesSet m_domStorageResources;
#endif
    ScriptState* m_scriptState;
    bool m_windowVisible;
    SpecialPanels m_showAfterVisible;
    long long m_nextIdentifier;
    RefPtr<Node> m_highlightedNode;
    unsigned m_groupLevel;
    bool m_searchingForNode;
    ConsoleMessage* m_previousMessage;
    bool m_resourceTrackingEnabled;
    bool m_resourceTrackingSettingsLoaded;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    bool m_debuggerEnabled;
    bool m_attachDebuggerWhenShown;
    bool m_profilerEnabled;
    bool m_recordingUserInitiatedProfile;
    int m_currentUserInitiatedProfileNumber;
    unsigned m_nextUserInitiatedProfileNumber;
    Timer<InspectorController> m_startProfiling;
    ProfilesArray m_profiles;
#endif
};

} // namespace WebCore

#endif // !defined(InspectorController_h)
