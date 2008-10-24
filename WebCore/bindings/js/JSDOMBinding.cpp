/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

// gcc 3.x can't handle including the HashMap pointer specialization in this file
#if defined __GNUC__ && !defined __GLIBCXX__ // less than gcc 3.4
#define HASH_MAP_PTR_SPEC_WORKAROUND 1
#endif

#include "config.h"
#include "JSDOMBinding.h"

#include "ActiveDOMObject.h"
#include "DOMCoreException.h"
#include "Document.h"
#include "EventException.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "JSDOMCoreException.h"
#include "JSDOMGlobalObject.h"
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
#include <kjs/PrototypeFunction.h>

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
    static ThreadSpecific<HashSet<DOMObject*> > staticWrapperSet;
    return *staticWrapperSet;
#else
    static HashSet<DOMObject*> staticWrapperSet;
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

void markDOMNodesForDocument(Document* doc)
{
    // If a node's JS wrapper holds custom properties, those properties must
    // persist every time the node is fetched from the DOM. So, we keep JS
    // wrappers like that from being garbage collected.

    JSWrapperCache& nodeDict = doc->wrapperCache();
    JSWrapperCache::iterator nodeEnd = nodeDict.end();
    for (JSWrapperCache::iterator nodeIt = nodeDict.begin(); nodeIt != nodeEnd; ++nodeIt) {
        JSNode* jsNode = nodeIt->second;
        Node* node = jsNode->impl();

        if (jsNode->marked())
            continue;

        // No need to preserve a wrapper that has no custom properties or is no
        // longer fetchable through the DOM.
        if (!jsNode->hasCustomProperties() || !node->inDocument()) {
            //... unless the wrapper wraps a loading image, since the "new Image"
            // syntax allows an orphan image wrapper to be the last reference
            // to a loading image, whose load event might have important side-effects.
            if (!node->hasTagName(imgTag) || static_cast<HTMLImageElement*>(node)->haveFiredLoadEvent())
                continue;
        }

        jsNode->mark();
    }
}

void markActiveObjectsForDocument(JSGlobalData& globalData, Document* doc)
{
    // If an element has pending activity that may result in listeners being called
    // (e.g. an XMLHttpRequest), we need to keep all JS wrappers alive.

    const HashMap<ActiveDOMObject*, void*>& activeObjects = doc->activeDOMObjects();
    HashMap<ActiveDOMObject*, void*>::const_iterator activeObjectsEnd = activeObjects.end();
    for (HashMap<ActiveDOMObject*, void*>::const_iterator iter = activeObjects.begin(); iter != activeObjectsEnd; ++iter) {
        if (iter->first->hasPendingActivity()) {
            DOMObject* wrapper = getCachedDOMObjectWrapper(globalData, iter->second);
            // An object with pending activity must have a wrapper to mark its listeners, so no null check.
            if (!wrapper->marked())
                wrapper->mark();
        }
    }

    const HashSet<MessagePort*>& messagePorts = doc->messagePorts();
    HashSet<MessagePort*>::const_iterator portsEnd = messagePorts.end();
    for (HashSet<MessagePort*>::const_iterator iter = messagePorts.begin(); iter != portsEnd; ++iter) {
        if ((*iter)->hasPendingActivity()) {
            DOMObject* wrapper = getCachedDOMObjectWrapper(globalData, *iter);
            // An object with pending activity must have a wrapper to mark its listeners, so no null check.
            if (!wrapper->marked())
                wrapper->mark();
        }
    }
}

void markCrossHeapDependentObjectsForDocument(JSGlobalData& globalData, Document* document)
{
    const HashSet<MessagePort*>& messagePorts = document->messagePorts();
    HashSet<MessagePort*>::const_iterator portsEnd = messagePorts.end();
    for (HashSet<MessagePort*>::const_iterator iter = messagePorts.begin(); iter != portsEnd; ++iter) {
        MessagePort* port = *iter;
        RefPtr<MessagePort> entangledPort = port->entangledPort();
        if (entangledPort) {
            // No wrapper, or wrapper is already marked - no need to examine cross-heap dependencies.
            DOMObject* wrapper = getCachedDOMObjectWrapper(globalData, port);
            if (!wrapper || wrapper->marked())
                continue;

            // Don't use cross-heap model of marking on same-heap pairs. Otherwise, they will never be destroyed, because a port will mark its entangled one,
            // and it will never get a chance to be marked as inaccessible. So, the port will keep getting marked in this function.
            if ((port->document() && entangledPort->document()) || (port->workerContext() == entangledPort->workerContext()))
                continue;

            // If the wrapper hasn't been marked during the mark phase of GC, then the port shouldn't protect its entangled one.
            // It's important not to call this when there is no wrapper. E.g., if GC is triggered after a MessageChannel is created, but before its ports are used from JS,
            // irreversibly telling the object that its (not yet existing) wrapper is inaccessible would be wrong. Similarly, ports posted via postMessage() may not
            // have wrappers until delivered.
            port->setJSWrapperIsInaccessible();

            // If the port is protected by its entangled one, mark it.
            // This is an atomic read of a boolean value, no synchronization between threads is required (at least on platforms that guarantee cache coherency).
            if (!entangledPort->jsWrapperIsInaccessible())
                wrapper->mark();
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

void markDOMObjectWrapper(JSGlobalData& globalData, void* object)
{
    if (!object)
        return;
    DOMObject* wrapper = getCachedDOMObjectWrapper(globalData, object);
    if (!wrapper || wrapper->marked())
        return;
    wrapper->mark();
}

JSValuePtr jsStringOrNull(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsNull();
    return jsString(exec, s);
}

JSValuePtr jsOwnedStringOrNull(ExecState* exec, const UString& s)
{
    if (s.isNull())
        return jsNull();
    return jsOwnedString(exec, s);
}

JSValuePtr jsStringOrUndefined(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsUndefined();
    return jsString(exec, s);
}

JSValuePtr jsStringOrFalse(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsBoolean(false);
    return jsString(exec, s);
}

JSValuePtr jsStringOrNull(ExecState* exec, const KURL& url)
{
    if (url.isNull())
        return jsNull();
    return jsString(exec, url.string());
}

JSValuePtr jsStringOrUndefined(ExecState* exec, const KURL& url)
{
    if (url.isNull())
        return jsUndefined();
    return jsString(exec, url.string());
}

JSValuePtr jsStringOrFalse(ExecState* exec, const KURL& url)
{
    if (url.isNull())
        return jsBoolean(false);
    return jsString(exec, url.string());
}

UString valueToStringWithNullCheck(ExecState* exec, JSValuePtr value)
{
    if (value->isNull())
        return UString();
    return value->toString(exec);
}

UString valueToStringWithUndefinedOrNullCheck(ExecState* exec, JSValuePtr value)
{
    if (value->isUndefinedOrNull())
        return UString();
    return value->toString(exec);
}

void setDOMException(ExecState* exec, ExceptionCode ec)
{
    if (!ec || exec->hadException())
        return;

    ExceptionCodeDescription description;
    getExceptionCodeDescription(ec, description);

    JSValuePtr errorObject = noValue();
    switch (description.type) {
        case DOMExceptionType:
            errorObject = toJS(exec, DOMCoreException::create(description));
            break;
        case RangeExceptionType:
            errorObject = toJS(exec, RangeException::create(description));
            break;
        case EventExceptionType:
            errorObject = toJS(exec, EventException::create(description));
            break;
        case XMLHttpRequestExceptionType:
            errorObject = toJS(exec, XMLHttpRequestException::create(description));
            break;
#if ENABLE(SVG)
        case SVGExceptionType:
            errorObject = toJS(exec, SVGException::create(description).get(), 0);
            break;
#endif
#if ENABLE(XPATH)
        case XPathExceptionType:
            errorObject = toJS(exec, XPathException::create(description));
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

void printErrorMessageForFrame(Frame* frame, const String& message)
{
    if (!frame)
        return;
    if (JSDOMWindow* window = toJSDOMWindow(frame))
        window->printErrorMessage(message);
}

JSValuePtr objectToStringFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) PrototypeFunction(exec, 0, propertyName, objectProtoFuncToString);
}

ExecState* execStateFromNode(Node* node)
{
    if (!node)
        return 0;
    Document* document = node->document();
    if (!document)
        return 0;
    Frame* frame = document->frame();
    if (!frame)
        return 0;
    if (!frame->script()->isEnabled())
        return 0;
    return frame->script()->globalObject()->globalExec();
}

StructureID* getCachedDOMStructure(ExecState* exec, const ClassInfo* classInfo)
{
    JSDOMStructureMap& structures = static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject())->structures();
    return structures.get(classInfo).get();
}

StructureID* cacheDOMStructure(ExecState* exec, PassRefPtr<StructureID> structureID, const ClassInfo* classInfo)
{
    JSDOMStructureMap& structures = static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject())->structures();
    ASSERT(!structures.contains(classInfo));
    return structures.set(classInfo, structureID).first->second.get();
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

} // namespace WebCore
