/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

namespace WebCore {

class SVGTextMetrics {
public:
    SVGTextMetrics() = default;
    SVGTextMetrics(unsigned length, float scaledWidth, float scaledHeight)
        : m_width(scaledWidth)
        , m_height(scaledHeight)
        , m_length(length)
    { }

    float width() const { return m_width; }
    float height() const { return m_height; }
    unsigned length() const { return m_length; }

private:
    float m_width { 0 };
    float m_height { 0 };
    unsigned m_length { 0 };
};

} // namespace WebCore
