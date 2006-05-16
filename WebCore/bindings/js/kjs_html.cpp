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
#include "HTMLAnchorElement.h"
#include "HTMLAppletElement.h"
#include "HTMLAreaElement.h"
#include "HTMLBaseElement.h"
#include "HTMLBaseFontElement.h"
#include "HTMLBlockquoteElement.h"
#include "HTMLBodyElement.h"
#include "HTMLBRElement.h"
#include "HTMLButtonElement.h"
#include "HTMLDirectoryElement.h"
#include "HTMLDivElement.h"
#include "HTMLDListElement.h"
#include "HTMLDocument.h"
#include "HTMLEmbedElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFontElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHeadingElement.h"
#include "HTMLHRElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLIsIndexElement.h"
#include "HTMLLabelElement.h"
#include "HTMLLegendElement.h"
#include "HTMLLIElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMapElement.h"
#include "HTMLMenuElement.h"
#include "HTMLMetaElement.h"
#include "HTMLModElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLOListElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "HTMLParagraphElement.h"
#include "HTMLParamElement.h"
#include "HTMLPreElement.h"
#include "HTMLScriptElement.h"
#include "HTMLSelectElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableColElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTitleElement.h"
#include "HTMLQuoteElement.h"
#include "HTMLUListElement.h"
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
  HTMLElementFunction(ExecState *exec, int i, int len, const Identifier& name);
  virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List&args);
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

JSValue *JSHTMLDocumentProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
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
                        JSObject *functionObject = window->get(exec, "open")->getObject();
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

JSHTMLDocument::JSHTMLDocument(ExecState *exec, HTMLDocument *d)
  : JSDocument(exec, d)
{
    setPrototype(JSHTMLDocumentProto::self(exec));
}

JSValue *JSHTMLDocument::namedItemGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
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

JSValue *JSHTMLDocument::getValueProperty(ExecState *exec, int token) const
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
  case Scripts: // TODO (IE-specific)
    {
      // To be implemented. Meanwhile, return an object with a length property set to 0
      JSObject *obj = new JSObject;
      obj->put(exec, lengthPropertyName, jsNumber(0));
      return obj;
    }
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

bool JSHTMLDocument::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
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

void JSHTMLDocument::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
    lookupPut<JSHTMLDocument, JSDocument>(exec, propertyName, value, attr, &HTMLDocumentTable, this);
}

void JSHTMLDocument::putValueProperty(ExecState *exec, int token, JSValue *value, int /*attr*/)
{
  DOMExceptionTranslator exception(exec);
  HTMLDocument &doc = *static_cast<HTMLDocument*>(impl());
  HTMLElement *body = doc.body();
  HTMLBodyElement *bodyElement = (body && body->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(body) : 0;

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

const ClassInfo JSHTMLElement::a_info = { "HTMLAnchorElement", &JSHTMLElement::info, &HTMLAnchorElementTable, 0 };
const ClassInfo JSHTMLElement::applet_info = { "HTMLAppletElement", &JSHTMLElement::info, &HTMLAppletElementTable, 0 };
const ClassInfo JSHTMLElement::area_info = { "HTMLAreaElement", &JSHTMLElement::info, &HTMLAreaElementTable, 0 };
const ClassInfo JSHTMLElement::baseFont_info = { "HTMLBaseFontElement", &JSHTMLElement::info, &HTMLBaseFontElementTable, 0 };
const ClassInfo JSHTMLElement::blockQuote_info = { "HTMLBlockQuoteElement", &JSHTMLElement::info, &HTMLBlockQuoteElementTable, 0 };
const ClassInfo JSHTMLElement::body_info = { "HTMLBodyElement", &JSHTMLElement::info, &HTMLBodyElementTable, 0 };
const ClassInfo JSHTMLElement::br_info = { "HTMLBRElement", &JSHTMLElement::info, &HTMLBRElementTable, 0 };
const ClassInfo JSHTMLElement::button_info = { "HTMLButtonElement", &JSHTMLElement::info, &HTMLButtonElementTable, 0 };
const ClassInfo JSHTMLElement::caption_info = { "HTMLTableCaptionElement", &JSHTMLElement::info, &HTMLTableCaptionElementTable, 0 };
const ClassInfo JSHTMLElement::col_info = { "HTMLTableColElement", &JSHTMLElement::info, &HTMLTableColElementTable, 0 };
const ClassInfo JSHTMLElement::dir_info = { "HTMLDirectoryElement", &JSHTMLElement::info, &HTMLDirectoryElementTable, 0 };
const ClassInfo JSHTMLElement::div_info = { "HTMLDivElement", &JSHTMLElement::info, &HTMLDivElementTable, 0 };
const ClassInfo JSHTMLElement::dl_info = { "HTMLDListElement", &JSHTMLElement::info, &HTMLDListElementTable, 0 };
const ClassInfo JSHTMLElement::embed_info = { "HTMLEmbedElement", &JSHTMLElement::info, &HTMLEmbedElementTable, 0 };
const ClassInfo JSHTMLElement::fieldSet_info = { "HTMLFieldSetElement", &JSHTMLElement::info, &HTMLFieldSetElementTable, 0 };
const ClassInfo JSHTMLElement::font_info = { "HTMLFontElement", &JSHTMLElement::info, &HTMLFontElementTable, 0 };
const ClassInfo JSHTMLElement::form_info = { "HTMLFormElement", &JSHTMLElement::info, &HTMLFormElementTable, 0 };
const ClassInfo JSHTMLElement::frameSet_info = { "HTMLFrameSetElement", &JSHTMLElement::info, &HTMLFrameSetElementTable, 0 };
const ClassInfo JSHTMLElement::frame_info = { "HTMLFrameElement", &JSHTMLElement::info, &HTMLFrameElementTable, 0 };
const ClassInfo JSHTMLElement::heading_info = { "HTMLHeadingElement", &JSHTMLElement::info, &HTMLHeadingElementTable, 0 };
const ClassInfo JSHTMLElement::hr_info = { "HTMLHRElement", &JSHTMLElement::info, &HTMLHRElementTable, 0 };
const ClassInfo JSHTMLElement::html_info = { "HTMLHtmlElement", &JSHTMLElement::info, &HTMLHtmlElementTable, 0 };
const ClassInfo JSHTMLElement::iFrame_info = { "HTMLIFrameElement", &JSHTMLElement::info, &HTMLIFrameElementTable, 0 };
const ClassInfo JSHTMLElement::img_info = { "HTMLImageElement", &JSHTMLElement::info, &HTMLImageElementTable, 0 };
const ClassInfo JSHTMLElement::info = { "HTMLElement", &JSElement::info, &HTMLElementTable, 0 };
const ClassInfo JSHTMLElement::input_info = { "HTMLInputElement", &JSHTMLElement::info, &HTMLInputElementTable, 0 };
const ClassInfo JSHTMLElement::isIndex_info = { "HTMLIsIndexElement", &JSHTMLElement::info, &HTMLIsIndexElementTable, 0 };
const ClassInfo JSHTMLElement::label_info = { "HTMLLabelElement", &JSHTMLElement::info, &HTMLLabelElementTable, 0 };
const ClassInfo JSHTMLElement::legend_info = { "HTMLLegendElement", &JSHTMLElement::info, &HTMLLegendElementTable, 0 };
const ClassInfo JSHTMLElement::li_info = { "HTMLLIElement", &JSHTMLElement::info, &HTMLLIElementTable, 0 };
const ClassInfo JSHTMLElement::map_info = { "HTMLMapElement", &JSHTMLElement::info, &HTMLMapElementTable, 0 };
const ClassInfo JSHTMLElement::marquee_info = { "HTMLMarqueeElement", &JSHTMLElement::info, &HTMLMarqueeElementTable, 0 };
const ClassInfo JSHTMLElement::menu_info = { "HTMLMenuElement", &JSHTMLElement::info, &HTMLMenuElementTable, 0 };
const ClassInfo JSHTMLElement::mod_info = { "HTMLModElement", &JSHTMLElement::info, &HTMLModElementTable, 0 };
const ClassInfo JSHTMLElement::object_info = { "HTMLObjectElement", &JSHTMLElement::info, &HTMLObjectElementTable, 0 };
const ClassInfo JSHTMLElement::ol_info = { "HTMLOListElement", &JSHTMLElement::info, &HTMLOListElementTable, 0 };
const ClassInfo JSHTMLElement::optGroup_info = { "HTMLOptGroupElement", &JSHTMLElement::info, &HTMLOptGroupElementTable, 0 };
const ClassInfo JSHTMLElement::option_info = { "HTMLOptionElement", &JSHTMLElement::info, &HTMLOptionElementTable, 0 };
const ClassInfo JSHTMLElement::p_info = { "HTMLParagraphElement", &JSHTMLElement::info, &HTMLParagraphElementTable, 0 };
const ClassInfo JSHTMLElement::param_info = { "HTMLParamElement", &JSHTMLElement::info, &HTMLParamElementTable, 0 };
const ClassInfo JSHTMLElement::pre_info = { "HTMLPreElement", &JSHTMLElement::info, &HTMLPreElementTable, 0 };
const ClassInfo JSHTMLElement::q_info = { "HTMLQuoteElement", &JSHTMLElement::info, &HTMLQuoteElementTable, 0 };
const ClassInfo JSHTMLElement::script_info = { "HTMLScriptElement", &JSHTMLElement::info, &HTMLScriptElementTable, 0 };
const ClassInfo JSHTMLElement::select_info = { "HTMLSelectElement", &JSHTMLElement::info, &HTMLSelectElementTable, 0 };
const ClassInfo JSHTMLElement::table_info = { "HTMLTableElement", &JSHTMLElement::info, &HTMLTableElementTable, 0 };
const ClassInfo JSHTMLElement::tablecell_info = { "HTMLTableCellElement", &JSHTMLElement::info, &HTMLTableCellElementTable, 0 };
const ClassInfo JSHTMLElement::tablesection_info = { "HTMLTableSectionElement", &JSHTMLElement::info, &HTMLTableSectionElementTable, 0 };
const ClassInfo JSHTMLElement::textArea_info = { "HTMLTextAreaElement", &JSHTMLElement::info, &HTMLTextAreaElementTable, 0 };
const ClassInfo JSHTMLElement::tr_info = { "HTMLTableRowElement", &JSHTMLElement::info, &HTMLTableRowElementTable, 0 };
const ClassInfo JSHTMLElement::ul_info = { "HTMLUListElement", &JSHTMLElement::info, &HTMLUListElementTable, 0 };

const ClassInfo* JSHTMLElement::classInfo() const
{
    static HashMap<WebCore::AtomicStringImpl*, const ClassInfo*> classInfoMap;
    if (classInfoMap.isEmpty()) {
        classInfoMap.set(appletTag.localName().impl(), &applet_info);
        classInfoMap.set(areaTag.localName().impl(), &area_info);
        classInfoMap.set(basefontTag.localName().impl(), &baseFont_info);
        classInfoMap.set(blockquoteTag.localName().impl(), &blockQuote_info);
        classInfoMap.set(bodyTag.localName().impl(), &body_info);
        classInfoMap.set(brTag.localName().impl(), &br_info);
        classInfoMap.set(buttonTag.localName().impl(), &button_info);
        classInfoMap.set(captionTag.localName().impl(), &caption_info);
        classInfoMap.set(colTag.localName().impl(), &col_info);
        classInfoMap.set(colgroupTag.localName().impl(), &col_info);
        classInfoMap.set(delTag.localName().impl(), &mod_info);
        classInfoMap.set(dirTag.localName().impl(), &dir_info);
        classInfoMap.set(divTag.localName().impl(), &div_info);
        classInfoMap.set(dlTag.localName().impl(), &dl_info);
        classInfoMap.set(embedTag.localName().impl(), &embed_info);
        classInfoMap.set(fieldsetTag.localName().impl(), &fieldSet_info);
        classInfoMap.set(fontTag.localName().impl(), &font_info);
        classInfoMap.set(formTag.localName().impl(), &form_info);
        classInfoMap.set(frameTag.localName().impl(), &frame_info);
        classInfoMap.set(framesetTag.localName().impl(), &frameSet_info);
        classInfoMap.set(h1Tag.localName().impl(), &heading_info);
        classInfoMap.set(h2Tag.localName().impl(), &heading_info);
        classInfoMap.set(h3Tag.localName().impl(), &heading_info);
        classInfoMap.set(h4Tag.localName().impl(), &heading_info);
        classInfoMap.set(h5Tag.localName().impl(), &heading_info);
        classInfoMap.set(h6Tag.localName().impl(), &heading_info);
        classInfoMap.set(hrTag.localName().impl(), &hr_info);
        classInfoMap.set(htmlTag.localName().impl(), &html_info);
        classInfoMap.set(iframeTag.localName().impl(), &iFrame_info);
        classInfoMap.set(imgTag.localName().impl(), &img_info);
        classInfoMap.set(inputTag.localName().impl(), &input_info);
        classInfoMap.set(insTag.localName().impl(), &mod_info);
        classInfoMap.set(isindexTag.localName().impl(), &isIndex_info);
        classInfoMap.set(labelTag.localName().impl(), &label_info);
        classInfoMap.set(legendTag.localName().impl(), &legend_info);
        classInfoMap.set(liTag.localName().impl(), &li_info);
        classInfoMap.set(listingTag.localName().impl(), &pre_info);
        classInfoMap.set(mapTag.localName().impl(), &map_info);
        classInfoMap.set(marqueeTag.localName().impl(), &marquee_info);
        classInfoMap.set(menuTag.localName().impl(), &menu_info);
        classInfoMap.set(objectTag.localName().impl(), &object_info);
        classInfoMap.set(olTag.localName().impl(), &ol_info);
        classInfoMap.set(optgroupTag.localName().impl(), &optGroup_info);
        classInfoMap.set(optionTag.localName().impl(), &option_info);
        classInfoMap.set(pTag.localName().impl(), &p_info);
        classInfoMap.set(paramTag.localName().impl(), &param_info);
        classInfoMap.set(preTag.localName().impl(), &pre_info);
        classInfoMap.set(qTag.localName().impl(), &q_info);
        classInfoMap.set(scriptTag.localName().impl(), &script_info);
        classInfoMap.set(selectTag.localName().impl(), &select_info);
        classInfoMap.set(tableTag.localName().impl(), &table_info);
        classInfoMap.set(tbodyTag.localName().impl(), &tablesection_info);
        classInfoMap.set(tdTag.localName().impl(), &tablecell_info);
        classInfoMap.set(textareaTag.localName().impl(), &textArea_info);
        classInfoMap.set(tfootTag.localName().impl(), &tablesection_info);
        classInfoMap.set(thTag.localName().impl(), &tablecell_info);
        classInfoMap.set(theadTag.localName().impl(), &tablesection_info);
        classInfoMap.set(trTag.localName().impl(), &tr_info);
        classInfoMap.set(ulTag.localName().impl(), &ul_info);
        classInfoMap.set(aTag.localName().impl(), &a_info);
    }
    
    HTMLElement* element = static_cast<HTMLElement*>(impl());
    const ClassInfo* result = classInfoMap.get(element->localName().impl());
    if (result)
        return result;
    return &info;
}

const JSHTMLElement::Accessors JSHTMLElement::html_accessors = { &JSHTMLElement::htmlGetter, &JSHTMLElement::htmlSetter };
const JSHTMLElement::Accessors JSHTMLElement::isIndex_accessors = { &JSHTMLElement::isIndexGetter, &JSHTMLElement::isIndexSetter };
const JSHTMLElement::Accessors JSHTMLElement::body_accessors = { &JSHTMLElement::bodyGetter, &JSHTMLElement::bodySetter };
const JSHTMLElement::Accessors JSHTMLElement::form_accessors = { &JSHTMLElement::formGetter, &JSHTMLElement::formSetter };
const JSHTMLElement::Accessors JSHTMLElement::select_accessors = { &JSHTMLElement::selectGetter, &JSHTMLElement::selectSetter };
const JSHTMLElement::Accessors JSHTMLElement::optGroup_accessors = { &JSHTMLElement::optGroupGetter, &JSHTMLElement::optGroupSetter };
const JSHTMLElement::Accessors JSHTMLElement::option_accessors = { &JSHTMLElement::optionGetter, &JSHTMLElement::optionSetter };
const JSHTMLElement::Accessors JSHTMLElement::input_accessors = { &JSHTMLElement::inputGetter, &JSHTMLElement::inputSetter };
const JSHTMLElement::Accessors JSHTMLElement::textArea_accessors = { &JSHTMLElement::textAreaGetter, &JSHTMLElement::textAreaSetter };
const JSHTMLElement::Accessors JSHTMLElement::button_accessors = { &JSHTMLElement::buttonGetter, &JSHTMLElement::buttonSetter };
const JSHTMLElement::Accessors JSHTMLElement::label_accessors = { &JSHTMLElement::labelGetter, &JSHTMLElement::labelSetter };
const JSHTMLElement::Accessors JSHTMLElement::fieldSet_accessors = { &JSHTMLElement::fieldSetGetter, &JSHTMLElement::fieldSetSetter };
const JSHTMLElement::Accessors JSHTMLElement::legend_accessors = { &JSHTMLElement::legendGetter, &JSHTMLElement::legendSetter };
const JSHTMLElement::Accessors JSHTMLElement::ul_accessors = { &JSHTMLElement::uListGetter, &JSHTMLElement::uListSetter };
const JSHTMLElement::Accessors JSHTMLElement::ol_accessors = { &JSHTMLElement::oListGetter, &JSHTMLElement::oListSetter };
const JSHTMLElement::Accessors JSHTMLElement::dl_accessors = { &JSHTMLElement::dListGetter, &JSHTMLElement::dListSetter };
const JSHTMLElement::Accessors JSHTMLElement::dir_accessors = { &JSHTMLElement::dirGetter, &JSHTMLElement::dirSetter };
const JSHTMLElement::Accessors JSHTMLElement::menu_accessors = { &JSHTMLElement::menuGetter, &JSHTMLElement::menuSetter };
const JSHTMLElement::Accessors JSHTMLElement::li_accessors = { &JSHTMLElement::liGetter, &JSHTMLElement::liSetter };
const JSHTMLElement::Accessors JSHTMLElement::div_accessors = { &JSHTMLElement::divGetter, &JSHTMLElement::divSetter };
const JSHTMLElement::Accessors JSHTMLElement::p_accessors = { &JSHTMLElement::paragraphGetter, &JSHTMLElement::paragraphSetter };
const JSHTMLElement::Accessors JSHTMLElement::heading_accessors = { &JSHTMLElement::headingGetter, &JSHTMLElement::headingSetter };
const JSHTMLElement::Accessors JSHTMLElement::blockQuote_accessors = { &JSHTMLElement::blockQuoteGetter, &JSHTMLElement::blockQuoteSetter };
const JSHTMLElement::Accessors JSHTMLElement::q_accessors = { &JSHTMLElement::quoteGetter, &JSHTMLElement::quoteSetter };
const JSHTMLElement::Accessors JSHTMLElement::pre_accessors = { &JSHTMLElement::preGetter, &JSHTMLElement::preSetter };
const JSHTMLElement::Accessors JSHTMLElement::br_accessors = { &JSHTMLElement::brGetter, &JSHTMLElement::brSetter };
const JSHTMLElement::Accessors JSHTMLElement::baseFont_accessors = { &JSHTMLElement::baseFontGetter, &JSHTMLElement::baseFontSetter };
const JSHTMLElement::Accessors JSHTMLElement::font_accessors = { &JSHTMLElement::fontGetter, &JSHTMLElement::fontSetter };
const JSHTMLElement::Accessors JSHTMLElement::hr_accessors = { &JSHTMLElement::hrGetter, &JSHTMLElement::hrSetter };
const JSHTMLElement::Accessors JSHTMLElement::mod_accessors = { &JSHTMLElement::modGetter, &JSHTMLElement::modSetter };
const JSHTMLElement::Accessors JSHTMLElement::a_accessors = { &JSHTMLElement::anchorGetter, &JSHTMLElement::anchorSetter };
const JSHTMLElement::Accessors JSHTMLElement::img_accessors = { &JSHTMLElement::imageGetter, &JSHTMLElement::imageSetter };
const JSHTMLElement::Accessors JSHTMLElement::object_accessors = { &JSHTMLElement::objectGetter, &JSHTMLElement::objectSetter };
const JSHTMLElement::Accessors JSHTMLElement::param_accessors = { &JSHTMLElement::paramGetter, &JSHTMLElement::paramSetter };
const JSHTMLElement::Accessors JSHTMLElement::applet_accessors = { &JSHTMLElement::appletGetter, &JSHTMLElement::appletSetter };
const JSHTMLElement::Accessors JSHTMLElement::embed_accessors = { &JSHTMLElement::embedGetter, &JSHTMLElement::embedSetter };
const JSHTMLElement::Accessors JSHTMLElement::map_accessors = { &JSHTMLElement::mapGetter, &JSHTMLElement::mapSetter };
const JSHTMLElement::Accessors JSHTMLElement::area_accessors = { &JSHTMLElement::areaGetter, &JSHTMLElement::areaSetter };
const JSHTMLElement::Accessors JSHTMLElement::script_accessors = { &JSHTMLElement::scriptGetter, &JSHTMLElement::scriptSetter };
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
        accessorMap.add(aTag.localName().impl(), &a_accessors);
        accessorMap.add(appletTag.localName().impl(), &applet_accessors);
        accessorMap.add(areaTag.localName().impl(), &area_accessors);
        accessorMap.add(basefontTag.localName().impl(), &baseFont_accessors);
        accessorMap.add(blockquoteTag.localName().impl(), &blockQuote_accessors); 
        accessorMap.add(bodyTag.localName().impl(), &body_accessors);
        accessorMap.add(brTag.localName().impl(), &br_accessors);
        accessorMap.add(buttonTag.localName().impl(), &button_accessors);
        accessorMap.add(captionTag.localName().impl(), &caption_accessors);
        accessorMap.add(colTag.localName().impl(), &col_accessors);
        accessorMap.add(colgroupTag.localName().impl(), &col_accessors);
        accessorMap.add(delTag.localName().impl(), &mod_accessors);
        accessorMap.add(dirTag.localName().impl(), &dir_accessors);
        accessorMap.add(divTag.localName().impl(), &div_accessors);
        accessorMap.add(dlTag.localName().impl(), &dl_accessors);
        accessorMap.add(embedTag.localName().impl(), &embed_accessors);
        accessorMap.add(fieldsetTag.localName().impl(), &fieldSet_accessors);
        accessorMap.add(fontTag.localName().impl(), &font_accessors);
        accessorMap.add(formTag.localName().impl(), &form_accessors);
        accessorMap.add(frameTag.localName().impl(), &frame_accessors);
        accessorMap.add(framesetTag.localName().impl(), &frameSet_accessors);
        accessorMap.add(h1Tag.localName().impl(), &heading_accessors);
        accessorMap.add(h2Tag.localName().impl(), &heading_accessors);
        accessorMap.add(h3Tag.localName().impl(), &heading_accessors);
        accessorMap.add(h4Tag.localName().impl(), &heading_accessors);
        accessorMap.add(h5Tag.localName().impl(), &heading_accessors);
        accessorMap.add(h6Tag.localName().impl(), &heading_accessors);
        accessorMap.add(hrTag.localName().impl(), &hr_accessors); 
        accessorMap.add(htmlTag.localName().impl(), &html_accessors);
        accessorMap.add(iframeTag.localName().impl(), &iFrame_accessors);
        accessorMap.add(imgTag.localName().impl(), &img_accessors);
        accessorMap.add(inputTag.localName().impl(), &input_accessors);
        accessorMap.add(insTag.localName().impl(), &mod_accessors);
        accessorMap.add(isindexTag.localName().impl(), &isIndex_accessors);
        accessorMap.add(labelTag.localName().impl(), &label_accessors);
        accessorMap.add(legendTag.localName().impl(), &legend_accessors);
        accessorMap.add(liTag.localName().impl(), &li_accessors);
        accessorMap.set(listingTag.localName().impl(), &pre_accessors);
        accessorMap.add(mapTag.localName().impl(), &map_accessors);
        accessorMap.add(marqueeTag.localName().impl(), &marquee_accessors);
        accessorMap.add(menuTag.localName().impl(), &menu_accessors);
        accessorMap.add(objectTag.localName().impl(), &object_accessors);
        accessorMap.add(olTag.localName().impl(), &ol_accessors);
        accessorMap.add(optionTag.localName().impl(), &option_accessors);
        accessorMap.add(optgroupTag.localName().impl(), &optGroup_accessors);
        accessorMap.add(pTag.localName().impl(), &p_accessors);
        accessorMap.add(paramTag.localName().impl(), &param_accessors);
        accessorMap.add(preTag.localName().impl(), &pre_accessors);
        accessorMap.add(qTag.localName().impl(), &q_accessors);
        accessorMap.add(scriptTag.localName().impl(), &script_accessors);
        accessorMap.add(selectTag.localName().impl(), &select_accessors);
        accessorMap.add(tableTag.localName().impl(), &table_accessors);
        accessorMap.add(tbodyTag.localName().impl(), &tablesection_accessors);
        accessorMap.add(tdTag.localName().impl(), &tablecell_accessors);
        accessorMap.add(textareaTag.localName().impl(), &textArea_accessors);
        accessorMap.add(thTag.localName().impl(), &tablecell_accessors);
        accessorMap.add(theadTag.localName().impl(), &tablesection_accessors);
        accessorMap.add(tfootTag.localName().impl(), &tablesection_accessors);
        accessorMap.add(trTag.localName().impl(), &tr_accessors);
        accessorMap.add(ulTag.localName().impl(), &ul_accessors);
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
@begin HTMLHtmlElementTable 1
  version       KJS::JSHTMLElement::HtmlVersion   DontDelete
@end
@begin HTMLHeadElementTable 1
  profile       KJS::JSHTMLElement::HeadProfile   DontDelete
@end
@begin HTMLLinkElementTable 11
  disabled      KJS::JSHTMLElement::LinkDisabled  DontDelete
  charset       KJS::JSHTMLElement::LinkCharset   DontDelete
  href          KJS::JSHTMLElement::LinkHref      DontDelete
  hreflang      KJS::JSHTMLElement::LinkHrefLang  DontDelete
  media         KJS::JSHTMLElement::LinkMedia     DontDelete
  rel           KJS::JSHTMLElement::LinkRel       DontDelete
  rev           KJS::JSHTMLElement::LinkRev       DontDelete
  target        KJS::JSHTMLElement::LinkTarget    DontDelete
  type          KJS::JSHTMLElement::LinkType      DontDelete
  sheet         KJS::JSHTMLElement::LinkSheet     DontDelete|ReadOnly
@end
@begin HTMLTitleElementTable 1
  text          KJS::JSHTMLElement::TitleText     DontDelete
@end
@begin HTMLMetaElementTable 4
  content       KJS::JSHTMLElement::MetaContent   DontDelete
  httpEquiv     KJS::JSHTMLElement::MetaHttpEquiv DontDelete
  name          KJS::JSHTMLElement::MetaName      DontDelete
  scheme        KJS::JSHTMLElement::MetaScheme    DontDelete
@end
@begin HTMLBaseElementTable 2
  href          KJS::JSHTMLElement::BaseHref      DontDelete
  target        KJS::JSHTMLElement::BaseTarget    DontDelete
@end
@begin HTMLIsIndexElementTable 2
  form          KJS::JSHTMLElement::IsIndexForm   DontDelete|ReadOnly
  prompt        KJS::JSHTMLElement::IsIndexPrompt DontDelete
@end
@begin HTMLStyleElementTable 4
  disabled      KJS::JSHTMLElement::StyleDisabled DontDelete
  media         KJS::JSHTMLElement::StyleMedia    DontDelete
  type          KJS::JSHTMLElement::StyleType     DontDelete
  sheet         KJS::JSHTMLElement::StyleSheet    DontDelete|ReadOnly
@end
@begin HTMLBodyElementTable 10
  aLink         KJS::JSHTMLElement::BodyALink     DontDelete
  background    KJS::JSHTMLElement::BodyBackground        DontDelete
  bgColor       KJS::JSHTMLElement::BodyBgColor   DontDelete
  link          KJS::JSHTMLElement::BodyLink      DontDelete
  text          KJS::JSHTMLElement::BodyText      DontDelete
  vLink         KJS::JSHTMLElement::BodyVLink     DontDelete
  scrollLeft    KJS::JSHTMLElement::BodyScrollLeft        DontDelete
  scrollTop     KJS::JSHTMLElement::BodyScrollTop         DontDelete
  scrollHeight  KJS::JSHTMLElement::BodyScrollHeight      DontDelete|ReadOnly
  scrollWidth   KJS::JSHTMLElement::BodyScrollWidth       DontDelete|ReadOnly
@end
@begin HTMLFormElementTable 11
# Also supported, by name/index
  elements      KJS::JSHTMLElement::FormElements  DontDelete|ReadOnly
  length        KJS::JSHTMLElement::FormLength    DontDelete|ReadOnly
  name          KJS::JSHTMLElement::FormName      DontDelete
  acceptCharset KJS::JSHTMLElement::FormAcceptCharset     DontDelete
  action        KJS::JSHTMLElement::FormAction    DontDelete
  enctype       KJS::JSHTMLElement::FormEncType   DontDelete
  method        KJS::JSHTMLElement::FormMethod    DontDelete
  target        KJS::JSHTMLElement::FormTarget    DontDelete
  submit        KJS::JSHTMLElement::FormSubmit    DontDelete|Function 0
  reset         KJS::JSHTMLElement::FormReset     DontDelete|Function 0
@end
@begin HTMLSelectElementTable 11
# Also supported, by index
  type          KJS::JSHTMLElement::SelectType    DontDelete|ReadOnly
  selectedIndex KJS::JSHTMLElement::SelectSelectedIndex   DontDelete
  value         KJS::JSHTMLElement::SelectValue   DontDelete
  length        KJS::JSHTMLElement::SelectLength  DontDelete
  form          KJS::JSHTMLElement::SelectForm    DontDelete|ReadOnly
  options       KJS::JSHTMLElement::SelectOptions DontDelete|ReadOnly
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
@begin HTMLOptGroupElementTable 2
  disabled      KJS::JSHTMLElement::OptGroupDisabled      DontDelete
  label         KJS::JSHTMLElement::OptGroupLabel         DontDelete
@end
@begin HTMLOptionElementTable 8
  form          KJS::JSHTMLElement::OptionForm            DontDelete|ReadOnly
  defaultSelected KJS::JSHTMLElement::OptionDefaultSelected       DontDelete
  text          KJS::JSHTMLElement::OptionText            DontDelete
  index         KJS::JSHTMLElement::OptionIndex           DontDelete|ReadOnly
  disabled      KJS::JSHTMLElement::OptionDisabled        DontDelete
  label         KJS::JSHTMLElement::OptionLabel           DontDelete
  selected      KJS::JSHTMLElement::OptionSelected        DontDelete
  value         KJS::JSHTMLElement::OptionValue           DontDelete
@end
@begin HTMLInputElementTable 24
  defaultValue  KJS::JSHTMLElement::InputDefaultValue     DontDelete
  defaultChecked KJS::JSHTMLElement::InputDefaultChecked  DontDelete
  form          KJS::JSHTMLElement::InputForm             DontDelete|ReadOnly
  accept        KJS::JSHTMLElement::InputAccept           DontDelete
  accessKey     KJS::JSHTMLElement::InputAccessKey        DontDelete
  align         KJS::JSHTMLElement::InputAlign            DontDelete
  alt           KJS::JSHTMLElement::InputAlt              DontDelete
  checked       KJS::JSHTMLElement::InputChecked          DontDelete
  disabled      KJS::JSHTMLElement::InputDisabled         DontDelete
  indeterminate KJS::JSHTMLElement::InputIndeterminate    DontDelete
  maxLength     KJS::JSHTMLElement::InputMaxLength        DontDelete
  name          KJS::JSHTMLElement::InputName             DontDelete
  readOnly      KJS::JSHTMLElement::InputReadOnly         DontDelete
  selectionStart        KJS::JSHTMLElement::InputSelectionStart   DontDelete
  selectionEnd  KJS::JSHTMLElement::InputSelectionEnd     DontDelete
  size          KJS::JSHTMLElement::InputSize             DontDelete
  src           KJS::JSHTMLElement::InputSrc              DontDelete
  tabIndex      KJS::JSHTMLElement::InputTabIndex         DontDelete
  type          KJS::JSHTMLElement::InputType             DontDelete
  useMap        KJS::JSHTMLElement::InputUseMap           DontDelete
  value         KJS::JSHTMLElement::InputValue            DontDelete
  blur          KJS::JSHTMLElement::InputBlur             DontDelete|Function 0
  focus         KJS::JSHTMLElement::InputFocus            DontDelete|Function 0
  select        KJS::JSHTMLElement::InputSelect           DontDelete|Function 0
  click         KJS::JSHTMLElement::InputClick            DontDelete|Function 0
  setSelectionRange     KJS::JSHTMLElement::InputSetSelectionRange        DontDelete|Function 2
@end
@begin HTMLTextAreaElementTable 17
  defaultValue  KJS::JSHTMLElement::TextAreaDefaultValue  DontDelete
  form          KJS::JSHTMLElement::TextAreaForm          DontDelete|ReadOnly
  accessKey     KJS::JSHTMLElement::TextAreaAccessKey     DontDelete
  cols          KJS::JSHTMLElement::TextAreaCols          DontDelete
  disabled      KJS::JSHTMLElement::TextAreaDisabled      DontDelete
  name          KJS::JSHTMLElement::TextAreaName          DontDelete
  readOnly      KJS::JSHTMLElement::TextAreaReadOnly      DontDelete
  rows          KJS::JSHTMLElement::TextAreaRows          DontDelete
  selectionStart        KJS::JSHTMLElement::TextAreaSelectionStart        DontDelete
  selectionEnd  KJS::JSHTMLElement::TextAreaSelectionEnd  DontDelete
  tabIndex      KJS::JSHTMLElement::TextAreaTabIndex      DontDelete
  type          KJS::JSHTMLElement::TextAreaType          DontDelete|ReadOnly
  value         KJS::JSHTMLElement::TextAreaValue         DontDelete
  blur          KJS::JSHTMLElement::TextAreaBlur          DontDelete|Function 0
  focus         KJS::JSHTMLElement::TextAreaFocus         DontDelete|Function 0
  select        KJS::JSHTMLElement::TextAreaSelect        DontDelete|Function 0
  setSelectionRange     KJS::JSHTMLElement::TextAreaSetSelectionRange     DontDelete|Function 2
@end
@begin HTMLButtonElementTable 7
  form          KJS::JSHTMLElement::ButtonForm            DontDelete|ReadOnly
  accessKey     KJS::JSHTMLElement::ButtonAccessKey       DontDelete
  disabled      KJS::JSHTMLElement::ButtonDisabled        DontDelete
  name          KJS::JSHTMLElement::ButtonName            DontDelete
  tabIndex      KJS::JSHTMLElement::ButtonTabIndex        DontDelete
  type          KJS::JSHTMLElement::ButtonType            DontDelete|ReadOnly
  value         KJS::JSHTMLElement::ButtonValue           DontDelete
  blur          KJS::JSHTMLElement::ButtonBlur            DontDelete|Function 0
  focus         KJS::JSHTMLElement::ButtonFocus           DontDelete|Function 0
@end
@begin HTMLLabelElementTable 4
  form          KJS::JSHTMLElement::LabelForm             DontDelete|ReadOnly
  accessKey     KJS::JSHTMLElement::LabelAccessKey        DontDelete
  htmlFor       KJS::JSHTMLElement::LabelHtmlFor          DontDelete
  focus     KJS::JSHTMLElement::LabelFocus        DontDelete|Function 0
@end
@begin HTMLFieldSetElementTable 1
  form          KJS::JSHTMLElement::FieldSetForm          DontDelete|ReadOnly
@end
@begin HTMLLegendElementTable 4
  form          KJS::JSHTMLElement::LegendForm            DontDelete|ReadOnly
  accessKey     KJS::JSHTMLElement::LegendAccessKey       DontDelete
  align         KJS::JSHTMLElement::LegendAlign           DontDelete
  focus     KJS::JSHTMLElement::LegendFocus     DontDelete|Function 0
@end
@begin HTMLUListElementTable 2
  compact       KJS::JSHTMLElement::UListCompact          DontDelete
  type          KJS::JSHTMLElement::UListType             DontDelete
@end
@begin HTMLOListElementTable 3
  compact       KJS::JSHTMLElement::OListCompact          DontDelete
  start         KJS::JSHTMLElement::OListStart            DontDelete
  type          KJS::JSHTMLElement::OListType             DontDelete
@end
@begin HTMLDListElementTable 1
  compact       KJS::JSHTMLElement::DListCompact          DontDelete
@end
@begin HTMLDirectoryElementTable 1
  compact       KJS::JSHTMLElement::DirectoryCompact      DontDelete
@end
@begin HTMLMenuElementTable 1
  compact       KJS::JSHTMLElement::MenuCompact           DontDelete
@end
@begin HTMLLIElementTable 2
  type          KJS::JSHTMLElement::LIType                DontDelete
  value         KJS::JSHTMLElement::LIValue               DontDelete
@end
@begin HTMLDivElementTable 1
  align         KJS::JSHTMLElement::DivAlign              DontDelete
@end
@begin HTMLParagraphElementTable 1
  align         KJS::JSHTMLElement::ParagraphAlign        DontDelete
@end
@begin HTMLHeadingElementTable 1
  align         KJS::JSHTMLElement::HeadingAlign          DontDelete
@end
@begin HTMLBlockQuoteElementTable 1
  cite          KJS::JSHTMLElement::BlockQuoteCite        DontDelete
@end
@begin HTMLQuoteElementTable 1
  cite          KJS::JSHTMLElement::QuoteCite             DontDelete
@end
@begin HTMLPreElementTable 1
  width         KJS::JSHTMLElement::PreWidth              DontDelete
@end
@begin HTMLBRElementTable 1
  clear         KJS::JSHTMLElement::BRClear               DontDelete
@end
@begin HTMLBaseFontElementTable 3
  color         KJS::JSHTMLElement::BaseFontColor         DontDelete
  face          KJS::JSHTMLElement::BaseFontFace          DontDelete
  size          KJS::JSHTMLElement::BaseFontSize          DontDelete
@end
@begin HTMLFontElementTable 3
  color         KJS::JSHTMLElement::FontColor             DontDelete
  face          KJS::JSHTMLElement::FontFace              DontDelete
  size          KJS::JSHTMLElement::FontSize              DontDelete
@end
@begin HTMLHRElementTable 4
  align         KJS::JSHTMLElement::HRAlign               DontDelete
  noShade       KJS::JSHTMLElement::HRNoShade             DontDelete
  size          KJS::JSHTMLElement::HRSize                DontDelete
  width         KJS::JSHTMLElement::HRWidth               DontDelete
@end
@begin HTMLModElementTable 2
  cite          KJS::JSHTMLElement::ModCite               DontDelete
  dateTime      KJS::JSHTMLElement::ModDateTime           DontDelete
@end
@begin HTMLAnchorElementTable 24
  accessKey     KJS::JSHTMLElement::AnchorAccessKey       DontDelete
  charset       KJS::JSHTMLElement::AnchorCharset         DontDelete
  coords        KJS::JSHTMLElement::AnchorCoords          DontDelete
  href          KJS::JSHTMLElement::AnchorHref            DontDelete
  hreflang      KJS::JSHTMLElement::AnchorHrefLang        DontDelete
  hash          KJS::JSHTMLElement::AnchorHash            DontDelete|ReadOnly
  host          KJS::JSHTMLElement::AnchorHost            DontDelete|ReadOnly
  hostname      KJS::JSHTMLElement::AnchorHostname        DontDelete|ReadOnly
  name          KJS::JSHTMLElement::AnchorName            DontDelete
  pathname      KJS::JSHTMLElement::AnchorPathName        DontDelete|ReadOnly
  port          KJS::JSHTMLElement::AnchorPort            DontDelete|ReadOnly
  protocol      KJS::JSHTMLElement::AnchorProtocol        DontDelete|ReadOnly
  rel           KJS::JSHTMLElement::AnchorRel             DontDelete
  rev           KJS::JSHTMLElement::AnchorRev             DontDelete
  search        KJS::JSHTMLElement::AnchorSearch          DontDelete|ReadOnly
  shape         KJS::JSHTMLElement::AnchorShape           DontDelete
  tabIndex      KJS::JSHTMLElement::AnchorTabIndex        DontDelete
  target        KJS::JSHTMLElement::AnchorTarget          DontDelete
  text          KJS::JSHTMLElement::AnchorText            DontDelete|ReadOnly
  type          KJS::JSHTMLElement::AnchorType            DontDelete
  blur          KJS::JSHTMLElement::AnchorBlur            DontDelete|Function 0
  focus         KJS::JSHTMLElement::AnchorFocus           DontDelete|Function 0
  toString      KJS::JSHTMLElement::AnchorToString        DontDelete|Function 0
@end
@begin HTMLImageElementTable 14
  name          KJS::JSHTMLElement::ImageName             DontDelete
  align         KJS::JSHTMLElement::ImageAlign            DontDelete
  alt           KJS::JSHTMLElement::ImageAlt              DontDelete
  border        KJS::JSHTMLElement::ImageBorder           DontDelete
  complete      KJS::JSHTMLElement::ImageComplete         DontDelete|ReadOnly
  height        KJS::JSHTMLElement::ImageHeight           DontDelete
  hspace        KJS::JSHTMLElement::ImageHspace           DontDelete
  isMap         KJS::JSHTMLElement::ImageIsMap            DontDelete
  longDesc      KJS::JSHTMLElement::ImageLongDesc         DontDelete
  src           KJS::JSHTMLElement::ImageSrc              DontDelete
  useMap        KJS::JSHTMLElement::ImageUseMap           DontDelete
  vspace        KJS::JSHTMLElement::ImageVspace           DontDelete
  width         KJS::JSHTMLElement::ImageWidth            DontDelete
  x         KJS::JSHTMLElement::ImageX            DontDelete|ReadOnly
  y         KJS::JSHTMLElement::ImageY            DontDelete|ReadOnly
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
@begin HTMLParamElementTable 4
  name          KJS::JSHTMLElement::ParamName             DontDelete
  type          KJS::JSHTMLElement::ParamType             DontDelete
  value         KJS::JSHTMLElement::ParamValue            DontDelete
  valueType     KJS::JSHTMLElement::ParamValueType        DontDelete
@end
@begin HTMLAppletElementTable 11
  align         KJS::JSHTMLElement::AppletAlign           DontDelete
  alt           KJS::JSHTMLElement::AppletAlt             DontDelete
  archive       KJS::JSHTMLElement::AppletArchive         DontDelete
  code          KJS::JSHTMLElement::AppletCode            DontDelete
  codeBase      KJS::JSHTMLElement::AppletCodeBase        DontDelete
  height        KJS::JSHTMLElement::AppletHeight          DontDelete
  hspace        KJS::JSHTMLElement::AppletHspace          DontDelete
  name          KJS::JSHTMLElement::AppletName            DontDelete
  object        KJS::JSHTMLElement::AppletObject          DontDelete
  vspace        KJS::JSHTMLElement::AppletVspace          DontDelete
  width         KJS::JSHTMLElement::AppletWidth           DontDelete
@end
@begin HTMLEmbedElementTable 6
  align         KJS::JSHTMLElement::EmbedAlign           DontDelete
  height        KJS::JSHTMLElement::EmbedHeight          DontDelete
  name          KJS::JSHTMLElement::EmbedName            DontDelete
  src           KJS::JSHTMLElement::EmbedSrc             DontDelete
  type          KJS::JSHTMLElement::EmbedType            DontDelete
  width         KJS::JSHTMLElement::EmbedWidth           DontDelete
@end
@begin HTMLMapElementTable 2
  areas         KJS::JSHTMLElement::MapAreas              DontDelete|ReadOnly
  name          KJS::JSHTMLElement::MapName               DontDelete
@end
@begin HTMLAreaElementTable 15
  accessKey     KJS::JSHTMLElement::AreaAccessKey         DontDelete
  alt           KJS::JSHTMLElement::AreaAlt               DontDelete
  coords        KJS::JSHTMLElement::AreaCoords            DontDelete
  href          KJS::JSHTMLElement::AreaHref              DontDelete
  hash          KJS::JSHTMLElement::AreaHash              DontDelete|ReadOnly
  host          KJS::JSHTMLElement::AreaHost              DontDelete|ReadOnly
  hostname      KJS::JSHTMLElement::AreaHostName          DontDelete|ReadOnly
  pathname      KJS::JSHTMLElement::AreaPathName          DontDelete|ReadOnly
  port          KJS::JSHTMLElement::AreaPort              DontDelete|ReadOnly
  protocol      KJS::JSHTMLElement::AreaProtocol          DontDelete|ReadOnly
  search        KJS::JSHTMLElement::AreaSearch            DontDelete|ReadOnly
  noHref        KJS::JSHTMLElement::AreaNoHref            DontDelete
  shape         KJS::JSHTMLElement::AreaShape             DontDelete
  tabIndex      KJS::JSHTMLElement::AreaTabIndex          DontDelete
  target        KJS::JSHTMLElement::AreaTarget            DontDelete
@end
@begin HTMLScriptElementTable 7
  text          KJS::JSHTMLElement::ScriptText            DontDelete
  htmlFor       KJS::JSHTMLElement::ScriptHtmlFor         DontDelete
  event         KJS::JSHTMLElement::ScriptEvent           DontDelete
  charset       KJS::JSHTMLElement::ScriptCharset         DontDelete
  defer         KJS::JSHTMLElement::ScriptDefer           DontDelete
  src           KJS::JSHTMLElement::ScriptSrc             DontDelete
  type          KJS::JSHTMLElement::ScriptType            DontDelete
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

JSHTMLElement::JSHTMLElement(ExecState *exec, HTMLElement *e)
    : WebCore::JSHTMLElement(exec, e)
{
    setPrototype(JSHTMLElementProto::self(exec));
}

JSValue *JSHTMLElement::formIndexGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement *thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLFormElement *form = static_cast<HTMLFormElement*>(thisObj->impl());

    return toJS(exec, form->elements()->item(slot.index()));
}

JSValue *JSHTMLElement::formNameGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement *thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLFormElement *form = static_cast<HTMLFormElement*>(thisObj->impl());
    
    return JSHTMLCollection(exec, form->elements().get()).getNamedItems(exec, propertyName);
}

JSValue *JSHTMLElement::selectIndexGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement *thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLSelectElement *select = static_cast<HTMLSelectElement*>(thisObj->impl());

    return toJS(exec, select->options()->item(slot.index()));
}

JSValue *JSHTMLElement::framesetNameGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement *thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement *element = static_cast<HTMLElement*>(thisObj->impl());

    WebCore::Node *frame = element->children()->namedItem(propertyName);
    if (Document* doc = static_cast<HTMLFrameElement*>(frame)->contentDocument())
        if (Window *window = Window::retrieveWindow(doc->frame()))
            return window;

    return jsUndefined();
}

JSValue *JSHTMLElement::runtimeObjectGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement *thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement *element = static_cast<HTMLElement*>(thisObj->impl());

    return getRuntimeObject(exec, element);
}

JSValue *JSHTMLElement::runtimeObjectPropertyGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement *thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement *element = static_cast<HTMLElement*>(thisObj->impl());

    if (JSValue *runtimeObject = getRuntimeObject(exec, element))
        return static_cast<JSObject*>(runtimeObject)->get(exec, propertyName);
    return jsUndefined();
}

bool JSHTMLElement::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    HTMLElement &element = *static_cast<HTMLElement*>(impl());

    // First look at dynamic properties
    if (element.hasLocalName(formTag)) {
        HTMLFormElement &form = static_cast<HTMLFormElement &>(element);
        // Check if we're retrieving an element (by index or by name)
        bool ok;
        unsigned u = propertyName.toUInt32(&ok);
        if (ok) {
            slot.setCustomIndex(this, u, formIndexGetter);
            return true;
        }

        // FIXME: need faster way to check for a named item and/or a way to pass on the named items subcollection
        JSValue *namedItems = JSHTMLCollection(exec, form.elements().get()).getNamedItems(exec, propertyName);
        if (!namedItems->isUndefined()) {
            slot.setCustom(this, formNameGetter);
            return true;
        }
    } else if (element.hasLocalName(selectTag)) {
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
            JSObject *imp = static_cast<JSObject*>(runtimeObject);
            if (imp->hasProperty(exec, propertyName)) {
                slot.setCustom(this, runtimeObjectPropertyGetter);
                return true;
            }
        }
    }

    const HashTable* table = classInfo()->propHashTable; // get the right hashtable
    const HashEntry* entry = Lookup::findEntry(table, propertyName);
    if (entry) {
        // don't expose selection properties for input types that can't have a selection
        if (element.hasLocalName(inputTag) && !static_cast<HTMLInputElement*>(impl())->canHaveSelection()) {
            switch (entry->value) {
            case InputSetSelectionRange:
            case InputSelectionStart:
            case InputSelectionEnd:
                break;
            default:
                if (entry->attr & Function)
                    slot.setStaticEntry(this, entry, staticFunctionGetter<HTMLElementFunction>); 
                else
                    slot.setStaticEntry(this, entry, staticValueGetter<JSHTMLElement>);
                return true;
            }
        } else {
            if (entry->attr & Function)
                slot.setStaticEntry(this, entry, staticFunctionGetter<HTMLElementFunction>); 
            else
                slot.setStaticEntry(this, entry, staticValueGetter<JSHTMLElement>);
            return true;
        }
    }

    // Base JSHTMLElement stuff or parent class forward, as usual
    return getStaticPropertySlot<HTMLElementFunction, JSHTMLElement, WebCore::JSHTMLElement>(exec, &HTMLElementTable, this, propertyName, slot);
}

bool JSHTMLElement::implementsCall() const
{
    HTMLElement *element = static_cast<HTMLElement*>(impl());
    if (element->hasTagName(embedTag) || element->hasTagName(objectTag) || element->hasTagName(appletTag)) {
        Document* doc = element->document();
        KJSProxy *proxy = doc->frame()->jScript();
        ExecState *exec = proxy->interpreter()->globalExec();
        if (JSValue *runtimeObject = getRuntimeObject(exec, element))
            return static_cast<JSObject*>(runtimeObject)->implementsCall();
    }
    return false;
}

JSValue *JSHTMLElement::callAsFunction(ExecState *exec, JSObject *thisObj, const List&args)
{
    HTMLElement *element = static_cast<HTMLElement*>(impl());
    if (element->hasTagName(embedTag) || element->hasTagName(objectTag) || element->hasTagName(appletTag)) {
        if (JSValue *runtimeObject = getRuntimeObject(exec, element))
            return static_cast<JSObject*>(runtimeObject)->call(exec, thisObj, args);
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::htmlGetter(ExecState* exec, int token) const
{
    HTMLHtmlElement& html = *static_cast<HTMLHtmlElement*>(impl());
    if (token == HtmlVersion)
        return jsString(html.version());
    return jsUndefined();
}

JSValue *JSHTMLElement::isIndexGetter(ExecState* exec, int token) const
{
    HTMLIsIndexElement& isindex = *static_cast<HTMLIsIndexElement*>(impl());
    switch (token) {
        case IsIndexForm:            return toJS(exec, isindex.form()); // type HTMLFormElement
        case IsIndexPrompt:          return jsString(isindex.prompt());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::bodyGetter(ExecState* exec, int token) const
{
    HTMLBodyElement& body = *static_cast<HTMLBodyElement*>(impl());
    switch (token) {
        case BodyALink:           return jsString(body.aLink());
        case BodyBackground:      return jsString(body.background());
        case BodyBgColor:         return jsString(body.bgColor());
        case BodyLink:            return jsString(body.link());
        case BodyText:            return jsString(body.text());
        case BodyVLink:           return jsString(body.vLink());
        default: {
            // Update the document's layout before we compute these attributes.
            Document *doc = body.document();
            doc->updateLayoutIgnorePendingStylesheets();
            FrameView *view = doc->view();
            switch (token) {
                case BodyScrollLeft:
                    return jsNumber(view ? view->contentsX() : 0);
                case BodyScrollTop:
                    return jsNumber(view ? view->contentsY() : 0);
                case BodyScrollHeight:
                    return jsNumber(view ? view->contentsHeight() : 0);
                case BodyScrollWidth:
                    return jsNumber(view ? view->contentsWidth() : 0);
            }
        }
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::formGetter(ExecState* exec, int token) const
{
    HTMLFormElement& form = *static_cast<HTMLFormElement*>(impl());
    switch (token) {
        case FormElements:        return getHTMLCollection(exec, form.elements().get());
        case FormLength:          return jsNumber(form.length());
        case FormName:            return jsString(form.name());
        case FormAcceptCharset:   return jsString(form.acceptCharset());
        case FormAction:          return jsString(form.action());
        case FormEncType:         return jsString(form.enctype());
        case FormMethod:          return jsString(form.method());
        case FormTarget:          return jsString(form.target());
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

JSValue *JSHTMLElement::optGroupGetter(ExecState* exec, int token) const
{
    HTMLOptGroupElement& optgroup = *static_cast<HTMLOptGroupElement*>(impl());
    switch (token) {
        case OptGroupDisabled:        return jsBoolean(optgroup.disabled());
        case OptGroupLabel:           return jsString(optgroup.label());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::optionGetter(ExecState* exec, int token) const
{
    HTMLOptionElement& option = *static_cast<HTMLOptionElement*>(impl());
    switch (token) {
        case OptionForm:            return toJS(exec,option.form()); // type HTMLFormElement
        case OptionDefaultSelected: return jsBoolean(option.defaultSelected());
        case OptionText:            return jsString(option.text());
        case OptionIndex:           return jsNumber(option.index());
        case OptionDisabled:        return jsBoolean(option.disabled());
        case OptionLabel:           return jsString(option.label());
        case OptionSelected:        return jsBoolean(option.selected());
        case OptionValue:           return jsString(option.value());
    }
    return jsUndefined();
}

static JSValue *getInputSelectionStart(HTMLInputElement &input)
{
    if (input.canHaveSelection())
        return jsNumber(input.selectionStart());
    return jsUndefined();
}

static JSValue *getInputSelectionEnd(HTMLInputElement &input)
{
    if (input.canHaveSelection())
        return jsNumber(input.selectionEnd());
    return jsUndefined();
}

JSValue *JSHTMLElement::inputGetter(ExecState* exec, int token) const
{
    HTMLInputElement& input = *static_cast<HTMLInputElement*>(impl());
    switch (token) {
        case InputDefaultValue:    return jsString(input.defaultValue());
        case InputDefaultChecked:  return jsBoolean(input.defaultChecked());
        case InputForm:            return toJS(exec,input.form()); // type HTMLFormElement
        case InputAccept:          return jsString(input.accept());
        case InputAccessKey:       return jsString(input.accessKey());
        case InputAlign:           return jsString(input.align());
        case InputAlt:             return jsString(input.alt());
        case InputChecked:         return jsBoolean(input.checked());
        case InputDisabled:        return jsBoolean(input.disabled());
        case InputIndeterminate:   return jsBoolean(input.indeterminate());
        case InputMaxLength:       return jsNumber(input.maxLength());
        case InputName:            return jsString(input.name());
        case InputReadOnly:        return jsBoolean(input.readOnly());
        case InputSelectionStart:  return getInputSelectionStart(input);
        case InputSelectionEnd:    return getInputSelectionEnd(input);
        case InputSize:            return jsNumber(input.size());
        case InputSrc:             return jsString(input.src());
        case InputTabIndex:        return jsNumber(input.tabIndex());
        case InputType:            return jsString(input.type());
        case InputUseMap:          return jsString(input.useMap());
        case InputValue:           return jsString(input.value());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::textAreaGetter(ExecState* exec, int token) const
{
    HTMLTextAreaElement& textarea = *static_cast<HTMLTextAreaElement*>(impl());
    switch (token) {
        case TextAreaDefaultValue:    return jsString(textarea.defaultValue());
        case TextAreaForm:            return toJS(exec,textarea.form()); // type HTMLFormElement
        case TextAreaAccessKey:       return jsString(textarea.accessKey());
        case TextAreaCols:            return jsNumber(textarea.cols());
        case TextAreaDisabled:        return jsBoolean(textarea.disabled());
        case TextAreaName:            return jsString(textarea.name());
        case TextAreaReadOnly:        return jsBoolean(textarea.readOnly());
        case TextAreaRows:            return jsNumber(textarea.rows());
        case TextAreaSelectionStart:  return jsNumber(textarea.selectionStart());
        case TextAreaSelectionEnd:    return jsNumber(textarea.selectionEnd());
        case TextAreaTabIndex:        return jsNumber(textarea.tabIndex());
        case TextAreaType:            return jsString(textarea.type());
        case TextAreaValue:           return jsString(textarea.value());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::buttonGetter(ExecState* exec, int token) const
{
    HTMLButtonElement& button = *static_cast<HTMLButtonElement*>(impl());
    switch (token) {
        case ButtonForm:            return toJS(exec,button.form()); // type HTMLFormElement
        case ButtonAccessKey:       return jsString(button.accessKey());
        case ButtonDisabled:        return jsBoolean(button.disabled());
        case ButtonName:            return jsString(button.name());
        case ButtonTabIndex:        return jsNumber(button.tabIndex());
        case ButtonType:            return jsString(button.type());
        case ButtonValue:           return jsString(button.value());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::labelGetter(ExecState* exec, int token) const
{
    HTMLLabelElement& label = *static_cast<HTMLLabelElement*>(impl());
    switch (token) {
        case LabelForm:            return toJS(exec,label.form()); // type HTMLFormElement
        case LabelAccessKey:       return jsString(label.accessKey());
        case LabelHtmlFor:         return jsString(label.htmlFor());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::fieldSetGetter(ExecState* exec, int token) const
{
    HTMLFieldSetElement& fieldSet = *static_cast<HTMLFieldSetElement*>(impl());
    if (token == FieldSetForm)
        return toJS(exec,fieldSet.form()); // type HTMLFormElement
    return jsUndefined();
}

JSValue *JSHTMLElement::legendGetter(ExecState* exec, int token) const
{
    HTMLLegendElement& legend = *static_cast<HTMLLegendElement*>(impl());
    switch (token) {
        case LegendForm:            return toJS(exec,legend.form()); // type HTMLFormElement
        case LegendAccessKey:       return jsString(legend.accessKey());
        case LegendAlign:           return jsString(legend.align());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::uListGetter(ExecState* exec, int token) const
{
    HTMLUListElement& uList = *static_cast<HTMLUListElement*>(impl());
    switch (token) {
        case UListCompact:         return jsBoolean(uList.compact());
        case UListType:            return jsString(uList.type());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::oListGetter(ExecState* exec, int token) const
{
    HTMLOListElement& oList = *static_cast<HTMLOListElement*>(impl());
    switch (token) {
        case OListCompact:         return jsBoolean(oList.compact());
        case OListStart:           return jsNumber(oList.start());
        case OListType:            return jsString(oList.type());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::dListGetter(ExecState* exec, int token) const
{
    HTMLDListElement& dList = *static_cast<HTMLDListElement*>(impl());
    if (token == DListCompact)
        return jsBoolean(dList.compact());
    return jsUndefined();
}

JSValue *JSHTMLElement::dirGetter(ExecState* exec, int token) const
{
    HTMLDirectoryElement& dir = *static_cast<HTMLDirectoryElement*>(impl());
    if (token == DirectoryCompact)
        return jsBoolean(dir.compact());
    return jsUndefined();
}

JSValue *JSHTMLElement::menuGetter(ExecState* exec, int token) const
{
    HTMLMenuElement& menu = *static_cast<HTMLMenuElement*>(impl());
    if (token == MenuCompact)
        return jsBoolean(menu.compact());
    return jsUndefined();
}

JSValue *JSHTMLElement::liGetter(ExecState* exec, int token) const
{
    HTMLLIElement& li = *static_cast<HTMLLIElement*>(impl());
    switch (token) {
        case LIType:            return jsString(li.type());
        case LIValue:           return jsNumber(li.value());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::divGetter(ExecState* exec, int token) const
{
    HTMLDivElement& div = *static_cast<HTMLDivElement*>(impl());
    if (token == DivAlign)
        return jsString(div.align());
    return jsUndefined();
}

JSValue *JSHTMLElement::paragraphGetter(ExecState* exec, int token) const
{
    HTMLParagraphElement& p = *static_cast<HTMLParagraphElement*>(impl());
    if (token == ParagraphAlign)
        return jsString(p.align());
    return jsUndefined();
}

JSValue *JSHTMLElement::headingGetter(ExecState* exec, int token) const
{
    HTMLHeadingElement& h = *static_cast<HTMLHeadingElement*>(impl());
    if (token == HeadingAlign)
        return jsString(h.align());
    return jsUndefined();
}

JSValue *JSHTMLElement::blockQuoteGetter(ExecState* exec, int token) const
{
    HTMLBlockquoteElement& blockQuote = *static_cast<HTMLBlockquoteElement*>(impl());
    if (token == BlockQuoteCite)
        return jsString(blockQuote.cite());
    return jsUndefined();
}

JSValue *JSHTMLElement::quoteGetter(ExecState* exec, int token) const
{
    HTMLQuoteElement& quote = *static_cast<HTMLQuoteElement*>(impl());
    if (token == QuoteCite)
        return jsString(quote.cite());
    return jsUndefined();
}

JSValue *JSHTMLElement::preGetter(ExecState* exec, int token) const
{
    // FIXME: Add support for 'wrap' when white-space: pre-wrap is implemented.
    HTMLPreElement& pre = *static_cast<HTMLPreElement*>(impl());
    if (token == PreWidth)
        return jsNumber(pre.width());
    if (token == PreWrap)
        return jsBoolean(pre.wrap());
    return jsUndefined();
}

JSValue *JSHTMLElement::brGetter(ExecState* exec, int token) const
{
    HTMLBRElement& br = *static_cast<HTMLBRElement*>(impl());
    if (token == BRClear)
        return jsString(br.clear());
    return jsUndefined();
}

JSValue *JSHTMLElement::baseFontGetter(ExecState* exec, int token) const
{
    HTMLBaseFontElement& baseFont = *static_cast<HTMLBaseFontElement*>(impl());
    switch (token) {
        case BaseFontColor:           return jsString(baseFont.color());
        case BaseFontFace:            return jsString(baseFont.face());
        case BaseFontSize:            return jsString(baseFont.size());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::fontGetter(ExecState* exec, int token) const
{
    HTMLFontElement& font = *static_cast<HTMLFontElement*>(impl());
    switch (token) {
        case FontColor:           return jsString(font.color());
        case FontFace:            return jsString(font.face());
        case FontSize:            return jsString(font.size());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::hrGetter(ExecState* exec, int token) const
{
    HTMLHRElement& hr = *static_cast<HTMLHRElement*>(impl());
    switch (token) {
        case HRAlign:           return jsString(hr.align());
        case HRNoShade:         return jsBoolean(hr.noShade());
        case HRSize:            return jsString(hr.size());
        case HRWidth:           return jsString(hr.width());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::modGetter(ExecState* exec, int token) const
{
    HTMLModElement& mod = *static_cast<HTMLModElement*>(impl());
    switch (token) {
        case ModCite:            return jsString(mod.cite());
        case ModDateTime:        return jsString(mod.dateTime());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::anchorGetter(ExecState* exec, int token) const
{
    HTMLAnchorElement& anchor = *static_cast<HTMLAnchorElement*>(impl());
    switch (token) {
        case AnchorAccessKey:       return jsString(anchor.accessKey());
        case AnchorCharset:         return jsString(anchor.charset());
        case AnchorCoords:          return jsString(anchor.coords());
        case AnchorHref:            return jsString(anchor.href());
        case AnchorHrefLang:        return jsString(anchor.hreflang());
        case AnchorHash:            return jsString('#'+KURL(anchor.href().deprecatedString()).ref());
        case AnchorHost:            return jsString(KURL(anchor.href().deprecatedString()).host());
        case AnchorHostname: {
            KURL url(anchor.href().deprecatedString());
            if (url.port()==0)
                return jsString(url.host());
            else
                return jsString(url.host() + ":" + DeprecatedString::number(url.port()));
        }
        case AnchorPathName:        return jsString(KURL(anchor.href().deprecatedString()).path());
        case AnchorPort:            return jsString(DeprecatedString::number(KURL(anchor.href().deprecatedString()).port()));
        case AnchorProtocol:        return jsString(KURL(anchor.href().deprecatedString()).protocol()+":");
        case AnchorSearch:          return jsString(KURL(anchor.href().deprecatedString()).query());
        case AnchorName:            return jsString(anchor.name());
        case AnchorRel:             return jsString(anchor.rel());
        case AnchorRev:             return jsString(anchor.rev());
        case AnchorShape:           return jsString(anchor.shape());
        case AnchorTabIndex:        return jsNumber(anchor.tabIndex());
        case AnchorTarget:          return jsString(anchor.target());
        case AnchorType:            return jsString(anchor.type());
        case AnchorText:
            anchor.document()->updateLayoutIgnorePendingStylesheets();
            return jsString(anchor.innerText());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::imageGetter(ExecState* exec, int token) const
{
    HTMLImageElement& image = *static_cast<HTMLImageElement*>(impl());
    switch (token) {
        case ImageName:            return jsString(image.name());
        case ImageAlign:           return jsString(image.align());
        case ImageAlt:             return jsString(image.alt());
        case ImageBorder:          return jsNumber(image.border());
        case ImageComplete:        return jsBoolean(image.complete());
        case ImageHeight:          return jsNumber(image.height(true));
        case ImageHspace:          return jsNumber(image.hspace());
        case ImageIsMap:           return jsBoolean(image.isMap());
        case ImageLongDesc:        return jsString(image.longDesc());
        case ImageSrc:             return jsString(image.src());
        case ImageUseMap:          return jsString(image.useMap());
        case ImageVspace:          return jsNumber(image.vspace());
        case ImageWidth:           return jsNumber(image.width(true));
        case ImageX:               return jsNumber(image.x());
        case ImageY:               return jsNumber(image.y());
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

JSValue *JSHTMLElement::paramGetter(ExecState* exec, int token) const
{
    HTMLParamElement& param = *static_cast<HTMLParamElement*>(impl());
    switch (token) {
        case ParamName:            return jsString(param.name());
        case ParamType:            return jsString(param.type());
        case ParamValue:           return jsString(param.value());
        case ParamValueType:       return jsString(param.valueType());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::appletGetter(ExecState* exec, int token) const
{
    HTMLAppletElement& applet = *static_cast<HTMLAppletElement*>(impl());
    switch (token) {
        case AppletAlign:           return jsString(applet.align());
        case AppletAlt:             return jsString(applet.alt());
        case AppletArchive:         return jsString(applet.archive());
        case AppletCode:            return jsString(applet.code());
        case AppletCodeBase:        return jsString(applet.codeBase());
        case AppletHeight:          return jsString(applet.height());
        case AppletHspace:          return jsString(applet.hspace());
        case AppletName:            return jsString(applet.name());
        case AppletObject:          return jsString(applet.object());
        case AppletVspace:          return jsString(applet.vspace());
        case AppletWidth:           return jsString(applet.width());
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

JSValue *JSHTMLElement::mapGetter(ExecState* exec, int token) const
{
    HTMLMapElement& map = *static_cast<HTMLMapElement*>(impl());
    switch (token) {
        case MapAreas:           return getHTMLCollection(exec, map.areas().get()); // type JSHTMLCollection
        case MapName:            return jsString(map.name());
    }
    return jsUndefined();
}

JSValue *JSHTMLElement::areaGetter(ExecState* exec, int token) const
{
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
    return jsUndefined();
}

JSValue *JSHTMLElement::scriptGetter(ExecState* exec, int token) const
{
    HTMLScriptElement& script = *static_cast<HTMLScriptElement*>(impl());
    switch (token) {
        case ScriptText:            return jsString(script.text());
        case ScriptHtmlFor:         return jsString(script.htmlFor());
        case ScriptEvent:           return jsString(script.event());
        case ScriptCharset:         return jsString(script.charset());
        case ScriptDefer:           return jsBoolean(script.defer());
        case ScriptSrc:             return jsString(script.src());
        case ScriptType:            return jsString(script.type());
    }
    return jsUndefined();
}

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

JSValue *JSHTMLElement::getValueProperty(ExecState *exec, int token) const
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

UString JSHTMLElement::toString(ExecState *exec) const
{
    if (impl()->hasTagName(aTag))
        return UString(static_cast<const HTMLAnchorElement*>(impl())->href());
    else
        return JSElement::toString(exec);
}

static HTMLFormElement *getForm(HTMLElement *element)
{
    if (element->isGenericFormElement())
        return static_cast<HTMLGenericFormElement*>(element)->form();
    if (element->hasTagName(labelTag))
        return static_cast<HTMLLabelElement*>(element)->form();
    if (element->hasTagName(objectTag))
        return static_cast<HTMLObjectElement*>(element)->form();

    return 0;
}

void JSHTMLElement::pushEventHandlerScope(ExecState *exec, ScopeChain &scope) const
{
  HTMLElement *element = static_cast<HTMLElement*>(impl());

  // The document is put on first, fall back to searching it only after the element and form.
  scope.push(static_cast<JSObject*>(toJS(exec, element->ownerDocument())));

  // The form is next, searched before the document, but after the element itself.
  
  // First try to obtain the form from the element itself.  We do this to deal with
  // the malformed case where <form>s aren't in our parent chain (e.g., when they were inside 
  // <table> or <tbody>.
  HTMLFormElement *form = getForm(element);
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

HTMLElementFunction::HTMLElementFunction(ExecState *exec, int i, int len, const Identifier& name)
  : InternalFunctionImp(static_cast<FunctionPrototype*>(exec->lexicalInterpreter()->builtinFunctionPrototype()), name)
  , id(i)
{
  put(exec,lengthPropertyName,jsNumber(len),DontDelete|ReadOnly|DontEnum);
}

JSValue *HTMLElementFunction::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
    if (!thisObj->inherits(&JSHTMLElement::info))
        return throwError(exec, TypeError);
    DOMExceptionTranslator exception(exec);
    HTMLElement &element = *static_cast<HTMLElement*>(static_cast<JSHTMLElement*>(thisObj)->impl());

    if (element.hasLocalName(formTag)) {
        HTMLFormElement &form = static_cast<HTMLFormElement &>(element);
        if (id == JSHTMLElement::FormSubmit) {
            form.submit();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::FormReset) {
            form.reset();
            return jsUndefined();
        }
    }
    else if (element.hasLocalName(selectTag)) {
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
    }
    else if (element.hasLocalName(inputTag)) {
        HTMLInputElement &input = static_cast<HTMLInputElement &>(element);
        if (id == JSHTMLElement::InputBlur) {
            input.blur();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::InputFocus) {
            input.focus();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::InputSelect) {
            input.select();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::InputClick) {
            input.click();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::InputSetSelectionRange) {
            input.setSelectionRange(args[0]->toInt32(exec), args[1]->toInt32(exec));
            return jsUndefined();
        }
    }
    else if (element.hasLocalName(buttonTag)) {
        HTMLButtonElement &button = static_cast<HTMLButtonElement &>(element);
        if (id == JSHTMLElement::ButtonBlur) {
            button.blur();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::ButtonFocus) {
            button.focus();
            return jsUndefined();
        }
    }
    else if (element.hasLocalName(labelTag)) {
        HTMLLabelElement &label = static_cast<HTMLLabelElement &>(element);
        if (id == JSHTMLElement::LabelFocus) {
            label.focus();
            return jsUndefined();
        }
    }
    else if (element.hasLocalName(legendTag)) {
        HTMLLegendElement &legend = static_cast<HTMLLegendElement &>(element);
        if (id == JSHTMLElement::LegendFocus) {
            legend.focus();
            return jsUndefined();
        }
    }
    else if (element.hasLocalName(textareaTag)) {
        HTMLTextAreaElement &textarea = static_cast<HTMLTextAreaElement &>(element);
        if (id == JSHTMLElement::TextAreaBlur) {
            textarea.blur();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::TextAreaFocus) {
            textarea.focus();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::TextAreaSelect) {
            textarea.select();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::TextAreaSetSelectionRange) {
            textarea.setSelectionRange(args[0]->toInt32(exec), args[1]->toInt32(exec));
            return jsUndefined();
        }
    }
    else if (element.hasLocalName(aTag)) {
        HTMLAnchorElement &anchor = static_cast<HTMLAnchorElement &>(element);
        if (id == JSHTMLElement::AnchorBlur) {
            anchor.blur();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::AnchorFocus) {
            anchor.focus();
            return jsUndefined();
        }
        else if (id == JSHTMLElement::AnchorToString)
            return jsString(thisObj->toString(exec));
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

void JSHTMLElement::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
    HTMLElement &element = *static_cast<HTMLElement*>(impl());
    // First look at dynamic properties
    if (element.hasLocalName(selectTag)) {
        HTMLSelectElement &select = static_cast<HTMLSelectElement &>(element);
        bool ok;
        /*unsigned u =*/ propertyName.toUInt32(&ok);
        if (ok) {
            JSObject *coll = static_cast<JSObject*>(getSelectHTMLCollection(exec, select.options().get(), &select));
            coll->put(exec,propertyName,value);
            return;
        }
    }
    else if (element.hasLocalName(embedTag) || element.hasLocalName(objectTag) || element.hasLocalName(appletTag)) {
        if (JSValue *runtimeObject = getRuntimeObject(exec, &element)) {
            JSObject *imp = static_cast<JSObject*>(runtimeObject);
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
        }
        else if (!(entry->attr & ReadOnly)) { // let lookupPut print the warning if read-only
            putValueProperty(exec, entry->value, value, attr);
            return;
        }
    }

    lookupPut<JSHTMLElement, JSElement>(exec, propertyName, value, attr, &HTMLElementTable, this);
}

void JSHTMLElement::htmlSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLHeadElement &head = *static_cast<HTMLHeadElement*>(impl());
    if (token == HeadProfile) 
        head.setProfile(str);
}

void JSHTMLElement::isIndexSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLIsIndexElement& isindex = *static_cast<HTMLIsIndexElement*>(impl());
    if (token == IsIndexPrompt)
        isindex.setPrompt(str);
}

void JSHTMLElement::bodySetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLBodyElement& body = *static_cast<HTMLBodyElement*>(impl());
    switch (token) {
        case BodyALink:           { body.setALink(str); return; }
        case BodyBackground:      { body.setBackground(str); return; }
        case BodyBgColor:         { body.setBgColor(str); return; }
        case BodyLink:            { body.setLink(str); return; }
        case BodyText:            { body.setText(str); return; }
        case BodyVLink:           { body.setVLink(str); return; }
        case BodyScrollLeft:
        case BodyScrollTop: {
            FrameView* sview = body.ownerDocument()->view();
            if (sview) {
                // Update the document's layout before we compute these attributes.
                body.document()->updateLayoutIgnorePendingStylesheets();
                if (token == BodyScrollLeft)
                    sview->setContentsPos(value->toInt32(exec), sview->contentsY());
                else
                    sview->setContentsPos(sview->contentsX(), value->toInt32(exec));
            }
            return;
        }
    }
}

void JSHTMLElement::formSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLFormElement& form = *static_cast<HTMLFormElement*>(impl());
    switch (token) {
        // read-only: elements
        // read-only: length
        case FormName:            { form.setName(str); return; }
        case FormAcceptCharset:   { form.setAcceptCharset(str); return; }
        case FormAction:          { form.setAction(str); return; }
        case FormEncType:         { form.setEnctype(str); return; }
        case FormMethod:          { form.setMethod(str); return; }
        case FormTarget:          { form.setTarget(str); return; }
    }
}

void JSHTMLElement::selectSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLSelectElement& select = *static_cast<HTMLSelectElement*>(impl());
    switch (token) {
        // read-only: type
        case SelectSelectedIndex:   { select.setSelectedIndex(value->toInt32(exec)); return; }
        case SelectValue:           { select.setValue(str); return; }
        case SelectLength:          { // read-only according to the NS spec, but webpages need it writeable
                                        JSObject *coll = static_cast<JSObject*>(getSelectHTMLCollection(exec, select.options().get(), &select));
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

void JSHTMLElement::optGroupSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLOptGroupElement& optgroup = *static_cast<HTMLOptGroupElement*>(impl());
    switch (token) {
        case OptGroupDisabled:        { optgroup.setDisabled(value->toBoolean(exec)); return; }
        case OptGroupLabel:           { optgroup.setLabel(str); return; }
    }
}

void JSHTMLElement::optionSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    DOMExceptionTranslator exception(exec);
    HTMLOptionElement& option = *static_cast<HTMLOptionElement*>(impl());
    switch (token) {
        // read-only: form
        case OptionDefaultSelected: { option.setDefaultSelected(value->toBoolean(exec)); return; }
        case OptionText:            { option.setText(str, exception); return; }
        // read-only: index
        case OptionDisabled:        { option.setDisabled(value->toBoolean(exec)); return; }
        case OptionLabel:           { option.setLabel(str); return; }
        case OptionSelected:        { option.setSelected(value->toBoolean(exec)); return; }
        case OptionValue:           { option.setValue(str); return; }
    }
}

void JSHTMLElement::inputSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLInputElement& input = *static_cast<HTMLInputElement*>(impl());
    switch (token) {
        case InputDefaultValue:    { input.setDefaultValue(str); return; }
        case InputDefaultChecked:  { input.setDefaultChecked(value->toBoolean(exec)); return; }
        // read-only: form
        case InputAccept:          { input.setAccept(str); return; }
        case InputAccessKey:       { input.setAccessKey(str); return; }
        case InputAlign:           { input.setAlign(str); return; }
        case InputAlt:             { input.setAlt(str); return; }
        case InputChecked:         { input.setChecked(value->toBoolean(exec)); return; }
        case InputDisabled:        { input.setDisabled(value->toBoolean(exec)); return; }
        case InputIndeterminate:   { input.setIndeterminate(value->toBoolean(exec)); return; }
        case InputMaxLength:       { input.setMaxLength(value->toInt32(exec)); return; }
        case InputName:            { input.setName(AtomicString(str)); return; }
        case InputReadOnly:        { input.setReadOnly(value->toBoolean(exec)); return; }
        case InputSize:            { input.setSize(value->toInt32(exec)); return; }
        case InputSelectionStart:  { input.setSelectionStart(value->toInt32(exec)); return; }
        case InputSelectionEnd:    { input.setSelectionEnd(value->toInt32(exec)); return; }
        case InputSrc:             { input.setSrc(str); return; }
        case InputTabIndex:        { input.setTabIndex(value->toInt32(exec)); return; }
        case InputType:            { input.setType(str); return; }
        case InputUseMap:          { input.setUseMap(str); return; }
        case InputValue:           { input.setValue(str); return; }
    }
}

void JSHTMLElement::textAreaSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLTextAreaElement& textarea = *static_cast<HTMLTextAreaElement*>(impl());
    switch (token) {
        case TextAreaDefaultValue:    { textarea.setDefaultValue(str); return; }
        // read-only: form
        case TextAreaAccessKey:       { textarea.setAccessKey(str); return; }
        case TextAreaCols:            { textarea.setCols(value->toInt32(exec)); return; }
        case TextAreaDisabled:        { textarea.setDisabled(value->toBoolean(exec)); return; }
        case TextAreaName:            { textarea.setName(AtomicString(str)); return; }
        case TextAreaReadOnly:        { textarea.setReadOnly(value->toBoolean(exec)); return; }
        case TextAreaRows:            { textarea.setRows(value->toInt32(exec)); return; }
        case TextAreaSelectionStart:  { textarea.setSelectionStart(value->toInt32(exec)); return; }
        case TextAreaSelectionEnd:    { textarea.setSelectionEnd(value->toInt32(exec)); return; }
        case TextAreaTabIndex:        { textarea.setTabIndex(value->toInt32(exec)); return; }
        // read-only: type
        case TextAreaValue:           { textarea.setValue(str); return; }
    }
}

void JSHTMLElement::buttonSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLButtonElement& button = *static_cast<HTMLButtonElement*>(impl());
    switch (token) {
        // read-only: form
        case ButtonAccessKey:       { button.setAccessKey(str); return; }
        case ButtonDisabled:        { button.setDisabled(value->toBoolean(exec)); return; }
        case ButtonName:            { button.setName(AtomicString(str)); return; }
        case ButtonTabIndex:        { button.setTabIndex(value->toInt32(exec)); return; }
        // read-only: type
        case ButtonValue:           { button.setValue(str); return; }
    }
}

void JSHTMLElement::labelSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLLabelElement& label = *static_cast<HTMLLabelElement*>(impl());
    switch (token) {
        // read-only: form
        case LabelAccessKey:       { label.setAccessKey(str); return; }
        case LabelHtmlFor:         { label.setHtmlFor(str); return; }
    }
}

void JSHTMLElement::fieldSetSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
}

void JSHTMLElement::legendSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLLegendElement& legend = *static_cast<HTMLLegendElement*>(impl());
    switch (token) {
        // read-only: form
        case LegendAccessKey:       { legend.setAccessKey(str); return; }
        case LegendAlign:           { legend.setAlign(str); return; }
    }
}

void JSHTMLElement::uListSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLUListElement& uList = *static_cast<HTMLUListElement*>(impl());
    switch (token) {
        case UListCompact:         { uList.setCompact(value->toBoolean(exec)); return; }
        case UListType:            { uList.setType(str); return; }
    }
}

void JSHTMLElement::oListSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLOListElement& oList = *static_cast<HTMLOListElement*>(impl());
    switch (token) {
        case OListCompact:         { oList.setCompact(value->toBoolean(exec)); return; }
        case OListStart:           { oList.setStart(value->toInt32(exec)); return; }
        case OListType:            { oList.setType(str); return; }
    }
}

void JSHTMLElement::dListSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLDListElement& dList = *static_cast<HTMLDListElement*>(impl());
    if (token == DListCompact)
        dList.setCompact(value->toBoolean(exec));
}

void JSHTMLElement::dirSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLDirectoryElement& directory = *static_cast<HTMLDirectoryElement*>(impl());
    if (token == DirectoryCompact)
        directory.setCompact(value->toBoolean(exec));
}

void JSHTMLElement::menuSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLMenuElement& menu = *static_cast<HTMLMenuElement*>(impl());
    if (token == MenuCompact)
        menu.setCompact(value->toBoolean(exec));
}

void JSHTMLElement::liSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLLIElement& li = *static_cast<HTMLLIElement*>(impl());
    switch (token) {
        case LIType:            { li.setType(str); return; }
        case LIValue:           { li.setValue(value->toInt32(exec)); return; }
    }
}

void JSHTMLElement::divSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLDivElement& div = *static_cast<HTMLDivElement*>(impl());
    if (token == DivAlign)
        div.setAlign(str);
}

void JSHTMLElement::paragraphSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLParagraphElement& paragraph = *static_cast<HTMLParagraphElement*>(impl());
    if (token == ParagraphAlign)
        paragraph.setAlign(str);
}

void JSHTMLElement::headingSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLHeadingElement& heading = *static_cast<HTMLHeadingElement*>(impl());
    if (token == HeadingAlign)
        heading.setAlign(str);
}

void JSHTMLElement::blockQuoteSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLBlockquoteElement& blockQuote = *static_cast<HTMLBlockquoteElement*>(impl());
    if (token == BlockQuoteCite)
        blockQuote.setCite(str);
}

void JSHTMLElement::quoteSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLQuoteElement& quote = *static_cast<HTMLQuoteElement*>(impl());
    if (token == QuoteCite)
        quote.setCite(str);
}

void JSHTMLElement::preSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLPreElement& pre = *static_cast<HTMLPreElement*>(impl());
    if (token == PreWidth)
        pre.setWidth(value->toInt32(exec));
    else if (token == PreWrap)
        pre.setWrap(value->toBoolean(exec));
}

void JSHTMLElement::brSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLBRElement& br = *static_cast<HTMLBRElement*>(impl());
    if (token == BRClear)
        br.setClear(str);
}

void JSHTMLElement::baseFontSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLBaseFontElement& baseFont = *static_cast<HTMLBaseFontElement*>(impl());
    switch (token) {
        case BaseFontColor:           { baseFont.setColor(str); return; }
        case BaseFontFace:            { baseFont.setFace(str); return; }
        case BaseFontSize:            { baseFont.setSize(str); return; }
    }
}

void JSHTMLElement::fontSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLFontElement& font = *static_cast<HTMLFontElement*>(impl());
    switch (token) {
        case FontColor:           { font.setColor(str); return; }
        case FontFace:            { font.setFace(str); return; }
        case FontSize:            { font.setSize(str); return; }
    }
}

void JSHTMLElement::hrSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLHRElement& hr = *static_cast<HTMLHRElement*>(impl());
    switch (token) {
        case HRAlign:           { hr.setAlign(str); return; }
        case HRNoShade:         { hr.setNoShade(value->toBoolean(exec)); return; }
        case HRSize:            { hr.setSize(str); return; }
        case HRWidth:           { hr.setWidth(str); return; }
    }
}

void JSHTMLElement::modSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLModElement& mod = *static_cast<HTMLModElement*>(impl());
    switch (token) {
        case ModCite:            { mod.setCite(str); return; }
        case ModDateTime:        { mod.setDateTime(str); return; }
    }
}

void JSHTMLElement::anchorSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLAnchorElement& anchor = *static_cast<HTMLAnchorElement*>(impl());
    switch (token) {
        case AnchorAccessKey:       { anchor.setAccessKey(str); return; }
        case AnchorCharset:         { anchor.setCharset(str); return; }
        case AnchorCoords:          { anchor.setCoords(str); return; }
        case AnchorHref:            { anchor.setHref(str); return; }
        case AnchorHrefLang:        { anchor.setHreflang(str); return; }
        case AnchorName:            { anchor.setName(str); return; }
        case AnchorRel:             { anchor.setRel(str); return; }
        case AnchorRev:             { anchor.setRev(str); return; }
        case AnchorShape:           { anchor.setShape(str); return; }
        case AnchorTabIndex:        { anchor.setTabIndex(value->toInt32(exec)); return; }
        case AnchorTarget:          { anchor.setTarget(str); return; }
        case AnchorType:            { anchor.setType(str); return; }
    }
}

void JSHTMLElement::imageSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLImageElement& image = *static_cast<HTMLImageElement*>(impl());
    switch (token) {
        case ImageName:            { image.setName(str); return; }
        case ImageAlign:           { image.setAlign(str); return; }
        case ImageAlt:             { image.setAlt(str); return; }
        case ImageBorder:          { image.setBorder(value->toInt32(exec)); return; }
        case ImageHeight:          { image.setHeight(value->toInt32(exec)); return; }
        case ImageHspace:          { image.setHspace(value->toInt32(exec)); return; }
        case ImageIsMap:           { image.setIsMap(value->toBoolean(exec)); return; }
        case ImageLongDesc:        { image.setLongDesc(str); return; }
        case ImageSrc:             { image.setSrc(str); return; }
        case ImageUseMap:          { image.setUseMap(str); return; }
        case ImageVspace:          { image.setVspace(value->toInt32(exec)); return; }
        case ImageWidth:           { image.setWidth(value->toInt32(exec)); return; }
    }
}

void JSHTMLElement::objectSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
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

void JSHTMLElement::paramSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLParamElement& param = *static_cast<HTMLParamElement*>(impl());
    switch (token) {
        case ParamName:            { param.setName(str); return; }
        case ParamType:            { param.setType(str); return; }
        case ParamValue:           { param.setValue(str); return; }
        case ParamValueType:       { param.setValueType(str); return; }
    }
}

void JSHTMLElement::appletSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLAppletElement& applet = *static_cast<HTMLAppletElement*>(impl());
    switch (token) {
        case AppletAlign:           { applet.setAlign(str); return; }
        case AppletAlt:             { applet.setAlt(str); return; }
        case AppletArchive:         { applet.setArchive(str); return; }
        case AppletCode:            { applet.setCode(str); return; }
        case AppletCodeBase:        { applet.setCodeBase(str); return; }
        case AppletHeight:          { applet.setHeight(str); return; }
        case AppletHspace:          { applet.setHspace(str); return; }
        case AppletName:            { applet.setName(str); return; }
        case AppletObject:          { applet.setObject(str); return; }
        case AppletVspace:          { applet.setVspace(str); return; }
        case AppletWidth:           { applet.setWidth(str); return; }
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

void JSHTMLElement::mapSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLMapElement& map = *static_cast<HTMLMapElement*>(impl());
    if (token == MapName)
        // read-only: areas
        map.setName(str);
}

void JSHTMLElement::areaSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLAreaElement& area = *static_cast<HTMLAreaElement*>(impl());
    switch (token) {
        case AreaAccessKey:       { area.setAccessKey(str); return; }
        case AreaAlt:             { area.setAlt(str); return; }
        case AreaCoords:          { area.setCoords(str); return; }
        case AreaHref:            { area.setHref(str); return; }
        case AreaNoHref:          { area.setNoHref(value->toBoolean(exec)); return; }
        case AreaShape:           { area.setShape(str); return; }
        case AreaTabIndex:        { area.setTabIndex(value->toInt32(exec)); return; }
        case AreaTarget:          { area.setTarget(str); return; }
    }
}

void JSHTMLElement::scriptSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLScriptElement& script = *static_cast<HTMLScriptElement*>(impl());
    switch (token) {
        case ScriptText:            { script.setText(str); return; }
        case ScriptHtmlFor:         { script.setHtmlFor(str); return; }
        case ScriptEvent:           { script.setEvent(str); return; }
        case ScriptCharset:         { script.setCharset(str); return; }
        case ScriptDefer:           { script.setDefer(value->toBoolean(exec)); return; }
        case ScriptSrc:             { script.setSrc(str); return; }
        case ScriptType:            { script.setType(str); return; }
    }
}

void JSHTMLElement::tableSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
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

void JSHTMLElement::tableCaptionSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLTableCaptionElement& tableCaption = *static_cast<HTMLTableCaptionElement*>(impl());
    if (token == TableCaptionAlign)
        tableCaption.setAlign(str);
}

void JSHTMLElement::tableColSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
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

void JSHTMLElement::tableSectionSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
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

void JSHTMLElement::tableRowSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
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

void JSHTMLElement::tableCellSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
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

void JSHTMLElement::frameSetSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    HTMLFrameSetElement& frameSet = *static_cast<HTMLFrameSetElement*>(impl());
    switch (token) {
        case FrameSetCols:            { frameSet.setCols(str); return; }
        case FrameSetRows:            { frameSet.setRows(str); return; }
    }
}

void JSHTMLElement::frameSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
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

void JSHTMLElement::iFrameSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
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

void JSHTMLElement::marqueeSetter(ExecState *exec, int token, JSValue *value, const WebCore::String& str)
{
    // FIXME: Find out what WinIE supports and implement it.
}

void JSHTMLElement::putValueProperty(ExecState *exec, int token, JSValue *value, int)
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

HTMLElement *toHTMLElement(JSValue *val)
{
    if (!val || !val->isObject(&JSHTMLElement::info))
        return 0;
    return static_cast<HTMLElement*>(static_cast<JSHTMLElement*>(val)->impl());
}

HTMLTableCaptionElement *toHTMLTableCaptionElement(JSValue *val)
{
    HTMLElement *e = toHTMLElement(val);
    if (e && e->hasTagName(captionTag))
        return static_cast<HTMLTableCaptionElement*>(e);
    return 0;
}

HTMLTableSectionElement *toHTMLTableSectionElement(JSValue *val)
{
    HTMLElement *e = toHTMLElement(val);
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

const ClassInfo JSHTMLCollection::info = { "HTMLCollection", 0, 0, 0 };

JSHTMLCollection::JSHTMLCollection(ExecState *exec, HTMLCollection *c)
  : m_impl(c) 
{
  setPrototype(HTMLCollectionProto::self(exec));
}

JSHTMLCollection::~JSHTMLCollection()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue *JSHTMLCollection::lengthGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLCollection *thisObj = static_cast<JSHTMLCollection*>(slot.slotBase());
    return jsNumber(thisObj->m_impl->length());
}

JSValue *JSHTMLCollection::indexGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLCollection *thisObj = static_cast<JSHTMLCollection*>(slot.slotBase());
    return toJS(exec, thisObj->m_impl->item(slot.index()));
}

JSValue *JSHTMLCollection::nameGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLCollection *thisObj = static_cast<JSHTMLCollection*>(slot.slotBase());
    return thisObj->getNamedItems(exec, propertyName);
}

bool JSHTMLCollection::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
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
JSValue *JSHTMLCollection::callAsFunction(ExecState *exec, JSObject *, const List &args)
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

JSValue *JSHTMLCollection::getNamedItems(ExecState *exec, const Identifier &propertyName) const
{
    DeprecatedValueList< RefPtr<WebCore::Node> > namedItems = m_impl->namedItems(propertyName);

    if (namedItems.isEmpty())
        return jsUndefined();

    if (namedItems.count() == 1)
        return toJS(exec, namedItems[0].get());

    return new DOMNamedNodesCollection(exec, namedItems);
}

JSValue *HTMLCollectionProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
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

JSHTMLSelectCollection::JSHTMLSelectCollection(ExecState *exec, HTMLCollection *c, HTMLSelectElement *e)
  : JSHTMLCollection(exec, c), m_element(e)
{
}

JSValue *JSHTMLSelectCollection::selectedIndexGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLSelectCollection *thisObj = static_cast<JSHTMLSelectCollection*>(slot.slotBase());
    return jsNumber(thisObj->m_element->selectedIndex());
}

bool JSHTMLSelectCollection::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (propertyName == "selectedIndex") {
    slot.setCustom(this, selectedIndexGetter);
    //result = jsNumber(m_element->selectedIndex());
    return true;
  }

  return JSHTMLCollection::getOwnPropertySlot(exec, propertyName, slot);
}

void JSHTMLSelectCollection::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int)
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
  HTMLElement *before = 0;
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

////////////////////// Option Object ////////////////////////

OptionConstructorImp::OptionConstructorImp(ExecState *exec, Document *d)
    : m_doc(d)
{
  put(exec,lengthPropertyName, jsNumber(4), ReadOnly|DontDelete|DontEnum);
}

bool OptionConstructorImp::implementsConstruct() const
{
  return true;
}

JSObject *OptionConstructorImp::construct(ExecState *exec, const List &args)
{
  int exception = 0;
  RefPtr<Element> el(m_doc->createElement("option", exception));
  HTMLOptionElement *opt = 0;
  if (el) {
    opt = static_cast<HTMLOptionElement*>(el.get());
    int sz = args.size();
    RefPtr<Text> t = m_doc->createTextNode("");
    opt->appendChild(t, exception);
    if (exception == 0 && sz > 0)
      t->setData(args[0]->toString(exec), exception); // set the text
    if (exception == 0 && sz > 1)
      opt->setValue(args[1]->toString(exec));
    if (exception == 0 && sz > 2)
      opt->setDefaultSelected(args[2]->toBoolean(exec));
    if (exception == 0 && sz > 3)
      opt->setSelected(args[3]->toBoolean(exec));
  }

  setDOMException(exec, exception);
  return static_cast<JSObject*>(toJS(exec,opt));
}

////////////////////// Image Object ////////////////////////

ImageConstructorImp::ImageConstructorImp(ExecState *, Document *d)
    : m_doc(d)
{
}

bool ImageConstructorImp::implementsConstruct() const
{
  return true;
}

JSObject *ImageConstructorImp::construct(ExecState * exec, const List & list)
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
        
    HTMLImageElement *result = new HTMLImageElement(m_doc.get());
    
    if (widthSet)
        result->setWidth(width);
    if (heightSet)
        result->setHeight(height);
    
    return static_cast<JSObject*>(toJS(exec, result));
}

////////////////////////////////////////////////////////////////
                     
JSValue *getAllHTMLCollection(ExecState *exec, HTMLCollection *c)
{
    return cacheDOMObject<HTMLCollection, HTMLAllCollection>(exec, c);
}

JSValue *getHTMLCollection(ExecState *exec, HTMLCollection *c)
{
  return cacheDOMObject<HTMLCollection, JSHTMLCollection>(exec, c);
}

JSValue *getSelectHTMLCollection(ExecState *exec, HTMLCollection *c, HTMLSelectElement *e)
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
