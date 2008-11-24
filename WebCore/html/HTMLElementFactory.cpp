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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "HTMLElementFactory.h"

#include "HTMLAnchorElement.h"
#include "HTMLAppletElement.h"
#include "HTMLAreaElement.h"
#include "HTMLAudioElement.h"
#include "HTMLBRElement.h"
#include "HTMLBaseElement.h"
#include "HTMLBaseFontElement.h"
#include "HTMLBlockquoteElement.h"
#include "HTMLBodyElement.h"
#include "HTMLButtonElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDListElement.h"
#include "HTMLDirectoryElement.h"
#include "HTMLDivElement.h"
#include "HTMLDocument.h"
#include "HTMLEmbedElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFontElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHRElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHeadingElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLIsIndexElement.h"
#include "HTMLKeygenElement.h"
#include "HTMLLIElement.h"
#include "HTMLLabelElement.h"
#include "HTMLLegendElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMapElement.h"
#include "HTMLMarqueeElement.h"
#include "HTMLMenuElement.h"
#include "HTMLMetaElement.h"
#include "HTMLModElement.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLObjectElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParagraphElement.h"
#include "HTMLParamElement.h"
#include "HTMLPreElement.h"
#include "HTMLQuoteElement.h"
#include "HTMLScriptElement.h"
#include "HTMLSelectElement.h"
#include "HTMLSourceElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableColElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTitleElement.h"
#include "HTMLUListElement.h"
#include "HTMLVideoElement.h"

namespace WebCore {

using namespace HTMLNames;

typedef PassRefPtr<HTMLElement> (*ConstructorFunc)(const QualifiedName& tagName, Document*, HTMLFormElement*, bool createdByParser);
typedef HashMap<AtomicStringImpl*, ConstructorFunc> FunctionMap;
static FunctionMap* gFunctionMap;

static PassRefPtr<HTMLElement> htmlConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLHtmlElement(doc);
}

static PassRefPtr<HTMLElement> headConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLHeadElement(doc);
}

static PassRefPtr<HTMLElement> bodyConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLBodyElement(doc);
}

static PassRefPtr<HTMLElement> baseConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLBaseElement(doc);
}

static PassRefPtr<HTMLElement> linkConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool createdByParser)
{
    RefPtr<HTMLLinkElement> link = new HTMLLinkElement(doc);
    link->setCreatedByParser(createdByParser);
    return link.release();
}

static PassRefPtr<HTMLElement> metaConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLMetaElement(doc);
}

static PassRefPtr<HTMLElement> styleConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool createdByParser)
{
    RefPtr<HTMLStyleElement> style = new HTMLStyleElement(doc);
    style->setCreatedByParser(createdByParser);
    return style.release();
}

static PassRefPtr<HTMLElement> titleConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLTitleElement(doc);
}

static PassRefPtr<HTMLElement> frameConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool createdByParser)
{
    RefPtr<HTMLFrameElement> frame = new HTMLFrameElement(doc);
    frame->setCreatedByParser(createdByParser);
    return frame.release();    
}

static PassRefPtr<HTMLElement> framesetConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLFrameSetElement(doc);
}

static PassRefPtr<HTMLElement> iframeConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool createdByParser)
{
    RefPtr<HTMLIFrameElement> iFrame = new HTMLIFrameElement(doc);
    iFrame->setCreatedByParser(createdByParser);
    return iFrame.release();    
}

static PassRefPtr<HTMLElement> formConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLFormElement(doc);
}

static PassRefPtr<HTMLElement> buttonConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLButtonElement(doc, form);
}

static PassRefPtr<HTMLElement> inputConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLInputElement(doc, form);
}

static PassRefPtr<HTMLElement> isindexConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLIsIndexElement(doc, form);
}

static PassRefPtr<HTMLElement> fieldsetConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLFieldSetElement(doc, form);
}

static PassRefPtr<HTMLElement> keygenConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLKeygenElement(doc, form);
}

static PassRefPtr<HTMLElement> labelConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLLabelElement(doc);
}

static PassRefPtr<HTMLElement> legendConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLLegendElement(doc, form);
}

static PassRefPtr<HTMLElement> optgroupConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLOptGroupElement(doc, form);
}

static PassRefPtr<HTMLElement> optionConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLOptionElement(doc, form);
}

static PassRefPtr<HTMLElement> selectConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLSelectElement(doc, form);
}

static PassRefPtr<HTMLElement> textareaConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLTextAreaElement(doc, form);
}

static PassRefPtr<HTMLElement> dlConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLDListElement(doc);
}

static PassRefPtr<HTMLElement> ulConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLUListElement(doc);
}

static PassRefPtr<HTMLElement> olConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLOListElement(doc);
}

static PassRefPtr<HTMLElement> dirConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLDirectoryElement(doc);
}

static PassRefPtr<HTMLElement> menuConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLMenuElement(doc);
}

static PassRefPtr<HTMLElement> liConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLLIElement(doc);
}

static PassRefPtr<HTMLElement> blockquoteConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLBlockquoteElement(doc);
}

static PassRefPtr<HTMLElement> divConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLDivElement(doc);
}

static PassRefPtr<HTMLElement> headingConstructor(const QualifiedName& tagName, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLHeadingElement(tagName, doc);
}

static PassRefPtr<HTMLElement> hrConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLHRElement(doc);
}

static PassRefPtr<HTMLElement> paragraphConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLParagraphElement(doc);
}

static PassRefPtr<HTMLElement> preConstructor(const QualifiedName& tagName, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLPreElement(tagName, doc);
}

static PassRefPtr<HTMLElement> basefontConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLBaseFontElement(doc);
}

static PassRefPtr<HTMLElement> fontConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLFontElement(doc);
}

static PassRefPtr<HTMLElement> modConstructor(const QualifiedName& tagName, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLModElement(tagName, doc);
}

static PassRefPtr<HTMLElement> anchorConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLAnchorElement(doc);
}

static PassRefPtr<HTMLElement> imageConstructor(const QualifiedName&, Document* doc, HTMLFormElement* form, bool)
{
    return new HTMLImageElement(doc, form);
}

static PassRefPtr<HTMLElement> mapConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLMapElement(doc);
}

static PassRefPtr<HTMLElement> areaConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLAreaElement(doc);
}

static PassRefPtr<HTMLElement> canvasConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLCanvasElement(doc);
}

static PassRefPtr<HTMLElement> appletConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLAppletElement(doc);
}

static PassRefPtr<HTMLElement> embedConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLEmbedElement(doc);
}

static PassRefPtr<HTMLElement> objectConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool createdByParser)
{
    RefPtr<HTMLObjectElement> object = new HTMLObjectElement(doc, createdByParser);
    return object.release();
}

static PassRefPtr<HTMLElement> paramConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLParamElement(doc);
}

static PassRefPtr<HTMLElement> scriptConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool createdByParser)
{
    RefPtr<HTMLScriptElement> script = new HTMLScriptElement(doc);
    script->setCreatedByParser(createdByParser);
    return script.release();
}

static PassRefPtr<HTMLElement> tableConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLTableElement(doc);
}

static PassRefPtr<HTMLElement> tableCaptionConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLTableCaptionElement(doc);
}

static PassRefPtr<HTMLElement> tableColConstructor(const QualifiedName& tagName, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLTableColElement(tagName, doc);
}

static PassRefPtr<HTMLElement> tableRowConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLTableRowElement(doc);
}

static PassRefPtr<HTMLElement> tableCellConstructor(const QualifiedName& tagName, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLTableCellElement(tagName, doc);
}

static PassRefPtr<HTMLElement> tableSectionConstructor(const QualifiedName& tagName, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLTableSectionElement(tagName, doc);
}

static PassRefPtr<HTMLElement> brConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLBRElement(doc);
}

static PassRefPtr<HTMLElement> quoteConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLQuoteElement(doc);
}

static PassRefPtr<HTMLElement> marqueeConstructor(const QualifiedName&, Document* doc, HTMLFormElement*, bool)
{
    return new HTMLMarqueeElement(doc);
}

#if ENABLE(VIDEO)
static PassRefPtr<HTMLElement> audioConstructor(const QualifiedName& tagName, Document* doc, HTMLFormElement*, bool)
{
    if (!MediaPlayer::isAvailable())
        return new HTMLElement(tagName, doc);
    return new HTMLAudioElement(doc);
}

static PassRefPtr<HTMLElement> videoConstructor(const QualifiedName& tagName, Document* doc, HTMLFormElement*, bool)
{
    if (!MediaPlayer::isAvailable())
        return new HTMLElement(tagName, doc);
    return new HTMLVideoElement(doc);
}

static PassRefPtr<HTMLElement> sourceConstructor(const QualifiedName& tagName, Document* doc, HTMLFormElement*, bool)
{
    if (!MediaPlayer::isAvailable())
        return new HTMLElement(tagName, doc);
    return new HTMLSourceElement(doc);
}
#endif

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
    addTag(keygenTag, keygenConstructor);
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
#if ENABLE(VIDEO)
    addTag(audioTag, audioConstructor);
    addTag(sourceTag, sourceConstructor);
    addTag(videoTag, videoConstructor);
#endif
}

PassRefPtr<HTMLElement> HTMLElementFactory::createHTMLElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* form, bool createdByParser)
{
    if (!doc)
        return 0; // Don't allow elements to ever be made without having a doc.

    if (!gFunctionMap)
        createFunctionMap();

    ConstructorFunc func = gFunctionMap->get(tagName.localName().impl());
    if (func)
        return func(tagName, doc, form, createdByParser);

    // elements with no special representation in the DOM
    return new HTMLElement(tagName, doc);
}

}

