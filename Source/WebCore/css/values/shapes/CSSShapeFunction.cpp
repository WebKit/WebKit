/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "CSSShapeFunction.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

void Serialize<ToPosition>::operator()(StringBuilder& builder, const ToPosition& value)
{
    // <to-position> = to <position>

    builder.append(nameLiteralForSerialization(value.affinity.value), ' ');
    serializationForCSS(builder, value.offset);
}

void Serialize<ByCoordinatePair>::operator()(StringBuilder& builder, const ByCoordinatePair& value)
{
    // <by-coordinate-pair> = by <coordinate-pair>

    builder.append(nameLiteralForSerialization(value.affinity.value), ' ');
    serializationForCSS(builder, value.offset);
}

void Serialize<RelativeControlPoint>::operator()(StringBuilder& builder, const RelativeControlPoint& value)
{
    // <relative-control-point> = [<coordinate-pair> [from [start | end | origin]]?]
    // Specified https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    serializationForCSS(builder, value.offset);

    if (value.anchor) {
        builder.append(' ', nameLiteralForSerialization(CSSValueFrom), ' ');
        serializationForCSS(builder, *value.anchor);
    }
}

void Serialize<AbsoluteControlPoint>::operator()(StringBuilder& builder, const AbsoluteControlPoint& value)
{
    // <to-control-point> = [<position> | <relative-control-point>]
    // Specified https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    // Representation diverges from grammar due to overlap between <position> and <relative-control-point>.

    serializationForCSS(builder, value.offset);

    if (value.anchor) {
        builder.append(' ', nameLiteralForSerialization(CSSValueFrom), ' ');
        serializationForCSS(builder, *value.anchor);
    }
}

void Serialize<MoveCommand>::operator()(StringBuilder& builder, const MoveCommand& value)
{
    // <move-command> = move [to <position>] | [by <coordinate-pair>]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-move-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    builder.append(nameLiteralForSerialization(value.name), ' ');
    serializationForCSS(builder, value.toBy);
}

void Serialize<LineCommand>::operator()(StringBuilder& builder, const LineCommand& value)
{
    // <line-command> = line [to <position>] | [by <coordinate-pair>]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-line-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    builder.append(nameLiteralForSerialization(value.name), ' ');
    serializationForCSS(builder, value.toBy);
}

void Serialize<HLineCommand::To>::operator()(StringBuilder& builder, const HLineCommand::To& value)
{
    builder.append(nameLiteralForSerialization(value.affinity.value), ' ');
    serializationForCSS(builder, value.offset);
}

void Serialize<HLineCommand::By>::operator()(StringBuilder& builder, const HLineCommand::By& value)
{
    builder.append(nameLiteralForSerialization(value.affinity.value), ' ');
    serializationForCSS(builder, value.offset);
}

void Serialize<HLineCommand>::operator()(StringBuilder& builder, const HLineCommand& value)
{
    // <horizontal-line-command> = hline [ to [ <length-percentage> | left | center | right | x-start | x-end ] | by <length-percentage> ]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-hv-line-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2426552611

    builder.append(nameLiteralForSerialization(value.name), ' ');
    serializationForCSS(builder, value.toBy);
}

void Serialize<VLineCommand::To>::operator()(StringBuilder& builder, const VLineCommand::To& value)
{
    builder.append(nameLiteralForSerialization(value.affinity.value), ' ');
    serializationForCSS(builder, value.offset);
}

void Serialize<VLineCommand::By>::operator()(StringBuilder& builder, const VLineCommand::By& value)
{
    builder.append(nameLiteralForSerialization(value.affinity.value), ' ');
    serializationForCSS(builder, value.offset);
}

void Serialize<VLineCommand>::operator()(StringBuilder& builder, const VLineCommand& value)
{
    // <vertical-line-command> = vline [ to [ <length-percentage> | top | center | bottom | y-start | y-end ] | by <length-percentage> ]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-hv-line-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2426552611

    builder.append(nameLiteralForSerialization(value.name), ' ');
    serializationForCSS(builder, value.toBy);
}

void Serialize<CurveCommand::To>::operator()(StringBuilder& builder, const CurveCommand::To& value)
{
    builder.append(nameLiteralForSerialization(value.affinity.value), ' ');
    serializationForCSS(builder, value.offset);

    builder.append(' ', nameLiteralForSerialization(CSSValueWith), ' ');
    serializationForCSS(builder, value.controlPoint1);
    if (value.controlPoint2) {
        builder.append(" / "_s);
        serializationForCSS(builder, *value.controlPoint2);
    }
}

void Serialize<CurveCommand::By>::operator()(StringBuilder& builder, const CurveCommand::By& value)
{
    builder.append(nameLiteralForSerialization(value.affinity.value), ' ');
    serializationForCSS(builder, value.offset);

    builder.append(' ', nameLiteralForSerialization(CSSValueWith), ' ');
    serializationForCSS(builder, value.controlPoint1);
    if (value.controlPoint2) {
        builder.append(" / "_s);
        serializationForCSS(builder, *value.controlPoint2);
    }
}

void Serialize<CurveCommand>::operator()(StringBuilder& builder, const CurveCommand& value)
{
    // <curve-command> = curve [to <position> with <to-control-point> [/ <to-control-point>]?]
    //                       | [by <coordinate-pair> with <relative-control-point> [/ <relative-control-point>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-curve-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    builder.append(nameLiteralForSerialization(value.name), ' ');
    serializationForCSS(builder, value.toBy);
}

void Serialize<SmoothCommand::To>::operator()(StringBuilder& builder, const SmoothCommand::To& value)
{
    builder.append(nameLiteralForSerialization(value.affinity.value), ' ');
    serializationForCSS(builder, value.offset);

    if (value.controlPoint) {
        builder.append(' ', nameLiteralForSerialization(CSSValueWith), ' ');
        serializationForCSS(builder, *value.controlPoint);
    }
}

void Serialize<SmoothCommand::By>::operator()(StringBuilder& builder, const SmoothCommand::By& value)
{
    builder.append(nameLiteralForSerialization(value.affinity.value), ' ');
    serializationForCSS(builder, value.offset);

    if (value.controlPoint) {
        builder.append(' ', nameLiteralForSerialization(CSSValueWith), ' ');
        serializationForCSS(builder, *value.controlPoint);
    }
}

void Serialize<SmoothCommand>::operator()(StringBuilder& builder, const SmoothCommand& value)
{
    // <smooth-command> = smooth [to <position> [with <to-control-point>]?]
    //                         | [by <coordinate-pair> [with <relative-control-point>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-smooth-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    builder.append(nameLiteralForSerialization(value.name), ' ');
    serializationForCSS(builder, value.toBy);
}

void Serialize<ArcCommand>::operator()(StringBuilder& builder, const ArcCommand& value)
{
    // <arc-command> = arc [to <position>] | [by <coordinate-pair>] of <length-percentage>{1,2} [<arc-sweep>? || <arc-size>? || [rotate <angle>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-arc-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    builder.append(nameLiteralForSerialization(value.name), ' ');
    serializationForCSS(builder, value.toBy);

    builder.append(' ', nameLiteralForSerialization(CSSValueOf), ' ');
    if (value.size.width() == value.size.height())
        serializationForCSS(builder, value.size.width());
    else
        serializationForCSS(builder, value.size);

    if (!std::holds_alternative<Ccw>(value.arcSweep)) {
        builder.append(' ');
        serializationForCSS(builder, value.arcSweep);
    }

    if (!std::holds_alternative<Small>(value.arcSize)) {
        builder.append(' ');
        serializationForCSS(builder, value.arcSize);
    }

    if (value.rotation != Angle<> { AngleRaw<> { CSSUnitType::CSS_DEG, 0 } }) {
        builder.append(' ', nameLiteralForSerialization(CSSValueRotate), ' ');
        serializationForCSS(builder, value.rotation);
    }
}

void Serialize<Shape>::operator()(StringBuilder& builder, const Shape& value)
{
    // shape() = shape( <'fill-rule'>? from <coordinate-pair>, <shape-command>#)

    if (value.fillRule && !std::holds_alternative<Nonzero>(*value.fillRule)) {
        serializationForCSS(builder, *value.fillRule);
        builder.append(' ');
    }

    builder.append(nameLiteralForSerialization(CSSValueFrom), ' ');
    serializationForCSS(builder, value.startingPoint);
    builder.append(", "_s);
    serializationForCSS(builder, value.commands);
}

} // namespace CSS
} // namespace WebCore
