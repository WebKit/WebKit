// -*- c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007 Apple Inc.
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
#include "kjs_html.h"

#include "DocLoader.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLDocument.h"
#include "HTMLEmbedElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "JSHTMLOptionsCollection.h"
#include "NameNodeList.h"
#include "RenderLayer.h"
#include "Text.h"
#include "kjs_css.h"
#include "kjs_events.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include <math.h>

#if ENABLE(SVG)
#include "SVGDocument.h"
#endif

#include "kjs_html.lut.h"

using namespace WebCore;
using namespace HTMLNames;
using namespace EventNames;

namespace KJS {

/*
@begin JSHTMLElementPrototypeTable 0
@end
*/

const ClassInfo JSHTMLElement::info = { "HTMLElement", &JSElement::info, 0, 0 };

KJS_IMPLEMENT_PROTOTYPE_FUNCTION(JSHTMLElementPrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("HTMLElement", JSHTMLElementPrototype, JSHTMLElementPrototypeFunction)

JSValue* JSHTMLElementPrototypeFunction::callAsFunction(ExecState*, JSObject*, const List&)
{
    return 0;
}

JSHTMLElement::JSHTMLElement(ExecState* exec, HTMLElement* e)
    : WebCore::JSHTMLElement(exec, e)
{
    setPrototype(JSHTMLElementPrototype::self(exec));
}

UString JSHTMLElement::toString(ExecState* exec) const
{
    if (impl()->hasTagName(aTag))
        return UString(static_cast<const HTMLAnchorElement*>(impl())->href());
    return JSElement::toString(exec);
}

static HTMLFormElement* getForm(HTMLElement* element)
{
    if (element->isGenericFormElement())
        return static_cast<HTMLGenericFormElement*>(element)->form();
    if (element->hasTagName(labelTag))
        return static_cast<HTMLLabelElement*>(element)->form();
    if (element->hasTagName(objectTag))
        return static_cast<HTMLObjectElement*>(element)->form();

    return 0;
}

void JSHTMLElement::pushEventHandlerScope(ExecState* exec, ScopeChain& scope) const
{
    HTMLElement* element = static_cast<HTMLElement*>(impl());

    // The document is put on first, fall back to searching it only after the element and form.
    scope.push(static_cast<JSObject*>(toJS(exec, element->ownerDocument())));

    // The form is next, searched before the document, but after the element itself.

    // First try to obtain the form from the element itself.  We do this to deal with
    // the malformed case where <form>s aren't in our parent chain (e.g., when they were inside 
    // <table> or <tbody>.
    HTMLFormElement* form = getForm(element);
    if (form)
        scope.push(static_cast<JSObject*>(toJS(exec, form)));
    else {
        WebCore::Node* form = element->parentNode();
        while (form && !form->hasTagName(formTag))
            form = form->parentNode();

        if (form)
            scope.push(static_cast<JSObject*>(toJS(exec, form)));
    }

    // The element is on top, searched first.
    scope.push(static_cast<JSObject*>(toJS(exec, element)));
}

HTMLElement* toHTMLElement(JSValue *val)
{
    if (!val || !val->isObject(&JSHTMLElement::info))
        return 0;
    return static_cast<HTMLElement*>(static_cast<JSHTMLElement*>(val)->impl());
}

// -------------------------------------------------------------------------
/* Source for JSHTMLCollectionPrototypeTable. Use "make hashtables" to regenerate.
@begin JSHTMLCollectionPrototypeTable 3
  item          JSHTMLCollection::Item            DontDelete|Function 1
  namedItem     JSHTMLCollection::NamedItem       DontDelete|Function 1
  tags          JSHTMLCollection::Tags            DontDelete|Function 1
@end
*/
KJS_IMPLEMENT_PROTOTYPE_FUNCTION(JSHTMLCollectionPrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("HTMLCollection",JSHTMLCollectionPrototype,JSHTMLCollectionPrototypeFunction)

const ClassInfo JSHTMLCollection::info = { "HTMLCollection", 0, 0, 0 };

JSHTMLCollection::JSHTMLCollection(ExecState* exec, HTMLCollection* c)
  : m_impl(c) 
{
  setPrototype(JSHTMLCollectionPrototype::self(exec));
}

JSHTMLCollection::~JSHTMLCollection()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue *JSHTMLCollection::lengthGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLCollection *thisObj = static_cast<JSHTMLCollection*>(slot.slotBase());
    return jsNumber(thisObj->m_impl->length());
}

JSValue *JSHTMLCollection::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLCollection *thisObj = static_cast<JSHTMLCollection*>(slot.slotBase());
    return toJS(exec, thisObj->m_impl->item(slot.index()));
}

JSValue *JSHTMLCollection::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLCollection *thisObj = static_cast<JSHTMLCollection*>(slot.slotBase());
    return thisObj->getNamedItems(exec, propertyName);
}

bool JSHTMLCollection::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (propertyName == exec->propertyNames().length) {
      slot.setCustom(this, lengthGetter);
      return true;
  } else {
    // Look in the prototype (for functions) before assuming it's an item's name
    JSValue *proto = prototype();
    if (proto->isObject() && static_cast<JSObject*>(proto)->hasProperty(exec, propertyName))
      return false;

    // name or index ?
    bool ok;
    unsigned int u = propertyName.toUInt32(&ok, false);
    if (ok) {
      slot.setCustomIndex(this, u, indexGetter);
      return true;
    }

    if (!getNamedItems(exec, propertyName)->isUndefined()) {
      slot.setCustom(this, nameGetter);
      return true;
    }
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

// HTMLCollections are strange objects, they support both get and call,
// so that document.forms.item(0) and document.forms(0) both work.
JSValue *JSHTMLCollection::callAsFunction(ExecState* exec, JSObject* , const List &args)
{
  // Do not use thisObj here. It can be the JSHTMLDocument, in the document.forms(i) case.
  HTMLCollection &collection = *m_impl;

  // Also, do we need the TypeError test here ?

  if (args.size() == 1) {
    // support for document.all(<index>) etc.
    bool ok;
    UString s = args[0]->toString(exec);
    unsigned int u = s.toUInt32(&ok, false);
    if (ok)
      return toJS(exec, collection.item(u));
    // support for document.images('<name>') etc.
    return getNamedItems(exec, Identifier(s));
  }
  else if (args.size() >= 1) // the second arg, if set, is the index of the item we want
  {
    bool ok;
    UString s = args[0]->toString(exec);
    unsigned int u = args[1]->toString(exec).toUInt32(&ok, false);
    if (ok)
    {
      WebCore::String pstr = s;
      WebCore::Node *node = collection.namedItem(pstr);
      while (node) {
        if (!u)
          return toJS(exec,node);
        node = collection.nextNamedItem(pstr);
        --u;
      }
    }
  }
  return jsUndefined();
}

JSValue *JSHTMLCollection::getNamedItems(ExecState* exec, const Identifier &propertyName) const
{
    Vector<RefPtr<Node> > namedItems;
    
    m_impl->namedItems(propertyName, namedItems);

    if (namedItems.isEmpty())
        return jsUndefined();

    if (namedItems.size() == 1)
        return toJS(exec, namedItems[0].get());

    return new DOMNamedNodesCollection(exec, namedItems);
}

JSValue* JSHTMLCollectionPrototypeFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&JSHTMLCollection::info))
    return throwError(exec, TypeError);
  HTMLCollection &coll = *static_cast<JSHTMLCollection*>(thisObj)->impl();

  switch (id) {
  case JSHTMLCollection::Tags:
    return toJS(exec, coll.base()->getElementsByTagName(args[0]->toString(exec)).get());
  case JSHTMLCollection::Item:
    {
        bool ok;
        uint32_t index = args[0]->toString(exec).toUInt32(&ok, false);
        if (ok)
            return toJS(exec, coll.item(index));
    }
    // Fall through
  case JSHTMLCollection::NamedItem:
    return static_cast<JSHTMLCollection*>(thisObj)->getNamedItems(exec, Identifier(args[0]->toString(exec)));
  default:
    return jsUndefined();
  }
}

////////////////////// Image Object ////////////////////////

ImageConstructorImp::ImageConstructorImp(ExecState* exec, Document* d)
    : m_doc(d)
{
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
}

bool ImageConstructorImp::implementsConstruct() const
{
  return true;
}

JSObject* ImageConstructorImp::construct(ExecState*  exec, const List & list)
{
    bool widthSet = false, heightSet = false;
    int width = 0, height = 0;
    if (list.size() > 0) {
        widthSet = true;
        JSValue *w = list.at(0);
        width = w->toInt32(exec);
    }
    if (list.size() > 1) {
        heightSet = true;
        JSValue *h = list.at(1);
        height = h->toInt32(exec);
    }
        
    HTMLImageElement* image = new HTMLImageElement(m_doc.get());
    JSObject* result = static_cast<JSObject*>(toJS(exec, image));
    
    if (widthSet)
        image->setWidth(width);
    if (heightSet)
        image->setHeight(height);
    
    return result;
}

////////////////////////////////////////////////////////////////
                     
JSValue* getAllHTMLCollection(ExecState* exec, HTMLCollection* c)
{
    return cacheDOMObject<HTMLCollection, HTMLAllCollection>(exec, c);
}

JSValue* getHTMLCollection(ExecState* exec, HTMLCollection* c)
{
    return cacheDOMObject<HTMLCollection, JSHTMLCollection>(exec, c);
}

JSValue* toJS(ExecState* exec, HTMLOptionsCollection* c)
{
    return cacheDOMObject<HTMLOptionsCollection, JSHTMLOptionsCollection>(exec, c);
}

JSValue* runtimeObjectGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());

    return getRuntimeObject(exec, element);
}

JSValue* runtimeObjectPropertyGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());

    if (JSValue* runtimeObject = getRuntimeObject(exec, element))
        return static_cast<JSObject*>(runtimeObject)->get(exec, propertyName);
    return jsUndefined();
}

bool runtimeObjectCustomGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, JSHTMLElement* originalObj, HTMLElement* thisImp)
{
    JSValue* runtimeObject = getRuntimeObject(exec, thisImp);
    if (runtimeObject) {
        JSObject* imp = static_cast<JSObject*>(runtimeObject);
        if (imp->hasProperty(exec, propertyName)) {
            slot.setCustom(originalObj, runtimeObjectPropertyGetter);
            return true;
        }
    }

    return false;
}

bool runtimeObjectCustomPut(ExecState* exec, const Identifier& propertyName, JSValue* value, int /*attr*/, HTMLElement* thisImp)
{
    if (JSValue* runtimeObject = getRuntimeObject(exec, thisImp)) {
        JSObject* imp = static_cast<JSObject*>(runtimeObject);
        if (imp->canPut(exec, propertyName)) {
            imp->put(exec, propertyName, value);
            return true;
        }
    }

    return false;
}

bool runtimeObjectImplementsCall(HTMLElement* thisImp)
{
    Frame* frame = thisImp->document()->frame();
    if (!frame)
        return false;
    KJSProxy* proxy = frame->scriptProxy();
    ExecState* exec = proxy->interpreter()->globalExec();
    if (JSValue* runtimeObject = getRuntimeObject(exec, thisImp))
        return static_cast<JSObject*>(runtimeObject)->implementsCall();

    return false;
}

JSValue* runtimeObjectCallAsFunction(ExecState* exec, JSObject* thisObj, const List& args, HTMLElement* thisImp)
{
    if (JSValue* runtimeObject = getRuntimeObject(exec, thisImp))
        return static_cast<JSObject*>(runtimeObject)->call(exec, thisObj, args);
    return jsUndefined();
}

} // namespace
