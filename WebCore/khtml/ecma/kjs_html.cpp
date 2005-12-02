// -*- c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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

#include "config.h"
#include "kjs_html.h"

#include "misc/loader.h"
#include "dom/dom_exception.h"
#include "xml/dom2_eventsimpl.h"

#include "xml/dom_textimpl.h"
#include "xml/EventNames.h"
#include "html/html_baseimpl.h"
#include "html/html_blockimpl.h"
#include "html/html_canvasimpl.h"
#include "html/html_documentimpl.h"
#include "html/html_formimpl.h"
#include "html/html_headimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_inlineimpl.h"
#include "html/html_listimpl.h"
#include "html/html_objectimpl.h"
#include "html/html_tableimpl.h"

#include "khtml_part.h"
#include "khtmlview.h"

#include "kjs_css.h"
#include "kjs_window.h"
#include "kjs_events.h"
#include "kjs_proxy.h"

#include "rendering/render_canvasimage.h"
#include "rendering/render_object.h"
#include "rendering/render_layer.h"

#include <kdebug.h>

#include "css/cssparser.h"
#include "css/css_stylesheetimpl.h"
#include "css/css_ruleimpl.h"

#include <qcolor.h>
#include <qpixmap.h>
#include <qpainter.h>

#include <ApplicationServices/ApplicationServices.h>

using namespace DOM::HTMLNames;
using namespace DOM::EventNames;

using DOM::DOMString;
using DOM::DocumentImpl;
using DOM::ElementImpl;
using DOM::EventImpl;
using DOM::HTMLAnchorElementImpl;
using DOM::HTMLAppletElementImpl;
using DOM::HTMLAreaElementImpl;
using DOM::HTMLBRElementImpl;
using DOM::HTMLBaseElementImpl;
using DOM::HTMLBaseFontElementImpl;
using DOM::HTMLBlockquoteElementImpl;
using DOM::HTMLBodyElementImpl;
using DOM::HTMLButtonElementImpl;
using DOM::HTMLCanvasElementImpl;
using DOM::HTMLCollectionImpl;
using DOM::HTMLDListElementImpl;
using DOM::HTMLDirectoryElementImpl;
using DOM::HTMLDivElementImpl;
using DOM::HTMLDocumentImpl;
using DOM::HTMLElementImpl;
using DOM::HTMLFieldSetElementImpl;
using DOM::HTMLFontElementImpl;
using DOM::HTMLFormElementImpl;
using DOM::HTMLFrameElementImpl;
using DOM::HTMLFrameSetElementImpl;
using DOM::HTMLGenericFormElementImpl;
using DOM::HTMLHRElementImpl;
using DOM::HTMLHeadElementImpl;
using DOM::HTMLHeadingElementImpl;
using DOM::HTMLHtmlElementImpl;
using DOM::HTMLIFrameElementImpl;
using DOM::HTMLIFrameElementImpl;
using DOM::HTMLImageElementImpl;
using DOM::HTMLInputElementImpl;
using DOM::HTMLIsIndexElementImpl;
using DOM::HTMLLIElementImpl;
using DOM::HTMLLabelElementImpl;
using DOM::HTMLLegendElementImpl;
using DOM::HTMLLinkElementImpl;
using DOM::HTMLMapElementImpl;
using DOM::HTMLMenuElementImpl;
using DOM::HTMLMetaElementImpl;
using DOM::HTMLModElementImpl;
using DOM::HTMLOListElementImpl;
using DOM::HTMLObjectElementImpl;
using DOM::HTMLOptGroupElementImpl;
using DOM::HTMLOptionElementImpl;
using DOM::HTMLParagraphElementImpl;
using DOM::HTMLParamElementImpl;
using DOM::HTMLPreElementImpl;
using DOM::HTMLQuoteElementImpl;
using DOM::HTMLScriptElementImpl;
using DOM::HTMLSelectElementImpl;
using DOM::HTMLStyleElementImpl;
using DOM::HTMLTableCaptionElementImpl;
using DOM::HTMLTableCellElementImpl;
using DOM::HTMLTableColElementImpl;
using DOM::HTMLTableElementImpl;
using DOM::HTMLTableRowElementImpl;
using DOM::HTMLTableSectionElementImpl;
using DOM::HTMLTextAreaElementImpl;
using DOM::HTMLTitleElementImpl;
using DOM::HTMLUListElementImpl;
using DOM::NodeImpl;
using DOM::NodeListImpl;
using DOM::TextImpl;

using khtml::RenderCanvasImage;

#include "kjs_html.lut.h"

namespace KJS {

class HTMLElementFunction : public DOMFunction {
public:
  HTMLElementFunction(ExecState *exec, int i, int len);
  virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List&args);
private:
  int id;
};

IMPLEMENT_PROTOFUNC(HTMLDocFunction)

ValueImp *KJS::HTMLDocFunction::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&HTMLDocument::info))
    return throwError(exec, TypeError);
  HTMLDocumentImpl &doc = *static_cast<HTMLDocumentImpl *>(static_cast<HTMLDocument *>(thisObj)->impl());

  switch (id) {
  case HTMLDocument::Clear: // even IE doesn't support that one...
    //doc.clear(); // TODO
    return Undefined();
  case HTMLDocument::Open:
    // For compatibility with other browsers, pass open calls with more than 2 parameters to the window.
    if (args.size() > 2) {
      KHTMLPart *part = doc.part();
      if (part) {
	Window *window = Window::retrieveWindow(part);
	if (window) {
	  ObjectImp *functionObject = window->get(exec, "open")->getObject();
	  if (!functionObject || !functionObject->implementsCall())
	    return throwError(exec, TypeError);
	  return functionObject->call(exec, window, args);
	}
      }
      return Undefined();
    }
    // In the case of two parameters or fewer, do a normal document open.
    doc.open();
    return Undefined();
  case HTMLDocument::Close:
    // see khtmltests/ecma/tokenizer-script-recursion.html
    doc.close();
    return Undefined();
  case HTMLDocument::Write:
  case HTMLDocument::WriteLn: {
    // DOM only specifies single string argument, but NS & IE allow multiple
    // or no arguments
    UString str = "";
    for (int i = 0; i < args.size(); i++)
      str += args[i]->toString(exec);
    if (id == HTMLDocument::WriteLn)
      str += "\n";
    //kdDebug() << "document.write: " << str.ascii() << endl;
    doc.write(str.domString());
    return Undefined();
  }
  case HTMLDocument::GetElementsByName:
    return getDOMNodeList(exec, doc.getElementsByName(args[0]->toString(exec).domString()).get());
  case HTMLDocument::CaptureEvents:
  case HTMLDocument::ReleaseEvents:
    // Do nothing for now. These are NS-specific legacy calls.
    break;
  }

  return Undefined();
}

const ClassInfo KJS::HTMLDocument::info =
  { "HTMLDocument", &DOMDocument::info, &HTMLDocumentTable, 0 };
/* Source for HTMLDocumentTable. Use "make hashtables" to regenerate.
@begin HTMLDocumentTable 30
  title			HTMLDocument::Title		DontDelete
  referrer		HTMLDocument::Referrer		DontDelete|ReadOnly
  domain		HTMLDocument::Domain		DontDelete
  URL			HTMLDocument::URL		DontDelete|ReadOnly
  body			HTMLDocument::Body		DontDelete
  location		HTMLDocument::Location		DontDelete
  cookie		HTMLDocument::Cookie		DontDelete
  images		HTMLDocument::Images		DontDelete|ReadOnly
  embeds		HTMLDocument::Embeds		DontDelete|ReadOnly
  applets		HTMLDocument::Applets		DontDelete|ReadOnly
  links			HTMLDocument::Links		DontDelete|ReadOnly
  forms			HTMLDocument::Forms		DontDelete|ReadOnly
  anchors		HTMLDocument::Anchors		DontDelete|ReadOnly
  scripts		HTMLDocument::Scripts		DontDelete|ReadOnly
# We want no document.all at all, not just a function that returns undefined.
# That means we lose the "document.all when spoofing as IE" feature, but we don't spoof in Safari.
# And this makes sites that set document.all explicitly work when they otherwise wouldn't, 
# e.g. https://corporateexchange.airborne.com
# (Not in APPLE_CHANGES since we can't do #if in KJS identifier lists.)
#  all			HTMLDocument::All		DontDelete|ReadOnly
  clear			HTMLDocument::Clear		DontDelete|Function 0
  open			HTMLDocument::Open		DontDelete|Function 0
  close			HTMLDocument::Close		DontDelete|Function 0
  write			HTMLDocument::Write		DontDelete|Function 1
  writeln		HTMLDocument::WriteLn		DontDelete|Function 1
  getElementsByName	HTMLDocument::GetElementsByName	DontDelete|Function 1
  captureEvents		HTMLDocument::CaptureEvents	DontDelete|Function 0
  releaseEvents		HTMLDocument::ReleaseEvents	DontDelete|Function 0
  bgColor		HTMLDocument::BgColor		DontDelete
  fgColor		HTMLDocument::FgColor		DontDelete
  alinkColor		HTMLDocument::AlinkColor	DontDelete
  linkColor		HTMLDocument::LinkColor		DontDelete
  vlinkColor		HTMLDocument::VlinkColor	DontDelete
  lastModified		HTMLDocument::LastModified	DontDelete|ReadOnly
  height		HTMLDocument::Height		DontDelete|ReadOnly
  width			HTMLDocument::Width		DontDelete|ReadOnly
  dir			HTMLDocument::Dir		DontDelete
  designMode            HTMLDocument::DesignMode        DontDelete
#potentially obsolete array properties
# layers
# plugins
# tags
#potentially obsolete properties
# embeds
# ids
@end
*/

HTMLDocument::HTMLDocument(ExecState *exec, HTMLDocumentImpl *d)
  : DOMDocument(exec, d)
{
}

ValueImp *HTMLDocument::namedItemGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
  HTMLDocument *thisObj = static_cast<HTMLDocument *>(slot.slotBase());
  HTMLDocumentImpl &doc = *static_cast<HTMLDocumentImpl *>(thisObj->impl());

  DOMString name = propertyName.domString();
  RefPtr<DOM::HTMLCollectionImpl> collection = doc.documentNamedItems(name);

  if (collection->length() == 1) {
    NodeImpl *node = collection->firstItem();
    KHTMLPart *part;
    if (node->hasTagName(iframeTag) && 
        (part = static_cast<DOM::HTMLIFrameElementImpl *>(node)->contentPart()))
      return Window::retrieve(part);

    return getDOMNode(exec, node);
  }

  return getHTMLCollection(exec, collection.get());
}

ValueImp *HTMLDocument::getValueProperty(ExecState *exec, int token) const
{
  HTMLDocumentImpl &doc = *static_cast<HTMLDocumentImpl *>(impl());

  KHTMLView *view = doc.view();
  KHTMLPart *part = doc.part();

  HTMLElementImpl *body = doc.body();
  HTMLBodyElementImpl *bodyElement = (body && body->hasTagName(bodyTag)) ? static_cast<HTMLBodyElementImpl *>(body) : 0;
    
  switch (token) {
  case Title:
    return String(doc.title());
  case Referrer:
    return String(doc.referrer());
  case Domain:
    return String(doc.domain());
  case URL:
    return String(doc.URL());
  case Body:
    return getDOMNode(exec, body);
  case Location:
    if (Window* win = Window::retrieveWindow(part))
      return win->location();
    return Undefined();
  case Cookie:
    return String(doc.cookie());
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
      kdWarning() << "KJS::HTMLDocument document.scripts called - not implemented" << endl;
      ObjectImp *obj = new ObjectImp;
      obj->put(exec, lengthPropertyName, Number(0));
      return obj;
    }
  case All:
    // Disable document.all when we try to be Netscape-compatible
    if (exec->dynamicInterpreter()->compatMode() == Interpreter::NetscapeCompat)
      return Undefined();
    return getHTMLCollection(exec, doc.all().get());
  case BgColor:
    if (!bodyElement)
      return Undefined();
    return String(bodyElement->bgColor());
  case FgColor:
    if (!bodyElement)
      return Undefined();
    return String(bodyElement->text());
  case AlinkColor:
    if (!bodyElement)
      return Undefined();
    return String(bodyElement->aLink());
  case LinkColor:
    if (!bodyElement)
      return Undefined();
    return String(bodyElement->link());
  case VlinkColor:
    if (!bodyElement)
      return Undefined();
    return String(bodyElement->vLink());
  case LastModified:
    return String(doc.lastModified());
  case Height:
    return Number(view ? view->contentsHeight() : 0);
  case Width:
    return Number(view ? view->contentsWidth() : 0);
  case Dir:
    if (!bodyElement)
      return Undefined();
    return String(bodyElement->dir());
  case DesignMode:
    return String(doc.inDesignMode() ? "on" : "off");
  default:
    assert(0);
    return Undefined();
  }
}

bool HTMLDocument::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  HTMLDocumentImpl &doc = *static_cast<HTMLDocumentImpl *>(impl());

  DOMString name = propertyName.domString();
  if (doc.hasNamedItem(name) || doc.hasDocExtraNamedItem(name)) {
    slot.setCustom(this, namedItemGetter);
    return true;
  }

  const HashEntry* entry = Lookup::findEntry(&HTMLDocumentTable, propertyName);
  if (entry) {
    if (entry->attr & Function)
      slot.setStaticEntry(this, entry, staticFunctionGetter<HTMLDocFunction>);
    else 
      slot.setStaticEntry(this, entry, staticValueGetter<HTMLDocument>);
    return true;
  }

  return DOMDocument::getOwnPropertySlot(exec, propertyName, slot);
}

void KJS::HTMLDocument::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "KJS::HTMLDocument::put " << propertyName.qstring() << endl;
#endif
  lookupPut<HTMLDocument, DOMDocument>( exec, propertyName, value, attr, &HTMLDocumentTable, this );
}

void KJS::HTMLDocument::putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/)
{
  DOMExceptionTranslator exception(exec);
  HTMLDocumentImpl &doc = *static_cast<HTMLDocumentImpl *>(impl());
  HTMLElementImpl *body = doc.body();
  HTMLBodyElementImpl *bodyElement = (body && body->hasTagName(bodyTag)) ? static_cast<HTMLBodyElementImpl *>(body) : 0;

  switch (token) {
  case Title:
    doc.setTitle(value->toString(exec).domString());
    break;
  case Body:
    doc.setBody(toHTMLElement(value), exception);
    break;
  case Domain: // not part of the DOM
    doc.setDomain(value->toString(exec).domString());
    break;
  case Cookie:
    doc.setCookie(value->toString(exec).domString());
    break;
  case Location: {
    KHTMLPart *part = doc.part();
    if (part)
    {
      QString str = value->toString(exec).qstring();

      // When assigning location, IE and Mozilla both resolve the URL
      // relative to the frame where the JavaScript is executing not
      // the target frame.
      KHTMLPart *activePart = static_cast<KJS::ScriptInterpreter *>( exec->dynamicInterpreter() )->part();
      if (activePart)
        str = activePart->xmlDocImpl()->completeURL(str);

      // We want a new history item if this JS was called via a user gesture
      bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
      part->scheduleLocationChange(str, activePart->referrer(), !userGesture);
    }
    break;
  }
  case BgColor:
    if (bodyElement)
      bodyElement->setBgColor(value->toString(exec).domString());
    break;
  case FgColor:
    if (bodyElement)
      bodyElement->setText(value->toString(exec).domString());
    break;
  case AlinkColor:
    if (bodyElement) {
      // this check is a bit silly, but some benchmarks like to set the
      // document's link colors over and over to the same value and we
      // don't want to incur a style update each time.
      DOMString newColor = value->toString(exec).domString();
      if (bodyElement->aLink() != newColor)
        bodyElement->setALink(newColor);
    }
    break;
  case LinkColor:
    if (bodyElement) {
      // this check is a bit silly, but some benchmarks like to set the
      // document's link colors over and over to the same value and we
      // don't want to incur a style update each time.
      DOMString newColor = value->toString(exec).domString();
      if (bodyElement->link() != newColor)
	bodyElement->setLink(newColor);
    }
    break;
  case VlinkColor:
    if (bodyElement) {
      // this check is a bit silly, but some benchmarks like to set the
      // document's link colors over and over to the same value and we
      // don't want to incur a style update each time.
      DOMString newColor = value->toString(exec).domString();
      if (bodyElement->vLink() != newColor)
	bodyElement->setVLink(newColor);
    }
    break;
  case Dir:
    body->setDir(value->toString(exec).domString());
    break;
  case DesignMode:
    {
      DOMString modeString = value->toString(exec).domString();
      DocumentImpl::InheritedBool mode;
      if (!strcasecmp(modeString, "on"))
        mode = DocumentImpl::on;
      else if (!strcasecmp(modeString, "off"))
        mode = DocumentImpl::off;
      else
        mode = DocumentImpl::inherit;
      doc.setDesignMode(mode);
      break;
    }
  default:
    kdWarning() << "HTMLDocument::putValueProperty unhandled token " << token << endl;
  }
}

// -------------------------------------------------------------------------

const ClassInfo KJS::HTMLElement::info = { "HTMLElement", &DOMElement::info, &HTMLElementTable, 0 };
const ClassInfo KJS::HTMLElement::html_info = { "HTMLHtmlElement", &KJS::HTMLElement::info, &HTMLHtmlElementTable, 0 };
const ClassInfo KJS::HTMLElement::head_info = { "HTMLHeadElement", &KJS::HTMLElement::info, &HTMLHeadElementTable, 0 };
const ClassInfo KJS::HTMLElement::link_info = { "HTMLLinkElement", &KJS::HTMLElement::info, &HTMLLinkElementTable, 0 };
const ClassInfo KJS::HTMLElement::title_info = { "HTMLTitleElement", &KJS::HTMLElement::info, &HTMLTitleElementTable, 0 };
const ClassInfo KJS::HTMLElement::meta_info = { "HTMLMetaElement", &KJS::HTMLElement::info, &HTMLMetaElementTable, 0 };
const ClassInfo KJS::HTMLElement::base_info = { "HTMLBaseElement", &KJS::HTMLElement::info, &HTMLBaseElementTable, 0 };
const ClassInfo KJS::HTMLElement::isIndex_info = { "HTMLIsIndexElement", &KJS::HTMLElement::info, &HTMLIsIndexElementTable, 0 };
const ClassInfo KJS::HTMLElement::style_info = { "HTMLStyleElement", &KJS::HTMLElement::info, &HTMLStyleElementTable, 0 };
const ClassInfo KJS::HTMLElement::body_info = { "HTMLBodyElement", &KJS::HTMLElement::info, &HTMLBodyElementTable, 0 };
const ClassInfo KJS::HTMLElement::form_info = { "HTMLFormElement", &KJS::HTMLElement::info, &HTMLFormElementTable, 0 };
const ClassInfo KJS::HTMLElement::select_info = { "HTMLSelectElement", &KJS::HTMLElement::info, &HTMLSelectElementTable, 0 };
const ClassInfo KJS::HTMLElement::optGroup_info = { "HTMLOptGroupElement", &KJS::HTMLElement::info, &HTMLOptGroupElementTable, 0 };
const ClassInfo KJS::HTMLElement::option_info = { "HTMLOptionElement", &KJS::HTMLElement::info, &HTMLOptionElementTable, 0 };
const ClassInfo KJS::HTMLElement::input_info = { "HTMLInputElement", &KJS::HTMLElement::info, &HTMLInputElementTable, 0 };
const ClassInfo KJS::HTMLElement::textArea_info = { "HTMLTextAreaElement", &KJS::HTMLElement::info, &HTMLTextAreaElementTable, 0 };
const ClassInfo KJS::HTMLElement::button_info = { "HTMLButtonElement", &KJS::HTMLElement::info, &HTMLButtonElementTable, 0 };
const ClassInfo KJS::HTMLElement::label_info = { "HTMLLabelElement", &KJS::HTMLElement::info, &HTMLLabelElementTable, 0 };
const ClassInfo KJS::HTMLElement::fieldSet_info = { "HTMLFieldSetElement", &KJS::HTMLElement::info, &HTMLFieldSetElementTable, 0 };
const ClassInfo KJS::HTMLElement::legend_info = { "HTMLLegendElement", &KJS::HTMLElement::info, &HTMLLegendElementTable, 0 };
const ClassInfo KJS::HTMLElement::ul_info = { "HTMLUListElement", &KJS::HTMLElement::info, &HTMLUListElementTable, 0 };
const ClassInfo KJS::HTMLElement::ol_info = { "HTMLOListElement", &KJS::HTMLElement::info, &HTMLOListElementTable, 0 };
const ClassInfo KJS::HTMLElement::dl_info = { "HTMLDListElement", &KJS::HTMLElement::info, &HTMLDListElementTable, 0 };
const ClassInfo KJS::HTMLElement::dir_info = { "HTMLDirectoryElement", &KJS::HTMLElement::info, &HTMLDirectoryElementTable, 0 };
const ClassInfo KJS::HTMLElement::menu_info = { "HTMLMenuElement", &KJS::HTMLElement::info, &HTMLMenuElementTable, 0 };
const ClassInfo KJS::HTMLElement::li_info = { "HTMLLIElement", &KJS::HTMLElement::info, &HTMLLIElementTable, 0 };
const ClassInfo KJS::HTMLElement::div_info = { "HTMLDivElement", &KJS::HTMLElement::info, &HTMLDivElementTable, 0 };
const ClassInfo KJS::HTMLElement::p_info = { "HTMLParagraphElement", &KJS::HTMLElement::info, &HTMLParagraphElementTable, 0 };
const ClassInfo KJS::HTMLElement::heading_info = { "HTMLHeadingElement", &KJS::HTMLElement::info, &HTMLHeadingElementTable, 0 };
const ClassInfo KJS::HTMLElement::blockQuote_info = { "HTMLBlockQuoteElement", &KJS::HTMLElement::info, &HTMLBlockQuoteElementTable, 0 };
const ClassInfo KJS::HTMLElement::q_info = { "HTMLQuoteElement", &KJS::HTMLElement::info, &HTMLQuoteElementTable, 0 };
const ClassInfo KJS::HTMLElement::pre_info = { "HTMLPreElement", &KJS::HTMLElement::info, &HTMLPreElementTable, 0 };
const ClassInfo KJS::HTMLElement::br_info = { "HTMLBRElement", &KJS::HTMLElement::info, &HTMLBRElementTable, 0 };
const ClassInfo KJS::HTMLElement::baseFont_info = { "HTMLBaseFontElement", &KJS::HTMLElement::info, &HTMLBaseFontElementTable, 0 };
const ClassInfo KJS::HTMLElement::font_info = { "HTMLFontElement", &KJS::HTMLElement::info, &HTMLFontElementTable, 0 };
const ClassInfo KJS::HTMLElement::hr_info = { "HTMLHRElement", &KJS::HTMLElement::info, &HTMLHRElementTable, 0 };
const ClassInfo KJS::HTMLElement::mod_info = { "HTMLModElement", &KJS::HTMLElement::info, &HTMLModElementTable, 0 };
const ClassInfo KJS::HTMLElement::a_info = { "HTMLAnchorElement", &KJS::HTMLElement::info, &HTMLAnchorElementTable, 0 };
const ClassInfo KJS::HTMLElement::canvas_info = { "HTMLCanvasElement", &KJS::HTMLElement::info, &HTMLCanvasElementTable, 0 };
const ClassInfo KJS::HTMLElement::img_info = { "HTMLImageElement", &KJS::HTMLElement::info, &HTMLImageElementTable, 0 };
const ClassInfo KJS::HTMLElement::object_info = { "HTMLObjectElement", &KJS::HTMLElement::info, &HTMLObjectElementTable, 0 };
const ClassInfo KJS::HTMLElement::param_info = { "HTMLParamElement", &KJS::HTMLElement::info, &HTMLParamElementTable, 0 };
const ClassInfo KJS::HTMLElement::applet_info = { "HTMLAppletElement", &KJS::HTMLElement::info, &HTMLAppletElementTable, 0 };
const ClassInfo KJS::HTMLElement::map_info = { "HTMLMapElement", &KJS::HTMLElement::info, &HTMLMapElementTable, 0 };
const ClassInfo KJS::HTMLElement::area_info = { "HTMLAreaElement", &KJS::HTMLElement::info, &HTMLAreaElementTable, 0 };
const ClassInfo KJS::HTMLElement::script_info = { "HTMLScriptElement", &KJS::HTMLElement::info, &HTMLScriptElementTable, 0 };
const ClassInfo KJS::HTMLElement::table_info = { "HTMLTableElement", &KJS::HTMLElement::info, &HTMLTableElementTable, 0 };
const ClassInfo KJS::HTMLElement::caption_info = { "HTMLTableCaptionElement", &KJS::HTMLElement::info, &HTMLTableCaptionElementTable, 0 };
const ClassInfo KJS::HTMLElement::col_info = { "HTMLTableColElement", &KJS::HTMLElement::info, &HTMLTableColElementTable, 0 };
const ClassInfo KJS::HTMLElement::tablesection_info = { "HTMLTableSectionElement", &KJS::HTMLElement::info, &HTMLTableSectionElementTable, 0 };
const ClassInfo KJS::HTMLElement::tr_info = { "HTMLTableRowElement", &KJS::HTMLElement::info, &HTMLTableRowElementTable, 0 };
const ClassInfo KJS::HTMLElement::tablecell_info = { "HTMLTableCellElement", &KJS::HTMLElement::info, &HTMLTableCellElementTable, 0 };
const ClassInfo KJS::HTMLElement::frameSet_info = { "HTMLFrameSetElement", &KJS::HTMLElement::info, &HTMLFrameSetElementTable, 0 };
const ClassInfo KJS::HTMLElement::frame_info = { "HTMLFrameElement", &KJS::HTMLElement::info, &HTMLFrameElementTable, 0 };
const ClassInfo KJS::HTMLElement::iFrame_info = { "HTMLIFrameElement", &KJS::HTMLElement::info, &HTMLIFrameElementTable, 0 };
const ClassInfo KJS::HTMLElement::marquee_info = { "HTMLMarqueeElement", &KJS::HTMLElement::info, &HTMLMarqueeElementTable, 0 };

const ClassInfo* KJS::HTMLElement::classInfo() const
{
    // FIXME: We want to use HashMap here, but there are compiler issues to deal with.
    static QPtrDict<const ClassInfo> classInfoDict;
    if (classInfoDict.isEmpty()) {
        classInfoDict.insert(aTag.localName().impl(), &a_info);
        classInfoDict.insert(appletTag.localName().impl(), &applet_info);
        classInfoDict.insert(areaTag.localName().impl(), &area_info);
        classInfoDict.insert(baseTag.localName().impl(), &base_info);
        classInfoDict.insert(basefontTag.localName().impl(), &baseFont_info);
        classInfoDict.insert(blockquoteTag.localName().impl(), &blockQuote_info);
        classInfoDict.insert(bodyTag.localName().impl(), &body_info);
        classInfoDict.insert(brTag.localName().impl(), &br_info);
        classInfoDict.insert(buttonTag.localName().impl(), &button_info);
        classInfoDict.insert(canvasTag.localName().impl(), &canvas_info);
        classInfoDict.insert(captionTag.localName().impl(), &caption_info);
        classInfoDict.insert(colTag.localName().impl(), &col_info);
        classInfoDict.insert(colgroupTag.localName().impl(), &col_info);
        classInfoDict.insert(delTag.localName().impl(), &mod_info);
        classInfoDict.insert(dirTag.localName().impl(), &dir_info);
        classInfoDict.insert(divTag.localName().impl(), &div_info);
        classInfoDict.insert(dlTag.localName().impl(), &dl_info);
        classInfoDict.insert(fieldsetTag.localName().impl(), &fieldSet_info);
        classInfoDict.insert(fontTag.localName().impl(), &font_info);
        classInfoDict.insert(formTag.localName().impl(), &form_info);
        classInfoDict.insert(frameTag.localName().impl(), &frame_info);
        classInfoDict.insert(framesetTag.localName().impl(), &frameSet_info);
        classInfoDict.insert(h1Tag.localName().impl(), &heading_info);
        classInfoDict.insert(h2Tag.localName().impl(), &heading_info);
        classInfoDict.insert(h3Tag.localName().impl(), &heading_info);
        classInfoDict.insert(h4Tag.localName().impl(), &heading_info);
        classInfoDict.insert(h5Tag.localName().impl(), &heading_info);
        classInfoDict.insert(h6Tag.localName().impl(), &heading_info);
        classInfoDict.insert(headTag.localName().impl(), &head_info);
        classInfoDict.insert(hrTag.localName().impl(), &hr_info);
        classInfoDict.insert(htmlTag.localName().impl(), &html_info);
        classInfoDict.insert(iframeTag.localName().impl(), &iFrame_info);
        classInfoDict.insert(imgTag.localName().impl(), &img_info);
        classInfoDict.insert(inputTag.localName().impl(), &input_info);
        classInfoDict.insert(insTag.localName().impl(), &mod_info);
        classInfoDict.insert(isindexTag.localName().impl(), &isIndex_info);
        classInfoDict.insert(labelTag.localName().impl(), &label_info);
        classInfoDict.insert(legendTag.localName().impl(), &legend_info);
        classInfoDict.insert(liTag.localName().impl(), &li_info);
        classInfoDict.insert(linkTag.localName().impl(), &link_info);
        classInfoDict.insert(mapTag.localName().impl(), &map_info);
        classInfoDict.insert(marqueeTag.localName().impl(), &marquee_info);
        classInfoDict.insert(menuTag.localName().impl(), &menu_info);
        classInfoDict.insert(metaTag.localName().impl(), &meta_info);
        classInfoDict.insert(objectTag.localName().impl(), &object_info);
        classInfoDict.insert(olTag.localName().impl(), &ol_info);
        classInfoDict.insert(optionTag.localName().impl(), &option_info);
        classInfoDict.insert(optgroupTag.localName().impl(), &optGroup_info);
        classInfoDict.insert(pTag.localName().impl(), &p_info);
        classInfoDict.insert(paramTag.localName().impl(), &param_info);
        classInfoDict.insert(preTag.localName().impl(), &pre_info);
        classInfoDict.insert(qTag.localName().impl(), &q_info);
        classInfoDict.insert(scriptTag.localName().impl(), &script_info);
        classInfoDict.insert(selectTag.localName().impl(), &select_info);
        classInfoDict.insert(styleTag.localName().impl(), &style_info);
        classInfoDict.insert(tableTag.localName().impl(), &table_info);
        classInfoDict.insert(tbodyTag.localName().impl(), &tablesection_info);
        classInfoDict.insert(tdTag.localName().impl(), &tablecell_info);
        classInfoDict.insert(textareaTag.localName().impl(), &textArea_info);
        classInfoDict.insert(thTag.localName().impl(), &tablecell_info);
        classInfoDict.insert(theadTag.localName().impl(), &tablesection_info);
        classInfoDict.insert(tfootTag.localName().impl(), &tablesection_info);
        classInfoDict.insert(titleTag.localName().impl(), &title_info);
        classInfoDict.insert(trTag.localName().impl(), &tr_info);
        classInfoDict.insert(ulTag.localName().impl(), &ul_info);
    }
    
    HTMLElementImpl* element = static_cast<HTMLElementImpl*>(impl());
    const ClassInfo* result = classInfoDict.find(element->localName().impl());
    if (result)
        return result;
    return &info;
}

const HTMLElement::Accessors HTMLElement::html_accessors = { &HTMLElement::htmlGetter, &HTMLElement::htmlSetter };
const HTMLElement::Accessors HTMLElement::head_accessors = { &HTMLElement::headGetter, &HTMLElement::headSetter };
const HTMLElement::Accessors HTMLElement::link_accessors = { &HTMLElement::linkGetter, &HTMLElement::linkSetter };
const HTMLElement::Accessors HTMLElement::title_accessors = { &HTMLElement::titleGetter, &HTMLElement::titleSetter };
const HTMLElement::Accessors HTMLElement::meta_accessors = { &HTMLElement::metaGetter, &HTMLElement::metaSetter };
const HTMLElement::Accessors HTMLElement::base_accessors = { &HTMLElement::baseGetter, &HTMLElement::baseSetter };
const HTMLElement::Accessors HTMLElement::isIndex_accessors = { &HTMLElement::isIndexGetter, &HTMLElement::isIndexSetter };
const HTMLElement::Accessors HTMLElement::style_accessors = { &HTMLElement::styleGetter, &HTMLElement::styleSetter };
const HTMLElement::Accessors HTMLElement::body_accessors = { &HTMLElement::bodyGetter, &HTMLElement::bodySetter };
const HTMLElement::Accessors HTMLElement::form_accessors = { &HTMLElement::formGetter, &HTMLElement::formSetter };
const HTMLElement::Accessors HTMLElement::select_accessors = { &HTMLElement::selectGetter, &HTMLElement::selectSetter };
const HTMLElement::Accessors HTMLElement::optGroup_accessors = { &HTMLElement::optGroupGetter, &HTMLElement::optGroupSetter };
const HTMLElement::Accessors HTMLElement::option_accessors = { &HTMLElement::optionGetter, &HTMLElement::optionSetter };
const HTMLElement::Accessors HTMLElement::input_accessors = { &HTMLElement::inputGetter, &HTMLElement::inputSetter };
const HTMLElement::Accessors HTMLElement::textArea_accessors = { &HTMLElement::textAreaGetter, &HTMLElement::textAreaSetter };
const HTMLElement::Accessors HTMLElement::button_accessors = { &HTMLElement::buttonGetter, &HTMLElement::buttonSetter };
const HTMLElement::Accessors HTMLElement::label_accessors = { &HTMLElement::labelGetter, &HTMLElement::labelSetter };
const HTMLElement::Accessors HTMLElement::fieldSet_accessors = { &HTMLElement::fieldSetGetter, &HTMLElement::fieldSetSetter };
const HTMLElement::Accessors HTMLElement::legend_accessors = { &HTMLElement::legendGetter, &HTMLElement::legendSetter };
const HTMLElement::Accessors HTMLElement::ul_accessors = { &HTMLElement::uListGetter, &HTMLElement::uListSetter };
const HTMLElement::Accessors HTMLElement::ol_accessors = { &HTMLElement::oListGetter, &HTMLElement::oListSetter };
const HTMLElement::Accessors HTMLElement::dl_accessors = { &HTMLElement::dListGetter, &HTMLElement::dListSetter };
const HTMLElement::Accessors HTMLElement::dir_accessors = { &HTMLElement::dirGetter, &HTMLElement::dirSetter };
const HTMLElement::Accessors HTMLElement::menu_accessors = { &HTMLElement::menuGetter, &HTMLElement::menuSetter };
const HTMLElement::Accessors HTMLElement::li_accessors = { &HTMLElement::liGetter, &HTMLElement::liSetter };
const HTMLElement::Accessors HTMLElement::div_accessors = { &HTMLElement::divGetter, &HTMLElement::divSetter };
const HTMLElement::Accessors HTMLElement::p_accessors = { &HTMLElement::paragraphGetter, &HTMLElement::paragraphSetter };
const HTMLElement::Accessors HTMLElement::heading_accessors = { &HTMLElement::headingGetter, &HTMLElement::headingSetter };
const HTMLElement::Accessors HTMLElement::blockQuote_accessors = { &HTMLElement::blockQuoteGetter, &HTMLElement::blockQuoteSetter };
const HTMLElement::Accessors HTMLElement::q_accessors = { &HTMLElement::quoteGetter, &HTMLElement::quoteSetter };
const HTMLElement::Accessors HTMLElement::pre_accessors = { &HTMLElement::preGetter, &HTMLElement::preSetter };
const HTMLElement::Accessors HTMLElement::br_accessors = { &HTMLElement::brGetter, &HTMLElement::brSetter };
const HTMLElement::Accessors HTMLElement::baseFont_accessors = { &HTMLElement::baseFontGetter, &HTMLElement::baseFontSetter };
const HTMLElement::Accessors HTMLElement::font_accessors = { &HTMLElement::fontGetter, &HTMLElement::fontSetter };
const HTMLElement::Accessors HTMLElement::hr_accessors = { &HTMLElement::hrGetter, &HTMLElement::hrSetter };
const HTMLElement::Accessors HTMLElement::mod_accessors = { &HTMLElement::modGetter, &HTMLElement::modSetter };
const HTMLElement::Accessors HTMLElement::a_accessors = { &HTMLElement::anchorGetter, &HTMLElement::anchorSetter };
const HTMLElement::Accessors HTMLElement::canvas_accessors = { &HTMLElement::imageGetter, &HTMLElement::imageSetter };
const HTMLElement::Accessors HTMLElement::img_accessors = { &HTMLElement::imageGetter, &HTMLElement::imageSetter };
const HTMLElement::Accessors HTMLElement::object_accessors = { &HTMLElement::objectGetter, &HTMLElement::objectSetter };
const HTMLElement::Accessors HTMLElement::param_accessors = { &HTMLElement::paramGetter, &HTMLElement::paramSetter };
const HTMLElement::Accessors HTMLElement::applet_accessors = { &HTMLElement::appletGetter, &HTMLElement::appletSetter };
const HTMLElement::Accessors HTMLElement::map_accessors = { &HTMLElement::mapGetter, &HTMLElement::mapSetter };
const HTMLElement::Accessors HTMLElement::area_accessors = { &HTMLElement::areaGetter, &HTMLElement::areaSetter };
const HTMLElement::Accessors HTMLElement::script_accessors = { &HTMLElement::scriptGetter, &HTMLElement::scriptSetter };
const HTMLElement::Accessors HTMLElement::table_accessors = { &HTMLElement::tableGetter, &HTMLElement::tableSetter };
const HTMLElement::Accessors HTMLElement::caption_accessors = { &HTMLElement::tableCaptionGetter, &HTMLElement::tableCaptionSetter };
const HTMLElement::Accessors HTMLElement::col_accessors = { &HTMLElement::tableColGetter, &HTMLElement::tableColSetter };
const HTMLElement::Accessors HTMLElement::tablesection_accessors = { &HTMLElement::tableSectionGetter, &HTMLElement::tableSectionSetter };
const HTMLElement::Accessors HTMLElement::tr_accessors = { &HTMLElement::tableRowGetter, &HTMLElement::tableRowSetter };
const HTMLElement::Accessors HTMLElement::tablecell_accessors = { &HTMLElement::tableCellGetter, &HTMLElement::tableCellSetter };
const HTMLElement::Accessors HTMLElement::frameSet_accessors = { &HTMLElement::frameSetGetter, &HTMLElement::frameSetSetter };
const HTMLElement::Accessors HTMLElement::frame_accessors = { &HTMLElement::frameGetter, &HTMLElement::frameSetter };
const HTMLElement::Accessors HTMLElement::iFrame_accessors = { &HTMLElement::iFrameGetter, &HTMLElement::iFrameSetter };
const HTMLElement::Accessors HTMLElement::marquee_accessors = { &HTMLElement::marqueeGetter, &HTMLElement::marqueeSetter };

const HTMLElement::Accessors* HTMLElement::getSetInfo() const
{
    // FIXME: We want to use HashMap here, but there are compiler issues to deal with.
    static QPtrDict<const HTMLElement::Accessors> getSetInfoDict;
    if (getSetInfoDict.isEmpty()) {
        getSetInfoDict.insert(aTag.localName().impl(), &a_accessors);
        getSetInfoDict.insert(appletTag.localName().impl(), &applet_accessors);
        getSetInfoDict.insert(areaTag.localName().impl(), &area_accessors);
        getSetInfoDict.insert(baseTag.localName().impl(), &base_accessors);
        getSetInfoDict.insert(basefontTag.localName().impl(), &baseFont_accessors);
        getSetInfoDict.insert(blockquoteTag.localName().impl(), &blockQuote_accessors); 
        getSetInfoDict.insert(bodyTag.localName().impl(), &body_accessors);
        getSetInfoDict.insert(brTag.localName().impl(), &br_accessors);
        getSetInfoDict.insert(buttonTag.localName().impl(), &button_accessors);
        getSetInfoDict.insert(canvasTag.localName().impl(), &canvas_accessors);
        getSetInfoDict.insert(captionTag.localName().impl(), &caption_accessors);
        getSetInfoDict.insert(colTag.localName().impl(), &col_accessors);
        getSetInfoDict.insert(colgroupTag.localName().impl(), &col_accessors);
        getSetInfoDict.insert(delTag.localName().impl(), &mod_accessors);
        getSetInfoDict.insert(dirTag.localName().impl(), &dir_accessors);
        getSetInfoDict.insert(divTag.localName().impl(), &div_accessors);
        getSetInfoDict.insert(dlTag.localName().impl(), &dl_accessors);
        getSetInfoDict.insert(fieldsetTag.localName().impl(), &fieldSet_accessors);
        getSetInfoDict.insert(fontTag.localName().impl(), &font_accessors);
        getSetInfoDict.insert(formTag.localName().impl(), &form_accessors);
        getSetInfoDict.insert(frameTag.localName().impl(), &frame_accessors);
        getSetInfoDict.insert(framesetTag.localName().impl(), &frameSet_accessors);
        getSetInfoDict.insert(h1Tag.localName().impl(), &heading_accessors);
        getSetInfoDict.insert(h2Tag.localName().impl(), &heading_accessors);
        getSetInfoDict.insert(h3Tag.localName().impl(), &heading_accessors);
        getSetInfoDict.insert(h4Tag.localName().impl(), &heading_accessors);
        getSetInfoDict.insert(h5Tag.localName().impl(), &heading_accessors);
        getSetInfoDict.insert(h6Tag.localName().impl(), &heading_accessors);
        getSetInfoDict.insert(headTag.localName().impl(), &head_accessors);
        getSetInfoDict.insert(hrTag.localName().impl(), &hr_accessors); 
        getSetInfoDict.insert(htmlTag.localName().impl(), &html_accessors);
        getSetInfoDict.insert(iframeTag.localName().impl(), &iFrame_accessors);
        getSetInfoDict.insert(imgTag.localName().impl(), &img_accessors);
        getSetInfoDict.insert(inputTag.localName().impl(), &input_accessors);
        getSetInfoDict.insert(insTag.localName().impl(), &mod_accessors);
        getSetInfoDict.insert(isindexTag.localName().impl(), &isIndex_accessors);
        getSetInfoDict.insert(labelTag.localName().impl(), &label_accessors);
        getSetInfoDict.insert(legendTag.localName().impl(), &legend_accessors);
        getSetInfoDict.insert(liTag.localName().impl(), &li_accessors);
        getSetInfoDict.insert(linkTag.localName().impl(), &link_accessors);
        getSetInfoDict.insert(mapTag.localName().impl(), &map_accessors);
        getSetInfoDict.insert(marqueeTag.localName().impl(), &marquee_accessors);
        getSetInfoDict.insert(menuTag.localName().impl(), &menu_accessors);
        getSetInfoDict.insert(metaTag.localName().impl(), &meta_accessors);
        getSetInfoDict.insert(objectTag.localName().impl(), &object_accessors);
        getSetInfoDict.insert(olTag.localName().impl(), &ol_accessors);
        getSetInfoDict.insert(optionTag.localName().impl(), &option_accessors);
        getSetInfoDict.insert(optgroupTag.localName().impl(), &optGroup_accessors);
        getSetInfoDict.insert(pTag.localName().impl(), &p_accessors);
        getSetInfoDict.insert(paramTag.localName().impl(), &param_accessors);
        getSetInfoDict.insert(preTag.localName().impl(), &pre_accessors);
        getSetInfoDict.insert(qTag.localName().impl(), &q_accessors);
        getSetInfoDict.insert(scriptTag.localName().impl(), &script_accessors);
        getSetInfoDict.insert(selectTag.localName().impl(), &select_accessors);
        getSetInfoDict.insert(styleTag.localName().impl(), &style_accessors);
        getSetInfoDict.insert(tableTag.localName().impl(), &table_accessors);
        getSetInfoDict.insert(tbodyTag.localName().impl(), &tablesection_accessors);
        getSetInfoDict.insert(tdTag.localName().impl(), &tablecell_accessors);
        getSetInfoDict.insert(textareaTag.localName().impl(), &textArea_accessors);
        getSetInfoDict.insert(thTag.localName().impl(), &tablecell_accessors);
        getSetInfoDict.insert(theadTag.localName().impl(), &tablesection_accessors);
        getSetInfoDict.insert(tfootTag.localName().impl(), &tablesection_accessors);
        getSetInfoDict.insert(titleTag.localName().impl(), &title_accessors);
        getSetInfoDict.insert(trTag.localName().impl(), &tr_accessors);
        getSetInfoDict.insert(ulTag.localName().impl(), &ul_accessors);
    }
    
    HTMLElementImpl* element = static_cast<HTMLElementImpl*>(impl());
    const HTMLElement::Accessors* result = getSetInfoDict.find(element->localName().impl());
    if (result)
        return result;
    return 0;
}

/*
@begin HTMLElementTable 14
  id		KJS::HTMLElement::ElementId	DontDelete
  title		KJS::HTMLElement::ElementTitle	DontDelete
  lang		KJS::HTMLElement::ElementLang	DontDelete
  dir		KJS::HTMLElement::ElementDir	DontDelete
### isn't this "class" in the HTML spec?
  className	KJS::HTMLElement::ElementClassName DontDelete
  innerHTML	KJS::HTMLElement::ElementInnerHTML DontDelete
  innerText	KJS::HTMLElement::ElementInnerText DontDelete
  outerHTML	KJS::HTMLElement::ElementOuterHTML DontDelete
  outerText	KJS::HTMLElement::ElementOuterText DontDelete
  document	KJS::HTMLElement::ElementDocument  DontDelete|ReadOnly
# IE extension
  children	KJS::HTMLElement::ElementChildren  DontDelete|ReadOnly
  contentEditable   KJS::HTMLElement::ElementContentEditable  DontDelete
  isContentEditable KJS::HTMLElement::ElementIsContentEditable  DontDelete|ReadOnly
@end
@begin HTMLHtmlElementTable 1
  version	KJS::HTMLElement::HtmlVersion	DontDelete
@end
@begin HTMLHeadElementTable 1
  profile	KJS::HTMLElement::HeadProfile	DontDelete
@end
@begin HTMLLinkElementTable 11
  disabled	KJS::HTMLElement::LinkDisabled	DontDelete
  charset	KJS::HTMLElement::LinkCharset	DontDelete
  href		KJS::HTMLElement::LinkHref	DontDelete
  hreflang	KJS::HTMLElement::LinkHrefLang	DontDelete
  media		KJS::HTMLElement::LinkMedia	DontDelete
  rel		KJS::HTMLElement::LinkRel      	DontDelete
  rev		KJS::HTMLElement::LinkRev	DontDelete
  target	KJS::HTMLElement::LinkTarget	DontDelete
  type		KJS::HTMLElement::LinkType	DontDelete
  sheet		KJS::HTMLElement::LinkSheet	DontDelete|ReadOnly
@end
@begin HTMLTitleElementTable 1
  text		KJS::HTMLElement::TitleText	DontDelete
@end
@begin HTMLMetaElementTable 4
  content	KJS::HTMLElement::MetaContent	DontDelete
  httpEquiv	KJS::HTMLElement::MetaHttpEquiv	DontDelete
  name		KJS::HTMLElement::MetaName	DontDelete
  scheme	KJS::HTMLElement::MetaScheme	DontDelete
@end
@begin HTMLBaseElementTable 2
  href		KJS::HTMLElement::BaseHref	DontDelete
  target	KJS::HTMLElement::BaseTarget	DontDelete
@end
@begin HTMLIsIndexElementTable 2
  form		KJS::HTMLElement::IsIndexForm	DontDelete|ReadOnly
  prompt	KJS::HTMLElement::IsIndexPrompt	DontDelete
@end
@begin HTMLStyleElementTable 4
  disabled	KJS::HTMLElement::StyleDisabled	DontDelete
  media		KJS::HTMLElement::StyleMedia	DontDelete
  type		KJS::HTMLElement::StyleType	DontDelete
  sheet		KJS::HTMLElement::StyleSheet	DontDelete|ReadOnly
@end
@begin HTMLBodyElementTable 10
  aLink		KJS::HTMLElement::BodyALink	DontDelete
  background	KJS::HTMLElement::BodyBackground	DontDelete
  bgColor	KJS::HTMLElement::BodyBgColor	DontDelete
  link		KJS::HTMLElement::BodyLink	DontDelete
  text		KJS::HTMLElement::BodyText	DontDelete
  vLink		KJS::HTMLElement::BodyVLink	DontDelete
  scrollLeft    KJS::HTMLElement::BodyScrollLeft        DontDelete
  scrollTop     KJS::HTMLElement::BodyScrollTop         DontDelete
  scrollHeight	KJS::HTMLElement::BodyScrollHeight	DontDelete|ReadOnly
  scrollWidth	KJS::HTMLElement::BodyScrollWidth	DontDelete|ReadOnly
@end
@begin HTMLFormElementTable 11
# Also supported, by name/index
  elements	KJS::HTMLElement::FormElements	DontDelete|ReadOnly
  length	KJS::HTMLElement::FormLength	DontDelete|ReadOnly
  name		KJS::HTMLElement::FormName	DontDelete
  acceptCharset	KJS::HTMLElement::FormAcceptCharset	DontDelete
  action	KJS::HTMLElement::FormAction	DontDelete
  enctype	KJS::HTMLElement::FormEncType	DontDelete
  method	KJS::HTMLElement::FormMethod	DontDelete
  target	KJS::HTMLElement::FormTarget	DontDelete
  submit	KJS::HTMLElement::FormSubmit	DontDelete|Function 0
  reset		KJS::HTMLElement::FormReset	DontDelete|Function 0
@end
@begin HTMLSelectElementTable 11
# Also supported, by index
  type		KJS::HTMLElement::SelectType	DontDelete|ReadOnly
  selectedIndex	KJS::HTMLElement::SelectSelectedIndex	DontDelete
  value		KJS::HTMLElement::SelectValue	DontDelete
  length	KJS::HTMLElement::SelectLength	DontDelete
  form		KJS::HTMLElement::SelectForm	DontDelete|ReadOnly
  options	KJS::HTMLElement::SelectOptions	DontDelete|ReadOnly
  disabled	KJS::HTMLElement::SelectDisabled	DontDelete
  multiple	KJS::HTMLElement::SelectMultiple	DontDelete
  name		KJS::HTMLElement::SelectName	DontDelete
  size		KJS::HTMLElement::SelectSize	DontDelete
  tabIndex	KJS::HTMLElement::SelectTabIndex	DontDelete
  add		KJS::HTMLElement::SelectAdd	DontDelete|Function 2
  remove	KJS::HTMLElement::SelectRemove	DontDelete|Function 1
  blur		KJS::HTMLElement::SelectBlur	DontDelete|Function 0
  focus		KJS::HTMLElement::SelectFocus	DontDelete|Function 0
@end
@begin HTMLOptGroupElementTable 2
  disabled	KJS::HTMLElement::OptGroupDisabled	DontDelete
  label		KJS::HTMLElement::OptGroupLabel		DontDelete
@end
@begin HTMLOptionElementTable 8
  form		KJS::HTMLElement::OptionForm		DontDelete|ReadOnly
  defaultSelected KJS::HTMLElement::OptionDefaultSelected	DontDelete
  text		KJS::HTMLElement::OptionText		DontDelete
  index		KJS::HTMLElement::OptionIndex		DontDelete|ReadOnly
  disabled	KJS::HTMLElement::OptionDisabled	DontDelete
  label		KJS::HTMLElement::OptionLabel		DontDelete
  selected	KJS::HTMLElement::OptionSelected	DontDelete
  value		KJS::HTMLElement::OptionValue		DontDelete
@end
@begin HTMLInputElementTable 24
  defaultValue	KJS::HTMLElement::InputDefaultValue	DontDelete
  defaultChecked KJS::HTMLElement::InputDefaultChecked	DontDelete
  form		KJS::HTMLElement::InputForm		DontDelete|ReadOnly
  accept	KJS::HTMLElement::InputAccept		DontDelete
  accessKey	KJS::HTMLElement::InputAccessKey	DontDelete
  align		KJS::HTMLElement::InputAlign		DontDelete
  alt		KJS::HTMLElement::InputAlt		DontDelete
  checked	KJS::HTMLElement::InputChecked		DontDelete
  disabled	KJS::HTMLElement::InputDisabled		DontDelete
  indeterminate KJS::HTMLElement::InputIndeterminate    DontDelete
  maxLength	KJS::HTMLElement::InputMaxLength	DontDelete
  name		KJS::HTMLElement::InputName		DontDelete
  readOnly	KJS::HTMLElement::InputReadOnly		DontDelete
  selectionStart	KJS::HTMLElement::InputSelectionStart	DontDelete
  selectionEnd	KJS::HTMLElement::InputSelectionEnd	DontDelete
  size		KJS::HTMLElement::InputSize		DontDelete
  src		KJS::HTMLElement::InputSrc		DontDelete
  tabIndex	KJS::HTMLElement::InputTabIndex		DontDelete
  type		KJS::HTMLElement::InputType		DontDelete
  useMap	KJS::HTMLElement::InputUseMap		DontDelete
  value		KJS::HTMLElement::InputValue		DontDelete
  blur		KJS::HTMLElement::InputBlur		DontDelete|Function 0
  focus		KJS::HTMLElement::InputFocus		DontDelete|Function 0
  select	KJS::HTMLElement::InputSelect		DontDelete|Function 0
  click		KJS::HTMLElement::InputClick		DontDelete|Function 0
  setSelectionRange	KJS::HTMLElement::InputSetSelectionRange	DontDelete|Function 2
@end
@begin HTMLTextAreaElementTable 17
  defaultValue	KJS::HTMLElement::TextAreaDefaultValue	DontDelete
  form		KJS::HTMLElement::TextAreaForm		DontDelete|ReadOnly
  accessKey	KJS::HTMLElement::TextAreaAccessKey	DontDelete
  cols		KJS::HTMLElement::TextAreaCols		DontDelete
  disabled	KJS::HTMLElement::TextAreaDisabled	DontDelete
  name		KJS::HTMLElement::TextAreaName		DontDelete
  readOnly	KJS::HTMLElement::TextAreaReadOnly	DontDelete
  rows		KJS::HTMLElement::TextAreaRows		DontDelete
  selectionStart	KJS::HTMLElement::TextAreaSelectionStart	DontDelete
  selectionEnd	KJS::HTMLElement::TextAreaSelectionEnd	DontDelete
  tabIndex	KJS::HTMLElement::TextAreaTabIndex	DontDelete
  type		KJS::HTMLElement::TextAreaType		DontDelete|ReadOnly
  value		KJS::HTMLElement::TextAreaValue		DontDelete
  blur		KJS::HTMLElement::TextAreaBlur		DontDelete|Function 0
  focus		KJS::HTMLElement::TextAreaFocus		DontDelete|Function 0
  select	KJS::HTMLElement::TextAreaSelect	DontDelete|Function 0
  setSelectionRange	KJS::HTMLElement::TextAreaSetSelectionRange	DontDelete|Function 2
@end
@begin HTMLButtonElementTable 7
  form		KJS::HTMLElement::ButtonForm		DontDelete|ReadOnly
  accessKey	KJS::HTMLElement::ButtonAccessKey	DontDelete
  disabled	KJS::HTMLElement::ButtonDisabled	DontDelete
  name		KJS::HTMLElement::ButtonName		DontDelete
  tabIndex	KJS::HTMLElement::ButtonTabIndex	DontDelete
  type		KJS::HTMLElement::ButtonType		DontDelete|ReadOnly
  value		KJS::HTMLElement::ButtonValue		DontDelete
  blur		KJS::HTMLElement::ButtonBlur		DontDelete|Function 0
  focus		KJS::HTMLElement::ButtonFocus		DontDelete|Function 0
@end
@begin HTMLLabelElementTable 4
  form		KJS::HTMLElement::LabelForm		DontDelete|ReadOnly
  accessKey	KJS::HTMLElement::LabelAccessKey	DontDelete
  htmlFor	KJS::HTMLElement::LabelHtmlFor		DontDelete
  focus     KJS::HTMLElement::LabelFocus        DontDelete|Function 0
@end
@begin HTMLFieldSetElementTable 1
  form		KJS::HTMLElement::FieldSetForm		DontDelete|ReadOnly
@end
@begin HTMLLegendElementTable 4
  form		KJS::HTMLElement::LegendForm		DontDelete|ReadOnly
  accessKey	KJS::HTMLElement::LegendAccessKey	DontDelete
  align		KJS::HTMLElement::LegendAlign		DontDelete
  focus     KJS::HTMLElement::LegendFocus     DontDelete|Function 0
@end
@begin HTMLUListElementTable 2
  compact	KJS::HTMLElement::UListCompact		DontDelete
  type		KJS::HTMLElement::UListType		DontDelete
@end
@begin HTMLOListElementTable 3
  compact	KJS::HTMLElement::OListCompact		DontDelete
  start		KJS::HTMLElement::OListStart		DontDelete
  type		KJS::HTMLElement::OListType		DontDelete
@end
@begin HTMLDListElementTable 1
  compact	KJS::HTMLElement::DListCompact		DontDelete
@end
@begin HTMLDirectoryElementTable 1
  compact	KJS::HTMLElement::DirectoryCompact	DontDelete
@end
@begin HTMLMenuElementTable 1
  compact	KJS::HTMLElement::MenuCompact		DontDelete
@end
@begin HTMLLIElementTable 2
  type		KJS::HTMLElement::LIType		DontDelete
  value		KJS::HTMLElement::LIValue		DontDelete
@end
@begin HTMLDivElementTable 1
  align		KJS::HTMLElement::DivAlign		DontDelete
@end
@begin HTMLParagraphElementTable 1
  align		KJS::HTMLElement::ParagraphAlign	DontDelete
@end
@begin HTMLHeadingElementTable 1
  align		KJS::HTMLElement::HeadingAlign		DontDelete
@end
@begin HTMLBlockQuoteElementTable 1
  cite		KJS::HTMLElement::BlockQuoteCite	DontDelete
@end
@begin HTMLQuoteElementTable 1
  cite		KJS::HTMLElement::QuoteCite		DontDelete
@end
@begin HTMLPreElementTable 1
  width		KJS::HTMLElement::PreWidth		DontDelete
@end
@begin HTMLBRElementTable 1
  clear		KJS::HTMLElement::BRClear		DontDelete
@end
@begin HTMLBaseFontElementTable 3
  color		KJS::HTMLElement::BaseFontColor		DontDelete
  face		KJS::HTMLElement::BaseFontFace		DontDelete
  size		KJS::HTMLElement::BaseFontSize		DontDelete
@end
@begin HTMLFontElementTable 3
  color		KJS::HTMLElement::FontColor		DontDelete
  face		KJS::HTMLElement::FontFace		DontDelete
  size		KJS::HTMLElement::FontSize		DontDelete
@end
@begin HTMLHRElementTable 4
  align		KJS::HTMLElement::HRAlign		DontDelete
  noShade	KJS::HTMLElement::HRNoShade		DontDelete
  size		KJS::HTMLElement::HRSize		DontDelete
  width		KJS::HTMLElement::HRWidth		DontDelete
@end
@begin HTMLModElementTable 2
  cite		KJS::HTMLElement::ModCite		DontDelete
  dateTime	KJS::HTMLElement::ModDateTime		DontDelete
@end
@begin HTMLAnchorElementTable 24
  accessKey	KJS::HTMLElement::AnchorAccessKey	DontDelete
  charset	KJS::HTMLElement::AnchorCharset		DontDelete
  coords	KJS::HTMLElement::AnchorCoords		DontDelete
  href		KJS::HTMLElement::AnchorHref		DontDelete
  hreflang	KJS::HTMLElement::AnchorHrefLang	DontDelete
  hash		KJS::HTMLElement::AnchorHash		DontDelete|ReadOnly
  host		KJS::HTMLElement::AnchorHost		DontDelete|ReadOnly
  hostname	KJS::HTMLElement::AnchorHostname	DontDelete|ReadOnly
  name		KJS::HTMLElement::AnchorName		DontDelete
  pathname	KJS::HTMLElement::AnchorPathName	DontDelete|ReadOnly
  port		KJS::HTMLElement::AnchorPort		DontDelete|ReadOnly
  protocol	KJS::HTMLElement::AnchorProtocol	DontDelete|ReadOnly
  rel		KJS::HTMLElement::AnchorRel		DontDelete
  rev		KJS::HTMLElement::AnchorRev		DontDelete
  search	KJS::HTMLElement::AnchorSearch		DontDelete|ReadOnly
  shape		KJS::HTMLElement::AnchorShape		DontDelete
  tabIndex	KJS::HTMLElement::AnchorTabIndex	DontDelete
  target	KJS::HTMLElement::AnchorTarget		DontDelete
  text		KJS::HTMLElement::AnchorText		DontDelete|ReadOnly
  type		KJS::HTMLElement::AnchorType		DontDelete
  blur		KJS::HTMLElement::AnchorBlur		DontDelete|Function 0
  focus		KJS::HTMLElement::AnchorFocus		DontDelete|Function 0
  toString      KJS::HTMLElement::AnchorToString        DontDelete|Function 0
@end
@begin HTMLImageElementTable 14
  name		KJS::HTMLElement::ImageName		DontDelete
  align		KJS::HTMLElement::ImageAlign		DontDelete
  alt		KJS::HTMLElement::ImageAlt		DontDelete
  border	KJS::HTMLElement::ImageBorder		DontDelete
  height	KJS::HTMLElement::ImageHeight		DontDelete
  hspace	KJS::HTMLElement::ImageHspace		DontDelete
  isMap		KJS::HTMLElement::ImageIsMap		DontDelete
  longDesc	KJS::HTMLElement::ImageLongDesc		DontDelete
  src		KJS::HTMLElement::ImageSrc		DontDelete
  useMap	KJS::HTMLElement::ImageUseMap		DontDelete
  vspace	KJS::HTMLElement::ImageVspace		DontDelete
  width		KJS::HTMLElement::ImageWidth		DontDelete
  x         KJS::HTMLElement::ImageX            DontDelete|ReadOnly
  y         KJS::HTMLElement::ImageY            DontDelete|ReadOnly
@end
@begin HTMLObjectElementTable 20
  form		  KJS::HTMLElement::ObjectForm		  DontDelete|ReadOnly
  code		  KJS::HTMLElement::ObjectCode		  DontDelete
  align		  KJS::HTMLElement::ObjectAlign		  DontDelete
  archive	  KJS::HTMLElement::ObjectArchive	  DontDelete
  border	  KJS::HTMLElement::ObjectBorder	  DontDelete
  codeBase	  KJS::HTMLElement::ObjectCodeBase	  DontDelete
  codeType	  KJS::HTMLElement::ObjectCodeType	  DontDelete
  contentDocument KJS::HTMLElement::ObjectContentDocument DontDelete|ReadOnly
  data		  KJS::HTMLElement::ObjectData		  DontDelete
  declare	  KJS::HTMLElement::ObjectDeclare	  DontDelete
  height	  KJS::HTMLElement::ObjectHeight	  DontDelete
  hspace	  KJS::HTMLElement::ObjectHspace	  DontDelete
  name		  KJS::HTMLElement::ObjectName		  DontDelete
  standby	  KJS::HTMLElement::ObjectStandby	  DontDelete
  tabIndex	  KJS::HTMLElement::ObjectTabIndex	  DontDelete
  type		  KJS::HTMLElement::ObjectType		  DontDelete
  useMap	  KJS::HTMLElement::ObjectUseMap	  DontDelete
  vspace	  KJS::HTMLElement::ObjectVspace	  DontDelete
  width		  KJS::HTMLElement::ObjectWidth		  DontDelete
@end
@begin HTMLParamElementTable 4
  name		KJS::HTMLElement::ParamName		DontDelete
  type		KJS::HTMLElement::ParamType		DontDelete
  value		KJS::HTMLElement::ParamValue		DontDelete
  valueType	KJS::HTMLElement::ParamValueType	DontDelete
@end
@begin HTMLAppletElementTable 11
  align		KJS::HTMLElement::AppletAlign		DontDelete
  alt		KJS::HTMLElement::AppletAlt		DontDelete
  archive	KJS::HTMLElement::AppletArchive		DontDelete
  code		KJS::HTMLElement::AppletCode		DontDelete
  codeBase	KJS::HTMLElement::AppletCodeBase	DontDelete
  height	KJS::HTMLElement::AppletHeight		DontDelete
  hspace	KJS::HTMLElement::AppletHspace		DontDelete
  name		KJS::HTMLElement::AppletName		DontDelete
  object	KJS::HTMLElement::AppletObject		DontDelete
  vspace	KJS::HTMLElement::AppletVspace		DontDelete
  width		KJS::HTMLElement::AppletWidth		DontDelete
@end
@begin HTMLMapElementTable 2
  areas		KJS::HTMLElement::MapAreas		DontDelete|ReadOnly
  name		KJS::HTMLElement::MapName		DontDelete
@end
@begin HTMLAreaElementTable 15
  accessKey	KJS::HTMLElement::AreaAccessKey		DontDelete
  alt		KJS::HTMLElement::AreaAlt		DontDelete
  coords	KJS::HTMLElement::AreaCoords		DontDelete
  href		KJS::HTMLElement::AreaHref		DontDelete
  hash		KJS::HTMLElement::AreaHash		DontDelete|ReadOnly
  host		KJS::HTMLElement::AreaHost		DontDelete|ReadOnly
  hostname	KJS::HTMLElement::AreaHostName		DontDelete|ReadOnly
  pathname	KJS::HTMLElement::AreaPathName		DontDelete|ReadOnly
  port		KJS::HTMLElement::AreaPort		DontDelete|ReadOnly
  protocol	KJS::HTMLElement::AreaProtocol		DontDelete|ReadOnly
  search	KJS::HTMLElement::AreaSearch		DontDelete|ReadOnly
  noHref	KJS::HTMLElement::AreaNoHref		DontDelete
  shape		KJS::HTMLElement::AreaShape		DontDelete
  tabIndex	KJS::HTMLElement::AreaTabIndex		DontDelete
  target	KJS::HTMLElement::AreaTarget		DontDelete
@end
@begin HTMLScriptElementTable 7
  text		KJS::HTMLElement::ScriptText		DontDelete
  htmlFor	KJS::HTMLElement::ScriptHtmlFor		DontDelete
  event		KJS::HTMLElement::ScriptEvent		DontDelete
  charset	KJS::HTMLElement::ScriptCharset		DontDelete
  defer		KJS::HTMLElement::ScriptDefer		DontDelete
  src		KJS::HTMLElement::ScriptSrc		DontDelete
  type		KJS::HTMLElement::ScriptType		DontDelete
@end
@begin HTMLTableElementTable 23
  caption	KJS::HTMLElement::TableCaption		DontDelete
  tHead		KJS::HTMLElement::TableTHead		DontDelete
  tFoot		KJS::HTMLElement::TableTFoot		DontDelete
  rows		KJS::HTMLElement::TableRows		DontDelete|ReadOnly
  tBodies	KJS::HTMLElement::TableTBodies		DontDelete|ReadOnly
  align		KJS::HTMLElement::TableAlign		DontDelete
  bgColor	KJS::HTMLElement::TableBgColor		DontDelete
  border	KJS::HTMLElement::TableBorder		DontDelete
  cellPadding	KJS::HTMLElement::TableCellPadding	DontDelete
  cellSpacing	KJS::HTMLElement::TableCellSpacing	DontDelete
  frame		KJS::HTMLElement::TableFrame		DontDelete
  rules		KJS::HTMLElement::TableRules		DontDelete
  summary	KJS::HTMLElement::TableSummary		DontDelete
  width		KJS::HTMLElement::TableWidth		DontDelete
  createTHead	KJS::HTMLElement::TableCreateTHead	DontDelete|Function 0
  deleteTHead	KJS::HTMLElement::TableDeleteTHead	DontDelete|Function 0
  createTFoot	KJS::HTMLElement::TableCreateTFoot	DontDelete|Function 0
  deleteTFoot	KJS::HTMLElement::TableDeleteTFoot	DontDelete|Function 0
  createCaption	KJS::HTMLElement::TableCreateCaption	DontDelete|Function 0
  deleteCaption	KJS::HTMLElement::TableDeleteCaption	DontDelete|Function 0
  insertRow	KJS::HTMLElement::TableInsertRow	DontDelete|Function 1
  deleteRow	KJS::HTMLElement::TableDeleteRow	DontDelete|Function 1
@end
@begin HTMLTableCaptionElementTable 1
  align		KJS::HTMLElement::TableCaptionAlign	DontDelete
@end
@begin HTMLTableColElementTable 7
  align		KJS::HTMLElement::TableColAlign		DontDelete
  ch		KJS::HTMLElement::TableColCh		DontDelete
  chOff		KJS::HTMLElement::TableColChOff		DontDelete
  span		KJS::HTMLElement::TableColSpan		DontDelete
  vAlign	KJS::HTMLElement::TableColVAlign	DontDelete
  width		KJS::HTMLElement::TableColWidth		DontDelete
@end
@begin HTMLTableSectionElementTable 7
  align		KJS::HTMLElement::TableSectionAlign		DontDelete
  ch		KJS::HTMLElement::TableSectionCh		DontDelete
  chOff		KJS::HTMLElement::TableSectionChOff		DontDelete
  vAlign	KJS::HTMLElement::TableSectionVAlign		DontDelete
  rows		KJS::HTMLElement::TableSectionRows		DontDelete|ReadOnly
  insertRow	KJS::HTMLElement::TableSectionInsertRow		DontDelete|Function 1
  deleteRow	KJS::HTMLElement::TableSectionDeleteRow		DontDelete|Function 1
@end
@begin HTMLTableRowElementTable 11
  rowIndex	KJS::HTMLElement::TableRowRowIndex		DontDelete|ReadOnly
  sectionRowIndex KJS::HTMLElement::TableRowSectionRowIndex	DontDelete|ReadOnly
  cells		KJS::HTMLElement::TableRowCells			DontDelete|ReadOnly
  align		KJS::HTMLElement::TableRowAlign			DontDelete
  bgColor	KJS::HTMLElement::TableRowBgColor		DontDelete
  ch		KJS::HTMLElement::TableRowCh			DontDelete
  chOff		KJS::HTMLElement::TableRowChOff			DontDelete
  vAlign	KJS::HTMLElement::TableRowVAlign		DontDelete
  insertCell	KJS::HTMLElement::TableRowInsertCell		DontDelete|Function 1
  deleteCell	KJS::HTMLElement::TableRowDeleteCell		DontDelete|Function 1
@end
@begin HTMLTableCellElementTable 15
  cellIndex	KJS::HTMLElement::TableCellCellIndex		DontDelete|ReadOnly
  abbr		KJS::HTMLElement::TableCellAbbr			DontDelete
  align		KJS::HTMLElement::TableCellAlign		DontDelete
  axis		KJS::HTMLElement::TableCellAxis			DontDelete
  bgColor	KJS::HTMLElement::TableCellBgColor		DontDelete
  ch		KJS::HTMLElement::TableCellCh			DontDelete
  chOff		KJS::HTMLElement::TableCellChOff		DontDelete
  colSpan	KJS::HTMLElement::TableCellColSpan		DontDelete
  headers	KJS::HTMLElement::TableCellHeaders		DontDelete
  height	KJS::HTMLElement::TableCellHeight		DontDelete
  noWrap	KJS::HTMLElement::TableCellNoWrap		DontDelete
  rowSpan	KJS::HTMLElement::TableCellRowSpan		DontDelete
  scope		KJS::HTMLElement::TableCellScope		DontDelete
  vAlign	KJS::HTMLElement::TableCellVAlign		DontDelete
  width		KJS::HTMLElement::TableCellWidth		DontDelete
@end
@begin HTMLFrameSetElementTable 2
  cols		KJS::HTMLElement::FrameSetCols			DontDelete
  rows		KJS::HTMLElement::FrameSetRows			DontDelete
@end
@begin HTMLFrameElementTable 9
  contentDocument KJS::HTMLElement::FrameContentDocument        DontDelete|ReadOnly
  contentWindow   KJS::HTMLElement::FrameContentWindow          DontDelete|ReadOnly
  frameBorder     KJS::HTMLElement::FrameFrameBorder		DontDelete
  longDesc	  KJS::HTMLElement::FrameLongDesc		DontDelete
  marginHeight	  KJS::HTMLElement::FrameMarginHeight		DontDelete
  marginWidth	  KJS::HTMLElement::FrameMarginWidth		DontDelete
  name		  KJS::HTMLElement::FrameName			DontDelete
  noResize	  KJS::HTMLElement::FrameNoResize		DontDelete
  scrolling	  KJS::HTMLElement::FrameScrolling		DontDelete
  src		  KJS::HTMLElement::FrameSrc			DontDelete
  location	  KJS::HTMLElement::FrameLocation		DontDelete
@end
@begin HTMLIFrameElementTable 12
  align		  KJS::HTMLElement::IFrameAlign			DontDelete
  contentDocument KJS::HTMLElement::IFrameContentDocument       DontDelete|ReadOnly
  contentWindow   KJS::HTMLElement::IFrameContentWindow         DontDelete|ReadOnly
  document	  KJS::HTMLElement::IFrameDocument		DontDelete|ReadOnly
  frameBorder	  KJS::HTMLElement::IFrameFrameBorder		DontDelete
  height	  KJS::HTMLElement::IFrameHeight		DontDelete
  longDesc	  KJS::HTMLElement::IFrameLongDesc		DontDelete
  marginHeight	  KJS::HTMLElement::IFrameMarginHeight		DontDelete
  marginWidth	  KJS::HTMLElement::IFrameMarginWidth		DontDelete
  name		  KJS::HTMLElement::IFrameName			DontDelete
  scrolling	  KJS::HTMLElement::IFrameScrolling		DontDelete
  src		  KJS::HTMLElement::IFrameSrc			DontDelete
  width		  KJS::HTMLElement::IFrameWidth			DontDelete
@end

@begin HTMLMarqueeElementTable 2
  start           KJS::HTMLElement::MarqueeStart		DontDelete|Function 0
  stop            KJS::HTMLElement::MarqueeStop                 DontDelete|Function 0
@end

@begin HTMLCanvasElementTable 1
  getContext      KJS::HTMLElement::GetContext                  DontDelete|Function 1
@end
*/

HTMLElement::HTMLElement(ExecState *exec, HTMLElementImpl *e)
    : DOMElement(exec, e)
{
}

ValueImp *HTMLElement::formIndexGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLElement *thisObj = static_cast<HTMLElement *>(slot.slotBase());
    HTMLFormElementImpl *form = static_cast<HTMLFormElementImpl *>(thisObj->impl());

    return getDOMNode(exec, form->elements()->item(slot.index()));
}

ValueImp *HTMLElement::formNameGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLElement *thisObj = static_cast<HTMLElement *>(slot.slotBase());
    HTMLFormElementImpl *form = static_cast<HTMLFormElementImpl *>(thisObj->impl());
    
    return HTMLCollection(exec, form->elements().get()).getNamedItems(exec, propertyName);
}

ValueImp *HTMLElement::selectIndexGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLElement *thisObj = static_cast<HTMLElement *>(slot.slotBase());
    HTMLSelectElementImpl *select = static_cast<HTMLSelectElementImpl *>(thisObj->impl());

    return getDOMNode(exec, select->optionsHTMLCollection()->item(slot.index()));
}

ValueImp *HTMLElement::framesetNameGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLElement *thisObj = static_cast<HTMLElement *>(slot.slotBase());
    HTMLElementImpl *element = static_cast<HTMLElementImpl *>(thisObj->impl());

    NodeImpl *frame = element->children()->namedItem(propertyName.domString());
    if (DocumentImpl* doc = static_cast<HTMLFrameElementImpl *>(frame)->contentDocument())
        if (Window *window = Window::retrieveWindow(doc->part()))
            return window;

    return Undefined();
}

ValueImp *HTMLElement::frameWindowPropertyGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLElement *thisObj = static_cast<HTMLElement *>(slot.slotBase());

    if (DocumentImpl *doc = static_cast<HTMLFrameElementImpl *>(thisObj->impl())->contentDocument())
        if (Window *window = Window::retrieveWindow(doc->part()))
            return window->get(exec, propertyName);

    return Undefined();
}

ValueImp *HTMLElement::runtimeObjectGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLElement *thisObj = static_cast<HTMLElement *>(slot.slotBase());
    HTMLElementImpl *element = static_cast<HTMLElementImpl *>(thisObj->impl());

    return getRuntimeObject(exec, element);
}

ValueImp *HTMLElement::runtimeObjectPropertyGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLElement *thisObj = static_cast<HTMLElement *>(slot.slotBase());
    HTMLElementImpl *element = static_cast<HTMLElementImpl *>(thisObj->impl());

    if (ValueImp *runtimeObject = getRuntimeObject(exec, element))
        return static_cast<ObjectImp *>(runtimeObject)->get(exec, propertyName);
    return Undefined();
}

bool HTMLElement::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    HTMLElementImpl &element = *static_cast<HTMLElementImpl *>(impl());

    // First look at dynamic properties
    if (element.hasLocalName(formTag)) {
        HTMLFormElementImpl &form = static_cast<HTMLFormElementImpl &>(element);
        // Check if we're retrieving an element (by index or by name)
        bool ok;
        uint u = propertyName.toUInt32(&ok);
        if (ok) {
            slot.setCustomIndex(this, u, formIndexGetter);
            return true;
        }

        // FIXME: need faster way to check for a named item and/or a way to pass on the named items subcollection
        ValueImp *namedItems = HTMLCollection(exec, form.elements().get()).getNamedItems(exec, propertyName);
        if (!namedItems->isUndefined()) {
            slot.setCustom(this, formNameGetter);
            return true;
        }
    } else if (element.hasLocalName(selectTag)) {
        bool ok;
        uint u = propertyName.toUInt32(&ok);
        if (ok) {
            // not specified by DOM(?) but supported in netscape/IE
            slot.setCustomIndex(this, u, selectIndexGetter);
            return true;
        }
    } else if (element.hasLocalName(framesetTag)) {
        NodeImpl *frame = element.children()->namedItem(propertyName.domString());
        if (frame && frame->hasTagName(frameTag)) {
            slot.setCustom(this, framesetNameGetter);
        }
    } else if (element.hasLocalName(frameTag) || element.hasLocalName(iframeTag)) {
        if (DocumentImpl* doc = static_cast<HTMLFrameElementImpl &>(element).contentDocument()) {
            Window *window = Window::retrieveWindow(doc->part());
            if (window && window->hasProperty(exec, propertyName)) {
                slot.setCustom(this, frameWindowPropertyGetter);
                return true;
            }
        }
    }
    else if (element.hasLocalName(embedTag) || element.hasLocalName(objectTag) ||
             element.hasLocalName(appletTag)) {
        if (propertyName == "__apple_runtime_object") {
            slot.setCustom(this, runtimeObjectGetter);
            return true;
        }
	ValueImp *runtimeObject = getRuntimeObject(exec,&element);
	if (runtimeObject) {
	    ObjectImp *imp = static_cast<ObjectImp *>(runtimeObject);
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
        if (element.hasLocalName(inputTag) &&
            !static_cast<HTMLInputElementImpl *>(impl())->canHaveSelection()) {
            switch (entry->value) {
            case InputSetSelectionRange:
            case InputSelectionStart:
            case InputSelectionEnd:
                break;
            default:
                if (entry->attr & Function)
                    slot.setStaticEntry(this, entry, staticFunctionGetter<HTMLElementFunction>); 
                else
                    slot.setStaticEntry(this, entry, staticValueGetter<HTMLElement>);
                return true;
            }
        } else {
            if (entry->attr & Function)
                slot.setStaticEntry(this, entry, staticFunctionGetter<HTMLElementFunction>); 
            else
                slot.setStaticEntry(this, entry, staticValueGetter<HTMLElement>);
            return true;
        }
    }

    // Base HTMLElement stuff or parent class forward, as usual
    return getStaticPropertySlot<HTMLElementFunction, HTMLElement, DOMElement>(exec, &HTMLElementTable, this, propertyName, slot);
}

bool KJS::HTMLElement::implementsCall() const
{
    HTMLElementImpl *element = static_cast<HTMLElementImpl *>(impl());
    if (element->hasTagName(embedTag) || element->hasTagName(objectTag) || element->hasTagName(appletTag)) {
        DocumentImpl* doc = element->getDocument();
        KJSProxy *proxy = KJSProxy::proxy(doc->part());
        ExecState *exec = proxy->interpreter()->globalExec();
        if (ValueImp *runtimeObject = getRuntimeObject(exec, element))
            return static_cast<ObjectImp *>(runtimeObject)->implementsCall();
    }
    return false;
}

ValueImp *KJS::HTMLElement::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List&args)
{
    HTMLElementImpl *element = static_cast<HTMLElementImpl *>(impl());
    if (element->hasTagName(embedTag) || element->hasTagName(objectTag) || element->hasTagName(appletTag)) {
        if (ValueImp *runtimeObject = getRuntimeObject(exec, element))
            return static_cast<ObjectImp *>(runtimeObject)->call(exec, thisObj, args);
    }
    return Undefined();
}

ValueImp *HTMLElement::htmlGetter(ExecState* exec, int token) const
{
    HTMLHtmlElementImpl& html = *static_cast<HTMLHtmlElementImpl*>(impl());
    if (token == HtmlVersion)
        return String(html.version());
    return Undefined();
}

ValueImp *HTMLElement::headGetter(ExecState* exec, int token) const
{
    HTMLHeadElementImpl &head = *static_cast<HTMLHeadElementImpl*>(impl());
    if (token == HeadProfile)
        return String(head.profile());
    return Undefined();
}

ValueImp *HTMLElement::linkGetter(ExecState* exec, int token) const
{
    HTMLLinkElementImpl &link = *static_cast<HTMLLinkElementImpl*>(impl());
    switch (token) {
        case LinkDisabled:
            return Boolean(link.disabled());
        case LinkCharset:
            return String(link.charset());
        case LinkHref:
            return String(link.href());
        case LinkHrefLang:
            return String(link.hreflang());
        case LinkMedia:           
            return String(link.media());
        case LinkRel:             
            return String(link.rel());
        case LinkRev:            
            return String(link.rev());
        case LinkTarget:          
            return String(link.target());
        case LinkType:            
            return String(link.type());
        case LinkSheet:           
            return getDOMStyleSheet(exec, link.sheet());
    }
    return Undefined();
}

ValueImp *HTMLElement::titleGetter(ExecState* exec, int token) const
{
    HTMLTitleElementImpl& title = *static_cast<HTMLTitleElementImpl*>(impl());
    if (token == TitleText)
        return String(title.text());
    return Undefined();
}

ValueImp *HTMLElement::metaGetter(ExecState* exec, int token) const
{
    HTMLMetaElementImpl& meta = *static_cast<HTMLMetaElementImpl*>(impl());
    switch (token) {
        case MetaContent:         return String(meta.content());
        case MetaHttpEquiv:       return String(meta.httpEquiv());
        case MetaName:            return String(meta.name());
        case MetaScheme:          return String(meta.scheme());
    }
    return Undefined();
}

ValueImp *HTMLElement::baseGetter(ExecState* exec, int token) const
{
    HTMLBaseElementImpl& base = *static_cast<HTMLBaseElementImpl*>(impl());
    switch (token) {
        case BaseHref:            return String(base.href());
        case BaseTarget:          return String(base.target());
    }
    return Undefined();
}

ValueImp *HTMLElement::isIndexGetter(ExecState* exec, int token) const
{
    HTMLIsIndexElementImpl& isindex = *static_cast<HTMLIsIndexElementImpl*>(impl());
    switch (token) {
        case IsIndexForm:            return getDOMNode(exec, isindex.form()); // type HTMLFormElement
        case IsIndexPrompt:          return String(isindex.prompt());
    }
    return Undefined();
}

ValueImp *HTMLElement::styleGetter(ExecState* exec, int token) const
{
    HTMLStyleElementImpl& style = *static_cast<HTMLStyleElementImpl*>(impl());
    switch (token) {
        case StyleDisabled:        return Boolean(style.disabled());
        case StyleMedia:           return String(style.media());
        case StyleType:            return String(style.type());
        case StyleSheet:           return getDOMStyleSheet(exec, style.sheet());
    }
    return Undefined();
}

ValueImp *HTMLElement::bodyGetter(ExecState* exec, int token) const
{
    HTMLBodyElementImpl& body = *static_cast<HTMLBodyElementImpl*>(impl());
    switch (token) {
        case BodyALink:           return String(body.aLink());
        case BodyBackground:      return String(body.background());
        case BodyBgColor:         return String(body.bgColor());
        case BodyLink:            return String(body.link());
        case BodyText:            return String(body.text());
        case BodyVLink:           return String(body.vLink());
        default: {
            // Update the document's layout before we compute these attributes.
            DocumentImpl *doc = body.getDocument();
            if (doc)
                doc->updateLayoutIgnorePendingStylesheets();
            KHTMLView *view = doc ? doc->view() : 0;
            switch (token) {
                case BodyScrollLeft:
                    return Number(view ? view->contentsX() : 0);
                case BodyScrollTop:
                    return Number(view ? view->contentsY() : 0);
                case BodyScrollHeight:
                    return Number(view ? view->contentsHeight() : 0);
                case BodyScrollWidth:
                    return Number(view ? view->contentsWidth() : 0);
            }
        }
    }
    return Undefined();
}

ValueImp *HTMLElement::formGetter(ExecState* exec, int token) const
{
    HTMLFormElementImpl& form = *static_cast<HTMLFormElementImpl*>(impl());
    switch (token) {
        case FormElements:        return getHTMLCollection(exec, form.elements().get());
        case FormLength:          return Number(form.length());
        case FormName:            return String(form.name());
        case FormAcceptCharset:   return String(form.acceptCharset());
        case FormAction:          return String(form.action());
        case FormEncType:         return String(form.enctype());
        case FormMethod:          return String(form.method());
        case FormTarget:          return String(form.target());
    }
    return Undefined();
}

ValueImp *HTMLElement::selectGetter(ExecState* exec, int token) const
{
    HTMLSelectElementImpl& select = *static_cast<HTMLSelectElementImpl*>(impl());
    switch (token) {
        case SelectType:            return String(select.type());
        case SelectSelectedIndex:   return Number(select.selectedIndex());
        case SelectValue:           return String(select.value());
        case SelectLength:          return Number(select.length());
        case SelectForm:            return getDOMNode(exec, select.form()); // type HTMLFormElement
        case SelectOptions:         return getSelectHTMLCollection(exec, select.optionsHTMLCollection().get(), &select); // type HTMLCollection
        case SelectDisabled:        return Boolean(select.disabled());
        case SelectMultiple:        return Boolean(select.multiple());
        case SelectName:            return String(select.name());
        case SelectSize:            return Number(select.size());
        case SelectTabIndex:        return Number(select.tabIndex());
    }
    return Undefined();
}

ValueImp *HTMLElement::optGroupGetter(ExecState* exec, int token) const
{
    HTMLOptGroupElementImpl& optgroup = *static_cast<HTMLOptGroupElementImpl*>(impl());
    switch (token) {
        case OptGroupDisabled:        return Boolean(optgroup.disabled());
        case OptGroupLabel:           return String(optgroup.label());
    }
    return Undefined();
}

ValueImp *HTMLElement::optionGetter(ExecState* exec, int token) const
{
    HTMLOptionElementImpl& option = *static_cast<HTMLOptionElementImpl*>(impl());
    switch (token) {
        case OptionForm:            return getDOMNode(exec,option.form()); // type HTMLFormElement
        case OptionDefaultSelected: return Boolean(option.defaultSelected());
        case OptionText:            return String(option.text());
        case OptionIndex:           return Number(option.index());
        case OptionDisabled:        return Boolean(option.disabled());
        case OptionLabel:           return String(option.label());
        case OptionSelected:        return Boolean(option.selected());
        case OptionValue:           return String(option.value());
    }
    return Undefined();
}

static ValueImp *getInputSelectionStart(HTMLInputElementImpl &input)
{
    if (input.canHaveSelection())
        return Number(input.selectionStart());
    return Undefined();
}

static ValueImp *getInputSelectionEnd(HTMLInputElementImpl &input)
{
    if (input.canHaveSelection())
        return Number(input.selectionEnd());
    return Undefined();
}

ValueImp *HTMLElement::inputGetter(ExecState* exec, int token) const
{
    HTMLInputElementImpl& input = *static_cast<HTMLInputElementImpl*>(impl());
    switch (token) {
        case InputDefaultValue:    return String(input.defaultValue());
        case InputDefaultChecked:  return Boolean(input.defaultChecked());
        case InputForm:            return getDOMNode(exec,input.form()); // type HTMLFormElement
        case InputAccept:          return String(input.accept());
        case InputAccessKey:       return String(input.accessKey());
        case InputAlign:           return String(input.align());
        case InputAlt:             return String(input.alt());
        case InputChecked:         return Boolean(input.checked());
        case InputDisabled:        return Boolean(input.disabled());
        case InputIndeterminate:   return Boolean(input.indeterminate());
        case InputMaxLength:       return Number(input.maxLength());
        case InputName:            return String(input.name());
        case InputReadOnly:        return Boolean(input.readOnly());
        case InputSelectionStart:  return getInputSelectionStart(input);
        case InputSelectionEnd:    return getInputSelectionEnd(input);
        case InputSize:            return Number(input.size());
        case InputSrc:             return String(input.src());
        case InputTabIndex:        return Number(input.tabIndex());
        case InputType:            return String(input.type());
        case InputUseMap:          return String(input.useMap());
        case InputValue:           return String(input.value());
    }
    return Undefined();
}

ValueImp *HTMLElement::textAreaGetter(ExecState* exec, int token) const
{
    HTMLTextAreaElementImpl& textarea = *static_cast<HTMLTextAreaElementImpl*>(impl());
    switch (token) {
        case TextAreaDefaultValue:    return String(textarea.defaultValue());
        case TextAreaForm:            return getDOMNode(exec,textarea.form()); // type HTMLFormElement
        case TextAreaAccessKey:       return String(textarea.accessKey());
        case TextAreaCols:            return Number(textarea.cols());
        case TextAreaDisabled:        return Boolean(textarea.disabled());
        case TextAreaName:            return String(textarea.name());
        case TextAreaReadOnly:        return Boolean(textarea.readOnly());
        case TextAreaRows:            return Number(textarea.rows());
        case TextAreaSelectionStart:  return Number(textarea.selectionStart());
        case TextAreaSelectionEnd:    return Number(textarea.selectionEnd());
        case TextAreaTabIndex:        return Number(textarea.tabIndex());
        case TextAreaType:            return String(textarea.type());
        case TextAreaValue:           return String(textarea.value());
    }
    return Undefined();
}

ValueImp *HTMLElement::buttonGetter(ExecState* exec, int token) const
{
    HTMLButtonElementImpl& button = *static_cast<HTMLButtonElementImpl*>(impl());
    switch (token) {
        case ButtonForm:            return getDOMNode(exec,button.form()); // type HTMLFormElement
        case ButtonAccessKey:       return String(button.accessKey());
        case ButtonDisabled:        return Boolean(button.disabled());
        case ButtonName:            return String(button.name());
        case ButtonTabIndex:        return Number(button.tabIndex());
        case ButtonType:            return String(button.type());
        case ButtonValue:           return String(button.value());
    }
    return Undefined();
}

ValueImp *HTMLElement::labelGetter(ExecState* exec, int token) const
{
    HTMLLabelElementImpl& label = *static_cast<HTMLLabelElementImpl*>(impl());
    switch (token) {
        case LabelForm:            return getDOMNode(exec,label.form()); // type HTMLFormElement
        case LabelAccessKey:       return String(label.accessKey());
        case LabelHtmlFor:         return String(label.htmlFor());
    }
    return Undefined();
}

ValueImp *HTMLElement::fieldSetGetter(ExecState* exec, int token) const
{
    HTMLFieldSetElementImpl& fieldSet = *static_cast<HTMLFieldSetElementImpl*>(impl());
    if (token == FieldSetForm)
        return getDOMNode(exec,fieldSet.form()); // type HTMLFormElement
    return Undefined();
}

ValueImp *HTMLElement::legendGetter(ExecState* exec, int token) const
{
    HTMLLegendElementImpl& legend = *static_cast<HTMLLegendElementImpl*>(impl());
    switch (token) {
        case LegendForm:            return getDOMNode(exec,legend.form()); // type HTMLFormElement
        case LegendAccessKey:       return String(legend.accessKey());
        case LegendAlign:           return String(legend.align());
    }
    return Undefined();
}

ValueImp *HTMLElement::uListGetter(ExecState* exec, int token) const
{
    HTMLUListElementImpl& uList = *static_cast<HTMLUListElementImpl*>(impl());
    switch (token) {
        case UListCompact:         return Boolean(uList.compact());
        case UListType:            return String(uList.type());
    }
    return Undefined();
}

ValueImp *HTMLElement::oListGetter(ExecState* exec, int token) const
{
    HTMLOListElementImpl& oList = *static_cast<HTMLOListElementImpl*>(impl());
    switch (token) {
        case OListCompact:         return Boolean(oList.compact());
        case OListStart:           return Number(oList.start());
        case OListType:            return String(oList.type());
    }
    return Undefined();
}

ValueImp *HTMLElement::dListGetter(ExecState* exec, int token) const
{
    HTMLDListElementImpl& dList = *static_cast<HTMLDListElementImpl*>(impl());
    if (token == DListCompact)
        return Boolean(dList.compact());
    return Undefined();
}

ValueImp *HTMLElement::dirGetter(ExecState* exec, int token) const
{
    HTMLDirectoryElementImpl& dir = *static_cast<HTMLDirectoryElementImpl*>(impl());
    if (token == DirectoryCompact)
        return Boolean(dir.compact());
    return Undefined();
}

ValueImp *HTMLElement::menuGetter(ExecState* exec, int token) const
{
    HTMLMenuElementImpl& menu = *static_cast<HTMLMenuElementImpl*>(impl());
    if (token == MenuCompact)
        return Boolean(menu.compact());
    return Undefined();
}

ValueImp *HTMLElement::liGetter(ExecState* exec, int token) const
{
    HTMLLIElementImpl& li = *static_cast<HTMLLIElementImpl*>(impl());
    switch (token) {
        case LIType:            return String(li.type());
        case LIValue:           return Number(li.value());
    }
    return Undefined();
}

ValueImp *HTMLElement::divGetter(ExecState* exec, int token) const
{
    HTMLDivElementImpl& div = *static_cast<HTMLDivElementImpl*>(impl());
    if (token == DivAlign)
        return String(div.align());
    return Undefined();
}

ValueImp *HTMLElement::paragraphGetter(ExecState* exec, int token) const
{
    HTMLParagraphElementImpl& p = *static_cast<HTMLParagraphElementImpl*>(impl());
    if (token == ParagraphAlign)
        return String(p.align());
    return Undefined();
}

ValueImp *HTMLElement::headingGetter(ExecState* exec, int token) const
{
    HTMLHeadingElementImpl& h = *static_cast<HTMLHeadingElementImpl*>(impl());
    if (token == HeadingAlign)
        return String(h.align());
    return Undefined();
}

ValueImp *HTMLElement::blockQuoteGetter(ExecState* exec, int token) const
{
    HTMLBlockquoteElementImpl& blockQuote = *static_cast<HTMLBlockquoteElementImpl*>(impl());
    if (token == BlockQuoteCite)
        return String(blockQuote.cite());
    return Undefined();
}

ValueImp *HTMLElement::quoteGetter(ExecState* exec, int token) const
{
    HTMLQuoteElementImpl& quote = *static_cast<HTMLQuoteElementImpl*>(impl());
    if (token == QuoteCite)
        return String(quote.cite());
    return Undefined();
}

ValueImp *HTMLElement::preGetter(ExecState* exec, int token) const
{
    // FIXME: Add support for 'wrap' when white-space: pre-wrap is implemented.
    HTMLPreElementImpl& pre = *static_cast<HTMLPreElementImpl*>(impl());
    if (token == PreWidth)
        return Number(pre.width());
    if (token == PreWrap)
        return Boolean(pre.wrap());
    return Undefined();
}

ValueImp *HTMLElement::brGetter(ExecState* exec, int token) const
{
    HTMLBRElementImpl& br = *static_cast<HTMLBRElementImpl*>(impl());
    if (token == BRClear)
        return String(br.clear());
    return Undefined();
}

ValueImp *HTMLElement::baseFontGetter(ExecState* exec, int token) const
{
    HTMLBaseFontElementImpl& baseFont = *static_cast<HTMLBaseFontElementImpl*>(impl());
    switch (token) {
        case BaseFontColor:           return String(baseFont.color());
        case BaseFontFace:            return String(baseFont.face());
        case BaseFontSize:            return String(baseFont.size());
    }
    return Undefined();
}

ValueImp *HTMLElement::fontGetter(ExecState* exec, int token) const
{
    HTMLFontElementImpl& font = *static_cast<HTMLFontElementImpl*>(impl());
    switch (token) {
        case FontColor:           return String(font.color());
        case FontFace:            return String(font.face());
        case FontSize:            return String(font.size());
    }
    return Undefined();
}

ValueImp *HTMLElement::hrGetter(ExecState* exec, int token) const
{
    HTMLHRElementImpl& hr = *static_cast<HTMLHRElementImpl*>(impl());
    switch (token) {
        case HRAlign:           return String(hr.align());
        case HRNoShade:         return Boolean(hr.noShade());
        case HRSize:            return String(hr.size());
        case HRWidth:           return String(hr.width());
    }
    return Undefined();
}

ValueImp *HTMLElement::modGetter(ExecState* exec, int token) const
{
    HTMLModElementImpl& mod = *static_cast<HTMLModElementImpl*>(impl());
    switch (token) {
        case ModCite:            return String(mod.cite());
        case ModDateTime:        return String(mod.dateTime());
    }
    return Undefined();
}

ValueImp *HTMLElement::anchorGetter(ExecState* exec, int token) const
{
    HTMLAnchorElementImpl& anchor = *static_cast<HTMLAnchorElementImpl*>(impl());
    switch (token) {
        case AnchorAccessKey:       return String(anchor.accessKey());
        case AnchorCharset:         return String(anchor.charset());
        case AnchorCoords:          return String(anchor.coords());
        case AnchorHref:            return String(anchor.href());
        case AnchorHrefLang:        return String(anchor.hreflang());
        case AnchorHash:            return String('#'+KURL(anchor.href().qstring()).ref());
        case AnchorHost:            return String(KURL(anchor.href().qstring()).host());
        case AnchorHostname: {
            KURL url(anchor.href().qstring());
            kdDebug(6070) << "anchor::hostname uses:" <<url.url()<<endl;
            if (url.port()==0)
                return String(url.host());
            else
                return String(url.host() + ":" + QString::number(url.port()));
        }
        case AnchorPathName:        return String(KURL(anchor.href().qstring()).path());
        case AnchorPort:            return String(QString::number(KURL(anchor.href().qstring()).port()));
        case AnchorProtocol:        return String(KURL(anchor.href().qstring()).protocol()+":");
        case AnchorSearch:          return String(KURL(anchor.href().qstring()).query());
        case AnchorName:            return String(anchor.name());
        case AnchorRel:             return String(anchor.rel());
        case AnchorRev:             return String(anchor.rev());
        case AnchorShape:           return String(anchor.shape());
        case AnchorTabIndex:        return Number(anchor.tabIndex());
        case AnchorTarget:          return String(anchor.target());
        case AnchorType:            return String(anchor.type());
        case AnchorText:
            if (DocumentImpl* doc = anchor.getDocument())
                doc->updateLayoutIgnorePendingStylesheets();
            return String(anchor.innerText());
    }
    return Undefined();
}

ValueImp *HTMLElement::imageGetter(ExecState* exec, int token) const
{
    HTMLImageElementImpl& image = *static_cast<HTMLImageElementImpl*>(impl());
    switch (token) {
        case ImageName:            return String(image.name());
        case ImageAlign:           return String(image.align());
        case ImageAlt:             return String(image.alt());
        case ImageBorder:          return Number(image.border());
        case ImageHeight:          return Number(image.height(true));
        case ImageHspace:          return Number(image.hspace());
        case ImageIsMap:           return Boolean(image.isMap());
        case ImageLongDesc:        return String(image.longDesc());
        case ImageSrc:             return String(image.src());
        case ImageUseMap:          return String(image.useMap());
        case ImageVspace:          return Number(image.vspace());
        case ImageWidth:           return Number(image.width(true));
        case ImageX:               return Number(image.x());
        case ImageY:               return Number(image.y());
    }
    return Undefined();
}

ValueImp *HTMLElement::objectGetter(ExecState* exec, int token) const
{
    HTMLObjectElementImpl& object = *static_cast<HTMLObjectElementImpl*>(impl());
    switch (token) {
        case ObjectForm:            return getDOMNode(exec,object.form()); // type HTMLFormElement
        case ObjectCode:            return String(object.code());
        case ObjectAlign:           return String(object.align());
        case ObjectArchive:         return String(object.archive());
        case ObjectBorder:          return String(object.border());
        case ObjectCodeBase:        return String(object.codeBase());
        case ObjectCodeType:        return String(object.codeType());
        case ObjectContentDocument: return checkNodeSecurity(exec,object.contentDocument()) ? 
                                           getDOMNode(exec, object.contentDocument()) : Undefined();
        case ObjectData:            return String(object.data());
        case ObjectDeclare:         return Boolean(object.declare());
        case ObjectHeight:          return String(object.height());
        case ObjectHspace:          return String(object.hspace());
        case ObjectName:            return String(object.name());
        case ObjectStandby:         return String(object.standby());
        case ObjectTabIndex:        return Number(object.tabIndex());
        case ObjectType:            return String(object.type());
        case ObjectUseMap:          return String(object.useMap());
        case ObjectVspace:          return String(object.vspace());
        case ObjectWidth:           return String(object.width());
    }
    return Undefined();
}

ValueImp *HTMLElement::paramGetter(ExecState* exec, int token) const
{
    HTMLParamElementImpl& param = *static_cast<HTMLParamElementImpl*>(impl());
    switch (token) {
        case ParamName:            return String(param.name());
        case ParamType:            return String(param.type());
        case ParamValue:           return String(param.value());
        case ParamValueType:       return String(param.valueType());
    }
    return Undefined();
}

ValueImp *HTMLElement::appletGetter(ExecState* exec, int token) const
{
    HTMLAppletElementImpl& applet = *static_cast<HTMLAppletElementImpl*>(impl());
    switch (token) {
        case AppletAlign:           return String(applet.align());
        case AppletAlt:             return String(applet.alt());
        case AppletArchive:         return String(applet.archive());
        case AppletCode:            return String(applet.code());
        case AppletCodeBase:        return String(applet.codeBase());
        case AppletHeight:          return String(applet.height());
        case AppletHspace:          return String(applet.hspace());
        case AppletName:            return String(applet.name());
        case AppletObject:          return String(applet.object());
        case AppletVspace:          return String(applet.vspace());
        case AppletWidth:           return String(applet.width());
    }
    return Undefined();
}

ValueImp *HTMLElement::mapGetter(ExecState* exec, int token) const
{
    HTMLMapElementImpl& map = *static_cast<HTMLMapElementImpl*>(impl());
    switch (token) {
        case MapAreas:           return getHTMLCollection(exec, map.areas().get()); // type HTMLCollection
        case MapName:            return String(map.name());
    }
    return Undefined();
}

ValueImp *HTMLElement::areaGetter(ExecState* exec, int token) const
{
    HTMLAreaElementImpl& area = *static_cast<HTMLAreaElementImpl*>(impl());
    switch (token) {
        case AreaAccessKey:       return String(area.accessKey());
        case AreaAlt:             return String(area.alt());
        case AreaCoords:          return String(area.coords());
        case AreaHref:            return String(area.href());
        case AreaHash:            return String('#'+KURL(area.href().qstring()).ref());
        case AreaHost:            return String(KURL(area.href().qstring()).host());
        case AreaHostName: {
            KURL url(area.href().qstring());
            kdDebug(6070) << "link::hostname uses:" <<url.url()<<endl;
            if (url.port()==0)
                return String(url.host());
            else
                return String(url.host() + ":" + QString::number(url.port()));
        }
        case AreaPathName:        return String(KURL(area.href().qstring()).path());
        case AreaPort:            return String(QString::number(KURL(area.href().qstring()).port()));
        case AreaProtocol:        return String(KURL(area.href().qstring()).protocol()+":");
        case AreaSearch:          return String(KURL(area.href().qstring()).query());
        case AreaNoHref:          return Boolean(area.noHref());
        case AreaShape:           return String(area.shape());
        case AreaTabIndex:        return Number(area.tabIndex());
        case AreaTarget:          return String(area.target());
    }
    return Undefined();
}

ValueImp *HTMLElement::scriptGetter(ExecState* exec, int token) const
{
    HTMLScriptElementImpl& script = *static_cast<HTMLScriptElementImpl*>(impl());
    switch (token) {
        case ScriptText:            return String(script.text());
        case ScriptHtmlFor:         return String(script.htmlFor());
        case ScriptEvent:           return String(script.event());
        case ScriptCharset:         return String(script.charset());
        case ScriptDefer:           return Boolean(script.defer());
        case ScriptSrc:             return String(script.src());
        case ScriptType:            return String(script.type());
    }
    return Undefined();
}

ValueImp *HTMLElement::tableGetter(ExecState* exec, int token) const
{
    HTMLTableElementImpl& table = *static_cast<HTMLTableElementImpl*>(impl());
    switch (token) {
        case TableCaption:         return getDOMNode(exec,table.caption()); // type HTMLTableCaptionElement
        case TableTHead:           return getDOMNode(exec,table.tHead()); // type HTMLTableSectionElement
        case TableTFoot:           return getDOMNode(exec,table.tFoot()); // type HTMLTableSectionElement
        case TableRows:            return getHTMLCollection(exec, table.rows().get()); // type HTMLCollection
        case TableTBodies:         return getHTMLCollection(exec, table.tBodies().get()); // type HTMLCollection
        case TableAlign:           return String(table.align());
        case TableBgColor:         return String(table.bgColor());
        case TableBorder:          return String(table.border());
        case TableCellPadding:     return String(table.cellPadding());
        case TableCellSpacing:     return String(table.cellSpacing());
        case TableFrame:           return String(table.frame());
        case TableRules:           return String(table.rules());
        case TableSummary:         return String(table.summary());
        case TableWidth:           return String(table.width());
    }
    return Undefined();
}

ValueImp *HTMLElement::tableCaptionGetter(ExecState* exec, int token) const
{
    HTMLTableCaptionElementImpl& tableCaption = *static_cast<HTMLTableCaptionElementImpl*>(impl());
    if (token == TableCaptionAlign)
        return String(tableCaption.align());
    return Undefined();
}

ValueImp *HTMLElement::tableColGetter(ExecState* exec, int token) const
{
    HTMLTableColElementImpl& tableCol = *static_cast<HTMLTableColElementImpl*>(impl());
    switch (token) {
        case TableColAlign:           return String(tableCol.align());
        case TableColCh:              return String(tableCol.ch());
        case TableColChOff:           return String(tableCol.chOff());
        case TableColSpan:            return Number(tableCol.span());
        case TableColVAlign:          return String(tableCol.vAlign());
        case TableColWidth:           return String(tableCol.width());
    }
    return Undefined();
}

ValueImp *HTMLElement::tableSectionGetter(ExecState* exec, int token) const
{
    HTMLTableSectionElementImpl& tableSection = *static_cast<HTMLTableSectionElementImpl*>(impl());
    switch (token) {
        case TableSectionAlign:           return String(tableSection.align());
        case TableSectionCh:              return String(tableSection.ch());
        case TableSectionChOff:           return String(tableSection.chOff());
        case TableSectionVAlign:          return String(tableSection.vAlign());
        case TableSectionRows:            return getHTMLCollection(exec, tableSection.rows().get()); // type HTMLCollection
    }
    return Undefined();
}

ValueImp *HTMLElement::tableRowGetter(ExecState* exec, int token) const
{
    HTMLTableRowElementImpl& tableRow = *static_cast<HTMLTableRowElementImpl*>(impl());
    switch (token) {
        case TableRowRowIndex:        return Number(tableRow.rowIndex());
        case TableRowSectionRowIndex: return Number(tableRow.sectionRowIndex());
        case TableRowCells:           return getHTMLCollection(exec, tableRow.cells().get()); // type HTMLCollection
        case TableRowAlign:           return String(tableRow.align());
        case TableRowBgColor:         return String(tableRow.bgColor());
        case TableRowCh:              return String(tableRow.ch());
        case TableRowChOff:           return String(tableRow.chOff());
        case TableRowVAlign:          return String(tableRow.vAlign());
    }
    return Undefined();
}

ValueImp *HTMLElement::tableCellGetter(ExecState* exec, int token) const
{
    HTMLTableCellElementImpl& tableCell = *static_cast<HTMLTableCellElementImpl*>(impl());
    switch (token) {
        case TableCellCellIndex:       return Number(tableCell.cellIndex());
        case TableCellAbbr:            return String(tableCell.abbr());
        case TableCellAlign:           return String(tableCell.align());
        case TableCellAxis:            return String(tableCell.axis());
        case TableCellBgColor:         return String(tableCell.bgColor());
        case TableCellCh:              return String(tableCell.ch());
        case TableCellChOff:           return String(tableCell.chOff());
        case TableCellColSpan:         return Number(tableCell.colSpan());
        case TableCellHeaders:         return String(tableCell.headers());
        case TableCellHeight:          return String(tableCell.height());
        case TableCellNoWrap:          return Boolean(tableCell.noWrap());
        case TableCellRowSpan:         return Number(tableCell.rowSpan());
        case TableCellScope:           return String(tableCell.scope());
        case TableCellVAlign:          return String(tableCell.vAlign());
        case TableCellWidth:           return String(tableCell.width());
    }
    return Undefined();
}

ValueImp *HTMLElement::frameSetGetter(ExecState* exec, int token) const
{
    HTMLFrameSetElementImpl& frameSet = *static_cast<HTMLFrameSetElementImpl*>(impl());
    switch (token) {
        case FrameSetCols:            return String(frameSet.cols());
        case FrameSetRows:            return String(frameSet.rows());
    }
    return Undefined();
}

ValueImp *HTMLElement::frameGetter(ExecState* exec, int token) const
{
    HTMLFrameElementImpl& frameElement = *static_cast<HTMLFrameElementImpl*>(impl());
    switch (token) {
        case FrameContentDocument: return checkNodeSecurity(exec,frameElement.contentDocument()) ? 
                                          getDOMNode(exec, frameElement.contentDocument()) : Undefined();
        case FrameContentWindow:   return checkNodeSecurity(exec,frameElement.contentDocument())
                                        ? Window::retrieve(frameElement.contentPart())
                                        : Undefined();
        case FrameFrameBorder:     return String(frameElement.frameBorder());
        case FrameLongDesc:        return String(frameElement.longDesc());
        case FrameMarginHeight:    return String(frameElement.marginHeight());
        case FrameMarginWidth:     return String(frameElement.marginWidth());
        case FrameName:            return String(frameElement.name());
        case FrameNoResize:        return Boolean(frameElement.noResize());
        case FrameScrolling:       return String(frameElement.scrolling());
        case FrameSrc:
        case FrameLocation:        return String(frameElement.src());
    }
    return Undefined();
}

ValueImp *HTMLElement::iFrameGetter(ExecState* exec, int token) const
{
    HTMLIFrameElementImpl& iFrame = *static_cast<HTMLIFrameElementImpl*>(impl());
    switch (token) {
        case IFrameAlign:                return String(iFrame.align());
          // ### security check ?
        case IFrameDocument: // non-standard, mapped to contentDocument
        case IFrameContentDocument: return checkNodeSecurity(exec,iFrame.contentDocument()) ? 
                                      getDOMNode(exec, iFrame.contentDocument()) : Undefined();
        case IFrameContentWindow:	return checkNodeSecurity(exec,iFrame.contentDocument()) 
                                        ? Window::retrieve(iFrame.contentPart())
                                        : Undefined();
        case IFrameFrameBorder:     return String(iFrame.frameBorder());
        case IFrameHeight:          return String(iFrame.height());
        case IFrameLongDesc:        return String(iFrame.longDesc());
        case IFrameMarginHeight:    return String(iFrame.marginHeight());
        case IFrameMarginWidth:     return String(iFrame.marginWidth());
        case IFrameName:            return String(iFrame.name());
        case IFrameScrolling:       return String(iFrame.scrolling());
        case IFrameSrc:             return String(iFrame.src());
        case IFrameWidth:           return String(iFrame.width());
    }
    return Undefined();
}

ValueImp *HTMLElement::marqueeGetter(ExecState* exec, int token) const
{
    // FIXME: Find out what WinIE exposes as properties and implement this.
    return Undefined();
}

ValueImp *HTMLElement::getValueProperty(ExecState *exec, int token) const
{
    // Check our set of generic properties first.
    HTMLElementImpl &element = *static_cast<HTMLElementImpl *>(impl());
    switch (token) {
        case ElementId:
        // iht.com relies on this value being "" when no id is present.  Other browsers do this as well.
        // So we use String() instead of String() here.
        return String(element.id());
        case ElementTitle:
            return String(element.title());
        case ElementLang:
            return String(element.lang());
        case ElementDir:
            return String(element.dir());
        case ElementClassName:
            return String(element.className());
        case ElementInnerHTML:
            return String(element.innerHTML());
        case ElementInnerText:
            if (DocumentImpl* doc = impl()->getDocument())
                doc->updateLayoutIgnorePendingStylesheets();
            return String(element.innerText());
        case ElementOuterHTML:
            return String(element.outerHTML());
        case ElementOuterText:
            return String(element.outerText());
        case ElementDocument:
            return getDOMNode(exec,element.ownerDocument());
        case ElementChildren:
            return getHTMLCollection(exec, element.children().get());
        case ElementContentEditable:
            return String(element.contentEditable());
        case ElementIsContentEditable:
            return Boolean(element.isContentEditable());
    }

    // Now check the properties specific to our element type.
    const Accessors* info = getSetInfo();
    if (info && info->m_getter)
        return (this->*(info->m_getter))(exec, token);
    return Undefined();
}

UString KJS::HTMLElement::toString(ExecState *exec) const
{
    if (impl()->hasTagName(aTag))
        return UString(static_cast<const HTMLAnchorElementImpl *>(impl())->href());
    else
        return DOMElement::toString(exec);
}

static HTMLFormElementImpl *getForm(HTMLElementImpl *element)
{
    if (element->isGenericFormElement())
        return static_cast<HTMLGenericFormElementImpl *>(element)->form();
    if (element->hasTagName(labelTag))
        return static_cast<HTMLLabelElementImpl *>(element)->form();
    if (element->hasTagName(objectTag))
        return static_cast<HTMLObjectElementImpl *>(element)->form();

    return 0;
}

void KJS::HTMLElement::pushEventHandlerScope(ExecState *exec, ScopeChain &scope) const
{
  HTMLElementImpl *element = static_cast<HTMLElementImpl *>(impl());

  // The document is put on first, fall back to searching it only after the element and form.
  scope.push(static_cast<ObjectImp *>(getDOMNode(exec, element->ownerDocument())));

  // The form is next, searched before the document, but after the element itself.
  
  // First try to obtain the form from the element itself.  We do this to deal with
  // the malformed case where <form>s aren't in our parent chain (e.g., when they were inside 
  // <table> or <tbody>.
  HTMLFormElementImpl *form = getForm(element);
  if (form)
    scope.push(static_cast<ObjectImp *>(getDOMNode(exec, form)));
  else {
    NodeImpl *form = element->parentNode();
    while (form && !form->hasTagName(formTag))
      form = form->parentNode();
    
    if (form)
      scope.push(static_cast<ObjectImp *>(getDOMNode(exec, form)));
  }
  
  // The element is on top, searched first.
  scope.push(static_cast<ObjectImp *>(getDOMNode(exec, element)));
}

HTMLElementFunction::HTMLElementFunction(ExecState *exec, int i, int len)
  : DOMFunction(), id(i)
{
  put(exec,lengthPropertyName,Number(len),DontDelete|ReadOnly|DontEnum);
}

ValueImp *KJS::HTMLElementFunction::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
    if (!thisObj->inherits(&KJS::HTMLElement::info))
        return throwError(exec, TypeError);
    kdDebug() << "KJS::HTMLElementFunction::tryCall " << endl;
    DOMExceptionTranslator exception(exec);
    HTMLElementImpl &element = *static_cast<HTMLElementImpl *>(static_cast<HTMLElement *>(thisObj)->impl());

    if (element.hasLocalName(formTag)) {
        HTMLFormElementImpl &form = static_cast<HTMLFormElementImpl &>(element);
        if (id == KJS::HTMLElement::FormSubmit) {
            form.submit();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::FormReset) {
            form.reset();
            return Undefined();
        }
    }
    else if (element.hasLocalName(selectTag)) {
        HTMLSelectElementImpl &select = static_cast<HTMLSelectElementImpl &>(element);
        if (id == KJS::HTMLElement::SelectAdd) {
            select.add(toHTMLElement(args[0]), toHTMLElement(args[1]), exception);
            return Undefined();
        }
        else if (id == KJS::HTMLElement::SelectRemove) {
            select.remove(int(args[0]->toNumber(exec)));
            return Undefined();
        }
        else if (id == KJS::HTMLElement::SelectBlur) {
            select.blur();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::SelectFocus) {
            select.focus();
            return Undefined();
        }
    }
    else if (element.hasLocalName(inputTag)) {
        HTMLInputElementImpl &input = static_cast<HTMLInputElementImpl &>(element);
        if (id == KJS::HTMLElement::InputBlur) {
            input.blur();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::InputFocus) {
            input.focus();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::InputSelect) {
            input.select();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::InputClick) {
            input.click();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::InputSetSelectionRange) {
            input.setSelectionRange(args[0]->toInt32(exec), args[1]->toInt32(exec));
            return Undefined();
        }
    }
    else if (element.hasLocalName(buttonTag)) {
        HTMLButtonElementImpl &button = static_cast<HTMLButtonElementImpl &>(element);
        if (id == KJS::HTMLElement::ButtonBlur) {
            button.blur();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::ButtonFocus) {
            button.focus();
            return Undefined();
        }
    }
    else if (element.hasLocalName(labelTag)) {
        HTMLLabelElementImpl &label = static_cast<HTMLLabelElementImpl &>(element);
        if (id == KJS::HTMLElement::LabelFocus) {
            label.focus();
            return Undefined();
        }
    }
    else if (element.hasLocalName(legendTag)) {
        HTMLLegendElementImpl &legend = static_cast<HTMLLegendElementImpl &>(element);
        if (id == KJS::HTMLElement::LegendFocus) {
            legend.focus();
            return Undefined();
        }
    }
    else if (element.hasLocalName(textareaTag)) {
        HTMLTextAreaElementImpl &textarea = static_cast<HTMLTextAreaElementImpl &>(element);
        if (id == KJS::HTMLElement::TextAreaBlur) {
            textarea.blur();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::TextAreaFocus) {
            textarea.focus();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::TextAreaSelect) {
            textarea.select();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::TextAreaSetSelectionRange) {
            textarea.setSelectionRange(args[0]->toInt32(exec), args[1]->toInt32(exec));
            return Undefined();
        }
    }
    else if (element.hasLocalName(aTag)) {
        HTMLAnchorElementImpl &anchor = static_cast<HTMLAnchorElementImpl &>(element);
        if (id == KJS::HTMLElement::AnchorBlur) {
            anchor.blur();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::AnchorFocus) {
            anchor.focus();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::AnchorToString)
            return String(thisObj->toString(exec));
    }
    else if (element.hasLocalName(tableTag)) {
        HTMLTableElementImpl &table = static_cast<HTMLTableElementImpl &>(element);
        if (id == KJS::HTMLElement::TableCreateTHead)
            return getDOMNode(exec,table.createTHead());
        else if (id == KJS::HTMLElement::TableDeleteTHead) {
            table.deleteTHead();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::TableCreateTFoot)
            return getDOMNode(exec,table.createTFoot());
        else if (id == KJS::HTMLElement::TableDeleteTFoot) {
            table.deleteTFoot();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::TableCreateCaption)
            return getDOMNode(exec,table.createCaption());
        else if (id == KJS::HTMLElement::TableDeleteCaption) {
            table.deleteCaption();
            return Undefined();
        }
        else if (id == KJS::HTMLElement::TableInsertRow)
            return getDOMNode(exec,table.insertRow(args[0]->toInt32(exec), exception));
        else if (id == KJS::HTMLElement::TableDeleteRow) {
            table.deleteRow(args[0]->toInt32(exec), exception);
            return Undefined();
        }
    }
    else if (element.hasLocalName(theadTag) ||
             element.hasLocalName(tbodyTag) ||
             element.hasLocalName(tfootTag)) {
        HTMLTableSectionElementImpl &tableSection = static_cast<HTMLTableSectionElementImpl &>(element);
        if (id == KJS::HTMLElement::TableSectionInsertRow)
            return getDOMNode(exec, tableSection.insertRow(args[0]->toInt32(exec), exception));
        else if (id == KJS::HTMLElement::TableSectionDeleteRow) {
            tableSection.deleteRow(args[0]->toInt32(exec), exception);
            return Undefined();
        }
    }
    else if (element.hasLocalName(trTag)) {
        HTMLTableRowElementImpl &tableRow = static_cast<HTMLTableRowElementImpl &>(element);
        if (id == KJS::HTMLElement::TableRowInsertCell)
            return getDOMNode(exec,tableRow.insertCell(args[0]->toInt32(exec), exception));
        else if (id == KJS::HTMLElement::TableRowDeleteCell) {
            tableRow.deleteCell(args[0]->toInt32(exec), exception);
            return Undefined();
        }
    }
    else if (element.hasLocalName(marqueeTag)) {
        if (id == KJS::HTMLElement::MarqueeStart && element.renderer() && 
            element.renderer()->layer() &&
            element.renderer()->layer()->marquee()) {
            element.renderer()->layer()->marquee()->start();
            return Undefined();
        }
        if (id == KJS::HTMLElement::MarqueeStop && element.renderer() && 
            element.renderer()->layer() &&
            element.renderer()->layer()->marquee()) {
            element.renderer()->layer()->marquee()->stop();
            return Undefined();
        }
    }
    else if (element.hasLocalName(canvasTag)) {
        if (id == KJS::HTMLElement::GetContext) {
            if (args.size() == 0 || (args.size() == 1 && args[0]->toString(exec).qstring().lower() == "2d")) {
                return new Context2D(&element);
            }
            return Undefined();
        }
    }

    return Undefined();
}

void KJS::HTMLElement::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
#ifdef KJS_VERBOSE
    DOM::DOMString str = value.isNull() ? DOM::DOMString() : value->toString(exec).domString();
#endif
    HTMLElementImpl &element = *static_cast<HTMLElementImpl *>(impl());
#ifdef KJS_VERBOSE
    kdDebug(6070) << "KJS::HTMLElement::tryPut " << propertyName.qstring()
                  << " thisTag=" << element.tagName().qstring()
                  << " str=" << str.qstring() << endl;
#endif
    // First look at dynamic properties
    if (element.hasLocalName(selectTag)) {
        HTMLSelectElementImpl &select = static_cast<HTMLSelectElementImpl &>(element);
        bool ok;
        /*uint u =*/ propertyName.toUInt32(&ok);
        if (ok) {
            ObjectImp *coll = static_cast<ObjectImp *>(getSelectHTMLCollection(exec, select.optionsHTMLCollection().get(), &select));
            coll->put(exec,propertyName,value);
            return;
        }
    }
    else if (element.hasLocalName(embedTag) || element.hasLocalName(objectTag) || element.hasLocalName(appletTag)) {
	if (ValueImp *runtimeObject = getRuntimeObject(exec, &element)) {
	    ObjectImp *imp = static_cast<ObjectImp *>(runtimeObject);
	    if (imp->canPut(exec, propertyName))
		return imp->put(exec, propertyName, value);
	}
    }

    const HashTable* table = classInfo()->propHashTable; // get the right hashtable
    const HashEntry* entry = Lookup::findEntry(table, propertyName);
    if (entry) {
        if (entry->attr & Function) { // function: put as override property
            ObjectImp::put(exec, propertyName, value, attr);
            return;
        }
        else if (!(entry->attr & ReadOnly)) { // let lookupPut print the warning if read-only
            putValueProperty(exec, entry->value, value, attr);
            return;
        }
    }

    lookupPut<KJS::HTMLElement, DOMElement>(exec, propertyName, value, attr, &HTMLElementTable, this);
}

void HTMLElement::htmlSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLHeadElementImpl &head = *static_cast<HTMLHeadElementImpl*>(impl());
    if (token == HeadProfile) 
        head.setProfile(str);
}

void HTMLElement::headSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLHeadElementImpl &head = *static_cast<HTMLHeadElementImpl*>(impl());
    if (token == HeadProfile) 
        head.setProfile(str);
}

void HTMLElement::linkSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLLinkElementImpl &link = *static_cast<HTMLLinkElementImpl*>(impl());
    switch (token) {
        case LinkDisabled:        { link.setDisabled(value->toBoolean(exec)); return; }
        case LinkCharset:         { link.setCharset(str); return; }
        case LinkHref:            { link.setHref(str); return; }
        case LinkHrefLang:        { link.setHreflang(str); return; }
        case LinkMedia:           { link.setMedia(str); return; }
        case LinkRel:             { link.setRel(str); return; }
        case LinkRev:             { link.setRev(str); return; }
        case LinkTarget:          { link.setTarget(str); return; }
        case LinkType:            { link.setType(str); return; }
    }
}

void HTMLElement::titleSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
     HTMLTitleElementImpl& title = *static_cast<HTMLTitleElementImpl*>(impl());
     if (token == TitleText)
        title.setText(str);
}

void HTMLElement::metaSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLMetaElementImpl& meta = *static_cast<HTMLMetaElementImpl*>(impl());
    switch (token) {
        case MetaContent:         { meta.setContent(str); return; }
        case MetaHttpEquiv:       { meta.setHttpEquiv(str); return; }
        case MetaName:            { meta.setName(str); return; }
        case MetaScheme:          { meta.setScheme(str); return; }
    }
}

void HTMLElement::baseSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLBaseElementImpl& base = *static_cast<HTMLBaseElementImpl*>(impl());
    switch (token) {
        case BaseHref:            { base.setHref(str); return; }
        case BaseTarget:          { base.setTarget(str); return; }
    }
}

void HTMLElement::isIndexSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLIsIndexElementImpl& isindex = *static_cast<HTMLIsIndexElementImpl*>(impl());
    if (token == IsIndexPrompt)
        isindex.setPrompt(str);
}

void HTMLElement::styleSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLStyleElementImpl& style = *static_cast<HTMLStyleElementImpl*>(impl());
    switch (token) {
        case StyleDisabled:        { style.setDisabled(value->toBoolean(exec)); return; }
        case StyleMedia:           { style.setMedia(str); return; }
        case StyleType:            { style.setType(str); return; }
    }
}

void HTMLElement::bodySetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLBodyElementImpl& body = *static_cast<HTMLBodyElementImpl*>(impl());
    switch (token) {
        case BodyALink:           { body.setALink(str); return; }
        case BodyBackground:      { body.setBackground(str); return; }
        case BodyBgColor:         { body.setBgColor(str); return; }
        case BodyLink:            { body.setLink(str); return; }
        case BodyText:            { body.setText(str); return; }
        case BodyVLink:           { body.setVLink(str); return; }
        case BodyScrollLeft:
        case BodyScrollTop: {
            QScrollView* sview = body.ownerDocument()->view();
            if (sview) {
                // Update the document's layout before we compute these attributes.
                if (DocumentImpl* doc = body.getDocument())
                    doc->updateLayoutIgnorePendingStylesheets();
                if (token == BodyScrollLeft)
                    sview->setContentsPos(value->toInt32(exec), sview->contentsY());
                else
                    sview->setContentsPos(sview->contentsX(), value->toInt32(exec));
            }
            return;
        }
    }
}

void HTMLElement::formSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLFormElementImpl& form = *static_cast<HTMLFormElementImpl*>(impl());
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

void HTMLElement::selectSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLSelectElementImpl& select = *static_cast<HTMLSelectElementImpl*>(impl());
    switch (token) {
        // read-only: type
        case SelectSelectedIndex:   { select.setSelectedIndex(value->toInt32(exec)); return; }
        case SelectValue:           { select.setValue(str); return; }
        case SelectLength:          { // read-only according to the NS spec, but webpages need it writeable
                                        ObjectImp *coll = static_cast<ObjectImp *>(getSelectHTMLCollection(exec, select.optionsHTMLCollection().get(), &select));
                                        coll->put(exec,lengthPropertyName,value);
                                        return;
                                    }
        // read-only: form
        // read-only: options
        case SelectDisabled:        { select.setDisabled(value->toBoolean(exec)); return; }
        case SelectMultiple:        { select.setMultiple(value->toBoolean(exec)); return; }
        case SelectName:            { select.setName(str); return; }
        case SelectSize:            { select.setSize(value->toInt32(exec)); return; }
        case SelectTabIndex:        { select.setTabIndex(value->toInt32(exec)); return; }
    }
}

void HTMLElement::optGroupSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLOptGroupElementImpl& optgroup = *static_cast<HTMLOptGroupElementImpl*>(impl());
    switch (token) {
        case OptGroupDisabled:        { optgroup.setDisabled(value->toBoolean(exec)); return; }
        case OptGroupLabel:           { optgroup.setLabel(str); return; }
    }
}

void HTMLElement::optionSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    DOMExceptionTranslator exception(exec);
    HTMLOptionElementImpl& option = *static_cast<HTMLOptionElementImpl*>(impl());
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

void HTMLElement::inputSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLInputElementImpl& input = *static_cast<HTMLInputElementImpl*>(impl());
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
        case InputName:            { input.setName(str); return; }
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

void HTMLElement::textAreaSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLTextAreaElementImpl& textarea = *static_cast<HTMLTextAreaElementImpl*>(impl());
    switch (token) {
        case TextAreaDefaultValue:    { textarea.setDefaultValue(str); return; }
        // read-only: form
        case TextAreaAccessKey:       { textarea.setAccessKey(str); return; }
        case TextAreaCols:            { textarea.setCols(value->toInt32(exec)); return; }
        case TextAreaDisabled:        { textarea.setDisabled(value->toBoolean(exec)); return; }
        case TextAreaName:            { textarea.setName(str); return; }
        case TextAreaReadOnly:        { textarea.setReadOnly(value->toBoolean(exec)); return; }
        case TextAreaRows:            { textarea.setRows(value->toInt32(exec)); return; }
        case TextAreaSelectionStart:  { textarea.setSelectionStart(value->toInt32(exec)); return; }
        case TextAreaSelectionEnd:    { textarea.setSelectionEnd(value->toInt32(exec)); return; }
        case TextAreaTabIndex:        { textarea.setTabIndex(value->toInt32(exec)); return; }
        // read-only: type
        case TextAreaValue:           { textarea.setValue(str); return; }
    }
}

void HTMLElement::buttonSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLButtonElementImpl& button = *static_cast<HTMLButtonElementImpl*>(impl());
    switch (token) {
        // read-only: form
        case ButtonAccessKey:       { button.setAccessKey(str); return; }
        case ButtonDisabled:        { button.setDisabled(value->toBoolean(exec)); return; }
        case ButtonName:            { button.setName(str); return; }
        case ButtonTabIndex:        { button.setTabIndex(value->toInt32(exec)); return; }
        // read-only: type
        case ButtonValue:           { button.setValue(str); return; }
    }
}

void HTMLElement::labelSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLLabelElementImpl& label = *static_cast<HTMLLabelElementImpl*>(impl());
    switch (token) {
        // read-only: form
        case LabelAccessKey:       { label.setAccessKey(str); return; }
        case LabelHtmlFor:         { label.setHtmlFor(str); return; }
    }
}

void HTMLElement::fieldSetSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
}

void HTMLElement::legendSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLLegendElementImpl& legend = *static_cast<HTMLLegendElementImpl*>(impl());
    switch (token) {
        // read-only: form
        case LegendAccessKey:       { legend.setAccessKey(str); return; }
        case LegendAlign:           { legend.setAlign(str); return; }
    }
}

void HTMLElement::uListSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLUListElementImpl& uList = *static_cast<HTMLUListElementImpl*>(impl());
    switch (token) {
        case UListCompact:         { uList.setCompact(value->toBoolean(exec)); return; }
        case UListType:            { uList.setType(str); return; }
    }
}

void HTMLElement::oListSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLOListElementImpl& oList = *static_cast<HTMLOListElementImpl*>(impl());
    switch (token) {
        case OListCompact:         { oList.setCompact(value->toBoolean(exec)); return; }
        case OListStart:           { oList.setStart(value->toInt32(exec)); return; }
        case OListType:            { oList.setType(str); return; }
    }
}

void HTMLElement::dListSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLDListElementImpl& dList = *static_cast<HTMLDListElementImpl*>(impl());
    if (token == DListCompact)
        dList.setCompact(value->toBoolean(exec));
}

void HTMLElement::dirSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLDirectoryElementImpl& directory = *static_cast<HTMLDirectoryElementImpl*>(impl());
    if (token == DirectoryCompact)
        directory.setCompact(value->toBoolean(exec));
}

void HTMLElement::menuSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLMenuElementImpl& menu = *static_cast<HTMLMenuElementImpl*>(impl());
    if (token == MenuCompact)
        menu.setCompact(value->toBoolean(exec));
}

void HTMLElement::liSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLLIElementImpl& li = *static_cast<HTMLLIElementImpl*>(impl());
    switch (token) {
        case LIType:            { li.setType(str); return; }
        case LIValue:           { li.setValue(value->toInt32(exec)); return; }
    }
}

void HTMLElement::divSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLDivElementImpl& div = *static_cast<HTMLDivElementImpl*>(impl());
    if (token == DivAlign)
        div.setAlign(str);
}

void HTMLElement::paragraphSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLParagraphElementImpl& paragraph = *static_cast<HTMLParagraphElementImpl*>(impl());
    if (token == ParagraphAlign)
        paragraph.setAlign(str);
}

void HTMLElement::headingSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLHeadingElementImpl& heading = *static_cast<HTMLHeadingElementImpl*>(impl());
    if (token == HeadingAlign)
        heading.setAlign(str);
}

void HTMLElement::blockQuoteSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLBlockquoteElementImpl& blockQuote = *static_cast<HTMLBlockquoteElementImpl*>(impl());
    if (token == BlockQuoteCite)
        blockQuote.setCite(str);
}

void HTMLElement::quoteSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLQuoteElementImpl& quote = *static_cast<HTMLQuoteElementImpl*>(impl());
    if (token == QuoteCite)
        quote.setCite(str);
}

void HTMLElement::preSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLPreElementImpl& pre = *static_cast<HTMLPreElementImpl*>(impl());
    if (token == PreWidth)
        pre.setWidth(value->toInt32(exec));
    else if (token == PreWrap)
        pre.setWrap(value->toBoolean(exec));
}

void HTMLElement::brSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLBRElementImpl& br = *static_cast<HTMLBRElementImpl*>(impl());
    if (token == BRClear)
        br.setClear(str);
}

void HTMLElement::baseFontSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLBaseFontElementImpl& baseFont = *static_cast<HTMLBaseFontElementImpl*>(impl());
    switch (token) {
        case BaseFontColor:           { baseFont.setColor(str); return; }
        case BaseFontFace:            { baseFont.setFace(str); return; }
        case BaseFontSize:            { baseFont.setSize(str); return; }
    }
}

void HTMLElement::fontSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLFontElementImpl& font = *static_cast<HTMLFontElementImpl*>(impl());
    switch (token) {
        case FontColor:           { font.setColor(str); return; }
        case FontFace:            { font.setFace(str); return; }
        case FontSize:            { font.setSize(str); return; }
    }
}

void HTMLElement::hrSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLHRElementImpl& hr = *static_cast<HTMLHRElementImpl*>(impl());
    switch (token) {
        case HRAlign:           { hr.setAlign(str); return; }
        case HRNoShade:         { hr.setNoShade(value->toBoolean(exec)); return; }
        case HRSize:            { hr.setSize(str); return; }
        case HRWidth:           { hr.setWidth(str); return; }
    }
}

void HTMLElement::modSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLModElementImpl& mod = *static_cast<HTMLModElementImpl*>(impl());
    switch (token) {
        case ModCite:            { mod.setCite(str); return; }
        case ModDateTime:        { mod.setDateTime(str); return; }
    }
}

void HTMLElement::anchorSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLAnchorElementImpl& anchor = *static_cast<HTMLAnchorElementImpl*>(impl());
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

void HTMLElement::imageSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLImageElementImpl& image = *static_cast<HTMLImageElementImpl*>(impl());
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

void HTMLElement::objectSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLObjectElementImpl& object = *static_cast<HTMLObjectElementImpl*>(impl());
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

void HTMLElement::paramSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLParamElementImpl& param = *static_cast<HTMLParamElementImpl*>(impl());
    switch (token) {
        case ParamName:            { param.setName(str); return; }
        case ParamType:            { param.setType(str); return; }
        case ParamValue:           { param.setValue(str); return; }
        case ParamValueType:       { param.setValueType(str); return; }
    }
}

void HTMLElement::appletSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLAppletElementImpl& applet = *static_cast<HTMLAppletElementImpl*>(impl());
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

void HTMLElement::mapSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLMapElementImpl& map = *static_cast<HTMLMapElementImpl*>(impl());
    if (token == MapName)
        // read-only: areas
        map.setName(str);
}

void HTMLElement::areaSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLAreaElementImpl& area = *static_cast<HTMLAreaElementImpl*>(impl());
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

void HTMLElement::scriptSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLScriptElementImpl& script = *static_cast<HTMLScriptElementImpl*>(impl());
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

void HTMLElement::tableSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLTableElementImpl& table = *static_cast<HTMLTableElementImpl*>(impl());
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

void HTMLElement::tableCaptionSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLTableCaptionElementImpl& tableCaption = *static_cast<HTMLTableCaptionElementImpl*>(impl());
    if (token == TableCaptionAlign)
        tableCaption.setAlign(str);
}

void HTMLElement::tableColSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLTableColElementImpl& tableCol = *static_cast<HTMLTableColElementImpl*>(impl());
    switch (token) {
        case TableColAlign:           { tableCol.setAlign(str); return; }
        case TableColCh:              { tableCol.setCh(str); return; }
        case TableColChOff:           { tableCol.setChOff(str); return; }
        case TableColSpan:            { tableCol.setSpan(value->toInt32(exec)); return; }
        case TableColVAlign:          { tableCol.setVAlign(str); return; }
        case TableColWidth:           { tableCol.setWidth(str); return; }
    }
}

void HTMLElement::tableSectionSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLTableSectionElementImpl& tableSection = *static_cast<HTMLTableSectionElementImpl*>(impl());
    switch (token) {
        case TableSectionAlign:           { tableSection.setAlign(str); return; }
        case TableSectionCh:              { tableSection.setCh(str); return; }
        case TableSectionChOff:           { tableSection.setChOff(str); return; }
        case TableSectionVAlign:          { tableSection.setVAlign(str); return; }
        // read-only: rows
    }
}

void HTMLElement::tableRowSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLTableRowElementImpl& tableRow = *static_cast<HTMLTableRowElementImpl*>(impl());
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

void HTMLElement::tableCellSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLTableCellElementImpl& tableCell = *static_cast<HTMLTableCellElementImpl*>(impl());
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

void HTMLElement::frameSetSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLFrameSetElementImpl& frameSet = *static_cast<HTMLFrameSetElementImpl*>(impl());
    switch (token) {
        case FrameSetCols:            { frameSet.setCols(str); return; }
        case FrameSetRows:            { frameSet.setRows(str); return; }
    }
}

void HTMLElement::frameSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLFrameElementImpl& frameElement = *static_cast<HTMLFrameElementImpl*>(impl());
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

void HTMLElement::iFrameSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    HTMLIFrameElementImpl& iFrame = *static_cast<HTMLIFrameElementImpl*>(impl());
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

void HTMLElement::marqueeSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str)
{
    // FIXME: Find out what WinIE supports and implement it.
}

void HTMLElement::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
    DOMExceptionTranslator exception(exec);
    DOM::DOMString str = value->toString(exec).domString();
 
    // Check our set of generic properties first.
    HTMLElementImpl &element = *static_cast<HTMLElementImpl *>(impl());
    switch (token) {
        case ElementId:
            element.setId(str);
            return;
        case ElementTitle:
            element.setTitle(str);
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
    const Accessors* info = getSetInfo();
    if (info && info->m_setter)
        return (this->*(info->m_setter))(exec, token, value, str);  
}

HTMLElementImpl *toHTMLElement(ValueImp *val)
{
    if (!val || !val->isObject(&HTMLElement::info))
        return 0;
    return static_cast<HTMLElementImpl *>(static_cast<HTMLElement *>(val)->impl());
}

HTMLTableCaptionElementImpl *toHTMLTableCaptionElement(ValueImp *val)
{
    HTMLElementImpl *e = toHTMLElement(val);
    if (e && e->hasTagName(captionTag))
        return static_cast<HTMLTableCaptionElementImpl *>(e);
    return 0;
}

HTMLTableSectionElementImpl *toHTMLTableSectionElement(ValueImp *val)
{
    HTMLElementImpl *e = toHTMLElement(val);
    if (e && (e->hasTagName(theadTag) || e->hasTagName(tbodyTag) || e->hasTagName(tfootTag)))
        return static_cast<HTMLTableSectionElementImpl *>(e);
    return 0;
}

// -------------------------------------------------------------------------
/* Source for HTMLCollectionProtoTable. Use "make hashtables" to regenerate.
@begin HTMLCollectionProtoTable 3
  item		HTMLCollection::Item		DontDelete|Function 1
  namedItem	HTMLCollection::NamedItem	DontDelete|Function 1
  tags		HTMLCollection::Tags		DontDelete|Function 1
@end
*/
DEFINE_PROTOTYPE("HTMLCollection", HTMLCollectionProto)
IMPLEMENT_PROTOFUNC(HTMLCollectionProtoFunc)
IMPLEMENT_PROTOTYPE(HTMLCollectionProto,HTMLCollectionProtoFunc)

const ClassInfo HTMLCollection::info = { "HTMLCollection", 0, 0, 0 };

HTMLCollection::HTMLCollection(ExecState *exec, HTMLCollectionImpl *c)
  : m_impl(c) 
{
  setPrototype(HTMLCollectionProto::self(exec));
}

HTMLCollection::~HTMLCollection()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

ValueImp *HTMLCollection::lengthGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLCollection *thisObj = static_cast<HTMLCollection *>(slot.slotBase());
    return Number(thisObj->m_impl->length());
}

ValueImp *HTMLCollection::indexGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLCollection *thisObj = static_cast<HTMLCollection *>(slot.slotBase());
    return getDOMNode(exec, thisObj->m_impl->item(slot.index()));
}

ValueImp *HTMLCollection::nameGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLCollection *thisObj = static_cast<HTMLCollection *>(slot.slotBase());
    return thisObj->getNamedItems(exec, propertyName);
}

bool HTMLCollection::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (propertyName == lengthPropertyName) {
      slot.setCustom(this, lengthGetter);
      return true;
  } else {
    // Look in the prototype (for functions) before assuming it's an item's name
    ValueImp *proto = prototype();
    if (proto->isObject() && static_cast<ObjectImp *>(proto)->hasProperty(exec, propertyName))
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
ValueImp *KJS::HTMLCollection::callAsFunction(ExecState *exec, ObjectImp *, const List &args)
{
  // Do not use thisObj here. It can be the HTMLDocument, in the document.forms(i) case.
  HTMLCollectionImpl &collection = *m_impl;

  // Also, do we need the TypeError test here ?

  if (args.size() == 1) {
    // support for document.all(<index>) etc.
    bool ok;
    UString s = args[0]->toString(exec);
    unsigned int u = s.toUInt32(&ok);
    if (ok)
      return getDOMNode(exec, collection.item(u));
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
      DOM::DOMString pstr = s.domString();
      NodeImpl *node = collection.namedItem(pstr);
      while (node) {
        if (!u)
          return getDOMNode(exec,node);
        node = collection.nextNamedItem(pstr);
        --u;
      }
    }
  }
  return Undefined();
}

ValueImp *KJS::HTMLCollection::getNamedItems(ExecState *exec, const Identifier &propertyName) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "KJS::HTMLCollection::getNamedItems " << propertyName.ascii() << endl;
#endif
  DOM::DOMString pstr = propertyName.domString();

  QValueList< RefPtr<NodeImpl> > namedItems = m_impl->namedItems(pstr);

  if (namedItems.isEmpty()) {
#ifdef KJS_VERBOSE
    kdDebug(6070) << "not found" << endl;
#endif
    return Undefined();
  }

  if (namedItems.count() == 1)
    return getDOMNode(exec, namedItems[0].get());
  
  return new DOMNamedNodesCollection(exec, namedItems);
}

ValueImp *KJS::HTMLCollectionProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::HTMLCollection::info))
    return throwError(exec, TypeError);
  HTMLCollectionImpl &coll = *static_cast<HTMLCollection *>(thisObj)->impl();

  switch (id) {
  case KJS::HTMLCollection::Item:
    return getDOMNode(exec,coll.item(args[0]->toUInt32(exec)));
  case KJS::HTMLCollection::Tags:
    return getDOMNodeList(exec, coll.base()->getElementsByTagName(args[0]->toString(exec).domString()).get());
  case KJS::HTMLCollection::NamedItem:
    return static_cast<HTMLCollection *>(thisObj)->getNamedItems(exec, Identifier(args[0]->toString(exec)));
  default:
    return Undefined();
  }
}

// -------------------------------------------------------------------------

HTMLSelectCollection::HTMLSelectCollection(ExecState *exec, HTMLCollectionImpl *c, HTMLSelectElementImpl *e)
  : HTMLCollection(exec, c), m_element(e)
{
}

ValueImp *HTMLSelectCollection::selectedIndexGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLSelectCollection *thisObj = static_cast<HTMLSelectCollection *>(slot.slotBase());
    return Number(thisObj->m_element->selectedIndex());
}

bool HTMLSelectCollection::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (propertyName == "selectedIndex") {
    slot.setCustom(this, selectedIndexGetter);
    //result = Number(m_element->selectedIndex());
    return true;
  }

  return HTMLCollection::getOwnPropertySlot(exec, propertyName, slot);
}

void KJS::HTMLSelectCollection::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "KJS::HTMLSelectCollection::put " << propertyName.qstring() << endl;
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

    if (!converted) {
      return;
    }

    int diff = m_element->length() - newLen;

    if (diff < 0) { // add dummy elements
      do {
        ElementImpl *option = m_element->ownerDocument()->createElement("option", exception);
        if (exception)
          break;         
        m_element->add(static_cast<HTMLElementImpl *>(option), 0, exception);
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
  NodeImpl *option = toNode(value);
  if (!option || !option->hasTagName(optionTag))
    return;

  int exception = 0;
  int diff = int(u) - m_element->length();
  HTMLElementImpl *before = 0;
  // out of array bounds ? first insert empty dummies
  if (diff > 0) {
    while (diff--) {
      ElementImpl *dummyOption = m_element->ownerDocument()->createElement("option", exception);
      if (!dummyOption)
        break;      
      m_element->add(static_cast<HTMLElementImpl *>(dummyOption), 0, exception);
      if (exception) 
          break;
    }
    // replace an existing entry ?
  } else if (diff < 0) {
    before = static_cast<HTMLElementImpl *>(m_element->options()->item(u+1));
    m_element->remove(u);
  }
  // finally add the new element
  if (exception == 0)
    m_element->add(static_cast<HTMLOptionElementImpl *>(option), before, exception);

  setDOMException(exec, exception);
}

////////////////////// Option Object ////////////////////////

OptionConstructorImp::OptionConstructorImp(ExecState *exec, DocumentImpl *d)
    : m_doc(d)
{
  // ## isn't there some redundancy between ObjectImp::_proto and the "prototype" property ?
  //put(exec,"prototype", ...,DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  // ## is 4 correct ? 0 to 4, it seems to be
  put(exec,lengthPropertyName, Number(4), ReadOnly|DontDelete|DontEnum);
}

bool OptionConstructorImp::implementsConstruct() const
{
  return true;
}

ObjectImp *OptionConstructorImp::construct(ExecState *exec, const List &args)
{
  int exception = 0;
  RefPtr<ElementImpl> el(m_doc->createElement("option", exception));
  HTMLOptionElementImpl *opt = 0;
  if (el) {
    opt = static_cast<HTMLOptionElementImpl *>(el.get());
    int sz = args.size();
    TextImpl *t = m_doc->createTextNode("");
    t->ref();
    opt->appendChild(t, exception);
    if (exception == 0 && sz > 0)
      t->setData(args[0]->toString(exec).domString(), exception); // set the text
    if (exception == 0 && sz > 1)
      opt->setValue(args[1]->toString(exec).domString());
    if (exception == 0 && sz > 2)
      opt->setDefaultSelected(args[2]->toBoolean(exec));
    if (exception == 0 && sz > 3)
      opt->setSelected(args[3]->toBoolean(exec));
    t->deref();
  }

  setDOMException(exec, exception);
  return static_cast<ObjectImp *>(getDOMNode(exec,opt));
}

////////////////////// Image Object ////////////////////////

ImageConstructorImp::ImageConstructorImp(ExecState *, DocumentImpl *d)
    : m_doc(d)
{
}

bool ImageConstructorImp::implementsConstruct() const
{
  return true;
}

ObjectImp *ImageConstructorImp::construct(ExecState * exec, const List & list)
{
    bool widthSet = false, heightSet = false;
    int width = 0, height = 0;
    if (list.size() > 0) {
        widthSet = true;
        ValueImp *w = list.at(0);
        width = w->toInt32(exec);
    }
    if (list.size() > 1) {
        heightSet = true;
        ValueImp *h = list.at(1);
        height = h->toInt32(exec);
    }
        
    ObjectImp *result(new Image(m_doc.get(), widthSet, width, heightSet, height));
  
    /* TODO: do we need a prototype ? */
    return result;
}

const ClassInfo KJS::Image::info = { "Image", 0, &ImageTable, 0 };

/* Source for ImageTable. Use "make hashtables" to regenerate.
@begin ImageTable 6
  src		Image::Src		DontDelete
  complete	Image::Complete		DontDelete|ReadOnly
  onload        Image::OnLoad           DontDelete
  width         Image::Width            DontDelete|ReadOnly
  height        Image::Height           DontDelete|ReadOnly
@end
*/

bool Image::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<Image,DOMObject>(exec, &ImageTable, this, propertyName, slot);
}

ValueImp *Image::getValueProperty(ExecState *, int token) const
{
  switch (token) {
  case Src:
    return jsString(doc ? doc->completeURL(src.domString()) : src);
  case Complete:
    return Boolean(!img || img->status() >= khtml::CachedObject::Persistent);
  case OnLoad:
    if (onLoadListener && onLoadListener->listenerObjImp()) {
      return onLoadListener->listenerObj();
    } else {
      return Null();
    }
  case Width: {
    if (widthSet)
        return Number(width);
    int w = 0;
    if (img) {
      QSize size = img->pixmap_size();
      if (size.isValid())
        w = size.width();
    }
    return Number(w);
  }
  case Height: {
    if (heightSet)
        return Number(height);
    int h = 0;
    if (img) {
      QSize size = img->pixmap_size();
      if (size.isValid())
        h = size.height();
    }
    return Number(h);
  }
  default:
    kdWarning() << "Image::getValueProperty unhandled token " << token << endl;
    return NULL;
  }
}

void Image::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
  lookupPut<Image,DOMObject>(exec, propertyName, value, attr, &ImageTable, this );
}

void Image::putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/)
{
  switch(token) {
  case Src:
  {
    src = value->toString(exec);
    if ( img ) img->deref(this);
    img = doc ? doc->docLoader()->requestImage( src.domString() ) : 0;
    if ( img ) img->ref(this);
    break;
  }
  case OnLoad:
    onLoadListener = Window::retrieveActive(exec)->getJSEventListener(value, true);
    if (onLoadListener) onLoadListener->ref();
    break;
  case Width:
    widthSet = true;
    width = value->toInt32(exec);
    break;
  case Height:
    heightSet = true;
    height = value->toInt32(exec);
    break;
  default:
    kdWarning() << "HTMLDocument::putValueProperty unhandled token " << token << endl;
  }
}

void Image::notifyFinished(khtml::CachedObject *)
{
  if (onLoadListener && doc->part()) {
    int ignoreException;
    EventImpl *ev = doc->createEvent("HTMLEvents", ignoreException);
    ev->ref();
    ev->initEvent(loadEvent, true, true);
    onLoadListener->handleEventImpl(ev, true);
    ev->deref();
  }
}

Image::Image(DocumentImpl *d, bool ws, int w, bool hs, int h)
  : doc(d), img(0), onLoadListener(0)
{
      widthSet = ws;
      width = w;
      heightSet = hs;
      height = h;
}

Image::~Image()
{
  if ( img ) img->deref(this);
  if ( onLoadListener ) onLoadListener->deref();
}


////////////////////// Context2D Object ////////////////////////

IMPLEMENT_PROTOFUNC(Context2DFunction)

static bool isGradient(ValueImp *value)
{
    return value->isObject(&Gradient::info);
}

static bool isImagePattern(ValueImp *value)
{
    return value->isObject(&ImagePattern::info);
}

#define BITS_PER_COMPONENT 8
#define BYTES_PER_ROW(width,bitsPerComponent,numComponents) ((width * bitsPerComponent * numComponents + 7)/8)

ValueImp *KJS::Context2DFunction::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
    if (!thisObj->inherits(&Context2D::info))
        return throwError(exec, TypeError);

    Context2D *contextObject = static_cast<KJS::Context2D *>(thisObj);
    khtml::RenderCanvasImage *renderer = static_cast<khtml::RenderCanvasImage*>(contextObject->_element->renderer());
    if (!renderer)
        return Undefined();

    CGContextRef drawingContext = renderer->drawingContext();
    if (!drawingContext)
        return Undefined();
    
    switch (id) {
        case Context2D::Save: {
            if (args.size() != 0)
                return throwError(exec, SyntaxError);
            CGContextSaveGState(drawingContext);
            
            contextObject->save();
            
            break;
        }
        case Context2D::Restore: {
            if (args.size() != 0)
                return throwError(exec, SyntaxError);
            CGContextRestoreGState(drawingContext);
            
            contextObject->restore();
            
            break;
        }
        case Context2D::BeginPath: {
            if (args.size() != 0)
                return throwError(exec, SyntaxError);
            CGContextBeginPath(drawingContext);
            break;
        }
        case Context2D::ClosePath: {
            if (args.size() != 0)
                return throwError(exec, SyntaxError);
            CGContextClosePath(drawingContext);
            break;
        }
        case Context2D::SetStrokeColor: {
            // string arg = named color
            // string arg, number arg = named color, alpha
            // number arg = gray color
            // number arg, number arg = gray color, alpha
            // 4 args (string or number) = r, g, b, a
            // 5 args (string or number) = c, m, y, k, a
            int numArgs = args.size();
            switch (numArgs) {
                case 1: {
                    if (args[0]->isString()) {                    
                        QRgb color = DOM::CSSParser::parseColor(args[0]->toString(exec).domString());
                        QColor qc(color);
                        CGContextSetRGBStrokeColor(drawingContext, qc.red()/255., qc.green()/255., qc.blue()/255., qc.alpha()/255.);

                    }
                    else {
                        float g = (float)args[0]->toNumber(exec);
                        CGContextSetGrayStrokeColor(drawingContext, g, 1.);
                    }
                }
                break;
                case 2: {
                    float a = args[1]->toNumber(exec);
                    if (args[0]->isString()) {
                        QRgb color = DOM::CSSParser::parseColor(args[0]->toString(exec).domString());
                        QColor qc(color);
                        CGContextSetRGBStrokeColor(drawingContext, qc.red()/255., qc.green()/255., qc.blue()/255., a);
                    }
                    else {
                        float g = (float)args[0]->toNumber(exec);
                        CGContextSetGrayStrokeColor(drawingContext, g, a);
                    }
                }
                break;
                case 4: {
                    float r = (float)args[0]->toNumber(exec);
                    float g = (float)args[1]->toNumber(exec);
                    float b = (float)args[2]->toNumber(exec);
                    float a = (float)args[3]->toNumber(exec);
                    CGContextSetRGBStrokeColor(drawingContext, r, g, b, a);
                }
                break;
                case 5: {
                    float c = (float)args[0]->toNumber(exec);
                    float m = (float)args[1]->toNumber(exec);
                    float y = (float)args[2]->toNumber(exec);
                    float k = (float)args[3]->toNumber(exec);
                    float a = (float)args[4]->toNumber(exec);
                    CGContextSetCMYKStrokeColor(drawingContext, c, m, y, k, a);
                }
                break;
                default:
                    return throwError(exec, SyntaxError);
            }
            break;
        }
        case Context2D::SetFillColor: {
            // string arg = named color
            // string arg, number arg = named color, alpha
            // number arg = gray color
            // number arg, number arg = gray color, alpha
            // 4 args (string or number) = r, g, b, a
            // 5 args (string or number) = c, m, y, k, a
            int numArgs = args.size();
            switch (numArgs) {
                case 1: {
                    if (args[0]->isString()) {
                        QRgb color = DOM::CSSParser::parseColor(args[0]->toString(exec).domString());
                        QColor qc(color);
                        CGContextSetRGBFillColor(drawingContext, qc.red()/255., qc.green()/255., qc.blue()/255., qc.alpha()/255.);
                    }
                    else {
                        float g = (float)args[0]->toNumber(exec);
                        CGContextSetGrayFillColor(drawingContext, g, 1.);
                    }
                }
                break;
                case 2: {
                    float a = args[1]->toNumber(exec);
                    if (args[0]->isString()) {
                        QRgb color = DOM::CSSParser::parseColor(args[0]->toString(exec).domString());
                        QColor qc(color);
                        CGContextSetRGBFillColor(drawingContext, qc.red()/255., qc.green()/255., qc.blue()/255., a);
                    }
                    else {
                        float g = (float)args[0]->toNumber(exec);
                        CGContextSetGrayFillColor(drawingContext, g, a);
                    }
                }
                break;
                case 4: {
                    float r = (float)args[0]->toNumber(exec);
                    float g = (float)args[1]->toNumber(exec);
                    float b = (float)args[2]->toNumber(exec);
                    float a = (float)args[3]->toNumber(exec);
                    CGContextSetRGBFillColor(drawingContext, r, g, b, a);
                }
                break;
                case 5: {
                    float c = (float)args[0]->toNumber(exec);
                    float m = (float)args[1]->toNumber(exec);
                    float y = (float)args[2]->toNumber(exec);
                    float k = (float)args[3]->toNumber(exec);
                    float a = (float)args[4]->toNumber(exec);
                    CGContextSetCMYKStrokeColor(drawingContext, c, m, y, k, a);
                }
                break;
                default:
                    return throwError(exec, SyntaxError);
            }
            break;
        }
        case Context2D::SetLineWidth: {
            if (args.size() != 1)
                return throwError(exec, SyntaxError);
            float w = (float)args[0]->toNumber(exec);
            CGContextSetLineWidth (drawingContext, w);
            break;
        }
        case Context2D::SetLineCap: {
            if (args.size() != 1)
                return throwError(exec, SyntaxError);
            CGLineCap cap = kCGLineCapButt;
            QString capString = args[0]->toString(exec).qstring().lower();
            if (capString == "round")
                cap = kCGLineCapRound;
            else if (capString == "square")
                cap = kCGLineCapSquare;
            CGContextSetLineCap (drawingContext, cap);
            break;
        }
        case Context2D::SetLineJoin: {
            if (args.size() != 1)
                return throwError(exec, SyntaxError);
            CGLineJoin join = kCGLineJoinMiter;
            QString joinString = args[0]->toString(exec).qstring().lower();
            if (joinString == "round")
                join = kCGLineJoinRound;
            else if (joinString == "bevel")
                join = kCGLineJoinBevel;
            CGContextSetLineJoin (drawingContext, join);
            break;
        }
        case Context2D::SetMiterLimit: {
            if (args.size() != 1)
                return throwError(exec, SyntaxError);
            float l = (float)args[0]->toNumber(exec);
            CGContextSetMiterLimit (drawingContext, l);
            break;
        }
        case Context2D::Fill: {
            if (args.size() != 0)
                return throwError(exec, SyntaxError);
            
            if (isGradient(contextObject->_fillStyle)) {
                CGContextSaveGState(drawingContext);
                
                // Set the clip from the current path because shading only
                // operates on clippin regions!  Odd, but true.
                CGContextClip(drawingContext);

                ObjectImp *o = static_cast<ObjectImp*>(contextObject->_fillStyle);
                Gradient *gradient = static_cast<Gradient*>(o);
                CGShadingRef shading = gradient->getShading();
                CGContextDrawShading(drawingContext, shading);
                
                CGContextRestoreGState(drawingContext);
            } else if (isImagePattern(contextObject->_fillStyle)) {
                contextObject->updateFillImagePattern();
                CGContextFillPath(drawingContext);
            }
            else
                CGContextFillPath(drawingContext);
                
            renderer->setNeedsImageUpdate();
            break;
        }
        case Context2D::Stroke: {
            if (args.size() != 0)
                return throwError(exec, SyntaxError);
            if (isGradient(contextObject->_strokeStyle)) {
                CGContextSaveGState(drawingContext);
                
                // Convert the stroke normally performed on the path
                // into a path.  Then set the clip from that path 
                // because shading only operates on clipping regions!  Odd, 
                // but true.
                CGContextReplacePathWithStrokedPath(drawingContext);
                CGContextClip(drawingContext);

                ObjectImp *o = static_cast<ObjectImp*>(contextObject->_strokeStyle);
                Gradient *gradient = static_cast<Gradient*>(o);
                
                CGShadingRef shading = gradient->getShading();
                CGContextDrawShading(drawingContext, shading);
                
                CGContextRestoreGState(drawingContext);
            } else if (isImagePattern(contextObject->_strokeStyle)) {
                contextObject->updateStrokeImagePattern();
                CGContextFillPath(drawingContext);
            }
            else
                CGContextStrokePath (drawingContext);
            renderer->setNeedsImageUpdate();
            break;
        }
        case Context2D::Scale: {
            if (args.size() != 2)
                return throwError(exec, SyntaxError);
            float sx = (float)args[0]->toNumber(exec);
            float sy = (float)args[1]->toNumber(exec);
            CGContextScaleCTM (drawingContext, sx, sy);
            contextObject->_needsFlushRasterCache = true;
            break;
        }
        case Context2D::Rotate: {
            if (args.size() != 1)
                return throwError(exec, SyntaxError);
            float angle = (float)args[0]->toNumber(exec);
            CGContextRotateCTM (drawingContext, angle);
            contextObject->_needsFlushRasterCache = true;
            break;
        }
        case Context2D::Translate: {
            if (args.size() != 2)
                return throwError(exec, SyntaxError);
            float tx = (float)args[0]->toNumber(exec);
            float ty = (float)args[1]->toNumber(exec);
            CGContextTranslateCTM (drawingContext, tx, ty);
            break;
        }
        case Context2D::MoveTo: {
            if (args.size() != 2)
                return throwError(exec, SyntaxError);
            float x = (float)args[0]->toNumber(exec);
            float y = (float)args[1]->toNumber(exec);
            CGContextMoveToPoint (drawingContext, x, y);
            renderer->setNeedsImageUpdate();
            break;
        }
        case Context2D::LineTo: {
            if (args.size() != 2)
                return throwError(exec, SyntaxError);
            float x = (float)args[0]->toNumber(exec);
            float y = (float)args[1]->toNumber(exec);
            CGContextAddLineToPoint (drawingContext, x, y);
            renderer->setNeedsImageUpdate();
            break;
        }
        case Context2D::QuadraticCurveTo: {
            if (args.size() != 4)
                return throwError(exec, SyntaxError);
            float cpx = (float)args[0]->toNumber(exec);
            float cpy = (float)args[1]->toNumber(exec);
            float x = (float)args[2]->toNumber(exec);
            float y = (float)args[3]->toNumber(exec);
            CGContextAddQuadCurveToPoint (drawingContext, cpx, cpy, x, y);
            renderer->setNeedsImageUpdate();
            break;
        }
        case Context2D::BezierCurveTo: {
            if (args.size() != 6)
                return throwError(exec, SyntaxError);
            float cp1x = (float)args[0]->toNumber(exec);
            float cp1y = (float)args[1]->toNumber(exec);
            float cp2x = (float)args[2]->toNumber(exec);
            float cp2y = (float)args[3]->toNumber(exec);
            float x = (float)args[4]->toNumber(exec);
            float y = (float)args[5]->toNumber(exec);
            CGContextAddCurveToPoint (drawingContext, cp1x, cp1y, cp2x, cp2y, x, y);
            renderer->setNeedsImageUpdate();
            break;
        }
        case Context2D::ArcTo: {
            if (args.size() != 5)
                return throwError(exec, SyntaxError);
            float x1 = (float)args[0]->toNumber(exec);
            float y1 = (float)args[1]->toNumber(exec);
            float x2 = (float)args[2]->toNumber(exec);
            float y2 = (float)args[3]->toNumber(exec);
            float r = (float)args[4]->toNumber(exec);
            CGContextAddArcToPoint (drawingContext, x1, y1, x2, y2, r);
            break;
        }
        case Context2D::Arc: {
            if (args.size() != 6)
                return throwError(exec, SyntaxError);
            float x = (float)args[0]->toNumber(exec);
            float y = (float)args[1]->toNumber(exec);
            float r = (float)args[2]->toNumber(exec);
            float sa = (float)args[3]->toNumber(exec);
            float ea = (float)args[4]->toNumber(exec);
            bool clockwise = args[5]->toBoolean(exec);
            CGContextAddArc (drawingContext, x, y, r, sa, ea, clockwise);
            break;
        }
        case Context2D::Rect: {
            if (args.size() != 4)
                return throwError(exec, SyntaxError);
            float x = (float)args[0]->toNumber(exec);
            float y = (float)args[1]->toNumber(exec);
            float w = (float)args[2]->toNumber(exec);
            float h = (float)args[3]->toNumber(exec);
            CGContextAddRect (drawingContext, CGRectMake(x,y,w,h));
            break;
        }
        case Context2D::Clip: {
            if (args.size() != 0)
                return throwError(exec, SyntaxError);
            CGContextClip (drawingContext);
            break;
        }

        case Context2D::ClearRect: {
            if (args.size() != 4)
                return throwError(exec, SyntaxError);
            float x = (float)args[0]->toNumber(exec);
            float y = (float)args[1]->toNumber(exec);
            float w = (float)args[2]->toNumber(exec);
            float h = (float)args[3]->toNumber(exec);
            CGContextClearRect (drawingContext, CGRectMake(x,y,w,h));
            renderer->setNeedsImageUpdate();
            break;
        }
        case Context2D::FillRect: {
            if (args.size() != 4)
                return throwError(exec, SyntaxError);
            float x = (float)args[0]->toNumber(exec);
            float y = (float)args[1]->toNumber(exec);
            float w = (float)args[2]->toNumber(exec);
            float h = (float)args[3]->toNumber(exec);
            if (isImagePattern(contextObject->_fillStyle))
                contextObject->updateFillImagePattern();
            CGContextFillRect (drawingContext, CGRectMake(x,y,w,h));
            renderer->setNeedsImageUpdate();
            break;
        }
        case Context2D::StrokeRect: {
            int size = args.size();
            if (size < 4)
                return throwError(exec, SyntaxError);
            float x = (float)args[0]->toNumber(exec);
            float y = (float)args[1]->toNumber(exec);
            float w = (float)args[2]->toNumber(exec);
            float h = (float)args[3]->toNumber(exec);
            
            if (isImagePattern(contextObject->_strokeStyle))
                contextObject->updateStrokeImagePattern();
            if (size > 4)
                CGContextStrokeRectWithWidth (drawingContext, CGRectMake(x,y,w,h), (float)args[4]->toNumber(exec));
            else
                CGContextStrokeRect (drawingContext, CGRectMake(x,y,w,h));
            renderer->setNeedsImageUpdate();
            break;
        }
        case Context2D::SetShadow: {
            int numArgs = args.size();
            
            if (numArgs < 3)
                return throwError(exec, SyntaxError);
            CGSize offset;
            
            offset.width = (float)args[0]->toNumber(exec);
            offset.height = (float)args[1]->toNumber(exec);
            float blur = (float)args[2]->toNumber(exec);
            
            QColor color = QColor(args[3]->toString(exec).ascii());

             if (numArgs == 3) {
                CGContextSetShadow (drawingContext, offset, blur);
            } else {
                CGColorSpaceRef colorSpace;
                float components[5];
                
                switch (numArgs - 3) {
                    case 1: {
                        if (args[3]->isString()) {
                            QRgb color = DOM::CSSParser::parseColor(args[3]->toString(exec).domString());
                            QColor qc(color);
                            components[0] = qc.red()/255.;
                            components[1] = qc.green()/255.;
                            components[2] = qc.blue()/255.;
                            components[3] = 1.0f;
                            colorSpace = CGColorSpaceCreateDeviceRGB();
                        }
                        else {
                            components[0] = (float)args[3]->toNumber(exec);
                            components[1] = 1.0f;
                            colorSpace = CGColorSpaceCreateDeviceGray();
                        }
                    }
                    break;
                    case 2: {
                        float a = args[4]->toNumber(exec);
                        if (args[3]->isString()) {
                            QRgb color = DOM::CSSParser::parseColor(args[3]->toString(exec).domString());
                            QColor qc(color);
                            components[0] = qc.red()/255.;
                            components[1] = qc.green()/255.;
                            components[2] = qc.blue()/255.;
                            components[3] = a;
                            colorSpace = CGColorSpaceCreateDeviceRGB();
                        }
                        else {
                            components[0] = (float)args[3]->toNumber(exec);
                            components[1] = a;
                            colorSpace = CGColorSpaceCreateDeviceGray();
                        }
                    }
                    break;
                    case 4: {
                        components[0] = (float)args[3]->toNumber(exec); // r
                        components[1] = (float)args[4]->toNumber(exec); // g
                        components[2] = (float)args[5]->toNumber(exec); // b
                        components[3] = (float)args[6]->toNumber(exec); // a
                        colorSpace = CGColorSpaceCreateDeviceRGB();
                    }
                    break;
                    case 5: {
                        components[0] = (float)args[3]->toNumber(exec); // c
                        components[1] = (float)args[4]->toNumber(exec); // m
                        components[2] = (float)args[5]->toNumber(exec); // y
                        components[3] = (float)args[6]->toNumber(exec); // k
                        components[4] = (float)args[7]->toNumber(exec); // a

                        colorSpace = CGColorSpaceCreateDeviceCMYK();
                    }
                    break;
                    default:
                        return throwError(exec, SyntaxError);
                }
                
                CGColorRef colorRef = CGColorCreate (colorSpace, components);
                CGContextSetShadowWithColor (drawingContext, offset, blur, colorRef);
                CFRelease (colorRef);
                CFRelease (colorSpace);
            }
            break;
        }
        case Context2D::ClearShadow: {
            if (args.size() != 0)
                return throwError(exec, SyntaxError);
            CGContextSetShadowWithColor (drawingContext, CGSizeMake(0, 0), 0, nil);
            break;
        }

        // DrawImage has three variants:
        // drawImage (img, dx, dy)
        // drawImage (img, dx, dy, dw, dh)
        // drawImage (img, sx, sy, sw, sh, dx, dy, dw, dh)
        // composite operation is specified with globalCompositeOperation
        // img parameter can be a JavaScript Image, <img>, or a <canvas>
        case Context2D::DrawImage: {
            if (args.size() < 3)
                return throwError(exec, SyntaxError);
            
            // Make sure first argument is an object.
            ObjectImp *o = static_cast<ObjectImp*>(args[0]);
            if (!o->isObject())
                return throwError(exec, TypeError);

            float w = 0; // quiet incorrect gcc 4.0 warning
            float h = 0; // quiet incorrect gcc 4.0 warning
            QPixmap pixmap;
            CGContextRef sourceContext = 0;
            
            // Check for JavaScript Image, <img> or <canvas>.
            if (o->inherits(&Image::info)) {
                Image *i = static_cast<Image*>(o);
                khtml::CachedImage *ci = i->image();
                if (ci) {
                    pixmap = ci->pixmap();
                }
                else {
                    // No image.
                    return Undefined();
                }
                w = pixmap.width();
                h = pixmap.height();
            }
            else if (o->inherits(&KJS::HTMLElement::img_info)){
                NodeImpl *n = static_cast<HTMLElement *>(args[0])->impl();
                HTMLImageElementImpl *e = static_cast<HTMLImageElementImpl*>(n);
                pixmap = e->pixmap();
                w = pixmap.width();
                h = pixmap.height();
            }
            else if (o->inherits(&KJS::HTMLElement::canvas_info)){
                NodeImpl *n = static_cast<HTMLElement *>(args[0])->impl();
                HTMLCanvasElementImpl *e = static_cast<HTMLCanvasElementImpl*>(n);
                khtml::RenderCanvasImage *renderer = static_cast<khtml::RenderCanvasImage*>(e->renderer());
                if (!renderer) {
                    // No renderer, nothing to draw.
                    return Undefined();
                }

                sourceContext = renderer->drawingContext();
                w = (float)CGBitmapContextGetWidth(sourceContext);
                h = (float)CGBitmapContextGetHeight(sourceContext);
            }
            else
                return throwError(exec, TypeError);
            
            float dx, dy, dw = w, dh = h;
            float sx = 0.f, sy = 0.f, sw = w, sh = h;
            
            if (args.size() == 3) {
                dx = args[1]->toNumber(exec);
                dy = args[2]->toNumber(exec);
            }
            else if (args.size() == 5) {
                dx = args[1]->toNumber(exec);
                dy = args[2]->toNumber(exec);
                dw = args[3]->toNumber(exec);
                dh = args[4]->toNumber(exec);
            }
            else if (args.size() == 9) {
                sx = args[1]->toNumber(exec);
                sy = args[2]->toNumber(exec);
                sw = args[3]->toNumber(exec);
                sh = args[4]->toNumber(exec);
                dx = args[5]->toNumber(exec);
                dy = args[6]->toNumber(exec);
                dw = args[7]->toNumber(exec);
                dh = args[8]->toNumber(exec);
            }
            else
                return throwError(exec, SyntaxError);

            if (!sourceContext) {
                QPainter p;
                p.drawFloatPixmap (dx, dy, dw, dh, pixmap, sx, sy, sw, sh, 
                    QPainter::compositeOperatorFromString(contextObject->_globalComposite->toString(exec).qstring().lower()), drawingContext);
            }
            else {
                // Cheap, because the image is backed by copy-on-write memory, and we're
                // guaranteed to release before doing any more drawing in the source context.
                CGImageRef sourceImage = CGBitmapContextCreateImage(sourceContext);
                if (sx == 0 && sy == 0 && sw == w && sh == h) {
                    // Fast path, yay!
                    CGContextDrawImage (drawingContext, CGRectMake(dx, dy, dw, dh), sourceImage);
                }
                else {
                    // Create a new bitmap of the appropriate size and then draw that into our context.
                    // Slow path, boo!
                    CGContextRef clippedSourceContext;
                    CGImageRef clippedSourceImage;
                    size_t csw = (size_t)sw;
                    size_t csh = (size_t)sh;
                                        
                    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
                    size_t numComponents = CGColorSpaceGetNumberOfComponents(colorSpace);
                    size_t bytesPerRow = BYTES_PER_ROW(csw,BITS_PER_COMPONENT,(numComponents+1)); // + 1 for alpha
                    void *_drawingContextData = fastMalloc(csh * bytesPerRow);
                    
                    clippedSourceContext = CGBitmapContextCreate(_drawingContextData, csw, csh, BITS_PER_COMPONENT, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast);
                    CGContextTranslateCTM (clippedSourceContext, -sx, -sy);
                    CGContextDrawImage (clippedSourceContext, CGRectMake(0,0,w,h), sourceImage);
                    clippedSourceImage = CGBitmapContextCreateImage(clippedSourceContext);
                    CGContextDrawImage (drawingContext, CGRectMake(dx, dy, dw, dh), clippedSourceImage);
                    
                    CGImageRelease (clippedSourceImage);
                    CGContextRelease (clippedSourceContext);
                    fastFree (_drawingContextData);
                }
                
                CGImageRelease (sourceImage);
            }
            
            if (contextObject->_needsFlushRasterCache)
                pixmap.flushRasterCache();

            renderer->setNeedsImageUpdate();

            break;
        }
        case Context2D::DrawImageFromRect: {
            if (args.size() != 10)
                return throwError(exec, SyntaxError);
            ObjectImp *o = static_cast<ObjectImp*>(args[0]);
            if (!o->isObject() || !o->inherits(&Image::info))
                return throwError(exec, TypeError);
            Image *i = static_cast<Image*>(o);
            float sx = args[1]->toNumber(exec);
            float sy = args[2]->toNumber(exec);
            float sw = args[3]->toNumber(exec);
            float sh = args[4]->toNumber(exec);
            float dx = args[5]->toNumber(exec);
            float dy = args[6]->toNumber(exec);
            float dw = args[7]->toNumber(exec);
            float dh = args[8]->toNumber(exec);
            QString compositeOperator = args[9]->toString(exec).qstring().lower();
            khtml::CachedImage *ci = i->image();
            if (ci) {
                QPixmap pixmap = ci->pixmap();
                QPainter p;

                p.drawFloatPixmap (dx, dy, dw, dh, pixmap, sx, sy, sw, sh, QPainter::compositeOperatorFromString(compositeOperator), drawingContext);
                
                if (contextObject->_needsFlushRasterCache)
                    pixmap.flushRasterCache();

                renderer->setNeedsImageUpdate();
            }
            break;
        }
        case Context2D::SetAlpha: {
            if (args.size() != 1)
                return throwError(exec, SyntaxError);
            float a =  (float)args[0]->toNumber(exec);
            CGContextSetAlpha (drawingContext, a);
            break;
        }
        case Context2D::SetCompositeOperation: {
            if (args.size() != 1)
                return throwError(exec, SyntaxError);
            QString compositeOperator = args[0]->toString(exec).qstring().lower();
            QPainter::setCompositeOperation (drawingContext,compositeOperator);
            break;
        }
        
        case Context2D::CreateLinearGradient: {
            if (args.size() != 4)
                return throwError(exec, SyntaxError);
            float x0 = args[0]->toNumber(exec);
            float y0 = args[1]->toNumber(exec);
            float x1 = args[2]->toNumber(exec);
            float y1 = args[3]->toNumber(exec);

            return new Gradient(x0, y0, x1, y1);
        }
        
        case Context2D::CreateRadialGradient: {
            if (args.size() != 6)
                return throwError(exec, SyntaxError);
            float x0 = args[0]->toNumber(exec);
            float y0 = args[1]->toNumber(exec);
            float r0 = args[2]->toNumber(exec);
            float x1 = args[3]->toNumber(exec);
            float y1 = args[4]->toNumber(exec);
            float r1 = args[5]->toNumber(exec);

            return new Gradient(x0, y0, r0, x1, y1, r1);
        }
        
        case Context2D::CreatePattern: {
            if (args.size() != 2)
                return throwError(exec, SyntaxError);
            ObjectImp *o = static_cast<ObjectImp*>(args[0]);
            if (!o->isObject() || !o->inherits(&Image::info))
                return throwError(exec, TypeError);
            int repetitionType = ImagePattern::Repeat;
            QString repetitionString = args[1]->toString(exec).qstring().lower();
            if (repetitionString == "repeat-x")
                repetitionType = ImagePattern::RepeatX;
            else if (repetitionString == "repeat-y")
                repetitionType = ImagePattern::RepeatY;
            else if (repetitionString == "no-repeat")
                repetitionType = ImagePattern::NoRepeat;
            return new ImagePattern(static_cast<Image*>(o), repetitionType);
        }
    }

    return Undefined();
}

const ClassInfo Context2D::info = { "Context2D", 0, &Context2DTable, 0 };

/* Source for Context2DTable. Use "make hashtables" to regenerate.
@begin Context2DTable 48
  strokeStyle              Context2D::StrokeStyle                 DontDelete
  fillStyle                Context2D::FillStyle                   DontDelete
  lineWidth                Context2D::LineWidth                   DontDelete
  lineCap                  Context2D::LineCap                     DontDelete
  lineJoin                 Context2D::LineJoin                    DontDelete
  miterLimit               Context2D::MiterLimit                  DontDelete
  shadowOffsetX            Context2D::ShadowOffsetX               DontDelete
  shadowOffsetY            Context2D::ShadowOffsetY               DontDelete
  shadowBlur               Context2D::ShadowBlur                  DontDelete
  shadowColor              Context2D::ShadowColor                 DontDelete
  globalAlpha              Context2D::GlobalAlpha                 DontDelete
  globalCompositeOperation Context2D::GlobalCompositeOperation    DontDelete
  save                     Context2D::Save                        DontDelete|Function 0
  restore                  Context2D::Restore                     DontDelete|Function 0
  scale                    Context2D::Scale                       DontDelete|Function 2
  rotate                   Context2D::Rotate                      DontDelete|Function 2
  translate                Context2D::Translate                   DontDelete|Function 1
  beginPath                Context2D::BeginPath                   DontDelete|Function 0
  closePath                Context2D::ClosePath                   DontDelete|Function 0
  setStrokeColor           Context2D::SetStrokeColor              DontDelete|Function 1
  setFillColor             Context2D::SetFillColor                DontDelete|Function 1
  setLineWidth             Context2D::SetLineWidth                DontDelete|Function 1
  setLineCap               Context2D::SetLineCap                  DontDelete|Function 1
  setLineJoin              Context2D::SetLineJoin                 DontDelete|Function 1
  setMiterLimit            Context2D::SetMiterLimit               DontDelete|Function 1
  fill                     Context2D::Fill                        DontDelete|Function 0
  stroke                   Context2D::Stroke                      DontDelete|Function 0
  moveTo                   Context2D::MoveTo                      DontDelete|Function 2
  lineTo                   Context2D::LineTo                      DontDelete|Function 2
  quadraticCurveTo         Context2D::QuadraticCurveTo            DontDelete|Function 4
  bezierCurveTo            Context2D::BezierCurveTo               DontDelete|Function 6
  arcTo                    Context2D::ArcTo                       DontDelete|Function 5
  arc                      Context2D::Arc                         DontDelete|Function 6
  rect                     Context2D::Rect                        DontDelete|Function 4
  clip                     Context2D::Clip                        DontDelete|Function 0
  clearRect                Context2D::ClearRect                   DontDelete|Function 4
  fillRect                 Context2D::FillRect                    DontDelete|Function 4
  strokeRect               Context2D::StrokeRect                  DontDelete|Function 4
  drawImage                Context2D::DrawImage                   DontDelete|Function 6
  drawImageFromRect        Context2D::DrawImageFromRect           DontDelete|Function 10
  setShadow                Context2D::SetShadow                   DontDelete|Function 3
  clearShadow              Context2D::ClearShadow                 DontDelete|Function 0
  setAlpha                 Context2D::SetAlpha                    DontDelete|Function 1
  setCompositeOperation    Context2D::SetCompositeOperation       DontDelete|Function 1
  createLinearGradient     Context2D::CreateLinearGradient        DontDelete|Function 4
  createRadialGradient     Context2D::CreateRadialGradient        DontDelete|Function 6
  createPattern            Context2D::CreatePattern               DontDelete|Function 2
@end
*/

bool Context2D::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    // FIXME: functions should be on the prototype, not in the object itself
    return getStaticPropertySlot<Context2DFunction, Context2D, DOMObject>(exec, &Context2DTable, this, propertyName, slot);
}

ValueImp *Context2D::getValueProperty(ExecState *, int token) const
{
    switch(token) {
        case StrokeStyle: {
            return _strokeStyle;
        }
        
        case FillStyle: {
            return _fillStyle;
        }
        
        case LineWidth: {
            return _lineWidth;
        }
        
        case LineCap: {
            return _lineCap;
        }
        
        case LineJoin: {
            return _lineJoin;
        }
        
        case MiterLimit: {
            return _miterLimit;
        }
        
        case ShadowOffsetX: {
            return _shadowOffsetX;
        }
        
        case ShadowOffsetY: {
            return _shadowOffsetY;
        }
        
        case ShadowBlur: {
            return _shadowBlur;
        }
        
        case ShadowColor: {
            return _shadowColor;
        }
        
        case GlobalAlpha: {
            return _globalAlpha;
        }
        
        case GlobalCompositeOperation: {
            return _globalComposite;
        }
        
        default: {
        }
    }
    
    return Undefined();
}

void Context2D::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
    lookupPut<Context2D,DOMObject>(exec, propertyName, value, attr, &Context2DTable, this );
}

CGContextRef Context2D::drawingContext()
{
    khtml::RenderCanvasImage *renderer = static_cast<khtml::RenderCanvasImage*>(_element->renderer());
    if (!renderer)
        return 0;
    
    CGContextRef context = renderer->drawingContext();
    if (!context)
        return 0;
        
    return context;
}


CGColorRef colorRefFromValue(ExecState *exec, ValueImp *value)
{
    CGColorSpaceRef colorSpace;
    float components[4];
    
    if (value->isString()) {
        QRgb color = DOM::CSSParser::parseColor(value->toString(exec).domString());
        QColor qc(color);
        components[0] = qc.red()/255.;
        components[1] = qc.green()/255.;
        components[2] = qc.blue()/255.;
        components[3] = qc.alpha();
        colorSpace = CGColorSpaceCreateDeviceRGB();
    }
    else
        return 0;
    
    CGColorRef colorRef = CGColorCreate (colorSpace, components);
    CFRelease (colorSpace);
    
    return colorRef;
}

QColor colorFromValue(ExecState *exec, ValueImp *value)
{
    QRgb color = DOM::CSSParser::parseColor(value->toString(exec).domString());
    return QColor(color);
}

void Context2D::setShadow(ExecState *exec)
{
    CGContextRef context = drawingContext();
    if (!context)
        return;
    
    CGSize offset;
    offset.width = (float)_shadowOffsetX->toNumber(exec);
    offset.height = (float)_shadowOffsetY->toNumber(exec);
    float blur = (float)_shadowBlur->toNumber(exec);
    CGColorRef colorRef = colorRefFromValue(exec, _shadowColor);
    CGContextSetShadowWithColor (context, offset, blur, colorRef);
    CFRelease (colorRef);
}

void Context2D::updateFillImagePattern()
{
    CGContextRef context = drawingContext();
    CGAffineTransform transform = CGContextGetCTM(context);
    
    if (!_validFillImagePattern || !CGAffineTransformEqualToTransform(transform, _lastFillImagePatternCTM)) {
        ImagePattern *imagePattern = static_cast<ImagePattern *>(_fillStyle);
        CGPatternRef pattern = imagePattern->createPattern(CGContextGetCTM(context));
        float patternAlpha = 1;
        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(0);
        CGContextSetFillColorSpace(context, patternSpace);
        CGContextSetFillPattern(context, pattern, &patternAlpha);
        CGColorSpaceRelease(patternSpace);
        CGPatternRelease(pattern);
        _validFillImagePattern = true;
        _lastFillImagePatternCTM = transform;
    }
}

void Context2D::updateStrokeImagePattern()
{
    CGContextRef context = drawingContext();
    CGAffineTransform transform = CGContextGetCTM(context);
    
    if (!_validStrokeImagePattern || !CGAffineTransformEqualToTransform(transform, _lastStrokeImagePatternCTM)) {
        ImagePattern *imagePattern = static_cast<ImagePattern *>(_fillStyle);
        CGPatternRef pattern = imagePattern->createPattern(CGContextGetCTM(context));
        float patternAlpha = 1;
        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(0);
        CGContextSetStrokeColorSpace(context, patternSpace);
        CGContextSetStrokePattern(context, pattern, &patternAlpha);
        CGColorSpaceRelease(patternSpace);
        CGPatternRelease(pattern);
        _validStrokeImagePattern = true;
        _lastStrokeImagePatternCTM = transform;
    }
}

void Context2D::putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/)
{
    CGContextRef context = drawingContext();
    if (!context)
        return;
    
    switch(token) {
        case StrokeStyle: {
            _strokeStyle = value;
            if (value->isString()) {
                QColor qc = colorFromValue(exec, value);
                CGContextSetRGBStrokeColor(context, qc.red()/255., qc.green()/255., qc.blue()/255., qc.alpha()/255.);
            }
            else {
                // _strokeStyle is used when stroke() is called on the context.
                // CG doesn't have the notion of a setting a stroke gradient.
                ObjectImp *o = static_cast<ObjectImp*>(value);
                
                if (!o->isObject() || !(o->inherits(&Gradient::info) || o->inherits(&ImagePattern::info)))
                    throwError(exec, TypeError);
            }
            break;
        }
        
        case FillStyle: {
            _fillStyle = value;
            if (value->isString()) {
                QColor qc = colorFromValue(exec, value);
                CGContextSetRGBFillColor(context, qc.red()/255., qc.green()/255., qc.blue()/255., qc.alpha()/255.);
            }
            else {
                // _fillStyle is checked when fill() is called on the context.
                // CG doesn't have the notion of setting a fill gradient.
                ObjectImp *o = static_cast<ObjectImp*>(value);
                
                if (!o->isObject() || !(o->inherits(&Gradient::info) || o->inherits(&ImagePattern::info)))
                    throwError(exec, TypeError);

                // Gradients and image patterns are constructed when needed during fill and stroke operations.
            }
            break;
        }
        
        case LineWidth: {
            _lineWidth = value;
            float w = (float)value->toNumber(exec);
            CGContextSetLineWidth (context, w);
            break;
        }
        
        case LineCap: {
            _lineCap = value;
        
            CGLineCap cap = kCGLineCapButt;
            QString capString = value->toString(exec).qstring().lower();
            if (capString == "round")
                cap = kCGLineCapRound;
            else if (capString == "square")
                cap = kCGLineCapSquare;
            CGContextSetLineCap (context, cap);
            break;
        }
        
        case LineJoin: {
            _lineJoin = value;
            
            CGLineJoin join = kCGLineJoinMiter;
            QString joinString = value->toString(exec).qstring().lower();
            if (joinString == "round")
                join = kCGLineJoinRound;
            else if (joinString == "bevel")
                join = kCGLineJoinBevel;
            CGContextSetLineJoin (context, join);
            break;
        }
        
        case MiterLimit: {
            _miterLimit = value;
            
            float l = (float)value->toNumber(exec);
            CGContextSetMiterLimit (context, l);
            break;
        }
        
        case ShadowOffsetX: {
            _shadowOffsetX = value;
            setShadow(exec);
            break;
        }
        
        case ShadowOffsetY: {
            _shadowOffsetY = value;
            setShadow(exec);
            break;
        }
        
        case ShadowBlur: {
            _shadowBlur = value;
            setShadow(exec);
            break;
        }
        
        case ShadowColor: {
            _shadowColor = value;
            setShadow(exec);
            break;
        }
        
        case GlobalAlpha: {
            _globalAlpha = value;
            float a =  (float)value->toNumber(exec);
            CGContextSetAlpha (context, a);
            break;
        }
        
        case GlobalCompositeOperation: {
            _globalComposite = value;
            QString compositeOperator = value->toString(exec).qstring().lower();
            QPainter::setCompositeOperation (context, compositeOperator);
            break;
        }
        
        default: {
        }
    }
}

void Context2D::save()
{
    List *list = new List();
    
    list->append(_strokeStyle);
    list->append(_fillStyle);
    list->append(_lineWidth);
    list->append(_lineCap);
    list->append(_lineJoin);
    list->append(_miterLimit);
    list->append(_shadowOffsetX);
    list->append(_shadowOffsetY);
    list->append(_shadowBlur);
    list->append(_shadowColor);
    list->append(_globalAlpha);
    list->append(_globalComposite);
    
    stateStack.append(list);
}

void Context2D::restore()
{
    if (stateStack.count() < 1) {
        return;
    }
    
    List *list = stateStack.last();
    
    int pos = 0;
    _strokeStyle = list->at(pos++);
    _fillStyle = list->at(pos++);
    _lineWidth = list->at(pos++);
    _lineCap = list->at(pos++);
    _lineJoin = list->at(pos++);
    _miterLimit = list->at(pos++);
    _shadowOffsetX = list->at(pos++);
    _shadowOffsetY = list->at(pos++);
    _shadowBlur = list->at(pos++);
    _shadowColor = list->at(pos++);
    _globalAlpha = list->at(pos++);
    _globalComposite = list->at(pos++);

    // This will delete list.
    stateStack.removeLast();
}

Context2D::Context2D(HTMLElementImpl *e)
  : _validFillImagePattern(false), _validStrokeImagePattern(false),
    _element(e), _needsFlushRasterCache(false),
    _strokeStyle(jsUndefined()),
    _fillStyle(jsUndefined()),
    _lineWidth(jsUndefined()),
    _lineCap(jsUndefined()),
    _lineJoin(jsUndefined()),
    _miterLimit(jsUndefined()),
    _shadowOffsetX(jsUndefined()),
    _shadowOffsetY(jsUndefined()),
    _shadowBlur(jsUndefined()),
    _shadowColor(jsUndefined()),
    _globalAlpha(jsUndefined()),
    _globalComposite(jsUndefined())
{
    _lineWidth = Number (1.);
    _strokeStyle = String ("black");
    _fillStyle = String ("black");
    
    _lineCap = String("butt");
    _lineJoin = String("miter");
    _miterLimit = Number(10.0);
    
    _shadowOffsetX = Number(0.);
    _shadowOffsetY = Number(0.);
    _shadowBlur = Number(0.);
    _shadowColor = String("black");
        
    _globalAlpha = Number(1.);
    _globalComposite = String("source-over");
    
    stateStack.setAutoDelete(true);
}

Context2D::~Context2D()
{
}

void Context2D::mark()
{
    ValueImp *v;

    v = _strokeStyle;
    if (!v->marked())
        v->mark();

    v = _fillStyle;
    if (!v->marked())
        v->mark();

    v = _lineWidth;
    if (!v->marked())
        v->mark();

    v = _lineCap;
    if (!v->marked())
        v->mark();

    v = _lineJoin;
    if (!v->marked())
        v->mark();

    v = _miterLimit;
    if (!v->marked())
        v->mark();

    v = _shadowOffsetX;
    if (!v->marked())
        v->mark();

    v = _shadowOffsetY;
    if (!v->marked())
        v->mark();

    v = _shadowBlur;
    if (!v->marked())
        v->mark();

    v = _shadowColor;
    if (!v->marked())
        v->mark();

    v = _globalAlpha;
    if (!v->marked())
        v->mark();

    v = _globalComposite;
    if (!v->marked())
        v->mark();

    QPtrListIterator<List> it(stateStack);
    List *list;
    while ((list = it.current())) {
        list->mark();
        ++it;
    }
    
    DOMObject::mark();
}

const ClassInfo KJS::Gradient::info = { "Gradient", 0, &GradientTable, 0 };

/* Source for GradientTable. Use "make hashtables" to regenerate.
@begin GradientTable 1
  addColorStop             Gradient::AddColorStop                DontDelete|Function 2
@end
*/

IMPLEMENT_PROTOFUNC(GradientFunction)

ValueImp *GradientFunction::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
    if (!thisObj->inherits(&Gradient::info))
        return throwError(exec, TypeError);

    Gradient *gradient = static_cast<KJS::Gradient *>(thisObj);

    switch (id) {
        case Gradient::AddColorStop: {
            if (args.size() != 2)
                return throwError(exec, SyntaxError);

            QColor color = colorFromValue(exec, args[1]);
            gradient->addColorStop ((float)args[0]->toNumber(exec), color.red()/255.f, color.green()/255.f, color.blue()/255.f, color.alpha()/255.f);
        }
    }

    return Undefined();
}

void gradientCallback (void *info, const float *in, float *out)
{
    Gradient *gradient = static_cast<Gradient*>(info);
    int numStops;
    const ColorStop *stops = gradient->colorStops(&numStops);
    float current = *in;
    
    assert (numStops >= 2);
    
    if (current == 0) {
        gradient->lastStop = 0;
        gradient->nextStop = 1;

        const ColorStop *thisStop = &stops[0];
        *out++ = thisStop->red;
        *out++ = thisStop->green;
        *out++ = thisStop->blue;
        *out = thisStop->alpha;
    }
    else if (current == 1) {
        const ColorStop *thisStop = &stops[numStops-1];
        *out++ = thisStop->red;
        *out++ = thisStop->green;
        *out++ = thisStop->blue;
        *out = thisStop->alpha;
    }
    else {
        if (current >= stops[gradient->nextStop].stop) {
            gradient->lastStop = gradient->nextStop;
            gradient->nextStop = gradient->lastStop+1;
        }
        
        // Add an interpolation for each component between
        // stops.
        const ColorStop *nextStop = &stops[gradient->nextStop];
        const ColorStop *lastStop = &stops[gradient->lastStop];
        
        float stopDelta = nextStop->stop - lastStop->stop;
        float stopOffset = current - lastStop->stop;
        float stopPercent = stopOffset/stopDelta;
        
        *out++ = lastStop->red + (nextStop->red - lastStop->red) * stopPercent;
        *out++ = lastStop->green + (nextStop->green - lastStop->green) * stopPercent;
        *out++ = lastStop->blue + (nextStop->blue - lastStop->blue) * stopPercent;
        *out = lastStop->alpha + (nextStop->alpha - lastStop->alpha) * stopPercent;
    }
}

static float intervalRangeDomin[] = { 0.f, 1.f };
static float colorComponentRangeDomains[] = { 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f };
CGFunctionCallbacks gradientCallbacks = {
    0, gradientCallback, NULL
};

void Gradient::commonInit()
{
    stops = 0;
    stopCount = 0;
    maxStops = 0;
    stopsNeedAdjusting = false;
    adjustedStopCount = 0;
    adjustedStops = 0;
    
    _shadingRef = 0;
    regenerateShading = true;
}

Gradient::Gradient(float x0, float y0, float x1, float y1)
{
    _gradientType = Gradient::Linear;
    _x0 = x0;
    _y0 = y0;
    _x1 = x1;
    _y1 = y1;

    commonInit();
}

Gradient::Gradient(float x0, float y0, float r0, float x1, float y1, float r1)
{
    _gradientType = Gradient::Radial;
    _x0 = x0;
    _y0 = y0;
    _r0 = r0;
    _x1 = x1;
    _y1 = y1;
    _r1 = r1;

    commonInit();
}

bool Gradient::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticPropertySlot<GradientFunction, Gradient, DOMObject>(exec, &GradientTable, this, propertyName, slot);
}

ValueImp *Gradient::getValueProperty(ExecState *, int token) const
{
    return Undefined();
}

void Gradient::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
    lookupPut<Gradient,DOMObject>(exec, propertyName, value, attr, &GradientTable, this );
}

void Gradient::putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/)
{
}

Gradient::~Gradient()
{
    if (_shadingRef) {
        CGShadingRelease(_shadingRef);
        _shadingRef = 0;
    }
    
    fastFree(stops);
    stops = 0;
    
    fastFree(adjustedStops);
    adjustedStops = 0;
}

CGShadingRef Gradient::getShading()
{
    if (!regenerateShading)
        return _shadingRef;
    
    if (_shadingRef)
        CGShadingRelease (_shadingRef);
        
    CGFunctionRef _colorFunction = CGFunctionCreate((void *)this, 1, intervalRangeDomin, 4, colorComponentRangeDomains, &gradientCallbacks);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    
    if (_gradientType == Gradient::Radial) {    
        _shadingRef = CGShadingCreateRadial(colorSpace, CGPointMake(_x0,_y0), _r0, CGPointMake(_x1,_y1), _r1, _colorFunction, true, true);
    }
    else {    
        _shadingRef = CGShadingCreateAxial(colorSpace, CGPointMake(_x0,_y0), CGPointMake(_x1,_y1), _colorFunction, true, true);
    }
    
    CGColorSpaceRelease (colorSpace);
    CGFunctionRelease (_colorFunction);
    
    regenerateShading = false;
    
    return _shadingRef;
}

void Gradient::addColorStop (float s, float r, float g, float b, float a)
{
    if (stopCount == 0) {
        maxStops = 4;
        stops = (ColorStop *)fastMalloc(maxStops * sizeof(ColorStop));
    }
    else if (stopCount+1 > maxStops) {
        maxStops *= 2;
        stops = (ColorStop *)fastRealloc(stops, maxStops * sizeof(ColorStop));
    }
    stops[stopCount++] = ColorStop (s, r, g, b, a);
    stopsNeedAdjusting = true;
}

static int sortStops(const ColorStop *a, const ColorStop *b)
{
    if (a->stop > b->stop)
        return 1;
    else if (a->stop < b->stop)
        return -1;
    return 0;
}

// Return a sorted array of stops guaranteed to contain a 0 and 1 stop.
const ColorStop *Gradient::colorStops(int *count) const
{
    if (stopsNeedAdjusting || !stops) {
        stopsNeedAdjusting = false;
        
        bool haveZeroStop = false;
        bool haveOneStop = false;
        if (stops) {
            qsort (stops, stopCount, sizeof(ColorStop), (int (*)(const void*, const void*))sortStops);
    
            // Is there a zero stop?
            haveZeroStop = (stops[0].stop == 0.f);

            // Is there a one stop.  If not add one at the end.
            haveOneStop = (stopCount > 0 && stops[stopCount-1].stop == 1.f);
        }
        
        adjustedStopCount = stopCount;
        if (!haveZeroStop)
            adjustedStopCount++;
        if (!haveOneStop)
            adjustedStopCount++;
            
        if (adjustedStopCount != stopCount) {
            adjustedStops = (ColorStop *)fastMalloc(adjustedStopCount * sizeof(ColorStop));
            memcpy (haveZeroStop ? adjustedStops : adjustedStops+1,
                stops, stopCount*sizeof(ColorStop));

            // If not specified use default start (stop 0) and end (stop 1) colors.
            // This color will be transparent black.
            if (!haveZeroStop) {
                adjustedStops[0] = ColorStop(0.f, 0.f, 0.f, 0.f, 1.f);
            }
            if (!haveOneStop) {
                adjustedStops[adjustedStopCount-1] = ColorStop(1.f, 0.f, 0.f, 0.f, 1.f);
            }
        }
        
        regenerateShading = true;
    }
            
    if (adjustedStops) {
        *count = adjustedStopCount;
        return adjustedStops;
    }
        
    *count = stopCount;
    return stops;
}

const ClassInfo ImagePattern::info = { "ImagePattern", 0, &ImagePatternTable, 0 };

/* Source for ImagePatternTable. Use "make hashtables" to regenerate.
@begin ImagePatternTable 0
@end
*/

static void drawPattern (void * info, CGContextRef context)
{
    ImagePattern *pattern = static_cast<ImagePattern*>(info);
    QPainter p;
    QPixmap pixmap = pattern->pixmap();
    float w = pixmap.width();
    float h = pixmap.height();
    
    // Try and draw bitmap directly
    CGImageRef ref = pixmap.imageRef();
    if (ref) {
        CGContextDrawImage (context, CGRectMake(0, 0, w, h), ref);    
    }
    else
        p.drawFloatPixmap (0, 0, w, h, pixmap, 0.f, 0.f, w, h, 
            QPainter::compositeOperatorFromString(QString("source-over")), context);
}

CGPatternCallbacks patternCallbacks = { 0, drawPattern, NULL };
ImagePattern::ImagePattern(Image *i, int repetitionType)
    :_rw(0), _rh(0)
{
    khtml::CachedImage *ci = i->image();
    if (ci) {
        _pixmap = ci->pixmap();
        float w = _pixmap.width();
        float h = _pixmap.height();
        _bounds = CGRectMake (0, 0, w, h);

        if (repetitionType == Repeat) {
            _rw = w; _rh = h;
        }
        else if (repetitionType == RepeatX) {
            _rw = w; _rh = 0;
        }
        else if (repetitionType == RepeatY) {
            _rw = 0; _rh = h;
        }
        else if (repetitionType == NoRepeat) {
            _rw = 0; _rh = 0;
        }
    }
}

CGPatternRef ImagePattern::createPattern(CGAffineTransform transform)
{
    if (_pixmap.isNull())
        return 0;
          
    CGAffineTransform patternTransform = transform;
    patternTransform = CGAffineTransformScale(patternTransform, -1, -1);
    patternTransform = CGAffineTransformTranslate(patternTransform, 0, -_pixmap.height());

    return CGPatternCreate(this, _bounds, patternTransform, _rw, _rh, kCGPatternTilingConstantSpacing, true, &patternCallbacks);
}

bool ImagePattern::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<ImagePattern, DOMObject>(exec, &ImagePatternTable, this, propertyName, slot);
}

ValueImp *ImagePattern::getValueProperty(ExecState *, int token) const
{
    return Undefined();
}

void ImagePattern::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
    lookupPut<ImagePattern,DOMObject>(exec, propertyName, value, attr, &ImagePatternTable, this );
}

void ImagePattern::putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/)
{
}

////////////////////////////////////////////////////////////////
                     

ValueImp *getHTMLCollection(ExecState *exec, HTMLCollectionImpl *c)
{
  return cacheDOMObject<HTMLCollectionImpl, HTMLCollection>(exec, c);
}

ValueImp *getSelectHTMLCollection(ExecState *exec, HTMLCollectionImpl *c, HTMLSelectElementImpl *e)
{
  DOMObject *ret;
  if (!c)
    return Null();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());
  if ((ret = interp->getDOMObject(c)))
    return ret;
  else {
    ret = new HTMLSelectCollection(exec, c, e);
    interp->putDOMObject(c,ret);
    return ret;
  }
}

} // namespace
