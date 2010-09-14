/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include "Chrome.h"
#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "IntSize.h"
#include "Page.h"
#include "PlatformString.h"
#include "ScriptableDocumentParser.h"

using namespace std;

namespace WebCore {

ViewportConfiguration findConfigurationForViewportData(ViewportArguments args, int desktopWidth, int deviceWidth, int deviceHeight, int deviceDPI, IntSize visibleViewport)
{
    ViewportConfiguration result;

    float availableWidth = visibleViewport.width();
    float availableHeight = visibleViewport.height();

    ASSERT(availableWidth > 0 && availableHeight > 0);

    switch (int(args.width)) {
    case ViewportArguments::ValueDesktopWidth:
        args.width = desktopWidth;
        break;
    case ViewportArguments::ValueDeviceWidth:
        args.width = deviceWidth;
        break;
    case ViewportArguments::ValueDeviceHeight:
        args.width = deviceHeight;
        break;
    }

    switch (int(args.height)) {
    case ViewportArguments::ValueDesktopWidth:
        args.height = desktopWidth;
        break;
    case ViewportArguments::ValueDeviceWidth:
        args.height = deviceWidth;
        break;
    case ViewportArguments::ValueDeviceHeight:
        args.height = deviceHeight;
        break;
    }

    result.devicePixelRatio = float(deviceDPI / 160.0);

    // Resolve non-'auto' width and height to pixel values.
    if (deviceDPI != 1.0) {
        deviceWidth /= result.devicePixelRatio;
        deviceHeight /= result.devicePixelRatio;

        if (args.width != ViewportArguments::ValueAuto)
            args.width /= result.devicePixelRatio;
        if (args.height != ViewportArguments::ValueAuto)
            args.height /= result.devicePixelRatio;
    }

    // Clamp values to range defined by spec and resolve minimum-scale and maximum-scale values
    if (args.width != ViewportArguments::ValueAuto)
        args.width = min(float(10000), max(args.width, float(1)));
    if (args.height != ViewportArguments::ValueAuto)
        args.height = min(float(10000), max(args.height, float(1)));

    if (args.initialScale != ViewportArguments::ValueAuto)
        args.initialScale = min(float(10), max(args.initialScale, float(0.1)));
    if (args.minimumScale != ViewportArguments::ValueAuto)
        args.minimumScale = min(float(10), max(args.minimumScale, float(0.1)));
    if (args.maximumScale != ViewportArguments::ValueAuto)
        args.maximumScale = min(float(10), max(args.maximumScale, float(0.1)));

    // Resolve minimum-scale and maximum-scale values according to spec.
    if (args.minimumScale == ViewportArguments::ValueAuto)
        result.minimumScale = float(0.25);
    else
        result.minimumScale = args.minimumScale;

    if (args.maximumScale == ViewportArguments::ValueAuto) {
        result.maximumScale = float(5.0);
        result.minimumScale = min(float(5.0), result.minimumScale);
    } else
        result.maximumScale = args.maximumScale;
    result.maximumScale = max(result.minimumScale, result.maximumScale);

    // Resolve initial-scale value.
    result.initialScale = args.initialScale;
    if (result.initialScale == ViewportArguments::ValueAuto) {
        result.initialScale = availableWidth / desktopWidth;
        if (args.width != ViewportArguments::ValueAuto)
            result.initialScale = availableWidth / args.width;
        if (args.height != ViewportArguments::ValueAuto) {
            // if 'auto', the initial-scale will be negative here and thus ignored.
            result.initialScale = max(result.initialScale, availableHeight / args.height);
        }
    }

    // Constrain initial-scale value to minimum-scale/maximum-scale range.
    result.initialScale = min(result.maximumScale, max(result.minimumScale, result.initialScale));

    // Resolve width value.
    float width;
    if (args.width != ViewportArguments::ValueAuto)
        width = args.width;
    else {
        if (args.initialScale == ViewportArguments::ValueAuto)
            width = desktopWidth;
        else if (args.height != ViewportArguments::ValueAuto)
            width = args.height * (availableWidth / availableHeight);
        else
            width = availableWidth / result.initialScale;
    }

    // Resolve height value.
    float height;
    if (args.height != ViewportArguments::ValueAuto)
        height = args.height;
    else
        height = width * availableHeight / availableWidth;

    // Extend width and height to fill the visual viewport for the resolved initial-scale.
    width = max(width, availableWidth / result.initialScale);
    height = max(height, availableHeight / result.initialScale);
    result.layoutViewport.setWidth(width);
    result.layoutViewport.setHeight(height);

    // Update minimum scale factor, to never allow zooming out more than viewport
    result.minimumScale = max(result.minimumScale, max(availableWidth / width, availableHeight / height));

    return result;
}

static float findSizeValue(const String& keyString, const String& valueString, Document* document)
{
    // 1) Non-negative number values are translated to px lengths.
    // 2) Negative number values are translated to auto.
    // 3) device-width and device-height are used as keywords.
    // 4) Other keywords and unknown values translate to 0.0.

    if (equalIgnoringCase(valueString, "desktop-width"))
        return ViewportArguments::ValueDesktopWidth;
    if (equalIgnoringCase(valueString, "device-width"))
        return ViewportArguments::ValueDeviceWidth;
    if (equalIgnoringCase(valueString, "device-height"))
        return ViewportArguments::ValueDeviceHeight;

    bool ok;
    float value = valueString.toFloat(&ok);
    if (!ok) {
        reportViewportWarning(document, UnrecognizedViewportArgumentError, keyString);
        return float(0.0);
    }

    if (value < 0)
        return ViewportArguments::ValueAuto;

    if (keyString == "width")
        reportViewportWarning(document, DeviceWidthShouldBeUsedWarning, keyString);
    else if (keyString == "height")
        reportViewportWarning(document, DeviceHeightShouldBeUsedWarning, keyString);

    return value;
}

static float findScaleValue(const String& keyString, const String& valueString, Document* document)
{
    // 1) Non-negative number values are translated to <number> values.
    // 2) Negative number values are translated to auto.
    // 3) yes is translated to 1.0.
    // 4) device-width and device-height are translated to 10.0.
    // 5) no and unknown values are translated to 0.0

    if (equalIgnoringCase(valueString, "yes"))
        return float(1.0);
    if (equalIgnoringCase(valueString, "no"))
        return float(0.0);
    if (equalIgnoringCase(valueString, "desktop-width"))
        return float(10.0);
    if (equalIgnoringCase(valueString, "device-width"))
        return float(10.0);
    if (equalIgnoringCase(valueString, "device-height"))
        return float(10.0);

    bool ok;
    float value = valueString.toFloat(&ok);
    if (!ok) {
        reportViewportWarning(document, UnrecognizedViewportArgumentError, keyString);
        return float(0.0);
    }

    if (value < 0)
        return ViewportArguments::ValueAuto;

    if (value > 10.0)
        reportViewportWarning(document, MaximumScaleTooLargeError, keyString);

    return value;
}

static bool findUserScalableValue(const String& keyString, const String& valueString, Document* document)
{
    // yes and no are used as keywords.
    // Numbers >= 1, numbers <= -1, device-width and device-height are mapped to yes.
    // Numbers in the range <-1, 1>, and unknown values, are mapped to no.

    if (equalIgnoringCase(valueString, "yes"))
        return true;
    if (equalIgnoringCase(valueString, "no"))
        return false;
    if (equalIgnoringCase(valueString, "desktop-width"))
        return true;
    if (equalIgnoringCase(valueString, "device-width"))
        return true;
    if (equalIgnoringCase(valueString, "device-height"))
        return true;

    bool ok;
    float value = valueString.toFloat(&ok);
    if (!ok) {
        reportViewportWarning(document, UnrecognizedViewportArgumentError, keyString);
        return false;
    }

    if (fabs(value) < 1)
        return false;

    return true;
}

static float findTargetDensityDPIValue(const String& keyString, const String& valueString, Document* document)
{
    if (equalIgnoringCase(valueString, "device-dpi"))
        return ViewportArguments::ValueDeviceDPI;
    if (equalIgnoringCase(valueString, "low-dpi"))
        return ViewportArguments::ValueLowDPI;
    if (equalIgnoringCase(valueString, "medium-dpi"))
        return ViewportArguments::ValueMediumDPI;
    if (equalIgnoringCase(valueString, "high-dpi"))
        return ViewportArguments::ValueHighDPI;

    bool ok;
    float value = valueString.toFloat(&ok);
    if (!ok) {
        reportViewportWarning(document, UnrecognizedViewportArgumentError, keyString);
        return ViewportArguments::ValueAuto;
    }

     if (value < 70 || value > 400) {
        reportViewportWarning(document, TargetDensityDpiTooSmallOrLargeError, keyString);
        return ViewportArguments::ValueAuto;
    }

    return value;
}

void setViewportFeature(const String& keyString, const String& valueString, Document* document, void* data)
{
    ViewportArguments* arguments = static_cast<ViewportArguments*>(data);

    if (keyString == "width")
        arguments->width = findSizeValue(keyString, valueString, document);
    else if (keyString == "height")
        arguments->height = findSizeValue(keyString, valueString, document);
    else if (keyString == "initial-scale")
        arguments->initialScale = findScaleValue(keyString, valueString, document);
    else if (keyString == "minimum-scale")
        arguments->minimumScale = findScaleValue(keyString, valueString, document);
    else if (keyString == "maximum-scale")
        arguments->maximumScale = findScaleValue(keyString, valueString, document);
    else if (keyString == "user-scalable")
        arguments->userScalable = findUserScalableValue(keyString, valueString, document);
    else if (keyString == "target-densitydpi")
        arguments->targetDensityDpi = findTargetDensityDPIValue(keyString, valueString, document);
}

static const char* viewportErrorMessageTemplate(ViewportErrorCode errorCode)
{
    static const char* const errors[] = {
        "Viewport width or height set to physical device width, try using \"device-width\" constant instead for future compatibility.",
        "Viewport height or height set to physical device height, try using \"device-height\" constant instead for future compatibility.",
        "Viewport argument \"%replacement\" not recognized. Content ignored.",
        "Viewport maximum-scale cannot be larger than 10.0.  The maximum-scale will be set to 10.0.",
        "Viewport target-densitydpi has to take a number between 70 and 400 as a valid target dpi, try using \"device-dpi\", \"low-dpi\", \"medium-dpi\" or \"high-dpi\" instead for future compatibility."
    };

    return errors[errorCode];
}

static MessageLevel viewportErrorMessageLevel(ViewportErrorCode errorCode)
{
    return errorCode == UnrecognizedViewportArgumentError || errorCode == MaximumScaleTooLargeError ? ErrorMessageLevel : TipMessageLevel;
}

// FIXME: Why is this different from SVGDocumentExtensions parserLineNumber?
// FIXME: Callers should probably use ScriptController::eventHandlerLineNumber()
static int parserLineNumber(Document* document)
{
    if (!document)
        return 0;
    ScriptableDocumentParser* parser = document->scriptableDocumentParser();
    if (!parser)
        return 0;
    return parser->lineNumber() + 1;
}

void reportViewportWarning(Document* document, ViewportErrorCode errorCode, const String& replacement)
{
    Frame* frame = document->frame();
    if (!frame)
        return;

    String message = viewportErrorMessageTemplate(errorCode);
    message.replace("%replacement", replacement);

    frame->domWindow()->console()->addMessage(HTMLMessageSource, LogMessageType, viewportErrorMessageLevel(errorCode), message, parserLineNumber(document), document->url().string());
}

} // namespace WebCore
