/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "Chrome.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "JSNode.h"
#include "Page.h"
#include "PlatformString.h"
#include "Range.h"
#include "RangeException.h"
#include "XMLHttpRequest.h"
#include "kjs_dom.h"
#include "kjs_window.h"
#include <kjs/collector.h>
#include <wtf/HashMap.h>

#if ENABLE(SVG)
#include "SVGException.h"
#endif

#if ENABLE(XPATH)
#include "XPathEvaluator.h"
#endif

using namespace WebCore;
using namespace EventNames;

namespace KJS {

typedef HashMap<void*, DOMObject*> DOMObjectMap;
typedef HashMap<Node*, JSNode*> NodeMap;
typedef HashMap<Document*, NodeMap*> NodePerDocMap;

// For debugging, keep a set of wrappers currently registered, and check that
// all are unregistered before they are destroyed. This has helped us fix at
// least one bug.

#ifdef NDEBUG

#define ADD_WRAPPER(wrapper)
#define REMOVE_WRAPPER(wrapper)
#define REMOVE_WRAPPERS(wrappers)

#else

#define ADD_WRAPPER(wrapper) addWrapper(wrapper)
#define REMOVE_WRAPPER(wrapper) removeWrapper(wrapper)
#define REMOVE_WRAPPERS(wrappers) removeWrappers(wrappers)

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

ScriptInterpreter::ScriptInterpreter(JSObject* global, Frame* frame)
    : Interpreter(global)
    , m_frame(frame)
    , m_currentEvent(0)
    , m_inlineCode(false)
    , m_timerCallback(false)
{
    // Time in milliseconds before the script timeout handler kicks in.
    setTimeoutTime(10000);
}

DOMObject* ScriptInterpreter::getDOMObject(void* objectHandle) 
{
    return domObjects().get(objectHandle);
}

void ScriptInterpreter::putDOMObject(void* objectHandle, DOMObject* wrapper) 
{
    ADD_WRAPPER(wrapper);
    domObjects().set(objectHandle, wrapper);
}

void ScriptInterpreter::forgetDOMObject(void* objectHandle)
{
    REMOVE_WRAPPER(domObjects().get(objectHandle));
    domObjects().remove(objectHandle);
}

JSNode* ScriptInterpreter::getDOMNodeForDocument(Document* document, Node* node)
{
    if (!document)
        return static_cast<JSNode*>(domObjects().get(node));
    NodeMap* documentDict = domNodesPerDocument().get(document);
    if (documentDict)
        return documentDict->get(node);
    return NULL;
}

void ScriptInterpreter::forgetDOMNodeForDocument(Document* document, Node* node)
{
    REMOVE_WRAPPER(getDOMNodeForDocument(document, node));
    if (!document) {
        domObjects().remove(node);
        return;
    }
    NodeMap* documentDict = domNodesPerDocument().get(document);
    if (documentDict)
        documentDict->remove(node);
}

void ScriptInterpreter::putDOMNodeForDocument(Document* document, Node* node, JSNode* wrapper)
{
    ADD_WRAPPER(wrapper);
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
    NodePerDocMap::iterator it = domNodesPerDocument().find(document);
    if (it != domNodesPerDocument().end()) {
        REMOVE_WRAPPERS(*it->second);
        delete it->second;
        domNodesPerDocument().remove(it);
    }
}

void ScriptInterpreter::markDOMNodesForDocument(Document* doc)
{
    NodePerDocMap::iterator dictIt = domNodesPerDocument().find(doc);
    if (dictIt != domNodesPerDocument().end()) {
        NodeMap* nodeDict = dictIt->second;
        NodeMap::iterator nodeEnd = nodeDict->end();
        for (NodeMap::iterator nodeIt = nodeDict->begin(); nodeIt != nodeEnd; ++nodeIt) {
            JSNode* node = nodeIt->second;
            // don't mark wrappers for nodes that are no longer in the
            // document - they should not be saved if the node is not
            // otherwise reachable from JS.
            if (node->impl()->inDocument() && !node->marked())
                node->mark();
        }
    }
}

ExecState* ScriptInterpreter::globalExec()
{
    // we need to make sure that any script execution happening in this
    // frame does not destroy it
    m_frame->keepAlive();
    return Interpreter::globalExec();
}

void ScriptInterpreter::updateDOMNodeDocument(Node* node, Document* oldDoc, Document* newDoc)
{
    ASSERT(oldDoc != newDoc);
    JSNode* wrapper = getDOMNodeForDocument(oldDoc, node);
    if (wrapper) {
        REMOVE_WRAPPER(wrapper);
        putDOMNodeForDocument(newDoc, node, wrapper);
        forgetDOMNodeForDocument(oldDoc, node);
        ADD_WRAPPER(wrapper);
    }
}

bool ScriptInterpreter::wasRunByUserGesture() const
{
    if (m_currentEvent) {
        const AtomicString& type = m_currentEvent->type();
        bool eventOk = ( // mouse events
            type == clickEvent || type == mousedownEvent ||
            type == mouseupEvent || type == dblclickEvent ||
            // keyboard events
            type == keydownEvent || type == keypressEvent ||
            type == keyupEvent ||
            // other accepted events
            type == selectEvent || type == changeEvent ||
            type == focusEvent || type == blurEvent ||
            type == submitEvent);
        if (eventOk)
            return true;
    } else { // no event
        if (m_inlineCode && !m_timerCallback)
            // This is the <a href="javascript:window.open('...')> case -> we let it through
            return true;
        // This is the <script>window.open(...)</script> case or a timer callback -> block it
    }
    return false;
}

bool ScriptInterpreter::isGlobalObject(JSValue* v)
{
    return v->isObject(&Window::info);
}

bool ScriptInterpreter::isSafeScript(const Interpreter* target)
{
    return Window::isSafeScript(this, static_cast<const ScriptInterpreter*>(target));
}

Interpreter* ScriptInterpreter::interpreterForGlobalObject(const JSValue* imp)
{
    const Window* win = static_cast<const Window*>(imp);
    return win->interpreter();
}

bool ScriptInterpreter::shouldInterruptScript() const
{
    Page* page = m_frame->page();

    // See <rdar://problem/5479443>. We don't think that page can ever be NULL
    // in this case, but if it is, we've gotten into a state where we may have
    // hung the UI, with no way to ask the client whether to cancel execution. 
    // For now, our solution is just to cancel execution no matter what, 
    // ensuring that we never hang. We might want to consider other solutions 
    // if we discover problems with this one.
    ASSERT(page);
    if (!page)
        return true;

    return page->chrome()->shouldInterruptJavaScript();
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

static const char* const exceptionNames[] = {
    0,
    "INDEX_SIZE_ERR",
    "DOMSTRING_SIZE_ERR",
    "HIERARCHY_REQUEST_ERR",
    "WRONG_DOCUMENT_ERR",
    "INVALID_CHARACTER_ERR",
    "NO_DATA_ALLOWED_ERR",
    "NO_MODIFICATION_ALLOWED_ERR",
    "NOT_FOUND_ERR",
    "NOT_SUPPORTED_ERR",
    "INUSE_ATTRIBUTE_ERR",
    "INVALID_STATE_ERR",
    "SYNTAX_ERR",
    "INVALID_MODIFICATION_ERR",
    "NAMESPACE_ERR",
    "INVALID_ACCESS_ERR",
    "VALIDATION_ERR",
    "TYPE_MISMATCH_ERR",
};

static const char* const rangeExceptionNames[] = {
    0, "BAD_BOUNDARYPOINTS_ERR", "INVALID_NODE_TYPE_ERR"
};

static const char* const eventExceptionNames[] = {
    "UNSPECIFIED_EVENT_TYPE_ERR"
};

static const char* const xmlHttpRequestExceptionNames[] = {
    "NETWORK_ERR"
};

#if ENABLE(XPATH)
static const char* const xpathExceptionNames[] = {
    "INVALID_EXPRESSION_ERR",
    "TYPE_ERR"
};
#endif

#if ENABLE(SVG)
static const char* const svgExceptionNames[] = {
    "SVG_WRONG_TYPE_ERR",
    "SVG_INVALID_VALUE_ERR",
    "SVG_MATRIX_NOT_INVERTABLE"
};
#endif

void setDOMException(ExecState* exec, ExceptionCode ec)
{
    if (ec == 0 || exec->hadException())
        return;

    const char* type = "DOM";
    int code = ec;

    const char* const* nameTable;
  
    int nameTableSize;
    int nameIndex;
    if (code >= RangeExceptionOffset && code <= RangeExceptionMax) {
        type = "DOM Range";
        code -= RangeExceptionOffset;
        nameIndex = code;
        nameTable = rangeExceptionNames;
        nameTableSize = sizeof(rangeExceptionNames) / sizeof(rangeExceptionNames[0]);
    } else if (code >= EventExceptionOffset && code <= EventExceptionMax) {
        type = "DOM Events";
        code -= EventExceptionOffset;
        nameIndex = code;
        nameTable = eventExceptionNames;
        nameTableSize = sizeof(eventExceptionNames) / sizeof(eventExceptionNames[0]);
    } else if (code == XMLHttpRequestExceptionOffset) {
        // FIXME: this exception should be replaced with DOM SECURITY_ERR when it finds its way to the spec.
        throwError(exec, GeneralError, "Permission denied");
        return;
    } else if (code > XMLHttpRequestExceptionOffset && code <= XMLHttpRequestExceptionMax) {
        type = "XMLHttpRequest";
        // XMLHttpRequest exception codes start with 101 and we don't want 100 empty elements in the name array
        nameIndex = code - NETWORK_ERR;
        code -= XMLHttpRequestExceptionOffset;
        nameTable = xmlHttpRequestExceptionNames;
        nameTableSize = sizeof(xmlHttpRequestExceptionNames) / sizeof(xmlHttpRequestExceptionNames[0]);
#if ENABLE(XPATH)
    } else if (code >= XPathExceptionOffset && code <= XPathExceptionMax) {
        type = "DOM XPath";
        // XPath exception codes start with 51 and we don't want 51 empty elements in the name array
        nameIndex = code - INVALID_EXPRESSION_ERR;
        code -= XPathExceptionOffset;
        nameTable = xpathExceptionNames;
        nameTableSize = sizeof(xpathExceptionNames) / sizeof(xpathExceptionNames[0]);
#endif
#if ENABLE(SVG)
    } else if (code >= SVGExceptionOffset && code <= SVGExceptionMax) {
        type = "DOM SVG";
        code -= SVGExceptionOffset;
        nameIndex = code;
        nameTable = svgExceptionNames;
        nameTableSize = sizeof(svgExceptionNames) / sizeof(svgExceptionNames[0]);
#endif
    } else {
        nameIndex = code;
        nameTable = exceptionNames;
        nameTableSize = sizeof(exceptionNames) / sizeof(exceptionNames[0]);
    }

    const char* name = (nameIndex < nameTableSize && nameIndex >= 0) ? nameTable[nameIndex] : 0;

    // 100 characters is a big enough buffer, because there are:
    //   13 characters in the message
    //   10 characters in the longest type, "DOM Events"
    //   27 characters in the longest name, "NO_MODIFICATION_ALLOWED_ERR"
    //   20 or so digits in the longest integer's ASCII form (even if int is 64-bit)
    //   1 byte for a null character
    // That adds up to about 70 bytes.
    char buffer[100];

    if (name)
        sprintf(buffer, "%s: %s Exception %d", name, type, code);
    else
        sprintf(buffer, "%s Exception %d", type, code);

    JSObject* errorObject = throwError(exec, GeneralError, buffer);
    errorObject->put(exec, "code", jsNumber(code));
}

}
