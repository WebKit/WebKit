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

#include "htmlnames.h"
#include "dom_qname.h"

namespace DOM {

const AtomicString& HTMLNames::xhtmlNamespaceURI()
{
    static AtomicString xhtmlNS("http://www.w3.org/1999/xhtml");
    return xhtmlNS;
}


// Tag names.
const QualifiedName& HTMLNames::a()
{
    static QualifiedName a(nullAtom, "a", xhtmlNamespaceURI());
    return a;
}

const QualifiedName& HTMLNames::address()
{
    static QualifiedName address(nullAtom, "address", xhtmlNamespaceURI());
    return address;
}

const QualifiedName& HTMLNames::applet()
{
    static QualifiedName applet(nullAtom, "applet", xhtmlNamespaceURI());
    return applet;
}

const QualifiedName& HTMLNames::area()
{
    static QualifiedName area(nullAtom, "area", xhtmlNamespaceURI());
    return area;
}

const QualifiedName& HTMLNames::b()
{
    static QualifiedName b(nullAtom, "b", xhtmlNamespaceURI());
    return b;
}

const QualifiedName& HTMLNames::base()
{
    static QualifiedName base(nullAtom, "base", xhtmlNamespaceURI());
    return base;
}

const QualifiedName& HTMLNames::basefont()
{
    static QualifiedName basefont(nullAtom, "basefont", xhtmlNamespaceURI());
    return basefont;
}

const QualifiedName& HTMLNames::big()
{
    static QualifiedName big(nullAtom, "big", xhtmlNamespaceURI());
    return big;
}

const QualifiedName& HTMLNames::blockquote()
{
    static QualifiedName blockquote(nullAtom, "blockquote", xhtmlNamespaceURI());
    return blockquote;
}

const QualifiedName& HTMLNames::body()
{
    static QualifiedName body(nullAtom, "body", xhtmlNamespaceURI());
    return body;
}

const QualifiedName& HTMLNames::br()
{
    static QualifiedName br(nullAtom, "br", xhtmlNamespaceURI());
    return br;
}

const QualifiedName& HTMLNames::button()
{
    static QualifiedName button(nullAtom, "button", xhtmlNamespaceURI());
    return button;
}

const QualifiedName& HTMLNames::canvas()
{
    static QualifiedName canvas(nullAtom, "canvas", xhtmlNamespaceURI());
    return canvas;
}

const QualifiedName& HTMLNames::caption()
{
    static QualifiedName caption(nullAtom, "caption", xhtmlNamespaceURI());
    return caption;
}

const QualifiedName& HTMLNames::center()
{
    static QualifiedName center(nullAtom, "center", xhtmlNamespaceURI());
    return center;
}

const QualifiedName& HTMLNames::code()
{
    static QualifiedName code(nullAtom, "code", xhtmlNamespaceURI());
    return code;
}

const QualifiedName& HTMLNames::col()
{
    static QualifiedName col(nullAtom, "col", xhtmlNamespaceURI());
    return col;
}

const QualifiedName& HTMLNames::colgroup()
{
    static QualifiedName colgroup(nullAtom, "colgroup", xhtmlNamespaceURI());
    return colgroup;
}

const QualifiedName& HTMLNames::dd()
{
    static QualifiedName dd(nullAtom, "dd", xhtmlNamespaceURI());
    return dd;
}

const QualifiedName& HTMLNames::del()
{
    static QualifiedName del(nullAtom, "del", xhtmlNamespaceURI());
    return del;
}

const QualifiedName& HTMLNames::dfn()
{
    static QualifiedName dfn(nullAtom, "dfn", xhtmlNamespaceURI());
    return dfn;
}

const QualifiedName& HTMLNames::dir()
{
    static QualifiedName dir(nullAtom, "dir", xhtmlNamespaceURI());
    return dir;
}

const QualifiedName& HTMLNames::div()
{
    static QualifiedName div(nullAtom, "div", xhtmlNamespaceURI());
    return div;
}

const QualifiedName& HTMLNames::dl()
{
    static QualifiedName dl(nullAtom, "dl", xhtmlNamespaceURI());
    return dl;
}

const QualifiedName& HTMLNames::dt()
{
    static QualifiedName dt(nullAtom, "dt", xhtmlNamespaceURI());
    return dt;
}

const QualifiedName& HTMLNames::em()
{
    static QualifiedName em(nullAtom, "em", xhtmlNamespaceURI());
    return em;
}

const QualifiedName& HTMLNames::embed()
{
    static QualifiedName embed(nullAtom, "embed", xhtmlNamespaceURI());
    return embed;
}

const QualifiedName& HTMLNames::fieldset()
{
    static QualifiedName fieldset(nullAtom, "fieldset", xhtmlNamespaceURI());
    return fieldset;
}

const QualifiedName& HTMLNames::font()
{
    static QualifiedName font(nullAtom, "font", xhtmlNamespaceURI());
    return font;
}

const QualifiedName& HTMLNames::form()
{
    static QualifiedName form(nullAtom, "form", xhtmlNamespaceURI());
    return form;
}

const QualifiedName& HTMLNames::frame()
{
    static QualifiedName frame(nullAtom, "frame", xhtmlNamespaceURI());
    return frame;
}

const QualifiedName& HTMLNames::frameset()
{
    static QualifiedName frameset(nullAtom, "frameset", xhtmlNamespaceURI());
    return frameset;
}

const QualifiedName& HTMLNames::head()
{
    static QualifiedName head(nullAtom, "head", xhtmlNamespaceURI());
    return head;
}

const QualifiedName& HTMLNames::h1()
{
    static QualifiedName h1(nullAtom, "h1", xhtmlNamespaceURI());
    return h1;
}

const QualifiedName& HTMLNames::h2()
{
    static QualifiedName h2(nullAtom, "h2", xhtmlNamespaceURI());
    return h2;
}

const QualifiedName& HTMLNames::h3()
{
    static QualifiedName h3(nullAtom, "h3", xhtmlNamespaceURI());
    return h3;
}

const QualifiedName& HTMLNames::h4()
{
    static QualifiedName h4(nullAtom, "h4", xhtmlNamespaceURI());
    return h4;
}

const QualifiedName& HTMLNames::h5()
{
    static QualifiedName h5(nullAtom, "h5", xhtmlNamespaceURI());
    return h5;
}

const QualifiedName& HTMLNames::h6()
{
    static QualifiedName h6(nullAtom, "h6", xhtmlNamespaceURI());
    return h6;
}

const QualifiedName& HTMLNames::hr()
{
    static QualifiedName hr(nullAtom, "hr", xhtmlNamespaceURI());
    return hr;
}

const QualifiedName& HTMLNames::html()
{
    static QualifiedName html(nullAtom, "html", xhtmlNamespaceURI());
    return html;
}

const QualifiedName& HTMLNames::i()
{
    static QualifiedName i(nullAtom, "i", xhtmlNamespaceURI());
    return i;
}

const QualifiedName& HTMLNames::iframe()
{
    static QualifiedName iframe(nullAtom, "iframe", xhtmlNamespaceURI());
    return iframe;
}

const QualifiedName& HTMLNames::img()
{
    static QualifiedName img(nullAtom, "img", xhtmlNamespaceURI());
    return img;
}

const QualifiedName& HTMLNames::input()
{
    static QualifiedName input(nullAtom, "input", xhtmlNamespaceURI());
    return input;
}

const QualifiedName& HTMLNames::ins()
{
    static QualifiedName ins(nullAtom, "ins", xhtmlNamespaceURI());
    return ins;
}

const QualifiedName& HTMLNames::isindex()
{
    static QualifiedName isindex(nullAtom, "isindex", xhtmlNamespaceURI());
    return isindex;
}

const QualifiedName& HTMLNames::kbd()
{
    static QualifiedName kbd(nullAtom, "kbd", xhtmlNamespaceURI());
    return kbd;
}

const QualifiedName& HTMLNames::keygen()
{
    static QualifiedName keygen(nullAtom, "keygen", xhtmlNamespaceURI());
    return keygen;
}

const QualifiedName& HTMLNames::label()
{
    static QualifiedName label(nullAtom, "label", xhtmlNamespaceURI());
    return label;
}

const QualifiedName& HTMLNames::legend()
{
    static QualifiedName legend(nullAtom, "legend", xhtmlNamespaceURI());
    return legend;
}

const QualifiedName& HTMLNames::li()
{
    static QualifiedName li(nullAtom, "li", xhtmlNamespaceURI());
    return li;
}

const QualifiedName& HTMLNames::link()
{
    static QualifiedName link(nullAtom, "link", xhtmlNamespaceURI());
    return link;
}

const QualifiedName& HTMLNames::map()
{
    static QualifiedName map(nullAtom, "map", xhtmlNamespaceURI());
    return map;
}

const QualifiedName& HTMLNames::marquee()
{
    static QualifiedName marquee(nullAtom, "marquee", xhtmlNamespaceURI());
    return marquee;
}

const QualifiedName& HTMLNames::menu()
{
    static QualifiedName menu(nullAtom, "menu", xhtmlNamespaceURI());
    return menu;
}

const QualifiedName& HTMLNames::meta()
{
    static QualifiedName meta(nullAtom, "meta", xhtmlNamespaceURI());
    return meta;
}

const QualifiedName& HTMLNames::noembed()
{
    static QualifiedName noembed(nullAtom, "noembed", xhtmlNamespaceURI());
    return noembed;
}

const QualifiedName& HTMLNames::noframes()
{
    static QualifiedName noframes(nullAtom, "noframes", xhtmlNamespaceURI());
    return noframes;
}

const QualifiedName& HTMLNames::noscript()
{
    static QualifiedName noscript(nullAtom, "noscript", xhtmlNamespaceURI());
    return noscript;
}

const QualifiedName& HTMLNames::object()
{
    static QualifiedName object(nullAtom, "object", xhtmlNamespaceURI());
    return object;
}

const QualifiedName& HTMLNames::ol()
{
    static QualifiedName ol(nullAtom, "ol", xhtmlNamespaceURI());
    return ol;
}

const QualifiedName& HTMLNames::optgroup()
{
    static QualifiedName optgroup(nullAtom, "optgroup", xhtmlNamespaceURI());
    return optgroup;
}

const QualifiedName& HTMLNames::p()
{
    static QualifiedName p(nullAtom, "p", xhtmlNamespaceURI());
    return p;
}

const QualifiedName& HTMLNames::param()
{
    static QualifiedName param(nullAtom, "param", xhtmlNamespaceURI());
    return param;
}

const QualifiedName& HTMLNames::pre()
{
    static QualifiedName pre(nullAtom, "pre", xhtmlNamespaceURI());
    return pre;
}

const QualifiedName& HTMLNames::q()
{
    static QualifiedName q(nullAtom, "q", xhtmlNamespaceURI());
    return q;
}

const QualifiedName& HTMLNames::s()
{
    static QualifiedName s(nullAtom, "s", xhtmlNamespaceURI());
    return s;
}

const QualifiedName& HTMLNames::samp()
{
    static QualifiedName samp(nullAtom, "samp", xhtmlNamespaceURI());
    return samp;
}

const QualifiedName& HTMLNames::script()
{
    static QualifiedName script(nullAtom, "script", xhtmlNamespaceURI());
    return script;
}

const QualifiedName& HTMLNames::select()
{
    static QualifiedName select(nullAtom, "select", xhtmlNamespaceURI());
    return select;
}

const QualifiedName& HTMLNames::small()
{
    static QualifiedName small(nullAtom, "small", xhtmlNamespaceURI());
    return small;
}

const QualifiedName& HTMLNames::strike()
{
    static QualifiedName strike(nullAtom, "strike", xhtmlNamespaceURI());
    return strike;
}

const QualifiedName& HTMLNames::strong()
{
    static QualifiedName strong(nullAtom, "strong", xhtmlNamespaceURI());
    return strong;
}

const QualifiedName& HTMLNames::style()
{
    static QualifiedName style(nullAtom, "style", xhtmlNamespaceURI());
    return style;
}

const QualifiedName& HTMLNames::table()
{
    static QualifiedName table(nullAtom, "table", xhtmlNamespaceURI());
    return table;
}

const QualifiedName& HTMLNames::tbody()
{
    static QualifiedName tbody(nullAtom, "tbody", xhtmlNamespaceURI());
    return tbody;
}

const QualifiedName& HTMLNames::td()
{
    static QualifiedName td(nullAtom, "td", xhtmlNamespaceURI());
    return td;
}

const QualifiedName& HTMLNames::textarea()
{
    static QualifiedName textarea(nullAtom, "textarea", xhtmlNamespaceURI());
    return textarea;
}

const QualifiedName& HTMLNames::tfoot()
{
    static QualifiedName tfoot(nullAtom, "tfoot", xhtmlNamespaceURI());
    return tfoot;
}

const QualifiedName& HTMLNames::th()
{
    static QualifiedName th(nullAtom, "th", xhtmlNamespaceURI());
    return th;
}

const QualifiedName& HTMLNames::thead()
{
    static QualifiedName thead(nullAtom, "thead", xhtmlNamespaceURI());
    return thead;
}

const QualifiedName& HTMLNames::title()
{
    static QualifiedName title(nullAtom, "title", xhtmlNamespaceURI());
    return title;
}

const QualifiedName& HTMLNames::tr()
{
    static QualifiedName tr(nullAtom, "tr", xhtmlNamespaceURI());
    return tr;
}

const QualifiedName& HTMLNames::tt()
{
    static QualifiedName tt(nullAtom, "tt", xhtmlNamespaceURI());
    return tt;
}

const QualifiedName& HTMLNames::u()
{
    static QualifiedName u(nullAtom, "u", xhtmlNamespaceURI());
    return u;
}

const QualifiedName& HTMLNames::ul()
{
    static QualifiedName ul(nullAtom, "ul", xhtmlNamespaceURI());
    return ul;
}

const QualifiedName& HTMLNames::var()
{
    static QualifiedName var(nullAtom, "var", xhtmlNamespaceURI());
    return var;
}

const QualifiedName& HTMLNames::xmp()
{
    static QualifiedName xmp(nullAtom, "xmp", xhtmlNamespaceURI());
    return xmp;
}

}
