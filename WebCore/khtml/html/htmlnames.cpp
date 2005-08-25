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

#define DOM_HTMLNAMES_HIDE_GLOBALS 1

#include "htmlnames.h"

namespace DOM { namespace HTMLNames {

// Define a properly-sized array of pointers to avoid static initialization.
// Use an array of pointers instead of an array of char in case there is some alignment issue.

#define DEFINE_UNINITIALIZED_GLOBAL(type, name) void *name[(sizeof(type) + sizeof(void *) - 1) / sizeof(void *)];

DEFINE_UNINITIALIZED_GLOBAL(AtomicString, xhtmlNamespaceURI)

#define DEFINE_TAG_GLOBAL(name) DEFINE_UNINITIALIZED_GLOBAL(QualifiedName, name##Tag)
DOM_HTMLNAMES_FOR_EACH_TAG(DEFINE_TAG_GLOBAL)

#define DEFINE_ATTR_GLOBAL(name) DEFINE_UNINITIALIZED_GLOBAL(QualifiedName, name##Attr)
DOM_HTMLNAMES_FOR_EACH_ATTR(DEFINE_ATTR_GLOBAL)

void init()
{
    static bool initialized;
    if (!initialized) {
        // Use placement new to initialize the globals.

        AtomicString xhtmlNS("http://www.w3.org/1999/xhtml");

        // Namespace
        new (&xhtmlNamespaceURI) AtomicString(xhtmlNS);

        // Tags
        #define INITIALIZE_TAG_GLOBAL(name) new (&name##Tag) QualifiedName(nullAtom, #name, xhtmlNS);
        DOM_HTMLNAMES_FOR_EACH_TAG(INITIALIZE_TAG_GLOBAL)

        // Attributes
        // Need a little trick to handle accept_charset and http_equiv, which need "_" characters.
        #define DEFINE_ATTR_STRING(name) const char *name##AttrString = #name;
        DOM_HTMLNAMES_FOR_EACH_ATTR(DEFINE_ATTR_STRING)
        accept_charsetAttrString = "accept-charset";
        http_equivAttrString = "http-equiv";
        #define INITIALIZE_ATTR_GLOBAL(name) new (&name##Attr) QualifiedName(nullAtom, name##AttrString, nullAtom);
        DOM_HTMLNAMES_FOR_EACH_ATTR(INITIALIZE_ATTR_GLOBAL)

        initialized = true;
    }
}

} }
