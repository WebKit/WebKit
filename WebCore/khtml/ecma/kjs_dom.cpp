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

#include <khtmlview.h>
#include "xml/dom2_eventsimpl.h"
#include "rendering/render_canvas.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include <kdebug.h>
#include <khtml_part.h>

#include "kjs_dom.h"
#include "kjs_html.h"
#include "kjs_css.h"
#include "kjs_range.h"
#include "kjs_traversal.h"
#include "kjs_events.h"
#include "kjs_views.h"
#include "kjs_window.h"
#include "dom/dom_exception.h"
#include "kjs_dom.lut.h"
#include "khtmlpart_p.h"

#include "html_objectimpl.h"

#include "misc/htmltags.h"

#if APPLE_CHANGES
#include <JavaScriptCore/runtime_object.h>
#endif

using namespace KJS;

using DOM::DOMException;
using DOM::DOMString;
using DOM::NodeFilter;

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

DOMNode::DOMNode(ExecState *exec, const DOM::Node &n)
  : DOMObject(DOMNodeProto::self(exec)), node(n)
{
}

DOMNode::DOMNode(const Object &proto, const DOM::Node &n)
  : DOMObject(proto), node(n)
{
}

bool DOMNode::toBoolean(ExecState *) const
{
    return !node.isNull();
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
Value DOMNode::tryGet(ExecState *exec, const Identifier &propertyName) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMNode::tryGet " << propertyName.qstring() << endl;
#endif
  return DOMObjectLookupGetValue<DOMNode, DOMObject>(exec, propertyName, &DOMNodeTable, this);
}

Value DOMNode::getValueProperty(ExecState *exec, int token) const
{
  switch (token) {
  case NodeName:
    return getStringOrNull(node.nodeName());
  case NodeValue:
    return getStringOrNull(node.nodeValue());
  case NodeType:
    return Number((unsigned int)node.nodeType());
  case ParentNode:
    return getDOMNode(exec,node.parentNode());
  case ParentElement: // IE only apparently
    return getDOMNode(exec,node.parentNode());
  case ChildNodes:
    return getDOMNodeList(exec,node.childNodes());
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
    DOM::DocumentImpl* docimpl = node.handle()->getDocument();
    if (docimpl) {
      docimpl->updateLayoutIgnorePendingStylesheets();
    }

    khtml::RenderObject *rend = node.handle()->renderer();

    switch (token) {
    case OffsetLeft:
      return rend ? static_cast<Value>(Number(rend->offsetLeft())) : Value(Undefined());
    case OffsetTop:
      return rend ? static_cast<Value>(Number(rend->offsetTop())) : Value(Undefined());
    case OffsetWidth:
      return rend ? static_cast<Value>(Number(rend->offsetWidth()) ) : Value(Undefined());
    case OffsetHeight:
      return rend ? static_cast<Value>(Number(rend->offsetHeight() ) ) : Value(Undefined());
    case OffsetParent: {
      khtml::RenderObject* par = rend ? rend->offsetParent() : 0;
      return getDOMNode(exec, par ? par->element() : 0);
    }
    case ClientWidth:
      return rend ? static_cast<Value>(Number(rend->clientWidth()) ) : Value(Undefined());
    case ClientHeight:
      return rend ? static_cast<Value>(Number(rend->clientHeight()) ) : Value(Undefined());
    case ScrollWidth:
        return rend ? static_cast<Value>(Number(rend->scrollWidth()) ) : Value(Undefined());
    case ScrollHeight:
        return rend ? static_cast<Value>(Number(rend->scrollHeight()) ) : Value(Undefined());
    case ScrollLeft:
      return Number(rend && rend->layer() ? rend->layer()->scrollXOffset() : 0);
    case ScrollTop:
      return Number(rend && rend->layer() ? rend->layer()->scrollYOffset() : 0);
    default:
      kdWarning() << "Unhandled token in DOMNode::getValueProperty : " << token << endl;
      break;
    }
  }

  return Value();
}

void DOMNode::tryPut(ExecState *exec, const Identifier& propertyName, const Value& value, int attr)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMNode::tryPut " << propertyName.qstring() << endl;
#endif
  DOMObjectLookupPut<DOMNode,DOMObject>(exec, propertyName, value, attr,
                                        &DOMNodeTable, this );
}

void DOMNode::putValue(ExecState *exec, int token, const Value& value, int /*attr*/)
{
  switch (token) {
  case NodeValue:
    node.setNodeValue(value.toString(exec).string());
    break;
  case Prefix:
    node.setPrefix(value.toString(exec).string());
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
#if APPLE_CHANGES
  case OnSearch:
    setListener(exec,DOM::EventImpl::SEARCH_EVENT,value);
    break;
#endif
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
    khtml::RenderObject *rend = node.handle() ? node.handle()->renderer() : 0L;
    if (rend && rend->hasOverflowClip())
        rend->layer()->scrollToYOffset(value.toInt32(exec));
    break;
  }
  case ScrollLeft: {
    khtml::RenderObject *rend = node.handle() ? node.handle()->renderer() : 0L;
    if (rend && rend->hasOverflowClip())
      rend->layer()->scrollToXOffset(value.toInt32(exec));
    break;
  }
  default:
    kdWarning() << "DOMNode::putValue unhandled token " << token << endl;
  }
}

Value DOMNode::toPrimitive(ExecState *exec, Type /*preferred*/) const
{
  if (node.isNull())
    return Null();

  return String(toString(exec));
}

UString DOMNode::toString(ExecState *) const
{
  if (node.isNull())
    return "null";
  UString s;

  DOM::Element e = node;
  if ( !e.isNull() ) {
    s = UString(e.nodeName().string());
  } else
    s = className(); // fallback

  return "[object " + s + "]";
}

void DOMNode::setListener(ExecState *exec, int eventId, Value func) const
{
  node.handle()->setHTMLEventListener(eventId,Window::retrieveActive(exec)->getJSEventListener(func,true));
}

Value DOMNode::getListener(int eventId) const
{
    DOM::EventListener *listener = node.handle()->getHTMLEventListener(eventId);
    JSEventListener *jsListener = static_cast<JSEventListener*>(listener);
    if ( jsListener && jsListener->listenerObjImp() )
	return jsListener->listenerObj();
    else
	return Null();
}

void DOMNode::pushEventHandlerScope(ExecState *, ScopeChain &) const
{
}

Value DOMNodeProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&DOMNode::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOM::Node node = static_cast<DOMNode *>( thisObj.imp() )->toNode();
  switch (id) {
    case DOMNode::HasAttributes:
      return Boolean(node.hasAttributes());
    case DOMNode::HasChildNodes:
      return Boolean(node.hasChildNodes());
    case DOMNode::CloneNode:
      return getDOMNode(exec,node.cloneNode(args[0].toBoolean(exec)));
    case DOMNode::Normalize:
      node.normalize();
      return Undefined();
    case DOMNode::IsSupported:
        return Boolean(node.isSupported(args[0].toString(exec).string(),
            (args[1].type() != UndefinedType && args[1].type() != NullType) ? args[1].toString(exec).string() : DOMString()));
    case DOMNode::AddEventListener: {
        JSEventListener *listener = Window::retrieveActive(exec)->getJSEventListener(args[1]);
        if (listener)
            node.addEventListener(args[0].toString(exec).string(),listener,args[2].toBoolean(exec));
        return Undefined();
    }
    case DOMNode::RemoveEventListener: {
        JSEventListener *listener = Window::retrieveActive(exec)->getJSEventListener(args[1]);
        if (listener)
            node.removeEventListener(args[0].toString(exec).string(),listener,args[2].toBoolean(exec));
        return Undefined();
    }
    case DOMNode::DispatchEvent:
      return Boolean(node.dispatchEvent(toEvent(args[0])));
    case DOMNode::AppendChild:
      return getDOMNode(exec,node.appendChild(toNode(args[0])));
    case DOMNode::RemoveChild:
      return getDOMNode(exec,node.removeChild(toNode(args[0])));
    case DOMNode::InsertBefore:
      return getDOMNode(exec,node.insertBefore(toNode(args[0]), toNode(args[1])));
    case DOMNode::ReplaceChild:
      return getDOMNode(exec,node.replaceChild(toNode(args[0]), toNode(args[1])));
    case DOMNode::Contains:
    {
        int exceptioncode=0;
	DOM::Node other = toNode(args[0]);
	if (!other.isNull() && node.nodeType()==DOM::Node::ELEMENT_NODE)
	{
	    DOM::NodeBaseImpl *impl = static_cast<DOM::NodeBaseImpl *>(node.handle());
	    bool retval = !impl->checkNoOwner(other.handle(),exceptioncode);
	    return Boolean(retval && exceptioncode == 0);
	}
        return Undefined();
    }
    case DOMNode::Item:
      return getDOMNode(exec, node.childNodes().item(static_cast<unsigned long>(args[0].toNumber(exec))));
  }

  return Undefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMNodeList::info = { "NodeList", 0, 0, 0 };

DOMNodeList::~DOMNodeList()
{
  ScriptInterpreter::forgetDOMObject(list.handle());
}

Value DOMNodeList::toPrimitive(ExecState *exec, Type /*preferred*/) const
{
  if (list.isNull())
    return Null();

  return String(toString(exec));
}



// We have to implement hasProperty since we don't use a hashtable for 'length' and 'item'
// ## this breaks "for (..in..)" though.
bool DOMNodeList::hasProperty(ExecState *exec, const Identifier &p) const
{
  if (p == lengthPropertyName || p == "item")
    return true;
  return ObjectImp::hasProperty(exec, p);
}

Value DOMNodeList::tryGet(ExecState *exec, const Identifier &p) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMNodeList::tryGet " << p.ascii() << endl;
#endif
  Value result;

  if (p == lengthPropertyName)
    result = Number(list.length());
  else if (p == "item") {
    // No need for a complete hashtable for a single func, but we still want
    // to use the caching feature of lookupOrCreateFunction.
    result = lookupOrCreateFunction<DOMNodeListFunc>(exec, p, this, DOMNodeListFunc::Item, 1, DontDelete|Function);
    //result = new DOMNodeListFunc(exec, DOMNodeListFunc::Item, 1);
  }
  else {
    // array index ?
    bool ok;
    long unsigned int idx = p.toULong(&ok);
    if (ok) {
      result = getDOMNode(exec,list.item(idx));
    } else {
      DOM::Node node = list.itemById(p.string());

      if (!node.isNull()) {
        result = getDOMNode(exec, node);
      } else {
        result = ObjectImp::get(exec, p);
      }
    }
  }

  return result;
}

// Need to support both get and call, so that list[0] and list(0) work.
Value DOMNodeList::call(ExecState *exec, Object &thisObj, const List &args)
{
  // This code duplication is necessary, DOMNodeList isn't a DOMFunction
  Value val;
  try {
    val = tryCall(exec, thisObj, args);
  }
  // pity there's no way to distinguish between these in JS code
  catch (...) {
    Object err = Error::create(exec, GeneralError, "Exception from DOMNodeList");
    exec->setException(err);
  }
  return val;
}

Value DOMNodeList::tryCall(ExecState *exec, Object &, const List &args)
{
  // Do not use thisObj here. See HTMLCollection.
  UString s = args[0].toString(exec);
  bool ok;
  unsigned int u = s.toULong(&ok);
  if (ok)
    return getDOMNode(exec,list.item(u));

  kdWarning() << "KJS::DOMNodeList::tryCall " << s.qstring() << " not implemented" << endl;
  return Undefined();
}

DOMNodeListFunc::DOMNodeListFunc(ExecState *exec, int i, int len)
  : DOMFunction(), id(i)
{
  Value protect(this);
  put(exec,lengthPropertyName,Number(len),DontDelete|ReadOnly|DontEnum);
}

// Not a prototype class currently, but should probably be converted to one
Value DOMNodeListFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMNodeList::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOM::NodeList list = static_cast<DOMNodeList *>(thisObj.imp())->nodeList();
  Value result;

  if (id == Item)
    result = getDOMNode(exec, list.item(args[0].toInt32(exec)));
  return result;
}

// -------------------------------------------------------------------------

const ClassInfo DOMAttr::info = { "Attr", &DOMNode::info, &DOMAttrTable, 0 };

/* Source for DOMAttrTable. Use "make hashtables" to regenerate.
@begin DOMAttrTable 5
  name		DOMAttr::Name		DontDelete|ReadOnly
  specified	DOMAttr::Specified	DontDelete|ReadOnly
  value		DOMAttr::ValueProperty	DontDelete|ReadOnly
  ownerElement	DOMAttr::OwnerElement	DontDelete|ReadOnly
@end
*/
Value DOMAttr::tryGet(ExecState *exec, const Identifier &propertyName) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMAttr::tryPut " << propertyName.qstring() << endl;
#endif
  return DOMObjectLookupGetValue<DOMAttr,DOMNode>(exec, propertyName,
                                                  &DOMAttrTable, this );
}

Value DOMAttr::getValueProperty(ExecState *exec, int token) const
{
  switch (token) {
  case Name:
    return getStringOrNull(static_cast<DOM::Attr>(node).name());
  case Specified:
    return Boolean(static_cast<DOM::Attr>(node).specified());
  case ValueProperty:
    return getStringOrNull(static_cast<DOM::Attr>(node).value());
  case OwnerElement: // DOM2
    return getDOMNode(exec,static_cast<DOM::Attr>(node).ownerElement());
  }
  return Value(); // not reached
}

void DOMAttr::tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMAttr::tryPut " << propertyName.qstring() << endl;
#endif
  DOMObjectLookupPut<DOMAttr,DOMNode>(exec, propertyName, value, attr,
                                      &DOMAttrTable, this );
}

void DOMAttr::putValue(ExecState *exec, int token, const Value& value, int /*attr*/)
{
  switch (token) {
  case ValueProperty:
    static_cast<DOM::Attr>(node).setValue(value.toString(exec).string());
    return;
  default:
    kdWarning() << "DOMAttr::putValue unhandled token " << token << endl;
  }
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

DOMDocument::DOMDocument(ExecState *exec, const DOM::Document &d)
  : DOMNode(DOMDocumentProto::self(exec), d) { }

DOMDocument::DOMDocument(const Object &proto, const DOM::Document &d)
  : DOMNode(proto, d) { }

DOMDocument::~DOMDocument()
{
  ScriptInterpreter::forgetDOMObject(node.handle());
}

Value DOMDocument::tryGet(ExecState *exec, const Identifier &propertyName) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMDocument::tryGet " << propertyName.qstring() << endl;
#endif
  return DOMObjectLookupGetValue<DOMDocument, DOMNode>(
    exec, propertyName, &DOMDocumentTable, this);
}

Value DOMDocument::getValueProperty(ExecState *exec, int token) const
{
  DOM::Document doc = static_cast<DOM::Document>(node);

  switch(token) {
  case DocType:
    return getDOMNode(exec,doc.doctype());
  case Implementation:
    return getDOMDOMImplementation(exec,doc.implementation());
  case DocumentElement:
    return getDOMNode(exec,doc.documentElement());
  case StyleSheets:
    //kdDebug() << "DOMDocument::StyleSheets, returning " << doc.styleSheets().length() << " stylesheets" << endl;
    return getDOMStyleSheetList(exec, doc.styleSheets(), doc);
  case PreferredStylesheetSet:
    return getStringOrNull(doc.preferredStylesheetSet());
  case SelectedStylesheetSet:
    return getStringOrNull(doc.selectedStylesheetSet());
  case ReadyState:
    {
    DOM::DocumentImpl* docimpl = node.handle()->getDocument();
    if ( docimpl )
    {
      KHTMLPart* part = docimpl->part();
      if ( part ) {
        if (part->d->m_bComplete) return String("complete");
        if (docimpl->parsing()) return String("loading");
        return String("loaded");
        // What does the interactive value mean ?
        // Missing support for "uninitialized"
      }
    }
    return Undefined();
    }
  case DOMDocument::DefaultView: // DOM2
    return getDOMAbstractView(exec,doc.defaultView());
  default:
    kdWarning() << "DOMDocument::getValueProperty unhandled token " << token << endl;
    return Value();
  }
}

void DOMDocument::tryPut(ExecState *exec, const Identifier& propertyName, const Value& value, int attr)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMDocument::tryPut " << propertyName.qstring() << endl;
#endif
  DOMObjectLookupPut<DOMDocument,DOMNode>(exec, propertyName, value, attr, &DOMDocumentTable, this );
}

void DOMDocument::putValue(ExecState *exec, int token, const Value& value, int /*attr*/)
{
  DOM::Document doc = static_cast<DOM::Document>(node);
  switch (token) {
    case SelectedStylesheetSet: {
      doc.setSelectedStylesheetSet(value.toString(exec).string());
      break;
    }
  }
}

Value DOMDocumentProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMNode::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOM::Node node = static_cast<DOMNode *>( thisObj.imp() )->toNode();
  DOM::Document doc = static_cast<DOM::Document>(node);
  String str = args[0].toString(exec);
  DOM::DOMString s = str.value().string();

  switch(id) {
  case DOMDocument::CreateElement:
    return getDOMNode(exec,doc.createElement(s));
  case DOMDocument::CreateDocumentFragment:
    return getDOMNode(exec,doc.createDocumentFragment());
  case DOMDocument::CreateTextNode:
    return getDOMNode(exec,doc.createTextNode(s));
  case DOMDocument::CreateComment:
    return getDOMNode(exec,doc.createComment(s));
  case DOMDocument::CreateCDATASection:
    return getDOMNode(exec,doc.createCDATASection(s));  /* TODO: okay ? */
  case DOMDocument::CreateProcessingInstruction:
    return getDOMNode(exec,doc.createProcessingInstruction(args[0].toString(exec).string(),
                                                                 args[1].toString(exec).string()));
  case DOMDocument::CreateAttribute:
    return getDOMNode(exec,doc.createAttribute(s));
  case DOMDocument::CreateEntityReference:
    return getDOMNode(exec,doc.createEntityReference(args[0].toString(exec).string()));
  case DOMDocument::ElementFromPoint:
    return getDOMNode(exec,doc.elementFromPoint((int)args[0].toNumber(exec), (int)args[1].toNumber(exec)));
  case DOMDocument::GetElementsByTagName:
    return getDOMNodeList(exec,doc.getElementsByTagName(s));
  case DOMDocument::ImportNode: // DOM2
    return getDOMNode(exec,doc.importNode(toNode(args[0]), args[1].toBoolean(exec)));
  case DOMDocument::CreateElementNS: // DOM2
    return getDOMNode(exec,doc.createElementNS(args[0].toString(exec).string(), args[1].toString(exec).string()));
  case DOMDocument::CreateAttributeNS: // DOM2
    return getDOMNode(exec,doc.createAttributeNS(args[0].toString(exec).string(),args[1].toString(exec).string()));
  case DOMDocument::GetElementsByTagNameNS: // DOM2
    return getDOMNodeList(exec,doc.getElementsByTagNameNS(args[0].toString(exec).string(),
                                                          args[1].toString(exec).string()));
  case DOMDocument::GetElementById:
    return getDOMNode(exec,doc.getElementById(args[0].toString(exec).string()));
  case DOMDocument::CreateRange:
    return getDOMRange(exec,doc.createRange());
  case DOMDocument::CreateNodeIterator: {
    NodeFilter filter;
    if (!args[2].isA(NullType)) {
        Object obj = Object::dynamicCast(args[2]);
        if (!obj.isNull())
            filter = NodeFilter(new JSNodeFilterCondition(obj));
    }
    return getDOMNodeIterator(exec, doc.createNodeIterator(toNode(args[0]), (long unsigned int)(args[1].toNumber(exec)), filter, args[3].toBoolean(exec)));
  }
  case DOMDocument::CreateTreeWalker: {
    NodeFilter filter;
    if (!args[2].isA(NullType)) {
        Object obj = Object::dynamicCast(args[2]);
        if (!obj.isNull())
            filter = NodeFilter(new JSNodeFilterCondition(obj));
    }
    return getDOMTreeWalker(exec, doc.createTreeWalker(toNode(args[0]), (long unsigned int)(args[1].toNumber(exec)), filter, args[3].toBoolean(exec)));
  }
  case DOMDocument::CreateEvent:
    return getDOMEvent(exec,doc.createEvent(s));
  case DOMDocument::GetOverrideStyle: {
    DOM::Node arg0 = toNode(args[0]);
    if (arg0.nodeType() != DOM::Node::ELEMENT_NODE)
      return Undefined(); // throw exception?
    else
      return getDOMCSSStyleDeclaration(exec,doc.getOverrideStyle(static_cast<DOM::Element>(arg0),args[1].toString(exec).string()));
  }
  case DOMDocument::ExecCommand: {
    return Boolean(doc.execCommand(args[0].toString(exec).string(), args[1].toBoolean(exec), args[2].toString(exec).string()));
  }
  case DOMDocument::QueryCommandEnabled: {
    return Boolean(doc.queryCommandEnabled(args[0].toString(exec).string()));
  }
  case DOMDocument::QueryCommandIndeterm: {
    return Boolean(doc.queryCommandIndeterm(args[0].toString(exec).string()));
  }
  case DOMDocument::QueryCommandState: {
    return Boolean(doc.queryCommandState(args[0].toString(exec).string()));
  }
  case DOMDocument::QueryCommandSupported: {
    return Boolean(doc.queryCommandSupported(args[0].toString(exec).string()));
  }
  case DOMDocument::QueryCommandValue: {
    DOM::DOMString commandValue(doc.queryCommandValue(args[0].toString(exec).string()));
    // Method returns null DOMString to signal command is unsupported.
    // Micorsoft documentation for this method says:
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
DOMElement::DOMElement(ExecState *exec, const DOM::Element &e)
  : DOMNode(DOMElementProto::self(exec), e) { }

DOMElement::DOMElement(const Object &proto, const DOM::Element &e)
  : DOMNode(proto, e) { }

Value DOMElement::tryGet(ExecState *exec, const Identifier &propertyName) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMElement::tryGet " << propertyName.qstring() << endl;
#endif
  DOM::Element element = static_cast<DOM::Element>(node);

  const HashEntry* entry = Lookup::findEntry(&DOMElementTable, propertyName);
  if (entry)
  {
    switch( entry->value ) {
    case TagName:
      return getStringOrNull(element.tagName());
    case Style:
      return getDOMCSSStyleDeclaration(exec,element.style());
    default:
      kdWarning() << "Unhandled token in DOMElement::tryGet : " << entry->value << endl;
      break;
    }
  }
  // We have to check in DOMNode before giving access to attributes, otherwise
  // onload="..." would make onload return the string (attribute value) instead of
  // the listener object (function).
  if (DOMNode::hasProperty(exec, propertyName))
    return DOMNode::tryGet(exec, propertyName);

  DOM::DOMString attr = element.getAttribute( propertyName.string() );
  // Give access to attributes
  if ( !attr.isNull() )
    return getStringOrNull( attr );

  return Undefined();
}

Value DOMElementProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMNode::info)) { // node should be enough here, given the cast
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOM::Node node = static_cast<DOMNode *>( thisObj.imp() )->toNode();
  DOM::Element element = static_cast<DOM::Element>(node);

  switch(id) {
    case DOMElement::GetAttribute:
      // getString should be used here, since if the attribute isn't present at all, you should
      // return null and not "".
      return getStringOrNull(element.getAttribute(args[0].toString(exec).string()));
    case DOMElement::SetAttribute:
      element.setAttribute(args[0].toString(exec).string(),args[1].toString(exec).string());
      return Undefined();
    case DOMElement::RemoveAttribute:
      element.removeAttribute(args[0].toString(exec).string());
      return Undefined();
    case DOMElement::GetAttributeNode:
      return getDOMNode(exec,element.getAttributeNode(args[0].toString(exec).string()));
    case DOMElement::SetAttributeNode:
      return getDOMNode(exec,element.setAttributeNode((new DOMNode(exec,KJS::toNode(args[0])))->toNode()));
    case DOMElement::RemoveAttributeNode:
      return getDOMNode(exec,element.removeAttributeNode((new DOMNode(exec,KJS::toNode(args[0])))->toNode()));
    case DOMElement::GetElementsByTagName:
      return getDOMNodeList(exec,element.getElementsByTagName(args[0].toString(exec).string()));
    case DOMElement::HasAttribute: // DOM2
      return Boolean(element.hasAttribute(args[0].toString(exec).string()));
    case DOMElement::GetAttributeNS: // DOM2
      return String(element.getAttributeNS(args[0].toString(exec).string(),args[1].toString(exec).string()));
    case DOMElement::SetAttributeNS: // DOM2
      element.setAttributeNS(args[0].toString(exec).string(),args[1].toString(exec).string(),args[2].toString(exec).string());
      return Undefined();
    case DOMElement::RemoveAttributeNS: // DOM2
      element.removeAttributeNS(args[0].toString(exec).string(),args[1].toString(exec).string());
      return Undefined();
    case DOMElement::GetAttributeNodeNS: // DOM2
      return getDOMNode(exec,element.getAttributeNodeNS(args[0].toString(exec).string(),args[1].toString(exec).string()));
    case DOMElement::SetAttributeNodeNS: // DOM2
      return getDOMNode(exec,element.setAttributeNodeNS((new DOMNode(exec,KJS::toNode(args[0])))->toNode()));
    case DOMElement::GetElementsByTagNameNS: // DOM2
      return getDOMNodeList(exec,element.getElementsByTagNameNS(args[0].toString(exec).string(),args[1].toString(exec).string()));
    case DOMElement::HasAttributeNS: // DOM2
      return Boolean(element.hasAttributeNS(args[0].toString(exec).string(),args[1].toString(exec).string()));
    case DOMElement::ScrollByLines:
    case DOMElement::ScrollByPages:
    {
        DOM::DocumentImpl* docimpl = node.handle()->getDocument();
        if (docimpl) {
            docimpl->updateLayoutIgnorePendingStylesheets();
        }            
        khtml::RenderObject *rend = node.handle() ? node.handle()->renderer() : 0L;
        if (rend && rend->hasOverflowClip()) {
            KWQScrollDirection direction = KWQScrollDown;
            int multiplier = args[0].toInt32(exec);
            if (multiplier < 0) {
                direction = KWQScrollUp;
                multiplier = -multiplier;
            }
            KWQScrollGranularity granularity = id == DOMElement::ScrollByLines ? KWQScrollLine : KWQScrollPage;
            rend->layer()->scroll(direction, granularity, multiplier);
        }
        return Undefined();
        
    }
  default:
    return Undefined();
  }
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

DOMDOMImplementation::DOMDOMImplementation(ExecState *exec, const DOM::DOMImplementation &i)
  : DOMObject(DOMDOMImplementationProto::self(exec)), implementation(i) { }

DOMDOMImplementation::~DOMDOMImplementation()
{
  ScriptInterpreter::forgetDOMObject(implementation.handle());
}

Value DOMDOMImplementationProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMDOMImplementation::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOM::DOMImplementation implementation = static_cast<DOMDOMImplementation *>( thisObj.imp() )->toImplementation();

  switch(id) {
  case DOMDOMImplementation::HasFeature:
    return Boolean(implementation.hasFeature(args[0].toString(exec).string(),
        (args[1].type() != UndefinedType && args[1].type() != NullType) ? args[1].toString(exec).string() : DOMString()));
  case DOMDOMImplementation::CreateDocumentType: // DOM2
    return getDOMNode(exec,implementation.createDocumentType(args[0].toString(exec).string(),args[1].toString(exec).string(),args[2].toString(exec).string()));
  case DOMDOMImplementation::CreateDocument: // DOM2
    return getDOMNode(exec,implementation.createDocument(args[0].toString(exec).string(),args[1].toString(exec).string(),toNode(args[2])));
  case DOMDOMImplementation::CreateCSSStyleSheet: // DOM2
    return getDOMStyleSheet(exec,implementation.createCSSStyleSheet(args[0].toString(exec).string(),args[1].toString(exec).string()));
  case DOMDOMImplementation::CreateHTMLDocument: // DOM2-HTML
    return getDOMNode(exec, implementation.createHTMLDocument(args[0].toString(exec).string()));
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
DOMDocumentType::DOMDocumentType(ExecState *exec, const DOM::DocumentType &dt)
  : DOMNode( /*### no proto yet*/exec, dt ) { }

Value DOMDocumentType::tryGet(ExecState *exec, const Identifier &propertyName) const
{
  return DOMObjectLookupGetValue<DOMDocumentType, DOMNode>(exec, propertyName, &DOMDocumentTypeTable, this);
}

Value DOMDocumentType::getValueProperty(ExecState *exec, int token) const
{
  DOM::DocumentType type = static_cast<DOM::DocumentType>(node);
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
    return Value();
  }
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

DOMNamedNodeMap::DOMNamedNodeMap(ExecState *exec, const DOM::NamedNodeMap &m)
  : DOMObject(DOMNamedNodeMapProto::self(exec)), map(m) { }

DOMNamedNodeMap::~DOMNamedNodeMap()
{
  ScriptInterpreter::forgetDOMObject(map.handle());
}

// We have to implement hasProperty since we don't use a hashtable for 'length'
// ## this breaks "for (..in..)" though.
bool DOMNamedNodeMap::hasProperty(ExecState *exec, const Identifier &p) const
{
  if (p == lengthPropertyName)
    return true;
  return DOMObject::hasProperty(exec, p);
}

Value DOMNamedNodeMap::tryGet(ExecState* exec, const Identifier &p) const
{
  if (p == lengthPropertyName)
    return Number(map.length());

  // array index ?
  bool ok;
  long unsigned int idx = p.toULong(&ok);
  if (ok)
    return getDOMNode(exec,map.item(idx));

  // Anything else (including functions, defined in the prototype)
  return DOMObject::tryGet(exec, p);
}

Value DOMNamedNodeMapProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMNamedNodeMap::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOM::NamedNodeMap map = static_cast<DOMNamedNodeMap *>(thisObj.imp())->toMap();

  switch(id) {
    case DOMNamedNodeMap::GetNamedItem:
      return getDOMNode(exec, map.getNamedItem(args[0].toString(exec).string()));
    case DOMNamedNodeMap::SetNamedItem:
      return getDOMNode(exec, map.setNamedItem((new DOMNode(exec,KJS::toNode(args[0])))->toNode()));
    case DOMNamedNodeMap::RemoveNamedItem:
      return getDOMNode(exec, map.removeNamedItem(args[0].toString(exec).string()));
    case DOMNamedNodeMap::Item:
      return getDOMNode(exec, map.item(args[0].toInt32(exec)));
    case DOMNamedNodeMap::GetNamedItemNS: // DOM2
      return getDOMNode(exec, map.getNamedItemNS(args[0].toString(exec).string(),args[1].toString(exec).string()));
    case DOMNamedNodeMap::SetNamedItemNS: // DOM2
      return getDOMNode(exec, map.setNamedItemNS(toNode(args[0])));
    case DOMNamedNodeMap::RemoveNamedItemNS: // DOM2
      return getDOMNode(exec, map.removeNamedItemNS(args[0].toString(exec).string(),args[1].toString(exec).string()));
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
Value DOMProcessingInstruction::tryGet(ExecState *exec, const Identifier &propertyName) const
{
  return DOMObjectLookupGetValue<DOMProcessingInstruction, DOMNode>(exec, propertyName, &DOMProcessingInstructionTable, this);
}

Value DOMProcessingInstruction::getValueProperty(ExecState *exec, int token) const
{
  switch (token) {
  case Target:
    return getStringOrNull(static_cast<DOM::ProcessingInstruction>(node).target());
  case Data:
    return getStringOrNull(static_cast<DOM::ProcessingInstruction>(node).data());
  case Sheet:
    return getDOMStyleSheet(exec,static_cast<DOM::ProcessingInstruction>(node).sheet());
  default:
    kdWarning() << "DOMProcessingInstruction::getValueProperty unhandled token " << token << endl;
    return Value();
  }
}

void DOMProcessingInstruction::tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr)
{
  // Not worth using the hashtable for this one ;)
  if (propertyName == "data")
    static_cast<DOM::ProcessingInstruction>(node).setData(value.toString(exec).string());
  else
    DOMNode::tryPut(exec, propertyName,value,attr);
}

// -------------------------------------------------------------------------

const ClassInfo DOMNotation::info = { "Notation", &DOMNode::info, &DOMNotationTable, 0 };

/* Source for DOMNotationTable. Use "make hashtables" to regenerate.
@begin DOMNotationTable 2
  publicId		DOMNotation::PublicId	DontDelete|ReadOnly
  systemId		DOMNotation::SystemId	DontDelete|ReadOnly
@end
*/
Value DOMNotation::tryGet(ExecState *exec, const Identifier &propertyName) const
{
  return DOMObjectLookupGetValue<DOMNotation, DOMNode>(exec, propertyName, &DOMNotationTable, this);
}

Value DOMNotation::getValueProperty(ExecState *, int token) const
{
  switch (token) {
  case PublicId:
    return getStringOrNull(static_cast<DOM::Notation>(node).publicId());
  case SystemId:
    return getStringOrNull(static_cast<DOM::Notation>(node).systemId());
  default:
    kdWarning() << "DOMNotation::getValueProperty unhandled token " << token << endl;
    return Value();
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
Value DOMEntity::tryGet(ExecState *exec, const Identifier &propertyName) const
{
  return DOMObjectLookupGetValue<DOMEntity, DOMNode>(exec, propertyName, &DOMEntityTable, this);
}

Value DOMEntity::getValueProperty(ExecState *, int token) const
{
  switch (token) {
  case PublicId:
    return getStringOrNull(static_cast<DOM::Entity>(node).publicId());
  case SystemId:
    return getStringOrNull(static_cast<DOM::Entity>(node).systemId());
  case NotationName:
    return getStringOrNull(static_cast<DOM::Entity>(node).notationName());
  default:
    kdWarning() << "DOMEntity::getValueProperty unhandled token " << token << endl;
    return Value();
  }
}

// -------------------------------------------------------------------------

Value KJS::getDOMDocumentNode(ExecState *exec, const DOM::Document &n)
{
  DOMDocument *ret = 0;
  ScriptInterpreter* interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());

  if ((ret = static_cast<DOMDocument *>(interp->getDOMObject(n.handle()))))
    return Value(ret);

  if (n.isHTMLDocument())
    ret = new HTMLDocument(exec, static_cast<DOM::HTMLDocument>(n));
  else
    ret = new DOMDocument(exec, n);

  Value val(ret);
  
  // Make sure the document is kept around by the window object, and works right with the
  // back/forward cache.
  if (n.view()) {
    static Identifier documentIdentifier("document");
    Window::retrieveWindow(n.view()->part())->putDirect(documentIdentifier, ret, DontDelete|ReadOnly);
  }

  interp->putDOMObject(n.handle(), ret);

  return val;
}

bool KJS::checkNodeSecurity(ExecState *exec, const DOM::Node& n)
{
  if (!n.handle()) 
    return false;

  // Check to see if the currently executing interpreter is allowed to access the specified node
  KHTMLPart *part = n.handle()->getDocument()->part();
  Window* win = part ? Window::retrieveWindow(part) : 0L;
  if ( !win || !win->isSafeScript(exec) )
    return false;
  return true;
}


Value KJS::getDOMNode(ExecState *exec, const DOM::Node &n)
{
  DOMObject *ret = 0;
  if (n.isNull())
    return Null();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());
  DOM::NodeImpl *doc = n.ownerDocument().handle();

  if ((ret = interp->getDOMObjectForDocument(static_cast<DOM::DocumentImpl *>(doc), n.handle())))
    return Value(ret);

  switch (n.nodeType()) {
    case DOM::Node::ELEMENT_NODE:
      if (static_cast<DOM::Element>(n).isHTMLElement())
        ret = new HTMLElement(exec, static_cast<DOM::HTMLElement>(n));
      else
        ret = new DOMElement(exec, static_cast<DOM::Element>(n));
      break;
    case DOM::Node::ATTRIBUTE_NODE:
      ret = new DOMAttr(exec, static_cast<DOM::Attr>(n));
      break;
    case DOM::Node::TEXT_NODE:
    case DOM::Node::CDATA_SECTION_NODE:
      ret = new DOMText(exec, static_cast<DOM::Text>(n));
      break;
    case DOM::Node::ENTITY_REFERENCE_NODE:
      ret = new DOMNode(exec, n);
      break;
    case DOM::Node::ENTITY_NODE:
      ret = new DOMEntity(exec, static_cast<DOM::Entity>(n));
      break;
    case DOM::Node::PROCESSING_INSTRUCTION_NODE:
      ret = new DOMProcessingInstruction(exec, static_cast<DOM::ProcessingInstruction>(n));
      break;
    case DOM::Node::COMMENT_NODE:
      ret = new DOMCharacterData(exec, static_cast<DOM::CharacterData>(n));
      break;
    case DOM::Node::DOCUMENT_NODE:
      // we don't want to cache the document itself in the per-document dictionary
      return getDOMDocumentNode(exec, static_cast<DOM::Document>(n));
    case DOM::Node::DOCUMENT_TYPE_NODE:
      ret = new DOMDocumentType(exec, static_cast<DOM::DocumentType>(n));
      break;
    case DOM::Node::DOCUMENT_FRAGMENT_NODE:
      ret = new DOMNode(exec, n);
      break;
    case DOM::Node::NOTATION_NODE:
      ret = new DOMNotation(exec, static_cast<DOM::Notation>(n));
      break;
    default:
      ret = new DOMNode(exec, n);
  }

  interp->putDOMObjectForDocument(static_cast<DOM::DocumentImpl *>(doc), n.handle(), ret);

  return Value(ret);
}

Value KJS::getDOMNamedNodeMap(ExecState *exec, const DOM::NamedNodeMap &m)
{
  return Value(cacheDOMObject<DOM::NamedNodeMap, KJS::DOMNamedNodeMap>(exec, m));
}

Value KJS::getRuntimeObject(ExecState *exec, const DOM::Node &node)
{
    DOM::HTMLElement element = static_cast<DOM::HTMLElement>(node);

    if (!node.isNull()) {
        if (node.handle()->id() == ID_APPLET) {
            DOM::HTMLAppletElementImpl *appletElement = static_cast<DOM::HTMLAppletElementImpl *>(element.handle());
            
            if (appletElement->getAppletInstance()) {
                // The instance is owned by the applet element.
                RuntimeObjectImp *appletImp = new RuntimeObjectImp(appletElement->getAppletInstance(), false);
                return Value(appletImp);
            }
        }
        else if (node.handle()->id() == ID_EMBED) {
            DOM::HTMLEmbedElementImpl *embedElement = static_cast<DOM::HTMLEmbedElementImpl *>(element.handle());
            
            if (embedElement->getEmbedInstance()) {
                RuntimeObjectImp *runtimeImp = new RuntimeObjectImp(embedElement->getEmbedInstance(), false);
                return Value(runtimeImp);
            }
        }
        else if (node.handle()->id() == ID_OBJECT) {
            DOM::HTMLObjectElementImpl *objectElement = static_cast<DOM::HTMLObjectElementImpl *>(element.handle());
            
            if (objectElement->getObjectInstance()) {
                RuntimeObjectImp *runtimeImp = new RuntimeObjectImp(objectElement->getObjectInstance(), false);
                return Value(runtimeImp);
            }
        }
    }
    
    // If we don't have a runtime object return the a Value that reports isNull() == true.
    return Value();
}

Value KJS::getDOMNodeList(ExecState *exec, const DOM::NodeList &l)
{
  return Value(cacheDOMObject<DOM::NodeList, KJS::DOMNodeList>(exec, l));
}

Value KJS::getDOMDOMImplementation(ExecState *exec, const DOM::DOMImplementation &i)
{
  return Value(cacheDOMObject<DOM::DOMImplementation, KJS::DOMDOMImplementation>(exec, i));
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
Value NodeConstructor::tryGet(ExecState *exec, const Identifier &propertyName) const
{
  return DOMObjectLookupGetValue<NodeConstructor, DOMObject>(exec, propertyName, &NodeConstructorTable, this);
}

Value NodeConstructor::getValueProperty(ExecState *, int token) const
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
    return Value();
  }
#endif
}

Object KJS::getNodeConstructor(ExecState *exec)
{
  return Object(cacheGlobalObject<NodeConstructor>(exec, "[[node.constructor]]"));
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

Value DOMExceptionConstructor::tryGet(ExecState *exec, const Identifier &propertyName) const
{
  return DOMObjectLookupGetValue<DOMExceptionConstructor, DOMObject>(exec, propertyName, &DOMExceptionConstructorTable, this);
}

Value DOMExceptionConstructor::getValueProperty(ExecState *, int token) const
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
    return Value();
  }
#endif
}

Object KJS::getDOMExceptionConstructor(ExecState *exec)
{
  return cacheGlobalObject<DOMExceptionConstructor>(exec, "[[DOMException.constructor]]");
}

// -------------------------------------------------------------------------

// Such a collection is usually very short-lived, it only exists
// for constructs like document.forms.<name>[1],
// so it shouldn't be a problem that it's storing all the nodes (with the same name). (David)
DOMNamedNodesCollection::DOMNamedNodesCollection(ExecState *, const QValueList<DOM::Node>& nodes )
  : DOMObject(), m_nodes(nodes)
{
  // Maybe we should ref (and deref in the dtor) the nodes, though ?
}

Value DOMNamedNodesCollection::tryGet(ExecState *exec, const Identifier &propertyName) const
{
  if (propertyName == lengthPropertyName)
    return Number(m_nodes.count());
  // index?
  bool ok;
  unsigned int u = propertyName.toULong(&ok);
  if (ok && u < m_nodes.count()) {
    DOM::Node node = m_nodes[u];
    return getDOMNode(exec,node);
  }
  // For IE compatibility, we need to be able to look up elements in a
  // document.formName.name result by id as well as be index.

  if (!ok) {
    for (QValueListConstIterator<DOM::Node> it = m_nodes.begin(); it != m_nodes.end(); it++) {
      DOM::Node node = *it;
      DOM::NamedNodeMap attributes = node.attributes();
      if (attributes.isNull()) {
	continue;
      }

      DOM::Node idAttr = attributes.getNamedItem("id");
      if (idAttr.isNull()) {
	continue;
      }

      if (idAttr.nodeValue() == propertyName.string()) {
	return getDOMNode(exec,node);
      }
    }
  }

  return DOMObject::tryGet(exec,propertyName);
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

DOMCharacterData::DOMCharacterData(ExecState *exec, const DOM::CharacterData &d)
 : DOMNode(DOMCharacterDataProto::self(exec), d) {}

DOMCharacterData::DOMCharacterData(const Object &proto, const DOM::CharacterData &d)
 : DOMNode(proto, d) {}

Value DOMCharacterData::tryGet(ExecState *exec, const Identifier &p) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070)<<"DOMCharacterData::tryGet "<<p.string().string()<<endl;
#endif
  return DOMObjectLookupGetValue<DOMCharacterData,DOMNode>(exec,p,&DOMCharacterDataTable,this);
}

Value DOMCharacterData::getValueProperty(ExecState *, int token) const
{
  DOM::CharacterData data = static_cast<DOM::CharacterData>(node);
  switch (token) {
  case Data:
    return String(data.data());
  case Length:
    return Number(data.length());
 default:
   kdWarning() << "Unhandled token in DOMCharacterData::getValueProperty : " << token << endl;
   return Value();
  }
}

void DOMCharacterData::tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr)
{
  if (propertyName == "data")
    static_cast<DOM::CharacterData>(node).setData(value.toString(exec).string());
  else
    DOMNode::tryPut(exec, propertyName,value,attr);
}

Value DOMCharacterDataProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMCharacterData::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOM::CharacterData data = static_cast<DOMCharacterData *>(thisObj.imp())->toData();
  switch(id) {
    case DOMCharacterData::SubstringData: {
      const int count = args[1].toInt32(exec);
      if (count < 0)
        throw DOMException(DOMException::INDEX_SIZE_ERR);
      return getStringOrNull(data.substringData(args[0].toInt32(exec), count));
    }
    case DOMCharacterData::AppendData:
      data.appendData(args[0].toString(exec).string());
      return Undefined();
    case DOMCharacterData::InsertData:
      data.insertData(args[0].toInt32(exec), args[1].toString(exec).string());
      return Undefined();
    case DOMCharacterData::DeleteData: {
      const int count = args[1].toInt32(exec);
      if (count < 0)
        throw DOMException(DOMException::INDEX_SIZE_ERR);
      data.deleteData(args[0].toInt32(exec), count);
      return Undefined();
    }
    case DOMCharacterData::ReplaceData: {
      const int count = args[1].toInt32(exec);
      if (count < 0)
        throw DOMException(DOMException::INDEX_SIZE_ERR);
      data.replaceData(args[0].toInt32(exec), count, args[2].toString(exec).string());
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

DOMText::DOMText(ExecState *exec, const DOM::Text &t)
  : DOMCharacterData(DOMTextProto::self(exec), t) { }

Value DOMText::tryGet(ExecState *exec, const Identifier &p) const
{
  if (p == "")
    return Undefined(); // ### TODO
  else
    return DOMCharacterData::tryGet(exec, p);
}

Value DOMTextProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMText::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOM::Text text = static_cast<DOMText *>(thisObj.imp())->toText();
  switch(id) {
    case DOMText::SplitText:
      return getDOMNode(exec,text.splitText(args[0].toInt32(exec)));
      break;
    default:
      return Undefined();
  }
}

