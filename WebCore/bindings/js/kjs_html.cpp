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
#include "HTMLFrameSetElement.h"
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

class HTMLElementFunction : public InternalFunctionImp {
public:
  HTMLElementFunction(ExecState* exec, int i, int len, const Identifier& name);
  virtual JSValue *callAsFunction(ExecState* exec, JSObject* thisObj, const List&args);
private:
  int id;
};

/* 
@begin JSHTMLDocumentPrototypeTable 8
clear                 JSHTMLDocument::Clear             DontDelete|Function 0
open                  JSHTMLDocument::Open              DontDelete|Function 0
close                 JSHTMLDocument::Close             DontDelete|Function 0
write                 JSHTMLDocument::Write             DontDelete|Function 1
writeln               JSHTMLDocument::WriteLn           DontDelete|Function 1
getElementsByName     JSHTMLDocument::GetElementsByName DontDelete|Function 1
captureEvents         JSHTMLDocument::CaptureEvents     DontDelete|Function 0
releaseEvents         JSHTMLDocument::ReleaseEvents     DontDelete|Function 0
@end
*/
KJS_IMPLEMENT_PROTOTYPE_FUNCTION(JSHTMLDocumentPrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("DOMHTMLDocument", JSHTMLDocumentPrototype, JSHTMLDocumentPrototypeFunction)

JSValue *JSHTMLDocumentPrototypeFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
    if (!thisObj->inherits(&JSHTMLDocument::info))
        return throwError(exec, TypeError);
    HTMLDocument &doc = *static_cast<HTMLDocument*>(static_cast<JSHTMLDocument*>(thisObj)->impl());
    
    switch (id) {
        case JSHTMLDocument::Clear: // even IE doesn't support that one...
                                    //doc.clear(); // TODO
            return jsUndefined();
        case JSHTMLDocument::Open:
            // For compatibility with other browsers, pass open calls with more than 2 parameters to the window.
            if (args.size() > 2) {
                Frame *frame = doc.frame();
                if (frame) {
                    Window *window = Window::retrieveWindow(frame);
                    if (window) {
                        JSObject* functionObject = window->get(exec, "open")->getObject();
                        if (!functionObject || !functionObject->implementsCall())
                            return throwError(exec, TypeError);
                        return functionObject->call(exec, window, args);
                    }
                }
                return jsUndefined();
            }
            // In the case of two parameters or fewer, do a normal document open.
            doc.open();
            return jsUndefined();
        case JSHTMLDocument::Close:
            doc.close();
            return jsUndefined();
        case JSHTMLDocument::Write:
        case JSHTMLDocument::WriteLn: {
            // DOM only specifies single string argument, but NS & IE allow multiple
            // or no arguments
            String str = "";
            for (int i = 0; i < args.size(); i++)
                str += args[i]->toString(exec);
            if (id == JSHTMLDocument::WriteLn)
                str += "\n";
            doc.write(str);
            return jsUndefined();
        }
        case JSHTMLDocument::GetElementsByName:
            return toJS(exec, doc.getElementsByName(args[0]->toString(exec)).get());
        case JSHTMLDocument::CaptureEvents:
        case JSHTMLDocument::ReleaseEvents:
            // Do nothing for now. These are NS-specific legacy calls.
            break;
    }
    
    return jsUndefined();
}


// FIXME: functions should be in the prototype
const ClassInfo JSHTMLDocument::info =
  { "DOMHTMLDocument", &JSDocument::info, &HTMLDocumentTable, 0 };
/* Source for HTMLDocumentTable. Use "make hashtables" to regenerate.
@begin HTMLDocumentTable 30
  title                 JSHTMLDocument::Title             DontDelete
  referrer              JSHTMLDocument::Referrer          DontDelete|ReadOnly
  domain                JSHTMLDocument::Domain            DontDelete
  body                  JSHTMLDocument::Body              DontDelete
  location              JSHTMLDocument::Location          DontDelete
  cookie                JSHTMLDocument::Cookie            DontDelete
  images                JSHTMLDocument::Images            DontDelete|ReadOnly
  embeds                JSHTMLDocument::Embeds            DontDelete|ReadOnly
  plugins               JSHTMLDocument::Embeds            DontDelete|ReadOnly
  applets               JSHTMLDocument::Applets           DontDelete|ReadOnly
  links                 JSHTMLDocument::Links             DontDelete|ReadOnly
  forms                 JSHTMLDocument::Forms             DontDelete|ReadOnly
  anchors               JSHTMLDocument::Anchors           DontDelete|ReadOnly
  scripts               JSHTMLDocument::Scripts           DontDelete|ReadOnly
  all                   JSHTMLDocument::All               
  bgColor               JSHTMLDocument::BgColor           DontDelete
  fgColor               JSHTMLDocument::FgColor           DontDelete
  alinkColor            JSHTMLDocument::AlinkColor        DontDelete
  linkColor             JSHTMLDocument::LinkColor         DontDelete
  vlinkColor            JSHTMLDocument::VlinkColor        DontDelete
  lastModified          JSHTMLDocument::LastModified      DontDelete|ReadOnly
  height                JSHTMLDocument::Height            DontDelete|ReadOnly
  width                 JSHTMLDocument::Width             DontDelete|ReadOnly
  dir                   JSHTMLDocument::Dir               DontDelete
  designMode            JSHTMLDocument::DesignMode        DontDelete
#potentially obsolete array properties
# layers
# plugins
# tags
#potentially obsolete properties
# embeds
# ids
@end
*/

JSHTMLDocument::JSHTMLDocument(ExecState* exec, HTMLDocument *d)
  : JSDocument(exec, d)
{
    setPrototype(JSHTMLDocumentPrototype::self(exec));
}

JSValue *JSHTMLDocument::namedItemGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  JSHTMLDocument *thisObj = static_cast<JSHTMLDocument*>(slot.slotBase());
  HTMLDocument &doc = *static_cast<HTMLDocument*>(thisObj->impl());

  String name = propertyName;
  RefPtr<HTMLCollection> collection = doc.documentNamedItems(name);

  if (collection->length() == 1) {
    Node* node = collection->firstItem();
    Frame *frame;
    if (node->hasTagName(iframeTag) && (frame = static_cast<HTMLIFrameElement*>(node)->contentFrame()))
      return Window::retrieve(frame);
    return toJS(exec, node);
  } else if (collection->length() == 0)
      return jsUndefined();

  return getHTMLCollection(exec, collection.get());
}

JSValue *JSHTMLDocument::getValueProperty(ExecState* exec, int token) const
{
  HTMLDocument& doc = *static_cast<HTMLDocument*>(impl());

  FrameView* view = doc.view();
  Frame* frame = doc.frame();

  HTMLElement* body = doc.body();
  HTMLBodyElement* bodyElement = (body && body->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(body) : 0;
    
  switch (token) {
  case Title:
    return jsString(doc.title());
  case Referrer:
    return jsString(doc.referrer());
  case Domain:
    return jsString(doc.domain());
  case Body:
    return toJS(exec, body);
  case Location:
    if (!frame)
      return jsNull();
    if (Window* win = Window::retrieveWindow(frame))
      return win->location();
    return jsUndefined();
  case Cookie:
    return jsString(doc.cookie());
  case Images:
    return getHTMLCollection(exec, doc.images().get());
  case Embeds:
    return getHTMLCollection(exec, doc.embeds().get());
  case Applets:
    return getHTMLCollection(exec, doc.applets().get());
  case Links:
    return getHTMLCollection(exec, doc.links().get());
  case Forms:
    return getHTMLCollection(exec, doc.forms().get());
  case Anchors:
    return getHTMLCollection(exec, doc.anchors().get());
  case Scripts:
    return getHTMLCollection(exec, doc.scripts().get());
  case All:
    // If "all" has been overwritten, return the overwritten value
    if (JSValue *v = getDirect("all"))
      return v;
    else
      return getAllHTMLCollection(exec, doc.all().get());
  case BgColor:
    if (!bodyElement)
      return jsUndefined();
    return jsString(bodyElement->bgColor());
  case FgColor:
    if (!bodyElement)
      return jsUndefined();
    return jsString(bodyElement->text());
  case AlinkColor:
    if (!bodyElement)
      return jsUndefined();
    return jsString(bodyElement->aLink());
  case LinkColor:
    if (!bodyElement)
      return jsUndefined();
    return jsString(bodyElement->link());
  case VlinkColor:
    if (!bodyElement)
      return jsUndefined();
    return jsString(bodyElement->vLink());
  case LastModified:
    return jsString(doc.lastModified());
  case Height:
    return jsNumber(view ? view->contentsHeight() : 0);
  case Width:
    return jsNumber(view ? view->contentsWidth() : 0);
  case Dir:
    if (!body)
      return jsString("");
    return jsString(body->dir());
  case DesignMode:
    return jsString(doc.inDesignMode() ? "on" : "off");
  default:
    ASSERT(0);
    return jsUndefined();
  }
}

bool JSHTMLDocument::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  HTMLDocument &doc = *static_cast<HTMLDocument*>(impl());

  String name = propertyName;
  if (doc.hasNamedItem(name) || doc.hasDocExtraNamedItem(name)) {
    slot.setCustom(this, namedItemGetter);
    return true;
  }

  return getStaticValueSlot<JSHTMLDocument, JSDocument>(exec, &HTMLDocumentTable, this, propertyName, slot);
}

void JSHTMLDocument::put(ExecState* exec, const Identifier &propertyName, JSValue *value, int attr)
{
    lookupPut<JSHTMLDocument, JSDocument>(exec, propertyName, value, attr, &HTMLDocumentTable, this);
}

void JSHTMLDocument::putValueProperty(ExecState* exec, int token, JSValue *value, int /*attr*/)
{
  DOMExceptionTranslator exception(exec);
  HTMLDocument &doc = *static_cast<HTMLDocument*>(impl());
  HTMLElement* body = doc.body();
  HTMLBodyElement* bodyElement = (body && body->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(body) : 0;

  switch (token) {
  case Title:
    doc.setTitle(valueToStringWithNullCheck(exec, value));
    break;
  case Body:
    doc.setBody(toHTMLElement(value), exception);
    break;
  case Domain: // not part of the DOM
    doc.setDomain(value->toString(exec));
    break;
  case Cookie:
    doc.setCookie(valueToStringWithNullCheck(exec, value));
    break;
  case Location:
    if (Frame* frame = doc.frame()) {
      DeprecatedString str = value->toString(exec);

      // When assigning location, IE and Mozilla both resolve the URL
      // relative to the frame where the JavaScript is executing not
      // the target frame.
      Frame* activeFrame = static_cast<ScriptInterpreter*>( exec->dynamicInterpreter() )->frame();
      if (activeFrame)
        str = activeFrame->document()->completeURL(str);

      // We want a new history item if this JS was called via a user gesture
      bool userGesture = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter())->wasRunByUserGesture();
      frame->loader()->scheduleLocationChange(str, activeFrame->loader()->outgoingReferrer(), !userGesture);
    }
    break;
  case BgColor:
    if (bodyElement)
      bodyElement->setBgColor(valueToStringWithNullCheck(exec, value));
    break;
  case FgColor:
    if (bodyElement)
      bodyElement->setText(valueToStringWithNullCheck(exec, value));
    break;
  case AlinkColor:
    if (bodyElement) {
      // this check is a bit silly, but some benchmarks like to set the
      // document's link colors over and over to the same value and we
      // don't want to incur a style update each time.
      String newColor = valueToStringWithNullCheck(exec, value);
      if (bodyElement->aLink() != newColor)
        bodyElement->setALink(newColor);
    }
    break;
  case LinkColor:
    if (bodyElement) {
      // this check is a bit silly, but some benchmarks like to set the
      // document's link colors over and over to the same value and we
      // don't want to incur a style update each time.
      String newColor = valueToStringWithNullCheck(exec, value);
      if (bodyElement->link() != newColor)
        bodyElement->setLink(newColor);
    }
    break;
  case VlinkColor:
    if (bodyElement) {
      // this check is a bit silly, but some benchmarks like to set the
      // document's link colors over and over to the same value and we
      // don't want to incur a style update each time.
      String newColor = valueToStringWithNullCheck(exec, value);
      if (bodyElement->vLink() != newColor)
        bodyElement->setVLink(newColor);
    }
    break;
  case Dir:
    if (body)
      body->setDir(valueToStringWithNullCheck(exec, value));
    break;
  case DesignMode:
    {
      String modeString = valueToStringWithNullCheck(exec, value);
      Document::InheritedBool mode;
      if (equalIgnoringCase(modeString, "on"))
        mode = Document::on;
      else if (equalIgnoringCase(modeString, "off"))
        mode = Document::off;
      else
        mode = Document::inherit;
      doc.setDesignMode(mode);
      break;
    }
  case All:
    // Add "all" to the property map.
    putDirect("all", value);
    break;
  }
}

// -------------------------------------------------------------------------

const ClassInfo JSHTMLElement::info = { "HTMLElement", &JSElement::info, &HTMLElementTable, 0 };

const ClassInfo JSHTMLElement::embed_info = { "HTMLEmbedElement", &JSHTMLElement::info, &HTMLEmbedElementTable, 0 };
const ClassInfo JSHTMLElement::frameSet_info = { "HTMLFrameSetElement", &JSHTMLElement::info, &HTMLFrameSetElementTable, 0 };
const ClassInfo JSHTMLElement::marquee_info = { "HTMLMarqueeElement", &JSHTMLElement::info, &HTMLMarqueeElementTable, 0 };
const ClassInfo JSHTMLElement::object_info = { "HTMLObjectElement", &JSHTMLElement::info, &HTMLObjectElementTable, 0 };

const ClassInfo* JSHTMLElement::classInfo() const
{
    static HashMap<AtomicStringImpl*, const ClassInfo*> classInfoMap;
    if (classInfoMap.isEmpty()) {
        classInfoMap.set(embedTag.localName().impl(), &embed_info);
        classInfoMap.set(framesetTag.localName().impl(), &frameSet_info);
        classInfoMap.set(marqueeTag.localName().impl(), &marquee_info);
        classInfoMap.set(objectTag.localName().impl(), &object_info);
    }
    
    HTMLElement* element = static_cast<HTMLElement*>(impl());
    const ClassInfo* result = classInfoMap.get(element->localName().impl());
    if (result)
        return result;
    return &info;
}

const JSHTMLElement::Accessors JSHTMLElement::object_accessors = { &JSHTMLElement::objectGetter, &JSHTMLElement::objectSetter };
const JSHTMLElement::Accessors JSHTMLElement::embed_accessors = { &JSHTMLElement::embedGetter, &JSHTMLElement::embedSetter };
const JSHTMLElement::Accessors JSHTMLElement::frameSet_accessors = { &JSHTMLElement::frameSetGetter, &JSHTMLElement::frameSetSetter };
const JSHTMLElement::Accessors JSHTMLElement::marquee_accessors = { &JSHTMLElement::marqueeGetter, &JSHTMLElement::marqueeSetter };

const JSHTMLElement::Accessors* JSHTMLElement::accessors() const
{
    static HashMap<AtomicStringImpl*, const Accessors*> accessorMap;
    if (accessorMap.isEmpty()) {
        accessorMap.add(embedTag.localName().impl(), &embed_accessors);
        accessorMap.add(framesetTag.localName().impl(), &frameSet_accessors);
        accessorMap.add(marqueeTag.localName().impl(), &marquee_accessors);
        accessorMap.add(objectTag.localName().impl(), &object_accessors);
    }
    
    HTMLElement* element = static_cast<HTMLElement*>(impl());
    return accessorMap.get(element->localName().impl());
}

/*

@begin JSHTMLElementPrototypeTable 0
@end

@begin HTMLElementTable 14
  id            KJS::JSHTMLElement::ElementId     DontDelete
  lang          KJS::JSHTMLElement::ElementLang   DontDelete
  dir           KJS::JSHTMLElement::ElementDir    DontDelete
### isn't this "class" in the HTML spec?
  className     KJS::JSHTMLElement::ElementClassName DontDelete
  innerHTML     KJS::JSHTMLElement::ElementInnerHTML DontDelete
  innerText     KJS::JSHTMLElement::ElementInnerText DontDelete
  outerHTML     KJS::JSHTMLElement::ElementOuterHTML DontDelete
  outerText     KJS::JSHTMLElement::ElementOuterText DontDelete
  document      KJS::JSHTMLElement::ElementDocument  DontDelete|ReadOnly
# IE extension
  children      KJS::JSHTMLElement::ElementChildren  DontDelete|ReadOnly
  contentEditable   KJS::JSHTMLElement::ElementContentEditable  DontDelete
  isContentEditable KJS::JSHTMLElement::ElementIsContentEditable  DontDelete|ReadOnly
@end
@begin HTMLObjectElementTable 20
  form            KJS::JSHTMLElement::ObjectForm            DontDelete|ReadOnly
  code            KJS::JSHTMLElement::ObjectCode            DontDelete
  align           KJS::JSHTMLElement::ObjectAlign           DontDelete
  archive         KJS::JSHTMLElement::ObjectArchive         DontDelete
  border          KJS::JSHTMLElement::ObjectBorder          DontDelete
  codeBase        KJS::JSHTMLElement::ObjectCodeBase        DontDelete
  codeType        KJS::JSHTMLElement::ObjectCodeType        DontDelete
  contentDocument KJS::JSHTMLElement::ObjectContentDocument DontDelete|ReadOnly
  data            KJS::JSHTMLElement::ObjectData            DontDelete
  declare         KJS::JSHTMLElement::ObjectDeclare         DontDelete
  height          KJS::JSHTMLElement::ObjectHeight          DontDelete
  hspace          KJS::JSHTMLElement::ObjectHspace          DontDelete
  getSVGDocument  KJS::JSHTMLElement::ObjectGetSVGDocument  DontDelete|Function 0
  name            KJS::JSHTMLElement::ObjectName            DontDelete
  standby         KJS::JSHTMLElement::ObjectStandby         DontDelete
  tabIndex        KJS::JSHTMLElement::ObjectTabIndex        DontDelete
  type            KJS::JSHTMLElement::ObjectType            DontDelete
  useMap          KJS::JSHTMLElement::ObjectUseMap          DontDelete
  vspace          KJS::JSHTMLElement::ObjectVspace          DontDelete
  width           KJS::JSHTMLElement::ObjectWidth           DontDelete
@end
@begin HTMLEmbedElementTable 6
  align         KJS::JSHTMLElement::EmbedAlign           DontDelete
  height        KJS::JSHTMLElement::EmbedHeight          DontDelete
  getSVGDocument KJS::JSHTMLElement::EmbedGetSVGDocument DontDelete|Function 0
  name          KJS::JSHTMLElement::EmbedName            DontDelete
  src           KJS::JSHTMLElement::EmbedSrc             DontDelete
  type          KJS::JSHTMLElement::EmbedType            DontDelete
  width         KJS::JSHTMLElement::EmbedWidth           DontDelete
@end
@begin HTMLFrameSetElementTable 2
cols          KJS::JSHTMLElement::FrameSetCols                  DontDelete
rows          KJS::JSHTMLElement::FrameSetRows                  DontDelete
@end
@begin HTMLMarqueeElementTable 2
  start           KJS::JSHTMLElement::MarqueeStart                DontDelete|Function 0
  stop            KJS::JSHTMLElement::MarqueeStop                 DontDelete|Function 0
@end
*/

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

JSValue *JSHTMLElement::framesetNameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());

    WebCore::Node *frame = element->children()->namedItem(propertyName);
    if (Document* doc = static_cast<HTMLFrameElement*>(frame)->contentDocument())
        if (Window *window = Window::retrieveWindow(doc->frame()))
            return window;

    return jsUndefined();
}

JSValue *JSHTMLElement::runtimeObjectGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());

    return getRuntimeObject(exec, element);
}

JSValue *JSHTMLElement::runtimeObjectPropertyGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());

    if (JSValue *runtimeObject = getRuntimeObject(exec, element))
        return static_cast<JSObject*>(runtimeObject)->get(exec, propertyName);
    return jsUndefined();
}

bool JSHTMLElement::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    HTMLElement &element = *static_cast<HTMLElement*>(impl());

    // First look at dynamic properties
    if (element.hasLocalName(framesetTag)) {
        WebCore::Node *frame = element.children()->namedItem(propertyName);
        if (frame && frame->hasTagName(frameTag)) {
            slot.setCustom(this, framesetNameGetter);
            return true;
        }
    } else if (element.hasLocalName(embedTag) || element.hasLocalName(objectTag) || element.hasLocalName(appletTag)) {
        if (propertyName == "__apple_runtime_object") {
            slot.setCustom(this, runtimeObjectGetter);
            return true;
        }
        JSValue *runtimeObject = getRuntimeObject(exec,&element);
        if (runtimeObject) {
            JSObject* imp = static_cast<JSObject*>(runtimeObject);
            if (imp->hasProperty(exec, propertyName)) {
                slot.setCustom(this, runtimeObjectPropertyGetter);
                return true;
            }
        }
    }

    const HashTable* table = classInfo()->propHashTable; // get the right hashtable
    const HashEntry* entry = Lookup::findEntry(table, propertyName);
    if (entry) {
        if (entry->attr & Function)
            slot.setStaticEntry(this, entry, staticFunctionGetter<HTMLElementFunction>); 
        else
            slot.setStaticEntry(this, entry, staticValueGetter<JSHTMLElement>);
        return true;
    }

    // Base JSHTMLElement stuff or parent class forward, as usual
    return getStaticPropertySlot<HTMLElementFunction, JSHTMLElement, WebCore::JSHTMLElement>(exec, &HTMLElementTable, this, propertyName, slot);
}

bool JSHTMLElement::implementsCall() const
{
    HTMLElement* element = static_cast<HTMLElement*>(impl());
    if (element->hasTagName(embedTag) || element->hasTagName(objectTag) || element->hasTagName(appletTag)) {
        Frame* frame = element->document()->frame();
        if (!frame)
            return false;
        KJSProxy *proxy = frame->scriptProxy();
        ExecState* exec = proxy->interpreter()->globalExec();
        if (JSValue *runtimeObject = getRuntimeObject(exec, element))
            return static_cast<JSObject*>(runtimeObject)->implementsCall();
    }
    return false;
}

JSValue *JSHTMLElement::callAsFunction(ExecState* exec, JSObject* thisObj, const List&args)
{
    HTMLElement* element = static_cast<HTMLElement*>(impl());
    if (element->hasTagName(embedTag) || element->hasTagName(objectTag) || element->hasTagName(appletTag)) {
        if (JSValue *runtimeObject = getRuntimeObject(exec, element))
            return static_cast<JSObject*>(runtimeObject)->call(exec, thisObj, args);
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::objectGetter(ExecState* exec, int token) const
{
    HTMLObjectElement& object = *static_cast<HTMLObjectElement*>(impl());
    switch (token) {
        case ObjectForm:            return toJS(exec,object.form()); // type HTMLFormElement
        case ObjectCode:            return jsString(object.code());
        case ObjectAlign:           return jsString(object.align());
        case ObjectArchive:         return jsString(object.archive());
        case ObjectBorder:          return jsString(object.border());
        case ObjectCodeBase:        return jsString(object.codeBase());
        case ObjectCodeType:        return jsString(object.codeType());
        case ObjectContentDocument: return checkNodeSecurity(exec,object.contentDocument()) ? 
                                           toJS(exec, object.contentDocument()) : jsUndefined();
        case ObjectData:            return jsString(object.data());
        case ObjectDeclare:         return jsBoolean(object.declare());
        case ObjectHeight:          return jsString(object.height());
        case ObjectHspace:          return jsNumber(object.hspace());
        case ObjectName:            return jsString(object.name());
        case ObjectStandby:         return jsString(object.standby());
        case ObjectTabIndex:        return jsNumber(object.tabIndex());
        case ObjectType:            return jsString(object.type());
        case ObjectUseMap:          return jsString(object.useMap());
        case ObjectVspace:          return jsNumber(object.vspace());
        case ObjectWidth:           return jsString(object.width());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::embedGetter(ExecState* exec, int token) const
{
    HTMLEmbedElement& embed = *static_cast<HTMLEmbedElement*>(impl());
    switch (token) {
        case EmbedAlign:           return jsString(embed.align());
        case EmbedHeight:          return jsString(embed.height());
        case EmbedName:            return jsString(embed.name());
        case EmbedSrc:             return jsString(embed.src());
        case EmbedType:            return jsString(embed.type());
        case EmbedWidth:           return jsString(embed.width());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::frameSetGetter(ExecState* exec, int token) const
{
    HTMLFrameSetElement& frameSet = *static_cast<HTMLFrameSetElement*>(impl());
    switch (token) {
        case FrameSetCols:            return jsString(frameSet.cols());
        case FrameSetRows:            return jsString(frameSet.rows());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::marqueeGetter(ExecState* exec, int token) const
{
    // FIXME: Find out what WinIE exposes as properties and implement this.
    return jsUndefined();
}

JSValue *JSHTMLElement::getValueProperty(ExecState* exec, int token) const
{
    // Check our set of generic properties first.
    HTMLElement &element = *static_cast<HTMLElement*>(impl());
    switch (token) {
        case ElementId:
            // iht.com relies on this value being "" when no id is present. Other browsers do this as well.
            // So we use jsString() instead of jsStringOrNull() here.
            return jsString(element.id());
        case ElementLang:
            return jsString(element.lang());
        case ElementDir:
            return jsString(element.dir());
        case ElementClassName:
            return jsString(element.className());
        case ElementInnerHTML:
            return jsString(element.innerHTML());
        case ElementInnerText:
            impl()->document()->updateLayoutIgnorePendingStylesheets();
            return jsString(element.innerText());
        case ElementOuterHTML:
            return jsString(element.outerHTML());
        case ElementOuterText:
            return jsString(element.outerText());
        case ElementDocument:
            return toJS(exec,element.ownerDocument());
        case ElementChildren:
            return getHTMLCollection(exec, element.children().get());
        case ElementContentEditable:
            return jsString(element.contentEditable());
        case ElementIsContentEditable:
            return jsBoolean(element.isContentEditable());
    }

    // Now check the properties specific to our element type.
    const Accessors* access = accessors();
    if (access && access->m_getter)
        return (this->*(access->m_getter))(exec, token);
    return jsUndefined();
}

UString JSHTMLElement::toString(ExecState* exec) const
{
    if (impl()->hasTagName(aTag))
        return UString(static_cast<const HTMLAnchorElement*>(impl())->href());
    else
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

void JSHTMLElement::pushEventHandlerScope(ExecState* exec, ScopeChain &scope) const
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

HTMLElementFunction::HTMLElementFunction(ExecState* exec, int i, int len, const Identifier& name)
  : InternalFunctionImp(static_cast<FunctionPrototype*>(exec->lexicalInterpreter()->builtinFunctionPrototype()), name)
  , id(i)
{
  put(exec, exec->propertyNames().length, jsNumber(len), DontDelete | ReadOnly | DontEnum);
}

JSValue *HTMLElementFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
    if (!thisObj->inherits(&JSHTMLElement::info))
        return throwError(exec, TypeError);
    DOMExceptionTranslator exception(exec);
    HTMLElement &element = *static_cast<HTMLElement*>(static_cast<JSHTMLElement*>(thisObj)->impl());

    if (element.hasLocalName(marqueeTag)) {
        if (id == JSHTMLElement::MarqueeStart && element.renderer() && 
            element.renderer()->hasLayer() &&
            element.renderer()->layer()->marquee()) {
            element.renderer()->layer()->marquee()->start();
            return jsUndefined();
        }
        if (id == JSHTMLElement::MarqueeStop && element.renderer() && 
            element.renderer()->hasLayer() &&
            element.renderer()->layer()->marquee()) {
            element.renderer()->layer()->marquee()->stop();
            return jsUndefined();
        }
    }
#if ENABLE(SVG)
    else if (element.hasLocalName(objectTag)) {
        HTMLObjectElement& object = static_cast<HTMLObjectElement&>(element);
        if (id == JSHTMLElement::ObjectGetSVGDocument)
            return checkNodeSecurity(exec, object.getSVGDocument(exception)) ? toJS(exec, object.getSVGDocument(exception)) : jsUndefined();
    } else if (element.hasLocalName(embedTag)) {
        HTMLEmbedElement& embed = static_cast<HTMLEmbedElement&>(element);
        if (id == JSHTMLElement::EmbedGetSVGDocument)
            return checkNodeSecurity(exec, embed.getSVGDocument(exception)) ? toJS(exec, embed.getSVGDocument(exception)) : jsUndefined();
    }
#endif

    return jsUndefined();
}

void JSHTMLElement::put(ExecState* exec, const Identifier &propertyName, JSValue *value, int attr)
{
    HTMLElement &element = *static_cast<HTMLElement*>(impl());
    // First look at dynamic properties
    if (element.hasLocalName(embedTag) || element.hasLocalName(objectTag) || element.hasLocalName(appletTag)) {
        if (JSValue *runtimeObject = getRuntimeObject(exec, &element)) {
            JSObject* imp = static_cast<JSObject*>(runtimeObject);
            if (imp->canPut(exec, propertyName))
                return imp->put(exec, propertyName, value);
        }
    }

    const HashTable* table = classInfo()->propHashTable; // get the right hashtable
    const HashEntry* entry = Lookup::findEntry(table, propertyName);
    if (entry) {
        if (entry->attr & Function) { // function: put as override property
            JSObject::put(exec, propertyName, value, attr);
            return;
        } else if (!(entry->attr & ReadOnly)) { // let lookupPut print the warning if read-only
            putValueProperty(exec, entry->value, value, attr);
            return;
        }
    }

    lookupPut<JSHTMLElement, WebCore::JSHTMLElement>(exec, propertyName, value, attr, &HTMLElementTable, this);
}

void JSHTMLElement::objectSetter(ExecState* exec, int token, JSValue* value)
{
    HTMLObjectElement& object = *static_cast<HTMLObjectElement*>(impl());
    switch (token) {
        // read-only: form
        case ObjectCode:            { object.setCode(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectAlign:           { object.setAlign(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectArchive:         { object.setArchive(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectBorder:          { object.setBorder(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectCodeBase:        { object.setCodeBase(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectCodeType:        { object.setCodeType(valueToStringWithNullCheck(exec, value)); return; }
        // read-only: ObjectContentDocument
        case ObjectData:            { object.setData(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectDeclare:         { object.setDeclare(value->toBoolean(exec)); return; }
        case ObjectHeight:          { object.setHeight(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectHspace:          { object.setHspace(value->toInt32(exec)); return; }
        case ObjectName:            { object.setName(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectStandby:         { object.setStandby(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectTabIndex:        { object.setTabIndex(value->toInt32(exec)); return; }
        case ObjectType:            { object.setType(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectUseMap:          { object.setUseMap(valueToStringWithNullCheck(exec, value)); return; }
        case ObjectVspace:          { object.setVspace(value->toInt32(exec)); return; }
        case ObjectWidth:           { object.setWidth(valueToStringWithNullCheck(exec, value)); return; }
    }
}

void JSHTMLElement::embedSetter(ExecState* exec, int token, JSValue* value)
{
    HTMLEmbedElement& embed = *static_cast<HTMLEmbedElement*>(impl());
    switch (token) {
        case EmbedAlign:           { embed.setAlign(valueToStringWithNullCheck(exec, value)); return; }
        case EmbedHeight:          { embed.setHeight(value->toString(exec)); return; }
        case EmbedName:            { embed.setName(valueToStringWithNullCheck(exec, value)); return; }
        case EmbedSrc:             { embed.setSrc(valueToStringWithNullCheck(exec, value)); return; }
        case EmbedType:            { embed.setType(valueToStringWithNullCheck(exec, value)); return; }
        case EmbedWidth:           { embed.setWidth(value->toString(exec)); return; }
    }
}

void JSHTMLElement::frameSetSetter(ExecState* exec, int token, JSValue* value)
{
    HTMLFrameSetElement& frameSet = *static_cast<HTMLFrameSetElement*>(impl());
    switch (token) {
        case FrameSetCols:            { frameSet.setCols(valueToStringWithNullCheck(exec, value)); return; }
        case FrameSetRows:            { frameSet.setRows(valueToStringWithNullCheck(exec, value)); return; }
    }
}

void JSHTMLElement::marqueeSetter(ExecState* exec, int token, JSValue* value)
{
    // FIXME: Find out what WinIE supports and implement it.
}

void JSHTMLElement::putValueProperty(ExecState* exec, int token, JSValue *value, int)
{
    DOMExceptionTranslator exception(exec);
 
    // Check our set of generic properties first.
    HTMLElement &element = *static_cast<HTMLElement*>(impl());
    switch (token) {
        case ElementId:
            element.setId(valueToStringWithNullCheck(exec, value));
            return;
        case ElementLang:
            element.setLang(valueToStringWithNullCheck(exec, value));
            return;
        case ElementDir:
            element.setDir(valueToStringWithNullCheck(exec, value));
            return;
        case ElementClassName:
            element.setClassName(valueToStringWithNullCheck(exec, value));
            return;
        case ElementInnerHTML:
            element.setInnerHTML(valueToStringWithNullCheck(exec, value), exception);
            return;
        case ElementInnerText:
            element.setInnerText(valueToStringWithNullCheck(exec, value), exception);
            return;
        case ElementOuterHTML:
            element.setOuterHTML(valueToStringWithNullCheck(exec, value), exception);
            return;
        case ElementOuterText:
            element.setOuterText(valueToStringWithNullCheck(exec, value), exception);
            return;
        case ElementContentEditable:
            element.setContentEditable(valueToStringWithNullCheck(exec, value));
            return;
    }

    // Now check for properties that apply to a specific element type.
    const Accessors* access = accessors();
    if (access && access->m_setter)
        return (this->*(access->m_setter))(exec, token, value);
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

} // namespace
