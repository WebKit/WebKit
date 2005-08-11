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

#ifndef KSVG_SVGElementImpl_H
#define KSVG_SVGElementImpl_H

#include <ksvg2/impl/svgtags.h>
#include <kdom/impl/XMLElementImpl.h>

namespace KDOM
{
	class DocumentImpl;
};

namespace KSVG
{
	class SVGMatrixImpl;
	class SVGSVGElementImpl;
	class SVGStyledElementImpl;
	class SVGDocumentImpl;

	class SVGElementImpl : public KDOM::XMLElementImpl
	{
	public:
		SVGElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix);
		virtual ~SVGElementImpl();

		virtual bool isSupported(const KDOM::DOMString &feature, const KDOM::DOMString &version) const;

		// 'SVGElement' functions
		KDOM::DOMString getId() const;
		KDOM::DOMString xmlbase() const;

		SVGSVGElementImpl *ownerSVGElement() const;
		SVGElementImpl *viewportElement() const;

		// Helper methods that returns the attr value if attr is set, otherwise the default value.
		// It throws NO_MODIFICATION_ALLOWED_ERR if the element is read-only.
		KDOM::DOMString tryGetAttribute(const KDOM::DOMString &name, const KDOM::DOMString &defaultVal = "") const;
		KDOM::DOMString tryGetAttributeNS(const KDOM::DOMString &namespaceURI, const KDOM::DOMString &localName, const KDOM::DOMString &defaultVal = "") const;

		// Internal
		virtual void parseAttribute(KDOM::AttributeImpl *attr);

		virtual void createStyleDeclaration() const;

		// To be implemented by any element which can establish new viewports...
		virtual QString adjustViewportClipping() const { return QString::null; }

		SVGDocumentImpl *getDocument() const;

	private:
		void addSVGEventListener(KDOM::Ecma *ecmaEngine, const KDOM::DOMString &type, const KDOM::DOMString &value);
	};
};

// Same like KDOM_IMPL_DTOR_ASSIGN_OP, just uses 'impl' instead of 'd'.
#define KSVG_IMPL_DTOR_ASSIGN_OP(T) \
T::~T() { if(impl) impl->deref(); } \
T &T::operator=(const T &other) { \
	KDOM_SAFE_SET(impl, other.impl); \
    return *this; \
} \
bool T::operator==(const T &other) const { \
    return impl == other.impl; \
} \
bool T::operator!=(const T &other) const { \
    return !operator==(other); \
} \

#endif

// vim:ts=4:noet
