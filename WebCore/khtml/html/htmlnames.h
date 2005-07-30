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
#ifndef HTMLNAMES_H
#define HTMLNAMES_H

#include "dom_qname.h"

namespace HTMLNames {

#if !KHTML_HTMLNAMES_HIDE_GLOBALS
    // Namespace
    extern const DOM::AtomicString  xhtmlNamespaceURI;

    // Tags
    extern const DOM::QualifiedName aTag;
    extern const DOM::QualifiedName abbrTag;
    extern const DOM::QualifiedName acronymTag;
    extern const DOM::QualifiedName addressTag;
    extern const DOM::QualifiedName appletTag;
    extern const DOM::QualifiedName areaTag;
    extern const DOM::QualifiedName bTag;
    extern const DOM::QualifiedName baseTag;
    extern const DOM::QualifiedName basefontTag;
    extern const DOM::QualifiedName bdoTag;
    extern const DOM::QualifiedName bigTag;
    extern const DOM::QualifiedName blockquoteTag;
    extern const DOM::QualifiedName bodyTag;
    extern const DOM::QualifiedName brTag;
    extern const DOM::QualifiedName buttonTag;
    extern const DOM::QualifiedName canvasTag;
    extern const DOM::QualifiedName captionTag;
    extern const DOM::QualifiedName centerTag;
    extern const DOM::QualifiedName citeTag;
    extern const DOM::QualifiedName codeTag;
    extern const DOM::QualifiedName colTag;
    extern const DOM::QualifiedName colgroupTag;
    extern const DOM::QualifiedName ddTag;
    extern const DOM::QualifiedName delTag;
    extern const DOM::QualifiedName dfnTag;
    extern const DOM::QualifiedName dirTag;
    extern const DOM::QualifiedName divTag;
    extern const DOM::QualifiedName dlTag;
    extern const DOM::QualifiedName dtTag;
    extern const DOM::QualifiedName emTag;
    extern const DOM::QualifiedName embedTag;
    extern const DOM::QualifiedName fieldsetTag;
    extern const DOM::QualifiedName fontTag;
    extern const DOM::QualifiedName formTag;
    extern const DOM::QualifiedName frameTag;
    extern const DOM::QualifiedName framesetTag;
    extern const DOM::QualifiedName headTag;
    extern const DOM::QualifiedName h1Tag;
    extern const DOM::QualifiedName h2Tag;
    extern const DOM::QualifiedName h3Tag;
    extern const DOM::QualifiedName h4Tag;
    extern const DOM::QualifiedName h5Tag;
    extern const DOM::QualifiedName h6Tag;
    extern const DOM::QualifiedName hrTag;
    extern const DOM::QualifiedName htmlTag;
    extern const DOM::QualifiedName iTag;
    extern const DOM::QualifiedName iframeTag;
    extern const DOM::QualifiedName imgTag;
    extern const DOM::QualifiedName inputTag;
    extern const DOM::QualifiedName insTag;
    extern const DOM::QualifiedName isindexTag;
    extern const DOM::QualifiedName kbdTag;
    extern const DOM::QualifiedName keygenTag;
    extern const DOM::QualifiedName labelTag;
    extern const DOM::QualifiedName layerTag;
    extern const DOM::QualifiedName legendTag;
    extern const DOM::QualifiedName liTag;
    extern const DOM::QualifiedName linkTag;
    extern const DOM::QualifiedName mapTag;
    extern const DOM::QualifiedName marqueeTag;
    extern const DOM::QualifiedName menuTag;
    extern const DOM::QualifiedName metaTag;
    extern const DOM::QualifiedName nobrTag;
    extern const DOM::QualifiedName noembedTag;
    extern const DOM::QualifiedName noframesTag;
    extern const DOM::QualifiedName nolayerTag;
    extern const DOM::QualifiedName noscriptTag;
    extern const DOM::QualifiedName objectTag;
    extern const DOM::QualifiedName olTag;
    extern const DOM::QualifiedName optgroupTag;
    extern const DOM::QualifiedName optionTag;
    extern const DOM::QualifiedName pTag;
    extern const DOM::QualifiedName paramTag;
    extern const DOM::QualifiedName plaintextTag;
    extern const DOM::QualifiedName preTag;
    extern const DOM::QualifiedName qTag;
    extern const DOM::QualifiedName sTag;
    extern const DOM::QualifiedName sampTag;
    extern const DOM::QualifiedName scriptTag;
    extern const DOM::QualifiedName selectTag;
    extern const DOM::QualifiedName smallTag;
    extern const DOM::QualifiedName spanTag;
    extern const DOM::QualifiedName strikeTag;
    extern const DOM::QualifiedName strongTag;
    extern const DOM::QualifiedName styleTag;
    extern const DOM::QualifiedName subTag;
    extern const DOM::QualifiedName supTag;
    extern const DOM::QualifiedName tableTag;
    extern const DOM::QualifiedName tbodyTag;
    extern const DOM::QualifiedName tdTag;
    extern const DOM::QualifiedName textareaTag;
    extern const DOM::QualifiedName tfootTag;
    extern const DOM::QualifiedName thTag;
    extern const DOM::QualifiedName theadTag;
    extern const DOM::QualifiedName titleTag;
    extern const DOM::QualifiedName trTag;
    extern const DOM::QualifiedName ttTag;
    extern const DOM::QualifiedName uTag;
    extern const DOM::QualifiedName ulTag;
    extern const DOM::QualifiedName varTag;
    extern const DOM::QualifiedName wbrTag;
    extern const DOM::QualifiedName xmpTag;

    // Attributes
    extern const DOM::QualifiedName abbrAttr;
    extern const DOM::QualifiedName accept_charsetAttr;
    extern const DOM::QualifiedName acceptAttr;
    extern const DOM::QualifiedName accesskeyAttr;
    extern const DOM::QualifiedName actionAttr;
    extern const DOM::QualifiedName alignAttr;
    extern const DOM::QualifiedName alinkAttr;
    extern const DOM::QualifiedName altAttr;
    extern const DOM::QualifiedName archiveAttr;
    extern const DOM::QualifiedName autocompleteAttr;
    extern const DOM::QualifiedName autosaveAttr;
    extern const DOM::QualifiedName axisAttr;
    extern const DOM::QualifiedName backgroundAttr;
    extern const DOM::QualifiedName behaviorAttr;
    extern const DOM::QualifiedName bgcolorAttr;
    extern const DOM::QualifiedName bgpropertiesAttr;
    extern const DOM::QualifiedName borderAttr;
    extern const DOM::QualifiedName bordercolorAttr;
    extern const DOM::QualifiedName cellpaddingAttr;
    extern const DOM::QualifiedName cellspacingAttr;
    extern const DOM::QualifiedName charAttr;
    extern const DOM::QualifiedName challengeAttr;
    extern const DOM::QualifiedName charoffAttr;
    extern const DOM::QualifiedName charsetAttr;
    extern const DOM::QualifiedName checkedAttr;
    extern const DOM::QualifiedName cellborderAttr;
    extern const DOM::QualifiedName citeAttr;
    extern const DOM::QualifiedName classAttr;
    extern const DOM::QualifiedName classidAttr;
    extern const DOM::QualifiedName clearAttr;
    extern const DOM::QualifiedName codeAttr;
    extern const DOM::QualifiedName codebaseAttr;
    extern const DOM::QualifiedName codetypeAttr;
    extern const DOM::QualifiedName colorAttr;
    extern const DOM::QualifiedName colsAttr;
    extern const DOM::QualifiedName colspanAttr;
    extern const DOM::QualifiedName compactAttr;
    extern const DOM::QualifiedName compositeAttr;
    extern const DOM::QualifiedName contentAttr;
    extern const DOM::QualifiedName contenteditableAttr;
    extern const DOM::QualifiedName coordsAttr;
    extern const DOM::QualifiedName dataAttr;
    extern const DOM::QualifiedName datetimeAttr;
    extern const DOM::QualifiedName declareAttr;
    extern const DOM::QualifiedName deferAttr;
    extern const DOM::QualifiedName dirAttr;
    extern const DOM::QualifiedName directionAttr;
    extern const DOM::QualifiedName disabledAttr;
    extern const DOM::QualifiedName enctypeAttr;
    extern const DOM::QualifiedName faceAttr;
    extern const DOM::QualifiedName forAttr;
    extern const DOM::QualifiedName frameAttr;
    extern const DOM::QualifiedName frameborderAttr;
    extern const DOM::QualifiedName headersAttr;
    extern const DOM::QualifiedName heightAttr;
    extern const DOM::QualifiedName hiddenAttr;
    extern const DOM::QualifiedName hrefAttr;
    extern const DOM::QualifiedName hreflangAttr;
    extern const DOM::QualifiedName hspaceAttr;
    extern const DOM::QualifiedName http_equivAttr;
    extern const DOM::QualifiedName idAttr;
    extern const DOM::QualifiedName incrementalAttr;
    extern const DOM::QualifiedName ismapAttr;
    extern const DOM::QualifiedName keytypeAttr;
    extern const DOM::QualifiedName labelAttr;
    extern const DOM::QualifiedName langAttr;
    extern const DOM::QualifiedName languageAttr;
    extern const DOM::QualifiedName leftAttr;
    extern const DOM::QualifiedName leftmarginAttr;
    extern const DOM::QualifiedName linkAttr;
    extern const DOM::QualifiedName longdescAttr;
    extern const DOM::QualifiedName loopAttr;
    extern const DOM::QualifiedName marginheightAttr;
    extern const DOM::QualifiedName marginwidthAttr;
    extern const DOM::QualifiedName maxAttr;
    extern const DOM::QualifiedName maxlengthAttr;
    extern const DOM::QualifiedName mayscriptAttr;
    extern const DOM::QualifiedName mediaAttr;
    extern const DOM::QualifiedName methodAttr;
    extern const DOM::QualifiedName minAttr;
    extern const DOM::QualifiedName multipleAttr;
    extern const DOM::QualifiedName nameAttr;
    extern const DOM::QualifiedName nohrefAttr;
    extern const DOM::QualifiedName noresizeAttr;
    extern const DOM::QualifiedName noshadeAttr;
    extern const DOM::QualifiedName nowrapAttr;
    extern const DOM::QualifiedName objectAttr;
    extern const DOM::QualifiedName onabortAttr;
    extern const DOM::QualifiedName onbeforecopyAttr;
    extern const DOM::QualifiedName onbeforecutAttr;
    extern const DOM::QualifiedName onbeforepasteAttr;
    extern const DOM::QualifiedName onblurAttr;
    extern const DOM::QualifiedName onchangeAttr;
    extern const DOM::QualifiedName onclickAttr;
    extern const DOM::QualifiedName oncontextmenuAttr;
    extern const DOM::QualifiedName oncopyAttr;
    extern const DOM::QualifiedName oncutAttr;
    extern const DOM::QualifiedName ondblclickAttr;
    extern const DOM::QualifiedName ondragAttr;
    extern const DOM::QualifiedName ondragendAttr;
    extern const DOM::QualifiedName ondragenterAttr;
    extern const DOM::QualifiedName ondragleaveAttr;
    extern const DOM::QualifiedName ondragoverAttr;
    extern const DOM::QualifiedName ondragstartAttr;
    extern const DOM::QualifiedName ondropAttr;
    extern const DOM::QualifiedName onerrorAttr;
    extern const DOM::QualifiedName onfocusAttr;
    extern const DOM::QualifiedName oninputAttr;
    extern const DOM::QualifiedName onkeydownAttr;
    extern const DOM::QualifiedName onkeypressAttr;
    extern const DOM::QualifiedName onkeyupAttr;
    extern const DOM::QualifiedName onloadAttr;
    extern const DOM::QualifiedName onmousedownAttr;
    extern const DOM::QualifiedName onmousemoveAttr;
    extern const DOM::QualifiedName onmouseoutAttr;
    extern const DOM::QualifiedName onmouseoverAttr;
    extern const DOM::QualifiedName onmouseupAttr;
    extern const DOM::QualifiedName onmousewheelAttr;
    extern const DOM::QualifiedName onpasteAttr;
    extern const DOM::QualifiedName onresetAttr;
    extern const DOM::QualifiedName onresizeAttr;
    extern const DOM::QualifiedName onscrollAttr;
    extern const DOM::QualifiedName onsearchAttr;
    extern const DOM::QualifiedName onselectAttr;
    extern const DOM::QualifiedName onselectstartAttr;
    extern const DOM::QualifiedName onsubmitAttr;
    extern const DOM::QualifiedName onunloadAttr;
    extern const DOM::QualifiedName pagexAttr;
    extern const DOM::QualifiedName pageyAttr;
    extern const DOM::QualifiedName placeholderAttr;
    extern const DOM::QualifiedName plainAttr;
    extern const DOM::QualifiedName pluginpageAttr;
    extern const DOM::QualifiedName pluginspageAttr;
    extern const DOM::QualifiedName pluginurlAttr;
    extern const DOM::QualifiedName precisionAttr;
    extern const DOM::QualifiedName profileAttr;
    extern const DOM::QualifiedName promptAttr;
    extern const DOM::QualifiedName readonlyAttr;
    extern const DOM::QualifiedName relAttr;
    extern const DOM::QualifiedName resultsAttr;
    extern const DOM::QualifiedName revAttr;
    extern const DOM::QualifiedName rowsAttr;
    extern const DOM::QualifiedName rowspanAttr;
    extern const DOM::QualifiedName rulesAttr;
    extern const DOM::QualifiedName schemeAttr;
    extern const DOM::QualifiedName scopeAttr;
    extern const DOM::QualifiedName scrollamountAttr;
    extern const DOM::QualifiedName scrolldelayAttr;
    extern const DOM::QualifiedName scrollingAttr;
    extern const DOM::QualifiedName selectedAttr;
    extern const DOM::QualifiedName shapeAttr;
    extern const DOM::QualifiedName sizeAttr;
    extern const DOM::QualifiedName spanAttr;
    extern const DOM::QualifiedName srcAttr;
    extern const DOM::QualifiedName standbyAttr;
    extern const DOM::QualifiedName startAttr;
    extern const DOM::QualifiedName styleAttr;
    extern const DOM::QualifiedName summaryAttr;
    extern const DOM::QualifiedName tabindexAttr;
    extern const DOM::QualifiedName tableborderAttr;
    extern const DOM::QualifiedName targetAttr;
    extern const DOM::QualifiedName textAttr;
    extern const DOM::QualifiedName titleAttr;
    extern const DOM::QualifiedName topAttr;
    extern const DOM::QualifiedName topmarginAttr;
    extern const DOM::QualifiedName truespeedAttr;
    extern const DOM::QualifiedName typeAttr;
    extern const DOM::QualifiedName usemapAttr;
    extern const DOM::QualifiedName valignAttr;
    extern const DOM::QualifiedName valueAttr;
    extern const DOM::QualifiedName valuetypeAttr;
    extern const DOM::QualifiedName versionAttr;
    extern const DOM::QualifiedName vlinkAttr;
    extern const DOM::QualifiedName vspaceAttr;
    extern const DOM::QualifiedName widthAttr;
    extern const DOM::QualifiedName wrapAttr;
#endif

    // Init routine
    void initHTMLNames();
}

#endif
