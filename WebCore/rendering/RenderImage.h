/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com) 
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderImage_h
#define RenderImage_h

#include "RenderReplaced.h"

namespace WebCore {

class HTMLMapElement;

class RenderImage : public RenderReplaced {
public:
    RenderImage(Node*);
    virtual ~RenderImage();

    bool setImageSizeForAltText(CachedImage* newImage = 0);

    void updateAltText();

    void setCachedImage(CachedImage*);
    CachedImage* cachedImage() const { return m_cachedImage.get(); }

    HTMLMapElement* imageMap() const;

    void resetAnimation();

    virtual bool hasImage() const { return m_cachedImage; }

    void highQualityRepaintTimerFired(Timer<RenderImage>*);

protected:
    virtual Image* image(int /* width */ = 0, int /* height */ = 0) { return m_cachedImage ? m_cachedImage->image() : nullImage(); }
    virtual bool errorOccurred() const { return m_cachedImage && m_cachedImage->errorOccurred(); }

    virtual void styleDidChange(StyleDifference, const RenderStyle*);

    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0);

    virtual void paintIntoRect(GraphicsContext*, const IntRect&);
    void paintFocusRings(PaintInfo&, const RenderStyle*);
    virtual void paint(PaintInfo&, int tx, int ty);

    bool isWidthSpecified() const;
    bool isHeightSpecified() const;

    virtual void intrinsicSizeChanged() { imageChanged(imagePtr()); }

private:
    virtual const char* renderName() const { return "RenderImage"; }

    virtual bool isImage() const { return true; }
    virtual bool isRenderImage() const { return true; }
    
    virtual void paintReplaced(PaintInfo&, int tx, int ty);

    virtual int minimumReplacedHeight() const;

    virtual void notifyFinished(CachedResource*);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    virtual int calcReplacedWidth(bool includeMaxWidth = true) const;
    virtual int calcReplacedHeight() const;

    virtual void calcPrefWidths();

    virtual bool usesImageContainerSize() const { return m_cachedImage ? m_cachedImage->usesImageContainerSize() : false; }
    virtual void setImageContainerSize(const IntSize& size) const { if (m_cachedImage) m_cachedImage->setImageContainerSize(size); }
    virtual bool imageHasRelativeWidth() const { return m_cachedImage ? m_cachedImage->imageHasRelativeWidth() : false; }
    virtual bool imageHasRelativeHeight() const { return m_cachedImage ? m_cachedImage->imageHasRelativeHeight() : false; }
    virtual IntSize imageSize(float multiplier) const { return m_cachedImage ? m_cachedImage->imageSize(multiplier) : IntSize(); }
    virtual WrappedImagePtr imagePtr() const { return m_cachedImage.get(); }

    IntSize imageSizeForError(CachedImage*) const;
    void imageDimensionsChanged(bool imageSizeChanged, const IntRect* = 0);

    int calcAspectRatioWidth() const;
    int calcAspectRatioHeight() const;

protected:
    // The image we are rendering.
    CachedResourceHandle<CachedImage> m_cachedImage;

private:
    // Text to display as long as the image isn't available.
    String m_altText;
    bool m_needsToSetSizeForAltText;

    static Image* nullImage();
    
    friend class RenderImageScaleObserver;
};

inline RenderImage* toRenderImage(RenderObject* object)
{ 
    ASSERT(!object || object->isRenderImage());
    return static_cast<RenderImage*>(object);
}

inline const RenderImage* toRenderImage(const RenderObject* object)
{ 
    ASSERT(!object || object->isRenderImage());
    return static_cast<const RenderImage*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderImage(const RenderImage*);

} // namespace WebCore

#endif // RenderImage_h
