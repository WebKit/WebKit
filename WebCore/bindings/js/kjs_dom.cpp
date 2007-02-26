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
#include "Event.h"
#include "EventTarget.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSAttr.h"
#include "JSCharacterData.h"
#include "JSDOMImplementation.h"
#include "JSDocumentFragment.h"
#include "JSDocumentType.h"
#include "JSElement.h"
#include "JSEntity.h"
#include "JSHTMLDocument.h"
#include "JSHTMLElementWrapperFactory.h"
#include "JSNode.h"
#include "JSNotation.h"
#include "JSProcessingInstruction.h"
#include "JSRange.h"
#include "JSText.h"
#include "Settings.h"
#include "NamedNodeMap.h"
#include "Notation.h"
#include "ProcessingInstruction.h"
#include "Range.h"
#include "RenderView.h"
#include "xmlhttprequest.h"
#include "kjs_css.h"
#include "kjs_events.h"
#include "kjs_traversal.h"
#include "kjs_window.h"

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
  assert(!marked());

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

  static HashSet<Node*> markingRoots;

  // If we're already marking this tree, then we can simply mark this wrapper
  // by calling the base class; our caller is iterating the tree.
  if (markingRoots.contains(root)) {
    DOMObject::mark();
    return;
  }

  // Mark the whole tree; use the global set of roots to avoid reentering.
  markingRoots.add(root);
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
  markingRoots.remove(root);

  // Double check that we actually ended up marked. This assert caught problems in the past.
  assert(marked());
}

bool DOMNode::toBoolean(ExecState* ) const
{
    return m_impl;
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
    node.setNodeValue(value->toString(exec), exception);
    break;
  case Prefix:
    node.setPrefix(value->toString(exec), exception);
    break;
  case TextContent:
    node.setTextContent(valueToStringWithNullCheck(exec, value), exception);
    break;
  }
}

JSValue* DOMNode::toPrimitive(ExecState* exec, JSType) const
{
  if (!m_impl)
    return jsNull();

  return jsString(toString(exec));
}

UString DOMNode::toString(ExecState*) const
{
    if (!m_impl)
        return "null";
    return "[object " + className() + "]";
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

EventTargetNode *toEventTargetNode(JSValue* val)
{
    if (!val || !val->isObject(&DOMEventTargetNode::info))
        return 0;
    return static_cast<EventTargetNode*>(static_cast<DOMEventTargetNode*>(val)->impl());
}

Node* toNode(JSValue* val)
{
    if (!val || !val->isObject(&DOMNode::info))
        return 0;
    return static_cast<DOMNode*>(val)->impl();
}

// -------------------------------------------------------------------------

/* Source for DOMEventTargetNodeTable
@begin DOMEventTargetNodeTable 50
onabort       DOMEventTargetNode::OnAbort                DontDelete
onblur        DOMEventTargetNode::OnBlur                 DontDelete
onchange      DOMEventTargetNode::OnChange               DontDelete
onclick       DOMEventTargetNode::OnClick                DontDelete
oncontextmenu DOMEventTargetNode::OnContextMenu          DontDelete
ondblclick    DOMEventTargetNode::OnDblClick             DontDelete
onbeforecut   DOMEventTargetNode::OnBeforeCut            DontDelete
oncut         DOMEventTargetNode::OnCut                  DontDelete
onbeforecopy  DOMEventTargetNode::OnBeforeCopy           DontDelete
oncopy        DOMEventTargetNode::OnCopy                 DontDelete
onbeforepaste DOMEventTargetNode::OnBeforePaste          DontDelete
onpaste       DOMEventTargetNode::OnPaste                DontDelete
ondrag        DOMEventTargetNode::OnDrag                 DontDelete
ondragend     DOMEventTargetNode::OnDragEnd              DontDelete
ondragenter   DOMEventTargetNode::OnDragEnter            DontDelete
ondragleave   DOMEventTargetNode::OnDragLeave            DontDelete
ondragover    DOMEventTargetNode::OnDragOver             DontDelete
ondragstart   DOMEventTargetNode::OnDragStart            DontDelete
ondrop        DOMEventTargetNode::OnDrop                 DontDelete
onerror       DOMEventTargetNode::OnError                DontDelete
onfocus       DOMEventTargetNode::OnFocus                DontDelete
oninput       DOMEventTargetNode::OnInput                DontDelete
onkeydown     DOMEventTargetNode::OnKeyDown              DontDelete
onkeypress    DOMEventTargetNode::OnKeyPress             DontDelete
onkeyup       DOMEventTargetNode::OnKeyUp                DontDelete
onload        DOMEventTargetNode::OnLoad                 DontDelete
onmousedown   DOMEventTargetNode::OnMouseDown            DontDelete
onmousemove   DOMEventTargetNode::OnMouseMove            DontDelete
onmouseout    DOMEventTargetNode::OnMouseOut             DontDelete
onmouseover   DOMEventTargetNode::OnMouseOver            DontDelete
onmouseup     DOMEventTargetNode::OnMouseUp              DontDelete
onmousewheel  DOMEventTargetNode::OnMouseWheel           DontDelete
onreset       DOMEventTargetNode::OnReset                DontDelete
onresize      DOMEventTargetNode::OnResize               DontDelete
onscroll      DOMEventTargetNode::OnScroll               DontDelete
onsearch      DOMEventTargetNode::OnSearch               DontDelete
onselect      DOMEventTargetNode::OnSelect               DontDelete
onselectstart DOMEventTargetNode::OnSelectStart          DontDelete
onsubmit      DOMEventTargetNode::OnSubmit               DontDelete
onunload      DOMEventTargetNode::OnUnload               DontDelete
@end
*/

DOMEventTargetNode::DOMEventTargetNode(ExecState* exec, Node* n)
    : JSNode(exec, n)
{
    setPrototype(DOMEventTargetNodePrototype::self(exec));
}

bool DOMEventTargetNode::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<DOMEventTargetNode, DOMNode>(exec, &DOMEventTargetNodeTable, this, propertyName, slot);
}

JSValue* DOMEventTargetNode::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
        case OnAbort:
            return getListener(abortEvent);
        case OnBlur:
            return getListener(blurEvent);
        case OnChange:
            return getListener(changeEvent);
        case OnClick:
            return getListener(clickEvent);
        case OnContextMenu:
            return getListener(contextmenuEvent);
        case OnDblClick:
            return getListener(dblclickEvent);
        case OnError:
            return getListener(errorEvent);
        case OnFocus:
            return getListener(focusEvent);
        case OnInput:
            return getListener(inputEvent);
        case OnKeyDown:
            return getListener(keydownEvent);
        case OnKeyPress:
            return getListener(keypressEvent);
        case OnKeyUp:
            return getListener(keyupEvent);
        case OnLoad:
            return getListener(loadEvent);
        case OnMouseDown:
            return getListener(mousedownEvent);
        case OnMouseMove:
            return getListener(mousemoveEvent);
        case OnMouseOut:
            return getListener(mouseoutEvent);
        case OnMouseOver:
            return getListener(mouseoverEvent);
        case OnMouseUp:
            return getListener(mouseupEvent);      
        case OnMouseWheel:
            return getListener(mousewheelEvent);      
        case OnBeforeCut:
            return getListener(beforecutEvent);
        case OnCut:
            return getListener(cutEvent);
        case OnBeforeCopy:
            return getListener(beforecopyEvent);
        case OnCopy:
            return getListener(copyEvent);
        case OnBeforePaste:
            return getListener(beforepasteEvent);
        case OnPaste:
            return getListener(pasteEvent);
        case OnDragEnter:
            return getListener(dragenterEvent);
        case OnDragOver:
            return getListener(dragoverEvent);
        case OnDragLeave:
            return getListener(dragleaveEvent);
        case OnDrop:
            return getListener(dropEvent);
        case OnDragStart:
            return getListener(dragstartEvent);
        case OnDrag:
            return getListener(dragEvent);
        case OnDragEnd:
            return getListener(dragendEvent);
        case OnReset:
            return getListener(resetEvent);
        case OnResize:
            return getListener(resizeEvent);
        case OnScroll:
            return getListener(scrollEvent);
        case OnSearch:
            return getListener(searchEvent);
        case OnSelect:
            return getListener(selectEvent);
        case OnSelectStart:
            return getListener(selectstartEvent);
        case OnSubmit:
            return getListener(submitEvent);
        case OnUnload:
            return getListener(unloadEvent);
    }
    
    return jsUndefined();
}

void DOMEventTargetNode::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    lookupPut<DOMEventTargetNode, DOMNode>(exec, propertyName, value, attr, &DOMEventTargetNodeTable, this);
}

void DOMEventTargetNode::putValueProperty(ExecState* exec, int token, JSValue* value, int /*attr*/)
{
    switch (token) {
        case OnAbort:
            setListener(exec, abortEvent, value);
            break;
        case OnBlur:
            setListener(exec, blurEvent, value);
            break;
        case OnChange:
            setListener(exec, changeEvent, value);
            break;
        case OnClick:
            setListener(exec, clickEvent, value);
            break;
        case OnContextMenu:
            setListener(exec, contextmenuEvent, value);
            break;
        case OnDblClick:
            setListener(exec, dblclickEvent, value);
            break;
        case OnError:
            setListener(exec, errorEvent, value);
            break;
        case OnFocus:
            setListener(exec, focusEvent, value);
            break;
        case OnInput:
            setListener(exec, inputEvent, value);
            break;
        case OnKeyDown:
            setListener(exec, keydownEvent, value);
            break;
        case OnKeyPress:
            setListener(exec, keypressEvent, value);
            break;
        case OnKeyUp:
            setListener(exec, keyupEvent, value);
            break;
        case OnLoad:
            setListener(exec, loadEvent, value);
            break;
        case OnMouseDown:
            setListener(exec, mousedownEvent, value);
            break;
        case OnMouseMove:
            setListener(exec, mousemoveEvent, value);
            break;
        case OnMouseOut:
            setListener(exec, mouseoutEvent, value);
            break;
        case OnMouseOver:
            setListener(exec, mouseoverEvent, value);
            break;
        case OnMouseUp:
            setListener(exec, mouseupEvent, value);
            break;
        case OnMouseWheel:
            setListener(exec, mousewheelEvent, value);
            break;
        case OnBeforeCut:
            setListener(exec, beforecutEvent, value);
            break;
        case OnCut:
            setListener(exec, cutEvent, value);
            break;
        case OnBeforeCopy:
            setListener(exec, beforecopyEvent, value);
            break;
        case OnCopy:
            setListener(exec, copyEvent, value);
            break;
        case OnBeforePaste:
            setListener(exec, beforepasteEvent, value);
            break;
        case OnPaste:
            setListener(exec, pasteEvent, value);
            break;
        case OnDragEnter:
            setListener(exec, dragenterEvent, value);
            break;
        case OnDragOver:
            setListener(exec, dragoverEvent, value);
            break;
        case OnDragLeave:
            setListener(exec, dragleaveEvent, value);
            break;
        case OnDrop:
            setListener(exec, dropEvent, value);
            break;
        case OnDragStart:
            setListener(exec, dragstartEvent, value);
            break;
        case OnDrag:
            setListener(exec, dragEvent, value);
            break;
        case OnDragEnd:
            setListener(exec, dragendEvent, value);
            break;
        case OnReset:
            setListener(exec, resetEvent, value);
            break;
        case OnResize:
            setListener(exec, resizeEvent, value);
            break;
        case OnScroll:
            setListener(exec, scrollEvent, value);
            break;
        case OnSearch:
            setListener(exec, searchEvent, value);
            break;
        case OnSelect:
            setListener(exec, selectEvent, value);
            break;
        case OnSelectStart:
            setListener(exec, selectstartEvent, value);
            break;
        case OnSubmit:
            setListener(exec, submitEvent, value);
            break;
        case OnUnload:
            setListener(exec, unloadEvent, value);
            break;
    }
}

void DOMEventTargetNode::setListener(ExecState* exec, const AtomicString &eventType, JSValue* func) const
{
    EventTargetNodeCast(impl())->setHTMLEventListener(eventType, Window::retrieveActive(exec)->findOrCreateJSEventListener(func, true));
}

JSValue* DOMEventTargetNode::getListener(const AtomicString &eventType) const
{
    EventListener *listener = EventTargetNodeCast(impl())->getHTMLEventListener(eventType);
    JSEventListener *jsListener = static_cast<JSEventListener*>(listener);
    if (jsListener && jsListener->listenerObj())
        return jsListener->listenerObj();
    else
        return jsNull();
}

void DOMEventTargetNode::pushEventHandlerScope(ExecState*, ScopeChain &) const
{
}

/*
@begin DOMEventTargetNodePrototypeTable 5
# from the EventTarget interface
addEventListener        DOMEventTargetNode::AddEventListener   DontDelete|Function 3
removeEventListener     DOMEventTargetNode::RemoveEventListener    DontDelete|Function 3
dispatchEvent           DOMEventTargetNode::DispatchEvent  DontDelete|Function 1
@end
*/

KJS_IMPLEMENT_PROTOTYPE_FUNCTION(DOMEventTargetNodePrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("DOMEventTargetNode", DOMEventTargetNodePrototype, DOMEventTargetNodePrototypeFunction)

JSValue* DOMEventTargetNodePrototypeFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
    if (!thisObj->inherits(&DOMEventTargetNode::info))
        return throwError(exec, TypeError);
    DOMExceptionTranslator exception(exec);
    DOMEventTargetNode* DOMNode = static_cast<DOMEventTargetNode*>(thisObj);
    EventTargetNode* node = static_cast<EventTargetNode*>(DOMNode->impl());
    switch (id) {
        case DOMEventTargetNode::AddEventListener: {
            JSEventListener *listener = Window::retrieveActive(exec)->findOrCreateJSEventListener(args[1]);
            if (listener)
                node->addEventListener(args[0]->toString(exec), listener,args[2]->toBoolean(exec));
            return jsUndefined();
        }
        case DOMEventTargetNode::RemoveEventListener: {
            JSEventListener *listener = Window::retrieveActive(exec)->findJSEventListener(args[1]);
            if (listener) 
                node->removeEventListener(args[0]->toString(exec), listener,args[2]->toBoolean(exec));
            return jsUndefined();
        }
        case DOMEventTargetNode::DispatchEvent:
            return jsBoolean(node->dispatchEvent(toEvent(args[0]), exception));
    }
    
    return jsUndefined();
}

// -------------------------------------------------------------------------

/*
@begin DOMNodeListTable 2
  length        DOMNodeList::Length     DontDelete|ReadOnly
  item          DOMNodeList::Item               DontDelete|Function 1
@end
*/

KJS_IMPLEMENT_PROTOTYPE_FUNCTION(DOMNodeListFunc)

const ClassInfo DOMNodeList::info = { "NodeList", 0, &DOMNodeListTable, 0 };

DOMNodeList::DOMNodeList(ExecState* exec, NodeList *l) 
: m_impl(l) 
{ 
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
}

DOMNodeList::~DOMNodeList()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* DOMNodeList::toPrimitive(ExecState* exec, JSType) const
{
  if (!m_impl)
    return jsNull();

  return jsString(toString(exec));
}

JSValue* DOMNodeList::getValueProperty(ExecState* exec, int token) const
{
  assert(token == Length);
  return jsNumber(m_impl->length());
}

JSValue* DOMNodeList::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNodeList *thisObj = static_cast<DOMNodeList*>(slot.slotBase());
  return toJS(exec, thisObj->m_impl->item(slot.index()));
}

JSValue* DOMNodeList::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNodeList *thisObj = static_cast<DOMNodeList*>(slot.slotBase());
  return toJS(exec, thisObj->m_impl->itemWithName(propertyName));
}

bool DOMNodeList::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  const HashEntry* entry = Lookup::findEntry(&DOMNodeListTable, propertyName);

  if (entry) {
    if (entry->attr & Function)
      slot.setStaticEntry(this, entry, staticFunctionGetter<DOMNodeListFunc>);
    else
      slot.setStaticEntry(this, entry, staticValueGetter<DOMNodeList>);
    return true;
  }

  // array index ?
  bool ok;
  unsigned idx = propertyName.toUInt32(&ok);
  if (ok && idx < m_impl->length()) {
    slot.setCustomIndex(this, idx, indexGetter);
    return true;
  } else if (m_impl->itemWithName(propertyName)) {
    slot.setCustom(this, nameGetter);
    return true;
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

// Need to support both get and call, so that list[0] and list(0) work.
JSValue* DOMNodeList::callAsFunction(ExecState* exec, JSObject*, const List &args)
{
    // Do not use thisObj here. See JSHTMLCollection.
    UString s = args[0]->toString(exec);
    bool ok;
    unsigned int u = s.toUInt32(&ok);
    if (ok)
        return toJS(exec, m_impl->item(u));

    return jsUndefined();
}

// Not a prototype class currently, but should probably be converted to one
JSValue* DOMNodeListFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMNodeList::info))
    return throwError(exec, TypeError);
  NodeList &list = *static_cast<DOMNodeList*>(thisObj)->impl();

  if (id == DOMNodeList::Item)
    return toJS(exec, list.item(args[0]->toInt32(exec)));

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

Element* toElement(JSValue* val)
{
    if (!val || !val->isObject(&JSElement::info))
        return 0;
    return static_cast<Element*>(static_cast<JSElement*>(val)->impl());
}

DocumentType* toDocumentType(JSValue* val)
{
    if (!val || !val->isObject(&JSDocumentType::info))
        return 0;
    return static_cast<DocumentType*>(static_cast<DOMNode*>(val)->impl());
}

// -------------------------------------------------------------------------

/* Source for DOMNamedNodeMapPrototypeTable. Use "make hashtables" to regenerate.
@begin DOMNamedNodeMapPrototypeTable 10
  getNamedItem          DOMNamedNodeMap::GetNamedItem           DontDelete|Function 1
  setNamedItem          DOMNamedNodeMap::SetNamedItem           DontDelete|Function 1
  removeNamedItem       DOMNamedNodeMap::RemoveNamedItem        DontDelete|Function 1
  item                  DOMNamedNodeMap::Item                   DontDelete|Function 1
# DOM2
  getNamedItemNS        DOMNamedNodeMap::GetNamedItemNS         DontDelete|Function 2
  setNamedItemNS        DOMNamedNodeMap::SetNamedItemNS         DontDelete|Function 1
  removeNamedItemNS     DOMNamedNodeMap::RemoveNamedItemNS      DontDelete|Function 2
@end
*/
KJS_DEFINE_PROTOTYPE(DOMNamedNodeMapPrototype)
KJS_IMPLEMENT_PROTOTYPE_FUNCTION(DOMNamedNodeMapPrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("NamedNodeMap", DOMNamedNodeMapPrototype, DOMNamedNodeMapPrototypeFunction)

const ClassInfo DOMNamedNodeMap::info = { "NamedNodeMap", 0, 0, 0 };

DOMNamedNodeMap::DOMNamedNodeMap(ExecState* exec, NamedNodeMap* m)
    : m_impl(m) 
{ 
    setPrototype(DOMNamedNodeMapPrototype::self(exec));
}

DOMNamedNodeMap::~DOMNamedNodeMap()
{
    ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* DOMNamedNodeMap::lengthGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNamedNodeMap* thisObj = static_cast<DOMNamedNodeMap*>(slot.slotBase());
  return jsNumber(thisObj->m_impl->length());
}

JSValue* DOMNamedNodeMap::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNamedNodeMap* thisObj = static_cast<DOMNamedNodeMap*>(slot.slotBase());
  return toJS(exec, thisObj->m_impl->item(slot.index()));
}

JSValue* DOMNamedNodeMap::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNamedNodeMap* thisObj = static_cast<DOMNamedNodeMap*>(slot.slotBase());
  return toJS(exec, thisObj->m_impl->getNamedItem(propertyName));
}

bool DOMNamedNodeMap::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (propertyName == lengthPropertyName) {
      slot.setCustom(this, lengthGetter);
      return true;
  } else {
    // Look in the prototype (for functions) before assuming it's an item's name
    JSValue* proto = prototype();
    if (proto->isObject() && static_cast<JSObject*>(proto)->hasProperty(exec, propertyName))
      return false;

    // name or index ?
    bool ok;
    unsigned idx = propertyName.toUInt32(&ok);
    if (ok && idx < m_impl->length()) {
      slot.setCustomIndex(this, idx, indexGetter);
      return true;
    }

    if (m_impl->getNamedItem(propertyName)) {
      slot.setCustom(this, nameGetter);
      return true;
    }
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

JSValue* DOMNamedNodeMapPrototypeFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMNamedNodeMap::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  NamedNodeMap &map = *static_cast<DOMNamedNodeMap*>(thisObj)->impl();
  switch (id) {
    case DOMNamedNodeMap::GetNamedItem:
      return toJS(exec, map.getNamedItem(args[0]->toString(exec)));
    case DOMNamedNodeMap::SetNamedItem:
      return toJS(exec, map.setNamedItem(toNode(args[0]), exception).get());
    case DOMNamedNodeMap::RemoveNamedItem:
      return toJS(exec, map.removeNamedItem(args[0]->toString(exec), exception).get());
    case DOMNamedNodeMap::Item:
      return toJS(exec, map.item(args[0]->toInt32(exec)));
    case DOMNamedNodeMap::GetNamedItemNS: // DOM2
      return toJS(exec, map.getNamedItemNS(valueToStringWithNullCheck(exec, args[0]), args[1]->toString(exec)));
    case DOMNamedNodeMap::SetNamedItemNS: // DOM2
      return toJS(exec, map.setNamedItemNS(toNode(args[0]), exception).get());
    case DOMNamedNodeMap::RemoveNamedItemNS: // DOM2
      return toJS(exec, map.removeNamedItemNS(valueToStringWithNullCheck(exec, args[0]), args[1]->toString(exec), exception).get());
  }
  return jsUndefined();
}

// -------------------------------------------------------------------------

JSValue* toJS(ExecState* exec, Document *n)
{
  if (!n)
    return jsNull();

  JSDocument* ret = 0;
  ScriptInterpreter* interp = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter());

  if ((ret = static_cast<JSDocument*>(interp->getDOMObject(n))))
    return ret;

  if (n->isHTMLDocument())
    ret = new WebCore::JSHTMLDocument(exec, static_cast<HTMLDocument*>(n));
#if ENABLE(SVG)
  else if (n->isSVGDocument())
    ret = new WebCore::JSSVGDocument(exec, static_cast<SVGDocument*>(n));
#endif
  else
    ret = new JSDocument(exec, n);

  // Make sure the document is kept around by the window object, and works right with the
  // back/forward cache.
  if (n->frame())
    Window::retrieveWindow(n->frame())->putDirect("document", ret, DontDelete|ReadOnly);

  interp->putDOMObject(n, ret);

  return ret;
}

bool checkNodeSecurity(ExecState* exec, Node* n)
{
  if (!n) 
    return false;

  // Check to see if the currently executing interpreter is allowed to access the specified node
  Window* win = Window::retrieveWindow(n->document()->frame());
  return win && win->isSafeScript(exec);
}

JSValue* toJS(ExecState* exec, PassRefPtr<Node> node)
{
  Node* n = node.get();
  DOMNode* ret = 0;
  if (!n)
    return jsNull();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter());
  Document* doc = n->document();

  if ((ret = interp->getDOMNodeForDocument(doc, n)))
    return ret;

  switch (n->nodeType()) {
    case Node::ELEMENT_NODE:
      if (n->isHTMLElement())
        ret = createJSHTMLWrapper(exec, static_pointer_cast<HTMLElement>(node));
#if ENABLE(SVG)
      else if (n->isSVGElement())
        ret = createJSSVGWrapper(exec, static_pointer_cast<SVGElement>(node));
#endif
      else
        ret = new JSElement(exec, static_cast<Element*>(n));
      break;
    case Node::ATTRIBUTE_NODE:
      ret = new JSAttr(exec, static_cast<Attr*>(n));
      break;
    case Node::TEXT_NODE:
    case Node::CDATA_SECTION_NODE:
      ret = new JSText(exec, static_cast<Text*>(n));
      break;
    case Node::ENTITY_NODE:
      ret = new JSEntity(exec, static_cast<Entity*>(n));
      break;
    case Node::PROCESSING_INSTRUCTION_NODE:
      ret = new JSProcessingInstruction(exec, static_cast<ProcessingInstruction*>(n));
      break;
    case Node::COMMENT_NODE:
      ret = new JSCharacterData(exec, static_cast<CharacterData*>(n));
      break;
    case Node::DOCUMENT_NODE:
      // we don't want to cache the document itself in the per-document dictionary
      return toJS(exec, static_cast<Document*>(n));
    case Node::DOCUMENT_TYPE_NODE:
      ret = new JSDocumentType(exec, static_cast<DocumentType*>(n));
      break;
    case Node::NOTATION_NODE:
      ret = new JSNotation(exec, static_cast<Notation*>(n));
      break;
    case Node::DOCUMENT_FRAGMENT_NODE:
      ret = new JSDocumentFragment(exec, static_cast<DocumentFragment*>(n));
      break;
    case Node::ENTITY_REFERENCE_NODE:
    default:
      ret = new JSNode(exec, n);
  }

  interp->putDOMNodeForDocument(doc, n, ret);

  return ret;
}

JSValue* toJS(ExecState* exec, NamedNodeMap* m)
{
    return cacheDOMObject<NamedNodeMap, DOMNamedNodeMap>(exec, m);
}

JSValue* toJS(ExecState* exec, EventTarget* target)
{
    if (!target)
        return jsNull();
    
#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
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

JSValue* toJS(ExecState* exec, PassRefPtr<NodeList> l)
{
    return cacheDOMObject<NodeList, DOMNodeList>(exec, l.get());
}

// -------------------------------------------------------------------------

const ClassInfo DOMExceptionConstructor::info = { "DOMExceptionConstructor", 0, 0, 0 };

/* Source for DOMExceptionConstructorTable. Use "make hashtables" to regenerate.
@begin DOMExceptionConstructorTable 15
  INDEX_SIZE_ERR                WebCore::INDEX_SIZE_ERR               DontDelete|ReadOnly
  DOMSTRING_SIZE_ERR            WebCore::DOMSTRING_SIZE_ERR   DontDelete|ReadOnly
  HIERARCHY_REQUEST_ERR         WebCore::HIERARCHY_REQUEST_ERR        DontDelete|ReadOnly
  WRONG_DOCUMENT_ERR            WebCore::WRONG_DOCUMENT_ERR   DontDelete|ReadOnly
  INVALID_CHARACTER_ERR         WebCore::INVALID_CHARACTER_ERR        DontDelete|ReadOnly
  NO_DATA_ALLOWED_ERR           WebCore::NO_DATA_ALLOWED_ERR  DontDelete|ReadOnly
  NO_MODIFICATION_ALLOWED_ERR   WebCore::NO_MODIFICATION_ALLOWED_ERR  DontDelete|ReadOnly
  NOT_FOUND_ERR                 WebCore::NOT_FOUND_ERR                DontDelete|ReadOnly
  NOT_SUPPORTED_ERR             WebCore::NOT_SUPPORTED_ERR    DontDelete|ReadOnly
  INUSE_ATTRIBUTE_ERR           WebCore::INUSE_ATTRIBUTE_ERR  DontDelete|ReadOnly
  INVALID_STATE_ERR             WebCore::INVALID_STATE_ERR    DontDelete|ReadOnly
  SYNTAX_ERR                    WebCore::SYNTAX_ERR           DontDelete|ReadOnly
  INVALID_MODIFICATION_ERR      WebCore::INVALID_MODIFICATION_ERR     DontDelete|ReadOnly
  NAMESPACE_ERR                 WebCore::NAMESPACE_ERR                DontDelete|ReadOnly
  INVALID_ACCESS_ERR            WebCore::INVALID_ACCESS_ERR   DontDelete|ReadOnly
@end
*/

DOMExceptionConstructor::DOMExceptionConstructor(ExecState* exec) 
{ 
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
}

bool DOMExceptionConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMExceptionConstructor, DOMObject>(exec, &DOMExceptionConstructorTable, this, propertyName, slot);
}

JSValue* DOMExceptionConstructor::getValueProperty(ExecState*, int token) const
{
  // We use the token as the value to return directly
  return jsNumber(token);
}

JSObject* getDOMExceptionConstructor(ExecState* exec)
{
  return cacheGlobalObject<DOMExceptionConstructor>(exec, "[[DOMException.constructor]]");
}

// -------------------------------------------------------------------------

const ClassInfo DOMNamedNodesCollection::info = { "Collection", 0, 0, 0 };

// Such a collection is usually very short-lived, it only exists
// for constructs like document.forms.<name>[1],
// so it shouldn't be a problem that it's storing all the nodes (with the same name). (David)
DOMNamedNodesCollection::DOMNamedNodesCollection(ExecState* exec, const Vector<RefPtr<Node> >& nodes)
  : m_nodes(nodes)
{
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
}

JSValue* DOMNamedNodesCollection::lengthGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNamedNodesCollection *thisObj = static_cast<DOMNamedNodesCollection*>(slot.slotBase());
  return jsNumber(thisObj->m_nodes.size());
}

JSValue* DOMNamedNodesCollection::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNamedNodesCollection *thisObj = static_cast<DOMNamedNodesCollection*>(slot.slotBase());
  return toJS(exec, thisObj->m_nodes[slot.index()].get());
}

bool DOMNamedNodesCollection::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (propertyName == lengthPropertyName) {
    slot.setCustom(this, lengthGetter);
    return true;
  }

  // array index ?
  bool ok;
  unsigned idx = propertyName.toUInt32(&ok);
  if (ok && idx < m_nodes.size()) {
    slot.setCustomIndex(this, idx, indexGetter);
    return true;
  }

  // For IE compatibility, we need to be able to look up elements in a
  // document.formName.name result by id as well as be index.

  AtomicString atomicPropertyName = propertyName;
  for (unsigned i = 0; i < m_nodes.size(); i++) {
    Node* node = m_nodes[i].get();
    if (node->hasAttributes() && node->attributes()->id() == atomicPropertyName) {
      slot.setCustomIndex(this, i, indexGetter);
      return true;
    }
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

} // namespace
