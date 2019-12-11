/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
 */

#pragma once

#include "ExceptionOr.h"
#include "QualifiedName.h"
#include "SVGNames.h"
#include "SVGZoomAndPanType.h"

namespace WebCore {

class SVGZoomAndPan {
    WTF_MAKE_NONCOPYABLE(SVGZoomAndPan);
public:
    // Forward declare enumerations in the W3C naming scheme, for IDL generation.
    enum {
        SVG_ZOOMANDPAN_UNKNOWN = SVGZoomAndPanUnknown,
        SVG_ZOOMANDPAN_DISABLE = SVGZoomAndPanDisable,
        SVG_ZOOMANDPAN_MAGNIFY = SVGZoomAndPanMagnify
    };

    SVGZoomAndPanType zoomAndPan() const { return m_zoomAndPan; }
    void setZoomAndPan(SVGZoomAndPanType zoomAndPan) { m_zoomAndPan = zoomAndPan; }
    ExceptionOr<void> setZoomAndPan(unsigned) { return Exception { NoModificationAllowedError }; }
    void reset() { m_zoomAndPan = SVGPropertyTraits<SVGZoomAndPanType>::initialValue(); }

    void parseAttribute(const QualifiedName&, const AtomString&);

protected:
    SVGZoomAndPan() = default;

    bool parseZoomAndPan(const UChar*&, const UChar*);

private:
    SVGZoomAndPanType m_zoomAndPan { SVGPropertyTraits<SVGZoomAndPanType>::initialValue() };
};

} // namespace WebCore
