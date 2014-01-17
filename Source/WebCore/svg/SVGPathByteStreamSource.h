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

#ifndef SVGPathByteStreamSource_h
#define SVGPathByteStreamSource_h

#if ENABLE(SVG)
#include "FloatPoint.h"
#include "SVGPathByteStream.h"
#include "SVGPathSource.h"

namespace WebCore {

class SVGPathByteStreamSource : public SVGPathSource {
public:
    explicit SVGPathByteStreamSource(SVGPathByteStream*);

private:
    virtual bool hasMoreData() const override;
    virtual bool moveToNextToken() override { return true; }
    virtual bool parseSVGSegmentType(SVGPathSegType&) override;
    virtual SVGPathSegType nextCommand(SVGPathSegType) override;

    virtual bool parseMoveToSegment(FloatPoint&) override;
    virtual bool parseLineToSegment(FloatPoint&) override;
    virtual bool parseLineToHorizontalSegment(float&) override;
    virtual bool parseLineToVerticalSegment(float&) override;
    virtual bool parseCurveToCubicSegment(FloatPoint&, FloatPoint&, FloatPoint&) override;
    virtual bool parseCurveToCubicSmoothSegment(FloatPoint&, FloatPoint&) override;
    virtual bool parseCurveToQuadraticSegment(FloatPoint&, FloatPoint&) override;
    virtual bool parseCurveToQuadraticSmoothSegment(FloatPoint&) override;
    virtual bool parseArcToSegment(float&, float&, float&, bool&, bool&, FloatPoint&) override;

#if COMPILER(MSVC)
#pragma warning(disable: 4701)
#endif
    template<typename DataType, typename ByteType>
    DataType readType()
    {
        ByteType data;
        size_t typeSize = sizeof(ByteType);

        for (size_t i = 0; i < typeSize; ++i) {
            ASSERT_WITH_SECURITY_IMPLICATION(m_streamCurrent < m_streamEnd);
            data.bytes[i] = *m_streamCurrent;
            ++m_streamCurrent;
        }

        return data.value;
    }

    bool readFlag()
    {
        return readType<bool, BoolByte>();
    }

    float readFloat()
    {
        return readType<float, FloatByte>();
    }

    unsigned short readSVGSegmentType()
    {
        return readType<unsigned short, UnsignedShortByte>();
    }

    FloatPoint readFloatPoint()
    {
        float x = readType<float, FloatByte>();
        float y = readType<float, FloatByte>();
        return FloatPoint(x, y);
    }

    SVGPathByteStream::DataIterator m_streamCurrent;
    SVGPathByteStream::DataIterator m_streamEnd;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGPathByteStreamSource_h
