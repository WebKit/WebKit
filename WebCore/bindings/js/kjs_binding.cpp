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
#include "kjs_binding.h"

#include "DOMCoreException.h"
#include "EventException.h"
#include "ExceptionCode.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "JSDOMCoreException.h"
#include "JSEventException.h"
#include "JSNode.h"
#include "JSRangeException.h"
#include "JSXMLHttpRequestException.h"
#include "RangeException.h"
#include "XMLHttpRequestException.h"
#include "kjs_window.h"

#if ENABLE(SVG)
#include "JSSVGException.h"
#include "SVGException.h"
#endif

#if ENABLE(XPATH)
#include "JSXPathException.h"
#include "XPathException.h"
#endif

using namespace KJS;
using namespace WebCore;
using namespace HTMLNames;

// FIXME: Move all this stuff into the WebCore namespace.

namespace KJS {

typedef HashMap<void*, DOMObject*> DOMObjectMap;
typedef HashMap<WebCore::Node*, JSNode*> NodeMap;
typedef HashMap<Document*, NodeMap*> NodePerDocMap;

// For debugging, keep a set of wrappers currently registered, and check that
// all are unregistered before they are destroyed. This has helped us fix at
// least one bug.

static void addWrapper(DOMObject* wrapper);
static void removeWrapper(DOMObject* wrapper);
static void removeWrappers(const NodeMap& wrappers);

#ifdef NDEBUG

static inline void addWrapper(DOMObject*)
{
}

static inline void removeWrapper(DOMObject*)
{
}

static inline void removeWrappers(const NodeMap&)
{
}

#else

static HashSet<DOMObject*>& wrapperSet()
{
    static HashSet<DOMObject*> staticWrapperSet;
    return staticWrapperSet;
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

static void removeWrappers(const NodeMap& wrappers)
{
    for (NodeMap::const_iterator it = wrappers.begin(); it != wrappers.end(); ++it)
        removeWrapper(it->second);
}

DOMObject::~DOMObject()
{
    ASSERT(!wrapperSet().contains(this));
}

#endif

static DOMObjectMap& domObjects()
{ 
    // Don't use malloc here. Calling malloc from a mark function can deadlock.
    static DOMObjectMap staticDOMObjects;
    return staticDOMObjects;
}

static NodePerDocMap& domNodesPerDocument()
{
    // domNodesPerDocument() callers must synchronize using the JSLock because 
    // domNodesPerDocument() is called from a mark function, which can run
    // on a secondary thread.
    ASSERT(JSLock::lockCount());

    // Don't use malloc here. Calling malloc from a mark function can deadlock.
    static NodePerDocMap staticDOMNodesPerDocument;
    return staticDOMNodesPerDocument;
}

DOMObject* ScriptInterpreter::getDOMObject(void* objectHandle) 
{
    return domObjects().get(objectHandle);
}

void ScriptInterpreter::putDOMObject(void* objectHandle, DOMObject* wrapper) 
{
    addWrapper(wrapper);
    domObjects().set(objectHandle, wrapper);
}

void ScriptInterpreter::forgetDOMObject(void* objectHandle)
{
    removeWrapper(domObjects().take(objectHandle));
}

JSNode* ScriptInterpreter::getDOMNodeForDocument(Document* document, WebCore::Node* node)
{
    if (!document)
        return static_cast<JSNode*>(domObjects().get(node));
    NodeMap* documentDict = domNodesPerDocument().get(document);
    if (documentDict)
        return documentDict->get(node);
    return NULL;
}

void ScriptInterpreter::forgetDOMNodeForDocument(Document* document, WebCore::Node* node)
{
    if (!document) {
        removeWrapper(domObjects().take(node));
        return;
    }
    NodeMap* documentDict = domNodesPerDocument().get(document);
    if (documentDict)
        removeWrapper(documentDict->take(node));
}

void ScriptInterpreter::putDOMNodeForDocument(Document* document, WebCore::Node* node, JSNode* wrapper)
{
    addWrapper(wrapper);
    if (!document) {
        domObjects().set(node, wrapper);
        return;
    }
    NodeMap* documentDict = domNodesPerDocument().get(document);
    if (!documentDict) {
        documentDict = new NodeMap;
        domNodesPerDocument().set(document, documentDict);
    }
    documentDict->set(node, wrapper);
}

void ScriptInterpreter::forgetAllDOMNodesForDocument(Document* document)
{
    ASSERT(document);
    NodeMap* map = domNodesPerDocument().take(document);
    if (!map)
        return;
    removeWrappers(*map);
    delete map;
}

void ScriptInterpreter::markDOMNodesForDocument(Document* doc)
{
    NodePerDocMap::iterator dictIt = domNodesPerDocument().find(doc);
    if (dictIt != domNodesPerDocument().end()) {
        NodeMap* nodeDict = dictIt->second;
        NodeMap::iterator nodeEnd = nodeDict->end();
        for (NodeMap::iterator nodeIt = nodeDict->begin(); nodeIt != nodeEnd; ++nodeIt) {
            JSNode* jsNode = nodeIt->second;
            WebCore::Node* node = jsNode->impl();
            
            // don't mark wrappers for nodes that are no longer in the
            // document - they should not be saved if the node is not
            // otherwise reachable from JS.
            // However, image elements that aren't in the document are also
            // marked, if they are not done loading yet.
            if (!jsNode->marked() && (node->inDocument() || (node->hasTagName(imgTag) &&
                                                             !static_cast<HTMLImageElement*>(node)->haveFiredLoadEvent())))
                jsNode->mark();
        }
    }
}

void ScriptInterpreter::updateDOMNodeDocument(WebCore::Node* node, Document* oldDoc, Document* newDoc)
{
    ASSERT(oldDoc != newDoc);
    JSNode* wrapper = getDOMNodeForDocument(oldDoc, node);
    if (wrapper) {
        removeWrapper(wrapper);
        putDOMNodeForDocument(newDoc, node, wrapper);
        forgetDOMNodeForDocument(oldDoc, node);
        addWrapper(wrapper);
    }
}

JSValue* jsStringOrNull(const String& s)
{
    if (s.isNull())
        return jsNull();
    return jsString(s);
}

JSValue* jsOwnedStringOrNull(const KJS::UString& s)
{
    if (s.isNull())
        return jsNull();
    return jsOwnedString(s);
}

JSValue* jsStringOrUndefined(const String& s)
{
    if (s.isNull())
        return jsUndefined();
    return jsString(s);
}

JSValue* jsStringOrFalse(const String& s)
{
    if (s.isNull())
        return jsBoolean(false);
    return jsString(s);
}

String valueToStringWithNullCheck(ExecState* exec, JSValue* val)
{
    if (val->isNull())
        return String();
    return val->toString(exec);
}

String valueToStringWithUndefinedOrNullCheck(ExecState* exec, JSValue* val)
{
    if (val->isUndefinedOrNull())
        return String();
    return val->toString(exec);
}

void setDOMException(ExecState* exec, ExceptionCode ec)
{
    if (!ec || exec->hadException())
        return;

    // To be removed: See XMLHttpRequest.h.
    if (ec == XMLHttpRequestException::PERMISSION_DENIED) {
        throwError(exec, GeneralError, "Permission denied");
        return;
    }

    ExceptionCodeDescription description;
    getExceptionCodeDescription(ec, description);

    JSValue* errorObject = 0;
    switch (description.type) {
        case DOMExceptionType:
            errorObject = toJS(exec, new DOMCoreException(description));
            break;
        case RangeExceptionType:
            errorObject = toJS(exec, new RangeException(description));
            break;
        case EventExceptionType:
            errorObject = toJS(exec, new EventException(description));
            break;
        case XMLHttpRequestExceptionType:
            errorObject = toJS(exec, new XMLHttpRequestException(description));
            break;
#if ENABLE(SVG)
        case SVGExceptionType:
            errorObject = toJS(exec, new SVGException(description), 0);
            break;
#endif
#if ENABLE(XPATH)
        case XPathExceptionType:
            errorObject = toJS(exec, new XPathException(description));
            break;
#endif
    }

    ASSERT(errorObject);
    exec->setException(errorObject);
}

} // namespace KJS

namespace WebCore {

bool allowsAccessFromFrame(ExecState* exec, Frame* frame)
{
    if (!frame)
        return false;
    Window* window = Window::retrieveWindow(frame);
    return window && window->allowsAccessFrom(exec);
}

bool allowsAccessFromFrame(ExecState* exec, Frame* frame, String& message)
{
    if (!frame)
        return false;
    Window* window = Window::retrieveWindow(frame);
    return window && window->allowsAccessFrom(exec, message);
}

void printErrorMessageForFrame(Frame* frame, const String& message)
{
    if (!frame)
        return;
    if (Window* window = Window::retrieveWindow(frame))
        window->printErrorMessage(message);
}

JSValue* nonCachingStaticFunctionGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    const HashEntry* entry = slot.staticEntry();
    return new PrototypeFunction(exec, entry->params, propertyName, entry->value.functionValue);
}

JSValue* objectToStringFunctionGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot&)
{
    return new PrototypeFunction(exec, 0, propertyName, objectProtoFuncToString);
}

} // namespace WebCore
