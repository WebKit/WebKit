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

#include "config.h"
#ifdef SVG_SUPPORT
#include "KCanvasClipper.h"

#include "TextStream.h"
#include "KCanvasTreeDebug.h"

namespace WebCore {

TextStream& operator<<(TextStream& ts, WindRule rule)
{
    switch (rule) {
        case RULE_NONZERO:
            ts << "NON-ZERO"; break;
        case RULE_EVENODD:
            ts << "EVEN-ODD"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const KCClipData &d)
{
    ts << "[winding=" << d.windRule() << "]";
    if (d.bboxUnits)
        ts << " [bounding box mode=" << d.bboxUnits  << "]";    
    ts << " [path=" << d.path.debugString() << "]";
    return ts;
}

KCanvasClipper::KCanvasClipper() : KCanvasResource()
{
}

KCanvasClipper::~KCanvasClipper()
{
}

void KCanvasClipper::resetClipData()
{
    m_clipData.clear();
}

void KCanvasClipper::addClipData(const Path& path, WindRule rule, bool bboxUnits)
{
    m_clipData.addPath(path, rule, bboxUnits);
}

KCClipDataList KCanvasClipper::clipData() const
{
    return m_clipData;
}

TextStream& KCanvasClipper::externalRepresentation(TextStream& ts) const
{
    ts << "[type=CLIPPER]";
    ts << " [clip data=" << clipData() << "]";
    return ts;
}

KCanvasClipper *getClipperById(Document *document, const AtomicString &id)
{
    KCanvasResource *resource = getResourceById(document, id);
    if (resource && resource->isClipper())
        return static_cast<KCanvasClipper*>(resource);
    return 0;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

