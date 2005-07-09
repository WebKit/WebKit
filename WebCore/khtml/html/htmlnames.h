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
    extern const QualifiedName aQName;
    extern const QualifiedName abbrQName;
    extern const QualifiedName acronymQName;
    extern const QualifiedName addressQName;
    extern const QualifiedName appletQName;
    extern const QualifiedName areaQName;
    extern const QualifiedName bQName;
    extern const QualifiedName baseQName;
    extern const QualifiedName basefontQName;
    extern const QualifiedName bdoQName;
    extern const QualifiedName bigQName;
    extern const QualifiedName blockquoteQName;
    extern const QualifiedName bodyQName;
    extern const QualifiedName brQName;
    extern const QualifiedName buttonQName;
    extern const QualifiedName canvasQName;
    extern const QualifiedName captionQName;
    extern const QualifiedName centerQName;
    extern const QualifiedName citeQName;
    extern const QualifiedName codeQName;
    extern const QualifiedName colQName;
    extern const QualifiedName colgroupQName;
    extern const QualifiedName ddQName;
    extern const QualifiedName delQName;
    extern const QualifiedName dfnQName;
    extern const QualifiedName dirQName;
    extern const QualifiedName divQName;
    extern const QualifiedName dlQName;
    extern const QualifiedName dtQName;
    extern const QualifiedName emQName;
    extern const QualifiedName embedQName;
    extern const QualifiedName fieldsetQName;
    extern const QualifiedName fontQName;
    extern const QualifiedName formQName;
    extern const QualifiedName frameQName;
    extern const QualifiedName framesetQName;
    extern const QualifiedName headQName;
    extern const QualifiedName h1QName;
    extern const QualifiedName h2QName;
    extern const QualifiedName h3QName;
    extern const QualifiedName h4QName;
    extern const QualifiedName h5QName;
    extern const QualifiedName h6QName;
    extern const QualifiedName hrQName;
    extern const QualifiedName htmlQName;
    extern const QualifiedName iQName;
    extern const QualifiedName iframeQName;
    extern const QualifiedName imgQName;
    extern const QualifiedName inputQName;
    extern const QualifiedName insQName;
    extern const QualifiedName isindexQName;
    extern const QualifiedName kbdQName;
    extern const QualifiedName keygenQName;
    extern const QualifiedName labelQName;
    extern const QualifiedName layerQName;
    extern const QualifiedName legendQName;
    extern const QualifiedName liQName;
    extern const QualifiedName linkQName;
    extern const QualifiedName mapQName;
    extern const QualifiedName marqueeQName;
    extern const QualifiedName menuQName;
    extern const QualifiedName metaQName;
    extern const QualifiedName nobrQName;
    extern const QualifiedName noembedQName;
    extern const QualifiedName noframesQName;
    extern const QualifiedName nolayerQName;
    extern const QualifiedName noscriptQName;
    extern const QualifiedName objectQName;
    extern const QualifiedName olQName;
    extern const QualifiedName optgroupQName;
    extern const QualifiedName optionQName;
    extern const QualifiedName pQName;
    extern const QualifiedName paramQName;
    extern const QualifiedName plaintextQName;
    extern const QualifiedName preQName;
    extern const QualifiedName qQName;
    extern const QualifiedName sQName;
    extern const QualifiedName sampQName;
    extern const QualifiedName scriptQName;
    extern const QualifiedName selectQName;
    extern const QualifiedName smallQName;
    extern const QualifiedName spanQName;
    extern const QualifiedName strikeQName;
    extern const QualifiedName strongQName;
    extern const QualifiedName styleQName;
    extern const QualifiedName subQName;
    extern const QualifiedName supQName;
    extern const QualifiedName tableQName;
    extern const QualifiedName tbodyQName;
    extern const QualifiedName tdQName;
    extern const QualifiedName textareaQName;
    extern const QualifiedName tfootQName;
    extern const QualifiedName thQName;
    extern const QualifiedName theadQName;
    extern const QualifiedName titleQName;
    extern const QualifiedName trQName;
    extern const QualifiedName ttQName;
    extern const QualifiedName uQName;
    extern const QualifiedName ulQName;
    extern const QualifiedName varQName;
    extern const QualifiedName wbrQName;
    extern const QualifiedName xmpQName;

#endif

// FIXME: Make this a namespace instead of a class.
class HTMLNames
{
public:
#if !KHTML_HTMLNAMES_HIDE_GLOBALS
    // The namespace URI.
    static const AtomicString& xhtmlNamespaceURI() { return xhtmlNamespaceURIAtom; }
    
// Full tag names.
#define DEFINE_GETTER(name) \
    static const QualifiedName& name() { return name ## QName; }

    DEFINE_GETTER(a)
    DEFINE_GETTER(abbr)
    DEFINE_GETTER(acronym)
    DEFINE_GETTER(address)
    DEFINE_GETTER(applet)
    DEFINE_GETTER(area)
    DEFINE_GETTER(b)
    DEFINE_GETTER(base)
    DEFINE_GETTER(basefont)
    DEFINE_GETTER(bdo)
    DEFINE_GETTER(big)
    DEFINE_GETTER(blockquote)
    DEFINE_GETTER(body)
    DEFINE_GETTER(br)
    DEFINE_GETTER(button)
    DEFINE_GETTER(canvas)
    DEFINE_GETTER(caption)
    DEFINE_GETTER(center)
    DEFINE_GETTER(cite)
    DEFINE_GETTER(code)
    DEFINE_GETTER(col)
    DEFINE_GETTER(colgroup)
    DEFINE_GETTER(dd)
    DEFINE_GETTER(del)
    DEFINE_GETTER(dfn)
    DEFINE_GETTER(dir)
    DEFINE_GETTER(div)
    DEFINE_GETTER(dl)
    DEFINE_GETTER(dt)
    DEFINE_GETTER(em)
    DEFINE_GETTER(embed)
    DEFINE_GETTER(fieldset)
    DEFINE_GETTER(font)
    DEFINE_GETTER(form)
    DEFINE_GETTER(frame)
    DEFINE_GETTER(frameset)
    DEFINE_GETTER(head)
    DEFINE_GETTER(h1)
    DEFINE_GETTER(h2)
    DEFINE_GETTER(h3)
    DEFINE_GETTER(h4)
    DEFINE_GETTER(h5)
    DEFINE_GETTER(h6)
    DEFINE_GETTER(hr)
    DEFINE_GETTER(html)
    DEFINE_GETTER(i)
    DEFINE_GETTER(iframe)
    DEFINE_GETTER(img)
    DEFINE_GETTER(input)
    DEFINE_GETTER(ins)
    DEFINE_GETTER(isindex)
    DEFINE_GETTER(kbd)
    DEFINE_GETTER(keygen)
    DEFINE_GETTER(label)
    DEFINE_GETTER(layer)
    DEFINE_GETTER(legend)
    DEFINE_GETTER(li)
    DEFINE_GETTER(link)
    DEFINE_GETTER(map)
    DEFINE_GETTER(marquee)
    DEFINE_GETTER(menu)
    DEFINE_GETTER(meta)
    DEFINE_GETTER(nobr)
    DEFINE_GETTER(noembed)
    DEFINE_GETTER(noframes)
    DEFINE_GETTER(nolayer)
    DEFINE_GETTER(noscript)
    DEFINE_GETTER(object)
    DEFINE_GETTER(ol)
    DEFINE_GETTER(optgroup)
    DEFINE_GETTER(option)
    DEFINE_GETTER(p)
    DEFINE_GETTER(param)
    DEFINE_GETTER(plaintext)
    DEFINE_GETTER(pre)
    DEFINE_GETTER(q)
    DEFINE_GETTER(s)
    DEFINE_GETTER(samp)
    DEFINE_GETTER(script)
    DEFINE_GETTER(select)
    DEFINE_GETTER(small)
    DEFINE_GETTER(span)
    DEFINE_GETTER(strike)
    DEFINE_GETTER(strong)
    DEFINE_GETTER(style)
    DEFINE_GETTER(sub)
    DEFINE_GETTER(sup)
    DEFINE_GETTER(table)
    DEFINE_GETTER(tbody)
    DEFINE_GETTER(td)
    DEFINE_GETTER(textarea)
    DEFINE_GETTER(tfoot)
    DEFINE_GETTER(th)
    DEFINE_GETTER(thead)
    DEFINE_GETTER(title)
    DEFINE_GETTER(tr)
    DEFINE_GETTER(tt)
    DEFINE_GETTER(u)
    DEFINE_GETTER(ul)
    DEFINE_GETTER(var)
    DEFINE_GETTER(wbr)
    DEFINE_GETTER(xmp)

    // Attribute names.
#endif

    // Init routine
    static void init();
};

}

#endif
