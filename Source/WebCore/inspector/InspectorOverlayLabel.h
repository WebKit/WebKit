/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Color.h"
#include "FloatPoint.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FloatSize;
class GraphicsContext;
class Path;

class InspectorOverlayLabel {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct Arrow {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        enum class Direction : uint8_t {
            None,
            Down,
            Up,
            Left,
            Right,
        };

        enum class Alignment : uint8_t {
            None,
            Leading, // Positioned at the left/top side of edge.
            Middle, // Positioned at the center on the edge.
            Trailing, // Positioned at the right/bottom side of the edge.
        };

        Direction direction;
        Alignment alignment;

        Arrow(Direction direction, Alignment alignment)
            : direction(direction)
            , alignment(alignment)
        {
            ASSERT(alignment != Alignment::None || direction == Direction::None);
        }

#if PLATFORM(IOS_FAMILY)
        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<InspectorOverlayLabel::Arrow> decode(Decoder&);
#endif
    };

    struct Content {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        struct Decoration {
            enum class Type : uint8_t {
                None,
                Bordered,
            };

            Type type;
            Color color;

#if PLATFORM(IOS_FAMILY)
            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<InspectorOverlayLabel::Content::Decoration> decode(Decoder&);
#endif
        };

        String text;
        Color textColor;
        Decoration decoration { Decoration::Type::None, Color::transparentBlack };

#if PLATFORM(IOS_FAMILY)
        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<InspectorOverlayLabel::Content> decode(Decoder&);
#endif
    };

    WEBCORE_EXPORT InspectorOverlayLabel(Vector<Content>&&, FloatPoint, Color backgroundColor, Arrow);
    InspectorOverlayLabel(const String&, FloatPoint, Color backgroundColor, Arrow);

    Path draw(GraphicsContext&, float maximumLineWidth = 0);

    static FloatSize expectedSize(const Vector<Content>&, Arrow::Direction);
    static FloatSize expectedSize(const String&, Arrow::Direction);

#if PLATFORM(IOS_FAMILY)
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<InspectorOverlayLabel> decode(Decoder&);
#endif

private:
    Vector<Content> m_contents;
    FloatPoint m_location;
    Color m_backgroundColor;
    Arrow m_arrow;
};

#if PLATFORM(IOS_FAMILY)

template<class Encoder> void InspectorOverlayLabel::encode(Encoder& encoder) const
{
    encoder << m_contents;
    encoder << m_location;
    encoder << m_backgroundColor;
    encoder << m_arrow;
}

template<class Decoder> std::optional<InspectorOverlayLabel> InspectorOverlayLabel::decode(Decoder& decoder)
{
    std::optional<Vector<Content>> contents;
    decoder >> contents;
    if (!contents)
        return std::nullopt;

    std::optional<FloatPoint> location;
    decoder >> location;
    if (!location)
        return std::nullopt;

    std::optional<Color> backgroundColor;
    decoder >> backgroundColor;
    if (!backgroundColor)
        return std::nullopt;

    std::optional<Arrow> arrow;
    decoder >> arrow;
    if (!arrow)
        return std::nullopt;

    return { {
        WTFMove(*contents),
        *location,
        *backgroundColor,
        *arrow
    } };
}

template<class Encoder> void InspectorOverlayLabel::Arrow::encode(Encoder& encoder) const
{
    encoder << direction;
    encoder << alignment;
}

template<class Decoder> std::optional<InspectorOverlayLabel::Arrow> InspectorOverlayLabel::Arrow::decode(Decoder& decoder)
{
    std::optional<Direction> direction;
    decoder >> direction;
    if (!direction)
        return std::nullopt;

    std::optional<Alignment> alignment;
    decoder >> alignment;
    if (!alignment)
        return std::nullopt;

    return { { *direction, *alignment } };
}

template<class Encoder> void InspectorOverlayLabel::Content::encode(Encoder& encoder) const
{
    encoder << text;
    encoder << textColor;
    encoder << decoration;
}

template<class Decoder> std::optional<InspectorOverlayLabel::Content> InspectorOverlayLabel::Content::decode(Decoder& decoder)
{
    std::optional<String> text;
    decoder >> text;
    if (!text)
        return std::nullopt;

    std::optional<Color> textColor;
    decoder >> textColor;
    if (!textColor)
        return std::nullopt;

    std::optional<Decoration> decoration;
    decoder >> decoration;
    if (!decoration)
        return std::nullopt;

    return { { *text, *textColor, *decoration } };
}

template<class Encoder> void InspectorOverlayLabel::Content::Decoration::encode(Encoder& encoder) const
{
    encoder << type;
    encoder << color;
}

template<class Decoder> std::optional<InspectorOverlayLabel::Content::Decoration> InspectorOverlayLabel::Content::Decoration::decode(Decoder& decoder)
{
    std::optional<Type> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;

    return { { *type, *color } };
}

#endif

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::InspectorOverlayLabel::Arrow::Direction> {
    using values = EnumValues<
        WebCore::InspectorOverlayLabel::Arrow::Direction,
        WebCore::InspectorOverlayLabel::Arrow::Direction::None,
        WebCore::InspectorOverlayLabel::Arrow::Direction::Down,
        WebCore::InspectorOverlayLabel::Arrow::Direction::Up,
        WebCore::InspectorOverlayLabel::Arrow::Direction::Left,
        WebCore::InspectorOverlayLabel::Arrow::Direction::Right
    >;
};

template<> struct EnumTraits<WebCore::InspectorOverlayLabel::Arrow::Alignment> {
    using values = EnumValues<
        WebCore::InspectorOverlayLabel::Arrow::Alignment,
        WebCore::InspectorOverlayLabel::Arrow::Alignment::None,
        WebCore::InspectorOverlayLabel::Arrow::Alignment::Leading,
        WebCore::InspectorOverlayLabel::Arrow::Alignment::Middle,
        WebCore::InspectorOverlayLabel::Arrow::Alignment::Trailing
    >;
};

template<> struct EnumTraits<WebCore::InspectorOverlayLabel::Content::Decoration::Type> {
    using values = EnumValues<
        WebCore::InspectorOverlayLabel::Content::Decoration::Type,
        WebCore::InspectorOverlayLabel::Content::Decoration::Type::None,
        WebCore::InspectorOverlayLabel::Content::Decoration::Type::Bordered
    >;
};

}
