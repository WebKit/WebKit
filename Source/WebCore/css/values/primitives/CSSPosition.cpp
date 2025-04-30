/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPosition.h"

namespace WebCore {
namespace CSS {

bool isCenterPosition(const Position& position)
{
    auto isCenter = [](const auto& component) {
        return WTF::switchOn(component.offset,
            [](auto)            { return false; },
            [](Keyword::Center) { return true;  },
            [](const LengthPercentage<>& value) {
                return WTF::switchOn(value,
                    [](const LengthPercentage<>::Raw& raw) { return raw == 50_css_percentage; },
                    [](const LengthPercentage<>::Calc&) { return false; }
                );
            }
        );
    };

    return WTF::switchOn(position,
        [&](const TwoComponentPositionHorizontalVertical& components) {
            return isCenter(get<0>(components)) && isCenter(get<1>(components));
        },
        [&](const auto&) {
            return false;
        }
    );
}

static auto toTwoComponent(const ThreeComponentPositionHorizontal& component) -> TwoComponentPositionHorizontal
{
    return WTF::switchOn(component.offset, [](auto keyword) -> TwoComponentPositionHorizontal { return { keyword }; });
}

static auto toTwoComponent(const ThreeComponentPositionVertical& component) -> TwoComponentPositionVertical
{
    return WTF::switchOn(component.offset, [](auto keyword) -> TwoComponentPositionVertical { return { keyword }; });
}

std::pair<CSS::PositionX, CSS::PositionY> split(CSS::PositionXY&& position)
{
    // CSS::PositionX and CSS::PositionY don't utilize the three component variants, so the
    // non-length containing item must be converted to its two component variant.

    return WTF::switchOn(WTFMove(position),
        [&](const auto& components) -> std::pair<CSS::PositionX, CSS::PositionY> {
            return { CSS::PositionX(get<0>(components)), CSS::PositionY(get<1>(components)) };
        },
        [&](const ThreeComponentPositionHorizontalVerticalLengthFirst& components) -> std::pair<CSS::PositionX, CSS::PositionY> {
            return { CSS::PositionX(get<0>(components)), CSS::PositionY(toTwoComponent(get<1>(components))) };
        },
        [&](const ThreeComponentPositionHorizontalVerticalLengthSecond& components) -> std::pair<CSS::PositionX, CSS::PositionY> {
            return { CSS::PositionX(toTwoComponent(get<0>(components))), CSS::PositionY(get<1>(components)) };
        }
    );
}

PositionX resolveToPhysicalX(const PositionLogical& position, WritingMode writingMode)
{
    return WTF::switchOn(position.value,
        [&](const TwoComponentPositionLogical& components) -> PositionX {
            return WTF::switchOn(components.offset,
                [&](Keyword::Start) -> PositionX {
                    return writingMode.isAnyLeftToRight()
                        ? TwoComponentPositionHorizontal { Keyword::Left { } }
                        : TwoComponentPositionHorizontal { Keyword::Right { } };
                },
                [&](Keyword::End) -> PositionX {
                    return writingMode.isAnyLeftToRight()
                        ? TwoComponentPositionHorizontal { Keyword::Right { } }
                        : TwoComponentPositionHorizontal { Keyword::Left { } };
                },
                [&](Keyword::Center) -> PositionX {
                    return TwoComponentPositionHorizontal { Keyword::Center { } };
                },
                [&](const LengthPercentage<>& length) -> PositionX {
                    return writingMode.isAnyLeftToRight()
                        ? FourComponentPositionHorizontal { { Keyword::Left { }, length } }
                        : FourComponentPositionHorizontal { { Keyword::Right { }, length } };
                }
            );
        },
        [&](const FourComponentPositionLogical& components) -> PositionX {
            return WTF::switchOn(get<0>(components.offset),
                [&](Keyword::Start) -> PositionX {
                    return writingMode.isAnyLeftToRight()
                        ? FourComponentPositionHorizontal { { Keyword::Left { }, get<1>(components.offset) } }
                        : FourComponentPositionHorizontal { { Keyword::Right { }, get<1>(components.offset) } };
                },
                [&](Keyword::End) -> PositionX {
                    return writingMode.isAnyLeftToRight()
                        ? FourComponentPositionHorizontal { { Keyword::Right { }, get<1>(components.offset) } }
                        : FourComponentPositionHorizontal { { Keyword::Left { }, get<1>(components.offset) } };
                }
            );
        }
    );
}

PositionY resolveToPhysicalY(const PositionLogical& position, WritingMode writingMode)
{
    return WTF::switchOn(position.value,
        [&](const TwoComponentPositionLogical& components) -> PositionY {
            return WTF::switchOn(components.offset,
                [&](Keyword::Start) -> PositionY {
                    return writingMode.isAnyTopToBottom()
                        ? TwoComponentPositionVertical { Keyword::Top { } }
                        : TwoComponentPositionVertical { Keyword::Bottom { } };
                },
                [&](Keyword::End) -> PositionY {
                    return writingMode.isAnyTopToBottom()
                        ? TwoComponentPositionVertical { Keyword::Bottom { } }
                        : TwoComponentPositionVertical { Keyword::Top { } };
                },
                [&](Keyword::Center) -> PositionY {
                    return TwoComponentPositionVertical { Keyword::Center { } };
                },
                [&](const LengthPercentage<>& length) -> PositionY {
                    return writingMode.isAnyTopToBottom()
                        ? FourComponentPositionVertical { { Keyword::Top { }, length } }
                        : FourComponentPositionVertical { { Keyword::Bottom { }, length } };
                }
            );
        },
        [&](const FourComponentPositionLogical& components) -> PositionY {
            return WTF::switchOn(get<0>(components.offset),
                [&](Keyword::Start) -> PositionY {
                    return writingMode.isAnyTopToBottom()
                        ? FourComponentPositionVertical { { Keyword::Top { }, get<1>(components.offset) } }
                        : FourComponentPositionVertical { { Keyword::Bottom { }, get<1>(components.offset) } };
                },
                [&](Keyword::End) -> PositionY {
                    return writingMode.isAnyTopToBottom()
                        ? FourComponentPositionVertical { { Keyword::Bottom { }, get<1>(components.offset) } }
                        : FourComponentPositionVertical { { Keyword::Top { }, get<1>(components.offset) } };
                }
            );
        }
    );
}

} // namespace CSS
} // namespace WebCore
