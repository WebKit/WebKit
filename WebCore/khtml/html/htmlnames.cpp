/*
 * This file is part of the HTML DOM implementation for KDE.
 *
 * Copyright (C) QualifiedName(nullAtom, " 2005 Apple Computer, Inc.
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

#define KHTML_HTMLNAMES_HIDE_GLOBALS 1

#include <qptrdict.h>
#include "htmlnames.h"
#include "dom_qname.h"

namespace DOM {

void* xhtmlNamespaceURIAtom[(sizeof(AtomicString) + sizeof(void*) - 1) / sizeof(void*)];

// Define a QualifiedName-sized array of pointers to avoid static initialization.
// Use an array of pointers instead of an array of char in case there is some alignment issue.
#define DEFINE_TAG_GLOBAL(name) \
    void* name ## Tag[(sizeof(QualifiedName) + sizeof(void*) - 1) / sizeof(void*)];

DEFINE_TAG_GLOBAL(a)
DEFINE_TAG_GLOBAL(abbr)
DEFINE_TAG_GLOBAL(acronym)
DEFINE_TAG_GLOBAL(address)
DEFINE_TAG_GLOBAL(applet)
DEFINE_TAG_GLOBAL(area)
DEFINE_TAG_GLOBAL(b)
DEFINE_TAG_GLOBAL(base)
DEFINE_TAG_GLOBAL(basefont)
DEFINE_TAG_GLOBAL(bdo)
DEFINE_TAG_GLOBAL(big)
DEFINE_TAG_GLOBAL(blockquote)
DEFINE_TAG_GLOBAL(body)
DEFINE_TAG_GLOBAL(br)
DEFINE_TAG_GLOBAL(button)
DEFINE_TAG_GLOBAL(canvas)
DEFINE_TAG_GLOBAL(caption)
DEFINE_TAG_GLOBAL(center)
DEFINE_TAG_GLOBAL(cite)
DEFINE_TAG_GLOBAL(code)
DEFINE_TAG_GLOBAL(col)
DEFINE_TAG_GLOBAL(colgroup)
DEFINE_TAG_GLOBAL(dd)
DEFINE_TAG_GLOBAL(del)
DEFINE_TAG_GLOBAL(dfn)
DEFINE_TAG_GLOBAL(dir)
DEFINE_TAG_GLOBAL(div)
DEFINE_TAG_GLOBAL(dl)
DEFINE_TAG_GLOBAL(dt)
DEFINE_TAG_GLOBAL(em)
DEFINE_TAG_GLOBAL(embed)
DEFINE_TAG_GLOBAL(fieldset)
DEFINE_TAG_GLOBAL(font)
DEFINE_TAG_GLOBAL(form)
DEFINE_TAG_GLOBAL(frame)
DEFINE_TAG_GLOBAL(frameset)
DEFINE_TAG_GLOBAL(head)
DEFINE_TAG_GLOBAL(h1)
DEFINE_TAG_GLOBAL(h2)
DEFINE_TAG_GLOBAL(h3)
DEFINE_TAG_GLOBAL(h4)
DEFINE_TAG_GLOBAL(h5)
DEFINE_TAG_GLOBAL(h6)
DEFINE_TAG_GLOBAL(hr)
DEFINE_TAG_GLOBAL(html)
DEFINE_TAG_GLOBAL(i)
DEFINE_TAG_GLOBAL(iframe)
DEFINE_TAG_GLOBAL(img)
DEFINE_TAG_GLOBAL(input)
DEFINE_TAG_GLOBAL(ins)
DEFINE_TAG_GLOBAL(isindex)
DEFINE_TAG_GLOBAL(kbd)
DEFINE_TAG_GLOBAL(keygen)
DEFINE_TAG_GLOBAL(label)
DEFINE_TAG_GLOBAL(layer)
DEFINE_TAG_GLOBAL(legend)
DEFINE_TAG_GLOBAL(li)
DEFINE_TAG_GLOBAL(link)
DEFINE_TAG_GLOBAL(map)
DEFINE_TAG_GLOBAL(marquee)
DEFINE_TAG_GLOBAL(menu)
DEFINE_TAG_GLOBAL(meta)
DEFINE_TAG_GLOBAL(nobr)
DEFINE_TAG_GLOBAL(noembed)
DEFINE_TAG_GLOBAL(noframes)
DEFINE_TAG_GLOBAL(nolayer)
DEFINE_TAG_GLOBAL(noscript)
DEFINE_TAG_GLOBAL(object)
DEFINE_TAG_GLOBAL(ol)
DEFINE_TAG_GLOBAL(optgroup)
DEFINE_TAG_GLOBAL(option)
DEFINE_TAG_GLOBAL(p)
DEFINE_TAG_GLOBAL(param)
DEFINE_TAG_GLOBAL(plaintext)
DEFINE_TAG_GLOBAL(pre)
DEFINE_TAG_GLOBAL(q)
DEFINE_TAG_GLOBAL(s)
DEFINE_TAG_GLOBAL(samp)
DEFINE_TAG_GLOBAL(script)
DEFINE_TAG_GLOBAL(select)
DEFINE_TAG_GLOBAL(small)
DEFINE_TAG_GLOBAL(span)
DEFINE_TAG_GLOBAL(strike)
DEFINE_TAG_GLOBAL(strong)
DEFINE_TAG_GLOBAL(style)
DEFINE_TAG_GLOBAL(sub)
DEFINE_TAG_GLOBAL(sup)
DEFINE_TAG_GLOBAL(table)
DEFINE_TAG_GLOBAL(tbody)
DEFINE_TAG_GLOBAL(td)
DEFINE_TAG_GLOBAL(textarea)
DEFINE_TAG_GLOBAL(tfoot)
DEFINE_TAG_GLOBAL(th)
DEFINE_TAG_GLOBAL(thead)
DEFINE_TAG_GLOBAL(title)
DEFINE_TAG_GLOBAL(tr)
DEFINE_TAG_GLOBAL(tt)
DEFINE_TAG_GLOBAL(u)
DEFINE_TAG_GLOBAL(ul)
DEFINE_TAG_GLOBAL(var)
DEFINE_TAG_GLOBAL(wbr)
DEFINE_TAG_GLOBAL(xmp)

// Attribute names.
#define DEFINE_ATTR_GLOBAL(name) \
    void* name ## Attr[(sizeof(QualifiedName) + sizeof(void*) - 1) / sizeof(void*)];

DEFINE_ATTR_GLOBAL(abbr)
DEFINE_ATTR_GLOBAL(accept_charset)
DEFINE_ATTR_GLOBAL(accept)
DEFINE_ATTR_GLOBAL(accesskey)
DEFINE_ATTR_GLOBAL(action)
DEFINE_ATTR_GLOBAL(align)
DEFINE_ATTR_GLOBAL(alink)
DEFINE_ATTR_GLOBAL(alt)
DEFINE_ATTR_GLOBAL(archive)
DEFINE_ATTR_GLOBAL(autocomplete)
DEFINE_ATTR_GLOBAL(autosave)
DEFINE_ATTR_GLOBAL(axis)
DEFINE_ATTR_GLOBAL(background)
DEFINE_ATTR_GLOBAL(behavior)
DEFINE_ATTR_GLOBAL(bgcolor)
DEFINE_ATTR_GLOBAL(bgproperties)
DEFINE_ATTR_GLOBAL(border)
DEFINE_ATTR_GLOBAL(bordercolor)
DEFINE_ATTR_GLOBAL(cellpadding)
DEFINE_ATTR_GLOBAL(cellspacing)
DEFINE_ATTR_GLOBAL(charAttr)
DEFINE_ATTR_GLOBAL(challenge)
DEFINE_ATTR_GLOBAL(charoff)
DEFINE_ATTR_GLOBAL(charset)
DEFINE_ATTR_GLOBAL(checked)
DEFINE_ATTR_GLOBAL(cellborder)
DEFINE_ATTR_GLOBAL(cite)
DEFINE_ATTR_GLOBAL(classAttr)
DEFINE_ATTR_GLOBAL(classid)
DEFINE_ATTR_GLOBAL(clear)
DEFINE_ATTR_GLOBAL(code)
DEFINE_ATTR_GLOBAL(codebase)
DEFINE_ATTR_GLOBAL(codetype)
DEFINE_ATTR_GLOBAL(color)
DEFINE_ATTR_GLOBAL(cols)
DEFINE_ATTR_GLOBAL(colspan)
DEFINE_ATTR_GLOBAL(compact)
DEFINE_ATTR_GLOBAL(composite)
DEFINE_ATTR_GLOBAL(content)
DEFINE_ATTR_GLOBAL(contenteditable)
DEFINE_ATTR_GLOBAL(coords)
DEFINE_ATTR_GLOBAL(data)
DEFINE_ATTR_GLOBAL(datetime)
DEFINE_ATTR_GLOBAL(declare)
DEFINE_ATTR_GLOBAL(defer)
DEFINE_ATTR_GLOBAL(dir)
DEFINE_ATTR_GLOBAL(direction)
DEFINE_ATTR_GLOBAL(disabled)
DEFINE_ATTR_GLOBAL(enctype)
DEFINE_ATTR_GLOBAL(face)
DEFINE_ATTR_GLOBAL(forAttr)
DEFINE_ATTR_GLOBAL(frame)
DEFINE_ATTR_GLOBAL(frameborder)
DEFINE_ATTR_GLOBAL(headers)
DEFINE_ATTR_GLOBAL(height)
DEFINE_ATTR_GLOBAL(hidden)
DEFINE_ATTR_GLOBAL(href)
DEFINE_ATTR_GLOBAL(hreflang)
DEFINE_ATTR_GLOBAL(hspace)
DEFINE_ATTR_GLOBAL(http_equiv)
DEFINE_ATTR_GLOBAL(idAttr)
DEFINE_ATTR_GLOBAL(incremental)
DEFINE_ATTR_GLOBAL(ismap)
DEFINE_ATTR_GLOBAL(keytype)
DEFINE_ATTR_GLOBAL(label)
DEFINE_ATTR_GLOBAL(lang)
DEFINE_ATTR_GLOBAL(language)
DEFINE_ATTR_GLOBAL(left)
DEFINE_ATTR_GLOBAL(leftmargin)
DEFINE_ATTR_GLOBAL(link)
DEFINE_ATTR_GLOBAL(longdesc)
DEFINE_ATTR_GLOBAL(loop)
DEFINE_ATTR_GLOBAL(marginheight)
DEFINE_ATTR_GLOBAL(marginwidth)
DEFINE_ATTR_GLOBAL(max)
DEFINE_ATTR_GLOBAL(maxlength)
DEFINE_ATTR_GLOBAL(mayscript)
DEFINE_ATTR_GLOBAL(media)
DEFINE_ATTR_GLOBAL(method)
DEFINE_ATTR_GLOBAL(min)
DEFINE_ATTR_GLOBAL(multiple)
DEFINE_ATTR_GLOBAL(name)
DEFINE_ATTR_GLOBAL(nohref)
DEFINE_ATTR_GLOBAL(noresize)
DEFINE_ATTR_GLOBAL(noshade)
DEFINE_ATTR_GLOBAL(nowrap)
DEFINE_ATTR_GLOBAL(object)
DEFINE_ATTR_GLOBAL(onabort)
DEFINE_ATTR_GLOBAL(onbeforecopy)
DEFINE_ATTR_GLOBAL(onbeforecut)
DEFINE_ATTR_GLOBAL(onbeforepaste)
DEFINE_ATTR_GLOBAL(onblur)
DEFINE_ATTR_GLOBAL(onchange)
DEFINE_ATTR_GLOBAL(onclick)
DEFINE_ATTR_GLOBAL(oncontextmenu)
DEFINE_ATTR_GLOBAL(oncopy)
DEFINE_ATTR_GLOBAL(oncut)
DEFINE_ATTR_GLOBAL(ondblclick)
DEFINE_ATTR_GLOBAL(ondrag)
DEFINE_ATTR_GLOBAL(ondragend)
DEFINE_ATTR_GLOBAL(ondragenter)
DEFINE_ATTR_GLOBAL(ondragleave)
DEFINE_ATTR_GLOBAL(ondragover)
DEFINE_ATTR_GLOBAL(ondragstart)
DEFINE_ATTR_GLOBAL(ondrop)
DEFINE_ATTR_GLOBAL(onerror)
DEFINE_ATTR_GLOBAL(onfocus)
DEFINE_ATTR_GLOBAL(oninput)
DEFINE_ATTR_GLOBAL(onkeydown)
DEFINE_ATTR_GLOBAL(onkeypress)
DEFINE_ATTR_GLOBAL(onkeyup)
DEFINE_ATTR_GLOBAL(onload)
DEFINE_ATTR_GLOBAL(onmousedown)
DEFINE_ATTR_GLOBAL(onmousemove)
DEFINE_ATTR_GLOBAL(onmouseout)
DEFINE_ATTR_GLOBAL(onmouseover)
DEFINE_ATTR_GLOBAL(onmouseup)
DEFINE_ATTR_GLOBAL(onmousewheel)
DEFINE_ATTR_GLOBAL(onpaste)
DEFINE_ATTR_GLOBAL(onreset)
DEFINE_ATTR_GLOBAL(onresize)
DEFINE_ATTR_GLOBAL(onscroll)
DEFINE_ATTR_GLOBAL(onsearch)
DEFINE_ATTR_GLOBAL(onselect)
DEFINE_ATTR_GLOBAL(onselectstart)
DEFINE_ATTR_GLOBAL(onsubmit)
DEFINE_ATTR_GLOBAL(onunload)
DEFINE_ATTR_GLOBAL(pagex)
DEFINE_ATTR_GLOBAL(pagey)
DEFINE_ATTR_GLOBAL(placeholder)
DEFINE_ATTR_GLOBAL(plain)
DEFINE_ATTR_GLOBAL(pluginpage)
DEFINE_ATTR_GLOBAL(pluginspage)
DEFINE_ATTR_GLOBAL(pluginurl)
DEFINE_ATTR_GLOBAL(precision)
DEFINE_ATTR_GLOBAL(profile)
DEFINE_ATTR_GLOBAL(prompt)
DEFINE_ATTR_GLOBAL(readonly)
DEFINE_ATTR_GLOBAL(rel)
DEFINE_ATTR_GLOBAL(results)
DEFINE_ATTR_GLOBAL(rev)
DEFINE_ATTR_GLOBAL(rows)
DEFINE_ATTR_GLOBAL(rowspan)
DEFINE_ATTR_GLOBAL(rules)
DEFINE_ATTR_GLOBAL(scheme)
DEFINE_ATTR_GLOBAL(scope)
DEFINE_ATTR_GLOBAL(scrollamount)
DEFINE_ATTR_GLOBAL(scrolldelay)
DEFINE_ATTR_GLOBAL(scrolling)
DEFINE_ATTR_GLOBAL(selected)
DEFINE_ATTR_GLOBAL(shape)
DEFINE_ATTR_GLOBAL(size)
DEFINE_ATTR_GLOBAL(span)
DEFINE_ATTR_GLOBAL(src)
DEFINE_ATTR_GLOBAL(standby)
DEFINE_ATTR_GLOBAL(start)
DEFINE_ATTR_GLOBAL(style)
DEFINE_ATTR_GLOBAL(summary)
DEFINE_ATTR_GLOBAL(tabindex)
DEFINE_ATTR_GLOBAL(tableborder)
DEFINE_ATTR_GLOBAL(target)
DEFINE_ATTR_GLOBAL(text)
DEFINE_ATTR_GLOBAL(title)
DEFINE_ATTR_GLOBAL(top)
DEFINE_ATTR_GLOBAL(topmargin)
DEFINE_ATTR_GLOBAL(truespeed)
DEFINE_ATTR_GLOBAL(type)
DEFINE_ATTR_GLOBAL(usemap)
DEFINE_ATTR_GLOBAL(valign)
DEFINE_ATTR_GLOBAL(value)
DEFINE_ATTR_GLOBAL(valuetype)
DEFINE_ATTR_GLOBAL(version)
DEFINE_ATTR_GLOBAL(vlink)
DEFINE_ATTR_GLOBAL(vspace)
DEFINE_ATTR_GLOBAL(width)
DEFINE_ATTR_GLOBAL(wrap)

void HTMLTags::init()
{
    static bool initialized;
    if (!initialized) {
        // Use placement new to initialize the globals.
        static AtomicString xhtmlNS("http://www.w3.org/1999/xhtml");
        new (&xhtmlNamespaceURIAtom) AtomicString(xhtmlNS);
        new (&aTag) QualifiedName(nullAtom, "a", xhtmlNS);
        new (&abbrTag) QualifiedName(nullAtom, "abbr", xhtmlNS);
        new (&acronymTag) QualifiedName(nullAtom, "acronym", xhtmlNS);
        new (&addressTag) QualifiedName(nullAtom, "address", xhtmlNS);
        new (&appletTag) QualifiedName(nullAtom, "applet", xhtmlNS);
        new (&areaTag) QualifiedName(nullAtom, "area", xhtmlNS);
        new (&bTag) QualifiedName(nullAtom, "b", xhtmlNS);
        new (&baseTag) QualifiedName(nullAtom, "base", xhtmlNS);
        new (&basefontTag) QualifiedName(nullAtom, "basefont", xhtmlNS);
        new (&bdoTag) QualifiedName(nullAtom, "bdo", xhtmlNS);
        new (&bigTag) QualifiedName(nullAtom, "big", xhtmlNS);
        new (&blockquoteTag) QualifiedName(nullAtom, "blockquote", xhtmlNS);
        new (&bodyTag) QualifiedName(nullAtom, "body", xhtmlNS);
        new (&brTag) QualifiedName(nullAtom, "br", xhtmlNS);
        new (&buttonTag) QualifiedName(nullAtom, "button", xhtmlNS);
        new (&canvasTag) QualifiedName(nullAtom, "canvas", xhtmlNS);
        new (&captionTag) QualifiedName(nullAtom, "caption", xhtmlNS);
        new (&centerTag) QualifiedName(nullAtom, "center", xhtmlNS);
        new (&citeTag) QualifiedName(nullAtom, "cite", xhtmlNS);
        new (&codeTag) QualifiedName(nullAtom, "code", xhtmlNS);
        new (&colTag) QualifiedName(nullAtom, "col", xhtmlNS);
        new (&colgroupTag) QualifiedName(nullAtom, "colgroup", xhtmlNS);
        new (&ddTag) QualifiedName(nullAtom, "dd", xhtmlNS);
        new (&delTag) QualifiedName(nullAtom, "del", xhtmlNS);
        new (&dfnTag) QualifiedName(nullAtom, "dfn", xhtmlNS);
        new (&dirTag) QualifiedName(nullAtom, "dir", xhtmlNS);
        new (&divTag) QualifiedName(nullAtom, "div", xhtmlNS);
        new (&dlTag) QualifiedName(nullAtom, "dl", xhtmlNS);
        new (&dtTag) QualifiedName(nullAtom, "dt", xhtmlNS);
        new (&emTag) QualifiedName(nullAtom, "em", xhtmlNS);
        new (&embedTag) QualifiedName(nullAtom, "embed", xhtmlNS);
        new (&fieldsetTag) QualifiedName(nullAtom, "fieldset", xhtmlNS);
        new (&fontTag) QualifiedName(nullAtom, "font", xhtmlNS);
        new (&formTag) QualifiedName(nullAtom, "form", xhtmlNS);
        new (&frameTag) QualifiedName(nullAtom, "frame", xhtmlNS);
        new (&framesetTag) QualifiedName(nullAtom, "frameset", xhtmlNS);
        new (&headTag) QualifiedName(nullAtom, "head", xhtmlNS);
        new (&h1Tag) QualifiedName(nullAtom, "h1", xhtmlNS);
        new (&h2Tag) QualifiedName(nullAtom, "h2", xhtmlNS);
        new (&h3Tag) QualifiedName(nullAtom, "h3", xhtmlNS);
        new (&h4Tag) QualifiedName(nullAtom, "h4", xhtmlNS);
        new (&h5Tag) QualifiedName(nullAtom, "h5", xhtmlNS);
        new (&h6Tag) QualifiedName(nullAtom, "h6", xhtmlNS);
        new (&hrTag) QualifiedName(nullAtom, "hr", xhtmlNS);
        new (&htmlTag) QualifiedName(nullAtom, "html", xhtmlNS);
        new (&iTag) QualifiedName(nullAtom, "i", xhtmlNS);
        new (&iframeTag) QualifiedName(nullAtom, "iframe", xhtmlNS);
        new (&imgTag) QualifiedName(nullAtom, "img", xhtmlNS);
        new (&inputTag) QualifiedName(nullAtom, "input", xhtmlNS);
        new (&insTag) QualifiedName(nullAtom, "ins", xhtmlNS);
        new (&isindexTag) QualifiedName(nullAtom, "isindex", xhtmlNS);
        new (&kbdTag) QualifiedName(nullAtom, "kbd", xhtmlNS);
        new (&keygenTag) QualifiedName(nullAtom, "keygen", xhtmlNS);
        new (&labelTag) QualifiedName(nullAtom, "label", xhtmlNS);
        new (&layerTag) QualifiedName(nullAtom, "layer", xhtmlNS);
        new (&legendTag) QualifiedName(nullAtom, "legend", xhtmlNS);
        new (&liTag) QualifiedName(nullAtom, "li", xhtmlNS);
        new (&linkTag) QualifiedName(nullAtom, "link", xhtmlNS);
        new (&mapTag) QualifiedName(nullAtom, "map", xhtmlNS);
        new (&marqueeTag) QualifiedName(nullAtom, "marquee", xhtmlNS);
        new (&menuTag) QualifiedName(nullAtom, "menu", xhtmlNS);
        new (&metaTag) QualifiedName(nullAtom, "meta", xhtmlNS);
        new (&nobrTag) QualifiedName(nullAtom, "nobr", xhtmlNS);
        new (&noembedTag) QualifiedName(nullAtom, "noembed", xhtmlNS);
        new (&noframesTag) QualifiedName(nullAtom, "noframes", xhtmlNS);
        new (&nolayerTag) QualifiedName(nullAtom, "nolayer", xhtmlNS);
        new (&noscriptTag) QualifiedName(nullAtom, "noscript", xhtmlNS);
        new (&objectTag) QualifiedName(nullAtom, "object", xhtmlNS);
        new (&olTag) QualifiedName(nullAtom, "ol", xhtmlNS);
        new (&optgroupTag) QualifiedName(nullAtom, "optgroup", xhtmlNS);
        new (&optionTag) QualifiedName(nullAtom, "option", xhtmlNS);
        new (&pTag) QualifiedName(nullAtom, "p", xhtmlNS);
        new (&paramTag) QualifiedName(nullAtom, "param", xhtmlNS);
        new (&plaintextTag) QualifiedName(nullAtom, "plaintext", xhtmlNS);
        new (&preTag) QualifiedName(nullAtom, "pre", xhtmlNS);
        new (&qTag) QualifiedName(nullAtom, "q", xhtmlNS);
        new (&sTag) QualifiedName(nullAtom, "s", xhtmlNS);
        new (&sampTag) QualifiedName(nullAtom, "samp", xhtmlNS);
        new (&scriptTag) QualifiedName(nullAtom, "script", xhtmlNS);
        new (&selectTag) QualifiedName(nullAtom, "select", xhtmlNS);
        new (&smallTag) QualifiedName(nullAtom, "small", xhtmlNS);
        new (&spanTag) QualifiedName(nullAtom, "span", xhtmlNS);
        new (&strikeTag) QualifiedName(nullAtom, "strike", xhtmlNS);
        new (&strongTag) QualifiedName(nullAtom, "strong", xhtmlNS);
        new (&styleTag) QualifiedName(nullAtom, "style", xhtmlNS);
        new (&subTag) QualifiedName(nullAtom, "sub", xhtmlNS);
        new (&supTag) QualifiedName(nullAtom, "sup", xhtmlNS);
        new (&tableTag) QualifiedName(nullAtom, "table", xhtmlNS);
        new (&tbodyTag) QualifiedName(nullAtom, "tbody", xhtmlNS);
        new (&tdTag) QualifiedName(nullAtom, "td", xhtmlNS);
        new (&textareaTag) QualifiedName(nullAtom, "textarea", xhtmlNS);
        new (&tfootTag) QualifiedName(nullAtom, "tfoot", xhtmlNS);
        new (&thTag) QualifiedName(nullAtom, "th", xhtmlNS);
        new (&theadTag) QualifiedName(nullAtom, "thead", xhtmlNS);
        new (&titleTag) QualifiedName(nullAtom, "title", xhtmlNS);
        new (&trTag) QualifiedName(nullAtom, "tr", xhtmlNS);
        new (&ttTag) QualifiedName(nullAtom, "tt", xhtmlNS);
        new (&uTag) QualifiedName(nullAtom, "u", xhtmlNS);
        new (&ulTag) QualifiedName(nullAtom, "ul", xhtmlNS);
        new (&varTag) QualifiedName(nullAtom, "var", xhtmlNS);
        new (&wbrTag) QualifiedName(nullAtom, "wbr", xhtmlNS);
        new (&xmpTag) QualifiedName(nullAtom, "xmp", xhtmlNS);
        initialized = true;
    }
}

void HTMLAttributes::init()
{
    static bool initialized;
    if (!initialized) {
        // Attribute names.
        new (&abbrAttr) QualifiedName(nullAtom, "abbr", nullAtom);
        new (&accept_charsetAttr) QualifiedName(nullAtom, "accept-charset", nullAtom);
        new (&acceptAttr) QualifiedName(nullAtom, "accept", nullAtom);
        new (&accesskeyAttr) QualifiedName(nullAtom, "accesskey", nullAtom);
        new (&actionAttr) QualifiedName(nullAtom, "action", nullAtom);
        new (&alignAttr) QualifiedName(nullAtom, "align", nullAtom);
        new (&alinkAttr) QualifiedName(nullAtom, "alink", nullAtom);
        new (&altAttr) QualifiedName(nullAtom, "alt", nullAtom);
        new (&archiveAttr) QualifiedName(nullAtom, "archive", nullAtom);
        new (&autocompleteAttr) QualifiedName(nullAtom, "autocomplete", nullAtom);
        new (&autosaveAttr) QualifiedName(nullAtom, "autosave", nullAtom);
        new (&axisAttr) QualifiedName(nullAtom, "axis", nullAtom);
        new (&backgroundAttr) QualifiedName(nullAtom, "background", nullAtom);
        new (&behaviorAttr) QualifiedName(nullAtom, "behavior", nullAtom);
        new (&bgcolorAttr) QualifiedName(nullAtom, "bgcolor", nullAtom);
        new (&bgpropertiesAttr) QualifiedName(nullAtom, "bgproperties", nullAtom);
        new (&borderAttr) QualifiedName(nullAtom, "border", nullAtom);
        new (&bordercolorAttr) QualifiedName(nullAtom, "bordercolor", nullAtom);
        new (&cellpaddingAttr) QualifiedName(nullAtom, "cellpadding", nullAtom);
        new (&cellspacingAttr) QualifiedName(nullAtom, "cellspacing", nullAtom);
        new (&charAttrAttr) QualifiedName(nullAtom, "char", nullAtom);
        new (&challengeAttr) QualifiedName(nullAtom, "challenge", nullAtom);
        new (&charoffAttr) QualifiedName(nullAtom, "charoff", nullAtom);
        new (&charsetAttr) QualifiedName(nullAtom, "charset", nullAtom);
        new (&checkedAttr) QualifiedName(nullAtom, "checked", nullAtom);
        new (&cellborderAttr) QualifiedName(nullAtom, "cellborder", nullAtom);
        new (&citeAttr) QualifiedName(nullAtom, "cite", nullAtom);
        new (&classAttrAttr) QualifiedName(nullAtom, "class", nullAtom);
        new (&classidAttr) QualifiedName(nullAtom, "classid", nullAtom);
        new (&clearAttr) QualifiedName(nullAtom, "clear", nullAtom);
        new (&codeAttr) QualifiedName(nullAtom, "code", nullAtom);
        new (&codebaseAttr) QualifiedName(nullAtom, "codebase", nullAtom);
        new (&codetypeAttr) QualifiedName(nullAtom, "codetype", nullAtom);
        new (&colorAttr) QualifiedName(nullAtom, "color", nullAtom);
        new (&colsAttr) QualifiedName(nullAtom, "cols", nullAtom);
        new (&colspanAttr) QualifiedName(nullAtom, "colspan", nullAtom);
        new (&compactAttr) QualifiedName(nullAtom, "compact", nullAtom);
        new (&compositeAttr) QualifiedName(nullAtom, "composite", nullAtom);
        new (&contentAttr) QualifiedName(nullAtom, "content", nullAtom);
        new (&contenteditableAttr) QualifiedName(nullAtom, "contenteditable", nullAtom);
        new (&coordsAttr) QualifiedName(nullAtom, "coords", nullAtom);
        new (&dataAttr) QualifiedName(nullAtom, "data", nullAtom);
        new (&datetimeAttr) QualifiedName(nullAtom, "datetime", nullAtom);
        new (&declareAttr) QualifiedName(nullAtom, "declare", nullAtom);
        new (&deferAttr) QualifiedName(nullAtom, "defer", nullAtom);
        new (&dirAttr) QualifiedName(nullAtom, "dir", nullAtom);
        new (&directionAttr) QualifiedName(nullAtom, "direction", nullAtom);
        new (&disabledAttr) QualifiedName(nullAtom, "disabled", nullAtom);
        new (&enctypeAttr) QualifiedName(nullAtom, "enctype", nullAtom);
        new (&faceAttr) QualifiedName(nullAtom, "face", nullAtom);
        new (&forAttrAttr) QualifiedName(nullAtom, "for", nullAtom);
        new (&frameAttr) QualifiedName(nullAtom, "frame", nullAtom);
        new (&frameborderAttr) QualifiedName(nullAtom, "frameborder", nullAtom);
        new (&headersAttr) QualifiedName(nullAtom, "headers", nullAtom);
        new (&heightAttr) QualifiedName(nullAtom, "height", nullAtom);
        new (&hiddenAttr) QualifiedName(nullAtom, "hidden", nullAtom);
        new (&hrefAttr) QualifiedName(nullAtom, "href", nullAtom);
        new (&hreflangAttr) QualifiedName(nullAtom, "hreflang", nullAtom);
        new (&hspaceAttr) QualifiedName(nullAtom, "hspace", nullAtom);
        new (&http_equivAttr) QualifiedName(nullAtom, "http-equiv", nullAtom);
        new (&idAttrAttr) QualifiedName(nullAtom, "id", nullAtom);
        new (&incrementalAttr) QualifiedName(nullAtom, "incremental", nullAtom);
        new (&ismapAttr) QualifiedName(nullAtom, "ismap", nullAtom);
        new (&keytypeAttr) QualifiedName(nullAtom, "keytype", nullAtom);
        new (&labelAttr) QualifiedName(nullAtom, "label", nullAtom);
        new (&langAttr) QualifiedName(nullAtom, "lang", nullAtom);
        new (&languageAttr) QualifiedName(nullAtom, "language", nullAtom);
        new (&leftAttr) QualifiedName(nullAtom, "left", nullAtom);
        new (&leftmarginAttr) QualifiedName(nullAtom, "leftmargin", nullAtom);
        new (&linkAttr) QualifiedName(nullAtom, "link", nullAtom);
        new (&longdescAttr) QualifiedName(nullAtom, "longdesc", nullAtom);
        new (&loopAttr) QualifiedName(nullAtom, "loop", nullAtom);
        new (&marginheightAttr) QualifiedName(nullAtom, "marginheight", nullAtom);
        new (&marginwidthAttr) QualifiedName(nullAtom, "marginwidth", nullAtom);
        new (&maxAttr) QualifiedName(nullAtom, "max", nullAtom);
        new (&maxlengthAttr) QualifiedName(nullAtom, "maxlength", nullAtom);
        new (&mayscriptAttr) QualifiedName(nullAtom, "mayscript", nullAtom);
        new (&mediaAttr) QualifiedName(nullAtom, "media", nullAtom);
        new (&methodAttr) QualifiedName(nullAtom, "method", nullAtom);
        new (&minAttr) QualifiedName(nullAtom, "min", nullAtom);
        new (&multipleAttr) QualifiedName(nullAtom, "multiple", nullAtom);
        new (&nameAttr) QualifiedName(nullAtom, "name", nullAtom);
        new (&nohrefAttr) QualifiedName(nullAtom, "nohref", nullAtom);
        new (&noresizeAttr) QualifiedName(nullAtom, "noresize", nullAtom);
        new (&noshadeAttr) QualifiedName(nullAtom, "noshade", nullAtom);
        new (&nowrapAttr) QualifiedName(nullAtom, "nowrap", nullAtom);
        new (&objectAttr) QualifiedName(nullAtom, "object", nullAtom);
        new (&onabortAttr) QualifiedName(nullAtom, "onabort", nullAtom);
        new (&onbeforecopyAttr) QualifiedName(nullAtom, "onbeforecopy", nullAtom);
        new (&onbeforecutAttr) QualifiedName(nullAtom, "onbeforecut", nullAtom);
        new (&onbeforepasteAttr) QualifiedName(nullAtom, "onbeforepaste", nullAtom);
        new (&onblurAttr) QualifiedName(nullAtom, "onblur", nullAtom);
        new (&onchangeAttr) QualifiedName(nullAtom, "onchange", nullAtom);
        new (&onclickAttr) QualifiedName(nullAtom, "onclick", nullAtom);
        new (&oncontextmenuAttr) QualifiedName(nullAtom, "oncontextmenu", nullAtom);
        new (&oncopyAttr) QualifiedName(nullAtom, "oncopy", nullAtom);
        new (&oncutAttr) QualifiedName(nullAtom, "oncut", nullAtom);
        new (&ondblclickAttr) QualifiedName(nullAtom, "ondblclick", nullAtom);
        new (&ondragAttr) QualifiedName(nullAtom, "ondrag", nullAtom);
        new (&ondragendAttr) QualifiedName(nullAtom, "ondragend", nullAtom);
        new (&ondragenterAttr) QualifiedName(nullAtom, "ondragenter", nullAtom);
        new (&ondragleaveAttr) QualifiedName(nullAtom, "ondragleave", nullAtom);
        new (&ondragoverAttr) QualifiedName(nullAtom, "ondragover", nullAtom);
        new (&ondragstartAttr) QualifiedName(nullAtom, "ondragstart", nullAtom);
        new (&ondropAttr) QualifiedName(nullAtom, "ondrop", nullAtom);
        new (&onerrorAttr) QualifiedName(nullAtom, "onerror", nullAtom);
        new (&onfocusAttr) QualifiedName(nullAtom, "onfocus", nullAtom);
        new (&oninputAttr) QualifiedName(nullAtom, "oninput", nullAtom);
        new (&onkeydownAttr) QualifiedName(nullAtom, "onkeydown", nullAtom);
        new (&onkeypressAttr) QualifiedName(nullAtom, "onkeypress", nullAtom);
        new (&onkeyupAttr) QualifiedName(nullAtom, "onkeyup", nullAtom);
        new (&onloadAttr) QualifiedName(nullAtom, "onload", nullAtom);
        new (&onmousedownAttr) QualifiedName(nullAtom, "onmousedown", nullAtom);
        new (&onmousemoveAttr) QualifiedName(nullAtom, "onmousemove", nullAtom);
        new (&onmouseoutAttr) QualifiedName(nullAtom, "onmouseout", nullAtom);
        new (&onmouseoverAttr) QualifiedName(nullAtom, "onmouseover", nullAtom);
        new (&onmouseupAttr) QualifiedName(nullAtom, "onmouseup", nullAtom);
        new (&onmousewheelAttr) QualifiedName(nullAtom, "onmousewheel", nullAtom);
        new (&onpasteAttr) QualifiedName(nullAtom, "onpaste", nullAtom);
        new (&onresetAttr) QualifiedName(nullAtom, "onreset", nullAtom);
        new (&onresizeAttr) QualifiedName(nullAtom, "onresize", nullAtom);
        new (&onscrollAttr) QualifiedName(nullAtom, "onscroll", nullAtom);
        new (&onsearchAttr) QualifiedName(nullAtom, "onsearch", nullAtom);
        new (&onselectAttr) QualifiedName(nullAtom, "onselect", nullAtom);
        new (&onselectstartAttr) QualifiedName(nullAtom, "onselectstart", nullAtom);
        new (&onsubmitAttr) QualifiedName(nullAtom, "onsubmit", nullAtom);
        new (&onunloadAttr) QualifiedName(nullAtom, "onunload", nullAtom);
        new (&pagexAttr) QualifiedName(nullAtom, "pagex", nullAtom);
        new (&pageyAttr) QualifiedName(nullAtom, "pagey", nullAtom);
        new (&placeholderAttr) QualifiedName(nullAtom, "placeholder", nullAtom);
        new (&plainAttr) QualifiedName(nullAtom, "plain", nullAtom);
        new (&pluginpageAttr) QualifiedName(nullAtom, "pluginpage", nullAtom);
        new (&pluginspageAttr) QualifiedName(nullAtom, "pluginspage", nullAtom);
        new (&pluginurlAttr) QualifiedName(nullAtom, "pluginurl", nullAtom);
        new (&precisionAttr) QualifiedName(nullAtom, "precision", nullAtom);
        new (&profileAttr) QualifiedName(nullAtom, "profile", nullAtom);
        new (&promptAttr) QualifiedName(nullAtom, "prompt", nullAtom);
        new (&readonlyAttr) QualifiedName(nullAtom, "readonly", nullAtom);
        new (&relAttr) QualifiedName(nullAtom, "rel", nullAtom);
        new (&resultsAttr) QualifiedName(nullAtom, "results", nullAtom);
        new (&revAttr) QualifiedName(nullAtom, "rev", nullAtom);
        new (&rowsAttr) QualifiedName(nullAtom, "rows", nullAtom);
        new (&rowspanAttr) QualifiedName(nullAtom, "rowspan", nullAtom);
        new (&rulesAttr) QualifiedName(nullAtom, "rules", nullAtom);
        new (&schemeAttr) QualifiedName(nullAtom, "scheme", nullAtom);
        new (&scopeAttr) QualifiedName(nullAtom, "scope", nullAtom);
        new (&scrollamountAttr) QualifiedName(nullAtom, "scrollamount", nullAtom);
        new (&scrolldelayAttr) QualifiedName(nullAtom, "scrolldelay", nullAtom);
        new (&scrollingAttr) QualifiedName(nullAtom, "scrolling", nullAtom);
        new (&selectedAttr) QualifiedName(nullAtom, "selected", nullAtom);
        new (&shapeAttr) QualifiedName(nullAtom, "shape", nullAtom);
        new (&sizeAttr) QualifiedName(nullAtom, "size", nullAtom);
        new (&spanAttr) QualifiedName(nullAtom, "span", nullAtom);
        new (&srcAttr) QualifiedName(nullAtom, "src", nullAtom);
        new (&standbyAttr) QualifiedName(nullAtom, "standby", nullAtom);
        new (&startAttr) QualifiedName(nullAtom, "start", nullAtom);
        new (&styleAttr) QualifiedName(nullAtom, "style", nullAtom);
        new (&summaryAttr) QualifiedName(nullAtom, "summary", nullAtom);
        new (&tabindexAttr) QualifiedName(nullAtom, "tabindex", nullAtom);
        new (&tableborderAttr) QualifiedName(nullAtom, "tableborder", nullAtom);
        new (&targetAttr) QualifiedName(nullAtom, "target", nullAtom);
        new (&textAttr) QualifiedName(nullAtom, "text", nullAtom);
        new (&titleAttr) QualifiedName(nullAtom, "title", nullAtom);
        new (&topAttr) QualifiedName(nullAtom, "top", nullAtom);
        new (&topmarginAttr) QualifiedName(nullAtom, "topmargin", nullAtom);
        new (&truespeedAttr) QualifiedName(nullAtom, "truespeed", nullAtom);
        new (&typeAttr) QualifiedName(nullAtom, "type", nullAtom);
        new (&usemapAttr) QualifiedName(nullAtom, "usemap", nullAtom);
        new (&valignAttr) QualifiedName(nullAtom, "valign", nullAtom);
        new (&valueAttr) QualifiedName(nullAtom, "value", nullAtom);
        new (&valuetypeAttr) QualifiedName(nullAtom, "valuetype", nullAtom);
        new (&versionAttr) QualifiedName(nullAtom, "version", nullAtom);
        new (&vlinkAttr) QualifiedName(nullAtom, "vlink", nullAtom);
        new (&vspaceAttr) QualifiedName(nullAtom, "vspace", nullAtom);
        new (&widthAttr) QualifiedName(nullAtom, "width", nullAtom);
        new (&wrapAttr) QualifiedName(nullAtom, "wrap", nullAtom);

        initialized = true;
    }
}

}
