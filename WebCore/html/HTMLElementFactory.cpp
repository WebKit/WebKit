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
#include "HTMLElementFactory.h"

#include "HTMLBaseFontElement.h"
#include "HTMLButtonElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLIsIndexElement.h"
#include "HTMLLabelElement.h"
#include "HTMLLegendElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "html_baseimpl.h"
#include "html_blockimpl.h"
#include "html_headimpl.h"
#include "html_imageimpl.h"
#include "html_listimpl.h"
#include "html_objectimpl.h"
#include "html_tableimpl.h"
#include <kxmlcore/HashMap.h>

namespace WebCore {

using namespace HTMLNames;

typedef PassRefPtr<HTMLElement> (*ConstructorFunc)(const AtomicString& tagName, Document*, HTMLFormElement*, bool createdByParser);
typedef HashMap<AtomicStringImpl*, ConstructorFunc> FunctionMap;
static FunctionMap* gFunctionMap;

PassRefPtr<HTMLElement> htmlConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLHtmlElement(docPtr);
}

PassRefPtr<HTMLElement> headConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLHeadElement(docPtr);
}

PassRefPtr<HTMLElement> bodyConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLBodyElement(docPtr);
}

PassRefPtr<HTMLElement> baseConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLBaseElement(docPtr);
}

PassRefPtr<HTMLElement> linkConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLLinkElement(docPtr);
}

PassRefPtr<HTMLElement> metaConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLMetaElement(docPtr);
}

PassRefPtr<HTMLElement> styleConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLStyleElement(docPtr);
}

PassRefPtr<HTMLElement> titleConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLTitleElement(docPtr);
}

PassRefPtr<HTMLElement> frameConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLFrameElement(docPtr);
}

PassRefPtr<HTMLElement> framesetConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLFrameSetElement(docPtr);
}

PassRefPtr<HTMLElement> iframeConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLIFrameElement(docPtr);
}

PassRefPtr<HTMLElement> formConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLFormElement(docPtr);
}

PassRefPtr<HTMLElement> buttonConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLButtonElement(docPtr, form);
}

PassRefPtr<HTMLElement> inputConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLInputElement(docPtr, form);
}

PassRefPtr<HTMLElement> isindexConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLIsIndexElement(docPtr, form);
}

PassRefPtr<HTMLElement> fieldsetConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLFieldSetElement(docPtr, form);
}

PassRefPtr<HTMLElement> labelConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLLabelElement(docPtr);
}

PassRefPtr<HTMLElement> legendConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLLegendElement(docPtr, form);
}

PassRefPtr<HTMLElement> optgroupConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLOptGroupElement(docPtr, form);
}

PassRefPtr<HTMLElement> optionConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLOptionElement(docPtr, form);
}

PassRefPtr<HTMLElement> selectConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLSelectElement(docPtr, form);
}

PassRefPtr<HTMLElement> textareaConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLTextAreaElement(docPtr, form);
}

PassRefPtr<HTMLElement> dlConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLDListElement(docPtr);
}

PassRefPtr<HTMLElement> ulConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLUListElement(docPtr);
}

PassRefPtr<HTMLElement> olConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLOListElement(docPtr);
}

PassRefPtr<HTMLElement> dirConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLDirectoryElement(docPtr);
}

PassRefPtr<HTMLElement> menuConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLMenuElement(docPtr);
}

PassRefPtr<HTMLElement> liConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLLIElement(docPtr);
}

PassRefPtr<HTMLElement> blockquoteConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLBlockquoteElement(docPtr);
}

PassRefPtr<HTMLElement> divConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLDivElement(docPtr);
}

PassRefPtr<HTMLElement> headingConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLHeadingElement(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

PassRefPtr<HTMLElement> hrConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLHRElement(docPtr);
}

PassRefPtr<HTMLElement> paragraphConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLParagraphElement(docPtr);
}

PassRefPtr<HTMLElement> preConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLPreElement(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

PassRefPtr<HTMLElement> basefontConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLBaseFontElement(docPtr);
}

PassRefPtr<HTMLElement> fontConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLFontElement(docPtr);
}

PassRefPtr<HTMLElement> modConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLModElement(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

PassRefPtr<HTMLElement> anchorConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLAnchorElement(docPtr);
}

PassRefPtr<HTMLElement> imageConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLImageElement(docPtr, form);
}

PassRefPtr<HTMLElement> mapConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLMapElement(docPtr);
}

PassRefPtr<HTMLElement> areaConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLAreaElement(docPtr);
}

PassRefPtr<HTMLElement> canvasConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLCanvasElement(docPtr);
}

PassRefPtr<HTMLElement> appletConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLAppletElement(docPtr);
}

PassRefPtr<HTMLElement> embedConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLEmbedElement(docPtr);
}

PassRefPtr<HTMLElement> objectConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    RefPtr<HTMLObjectElement> object = new HTMLObjectElement(docPtr);
    object->setComplete(!createdByParser);
    return object.release();
}

PassRefPtr<HTMLElement> paramConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLParamElement(docPtr);
}

PassRefPtr<HTMLElement> scriptConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    RefPtr<HTMLScriptElement> script = new HTMLScriptElement(docPtr);
    script->setCreatedByParser(createdByParser);
    return script.release();
}

PassRefPtr<HTMLElement> tableConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLTableElement(docPtr);
}

PassRefPtr<HTMLElement> tableCaptionConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLTableCaptionElement(docPtr);
}

PassRefPtr<HTMLElement> tableColConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLTableColElement(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

PassRefPtr<HTMLElement> tableRowConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLTableRowElement(docPtr);
}

PassRefPtr<HTMLElement> tableCellConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLTableCellElement(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr);
}

PassRefPtr<HTMLElement> tableSectionConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLTableSectionElement(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), docPtr, false);
}

PassRefPtr<HTMLElement> brConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLBRElement(docPtr);
}

PassRefPtr<HTMLElement> quoteConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLQuoteElement(docPtr);
}

PassRefPtr<HTMLElement> marqueeConstructor(const AtomicString& tagName, Document* docPtr, HTMLFormElement* form, bool createdByParser)
{
    return new HTMLMarqueeElement(docPtr);
}

static void addTag(const QualifiedName& tag, ConstructorFunc func)
{
    gFunctionMap->set(tag.localName().impl(), func);
}

static void createFunctionMap()
{
    // Create the table.
    gFunctionMap = new FunctionMap;
    
    // Populate it with constructor functions.
    addTag(aTag, anchorConstructor);
    addTag(appletTag, appletConstructor);
    addTag(areaTag, areaConstructor);
    addTag(baseTag, baseConstructor);
    addTag(basefontTag, basefontConstructor);
    addTag(blockquoteTag, blockquoteConstructor);
    addTag(bodyTag, bodyConstructor);
    addTag(brTag, brConstructor);
    addTag(buttonTag, buttonConstructor);
    addTag(canvasTag, canvasConstructor);
    addTag(captionTag, tableCaptionConstructor);
    addTag(colTag, tableColConstructor);
    addTag(colgroupTag, tableColConstructor);
    addTag(delTag, modConstructor);
    addTag(dirTag, dirConstructor);
    addTag(divTag, divConstructor);
    addTag(dlTag, dlConstructor);
    addTag(embedTag, embedConstructor);
    addTag(fieldsetTag, fieldsetConstructor);
    addTag(fontTag, fontConstructor);
    addTag(formTag, formConstructor);
    addTag(frameTag, frameConstructor);
    addTag(framesetTag, framesetConstructor);
    addTag(h1Tag, headingConstructor);
    addTag(h2Tag, headingConstructor);
    addTag(h3Tag, headingConstructor);
    addTag(h4Tag, headingConstructor);
    addTag(h5Tag, headingConstructor);
    addTag(h6Tag, headingConstructor);
    addTag(headTag, headConstructor);
    addTag(hrTag, hrConstructor);
    addTag(htmlTag, htmlConstructor);
    addTag(iframeTag, iframeConstructor);
    addTag(imageTag, imageConstructor);
    addTag(imgTag, imageConstructor);
    addTag(inputTag, inputConstructor);
    addTag(insTag, modConstructor);
    addTag(isindexTag, isindexConstructor);
    addTag(labelTag, labelConstructor);
    addTag(legendTag, legendConstructor);
    addTag(liTag, liConstructor);
    addTag(linkTag, linkConstructor);
    addTag(listingTag, preConstructor);
    addTag(mapTag, mapConstructor);
    addTag(marqueeTag, marqueeConstructor);
    addTag(menuTag, menuConstructor);
    addTag(metaTag, metaConstructor);
    addTag(objectTag, objectConstructor);
    addTag(olTag, olConstructor);
    addTag(optgroupTag, optgroupConstructor);
    addTag(optionTag, optionConstructor);
    addTag(pTag, paragraphConstructor);
    addTag(paramTag, paramConstructor);
    addTag(preTag, preConstructor);
    addTag(qTag, quoteConstructor);
    addTag(scriptTag, scriptConstructor);
    addTag(selectTag, selectConstructor);
    addTag(styleTag, styleConstructor);
    addTag(tableTag, tableConstructor);
    addTag(tbodyTag, tableSectionConstructor);
    addTag(tdTag, tableCellConstructor);
    addTag(textareaTag, textareaConstructor);
    addTag(tfootTag, tableSectionConstructor);
    addTag(thTag, tableCellConstructor);
    addTag(theadTag, tableSectionConstructor);
    addTag(titleTag, titleConstructor);
    addTag(trTag, tableRowConstructor);
    addTag(ulTag, ulConstructor);
    addTag(xmpTag, preConstructor);
}

PassRefPtr<HTMLElement> HTMLElementFactory::createHTMLElement(const AtomicString& tagName, Document* doc, HTMLFormElement* form, bool createdByParser)
{
    if (!doc)
        return 0; // Don't allow elements to ever be made without having a doc.

    if (!gFunctionMap)
        createFunctionMap();
    
    ConstructorFunc func = gFunctionMap->get(tagName.impl());
    if (func)
        return func(tagName, doc, form, createdByParser);
 
    // elements with no special representation in the DOM
    return new HTMLElement(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), doc);
}

}

