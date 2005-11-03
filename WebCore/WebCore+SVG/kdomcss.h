/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOMCSS_H
#define KDOMCSS_H

// General namespace specific definitions
namespace KDOM
{
    enum RuleType
    {
        UNKNOWN_RULE        = 0,
        STYLE_RULE            = 1,
        CHARSET_RULE        = 2,
        IMPORT_RULE            = 3,
        MEDIA_RULE            = 4,
        FONT_FACE_RULE        = 5,
        PAGE_RULE            = 6
    };

    enum ValueTypes /* Was also named UnitTypes, in the css.idl! Name clash! */
    {
        CSS_INHERIT            = 0,
        CSS_PRIMITIVE_VALUE    = 1,
        CSS_VALUE_LIST        = 2,
        CSS_CUSTOM            = 3,
        CSS_INITIAL            = 4
    };
};

#endif

// vim:ts=4:noet
