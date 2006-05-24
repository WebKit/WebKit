/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// gcc 3.x can't handle including the HashMap pointer specialization in this file
#ifndef __GLIBCXX__ // less than gcc 3.4
#define HASH_MAP_PTR_SPEC_WORKAROUND 1
#endif

#include "config.h"
#include "kjs_binding.h"

#include "EventNames.h"
#include "Frame.h"
#include "PlatformString.h"
#include "Range.h"
#include "dom2_eventsimpl.h"
#include "XPathEvaluator.h"
#include "kjs_dom.h"
#include "kjs_window.h"
#include <kjs/collector.h>
#include <kjs/internal.h> // for InterpreterImp
#include <wtf/HashMap.h>

using namespace WebCore;
using namespace EventNames;

namespace KJS {

UString DOMObject::toString(ExecState *) const
{
  return "[object " + className() + "]";
}

typedef HashMap<void*, DOMObject*> DOMObjectMap;
typedef HashMap<WebCore::Node*, DOMNode*> NodeMap;
typedef HashMap<Document*, NodeMap*> NodePerDocMap;

static DOMObjectMap *domObjects()
{ 
  static DOMObjectMap* staticDomObjects = new DOMObjectMap();
  return staticDomObjects;
}

static NodePerDocMap *domNodesPerDocument()
{
  static NodePerDocMap *staticDOMNodesPerDocument = new NodePerDocMap();
  return staticDOMNodesPerDocument;
}


ScriptInterpreter::ScriptInterpreter( JSObject *global, Frame *frame )
  : Interpreter( global ), m_frame(frame),
    m_evt( 0L ), m_inlineCode(false), m_timerCallback(false)
{
}

ScriptInterpreter::~ScriptInterpreter()
{
}

DOMObject* ScriptInterpreter::getDOMObject(void* objectHandle) 
{
    return domObjects()->get(objectHandle);
}

void ScriptInterpreter::putDOMObject(void* objectHandle, DOMObject* obj) 
{
    domObjects()->set(objectHandle, obj);
}

void ScriptInterpreter::forgetDOMObject(void* objectHandle)
{
    domObjects()->remove(objectHandle);
}

DOMNode *ScriptInterpreter::getDOMNodeForDocument(WebCore::Document *document, WebCore::Node *node)
{
    if (!document)
        return static_cast<DOMNode *>(domObjects()->get(node));
    NodeMap *documentDict = domNodesPerDocument()->get(document);
    if (documentDict)
        return documentDict->get(node);
    return NULL;
}

void ScriptInterpreter::forgetDOMNodeForDocument(WebCore::Document *document, WebCore::Node *node)
{
    if (!document) {
        domObjects()->remove(node);
        return;
    }
    NodeMap *documentDict = domNodesPerDocument()->get(document);
    if (documentDict)
        documentDict->remove(node);
}

void ScriptInterpreter::putDOMNodeForDocument(WebCore::Document *document, WebCore::Node *nodeHandle, DOMNode *nodeWrapper)
{
    if (!document) {
        domObjects()->set(nodeHandle, nodeWrapper);
        return;
    }
    NodeMap *documentDict = domNodesPerDocument()->get(document);
    if (!documentDict) {
        documentDict = new NodeMap();
        domNodesPerDocument()->set(document, documentDict);
    }
    documentDict->set(nodeHandle, nodeWrapper);
}

void ScriptInterpreter::forgetAllDOMNodesForDocument(WebCore::Document *document)
{
    assert(document);
    NodePerDocMap::iterator it = domNodesPerDocument()->find(document);
    if (it != domNodesPerDocument()->end()) {
        delete it->second;
        domNodesPerDocument()->remove(it);
    }
}

void ScriptInterpreter::mark(bool currentThreadIsMainThread)
{
  NodePerDocMap::iterator dictEnd = domNodesPerDocument()->end();
  for (NodePerDocMap::iterator dictIt = domNodesPerDocument()->begin();
       dictIt != dictEnd;
       ++dictIt) {
    
      NodeMap *nodeDict = dictIt->second;
      NodeMap::iterator nodeEnd = nodeDict->end();
      for (NodeMap::iterator nodeIt = nodeDict->begin();
           nodeIt != nodeEnd;
           ++nodeIt) {

        DOMNode *node = nodeIt->second;
        // don't mark wrappers for nodes that are no longer in the
        // document - they should not be saved if the node is not
        // otherwise reachable from JS.
        if (node->impl()->inDocument() && !node->marked())
            node->mark();
      }
  }
  
  if (!currentThreadIsMainThread) {
      // On alternate threads, DOMObjects remain in the cache because they're not collected.
      // So, they need an opportunity to mark their children.
      DOMObjectMap::iterator objectEnd = domObjects()->end();
      for (DOMObjectMap::iterator objectIt = domObjects()->begin();
           objectIt != objectEnd;
           ++objectIt) {
          DOMObject* object = objectIt->second;
          if (!object->marked())
              object->mark();
      }
  }
}

ExecState *ScriptInterpreter::globalExec()
{
    // we need to make sure that any script execution happening in this
    // frame does not destroy it
    m_frame->keepAlive();
    return Interpreter::globalExec();
}

void ScriptInterpreter::updateDOMNodeDocument(WebCore::Node *node, WebCore::Document *oldDoc, WebCore::Document *newDoc)
{
  DOMNode *cachedObject = getDOMNodeForDocument(oldDoc, node);
  if (cachedObject) {
    putDOMNodeForDocument(newDoc, node, cachedObject);
    forgetDOMNodeForDocument(oldDoc, node);
  }
}

bool ScriptInterpreter::wasRunByUserGesture() const
{
  if ( m_evt )
  {
    const AtomicString &type = m_evt->type();
    bool eventOk = ( // mouse events
      type == clickEvent || type == mousedownEvent ||
      type == mouseupEvent || type == dblclickEvent ||
      // keyboard events
      type == keydownEvent || type == keypressEvent ||
      type == keyupEvent ||
      // other accepted events
      type == selectEvent || type == changeEvent ||
      type == focusEvent || type == blurEvent ||
      type == submitEvent );
    if (eventOk)
      return true;
  } else { // no event
    if (m_inlineCode  && !m_timerCallback)
      // This is the <a href="javascript:window.open('...')> case -> we let it through
      return true;
    // This is the <script>window.open(...)</script> case or a timer callback -> block it
  }
  return false;
}

bool ScriptInterpreter::isGlobalObject(JSValue *v)
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

void *ScriptInterpreter::createLanguageInstanceForValue (ExecState *exec, int language, JSObject *value, const Bindings::RootObject *origin, const Bindings::RootObject *current)
{
    void *result = 0;
    
#if __APPLE__
    // FIXME: Need to implement bindings support.
    if (language == Bindings::Instance::ObjectiveCLanguage)
        result = createObjcInstanceForValue (exec, value, origin, current);
    
    if (!result)
        result = Interpreter::createLanguageInstanceForValue (exec, language, value, origin, current);
#endif
    return result;
}


//////

JSValue *jsStringOrNull(const String &s)
{
    if (s.isNull())
        return jsNull();
    return jsString(s);
}

JSValue *jsStringOrUndefined(const String &s)
{
    if (s.isNull())
        return jsUndefined();
    return jsString(s);
}

JSValue *jsStringOrFalse(const String &s)
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

bool valueToBooleanTreatUndefinedAsTrue(ExecState* exec, JSValue* val)
{
    if (val->isUndefined())
        return true;
    return val->toBoolean(exec);
}

static const char * const exceptionNames[] = {
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

static const char * const rangeExceptionNames[] = {
    0, "BAD_BOUNDARYPOINTS_ERR", "INVALID_NODE_TYPE_ERR"
};

static const char * const eventExceptionNames[] = {
    "UNSPECIFIED_EVENT_TYPE_ERR"
};

#if XPATH_SUPPORT
static const char * const xpathExceptionNames[] = {
    "INVALID_EXPRESSION_ERR",
    "TYPE_ERR"
};
#endif

void setDOMException(ExecState* exec, ExceptionCode ec)
{
  if (ec == 0 || exec->hadException())
    return;

  const char* type = "DOM";
  int code = ec;

  const char * const * nameTable;
  
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
#if XPATH_SUPPORT
  } else if (code >= XPathExceptionOffset && code <= XPathExceptionMax) {
    type = "DOM XPath";
    // XPath exception codes start with 51 and we don't want 51 empty elements in the name array
    nameIndex = code - INVALID_EXPRESSION_ERR;
    code -= XPathExceptionOffset;
    nameTable = xpathExceptionNames;
    nameTableSize = sizeof(xpathExceptionNames) / sizeof(xpathExceptionNames[0]);
#endif
  } else {
    nameIndex = code;
    nameTable = exceptionNames;
    nameTableSize = sizeof(exceptionNames) / sizeof(exceptionNames[0]);
  }

  const char* name = nameIndex < nameTableSize ? nameTable[nameIndex] : 0;

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
