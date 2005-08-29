// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004 Apple Computer, Inc.
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

#include "kjs_dom.h"

#include <kdebug.h>
#include <khtml_part.h>
#include <khtmlview.h>
#include "xml/dom2_eventsimpl.h"
#include "xml/dom2_viewsimpl.h"
#include "rendering/render_canvas.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_textimpl.h"
#include "xml/dom_xmlimpl.h"
#include "html/html_documentimpl.h"
#include "css/css_ruleimpl.h"
#include "css/css_stylesheetimpl.h"

#include "kjs_html.h"
#include "kjs_css.h"
#include "kjs_range.h"
#include "kjs_traversal.h"
#include "kjs_events.h"
#include "kjs_views.h"
#include "kjs_window.h"
#include "dom/dom_exception.h"
#include "khtmlpart_p.h"

#include "html_objectimpl.h"

#if APPLE_CHANGES
#include <JavaScriptCore/runtime_object.h>
#endif

using namespace DOM::HTMLNames;

using DOM::AttrImpl;
using DOM::CharacterDataImpl;
using DOM::DocumentImpl;
using DOM::DocumentTypeImpl;
using DOM::DOMException;
using DOM::DOMImplementationImpl;
using DOM::DOMString;
using DOM::ElementImpl;
using DOM::EntityImpl;
using DOM::EventImpl;
using DOM::HTMLAppletElementImpl;
using DOM::HTMLDocumentImpl;
using DOM::HTMLElementImpl;
using DOM::HTMLEmbedElementImpl;
using DOM::HTMLObjectElementImpl;
using DOM::NamedNodeMapImpl;
using DOM::Node;
using DOM::NodeFilterImpl;
using DOM::NodeImpl;
using DOM::NodeListImpl;
using DOM::NotationImpl;
using DOM::ProcessingInstructionImpl;
using DOM::TextImpl;

using khtml::RenderObject;
using khtml::SharedPtr;

#include "kjs_dom.lut.h"

namespace KJS {

class DOMNodeListFunc : public DOMFunction {
    friend class DOMNodeList;
public:
    DOMNodeListFunc(ExecState *exec, int id, int len);
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &);
private:
    int id;
};

// -------------------------------------------------------------------------
/* Source for DOMNodeProtoTable. Use "make hashtables" to regenerate.
@begin DOMNodeProtoTable 13
  insertBefore	DOMNode::InsertBefore	DontDelete|Function 2
  replaceChild	DOMNode::ReplaceChild	DontDelete|Function 2
  removeChild	DOMNode::RemoveChild	DontDelete|Function 1
  appendChild	DOMNode::AppendChild	DontDelete|Function 1
  hasAttributes	DOMNode::HasAttributes	DontDelete|Function 0
  hasChildNodes	DOMNode::HasChildNodes	DontDelete|Function 0
  cloneNode	DOMNode::CloneNode	DontDelete|Function 1
# DOM2
  normalize	DOMNode::Normalize	DontDelete|Function 0
  isSupported   DOMNode::IsSupported	DontDelete|Function 2
# from the EventTarget interface
  addEventListener	DOMNode::AddEventListener	DontDelete|Function 3
  removeEventListener	DOMNode::RemoveEventListener	DontDelete|Function 3
  dispatchEvent		DOMNode::DispatchEvent	DontDelete|Function 1
  contains	DOMNode::Contains		DontDelete|Function 1
# "DOM level 0" (from Gecko DOM reference; also in WinIE)
  item          DOMNode::Item           DontDelete|Function 1
@end
*/
DEFINE_PROTOTYPE("DOMNode",DOMNodeProto)
IMPLEMENT_PROTOFUNC(DOMNodeProtoFunc)
IMPLEMENT_PROTOTYPE(DOMNodeProto,DOMNodeProtoFunc)

const ClassInfo DOMNode::info = { "Node", 0, &DOMNodeTable, 0 };

DOMNode::DOMNode(ExecState *exec, NodeImpl *n)
  : m_impl(n)
{
  setPrototype(DOMNodeProto::self(exec));
}

DOMNode::DOMNode(NodeImpl *n)
  : m_impl(n)
{
}

DOMNode::~DOMNode()
{
  ScriptInterpreter::forgetDOMNodeForDocument(m_impl->getDocument(), m_impl.get());
}

void DOMNode::mark()
{
  assert(!marked());

  NodeImpl *node = m_impl.get();

  // Nodes in the document are kept alive by ScriptInterpreter::mark,
  // so we have no special responsibilities and can just call the base class here.
  if (node->inDocument()) {
    DOMObject::mark();
    return;
  }

  // This is a node outside the document, so find the root of the tree it is in,
  // and start marking from there.
  NodeImpl *root = node;
  for (NodeImpl *current = m_impl.get(); current; current = current->parentNode()) {
    root = current;
  }

  static QPtrDict<NodeImpl> markingRoots;

  // If we're already marking this tree, then we can simply mark this wrapper
  // by calling the base class; our caller is iterating the tree.
  if (markingRoots.find(root)) {
    DOMObject::mark();
    return;
  }

  DocumentImpl *document = m_impl->getDocument();

  // Mark the whole tree; use the global set of roots to avoid reentering.
  markingRoots.insert(root, root);
  for (NodeImpl *nodeToMark = root; nodeToMark; nodeToMark = nodeToMark->traverseNextNode()) {
    DOMNode *wrapper = ScriptInterpreter::getDOMNodeForDocument(document, nodeToMark);
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

bool DOMNode::toBoolean(ExecState *) const
{
    return m_impl.notNull();
}

/* Source for DOMNodeTable. Use "make hashtables" to regenerate.
@begin DOMNodeTable 67
  nodeName	DOMNode::NodeName	DontDelete|ReadOnly
  nodeValue	DOMNode::NodeValue	DontDelete
  nodeType	DOMNode::NodeType	DontDelete|ReadOnly
  parentNode	DOMNode::ParentNode	DontDelete|ReadOnly
  parentElement	DOMNode::ParentElement	DontDelete|ReadOnly
  childNodes	DOMNode::ChildNodes	DontDelete|ReadOnly
  firstChild	DOMNode::FirstChild	DontDelete|ReadOnly
  lastChild	DOMNode::LastChild	DontDelete|ReadOnly
  previousSibling  DOMNode::PreviousSibling DontDelete|ReadOnly
  nextSibling	DOMNode::NextSibling	DontDelete|ReadOnly
  attributes	DOMNode::Attributes	DontDelete|ReadOnly
  namespaceURI	DOMNode::NamespaceURI	DontDelete|ReadOnly
# DOM2
  prefix	DOMNode::Prefix		DontDelete
  localName	DOMNode::LocalName	DontDelete|ReadOnly
  ownerDocument	DOMNode::OwnerDocument	DontDelete|ReadOnly
#
  onabort	DOMNode::OnAbort		DontDelete
  onblur	DOMNode::OnBlur			DontDelete
  onchange	DOMNode::OnChange		DontDelete
  onclick	DOMNode::OnClick		DontDelete
  oncontextmenu	DOMNode::OnContextMenu		DontDelete
  ondblclick	DOMNode::OnDblClick		DontDelete
  onbeforecut	DOMNode::OnBeforeCut		DontDelete
  oncut         DOMNode::OnCut                  DontDelete
  onbeforecopy	DOMNode::OnBeforeCopy		DontDelete
  oncopy	DOMNode::OnCopy                 DontDelete
  onbeforepaste	DOMNode::OnBeforePaste		DontDelete
  onpaste	DOMNode::OnPaste		DontDelete
  ondrag	DOMNode::OnDrag			DontDelete
  ondragdrop	DOMNode::OnDragDrop		DontDelete
  ondragend	DOMNode::OnDragEnd		DontDelete
  ondragenter	DOMNode::OnDragEnter		DontDelete
  ondragleave	DOMNode::OnDragLeave		DontDelete
  ondragover	DOMNode::OnDragOver		DontDelete
  ondragstart	DOMNode::OnDragStart		DontDelete
  ondrop	DOMNode::OnDrop                 DontDelete
  onerror	DOMNode::OnError		DontDelete
  onfocus	DOMNode::OnFocus       		DontDelete
  oninput       DOMNode::OnInput                DontDelete
  onkeydown	DOMNode::OnKeyDown		DontDelete
  onkeypress	DOMNode::OnKeyPress		DontDelete
  onkeyup	DOMNode::OnKeyUp		DontDelete
  onload	DOMNode::OnLoad			DontDelete
  onmousedown	DOMNode::OnMouseDown		DontDelete
  onmousemove	DOMNode::OnMouseMove		DontDelete
  onmouseout	DOMNode::OnMouseOut		DontDelete
  onmouseover	DOMNode::OnMouseOver		DontDelete
  onmouseup	DOMNode::OnMouseUp		DontDelete
  onmousewheel	DOMNode::OnMouseWheel		DontDelete
  onmove	DOMNode::OnMove			DontDelete
  onreset	DOMNode::OnReset		DontDelete
  onresize	DOMNode::OnResize		DontDelete
  onscroll      DOMNode::OnScroll               DontDelete
  onsearch      DOMNode::OnSearch               DontDelete
  onselect	DOMNode::OnSelect		DontDelete
  onselectstart	DOMNode::OnSelectStart		DontDelete
  onsubmit	DOMNode::OnSubmit		DontDelete
  onunload	DOMNode::OnUnload		DontDelete
# IE extensions
  offsetLeft	DOMNode::OffsetLeft		DontDelete|ReadOnly
  offsetTop	DOMNode::OffsetTop		DontDelete|ReadOnly
  offsetWidth	DOMNode::OffsetWidth		DontDelete|ReadOnly
  offsetHeight	DOMNode::OffsetHeight		DontDelete|ReadOnly
  offsetParent	DOMNode::OffsetParent		DontDelete|ReadOnly
  clientWidth	DOMNode::ClientWidth		DontDelete|ReadOnly
  clientHeight	DOMNode::ClientHeight		DontDelete|ReadOnly
  scrollLeft	DOMNode::ScrollLeft		DontDelete
  scrollTop	DOMNode::ScrollTop		DontDelete
  scrollWidth   DOMNode::ScrollWidth            DontDelete|ReadOnly
  scrollHeight  DOMNode::ScrollHeight           DontDelete|ReadOnly
@end
*/
bool DOMNode::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMNode, DOMObject>(exec, &DOMNodeTable, this, propertyName, slot);
}

ValueImp *DOMNode::getValueProperty(ExecState *exec, int token) const
{
  NodeImpl &node = *m_impl;
  switch (token) {
  case NodeName:
    return getStringOrNull(node.nodeName());
  case NodeValue:
    return getStringOrNull(node.nodeValue());
  case NodeType:
    return Number(node.nodeType());
  case ParentNode:
  case ParentElement: // IE only apparently
    return getDOMNode(exec,node.parentNode());
  case ChildNodes:
    return getDOMNodeList(exec,node.childNodes().get());
  case FirstChild:
    return getDOMNode(exec,node.firstChild());
  case LastChild:
    return getDOMNode(exec,node.lastChild());
  case PreviousSibling:
    return getDOMNode(exec,node.previousSibling());
  case NextSibling:
    return getDOMNode(exec,node.nextSibling());
  case Attributes:
    return getDOMNamedNodeMap(exec,node.attributes());
  case NamespaceURI:
    return getStringOrNull(node.namespaceURI());
  case Prefix:
    return getStringOrNull(node.prefix());
  case LocalName:
    return getStringOrNull(node.localName());
  case OwnerDocument:
    return getDOMNode(exec,node.ownerDocument());
  case OnAbort:
    return getListener(DOM::EventImpl::ABORT_EVENT);
  case OnBlur:
    return getListener(DOM::EventImpl::BLUR_EVENT);
  case OnChange:
    return getListener(DOM::EventImpl::CHANGE_EVENT);
  case OnClick:
    return getListener(DOM::EventImpl::KHTML_CLICK_EVENT);
  case OnContextMenu:
    return getListener(DOM::EventImpl::CONTEXTMENU_EVENT);
  case OnDblClick:
    return getListener(DOM::EventImpl::KHTML_DBLCLICK_EVENT);
  case OnDragDrop:
    return getListener(DOM::EventImpl::KHTML_DRAGDROP_EVENT);
  case OnError:
    return getListener(DOM::EventImpl::KHTML_ERROR_EVENT);
  case OnFocus:
    return getListener(DOM::EventImpl::FOCUS_EVENT);
  case OnInput:
    return getListener(DOM::EventImpl::INPUT_EVENT);
  case OnKeyDown:
    return getListener(DOM::EventImpl::KEYDOWN_EVENT);
  case OnKeyPress:
    return getListener(DOM::EventImpl::KEYPRESS_EVENT);
  case OnKeyUp:
    return getListener(DOM::EventImpl::KEYUP_EVENT);
  case OnLoad:
    return getListener(DOM::EventImpl::LOAD_EVENT);
  case OnMouseDown:
    return getListener(DOM::EventImpl::MOUSEDOWN_EVENT);
  case OnMouseMove:
    return getListener(DOM::EventImpl::MOUSEMOVE_EVENT);
  case OnMouseOut:
    return getListener(DOM::EventImpl::MOUSEOUT_EVENT);
  case OnMouseOver:
    return getListener(DOM::EventImpl::MOUSEOVER_EVENT);
  case OnMouseUp:
    return getListener(DOM::EventImpl::MOUSEUP_EVENT);      
  case OnMouseWheel:
    return getListener(DOM::EventImpl::MOUSEWHEEL_EVENT);      
  case OnBeforeCut:
    return getListener(DOM::EventImpl::BEFORECUT_EVENT);
  case OnCut:
    return getListener(DOM::EventImpl::CUT_EVENT);
  case OnBeforeCopy:
    return getListener(DOM::EventImpl::BEFORECOPY_EVENT);
  case OnCopy:
    return getListener(DOM::EventImpl::COPY_EVENT);
  case OnBeforePaste:
    return getListener(DOM::EventImpl::BEFOREPASTE_EVENT);
  case OnPaste:
    return getListener(DOM::EventImpl::PASTE_EVENT);
  case OnDragEnter:
    return getListener(DOM::EventImpl::DRAGENTER_EVENT);
  case OnDragOver:
    return getListener(DOM::EventImpl::DRAGOVER_EVENT);
  case OnDragLeave:
    return getListener(DOM::EventImpl::DRAGLEAVE_EVENT);
  case OnDrop:
    return getListener(DOM::EventImpl::DROP_EVENT);
  case OnDragStart:
    return getListener(DOM::EventImpl::DRAGSTART_EVENT);
  case OnDrag:
    return getListener(DOM::EventImpl::DRAG_EVENT);
  case OnDragEnd:
    return getListener(DOM::EventImpl::DRAGEND_EVENT);
  case OnMove:
    return getListener(DOM::EventImpl::KHTML_MOVE_EVENT);
  case OnReset:
    return getListener(DOM::EventImpl::RESET_EVENT);
  case OnResize:
    return getListener(DOM::EventImpl::RESIZE_EVENT);
  case OnScroll:
    return getListener(DOM::EventImpl::SCROLL_EVENT);
#if APPLE_CHANGES
  case OnSearch:
    return getListener(DOM::EventImpl::SEARCH_EVENT);
#endif
  case OnSelect:
    return getListener(DOM::EventImpl::SELECT_EVENT);
  case OnSelectStart:
    return getListener(DOM::EventImpl::SELECTSTART_EVENT);
  case OnSubmit:
    return getListener(DOM::EventImpl::SUBMIT_EVENT);
  case OnUnload:
    return getListener(DOM::EventImpl::UNLOAD_EVENT);
  default:
    // no DOM standard, found in IE only

    // Make sure our layout is up to date before we allow a query on these attributes.
    DOM::DocumentImpl* docimpl = node.getDocument();
    if (docimpl) {
      docimpl->updateLayoutIgnorePendingStylesheets();
    }

    khtml::RenderObject *rend = node.renderer();

    switch (token) {
    case OffsetLeft:
      return rend ? jsNumber(rend->offsetLeft()) : static_cast<ValueImp *>(jsUndefined());
    case OffsetTop:
      return rend ? jsNumber(rend->offsetTop()) : static_cast<ValueImp *>(jsUndefined());
    case OffsetWidth:
      return rend ? jsNumber(rend->offsetWidth()) : static_cast<ValueImp *>(jsUndefined());
    case OffsetHeight:
      return rend ? jsNumber(rend->offsetHeight()) : static_cast<ValueImp *>(jsUndefined());
    case OffsetParent: {
      khtml::RenderObject* par = rend ? rend->offsetParent() : 0;
      return getDOMNode(exec, par ? par->element() : 0);
    }
    case ClientWidth:
      return rend ? jsNumber(rend->clientWidth()) : static_cast<ValueImp *>(jsUndefined());
    case ClientHeight:
      return rend ? jsNumber(rend->clientHeight()) : static_cast<ValueImp *>(jsUndefined());
    case ScrollWidth:
        return rend ? jsNumber(rend->scrollWidth()) : static_cast<ValueImp *>(jsUndefined());
    case ScrollHeight:
        return rend ? jsNumber(rend->scrollHeight()) : static_cast<ValueImp *>(jsUndefined());
    case ScrollLeft:
      return Number(rend && rend->layer() ? rend->layer()->scrollXOffset() : 0);
    case ScrollTop:
      return Number(rend && rend->layer() ? rend->layer()->scrollYOffset() : 0);
    default:
      kdWarning() << "Unhandled token in DOMNode::getValueProperty : " << token << endl;
      break;
    }
  }

  return NULL;
}

void DOMNode::put(ExecState *exec, const Identifier& propertyName, ValueImp *value, int attr)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMNode::put " << propertyName.qstring() << endl;
#endif
  lookupPut<DOMNode,DOMObject>(exec, propertyName, value, attr,
                                        &DOMNodeTable, this );
}

void DOMNode::putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/)
{
  DOMExceptionTranslator exception(exec);
  NodeImpl &node = *m_impl;
  switch (token) {
  case NodeValue:
    node.setNodeValue(value->toString(exec).domString(), exception);
    break;
  case Prefix:
    node.setPrefix(value->toString(exec).domString().implementation(), exception);
    break;
  case OnAbort:
    setListener(exec,DOM::EventImpl::ABORT_EVENT,value);
    break;
  case OnBlur:
    setListener(exec,DOM::EventImpl::BLUR_EVENT,value);
    break;
  case OnChange:
    setListener(exec,DOM::EventImpl::CHANGE_EVENT,value);
    break;
  case OnClick:
    setListener(exec,DOM::EventImpl::KHTML_CLICK_EVENT,value);
    break;
  case OnContextMenu:
    setListener(exec,DOM::EventImpl::CONTEXTMENU_EVENT,value);
    break;
  case OnDblClick:
    setListener(exec,DOM::EventImpl::KHTML_DBLCLICK_EVENT,value);
    break;
  case OnDragDrop:
    setListener(exec,DOM::EventImpl::KHTML_DRAGDROP_EVENT,value);
    break;
  case OnError:
    setListener(exec,DOM::EventImpl::KHTML_ERROR_EVENT,value);
    break;
  case OnFocus:
    setListener(exec,DOM::EventImpl::FOCUS_EVENT,value);
    break;
  case OnInput:
    setListener(exec,DOM::EventImpl::INPUT_EVENT,value);
    break;
  case OnKeyDown:
    setListener(exec,DOM::EventImpl::KEYDOWN_EVENT,value);
    break;
  case OnKeyPress:
    setListener(exec,DOM::EventImpl::KEYPRESS_EVENT,value);
    break;
  case OnKeyUp:
    setListener(exec,DOM::EventImpl::KEYUP_EVENT,value);
    break;
  case OnLoad:
    setListener(exec,DOM::EventImpl::LOAD_EVENT,value);
    break;
  case OnMouseDown:
    setListener(exec,DOM::EventImpl::MOUSEDOWN_EVENT,value);
    break;
  case OnMouseMove:
    setListener(exec,DOM::EventImpl::MOUSEMOVE_EVENT,value);
    break;
  case OnMouseOut:
    setListener(exec,DOM::EventImpl::MOUSEOUT_EVENT,value);
    break;
  case OnMouseOver:
    setListener(exec,DOM::EventImpl::MOUSEOVER_EVENT,value);
    break;
  case OnMouseUp:
    setListener(exec,DOM::EventImpl::MOUSEUP_EVENT,value);
    break;
  case OnMouseWheel:
    setListener(exec,DOM::EventImpl::MOUSEWHEEL_EVENT,value);
    break;
  case OnBeforeCut:
    setListener(exec,DOM::EventImpl::BEFORECUT_EVENT,value);
    break;
  case OnCut:
    setListener(exec,DOM::EventImpl::CUT_EVENT,value);
    break;
  case OnBeforeCopy:
    setListener(exec,DOM::EventImpl::BEFORECOPY_EVENT,value);
    break;
  case OnCopy:
    setListener(exec,DOM::EventImpl::COPY_EVENT,value);
    break;
  case OnBeforePaste:
    setListener(exec,DOM::EventImpl::BEFOREPASTE_EVENT,value);
    break;
  case OnPaste:
    setListener(exec,DOM::EventImpl::PASTE_EVENT,value);
    break;
  case OnDragEnter:
    setListener(exec,DOM::EventImpl::DRAGENTER_EVENT,value);
    break;
  case OnDragOver:
    setListener(exec,DOM::EventImpl::DRAGOVER_EVENT,value);
    break;
  case OnDragLeave:
    setListener(exec,DOM::EventImpl::DRAGLEAVE_EVENT,value);
    break;
  case OnDrop:
    setListener(exec,DOM::EventImpl::DROP_EVENT,value);
    break;
  case OnDragStart:
    setListener(exec,DOM::EventImpl::DRAGSTART_EVENT,value);
    break;
  case OnDrag:
    setListener(exec,DOM::EventImpl::DRAG_EVENT,value);
    break;
  case OnDragEnd:
    setListener(exec,DOM::EventImpl::DRAGEND_EVENT,value);
    break;
  case OnMove:
    setListener(exec,DOM::EventImpl::KHTML_MOVE_EVENT,value);
    break;
  case OnReset:
    setListener(exec,DOM::EventImpl::RESET_EVENT,value);
    break;
  case OnResize:
    setListener(exec,DOM::EventImpl::RESIZE_EVENT,value);
    break;
  case OnScroll:
    setListener(exec,DOM::EventImpl::SCROLL_EVENT,value);
    break;
  case OnSearch:
    setListener(exec,DOM::EventImpl::SEARCH_EVENT,value);
    break;
  case OnSelect:
    setListener(exec,DOM::EventImpl::SELECT_EVENT,value);
    break;
  case OnSelectStart:
    setListener(exec,DOM::EventImpl::SELECTSTART_EVENT,value);
    break;
  case OnSubmit:
    setListener(exec,DOM::EventImpl::SUBMIT_EVENT,value);
    break;
  case OnUnload:
    setListener(exec,DOM::EventImpl::UNLOAD_EVENT,value);
    break;
  case ScrollTop: {
    khtml::RenderObject *rend = node.renderer();
    if (rend && rend->hasOverflowClip())
        rend->layer()->scrollToYOffset(value->toInt32(exec));
    break;
  }
  case ScrollLeft: {
    khtml::RenderObject *rend = node.renderer();
    if (rend && rend->hasOverflowClip())
      rend->layer()->scrollToXOffset(value->toInt32(exec));
    break;
  }
  default:
    kdWarning() << "DOMNode::putValueProperty unhandled token " << token << endl;
  }
}

ValueImp *DOMNode::toPrimitive(ExecState *exec, Type /*preferred*/) const
{
  if (m_impl.isNull())
    return Null();

  return String(toString(exec));
}

UString DOMNode::toString(ExecState *) const
{
  if (m_impl.isNull())
    return "null";
  return "[object " + (m_impl->isElementNode() ? m_impl->nodeName() : className()) + "]";
}

void DOMNode::setListener(ExecState *exec, int eventId, ValueImp *func) const
{
  m_impl->setHTMLEventListener(eventId, Window::retrieveActive(exec)->getJSEventListener(func, true));
}

ValueImp *DOMNode::getListener(int eventId) const
{
    DOM::EventListener *listener = m_impl->getHTMLEventListener(eventId);
    JSEventListener *jsListener = static_cast<JSEventListener*>(listener);
    if ( jsListener && jsListener->listenerObjImp() )
	return jsListener->listenerObj();
    else
	return Null();
}

void DOMNode::pushEventHandlerScope(ExecState *, ScopeChain &) const
{
}

ValueImp *DOMNodeProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMNode::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  NodeImpl &node = *static_cast<DOMNode *>(thisObj)->impl();
  switch (id) {
    case DOMNode::HasAttributes:
      return Boolean(node.hasAttributes());
    case DOMNode::HasChildNodes:
      return Boolean(node.hasChildNodes());
    case DOMNode::CloneNode:
      return getDOMNode(exec,node.cloneNode(args[0]->toBoolean(exec)));
    case DOMNode::Normalize:
      node.normalize();
      return Undefined();
    case DOMNode::IsSupported:
        return Boolean(node.isSupported(args[0]->toString(exec).domString(),
            args[1]->isUndefinedOrNull() ? DOMString() : args[1]->toString(exec).domString()));
    case DOMNode::AddEventListener: {
        JSEventListener *listener = Window::retrieveActive(exec)->getJSEventListener(args[1]);
        if (listener)
            node.addEventListener(args[0]->toString(exec).domString(),listener,args[2]->toBoolean(exec));
        return Undefined();
    }
    case DOMNode::RemoveEventListener: {
        JSEventListener *listener = Window::retrieveActive(exec)->getJSEventListener(args[1]);
        if (listener)
            node.removeEventListener(args[0]->toString(exec).domString(),listener,args[2]->toBoolean(exec));
        return Undefined();
    }
    case DOMNode::DispatchEvent:
      return Boolean(node.dispatchEvent(toEvent(args[0]), exception));
    case DOMNode::AppendChild:
      return getDOMNode(exec,node.appendChild(toNode(args[0]), exception));
    case DOMNode::RemoveChild:
      return getDOMNode(exec,node.removeChild(toNode(args[0]), exception));
    case DOMNode::InsertBefore:
      return getDOMNode(exec,node.insertBefore(toNode(args[0]), toNode(args[1]), exception));
    case DOMNode::ReplaceChild:
      return getDOMNode(exec,node.replaceChild(toNode(args[0]), toNode(args[1]), exception));
    case DOMNode::Contains:
      if (node.isElementNode())
        if (NodeImpl *node0 = toNode(args[0]))
          return Boolean(node.isAncestor(node0));
      // FIXME: Is there a good reason to return undefined rather than false
      // when the parameter is not a node? When the object is not an element?
      return Undefined();
    case DOMNode::Item:
      return getDOMNode(exec, node.childNodes()->item(args[0]->toInt32(exec)));
  }

  return Undefined();
}

NodeImpl *toNode(ValueImp *val)
{
    if (!val || !val->isObject(&DOMNode::info))
        return 0;
    return static_cast<DOMNode *>(val)->impl();
}

// -------------------------------------------------------------------------

/*
@begin DOMNodeListTable 2
  length	DOMNodeList::Length	DontDelete|ReadOnly
  item		DOMNodeList::Item		DontDelete|Function 1
@end
*/

const ClassInfo DOMNodeList::info = { "NodeList", 0, &DOMNodeListTable, 0 };

DOMNodeList::~DOMNodeList()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

ValueImp *DOMNodeList::toPrimitive(ExecState *exec, Type /*preferred*/) const
{
  if (m_impl.isNull())
    return Null();

  return String(toString(exec));
}

ValueImp *DOMNodeList::getValueProperty(ExecState *exec, int token) const
{
  assert(token == Length);
  return Number(m_impl->length());
}

ValueImp *DOMNodeList::indexGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNodeList *thisObj = static_cast<DOMNodeList *>(slot.slotBase());
  return getDOMNode(exec, thisObj->m_impl->item(slot.index()));
}

ValueImp *DOMNodeList::nameGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNodeList *thisObj = static_cast<DOMNodeList *>(slot.slotBase());
  return getDOMNode(exec, thisObj->m_impl->itemById(propertyName.domString()));
}

bool DOMNodeList::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  const HashEntry* entry = Lookup::findEntry(&DOMNodeListTable, propertyName);

  if (entry) {
    if (entry->attr & Function)
      slot.setStaticEntry(this, entry, staticFunctionGetter<DOMNodeListFunc>);
    else
      slot.setStaticEntry(this, entry, staticValueGetter<DOMNodeList>);
    return true;
  }

  NodeListImpl &list = *m_impl;

  // array index ?
  bool ok;
  long unsigned int idx = propertyName.toULong(&ok);
  if (ok && idx < list.length()) {
    slot.setCustomIndex(this, idx, indexGetter);
    return true;
  } else {
    if (list.itemById(propertyName.domString())) {
      slot.setCustom(this, nameGetter);
      return true;
    }
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

// Need to support both get and call, so that list[0] and list(0) work.
ValueImp *DOMNodeList::callAsFunction(ExecState *exec, ObjectImp *, const List &args)
{
  // Do not use thisObj here. See HTMLCollection.
  UString s = args[0]->toString(exec);
  bool ok;
  unsigned int u = s.toULong(&ok);
  if (ok)
    return getDOMNode(exec, m_impl->item(u));

  return Undefined();
}

DOMNodeListFunc::DOMNodeListFunc(ExecState *exec, int i, int len)
  : id(i)
{
  put(exec,lengthPropertyName,Number(len),DontDelete|ReadOnly|DontEnum);
}

// Not a prototype class currently, but should probably be converted to one
ValueImp *DOMNodeListFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMNodeList::info))
    return throwError(exec, TypeError);
  DOM::NodeListImpl &list = *static_cast<DOMNodeList *>(thisObj)->impl();

  if (id == DOMNodeList::Item)
    return getDOMNode(exec, list.item(args[0]->toInt32(exec)));

  return jsUndefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMAttr::info = { "Attr", &DOMNode::info, &DOMAttrTable, 0 };

/* Source for DOMAttrTable. Use "make hashtables" to regenerate.
@begin DOMAttrTable 5
  name		DOMAttr::Name		DontDelete|ReadOnly
  specified	DOMAttr::Specified	DontDelete|ReadOnly
  value		DOMAttr::ValueProperty	DontDelete
  ownerElement	DOMAttr::OwnerElement	DontDelete|ReadOnly
@end
*/

DOMAttr::DOMAttr(ExecState *exec, AttrImpl *a)
  : DOMNode(exec, a)
{
}

bool DOMAttr::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMAttr, DOMNode>(exec, &DOMAttrTable, this, propertyName, slot);
}

ValueImp *DOMAttr::getValueProperty(ExecState *exec, int token) const
{
  AttrImpl *attr = static_cast<AttrImpl *>(impl());
  switch (token) {
  case Name:
    return getStringOrNull(attr->name());
  case Specified:
    return Boolean(attr->specified());
  case ValueProperty:
    return getStringOrNull(attr->value());
  case OwnerElement: // DOM2
    return getDOMNode(exec, attr->ownerElement());
  }
  return NULL; // not reached
}

void DOMAttr::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMAttr::put " << propertyName.qstring() << endl;
#endif
  lookupPut<DOMAttr,DOMNode>(exec, propertyName, value, attr,
                                      &DOMAttrTable, this );
}

void DOMAttr::putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/)
{
  DOMExceptionTranslator exception(exec);
  switch (token) {
  case ValueProperty:
    static_cast<AttrImpl *>(impl())->setValue(value->toString(exec).domString(), exception);
    return;
  default:
    kdWarning() << "DOMAttr::putValueProperty unhandled token " << token << endl;
  }
}

AttrImpl *toAttr(ValueImp *val)
{
    if (!val || !val->isObject(&DOMAttr::info))
        return 0;
    return static_cast<AttrImpl *>(static_cast<DOMNode *>(val)->impl());
}

// -------------------------------------------------------------------------

/* Source for DOMDocumentProtoTable. Use "make hashtables" to regenerate.
@begin DOMDocumentProtoTable 29
  createElement   DOMDocument::CreateElement                   DontDelete|Function 1
  createDocumentFragment DOMDocument::CreateDocumentFragment   DontDelete|Function 1
  createTextNode  DOMDocument::CreateTextNode                  DontDelete|Function 1
  createComment   DOMDocument::CreateComment                   DontDelete|Function 1
  createCDATASection DOMDocument::CreateCDATASection           DontDelete|Function 1
  createProcessingInstruction DOMDocument::CreateProcessingInstruction DontDelete|Function 1
  createAttribute DOMDocument::CreateAttribute                 DontDelete|Function 1
  createEntityReference DOMDocument::CreateEntityReference     DontDelete|Function 1
  elementFromPoint     DOMDocument::ElementFromPoint           DontDelete|Function 1
  getElementsByTagName  DOMDocument::GetElementsByTagName      DontDelete|Function 1
  importNode           DOMDocument::ImportNode                 DontDelete|Function 2
  createElementNS      DOMDocument::CreateElementNS            DontDelete|Function 2
  createAttributeNS    DOMDocument::CreateAttributeNS          DontDelete|Function 2
  getElementsByTagNameNS  DOMDocument::GetElementsByTagNameNS  DontDelete|Function 2
  getElementById     DOMDocument::GetElementById               DontDelete|Function 1
  createRange        DOMDocument::CreateRange                  DontDelete|Function 0
  createNodeIterator DOMDocument::CreateNodeIterator           DontDelete|Function 3
  createTreeWalker   DOMDocument::CreateTreeWalker             DontDelete|Function 4
  createEvent        DOMDocument::CreateEvent                  DontDelete|Function 1
  getOverrideStyle   DOMDocument::GetOverrideStyle             DontDelete|Function 2
  execCommand        DOMDocument::ExecCommand                  DontDelete|Function 3
  queryCommandEnabled DOMDocument::QueryCommandEnabled         DontDelete|Function 1
  queryCommandIndeterm DOMDocument::QueryCommandIndeterm       DontDelete|Function 1
  queryCommandState DOMDocument::QueryCommandState             DontDelete|Function 1
  queryCommandSupported DOMDocument::QueryCommandSupported     DontDelete|Function 1
  queryCommandValue DOMDocument::QueryCommandValue             DontDelete|Function 1
@end
*/
DEFINE_PROTOTYPE("DOMDocument", DOMDocumentProto)
IMPLEMENT_PROTOFUNC(DOMDocumentProtoFunc)
IMPLEMENT_PROTOTYPE_WITH_PARENT(DOMDocumentProto, DOMDocumentProtoFunc, DOMNodeProto)

const ClassInfo DOMDocument::info = { "Document", &DOMNode::info, &DOMDocumentTable, 0 };

/* Source for DOMDocumentTable. Use "make hashtables" to regenerate.
@begin DOMDocumentTable 4
  doctype         DOMDocument::DocType                         DontDelete|ReadOnly
  implementation  DOMDocument::Implementation                  DontDelete|ReadOnly
  documentElement DOMDocument::DocumentElement                 DontDelete|ReadOnly
  styleSheets     DOMDocument::StyleSheets                     DontDelete|ReadOnly
  preferredStylesheetSet  DOMDocument::PreferredStylesheetSet  DontDelete|ReadOnly
  selectedStylesheetSet  DOMDocument::SelectedStylesheetSet    DontDelete
  readyState      DOMDocument::ReadyState                      DontDelete|ReadOnly
  defaultView        DOMDocument::DefaultView                  DontDelete|ReadOnly
@end
*/

DOMDocument::DOMDocument(ExecState *exec, DocumentImpl *d)
  : DOMNode(d) 
{ 
  setPrototype(DOMDocumentProto::self(exec));
}

DOMDocument::DOMDocument(DocumentImpl *d)
  : DOMNode(d) 
{ 
}

DOMDocument::~DOMDocument()
{
  ScriptInterpreter::forgetDOMObject(static_cast<DocumentImpl *>(m_impl.get()));
}

bool DOMDocument::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMDocument, DOMNode>(exec, &DOMDocumentTable, this, propertyName, slot);
}

ValueImp *DOMDocument::getValueProperty(ExecState *exec, int token) const
{
  DocumentImpl &doc = *static_cast<DocumentImpl *>(impl());

  switch(token) {
  case DocType:
    return getDOMNode(exec,doc.doctype());
  case Implementation:
    return getDOMDOMImplementation(exec,doc.implementation());
  case DocumentElement:
    return getDOMNode(exec,doc.documentElement());
  case StyleSheets:
    //kdDebug() << "DOMDocument::StyleSheets, returning " << doc.styleSheets().length() << " stylesheets" << endl;
    return getDOMStyleSheetList(exec, doc.styleSheets(), &doc);
  case PreferredStylesheetSet:
    return getStringOrNull(doc.preferredStylesheetSet());
  case SelectedStylesheetSet:
    return getStringOrNull(doc.selectedStylesheetSet());
  case ReadyState:
    if (KHTMLPart* part = doc.part()) {
      if (part->d->m_bComplete) return String("complete");
      if (doc.parsing()) return String("loading");
      return String("loaded");
      // What does the interactive value mean ?
      // Missing support for "uninitialized"
    }
    return Undefined();
  case DOMDocument::DefaultView: // DOM2
    return getDOMAbstractView(exec,doc.defaultView());
  default:
    kdWarning() << "DOMDocument::getValueProperty unhandled token " << token << endl;
    return NULL;
  }
}

void DOMDocument::put(ExecState *exec, const Identifier& propertyName, ValueImp *value, int attr)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMDocument::put " << propertyName.qstring() << endl;
#endif
  lookupPut<DOMDocument,DOMNode>(exec, propertyName, value, attr, &DOMDocumentTable, this );
}

void DOMDocument::putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/)
{
  DocumentImpl &doc = *static_cast<DocumentImpl *>(impl());
  switch (token) {
    case SelectedStylesheetSet: {
      doc.setSelectedStylesheetSet(value->toString(exec).domString());
      break;
    }
  }
}

ValueImp *DOMDocumentProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMNode::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  NodeImpl &node = *static_cast<DOMNode *>(thisObj)->impl();
  DocumentImpl &doc = static_cast<DocumentImpl &>(node);
  UString str = args[0]->toString(exec);
  DOM::DOMString s = str.domString();

  switch(id) {
  case DOMDocument::CreateElement:
    return getDOMNode(exec,doc.createElement(s, exception));
  case DOMDocument::CreateDocumentFragment:
    return getDOMNode(exec,doc.createDocumentFragment());
  case DOMDocument::CreateTextNode:
    return getDOMNode(exec,doc.createTextNode(s));
  case DOMDocument::CreateComment:
    return getDOMNode(exec,doc.createComment(s));
  case DOMDocument::CreateCDATASection:
    return getDOMNode(exec, doc.createCDATASection(s, exception));
  case DOMDocument::CreateProcessingInstruction:
    return getDOMNode(exec, doc.createProcessingInstruction(args[0]->toString(exec).domString(), args[1]->toString(exec).domString(), exception));
  case DOMDocument::CreateAttribute:
    return getDOMNode(exec,doc.createAttribute(s, exception));
  case DOMDocument::CreateEntityReference:
    return getDOMNode(exec, doc.createEntityReference(s, exception));
  case DOMDocument::ElementFromPoint:
    return getDOMNode(exec,doc.elementFromPoint((int)args[0]->toNumber(exec), (int)args[1]->toNumber(exec)));
  case DOMDocument::GetElementsByTagName:
    return getDOMNodeList(exec,doc.getElementsByTagName(s).get());
  case DOMDocument::ImportNode: // DOM2
    return getDOMNode(exec,doc.importNode(toNode(args[0]), args[1]->toBoolean(exec), exception));
  case DOMDocument::CreateElementNS: // DOM2
    return getDOMNode(exec,doc.createElementNS(s, args[1]->toString(exec).domString(), exception));
  case DOMDocument::CreateAttributeNS: // DOM2
    return getDOMNode(exec,doc.createAttributeNS(s, args[1]->toString(exec).domString(), exception));
  case DOMDocument::GetElementsByTagNameNS: // DOM2
    return getDOMNodeList(exec,doc.getElementsByTagNameNS(s, args[1]->toString(exec).domString()).get());
  case DOMDocument::GetElementById:
    return getDOMNode(exec,doc.getElementById(args[0]->toString(exec).domString()));
  case DOMDocument::CreateRange:
    return getDOMRange(exec,doc.createRange());
  case DOMDocument::CreateNodeIterator: {
    NodeFilterImpl *filter = 0;
    ValueImp *arg2 = args[2];
    if (arg2->isObject()) {
      ObjectImp *o(static_cast<ObjectImp *>(arg2));
      filter = new NodeFilterImpl(new JSNodeFilterCondition(o));
    }
    return getDOMNodeIterator(exec,doc.createNodeIterator(toNode(args[0]), args[1]->toUInt32(exec),
        filter, args[3]->toBoolean(exec), exception));
  }
  case DOMDocument::CreateTreeWalker: {
    NodeFilterImpl *filter = 0;
    ValueImp *arg2 = args[2];
    if (arg2->isObject()) {
      ObjectImp *o(static_cast<ObjectImp *>(arg2));
      filter = new NodeFilterImpl(new JSNodeFilterCondition(o));
    }
    return getDOMTreeWalker(exec,doc.createTreeWalker(toNode(args[0]), args[1]->toUInt32(exec),
        filter, args[3]->toBoolean(exec), exception));
  }
  case DOMDocument::CreateEvent:
    return getDOMEvent(exec,doc.createEvent(s, exception));
  case DOMDocument::GetOverrideStyle:
    if (ElementImpl *element0 = toElement(args[0]))
        return getDOMCSSStyleDeclaration(exec,doc.getOverrideStyle(element0, args[1]->toString(exec).domString()));
    // FIXME: Is undefined right here, or should we raise an exception?
    return Undefined();
  case DOMDocument::ExecCommand: {
    return Boolean(doc.execCommand(args[0]->toString(exec).domString(), args[1]->toBoolean(exec), args[2]->toString(exec).domString()));
  }
  case DOMDocument::QueryCommandEnabled: {
    return Boolean(doc.queryCommandEnabled(args[0]->toString(exec).domString()));
  }
  case DOMDocument::QueryCommandIndeterm: {
    return Boolean(doc.queryCommandIndeterm(args[0]->toString(exec).domString()));
  }
  case DOMDocument::QueryCommandState: {
    return Boolean(doc.queryCommandState(args[0]->toString(exec).domString()));
  }
  case DOMDocument::QueryCommandSupported: {
    return Boolean(doc.queryCommandSupported(args[0]->toString(exec).domString()));
  }
  case DOMDocument::QueryCommandValue: {
    DOM::DOMString commandValue(doc.queryCommandValue(args[0]->toString(exec).domString()));
    // Method returns null DOMString to signal command is unsupported.
    // Microsoft documentation for this method says:
    // "If not supported [for a command identifier], this method returns a Boolean set to false."
    if (commandValue.isNull())
        return Boolean(false);
    else 
        return String(commandValue);
  }
  default:
    break;
  }

  return Undefined();
}

// -------------------------------------------------------------------------

/* Source for DOMElementProtoTable. Use "make hashtables" to regenerate.
@begin DOMElementProtoTable 17
  getAttribute		DOMElement::GetAttribute	DontDelete|Function 1
  setAttribute		DOMElement::SetAttribute	DontDelete|Function 2
  removeAttribute	DOMElement::RemoveAttribute	DontDelete|Function 1
  getAttributeNode	DOMElement::GetAttributeNode	DontDelete|Function 1
  setAttributeNode	DOMElement::SetAttributeNode	DontDelete|Function 2
  removeAttributeNode	DOMElement::RemoveAttributeNode	DontDelete|Function 1
  getElementsByTagName	DOMElement::GetElementsByTagName	DontDelete|Function 1
  hasAttribute		DOMElement::HasAttribute	DontDelete|Function 1
  getAttributeNS	DOMElement::GetAttributeNS	DontDelete|Function 2
  setAttributeNS	DOMElement::SetAttributeNS	DontDelete|Function 3
  removeAttributeNS	DOMElement::RemoveAttributeNS	DontDelete|Function 2
  getAttributeNodeNS	DOMElement::GetAttributeNodeNS	DontDelete|Function 2
  setAttributeNodeNS	DOMElement::SetAttributeNodeNS	DontDelete|Function 1
  getElementsByTagNameNS DOMElement::GetElementsByTagNameNS	DontDelete|Function 2
  hasAttributeNS	DOMElement::HasAttributeNS	DontDelete|Function 2
  scrollIntoView        DOMElement::ScrollIntoView      DontDelete|Function 1

# extension for Safari RSS
  scrollByLines         DOMElement::ScrollByLines       DontDelete|Function 1
  scrollByPages         DOMElement::ScrollByPages       DontDelete|Function 1

@end
*/
DEFINE_PROTOTYPE("DOMElement",DOMElementProto)
IMPLEMENT_PROTOFUNC(DOMElementProtoFunc)
IMPLEMENT_PROTOTYPE_WITH_PARENT(DOMElementProto,DOMElementProtoFunc,DOMNodeProto)

const ClassInfo DOMElement::info = { "Element", &DOMNode::info, &DOMElementTable, 0 };
/* Source for DOMElementTable. Use "make hashtables" to regenerate.
@begin DOMElementTable 3
  tagName	DOMElement::TagName                         DontDelete|ReadOnly
  style		DOMElement::Style                           DontDelete|ReadOnly
@end
*/
DOMElement::DOMElement(ExecState *exec, ElementImpl *e)
  : DOMNode(e) 
{
  setPrototype(DOMElementProto::self(exec));
}

DOMElement::DOMElement(ElementImpl *e)
  : DOMNode(e) 
{ 
}

ValueImp *DOMElement::getValueProperty(ExecState *exec, int token) const
{
  ElementImpl *element = static_cast<ElementImpl *>(impl());
  switch (token) {
  case TagName:
    return getStringOrNull(element->nodeName());
  case Style:
    return getDOMCSSStyleDeclaration(exec, element->style());
  default:
    assert(0);
    return Undefined();
  }
}

ValueImp *DOMElement::attributeGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMElement *thisObj = static_cast<DOMElement *>(slot.slotBase());

  ElementImpl *element = static_cast<ElementImpl *>(thisObj->impl());
  DOM::DOMString attr = element->getAttribute(propertyName.domString());
  return getStringOrNull(attr);
}

bool DOMElement::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  const HashEntry* entry = Lookup::findEntry(&DOMElementTable, propertyName);
  if (entry) {
    slot.setStaticEntry(this, entry, staticValueGetter<DOMElement>);
    return true;
  }

  // We have to check in DOMNode before giving access to attributes, otherwise
  // onload="..." would make onload return the string (attribute value) instead of
  // the listener object (function).
  if (DOMNode::getOwnPropertySlot(exec, propertyName, slot))
    return true;

  ValueImp *proto = prototype();
  if (proto->isObject() && static_cast<ObjectImp *>(proto)->hasProperty(exec, propertyName))
    return false;

  ElementImpl &element = *static_cast<ElementImpl *>(impl());

  // FIXME: do we really want to do this attribute lookup thing? Mozilla doesn't do it,
  // and it seems like it could interfere with XBL.
  DOM::DOMString attr = element.getAttribute(propertyName.domString());
  if (!attr.isNull()) {
    slot.setCustom(this, attributeGetter);
    return true;
  }

  return false;
}

ValueImp *DOMElementProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMNode::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  NodeImpl &node = *static_cast<DOMNode *>(thisObj)->impl();
  ElementImpl &element = static_cast<ElementImpl &>(node);

  switch(id) {
    case DOMElement::GetAttribute:
      // getStringOrNull should be used here, since if the attribute isn't present at all, you should
      // return null and not "".
      return getStringOrNull(element.getAttribute(args[0]->toString(exec).domString()));
    case DOMElement::SetAttribute:
      element.setAttribute(args[0]->toString(exec).domString(), args[1]->toString(exec).domString(), exception);
      return Undefined();
    case DOMElement::RemoveAttribute:
      element.removeAttribute(args[0]->toString(exec).domString(), exception);
      return Undefined();
    case DOMElement::GetAttributeNode:
      return getDOMNode(exec,element.getAttributeNode(args[0]->toString(exec).domString()));
    case DOMElement::SetAttributeNode:
      return getDOMNode(exec,element.setAttributeNode(toAttr(args[0]), exception).get());
    case DOMElement::RemoveAttributeNode:
      return getDOMNode(exec,element.removeAttributeNode(toAttr(args[0]), exception).get());
    case DOMElement::GetElementsByTagName:
      return getDOMNodeList(exec, element.getElementsByTagName(args[0]->toString(exec).domString()).get());
    case DOMElement::HasAttribute: // DOM2
      return Boolean(element.hasAttribute(args[0]->toString(exec).domString()));
    case DOMElement::GetAttributeNS: // DOM2
      return String(element.getAttributeNS(args[0]->toString(exec).domString(),args[1]->toString(exec).domString()).domString());
    case DOMElement::SetAttributeNS: // DOM2
      element.setAttributeNS(args[0]->toString(exec).domString(), args[1]->toString(exec).domString(), args[2]->toString(exec).domString(), exception);
      return Undefined();
    case DOMElement::RemoveAttributeNS: // DOM2
      element.removeAttributeNS(args[0]->toString(exec).domString(), args[1]->toString(exec).domString(), exception);
      return Undefined();
    case DOMElement::GetAttributeNodeNS: // DOM2
      return getDOMNode(exec,element.getAttributeNodeNS(args[0]->toString(exec).domString(),args[1]->toString(exec).domString()));
    case DOMElement::SetAttributeNodeNS: // DOM2
      return getDOMNode(exec, element.setAttributeNodeNS(toAttr(args[0]), exception).get());
    case DOMElement::GetElementsByTagNameNS: // DOM2
      return getDOMNodeList(exec, element.getElementsByTagNameNS(args[0]->toString(exec).domString(), args[1]->toString(exec).domString()).get());
    case DOMElement::HasAttributeNS: // DOM2
      return Boolean(element.hasAttributeNS(args[0]->toString(exec).domString(),args[1]->toString(exec).domString()));
    case DOMElement::ScrollIntoView: 
        element.scrollIntoView(args[0]->isUndefinedOrNull() || args[0]->toBoolean(exec));
        return Undefined();
    case DOMElement::ScrollByLines:
    case DOMElement::ScrollByPages:
      if (DocumentImpl* doc = element.getDocument()) {
        doc->updateLayoutIgnorePendingStylesheets();
        if (RenderObject *rend = element.renderer())
          if (rend->hasOverflowClip()) {
            KWQScrollDirection direction = KWQScrollDown;
            int multiplier = args[0]->toInt32(exec);
            if (multiplier < 0) {
                direction = KWQScrollUp;
                multiplier = -multiplier;
            }
            KWQScrollGranularity granularity = id == DOMElement::ScrollByLines ? KWQScrollLine : KWQScrollPage;
            rend->layer()->scroll(direction, granularity, multiplier);
          }
      }
      return Undefined();
    default:
      return Undefined();
    }
}

ElementImpl *toElement(ValueImp *val)
{
    if (!val || !val->isObject(&DOMElement::info))
        return 0;
    return static_cast<ElementImpl *>(static_cast<DOMElement *>(val)->impl());
}

// -------------------------------------------------------------------------

/* Source for DOMDOMImplementationProtoTable. Use "make hashtables" to regenerate.
@begin DOMDOMImplementationProtoTable 5
  hasFeature		DOMDOMImplementation::HasFeature		DontDelete|Function 2
# DOM2
  createCSSStyleSheet	DOMDOMImplementation::CreateCSSStyleSheet	DontDelete|Function 2
  createDocumentType	DOMDOMImplementation::CreateDocumentType	DontDelete|Function 3
  createDocument	DOMDOMImplementation::CreateDocument		DontDelete|Function 3
  createHTMLDocument    DOMDOMImplementation::CreateHTMLDocument        DontDelete|Function 1
@end
*/
DEFINE_PROTOTYPE("DOMImplementation",DOMDOMImplementationProto)
IMPLEMENT_PROTOFUNC(DOMDOMImplementationProtoFunc)
IMPLEMENT_PROTOTYPE(DOMDOMImplementationProto,DOMDOMImplementationProtoFunc)

const ClassInfo DOMDOMImplementation::info = { "DOMImplementation", 0, 0, 0 };

DOMDOMImplementation::DOMDOMImplementation(ExecState *exec, DOMImplementationImpl *i)
  : m_impl(i) 
{ 
  setPrototype(DOMDOMImplementationProto::self(exec));
}

DOMDOMImplementation::~DOMDOMImplementation()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

ValueImp *DOMDOMImplementationProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMDOMImplementation::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  DOMImplementationImpl &implementation = *static_cast<DOMDOMImplementation *>(thisObj)->impl();

  switch(id) {
  case DOMDOMImplementation::HasFeature:
    return Boolean(implementation.hasFeature(args[0]->toString(exec).domString(),
        args[1]->isUndefinedOrNull() ? DOMString() : args[1]->toString(exec).domString()));
  case DOMDOMImplementation::CreateDocumentType: // DOM2
    return getDOMNode(exec, implementation.createDocumentType(args[0]->toString(exec).domString(),
        args[1]->toString(exec).domString(), args[2]->toString(exec).domString(), exception));
  case DOMDOMImplementation::CreateDocument: // DOM2
    return getDOMNode(exec, implementation.createDocument(args[0]->toString(exec).domString(),
        args[1]->toString(exec).domString(), toDocumentType(args[2]), exception));
  case DOMDOMImplementation::CreateCSSStyleSheet: // DOM2
    return getDOMStyleSheet(exec, implementation.createCSSStyleSheet(args[0]->toString(exec).domString(), args[1]->toString(exec).domString(), exception));
  case DOMDOMImplementation::CreateHTMLDocument: // DOM2-HTML
    return getDOMNode(exec, implementation.createHTMLDocument(args[0]->toString(exec).domString()));
  default:
    break;
  }
  return Undefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMDocumentType::info = { "DocumentType", &DOMNode::info, &DOMDocumentTypeTable, 0 };

/* Source for DOMDocumentTypeTable. Use "make hashtables" to regenerate.
@begin DOMDocumentTypeTable 6
  name			DOMDocumentType::Name		DontDelete|ReadOnly
  entities		DOMDocumentType::Entities	DontDelete|ReadOnly
  notations		DOMDocumentType::Notations	DontDelete|ReadOnly
# DOM2
  publicId		DOMDocumentType::PublicId	DontDelete|ReadOnly
  systemId		DOMDocumentType::SystemId	DontDelete|ReadOnly
  internalSubset	DOMDocumentType::InternalSubset	DontDelete|ReadOnly
@end
*/
DOMDocumentType::DOMDocumentType(ExecState *exec, DocumentTypeImpl *dt)
  : DOMNode( /*### no proto yet*/exec, dt ) { }

bool DOMDocumentType::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMDocumentType, DOMNode>(exec, &DOMDocumentTypeTable, this, propertyName, slot);
}

ValueImp *DOMDocumentType::getValueProperty(ExecState *exec, int token) const
{
  DocumentTypeImpl &type = *static_cast<DocumentTypeImpl *>(impl());
  switch (token) {
  case Name:
    return getStringOrNull(type.name());
  case Entities:
    return getDOMNamedNodeMap(exec,type.entities());
  case Notations:
    return getDOMNamedNodeMap(exec,type.notations());
  case PublicId: // DOM2
    return getStringOrNull(type.publicId());
  case SystemId: // DOM2
    return getStringOrNull(type.systemId());
  case InternalSubset: // DOM2
    return getStringOrNull(type.internalSubset());
  default:
    kdWarning() << "DOMDocumentType::getValueProperty unhandled token " << token << endl;
    return NULL;
  }
}

DocumentTypeImpl *toDocumentType(ValueImp *val)
{
    if (!val || !val->isObject(&DOMDocumentType::info))
        return 0;
    return static_cast<DocumentTypeImpl *>(static_cast<DOMNode *>(val)->impl());
}

// -------------------------------------------------------------------------

/* Source for DOMNamedNodeMapProtoTable. Use "make hashtables" to regenerate.
@begin DOMNamedNodeMapProtoTable 7
  getNamedItem		DOMNamedNodeMap::GetNamedItem		DontDelete|Function 1
  setNamedItem		DOMNamedNodeMap::SetNamedItem		DontDelete|Function 1
  removeNamedItem	DOMNamedNodeMap::RemoveNamedItem	DontDelete|Function 1
  item			DOMNamedNodeMap::Item			DontDelete|Function 1
# DOM2
  getNamedItemNS	DOMNamedNodeMap::GetNamedItemNS		DontDelete|Function 2
  setNamedItemNS	DOMNamedNodeMap::SetNamedItemNS		DontDelete|Function 1
  removeNamedItemNS	DOMNamedNodeMap::RemoveNamedItemNS	DontDelete|Function 2
@end
*/
DEFINE_PROTOTYPE("NamedNodeMap", DOMNamedNodeMapProto)
IMPLEMENT_PROTOFUNC(DOMNamedNodeMapProtoFunc)
IMPLEMENT_PROTOTYPE(DOMNamedNodeMapProto,DOMNamedNodeMapProtoFunc)

const ClassInfo DOMNamedNodeMap::info = { "NamedNodeMap", 0, 0, 0 };

DOMNamedNodeMap::DOMNamedNodeMap(ExecState *exec, NamedNodeMapImpl *m)
  : m_impl(m) 
{ 
  setPrototype(DOMNamedNodeMapProto::self(exec));
}

DOMNamedNodeMap::~DOMNamedNodeMap()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

ValueImp *DOMNamedNodeMap::lengthGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNamedNodeMap *thisObj = static_cast<DOMNamedNodeMap *>(slot.slotBase());
  return Number(thisObj->m_impl->length());
}

ValueImp *DOMNamedNodeMap::indexGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNamedNodeMap *thisObj = static_cast<DOMNamedNodeMap *>(slot.slotBase());
  return getDOMNode(exec, thisObj->m_impl->item(slot.index()));
}

bool DOMNamedNodeMap::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  NamedNodeMapImpl &map = *m_impl;
  if (propertyName == lengthPropertyName) {
    slot.setCustom(this, lengthGetter);
    return true;
  }

  // array index ?
  bool ok;
  long unsigned int idx = propertyName.toULong(&ok);
  if (ok && idx < map.length()) {
    slot.setCustomIndex(this, idx, indexGetter);
    return true;
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

ValueImp *DOMNamedNodeMapProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMNamedNodeMap::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  NamedNodeMapImpl &map = *static_cast<DOMNamedNodeMap *>(thisObj)->impl();

  switch(id) {
    case DOMNamedNodeMap::GetNamedItem:
      return getDOMNode(exec, map.getNamedItem(args[0]->toString(exec).domString()));
    case DOMNamedNodeMap::SetNamedItem:
      return getDOMNode(exec, map.setNamedItem(toNode(args[0]), exception).get());
    case DOMNamedNodeMap::RemoveNamedItem:
      return getDOMNode(exec, map.removeNamedItem(args[0]->toString(exec).domString(), exception).get());
    case DOMNamedNodeMap::Item:
      return getDOMNode(exec, map.item(args[0]->toInt32(exec)));
    case DOMNamedNodeMap::GetNamedItemNS: // DOM2
      return getDOMNode(exec, map.getNamedItemNS(args[0]->toString(exec).domString(), args[1]->toString(exec).domString()));
    case DOMNamedNodeMap::SetNamedItemNS: // DOM2
      return getDOMNode(exec, map.setNamedItemNS(toNode(args[0]), exception).get());
    case DOMNamedNodeMap::RemoveNamedItemNS: // DOM2
      return getDOMNode(exec, map.removeNamedItemNS(args[0]->toString(exec).domString(), args[1]->toString(exec).domString(), exception).get());
    default:
      break;
  }

  return Undefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMProcessingInstruction::info = { "ProcessingInstruction", &DOMNode::info, &DOMProcessingInstructionTable, 0 };

/* Source for DOMProcessingInstructionTable. Use "make hashtables" to regenerate.
@begin DOMProcessingInstructionTable 3
  target	DOMProcessingInstruction::Target	DontDelete|ReadOnly
  data		DOMProcessingInstruction::Data		DontDelete
  sheet		DOMProcessingInstruction::Sheet		DontDelete|ReadOnly
@end
*/

DOMProcessingInstruction::DOMProcessingInstruction(ExecState *exec, ProcessingInstructionImpl *i)
  : DOMNode(exec, i)
{
}

bool DOMProcessingInstruction::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMProcessingInstruction, DOMNode>(exec, &DOMProcessingInstructionTable, this, propertyName, slot);
}

ValueImp *DOMProcessingInstruction::getValueProperty(ExecState *exec, int token) const
{
  ProcessingInstructionImpl *pi = static_cast<ProcessingInstructionImpl *>(impl());
  switch (token) {
  case Target:
    return getStringOrNull(pi->target());
  case Data:
    return getStringOrNull(pi->data());
  case Sheet:
    return getDOMStyleSheet(exec,pi->sheet());
  default:
    kdWarning() << "DOMProcessingInstruction::getValueProperty unhandled token " << token << endl;
    return NULL;
  }
}

void DOMProcessingInstruction::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
  ProcessingInstructionImpl *pi = static_cast<ProcessingInstructionImpl *>(impl());
  DOMExceptionTranslator exception(exec);
  // Not worth using the hashtable for this one ;)
  if (propertyName == "data")
    pi->setData(value->toString(exec).domString(), exception);
  else
    DOMNode::put(exec, propertyName, value, attr);
}

// -------------------------------------------------------------------------

const ClassInfo DOMNotation::info = { "Notation", &DOMNode::info, &DOMNotationTable, 0 };

/* Source for DOMNotationTable. Use "make hashtables" to regenerate.
@begin DOMNotationTable 2
  publicId		DOMNotation::PublicId	DontDelete|ReadOnly
  systemId		DOMNotation::SystemId	DontDelete|ReadOnly
@end
*/

DOMNotation::DOMNotation(ExecState *exec, NotationImpl *n)
  : DOMNode(exec, n)
{
}

bool DOMNotation::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMNotation, DOMNode>(exec, &DOMNotationTable, this, propertyName, slot);
}

ValueImp *DOMNotation::getValueProperty(ExecState *, int token) const
{
  switch (token) {
  case PublicId:
    return getStringOrNull(static_cast<NotationImpl *>(impl())->publicId());
  case SystemId:
    return getStringOrNull(static_cast<NotationImpl *>(impl())->systemId());
  default:
    kdWarning() << "DOMNotation::getValueProperty unhandled token " << token << endl;
    return NULL;
  }
}

// -------------------------------------------------------------------------

const ClassInfo DOMEntity::info = { "Entity", &DOMNode::info, 0, 0 };

/* Source for DOMEntityTable. Use "make hashtables" to regenerate.
@begin DOMEntityTable 2
  publicId		DOMEntity::PublicId		DontDelete|ReadOnly
  systemId		DOMEntity::SystemId		DontDelete|ReadOnly
  notationName		DOMEntity::NotationName	DontDelete|ReadOnly
@end
*/

DOMEntity::DOMEntity(ExecState *exec, EntityImpl *e)
  : DOMNode(exec, e)
{
}

bool DOMEntity::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMEntity, DOMNode>(exec, &DOMEntityTable, this, propertyName, slot);
}

ValueImp *DOMEntity::getValueProperty(ExecState *, int token) const
{
  switch (token) {
  case PublicId:
    return getStringOrNull(static_cast<EntityImpl *>(impl())->publicId());
  case SystemId:
    return getStringOrNull(static_cast<EntityImpl *>(impl())->systemId());
  case NotationName:
    return getStringOrNull(static_cast<EntityImpl *>(impl())->notationName());
  default:
    kdWarning() << "DOMEntity::getValueProperty unhandled token " << token << endl;
    return NULL;
  }
}

// -------------------------------------------------------------------------

ValueImp *getDOMDocumentNode(ExecState *exec, DocumentImpl *n)
{
  DOMDocument *ret = 0;
  ScriptInterpreter* interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());

  if ((ret = static_cast<DOMDocument *>(interp->getDOMObject(n))))
    return ret;

  if (n->isHTMLDocument())
    ret = new HTMLDocument(exec, static_cast<HTMLDocumentImpl *>(n));
  else
    ret = new DOMDocument(exec, n);

  // Make sure the document is kept around by the window object, and works right with the
  // back/forward cache.
  if (n->view()) {
    static Identifier documentIdentifier("document");
    Window::retrieveWindow(n->view()->part())->putDirect(documentIdentifier, ret, DontDelete|ReadOnly);
  }

  interp->putDOMObject(n, ret);

  return ret;
}

bool checkNodeSecurity(ExecState *exec, NodeImpl *n)
{
  if (!n) 
    return false;

  // Check to see if the currently executing interpreter is allowed to access the specified node
  KHTMLPart *part = n->getDocument()->part();
  if (!part)
    return false;
  Window *win = Window::retrieveWindow(part);
  return win && win->isSafeScript(exec);
}

ValueImp *getDOMNode(ExecState *exec, NodeImpl *n)
{
  DOMNode *ret = 0;
  if (!n)
    return Null();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());
  DocumentImpl *doc = n->getDocument();

  if ((ret = interp->getDOMNodeForDocument(doc, n)))
    return ret;

  switch (n->nodeType()) {
    case DOM::Node::ELEMENT_NODE:
      if (n->isHTMLElement())
        ret = new HTMLElement(exec, static_cast<HTMLElementImpl *>(n));
      else
        ret = new DOMElement(exec, static_cast<ElementImpl *>(n));
      break;
    case DOM::Node::ATTRIBUTE_NODE:
      ret = new DOMAttr(exec, static_cast<AttrImpl *>(n));
      break;
    case DOM::Node::TEXT_NODE:
    case DOM::Node::CDATA_SECTION_NODE:
      ret = new DOMText(exec, static_cast<TextImpl *>(n));
      break;
    case DOM::Node::ENTITY_NODE:
      ret = new DOMEntity(exec, static_cast<EntityImpl *>(n));
      break;
    case DOM::Node::PROCESSING_INSTRUCTION_NODE:
      ret = new DOMProcessingInstruction(exec, static_cast<ProcessingInstructionImpl *>(n));
      break;
    case DOM::Node::COMMENT_NODE:
      ret = new DOMCharacterData(exec, static_cast<CharacterDataImpl *>(n));
      break;
    case DOM::Node::DOCUMENT_NODE:
      // we don't want to cache the document itself in the per-document dictionary
      return getDOMDocumentNode(exec, static_cast<DocumentImpl *>(n));
    case DOM::Node::DOCUMENT_TYPE_NODE:
      ret = new DOMDocumentType(exec, static_cast<DocumentTypeImpl *>(n));
      break;
    case DOM::Node::NOTATION_NODE:
      ret = new DOMNotation(exec, static_cast<NotationImpl *>(n));
      break;
    case DOM::Node::DOCUMENT_FRAGMENT_NODE:
    case DOM::Node::ENTITY_REFERENCE_NODE:
    default:
      ret = new DOMNode(exec, n);
  }

  interp->putDOMNodeForDocument(doc, n, ret);

  return ret;
}

ValueImp *getDOMNamedNodeMap(ExecState *exec, NamedNodeMapImpl *m)
{
  return cacheDOMObject<NamedNodeMapImpl, DOMNamedNodeMap>(exec, m);
}

ValueImp *getRuntimeObject(ExecState *exec, NodeImpl *n)
{
    if (!n)
        return 0;

    if (n->hasTagName(appletTag)) {
        HTMLAppletElementImpl *appletElement = static_cast<HTMLAppletElementImpl *>(n);
        if (appletElement->getAppletInstance())
            // The instance is owned by the applet element.
            return new RuntimeObjectImp(appletElement->getAppletInstance(), false);
    }
    else if (n->hasTagName(embedTag)) {
        HTMLEmbedElementImpl *embedElement = static_cast<HTMLEmbedElementImpl *>(n);
        if (embedElement->getEmbedInstance())
            return new RuntimeObjectImp(embedElement->getEmbedInstance(), false);
    }
    else if (n->hasTagName(objectTag)) {
        HTMLObjectElementImpl *objectElement = static_cast<HTMLObjectElementImpl *>(n);
        if (objectElement->getObjectInstance())
            return new RuntimeObjectImp(objectElement->getObjectInstance(), false);
    }
    
    // If we don't have a runtime object return 0.
    return 0;
}

ValueImp *getDOMNodeList(ExecState *exec, NodeListImpl *l)
{
  return cacheDOMObject<NodeListImpl, DOMNodeList>(exec, l);
}

ValueImp *getDOMDOMImplementation(ExecState *exec, DOMImplementationImpl *i)
{
  return cacheDOMObject<DOMImplementationImpl, DOMDOMImplementation>(exec, i);
}

// -------------------------------------------------------------------------

const ClassInfo NodeConstructor::info = { "NodeConstructor", 0, &NodeConstructorTable, 0 };
/* Source for NodeConstructorTable. Use "make hashtables" to regenerate.
@begin NodeConstructorTable 11
  ELEMENT_NODE		DOM::Node::ELEMENT_NODE		DontDelete|ReadOnly
  ATTRIBUTE_NODE	DOM::Node::ATTRIBUTE_NODE		DontDelete|ReadOnly
  TEXT_NODE		DOM::Node::TEXT_NODE		DontDelete|ReadOnly
  CDATA_SECTION_NODE	DOM::Node::CDATA_SECTION_NODE	DontDelete|ReadOnly
  ENTITY_REFERENCE_NODE	DOM::Node::ENTITY_REFERENCE_NODE	DontDelete|ReadOnly
  ENTITY_NODE		DOM::Node::ENTITY_NODE		DontDelete|ReadOnly
  PROCESSING_INSTRUCTION_NODE DOM::Node::PROCESSING_INSTRUCTION_NODE DontDelete|ReadOnly
  COMMENT_NODE		DOM::Node::COMMENT_NODE		DontDelete|ReadOnly
  DOCUMENT_NODE		DOM::Node::DOCUMENT_NODE		DontDelete|ReadOnly
  DOCUMENT_TYPE_NODE	DOM::Node::DOCUMENT_TYPE_NODE	DontDelete|ReadOnly
  DOCUMENT_FRAGMENT_NODE DOM::Node::DOCUMENT_FRAGMENT_NODE	DontDelete|ReadOnly
  NOTATION_NODE		DOM::Node::NOTATION_NODE		DontDelete|ReadOnly
@end
*/
bool NodeConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<NodeConstructor, DOMObject>(exec, &NodeConstructorTable, this, propertyName, slot);
}

ValueImp *NodeConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return Number((unsigned int)token);
#if 0
  switch (token) {
  case ELEMENT_NODE:
    return Number((unsigned int)DOM::Node::ELEMENT_NODE);
  case ATTRIBUTE_NODE:
    return Number((unsigned int)DOM::Node::ATTRIBUTE_NODE);
  case TEXT_NODE:
    return Number((unsigned int)DOM::Node::TEXT_NODE);
  case CDATA_SECTION_NODE:
    return Number((unsigned int)DOM::Node::CDATA_SECTION_NODE);
  case ENTITY_REFERENCE_NODE:
    return Number((unsigned int)DOM::Node::ENTITY_REFERENCE_NODE);
  case ENTITY_NODE:
    return Number((unsigned int)DOM::Node::ENTITY_NODE);
  case PROCESSING_INSTRUCTION_NODE:
    return Number((unsigned int)DOM::Node::PROCESSING_INSTRUCTION_NODE);
  case COMMENT_NODE:
    return Number((unsigned int)DOM::Node::COMMENT_NODE);
  case DOCUMENT_NODE:
    return Number((unsigned int)DOM::Node::DOCUMENT_NODE);
  case DOCUMENT_TYPE_NODE:
    return Number((unsigned int)DOM::Node::DOCUMENT_TYPE_NODE);
  case DOCUMENT_FRAGMENT_NODE:
    return Number((unsigned int)DOM::Node::DOCUMENT_FRAGMENT_NODE);
  case NOTATION_NODE:
    return Number((unsigned int)DOM::Node::NOTATION_NODE);
  default:
    kdWarning() << "NodeConstructor::getValueProperty unhandled token " << token << endl;
    return NULL;
  }
#endif
}

ObjectImp *getNodeConstructor(ExecState *exec)
{
  return cacheGlobalObject<NodeConstructor>(exec, "[[node.constructor]]");
}

// -------------------------------------------------------------------------

const ClassInfo DOMExceptionConstructor::info = { "DOMExceptionConstructor", 0, 0, 0 };

/* Source for DOMExceptionConstructorTable. Use "make hashtables" to regenerate.
@begin DOMExceptionConstructorTable 15
  INDEX_SIZE_ERR		DOM::DOMException::INDEX_SIZE_ERR		DontDelete|ReadOnly
  DOMSTRING_SIZE_ERR		DOM::DOMException::DOMSTRING_SIZE_ERR	DontDelete|ReadOnly
  HIERARCHY_REQUEST_ERR		DOM::DOMException::HIERARCHY_REQUEST_ERR	DontDelete|ReadOnly
  WRONG_DOCUMENT_ERR		DOM::DOMException::WRONG_DOCUMENT_ERR	DontDelete|ReadOnly
  INVALID_CHARACTER_ERR		DOM::DOMException::INVALID_CHARACTER_ERR	DontDelete|ReadOnly
  NO_DATA_ALLOWED_ERR		DOM::DOMException::NO_DATA_ALLOWED_ERR	DontDelete|ReadOnly
  NO_MODIFICATION_ALLOWED_ERR	DOM::DOMException::NO_MODIFICATION_ALLOWED_ERR	DontDelete|ReadOnly
  NOT_FOUND_ERR			DOM::DOMException::NOT_FOUND_ERR		DontDelete|ReadOnly
  NOT_SUPPORTED_ERR		DOM::DOMException::NOT_SUPPORTED_ERR	DontDelete|ReadOnly
  INUSE_ATTRIBUTE_ERR		DOM::DOMException::INUSE_ATTRIBUTE_ERR	DontDelete|ReadOnly
  INVALID_STATE_ERR		DOM::DOMException::INVALID_STATE_ERR	DontDelete|ReadOnly
  SYNTAX_ERR			DOM::DOMException::SYNTAX_ERR		DontDelete|ReadOnly
  INVALID_MODIFICATION_ERR	DOM::DOMException::INVALID_MODIFICATION_ERR	DontDelete|ReadOnly
  NAMESPACE_ERR			DOM::DOMException::NAMESPACE_ERR		DontDelete|ReadOnly
  INVALID_ACCESS_ERR		DOM::DOMException::INVALID_ACCESS_ERR	DontDelete|ReadOnly
@end
*/

bool DOMExceptionConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMExceptionConstructor, DOMObject>(exec, &DOMExceptionConstructorTable, this, propertyName, slot);
}

ValueImp *DOMExceptionConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return Number((unsigned int)token);
#if 0
  switch (token) {
  case INDEX_SIZE_ERR:
    return Number((unsigned int)DOM::DOMException::INDEX_SIZE_ERR);
  case DOMSTRING_SIZE_ERR:
    return Number((unsigned int)DOM::DOMException::DOMSTRING_SIZE_ERR);
  case HIERARCHY_REQUEST_ERR:
    return Number((unsigned int)DOM::DOMException::HIERARCHY_REQUEST_ERR);
  case WRONG_DOCUMENT_ERR:
    return Number((unsigned int)DOM::DOMException::WRONG_DOCUMENT_ERR);
  case INVALID_CHARACTER_ERR:
    return Number((unsigned int)DOM::DOMException::INVALID_CHARACTER_ERR);
  case NO_DATA_ALLOWED_ERR:
    return Number((unsigned int)DOM::DOMException::NO_DATA_ALLOWED_ERR);
  case NO_MODIFICATION_ALLOWED_ERR:
    return Number((unsigned int)DOM::DOMException::NO_MODIFICATION_ALLOWED_ERR);
  case NOT_FOUND_ERR:
    return Number((unsigned int)DOM::DOMException::NOT_FOUND_ERR);
  case NOT_SUPPORTED_ERR:
    return Number((unsigned int)DOM::DOMException::NOT_SUPPORTED_ERR);
  case INUSE_ATTRIBUTE_ERR:
    return Number((unsigned int)DOM::DOMException::INUSE_ATTRIBUTE_ERR);
  case INVALID_STATE_ERR:
    return Number((unsigned int)DOM::DOMException::INVALID_STATE_ERR);
  case SYNTAX_ERR:
    return Number((unsigned int)DOM::DOMException::SYNTAX_ERR);
  case INVALID_MODIFICATION_ERR:
    return Number((unsigned int)DOM::DOMException::INVALID_MODIFICATION_ERR);
  case NAMESPACE_ERR:
    return Number((unsigned int)DOM::DOMException::NAMESPACE_ERR);
  case INVALID_ACCESS_ERR:
    return Number((unsigned int)DOM::DOMException::INVALID_ACCESS_ERR);
  default:
    kdWarning() << "DOMExceptionConstructor::getValueProperty unhandled token " << token << endl;
    return NULL;
  }
#endif
}

ObjectImp *getDOMExceptionConstructor(ExecState *exec)
{
  return cacheGlobalObject<DOMExceptionConstructor>(exec, "[[DOMException.constructor]]");
}

// -------------------------------------------------------------------------

// Such a collection is usually very short-lived, it only exists
// for constructs like document.forms.<name>[1],
// so it shouldn't be a problem that it's storing all the nodes (with the same name). (David)
DOMNamedNodesCollection::DOMNamedNodesCollection(ExecState *, const QValueList< SharedPtr<NodeImpl> >& nodes )
  : m_nodes(nodes)
{
}

ValueImp *DOMNamedNodesCollection::lengthGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNamedNodesCollection *thisObj = static_cast<DOMNamedNodesCollection *>(slot.slotBase());
  return Number(thisObj->m_nodes.count());
}

ValueImp *DOMNamedNodesCollection::indexGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMNamedNodesCollection *thisObj = static_cast<DOMNamedNodesCollection *>(slot.slotBase());
  return getDOMNode(exec, thisObj->m_nodes[slot.index()].get());
}

bool DOMNamedNodesCollection::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (propertyName == lengthPropertyName) {
    slot.setCustom(this, lengthGetter);
    return true;
  }

  // array index ?
  bool ok;
  long unsigned int idx = propertyName.toULong(&ok);
  if (ok && idx < m_nodes.count()) {
    slot.setCustomIndex(this, idx, indexGetter);
    return true;
  }

  // For IE compatibility, we need to be able to look up elements in a
  // document.formName.name result by id as well as be index.

  QValueListConstIterator< SharedPtr<NodeImpl> > end = m_nodes.end();
  int i = 0;
  for (QValueListConstIterator< SharedPtr<NodeImpl> > it = m_nodes.begin(); it != end; ++it, ++i) {
    NodeImpl *node = (*it).get();
    if (node->hasAttributes() &&
        node->attributes()->id() == propertyName.domString()) {
      slot.setCustomIndex(this, i, indexGetter);
      return true;
    }
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

// -------------------------------------------------------------------------

const ClassInfo DOMCharacterData::info = { "CharacterImp",
					  &DOMNode::info, &DOMCharacterDataTable, 0 };
/*
@begin DOMCharacterDataTable 2
  data		DOMCharacterData::Data		DontDelete
  length	DOMCharacterData::Length	DontDelete|ReadOnly
@end
@begin DOMCharacterDataProtoTable 7
  substringData	DOMCharacterData::SubstringData	DontDelete|Function 2
  appendData	DOMCharacterData::AppendData	DontDelete|Function 1
  insertData	DOMCharacterData::InsertData	DontDelete|Function 2
  deleteData	DOMCharacterData::DeleteData	DontDelete|Function 2
  replaceData	DOMCharacterData::ReplaceData	DontDelete|Function 2
@end
*/
DEFINE_PROTOTYPE("DOMCharacterData",DOMCharacterDataProto)
IMPLEMENT_PROTOFUNC(DOMCharacterDataProtoFunc)
IMPLEMENT_PROTOTYPE_WITH_PARENT(DOMCharacterDataProto,DOMCharacterDataProtoFunc, DOMNodeProto)

DOMCharacterData::DOMCharacterData(ExecState *exec, CharacterDataImpl *d)
 : DOMNode(d) 
{
  setPrototype(DOMCharacterDataProto::self(exec));
}

DOMCharacterData::DOMCharacterData(CharacterDataImpl *d)
 : DOMNode(d) 
{
}

bool DOMCharacterData::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMCharacterData, DOMNode>(exec, &DOMCharacterDataTable, this, propertyName, slot);
}

ValueImp *DOMCharacterData::getValueProperty(ExecState *, int token) const
{
  CharacterDataImpl &data = *static_cast<CharacterDataImpl *>(impl());
  switch (token) {
  case Data:
    return String(data.data());
  case Length:
    return Number(data.length());
  default:
    kdWarning() << "Unhandled token in DOMCharacterData::getValueProperty : " << token << endl;
    return NULL;
  }
}

void DOMCharacterData::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
  DOMExceptionTranslator exception(exec);
  if (propertyName == "data")
    static_cast<CharacterDataImpl *>(impl())->setData(value->toString(exec).domString(), exception);
  else
    DOMNode::put(exec, propertyName,value,attr);
}

ValueImp *DOMCharacterDataProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMCharacterData::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  CharacterDataImpl &data = *static_cast<CharacterDataImpl *>(static_cast<DOMCharacterData *>(thisObj)->impl());
  switch(id) {
    case DOMCharacterData::SubstringData: {
      const int count = args[1]->toInt32(exec);
      if (count < 0)
        setDOMException(exec, DOMException::INDEX_SIZE_ERR);
      else
        return getStringOrNull(data.substringData(args[0]->toInt32(exec), count, exception));
    }
    case DOMCharacterData::AppendData:
      data.appendData(args[0]->toString(exec).domString(), exception);
      return Undefined();
    case DOMCharacterData::InsertData:
      data.insertData(args[0]->toInt32(exec), args[1]->toString(exec).domString(), exception);
      return Undefined();
    case DOMCharacterData::DeleteData: {
      const int count = args[1]->toInt32(exec);
      if (count < 0)
        setDOMException(exec, DOMException::INDEX_SIZE_ERR);
      else
        data.deleteData(args[0]->toInt32(exec), count, exception);
      return Undefined();
    }
    case DOMCharacterData::ReplaceData: {
      const int count = args[1]->toInt32(exec);
      if (count < 0)
        setDOMException(exec, DOMException::INDEX_SIZE_ERR);
      else
        data.replaceData(args[0]->toInt32(exec), count, args[2]->toString(exec).domString(), exception);
      return Undefined();
    }
    default:
      return Undefined();
  }
}

// -------------------------------------------------------------------------

const ClassInfo DOMText::info = { "Text",
				 &DOMCharacterData::info, 0, 0 };
/*
@begin DOMTextProtoTable 1
  splitText	DOMText::SplitText	DontDelete|Function 1
@end
*/
DEFINE_PROTOTYPE("DOMText",DOMTextProto)
IMPLEMENT_PROTOFUNC(DOMTextProtoFunc)
IMPLEMENT_PROTOTYPE_WITH_PARENT(DOMTextProto,DOMTextProtoFunc,DOMCharacterDataProto)

DOMText::DOMText(ExecState *exec, TextImpl *t)
  : DOMCharacterData(t) 
{ 
  setPrototype(DOMTextProto::self(exec));
}

ValueImp *DOMTextProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMText::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  TextImpl &text = *static_cast<TextImpl *>(static_cast<DOMText *>(thisObj)->impl());
  switch(id) {
    case DOMText::SplitText:
      return getDOMNode(exec, text.splitText(args[0]->toInt32(exec), exception));
  }
  return Undefined();
}

} // namespace
