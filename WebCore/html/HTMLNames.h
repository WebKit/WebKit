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

#include "QualifiedName.h"

namespace WebCore { namespace HTMLNames {

#define DOM_HTMLNAMES_FOR_EACH_TAG(macro) \
    macro(a) \
    macro(abbr) \
    macro(acronym) \
    macro(address) \
    macro(applet) \
    macro(area) \
    macro(b) \
    macro(base) \
    macro(basefont) \
    macro(bdo) \
    macro(big) \
    macro(blockquote) \
    macro(body) \
    macro(br) \
    macro(button) \
    macro(canvas) \
    macro(caption) \
    macro(center) \
    macro(cite) \
    macro(code) \
    macro(col) \
    macro(colgroup) \
    macro(dd) \
    macro(del) \
    macro(dfn) \
    macro(dir) \
    macro(div) \
    macro(dl) \
    macro(dt) \
    macro(em) \
    macro(embed) \
    macro(fieldset) \
    macro(font) \
    macro(form) \
    macro(frame) \
    macro(frameset) \
    macro(head) \
    macro(h1) \
    macro(h2) \
    macro(h3) \
    macro(h4) \
    macro(h5) \
    macro(h6) \
    macro(hr) \
    macro(html) \
    macro(i) \
    macro(iframe) \
    macro(image) \
    macro(img) \
    macro(input) \
    macro(ins) \
    macro(isindex) \
    macro(kbd) \
    macro(keygen) \
    macro(label) \
    macro(layer) \
    macro(legend) \
    macro(li) \
    macro(link) \
    macro(map) \
    macro(marquee) \
    macro(menu) \
    macro(meta) \
    macro(nobr) \
    macro(noembed) \
    macro(noframes) \
    macro(nolayer) \
    macro(noscript) \
    macro(object) \
    macro(ol) \
    macro(optgroup) \
    macro(option) \
    macro(p) \
    macro(param) \
    macro(plaintext) \
    macro(pre) \
    macro(q) \
    macro(s) \
    macro(samp) \
    macro(script) \
    macro(select) \
    macro(small) \
    macro(span) \
    macro(strike) \
    macro(strong) \
    macro(style) \
    macro(sub) \
    macro(sup) \
    macro(table) \
    macro(tbody) \
    macro(td) \
    macro(textarea) \
    macro(tfoot) \
    macro(th) \
    macro(thead) \
    macro(title) \
    macro(tr) \
    macro(tt) \
    macro(u) \
    macro(ul) \
    macro(var) \
    macro(wbr) \
    macro(xmp) \
// end of macro

#define DOM_HTMLNAMES_FOR_EACH_ATTR(macro) \
    macro(abbr) \
    macro(accept_charset) \
    macro(accept) \
    macro(accesskey) \
    macro(action) \
    macro(align) \
    macro(alink) \
    macro(alt) \
    macro(archive) \
    macro(autocomplete) \
    macro(autosave) \
    macro(axis) \
    macro(background) \
    macro(behavior) \
    macro(bgcolor) \
    macro(bgproperties) \
    macro(border) \
    macro(bordercolor) \
    macro(cellpadding) \
    macro(cellspacing) \
    macro(char) \
    macro(challenge) \
    macro(charoff) \
    macro(charset) \
    macro(checked) \
    macro(cellborder) \
    macro(cite) \
    macro(class) \
    macro(classid) \
    macro(clear) \
    macro(code) \
    macro(codebase) \
    macro(codetype) \
    macro(color) \
    macro(cols) \
    macro(colspan) \
    macro(compact) \
    macro(composite) \
    macro(content) \
    macro(contenteditable) \
    macro(coords) \
    macro(data) \
    macro(datetime) \
    macro(declare) \
    macro(defer) \
    macro(dir) \
    macro(direction) \
    macro(disabled) \
    macro(enctype) \
    macro(face) \
    macro(for) \
    macro(frame) \
    macro(frameborder) \
    macro(headers) \
    macro(height) \
    macro(hidden) \
    macro(href) \
    macro(hreflang) \
    macro(hspace) \
    macro(http_equiv) \
    macro(id) \
    macro(incremental) \
    macro(ismap) \
    macro(keytype) \
    macro(label) \
    macro(lang) \
    macro(language) \
    macro(left) \
    macro(leftmargin) \
    macro(link) \
    macro(longdesc) \
    macro(loop) \
    macro(marginheight) \
    macro(marginwidth) \
    macro(max) \
    macro(maxlength) \
    macro(mayscript) \
    macro(media) \
    macro(method) \
    macro(min) \
    macro(multiple) \
    macro(name) \
    macro(nohref) \
    macro(noresize) \
    macro(noshade) \
    macro(nowrap) \
    macro(object) \
    macro(onabort) \
    macro(onbeforecopy) \
    macro(onbeforecut) \
    macro(onbeforepaste) \
    macro(onbeforeunload) \
    macro(onblur) \
    macro(onchange) \
    macro(onclick) \
    macro(oncontextmenu) \
    macro(oncopy) \
    macro(oncut) \
    macro(ondblclick) \
    macro(ondrag) \
    macro(ondragend) \
    macro(ondragenter) \
    macro(ondragleave) \
    macro(ondragover) \
    macro(ondragstart) \
    macro(ondrop) \
    macro(onerror) \
    macro(onfocus) \
    macro(oninput) \
    macro(onkeydown) \
    macro(onkeypress) \
    macro(onkeyup) \
    macro(onload) \
    macro(onmousedown) \
    macro(onmousemove) \
    macro(onmouseout) \
    macro(onmouseover) \
    macro(onmouseup) \
    macro(onmousewheel) \
    macro(onpaste) \
    macro(onreset) \
    macro(onresize) \
    macro(onscroll) \
    macro(onsearch) \
    macro(onselect) \
    macro(onselectstart) \
    macro(onsubmit) \
    macro(onunload) \
    macro(pagex) \
    macro(pagey) \
    macro(placeholder) \
    macro(plain) \
    macro(pluginpage) \
    macro(pluginspage) \
    macro(pluginurl) \
    macro(precision) \
    macro(profile) \
    macro(prompt) \
    macro(readonly) \
    macro(rel) \
    macro(results) \
    macro(rev) \
    macro(rows) \
    macro(rowspan) \
    macro(rules) \
    macro(scheme) \
    macro(scope) \
    macro(scrollamount) \
    macro(scrolldelay) \
    macro(scrolling) \
    macro(selected) \
    macro(shape) \
    macro(size) \
    macro(span) \
    macro(src) \
    macro(standby) \
    macro(start) \
    macro(style) \
    macro(summary) \
    macro(tabindex) \
    macro(tableborder) \
    macro(target) \
    macro(text) \
    macro(title) \
    macro(top) \
    macro(topmargin) \
    macro(truespeed) \
    macro(type) \
    macro(usemap) \
    macro(valign) \
    macro(value) \
    macro(valuetype) \
    macro(version) \
    macro(vlink) \
    macro(vspace) \
    macro(width) \
    macro(wrap) \
// end of macro

#if !DOM_HTMLNAMES_HIDE_GLOBALS
    // Namespace
    extern const AtomicString xhtmlNamespaceURI;

    // Tags
    #define DOM_HTMLNAMES_DEFINE_TAG_GLOBAL(name) extern const QualifiedName name##Tag;
    DOM_HTMLNAMES_FOR_EACH_TAG(DOM_HTMLNAMES_DEFINE_TAG_GLOBAL)
    #undef DOM_HTMLNAMES_DEFINE_TAG_GLOBAL

    // Attributes
    #define DOM_HTMLNAMES_DEFINE_ATTR_GLOBAL(name) extern const QualifiedName name##Attr;
    DOM_HTMLNAMES_FOR_EACH_ATTR(DOM_HTMLNAMES_DEFINE_ATTR_GLOBAL)
    #undef DOM_HTMLNAMES_DEFINE_ATTR_GLOBAL
#endif

    void init();
} }

#endif
