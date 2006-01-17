/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
#ifndef css_helper_h
#define css_helper_h

#include <qcolor.h>
#include <qfont.h>

#include "dom/dom_string.h"

class QPaintDeviceMetrics;
class KHTMLSettings;

namespace DOM
{
    class CSSPrimitiveValueImpl;
};

namespace khtml
{
    class RenderStyle;

    /*
     * mostly just removes the url("...") brace
     */
    DOM::DOMString parseURL(const DOM::DOMString &url);

};



#endif
