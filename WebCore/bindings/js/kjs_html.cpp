// -*- c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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

#include "config.h"
#include "kjs_html.h"

#include "DocLoader.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLAppletElement.h"
#include "HTMLAreaElement.h"
#include "HTMLBaseFontElement.h"
#include "HTMLBlockquoteElement.h"
#include "HTMLBodyElement.h"
#include "HTMLBRElement.h"
#include "HTMLDocument.h"
#include "HTMLEmbedElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFontElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHeadingElement.h"
#include "HTMLHRElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLIsIndexElement.h"
#include "HTMLLabelElement.h"
#include "HTMLLegendElement.h"
#include "HTMLLIElement.h"
#include "HTMLMapElement.h"
#include "HTMLMenuElement.h"
#include "HTMLModElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "HTMLParagraphElement.h"
#include "HTMLParamElement.h"
#include "HTMLPreElement.h"
#include "HTMLScriptElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableColElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLQuoteElement.h"
#include "JSHTMLImageElement.h"
#include "NameNodeList.h"
#include "RenderLayer.h"
#include "Text.h"
#include "dom2_eventsimpl.h"
#include "kjs_css.h"
#include "kjs_events.h"
#include "kjs_proxy.h"
#include "kjs_window.h"

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
@begin JSHTMLDocumentProtoTable 8
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
KJS_IMPLEMENT_PROTOFUNC(JSHTMLDocumentProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("HTMLDocument", JSHTMLDocumentProto, JSHTMLDocumentProtoFunc)

JSValue *JSHTMLDocumentProtoFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
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
  { "HTMLDocument", &JSDocument::info, &HTMLDocumentTable, 0 };
/* Source for HTMLDocumentTable. Use "make hashtables" to regenerate.
@begin HTMLDocumentTable 30
  title                 JSHTMLDocument::Title             DontDelete
  referrer              JSHTMLDocument::Referrer          DontDelete|ReadOnly
  domain                JSHTMLDocument::Domain            DontDelete
  URL                   JSHTMLDocument::URL               DontDelete|ReadOnly
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
    setPrototype(JSHTMLDocumentProto::self(exec));
}

JSValue *JSHTMLDocument::namedItemGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  JSHTMLDocument *thisObj = static_cast<JSHTMLDocument*>(slot.slotBase());
  HTMLDocument &doc = *static_cast<HTMLDocument*>(thisObj->impl());

  String name = propertyName;
  RefPtr<WebCore::HTMLCollection> collection = doc.documentNamedItems(name);

  if (collection->length() == 1) {
    WebCore::Node* node = collection->firstItem();
    Frame *frame;
    if (node->hasTagName(iframeTag) && (frame = static_cast<WebCore::HTMLIFrameElement*>(node)->contentFrame()))
      return Window::retrieve(frame);
    return toJS(exec, node);
  }

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
  case URL:
    return jsString(doc.URL());
  case Body:
    return toJS(exec, body);
  case Location:
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
    if (!bodyElement)
      return jsUndefined();
    return jsString(bodyElement->dir());
  case DesignMode:
    return jsString(doc.inDesignMode() ? "on" : "off");
  default:
    assert(0);
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

  const HashEntry* entry = Lookup::findEntry(&HTMLDocumentTable, propertyName);
  if (entry) {
    slot.setStaticEntry(this, entry, staticValueGetter<JSHTMLDocument>);
    return true;
  }

  return JSDocument::getOwnPropertySlot(exec, propertyName, slot);
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
    doc.setTitle(value->toString(exec));
    break;
  case Body:
    doc.setBody(toHTMLElement(value), exception);
    break;
  case Domain: // not part of the DOM
    doc.setDomain(value->toString(exec));
    break;
  case Cookie:
    doc.setCookie(value->toString(exec));
    break;
  case Location: {
    Frame *frame = doc.frame();
    if (frame)
    {
      DeprecatedString str = value->toString(exec);

      // When assigning location, IE and Mozilla both resolve the URL
      // relative to the frame where the JavaScript is executing not
      // the target frame.
      Frame *activePart = static_cast<ScriptInterpreter*>( exec->dynamicInterpreter() )->frame();
      if (activePart)
        str = activePart->document()->completeURL(str);

      // We want a new history item if this JS was called via a user gesture
      bool userGesture = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter())->wasRunByUserGesture();
      frame->scheduleLocationChange(str, activePart->referrer(), !userGesture);
    }
    break;
  }
  case BgColor:
    if (bodyElement)
      bodyElement->setBgColor(value->toString(exec));
    break;
  case FgColor:
    if (bodyElement)
      bodyElement->setText(value->toString(exec));
    break;
  case AlinkColor:
    if (bodyElement) {
      // this check is a bit silly, but some benchmarks like to set the
      // document's link colors over and over to the same value and we
      // don't want to incur a style update each time.
      String newColor = value->toString(exec);
      if (bodyElement->aLink() != newColor)
        bodyElement->setALink(newColor);
    }
    break;
  case LinkColor:
    if (bodyElement) {
      // this check is a bit silly, but some benchmarks like to set the
      // document's link colors over and over to the same value and we
      // don't want to incur a style update each time.
      String newColor = value->toString(exec);
      if (bodyElement->link() != newColor)
        bodyElement->setLink(newColor);
    }
    break;
  case VlinkColor:
    if (bodyElement) {
      // this check is a bit silly, but some benchmarks like to set the
      // document's link colors over and over to the same value and we
      // don't want to incur a style update each time.
      String newColor = value->toString(exec);
      if (bodyElement->vLink() != newColor)
        bodyElement->setVLink(newColor);
    }
    break;
  case Dir:
    body->setDir(value->toString(exec));
    break;
  case DesignMode:
    {
      String modeString = value->toString(exec);
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

const ClassInfo JSHTMLElement::caption_info = { "HTMLTableCaptionElement", &JSHTMLElement::info, &HTMLTableCaptionElementTable, 0 };
const ClassInfo JSHTMLElement::col_info = { "HTMLTableColElement", &JSHTMLElement::info, &HTMLTableColElementTable, 0 };
const ClassInfo JSHTMLElement::embed_info = { "HTMLEmbedElement", &JSHTMLElement::info, &HTMLEmbedElementTable, 0 };
const ClassInfo JSHTMLElement::frameSet_info = { "HTMLFrameSetElement", &JSHTMLElement::info, &HTMLFrameSetElementTable, 0 };
const ClassInfo JSHTMLElement::frame_info = { "HTMLFrameElement", &JSHTMLElement::info, &HTMLFrameElementTable, 0 };
const ClassInfo JSHTMLElement::iFrame_info = { "HTMLIFrameElement", &JSHTMLElement::info, &HTMLIFrameElementTable, 0 };
const ClassInfo JSHTMLElement::info = { "HTMLElement", &JSElement::info, &HTMLElementTable, 0 };
const ClassInfo JSHTMLElement::marquee_info = { "HTMLMarqueeElement", &JSHTMLElement::info, &HTMLMarqueeElementTable, 0 };
const ClassInfo JSHTMLElement::object_info = { "HTMLObjectElement", &JSHTMLElement::info, &HTMLObjectElementTable, 0 };
const ClassInfo JSHTMLElement::select_info = { "HTMLSelectElement", &JSHTMLElement::info, &HTMLSelectElementTable, 0 };
const ClassInfo JSHTMLElement::table_info = { "HTMLTableElement", &JSHTMLElement::info, &HTMLTableElementTable, 0 };
const ClassInfo JSHTMLElement::tablecell_info = { "HTMLTableCellElement", &JSHTMLElement::info, &HTMLTableCellElementTable, 0 };
const ClassInfo JSHTMLElement::tablesection_info = { "HTMLTableSectionElement", &JSHTMLElement::info, &HTMLTableSectionElementTable, 0 };
const ClassInfo JSHTMLElement::tr_info = { "HTMLTableRowElement", &JSHTMLElement::info, &HTMLTableRowElementTable, 0 };

const ClassInfo* JSHTMLElement::classInfo() const
{
    static HashMap<WebCore::AtomicStringImpl*, const ClassInfo*> classInfoMap;
    if (classInfoMap.isEmpty()) {
        classInfoMap.set(captionTag.localName().impl(), &caption_info);
        classInfoMap.set(colTag.localName().impl(), &col_info);
        classInfoMap.set(colgroupTag.localName().impl(), &col_info);
        classInfoMap.set(embedTag.localName().impl(), &embed_info);
        classInfoMap.set(frameTag.localName().impl(), &frame_info);
        classInfoMap.set(framesetTag.localName().impl(), &frameSet_info);
        classInfoMap.set(iframeTag.localName().impl(), &iFrame_info);
        classInfoMap.set(marqueeTag.localName().impl(), &marquee_info);
        classInfoMap.set(objectTag.localName().impl(), &object_info);
        classInfoMap.set(selectTag.localName().impl(), &select_info);
        classInfoMap.set(tableTag.localName().impl(), &table_info);
        classInfoMap.set(tbodyTag.localName().impl(), &tablesection_info);
        classInfoMap.set(tdTag.localName().impl(), &tablecell_info);
        classInfoMap.set(tfootTag.localName().impl(), &tablesection_info);
        classInfoMap.set(thTag.localName().impl(), &tablecell_info);
        classInfoMap.set(theadTag.localName().impl(), &tablesection_info);
        classInfoMap.set(trTag.localName().impl(), &tr_info);
    }
    
    HTMLElement* element = static_cast<HTMLElement*>(impl());
    const ClassInfo* result = classInfoMap.get(element->localName().impl());
    if (result)
        return result;
    return &info;
}

const JSHTMLElement::Accessors JSHTMLElement::select_accessors = { &JSHTMLElement::selectGetter, &JSHTMLElement::selectSetter };
const JSHTMLElement::Accessors JSHTMLElement::object_accessors = { &JSHTMLElement::objectGetter, &JSHTMLElement::objectSetter };
const JSHTMLElement::Accessors JSHTMLElement::embed_accessors = { &JSHTMLElement::embedGetter, &JSHTMLElement::embedSetter };
const JSHTMLElement::Accessors JSHTMLElement::table_accessors = { &JSHTMLElement::tableGetter, &JSHTMLElement::tableSetter };
const JSHTMLElement::Accessors JSHTMLElement::caption_accessors = { &JSHTMLElement::tableCaptionGetter, &JSHTMLElement::tableCaptionSetter };
const JSHTMLElement::Accessors JSHTMLElement::col_accessors = { &JSHTMLElement::tableColGetter, &JSHTMLElement::tableColSetter };
const JSHTMLElement::Accessors JSHTMLElement::tablesection_accessors = { &JSHTMLElement::tableSectionGetter, &JSHTMLElement::tableSectionSetter };
const JSHTMLElement::Accessors JSHTMLElement::tr_accessors = { &JSHTMLElement::tableRowGetter, &JSHTMLElement::tableRowSetter };
const JSHTMLElement::Accessors JSHTMLElement::tablecell_accessors = { &JSHTMLElement::tableCellGetter, &JSHTMLElement::tableCellSetter };
const JSHTMLElement::Accessors JSHTMLElement::frameSet_accessors = { &JSHTMLElement::frameSetGetter, &JSHTMLElement::frameSetSetter };
const JSHTMLElement::Accessors JSHTMLElement::frame_accessors = { &JSHTMLElement::frameGetter, &JSHTMLElement::frameSetter };
const JSHTMLElement::Accessors JSHTMLElement::iFrame_accessors = { &JSHTMLElement::iFrameGetter, &JSHTMLElement::iFrameSetter };
const JSHTMLElement::Accessors JSHTMLElement::marquee_accessors = { &JSHTMLElement::marqueeGetter, &JSHTMLElement::marqueeSetter };

const JSHTMLElement::Accessors* JSHTMLElement::accessors() const
{
    static HashMap<WebCore::AtomicStringImpl*, const Accessors*> accessorMap;
    if (accessorMap.isEmpty()) {
        accessorMap.add(captionTag.localName().impl(), &caption_accessors);
        accessorMap.add(colTag.localName().impl(), &col_accessors);
        accessorMap.add(colgroupTag.localName().impl(), &col_accessors);
        accessorMap.add(embedTag.localName().impl(), &embed_accessors);
        accessorMap.add(frameTag.localName().impl(), &frame_accessors);
        accessorMap.add(framesetTag.localName().impl(), &frameSet_accessors);
        accessorMap.add(iframeTag.localName().impl(), &iFrame_accessors);
        accessorMap.add(marqueeTag.localName().impl(), &marquee_accessors);
        accessorMap.add(objectTag.localName().impl(), &object_accessors);
        accessorMap.add(selectTag.localName().impl(), &select_accessors);
        accessorMap.add(tableTag.localName().impl(), &table_accessors);
        accessorMap.add(tbodyTag.localName().impl(), &tablesection_accessors);
        accessorMap.add(tdTag.localName().impl(), &tablecell_accessors);
        accessorMap.add(thTag.localName().impl(), &tablecell_accessors);
        accessorMap.add(theadTag.localName().impl(), &tablesection_accessors);
        accessorMap.add(tfootTag.localName().impl(), &tablesection_accessors);
        accessorMap.add(trTag.localName().impl(), &tr_accessors);
    }
    
    HTMLElement* element = static_cast<HTMLElement*>(impl());
    return accessorMap.get(element->localName().impl());
}

/*

@begin JSHTMLElementProtoTable 0
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
@begin HTMLSelectElementTable 11
# Also supported, by index
  type          KJS::JSHTMLElement::SelectType    DontDelete|ReadOnly
  selectedIndex KJS::JSHTMLElement::SelectSelectedIndex   DontDelete
  value         KJS::JSHTMLElement::SelectValue   DontDelete
  length        KJS::JSHTMLElement::SelectLength  DontDelete
  form          KJS::JSHTMLElement::SelectForm    DontDelete|ReadOnly
  options       KJS::JSHTMLElement::SelectOptions DontDelete|ReadOnly
  namedItem     KJS::JSHTMLElement::SelectNamedItem       DontDelete|Function 1
  disabled      KJS::JSHTMLElement::SelectDisabled        DontDelete
  multiple      KJS::JSHTMLElement::SelectMultiple        DontDelete
  name          KJS::JSHTMLElement::SelectName    DontDelete
  size          KJS::JSHTMLElement::SelectSize    DontDelete
  tabIndex      KJS::JSHTMLElement::SelectTabIndex        DontDelete
  add           KJS::JSHTMLElement::SelectAdd     DontDelete|Function 2
  remove        KJS::JSHTMLElement::SelectRemove  DontDelete|Function 1
  blur          KJS::JSHTMLElement::SelectBlur    DontDelete|Function 0
  focus         KJS::JSHTMLElement::SelectFocus   DontDelete|Function 0
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
  name          KJS::JSHTMLElement::EmbedName            DontDelete
  src           KJS::JSHTMLElement::EmbedSrc             DontDelete
  type          KJS::JSHTMLElement::EmbedType            DontDelete
  width         KJS::JSHTMLElement::EmbedWidth           DontDelete
@end
@begin HTMLTableElementTable 23
  caption       KJS::JSHTMLElement::TableCaption          DontDelete
  tHead         KJS::JSHTMLElement::TableTHead            DontDelete
  tFoot         KJS::JSHTMLElement::TableTFoot            DontDelete
  rows          KJS::JSHTMLElement::TableRows             DontDelete|ReadOnly
  tBodies       KJS::JSHTMLElement::TableTBodies          DontDelete|ReadOnly
  align         KJS::JSHTMLElement::TableAlign            DontDelete
  bgColor       KJS::JSHTMLElement::TableBgColor          DontDelete
  border        KJS::JSHTMLElement::TableBorder           DontDelete
  cellPadding   KJS::JSHTMLElement::TableCellPadding      DontDelete
  cellSpacing   KJS::JSHTMLElement::TableCellSpacing      DontDelete
  frame         KJS::JSHTMLElement::TableFrame            DontDelete
  rules         KJS::JSHTMLElement::TableRules            DontDelete
  summary       KJS::JSHTMLElement::TableSummary          DontDelete
  width         KJS::JSHTMLElement::TableWidth            DontDelete
  createTHead   KJS::JSHTMLElement::TableCreateTHead      DontDelete|Function 0
  deleteTHead   KJS::JSHTMLElement::TableDeleteTHead      DontDelete|Function 0
  createTFoot   KJS::JSHTMLElement::TableCreateTFoot      DontDelete|Function 0
  deleteTFoot   KJS::JSHTMLElement::TableDeleteTFoot      DontDelete|Function 0
  createCaption KJS::JSHTMLElement::TableCreateCaption    DontDelete|Function 0
  deleteCaption KJS::JSHTMLElement::TableDeleteCaption    DontDelete|Function 0
  insertRow     KJS::JSHTMLElement::TableInsertRow        DontDelete|Function 1
  deleteRow     KJS::JSHTMLElement::TableDeleteRow        DontDelete|Function 1
@end
@begin HTMLTableCaptionElementTable 1
  align         KJS::JSHTMLElement::TableCaptionAlign     DontDelete
@end
@begin HTMLTableColElementTable 7
  align         KJS::JSHTMLElement::TableColAlign         DontDelete
  ch            KJS::JSHTMLElement::TableColCh            DontDelete
  chOff         KJS::JSHTMLElement::TableColChOff         DontDelete
  span          KJS::JSHTMLElement::TableColSpan          DontDelete
  vAlign        KJS::JSHTMLElement::TableColVAlign        DontDelete
  width         KJS::JSHTMLElement::TableColWidth         DontDelete
@end
@begin HTMLTableSectionElementTable 7
  align         KJS::JSHTMLElement::TableSectionAlign             DontDelete
  ch            KJS::JSHTMLElement::TableSectionCh                DontDelete
  chOff         KJS::JSHTMLElement::TableSectionChOff             DontDelete
  vAlign        KJS::JSHTMLElement::TableSectionVAlign            DontDelete
  rows          KJS::JSHTMLElement::TableSectionRows              DontDelete|ReadOnly
  insertRow     KJS::JSHTMLElement::TableSectionInsertRow         DontDelete|Function 1
  deleteRow     KJS::JSHTMLElement::TableSectionDeleteRow         DontDelete|Function 1
@end
@begin HTMLTableRowElementTable 11
  rowIndex      KJS::JSHTMLElement::TableRowRowIndex              DontDelete|ReadOnly
  sectionRowIndex KJS::JSHTMLElement::TableRowSectionRowIndex     DontDelete|ReadOnly
  cells         KJS::JSHTMLElement::TableRowCells                 DontDelete|ReadOnly
  align         KJS::JSHTMLElement::TableRowAlign                 DontDelete
  bgColor       KJS::JSHTMLElement::TableRowBgColor               DontDelete
  ch            KJS::JSHTMLElement::TableRowCh                    DontDelete
  chOff         KJS::JSHTMLElement::TableRowChOff                 DontDelete
  vAlign        KJS::JSHTMLElement::TableRowVAlign                DontDelete
  insertCell    KJS::JSHTMLElement::TableRowInsertCell            DontDelete|Function 1
  deleteCell    KJS::JSHTMLElement::TableRowDeleteCell            DontDelete|Function 1
@end
@begin HTMLTableCellElementTable 15
  cellIndex     KJS::JSHTMLElement::TableCellCellIndex            DontDelete|ReadOnly
  abbr          KJS::JSHTMLElement::TableCellAbbr                 DontDelete
  align         KJS::JSHTMLElement::TableCellAlign                DontDelete
  axis          KJS::JSHTMLElement::TableCellAxis                 DontDelete
  bgColor       KJS::JSHTMLElement::TableCellBgColor              DontDelete
  ch            KJS::JSHTMLElement::TableCellCh                   DontDelete
  chOff         KJS::JSHTMLElement::TableCellChOff                DontDelete
  colSpan       KJS::JSHTMLElement::TableCellColSpan              DontDelete
  headers       KJS::JSHTMLElement::TableCellHeaders              DontDelete
  height        KJS::JSHTMLElement::TableCellHeight               DontDelete
  noWrap        KJS::JSHTMLElement::TableCellNoWrap               DontDelete
  rowSpan       KJS::JSHTMLElement::TableCellRowSpan              DontDelete
  scope         KJS::JSHTMLElement::TableCellScope                DontDelete
  vAlign        KJS::JSHTMLElement::TableCellVAlign               DontDelete
  width         KJS::JSHTMLElement::TableCellWidth                DontDelete
@end
@begin HTMLFrameSetElementTable 2
cols          KJS::JSHTMLElement::FrameSetCols                  DontDelete
rows          KJS::JSHTMLElement::FrameSetRows                  DontDelete
@end
@begin HTMLFrameElementTable 9
  contentDocument KJS::JSHTMLElement::FrameContentDocument        DontDelete|ReadOnly
  contentWindow   KJS::JSHTMLElement::FrameContentWindow          DontDelete|ReadOnly
  frameBorder     KJS::JSHTMLElement::FrameFrameBorder            DontDelete
  longDesc        KJS::JSHTMLElement::FrameLongDesc               DontDelete
  marginHeight    KJS::JSHTMLElement::FrameMarginHeight           DontDelete
  marginWidth     KJS::JSHTMLElement::FrameMarginWidth            DontDelete
  name            KJS::JSHTMLElement::FrameName                   DontDelete
  noResize        KJS::JSHTMLElement::FrameNoResize               DontDelete
  width           KJS::JSHTMLElement::FrameWidth                  DontDelete|ReadOnly
  height          KJS::JSHTMLElement::FrameHeight                 DontDelete|ReadOnly
  scrolling       KJS::JSHTMLElement::FrameScrolling              DontDelete
  src             KJS::JSHTMLElement::FrameSrc                    DontDelete
  location        KJS::JSHTMLElement::FrameLocation               DontDelete
@end
@begin HTMLIFrameElementTable 12
  align           KJS::JSHTMLElement::IFrameAlign                 DontDelete
  contentDocument KJS::JSHTMLElement::IFrameContentDocument       DontDelete|ReadOnly
  contentWindow   KJS::JSHTMLElement::IFrameContentWindow         DontDelete|ReadOnly
  document        KJS::JSHTMLElement::IFrameDocument              DontDelete|ReadOnly
  frameBorder     KJS::JSHTMLElement::IFrameFrameBorder           DontDelete
  height          KJS::JSHTMLElement::IFrameHeight                DontDelete
  longDesc        KJS::JSHTMLElement::IFrameLongDesc              DontDelete
  marginHeight    KJS::JSHTMLElement::IFrameMarginHeight          DontDelete
  marginWidth     KJS::JSHTMLElement::IFrameMarginWidth           DontDelete
  name            KJS::JSHTMLElement::IFrameName                  DontDelete
  scrolling       KJS::JSHTMLElement::IFrameScrolling             DontDelete
  src             KJS::JSHTMLElement::IFrameSrc                   DontDelete
  width           KJS::JSHTMLElement::IFrameWidth                 DontDelete
@end

@begin HTMLMarqueeElementTable 2
  start           KJS::JSHTMLElement::MarqueeStart                DontDelete|Function 0
  stop            KJS::JSHTMLElement::MarqueeStop                 DontDelete|Function 0
@end
*/

KJS_IMPLEMENT_PROTOFUNC(JSHTMLElementProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("HTMLElement", JSHTMLElementProto, JSHTMLElementProtoFunc)

JSValue* JSHTMLElementProtoFunc::callAsFunction(ExecState*, JSObject*, const List&)
{
    return 0;
}

JSHTMLElement::JSHTMLElement(ExecState* exec, HTMLElement* e)
    : WebCore::JSHTMLElement(exec, e)
{
    setPrototype(JSHTMLElementProto::self(exec));
}

JSValue *JSHTMLElement::selectIndexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(thisObj->impl());

    return toJS(exec, select->options()->item(slot.index()));
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
    if (element.hasLocalName(selectTag)) {
        bool ok;
        unsigned u = propertyName.toUInt32(&ok);
        if (ok) {
            // not specified by DOM(?) but supported in netscape/IE
            slot.setCustomIndex(this, u, selectIndexGetter);
            return true;
        }
    } else if (element.hasLocalName(framesetTag)) {
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
        Document* doc = element->document();
        KJSProxy *proxy = doc->frame()->jScript();
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

JSValue *JSHTMLElement::selectGetter(ExecState* exec, int token) const
{
    HTMLSelectElement& select = *static_cast<HTMLSelectElement*>(impl());
    switch (token) {
        case SelectType:            return jsString(select.type());
        case SelectSelectedIndex:   return jsNumber(select.selectedIndex());
        case SelectValue:           return jsString(select.value());
        case SelectLength:          return jsNumber(select.length());
        case SelectForm:            return toJS(exec, select.form()); // type HTMLFormElement
        case SelectOptions:         return getSelectHTMLCollection(exec, select.options().get(), &select); // type JSHTMLCollection
        case SelectDisabled:        return jsBoolean(select.disabled());
        case SelectMultiple:        return jsBoolean(select.multiple());
        case SelectName:            return jsString(select.name());
        case SelectSize:            return jsNumber(select.size());
        case SelectTabIndex:        return jsNumber(select.tabIndex());
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
        case ObjectHspace:          return jsString(object.hspace());
        case ObjectName:            return jsString(object.name());
        case ObjectStandby:         return jsString(object.standby());
        case ObjectTabIndex:        return jsNumber(object.tabIndex());
        case ObjectType:            return jsString(object.type());
        case ObjectUseMap:          return jsString(object.useMap());
        case ObjectVspace:          return jsString(object.vspace());
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

#ifdef FIXME
    HTMLAreaElement& area = *static_cast<HTMLAreaElement*>(impl());
    switch (token) {
        case AreaAccessKey:       return jsString(area.accessKey());
        case AreaAlt:             return jsString(area.alt());
        case AreaCoords:          return jsString(area.coords());
        case AreaHref:            return jsString(area.href());
        case AreaHash:            return jsString('#'+KURL(area.href().deprecatedString()).ref());
        case AreaHost:            return jsString(KURL(area.href().deprecatedString()).host());
        case AreaHostName: {
            KURL url(area.href().deprecatedString());
            if (url.port()==0)
                return jsString(url.host());
            else
                return jsString(url.host() + ":" + DeprecatedString::number(url.port()));
        }
        case AreaPathName:        return jsString(KURL(area.href().deprecatedString()).path());
        case AreaPort:            return jsString(DeprecatedString::number(KURL(area.href().deprecatedString()).port()));
        case AreaProtocol:        return jsString(KURL(area.href().deprecatedString()).protocol()+":");
        case AreaSearch:          return jsString(KURL(area.href().deprecatedString()).query());
        case AreaNoHref:          return jsBoolean(area.noHref());
        case AreaShape:           return jsString(area.shape());
        case AreaTabIndex:        return jsNumber(area.tabIndex());
        case AreaTarget:          return jsString(area.target());
    }
#endif

JSValue *JSHTMLElement::tableGetter(ExecState* exec, int token) const
{
    HTMLTableElement& table = *static_cast<HTMLTableElement*>(impl());
    switch (token) {
        case TableCaption:         return toJS(exec,table.caption()); // type HTMLTableCaptionElement
        case TableTHead:           return toJS(exec,table.tHead()); // type HTMLTableSectionElement
        case TableTFoot:           return toJS(exec,table.tFoot()); // type HTMLTableSectionElement
        case TableRows:            return getHTMLCollection(exec, table.rows().get()); // type JSHTMLCollection
        case TableTBodies:         return getHTMLCollection(exec, table.tBodies().get()); // type JSHTMLCollection
        case TableAlign:           return jsString(table.align());
        case TableBgColor:         return jsString(table.bgColor());
        case TableBorder:          return jsString(table.border());
        case TableCellPadding:     return jsString(table.cellPadding());
        case TableCellSpacing:     return jsString(table.cellSpacing());
        case TableFrame:           return jsString(table.frame());
        case TableRules:           return jsString(table.rules());
        case TableSummary:         return jsString(table.summary());
        case TableWidth:           return jsString(table.width());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::tableCaptionGetter(ExecState* exec, int token) const
{
    HTMLTableCaptionElement& tableCaption = *static_cast<HTMLTableCaptionElement*>(impl());
    if (token == TableCaptionAlign)
        return jsString(tableCaption.align());
    return jsUndefined();
}

JSValue *JSHTMLElement::tableColGetter(ExecState* exec, int token) const
{
    HTMLTableColElement& tableCol = *static_cast<HTMLTableColElement*>(impl());
    switch (token) {
        case TableColAlign:           return jsString(tableCol.align());
        case TableColCh:              return jsString(tableCol.ch());
        case TableColChOff:           return jsString(tableCol.chOff());
        case TableColSpan:            return jsNumber(tableCol.span());
        case TableColVAlign:          return jsString(tableCol.vAlign());
        case TableColWidth:           return jsString(tableCol.width());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::tableSectionGetter(ExecState* exec, int token) const
{
    HTMLTableSectionElement& tableSection = *static_cast<HTMLTableSectionElement*>(impl());
    switch (token) {
        case TableSectionAlign:           return jsString(tableSection.align());
        case TableSectionCh:              return jsString(tableSection.ch());
        case TableSectionChOff:           return jsString(tableSection.chOff());
        case TableSectionVAlign:          return jsString(tableSection.vAlign());
        case TableSectionRows:            return getHTMLCollection(exec, tableSection.rows().get()); // type JSHTMLCollection
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::tableRowGetter(ExecState* exec, int token) const
{
    HTMLTableRowElement& tableRow = *static_cast<HTMLTableRowElement*>(impl());
    switch (token) {
        case TableRowRowIndex:        return jsNumber(tableRow.rowIndex());
        case TableRowSectionRowIndex: return jsNumber(tableRow.sectionRowIndex());
        case TableRowCells:           return getHTMLCollection(exec, tableRow.cells().get()); // type JSHTMLCollection
        case TableRowAlign:           return jsString(tableRow.align());
        case TableRowBgColor:         return jsString(tableRow.bgColor());
        case TableRowCh:              return jsString(tableRow.ch());
        case TableRowChOff:           return jsString(tableRow.chOff());
        case TableRowVAlign:          return jsString(tableRow.vAlign());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::tableCellGetter(ExecState* exec, int token) const
{
    HTMLTableCellElement& tableCell = *static_cast<HTMLTableCellElement*>(impl());
    switch (token) {
        case TableCellCellIndex:       return jsNumber(tableCell.cellIndex());
        case TableCellAbbr:            return jsString(tableCell.abbr());
        case TableCellAlign:           return jsString(tableCell.align());
        case TableCellAxis:            return jsString(tableCell.axis());
        case TableCellBgColor:         return jsString(tableCell.bgColor());
        case TableCellCh:              return jsString(tableCell.ch());
        case TableCellChOff:           return jsString(tableCell.chOff());
        case TableCellColSpan:         return jsNumber(tableCell.colSpan());
        case TableCellHeaders:         return jsString(tableCell.headers());
        case TableCellHeight:          return jsString(tableCell.height());
        case TableCellNoWrap:          return jsBoolean(tableCell.noWrap());
        case TableCellRowSpan:         return jsNumber(tableCell.rowSpan());
        case TableCellScope:           return jsString(tableCell.scope());
        case TableCellVAlign:          return jsString(tableCell.vAlign());
        case TableCellWidth:           return jsString(tableCell.width());
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

JSValue *JSHTMLElement::frameGetter(ExecState* exec, int token) const
{
    HTMLFrameElement& frameElement = *static_cast<HTMLFrameElement*>(impl());
    switch (token) {
        case FrameContentDocument: return checkNodeSecurity(exec,frameElement.contentDocument()) ? 
                                          toJS(exec, frameElement.contentDocument()) : jsUndefined();
        case FrameContentWindow:   return checkNodeSecurity(exec,frameElement.contentDocument())
                                        ? Window::retrieve(frameElement.contentFrame())
                                        : jsUndefined();
        case FrameFrameBorder:     return jsString(frameElement.frameBorder());
        case FrameLongDesc:        return jsString(frameElement.longDesc());
        case FrameMarginHeight:    return jsString(frameElement.marginHeight());
        case FrameMarginWidth:     return jsString(frameElement.marginWidth());
        case FrameName:            return jsString(frameElement.name());
        case FrameNoResize:        return jsBoolean(frameElement.noResize());
        case FrameWidth:           return jsNumber(frameElement.frameWidth());
        case FrameHeight:          return jsNumber(frameElement.frameHeight());
        case FrameScrolling:       return jsString(frameElement.scrolling());
        case FrameSrc:
        case FrameLocation:        return jsString(frameElement.src());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::iFrameGetter(ExecState* exec, int token) const
{
    HTMLIFrameElement& iFrame = *static_cast<HTMLIFrameElement*>(impl());
    switch (token) {
        case IFrameAlign:                return jsString(iFrame.align());
          // ### security check ?
        case IFrameDocument: // non-standard, mapped to contentDocument
        case IFrameContentDocument: return checkNodeSecurity(exec,iFrame.contentDocument()) ? 
                                      toJS(exec, iFrame.contentDocument()) : jsUndefined();
        case IFrameContentWindow:   return checkNodeSecurity(exec,iFrame.contentDocument()) 
                                        ? Window::retrieve(iFrame.contentFrame())
                                        : jsUndefined();
        case IFrameFrameBorder:     return jsString(iFrame.frameBorder());
        case IFrameHeight:          return jsString(iFrame.height());
        case IFrameLongDesc:        return jsString(iFrame.longDesc());
        case IFrameMarginHeight:    return jsString(iFrame.marginHeight());
        case IFrameMarginWidth:     return jsString(iFrame.marginWidth());
        case IFrameName:            return jsString(iFrame.name());
        case IFrameScrolling:       return jsString(iFrame.scrolling());
        case IFrameSrc:             return jsString(iFrame.src());
        case IFrameWidth:           return jsString(iFrame.width());
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
  put(exec,lengthPropertyName,jsNumber(len),DontDelete|ReadOnly|DontEnum);
}

JSValue *HTMLElementFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
    if (!thisObj->inherits(&JSHTMLElement::info))
        return throwError(exec, TypeError);
    DOMExceptionTranslator exception(exec);
    HTMLElement &element = *static_cast<HTMLElement*>(static_cast<JSHTMLElement*>(thisObj)->impl());

    if (element.hasLocalName(selectTag)) {
        HTMLSelectElement &select = static_cast<HTMLSelectElement &>(element);
        if (id == JSHTMLElement::SelectAdd) {
            select.add(toHTMLElement(args[0]), toHTMLElement(args[1]), exception);
            return jsUndefined();
        }
        else if (id == JSHTMLElement::SelectRemove) {
            select.remove(int(args[0]->toNumber(exec)));
            return jsUndefined();
        }
        else if (id == JSHTMLElement::SelectBlur) {
            select.blur();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::SelectFocus) {
            select.focus();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::SelectNamedItem) {
            return toJS(exec, select.namedItem(Identifier(args[0]->toString(exec))));
        }
    }
    else if (element.hasLocalName(tableTag)) {
        HTMLTableElement &table = static_cast<HTMLTableElement &>(element);
        if (id == JSHTMLElement::TableCreateTHead)
            return toJS(exec,table.createTHead());
        else if (id == JSHTMLElement::TableDeleteTHead) {
            table.deleteTHead();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::TableCreateTFoot)
            return toJS(exec,table.createTFoot());
        else if (id == JSHTMLElement::TableDeleteTFoot) {
            table.deleteTFoot();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::TableCreateCaption)
            return toJS(exec,table.createCaption());
        else if (id == JSHTMLElement::TableDeleteCaption) {
            table.deleteCaption();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::TableInsertRow)
            return toJS(exec,table.insertRow(args[0]->toInt32(exec), exception));
        else if (id == JSHTMLElement::TableDeleteRow) {
            table.deleteRow(args[0]->toInt32(exec), exception);
            return jsUndefined();
        }
    }
    else if (element.hasLocalName(theadTag) ||
             element.hasLocalName(tbodyTag) ||
             element.hasLocalName(tfootTag)) {
        HTMLTableSectionElement &tableSection = static_cast<HTMLTableSectionElement &>(element);
        if (id == JSHTMLElement::TableSectionInsertRow)
            return toJS(exec, tableSection.insertRow(args[0]->toInt32(exec), exception));
        else if (id == JSHTMLElement::TableSectionDeleteRow) {
            tableSection.deleteRow(args[0]->toInt32(exec), exception);
            return jsUndefined();
        }
    }
    else if (element.hasLocalName(trTag)) {
        HTMLTableRowElement &tableRow = static_cast<HTMLTableRowElement &>(element);
        if (id == JSHTMLElement::TableRowInsertCell)
            return toJS(exec,tableRow.insertCell(args[0]->toInt32(exec), exception));
        else if (id == JSHTMLElement::TableRowDeleteCell) {
            tableRow.deleteCell(args[0]->toInt32(exec), exception);
            return jsUndefined();
        }
    }
    else if (element.hasLocalName(marqueeTag)) {
        if (id == JSHTMLElement::MarqueeStart && element.renderer() && 
            element.renderer()->layer() &&
            element.renderer()->layer()->marquee()) {
            element.renderer()->layer()->marquee()->start();
            return jsUndefined();
        }
        if (id == JSHTMLElement::MarqueeStop && element.renderer() && 
            element.renderer()->layer() &&
            element.renderer()->layer()->marquee()) {
            element.renderer()->layer()->marquee()->stop();
            return jsUndefined();
        }
    }

    return jsUndefined();
}

void JSHTMLElement::put(ExecState* exec, const Identifier &propertyName, JSValue *value, int attr)
{
    HTMLElement &element = *static_cast<HTMLElement*>(impl());
    // First look at dynamic properties
    if (element.hasLocalName(selectTag)) {
        HTMLSelectElement &select = static_cast<HTMLSelectElement &>(element);
        bool ok;
        /*unsigned u =*/ propertyName.toUInt32(&ok);
        if (ok) {
            JSObject* coll = static_cast<JSObject*>(getSelectHTMLCollection(exec, select.options().get(), &select));
            coll->put(exec,propertyName,value);
            return;
        }
    }
    else if (element.hasLocalName(embedTag) || element.hasLocalName(objectTag) || element.hasLocalName(appletTag)) {
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

void JSHTMLElement::selectSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLSelectElement& select = *static_cast<HTMLSelectElement*>(impl());
    switch (token) {
        // read-only: type
        case SelectSelectedIndex:   { select.setSelectedIndex(value->toInt32(exec)); return; }
        case SelectValue:           { select.setValue(str); return; }
        case SelectLength:          { // read-only according to the NS spec, but webpages need it writeable
                                        JSObject* coll = static_cast<JSObject*>(getSelectHTMLCollection(exec, select.options().get(), &select));
                                        coll->put(exec,lengthPropertyName,value);
                                        return;
                                    }
        // read-only: form
        // read-only: options
        case SelectDisabled:        { select.setDisabled(value->toBoolean(exec)); return; }
        case SelectMultiple:        { select.setMultiple(value->toBoolean(exec)); return; }
        case SelectName:            { select.setName(AtomicString(str)); return; }
        case SelectSize:            { select.setSize(value->toInt32(exec)); return; }
        case SelectTabIndex:        { select.setTabIndex(value->toInt32(exec)); return; }
    }
}

void JSHTMLElement::objectSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLObjectElement& object = *static_cast<HTMLObjectElement*>(impl());
    switch (token) {
        // read-only: form
        case ObjectCode:            { object.setCode(str); return; }
        case ObjectAlign:           { object.setAlign(str); return; }
        case ObjectArchive:         { object.setArchive(str); return; }
        case ObjectBorder:          { object.setBorder(str); return; }
        case ObjectCodeBase:        { object.setCodeBase(str); return; }
        case ObjectCodeType:        { object.setCodeType(str); return; }
        // read-only: ObjectContentDocument
        case ObjectData:            { object.setData(str); return; }
        case ObjectDeclare:         { object.setDeclare(value->toBoolean(exec)); return; }
        case ObjectHeight:          { object.setHeight(str); return; }
        case ObjectHspace:          { object.setHspace(str); return; }
        case ObjectName:            { object.setName(str); return; }
        case ObjectStandby:         { object.setStandby(str); return; }
        case ObjectTabIndex:        { object.setTabIndex(value->toInt32(exec)); return; }
        case ObjectType:            { object.setType(str); return; }
        case ObjectUseMap:          { object.setUseMap(str); return; }
        case ObjectVspace:          { object.setVspace(str); return; }
        case ObjectWidth:           { object.setWidth(str); return; }
    }
}

void JSHTMLElement::embedSetter(ExecState*, int token, JSValue*, const WebCore::String& str)
{
    HTMLEmbedElement& embed = *static_cast<HTMLEmbedElement*>(impl());
    switch (token) {
        case EmbedAlign:           { embed.setAlign(str); return; }
        case EmbedHeight:          { embed.setHeight(str); return; }
        case EmbedName:            { embed.setName(str); return; }
        case EmbedSrc:             { embed.setSrc(str); return; }
        case EmbedType:            { embed.setType(str); return; }
        case EmbedWidth:           { embed.setWidth(str); return; }
    }
}

void JSHTMLElement::tableSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLTableElement& table = *static_cast<HTMLTableElement*>(impl());
    switch (token) {
        case TableCaption:         { table.setCaption(toHTMLTableCaptionElement(value)); return; }
        case TableTHead:           { table.setTHead(toHTMLTableSectionElement(value)); return; }
        case TableTFoot:           { table.setTFoot(toHTMLTableSectionElement(value)); return; }
        // read-only: rows
        // read-only: tbodies
        case TableAlign:           { table.setAlign(str); return; }
        case TableBgColor:         { table.setBgColor(str); return; }
        case TableBorder:          { table.setBorder(str); return; }
        case TableCellPadding:     { table.setCellPadding(str); return; }
        case TableCellSpacing:     { table.setCellSpacing(str); return; }
        case TableFrame:           { table.setFrame(str); return; }
        case TableRules:           { table.setRules(str); return; }
        case TableSummary:         { table.setSummary(str); return; }
        case TableWidth:           { table.setWidth(str); return; }
    }
}

void JSHTMLElement::tableCaptionSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLTableCaptionElement& tableCaption = *static_cast<HTMLTableCaptionElement*>(impl());
    if (token == TableCaptionAlign)
        tableCaption.setAlign(str);
}

void JSHTMLElement::tableColSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLTableColElement& tableCol = *static_cast<HTMLTableColElement*>(impl());
    switch (token) {
        case TableColAlign:           { tableCol.setAlign(str); return; }
        case TableColCh:              { tableCol.setCh(str); return; }
        case TableColChOff:           { tableCol.setChOff(str); return; }
        case TableColSpan:            { tableCol.setSpan(value->toInt32(exec)); return; }
        case TableColVAlign:          { tableCol.setVAlign(str); return; }
        case TableColWidth:           { tableCol.setWidth(str); return; }
    }
}

void JSHTMLElement::tableSectionSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLTableSectionElement& tableSection = *static_cast<HTMLTableSectionElement*>(impl());
    switch (token) {
        case TableSectionAlign:           { tableSection.setAlign(str); return; }
        case TableSectionCh:              { tableSection.setCh(str); return; }
        case TableSectionChOff:           { tableSection.setChOff(str); return; }
        case TableSectionVAlign:          { tableSection.setVAlign(str); return; }
        // read-only: rows
    }
}

void JSHTMLElement::tableRowSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLTableRowElement& tableRow = *static_cast<HTMLTableRowElement*>(impl());
    switch (token) {
        // read-only: rowIndex
        // read-only: sectionRowIndex
        // read-only: cells
        case TableRowAlign:           { tableRow.setAlign(str); return; }
        case TableRowBgColor:         { tableRow.setBgColor(str); return; }
        case TableRowCh:              { tableRow.setCh(str); return; }
        case TableRowChOff:           { tableRow.setChOff(str); return; }
        case TableRowVAlign:          { tableRow.setVAlign(str); return; }
    }
}

void JSHTMLElement::tableCellSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLTableCellElement& tableCell = *static_cast<HTMLTableCellElement*>(impl());
    switch (token) {
        // read-only: cellIndex
        case TableCellAbbr:            { tableCell.setAbbr(str); return; }
        case TableCellAlign:           { tableCell.setAlign(str); return; }
        case TableCellAxis:            { tableCell.setAxis(str); return; }
        case TableCellBgColor:         { tableCell.setBgColor(str); return; }
        case TableCellCh:              { tableCell.setCh(str); return; }
        case TableCellChOff:           { tableCell.setChOff(str); return; }
        case TableCellColSpan:         { tableCell.setColSpan(value->toInt32(exec)); return; }
        case TableCellHeaders:         { tableCell.setHeaders(str); return; }
        case TableCellHeight:          { tableCell.setHeight(str); return; }
        case TableCellNoWrap:          { tableCell.setNoWrap(value->toBoolean(exec)); return; }
        case TableCellRowSpan:         { tableCell.setRowSpan(value->toInt32(exec)); return; }
        case TableCellScope:           { tableCell.setScope(str); return; }
        case TableCellVAlign:          { tableCell.setVAlign(str); return; }
        case TableCellWidth:           { tableCell.setWidth(str); return; }
    }
}

void JSHTMLElement::frameSetSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLFrameSetElement& frameSet = *static_cast<HTMLFrameSetElement*>(impl());
    switch (token) {
        case FrameSetCols:            { frameSet.setCols(str); return; }
        case FrameSetRows:            { frameSet.setRows(str); return; }
    }
}

void JSHTMLElement::frameSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLFrameElement& frameElement = *static_cast<HTMLFrameElement*>(impl());
    switch (token) {
        // read-only: FrameContentDocument:
        case FrameFrameBorder:     { frameElement.setFrameBorder(str); return; }
        case FrameLongDesc:        { frameElement.setLongDesc(str); return; }
        case FrameMarginHeight:    { frameElement.setMarginHeight(str); return; }
        case FrameMarginWidth:     { frameElement.setMarginWidth(str); return; }
        case FrameName:            { frameElement.setName(str); return; }
        case FrameNoResize:        { frameElement.setNoResize(value->toBoolean(exec)); return; }
        case FrameScrolling:       { frameElement.setScrolling(str); return; }
        case FrameSrc:             { frameElement.setSrc(str); return; }
        case FrameLocation:        { frameElement.setLocation(str); return; }
    }
}

void JSHTMLElement::iFrameSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLIFrameElement& iFrame = *static_cast<HTMLIFrameElement*>(impl());
    switch (token) {
        case IFrameAlign:           { iFrame.setAlign(str); return; }
        // read-only: IFrameContentDocument
        case IFrameFrameBorder:     { iFrame.setFrameBorder(str); return; }
        case IFrameHeight:          { iFrame.setHeight(str); return; }
        case IFrameLongDesc:        { iFrame.setLongDesc(str); return; }
        case IFrameMarginHeight:    { iFrame.setMarginHeight(str); return; }
        case IFrameMarginWidth:     { iFrame.setMarginWidth(str); return; }
        case IFrameName:            { iFrame.setName(str); return; }
        case IFrameScrolling:       { iFrame.setScrolling(str); return; }
        case IFrameSrc:             { iFrame.setSrc(str); return; }
        case IFrameWidth:           { iFrame.setWidth(str); return; }
    }
}

void JSHTMLElement::marqueeSetter(ExecState* exec, int token, JSValue *value, const WebCore::String& str)
{
    // FIXME: Find out what WinIE supports and implement it.
}

void JSHTMLElement::putValueProperty(ExecState* exec, int token, JSValue *value, int)
{
    DOMExceptionTranslator exception(exec);
    WebCore::String str = value->toString(exec);
 
    // Check our set of generic properties first.
    HTMLElement &element = *static_cast<HTMLElement*>(impl());
    switch (token) {
        case ElementId:
            element.setId(str);
            return;
        case ElementLang:
            element.setLang(str);
            return;
        case ElementDir:
            element.setDir(str);
            return;
        case ElementClassName:
            element.setClassName(str);
            return;
        case ElementInnerHTML:
            element.setInnerHTML(str, exception);
            return;
        case ElementInnerText:
            element.setInnerText(str, exception);
            return;
        case ElementOuterHTML:
            element.setOuterHTML(str, exception);
            return;
        case ElementOuterText:
            element.setOuterText(str, exception);
            return;
        case ElementContentEditable:
            element.setContentEditable(str);
            return;
    }

    // Now check for properties that apply to a specific element type.
    const Accessors* access = accessors();
    if (access && access->m_setter)
        return (this->*(access->m_setter))(exec, token, value, str);  
}

HTMLElement* toHTMLElement(JSValue *val)
{
    if (!val || !val->isObject(&JSHTMLElement::info))
        return 0;
    return static_cast<HTMLElement*>(static_cast<JSHTMLElement*>(val)->impl());
}

HTMLTableCaptionElement* toHTMLTableCaptionElement(JSValue *val)
{
    HTMLElement* e = toHTMLElement(val);
    if (e && e->hasTagName(captionTag))
        return static_cast<HTMLTableCaptionElement*>(e);
    return 0;
}

HTMLTableSectionElement* toHTMLTableSectionElement(JSValue *val)
{
    HTMLElement* e = toHTMLElement(val);
    if (e && (e->hasTagName(theadTag) || e->hasTagName(tbodyTag) || e->hasTagName(tfootTag)))
        return static_cast<HTMLTableSectionElement*>(e);
    return 0;
}

// -------------------------------------------------------------------------
/* Source for HTMLCollectionProtoTable. Use "make hashtables" to regenerate.
@begin HTMLCollectionProtoTable 3
  item          JSHTMLCollection::Item            DontDelete|Function 1
  namedItem     JSHTMLCollection::NamedItem       DontDelete|Function 1
  tags          JSHTMLCollection::Tags            DontDelete|Function 1
@end
*/
KJS_DEFINE_PROTOTYPE(HTMLCollectionProto)
KJS_IMPLEMENT_PROTOFUNC(HTMLCollectionProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("HTMLCollection",HTMLCollectionProto,HTMLCollectionProtoFunc)

const ClassInfo JSHTMLCollection::info = { "Collection", 0, 0, 0 };

JSHTMLCollection::JSHTMLCollection(ExecState* exec, HTMLCollection *c)
  : m_impl(c) 
{
  setPrototype(HTMLCollectionProto::self(exec));
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
  if (propertyName == lengthPropertyName) {
      slot.setCustom(this, lengthGetter);
      return true;
  } else {
    // Look in the prototype (for functions) before assuming it's an item's name
    JSValue *proto = prototype();
    if (proto->isObject() && static_cast<JSObject*>(proto)->hasProperty(exec, propertyName))
      return false;

    // name or index ?
    bool ok;
    unsigned int u = propertyName.toUInt32(&ok);
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
    unsigned int u = s.toUInt32(&ok);
    if (ok)
      return toJS(exec, collection.item(u));
    // support for document.images('<name>') etc.
    return getNamedItems(exec, Identifier(s));
  }
  else if (args.size() >= 1) // the second arg, if set, is the index of the item we want
  {
    bool ok;
    UString s = args[0]->toString(exec);
    unsigned int u = args[1]->toString(exec).toUInt32(&ok);
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
    DeprecatedValueList< RefPtr<WebCore::Node> > namedItems = m_impl->namedItems(propertyName);

    if (namedItems.isEmpty())
        return jsUndefined();

    if (namedItems.count() == 1)
        return toJS(exec, namedItems[0].get());

    return new DOMNamedNodesCollection(exec, namedItems);
}

JSValue *HTMLCollectionProtoFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&JSHTMLCollection::info))
    return throwError(exec, TypeError);
  HTMLCollection &coll = *static_cast<JSHTMLCollection*>(thisObj)->impl();

  switch (id) {
  case JSHTMLCollection::Item:
    return toJS(exec,coll.item(args[0]->toUInt32(exec)));
  case JSHTMLCollection::Tags:
    return toJS(exec, coll.base()->getElementsByTagName(args[0]->toString(exec)).get());
  case JSHTMLCollection::NamedItem:
    return static_cast<JSHTMLCollection*>(thisObj)->getNamedItems(exec, Identifier(args[0]->toString(exec)));
  default:
    return jsUndefined();
  }
}

// -------------------------------------------------------------------------

JSHTMLSelectCollection::JSHTMLSelectCollection(ExecState* exec, HTMLCollection *c, HTMLSelectElement* e)
  : JSHTMLCollection(exec, c), m_element(e)
{
}

JSValue *JSHTMLSelectCollection::selectedIndexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLSelectCollection *thisObj = static_cast<JSHTMLSelectCollection*>(slot.slotBase());
    return jsNumber(thisObj->m_element->selectedIndex());
}

bool JSHTMLSelectCollection::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (propertyName == "selectedIndex") {
    slot.setCustom(this, selectedIndexGetter);
    //result = jsNumber(m_element->selectedIndex());
    return true;
  }

  return JSHTMLCollection::getOwnPropertySlot(exec, propertyName, slot);
}

void JSHTMLSelectCollection::put(ExecState* exec, const Identifier &propertyName, JSValue *value, int)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "JSHTMLSelectCollection::put " << propertyName.deprecatedString() << endl;
#endif
  if ( propertyName == "selectedIndex" ) {
    m_element->setSelectedIndex( value->toInt32( exec ) );
    return;
  }
  // resize ?
  else if (propertyName == lengthPropertyName) {
    int exception = 0;

    unsigned newLen;
    bool converted = value->getUInt32(newLen);

    if (!converted)
      return;

    int diff = m_element->length() - newLen;

    if (diff < 0) { // add dummy elements
      do {
        RefPtr<Element> option = m_element->ownerDocument()->createElement("option", exception);
        if (exception)
          break;         
        m_element->add(static_cast<HTMLElement*>(option.get()), 0, exception);
        if (exception)
          break;
      } while (++diff);
    }
    else // remove elements
      while (diff-- > 0)
        m_element->remove(newLen);

    setDOMException(exec, exception);
    return;
  }
  // an index ?
  bool ok;
  unsigned int u = propertyName.toUInt32(&ok);
  if (!ok)
    return;

  if (value->isUndefinedOrNull()) {
    // null and undefined delete. others, too ?
    m_element->remove(u);
    return;
  }

  // is v an option element ?
  WebCore::Node *option = toNode(value);
  if (!option || !option->hasTagName(optionTag))
    return;

  int exception = 0;
  int diff = int(u) - m_element->length();
  HTMLElement* before = 0;
  // out of array bounds ? first insert empty dummies
  if (diff > 0) {
    while (diff--) {
      RefPtr<Element> dummyOption = m_element->ownerDocument()->createElement("option", exception);
      if (!dummyOption)
        break;      
      m_element->add(static_cast<HTMLElement*>(dummyOption.get()), 0, exception);
      if (exception) 
          break;
    }
    // replace an existing entry ?
  } else if (diff < 0) {
    before = static_cast<HTMLElement*>(m_element->options()->item(u+1));
    m_element->remove(u);
  }
  // finally add the new element
  if (exception == 0)
    m_element->add(static_cast<HTMLOptionElement*>(option), before, exception);

  setDOMException(exec, exception);
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
        
    HTMLImageElement* result = new HTMLImageElement(m_doc.get());
    
    if (widthSet)
        result->setWidth(width);
    if (heightSet)
        result->setHeight(height);
    
    return static_cast<JSObject*>(toJS(exec, result));
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

JSValue *getSelectHTMLCollection(ExecState* exec, HTMLCollection *c, HTMLSelectElement* e)
{
  DOMObject *ret;
  if (!c)
    return jsNull();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter());
  if ((ret = interp->getDOMObject(c)))
    return ret;
  else {
    ret = new JSHTMLSelectCollection(exec, c, e);
    interp->putDOMObject(c,ret);
    return ret;
  }
}

} // namespace
