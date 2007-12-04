/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com) 
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "CachedImage.h"
#include "RenderReplaced.h"

namespace WebCore {

class HTMLMapElement;

class RenderImage : public RenderReplaced {
public:
    RenderImage(Node*);
    virtual ~RenderImage();

    virtual const char* renderName() const { return "RenderImage"; }

    virtual bool isImage() const { return true; }
    
    virtual void paintReplaced(PaintInfo& paintInfo, int tx, int ty);

    virtual int minimumReplacedHeight() const;

    virtual void imageChanged(CachedImage*);
    
    bool setImageSizeForAltText(CachedImage* newImage = 0);

    void updateAltText();

    void setIsAnonymousImage(bool anon) { m_isAnonymousImage = anon; }
    bool isAnonymousImage() { return m_isAnonymousImage; }

    void setCachedImage(CachedImage*);
    CachedImage* cachedImage() const { return m_cachedImage; }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    virtual int calcReplacedWidth() const;
    virtual int calcReplacedHeight() const;

    virtual void calcPrefWidths();

    HTMLMapElement* imageMap();

    void resetAnimation();

protected:
    Image* image() { return m_cachedImage ? m_cachedImage->image() : nullImage(); }
    
    bool errorOccurred() const { return m_cachedImage && m_cachedImage->errorOccurred(); }

private:
    int calcAspectRatioWidth() const;
    int calcAspectRatioHeight() const;

    bool isWidthSpecified() const;
    bool isHeightSpecified() const;

    // The image we are rendering.
    CachedImage* m_cachedImage;

    // True if the image is set through the content: property
    bool m_isAnonymousImage;

    // Text to display as long as the image isn't available.
    String m_altText;

    static Image* nullImage();
};

} // namespace WebCore

#endif // RenderImage_h
