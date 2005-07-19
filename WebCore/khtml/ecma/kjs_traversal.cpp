// -*- c-basic-offset: 2 -*-
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

#include "kjs_traversal.h"
#include "kjs_traversal.lut.h"
#include "kjs_proxy.h"
#include <dom/dom_node.h>
#include <xml/dom_nodeimpl.h>
#include <xml/dom_docimpl.h>
#include <khtmlview.h>
#include <kdebug.h>

using DOM::FilterNode;
using DOM::NodeFilterImpl;
using DOM::NodeImpl;
using DOM::NodeIteratorImpl;
using DOM::TreeWalkerImpl;

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
DEFINE_PROTOTYPE("DOMNodeIterator",DOMNodeIteratorProto)
IMPLEMENT_PROTOFUNC(DOMNodeIteratorProtoFunc)
IMPLEMENT_PROTOTYPE(DOMNodeIteratorProto,DOMNodeIteratorProtoFunc)

DOMNodeIterator::DOMNodeIterator(ExecState *exec, NodeIteratorImpl *ni)
  : m_impl(ni)
{
  setPrototype(DOMNodeIteratorProto::self(exec));
}

DOMNodeIterator::~DOMNodeIterator()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

Value DOMNodeIterator::get(ExecState *exec, const Identifier &p) const
{
  return lookupGetValue<DOMNodeIterator,DOMObject>(exec,p,&DOMNodeIteratorTable,this);
}

Value DOMNodeIterator::getValueProperty(ExecState *exec, int token) const
{
  NodeIteratorImpl &ni = *m_impl;
  switch (token) {
  case Root:
    return getDOMNode(exec,ni.root());
  case WhatToShow:
    return Number(ni.whatToShow());
  case Filter:
    return getDOMNodeFilter(exec,ni.filter());
  case ExpandEntityReferences:
    return Boolean(ni.expandEntityReferences());
  case ReferenceNode:
    return getDOMNode(exec,ni.referenceNode());
  case PointerBeforeReferenceNode:
    return Boolean(ni.pointerBeforeReferenceNode());
 default:
   kdWarning() << "Unhandled token in DOMNodeIterator::getValueProperty : " << token << endl;
   return Value();
  }
}

Value DOMNodeIteratorProtoFunc::call(ExecState *exec, Object &thisObj, const List &)
{
  if (!thisObj.inherits(&KJS::DOMNodeIterator::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOMExceptionTranslator exception(exec);
  NodeIteratorImpl &nodeIterator = *static_cast<DOMNodeIterator *>(thisObj.imp())->impl();
  switch (id) {
  case DOMNodeIterator::PreviousNode:
    return getDOMNode(exec,nodeIterator.previousNode(exception));
  case DOMNodeIterator::NextNode:
    return getDOMNode(exec,nodeIterator.nextNode(exception));
  case DOMNodeIterator::Detach:
    nodeIterator.detach(exception);
    return Undefined();
  }
  return Undefined();
}

ValueImp *getDOMNodeIterator(ExecState *exec, NodeIteratorImpl *ni)
{
  return cacheDOMObject<NodeIteratorImpl, DOMNodeIterator>(exec, ni);
}


// -------------------------------------------------------------------------

const ClassInfo NodeFilterConstructor::info = { "NodeFilterConstructor", 0, &NodeFilterConstructorTable, 0 };
/*
@begin NodeFilterConstructorTable 17
  FILTER_ACCEPT		DOM::NodeFilter::FILTER_ACCEPT	DontDelete|ReadOnly
  FILTER_REJECT		DOM::NodeFilter::FILTER_REJECT	DontDelete|ReadOnly
  FILTER_SKIP		DOM::NodeFilter::FILTER_SKIP	DontDelete|ReadOnly
  SHOW_ALL		DOM::NodeFilter::SHOW_ALL	DontDelete|ReadOnly
  SHOW_ELEMENT		DOM::NodeFilter::SHOW_ELEMENT	DontDelete|ReadOnly
  SHOW_ATTRIBUTE	DOM::NodeFilter::SHOW_ATTRIBUTE	DontDelete|ReadOnly
  SHOW_TEXT		DOM::NodeFilter::SHOW_TEXT	DontDelete|ReadOnly
  SHOW_CDATA_SECTION	DOM::NodeFilter::SHOW_CDATA_SECTION	DontDelete|ReadOnly
  SHOW_ENTITY_REFERENCE	DOM::NodeFilter::SHOW_ENTITY_REFERENCE	DontDelete|ReadOnly
  SHOW_ENTITY		DOM::NodeFilter::SHOW_ENTITY	DontDelete|ReadOnly
  SHOW_PROCESSING_INSTRUCTION	DOM::NodeFilter::SHOW_PROCESSING_INSTRUCTION	DontDelete|ReadOnly
  SHOW_COMMENT		DOM::NodeFilter::SHOW_COMMENT	DontDelete|ReadOnly
  SHOW_DOCUMENT		DOM::NodeFilter::SHOW_DOCUMENT	DontDelete|ReadOnly
  SHOW_DOCUMENT_TYPE	DOM::NodeFilter::SHOW_DOCUMENT_TYPE	DontDelete|ReadOnly
  SHOW_DOCUMENT_FRAGMENT	DOM::NodeFilter::SHOW_DOCUMENT_FRAGMENT	DontDelete|ReadOnly
  SHOW_NOTATION		DOM::NodeFilter::SHOW_NOTATION	DontDelete|ReadOnly
@end
*/
Value NodeFilterConstructor::get(ExecState *exec, const Identifier &p) const
{
  return lookupGetValue<NodeFilterConstructor,DOMObject>(exec,p,&NodeFilterConstructorTable,this);
}

Value NodeFilterConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return Number(token);
}

Value getNodeFilterConstructor(ExecState *exec)
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
DEFINE_PROTOTYPE("DOMNodeFilter",DOMNodeFilterProto)
IMPLEMENT_PROTOFUNC(DOMNodeFilterProtoFunc)
IMPLEMENT_PROTOTYPE(DOMNodeFilterProto,DOMNodeFilterProtoFunc)

DOMNodeFilter::DOMNodeFilter(ExecState *exec, NodeFilterImpl *nf)
  : m_impl(nf) 
{
  setPrototype(DOMNodeFilterProto::self(exec));
}

DOMNodeFilter::~DOMNodeFilter()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

Value DOMNodeFilterProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMNodeFilter::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  NodeFilterImpl &nodeFilter = *static_cast<DOMNodeFilter *>(thisObj.imp())->impl();
  switch (id) {
    case DOMNodeFilter::AcceptNode:
      return Number(nodeFilter.acceptNode(toNode(args[0])));
  }
  return Undefined();
}

ValueImp *getDOMNodeFilter(ExecState *exec, NodeFilterImpl *nf)
{
    return cacheDOMObject<NodeFilterImpl, DOMNodeFilter>(exec, nf);
}

NodeFilterImpl *toNodeFilter(ValueImp *val)
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
DEFINE_PROTOTYPE("DOMTreeWalker",DOMTreeWalkerProto)
IMPLEMENT_PROTOFUNC(DOMTreeWalkerProtoFunc)
IMPLEMENT_PROTOTYPE(DOMTreeWalkerProto,DOMTreeWalkerProtoFunc)

DOMTreeWalker::DOMTreeWalker(ExecState *exec, TreeWalkerImpl *tw)
  : m_impl(tw)
{
  setPrototype(DOMTreeWalkerProto::self(exec));
}

DOMTreeWalker::~DOMTreeWalker()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

Value DOMTreeWalker::get(ExecState *exec, const Identifier &p) const
{
  return lookupGetValue<DOMTreeWalker,DOMObject>(exec,p,&DOMTreeWalkerTable,this);
}

Value DOMTreeWalker::getValueProperty(ExecState *exec, int token) const
{
  TreeWalkerImpl &tw = *m_impl;
  switch (token) {
  case Root:
    return getDOMNode(exec,tw.root());
  case WhatToShow:
    return Number(tw.whatToShow());
  case Filter:
    return getDOMNodeFilter(exec,tw.filter());
  case ExpandEntityReferences:
    return Boolean(tw.expandEntityReferences());
  case CurrentNode:
    return getDOMNode(exec,tw.currentNode());
  default:
    kdWarning() << "Unhandled token in DOMTreeWalker::getValueProperty : " << token << endl;
    return Value();
  }
}

void DOMTreeWalker::put(ExecState *exec, const Identifier &propertyName,
                           const Value& value, int attr)
{
  if (propertyName == "currentNode") {
    DOMExceptionTranslator exception(exec);
    m_impl->setCurrentNode(toNode(value), exception);
  }
  else
    ObjectImp::put(exec, propertyName, value, attr);
}

Value DOMTreeWalkerProtoFunc::call(ExecState *exec, Object &thisObj, const List &)
{
  if (!thisObj.inherits(&KJS::DOMTreeWalker::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  TreeWalkerImpl &treeWalker = *static_cast<DOMTreeWalker *>(thisObj.imp())->impl();
  switch (id) {
    case DOMTreeWalker::ParentNode:
      return getDOMNode(exec,treeWalker.parentNode());
    case DOMTreeWalker::FirstChild:
      return getDOMNode(exec,treeWalker.firstChild());
    case DOMTreeWalker::LastChild:
      return getDOMNode(exec,treeWalker.lastChild());
    case DOMTreeWalker::PreviousSibling:
      return getDOMNode(exec,treeWalker.previousSibling());
    case DOMTreeWalker::NextSibling:
      return getDOMNode(exec,treeWalker.nextSibling());
    case DOMTreeWalker::PreviousNode:
      return getDOMNode(exec,treeWalker.previousNode());
    case DOMTreeWalker::NextNode:
      return getDOMNode(exec,treeWalker.nextNode());
  }
  return Undefined();
}

ValueImp *getDOMTreeWalker(ExecState *exec, TreeWalkerImpl *tw)
{
  return cacheDOMObject<TreeWalkerImpl, DOMTreeWalker>(exec, tw);
}

// -------------------------------------------------------------------------

JSNodeFilterCondition::JSNodeFilterCondition(Object & _filter) : filter( _filter )
{
}

short JSNodeFilterCondition::acceptNode(FilterNode filterNode) const
{
#if !KHTML_NO_CPLUSPLUS_DOM
    NodeImpl *node = filterNode.handle();
#else
    NodeImpl *node = filterNode;
#endif
    KHTMLPart *part = node->getDocument()->part();
    KJSProxy *proxy = KJSProxy::proxy(part);
    if (proxy && filter.implementsCall()) {
        ExecState *exec = proxy->interpreter()->globalExec();
        List args;
        args.append(getDOMNode(exec, node));
        Object obj = const_cast<ProtectedObject &>(filter);
        Value result = obj.call(exec, obj, args);
        return result.toInt32(exec);
    }

    return DOM::NodeFilter::FILTER_REJECT;
}

} // namespace
