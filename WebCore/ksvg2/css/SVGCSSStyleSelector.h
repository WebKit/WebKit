/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml css code by:
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2003 Apple Computer, Inc.

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

#ifndef KSVG_SVGCSSStyleSelector_H
#define KSVG_SVGCSSStyleSelector_H

#include <kdom/css/impl/CSSStyleSelector.h>

namespace KDOM
{
	class RenderStyle;
	class ElementImpl;
	class DocumentImpl;
	class CSSValueImpl;
	class CSSStyleSheetImpl;
	class StyleSheetListImpl;
	class CSSOrderedProperty;
	class CSSStyleDeclarationImpl;
}

namespace KSVG
{
	/*
	 * the StyleSelector implementation for SVG css props.
	 */
	class SVGCSSStyleSelector : public KDOM::CSSStyleSelector
	{
	public:
		/**
		 * creates a new StyleSelector for a Document.
		 * goes through all StyleSheets defined in the document and
		 * creates a list of rules it needs to apply to objects
		 *
		 * Also takes into account special cases for HTML documents,
		 * including the defaultStyle (which is html only)
		 */
		SVGCSSStyleSelector(KDOM::DocumentImpl *doc, const QString &userStyleSheet,
							KDOM::StyleSheetListImpl *styleSheets,
							const KURL &url, bool strictParsing);
		/*
		 * same as above but for a single stylesheet.
		 */
		SVGCSSStyleSelector(KDOM::CSSStyleSheetImpl *sheet);
		
		virtual ~SVGCSSStyleSelector();

	protected:
		static KDOM::CSSStyleSheetImpl *s_defaultSheet;
		static KDOM::CSSStyleSheetImpl *s_quirksSheet;
		static KDOM::CSSStyleSelectorList *s_defaultStyle;
		static KDOM::CSSStyleSelectorList *s_defaultPrintStyle;

	protected:
		virtual unsigned int addExtraDeclarations(KDOM::ElementImpl *e, unsigned int numProps);

		QMemArray<KDOM::CSSOrderedProperty> presentationAttrs;

		void loadDefaultStyle(KDOM::DocumentImpl *doc);

		virtual void applyRule(int id, KDOM::CSSValueImpl *value);
	};
}

#endif

// vim:ts=4:noet
