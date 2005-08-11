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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_KCanvasRenderingStyle_H
#define KSVG_KCanvasRenderingStyle_H

#include <kdom/css/impl/CSSPrimitiveValueImpl.h>

#include <kcanvas/KCanvasMatrix.h>
#include <kcanvas/device/KRenderingStyle.h>

namespace KSVG
{
	class SVGRenderStyle;
	class KCanvasRenderingStyle : public KRenderingStyle
	{
	public:
		KCanvasRenderingStyle(KCanvas *canvas, const SVGRenderStyle *style);
		virtual ~KCanvasRenderingStyle();

		void updateFill(KCanvasItem *item);
		void updateStroke(KCanvasItem *item);

		void updateStyle(const SVGRenderStyle *style, KCanvasItem *item);

		// World matrix property
		virtual KCanvasMatrix objectMatrix() const;
		virtual void setObjectMatrix(const KCanvasMatrix &objectMatrix);

		// Stroke (aka Pen) properties
		virtual bool isStroked() const;

		virtual KRenderingStrokePainter *strokePainter();
		virtual void disableStrokePainter();

		double cssPrimitiveToLength(KCanvasItem *item, KDOM::CSSValueImpl *value, double defaultValue = 0.0) const;

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
		virtual void setOpacity(int);

		// Clipping
		virtual QStringList clipPaths() const;

		void addClipPath(const QString &clipPath);
		void removeClipPaths();

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

	private:
		KCanvasRenderingStyle(const KCanvasRenderingStyle &other);

		// Data
		KCanvasMatrix m_matrix;
		mutable QStringList m_clipPaths;

		const SVGRenderStyle *m_style;

		// KCanvas stuff
		KCanvas *m_canvas;
		KRenderingFillPainter *m_fillPainter;
		KRenderingStrokePainter *m_strokePainter;
	};
};

#endif

// vim:ts=4:noet
