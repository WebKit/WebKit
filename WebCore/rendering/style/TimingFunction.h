/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef TimingFunction_h
#define TimingFunction_h

#include "RenderStyleConstants.h"

namespace WebCore {

struct TimingFunction {
    TimingFunction()
        : m_type(CubicBezierTimingFunction)
        , m_x1(0.25)
        , m_y1(0.1)
        , m_x2(0.25)
        , m_y2(1.0)
    {
    }

    TimingFunction(ETimingFunctionType timingFunction, double x1 = 0.0, double y1 = 0.0, double x2 = 1.0, double y2 = 1.0)
        : m_type(timingFunction)
        , m_x1(x1)
        , m_y1(y1)
        , m_x2(x2)
        , m_y2(y2)
    {
    }

    bool operator==(const TimingFunction& o) const
    {
        return m_type == o.m_type && m_x1 == o.m_x1 && m_y1 == o.m_y1 && m_x2 == o.m_x2 && m_y2 == o.m_y2;
    }

    double x1() const { return m_x1; }
    double y1() const { return m_y1; }
    double x2() const { return m_x2; }
    double y2() const { return m_y2; }

    ETimingFunctionType type() const { return m_type; }

private:
    ETimingFunctionType m_type;

    double m_x1;
    double m_y1;
    double m_x2;
    double m_y2;
};

} // namespace WebCore

#endif // TimingFunction_h
