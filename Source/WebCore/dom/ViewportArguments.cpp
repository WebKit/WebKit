/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#include "config.h"
#include "ViewportArguments.h"

#include "Document.h"
#include "Frame.h"
#include "IntSize.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

#if PLATFORM(GTK)
const float ViewportArguments::deprecatedTargetDPI = 160;
#endif

static const float& compareIgnoringAuto(const float& value1, const float& value2, const float& (*compare) (const float&, const float&))
{
    ASSERT(value1 != ViewportArguments::ValueAuto || value2 != ViewportArguments::ValueAuto);

    if (value1 == ViewportArguments::ValueAuto)
        return value2;

    if (value2 == ViewportArguments::ValueAuto)
        return value1;

    return compare(value1, value2);
}

static inline float clampLengthValue(float value)
{
    ASSERT(value != ViewportArguments::ValueDeviceWidth);
    ASSERT(value != ViewportArguments::ValueDeviceHeight);

    // Limits as defined in the css-device-adapt spec.
    if (value != ViewportArguments::ValueAuto)
        return std::min<float>(10000, std::max<float>(value, 1));
    return value;
}

static inline float clampScaleValue(float value)
{
    ASSERT(value != ViewportArguments::ValueDeviceWidth);
    ASSERT(value != ViewportArguments::ValueDeviceHeight);

    // Limits as defined in the css-device-adapt spec.
    if (value != ViewportArguments::ValueAuto)
        return std::min<float>(10, std::max<float>(value, 0.1));
    return value;
}

ViewportAttributes ViewportArguments::resolve(const FloatSize& initialViewportSize, const FloatSize& deviceSize, int defaultWidth) const
{
    float resultWidth = width;
    float resultMaxWidth = maxWidth;
    float resultMinWidth = minWidth;
    float resultHeight = height;
    float resultMinHeight = minHeight;
    float resultMaxHeight = maxHeight;
    float resultZoom = zoom;
    float resultMinZoom = minZoom;
    float resultMaxZoom = maxZoom;

    switch (int(resultWidth)) {
    case ViewportArguments::ValueDeviceWidth:
        resultWidth = deviceSize.width();
        break;
    case ViewportArguments::ValueDeviceHeight:
        resultWidth = deviceSize.height();
        break;
    }

    switch (int(resultHeight)) {
    case ViewportArguments::ValueDeviceWidth:
        resultHeight = deviceSize.width();
        break;
    case ViewportArguments::ValueDeviceHeight:
        resultHeight = deviceSize.height();
        break;
    }

    if (type == ViewportArguments::CSSDeviceAdaptation) {
        switch (int(resultMinWidth)) {
        case ViewportArguments::ValueDeviceWidth:
            resultMinWidth = deviceSize.width();
            break;
        case ViewportArguments::ValueDeviceHeight:
            resultMinWidth = deviceSize.height();
            break;
        }

        switch (int(resultMaxWidth)) {
        case ViewportArguments::ValueDeviceWidth:
            resultMaxWidth = deviceSize.width();
            break;
        case ViewportArguments::ValueDeviceHeight:
            resultMaxWidth = deviceSize.height();
            break;
        }

        switch (int(resultMinHeight)) {
        case ViewportArguments::ValueDeviceWidth:
            resultMinHeight = deviceSize.width();
            break;
        case ViewportArguments::ValueDeviceHeight:
            resultMinHeight = deviceSize.height();
            break;
        }

        switch (int(resultMaxHeight)) {
        case ViewportArguments::ValueDeviceWidth:
            resultMaxHeight = deviceSize.width();
            break;
        case ViewportArguments::ValueDeviceHeight:
            resultMaxHeight = deviceSize.height();
            break;
        }

        if (resultMinWidth != ViewportArguments::ValueAuto || resultMaxWidth != ViewportArguments::ValueAuto)
            resultWidth = compareIgnoringAuto(resultMinWidth, compareIgnoringAuto(resultMaxWidth, deviceSize.width(), std::min), std::max);

        if (resultMinHeight != ViewportArguments::ValueAuto || resultMaxHeight != ViewportArguments::ValueAuto)
            resultHeight = compareIgnoringAuto(resultMinHeight, compareIgnoringAuto(resultMaxHeight, deviceSize.height(), std::min), std::max);

        if (resultMinZoom != ViewportArguments::ValueAuto && resultMaxZoom != ViewportArguments::ValueAuto)
            resultMaxZoom = std::max(resultMinZoom, resultMaxZoom);

        if (resultZoom != ViewportArguments::ValueAuto)
            resultZoom = compareIgnoringAuto(resultMinZoom, compareIgnoringAuto(resultMaxZoom, resultZoom, std::min), std::max);

        if (resultWidth == ViewportArguments::ValueAuto && resultZoom == ViewportArguments::ValueAuto)
            resultWidth = deviceSize.width();

        if (resultWidth == ViewportArguments::ValueAuto && resultHeight == ViewportArguments::ValueAuto)
            resultWidth = deviceSize.width() / resultZoom;

        if (resultWidth == ViewportArguments::ValueAuto)
            resultWidth = resultHeight * deviceSize.width() / deviceSize.height();

        if (resultHeight == ViewportArguments::ValueAuto)
            resultHeight = resultWidth * deviceSize.height() / deviceSize.width();

        if (resultZoom != ViewportArguments::ValueAuto || resultMaxZoom != ViewportArguments::ValueAuto) {
            resultWidth = compareIgnoringAuto(resultWidth, deviceSize.width() / compareIgnoringAuto(resultZoom, resultMaxZoom, std::min), std::max);
            resultHeight = compareIgnoringAuto(resultHeight, deviceSize.height() / compareIgnoringAuto(resultZoom, resultMaxZoom, std::min), std::max);
        }

        resultWidth = std::max<float>(1, resultWidth);
        resultHeight = std::max<float>(1, resultHeight);
    }

    if (type != ViewportArguments::CSSDeviceAdaptation && type != ViewportArguments::Implicit) {
        // Clamp values to a valid range, but not for @viewport since is
        // not mandated by the specification.
        resultWidth = clampLengthValue(resultWidth);
        resultHeight = clampLengthValue(resultHeight);
        resultZoom = clampScaleValue(resultZoom);
        resultMinZoom = clampScaleValue(resultMinZoom);
        resultMaxZoom = clampScaleValue(resultMaxZoom);
    }

    ViewportAttributes result;

    // Resolve minimum-scale and maximum-scale values according to spec.
    if (resultMinZoom == ViewportArguments::ValueAuto)
        result.minimumScale = float(0.25);
    else
        result.minimumScale = resultMinZoom;

    if (resultMaxZoom == ViewportArguments::ValueAuto) {
        result.maximumScale = 5;
        result.minimumScale = std::min<float>(5, result.minimumScale);
    } else
        result.maximumScale = resultMaxZoom;
    result.maximumScale = std::max(result.minimumScale, result.maximumScale);

    // Resolve initial-scale value.
    result.initialScale = resultZoom;
    if (resultZoom == ViewportArguments::ValueAuto) {
        result.initialScale = initialViewportSize.width() / defaultWidth;
        if (resultWidth != ViewportArguments::ValueAuto)
            result.initialScale = initialViewportSize.width() / resultWidth;
        if (resultHeight != ViewportArguments::ValueAuto) {
            // if 'auto', the initial-scale will be negative here and thus ignored.
            result.initialScale = std::max<float>(result.initialScale, initialViewportSize.height() / resultHeight);
        }
    }

    // Constrain initial-scale value to minimum-scale/maximum-scale range.
    result.initialScale = std::min(result.maximumScale, std::max(result.minimumScale, result.initialScale));

    // Resolve width value.
    if (resultWidth == ViewportArguments::ValueAuto) {
        if (resultZoom == ViewportArguments::ValueAuto)
            resultWidth = defaultWidth;
        else if (resultHeight != ViewportArguments::ValueAuto)
            resultWidth = resultHeight * (initialViewportSize.width() / initialViewportSize.height());
        else
            resultWidth = initialViewportSize.width() / result.initialScale;
    }

    // Resolve height value.
    if (resultHeight == ViewportArguments::ValueAuto)
        resultHeight = resultWidth * (initialViewportSize.height() / initialViewportSize.width());

    if (type == ViewportArguments::ViewportMeta) {
        // Extend width and height to fill the visual viewport for the resolved initial-scale.
        resultWidth = std::max<float>(resultWidth, initialViewportSize.width() / result.initialScale);
        resultHeight = std::max<float>(resultHeight, initialViewportSize.height() / result.initialScale);
    }

    result.layoutSize.setWidth(resultWidth);
    result.layoutSize.setHeight(resultHeight);

    // FIXME: This might affect some ports, but is the right thing to do.
    // Only set initialScale to a value if it was explicitly set.
    // if (resultZoom == ViewportArguments::ValueAuto)
    //    result.initialScale = ViewportArguments::ValueAuto;

    result.userScalable = userZoom;
    result.orientation = orientation;
    result.shrinkToFit = shrinkToFit;
    result.viewportFit = viewportFit;

    return result;
}

static FloatSize convertToUserSpace(const FloatSize& deviceSize, float devicePixelRatio)
{
    FloatSize result = deviceSize;
    if (devicePixelRatio != 1)
        result.scale(1 / devicePixelRatio);
    return result;
}

ViewportAttributes computeViewportAttributes(ViewportArguments args, int desktopWidth, int deviceWidth, int deviceHeight, float devicePixelRatio, IntSize visibleViewport)
{
    FloatSize initialViewportSize = convertToUserSpace(visibleViewport, devicePixelRatio);
    FloatSize deviceSize = convertToUserSpace(FloatSize(deviceWidth, deviceHeight), devicePixelRatio);

    return args.resolve(initialViewportSize, deviceSize, desktopWidth);
}

float computeMinimumScaleFactorForContentContained(const ViewportAttributes& result, const IntSize& visibleViewport, const IntSize& contentsSize)
{
    FloatSize viewportSize(visibleViewport);
    return std::max<float>(result.minimumScale, std::max(viewportSize.width() / contentsSize.width(), viewportSize.height() / contentsSize.height()));
}

void restrictMinimumScaleFactorToViewportSize(ViewportAttributes& result, IntSize visibleViewport, float devicePixelRatio)
{
    FloatSize viewportSize = convertToUserSpace(visibleViewport, devicePixelRatio);

    result.minimumScale = std::max<float>(result.minimumScale, std::max(viewportSize.width() / result.layoutSize.width(), viewportSize.height() / result.layoutSize.height()));
}

void restrictScaleFactorToInitialScaleIfNotUserScalable(ViewportAttributes& result)
{
    if (!result.userScalable)
        result.maximumScale = result.minimumScale = result.initialScale;
}

static void reportViewportWarning(Document&, ViewportErrorCode, StringView replacement1 = { }, StringView replacement2 = { });

static float numericPrefix(Document& document, StringView key, StringView value, bool* ok = nullptr)
{
    size_t parsedLength;
    float numericValue;
    if (value.is8Bit())
        numericValue = charactersToFloat(value.characters8(), value.length(), parsedLength);
    else
        numericValue = charactersToFloat(value.characters16(), value.length(), parsedLength);
    if (!parsedLength) {
        reportViewportWarning(document, UnrecognizedViewportArgumentValueError, value, key);
        if (ok)
            *ok = false;
        return 0;
    }
    if (parsedLength < value.length())
        reportViewportWarning(document, TruncatedViewportArgumentValueError, value, key);
    if (ok)
        *ok = true;
    return numericValue;
}

static float findSizeValue(Document& document, StringView key, StringView value, bool* valueWasExplicit = nullptr)
{
    // 1) Non-negative number values are translated to px lengths.
    // 2) Negative number values are translated to auto.
    // 3) device-width and device-height are used as keywords.
    // 4) Other keywords and unknown values translate to 0.0.

    if (valueWasExplicit)
        *valueWasExplicit = true;

    if (equalLettersIgnoringASCIICase(value, "device-width"))
        return ViewportArguments::ValueDeviceWidth;

    if (equalLettersIgnoringASCIICase(value, "device-height"))
        return ViewportArguments::ValueDeviceHeight;

    float sizeValue = numericPrefix(document, key, value);

    if (sizeValue < 0) {
        if (valueWasExplicit)
            *valueWasExplicit = false;
        return ViewportArguments::ValueAuto;
    }

    return sizeValue;
}

static float findScaleValue(Document& document, StringView key, StringView value)
{
    // 1) Non-negative number values are translated to <number> values.
    // 2) Negative number values are translated to auto.
    // 3) yes is translated to 1.0.
    // 4) device-width and device-height are translated to 10.0.
    // 5) no and unknown values are translated to 0.0

    if (equalLettersIgnoringASCIICase(value, "yes"))
        return 1;
    if (equalLettersIgnoringASCIICase(value, "no"))
        return 0;
    if (equalLettersIgnoringASCIICase(value, "device-width"))
        return 10;
    if (equalLettersIgnoringASCIICase(value, "device-height"))
        return 10;

    float numericValue = numericPrefix(document, key, value);

    if (numericValue < 0)
        return ViewportArguments::ValueAuto;

    if (numericValue > 10.0)
        reportViewportWarning(document, MaximumScaleTooLargeError);

    return numericValue;
}

static bool findBooleanValue(Document& document, StringView key, StringView value)
{
    // yes and no are used as keywords.
    // Numbers >= 1, numbers <= -1, device-width and device-height are mapped to yes.
    // Numbers in the range <-1, 1>, and unknown values, are mapped to no.

    if (equalLettersIgnoringASCIICase(value, "yes"))
        return true;
    if (equalLettersIgnoringASCIICase(value, "no"))
        return false;
    if (equalLettersIgnoringASCIICase(value, "device-width"))
        return true;
    if (equalLettersIgnoringASCIICase(value, "device-height"))
        return true;
    return std::abs(numericPrefix(document, key, value)) >= 1;
}

static ViewportFit parseViewportFitValue(Document& document, StringView key, StringView value)
{
    if (equalLettersIgnoringASCIICase(value, "auto"))
        return ViewportFit::Auto;
    if (equalLettersIgnoringASCIICase(value, "contain"))
        return ViewportFit::Contain;
    if (equalLettersIgnoringASCIICase(value, "cover"))
        return ViewportFit::Cover;

    reportViewportWarning(document, UnrecognizedViewportArgumentValueError, value, key);

    return ViewportFit::Auto;
}

void setViewportFeature(ViewportArguments& arguments, Document& document, StringView key, StringView value)
{
    if (equalLettersIgnoringASCIICase(key, "width"))
        arguments.width = findSizeValue(document, key, value, &arguments.widthWasExplicit);
    else if (equalLettersIgnoringASCIICase(key, "height"))
        arguments.height = findSizeValue(document, key, value);
    else if (equalLettersIgnoringASCIICase(key, "initial-scale"))
        arguments.zoom = findScaleValue(document, key, value);
    else if (equalLettersIgnoringASCIICase(key, "minimum-scale"))
        arguments.minZoom = findScaleValue(document, key, value);
    else if (equalLettersIgnoringASCIICase(key, "maximum-scale"))
        arguments.maxZoom = findScaleValue(document, key, value);
    else if (equalLettersIgnoringASCIICase(key, "user-scalable"))
        arguments.userZoom = findBooleanValue(document, key, value);
#if PLATFORM(IOS_FAMILY)
    else if (equalLettersIgnoringASCIICase(key, "minimal-ui")) {
        // FIXME: Ignore silently for now. This code should eventually be removed
        // so we start giving the warning in the web inspector as for other unimplemented keys.
    }
#endif
    else if (equalLettersIgnoringASCIICase(key, "shrink-to-fit"))
        arguments.shrinkToFit = findBooleanValue(document, key, value);
    else if (equalLettersIgnoringASCIICase(key, "viewport-fit") && document.settings().viewportFitEnabled())
        arguments.viewportFit = parseViewportFitValue(document, key, value);
    else
        reportViewportWarning(document, UnrecognizedViewportArgumentKeyError, key);
}

static const char* viewportErrorMessageTemplate(ViewportErrorCode errorCode)
{
    static const char* const errors[] = {
        "Viewport argument key \"%replacement1\" not recognized and ignored.",
        "Viewport argument value \"%replacement1\" for key \"%replacement2\" is invalid, and has been ignored.",
        "Viewport argument value \"%replacement1\" for key \"%replacement2\" was truncated to its numeric prefix.",
        "Viewport maximum-scale cannot be larger than 10.0. The maximum-scale will be set to 10.0."
    };

    return errors[errorCode];
}

static MessageLevel viewportErrorMessageLevel(ViewportErrorCode errorCode)
{
    switch (errorCode) {
    case TruncatedViewportArgumentValueError:
        return MessageLevel::Warning;
    case UnrecognizedViewportArgumentKeyError:
    case UnrecognizedViewportArgumentValueError:
    case MaximumScaleTooLargeError:
        return MessageLevel::Error;
    }

    ASSERT_NOT_REACHED();
    return MessageLevel::Error;
}

void reportViewportWarning(Document& document, ViewportErrorCode errorCode, StringView replacement1, StringView replacement2)
{
    // FIXME: Why is this null check needed? Can't addConsoleMessage deal with this?
    if (!document.frame())
        return;

    String message = viewportErrorMessageTemplate(errorCode);
    if (!replacement1.isNull())
        message.replace("%replacement1", replacement1.toStringWithoutCopying());
    // FIXME: This will do the wrong thing if replacement1 contains the substring "%replacement2".
    if (!replacement2.isNull())
        message.replace("%replacement2", replacement2.toStringWithoutCopying());

    if ((errorCode == UnrecognizedViewportArgumentValueError || errorCode == TruncatedViewportArgumentValueError) && replacement1.contains(';'))
        message.append(" Note that ';' is not a separator in viewport values. The list should be comma-separated.");

    // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    document.addConsoleMessage(MessageSource::Rendering, viewportErrorMessageLevel(errorCode), message);
}

TextStream& operator<<(TextStream& ts, const ViewportArguments& viewportArguments)
{
    TextStream::IndentScope indentScope(ts);

    ts << "\n" << indent << "(width " << viewportArguments.width << ", minWidth " << viewportArguments.minWidth << ", maxWidth " << viewportArguments.maxWidth << ")";
    ts << "\n" << indent << "(height " << viewportArguments.height << ", minHeight " << viewportArguments.minHeight << ", maxHeight " << viewportArguments.maxHeight << ")";
    ts << "\n" << indent << "(zoom " << viewportArguments.zoom << ", minZoom " << viewportArguments.minZoom << ", maxZoom " << viewportArguments.maxZoom << ")";

    return ts;
}

} // namespace WebCore
