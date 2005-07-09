/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#ifndef KSVG_SVGAnimateColorElementImpl_H
#define KSVG_SVGAnimateColorElementImpl_H

#include "SVGColorImpl.h"
#include "SVGAnimationElementImpl.h"

namespace KSVG
{
	class SVGAnimateColorElementImpl : public SVGAnimationElementImpl
	{
	public:
		SVGAnimateColorElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix);
		virtual ~SVGAnimateColorElementImpl();

		virtual void handleTimerEvent(double timePercentage);

		// Helper
		QColor clampColor(int r, int g, int b) const;
		void calculateColor(double time, int &r, int &g, int &b) const;

		QColor color() const;
		QColor initialColor() const;

	private:
		QColor m_lastColor;
		QColor m_currentColor;
		QColor m_initialColor;

		SVGColorImpl *m_toColor;
		SVGColorImpl *m_fromColor;

		int m_currentItem;
		int m_redDiff, m_greenDiff, m_blueDiff;
	};
};

#endif

// vim:ts=4:noet
