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

using namespace HTMLNames;

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
    return new HTMLHeadingElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
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
    return new HTMLPreElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
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
    return new HTMLModElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
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
    return new HTMLTableColElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

HTMLElementImpl* tableRowConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableRowElementImpl(docPtr);
}

HTMLElementImpl* tableCellConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableCellElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

HTMLElementImpl* tableSectionConstructor(const AtomicString& tagName, DocumentPtr* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableSectionElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr, false);
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
        gFunctionMap->insert(htmlTag.localName().impl(), (void*)&htmlConstructor);
        gFunctionMap->insert(headTag.localName().impl(), (void*)&headConstructor);
        gFunctionMap->insert(bodyTag.localName().impl(), (void*)&bodyConstructor);
        gFunctionMap->insert(baseTag.localName().impl(), (void*)&baseConstructor);
        gFunctionMap->insert(linkTag.localName().impl(), (void*)&linkConstructor);
        gFunctionMap->insert(metaTag.localName().impl(), (void*)&metaConstructor);
        gFunctionMap->insert(styleTag.localName().impl(), (void*)&styleConstructor);
        gFunctionMap->insert(titleTag.localName().impl(), (void*)&titleConstructor);
        gFunctionMap->insert(frameTag.localName().impl(), (void*)&frameConstructor);
        gFunctionMap->insert(framesetTag.localName().impl(), (void*)&framesetConstructor);
        gFunctionMap->insert(iframeTag.localName().impl(), (void*)&iframeConstructor);
        gFunctionMap->insert(formTag.localName().impl(), (void*)&formConstructor);
        gFunctionMap->insert(buttonTag.localName().impl(), (void*)&buttonConstructor);
        gFunctionMap->insert(inputTag.localName().impl(), (void*)&inputConstructor);
        gFunctionMap->insert(isindexTag.localName().impl(), (void*)&isindexConstructor);
        gFunctionMap->insert(fieldsetTag.localName().impl(), (void*)&fieldsetConstructor);
        gFunctionMap->insert(labelTag.localName().impl(), (void*)&labelConstructor);
        gFunctionMap->insert(legendTag.localName().impl(), (void*)&legendConstructor);
        gFunctionMap->insert(optgroupTag.localName().impl(), (void*)&optgroupConstructor);
        gFunctionMap->insert(optionTag.localName().impl(), (void*)&optionConstructor);
        gFunctionMap->insert(selectTag.localName().impl(), (void*)&selectConstructor);
        gFunctionMap->insert(textareaTag.localName().impl(), (void*)&textareaConstructor);
        gFunctionMap->insert(dlTag.localName().impl(), (void*)&dlConstructor);
        gFunctionMap->insert(olTag.localName().impl(), (void*)&olConstructor);
        gFunctionMap->insert(ulTag.localName().impl(), (void*)&ulConstructor);
        gFunctionMap->insert(dirTag.localName().impl(), (void*)&dirConstructor);
        gFunctionMap->insert(menuTag.localName().impl(), (void*)&menuConstructor);
        gFunctionMap->insert(liTag.localName().impl(), (void*)&liConstructor);
        gFunctionMap->insert(blockquoteTag.localName().impl(), (void*)&blockquoteConstructor);
        gFunctionMap->insert(divTag.localName().impl(), (void*)&divConstructor);
        gFunctionMap->insert(h1Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->insert(h2Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->insert(h3Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->insert(h4Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->insert(h5Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->insert(h6Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->insert(hrTag.localName().impl(), (void*)&hrConstructor);
        gFunctionMap->insert(pTag.localName().impl(), (void*)&paragraphConstructor);
        gFunctionMap->insert(preTag.localName().impl(), (void*)&preConstructor);
        gFunctionMap->insert(xmpTag.localName().impl(), (void*)&preConstructor);
        gFunctionMap->insert(basefontTag.localName().impl(), (void*)&basefontConstructor);
        gFunctionMap->insert(fontTag.localName().impl(), (void*)&fontConstructor);
        gFunctionMap->insert(delTag.localName().impl(), (void*)&modConstructor);
        gFunctionMap->insert(insTag.localName().impl(), (void*)&modConstructor);
        gFunctionMap->insert(aTag.localName().impl(), (void*)&anchorConstructor);
        gFunctionMap->insert(imageTag.localName().impl(), (void*)&imageConstructor);
        gFunctionMap->insert(imgTag.localName().impl(), (void*)&imageConstructor);
        gFunctionMap->insert(mapTag.localName().impl(), (void*)&mapConstructor);
        gFunctionMap->insert(areaTag.localName().impl(), (void*)&areaConstructor);
        gFunctionMap->insert(canvasTag.localName().impl(), (void*)&canvasConstructor);
        gFunctionMap->insert(appletTag.localName().impl(), (void*)&appletConstructor);
        gFunctionMap->insert(embedTag.localName().impl(), (void*)&embedConstructor);
        gFunctionMap->insert(objectTag.localName().impl(), (void*)&objectConstructor);
        gFunctionMap->insert(paramTag.localName().impl(), (void*)&paramConstructor);
        gFunctionMap->insert(scriptTag.localName().impl(), (void*)&scriptConstructor);
        gFunctionMap->insert(tableTag.localName().impl(), (void*)&tableConstructor);
        gFunctionMap->insert(captionTag.localName().impl(), (void*)&tableCaptionConstructor);
        gFunctionMap->insert(colgroupTag.localName().impl(), (void*)&tableColConstructor);
        gFunctionMap->insert(colTag.localName().impl(), (void*)&tableColConstructor);
        gFunctionMap->insert(trTag.localName().impl(), (void*)&tableRowConstructor);
        gFunctionMap->insert(tdTag.localName().impl(), (void*)&tableCellConstructor);
        gFunctionMap->insert(thTag.localName().impl(), (void*)&tableCellConstructor);
        gFunctionMap->insert(theadTag.localName().impl(), (void*)&tableSectionConstructor);
        gFunctionMap->insert(tbodyTag.localName().impl(), (void*)&tableSectionConstructor);
        gFunctionMap->insert(tfootTag.localName().impl(), (void*)&tableSectionConstructor);
        gFunctionMap->insert(brTag.localName().impl(), (void*)&brConstructor);
        gFunctionMap->insert(qTag.localName().impl(), (void*)&quoteConstructor);
        gFunctionMap->insert(marqueeTag.localName().impl(), (void*)&marqueeConstructor);
    }
    
    void* result = gFunctionMap->get(tagName.impl());
    if (result) {
        ConstructorFunc func = (ConstructorFunc)result;
        return (func)(tagName, docPtr, form, createdByParser);
    }
 
    // elements with no special representation in the DOM
    return new HTMLElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);

}

}

