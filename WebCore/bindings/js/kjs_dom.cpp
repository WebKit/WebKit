/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "kjs_dom.h"

#include "CDATASection.h"
#include "Comment.h"
#include "DOMImplementation.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Element.h"
#include "Entity.h"
#include "EntityReference.h"
#include "Event.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSAttr.h"
#include "JSCDATASection.h"
#include "JSComment.h"
#include "JSDOMImplementation.h"
#include "JSDocumentFragment.h"
#include "JSDocumentType.h"
#include "JSElement.h"
#include "JSEntity.h"
#include "JSEntityReference.h"
#include "JSHTMLDocument.h"
#include "JSHTMLElementWrapperFactory.h"
#include "JSNamedNodeMap.h"
#include "JSNode.h"
#include "JSNodeList.h"
#include "JSNotation.h"
#include "JSProcessingInstruction.h"
#include "JSRange.h"
#include "JSText.h"
#include "NamedNodeMap.h"
#include "NodeList.h"
#include "Notation.h"
#include "ProcessingInstruction.h"
#include "Range.h"
#include "RenderView.h"
#include "kjs_css.h"
#include "kjs_events.h"
#include "kjs_window.h"
#include "xmlhttprequest.h"

#if ENABLE(SVG)
#include "JSSVGDocument.h"
#include "JSSVGElementInstance.h"
#include "JSSVGElementWrapperFactory.h"
#include "SVGDocument.h"
#include "SVGElement.h"
#endif

#if USE(JAVASCRIPTCORE_BINDINGS)
#include <JavaScriptCore/runtime_object.h>
#endif

using namespace WebCore;
using namespace HTMLNames;
using namespace EventNames;

#include "kjs_dom.lut.h"

namespace KJS {

// -------------------------------------------------------------------------
/* Source for DOMNodePrototypeTable. Use "make hashtables" to regenerate.
@begin DOMNodePrototypeTable 25
  insertBefore  DOMNode::InsertBefore   DontDelete|Function 2
  replaceChild  DOMNode::ReplaceChild   DontDelete|Function 2
  removeChild   DOMNode::RemoveChild    DontDelete|Function 1
  appendChild   DOMNode::AppendChild    DontDelete|Function 1
  hasAttributes DOMNode::HasAttributes  DontDelete|Function 0
  hasChildNodes DOMNode::HasChildNodes  DontDelete|Function 0
  cloneNode     DOMNode::CloneNode      DontDelete|Function 1
# DOM2
  normalize     DOMNode::Normalize      DontDelete|Function 0
  isSupported   DOMNode::IsSupported    DontDelete|Function 2
# DOM3
  isSameNode    DOMNode::IsSameNode     DontDelete|Function 1
  isEqualNode   DOMNode::IsEqualNode    DontDelete|Function 1
  isDefaultNamespace    DOMNode::IsDefaultNamespace DontDelete|Function 1
  lookupNamespaceURI    DOMNode::LookupNamespaceURI DontDelete|Function 1
  lookupPrefix  DOMNode::LookupPrefix   DontDelete|Function 1
@end
*/
KJS_IMPLEMENT_PROTOTYPE_FUNCTION(DOMNodePrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("DOMNode", DOMNodePrototype, DOMNodePrototypeFunction)

const ClassInfo DOMNode::info = { "Node", 0, &DOMNodeTable, 0 };

DOMNode::DOMNode(ExecState* exec, Node* n)
  : m_impl(n)
{
  setPrototype(DOMNodePrototype::self(exec));
}

DOMNode::~DOMNode()
{
  ScriptInterpreter::forgetDOMNodeForDocument(m_impl->document(), m_impl.get());
}

void DOMNode::mark()
{
  ASSERT(!marked());

  Node* node = m_impl.get();

  // Nodes in the document are kept alive by ScriptInterpreter::mark,
  // so we have no special responsibilities and can just call the base class here.
  if (node->inDocument()) {
    DOMObject::mark();
    return;
  }

  // This is a node outside the document, so find the root of the tree it is in,
  // and start marking from there.
  Node* root = node;
  for (Node* current = m_impl.get(); current; current = current->parentNode()) {
    root = current;
  }

  // If we're already marking this tree, then we can simply mark this wrapper
  // by calling the base class; our caller is iterating the tree.
  if (root->m_inSubtreeMark) {
    DOMObject::mark();
    return;
  }

  // Mark the whole tree; use the global set of roots to avoid reentering.
  root->m_inSubtreeMark = true;
  for (Node* nodeToMark = root; nodeToMark; nodeToMark = nodeToMark->traverseNextNode()) {
    DOMNode *wrapper = ScriptInterpreter::getDOMNodeForDocument(m_impl->document(), nodeToMark);
    if (wrapper) {
      if (!wrapper->marked())
        wrapper->mark();
    } else if (nodeToMark == node) {
      // This is the case where the map from the document to wrappers has
      // been cleared out, but a wrapper is being marked. For now, we'll
      // let the rest of the tree of wrappers get collected, because we have
      // no good way of finding them. Later we should test behavior of other
      // browsers and see if we need to preserve other wrappers in this case.
      if (!marked())
        mark();
    }
  }
  root->m_inSubtreeMark = false;

  // Double check that we actually ended up marked. This assert caught problems in the past.
  ASSERT(marked());
}

/* Source for DOMNodeTable. Use "make hashtables" to regenerate.
@begin DOMNodeTable 25
  nodeName      DOMNode::NodeName       DontDelete|ReadOnly
  nodeValue     DOMNode::NodeValue      DontDelete
  nodeType      DOMNode::NodeType       DontDelete|ReadOnly
  parentNode    DOMNode::ParentNode     DontDelete|ReadOnly
  parentElement DOMNode::ParentElement  DontDelete|ReadOnly
  childNodes    DOMNode::ChildNodes     DontDelete|ReadOnly
  firstChild    DOMNode::FirstChild     DontDelete|ReadOnly
  lastChild     DOMNode::LastChild      DontDelete|ReadOnly
  previousSibling  DOMNode::PreviousSibling DontDelete|ReadOnly
  nextSibling   DOMNode::NextSibling    DontDelete|ReadOnly
  attributes    DOMNode::Attributes     DontDelete|ReadOnly
  baseURI       DOMNode::BaseURI        DontDelete|ReadOnly
  namespaceURI  DOMNode::NamespaceURI   DontDelete|ReadOnly
# DOM2
  prefix        DOMNode::Prefix         DontDelete
  localName     DOMNode::LocalName      DontDelete|ReadOnly
  ownerDocument DOMNode::OwnerDocument  DontDelete|ReadOnly
# DOM3
  textContent   DOMNode::TextContent    DontDelete
@end
*/
bool DOMNode::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMNode, DOMObject>(exec, &DOMNodeTable, this, propertyName, slot);
}

JSValue* DOMNode::getValueProperty(ExecState* exec, int token) const
{
  Node& node = *m_impl;
  switch (token) {
  case NodeName:
    return jsStringOrNull(node.nodeName());
  case NodeValue:
    return jsStringOrNull(node.nodeValue());
  case NodeType:
    return jsNumber(node.nodeType());
  case ParentNode:
  case ParentElement: // IE only apparently
    return toJS(exec,node.parentNode());
  case ChildNodes:
    return toJS(exec,node.childNodes().get());
  case FirstChild:
    return toJS(exec,node.firstChild());
  case LastChild:
    return toJS(exec,node.lastChild());
  case PreviousSibling:
    return toJS(exec,node.previousSibling());
  case NextSibling:
    return toJS(exec,node.nextSibling());
  case Attributes:
    return toJS(exec,node.attributes());
  case BaseURI:
    return jsStringOrNull(node.baseURI());
  case NamespaceURI:
    return jsStringOrNull(node.namespaceURI());
  case Prefix:
    return jsStringOrNull(node.prefix());
  case LocalName:
    return jsStringOrNull(node.localName());
  case OwnerDocument:
    return toJS(exec,node.ownerDocument());
  case TextContent:
    return jsStringOrNull(node.textContent());
  }

  return jsUndefined();
}

void DOMNode::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    lookupPut<DOMNode,DOMObject>(exec, propertyName, value, attr, &DOMNodeTable, this);
}

void DOMNode::putValueProperty(ExecState* exec, int token, JSValue* value, int /*attr*/)
{
  DOMExceptionTranslator exception(exec);
  Node& node = *m_impl;
  switch (token) {
  case NodeValue:
    node.setNodeValue(valueToStringWithNullCheck(exec, value), exception);
    break;
  case Prefix:
    node.setPrefix(valueToStringWithNullCheck(exec, value), exception);
    break;
  case TextContent:
    node.setTextContent(valueToStringWithNullCheck(exec, value), exception);
    break;
  }
}

JSValue* DOMNodePrototypeFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMNode::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  Node& node = *static_cast<DOMNode*>(thisObj)->impl();
  switch (id) {
    case DOMNode::HasAttributes:
      return jsBoolean(node.hasAttributes());
    case DOMNode::HasChildNodes:
      return jsBoolean(node.hasChildNodes());
    case DOMNode::CloneNode:
      return toJS(exec,node.cloneNode(args[0]->toBoolean(exec)));
    case DOMNode::Normalize:
      node.normalize();
      return jsUndefined();
    case DOMNode::IsSupported:
        return jsBoolean(node.isSupported(args[0]->toString(exec),
                                          valueToStringWithNullCheck(exec, args[1])));
    case DOMNode::IsSameNode:
        return jsBoolean(node.isSameNode(toNode(args[0])));
    case DOMNode::IsEqualNode:
        return jsBoolean(node.isEqualNode(toNode(args[0])));
    case DOMNode::IsDefaultNamespace:
        return jsBoolean(node.isDefaultNamespace(valueToStringWithNullCheck(exec, args[0])));
    case DOMNode::LookupNamespaceURI:
        return jsStringOrNull(node.lookupNamespaceURI(valueToStringWithNullCheck(exec, args[0])));
    case DOMNode::LookupPrefix:
        return jsStringOrNull(node.lookupPrefix(valueToStringWithNullCheck(exec, args[0])));
    case DOMNode::AppendChild:
      if (node.appendChild(toNode(args[0]), exception))
        return args[0];
      return jsNull();
    case DOMNode::RemoveChild:
      if (node.removeChild(toNode(args[0]), exception))
        return args[0];
      return jsNull();
    case DOMNode::InsertBefore:
      if (node.insertBefore(toNode(args[0]), toNode(args[1]), exception))
        return args[0];
      return jsNull();
    case DOMNode::ReplaceChild:
     if (node.replaceChild(toNode(args[0]), toNode(args[1]), exception))
        return args[1];
      return jsNull();
  }

  return jsUndefined();
}

Attr* toAttr(JSValue* val, bool& ok)
{
    if (!val || !val->isObject(&JSAttr::info)) {
        ok = false;
        return 0;
    }

    ok = true;
    return static_cast<Attr*>(static_cast<DOMNode*>(val)->impl());
}

// -------------------------------------------------------------------------

bool checkNodeSecurity(ExecState* exec, Node* n)
{
  if (!n) 
    return false;

  // Check to see if the currently executing interpreter is allowed to access the specified node
  Window* win = Window::retrieveWindow(n->document()->frame());
  return win && win->isSafeScript(exec);
}

JSValue* toJS(ExecState* exec, EventTarget* target)
{
    if (!target)
        return jsNull();
    
#if ENABLE(SVG)
    // SVGElementInstance supports both toSVGElementInstance and toNode since so much mouse handling code depends on toNode returning a valid node.
    SVGElementInstance* instance = target->toSVGElementInstance();
    if (instance)
        return toJS(exec, instance);
#endif
    
    Node* node = target->toNode();
    if (node)
        return toJS(exec, node);

    XMLHttpRequest* xhr = target->toXMLHttpRequest();
    if (xhr) {
        // XMLHttpRequest is always created via JS, so we don't need to use cacheDOMObject() here.
        ScriptInterpreter* interp = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter());
        return interp->getDOMObject(xhr);
    }

    // There are two kinds of EventTargets: EventTargetNode and XMLHttpRequest.
    // If SVG support is enabled, there is also SVGElementInstance.
    ASSERT(0);
    return jsNull();
}

JSValue* getRuntimeObject(ExecState* exec, Node* n)
{
    if (!n)
        return 0;

#if USE(JAVASCRIPTCORE_BINDINGS)
    if (n->hasTagName(objectTag) || n->hasTagName(embedTag) || n->hasTagName(appletTag)) {
        HTMLPlugInElement* plugInElement = static_cast<HTMLPlugInElement*>(n);
        if (plugInElement->getInstance())
            // The instance is owned by the PlugIn element.
            return new RuntimeObjectImp(plugInElement->getInstance());
    }
#endif

    // If we don't have a runtime object return 0.
    return 0;
}

} // namespace KJS
