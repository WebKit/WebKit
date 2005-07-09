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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KRenderingStyle_H
#define KRenderingStyle_H

#include <qvaluelist.h>
#include <qstringlist.h>

// Enumerations
typedef enum
{
	CAP_BUTT = 1,
	CAP_ROUND = 2,
	CAP_SQUARE = 3
} KCCapStyle;

typedef enum
{
	JOIN_MITER = 1,
	JOIN_ROUND = 2,
	JOIN_BEVEL = 3
} KCJoinStyle;

typedef enum
{
	CI_SRGB = 1,
	CI_LINEARRGB = 2
} KCColorInterpolation;

typedef enum
{
	IR_OPTIMIZE_SPEED = 0,
	IR_OPTIMIZE_QUALITY = 1
} KCImageRendering;

// Special types
typedef QValueList<float> KCDashArray;

class KCanvasMatrix;
class KCanvasFilter;
class KCanvasMarker;
class KRenderingFillPainter;
class KRenderingStrokePainter;
class KRenderingStyle
{
public:
	KRenderingStyle();
	virtual ~KRenderingStyle();

	// World matrix property
	virtual KCanvasMatrix objectMatrix() const;
	virtual void setObjectMatrix(const KCanvasMatrix &objectMatrix);

	// Stroke (aka Pen) properties
	virtual bool isStroked() const;

	virtual KRenderingStrokePainter *strokePainter();
	virtual void disableStrokePainter();

	// Fill (aka Brush) properties
	virtual bool isFilled() const;

	virtual KRenderingFillPainter *fillPainter();
	virtual void disableFillPainter();

	// Display states
	virtual bool visible() const;
	virtual void setVisible(bool visible);
	
	// Color interpolation
	virtual KCColorInterpolation colorInterpolation() const;
	virtual void setColorInterpolation(KCColorInterpolation interpolation);

	// Quality vs. speed control
	virtual KCImageRendering imageRendering() const;
	virtual void setImageRendering(KCImageRendering ir);

	// Overall opacity
	virtual int opacity() const;
	virtual void setOpacity(int opacity);

	// Clipping
	virtual QStringList clipPaths() const;
	virtual void setClipPaths(const QStringList &data) const;

	// Markers
	virtual KCanvasMarker *startMarker() const;
	virtual void setStartMarker(KCanvasMarker *marker);

	virtual KCanvasMarker *midMarker() const;
	virtual void setMidMarker(KCanvasMarker *marker);

	virtual KCanvasMarker *endMarker() const;
	virtual void setEndMarker(KCanvasMarker *marker);

	virtual bool hasMarkers() const;

	// Filter support
	virtual KCanvasFilter *filter() const;
	virtual void setFilter(KCanvasFilter *newFilter) const;

	// TODO: Last remaining issue here: text & font!

private:
	class Private;
	Private *d;
};

#endif

// vim:ts=4:noet
