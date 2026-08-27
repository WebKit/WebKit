/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorOverlayConfigParser.h"

#include "ColorTypes.h"
#include "FloatQuad.h"
#include <wtf/JSONValues.h>

namespace WebCore {

std::optional<Color> parseInspectorOverlayConfigColor(RefPtr<JSON::Object>&& colorObject)
{
    if (!colorObject)
        return std::nullopt;

    auto r = colorObject->getInteger("r"_s);
    auto g = colorObject->getInteger("g"_s);
    auto b = colorObject->getInteger("b"_s);
    if (!r || !g || !b)
        return std::nullopt;

    auto a = colorObject->getDouble("a"_s);
    if (!a)
        return { makeFromComponentsClamping<SRGBA<uint8_t>>(*r, *g, *b) };
    return { makeFromComponentsClampingExceptAlpha<SRGBA<uint8_t>>(*r, *g, *b, convertFloatAlphaTo<uint8_t>(*a)) };
}

static std::optional<Color> parseRequiredConfigColor(const String& fieldName, JSON::Object& configObject)
{
    return parseInspectorOverlayConfigColor(configObject.getObject(fieldName));
}

static Color parseOptionalConfigColor(const String& fieldName, JSON::Object& configObject)
{
    return parseRequiredConfigColor(fieldName, configObject).value_or(Color::transparentBlack);
}

bool parseInspectorQuad(Ref<JSON::Array>&& quadArray, FloatQuad* quad)
{
    std::array<double, 8> coordinates { };
    if (quadArray->length() != coordinates.size())
        return false;
    for (size_t i = 0; i < coordinates.size(); ++i) {
        auto coordinate = quadArray->get(i)->asDouble();
        if (!coordinate)
            return false;
        coordinates[i] = *coordinate;
    }
    quad->setP1(FloatPoint(coordinates[0], coordinates[1]));
    quad->setP2(FloatPoint(coordinates[2], coordinates[3]));
    quad->setP3(FloatPoint(coordinates[4], coordinates[5]));
    quad->setP4(FloatPoint(coordinates[6], coordinates[7]));

    return true;
}

Inspector::CommandResult<std::unique_ptr<InspectorOverlay::Highlight::Config>> highlightConfigFromInspectorObject(RefPtr<JSON::Object>&& highlightInspectorObject)
{
    if (!highlightInspectorObject)
        return makeUnexpected("Internal error: highlight configuration parameter is missing"_s);

    auto highlightConfig = makeUnique<InspectorOverlay::Highlight::Config>();
    highlightConfig->showInfo = highlightInspectorObject->getBoolean("showInfo"_s).value_or(false);
    highlightConfig->content = parseOptionalConfigColor("contentColor"_s, *highlightInspectorObject);
    highlightConfig->padding = parseOptionalConfigColor("paddingColor"_s, *highlightInspectorObject);
    highlightConfig->border = parseOptionalConfigColor("borderColor"_s, *highlightInspectorObject);
    highlightConfig->margin = parseOptionalConfigColor("marginColor"_s, *highlightInspectorObject);
    return highlightConfig;
}

Inspector::CommandResult<std::optional<InspectorOverlay::Grid::Config>> gridOverlayConfigFromInspectorObject(RefPtr<JSON::Object>&& gridOverlayInspectorObject)
{
    if (!gridOverlayInspectorObject)
        return { std::nullopt };

    auto gridColor = parseRequiredConfigColor("gridColor"_s, *gridOverlayInspectorObject);
    if (!gridColor)
        return makeUnexpected("Internal error: grid color property of grid overlay configuration parameter is missing"_s);

    InspectorOverlay::Grid::Config gridOverlayConfig;
    gridOverlayConfig.gridColor = *gridColor;
    gridOverlayConfig.showLineNames = gridOverlayInspectorObject->getBoolean("showLineNames"_s).value_or(false);
    gridOverlayConfig.showLineNumbers = gridOverlayInspectorObject->getBoolean("showLineNumbers"_s).value_or(false);
    gridOverlayConfig.showExtendedGridLines = gridOverlayInspectorObject->getBoolean("showExtendedGridLines"_s).value_or(false);
    gridOverlayConfig.showTrackSizes = gridOverlayInspectorObject->getBoolean("showTrackSizes"_s).value_or(false);
    gridOverlayConfig.showAreaNames = gridOverlayInspectorObject->getBoolean("showAreaNames"_s).value_or(false);
    gridOverlayConfig.showOrderNumbers = gridOverlayInspectorObject->getBoolean("showOrderNumbers"_s).value_or(false);
    return { WTF::move(gridOverlayConfig) };
}

Inspector::CommandResult<std::optional<InspectorOverlay::Flex::Config>> flexOverlayConfigFromInspectorObject(RefPtr<JSON::Object>&& flexOverlayInspectorObject)
{
    if (!flexOverlayInspectorObject)
        return { std::nullopt };

    auto flexColor = parseRequiredConfigColor("flexColor"_s, *flexOverlayInspectorObject);
    if (!flexColor)
        return makeUnexpected("Internal error: flex color property of flex overlay configuration parameter is missing"_s);

    InspectorOverlay::Flex::Config flexOverlayConfig;
    flexOverlayConfig.flexColor = *flexColor;
    flexOverlayConfig.showOrderNumbers = flexOverlayInspectorObject->getBoolean("showOrderNumbers"_s).value_or(false);
    return { WTF::move(flexOverlayConfig) };
}

} // namespace WebCore
