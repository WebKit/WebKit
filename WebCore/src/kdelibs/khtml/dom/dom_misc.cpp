/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
 * $Id$
 */

#include "dom_misc.h"
using namespace DOM;
#ifdef APPLE_CHANGES
#include <stdio.h>
#endif /* APPLE_CHANGES */

DomShared::~DomShared()
{
  // deliberately left blank
}


bool DomShared::deleteMe()
{
    return !_ref;
}


#ifdef APPLE_CHANGES
void *DomShared::instanceToCheck;

void DomShared::ref()
{
    if (((void *)this) == instanceToCheck){
        printf ("0x%08x incrementing ref %d\n", (unsigned int)this, _ref);
    }
    _ref++;
}

void DomShared::deref() 
{
    if (((void *)this) == instanceToCheck){
        fprintf (stdout, "0x%08x decrementing ref %d\n", (unsigned int)this, _ref);
    }
    if(_ref)
        _ref--; 
    if(!_ref && deleteMe())
        delete this; 
}
#endif /* APPLE_CHANGES */
