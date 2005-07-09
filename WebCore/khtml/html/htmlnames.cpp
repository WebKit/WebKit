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

#define KHTML_HTMLNAMES_HIDE_GLOBALS 1

#include <qptrdict.h>
#include "htmlnames.h"
#include "dom_qname.h"

namespace DOM {

void* xhtmlNamespaceURIAtom[(sizeof(AtomicString) + sizeof(void*) - 1) / sizeof(void*)];

// Define a QualifiedName-sized array of pointers to avoid static initialization.
// Use an array of pointers instead of an array of char in case there is some alignment issue.
#define DEFINE_GLOBAL(name) \
    void* name ## QName[(sizeof(QualifiedName) + sizeof(void*) - 1) / sizeof(void*)];

DEFINE_GLOBAL(a)
DEFINE_GLOBAL(abbr)
DEFINE_GLOBAL(acronym)
DEFINE_GLOBAL(address)
DEFINE_GLOBAL(applet)
DEFINE_GLOBAL(area)
DEFINE_GLOBAL(b)
DEFINE_GLOBAL(base)
DEFINE_GLOBAL(basefont)
DEFINE_GLOBAL(bdo)
DEFINE_GLOBAL(big)
DEFINE_GLOBAL(blockquote)
DEFINE_GLOBAL(body)
DEFINE_GLOBAL(br)
DEFINE_GLOBAL(button)
DEFINE_GLOBAL(canvas)
DEFINE_GLOBAL(caption)
DEFINE_GLOBAL(center)
DEFINE_GLOBAL(cite)
DEFINE_GLOBAL(code)
DEFINE_GLOBAL(col)
DEFINE_GLOBAL(colgroup)
DEFINE_GLOBAL(dd)
DEFINE_GLOBAL(del)
DEFINE_GLOBAL(dfn)
DEFINE_GLOBAL(dir)
DEFINE_GLOBAL(div)
DEFINE_GLOBAL(dl)
DEFINE_GLOBAL(dt)
DEFINE_GLOBAL(em)
DEFINE_GLOBAL(embed)
DEFINE_GLOBAL(fieldset)
DEFINE_GLOBAL(font)
DEFINE_GLOBAL(form)
DEFINE_GLOBAL(frame)
DEFINE_GLOBAL(frameset)
DEFINE_GLOBAL(head)
DEFINE_GLOBAL(h1)
DEFINE_GLOBAL(h2)
DEFINE_GLOBAL(h3)
DEFINE_GLOBAL(h4)
DEFINE_GLOBAL(h5)
DEFINE_GLOBAL(h6)
DEFINE_GLOBAL(hr)
DEFINE_GLOBAL(html)
DEFINE_GLOBAL(i)
DEFINE_GLOBAL(iframe)
DEFINE_GLOBAL(img)
DEFINE_GLOBAL(input)
DEFINE_GLOBAL(ins)
DEFINE_GLOBAL(isindex)
DEFINE_GLOBAL(kbd)
DEFINE_GLOBAL(keygen)
DEFINE_GLOBAL(label)
DEFINE_GLOBAL(layer)
DEFINE_GLOBAL(legend)
DEFINE_GLOBAL(li)
DEFINE_GLOBAL(link)
DEFINE_GLOBAL(map)
DEFINE_GLOBAL(marquee)
DEFINE_GLOBAL(menu)
DEFINE_GLOBAL(meta)
DEFINE_GLOBAL(nobr)
DEFINE_GLOBAL(noembed)
DEFINE_GLOBAL(noframes)
DEFINE_GLOBAL(nolayer)
DEFINE_GLOBAL(noscript)
DEFINE_GLOBAL(object)
DEFINE_GLOBAL(ol)
DEFINE_GLOBAL(optgroup)
DEFINE_GLOBAL(option)
DEFINE_GLOBAL(p)
DEFINE_GLOBAL(param)
DEFINE_GLOBAL(plaintext)
DEFINE_GLOBAL(pre)
DEFINE_GLOBAL(q)
DEFINE_GLOBAL(s)
DEFINE_GLOBAL(samp)
DEFINE_GLOBAL(script)
DEFINE_GLOBAL(select)
DEFINE_GLOBAL(small)
DEFINE_GLOBAL(span)
DEFINE_GLOBAL(strike)
DEFINE_GLOBAL(strong)
DEFINE_GLOBAL(style)
DEFINE_GLOBAL(sub)
DEFINE_GLOBAL(sup)
DEFINE_GLOBAL(table)
DEFINE_GLOBAL(tbody)
DEFINE_GLOBAL(td)
DEFINE_GLOBAL(textarea)
DEFINE_GLOBAL(tfoot)
DEFINE_GLOBAL(th)
DEFINE_GLOBAL(thead)
DEFINE_GLOBAL(title)
DEFINE_GLOBAL(tr)
DEFINE_GLOBAL(tt)
DEFINE_GLOBAL(u)
DEFINE_GLOBAL(ul)
DEFINE_GLOBAL(var)
DEFINE_GLOBAL(wbr)
DEFINE_GLOBAL(xmp)

void HTMLNames::init()
{
    static bool initialized;
    if (!initialized) {
        // Use placement new to initialize the globals.
        static AtomicString xhtmlNS("http://www.w3.org/1999/xhtml");
        new (&xhtmlNamespaceURIAtom) AtomicString(xhtmlNS);
        new (&aQName) QualifiedName(nullAtom, "a", xhtmlNS);
        new (&abbrQName) QualifiedName(nullAtom, "abbr", xhtmlNS);
        new (&acronymQName) QualifiedName(nullAtom, "acronym", xhtmlNS);
        new (&addressQName) QualifiedName(nullAtom, "address", xhtmlNS);
        new (&appletQName) QualifiedName(nullAtom, "applet", xhtmlNS);
        new (&areaQName) QualifiedName(nullAtom, "area", xhtmlNS);
        new (&bQName) QualifiedName(nullAtom, "b", xhtmlNS);
        new (&baseQName) QualifiedName(nullAtom, "base", xhtmlNS);
        new (&basefontQName) QualifiedName(nullAtom, "basefont", xhtmlNS);
        new (&bdoQName) QualifiedName(nullAtom, "bdo", xhtmlNS);
        new (&bigQName) QualifiedName(nullAtom, "big", xhtmlNS);
        new (&blockquoteQName) QualifiedName(nullAtom, "blockquote", xhtmlNS);
        new (&bodyQName) QualifiedName(nullAtom, "body", xhtmlNS);
        new (&brQName) QualifiedName(nullAtom, "br", xhtmlNS);
        new (&buttonQName) QualifiedName(nullAtom, "button", xhtmlNS);
        new (&canvasQName) QualifiedName(nullAtom, "canvas", xhtmlNS);
        new (&captionQName) QualifiedName(nullAtom, "caption", xhtmlNS);
        new (&centerQName) QualifiedName(nullAtom, "center", xhtmlNS);
        new (&citeQName) QualifiedName(nullAtom, "cite", xhtmlNS);
        new (&codeQName) QualifiedName(nullAtom, "code", xhtmlNS);
        new (&colQName) QualifiedName(nullAtom, "col", xhtmlNS);
        new (&colgroupQName) QualifiedName(nullAtom, "colgroup", xhtmlNS);
        new (&ddQName) QualifiedName(nullAtom, "dd", xhtmlNS);
        new (&delQName) QualifiedName(nullAtom, "del", xhtmlNS);
        new (&dfnQName) QualifiedName(nullAtom, "dfn", xhtmlNS);
        new (&dirQName) QualifiedName(nullAtom, "dir", xhtmlNS);
        new (&divQName) QualifiedName(nullAtom, "div", xhtmlNS);
        new (&dlQName) QualifiedName(nullAtom, "dl", xhtmlNS);
        new (&dtQName) QualifiedName(nullAtom, "dt", xhtmlNS);
        new (&emQName) QualifiedName(nullAtom, "em", xhtmlNS);
        new (&embedQName) QualifiedName(nullAtom, "embed", xhtmlNS);
        new (&fieldsetQName) QualifiedName(nullAtom, "fieldset", xhtmlNS);
        new (&fontQName) QualifiedName(nullAtom, "font", xhtmlNS);
        new (&formQName) QualifiedName(nullAtom, "form", xhtmlNS);
        new (&frameQName) QualifiedName(nullAtom, "frame", xhtmlNS);
        new (&framesetQName) QualifiedName(nullAtom, "frameset", xhtmlNS);
        new (&headQName) QualifiedName(nullAtom, "head", xhtmlNS);
        new (&h1QName) QualifiedName(nullAtom, "h1", xhtmlNS);
        new (&h2QName) QualifiedName(nullAtom, "h2", xhtmlNS);
        new (&h3QName) QualifiedName(nullAtom, "h3", xhtmlNS);
        new (&h4QName) QualifiedName(nullAtom, "h4", xhtmlNS);
        new (&h5QName) QualifiedName(nullAtom, "h5", xhtmlNS);
        new (&h6QName) QualifiedName(nullAtom, "h6", xhtmlNS);
        new (&hrQName) QualifiedName(nullAtom, "hr", xhtmlNS);
        new (&htmlQName) QualifiedName(nullAtom, "html", xhtmlNS);
        new (&iQName) QualifiedName(nullAtom, "i", xhtmlNS);
        new (&iframeQName) QualifiedName(nullAtom, "iframe", xhtmlNS);
        new (&imgQName) QualifiedName(nullAtom, "img", xhtmlNS);
        new (&inputQName) QualifiedName(nullAtom, "input", xhtmlNS);
        new (&insQName) QualifiedName(nullAtom, "ins", xhtmlNS);
        new (&isindexQName) QualifiedName(nullAtom, "isindex", xhtmlNS);
        new (&kbdQName) QualifiedName(nullAtom, "kbd", xhtmlNS);
        new (&keygenQName) QualifiedName(nullAtom, "keygen", xhtmlNS);
        new (&labelQName) QualifiedName(nullAtom, "label", xhtmlNS);
        new (&layerQName) QualifiedName(nullAtom, "layer", xhtmlNS);
        new (&legendQName) QualifiedName(nullAtom, "legend", xhtmlNS);
        new (&liQName) QualifiedName(nullAtom, "li", xhtmlNS);
        new (&linkQName) QualifiedName(nullAtom, "link", xhtmlNS);
        new (&mapQName) QualifiedName(nullAtom, "map", xhtmlNS);
        new (&marqueeQName) QualifiedName(nullAtom, "marquee", xhtmlNS);
        new (&menuQName) QualifiedName(nullAtom, "menu", xhtmlNS);
        new (&metaQName) QualifiedName(nullAtom, "meta", xhtmlNS);
        new (&nobrQName) QualifiedName(nullAtom, "nobr", xhtmlNS);
        new (&noembedQName) QualifiedName(nullAtom, "noembed", xhtmlNS);
        new (&noframesQName) QualifiedName(nullAtom, "noframes", xhtmlNS);
        new (&nolayerQName) QualifiedName(nullAtom, "nolayer", xhtmlNS);
        new (&noscriptQName) QualifiedName(nullAtom, "noscript", xhtmlNS);
        new (&objectQName) QualifiedName(nullAtom, "object", xhtmlNS);
        new (&olQName) QualifiedName(nullAtom, "ol", xhtmlNS);
        new (&optgroupQName) QualifiedName(nullAtom, "optgroup", xhtmlNS);
        new (&optionQName) QualifiedName(nullAtom, "option", xhtmlNS);
        new (&pQName) QualifiedName(nullAtom, "p", xhtmlNS);
        new (&paramQName) QualifiedName(nullAtom, "param", xhtmlNS);
        new (&plaintextQName) QualifiedName(nullAtom, "plaintext", xhtmlNS);
        new (&preQName) QualifiedName(nullAtom, "pre", xhtmlNS);
        new (&qQName) QualifiedName(nullAtom, "q", xhtmlNS);
        new (&sQName) QualifiedName(nullAtom, "s", xhtmlNS);
        new (&sampQName) QualifiedName(nullAtom, "samp", xhtmlNS);
        new (&scriptQName) QualifiedName(nullAtom, "script", xhtmlNS);
        new (&selectQName) QualifiedName(nullAtom, "select", xhtmlNS);
        new (&smallQName) QualifiedName(nullAtom, "small", xhtmlNS);
        new (&spanQName) QualifiedName(nullAtom, "span", xhtmlNS);
        new (&strikeQName) QualifiedName(nullAtom, "strike", xhtmlNS);
        new (&strongQName) QualifiedName(nullAtom, "strong", xhtmlNS);
        new (&styleQName) QualifiedName(nullAtom, "style", xhtmlNS);
        new (&subQName) QualifiedName(nullAtom, "sub", xhtmlNS);
        new (&supQName) QualifiedName(nullAtom, "sup", xhtmlNS);
        new (&tableQName) QualifiedName(nullAtom, "table", xhtmlNS);
        new (&tbodyQName) QualifiedName(nullAtom, "tbody", xhtmlNS);
        new (&tdQName) QualifiedName(nullAtom, "td", xhtmlNS);
        new (&textareaQName) QualifiedName(nullAtom, "textarea", xhtmlNS);
        new (&tfootQName) QualifiedName(nullAtom, "tfoot", xhtmlNS);
        new (&thQName) QualifiedName(nullAtom, "th", xhtmlNS);
        new (&theadQName) QualifiedName(nullAtom, "thead", xhtmlNS);
        new (&titleQName) QualifiedName(nullAtom, "title", xhtmlNS);
        new (&trQName) QualifiedName(nullAtom, "tr", xhtmlNS);
        new (&ttQName) QualifiedName(nullAtom, "tt", xhtmlNS);
        new (&uQName) QualifiedName(nullAtom, "u", xhtmlNS);
        new (&ulQName) QualifiedName(nullAtom, "ul", xhtmlNS);
        new (&varQName) QualifiedName(nullAtom, "var", xhtmlNS);
        new (&wbrQName) QualifiedName(nullAtom, "wbr", xhtmlNS);
        new (&xmpQName) QualifiedName(nullAtom, "xmp", xhtmlNS);
        initialized = true;
    }
}

}
