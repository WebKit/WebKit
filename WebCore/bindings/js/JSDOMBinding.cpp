/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSDOMBinding.h"

#include "ActiveDOMObject.h"
#include "DOMCoreException.h"
#include "Document.h"
#include "EventException.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "HTMLAudioElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLScriptElement.h"
#include "HTMLNames.h"
#include "JSDOMCoreException.h"
#include "JSDOMWindowCustom.h"
#include "JSEventException.h"
#include "JSNode.h"
#include "JSRangeException.h"
#include "JSXMLHttpRequestException.h"
#include "KURL.h"
#include "MessagePort.h"
#include "RangeException.h"
#include "ScriptController.h"
#include "XMLHttpRequestException.h"
#include <runtime/Error.h>
#include <runtime/JSFunction.h>
#include <runtime/PrototypeFunction.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(SVG)
#include "JSSVGException.h"
#include "SVGException.h"
#endif

#if ENABLE(XPATH)
#include "JSXPathException.h"
#include "XPathException.h"
#endif

#if ENABLE(WORKERS)
#include <wtf/ThreadSpecific.h>
using namespace WTF;
#endif

using namespace JSC;

namespace WebCore {

using namespace HTMLNames;

typedef Document::JSWrapperCache JSWrapperCache;

// For debugging, keep a set of wrappers currently registered, and check that
// all are unregistered before they are destroyed. This has helped us fix at
// least one bug.

static void addWrapper(DOMObject* wrapper);
static void removeWrapper(DOMObject* wrapper);
static void removeWrappers(const JSWrapperCache& wrappers);

#ifdef NDEBUG

static inline void addWrapper(DOMObject*)
{
}

static inline void removeWrapper(DOMObject*)
{
}

static inline void removeWrappers(const JSWrapperCache&)
{
}

#else

static HashSet<DOMObject*>& wrapperSet()
{
#if ENABLE(WORKERS)
    DEFINE_STATIC_LOCAL(ThreadSpecific<HashSet<DOMObject*> >, staticWrapperSet, ());
    return *staticWrapperSet;
#else
    DEFINE_STATIC_LOCAL(HashSet<DOMObject*>, staticWrapperSet, ());
    return staticWrapperSet;
#endif
}

static void addWrapper(DOMObject* wrapper)
{
    ASSERT(!wrapperSet().contains(wrapper));
    wrapperSet().add(wrapper);
}

static void removeWrapper(DOMObject* wrapper)
{
    if (!wrapper)
        return;
    ASSERT(wrapperSet().contains(wrapper));
    wrapperSet().remove(wrapper);
}

static void removeWrappers(const JSWrapperCache& wrappers)
{
    for (JSWrapperCache::const_iterator it = wrappers.begin(); it != wrappers.end(); ++it)
        removeWrapper(it->second);
}

DOMObject::~DOMObject()
{
    ASSERT(!wrapperSet().contains(this));
}

#endif

class DOMObjectWrapperMap {
public:
    static DOMObjectWrapperMap& mapFor(JSGlobalData&);

    DOMObject* get(void* objectHandle)
    {
        return m_map.get(objectHandle);
    }

    void set(void* objectHandle, DOMObject* wrapper)
    {
        addWrapper(wrapper);
        m_map.set(objectHandle, wrapper);
    }

    void remove(void* objectHandle)
    {
        removeWrapper(m_map.take(objectHandle));
    }

private:
    HashMap<void*, DOMObject*> m_map;
};

// Map from static HashTable instances to per-GlobalData ones.
class DOMObjectHashTableMap {
public:
    static DOMObjectHashTableMap& mapFor(JSGlobalData&);

    ~DOMObjectHashTableMap()
    {
        HashMap<const JSC::HashTable*, JSC::HashTable>::iterator mapEnd = m_map.end();
        for (HashMap<const JSC::HashTable*, JSC::HashTable>::iterator iter = m_map.begin(); iter != m_map.end(); ++iter)
            iter->second.deleteTable();
    }

    const JSC::HashTable* get(const JSC::HashTable* staticTable)
    {
        HashMap<const JSC::HashTable*, JSC::HashTable>::iterator iter = m_map.find(staticTable);
        if (iter != m_map.end())
            return &iter->second;
        return &m_map.set(staticTable, JSC::HashTable(*staticTable)).first->second;
    }

private:
    HashMap<const JSC::HashTable*, JSC::HashTable> m_map;
};

class WebCoreJSClientData : public JSGlobalData::ClientData {
public:
    DOMObjectHashTableMap hashTableMap;
    DOMObjectWrapperMap wrapperMap;
};

DOMObjectHashTableMap& DOMObjectHashTableMap::mapFor(JSGlobalData& globalData)
{
    JSGlobalData::ClientData* clientData = globalData.clientData;
    if (!clientData) {
        clientData = new WebCoreJSClientData;
        globalData.clientData = clientData;
    }
    return static_cast<WebCoreJSClientData*>(clientData)->hashTableMap;
}

const JSC::HashTable* getHashTableForGlobalData(JSGlobalData& globalData, const JSC::HashTable* staticTable)
{
    return DOMObjectHashTableMap::mapFor(globalData).get(staticTable);
}

inline DOMObjectWrapperMap& DOMObjectWrapperMap::mapFor(JSGlobalData& globalData)
{
    JSGlobalData::ClientData* clientData = globalData.clientData;
    if (!clientData) {
        clientData = new WebCoreJSClientData;
        globalData.clientData = clientData;
    }
    return static_cast<WebCoreJSClientData*>(clientData)->wrapperMap;
}

DOMObject* getCachedDOMObjectWrapper(JSGlobalData& globalData, void* objectHandle) 
{
    return DOMObjectWrapperMap::mapFor(globalData).get(objectHandle);
}

void cacheDOMObjectWrapper(JSGlobalData& globalData, void* objectHandle, DOMObject* wrapper) 
{
    DOMObjectWrapperMap::mapFor(globalData).set(objectHandle, wrapper);
}

void forgetDOMObject(JSGlobalData& globalData, void* objectHandle)
{
    DOMObjectWrapperMap::mapFor(globalData).remove(objectHandle);
}

JSNode* getCachedDOMNodeWrapper(Document* document, Node* node)
{
    if (!document)
        return static_cast<JSNode*>(DOMObjectWrapperMap::mapFor(*JSDOMWindow::commonJSGlobalData()).get(node));
    return document->wrapperCache().get(node);
}

void forgetDOMNode(Document* document, Node* node)
{
    if (!document) {
        DOMObjectWrapperMap::mapFor(*JSDOMWindow::commonJSGlobalData()).remove(node);
        return;
    }
    removeWrapper(document->wrapperCache().take(node));
}

void cacheDOMNodeWrapper(Document* document, Node* node, JSNode* wrapper)
{
    if (!document) {
        DOMObjectWrapperMap::mapFor(*JSDOMWindow::commonJSGlobalData()).set(node, wrapper);
        return;
    }
    addWrapper(wrapper);
    document->wrapperCache().set(node, wrapper);
}

void forgetAllDOMNodesForDocument(Document* document)
{
    ASSERT(document);
    removeWrappers(document->wrapperCache());
}

static inline bool isObservableThroughDOM(JSNode* jsNode)
{
    // Certain conditions implicitly make a JS DOM node wrapper observable
    // through the DOM, even if no explicit reference to it remains.

    Node* node = jsNode->impl();

    if (node->inDocument()) {
        // If a node is in the document, and its wrapper has custom properties,
        // the wrapper is observable because future access to the node through the
        // DOM must reflect those properties.
        if (jsNode->hasCustomProperties())
            return true;

        // If a node is in the document, and has event listeners, its wrapper is
        // observable because its wrapper is responsible for marking those event listeners.
        if (node->eventListeners().size())
            return true; // Technically, we may overzealously mark a wrapper for a node that has only non-JS event listeners. Oh well.

        // If a node owns another object with a wrapper with custom properties,
        // the wrapper must be treated as observable, because future access to
        // those objects through the DOM must reflect those properties.
        // FIXME: It would be better if this logic could be in the node next to
        // the custom markChildren functions rather than here.
        if (node->isElementNode()) {
            if (NamedNodeMap* attributes = static_cast<Element*>(node)->attributeMap()) {
                if (DOMObject* wrapper = getCachedDOMObjectWrapper(*jsNode->globalObject()->globalData(), attributes)) {
                    if (wrapper->hasCustomProperties())
                        return true;
                }
            }
            if (node->isStyledElement()) {
                if (CSSMutableStyleDeclaration* style = static_cast<StyledElement*>(node)->inlineStyleDecl()) {
                    if (DOMObject* wrapper = getCachedDOMObjectWrapper(*jsNode->globalObject()->globalData(), style)) {
                        if (wrapper->hasCustomProperties())
                            return true;
                    }
                }
            }
            if (static_cast<Element*>(node)->hasTagName(canvasTag)) {
                if (CanvasRenderingContext* context = static_cast<HTMLCanvasElement*>(node)->renderingContext()) {
                    if (DOMObject* wrapper = getCachedDOMObjectWrapper(*jsNode->globalObject()->globalData(), context)) {
                        if (wrapper->hasCustomProperties())
                            return true;
                    }
                }
            }
        }
    } else {
        // If a wrapper is the last reference to an image or script element
        // that is loading but not in the document, the wrapper is observable
        // because it is the only thing keeping the image element alive, and if
        // the image element is destroyed, its load event will not fire.
        // FIXME: The DOM should manage this issue without the help of JavaScript wrappers.
        if (node->hasTagName(imgTag) && !static_cast<HTMLImageElement*>(node)->haveFiredLoadEvent())
            return true;
        if (node->hasTagName(scriptTag) && !static_cast<HTMLScriptElement*>(node)->haveFiredLoadEvent())
            return true;
#if ENABLE(VIDEO)
        if (node->hasTagName(audioTag) && !static_cast<HTMLAudioElement*>(node)->paused())
            return true;
#endif
    }

    return false;
}

void markDOMNodesForDocument(MarkStack& markStack, Document* doc)
{
    JSWrapperCache& nodeDict = doc->wrapperCache();
    JSWrapperCache::iterator nodeEnd = nodeDict.end();
    for (JSWrapperCache::iterator nodeIt = nodeDict.begin(); nodeIt != nodeEnd; ++nodeIt) {
        JSNode* jsNode = nodeIt->second;
        if (isObservableThroughDOM(jsNode))
            markStack.append(jsNode);
    }
}

void markActiveObjectsForContext(MarkStack& markStack, JSGlobalData& globalData, ScriptExecutionContext* scriptExecutionContext)
{
    // If an element has pending activity that may result in event listeners being called
    // (e.g. an XMLHttpRequest), we need to keep JS wrappers alive.

    const HashMap<ActiveDOMObject*, void*>& activeObjects = scriptExecutionContext->activeDOMObjects();
    HashMap<ActiveDOMObject*, void*>::const_iterator activeObjectsEnd = activeObjects.end();
    for (HashMap<ActiveDOMObject*, void*>::const_iterator iter = activeObjects.begin(); iter != activeObjectsEnd; ++iter) {
        if (iter->first->hasPendingActivity()) {
            DOMObject* wrapper = getCachedDOMObjectWrapper(globalData, iter->second);
            // Generally, an active object with pending activity must have a wrapper to mark its listeners.
            // However, some ActiveDOMObjects don't have JS wrappers (timers created by setTimeout is one example).
            // FIXME: perhaps need to make sure even timers have a markable 'wrapper'.
            if (wrapper)
                markStack.append(wrapper);
        }
    }

    const HashSet<MessagePort*>& messagePorts = scriptExecutionContext->messagePorts();
    HashSet<MessagePort*>::const_iterator portsEnd = messagePorts.end();
    for (HashSet<MessagePort*>::const_iterator iter = messagePorts.begin(); iter != portsEnd; ++iter) {
        // If the message port is remotely entangled, then always mark it as in-use because we can't determine reachability across threads.
        if (!(*iter)->locallyEntangledPort() || (*iter)->hasPendingActivity()) {
            DOMObject* wrapper = getCachedDOMObjectWrapper(globalData, *iter);
            if (wrapper)
                markStack.append(wrapper);
        }
    }
}

void updateDOMNodeDocument(Node* node, Document* oldDocument, Document* newDocument)
{
    ASSERT(oldDocument != newDocument);
    JSNode* wrapper = getCachedDOMNodeWrapper(oldDocument, node);
    if (!wrapper)
        return;
    removeWrapper(wrapper);
    cacheDOMNodeWrapper(newDocument, node, wrapper);
    forgetDOMNode(oldDocument, node);
    addWrapper(wrapper);
}

void markDOMObjectWrapper(MarkStack& markStack, JSGlobalData& globalData, void* object)
{
    // FIXME: This could be changed to only mark wrappers that are "observable"
    // as markDOMNodesForDocument does, allowing us to collect more wrappers,
    // but doing this correctly would be challenging.
    if (!object)
        return;
    DOMObject* wrapper = getCachedDOMObjectWrapper(globalData, object);
    if (!wrapper)
        return;
    markStack.append(wrapper);
}

JSValue jsStringOrNull(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsNull();
    return jsString(exec, s);
}

JSValue jsOwnedStringOrNull(ExecState* exec, const UString& s)
{
    if (s.isNull())
        return jsNull();
    return jsOwnedString(exec, s);
}

JSValue jsStringOrUndefined(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsUndefined();
    return jsString(exec, s);
}

JSValue jsStringOrFalse(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsBoolean(false);
    return jsString(exec, s);
}

JSValue jsStringOrNull(ExecState* exec, const KURL& url)
{
    if (url.isNull())
        return jsNull();
    return jsString(exec, url.string());
}

JSValue jsStringOrUndefined(ExecState* exec, const KURL& url)
{
    if (url.isNull())
        return jsUndefined();
    return jsString(exec, url.string());
}

JSValue jsStringOrFalse(ExecState* exec, const KURL& url)
{
    if (url.isNull())
        return jsBoolean(false);
    return jsString(exec, url.string());
}

UString valueToStringWithNullCheck(ExecState* exec, JSValue value)
{
    if (value.isNull())
        return UString();
    return value.toString(exec);
}

UString valueToStringWithUndefinedOrNullCheck(ExecState* exec, JSValue value)
{
    if (value.isUndefinedOrNull())
        return UString();
    return value.toString(exec);
}

void reportException(ExecState* exec, JSValue exception)
{
    UString errorMessage = exception.toString(exec);
    JSObject* exceptionObject = exception.toObject(exec);
    int lineNumber = exceptionObject->get(exec, Identifier(exec, "line")).toInt32(exec);
    UString exceptionSourceURL = exceptionObject->get(exec, Identifier(exec, "sourceURL")).toString(exec);
    exec->clearException();

    ScriptExecutionContext* scriptExecutionContext = static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject())->scriptExecutionContext();
    ASSERT(scriptExecutionContext);

    // Crash data indicates null-dereference crashes at this point in the Safari 4 Public Beta.
    // It's harmless to return here without reporting the exception to the log and the debugger in this case.
    if (!scriptExecutionContext)
        return;

    scriptExecutionContext->reportException(errorMessage, lineNumber, exceptionSourceURL);
}

void reportCurrentException(ExecState* exec)
{
    JSValue exception = exec->exception();
    exec->clearException();
    reportException(exec, exception);
}

void setDOMException(ExecState* exec, ExceptionCode ec)
{
    if (!ec || exec->hadException())
        return;

    // FIXME: All callers to setDOMException need to pass in the right global object
    // for now, we're going to assume the lexicalGlobalObject.  Which is wrong in cases like this:
    // frames[0].document.createElement(null, null); // throws an exception which should have the subframes prototypes.
    JSDOMGlobalObject* globalObject = deprecatedGlobalObjectForPrototype(exec);

    ExceptionCodeDescription description;
    getExceptionCodeDescription(ec, description);

    JSValue errorObject;
    switch (description.type) {
        case DOMExceptionType:
            errorObject = toJS(exec, globalObject, DOMCoreException::create(description));
            break;
        case RangeExceptionType:
            errorObject = toJS(exec, globalObject, RangeException::create(description));
            break;
        case EventExceptionType:
            errorObject = toJS(exec, globalObject, EventException::create(description));
            break;
        case XMLHttpRequestExceptionType:
            errorObject = toJS(exec, globalObject, XMLHttpRequestException::create(description));
            break;
#if ENABLE(SVG)
        case SVGExceptionType:
            errorObject = toJS(exec, globalObject, SVGException::create(description).get(), 0);
            break;
#endif
#if ENABLE(XPATH)
        case XPathExceptionType:
            errorObject = toJS(exec, globalObject, XPathException::create(description));
            break;
#endif
    }

    ASSERT(errorObject);
    exec->setException(errorObject);
}

bool checkNodeSecurity(ExecState* exec, Node* node)
{
    return node && allowsAccessFromFrame(exec, node->document()->frame());
}

bool allowsAccessFromFrame(ExecState* exec, Frame* frame)
{
    if (!frame)
        return false;
    JSDOMWindow* window = toJSDOMWindow(frame);
    return window && window->allowsAccessFrom(exec);
}

bool allowsAccessFromFrame(ExecState* exec, Frame* frame, String& message)
{
    if (!frame)
        return false;
    JSDOMWindow* window = toJSDOMWindow(frame);
    return window && window->allowsAccessFrom(exec, message);
}

bool shouldAllowNavigation(ExecState* exec, Frame* frame)
{
    Frame* lexicalFrame = toLexicalFrame(exec);
    return lexicalFrame && lexicalFrame->loader()->shouldAllowNavigation(frame);
}

void printErrorMessageForFrame(Frame* frame, const String& message)
{
    if (!frame)
        return;
    if (JSDOMWindow* window = toJSDOMWindow(frame))
        window->printErrorMessage(message);
}

Frame* toLexicalFrame(ExecState* exec)
{
    return asJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame();
}

Frame* toDynamicFrame(ExecState* exec)
{
    return asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
}

bool processingUserGesture(ExecState* exec)
{
    Frame* frame = toDynamicFrame(exec);
    return frame && frame->script()->processingUserGesture();
}

KURL completeURL(ExecState* exec, const String& relativeURL)
{
    // For histoical reasons, we need to complete the URL using the dynamic frame.
    Frame* frame = toDynamicFrame(exec);
    if (!frame)
        return KURL();
    return frame->loader()->completeURL(relativeURL);
}

JSValue objectToStringFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) NativeFunctionWrapper(exec, exec->lexicalGlobalObject()->prototypeFunctionStructure(), 0, propertyName, objectProtoFuncToString);
}

Structure* getCachedDOMStructure(JSDOMGlobalObject* globalObject, const ClassInfo* classInfo)
{
    JSDOMStructureMap& structures = globalObject->structures();
    return structures.get(classInfo).get();
}

Structure* cacheDOMStructure(JSDOMGlobalObject* globalObject, PassRefPtr<Structure> structure, const ClassInfo* classInfo)
{
    JSDOMStructureMap& structures = globalObject->structures();
    ASSERT(!structures.contains(classInfo));
    return structures.set(classInfo, structure).first->second.get();
}

Structure* getCachedDOMStructure(ExecState* exec, const ClassInfo* classInfo)
{
    return getCachedDOMStructure(static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject()), classInfo);
}

Structure* cacheDOMStructure(ExecState* exec, PassRefPtr<Structure> structure, const ClassInfo* classInfo)
{
    return cacheDOMStructure(static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject()), structure, classInfo);
}

JSObject* getCachedDOMConstructor(ExecState* exec, const ClassInfo* classInfo)
{
    JSDOMConstructorMap& constructors = static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject())->constructors();
    return constructors.get(classInfo);
}

void cacheDOMConstructor(ExecState* exec, const ClassInfo* classInfo, JSObject* constructor)
{
    JSDOMConstructorMap& constructors = static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject())->constructors();
    ASSERT(!constructors.contains(classInfo));
    constructors.set(classInfo, constructor);
}

JSC::JSObject* toJSSequence(ExecState* exec, JSValue value, unsigned& length)
{
    JSObject* object = value.getObject();
    if (!object) {
        throwError(exec, TypeError);
        return 0;
    }
    JSValue lengthValue = object->get(exec, exec->propertyNames().length);
    if (exec->hadException())
        return 0;

    if (lengthValue.isUndefinedOrNull()) {
        throwError(exec, TypeError);
        return 0;
    }

    length = lengthValue.toUInt32(exec);
    if (exec->hadException())
        return 0;

    return object;
}

bool DOMObject::defineOwnProperty(ExecState* exec, const Identifier&, PropertyDescriptor&, bool)
{
    throwError(exec, TypeError, "defineProperty is not supported on DOM Objects");
    return false;
}

} // namespace WebCore
