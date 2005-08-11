/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#include <qapplication.h>

#include "RenderStyle.h"
#include "RenderStyleDefs.h"

using namespace KDOM;

// CSS says 'Fixed' for the default padding value, but we treat 'Variable' as 0
// padding anyway, and like this is works fine for table paddings as well...
StyleSurroundData::StyleSurroundData() : Shared(false), margin(LT_FIXED), padding(LT_VARIABLE)
{
}

StyleSurroundData::StyleSurroundData(const StyleSurroundData &other) : Shared(false)
{
	offset = other.offset; margin = other.margin;
	padding = other.padding; border = other.border;
}

bool StyleSurroundData::operator==(const StyleSurroundData &other) const
{
    return (offset == other.offset) && (margin == other.margin) &&
		   (padding == other.padding) && (border == other.border);
}

StyleBoxData::StyleBoxData() : Shared(false), zIndex(0), zAuto(true)
{
	width = RenderStyle::initialWidth();
	height = RenderStyle::initialHeight();
    minWidth = RenderStyle::initialMinWidth();
	minHeight = RenderStyle::initialMinHeight();
    maxWidth = RenderStyle::initialMaxWidth();
	maxHeight = RenderStyle::initialMaxHeight();
    boxSizing = RenderStyle::initialBoxSizing();
}

StyleBoxData::StyleBoxData(const StyleBoxData &other) : Shared(false)
{
	width = other.width; height = other.height;
	minWidth = other.minWidth; minHeight = other.minHeight;
	maxWidth = other.maxWidth; maxHeight = other.maxHeight;
	boxSizing = other.boxSizing; zIndex = other.zIndex;
	zAuto = other.zAuto;
}

bool StyleBoxData::operator==(const StyleBoxData &other) const
{
    return (width == other.width) && (height == other.height) &&
		   (minWidth == other.minWidth) && (maxWidth == other.maxWidth) &&
		   (minHeight == other.minHeight) && (maxHeight == other.maxHeight) &&
		   (boxSizing == other.boxSizing) &&  (zIndex == other.zIndex) &&
		   (zAuto == other.zAuto);
}

StyleVisualData::StyleVisualData() : Shared(false)
{
	textDecoration = RenderStyle::initialTextDecoration();
	palette = QApplication::palette();
}

StyleVisualData::StyleVisualData(const StyleVisualData &other) : Shared(false)
{
	clip = other.clip;
	textDecoration = other.textDecoration;
	palette = other.palette;
}

bool StyleVisualData::operator==(const StyleVisualData &other) const
{
	return (clip == other.clip) &&
		   (textDecoration == other.textDecoration) &&
		   (palette == other.palette);
}

StyleBackgroundData::StyleBackgroundData() : Shared(false)
{
	image = RenderStyle::initialBackgroundImage();
}

StyleBackgroundData::StyleBackgroundData(const StyleBackgroundData &other) : Shared(false)
{
	color = other.color;
	image = other.image;
	xPosition = other.xPosition;
	yPosition = other.yPosition;
	outline = other.outline;
}

bool StyleBackgroundData::operator==(const StyleBackgroundData &other) const
{
    return (color == other.color) && (image == other.image) &&
		   (xPosition == other.xPosition) && (yPosition == other.yPosition) &&
		   (outline == other.outline);
}

StyleMarqueeData::StyleMarqueeData() : Shared(false)
{
	increment = RenderStyle::initialMarqueeIncrement();
	speed = RenderStyle::initialMarqueeSpeed();
	direction = RenderStyle::initialMarqueeDirection();
	behavior = RenderStyle::initialMarqueeBehavior();
	loops = RenderStyle::initialMarqueeLoopCount();
}

StyleMarqueeData::StyleMarqueeData(const StyleMarqueeData &other) : Shared(false)
{
	increment = other.increment;
	speed = other.speed;
	loops = other.loops;
	behavior = other.behavior;
	direction = other.direction;
}

bool StyleMarqueeData::operator==(const StyleMarqueeData &other) const
{
	return (increment == other.increment && speed == other.speed &&
			direction == other.direction && behavior == other.behavior && loops == other.loops);
}

StyleCSS3NonInheritedData::StyleCSS3NonInheritedData() : Shared(false)
{
	opacity = RenderStyle::initialOpacity();
}

StyleCSS3NonInheritedData::StyleCSS3NonInheritedData(const StyleCSS3NonInheritedData &other) : Shared(false)
{
	opacity = other.opacity;
	marquee = other.marquee;
}

bool StyleCSS3NonInheritedData::operator==(const StyleCSS3NonInheritedData &other) const
{
	return (opacity == other.opacity) && (marquee == other.marquee);
}

StyleCSS3InheritedData::StyleCSS3InheritedData() : Shared(false), textShadow(0)
{
}

StyleCSS3InheritedData::StyleCSS3InheritedData(const StyleCSS3InheritedData &other) : Shared(false)
{
	textShadow = other.textShadow ? new ShadowData(*other.textShadow) : 0;
}

StyleCSS3InheritedData::~StyleCSS3InheritedData()
{
	delete textShadow;
}

bool StyleCSS3InheritedData::operator==(const StyleCSS3InheritedData &other) const
{
	return shadowDataEquivalent(other);
}

bool StyleCSS3InheritedData::shadowDataEquivalent(const StyleCSS3InheritedData &other) const
{
	if(!textShadow && other.textShadow || textShadow && !other.textShadow)
		return false;
	if(textShadow && other.textShadow && (*textShadow != *other.textShadow))
		return false;
	
	return true;
}

StyleInheritedData::StyleInheritedData() : Shared(false), font()
{
	indent = RenderStyle::initialTextIndent();
	lineHeight = RenderStyle::initialLineHeight();
	styleImage = RenderStyle::initialListStyleImage();
	color = RenderStyle::initialColor();

	borderHSpacing = RenderStyle::initialBorderHSpacing();
   	borderVSpacing = RenderStyle::initialBorderVSpacing();
	
	quotes = 0;
	widows = RenderStyle::initialWidows();
	orphans = RenderStyle::initialOrphans();
	pageBreakInside = RenderStyle::initialPageBreakInside();
}

StyleInheritedData::StyleInheritedData(const StyleInheritedData &other) : Shared(false)
{
	indent = other.indent; lineHeight = other.lineHeight;
	styleImage = other.styleImage; font = other.font;
	color = other.color; decorationColor = other.decorationColor;
	borderHSpacing = other.borderHSpacing; borderVSpacing = other.borderVSpacing;
	widows = other.widows; orphans = other.orphans; pageBreakInside = other.pageBreakInside;

	quotes = other.quotes;
	if(quotes)
		quotes->ref();
}

bool StyleInheritedData::operator==(const StyleInheritedData &other) const
{
    return (indent == other.indent) && (lineHeight == other.lineHeight) &&
		   (borderHSpacing == other.borderHSpacing) && (borderVSpacing == other.borderVSpacing) &&
		   (styleImage == other.styleImage) && (font == other.font) &&
		   (color == other.color) && (decorationColor == other.decorationColor) &&
		   (widows == other.widows) && (orphans == other.orphans) &&
		   (pageBreakInside == other.pageBreakInside) && (quotes == other.quotes);
}

// vim:ts=4:noet
