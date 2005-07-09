/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Computer, Inc.

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

#include "SVGRenderStyle.h"

using namespace KSVG;

SVGRenderStyle *SVGRenderStyle::s_defaultStyle = 0;

SVGRenderStyle::SVGRenderStyle() : KDOM::RenderStyle()
{
	if(!s_defaultStyle)	
		s_defaultStyle = new SVGRenderStyle(true);

	fill = s_defaultStyle->fill;
	stroke = s_defaultStyle->stroke;
	stops = s_defaultStyle->stops;
	clip = s_defaultStyle->clip;
	misc = s_defaultStyle->misc;
	markers = s_defaultStyle->markers;

	setBitDefaults();
}

SVGRenderStyle::SVGRenderStyle(bool) : KDOM::RenderStyle(true)
{
	setBitDefaults();

	fill.init();
	stroke.init();
	stops.init();
	clip.init();
	misc.init();
	markers.init();
}

SVGRenderStyle::SVGRenderStyle(const SVGRenderStyle &other) : KDOM::RenderStyle(other)
{
	fill = other.fill;
	stroke = other.stroke;
	stops = other.stops;
	clip = other.clip;
	misc = other.misc;
	markers = other.markers;

	svg_inherited_flags = other.svg_inherited_flags;
	svg_noninherited_flags = other.svg_noninherited_flags;
}

SVGRenderStyle::~SVGRenderStyle()
{
}

void SVGRenderStyle::cleanup()
{
	delete s_defaultStyle;
	s_defaultStyle = 0;
}

bool SVGRenderStyle::equals(KDOM::RenderStyle *other) const
{
	SVGRenderStyle *svgOther = dynamic_cast<SVGRenderStyle *>(other);
	if(!svgOther)
		return false;

	return (fill == svgOther->fill && stroke == svgOther->stroke &&
		stops == svgOther->stops && clip == svgOther->clip &&
		misc == svgOther->misc && markers == svgOther->markers &&
		KDOM::RenderStyle::equals(other));
}

void SVGRenderStyle::inheritFrom(const RenderStyle *inheritParent)
{
	const SVGRenderStyle *svgInheritParent = static_cast<const SVGRenderStyle *>(inheritParent);
	if(!svgInheritParent)
		return;

	KDOM::RenderStyle::inheritFrom(inheritParent);

	fill = svgInheritParent->fill;
	stroke = svgInheritParent->stroke;
	stops = svgInheritParent->stops;
	//misc = svgInheritParent->misc;
	markers = svgInheritParent->markers;
	setOpacity(initialOpacity());

	svg_inherited_flags = svgInheritParent->svg_inherited_flags;
}

// vim:ts=4:noet
