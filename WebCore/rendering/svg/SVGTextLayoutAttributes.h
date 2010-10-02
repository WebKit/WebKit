/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGTextLayoutAttributes_h
#define SVGTextLayoutAttributes_h

#if ENABLE(SVG)
#include "SVGTextMetrics.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SVGTextLayoutAttributes {
public:
    SVGTextLayoutAttributes();

    void reserveCapacity(unsigned length);
    void dump() const;

    static float emptyValue();

    Vector<float>& xValues() { return m_xValues; }
    const Vector<float>& xValues() const { return m_xValues; }

    Vector<float>& yValues() { return m_yValues; }
    const Vector<float>& yValues() const { return m_yValues; }

    Vector<float>& dxValues() { return m_dxValues; }
    const Vector<float>& dxValues() const { return m_dxValues; }

    Vector<float>& dyValues() { return m_dyValues; }
    const Vector<float>& dyValues() const { return m_dyValues; }

    Vector<float>& rotateValues() { return m_rotateValues; }
    const Vector<float>& rotateValues() const { return m_rotateValues; }

    Vector<SVGTextMetrics>& textMetricsValues() { return m_textMetricsValues; }
    const Vector<SVGTextMetrics>& textMetricsValues() const { return m_textMetricsValues; }

private:
    Vector<float> m_xValues;
    Vector<float> m_yValues;
    Vector<float> m_dxValues;
    Vector<float> m_dyValues;
    Vector<float> m_rotateValues;
    Vector<SVGTextMetrics> m_textMetricsValues;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
