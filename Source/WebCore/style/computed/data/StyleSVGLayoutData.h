/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
    Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>

    Based on khtml code by:
    Copyright (C) 2000-2003 Lars Knoll (knoll@kde.org)
              (C) 2000 Antti Koivisto (koivisto@kde.org)
              (C) 2000-2003 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Inc. All rights reserved.

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#pragma once

#include <WebCore/StyleSVGCenterCoordinateComponent.h>
#include <WebCore/StyleSVGCoordinateComponent.h>
#include <WebCore/StyleSVGPathData.h>
#include <WebCore/StyleSVGRadius.h>
#include <WebCore/StyleSVGRadiusComponent.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {
namespace Style {

// Positioning and sizing properties.
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SVGLayoutData);
class SVGLayoutData : public RefCounted<SVGLayoutData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(SVGLayoutData, SVGLayoutData);
public:
    static Ref<SVGLayoutData> create() { return adoptRef(*new SVGLayoutData); }
    Ref<SVGLayoutData> copy() const;

    bool operator==(const SVGLayoutData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const SVGLayoutData&) const;
#endif

    SVGCenterCoordinateComponent cx;
    SVGCenterCoordinateComponent cy;
    SVGRadius r;
    SVGRadiusComponent rx;
    SVGRadiusComponent ry;
    SVGCoordinateComponent x;
    SVGCoordinateComponent y;
    SVGPathData d;

private:
    SVGLayoutData();
    SVGLayoutData(const SVGLayoutData&);
};

WTF::TextStream& operator<<(WTF::TextStream&, const SVGLayoutData&);

} // namespace Style
} // namespace WebCore
