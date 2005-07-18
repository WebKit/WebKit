/*
 * This file is part of the HTML DOM implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "htmlfactory.h"
#include "dom_docimpl.h"

#include "html/html_baseimpl.h"
#include "html/html_blockimpl.h"
#include "html/html_canvasimpl.h"
#include "html/html_documentimpl.h"
#include "html/html_formimpl.h"
#include "html/html_headimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_listimpl.h"
#include "html/html_miscimpl.h"
#include "html/html_tableimpl.h"
#include "html/html_objectimpl.h"

#include "misc/pointerhash.h"

using khtml::PointerHash;
using khtml::HashMap;

namespace DOM
{

typedef HTMLElementImpl* (*ConstructorFunc)(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser);
typedef HashMap<DOMStringImpl *, void*, PointerHash<DOMStringImpl *> > FunctionMap;
static FunctionMap* gFunctionMap;

HTMLElementImpl* htmlConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLHtmlElementImpl(docPtr);
}

HTMLElementImpl* headConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLHeadElementImpl(docPtr);
}

HTMLElementImpl* bodyConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLBodyElementImpl(docPtr);
}

HTMLElementImpl* baseConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLBaseElementImpl(docPtr);
}

HTMLElementImpl* linkConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLLinkElementImpl(docPtr);
}

HTMLElementImpl* metaConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLMetaElementImpl(docPtr);
}

HTMLElementImpl* styleConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLStyleElementImpl(docPtr);
}

HTMLElementImpl* titleConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTitleElementImpl(docPtr);
}

HTMLElementImpl* frameConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLFrameElementImpl(docPtr);
}

HTMLElementImpl* framesetConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLFrameSetElementImpl(docPtr);
}

HTMLElementImpl* iframeConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLIFrameElementImpl(docPtr);
}

HTMLElementImpl* formConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLFormElementImpl(docPtr);
}

HTMLElementImpl* buttonConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLButtonElementImpl(docPtr, form);
}

HTMLElementImpl* inputConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLInputElementImpl(docPtr, form);
}

HTMLElementImpl* isindexConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLIsIndexElementImpl(docPtr, form);
}

HTMLElementImpl* fieldsetConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLFieldSetElementImpl(docPtr, form);
}

HTMLElementImpl* labelConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLLabelElementImpl(docPtr);
}

HTMLElementImpl* legendConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLLegendElementImpl(docPtr, form);
}

HTMLElementImpl* optgroupConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLOptGroupElementImpl(docPtr, form);
}

HTMLElementImpl* optionConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLOptionElementImpl(docPtr, form);
}

HTMLElementImpl* selectConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLSelectElementImpl(docPtr, form);
}

HTMLElementImpl* textareaConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTextAreaElementImpl(docPtr, form);
}

HTMLElementImpl* dlConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLDListElementImpl(docPtr);
}

HTMLElementImpl* ulConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLUListElementImpl(docPtr);
}

HTMLElementImpl* olConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLOListElementImpl(docPtr);
}

HTMLElementImpl* dirConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLDirectoryElementImpl(docPtr);
}

HTMLElementImpl* menuConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLMenuElementImpl(docPtr);
}

HTMLElementImpl* liConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLLIElementImpl(docPtr);
}

HTMLElementImpl* blockquoteConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLBlockquoteElementImpl(docPtr);
}

HTMLElementImpl* divConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLDivElementImpl(docPtr);
}

HTMLElementImpl* headingConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLHeadingElementImpl(QualifiedName(nullAtom, tagName, HTMLTags::xhtmlNamespaceURI()), docPtr);
}

HTMLElementImpl* hrConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLHRElementImpl(docPtr);
}

HTMLElementImpl* paragraphConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLParagraphElementImpl(docPtr);
}

HTMLElementImpl* preConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLPreElementImpl(QualifiedName(nullAtom, tagName, HTMLTags::xhtmlNamespaceURI()), docPtr);
}

HTMLElementImpl* basefontConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLBaseFontElementImpl(docPtr);
}

HTMLElementImpl* fontConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLFontElementImpl(docPtr);
}

HTMLElementImpl* modConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLModElementImpl(QualifiedName(nullAtom, tagName, HTMLTags::xhtmlNamespaceURI()), docPtr);
}

HTMLElementImpl* anchorConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLAnchorElementImpl(docPtr);
}

HTMLElementImpl* imageConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLImageElementImpl(docPtr, form);
}

HTMLElementImpl* mapConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLMapElementImpl(docPtr);
}

HTMLElementImpl* areaConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLAreaElementImpl(docPtr);
}

HTMLElementImpl* canvasConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLCanvasElementImpl(docPtr);
}

HTMLElementImpl* appletConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLAppletElementImpl(docPtr);
}

HTMLElementImpl* embedConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLEmbedElementImpl(docPtr);
}

HTMLElementImpl* objectConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLObjectElementImpl(docPtr);
}

HTMLElementImpl* paramConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLParamElementImpl(docPtr);
}

HTMLElementImpl* scriptConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    HTMLScriptElementImpl* script = new HTMLScriptElementImpl(docPtr);
    script->setCreatedByParser(createdByParser);
    return script;
}

HTMLElementImpl* tableConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableElementImpl(docPtr);
}

HTMLElementImpl* tableCaptionConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableCaptionElementImpl(docPtr);
}

HTMLElementImpl* tableColConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableColElementImpl(QualifiedName(nullAtom, tagName, HTMLTags::xhtmlNamespaceURI()), docPtr);
}

HTMLElementImpl* tableRowConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableRowElementImpl(docPtr);
}

HTMLElementImpl* tableCellConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableCellElementImpl(QualifiedName(nullAtom, tagName, HTMLTags::xhtmlNamespaceURI()), docPtr);
}

HTMLElementImpl* tableSectionConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableSectionElementImpl(QualifiedName(nullAtom, tagName, HTMLTags::xhtmlNamespaceURI()), docPtr, false);
}

HTMLElementImpl* brConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLBRElementImpl(docPtr);
}

HTMLElementImpl* quoteConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLQuoteElementImpl(docPtr);
}

HTMLElementImpl* marqueeConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLMarqueeElementImpl(docPtr);
}

HTMLElementImpl* HTMLElementFactory::createHTMLElement(const AtomicString& tagName, DocumentImpl* doc, HTMLFormElementImpl* form, bool createdByParser)
{
    if (!doc)
        return 0; // Don't allow elements to ever be made without having a doc.

    DocumentPtr* docPtr = doc->docPtr();
    if (!gFunctionMap) {
        // Create the table.
        gFunctionMap = new FunctionMap;
        
        // Populate it with constructor functions.
        gFunctionMap->insert(HTMLTags::html().localName().implementation(), (void*)&htmlConstructor);
        gFunctionMap->insert(HTMLTags::head().localName().implementation(), (void*)&headConstructor);
        gFunctionMap->insert(HTMLTags::body().localName().implementation(), (void*)&bodyConstructor);
        gFunctionMap->insert(HTMLTags::base().localName().implementation(), (void*)&baseConstructor);
        gFunctionMap->insert(HTMLTags::link().localName().implementation(), (void*)&linkConstructor);
        gFunctionMap->insert(HTMLTags::meta().localName().implementation(), (void*)&metaConstructor);
        gFunctionMap->insert(HTMLTags::style().localName().implementation(), (void*)&styleConstructor);
        gFunctionMap->insert(HTMLTags::title().localName().implementation(), (void*)&titleConstructor);
        gFunctionMap->insert(HTMLTags::frame().localName().implementation(), (void*)&frameConstructor);
        gFunctionMap->insert(HTMLTags::frameset().localName().implementation(), (void*)&framesetConstructor);
        gFunctionMap->insert(HTMLTags::iframe().localName().implementation(), (void*)&iframeConstructor);
        gFunctionMap->insert(HTMLTags::form().localName().implementation(), (void*)&formConstructor);
        gFunctionMap->insert(HTMLTags::button().localName().implementation(), (void*)&buttonConstructor);
        gFunctionMap->insert(HTMLTags::input().localName().implementation(), (void*)&inputConstructor);
        gFunctionMap->insert(HTMLTags::isindex().localName().implementation(), (void*)&isindexConstructor);
        gFunctionMap->insert(HTMLTags::fieldset().localName().implementation(), (void*)&fieldsetConstructor);
        gFunctionMap->insert(HTMLTags::label().localName().implementation(), (void*)&labelConstructor);
        gFunctionMap->insert(HTMLTags::legend().localName().implementation(), (void*)&legendConstructor);
        gFunctionMap->insert(HTMLTags::optgroup().localName().implementation(), (void*)&optgroupConstructor);
        gFunctionMap->insert(HTMLTags::option().localName().implementation(), (void*)&optionConstructor);
        gFunctionMap->insert(HTMLTags::select().localName().implementation(), (void*)&selectConstructor);
        gFunctionMap->insert(HTMLTags::textarea().localName().implementation(), (void*)&textareaConstructor);
        gFunctionMap->insert(HTMLTags::dl().localName().implementation(), (void*)&dlConstructor);
        gFunctionMap->insert(HTMLTags::ol().localName().implementation(), (void*)&olConstructor);
        gFunctionMap->insert(HTMLTags::ul().localName().implementation(), (void*)&ulConstructor);
        gFunctionMap->insert(HTMLTags::dir().localName().implementation(), (void*)&dirConstructor);
        gFunctionMap->insert(HTMLTags::menu().localName().implementation(), (void*)&menuConstructor);
        gFunctionMap->insert(HTMLTags::li().localName().implementation(), (void*)&liConstructor);
        gFunctionMap->insert(HTMLTags::blockquote().localName().implementation(), (void*)&blockquoteConstructor);
        gFunctionMap->insert(HTMLTags::div().localName().implementation(), (void*)&divConstructor);
        gFunctionMap->insert(HTMLTags::h1().localName().implementation(), (void*)&headingConstructor);
        gFunctionMap->insert(HTMLTags::h2().localName().implementation(), (void*)&headingConstructor);
        gFunctionMap->insert(HTMLTags::h3().localName().implementation(), (void*)&headingConstructor);
        gFunctionMap->insert(HTMLTags::h4().localName().implementation(), (void*)&headingConstructor);
        gFunctionMap->insert(HTMLTags::h5().localName().implementation(), (void*)&headingConstructor);
        gFunctionMap->insert(HTMLTags::h6().localName().implementation(), (void*)&headingConstructor);
        gFunctionMap->insert(HTMLTags::hr().localName().implementation(), (void*)&hrConstructor);
        gFunctionMap->insert(HTMLTags::p().localName().implementation(), (void*)&paragraphConstructor);
        gFunctionMap->insert(HTMLTags::pre().localName().implementation(), (void*)&preConstructor);
        gFunctionMap->insert(HTMLTags::xmp().localName().implementation(), (void*)&preConstructor);
        gFunctionMap->insert(HTMLTags::basefont().localName().implementation(), (void*)&basefontConstructor);
        gFunctionMap->insert(HTMLTags::font().localName().implementation(), (void*)&fontConstructor);
        gFunctionMap->insert(HTMLTags::del().localName().implementation(), (void*)&modConstructor);
        gFunctionMap->insert(HTMLTags::ins().localName().implementation(), (void*)&modConstructor);
        gFunctionMap->insert(HTMLTags::a().localName().implementation(), (void*)&anchorConstructor);
        gFunctionMap->insert(HTMLTags::img().localName().implementation(), (void*)&imageConstructor);
        gFunctionMap->insert(HTMLTags::map().localName().implementation(), (void*)&mapConstructor);
        gFunctionMap->insert(HTMLTags::area().localName().implementation(), (void*)&areaConstructor);
        gFunctionMap->insert(HTMLTags::canvas().localName().implementation(), (void*)&canvasConstructor);
        gFunctionMap->insert(HTMLTags::applet().localName().implementation(), (void*)&appletConstructor);
        gFunctionMap->insert(HTMLTags::embed().localName().implementation(), (void*)&embedConstructor);
        gFunctionMap->insert(HTMLTags::object().localName().implementation(), (void*)&objectConstructor);
        gFunctionMap->insert(HTMLTags::param().localName().implementation(), (void*)&paramConstructor);
        gFunctionMap->insert(HTMLTags::script().localName().implementation(), (void*)&scriptConstructor);
        gFunctionMap->insert(HTMLTags::table().localName().implementation(), (void*)&tableConstructor);
        gFunctionMap->insert(HTMLTags::caption().localName().implementation(), (void*)&tableCaptionConstructor);
        gFunctionMap->insert(HTMLTags::colgroup().localName().implementation(), (void*)&tableColConstructor);
        gFunctionMap->insert(HTMLTags::col().localName().implementation(), (void*)&tableColConstructor);
        gFunctionMap->insert(HTMLTags::tr().localName().implementation(), (void*)&tableRowConstructor);
        gFunctionMap->insert(HTMLTags::td().localName().implementation(), (void*)&tableCellConstructor);
        gFunctionMap->insert(HTMLTags::th().localName().implementation(), (void*)&tableCellConstructor);
        gFunctionMap->insert(HTMLTags::thead().localName().implementation(), (void*)&tableSectionConstructor);
        gFunctionMap->insert(HTMLTags::tbody().localName().implementation(), (void*)&tableSectionConstructor);
        gFunctionMap->insert(HTMLTags::tfoot().localName().implementation(), (void*)&tableSectionConstructor);
        gFunctionMap->insert(HTMLTags::br().localName().implementation(), (void*)&brConstructor);
        gFunctionMap->insert(HTMLTags::q().localName().implementation(), (void*)&quoteConstructor);
        gFunctionMap->insert(HTMLTags::marquee().localName().implementation(), (void*)&marqueeConstructor);
    }
    
    void* result = gFunctionMap->get(tagName.implementation());
    if (result) {
        ConstructorFunc func = (ConstructorFunc)result;
        return (func)(tagName, docPtr, form, createdByParser);
    }
 
    // elements with no special representation in the DOM
    return new HTMLElementImpl(QualifiedName(nullAtom, tagName, HTMLTags::xhtmlNamespaceURI()), docPtr);

}

}

