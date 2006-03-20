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

#include "html_baseimpl.h"
#include "html_blockimpl.h"
#include "HTMLCanvasElement.h"
#include "HTMLDocument.h"
#include "html_headimpl.h"
#include "html_imageimpl.h"
#include "html_listimpl.h"
#include "html_tableimpl.h"
#include "html_objectimpl.h"

#include "HTMLCollection.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLIsIndexElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLLabelElement.h"
#include "HTMLLegendElement.h"
#include "HTMLButtonElement.h"
#include "HTMLOptionElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLBaseFontElement.h"

#include <kxmlcore/HashMap.h>

namespace WebCore
{

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

PassRefPtr<HTMLElement> HTMLElementFactory::createHTMLElement(const AtomicString& tagName, Document* doc, HTMLFormElement* form, bool createdByParser)
{
    if (!doc)
        return 0; // Don't allow elements to ever be made without having a doc.

    if (!gFunctionMap) {
        // Create the table.
        gFunctionMap = new FunctionMap;
        
        // Populate it with constructor functions.
        gFunctionMap->set(htmlTag.localName().impl(), htmlConstructor);
        gFunctionMap->set(headTag.localName().impl(), headConstructor);
        gFunctionMap->set(bodyTag.localName().impl(), bodyConstructor);
        gFunctionMap->set(baseTag.localName().impl(), baseConstructor);
        gFunctionMap->set(linkTag.localName().impl(), linkConstructor);
        gFunctionMap->set(metaTag.localName().impl(), metaConstructor);
        gFunctionMap->set(styleTag.localName().impl(), styleConstructor);
        gFunctionMap->set(titleTag.localName().impl(), titleConstructor);
        gFunctionMap->set(frameTag.localName().impl(), frameConstructor);
        gFunctionMap->set(framesetTag.localName().impl(), framesetConstructor);
        gFunctionMap->set(iframeTag.localName().impl(), iframeConstructor);
        gFunctionMap->set(formTag.localName().impl(), formConstructor);
        gFunctionMap->set(buttonTag.localName().impl(), buttonConstructor);
        gFunctionMap->set(inputTag.localName().impl(), inputConstructor);
        gFunctionMap->set(isindexTag.localName().impl(), isindexConstructor);
        gFunctionMap->set(fieldsetTag.localName().impl(), fieldsetConstructor);
        gFunctionMap->set(labelTag.localName().impl(), labelConstructor);
        gFunctionMap->set(legendTag.localName().impl(), legendConstructor);
        gFunctionMap->set(optgroupTag.localName().impl(), optgroupConstructor);
        gFunctionMap->set(optionTag.localName().impl(), optionConstructor);
        gFunctionMap->set(selectTag.localName().impl(), selectConstructor);
        gFunctionMap->set(textareaTag.localName().impl(), textareaConstructor);
        gFunctionMap->set(dlTag.localName().impl(), dlConstructor);
        gFunctionMap->set(olTag.localName().impl(), olConstructor);
        gFunctionMap->set(ulTag.localName().impl(), ulConstructor);
        gFunctionMap->set(dirTag.localName().impl(), dirConstructor);
        gFunctionMap->set(menuTag.localName().impl(), menuConstructor);
        gFunctionMap->set(liTag.localName().impl(), liConstructor);
        gFunctionMap->set(blockquoteTag.localName().impl(), blockquoteConstructor);
        gFunctionMap->set(divTag.localName().impl(), divConstructor);
        gFunctionMap->set(h1Tag.localName().impl(), headingConstructor);
        gFunctionMap->set(h2Tag.localName().impl(), headingConstructor);
        gFunctionMap->set(h3Tag.localName().impl(), headingConstructor);
        gFunctionMap->set(h4Tag.localName().impl(), headingConstructor);
        gFunctionMap->set(h5Tag.localName().impl(), headingConstructor);
        gFunctionMap->set(h6Tag.localName().impl(), headingConstructor);
        gFunctionMap->set(hrTag.localName().impl(), hrConstructor);
        gFunctionMap->set(pTag.localName().impl(), paragraphConstructor);
        gFunctionMap->set(preTag.localName().impl(), preConstructor);
        gFunctionMap->set(xmpTag.localName().impl(), preConstructor);
        gFunctionMap->set(basefontTag.localName().impl(), basefontConstructor);
        gFunctionMap->set(fontTag.localName().impl(), fontConstructor);
        gFunctionMap->set(delTag.localName().impl(), modConstructor);
        gFunctionMap->set(insTag.localName().impl(), modConstructor);
        gFunctionMap->set(aTag.localName().impl(), anchorConstructor);
        gFunctionMap->set(imageTag.localName().impl(), imageConstructor);
        gFunctionMap->set(imgTag.localName().impl(), imageConstructor);
        gFunctionMap->set(mapTag.localName().impl(), mapConstructor);
        gFunctionMap->set(areaTag.localName().impl(), areaConstructor);
        gFunctionMap->set(canvasTag.localName().impl(), canvasConstructor);
        gFunctionMap->set(appletTag.localName().impl(), appletConstructor);
        gFunctionMap->set(embedTag.localName().impl(), embedConstructor);
        gFunctionMap->set(objectTag.localName().impl(), objectConstructor);
        gFunctionMap->set(paramTag.localName().impl(), paramConstructor);
        gFunctionMap->set(scriptTag.localName().impl(), scriptConstructor);
        gFunctionMap->set(tableTag.localName().impl(), tableConstructor);
        gFunctionMap->set(captionTag.localName().impl(), tableCaptionConstructor);
        gFunctionMap->set(colgroupTag.localName().impl(), tableColConstructor);
        gFunctionMap->set(colTag.localName().impl(), tableColConstructor);
        gFunctionMap->set(trTag.localName().impl(), tableRowConstructor);
        gFunctionMap->set(tdTag.localName().impl(), tableCellConstructor);
        gFunctionMap->set(thTag.localName().impl(), tableCellConstructor);
        gFunctionMap->set(theadTag.localName().impl(), tableSectionConstructor);
        gFunctionMap->set(tbodyTag.localName().impl(), tableSectionConstructor);
        gFunctionMap->set(tfootTag.localName().impl(), tableSectionConstructor);
        gFunctionMap->set(brTag.localName().impl(), brConstructor);
        gFunctionMap->set(qTag.localName().impl(), quoteConstructor);
        gFunctionMap->set(marqueeTag.localName().impl(), marqueeConstructor);
    }
    
    ConstructorFunc func = gFunctionMap->get(tagName.impl());
    if (func)
        return func(tagName, doc, form, createdByParser);
 
    // elements with no special representation in the DOM
    return new HTMLElement(QualifiedName(nullAtom, tagName, xhtmlNamespaceURI), doc);
}

}

