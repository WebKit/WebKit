/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitDOMHTMLPrivate.h"

#include <WebCore/HTMLAnchorElement.h>
#include <WebCore/HTMLAppletElement.h>
#include <WebCore/HTMLAreaElement.h>
#include <WebCore/HTMLAudioElement.h>
#include <WebCore/HTMLBRElement.h>
#include <WebCore/HTMLBaseElement.h>
#include <WebCore/HTMLBodyElement.h>
#include <WebCore/HTMLButtonElement.h>
#include <WebCore/HTMLCanvasElement.h>
#include <WebCore/HTMLDListElement.h>
#include <WebCore/HTMLDirectoryElement.h>
#include <WebCore/HTMLDivElement.h>
#include <WebCore/HTMLElement.h>
#include <WebCore/HTMLEmbedElement.h>
#include <WebCore/HTMLFieldSetElement.h>
#include <WebCore/HTMLFontElement.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLFrameElement.h>
#include <WebCore/HTMLFrameSetElement.h>
#include <WebCore/HTMLHRElement.h>
#include <WebCore/HTMLHeadElement.h>
#include <WebCore/HTMLHeadingElement.h>
#include <WebCore/HTMLHtmlElement.h>
#include <WebCore/HTMLIFrameElement.h>
#include <WebCore/HTMLImageElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLKeygenElement.h>
#include <WebCore/HTMLLIElement.h>
#include <WebCore/HTMLLabelElement.h>
#include <WebCore/HTMLLegendElement.h>
#include <WebCore/HTMLLinkElement.h>
#include <WebCore/HTMLMapElement.h>
#include <WebCore/HTMLMarqueeElement.h>
#include <WebCore/HTMLMenuElement.h>
#include <WebCore/HTMLMetaElement.h>
#include <WebCore/HTMLModElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLOListElement.h>
#include <WebCore/HTMLObjectElement.h>
#include <WebCore/HTMLOptGroupElement.h>
#include <WebCore/HTMLOptionElement.h>
#include <WebCore/HTMLParagraphElement.h>
#include <WebCore/HTMLParamElement.h>
#include <WebCore/HTMLPreElement.h>
#include <WebCore/HTMLQuoteElement.h>
#include <WebCore/HTMLScriptElement.h>
#include <WebCore/HTMLSelectElement.h>
#include <WebCore/HTMLStyleElement.h>
#include <WebCore/HTMLTableCaptionElement.h>
#include <WebCore/HTMLTableCellElement.h>
#include <WebCore/HTMLTableColElement.h>
#include <WebCore/HTMLTableElement.h>
#include <WebCore/HTMLTableRowElement.h>
#include <WebCore/HTMLTableSectionElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/HTMLTitleElement.h>
#include <WebCore/HTMLUListElement.h>
#include <WebCore/HTMLVideoElement.h>
#include "WebKitDOMHTMLAnchorElementPrivate.h"
#include "WebKitDOMHTMLAppletElementPrivate.h"
#include "WebKitDOMHTMLAreaElementPrivate.h"
#include "WebKitDOMHTMLBRElementPrivate.h"
#include "WebKitDOMHTMLBaseElementPrivate.h"
#include "WebKitDOMHTMLBodyElementPrivate.h"
#include "WebKitDOMHTMLButtonElementPrivate.h"
#include "WebKitDOMHTMLCanvasElementPrivate.h"
#include "WebKitDOMHTMLDListElementPrivate.h"
#include "WebKitDOMHTMLDirectoryElementPrivate.h"
#include "WebKitDOMHTMLDivElementPrivate.h"
#include "WebKitDOMHTMLElementPrivate.h"
#include "WebKitDOMHTMLEmbedElementPrivate.h"
#include "WebKitDOMHTMLFieldSetElementPrivate.h"
#include "WebKitDOMHTMLFontElementPrivate.h"
#include "WebKitDOMHTMLFormElementPrivate.h"
#include "WebKitDOMHTMLFrameElementPrivate.h"
#include "WebKitDOMHTMLFrameSetElementPrivate.h"
#include "WebKitDOMHTMLHRElementPrivate.h"
#include "WebKitDOMHTMLHeadElementPrivate.h"
#include "WebKitDOMHTMLHeadingElementPrivate.h"
#include "WebKitDOMHTMLHtmlElementPrivate.h"
#include "WebKitDOMHTMLIFrameElementPrivate.h"
#include "WebKitDOMHTMLImageElementPrivate.h"
#include "WebKitDOMHTMLInputElementPrivate.h"
#include "WebKitDOMHTMLLIElementPrivate.h"
#include "WebKitDOMHTMLLabelElementPrivate.h"
#include "WebKitDOMHTMLLegendElementPrivate.h"
#include "WebKitDOMHTMLLinkElementPrivate.h"
#include "WebKitDOMHTMLMapElementPrivate.h"
#include "WebKitDOMHTMLMarqueeElementPrivate.h"
#include "WebKitDOMHTMLMenuElementPrivate.h"
#include "WebKitDOMHTMLMetaElementPrivate.h"
#include "WebKitDOMHTMLModElementPrivate.h"
#include "WebKitDOMHTMLOListElementPrivate.h"
#include "WebKitDOMHTMLObjectElementPrivate.h"
#include "WebKitDOMHTMLOptGroupElementPrivate.h"
#include "WebKitDOMHTMLOptionElementPrivate.h"
#include "WebKitDOMHTMLParagraphElementPrivate.h"
#include "WebKitDOMHTMLParamElementPrivate.h"
#include "WebKitDOMHTMLPreElementPrivate.h"
#include "WebKitDOMHTMLQuoteElementPrivate.h"
#include "WebKitDOMHTMLScriptElementPrivate.h"
#include "WebKitDOMHTMLSelectElementPrivate.h"
#include "WebKitDOMHTMLStyleElementPrivate.h"
#include "WebKitDOMHTMLTableCaptionElementPrivate.h"
#include "WebKitDOMHTMLTableCellElementPrivate.h"
#include "WebKitDOMHTMLTableColElementPrivate.h"
#include "WebKitDOMHTMLTableElementPrivate.h"
#include "WebKitDOMHTMLTableRowElementPrivate.h"
#include "WebKitDOMHTMLTableSectionElementPrivate.h"
#include "WebKitDOMHTMLTextAreaElementPrivate.h"
#include "WebKitDOMHTMLTitleElementPrivate.h"
#include "WebKitDOMHTMLUListElementPrivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

using namespace WebCore;
using namespace WebCore::HTMLNames;

// macro(TagName, ElementName)

#define FOR_EACH_HTML_TAG(macro) \
    macro(a, Anchor) \
    macro(applet, Applet) \
    macro(area, Area) \
    macro(base, Base) \
    macro(blockquote, Quote) \
    macro(body, Body) \
    macro(br, BR) \
    macro(button, Button) \
    macro(canvas, Canvas) \
    macro(caption, TableCaption) \
    macro(col, TableCol) \
    macro(del, Mod) \
    macro(dir, Directory) \
    macro(div, Div) \
    macro(dl, DList) \
    macro(embed, Embed) \
    macro(fieldset, FieldSet) \
    macro(font, Font) \
    macro(form, Form) \
    macro(frame, Frame) \
    macro(frameset, FrameSet) \
    macro(h1, Heading) \
    macro(head, Head) \
    macro(hr, HR) \
    macro(html, Html) \
    macro(iframe, IFrame) \
    macro(img, Image) \
    macro(input, Input) \
    macro(label, Label) \
    macro(legend, Legend) \
    macro(li, LI) \
    macro(link, Link) \
    macro(map, Map) \
    macro(marquee, Marquee) \
    macro(menu, Menu) \
    macro(meta, Meta) \
    macro(object, Object) \
    macro(ol, OList) \
    macro(optgroup, OptGroup) \
    macro(option, Option) \
    macro(p, Paragraph) \
    macro(param, Param) \
    macro(pre, Pre) \
    macro(q, Quote) \
    macro(script, Script) \
    macro(select, Select) \
    macro(style, Style) \
    macro(table, Table) \
    macro(tbody, TableSection) \
    macro(td, TableCell) \
    macro(textarea, TextArea) \
    macro(title, Title) \
    macro(tr, TableRow) \
    macro(ul, UList) \
    macro(colgroup, TableCol) \
    macro(h2, Heading) \
    macro(h3, Heading) \
    macro(h4, Heading) \
    macro(h5, Heading) \
    macro(h6, Heading) \
    macro(image, Image) \
    macro(ins, Mod) \
    macro(listing, Pre) \
    macro(tfoot, TableSection) \
    macro(th, TableCell) \
    macro(thead, TableSection) \
    macro(xmp, Pre)

#define DEFINE_HTML_WRAPPER(TagName, ElementName) \
    static WebKitDOMHTMLElement* TagName##Wrapper(HTMLElement* element) \
    { \
        return WEBKIT_DOM_HTML_ELEMENT(wrapHTML##ElementName##Element(static_cast<HTML##ElementName##Element*>(element))); \
    }
    FOR_EACH_HTML_TAG(DEFINE_HTML_WRAPPER)
#undef DEFINE_HTML_WRAPPER

typedef WebKitDOMHTMLElement* (*HTMLElementWrapFunction)(HTMLElement*);

WebKitDOMHTMLElement* wrap(HTMLElement* element)
{
    static HashMap<const QualifiedName::QualifiedNameImpl*, HTMLElementWrapFunction> map;
    if (map.isEmpty()) {
#define ADD_HTML_WRAPPER(TagName, ElementName) map.set(TagName##Tag->impl(), TagName##Wrapper);
        FOR_EACH_HTML_TAG(ADD_HTML_WRAPPER)
#undef ADD_HTML_WRAPPER
    }

    if (HTMLElementWrapFunction wrapFunction = map.get(element->tagQName().impl()))
        return wrapFunction(element);

    return wrapHTMLElement(element);
}

}
G_GNUC_END_IGNORE_DEPRECATIONS;
