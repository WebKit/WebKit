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
#include "FrameDestructionObserverInlines.h"
#include "IntSize.h"
#include "LocalFrame.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

typedef Function<void(ViewportErrorCode, StringView, StringView)> InternalViewportErrorHandler;

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

    if (type == ViewportArguments::Type::CSSDeviceAdaptation) {
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

    if (type != ViewportArguments::Type::CSSDeviceAdaptation && type != ViewportArguments::Type::Implicit) {
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

    if (type == ViewportArguments::Type::ViewportMeta) {
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

static float numericPrefix(StringView key, StringView value, const InternalViewportErrorHandler& errorHandler, bool* ok = nullptr)
{
    size_t parsedLength;
    float numericValue;
    if (value.is8Bit())
        numericValue = charactersToFloat(value.characters8(), value.length(), parsedLength);
    else
        numericValue = charactersToFloat(value.characters16(), value.length(), parsedLength);
    if (!parsedLength) {
        errorHandler(UnrecognizedViewportArgumentValueError, value, key);
        if (ok)
            *ok = false;
        return 0;
    }
    if (parsedLength < value.length())
        errorHandler(TruncatedViewportArgumentValueError, value, key);
    if (ok)
        *ok = true;
    return numericValue;
}

static float findSizeValue(StringView key, StringView value, const InternalViewportErrorHandler& errorHandler, bool* valueWasExplicit = nullptr)
{
    // 1) Non-negative number values are translated to px lengths.
    // 2) Negative number values are translated to auto.
    // 3) device-width and device-height are used as keywords.
    // 4) Other keywords and unknown values translate to 0.0.

    if (valueWasExplicit)
        *valueWasExplicit = true;

    if (equalLettersIgnoringASCIICase(value, "device-width"_s))
        return ViewportArguments::ValueDeviceWidth;

    if (equalLettersIgnoringASCIICase(value, "device-height"_s))
        return ViewportArguments::ValueDeviceHeight;

    float sizeValue = numericPrefix(key, value, errorHandler);

    if (sizeValue < 0) {
        if (valueWasExplicit)
            *valueWasExplicit = false;
        return ViewportArguments::ValueAuto;
    }

    return sizeValue;
}

static float findScaleValue(StringView key, StringView value, const InternalViewportErrorHandler& errorHandler)
{
    // 1) Non-negative number values are translated to <number> values.
    // 2) Negative number values are translated to auto.
    // 3) yes is translated to 1.0.
    // 4) device-width and device-height are translated to 10.0.
    // 5) no and unknown values are translated to 0.0

    if (equalLettersIgnoringASCIICase(value, "yes"_s))
        return 1;
    if (equalLettersIgnoringASCIICase(value, "no"_s))
        return 0;
    if (equalLettersIgnoringASCIICase(value, "device-width"_s))
        return 10;
    if (equalLettersIgnoringASCIICase(value, "device-height"_s))
        return 10;

    float numericValue = numericPrefix(key, value, errorHandler);

    if (numericValue < 0)
        return ViewportArguments::ValueAuto;

    if (numericValue > 10.0)
        errorHandler(MaximumScaleTooLargeError, { }, { });

    return numericValue;
}

static bool findBooleanValue(StringView key, StringView value, const InternalViewportErrorHandler& errorHandler)
{
    // yes and no are used as keywords.
    // Numbers >= 1, numbers <= -1, device-width and device-height are mapped to yes.
    // Numbers in the range <-1, 1>, and unknown values, are mapped to no.

    if (equalLettersIgnoringASCIICase(value, "yes"_s))
        return true;
    if (equalLettersIgnoringASCIICase(value, "no"_s))
        return false;
    if (equalLettersIgnoringASCIICase(value, "device-width"_s))
        return true;
    if (equalLettersIgnoringASCIICase(value, "device-height"_s))
        return true;
    return std::abs(numericPrefix(key, value, errorHandler)) >= 1;
}

static ViewportFit parseViewportFitValue(StringView key, StringView value, const InternalViewportErrorHandler& errorHandler)
{
    if (equalLettersIgnoringASCIICase(value, "auto"_s))
        return ViewportFit::Auto;
    if (equalLettersIgnoringASCIICase(value, "contain"_s))
        return ViewportFit::Contain;
    if (equalLettersIgnoringASCIICase(value, "cover"_s))
        return ViewportFit::Cover;

    errorHandler(UnrecognizedViewportArgumentValueError, value, key);

    return ViewportFit::Auto;
}

static ASCIILiteral viewportErrorMessageTemplate(ViewportErrorCode errorCode)
{
    static constexpr ASCIILiteral errors[] = {
        "Viewport argument key \"%replacement1\" not recognized and ignored."_s,
        "Viewport argument value \"%replacement1\" for key \"%replacement2\" is invalid, and has been ignored."_s,
        "Viewport argument value \"%replacement1\" for key \"%replacement2\" was truncated to its numeric prefix."_s,
        "Viewport maximum-scale cannot be larger than 10.0. The maximum-scale will be set to 10.0."_s
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

static String viewportErrorMessage(ViewportErrorCode errorCode, StringView replacement1, StringView replacement2)
{
    String message = viewportErrorMessageTemplate(errorCode);
    if (!replacement1.isNull())
        message = makeStringByReplacingAll(message, "%replacement1"_s, replacement1);
    // FIXME: This will do the wrong thing if replacement1 contains the substring "%replacement2".
    if (!replacement2.isNull())
        message = makeStringByReplacingAll(message, "%replacement2"_s, replacement2);

    if ((errorCode == UnrecognizedViewportArgumentValueError || errorCode == TruncatedViewportArgumentValueError) && replacement1.contains(';'))
        message = makeString(message, " Note that ';' is not a separator in viewport values. The list should be comma-separated."_s);

    return message;
}

static void reportViewportWarning(Document& document, ViewportErrorCode errorCode, const String& message)
{
    // FIXME: Why is this null check needed? Can't addConsoleMessage deal with this?
    if (!document.frame())
        return;

    // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    document.addConsoleMessage(MessageSource::Rendering, viewportErrorMessageLevel(errorCode), message);
}

void setViewportFeature(ViewportArguments& arguments, StringView key, StringView value, const ViewportErrorHandler& errorHandler)
{
    InternalViewportErrorHandler internalErrorHandler = [&errorHandler] (ViewportErrorCode errorCode, StringView replacement1, StringView replacement2) {
        errorHandler(errorCode, viewportErrorMessage(errorCode, replacement1, replacement2));
    };

    if (equalLettersIgnoringASCIICase(key, "width"_s))
        arguments.width = findSizeValue(key, value, internalErrorHandler, &arguments.widthWasExplicit);
    else if (equalLettersIgnoringASCIICase(key, "height"_s))
        arguments.height = findSizeValue(key, value, internalErrorHandler);
    else if (equalLettersIgnoringASCIICase(key, "initial-scale"_s))
        arguments.zoom = findScaleValue(key, value, internalErrorHandler);
    else if (equalLettersIgnoringASCIICase(key, "minimum-scale"_s))
        arguments.minZoom = findScaleValue(key, value, internalErrorHandler);
    else if (equalLettersIgnoringASCIICase(key, "maximum-scale"_s))
        arguments.maxZoom = findScaleValue(key, value, internalErrorHandler);
    else if (equalLettersIgnoringASCIICase(key, "user-scalable"_s))
        arguments.userZoom = findBooleanValue(key, value, internalErrorHandler);
#if PLATFORM(IOS_FAMILY)
    else if (equalLettersIgnoringASCIICase(key, "minimal-ui"_s)) {
        // FIXME: Ignore silently for now. This code should eventually be removed
        // so we start giving the warning in the web inspector as for other unimplemented keys.
    }
#endif
    else if (equalLettersIgnoringASCIICase(key, "shrink-to-fit"_s))
        arguments.shrinkToFit = findBooleanValue(key, value, internalErrorHandler);
    else if (equalLettersIgnoringASCIICase(key, "viewport-fit"_s))
        arguments.viewportFit = parseViewportFitValue(key, value, internalErrorHandler);
    else
        internalErrorHandler(UnrecognizedViewportArgumentKeyError, key, { });
}

void setViewportFeature(ViewportArguments& arguments, Document& document, StringView key, StringView value)
{
    setViewportFeature(arguments, key, value, [&](ViewportErrorCode errorCode, const String& message) {
        reportViewportWarning(document, errorCode, message);
    });
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
