/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#include "config.h"
#include "kjs_traversal.h"

#include "Document.h"
#include "Frame.h"
#include "kjs_proxy.h"

#include "kjs_traversal.lut.h"

using namespace WebCore;

namespace KJS {

// -------------------------------------------------------------------------

const ClassInfo DOMNodeIterator::info = { "NodeIterator", 0, &DOMNodeIteratorTable, 0 };
/*
@begin DOMNodeIteratorTable 5
  root				DOMNodeIterator::Root			DontDelete|ReadOnly
  whatToShow			DOMNodeIterator::WhatToShow		DontDelete|ReadOnly
  filter			DOMNodeIterator::Filter			DontDelete|ReadOnly
  expandEntityReferences	DOMNodeIterator::ExpandEntityReferences	DontDelete|ReadOnly
  referenceNode	DOMNodeIterator::ReferenceNode	DontDelete|ReadOnly
  pointerBeforeReferenceNode DOMNodeIterator::PointerBeforeReferenceNode	DontDelete|ReadOnly
@end
@begin DOMNodeIteratorProtoTable 3
  nextNode	DOMNodeIterator::NextNode	DontDelete|Function 0
  previousNode	DOMNodeIterator::PreviousNode	DontDelete|Function 0
  detach	DOMNodeIterator::Detach		DontDelete|Function 0
@end
*/
KJS_DEFINE_PROTOTYPE(DOMNodeIteratorProto)
KJS_IMPLEMENT_PROTOFUNC(DOMNodeIteratorProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("DOMNodeIterator",DOMNodeIteratorProto,DOMNodeIteratorProtoFunc)

DOMNodeIterator::DOMNodeIterator(ExecState *exec, NodeIterator *ni)
  : m_impl(ni)
{
  setPrototype(DOMNodeIteratorProto::self(exec));
}

DOMNodeIterator::~DOMNodeIterator()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

void DOMNodeIterator::mark()
{
    m_impl->filter()->mark();
    DOMObject::mark();
}

bool DOMNodeIterator::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMNodeIterator, DOMObject>(exec, &DOMNodeIteratorTable, this, propertyName, slot);
}

JSValue *DOMNodeIterator::getValueProperty(ExecState *exec, int token) const
{
  NodeIterator &ni = *m_impl;
  switch (token) {
  case Root:
    return toJS(exec,ni.root());
  case WhatToShow:
    return jsNumber(ni.whatToShow());
  case Filter:
    return toJS(exec,ni.filter());
  case ExpandEntityReferences:
    return jsBoolean(ni.expandEntityReferences());
  case ReferenceNode:
    return toJS(exec,ni.referenceNode());
  case PointerBeforeReferenceNode:
    return jsBoolean(ni.pointerBeforeReferenceNode());
 default:
   return 0;
  }
}

JSValue *DOMNodeIteratorProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &)
{
  if (!thisObj->inherits(&KJS::DOMNodeIterator::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  NodeIterator &nodeIterator = *static_cast<DOMNodeIterator *>(thisObj)->impl();
  switch (id) {
  case DOMNodeIterator::PreviousNode:
    return toJS(exec,nodeIterator.previousNode(exception));
  case DOMNodeIterator::NextNode:
    return toJS(exec,nodeIterator.nextNode(exception));
  case DOMNodeIterator::Detach:
    nodeIterator.detach(exception);
    return jsUndefined();
  }
  return jsUndefined();
}

JSValue *toJS(ExecState *exec, NodeIterator *ni)
{
  return cacheDOMObject<NodeIterator, DOMNodeIterator>(exec, ni);
}


// -------------------------------------------------------------------------

const ClassInfo NodeFilterConstructor::info = { "NodeFilterConstructor", 0, &NodeFilterConstructorTable, 0 };
/*
@begin NodeFilterConstructorTable 17
  FILTER_ACCEPT		WebCore::NodeFilter::FILTER_ACCEPT	DontDelete|ReadOnly
  FILTER_REJECT		WebCore::NodeFilter::FILTER_REJECT	DontDelete|ReadOnly
  FILTER_SKIP		WebCore::NodeFilter::FILTER_SKIP	DontDelete|ReadOnly
  SHOW_ALL		WebCore::NodeFilter::SHOW_ALL	DontDelete|ReadOnly
  SHOW_ELEMENT		WebCore::NodeFilter::SHOW_ELEMENT	DontDelete|ReadOnly
  SHOW_ATTRIBUTE	WebCore::NodeFilter::SHOW_ATTRIBUTE	DontDelete|ReadOnly
  SHOW_TEXT		WebCore::NodeFilter::SHOW_TEXT	DontDelete|ReadOnly
  SHOW_CDATA_SECTION	WebCore::NodeFilter::SHOW_CDATA_SECTION	DontDelete|ReadOnly
  SHOW_ENTITY_REFERENCE	WebCore::NodeFilter::SHOW_ENTITY_REFERENCE	DontDelete|ReadOnly
  SHOW_ENTITY		WebCore::NodeFilter::SHOW_ENTITY	DontDelete|ReadOnly
  SHOW_PROCESSING_INSTRUCTION	WebCore::NodeFilter::SHOW_PROCESSING_INSTRUCTION	DontDelete|ReadOnly
  SHOW_COMMENT		WebCore::NodeFilter::SHOW_COMMENT	DontDelete|ReadOnly
  SHOW_DOCUMENT		WebCore::NodeFilter::SHOW_DOCUMENT	DontDelete|ReadOnly
  SHOW_DOCUMENT_TYPE	WebCore::NodeFilter::SHOW_DOCUMENT_TYPE	DontDelete|ReadOnly
  SHOW_DOCUMENT_FRAGMENT	WebCore::NodeFilter::SHOW_DOCUMENT_FRAGMENT	DontDelete|ReadOnly
  SHOW_NOTATION		WebCore::NodeFilter::SHOW_NOTATION	DontDelete|ReadOnly
@end
*/
bool NodeFilterConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<NodeFilterConstructor, DOMObject>(exec, &NodeFilterConstructorTable, this, propertyName, slot);
}

JSValue *NodeFilterConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return jsNumber(token);
}

JSValue *getNodeFilterConstructor(ExecState *exec)
{
  return cacheGlobalObject<NodeFilterConstructor>(exec, "[[nodeFilter.constructor]]");
}

// -------------------------------------------------------------------------

const ClassInfo DOMNodeFilter::info = { "NodeFilter", 0, 0, 0 };
/*
@begin DOMNodeFilterProtoTable 1
  acceptNode	DOMNodeFilter::AcceptNode	DontDelete|Function 0
@end
*/
KJS_DEFINE_PROTOTYPE(DOMNodeFilterProto)
KJS_IMPLEMENT_PROTOFUNC(DOMNodeFilterProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("DOMNodeFilter",DOMNodeFilterProto,DOMNodeFilterProtoFunc)

DOMNodeFilter::DOMNodeFilter(ExecState *exec, NodeFilter *nf)
  : m_impl(nf) 
{
  setPrototype(DOMNodeFilterProto::self(exec));
}

DOMNodeFilter::~DOMNodeFilter()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

void DOMNodeFilter::mark()
{
    m_impl->mark();
    DOMObject::mark();
}

JSValue *DOMNodeFilterProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMNodeFilter::info))
    return throwError(exec, TypeError);
  NodeFilter &nodeFilter = *static_cast<DOMNodeFilter *>(thisObj)->impl();
  switch (id) {
    case DOMNodeFilter::AcceptNode:
      return jsNumber(nodeFilter.acceptNode(toNode(args[0])));
  }
  return jsUndefined();
}

JSValue *toJS(ExecState *exec, NodeFilter *nf)
{
    return cacheDOMObject<NodeFilter, DOMNodeFilter>(exec, nf);
}

NodeFilter *toNodeFilter(JSValue *val)
{
    if (!val || !val->isObject(&DOMNodeFilter::info))
        return 0;
    return static_cast<DOMNodeFilter *>(val)->impl();
}

// -------------------------------------------------------------------------

const ClassInfo DOMTreeWalker::info = { "TreeWalker", 0, &DOMTreeWalkerTable, 0 };
/*
@begin DOMTreeWalkerTable 5
  root			DOMTreeWalker::Root		DontDelete|ReadOnly
  whatToShow		DOMTreeWalker::WhatToShow	DontDelete|ReadOnly
  filter		DOMTreeWalker::Filter		DontDelete|ReadOnly
  expandEntityReferences DOMTreeWalker::ExpandEntityReferences	DontDelete|ReadOnly
  currentNode		DOMTreeWalker::CurrentNode	DontDelete
@end
@begin DOMTreeWalkerProtoTable 7
  parentNode	DOMTreeWalker::ParentNode	DontDelete|Function 0
  firstChild	DOMTreeWalker::FirstChild	DontDelete|Function 0
  lastChild	DOMTreeWalker::LastChild	DontDelete|Function 0
  previousSibling DOMTreeWalker::PreviousSibling	DontDelete|Function 0
  nextSibling	DOMTreeWalker::NextSibling	DontDelete|Function 0
  previousNode	DOMTreeWalker::PreviousNode	DontDelete|Function 0
  nextNode	DOMTreeWalker::NextNode		DontDelete|Function 0
@end
*/
KJS_DEFINE_PROTOTYPE(DOMTreeWalkerProto)
KJS_IMPLEMENT_PROTOFUNC(DOMTreeWalkerProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("DOMTreeWalker",DOMTreeWalkerProto,DOMTreeWalkerProtoFunc)

DOMTreeWalker::DOMTreeWalker(ExecState *exec, TreeWalker *tw)
  : m_impl(tw)
{
  setPrototype(DOMTreeWalkerProto::self(exec));
}

DOMTreeWalker::~DOMTreeWalker()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

void DOMTreeWalker::mark()
{
    m_impl->filter()->mark();
    DOMObject::mark();
}

bool DOMTreeWalker::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMTreeWalker, DOMObject>(exec, &DOMTreeWalkerTable, this, propertyName, slot);
}

JSValue *DOMTreeWalker::getValueProperty(ExecState *exec, int token) const
{
  TreeWalker &tw = *m_impl;
  switch (token) {
  case Root:
    return toJS(exec,tw.root());
  case WhatToShow:
    return jsNumber(tw.whatToShow());
  case Filter:
    return toJS(exec,tw.filter());
  case ExpandEntityReferences:
    return jsBoolean(tw.expandEntityReferences());
  case CurrentNode:
    return toJS(exec,tw.currentNode());
  default:
    return 0;
  }
}

void DOMTreeWalker::put(ExecState *exec, const Identifier &propertyName,
                           JSValue *value, int attr)
{
  if (propertyName == "currentNode") {
    DOMExceptionTranslator exception(exec);
    m_impl->setCurrentNode(toNode(value), exception);
  }
  else
    JSObject::put(exec, propertyName, value, attr);
}

JSValue *DOMTreeWalkerProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &)
{
  if (!thisObj->inherits(&KJS::DOMTreeWalker::info))
    return throwError(exec, TypeError);
  TreeWalker &treeWalker = *static_cast<DOMTreeWalker *>(thisObj)->impl();
  switch (id) {
    case DOMTreeWalker::ParentNode:
      return toJS(exec,treeWalker.parentNode());
    case DOMTreeWalker::FirstChild:
      return toJS(exec,treeWalker.firstChild());
    case DOMTreeWalker::LastChild:
      return toJS(exec,treeWalker.lastChild());
    case DOMTreeWalker::PreviousSibling:
      return toJS(exec,treeWalker.previousSibling());
    case DOMTreeWalker::NextSibling:
      return toJS(exec,treeWalker.nextSibling());
    case DOMTreeWalker::PreviousNode:
      return toJS(exec,treeWalker.previousNode());
    case DOMTreeWalker::NextNode:
      return toJS(exec,treeWalker.nextNode());
  }
  return jsUndefined();
}

JSValue *toJS(ExecState *exec, TreeWalker *tw)
{
  return cacheDOMObject<TreeWalker, DOMTreeWalker>(exec, tw);
}

// -------------------------------------------------------------------------

JSNodeFilterCondition::JSNodeFilterCondition(JSObject * _filter)
    : filter( _filter )
{
}

void JSNodeFilterCondition::mark()
{
    filter->mark();
}

short JSNodeFilterCondition::acceptNode(Node* filterNode) const
{
    Node *node = filterNode;
    Frame *frame = node->document()->frame();
    KJSProxy *proxy = frame->jScript();
    if (proxy && filter->implementsCall()) {
        JSLock lock;
        ExecState *exec = proxy->interpreter()->globalExec();
        List args;
        args.append(toJS(exec, node));
        JSObject *obj = filter;
        JSValue *result = obj->call(exec, obj, args);
        return result->toInt32(exec);
    }

    return NodeFilter::FILTER_REJECT;
}

} // namespace
