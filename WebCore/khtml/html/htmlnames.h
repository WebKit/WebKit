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

namespace DOM
{

class QualifiedName;

// The globals.
// Define external global variables for all HTML names above.
#if !KHTML_HTMLNAMES_HIDE_GLOBALS
    extern const AtomicString  xhtmlNamespaceURIAtom;
    extern const QualifiedName aTag;
    extern const QualifiedName abbrTag;
    extern const QualifiedName acronymTag;
    extern const QualifiedName addressTag;
    extern const QualifiedName appletTag;
    extern const QualifiedName areaTag;
    extern const QualifiedName bTag;
    extern const QualifiedName baseTag;
    extern const QualifiedName basefontTag;
    extern const QualifiedName bdoTag;
    extern const QualifiedName bigTag;
    extern const QualifiedName blockquoteTag;
    extern const QualifiedName bodyTag;
    extern const QualifiedName brTag;
    extern const QualifiedName buttonTag;
    extern const QualifiedName canvasTag;
    extern const QualifiedName captionTag;
    extern const QualifiedName centerTag;
    extern const QualifiedName citeTag;
    extern const QualifiedName codeTag;
    extern const QualifiedName colTag;
    extern const QualifiedName colgroupTag;
    extern const QualifiedName ddTag;
    extern const QualifiedName delTag;
    extern const QualifiedName dfnTag;
    extern const QualifiedName dirTag;
    extern const QualifiedName divTag;
    extern const QualifiedName dlTag;
    extern const QualifiedName dtTag;
    extern const QualifiedName emTag;
    extern const QualifiedName embedTag;
    extern const QualifiedName fieldsetTag;
    extern const QualifiedName fontTag;
    extern const QualifiedName formTag;
    extern const QualifiedName frameTag;
    extern const QualifiedName framesetTag;
    extern const QualifiedName headTag;
    extern const QualifiedName h1Tag;
    extern const QualifiedName h2Tag;
    extern const QualifiedName h3Tag;
    extern const QualifiedName h4Tag;
    extern const QualifiedName h5Tag;
    extern const QualifiedName h6Tag;
    extern const QualifiedName hrTag;
    extern const QualifiedName htmlTag;
    extern const QualifiedName iTag;
    extern const QualifiedName iframeTag;
    extern const QualifiedName imgTag;
    extern const QualifiedName inputTag;
    extern const QualifiedName insTag;
    extern const QualifiedName isindexTag;
    extern const QualifiedName kbdTag;
    extern const QualifiedName keygenTag;
    extern const QualifiedName labelTag;
    extern const QualifiedName layerTag;
    extern const QualifiedName legendTag;
    extern const QualifiedName liTag;
    extern const QualifiedName linkTag;
    extern const QualifiedName mapTag;
    extern const QualifiedName marqueeTag;
    extern const QualifiedName menuTag;
    extern const QualifiedName metaTag;
    extern const QualifiedName nobrTag;
    extern const QualifiedName noembedTag;
    extern const QualifiedName noframesTag;
    extern const QualifiedName nolayerTag;
    extern const QualifiedName noscriptTag;
    extern const QualifiedName objectTag;
    extern const QualifiedName olTag;
    extern const QualifiedName optgroupTag;
    extern const QualifiedName optionTag;
    extern const QualifiedName pTag;
    extern const QualifiedName paramTag;
    extern const QualifiedName plaintextTag;
    extern const QualifiedName preTag;
    extern const QualifiedName qTag;
    extern const QualifiedName sTag;
    extern const QualifiedName sampTag;
    extern const QualifiedName scriptTag;
    extern const QualifiedName selectTag;
    extern const QualifiedName smallTag;
    extern const QualifiedName spanTag;
    extern const QualifiedName strikeTag;
    extern const QualifiedName strongTag;
    extern const QualifiedName styleTag;
    extern const QualifiedName subTag;
    extern const QualifiedName supTag;
    extern const QualifiedName tableTag;
    extern const QualifiedName tbodyTag;
    extern const QualifiedName tdTag;
    extern const QualifiedName textareaTag;
    extern const QualifiedName tfootTag;
    extern const QualifiedName thTag;
    extern const QualifiedName theadTag;
    extern const QualifiedName titleTag;
    extern const QualifiedName trTag;
    extern const QualifiedName ttTag;
    extern const QualifiedName uTag;
    extern const QualifiedName ulTag;
    extern const QualifiedName varTag;
    extern const QualifiedName wbrTag;
    extern const QualifiedName xmpTag;

    // Attributes
    extern const QualifiedName abbrAttr;
    extern const QualifiedName accept_charsetAttr;
    extern const QualifiedName acceptAttr;
    extern const QualifiedName accesskeyAttr;
    extern const QualifiedName actionAttr;
    extern const QualifiedName alignAttr;
    extern const QualifiedName alinkAttr;
    extern const QualifiedName altAttr;
    extern const QualifiedName archiveAttr;
    extern const QualifiedName autocompleteAttr;
    extern const QualifiedName autosaveAttr;
    extern const QualifiedName axisAttr;
    extern const QualifiedName backgroundAttr;
    extern const QualifiedName behaviorAttr;
    extern const QualifiedName bgcolorAttr;
    extern const QualifiedName bgpropertiesAttr;
    extern const QualifiedName borderAttr;
    extern const QualifiedName bordercolorAttr;
    extern const QualifiedName cellpaddingAttr;
    extern const QualifiedName cellspacingAttr;
    extern const QualifiedName charAttrAttr;
    extern const QualifiedName challengeAttr;
    extern const QualifiedName charoffAttr;
    extern const QualifiedName charsetAttr;
    extern const QualifiedName checkedAttr;
    extern const QualifiedName cellborderAttr;
    extern const QualifiedName citeAttr;
    extern const QualifiedName classAttrAttr;
    extern const QualifiedName classidAttr;
    extern const QualifiedName clearAttr;
    extern const QualifiedName codeAttr;
    extern const QualifiedName codebaseAttr;
    extern const QualifiedName codetypeAttr;
    extern const QualifiedName colorAttr;
    extern const QualifiedName colsAttr;
    extern const QualifiedName colspanAttr;
    extern const QualifiedName compactAttr;
    extern const QualifiedName compositeAttr;
    extern const QualifiedName contentAttr;
    extern const QualifiedName contenteditableAttr;
    extern const QualifiedName coordsAttr;
    extern const QualifiedName dataAttr;
    extern const QualifiedName datetimeAttr;
    extern const QualifiedName declareAttr;
    extern const QualifiedName deferAttr;
    extern const QualifiedName dirAttr;
    extern const QualifiedName directionAttr;
    extern const QualifiedName disabledAttr;
    extern const QualifiedName enctypeAttr;
    extern const QualifiedName faceAttr;
    extern const QualifiedName forAttrAttr;
    extern const QualifiedName frameAttr;
    extern const QualifiedName frameborderAttr;
    extern const QualifiedName headersAttr;
    extern const QualifiedName heightAttr;
    extern const QualifiedName hiddenAttr;
    extern const QualifiedName hrefAttr;
    extern const QualifiedName hreflangAttr;
    extern const QualifiedName hspaceAttr;
    extern const QualifiedName http_equivAttr;
    extern const QualifiedName idAttrAttr;
    extern const QualifiedName incrementalAttr;
    extern const QualifiedName ismapAttr;
    extern const QualifiedName keytypeAttr;
    extern const QualifiedName labelAttr;
    extern const QualifiedName langAttr;
    extern const QualifiedName languageAttr;
    extern const QualifiedName leftAttr;
    extern const QualifiedName leftmarginAttr;
    extern const QualifiedName linkAttr;
    extern const QualifiedName longdescAttr;
    extern const QualifiedName loopAttr;
    extern const QualifiedName marginheightAttr;
    extern const QualifiedName marginwidthAttr;
    extern const QualifiedName maxAttr;
    extern const QualifiedName maxlengthAttr;
    extern const QualifiedName mayscriptAttr;
    extern const QualifiedName mediaAttr;
    extern const QualifiedName methodAttr;
    extern const QualifiedName minAttr;
    extern const QualifiedName multipleAttr;
    extern const QualifiedName nameAttr;
    extern const QualifiedName nohrefAttr;
    extern const QualifiedName noresizeAttr;
    extern const QualifiedName noshadeAttr;
    extern const QualifiedName nowrapAttr;
    extern const QualifiedName objectAttr;
    extern const QualifiedName onabortAttr;
    extern const QualifiedName onbeforecopyAttr;
    extern const QualifiedName onbeforecutAttr;
    extern const QualifiedName onbeforepasteAttr;
    extern const QualifiedName onblurAttr;
    extern const QualifiedName onchangeAttr;
    extern const QualifiedName onclickAttr;
    extern const QualifiedName oncontextmenuAttr;
    extern const QualifiedName oncopyAttr;
    extern const QualifiedName oncutAttr;
    extern const QualifiedName ondblclickAttr;
    extern const QualifiedName ondragAttr;
    extern const QualifiedName ondragendAttr;
    extern const QualifiedName ondragenterAttr;
    extern const QualifiedName ondragleaveAttr;
    extern const QualifiedName ondragoverAttr;
    extern const QualifiedName ondragstartAttr;
    extern const QualifiedName ondropAttr;
    extern const QualifiedName onerrorAttr;
    extern const QualifiedName onfocusAttr;
    extern const QualifiedName oninputAttr;
    extern const QualifiedName onkeydownAttr;
    extern const QualifiedName onkeypressAttr;
    extern const QualifiedName onkeyupAttr;
    extern const QualifiedName onloadAttr;
    extern const QualifiedName onmousedownAttr;
    extern const QualifiedName onmousemoveAttr;
    extern const QualifiedName onmouseoutAttr;
    extern const QualifiedName onmouseoverAttr;
    extern const QualifiedName onmouseupAttr;
    extern const QualifiedName onmousewheelAttr;
    extern const QualifiedName onpasteAttr;
    extern const QualifiedName onresetAttr;
    extern const QualifiedName onresizeAttr;
    extern const QualifiedName onscrollAttr;
    extern const QualifiedName onsearchAttr;
    extern const QualifiedName onselectAttr;
    extern const QualifiedName onselectstartAttr;
    extern const QualifiedName onsubmitAttr;
    extern const QualifiedName onunloadAttr;
    extern const QualifiedName pagexAttr;
    extern const QualifiedName pageyAttr;
    extern const QualifiedName placeholderAttr;
    extern const QualifiedName plainAttr;
    extern const QualifiedName pluginpageAttr;
    extern const QualifiedName pluginspageAttr;
    extern const QualifiedName pluginurlAttr;
    extern const QualifiedName precisionAttr;
    extern const QualifiedName profileAttr;
    extern const QualifiedName promptAttr;
    extern const QualifiedName readonlyAttr;
    extern const QualifiedName relAttr;
    extern const QualifiedName resultsAttr;
    extern const QualifiedName revAttr;
    extern const QualifiedName rowsAttr;
    extern const QualifiedName rowspanAttr;
    extern const QualifiedName rulesAttr;
    extern const QualifiedName schemeAttr;
    extern const QualifiedName scopeAttr;
    extern const QualifiedName scrollamountAttr;
    extern const QualifiedName scrolldelayAttr;
    extern const QualifiedName scrollingAttr;
    extern const QualifiedName selectedAttr;
    extern const QualifiedName shapeAttr;
    extern const QualifiedName sizeAttr;
    extern const QualifiedName spanAttr;
    extern const QualifiedName srcAttr;
    extern const QualifiedName standbyAttr;
    extern const QualifiedName startAttr;
    extern const QualifiedName styleAttr;
    extern const QualifiedName summaryAttr;
    extern const QualifiedName tabindexAttr;
    extern const QualifiedName tableborderAttr;
    extern const QualifiedName targetAttr;
    extern const QualifiedName textAttr;
    extern const QualifiedName titleAttr;
    extern const QualifiedName topAttr;
    extern const QualifiedName topmarginAttr;
    extern const QualifiedName truespeedAttr;
    extern const QualifiedName typeAttr;
    extern const QualifiedName usemapAttr;
    extern const QualifiedName valignAttr;
    extern const QualifiedName valueAttr;
    extern const QualifiedName valuetypeAttr;
    extern const QualifiedName versionAttr;
    extern const QualifiedName vlinkAttr;
    extern const QualifiedName vspaceAttr;
    extern const QualifiedName widthAttr;
    extern const QualifiedName wrapAttr;

#endif

// FIXME: Make this a namespace instead of a class.
// FIXME: Rename this to HTMLTags
// FIXME: Just make the xhtmlNamespaceURI() a global outside of any class
class HTMLTags
{
public:
#if !KHTML_HTMLNAMES_HIDE_GLOBALS
    // The namespace URI.
    static const AtomicString& xhtmlNamespaceURI() { return xhtmlNamespaceURIAtom; }
    
// Full tag names.
#define DEFINE_TAG_GETTER(name) \
    static const QualifiedName& name() { return name ## Tag; }

    DEFINE_TAG_GETTER(a)
    DEFINE_TAG_GETTER(abbr)
    DEFINE_TAG_GETTER(acronym)
    DEFINE_TAG_GETTER(address)
    DEFINE_TAG_GETTER(applet)
    DEFINE_TAG_GETTER(area)
    DEFINE_TAG_GETTER(b)
    DEFINE_TAG_GETTER(base)
    DEFINE_TAG_GETTER(basefont)
    DEFINE_TAG_GETTER(bdo)
    DEFINE_TAG_GETTER(big)
    DEFINE_TAG_GETTER(blockquote)
    DEFINE_TAG_GETTER(body)
    DEFINE_TAG_GETTER(br)
    DEFINE_TAG_GETTER(button)
    DEFINE_TAG_GETTER(canvas)
    DEFINE_TAG_GETTER(caption)
    DEFINE_TAG_GETTER(center)
    DEFINE_TAG_GETTER(cite)
    DEFINE_TAG_GETTER(code)
    DEFINE_TAG_GETTER(col)
    DEFINE_TAG_GETTER(colgroup)
    DEFINE_TAG_GETTER(dd)
    DEFINE_TAG_GETTER(del)
    DEFINE_TAG_GETTER(dfn)
    DEFINE_TAG_GETTER(dir)
    DEFINE_TAG_GETTER(div)
    DEFINE_TAG_GETTER(dl)
    DEFINE_TAG_GETTER(dt)
    DEFINE_TAG_GETTER(em)
    DEFINE_TAG_GETTER(embed)
    DEFINE_TAG_GETTER(fieldset)
    DEFINE_TAG_GETTER(font)
    DEFINE_TAG_GETTER(form)
    DEFINE_TAG_GETTER(frame)
    DEFINE_TAG_GETTER(frameset)
    DEFINE_TAG_GETTER(head)
    DEFINE_TAG_GETTER(h1)
    DEFINE_TAG_GETTER(h2)
    DEFINE_TAG_GETTER(h3)
    DEFINE_TAG_GETTER(h4)
    DEFINE_TAG_GETTER(h5)
    DEFINE_TAG_GETTER(h6)
    DEFINE_TAG_GETTER(hr)
    DEFINE_TAG_GETTER(html)
    DEFINE_TAG_GETTER(i)
    DEFINE_TAG_GETTER(iframe)
    DEFINE_TAG_GETTER(img)
    DEFINE_TAG_GETTER(input)
    DEFINE_TAG_GETTER(ins)
    DEFINE_TAG_GETTER(isindex)
    DEFINE_TAG_GETTER(kbd)
    DEFINE_TAG_GETTER(keygen)
    DEFINE_TAG_GETTER(label)
    DEFINE_TAG_GETTER(layer)
    DEFINE_TAG_GETTER(legend)
    DEFINE_TAG_GETTER(li)
    DEFINE_TAG_GETTER(link)
    DEFINE_TAG_GETTER(map)
    DEFINE_TAG_GETTER(marquee)
    DEFINE_TAG_GETTER(menu)
    DEFINE_TAG_GETTER(meta)
    DEFINE_TAG_GETTER(nobr)
    DEFINE_TAG_GETTER(noembed)
    DEFINE_TAG_GETTER(noframes)
    DEFINE_TAG_GETTER(nolayer)
    DEFINE_TAG_GETTER(noscript)
    DEFINE_TAG_GETTER(object)
    DEFINE_TAG_GETTER(ol)
    DEFINE_TAG_GETTER(optgroup)
    DEFINE_TAG_GETTER(option)
    DEFINE_TAG_GETTER(p)
    DEFINE_TAG_GETTER(param)
    DEFINE_TAG_GETTER(plaintext)
    DEFINE_TAG_GETTER(pre)
    DEFINE_TAG_GETTER(q)
    DEFINE_TAG_GETTER(s)
    DEFINE_TAG_GETTER(samp)
    DEFINE_TAG_GETTER(script)
    DEFINE_TAG_GETTER(select)
    DEFINE_TAG_GETTER(small)
    DEFINE_TAG_GETTER(span)
    DEFINE_TAG_GETTER(strike)
    DEFINE_TAG_GETTER(strong)
    DEFINE_TAG_GETTER(style)
    DEFINE_TAG_GETTER(sub)
    DEFINE_TAG_GETTER(sup)
    DEFINE_TAG_GETTER(table)
    DEFINE_TAG_GETTER(tbody)
    DEFINE_TAG_GETTER(td)
    DEFINE_TAG_GETTER(textarea)
    DEFINE_TAG_GETTER(tfoot)
    DEFINE_TAG_GETTER(th)
    DEFINE_TAG_GETTER(thead)
    DEFINE_TAG_GETTER(title)
    DEFINE_TAG_GETTER(tr)
    DEFINE_TAG_GETTER(tt)
    DEFINE_TAG_GETTER(u)
    DEFINE_TAG_GETTER(ul)
    DEFINE_TAG_GETTER(var)
    DEFINE_TAG_GETTER(wbr)
    DEFINE_TAG_GETTER(xmp)
#endif

    // Init routine
    static void init();
};

// FIXME: Make this a namespace instead of a class.
class HTMLAttributes
{
public:
#if !KHTML_HTMLNAMES_HIDE_GLOBALS

// Full tag names.
#define DEFINE_ATTR_GETTER(name) \
    static const QualifiedName& name() { return name ## Attr; }

    // Attribute names.
    DEFINE_ATTR_GETTER(abbr)
    DEFINE_ATTR_GETTER(accept_charset)
    DEFINE_ATTR_GETTER(accept)
    DEFINE_ATTR_GETTER(accesskey)
    DEFINE_ATTR_GETTER(action)
    DEFINE_ATTR_GETTER(align)
    DEFINE_ATTR_GETTER(alink)
    DEFINE_ATTR_GETTER(alt)
    DEFINE_ATTR_GETTER(archive)
    DEFINE_ATTR_GETTER(autocomplete)
    DEFINE_ATTR_GETTER(autosave)
    DEFINE_ATTR_GETTER(axis)
    DEFINE_ATTR_GETTER(background)
    DEFINE_ATTR_GETTER(behavior)
    DEFINE_ATTR_GETTER(bgcolor)
    DEFINE_ATTR_GETTER(bgproperties)
    DEFINE_ATTR_GETTER(border)
    DEFINE_ATTR_GETTER(bordercolor)
    DEFINE_ATTR_GETTER(cellpadding)
    DEFINE_ATTR_GETTER(cellspacing)
    DEFINE_ATTR_GETTER(charAttr)
    DEFINE_ATTR_GETTER(challenge)
    DEFINE_ATTR_GETTER(charoff)
    DEFINE_ATTR_GETTER(charset)
    DEFINE_ATTR_GETTER(checked)
    DEFINE_ATTR_GETTER(cellborder)
    DEFINE_ATTR_GETTER(cite)
    DEFINE_ATTR_GETTER(classAttr)
    DEFINE_ATTR_GETTER(classid)
    DEFINE_ATTR_GETTER(clear)
    DEFINE_ATTR_GETTER(code)
    DEFINE_ATTR_GETTER(codebase)
    DEFINE_ATTR_GETTER(codetype)
    DEFINE_ATTR_GETTER(color)
    DEFINE_ATTR_GETTER(cols)
    DEFINE_ATTR_GETTER(colspan)
    DEFINE_ATTR_GETTER(compact)
    DEFINE_ATTR_GETTER(composite)
    DEFINE_ATTR_GETTER(content)
    DEFINE_ATTR_GETTER(contenteditable)
    DEFINE_ATTR_GETTER(coords)
    DEFINE_ATTR_GETTER(data)
    DEFINE_ATTR_GETTER(datetime)
    DEFINE_ATTR_GETTER(declare)
    DEFINE_ATTR_GETTER(defer)
    DEFINE_ATTR_GETTER(dir)
    DEFINE_ATTR_GETTER(direction)
    DEFINE_ATTR_GETTER(disabled)
    DEFINE_ATTR_GETTER(enctype)
    DEFINE_ATTR_GETTER(face)
    DEFINE_ATTR_GETTER(forAttr)
    DEFINE_ATTR_GETTER(frame)
    DEFINE_ATTR_GETTER(frameborder)
    DEFINE_ATTR_GETTER(headers)
    DEFINE_ATTR_GETTER(height)
    DEFINE_ATTR_GETTER(hidden)
    DEFINE_ATTR_GETTER(href)
    DEFINE_ATTR_GETTER(hreflang)
    DEFINE_ATTR_GETTER(hspace)
    DEFINE_ATTR_GETTER(http_equiv)
    DEFINE_ATTR_GETTER(idAttr)
    DEFINE_ATTR_GETTER(incremental)
    DEFINE_ATTR_GETTER(ismap)
    DEFINE_ATTR_GETTER(keytype)
    DEFINE_ATTR_GETTER(label)
    DEFINE_ATTR_GETTER(lang)
    DEFINE_ATTR_GETTER(language)
    DEFINE_ATTR_GETTER(left)
    DEFINE_ATTR_GETTER(leftmargin)
    DEFINE_ATTR_GETTER(link)
    DEFINE_ATTR_GETTER(longdesc)
    DEFINE_ATTR_GETTER(loop)
    DEFINE_ATTR_GETTER(marginheight)
    DEFINE_ATTR_GETTER(marginwidth)
    DEFINE_ATTR_GETTER(max)
    DEFINE_ATTR_GETTER(maxlength)
    DEFINE_ATTR_GETTER(mayscript)
    DEFINE_ATTR_GETTER(media)
    DEFINE_ATTR_GETTER(method)
    DEFINE_ATTR_GETTER(min)
    DEFINE_ATTR_GETTER(multiple)
    DEFINE_ATTR_GETTER(name)
    DEFINE_ATTR_GETTER(nohref)
    DEFINE_ATTR_GETTER(noresize)
    DEFINE_ATTR_GETTER(noshade)
    DEFINE_ATTR_GETTER(nowrap)
    DEFINE_ATTR_GETTER(object)
    DEFINE_ATTR_GETTER(onabort)
    DEFINE_ATTR_GETTER(onbeforecopy)
    DEFINE_ATTR_GETTER(onbeforecut)
    DEFINE_ATTR_GETTER(onbeforepaste)
    DEFINE_ATTR_GETTER(onblur)
    DEFINE_ATTR_GETTER(onchange)
    DEFINE_ATTR_GETTER(onclick)
    DEFINE_ATTR_GETTER(oncontextmenu)
    DEFINE_ATTR_GETTER(oncopy)
    DEFINE_ATTR_GETTER(oncut)
    DEFINE_ATTR_GETTER(ondblclick)
    DEFINE_ATTR_GETTER(ondrag)
    DEFINE_ATTR_GETTER(ondragend)
    DEFINE_ATTR_GETTER(ondragenter)
    DEFINE_ATTR_GETTER(ondragleave)
    DEFINE_ATTR_GETTER(ondragover)
    DEFINE_ATTR_GETTER(ondragstart)
    DEFINE_ATTR_GETTER(ondrop)
    DEFINE_ATTR_GETTER(onerror)
    DEFINE_ATTR_GETTER(onfocus)
    DEFINE_ATTR_GETTER(oninput)
    DEFINE_ATTR_GETTER(onkeydown)
    DEFINE_ATTR_GETTER(onkeypress)
    DEFINE_ATTR_GETTER(onkeyup)
    DEFINE_ATTR_GETTER(onload)
    DEFINE_ATTR_GETTER(onmousedown)
    DEFINE_ATTR_GETTER(onmousemove)
    DEFINE_ATTR_GETTER(onmouseout)
    DEFINE_ATTR_GETTER(onmouseover)
    DEFINE_ATTR_GETTER(onmouseup)
    DEFINE_ATTR_GETTER(onmousewheel)
    DEFINE_ATTR_GETTER(onpaste)
    DEFINE_ATTR_GETTER(onreset)
    DEFINE_ATTR_GETTER(onresize)
    DEFINE_ATTR_GETTER(onscroll)
    DEFINE_ATTR_GETTER(onsearch)
    DEFINE_ATTR_GETTER(onselect)
    DEFINE_ATTR_GETTER(onselectstart)
    DEFINE_ATTR_GETTER(onsubmit)
    DEFINE_ATTR_GETTER(onunload)
    DEFINE_ATTR_GETTER(pagex)
    DEFINE_ATTR_GETTER(pagey)
    DEFINE_ATTR_GETTER(placeholder)
    DEFINE_ATTR_GETTER(plain)
    DEFINE_ATTR_GETTER(pluginpage)
    DEFINE_ATTR_GETTER(pluginspage)
    DEFINE_ATTR_GETTER(pluginurl)
    DEFINE_ATTR_GETTER(precision)
    DEFINE_ATTR_GETTER(profile)
    DEFINE_ATTR_GETTER(prompt)
    DEFINE_ATTR_GETTER(readonly)
    DEFINE_ATTR_GETTER(rel)
    DEFINE_ATTR_GETTER(results)
    DEFINE_ATTR_GETTER(rev)
    DEFINE_ATTR_GETTER(rows)
    DEFINE_ATTR_GETTER(rowspan)
    DEFINE_ATTR_GETTER(rules)
    DEFINE_ATTR_GETTER(scheme)
    DEFINE_ATTR_GETTER(scope)
    DEFINE_ATTR_GETTER(scrollamount)
    DEFINE_ATTR_GETTER(scrolldelay)
    DEFINE_ATTR_GETTER(scrolling)
    DEFINE_ATTR_GETTER(selected)
    DEFINE_ATTR_GETTER(shape)
    DEFINE_ATTR_GETTER(size)
    DEFINE_ATTR_GETTER(span)
    DEFINE_ATTR_GETTER(src)
    DEFINE_ATTR_GETTER(standby)
    DEFINE_ATTR_GETTER(start)
    DEFINE_ATTR_GETTER(style)
    DEFINE_ATTR_GETTER(summary)
    DEFINE_ATTR_GETTER(tabindex)
    DEFINE_ATTR_GETTER(tableborder)
    DEFINE_ATTR_GETTER(target)
    DEFINE_ATTR_GETTER(text)
    DEFINE_ATTR_GETTER(title)
    DEFINE_ATTR_GETTER(top)
    DEFINE_ATTR_GETTER(topmargin)
    DEFINE_ATTR_GETTER(truespeed)
    DEFINE_ATTR_GETTER(type)
    DEFINE_ATTR_GETTER(usemap)
    DEFINE_ATTR_GETTER(valign)
    DEFINE_ATTR_GETTER(value)
    DEFINE_ATTR_GETTER(valuetype)
    DEFINE_ATTR_GETTER(version)
    DEFINE_ATTR_GETTER(vlink)
    DEFINE_ATTR_GETTER(vspace)
    DEFINE_ATTR_GETTER(width)
    DEFINE_ATTR_GETTER(wrap)

#endif

    // Init routine
    static void init();
};

}

#endif
