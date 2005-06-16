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

#include "xml/dom_elementimpl.h"

namespace DOM
{

class QualifiedName;

class HTMLNames
{
public:
    // The namespace URI.
    static const AtomicString& xhtmlNamespaceURI();
    
    // Full tag names.
    static const QualifiedName& a();
    static const QualifiedName& address();
    static const QualifiedName& applet();
    static const QualifiedName& area();
    static const QualifiedName& b();
    static const QualifiedName& base();
    static const QualifiedName& basefont();
    static const QualifiedName& big();
    static const QualifiedName& blockquote();
    static const QualifiedName& body();
    static const QualifiedName& br();
    static const QualifiedName& button();
    static const QualifiedName& canvas();
    static const QualifiedName& caption();
    static const QualifiedName& center();
    static const QualifiedName& code();
    static const QualifiedName& col();
    static const QualifiedName& colgroup();
    static const QualifiedName& dd();
    static const QualifiedName& del();
    static const QualifiedName& dfn();
    static const QualifiedName& dir();
    static const QualifiedName& div();
    static const QualifiedName& dl();
    static const QualifiedName& dt();
    static const QualifiedName& em();
    static const QualifiedName& embed();
    static const QualifiedName& fieldset();
    static const QualifiedName& font();
    static const QualifiedName& form();
    static const QualifiedName& frame();
    static const QualifiedName& frameset();
    static const QualifiedName& head();
    static const QualifiedName& h1();
    static const QualifiedName& h2();
    static const QualifiedName& h3();
    static const QualifiedName& h4();
    static const QualifiedName& h5();
    static const QualifiedName& h6();
    static const QualifiedName& hr();
    static const QualifiedName& html();
    static const QualifiedName& i();
    static const QualifiedName& iframe();
    static const QualifiedName& img();
    static const QualifiedName& input();
    static const QualifiedName& ins();
    static const QualifiedName& isindex();
    static const QualifiedName& kbd();
    static const QualifiedName& keygen();
    static const QualifiedName& label();
    static const QualifiedName& legend();
    static const QualifiedName& li();
    static const QualifiedName& link();
    static const QualifiedName& map();
    static const QualifiedName& marquee();
    static const QualifiedName& menu();
    static const QualifiedName& meta();
    static const QualifiedName& noembed();
    static const QualifiedName& noframes();
    static const QualifiedName& noscript();
    static const QualifiedName& object();
    static const QualifiedName& ol();
    static const QualifiedName& optgroup();
    static const QualifiedName& option();
    static const QualifiedName& p();
    static const QualifiedName& param();
    static const QualifiedName& pre();
    static const QualifiedName& q();
    static const QualifiedName& s();
    static const QualifiedName& samp();
    static const QualifiedName& script();
    static const QualifiedName& select();
    static const QualifiedName& small();
    static const QualifiedName& strike();
    static const QualifiedName& strong();
    static const QualifiedName& style();
    static const QualifiedName& table();
    static const QualifiedName& tbody();
    static const QualifiedName& td();
    static const QualifiedName& textarea();
    static const QualifiedName& tfoot();
    static const QualifiedName& th();
    static const QualifiedName& thead();
    static const QualifiedName& title();
    static const QualifiedName& tr();
    static const QualifiedName& tt();
    static const QualifiedName& u();
    static const QualifiedName& ul();
    static const QualifiedName& var();
    static const QualifiedName& xmp();

    // Attribute names.
};

}

#endif
