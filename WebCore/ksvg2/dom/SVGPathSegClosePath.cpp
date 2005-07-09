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

#include "SVGPathSegClosePath.h"
#include "SVGPathSegClosePathImpl.h"

using namespace KSVG;

SVGPathSegClosePath SVGPathSegClosePath::null;

SVGPathSegClosePath::SVGPathSegClosePath() : SVGPathSeg()
{
}

SVGPathSegClosePath::SVGPathSegClosePath(SVGPathSegClosePathImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegClosePath::SVGPathSegClosePath(const SVGPathSegClosePath &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegClosePath::SVGPathSegClosePath(const SVGPathSeg &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegClosePath::~SVGPathSegClosePath()
{
}

SVGPathSegClosePath &SVGPathSegClosePath::operator=(const SVGPathSegClosePath &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegClosePath, PATHSEG_CLOSEPATH)

// vim:ts=4:noet
