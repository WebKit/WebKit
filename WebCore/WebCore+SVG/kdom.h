/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.

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

#ifndef KDOM_H
#define KDOM_H
#if SVG_SUPPORT

#include "DeprecatedString.h"
#include "PlatformString.h"

/**
 * @short General namespace specific definitions.
 */
namespace WebCore
{
    /**
     * All DOM constants
     */
    enum DocumentPosition
    {
        DOCUMENT_POSITION_DISCONNECTED              = 0x01,
        DOCUMENT_POSITION_PRECEDING                 = 0x02,
        DOCUMENT_POSITION_FOLLOWING                 = 0x04,
        DOCUMENT_POSITION_CONTAINS                  = 0x08,
        DOCUMENT_POSITION_CONTAINED_BY              = 0x10,
        DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC   = 0x20
    };

    enum DerivationMethods
    {
        DERIVATION_RESTRICTION         = 0x00000001,
        DERIVATION_EXTENSION           = 0x00000002,
        DERIVATION_UNION               = 0x00000004,
        DERIVATION_LIST                = 0x00000008
    };
    
    enum
    {
        FEATURE_CANONICAL_FORM                  = 0x000001,
        FEATURE_CDATA_SECTIONS                  = 0x000002,
        FEATURE_COMMENTS                        = 0x000004,
        FEATURE_DATATYPE_NORMALIZATION          = 0x000008,
        FEATURE_ENTITIES                        = 0x000010,
        FEATURE_WELL_FORMED                     = 0x000020,
        FEATURE_NAMESPACES                      = 0x000040,
        FEATURE_NAMESPACE_DECLARATIONS          = 0x000080,
        FEATURE_NORMALIZE_CHARACTERS            = 0x000100,
        FEATURE_SPLIT_CDATA_SECTIONS            = 0x000200,
        FEATURE_VALIDATE                        = 0x000400,
        FEATURE_VALIDATE_IF_SCHEMA              = 0x000800,
        FEATURE_WHITESPACE_IN_ELEMENT_CONTENT   = 0x001000,
        FEATURE_CHECK_CHARACTER_NORMALIZATION   = 0x002000,

        // LSParser specific
        FEATURE_CHARSET_OVERRIDES_XML_ENCODING   = 0x004000,
        FEATURE_DISALLOW_DOCTYPE                 = 0x008000,
        FEATURE_IGNORE_UNKNOWN_CD                = 0x010000,
        FEATURE_SUPPORTED_MEDIA_TYPE_ONLY        = 0x020000,

        // LS Serializer specific
        FEATURE_DISCARD_DEFAULT_CONTENT            = 0x040000,
        FEATURE_FORMAT_PRETTY_PRINT                = 0x080000,
        FEATURE_XML_DECLARATION                    = 0x100000
    };
}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
