/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022-2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace IPC {
template<typename T, typename U> struct ArgumentCoder;
}

namespace WebCore {

namespace DMABufFormatImpl {

static constexpr uint32_t createFourCC(char a, char b, char c, char d)
{
    return uint32_t(a) | (uint32_t(b) << 8) | (uint32_t(c) << 16) | (uint32_t(d) << 24);
}

}

struct DMABufFormat {
    enum class FourCC : uint32_t {
        Invalid = 0,

        R8 = DMABufFormatImpl::createFourCC('R', '8', ' ', ' '),
        GR88 = DMABufFormatImpl::createFourCC('G', 'R', '8', '8'),
        R16 = DMABufFormatImpl::createFourCC('R', '1', '6', ' '),
        GR32 = DMABufFormatImpl::createFourCC('G', 'R', '3', '2'),

        XRGB8888 = DMABufFormatImpl::createFourCC('X', 'R', '2', '4'),
        XBGR8888 = DMABufFormatImpl::createFourCC('X', 'B', '2', '4'),
        RGBX8888 = DMABufFormatImpl::createFourCC('R', 'X', '2', '4'),
        BGRX8888 = DMABufFormatImpl::createFourCC('B', 'X', '2', '4'),
        ARGB8888 = DMABufFormatImpl::createFourCC('A', 'R', '2', '4'),
        ABGR8888 = DMABufFormatImpl::createFourCC('A', 'B', '2', '4'),
        RGBA8888 = DMABufFormatImpl::createFourCC('R', 'A', '2', '4'),
        BGRA8888 = DMABufFormatImpl::createFourCC('B', 'A', '2', '4'),
        RGB888 = DMABufFormatImpl::createFourCC('R', 'G', '2', '4'),
        BGR888 = DMABufFormatImpl::createFourCC('B', 'G', '2', '4'),

        I420 = DMABufFormatImpl::createFourCC('I', '4', '2', '0'),
        YV12 = DMABufFormatImpl::createFourCC('Y', 'V', '1', '2'),
        A420 = DMABufFormatImpl::createFourCC('A', '4', '2', '0'),
        NV12 = DMABufFormatImpl::createFourCC('N', 'V', '1', '2'),
        NV21 = DMABufFormatImpl::createFourCC('N', 'V', '2', '1'),

        YUY2 = DMABufFormatImpl::createFourCC('Y', 'U', 'Y', '2'),
        YVYU = DMABufFormatImpl::createFourCC('Y', 'V', 'Y', 'U'),
        UYVY = DMABufFormatImpl::createFourCC('U', 'Y', 'V', 'Y'),
        VYUY = DMABufFormatImpl::createFourCC('V', 'Y', 'U', 'Y'),
        VUYA = DMABufFormatImpl::createFourCC('V', 'U', 'Y', 'A'),
        AYUV = DMABufFormatImpl::createFourCC('A', 'Y', 'U', 'V'),

        Y444 = DMABufFormatImpl::createFourCC('Y', '4', '4', '4'),
        Y41B = DMABufFormatImpl::createFourCC('Y', '4', '1', 'B'),
        Y42B = DMABufFormatImpl::createFourCC('Y', '4', '2', 'B'),

        P010 = DMABufFormatImpl::createFourCC('P', '0', '1', '0'),
        P016 = DMABufFormatImpl::createFourCC('P', '0', '1', '6'),
    };

    enum class Modifier : uint64_t {
        Invalid = ((1ULL << 56) - 1),
    };

    static constexpr unsigned c_maxPlanes = 4;

    template<FourCC> static DMABufFormat create() = delete;
    static DMABufFormat create(uint32_t);

    template<FourCC fourccValue, typename... PlaneDefinitionTypes>
    static DMABufFormat instantiate();

    FourCC fourcc { FourCC::Invalid };
    unsigned numPlanes { 0 };

    uint32_t planeWidth(unsigned planeIndex, uint32_t width) const
    {
        ASSERT(planeIndex < c_maxPlanes);
        return (width >> planes[planeIndex].horizontalSubsampling);
    }

    uint32_t planeHeight(unsigned planeIndex, uint32_t height) const
    {
        ASSERT(planeIndex < c_maxPlanes);
        return (height >> planes[planeIndex].verticalSubsampling);
    }

    template<FourCC fourccValue, unsigned hsValue, unsigned vsValue>
    struct PlaneDefinition {
        static constexpr FourCC fourcc = fourccValue;
        static constexpr unsigned horizontalSubsampling = hsValue;
        static constexpr unsigned verticalSubsampling = vsValue;
    };

    struct Plane {
        Plane() = default;

        template<typename PlaneDefinitionType>
        Plane(const PlaneDefinitionType&)
            : fourcc(PlaneDefinitionType::fourcc)
            , horizontalSubsampling(PlaneDefinitionType::horizontalSubsampling)
            , verticalSubsampling(PlaneDefinitionType::verticalSubsampling)
        { }

        FourCC fourcc { FourCC::Invalid };
        unsigned horizontalSubsampling { 0 };
        unsigned verticalSubsampling { 0 };

    private:
        Plane(const FourCC& fourcc, const unsigned& hsValue, const unsigned& vsValue)
            : fourcc(fourcc)
            , horizontalSubsampling(hsValue)
            , verticalSubsampling(vsValue)
        { }

        friend struct IPC::ArgumentCoder<Plane, void>;
    };
    std::array<Plane, c_maxPlanes> planes;
};

namespace DMABufFormatImpl {

template<DMABufFormat::FourCC fourccValue>
inline DMABufFormat createSinglePlaneRGBA()
{
    return DMABufFormat::instantiate<fourccValue,
        DMABufFormat::PlaneDefinition<fourccValue, 0, 0>>();
}

template<size_t Index, size_t Size, typename PlaneDefinitionType, typename... PlaneDefinitionTypes>
void definePlane(std::array<DMABufFormat::Plane, DMABufFormat::c_maxPlanes>& planes)
{
    new (&planes[Index]) DMABufFormat::Plane(PlaneDefinitionType { });
    if constexpr ((Index + 1) < Size)
        definePlane<Index + 1, Size, PlaneDefinitionTypes...>(planes);
}

}

template<DMABufFormat::FourCC fourccValue, typename... PlaneDefinitionTypes>
inline DMABufFormat DMABufFormat::instantiate()
{
    static_assert(sizeof...(PlaneDefinitionTypes) <= 4);

    DMABufFormat format;
    format.fourcc = fourccValue;
    format.numPlanes = sizeof...(PlaneDefinitionTypes);
    DMABufFormatImpl::definePlane<0, sizeof...(PlaneDefinitionTypes), PlaneDefinitionTypes...>(format.planes);
    return format;
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::XRGB8888>()
{
    return DMABufFormatImpl::createSinglePlaneRGBA<FourCC::XRGB8888>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::XBGR8888>()
{
    return DMABufFormatImpl::createSinglePlaneRGBA<FourCC::XBGR8888>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::RGBX8888>()
{
    return DMABufFormatImpl::createSinglePlaneRGBA<FourCC::RGBX8888>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::BGRX8888>()
{
    return DMABufFormatImpl::createSinglePlaneRGBA<FourCC::BGRX8888>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::ARGB8888>()
{
    return DMABufFormatImpl::createSinglePlaneRGBA<FourCC::ARGB8888>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::ABGR8888>()
{
    return DMABufFormatImpl::createSinglePlaneRGBA<FourCC::ABGR8888>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::RGBA8888>()
{
    return DMABufFormatImpl::createSinglePlaneRGBA<FourCC::RGBA8888>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::BGRA8888>()
{
    return DMABufFormatImpl::createSinglePlaneRGBA<FourCC::BGRA8888>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::I420>()
{
    return DMABufFormat::instantiate<FourCC::I420,
        PlaneDefinition<FourCC::R8, 0, 0>,
        PlaneDefinition<FourCC::R8, 1, 1>,
        PlaneDefinition<FourCC::R8, 1, 1>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::YV12>()
{
    return DMABufFormat::instantiate<FourCC::YV12,
        PlaneDefinition<FourCC::R8, 0, 0>,
        PlaneDefinition<FourCC::R8, 1, 1>,
        PlaneDefinition<FourCC::R8, 1, 1>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::A420>()
{
    return DMABufFormat::instantiate<FourCC::A420,
        PlaneDefinition<FourCC::R8, 0, 0>,
        PlaneDefinition<FourCC::R8, 1, 1>,
        PlaneDefinition<FourCC::R8, 1, 1>,
        PlaneDefinition<FourCC::R8, 0, 0>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::NV12>()
{
    return DMABufFormat::instantiate<FourCC::NV12,
        PlaneDefinition<FourCC::R8, 0, 0>,
        PlaneDefinition<FourCC::GR88, 1, 1>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::NV21>()
{
    return DMABufFormat::instantiate<FourCC::NV21,
        PlaneDefinition<FourCC::R8, 0, 0>,
        PlaneDefinition<FourCC::GR88, 1, 1>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::YUY2>()
{
    return DMABufFormat::instantiate<FourCC::YUY2,
        PlaneDefinition<FourCC::GR88, 0, 0>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::YVYU>()
{
    return DMABufFormat::instantiate<FourCC::YVYU,
        PlaneDefinition<FourCC::GR88, 0, 0>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::UYVY>()
{
    return DMABufFormat::instantiate<FourCC::UYVY,
        PlaneDefinition<FourCC::GR88, 0, 0>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::VYUY>()
{
    return DMABufFormat::instantiate<FourCC::VYUY,
        PlaneDefinition<FourCC::GR88, 0, 0>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::VUYA>()
{
    return DMABufFormat::instantiate<FourCC::VUYA,
        PlaneDefinition<FourCC::ABGR8888, 0, 0>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::AYUV>()
{
    return DMABufFormat::instantiate<FourCC::AYUV,
        PlaneDefinition<FourCC::ABGR8888, 0, 0>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::Y444>()
{
    return DMABufFormat::instantiate<FourCC::Y444,
        PlaneDefinition<FourCC::R8, 0, 0>,
        PlaneDefinition<FourCC::R8, 0, 0>,
        PlaneDefinition<FourCC::R8, 0, 0>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::Y41B>()
{
    return DMABufFormat::instantiate<FourCC::Y41B,
        PlaneDefinition<FourCC::R8, 0, 0>,
        PlaneDefinition<FourCC::R8, 2, 0>,
        PlaneDefinition<FourCC::R8, 2, 0>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::Y42B>()
{
    return DMABufFormat::instantiate<FourCC::Y42B,
        PlaneDefinition<FourCC::R8, 0, 0>,
        PlaneDefinition<FourCC::R8, 1, 0>,
        PlaneDefinition<FourCC::R8, 1, 0>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::P010>()
{
    return DMABufFormat::instantiate<FourCC::P010,
        PlaneDefinition<FourCC::R16, 0, 0>,
        PlaneDefinition<FourCC::GR32, 1, 1>>();
}

template<>
inline DMABufFormat DMABufFormat::create<DMABufFormat::FourCC::P016>()
{
    return DMABufFormat::instantiate<FourCC::P010,
        PlaneDefinition<FourCC::R16, 0, 0>,
        PlaneDefinition<FourCC::GR32, 1, 1>>();
}

inline DMABufFormat DMABufFormat::create(uint32_t fourccValue)
{
#define CREATE_FORMAT_FOR_FOURCC(FourCCValue) \
    case uint32_t(FourCC::FourCCValue): \
        return create<FourCC::FourCCValue>();

    switch (fourccValue) {
    CREATE_FORMAT_FOR_FOURCC(XRGB8888);
    CREATE_FORMAT_FOR_FOURCC(XBGR8888);
    CREATE_FORMAT_FOR_FOURCC(RGBX8888);
    CREATE_FORMAT_FOR_FOURCC(BGRX8888);
    CREATE_FORMAT_FOR_FOURCC(ARGB8888);
    CREATE_FORMAT_FOR_FOURCC(ABGR8888);
    CREATE_FORMAT_FOR_FOURCC(RGBA8888);
    CREATE_FORMAT_FOR_FOURCC(BGRA8888);
    CREATE_FORMAT_FOR_FOURCC(I420);
    CREATE_FORMAT_FOR_FOURCC(YV12);
    CREATE_FORMAT_FOR_FOURCC(A420);
    CREATE_FORMAT_FOR_FOURCC(NV12);
    CREATE_FORMAT_FOR_FOURCC(NV21);
    CREATE_FORMAT_FOR_FOURCC(YUY2);
    CREATE_FORMAT_FOR_FOURCC(YVYU);
    CREATE_FORMAT_FOR_FOURCC(UYVY);
    CREATE_FORMAT_FOR_FOURCC(VYUY);
    CREATE_FORMAT_FOR_FOURCC(VUYA);
    CREATE_FORMAT_FOR_FOURCC(AYUV);
    CREATE_FORMAT_FOR_FOURCC(Y444);
    CREATE_FORMAT_FOR_FOURCC(Y41B);
    CREATE_FORMAT_FOR_FOURCC(Y42B);
    CREATE_FORMAT_FOR_FOURCC(P010);
    CREATE_FORMAT_FOR_FOURCC(P016);
    default:
        break;
    }

#undef CREATE_FORMAT_FOR_FOURCC

    return { };
}

} // namespace WebCore
