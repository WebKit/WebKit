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
#include <qptrdict.h>

using namespace KJS;

QPtrDict<DOMNodeIterator> nodeIterators;
QPtrDict<DOMNodeFilter> nodeFilters;
QPtrDict<DOMTreeWalker> treeWalkers;

// -------------------------------------------------------------------------

const TypeInfo DOMNodeIterator::info = { "NodeIterator", HostType, 0, 0, 0 };

DOMNodeIterator::~DOMNodeIterator()
{
  nodeIterators.remove(nodeIterator.handle());
}

KJSO DOMNodeIterator::tryGet(const UString &p) const
{
  DOM::NodeIterator ni(nodeIterator);
  if (p == "root")
    return getDOMNode(ni.root());
  else if (p == "whatToShow")
    return Number(ni.whatToShow());
  else if (p == "filter")
    return getDOMNodeFilter(ni.filter());
  else if (p == "expandEntityReferences")
    return Boolean(ni.expandEntityReferences());
  else if (p == "nextNode")
    return new DOMNodeIteratorFunc(nodeIterator,DOMNodeIteratorFunc::NextNode);
  else if (p == "previousNode")
    return new DOMNodeIteratorFunc(nodeIterator,DOMNodeIteratorFunc::PreviousNode);
  else if (p == "detach")
    return new DOMNodeIteratorFunc(nodeIterator,DOMNodeIteratorFunc::Detach);
  else
    return DOMObject::tryGet(p);
}

Completion DOMNodeIteratorFunc::tryExecute(const List &/*args*/)
{
  KJSO result;

  switch (id) {
    case PreviousNode:
      result = getDOMNode(nodeIterator.previousNode());
      break;
    case NextNode:
      result = getDOMNode(nodeIterator.nextNode());
      break;
    case Detach:
      nodeIterator.detach();
      result = Undefined();
      break;
  };

  return Completion(ReturnValue,result);
}

KJSO KJS::getDOMNodeIterator(DOM::NodeIterator ni)
{
  DOMNodeIterator *ret;
  if (ni.isNull())
    return Null();
  else if ((ret = nodeIterators[ni.handle()]))
    return ret;
  else {
    ret = new DOMNodeIterator(ni);
    nodeIterators.insert(ni.handle(),ret);
    return ret;
  }
}


// -------------------------------------------------------------------------

const TypeInfo NodeFilterPrototype::info = { "NodeFilterPrototype", HostType, 0, 0, 0 };
// ### make this protype of Range objects? (also for Node)

KJSO NodeFilterPrototype::tryGet(const UString &p) const
{
  if (p == "FILTER_ACCEPT")
    return Number((long unsigned int)DOM::NodeFilter::FILTER_ACCEPT);
  if (p == "FILTER_REJECT")
    return Number((long unsigned int)DOM::NodeFilter::FILTER_REJECT);
  if (p == "FILTER_SKIP")
    return Number((long unsigned int)DOM::NodeFilter::FILTER_SKIP);
  if (p == "SHOW_ALL")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_ALL);
  if (p == "SHOW_ELEMENT")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_ELEMENT);
  if (p == "SHOW_ATTRIBUTE")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_ATTRIBUTE);
  if (p == "SHOW_TEXT")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_TEXT);
  if (p == "SHOW_CDATA_SECTION")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_CDATA_SECTION);
  if (p == "SHOW_ENTITY_REFERENCE")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_ENTITY_REFERENCE);
  if (p == "SHOW_ENTITY")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_ENTITY);
  if (p == "SHOW_PROCESSING_INSTRUCTION")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_PROCESSING_INSTRUCTION);
  if (p == "SHOW_COMMENT")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_COMMENT);
  if (p == "SHOW_DOCUMENT")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_DOCUMENT);
  if (p == "SHOW_DOCUMENT_TYPE")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_DOCUMENT_TYPE);
  if (p == "SHOW_DOCUMENT_FRAGMENT")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_DOCUMENT_FRAGMENT);
  if (p == "SHOW_NOTATION")
    return Number((long unsigned int)DOM::NodeFilter::SHOW_NOTATION);

  return DOMObject::tryGet(p);
}

KJSO KJS::getNodeFilterPrototype()
{
    KJSO proto = Global::current().get("[[nodeFilter.prototype]]");
    if (proto.isDefined())
        return proto;
    else
    {
        Object nodeFilterProto( new NodeFilterPrototype );
        Global::current().put("[[nodeFilter.prototype]]", nodeFilterProto);
        return nodeFilterProto;
    }
}


// -------------------------------------------------------------------------

const TypeInfo DOMNodeFilter::info = { "NodeFilter", HostType, 0, 0, 0 };


DOMNodeFilter::~DOMNodeFilter()
{
  nodeFilters.remove(nodeFilter.handle());
}

KJSO DOMNodeFilter::tryGet(const UString &p) const
{
  if (p == "acceptNode")
    return new DOMNodeFilterFunc(nodeFilter,DOMNodeFilterFunc::AcceptNode);
  else
    return DOMObject::tryGet(p);
}

Completion DOMNodeFilterFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (id) {
    case AcceptNode:
      result = Number(nodeFilter.acceptNode(toNode(args[0])));
      break;
  };

  return Completion(ReturnValue,result);
}

KJSO KJS::getDOMNodeFilter(DOM::NodeFilter nf)
{
  DOMNodeFilter *ret;
  if (nf.isNull())
    return Null();
  else if ((ret = nodeFilters[nf.handle()]))
    return ret;
  else {
    ret = new DOMNodeFilter(nf);
    nodeFilters.insert(nf.handle(),ret);
    return ret;
  }
}



// -------------------------------------------------------------------------

const TypeInfo DOMTreeWalker::info = { "TreeWalker", HostType, 0, 0, 0 };


DOMTreeWalker::~DOMTreeWalker()
{
  treeWalkers.remove(treeWalker.handle());
}

KJSO DOMTreeWalker::tryGet(const UString &p) const
{
  DOM::TreeWalker tw(treeWalker);
  if (p == "root")
    return getDOMNode(tw.root());
  if (p == "whatToShow")
    return Number(tw.whatToShow());
  if (p == "filter")
    return getDOMNodeFilter(tw.filter());
  if (p == "expandEntityReferences")
    return Boolean(tw.expandEntityReferences());
  if (p == "currentNode")
    return getDOMNode(tw.currentNode());
  if (p == "parentNode")
    return new DOMTreewalkerFunc(treeWalker,DOMTreewalkerFunc::ParentNode);
  if (p == "firstChild")
    return new DOMTreewalkerFunc(treeWalker,DOMTreewalkerFunc::FirstChild);
  if (p == "lastChild")
    return new DOMTreewalkerFunc(treeWalker,DOMTreewalkerFunc::LastChild);
  if (p == "previousSibling")
    return new DOMTreewalkerFunc(treeWalker,DOMTreewalkerFunc::PreviousSibling);
  if (p == "nextSibling")
    return new DOMTreewalkerFunc(treeWalker,DOMTreewalkerFunc::NextSibling);
  if (p == "previousNode")
    return new DOMTreewalkerFunc(treeWalker,DOMTreewalkerFunc::PreviousNode);
  if (p == "nextNode")
    return new DOMTreewalkerFunc(treeWalker,DOMTreewalkerFunc::NextNode);
  else
    return DOMObject::tryGet(p);
}


void DOMTreeWalker::tryPut(const UString &p, const KJSO& v)
{
  if (p == "currentNode") {
    treeWalker.setCurrentNode(toNode(v));
  }
  else
    Imp::put(p, v);
}

Completion DOMTreewalkerFunc::tryExecute(const List &/*args*/)
{
  KJSO result;

  switch (id) {
    case ParentNode:
      result = getDOMNode(treeWalker.parentNode());
      break;
    case FirstChild:
      result = getDOMNode(treeWalker.firstChild());
      break;
    case LastChild:
      result = getDOMNode(treeWalker.lastChild());
      break;
    case PreviousSibling:
      result = getDOMNode(treeWalker.previousSibling());
      break;
    case NextSibling:
      result = getDOMNode(treeWalker.nextSibling());
      break;
    case PreviousNode:
      result = getDOMNode(treeWalker.previousSibling());
      break;
    case NextNode:
      result = getDOMNode(treeWalker.nextNode());
      break;
  };

  return Completion(ReturnValue,result);
}

KJSO KJS::getDOMTreeWalker(DOM::TreeWalker tw)
{
  DOMTreeWalker *ret;
  if (tw.isNull())
    return Null();
  else if ((ret = treeWalkers[tw.handle()]))
    return ret;
  else {
    ret = new DOMTreeWalker(tw);
    treeWalkers.insert(tw.handle(),ret);
    return ret;
  }
}

DOM::NodeFilter KJS::toNodeFilter(const KJSO& obj)
{
  if (!obj.derivedFrom("NodeFilter"))
    return DOM::NodeFilter();

  const DOMNodeFilter *dobj = static_cast<const DOMNodeFilter*>(obj.imp());
  return dobj->toNodeFilter();
}

// -------------------------------------------------------------------------

JSNodeFilter::JSNodeFilter(KJSO _filter) : DOM::CustomNodeFilter()
{
    filter = _filter;
}

JSNodeFilter::~JSNodeFilter()
{
}

short JSNodeFilter::acceptNode(const DOM::Node &n)
{
    KJSO acceptNodeFunc = filter.get("acceptNode");
    if (acceptNodeFunc.implementsCall()) {
	List args;
	args.append(getDOMNode(n));
	KJSO result = acceptNodeFunc.executeCall(filter,&args);
	return result.toNumber().intValue();
    }
    else
	return DOM::NodeFilter::FILTER_REJECT;
}
