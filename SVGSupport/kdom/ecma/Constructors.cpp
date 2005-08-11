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

#include "kdom.h"
#include "kdomcss.h"
#include "kdomrange.h"
#include "kdomevents.h"
#include "Constructors.h"
#include "Constructors.lut.h"
using namespace KDOM;

// EcmaScript Constructors (enumerations)

/*
@begin NodeConstructor::s_hashTable 13
 ELEMENT_NODE					KDOM::ELEMENT_NODE					DontDelete|ReadOnly
 ATTRIBUTE_NODE					KDOM::ATTRIBUTE_NODE				DontDelete|ReadOnly
 TEXT_NODE						KDOM::TEXT_NODE						DontDelete|ReadOnly
 CDATA_SECTION_NODE				KDOM::CDATA_SECTION_NODE			DontDelete|ReadOnly
 ENTITY_REFERENCE_NODE			KDOM::ENTITY_REFERENCE_NODE			DontDelete|ReadOnly
 ENTITY_NODE					KDOM::ENTITY_NODE					DontDelete|ReadOnly
 PROCESSING_INSTRUCTION_NODE	KDOM::PROCESSING_INSTRUCTION_NODE	DontDelete|ReadOnly
 COMMENT_NODE					KDOM::COMMENT_NODE					DontDelete|ReadOnly
 DOCUMENT_NODE					KDOM::DOCUMENT_NODE					DontDelete|ReadOnly
 DOCUMENT_TYPE_NODE				KDOM::DOCUMENT_TYPE_NODE			DontDelete|ReadOnly
 DOCUMENT_FRAGMENT_NODE			KDOM::DOCUMENT_FRAGMENT_NODE		DontDelete|ReadOnly
 NOTATION_NODE					KDOM::NOTATION_NODE					DontDelete|ReadOnly
@end
*/

KDOM_IMPLEMENT_CONSTRUCTOR(NodeConstructor, "Node")

/*
@begin DOMExceptionConstructor::s_hashTable 17
 INDEX_SIZE_ERR					KDOM::INDEX_SIZE_ERR				DontDelete|ReadOnly
 DOMSTRING_SIZE_ERR				KDOM::DOMSTRING_SIZE_ERR			DontDelete|ReadOnly
 HIERARCHY_REQUEST_ERR			KDOM::HIERARCHY_REQUEST_ERR			DontDelete|ReadOnly
 WRONG_DOCUMENT_ERR				KDOM::WRONG_DOCUMENT_ERR			DontDelete|ReadOnly
 INVALID_CHARACTER_ERR			KDOM::INVALID_CHARACTER_ERR			DontDelete|ReadOnly
 NO_DATA_ALLOWED_ERR			KDOM::NO_DATA_ALLOWED_ERR			DontDelete|ReadOnly
 NO_MODIFICATION_ALLOWED_ERR	KDOM::NO_MODIFICATION_ALLOWED_ERR	DontDelete|ReadOnly
 NOT_FOUND_ERR					KDOM::NOT_FOUND_ERR					DontDelete|ReadOnly
 NOT_SUPPORTED_ERR				KDOM::NOT_SUPPORTED_ERR				DontDelete|ReadOnly
 INUSE_ATTRIBUTE_ERR			KDOM::INUSE_ATTRIBUTE_ERR			DontDelete|ReadOnly
 INVALID_STATE_ERR				KDOM::INVALID_STATE_ERR				DontDelete|ReadOnly
 SYNTAX_ERR						KDOM::SYNTAX_ERR					DontDelete|ReadOnly
 INVALID_MODIFICATION_ERR		KDOM::INVALID_MODIFICATION_ERR		DontDelete|ReadOnly
 NAMESPACE_ERR					KDOM::NAMESPACE_ERR					DontDelete|ReadOnly
 INVALID_ACCESS_ERR				KDOM::INVALID_ACCESS_ERR			DontDelete|ReadOnly
@end
*/

KDOM_IMPLEMENT_CONSTRUCTOR(DOMExceptionConstructor, "DOMException")

/*
@begin CSSRuleConstructor::s_hashTable 7
 UNKNOWN_RULE	KDOM::UNKNOWN_RULE		DontDelete|ReadOnly
 STYLE_RULE		KDOM::STYLE_RULE		DontDelete|ReadOnly
 CHARSET_RULE	KDOM::CHARSET_RULE		DontDelete|ReadOnly
 IMPORT_RULE	KDOM::IMPORT_RULE		DontDelete|ReadOnly
 MEDIA_RULE		KDOM::MEDIA_RULE		DontDelete|ReadOnly
 FONT_FACE_RULE	KDOM::FONT_FACE_RULE	DontDelete|ReadOnly
 PAGE_RULE		KDOM::PAGE_RULE			DontDelete|ReadOnly
@end
*/

KDOM_IMPLEMENT_CONSTRUCTOR(CSSRuleConstructor, "CSSRule")

/*
@begin CSSValueConstructor::s_hashTable 5
 CSS_INHERIT			KDOM::CSS_INHERIT			DontDelete|ReadOnly
 CSS_PRIMITIVE_VALUE	KDOM::CSS_PRIMITIVE_VALUE	DontDelete|ReadOnly
 CSS_VALUE_LIST			KDOM::CSS_VALUE_LIST		DontDelete|ReadOnly
 CSS_CUSTOM				KDOM::CSS_CUSTOM			DontDelete|ReadOnly
@end
*/

KDOM_IMPLEMENT_CONSTRUCTOR(CSSValueConstructor, "CSSValue")

/*
@begin CSSPrimitiveValueConstructor::s_hashTable 27
 CSS_UNKNOWN           KDOM::CSS_UNKNOWN     DontDelete|ReadOnly
 CSS_NUMBER            KDOM::CSS_NUMBER      DontDelete|ReadOnly
 CSS_PERCENTAGE        KDOM::CSS_PERCENTAGE  DontDelete|ReadOnly
 CSS_EMS               KDOM::CSS_EMS         DontDelete|ReadOnly
 CSS_EXS               KDOM::CSS_EXS         DontDelete|ReadOnly
 CSS_PX                KDOM::CSS_PX          DontDelete|ReadOnly
 CSS_CM                KDOM::CSS_CM          DontDelete|ReadOnly
 CSS_MM                KDOM::CSS_MM          DontDelete|ReadOnly
 CSS_IN                KDOM::CSS_IN          DontDelete|ReadOnly
 CSS_PT                KDOM::CSS_PT          DontDelete|ReadOnly
 CSS_PC                KDOM::CSS_PC          DontDelete|ReadOnly
 CSS_DEG               KDOM::CSS_DEG         DontDelete|ReadOnly
 CSS_RAD               KDOM::CSS_RAD         DontDelete|ReadOnly
 CSS_GRAD              KDOM::CSS_GRAD        DontDelete|ReadOnly
 CSS_MS                KDOM::CSS_MS          DontDelete|ReadOnly
 CSS_S                 KDOM::CSS_S           DontDelete|ReadOnly
 CSS_HZ                KDOM::CSS_HZ          DontDelete|ReadOnly
 CSS_KHZ               KDOM::CSS_KHZ         DontDelete|ReadOnly
 CSS_DIMENSION         KDOM::CSS_DIMENSION   DontDelete|ReadOnly
 CSS_STRING            KDOM::CSS_STRING      DontDelete|ReadOnly
 CSS_URI               KDOM::CSS_URI         DontDelete|ReadOnly
 CSS_IDENT             KDOM::CSS_IDENT       DontDelete|ReadOnly
 CSS_ATTR              KDOM::CSS_ATTR        DontDelete|ReadOnly
 CSS_COUNTER           KDOM::CSS_COUNTER     DontDelete|ReadOnly
 CSS_RECT              KDOM::CSS_RECT        DontDelete|ReadOnly
 CSS_RGBCOLOR          KDOM::CSS_RGBCOLOR    DontDelete|ReadOnly
@end
*/

KDOM_IMPLEMENT_CONSTRUCTOR(CSSPrimitiveValueConstructor, "CSSPrimitiveValue")

/*
@begin MutationEventConstructor::s_hashTable 3
 MODIFICATION	KDOM::MODIFICATION	DontDelete|ReadOnly
 ADDITION		KDOM::ADDITION		DontDelete|ReadOnly
 REMOVAL		KDOM::REMOVAL		DontDelete|ReadOnly
@end
*/

KDOM_IMPLEMENT_CONSTRUCTOR(MutationEventConstructor, "MutationEvent")

/*
@begin EventConstructor::s_hashTable 3
 CAPTURING_PHASE	KDOM::CAPTURING_PHASE	DontDelete|ReadOnly
 AT_TARGET			KDOM::AT_TARGET			DontDelete|ReadOnly
 BUBBLING_PHASE		KDOM::BUBBLING_PHASE	DontDelete|ReadOnly
@end
*/

KDOM_IMPLEMENT_CONSTRUCTOR(EventConstructor, "Event")

/*
@begin EventExceptionConstructor::s_hashTable 2
 UNSPECIFIED_EVENT_TYPE_ERR		KDOM::UNSPECIFIED_EVENT_TYPE_ERR 	DontDelete|ReadOnly
@end
*/

KDOM_IMPLEMENT_CONSTRUCTOR(EventExceptionConstructor, "EventException")

/*
@begin RangeConstructor::s_hashTable 5

 START_TO_START        KDOM::START_TO_START      DontDelete|ReadOnly
 START_TO_END          KDOM::START_TO_END        DontDelete|ReadOnly
 END_TO_END            KDOM::END_TO_END          DontDelete|ReadOnly
 END_TO_START          KDOM::END_TO_START        DontDelete|ReadOnly
@end
*/

KDOM_IMPLEMENT_CONSTRUCTOR(RangeConstructor, "Range")

// vim:ts=4:noet
// 
