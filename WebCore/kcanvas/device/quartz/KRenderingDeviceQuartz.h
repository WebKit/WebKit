/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */


#ifndef KRenderingDeviceQuartz_H
#define KRenderingDeviceQuartz_H

#import <kcanvas/device/KRenderingDevice.h>
#import "KCanvasTypes.h"

typedef struct CGRect CGRect;
typedef struct CGContext *CGContextRef;

class KRenderingDeviceContextQuartz : public KRenderingDeviceContext
{
public:
	KRenderingDeviceContextQuartz() : m_cgContext(0) { }
	virtual void setWorldMatrix(const KCanvasMatrix &worldMatrix);
	virtual KCanvasMatrix worldMatrix() const;
	
	virtual QRect mapFromVisual(const QRect &rect);
	virtual QRect mapToVisual(const QRect &rect);
	
	CGContextRef cgContext() { return m_cgContext; };
	void setCGContext(CGContextRef newContext) { m_cgContext = newContext; }
	
private:
	CGContextRef m_cgContext;
};

class KRenderingDeviceQuartz : public KRenderingDevice
{
Q_OBJECT
public:
	KRenderingDeviceQuartz() { }
	virtual ~KRenderingDeviceQuartz() { }

	virtual bool isBuffered() const { return false; }

	// context management.
	KRenderingDeviceContextQuartz *quartzContext() const;
	CGContextRef currentCGContext() const;
	virtual KRenderingDeviceContext *contextForImage(KCanvasImage *) const;

	// Vector path creation
	virtual void deletePath(KCanvasUserData path);
	virtual void startPath();
	virtual void endPath();

	virtual void moveTo(double x, double y);
	virtual void lineTo(double x, double y);
	virtual void curveTo(double x1, double y1, double x2, double y2, double x3, double y3);
	virtual void closeSubpath();
	
	KCanvasUserData pathForRect(const QRect &) const;

	// Resource creation
	virtual KCanvasResource *createResource(const KCResourceType &type) const;
	virtual KRenderingPaintServer *createPaintServer(const KCPaintServerType &type) const;
	virtual KCanvasFilterEffect *createFilterEffect(const KCFilterEffectType &type) const;
	
	// item creation
	virtual KCanvasItem *createItem(KCanvas *canvas, KRenderingStyle *style, KCanvasUserData path) const;
	virtual KCanvasContainer *createContainer(KCanvas *canvas, KRenderingStyle *style) const;

	// filters (mostly debugging)
	static bool filtersEnabled();
	static void setFiltersEnabled(bool enabled);
};

// Wraps NSBezierPaths for c++ consumption
class KCanvasQuartzPathData
{
public:
	KCanvasQuartzPathData();

	~KCanvasQuartzPathData();

	CGMutablePathRef path;
	bool hasValidBBox;
	CGRect bbox;
};

class KCanvasQuartzUserData : public KCanvasPrivateUserData {
public: 
	KCanvasQuartzUserData();
	~KCanvasQuartzUserData();
};

#endif