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

#include "config.h"
#include "htmlfactory.h"
#include "dom_docimpl.h"

#include "html/html_baseimpl.h"
#include "html/html_blockimpl.h"
#include "html/html_canvasimpl.h"
#include "html/html_documentimpl.h"
#include "html/html_headimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_listimpl.h"
#include "html/html_miscimpl.h"
#include "html/html_tableimpl.h"
#include "html/html_objectimpl.h"

#include "HTMLFormElementImpl.h"
#include "HTMLInputElementImpl.h"
#include "HTMLIsIndexElementImpl.h"
#include "HTMLFieldSetElementImpl.h"
#include "HTMLLabelElementImpl.h"
#include "HTMLLegendElementImpl.h"
#include "HTMLButtonElementImpl.h"
#include "HTMLOptionElementImpl.h"
#include "HTMLOptGroupElementImpl.h"
#include "HTMLSelectElementImpl.h"
#include "HTMLTextAreaElementImpl.h"

#include <kxmlcore/HashMap.h>

namespace DOM
{

using namespace HTMLNames;

typedef HTMLElementImpl* (*ConstructorFunc)(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser);
typedef HashMap<DOMStringImpl *, void*, PointerHash<DOMStringImpl *> > FunctionMap;
static FunctionMap* gFunctionMap;

HTMLElementImpl* htmlConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLHtmlElementImpl(docPtr);
}

HTMLElementImpl* headConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLHeadElementImpl(docPtr);
}

HTMLElementImpl* bodyConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLBodyElementImpl(docPtr);
}

HTMLElementImpl* baseConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLBaseElementImpl(docPtr);
}

HTMLElementImpl* linkConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLLinkElementImpl(docPtr);
}

HTMLElementImpl* metaConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLMetaElementImpl(docPtr);
}

HTMLElementImpl* styleConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLStyleElementImpl(docPtr);
}

HTMLElementImpl* titleConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTitleElementImpl(docPtr);
}

HTMLElementImpl* frameConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLFrameElementImpl(docPtr);
}

HTMLElementImpl* framesetConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLFrameSetElementImpl(docPtr);
}

HTMLElementImpl* iframeConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLIFrameElementImpl(docPtr);
}

HTMLElementImpl* formConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLFormElementImpl(docPtr);
}

HTMLElementImpl* buttonConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLButtonElementImpl(docPtr, form);
}

HTMLElementImpl* inputConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLInputElementImpl(docPtr, form);
}

HTMLElementImpl* isindexConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLIsIndexElementImpl(docPtr, form);
}

HTMLElementImpl* fieldsetConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLFieldSetElementImpl(docPtr, form);
}

HTMLElementImpl* labelConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLLabelElementImpl(docPtr);
}

HTMLElementImpl* legendConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLLegendElementImpl(docPtr, form);
}

HTMLElementImpl* optgroupConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLOptGroupElementImpl(docPtr, form);
}

HTMLElementImpl* optionConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLOptionElementImpl(docPtr, form);
}

HTMLElementImpl* selectConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLSelectElementImpl(docPtr, form);
}

HTMLElementImpl* textareaConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTextAreaElementImpl(docPtr, form);
}

HTMLElementImpl* dlConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLDListElementImpl(docPtr);
}

HTMLElementImpl* ulConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLUListElementImpl(docPtr);
}

HTMLElementImpl* olConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLOListElementImpl(docPtr);
}

HTMLElementImpl* dirConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLDirectoryElementImpl(docPtr);
}

HTMLElementImpl* menuConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLMenuElementImpl(docPtr);
}

HTMLElementImpl* liConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLLIElementImpl(docPtr);
}

HTMLElementImpl* blockquoteConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLBlockquoteElementImpl(docPtr);
}

HTMLElementImpl* divConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLDivElementImpl(docPtr);
}

HTMLElementImpl* headingConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLHeadingElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

HTMLElementImpl* hrConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLHRElementImpl(docPtr);
}

HTMLElementImpl* paragraphConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLParagraphElementImpl(docPtr);
}

HTMLElementImpl* preConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLPreElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

HTMLElementImpl* basefontConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLBaseFontElementImpl(docPtr);
}

HTMLElementImpl* fontConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLFontElementImpl(docPtr);
}

HTMLElementImpl* modConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLModElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

HTMLElementImpl* anchorConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLAnchorElementImpl(docPtr);
}

HTMLElementImpl* imageConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLImageElementImpl(docPtr, form);
}

HTMLElementImpl* mapConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLMapElementImpl(docPtr);
}

HTMLElementImpl* areaConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLAreaElementImpl(docPtr);
}

HTMLElementImpl* canvasConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLCanvasElementImpl(docPtr);
}

HTMLElementImpl* appletConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLAppletElementImpl(docPtr);
}

HTMLElementImpl* embedConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLEmbedElementImpl(docPtr);
}

HTMLElementImpl* objectConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    HTMLObjectElementImpl * object = new HTMLObjectElementImpl(docPtr);
    object->setComplete(!createdByParser);
    return object;
}

HTMLElementImpl* paramConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLParamElementImpl(docPtr);
}

HTMLElementImpl* scriptConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    HTMLScriptElementImpl* script = new HTMLScriptElementImpl(docPtr);
    script->setCreatedByParser(createdByParser);
    return script;
}

HTMLElementImpl* tableConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableElementImpl(docPtr);
}

HTMLElementImpl* tableCaptionConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableCaptionElementImpl(docPtr);
}

HTMLElementImpl* tableColConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableColElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

HTMLElementImpl* tableRowConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableRowElementImpl(docPtr);
}

HTMLElementImpl* tableCellConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableCellElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

HTMLElementImpl* tableSectionConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLTableSectionElementImpl(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr, false);
}

HTMLElementImpl* brConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLBRElementImpl(docPtr);
}

HTMLElementImpl* quoteConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLQuoteElementImpl(docPtr);
}

HTMLElementImpl* marqueeConstructor(const AtomicString& tagName, DocumentImpl* docPtr, HTMLFormElementImpl* form, bool createdByParser)
{
    return new HTMLMarqueeElementImpl(docPtr);
}

HTMLElementImpl* HTMLElementFactory::createHTMLElement(const AtomicString& tagName, DocumentImpl* doc, HTMLFormElementImpl* form, bool createdByParser)
{
    if (!doc)
        return 0; // Don't allow elements to ever be made without having a doc.

    DocumentImpl* docPtr = doc;
    if (!gFunctionMap) {
        // Create the table.
        gFunctionMap = new FunctionMap;
        
        // Populate it with constructor functions.
        gFunctionMap->set(htmlTag.localName().impl(), (void*)&htmlConstructor);
        gFunctionMap->set(headTag.localName().impl(), (void*)&headConstructor);
        gFunctionMap->set(bodyTag.localName().impl(), (void*)&bodyConstructor);
        gFunctionMap->set(baseTag.localName().impl(), (void*)&baseConstructor);
        gFunctionMap->set(linkTag.localName().impl(), (void*)&linkConstructor);
        gFunctionMap->set(metaTag.localName().impl(), (void*)&metaConstructor);
        gFunctionMap->set(styleTag.localName().impl(), (void*)&styleConstructor);
        gFunctionMap->set(titleTag.localName().impl(), (void*)&titleConstructor);
        gFunctionMap->set(frameTag.localName().impl(), (void*)&frameConstructor);
        gFunctionMap->set(framesetTag.localName().impl(), (void*)&framesetConstructor);
        gFunctionMap->set(iframeTag.localName().impl(), (void*)&iframeConstructor);
        gFunctionMap->set(formTag.localName().impl(), (void*)&formConstructor);
        gFunctionMap->set(buttonTag.localName().impl(), (void*)&buttonConstructor);
        gFunctionMap->set(inputTag.localName().impl(), (void*)&inputConstructor);
        gFunctionMap->set(isindexTag.localName().impl(), (void*)&isindexConstructor);
        gFunctionMap->set(fieldsetTag.localName().impl(), (void*)&fieldsetConstructor);
        gFunctionMap->set(labelTag.localName().impl(), (void*)&labelConstructor);
        gFunctionMap->set(legendTag.localName().impl(), (void*)&legendConstructor);
        gFunctionMap->set(optgroupTag.localName().impl(), (void*)&optgroupConstructor);
        gFunctionMap->set(optionTag.localName().impl(), (void*)&optionConstructor);
        gFunctionMap->set(selectTag.localName().impl(), (void*)&selectConstructor);
        gFunctionMap->set(textareaTag.localName().impl(), (void*)&textareaConstructor);
        gFunctionMap->set(dlTag.localName().impl(), (void*)&dlConstructor);
        gFunctionMap->set(olTag.localName().impl(), (void*)&olConstructor);
        gFunctionMap->set(ulTag.localName().impl(), (void*)&ulConstructor);
        gFunctionMap->set(dirTag.localName().impl(), (void*)&dirConstructor);
        gFunctionMap->set(menuTag.localName().impl(), (void*)&menuConstructor);
        gFunctionMap->set(liTag.localName().impl(), (void*)&liConstructor);
        gFunctionMap->set(blockquoteTag.localName().impl(), (void*)&blockquoteConstructor);
        gFunctionMap->set(divTag.localName().impl(), (void*)&divConstructor);
        gFunctionMap->set(h1Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->set(h2Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->set(h3Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->set(h4Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->set(h5Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->set(h6Tag.localName().impl(), (void*)&headingConstructor);
        gFunctionMap->set(hrTag.localName().impl(), (void*)&hrConstructor);
        gFunctionMap->set(pTag.localName().impl(), (void*)&paragraphConstructor);
        gFunctionMap->set(preTag.localName().impl(), (void*)&preConstructor);
        gFunctionMap->set(xmpTag.localName().impl(), (void*)&preConstructor);
        gFunctionMap->set(basefontTag.localName().impl(), (void*)&basefontConstructor);
        gFunctionMap->set(fontTag.localName().impl(), (void*)&fontConstructor);
        gFunctionMap->set(delTag.localName().impl(), (void*)&modConstructor);
        gFunctionMap->set(insTag.localName().impl(), (void*)&modConstructor);
        gFunctionMap->set(aTag.localName().impl(), (void*)&anchorConstructor);
        gFunctionMap->set(imageTag.localName().impl(), (void*)&imageConstructor);
        gFunctionMap->set(imgTag.localName().impl(), (void*)&imageConstructor);
        gFunctionMap->set(mapTag.localName().impl(), (void*)&mapConstructor);
        gFunctionMap->set(areaTag.localName().impl(), (void*)&areaConstructor);
        gFunctionMap->set(canvasTag.localName().impl(), (void*)&canvasConstructor);
        gFunctionMap->set(appletTag.localName().impl(), (void*)&appletConstructor);
        gFunctionMap->set(embedTag.localName().impl(), (void*)&embedConstructor);
        gFunctionMap->set(objectTag.localName().impl(), (void*)&objectConstructor);
        gFunctionMap->set(paramTag.localName().impl(), (void*)&paramConstructor);
        gFunctionMap->set(scriptTag.localName().impl(), (void*)&scriptConstructor);
        gFunctionMap->set(tableTag.localName().impl(), (void*)&tableConstructor);
        gFunctionMap->set(captionTag.localName().impl(), (void*)&tableCaptionConstructor);
        gFunctionMap->set(colgroupTag.localName().impl(), (void*)&tableColConstructor);
        gFunctionMap->set(colTag.localName().impl(), (void*)&tableColConstructor);
        gFunctionMap->set(trTag.localName().impl(), (void*)&tableRowConstructor);
        gFunctionMap->set(tdTag.localName().impl(), (void*)&tableCellConstructor);
        gFunctionMap->set(thTag.localName().impl(), (void*)&tableCellConstructor);
        gFunctionMap->set(theadTag.localName().impl(), (void*)&tableSectionConstructor);
        gFunctionMap->set(tbodyTag.localName().impl(), (void*)&tableSectionConstructor);
        gFunctionMap->set(tfootTag.localName().impl(), (void*)&tableSectionConstructor);
        gFunctionMap->set(brTag.localName().impl(), (void*)&brConstructor);
        gFunctionMap->set(qTag.localName().impl(), (void*)&quoteConstructor);
        gFunctionMap->set(marqueeTag.localName().impl(), (void*)&marqueeConstructor);
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

