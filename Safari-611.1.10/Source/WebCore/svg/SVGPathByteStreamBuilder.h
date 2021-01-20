/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "FloatPoint.h"
#include "SVGPathByteStream.h"
#include "SVGPathConsumer.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class SVGPathByteStreamBuilder final : public SVGPathConsumer {
public:
    SVGPathByteStreamBuilder(SVGPathByteStream&);

private:
    void incrementPathSegmentCount() final { }
    bool continueConsuming() final { return true; }

    // Used in UnalteredParsing/NormalizedParsing modes.
    void moveTo(const FloatPoint&, bool closed, PathCoordinateMode) final;
    void lineTo(const FloatPoint&, PathCoordinateMode) final;
    void curveToCubic(const FloatPoint&, const FloatPoint&, const FloatPoint&, PathCoordinateMode) final;
    void closePath() final;

    // Only used in UnalteredParsing mode.
    void lineToHorizontal(float, PathCoordinateMode) final;
    void lineToVertical(float, PathCoordinateMode) final;
    void curveToCubicSmooth(const FloatPoint&, const FloatPoint&, PathCoordinateMode) final;
    void curveToQuadratic(const FloatPoint&, const FloatPoint&, PathCoordinateMode) final;
    void curveToQuadraticSmooth(const FloatPoint&, PathCoordinateMode) final;
    void arcTo(float, float, float, bool largeArcFlag, bool sweepFlag, const FloatPoint&, PathCoordinateMode) final;

    template<typename ByteType>
    void writeType(const ByteType& type)
    {
        size_t typeSize = sizeof(ByteType);
        for (size_t i = 0; i < typeSize; ++i)
            m_byteStream.append(type.bytes[i]);
    }

    void writeFlag(bool value)
    {
        BoolByte data;
        data.value = value;
        writeType(data);
    }

    void writeFloat(float value)
    {
        FloatByte data;
        data.value = value;
        writeType(data);
    }

    void writeFloatPoint(const FloatPoint& point)
    {
        writeFloat(point.x());
        writeFloat(point.y());
    }

    void writeSegmentType(unsigned short value)
    {
        UnsignedShortByte data;
        data.value = value;
        writeType(data);
    }

    SVGPathByteStream& m_byteStream;
};

} // namespace WebCore
