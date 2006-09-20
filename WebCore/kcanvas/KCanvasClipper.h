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

#ifndef KCanvasClipper_h
#define KCanvasClipper_h
#ifdef SVG_SUPPORT

#include "KCanvasResource.h"

namespace WebCore {

typedef DeprecatedValueList<const RenderPath *> RenderPathList;

class TextStream;

struct KCClipData
{
    WindRule windRule() const { return m_windRule; }
    WindRule m_windRule;
    bool bboxUnits : 1;
    Path path;
};

class KCClipDataList : public DeprecatedValueList<KCClipData>
{
public:
    KCClipDataList() { }

    inline void addPath(const Path& pathData, WindRule windRule, bool bboxUnits)
    {
        KCClipData clipData;
        clipData.bboxUnits = bboxUnits;
        clipData.m_windRule = windRule;
        clipData.path = pathData;
        append(clipData);
    }
};

class KCanvasClipper : public KCanvasResource
{
public:
    KCanvasClipper();
    virtual ~KCanvasClipper();
    
    virtual bool isClipper() const { return true; }

    void resetClipData();
    void addClipData(const Path&, WindRule, bool bboxUnits);
    
    virtual void applyClip(const FloatRect& boundingBox) const = 0;

    KCClipDataList clipData() const;

    TextStream& externalRepresentation(TextStream &) const; 
protected:
    KCClipDataList m_clipData;
};

TextStream& operator<<(TextStream&, WindRule);
TextStream& operator<<(TextStream&, const KCClipData&);

KCanvasClipper* getClipperById(Document*, const AtomicString&);

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
