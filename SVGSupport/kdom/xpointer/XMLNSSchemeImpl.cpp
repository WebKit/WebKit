/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich     <frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include <qmap.h>

#include <kdom/Helper.h>
#include <kdom/Shared.h>

#include "DOMString.h"
#include "PointerPartImpl.h"
#include "XMLNSSchemeImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

XMLNSSchemeImpl::XMLNSSchemeImpl(DOMStringImpl *schemeDataImpl, NBCImpl *nbc) : NBCImpl(nbc)
{
    DOMString schemeData(schemeDataImpl);

    /* Note:
     * "[...] if scheme data in a pointer part with the xmlns() scheme does not conform to 
     * the syntax defined in this section the pointer part does not contribute an entry to 
     * the namespace binding context."
     *
     * It doesn't say syntax errors should be flagged as errors, only not added.
     */
    const int delimiter = schemeData.find('=');
    const unsigned int length = schemeData.length();

    if(delimiter == -1)
        return;

    const QString qSchemeData = schemeData.string();

    DOMString prefix = qSchemeData.mid(0, delimiter).stripWhiteSpace();
    DOMString ns = qSchemeData.mid(delimiter + 1, length).stripWhiteSpace();

    if(!Helper::IsValidNCName(prefix.handle()))
        return;

    /* The ns string is simply xpointer-escaped unicode chars, and any wrong escaping would
     * be caught by the builder, so no syntax checking needed. */

    /* NBCImpl does the xml/xmlns checks. */
    
    addMapping(prefix.handle(), ns.handle());
}

XMLNSSchemeImpl::~XMLNSSchemeImpl()
{
}

// vim:ts=4:noet
