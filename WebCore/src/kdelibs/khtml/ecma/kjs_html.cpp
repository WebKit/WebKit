// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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

#include <stdio.h>

#include <qptrdict.h>

#include <khtml_part.h>
#include <misc/loader.h>
#include <dom/html_base.h>
#include <dom/html_block.h>
#include <dom/html_document.h>
#include <dom/html_element.h>
#include <dom/html_form.h>
#include <dom/html_head.h>
#include <dom/html_image.h>
#include <dom/html_inline.h>
#include <dom/html_list.h>
#include <dom/html_misc.h>
#include <dom/html_table.h>
#include <dom/html_object.h>
#include <dom/dom_node.h>
#include <dom_string.h>
#include <dom_exception.h>

// ### HACK
#include <html/html_baseimpl.h>
#include <xml/dom_docimpl.h>
#include <xml/dom2_eventsimpl.h>
#include <khtmlview.h>

#include <kjs/operations.h>
#include "kjs_dom.h"
#include "kjs_html.h"
#include "kjs_window.h"
#include "kjs_events.h"

#include <htmltags.h>
#include <kdebug.h>

using namespace KJS;

QPtrDict<KJS::HTMLCollection> htmlCollections;

KJSO KJS::HTMLDocFunction::tryGet(const UString &p) const
{
  DOM::HTMLCollection coll;

  switch (id) {
  case Images:
    coll = doc.images();
    break;
  case Applets:
    coll = doc.applets();
    break;
  case Links:
    coll = doc.links();
    break;
  case Forms:
    coll = doc.forms();
    break;
  case Anchors:
    coll = doc.anchors();
    break;
  case All:  // IE specific, not part of the DOM
    coll = doc.all();
    break;
  default:
    return Undefined();
  }

  KJSO tmp(new KJS::HTMLCollection(coll));

  return tmp.get(p);
}

Completion KJS::HTMLDocFunction::tryExecute(const List &args)
{
  KJSO result;
  String s;
  DOM::HTMLElement element;
  DOM::HTMLCollection coll;

  KJSO v = args[0];

  switch (id) {
  case Images:
    coll = doc.images();
    break;
  case Applets:
    coll = doc.applets();
    break;
  case Links:
    coll = doc.links();
    break;
  case Forms:
    coll = doc.forms();
    break;
  case Anchors:
    coll = doc.anchors();
    break;
  case All:
    coll = doc.all();
    break;
  case Open:
    // this is just a dummy function,  has no purpose anymore
    //doc.open();
    result = Undefined();
    break;
  case Close:
    // this is just a dummy function,  has no purpose
    // see khtmltests/ecma/tokenizer-script-recursion.html
    // doc.close();
    result = Undefined();
    break;
  case Write:
  case WriteLn: {
    // DOM only specifies single string argument, but NS & IE allow multiple
    UString str = v.toString().value();
    for (int i = 1; i < args.size(); i++)
      str += args[i].toString().value();
    if (id == WriteLn)
      str += "\n";
    doc.write(str.string());
    result = Undefined();
    break;
  }
  case GetElementById:
    s = v.toString();
    result = getDOMNode(doc.getElementById(s.value().string()));
    break;
  case GetElementsByName:
    s = v.toString();
    result = getDOMNodeList(doc.getElementsByName(s.value().string()));
    break;
  }

  // retrieve element from collection. Either by name or indexed.
  if (id == Images || id == Applets || id == Links ||
      id == Forms || id == Anchors || id == All) {
    bool ok;
    UString s = args[0].toString().value();
    unsigned int u = s.toULong(&ok);
    if (ok)
      element = coll.item(u);
    else
      element = coll.namedItem(s.string());
    result = getDOMNode(element);
  }

  return Completion(ReturnValue, result);
}

const TypeInfo KJS::HTMLDocument::info = { "HTMLDocument", HostType,
					   &DOMDocument::info, 0, 0 };

bool KJS::HTMLDocument::hasProperty(const UString &p, bool recursive) const
{
  if (p == "title" || p == "referrer" || p == "domain" || p == "URL" ||
      p == "body" || p == "location" || p == "images" || p == "applets" ||
      p == "links" || p == "forms" || p == "anchors" || p == "all" ||
      p == "cookie" || p == "open" || p == "close" || p == "write" ||
      p == "writeln" || p == "getElementById" || p == "getElementsByName")
    return true;
  if (!static_cast<DOM::HTMLDocument>(node).all().
      namedItem(p.string()).isNull())
    return true;
  return recursive && DOMDocument::hasProperty(p, true);
}

KJSO KJS::HTMLDocument::tryGet(const UString &p) const
{
  //kdDebug() << "KJS::HTMLDocument::get " << p.qstring() << endl;
  DOM::HTMLDocument doc = static_cast<DOM::HTMLDocument>(node);

  // image and form elements with the name p will be looked up first
  DOM::HTMLCollection coll = doc.all();
  DOM::HTMLElement element = coll.namedItem(p.string());
  if (!element.isNull() &&
      (element.elementId() == ID_IMG || element.elementId() == ID_FORM))
    return getDOMNode(element);

  if (p == "title")
    return getString(doc.title());
  else if (p == "referrer")
    return String(doc.referrer());
  else if (p == "domain")
    return String(doc.domain());
  else if (p == "URL")
    return getString(doc.URL());
  else if (p == "body")
    return getDOMNode(doc.body());
  else if (p == "location") {
    KHTMLPart *part = static_cast<DOM::DocumentImpl*>(doc.handle())->
		      view()->part();
    return Window::retrieveWindow(part)->location();
  } else if (p == "images")
    return new HTMLDocFunction(doc, HTMLDocFunction::Images);
  else if (p == "applets")
    return new HTMLDocFunction(doc, HTMLDocFunction::Applets);
  else if (p == "links")
    return new HTMLDocFunction(doc, HTMLDocFunction::Links);
  else if (p == "forms")
    return new HTMLDocFunction(doc, HTMLDocFunction::Forms);
  else if (p == "anchors")
    return new HTMLDocFunction(doc, HTMLDocFunction::Anchors);
  else if (p == "all")
    return new HTMLDocFunction(doc, HTMLDocFunction::All);
  else if (p == "cookie")
    return String(doc.cookie());
  else if (DOMDocument::hasProperty(p))	// expandos override functions
    return DOMDocument::tryGet(p);
  else if (p == "open")
    return new HTMLDocFunction(doc, HTMLDocFunction::Open);
  else if (p == "close")
    return new HTMLDocFunction(doc, HTMLDocFunction::Close);
  else if (p == "write")
    return new HTMLDocFunction(doc, HTMLDocFunction::Write);
  else if (p == "writeln")
    return new HTMLDocFunction(doc, HTMLDocFunction::WriteLn);
  else if (p == "getElementById")
    return new HTMLDocFunction(doc, HTMLDocFunction::GetElementById);
  else if (p == "getElementsByName")
    return new HTMLDocFunction(doc, HTMLDocFunction::GetElementsByName);
  else {
    if(!element.isNull())
      return getDOMNode(element);
    return Undefined();
  }
}

void KJS::HTMLDocument::tryPut(const UString &p, const KJSO& v)
{
  DOM::HTMLDocument doc = static_cast<DOM::HTMLDocument>(node);

  if (p == "title")
    doc.setTitle(v.toString().value().string());
  else if (p == "body")
    doc.setBody((new DOMNode(KJS::toNode(v)))->toNode());
  else if (p == "cookie")
    doc.setCookie(v.toString().value().string());
  else if (p == "location") {
    KHTMLPart *part = static_cast<DOM::DocumentImpl *>( doc.handle() )->view()->part();
    QString str = v.toString().value().qstring();
    part->scheduleRedirection(0, str);
  }
  else if (p == "onclick")
    doc.handle()->setHTMLEventListener(DOM::EventImpl::KHTML_CLICK_EVENT,Window::retrieveActive()->getJSEventListener(v,true));
  else if (p == "ondblclick")
    doc.handle()->setHTMLEventListener(DOM::EventImpl::KHTML_DBLCLICK_EVENT,Window::retrieveActive()->getJSEventListener(v,true));
  else if (p == "onkeydown")
    doc.handle()->setHTMLEventListener(DOM::EventImpl::KHTML_KEYDOWN_EVENT,Window::retrieveActive()->getJSEventListener(v,true));
  else if (p == "onkeypress")
    doc.handle()->setHTMLEventListener(DOM::EventImpl::KHTML_KEYPRESS_EVENT,Window::retrieveActive()->getJSEventListener(v,true));
  else if (p == "onkeyup")
    doc.handle()->setHTMLEventListener(DOM::EventImpl::KHTML_KEYUP_EVENT,Window::retrieveActive()->getJSEventListener(v,true));
  else if (p == "onmousedown")
    doc.handle()->setHTMLEventListener(DOM::EventImpl::MOUSEDOWN_EVENT,Window::retrieveActive()->getJSEventListener(v,true));
  else if (p == "onmouseup")
    doc.handle()->setHTMLEventListener(DOM::EventImpl::MOUSEUP_EVENT,Window::retrieveActive()->getJSEventListener(v,true));
  else
    DOMDocument::tryPut(p,v);
}

// -------------------------------------------------------------------------

const TypeInfo KJS::HTMLElement::info = { "HTMLElement", HostType,
					  &DOMElement::info, 0, 0 };

KJSO KJS::HTMLElement::tryGet(const UString &p) const
{
  DOM::HTMLElement element = static_cast<DOM::HTMLElement>(node);
  //kdDebug() << "KJS::HTMLElement::tryGet " << p.qstring() << " id=" << element.elementId() << endl;

  switch (element.elementId()) {
    case ID_HTML: {
      DOM::HTMLHtmlElement html = element;
      if      (p == "version")         return getString(html.version());
    }
    break;
    case ID_HEAD: {
      DOM::HTMLHeadElement head = element;
      if      (p == "profile")         return getString(head.profile());
    }
    break;
    case ID_LINK: {
      DOM::HTMLLinkElement link = element;
      if      (p == "disabled")        return Boolean(link.disabled());
      else if (p == "charset")         return getString(link.charset());
      else if (p == "href")            return getString(link.href());
      else if (p == "hreflang")        return getString(link.hreflang());
      else if (p == "media")           return getString(link.media());
      else if (p == "rel")             return getString(link.rel());
      else if (p == "rev")             return getString(link.rev());
      else if (p == "target")          return getString(link.target());
      else if (p == "type")            return getString(link.type());
      else if (p == "sheet")           return getDOMStyleSheet(static_cast<DOM::ProcessingInstruction>(node).sheet());
    }
    break;
    case ID_TITLE: {
      DOM::HTMLTitleElement title = element;
      if (p == "text")                 return getString(title.text());
    }
    break;
    case ID_META: {
      DOM::HTMLMetaElement meta = element;
      if      (p == "content")         return getString(meta.content());
      else if (p == "httpEquiv")       return getString(meta.httpEquiv());
      else if (p == "name")            return getString(meta.name());
      else if (p == "scheme")          return getString(meta.scheme());
    }
    break;
    case ID_BASE: {
      DOM::HTMLBaseElement base = element;
      if      (p == "href")            return getString(base.href());
      else if (p == "target")          return getString(base.target());
    }
    break;
    case ID_ISINDEX: {
      DOM::HTMLIsIndexElement isindex = element;
      if      (p == "form")            return getDOMNode(isindex.form()); // type HTMLFormElement
      else if (p == "prompt")          return getString(isindex.prompt());
    }
    break;
    case ID_STYLE: {
      DOM::HTMLStyleElement style = element;
      if      (p == "disabled")        return Boolean(style.disabled());
      else if (p == "media")           return getString(style.media());
      else if (p == "type")            return getString(style.type());
      else if (p == "sheet")           return getDOMStyleSheet(static_cast<DOM::ProcessingInstruction>(node).sheet());
    }
    break;
    case ID_BODY: {
      DOM::HTMLBodyElement body = element;
      if      (p == "aLink")           return getString(body.aLink());
      else if (p == "background")      return getString(body.background());
      else if (p == "bgColor")         return getString(body.bgColor());
      else if (p == "link")            return getString(body.link());
      else if (p == "text")            return getString(body.text());
      else if (p == "vLink")           return getString(body.vLink());
      else if (p == "scrollHeight" )   return Number(body.ownerDocument().view() ? body.ownerDocument().view()->contentsHeight() : 0);
      else if (p == "scrollWidth" )    return Number(body.ownerDocument().view() ? body.ownerDocument().view()->contentsWidth() : 0);
    }
    break;
    case ID_FORM: {
      DOM::HTMLFormElement form = element;
      DOM::Node n = form.elements().namedItem(p.string());
      if(!n.isNull())  return getDOMNode(n);
      else if (p == "elements")        return getHTMLCollection(form.elements());
      else if (p == "length")          return Number(form.length());
      else if (p == "name")            return getString(form.name());
      else if (p == "acceptCharset")   return getString(form.acceptCharset());
      else if (p == "action")          return getString(form.action());
      else if (p == "enctype")         return getString(form.enctype());
      else if (p == "method")          return getString(form.method());
      else if (p == "target")          return getString(form.target());
      // methods
      else if (p == "submit")          return new HTMLElementFunction(element,HTMLElementFunction::Submit);
      else if (p == "reset")           return new HTMLElementFunction(element,HTMLElementFunction::Reset);
      else
        return DOMElement::tryGet(p);
    }
    break;
    case ID_SELECT: {
      DOM::HTMLSelectElement select = element;
      if      (p == "type")            return getString(select.type());
      else if (p == "selectedIndex")   return Number(select.selectedIndex());
      else if (p == "value")           return getString(select.value());
      else if (p == "length")          return Number(select.length());
      else if (p == "form")            return getDOMNode(select.form()); // type HTMLFormElement
      else if (p == "options")         return getSelectHTMLCollection(select.options(), select); // type HTMLCollection
      else if (p == "disabled")        return Boolean(select.disabled());
      else if (p == "multiple")        return Boolean(select.multiple());
      else if (p == "name")            return getString(select.name());
      else if (p == "size")            return Number(select.size());
      else if (p == "tabIndex")        return Number(select.tabIndex());
      // methods
      else if (p == "add")             return new HTMLElementFunction(element,HTMLElementFunction::Add);
      else if (p == "remove")          return new HTMLElementFunction(element,HTMLElementFunction::Remove);
      else if (p == "blur")            return new HTMLElementFunction(element,HTMLElementFunction::Blur);
      else if (p == "focus")           return new HTMLElementFunction(element,HTMLElementFunction::Focus);
      else {
	bool ok;
	uint u = p.toULong(&ok);
	if (ok)
	  return getDOMNode(select.options().item(u)); // not specified by DOM(?) but supported in netscape/IE
      }
    }
    break;
    case ID_OPTGROUP: {
      DOM::HTMLOptGroupElement optgroup = element;
      if      (p == "disabled")        return Boolean(optgroup.disabled());
      else if (p == "label")           return getString(optgroup.label());
    }
    break;
    case ID_OPTION: {
      DOM::HTMLOptionElement option = element;
      if      (p == "form")            return getDOMNode(option.form()); // type HTMLFormElement
      else if (p == "defaultSelected") return Boolean(option.defaultSelected());
      else if (p == "text")            return getString(option.text());
      else if (p == "index")           return Number(option.index());
      else if (p == "disabled")        return Boolean(option.disabled());
      else if (p == "label")           return getString(option.label());
      else if (p == "selected")        return Boolean(option.selected());
      else if (p == "value")           return getString(option.value());
    }
    break;
    case ID_INPUT: {
      DOM::HTMLInputElement input = element;
      if      (p == "defaultValue")    return getString(input.defaultValue());
      else if (p == "defaultChecked")  return Boolean(input.defaultChecked());
      else if (p == "form")            return getDOMNode(input.form()); // type HTMLFormElement
      else if (p == "accept")          return getString(input.accept());
      else if (p == "accessKey")       return getString(input.accessKey());
      else if (p == "align")           return getString(input.align());
      else if (p == "alt")             return getString(input.alt());
      else if (p == "checked")         return Boolean(input.checked());
      else if (p == "disabled")        return Boolean(input.disabled());

      else if (p == "length") {
        // SLOOOOOOW
        DOM::HTMLCollection c( input.form().elements() );
        unsigned long len = 0;
        for ( unsigned long i = 0; i < c.length(); i++ )
          if ( static_cast<DOM::Element>( c.item( i ) ).getAttribute( "name" ) == input.name() )
            len++;
        return Number(len);
      }
      else if (p == "maxLength")       return Number(input.maxLength());
      else if (p == "name")            return getString(input.name());
      else if (p == "readOnly")        return Boolean(input.readOnly());
      else if (p == "size")            return getString(input.size());
      else if (p == "src")             return getString(input.src());
      else if (p == "tabIndex")        return Number(input.tabIndex());
      else if (p == "type")            return getString(input.type());
      else if (p == "useMap")          return getString(input.useMap());
      else if (p == "value")           return getString(input.value());
      // methods
      else if (p == "blur")            return new HTMLElementFunction(element,HTMLElementFunction::Blur);
      else if (p == "focus")           return new HTMLElementFunction(element,HTMLElementFunction::Focus);
      else if (p == "select")          return new HTMLElementFunction(element,HTMLElementFunction::Select);
      else if (p == "click")           return new HTMLElementFunction(element,HTMLElementFunction::Click);
      else {
        // ### SLOOOOOOOW
        bool ok;
        uint u = p.toULong(&ok);
        if ( !ok ) break;

        DOM::HTMLCollection c( input.form().elements() );
         for ( unsigned long i = 0; i < c.length(); i++ )
           if ( static_cast<DOM::Element>( c.item( i ) ).getAttribute( "name" ) == input.name() )
             if ( u-- == 0 )
               return getDOMNode( c.item( i ) );
      }
    }
    break;
    case ID_TEXTAREA: {
      DOM::HTMLTextAreaElement textarea = element;
      if      (p == "defaultValue")    return getString(textarea.defaultValue());
      else if (p == "form")            return getDOMNode(textarea.form()); // type HTMLFormElement
      else if (p == "accessKey")       return getString(textarea.accessKey());
      else if (p == "cols")            return Number(textarea.cols());
      else if (p == "disabled")        return Boolean(textarea.disabled());
      else if (p == "name")            return getString(textarea.name());
      else if (p == "readOnly")        return Boolean(textarea.readOnly());
      else if (p == "rows")            return Number(textarea.rows());
      else if (p == "tabIndex")        return Number(textarea.tabIndex());
      else if (p == "type")            return getString(textarea.type());
      else if (p == "value")           return getString(textarea.value());
      // methods
      else if (p == "blur")            return new HTMLElementFunction(element,HTMLElementFunction::Blur);
      else if (p == "focus")           return new HTMLElementFunction(element,HTMLElementFunction::Focus);
      else if (p == "select")          return new HTMLElementFunction(element,HTMLElementFunction::Select);
    }
    break;
    case ID_BUTTON: {
      DOM::HTMLButtonElement button = element;
      if      (p == "form")            return getDOMNode(button.form()); // type HTMLFormElement
      else if (p == "accessKey")       return getString(button.accessKey());
      else if (p == "disabled")        return Boolean(button.disabled());
      else if (p == "name")            return getString(button.name());
      else if (p == "tabIndex")        return Number(button.tabIndex());
      else if (p == "type")            return getString(button.type());
      else if (p == "value")           return getString(button.value());
    }
    break;
    case ID_LABEL: {
      DOM::HTMLLabelElement label = element;
      if      (p == "form")            return getDOMNode(label.form()); // type HTMLFormElement
      else if (p == "accessKey")       return getString(label.accessKey());
      else if (p == "htmlFor")         return getString(label.htmlFor());
    }
    break;
    case ID_FIELDSET: {
      DOM::HTMLFieldSetElement fieldSet = element;
      if      (p == "form")            return getDOMNode(fieldSet.form()); // type HTMLFormElement
    }
    break;
    case ID_LEGEND: {
      DOM::HTMLLegendElement legend = element;
      if      (p == "form")            return getDOMNode(legend.form()); // type HTMLFormElement
      else if (p == "accessKey")       return getString(legend.accessKey());
      else if (p == "align")           return getString(legend.align());
    }
    break;
    case ID_UL: {
      DOM::HTMLUListElement uList = element;
      if      (p == "compact")         return Boolean(uList.compact());
      else if (p == "type")            return getString(uList.type());
    }
    break;
    case ID_OL: {
      DOM::HTMLOListElement oList = element;
      if      (p == "compact")         return Boolean(oList.compact());
      else if (p == "start")           return Number(oList.start());
      else if (p == "type")            return getString(oList.type());
    }
    break;
    case ID_DL: {
      DOM::HTMLDListElement dList = element;
      if      (p == "compact")         return Boolean(dList.compact());
    }
    break;
    case ID_DIR: {
      DOM::HTMLDirectoryElement directory = element;
      if      (p == "compact")         return Boolean(directory.compact());
    }
    break;
    case ID_MENU: {
      DOM::HTMLMenuElement menu = element;
      if      (p == "compact")         return Boolean(menu.compact());
    }
    break;
    case ID_LI: {
      DOM::HTMLLIElement li = element;
      if      (p == "type")            return getString(li.type());
      else if (p == "value")           return Number(li.value());
    }
    break;
    case ID_DIV: {
      DOM::HTMLDivElement div = element;
      if      (p == "align")           return getString(div.align());
    }
    break;
    case ID_P: {
      DOM::HTMLParagraphElement paragraph = element;
      if      (p == "align")           return getString(paragraph.align());
    }
    break;
    case ID_H1: { // ### H2, H3 ,H4 ,H5 ,H6
      DOM::HTMLHeadingElement heading = element;
      if      (p == "align")           return getString(heading.align());
    }
    break;
    case ID_BLOCKQUOTE: {
      DOM::HTMLBlockquoteElement blockquote = element;
      if      (p == "cite")            return getString(blockquote.cite());
    }
    case ID_Q: {
      DOM::HTMLQuoteElement quote = element;
      if      (p == "cite")            return getString(quote.cite());
    }
    case ID_PRE: {
      DOM::HTMLPreElement pre = element;
      if      (p == "width")           return Number(pre.width());
    }
    break;
    case ID_BR: {
      DOM::HTMLBRElement br = element;
      if      (p == "clear")           return getString(br.clear());
    }
    break;
    case ID_BASEFONT: {
      DOM::HTMLBaseFontElement baseFont = element;
      if      (p == "color")           return getString(baseFont.color());
      else if (p == "face")            return getString(baseFont.face());
      else if (p == "size")            return getString(baseFont.size());
    }
    break;
    case ID_FONT: {
      DOM::HTMLFontElement font = element;
      if      (p == "color")           return getString(font.color());
      else if (p == "face")            return getString(font.face());
      else if (p == "size")            return getString(font.size());
    }
    break;
    case ID_HR: {
      DOM::HTMLHRElement hr = element;
      if      (p == "align")           return getString(hr.align());
      else if (p == "noShade")         return Boolean(hr.noShade());
      else if (p == "size")            return getString(hr.size());
      else if (p == "width")           return getString(hr.width());
    }
    break;
    case ID_INS:
    case ID_DEL: {
      DOM::HTMLModElement mod = element;
      if      (p == "cite")            return getString(mod.cite());
      else if (p == "dateTime")        return getString(mod.dateTime());
    }
    break;
    case ID_A: {
      DOM::HTMLAnchorElement anchor = element;
      if      (p == "accessKey")       return getString(anchor.accessKey());
      else if (p == "charset")         return getString(anchor.charset());
      else if (p == "coords")          return getString(anchor.coords());
      else if (p == "href")            return getString(anchor.href());
      else if (p == "hreflang")        return getString(anchor.hreflang());
      else if (p == "hash")            return getString('#'+KURL(anchor.href().string()).ref());
      else if (p == "host")            return getString(KURL(anchor.href().string()).host());
      else if (p == "hostname") {
        KURL url(anchor.href().string());
        kdDebug(6070) << "anchor::hostname uses:" <<url.url()<<endl;
        if (url.port()==0)
          return getString(url.host());
        else
          return getString(url.host() + ":" + QString::number(url.port()));
      }
      else if (p == "pathname")        return getString(KURL(anchor.href().string()).path());
      else if (p == "port")            return getString(QString::number(KURL(anchor.href().string()).port()));
      else if (p == "protocol")        return getString(KURL(anchor.href().string()).protocol()+":");
      else if (p == "search")          return getString(KURL(anchor.href().string()).query());
      else if (p == "name")            return getString(anchor.name());
      else if (p == "rel")             return getString(anchor.rel());
      else if (p == "rev")             return getString(anchor.rev());
      else if (p == "shape")           return getString(anchor.shape());
      else if (p == "tabIndex")        return Number(anchor.tabIndex());
      else if (p == "target")          return getString(anchor.target());
      else if (p == "text")            return getString(anchor.innerHTML());
      else if (p == "type")            return getString(anchor.type());
      // methods
      else if (p == "blur")            return new HTMLElementFunction(element,HTMLElementFunction::Blur);
      else if (p == "focus")           return new HTMLElementFunction(element,HTMLElementFunction::Focus);
    }
    break;
    case ID_IMG: {
      DOM::HTMLImageElement image = element;
      if      (p == "lowSrc")          return getString(image.lowSrc());
      else if (p == "name")            return getString(image.name());
      else if (p == "align")           return getString(image.align());
      else if (p == "alt")             return getString(image.alt());
      else if (p == "border")          return getString(image.border());
      else if (p == "height")          return getString(image.height());
      else if (p == "hspace")          return getString(image.hspace());
      else if (p == "isMap")           return Boolean(image.isMap());
      else if (p == "longDesc")        return getString(image.longDesc());
      else if (p == "src")             return getString(image.src());
      else if (p == "useMap")          return getString(image.useMap());
      else if (p == "vspace")          return getString(image.vspace());
      else if (p == "width")           return getString(image.width());
    }
    break;
    case ID_OBJECT: {
      DOM::HTMLObjectElement object = element;
      if      (p == "form")            return getDOMNode(object.form()); // type HTMLFormElement
      else if (p == "code")            return getString(object.code());
      else if (p == "align")           return getString(object.align());
      else if (p == "archive")         return getString(object.archive());
      else if (p == "border")          return getString(object.border());
      else if (p == "codeBase")        return getString(object.codeBase());
      else if (p == "codeType")        return getString(object.codeType());
      else if (p == "data")            return getString(object.data());
      else if (p == "declare")         return Boolean(object.declare());
      else if (p == "height")          return getString(object.height());
      else if (p == "hspace")          return getString(object.hspace());
      else if (p == "name")            return getString(object.name());
      else if (p == "standby")         return getString(object.standby());
      else if (p == "tabIndex")        return Number(object.tabIndex());
      else if (p == "type")            return getString(object.type());
      else if (p == "useMap")          return getString(object.useMap());
      else if (p == "vspace")          return getString(object.vspace());
      else if (p == "width")           return getString(object.width());
//      else if (p == "contentDocument") // new for DOM2 - not yet in khtml
//        return getDOMNode(object.contentDocument()); // type Document
    }
    break;
    case ID_PARAM: {
      DOM::HTMLParamElement param = element;
      if      (p == "name")            return getString(param.name());
      else if (p == "type")            return getString(param.type());
      else if (p == "value")           return getString(param.value());
      else if (p == "valueType")       return getString(param.valueType());
    }
    break;
    case ID_APPLET: {
      DOM::HTMLAppletElement applet = element;
      if      (p == "align")           return getString(applet.align());
      else if (p == "alt")             return getString(applet.alt());
      else if (p == "archive")         return getString(applet.archive());
      else if (p == "code")            return getString(applet.code());
      else if (p == "codeBase")        return getString(applet.codeBase());
      else if (p == "height")          return getString(applet.height());
      else if (p == "hspace")          return getString(applet.hspace());
      else if (p == "name")            return getString(applet.name());
      else if (p == "object")          return getString(applet.object());
      else if (p == "vspace")          return getString(applet.vspace());
      else if (p == "width")           return getString(applet.width());
    }
    break;
    case ID_MAP: {
      DOM::HTMLMapElement map = element;
      if      (p == "areas")           return getHTMLCollection(map.areas()); // type HTMLCollection
      else if (p == "name")            return getString(map.name());
    }
    break;
    case ID_AREA: {
      DOM::HTMLAreaElement area = element;
      if      (p == "accessKey")       return getString(area.accessKey());
      else if (p == "alt")             return getString(area.alt());
      else if (p == "coords")          return getString(area.coords());
      else if (p == "href")            return getString(area.href());
      else if (p == "hash")            return getString('#'+KURL(area.href().string()).ref());
      else if (p == "host")            return getString(KURL(area.href().string()).host());
      else if (p == "hostname") {
        KURL url(area.href().string());
        kdDebug(6070) << "link::hostname uses:" <<url.url()<<endl;
        if (url.port()==0)
          return getString(url.host());
        else
          return getString(url.host() + ":" + QString::number(url.port()));
      }
      else if (p == "pathname")        return getString(KURL(area.href().string()).path());
      else if (p == "port")            return getString(QString::number(KURL(area.href().string()).port()));
      else if (p == "protocol")        return getString(KURL(area.href().string()).protocol()+":");
      else if (p == "search")          return getString(KURL(area.href().string()).query());

      else if (p == "noHref")          return Boolean(area.noHref());
      else if (p == "shape")           return getString(area.shape());
      else if (p == "tabIndex")        return Number(area.tabIndex());
      else if (p == "target")          return getString(area.target());
    }
    break;
    case ID_SCRIPT: {
      DOM::HTMLScriptElement script = element;
      if      (p == "text")            return getString(script.text());
      else if (p == "htmlFor")         return getString(script.htmlFor());
      else if (p == "event")           return getString(script.event());
      else if (p == "charset")         return getString(script.charset());
      else if (p == "defer")           return Boolean(script.defer());
      else if (p == "src")             return getString(script.src());
      else if (p == "type")            return getString(script.type());
    }
    break;
    case ID_TABLE: {
      DOM::HTMLTableElement table = element;
      if      (p == "caption")         return getDOMNode(table.caption()); // type HTMLTableCaptionElement
      else if (p == "tHead")           return getDOMNode(table.tHead()); // type HTMLTableSectionElement
      else if (p == "tFoot")           return getDOMNode(table.tFoot()); // type HTMLTableSectionElement
      else if (p == "rows")            return getHTMLCollection(table.rows()); // type HTMLCollection
      else if (p == "tBodies")         return getHTMLCollection(table.tBodies()); // type HTMLCollection
      else if (p == "align")           return getString(table.align());
      else if (p == "bgColor")         return getString(table.bgColor());
      else if (p == "border")          return getString(table.border());
      else if (p == "cellPadding")     return getString(table.cellPadding());
      else if (p == "cellSpacing")     return getString(table.cellSpacing());
      else if (p == "frame")           return getString(table.frame());
      else if (p == "rules")           return getString(table.rules());
      else if (p == "summary")         return getString(table.summary());
      else if (p == "width")           return getString(table.width());
      // methods
      else if (p == "createTHead")     return new HTMLElementFunction(element,HTMLElementFunction::CreateTHead);
      else if (p == "deleteTHead")     return new HTMLElementFunction(element,HTMLElementFunction::DeleteTHead);
      else if (p == "createTFoot")     return new HTMLElementFunction(element,HTMLElementFunction::CreateTFoot);
      else if (p == "deleteTFoot")     return new HTMLElementFunction(element,HTMLElementFunction::DeleteTFoot);
      else if (p == "createCaption")   return new HTMLElementFunction(element,HTMLElementFunction::CreateCaption);
      else if (p == "deleteCaption")   return new HTMLElementFunction(element,HTMLElementFunction::DeleteCaption);
      else if (p == "insertRow")       return new HTMLElementFunction(element,HTMLElementFunction::InsertRow);
      else if (p == "deleteRow")       return new HTMLElementFunction(element,HTMLElementFunction::DeleteRow);
    }
    break;
    case ID_CAPTION: {
      DOM::HTMLTableCaptionElement tableCaption;
      if      (p == "align")           return getString(tableCaption.align());
    }
    break;
    case ID_COL: {
      DOM::HTMLTableColElement tableCol = element;
      if      (p == "align")           return getString(tableCol.align());
      else if (p == "ch")              return getString(tableCol.ch());
      else if (p == "chOff")           return getString(tableCol.chOff());
      else if (p == "span")            return Number(tableCol.span());
      else if (p == "vAlign")          return getString(tableCol.vAlign());
      else if (p == "width")           return getString(tableCol.width());
    }
    break;
    case ID_THEAD:
    case ID_TBODY:
    case ID_TFOOT: {
      DOM::HTMLTableSectionElement tableSection = element;
      if      (p == "align")           return getString(tableSection.align());
      else if (p == "ch")              return getString(tableSection.ch());
      else if (p == "chOff")           return getString(tableSection.chOff());
      else if (p == "vAlign")          return getString(tableSection.vAlign());
      else if (p == "rows")            return getHTMLCollection(tableSection.rows()); // type HTMLCollection
      // methods
      else if (p == "insertRow")       return new HTMLElementFunction(element,HTMLElementFunction::InsertRow);
      else if (p == "deleteRow")       return new HTMLElementFunction(element,HTMLElementFunction::DeleteRow);
    }
    break;
    case ID_TR: {
      DOM::HTMLTableRowElement tableRow = element;
      if      (p == "rowIndex")        return Number(tableRow.rowIndex());
      else if (p == "sectionRowIndex") return Number(tableRow.sectionRowIndex());
      else if (p == "cells")           return getHTMLCollection(tableRow.cells()); // type HTMLCollection
      else if (p == "align")           return getString(tableRow.align());
      else if (p == "bgColor")         return getString(tableRow.bgColor());
      else if (p == "ch")              return getString(tableRow.ch());
      else if (p == "chOff")           return getString(tableRow.chOff());
      else if (p == "vAlign")          return getString(tableRow.vAlign());
      // methods
      else if (p == "insertCell")       return new HTMLElementFunction(element,HTMLElementFunction::InsertCell);
      else if (p == "deleteCell")       return new HTMLElementFunction(element,HTMLElementFunction::DeleteCell);
    }
    break;
    case ID_TH:
    case ID_TD: {
      DOM::HTMLTableCellElement tableCell = element;
      if      (p == "cellIndex")       return Number(tableCell.cellIndex());
      else if (p == "abbr")            return getString(tableCell.abbr());
      else if (p == "align")           return getString(tableCell.align());
      else if (p == "axis")            return getString(tableCell.axis());
      else if (p == "bgColor")         return getString(tableCell.bgColor());
      else if (p == "ch")              return getString(tableCell.ch());
      else if (p == "chOff")           return getString(tableCell.chOff());
      else if (p == "colSpan")         return Number(tableCell.colSpan());
      else if (p == "headers")         return getString(tableCell.headers());
      else if (p == "height")          return getString(tableCell.height());
      else if (p == "noWrap")          return Boolean(tableCell.noWrap());
      else if (p == "rowSpan")         return Number(tableCell.rowSpan());
      else if (p == "scope")           return getString(tableCell.scope());
      else if (p == "vAlign")          return getString(tableCell.vAlign());
      else if (p == "width")           return getString(tableCell.width());
    }
    break;
    case ID_FRAMESET: {
      DOM::HTMLFrameSetElement frameSet = element;
      if      (p == "cols")            return getString(frameSet.cols());
      else if (p == "rows")            return getString(frameSet.rows());
    }
    break;
    case ID_FRAME: {
      DOM::HTMLFrameElement frameElement = element;

      // p == "document" ?
      if (p == "frameBorder")          return getString(frameElement.frameBorder());
      else if (p == "longDesc")        return getString(frameElement.longDesc());
      else if (p == "marginHeight")    return getString(frameElement.marginHeight());
      else if (p == "marginWidth")     return getString(frameElement.marginWidth());
      else if (p == "name")            return getString(frameElement.name());
      else if (p == "noResize")        return Boolean(frameElement.noResize());
      else if (p == "scrolling")       return getString(frameElement.scrolling());
      else if (p == "src")             return getString(frameElement.src());
//      else if (p == "contentDocument") // new for DOM2 - not yet in khtml
//        return getDOMNode(frameElement.contentDocument()); // type Document
    }
    break;
    case ID_IFRAME: {
      DOM::HTMLIFrameElement iFrame = element;
      if (p == "align")                return getString(iFrame.align());
      // ### security check ?
      else if (p == "document") {
        if ( !iFrame.isNull() )
          return getDOMNode( static_cast<DOM::HTMLIFrameElementImpl*>(iFrame.handle() )->frameDocument() );

        return Undefined();
      }
      else if (p == "frameBorder")     return getString(iFrame.frameBorder());
      else if (p == "height")          return getString(iFrame.height());
      else if (p == "longDesc")        return getString(iFrame.longDesc());
      else if (p == "marginHeight")    return getString(iFrame.marginHeight());
      else if (p == "marginWidth")     return getString(iFrame.marginWidth());
      else if (p == "name")            return getString(iFrame.name());
      else if (p == "scrolling")       return getString(iFrame.scrolling());
      else if (p == "src")             return getString(iFrame.src());
      else if (p == "width")           return getString(iFrame.width());
//      else if (p == "contentDocument") // new for DOM2 - not yet in khtml
//        return getDOMNode(iFrame.contentDocument); // type Document
    }
    break;
  }

  // generic properties
  if (p == "id")
    return getString(element.id());
  else if (p == "title")
    return getString(element.title());
  else if (p == "lang")
    return getString(element.lang());
  else if (p == "dir")
    return getString(element.dir());
  else if (p == "className") // ### isn't this "class" in the HTML spec?
    return getString(element.className());
  else if ( p == "innerHTML")
      return getString(element.innerHTML());
  else if ( p == "innerText")
      return getString(element.innerText());
  // ### what about style? or is this used instead for DOM2 stylesheets?
  else
    return DOMElement::tryGet(p);
}

bool KJS::HTMLElement::hasProperty(const UString &p, bool recursive) const
{
    KJSO tmp = tryGet(p);
    if (tmp.isDefined())
	return true;
    return recursive && DOMElement::hasProperty(p, true);
}

String KJS::HTMLElement::toString() const
{
  if (node.elementId() == ID_A)
    return UString(static_cast<const DOM::HTMLAnchorElement&>(node).href());
  else
    return DOMElement::toString();
}

List *KJS::HTMLElement::eventHandlerScope() const
{
  DOM::HTMLElement element = static_cast<DOM::HTMLElement>(node);

  List *scope = new List();
  scope->append(getDOMNode(element.ownerDocument()));

  DOM::Node form = element.parentNode();
  while (!form.isNull() && form.elementId() != ID_FORM)
    form = form.parentNode();
  if (!form.isNull())
    scope->append(getDOMNode(form));

  scope->append(getDOMNode(element));
  return scope;
}

Completion KJS::HTMLElementFunction::tryExecute(const List &args)
{
  KJSO result;

  switch (element.elementId()) {
    case ID_FORM: {
      DOM::HTMLFormElement form = element;
      if (id == Submit) {
        form.submit();
        result = Undefined();
      }
      else if (id == Reset) {
        form.reset();
        result = Undefined();
      }
    }
    break;
    case ID_SELECT: {
      DOM::HTMLSelectElement select = element;
      if (id == Add) {
        select.add(KJS::toNode(args[0]),KJS::toNode(args[1]));
        result = Undefined();
      }
      else if (id == Remove) {
        select.remove(args[0].toNumber().intValue());
        result = Undefined();
      }
      else if (id == Blur) {
        select.blur();
        result = Undefined();
      }
      else if (id == Focus) {
        select.focus();
        result = Undefined();
      }
    }
    break;
    case ID_INPUT: {
      DOM::HTMLInputElement input = element;
      if (id == Blur) {
        input.blur();
        result = Undefined();
      }
      else if (id == Focus) {
        input.focus();
        result = Undefined();
      }
      else if (id == Select) {
        input.select();
        result = Undefined();
      }
      else if (id == Click) {
        input.click();
        result = Undefined();
      }
    }
    break;
    case ID_TEXTAREA: {
      DOM::HTMLTextAreaElement textarea = element;
      if (id == Blur) {
        textarea.blur();
        result = Undefined();
      }
      else if (id == Focus) {
        textarea.focus();
        result = Undefined();
      }
      else if (id == Select) {
        textarea.select();
        result = Undefined();
      }
    }
    break;
    case ID_A: {
      DOM::HTMLAnchorElement anchor = element;
      if (id == Blur) {
        anchor.blur();
        result = Undefined();
      }
      else if (id == Focus) {
        anchor.focus();
        result = Undefined();
      }
    }
    break;
    case ID_TABLE: {
      DOM::HTMLTableElement table = element;
      if (id == CreateTHead)
        result = getDOMNode(table.createTHead());
      else if (id == DeleteTHead) {
        table.deleteTHead();
        result = Undefined();
      }
      else if (id == CreateTFoot)
        result = getDOMNode(table.createTFoot());
      else if (id == DeleteTFoot) {
        table.deleteTFoot();
        result = Undefined();
      }
      else if (id == CreateCaption)
        result = getDOMNode(table.createCaption());
      else if (id == DeleteCaption) {
        table.deleteCaption();
        result = Undefined();
      }
      else if (id == InsertRow)
        result = getDOMNode(table.insertRow(args[0].toNumber().intValue()));
      else if (id == DeleteRow) {
        table.deleteRow(args[0].toNumber().intValue());
        result = Undefined();
      }
    }
    break;
    case ID_THEAD:
    case ID_TBODY:
    case ID_TFOOT: {
      DOM::HTMLTableSectionElement tableSection = element;
      if (id == InsertRow)
        result = getDOMNode(tableSection.insertRow(args[0].toNumber().intValue()));
      else if (id == DeleteRow) {
        tableSection.deleteRow(args[0].toInt32());
        result = Undefined();
      }
    }
    break;
    case ID_TR: {
      DOM::HTMLTableRowElement tableRow = element;
      if (id == InsertCell)
        result = getDOMNode(tableRow.insertCell(args[0].toNumber().intValue()));
      else if (id == DeleteCell) {
        tableRow.deleteCell(args[0].toInt32());
        result = Undefined();
      }
    }
    break;
  }

  return Completion(ReturnValue, result);
}

void KJS::HTMLElement::tryPut(const UString &p, const KJSO& v)
{
  DOM::DOMString str = v.isA(NullType) ? DOM::DOMString(0) : v.toString().value().string();
  DOM::Node n = (new DOMNode(KJS::toNode(v)))->toNode();
  DOM::HTMLElement element = static_cast<DOM::HTMLElement>(node);
  //kdDebug() << "KJS::HTMLElement::tryPut " << p.qstring() << " id=" << element.elementId() << endl;

  switch (element.elementId()) {
    case ID_HTML: {
      DOM::HTMLHtmlElement html = element;
      if      (p == "version")         { html.setVersion(str); return; }
    }
    break;
    case ID_HEAD: {
      DOM::HTMLHeadElement head = element;
      if      (p == "profile")         { head.setProfile(str); return; }
    }
    break;
    case ID_LINK: {
      DOM::HTMLLinkElement link = element;
      if      (p == "disabled")        { link.setDisabled(v.toBoolean().value()); return; }
      else if (p == "charset")         { link.setCharset(str); return; }
      else if (p == "href")            { link.setHref(str); return; }
      else if (p == "hreflang")        { link.setHreflang(str); return; }
      else if (p == "media")           { link.setMedia(str); return; }
      else if (p == "rel")             { link.setRel(str); return; }
      else if (p == "rev")             { link.setRev(str); return; }
      else if (p == "target")          { link.setTarget(str); return; }
      else if (p == "type")            { link.setType(str); return; }
    }
    break;
    case ID_TITLE: {
      DOM::HTMLTitleElement title = element;
      if (p == "text")                 { title.setText(str); return; }
    }
    break;
    case ID_META: {
      DOM::HTMLMetaElement meta = element;
      if      (p == "content")         { meta.setContent(str); return; }
      else if (p == "httpEquiv")       { meta.setHttpEquiv(str); return; }
      else if (p == "name")            { meta.setName(str); return; }
      else if (p == "scheme")          { meta.setScheme(str); return; }
    }
    break;
    case ID_BASE: {
      DOM::HTMLBaseElement base = element;
      if      (p == "href")            { base.setHref(str); return; }
      else if (p == "target")          { base.setTarget(str); return; }
    }
    break;
    case ID_ISINDEX: {
      DOM::HTMLIsIndexElement isindex = element;
      // read-only: form
      if (p == "prompt")               { isindex.setPrompt(str); return; }
    }
    break;
    case ID_STYLE: {
      DOM::HTMLStyleElement style = element;
      if      (p == "disabled")        { style.setDisabled(v.toBoolean().value()); return; }
      else if (p == "media")           { style.setMedia(str); return; }
      else if (p == "type")            { style.setType(str); return; }
    }
    break;
    case ID_BODY: {
      DOM::HTMLBodyElement body = element;
      if      (p == "aLink")           { body.setALink(str); return; }
      else if (p == "background")      { body.setBackground(str); return; }
      else if (p == "bgColor")         { body.setBgColor(str); return; }
      else if (p == "link")            { body.setLink(str); return; }
      else if (p == "text")            { body.setText(str); return; }
      else if (p == "vLink")           { body.setVLink(str); return; }
    }
    break;
    case ID_FORM: {
      DOM::HTMLFormElement form = element;
      // read-only: elements
      // read-only: length
      if (p == "name")                 { form.setName(str); return; }
      else if (p == "acceptCharset")   { form.setAcceptCharset(str); return; }
      else if (p == "action") {
        form.setAction(Window::retrieveActive()->part()->completeURL( str.string() ).url());
        return;
      }
      else if (p == "enctype")         { form.setEnctype(str); return; }
      else if (p == "method")          { form.setMethod(str); return; }
      else if (p == "target")          { form.setTarget(str); return; }
    }
    break;
    case ID_SELECT: {
      DOM::HTMLSelectElement select = element;
      // read-only: type
      if (p == "selectedIndex")        { select.setSelectedIndex(v.toNumber().intValue()); return; }
      else if (p == "value")           { select.setValue(str); return; }
      else if (p == "length")          { // read-only according to the NS spec, but webpages need it writeable
                                         KJSO coll = getSelectHTMLCollection(select.options(), select);
                                         coll.put( p, v );
                                         return;
                                       }
      // read-only: form
      // read-only: options
      else if (p == "disabled")        { select.setDisabled(v.toBoolean().value()); return; }
      else if (p == "multiple")        { select.setMultiple(v.toBoolean().value()); return; }
      else if (p == "name")            { select.setName(str); return; }
      else if (p == "size")            { select.setSize(v.toNumber().intValue()); return; }
      else if (p == "tabIndex")        { select.setTabIndex(v.toNumber().intValue()); return; }
    }
    break;
    case ID_OPTGROUP: {
      DOM::HTMLOptGroupElement optgroup = element;
      if      (p == "disabled")        { optgroup.setDisabled(v.toBoolean().value()); return; }
      else if (p == "label")           { optgroup.setLabel(str); return; }
    }
    break;
    case ID_OPTION: {
      DOM::HTMLOptionElement option = element;
      // read-only: form
      if (p == "defaultSelected")      { option.setDefaultSelected(v.toBoolean().value()); return; }
      // read-only: text  <--- According to the DOM, but JavaScript and JScript both allow changes.
      // So, we'll do it here and not add it to our DOM headers.
      else if (p == "text")            { DOM::NodeList nl(option.childNodes());
                                         for (unsigned int i = 0; i < nl.length(); i++) {
                                             if (nl.item(i).nodeType() == DOM::Node::TEXT_NODE) {
                                                 static_cast<DOM::Text>(nl.item(i)).setData(str);
                                                 return;
                                             }
                                         }
                                         kdWarning() << "HTMLElement::tryPut failed, no text node in option" << endl;
                                         return;
      }
      // read-only: index
      else if (p == "disabled")        { option.setDisabled(v.toBoolean().value()); return; }
      else if (p == "label")           { option.setLabel(str); return; }
      else if (p == "selected")        { option.setSelected(v.toBoolean().value()); return; }
      // despide its name, it doesn't seem to modify the value, but the text!
      // (DF: ??? testcases shows that this indeed modifies the value! - code looks fine)
      else if (p == "value")           { option.setValue(str); return; }
    }
    break;
    case ID_INPUT: {
      DOM::HTMLInputElement input = element;
      if      (p == "defaultValue")    { input.setDefaultValue(str); return; }
      else if (p == "defaultChecked")  { input.setDefaultChecked(v.toBoolean().value()); return; }
      // read-only: form
      else if (p == "accept")          { input.setAccept(str); return; }
      else if (p == "accessKey")       { input.setAccessKey(str); return; }
      else if (p == "align")           { input.setAlign(str); return; }
      else if (p == "alt")             { input.setAlt(str); return; }
      else if (p == "checked")         { input.setChecked(v.toBoolean().value()); return; }
      else if (p == "disabled")        { input.setDisabled(v.toBoolean().value()); return; }
      else if (p == "maxLength")       { input.setMaxLength(v.toNumber().intValue()); return; }
      else if (p == "name")            { input.setName(str); return; }
      else if (p == "readOnly")        { input.setReadOnly(v.toBoolean().value()); return; }
      else if (p == "size")            { input.setSize(str); return; }
      else if (p == "src")             { input.setSrc(str); return; }
      else if (p == "tabIndex")        { input.setTabIndex(v.toNumber().intValue()); return; }
      // read-only: type
      else if (p == "useMap")          { input.setUseMap(str); return; }
      else if (p == "value")           { input.setValue(str); return; }
    }
    break;
    case ID_TEXTAREA: {
      DOM::HTMLTextAreaElement textarea = element;
      if      (p == "defaultValue")    { textarea.setDefaultValue(str); return; }
      // read-only: form
      else if (p == "accessKey")       { textarea.setAccessKey(str); return; }
      else if (p == "cols")            { textarea.setCols(v.toNumber().intValue()); return; }
      else if (p == "disabled")        { textarea.setDisabled(v.toBoolean().value()); return; }
      else if (p == "name")            { textarea.setName(str); return; }
      else if (p == "readOnly")        { textarea.setReadOnly(v.toBoolean().value()); return; }
      else if (p == "rows")            { textarea.setRows(v.toNumber().intValue()); return; }
      else if (p == "tabIndex")        { textarea.setTabIndex(v.toNumber().intValue()); return; }
      // read-only: type
      else if (p == "value")           { textarea.setValue(str); return; }
    }
    break;
    case ID_BUTTON: {
      DOM::HTMLButtonElement button = element;
      // read-only: form
      if (p == "accessKey")            { button.setAccessKey(str); return; }
      else if (p == "disabled")        { button.setDisabled(v.toBoolean().value()); return; }
      else if (p == "name")            { button.setName(str); return; }
      else if (p == "tabIndex")        { button.setTabIndex(v.toNumber().intValue()); return; }
      // read-only: type
      else if (p == "value")           { button.setValue(str); return; }
    }
    break;
    case ID_LABEL: {
      DOM::HTMLLabelElement label = element;
      // read-only: form
      if (p == "accessKey")            { label.setAccessKey(str); return; }
      else if (p == "htmlFor")         { label.setHtmlFor(str); return; }
    }
    break;
//    case ID_FIELDSET: {
//      DOM::HTMLFieldSetElement fieldSet = element;
//      // read-only: form
//    }
//    break;
    case ID_LEGEND: {
      DOM::HTMLLegendElement legend = element;
      // read-only: form
      if (p == "accessKey")            { legend.setAccessKey(str); return; }
      else if (p == "align")           { legend.setAlign(str); return; }
    }
    break;
    case ID_UL: {
      DOM::HTMLUListElement uList = element;
      if      (p == "compact")         { uList.setCompact(v.toBoolean().value()); return; }
      else if (p == "type")            { uList.setType(str); return; }
    }
    break;
    case ID_OL: {
      DOM::HTMLOListElement oList = element;
      if      (p == "compact")         { oList.setCompact(v.toBoolean().value()); return; }
      else if (p == "start")           { oList.setStart(v.toNumber().intValue()); return; }
      else if (p == "type")            { oList.setType(str); return; }
    }
    break;
    case ID_DL: {
      DOM::HTMLDListElement dList = element;
      if      (p == "compact")         { dList.setCompact(v.toBoolean().value()); return; }
    }
    break;
    case ID_DIR: {
      DOM::HTMLDirectoryElement directory = element;
      if      (p == "compact")         { directory.setCompact(v.toBoolean().value()); return; }
    }
    break;
    case ID_MENU: {
      DOM::HTMLMenuElement menu = element;
      if      (p == "compact")         { menu.setCompact(v.toBoolean().value()); return; }
    }
    break;
    case ID_LI: {
      DOM::HTMLLIElement li = element;
      if      (p == "type")            { li.setType(str); return; }
      else if (p == "value")           { li.setValue(v.toNumber().intValue()); return; }
    }
    break;
    case ID_DIV: {
      DOM::HTMLDivElement div = element;
      if      (p == "align")           { div.setAlign(str); return; }
    }
    break;
    case ID_P: {
      DOM::HTMLParagraphElement paragraph = element;
      if      (p == "align")           { paragraph.setAlign(str); return; }
    }
    break;
    case ID_H1:
    case ID_H2:
    case ID_H3:
    case ID_H4:
    case ID_H5:
    case ID_H6: {
      DOM::HTMLHeadingElement heading = element;
      if      (p == "align")           { heading.setAlign(str); return; }
    }
    break;
    case ID_BLOCKQUOTE: {
      DOM::HTMLBlockquoteElement blockquote = element;
      if      (p == "cite")            { blockquote.setCite(str); return; }
    }
    case ID_Q: {
      DOM::HTMLQuoteElement quote = element;
      if      (p == "cite")            { quote.setCite(str); return; }
    }
    break;
    case ID_PRE: {
      DOM::HTMLPreElement pre = element;
      if      (p == "width")           { pre.setWidth(v.toNumber().intValue()); return; }
    }
    break;
    case ID_BR: {
      DOM::HTMLBRElement br = element;
      if      (p == "clear")           { br.setClear(str); return; }
    }
    break;
    case ID_BASEFONT: {
      DOM::HTMLBaseFontElement baseFont = element;
      if      (p == "color")           { baseFont.setColor(str); return; }
      else if (p == "face")            { baseFont.setFace(str); return; }
      else if (p == "size")            { baseFont.setSize(str); return; }
    }
    break;
    case ID_FONT: {
      DOM::HTMLFontElement font = element;
      if      (p == "color")           { font.setColor(str); return; }
      else if (p == "face")            { font.setFace(str); return; }
      else if (p == "size")            { font.setSize(str); return; }
    }
    break;
    case ID_HR: {
      DOM::HTMLHRElement hr = element;
      if      (p == "align")           { hr.setAlign(str); return; }
      else if (p == "noShade")         { hr.setNoShade(v.toBoolean().value()); return; }
      else if (p == "size")            { hr.setSize(str); return; }
      else if (p == "width")           { hr.setWidth(str); return; }
    }
    break;
    case ID_INS:
    case ID_DEL: {
      DOM::HTMLModElement mod = element;
      if      (p == "cite")            { mod.setCite(str); return; }
      else if (p == "dateTime")        { mod.setDateTime(str); return; }
    }
    break;
    case ID_A: {
      DOM::HTMLAnchorElement anchor = element;
      if      (p == "accessKey")       { anchor.setAccessKey(str); return; }
      else if (p == "charset")         { anchor.setCharset(str); return; }
      else if (p == "coords")          { anchor.setCoords(str); return; }
      else if (p == "href")            { anchor.setHref(str); return; }
      else if (p == "hreflang")        { anchor.setHreflang(str); return; }
      else if (p == "name")            { anchor.setName(str); return; }
      else if (p == "rel")             { anchor.setRel(str); return; }
      else if (p == "rev")             { anchor.setRev(str); return; }
      else if (p == "shape")           { anchor.setShape(str); return; }
      else if (p == "tabIndex")        { anchor.setTabIndex(v.toNumber().intValue()); return; }
      else if (p == "target")          { anchor.setTarget(str); return; }
      else if (p == "type")            { anchor.setType(str); return; }
    }
    break;
    case ID_IMG: {
      DOM::HTMLImageElement image = element;
      if      (p == "lowSrc")          { image.setLowSrc(str); return; }
      else if (p == "name")            { image.setName(str); return; }
      else if (p == "align")           { image.setAlign(str); return; }
      else if (p == "alt")             { image.setAlt(str); return; }
      else if (p == "border")          { image.setBorder(str); return; }
      else if (p == "height")          { image.setHeight(str); return; }
      else if (p == "hspace")          { image.setHspace(str); return; }
      else if (p == "isMap")           { image.setIsMap(v.toBoolean().value()); return; }
      else if (p == "longDesc")        { image.setLongDesc(str); return; }
      else if (p == "src")             { image.setSrc(str); return; }
      else if (p == "useMap")          { image.setUseMap(str); return; }
      else if (p == "vspace")          { image.setVspace(str); return; }
      else if (p == "width")           { image.setWidth(str); return; }
    }
    break;
    case ID_OBJECT: {
      DOM::HTMLObjectElement object = element;
      // read-only: form
      if (p == "code")                 { object.setCode(str); return; }
      else if (p == "align")           { object.setAlign(str); return; }
      else if (p == "archive")         { object.setArchive(str); return; }
      else if (p == "border")          { object.setBorder(str); return; }
      else if (p == "codeBase")        { object.setCodeBase(str); return; }
      else if (p == "codeType")        { object.setCodeType(str); return; }
      else if (p == "data")            { object.setData(str); return; }
      else if (p == "declare")         { object.setDeclare(v.toBoolean().value()); return; }
      else if (p == "height")          { object.setHeight(str); return; }
      else if (p == "hspace")          { object.setHspace(str); return; }
      else if (p == "name")            { object.setName(str); return; }
      else if (p == "standby")         { object.setStandby(str); return; }
      else if (p == "tabIndex")        { object.setTabIndex(v.toNumber().intValue()); return; }
      else if (p == "type")            { object.setType(str); return; }
      else if (p == "useMap")          { object.setUseMap(str); return; }
      else if (p == "vspace")          { object.setVspace(str); return; }
      else if (p == "width")           { object.setWidth(str); return; }
//      else if (p == "contentDocument") // new for DOM2 - not yet in khtml
//        return getDOMNode(object.contentDocument()); // type Document
    }
    break;
    case ID_PARAM: {
      DOM::HTMLParamElement param = element;
      if      (p == "name")            { param.setName(str); return; }
      else if (p == "type")            { param.setType(str); return; }
      else if (p == "value")           { param.setValue(str); return; }
      else if (p == "valueType")       { param.setValueType(str); return; }
    }
    break;
    case ID_APPLET: {
      DOM::HTMLAppletElement applet = element;
      if      (p == "align")           { applet.setAlign(str); return; }
      else if (p == "alt")             { applet.setAlt(str); return; }
      else if (p == "archive")         { applet.setArchive(str); return; }
      else if (p == "code")            { applet.setCode(str); return; }
      else if (p == "codeBase")        { applet.setCodeBase(str); return; }
      else if (p == "height")          { applet.setHeight(str); return; }
      else if (p == "hspace")          { applet.setHspace(str); return; }
      else if (p == "name")            { applet.setName(str); return; }
      else if (p == "object")          { applet.setObject(str); return; }
      else if (p == "vspace")          { applet.setVspace(str); return; }
      else if (p == "width")           { applet.setWidth(str); return; }
    }
    break;
    case ID_MAP: {
      DOM::HTMLMapElement map = element;
      // read-only: areas
      if (p == "name")                 { map.setName(str); return; }
    }
    break;
    case ID_AREA: {
      DOM::HTMLAreaElement area = element;
      if      (p == "accessKey")       { area.setAccessKey(str); return; }
      else if (p == "alt")             { area.setAlt(str); return; }
      else if (p == "coords")          { area.setCoords(str); return; }
      else if (p == "href")            { area.setHref(str); return; }
      else if (p == "noHref")          { area.setNoHref(v.toBoolean().value()); return; }
      else if (p == "shape")           { area.setShape(str); return; }
      else if (p == "tabIndex")        { area.setTabIndex(v.toNumber().intValue()); return; }
      else if (p == "target")          { area.setTarget(str); return; }
    }
    break;
    case ID_SCRIPT: {
      DOM::HTMLScriptElement script = element;
      if      (p == "text")            { script.setText(str); return; }
      else if (p == "htmlFor")         { script.setHtmlFor(str); return; }
      else if (p == "event")           { script.setEvent(str); return; }
      else if (p == "charset")         { script.setCharset(str); return; }
      else if (p == "defer")           { script.setDefer(v.toBoolean().value()); return; }
      else if (p == "src")             { script.setSrc(str); return; }
      else if (p == "type")            { script.setType(str); return; }
    }
    break;
    case ID_TABLE: {
      DOM::HTMLTableElement table = element;
      if      (p == "caption")         { table.setCaption(n); return; } // type HTMLTableCaptionElement
      else if (p == "tHead")           { table.setTHead(n); return; } // type HTMLTableSectionElement
      else if (p == "tFoot")           { table.setTFoot(n); return; } // type HTMLTableSectionElement
      // read-only: rows
      // read-only: tbodies
      else if (p == "align")           { table.setAlign(str); return; }
      else if (p == "bgColor")         { table.setBgColor(str); return; }
      else if (p == "border")          { table.setBorder(str); return; }
      else if (p == "cellPadding")     { table.setCellPadding(str); return; }
      else if (p == "cellSpacing")     { table.setCellSpacing(str); return; }
      else if (p == "frame")           { table.setFrame(str); return; }
      else if (p == "rules")           { table.setRules(str); return; }
      else if (p == "summary")         { table.setSummary(str); return; }
      else if (p == "width")           { table.setWidth(str); return; }
    }
    break;
    case ID_CAPTION: {
      DOM::HTMLTableCaptionElement tableCaption;
      if      (p == "align")           { tableCaption.setAlign(str); return; }
    }
    break;
    case ID_COL: {
      DOM::HTMLTableColElement tableCol = element;
      if      (p == "align")           { tableCol.setAlign(str); return; }
      else if (p == "ch")              { tableCol.setCh(str); return; }
      else if (p == "chOff")           { tableCol.setChOff(str); return; }
      else if (p == "span")            { tableCol.setSpan(v.toNumber().intValue()); return; }
      else if (p == "vAlign")          { tableCol.setVAlign(str); return; }
      else if (p == "width")           { tableCol.setWidth(str); return; }
    }
    break;
    case ID_THEAD:
    case ID_TBODY:
    case ID_TFOOT: {
      DOM::HTMLTableSectionElement tableSection = element;
      if      (p == "align")           { tableSection.setAlign(str); return; }
      else if (p == "ch")              { tableSection.setCh(str); return; }
      else if (p == "chOff")           { tableSection.setChOff(str); return; }
      else if (p == "vAlign")          { tableSection.setVAlign(str); return; }
      // read-only: rows
    }
    break;
    case ID_TR: {
      DOM::HTMLTableRowElement tableRow = element;
      // read-only: rowIndex
      // read-only: sectionRowIndex
      // read-only: cells
      if      (p == "align")           { tableRow.setAlign(str); return; }
      else if (p == "bgColor")         { tableRow.setBgColor(str); return; }
      else if (p == "ch")              { tableRow.setCh(str); return; }
      else if (p == "chOff")           { tableRow.setChOff(str); return; }
      else if (p == "vAlign")          { tableRow.setVAlign(str); return; }
    }
    break;
    case ID_TH:
    case ID_TD: {
      DOM::HTMLTableCellElement tableCell = element;
      // read-only: cellIndex
      if      (p == "abbr")            { tableCell.setAbbr(str); return; }
      else if (p == "align")           { tableCell.setAlign(str); return; }
      else if (p == "axis")            { tableCell.setAxis(str); return; }
      else if (p == "bgColor")         { tableCell.setBgColor(str); return; }
      else if (p == "ch")              { tableCell.setCh(str); return; }
      else if (p == "chOff")           { tableCell.setChOff(str); return; }
      else if (p == "colSpan")         { tableCell.setColSpan(v.toNumber().intValue()); return; }
      else if (p == "headers")         { tableCell.setHeaders(str); return; }
      else if (p == "height")          { tableCell.setHeight(str); return; }
      else if (p == "noWrap")          { tableCell.setNoWrap(v.toBoolean().value()); return; }
      else if (p == "rowSpan")         { tableCell.setRowSpan(v.toNumber().intValue()); return; }
      else if (p == "scope")           { tableCell.setScope(str); return; }
      else if (p == "vAlign")          { tableCell.setVAlign(str); return; }
      else if (p == "width")           { tableCell.setWidth(str); return; }
    }
    break;
    case ID_FRAMESET: {
      DOM::HTMLFrameSetElement frameSet = element;
      if      (p == "cols")            { frameSet.setCols(str); return; }
      else if (p == "rows")            { frameSet.setRows(str); return; }
    }
    break;
    case ID_FRAME: {
      DOM::HTMLFrameElement frameElement = element;
      if (p == "frameBorder")          { frameElement.setFrameBorder(str); return; }
      else if (p == "longDesc")        { frameElement.setLongDesc(str); return; }
      else if (p == "marginHeight")    { frameElement.setMarginHeight(str); return; }
      else if (p == "marginWidth")     { frameElement.setMarginWidth(str); return; }
      else if (p == "name")            { frameElement.setName(str); return; }
      else if (p == "noResize")        { frameElement.setNoResize(v.toBoolean().value()); return; }
      else if (p == "scrolling")       { frameElement.setScrolling(str); return; }
      else if (p == "src")             { frameElement.setSrc(str); return; }
//      else if (p == "contentDocument") // new for DOM2 - not yet in khtml
//        return getDOMNode(frameElement.contentDocument()); // type Document
    }
    break;
    case ID_IFRAME: {
      DOM::HTMLIFrameElement iFrame = element;
      if (p == "align")                { iFrame.setAlign(str); return; }
      else if (p == "frameBorder")     { iFrame.setFrameBorder(str); return; }
      else if (p == "height")          { iFrame.setHeight(str); return; }
      else if (p == "longDesc")        { iFrame.setLongDesc(str); return; }
      else if (p == "marginHeight")    { iFrame.setMarginHeight(str); return; }
      else if (p == "marginWidth")     { iFrame.setMarginWidth(str); return; }
      else if (p == "name")            { iFrame.setName(str); return; }
      else if (p == "scrolling")       { iFrame.setScrolling(str); return; }
      else if (p == "src")             { iFrame.setSrc(str); return; }
      else if (p == "width")           { iFrame.setWidth(str); return; }
//      else if (p == "contentDocument") // new for DOM2 - not yet in khtml
//        return getDOMNode(iFrame.contentDocument); // type Document
    }
    break;
  }

  // generic properties
  if (p == "id")
    element.setId(str);
  else if (p == "title")
    element.setTitle(str);
  else if (p == "lang")
    element.setLang(str);
  else if (p == "dir")
    element.setDir(str);
  else if (p == "className")
    element.setClassName(str);
  else if ( p == "innerHTML")
      element.setInnerHTML(str);
  else if ( p == "innerText")
      element.setInnerText(str);
  else
    DOMElement::tryPut(p,v);
}

// -------------------------------------------------------------------------

HTMLCollection::~HTMLCollection()
{
  htmlCollections.remove(collection.handle());
}

KJSO KJS::HTMLCollection::tryGet(const UString &p) const
{
  KJSO result;

  if (p == "length")
    result = Number(collection.length());
  else if (p == "item")
    result = new HTMLCollectionFunc(collection, HTMLCollectionFunc::Item);
  else if (p == "namedItem")
    result = new HTMLCollectionFunc(collection, HTMLCollectionFunc::NamedItem);
  else if ( p == "tags" )
    result = new HTMLCollectionFunc( collection,  HTMLCollectionFunc::Tags );
  else if (p == "selectedIndex" &&
	   collection.item(0).elementId() == ID_OPTION) {
    // NON-STANDARD options.selectedIndex
    DOM::Node node = collection.item(0).parentNode();
    while(!node.isNull()) {
      if(node.elementId() == ID_SELECT) {
	DOM::HTMLSelectElement sel = static_cast<DOM::HTMLSelectElement>(node);
	return Number(sel.selectedIndex());
      }
      node = node.parentNode();
    }
    result = Undefined();
  } else {
    DOM::Node node;
    DOM::HTMLElement element;

    // name or index ?
    bool ok;
    unsigned int u = p.toULong(&ok);
    if (ok)
      node = collection.item(u);
    else
      node = collection.namedItem(p.string());

    element = node;
    result = getDOMNode(element);
  }

  return result;
}

Completion KJS::HTMLCollectionFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (id) {
  case Item:
    result = getDOMNode(coll.item(args[0].toUInt32()));
    break;
  case Tags:
  {
    DOM::HTMLElement e = coll.base();
    result = getDOMNodeList( e.getElementsByTagName( args[0].toString().value().string() ) );
    break;
  }
  case NamedItem:
    result = getDOMNode(coll.namedItem(args[0].toString().value().string()));
    break;
  }

  return Completion(ReturnValue, result);
}

KJSO KJS::HTMLSelectCollection::tryGet(const UString &p) const
{
  if (p == "selectedIndex")
    return Number(element.selectedIndex());

  return  HTMLCollection::tryGet(p);
}

DOM::Element KJS::HTMLSelectCollection::dummyElement()
{
  DOM::Document doc = element.ownerDocument();
  DOM::Element dummy = doc.createElement("OPTION");
  // Add an empty textnode inside, so that the text can be set from Javascript.
  DOM::Text text = doc.createTextNode( "" );
  dummy.appendChild( text );
  return dummy;
}

void KJS::HTMLSelectCollection::tryPut(const UString &p, const KJSO& v)
{
  //kdDebug() << "KJS::HTMLSelectCollection::tryPut " << p.qstring() << endl;
  // resize ?
  if (p == "length") {
    long newLen = v.toInt32();
    long diff = element.length() - newLen;

    if (diff < 0) { // add dummy elements
      do {
        element.add(dummyElement(), DOM::HTMLElement());
      } while (++diff);
    }
    else // remove elements
      while (diff-- > 0)
        element.remove(newLen);

    return;
  }
  // an index ?
  bool ok;
  unsigned int u = p.toULong(&ok);
  if (!ok)
    return;

  if (v.isA(NullType) || v.isA(UndefinedType)) {
    // null and undefined delete. others, too ?
    element.remove(u);
    return;
  }

  // is v an option element ?
  DOM::Node node = KJS::toNode(v);
  if (node.isNull() || node.elementId() != ID_OPTION)
    return;

  DOM::HTMLOptionElement option = static_cast<DOM::HTMLOptionElement>(node);
  long diff = long(u) - element.length();
  DOM::HTMLElement before;
  // out of array bounds ? first insert empty dummies
  if (diff > 0) {
    while (diff--) {
      element.add(dummyElement(), before);
    }
    // replace an existing entry ?
  } else if (diff < 0) {
    before = element.options().item(u+1);
    element.remove(u);
  }
  // finally add the new element
  element.add(option, before);
}

////////////////////// Option Object ////////////////////////

OptionConstructor::OptionConstructor(const DOM::Document &d)
    : doc(d)
{
  setPrototype(global.functionPrototype());
}

Object OptionConstructor::construct(const List &args)
{
  DOM::Element el = doc.createElement("OPTION");
  DOM::HTMLOptionElement opt = static_cast<DOM::HTMLOptionElement>(el);
  int sz = args.size();
  DOM::Text t = doc.createTextNode("");
  try { opt.appendChild(t); }
  catch(DOM::DOMException& e) {
    // oh well
  }
  if (sz > 0)
    t.setData( args[0].toString().value().string() ); // set the text
  if (sz > 1)
    opt.setValue(args[1].toString().value().string());
  if (sz > 2)
    opt.setDefaultSelected( args[2].toBoolean().value() );
  if (sz > 3)
    opt.setSelected( args[3].toBoolean().value() );

  return Object(getDOMNode(opt).imp());
}

////////////////////// Image Object ////////////////////////

#if 0
ImageObject::ImageObject(const Global& global)
{
  Constructor ctor(new ImageConstructor(global));
  setConstructor(ctor);
  setPrototype(global.objectPrototype());

  put("length", Number(2), DontEnum);
}

Completion ImageObject::tryExecute(const List &)
{
  return Completion(ReturnValue);
}
#endif

ImageConstructor::ImageConstructor(const Global& glob, const DOM::Document &d)
  : global(glob),
    doc(d)
{
  setPrototype(global.functionPrototype());
}

Object ImageConstructor::construct(const List &)
{
  /* TODO: fetch optional height & width from arguments */

  Object result(new Image(doc));
  /* TODO: do we need a prototype ? */

  return result;
}

KJSO Image::tryGet(const UString &p) const
{
  KJSO result;

  if (p == "src")
    result = String(src);
  else if (p == "complete")
    result = Boolean(!img || img->status() >= khtml::CachedObject::Persistent);
  else
    result = DOMObject::tryGet(p);

  return result;
}

void Image::tryPut(const UString &p, const KJSO& v)
{
  if (p == "src") {
    String str = v.toString();
    src = str.value();
    if ( img ) img->deref(this);
    img = static_cast<DOM::DocumentImpl*>( doc.handle() )->docLoader()->requestImage( src.string(),
                                     doc.view()->part()->baseURL().url());
    if ( img ) img->ref(this);
  } else {
    DOMObject::tryPut(p, v);
  }
}

Image::~Image()
{
  if ( img ) img->deref(this);
}

KJSO KJS::getHTMLCollection(DOM::HTMLCollection c)
{
  HTMLCollection *ret;
  if (c.isNull())
    return Null();
  else if ((ret = htmlCollections[c.handle()]))
    return ret;
  else {
    ret = new HTMLCollection(c);
    htmlCollections.insert(c.handle(),ret);
    return ret;
  }
}

KJSO KJS::getSelectHTMLCollection(DOM::HTMLCollection c, DOM::HTMLSelectElement e)
{
  HTMLCollection *ret;
  if (c.isNull())
    return Null();
  else if ((ret = htmlCollections[c.handle()]))
    return ret;
  else {
    ret = new HTMLSelectCollection(c, e);
    htmlCollections.insert(c.handle(),ret);
    return ret;
  }
}
