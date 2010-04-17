/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "Page.h"
#include "PlatformString.h"
#include "Tokenizer.h"

namespace WebCore {

void setViewportFeature(const String& keyString, const String& valueString, Document* document, void* data)
{
    ViewportArguments* arguments = static_cast<ViewportArguments*>(data);
    float value = ViewportArguments::ValueUndefined;
    bool didUseConstants = false;

    if (equalIgnoringCase(valueString, "yes"))
        value = 1;
    else if (equalIgnoringCase(valueString, "device-width")) {
        didUseConstants = true;
        if (document->page())
            value = document->page()->chrome()->windowRect().width();
    } else if (equalIgnoringCase(valueString, "device-height")) {
        didUseConstants = true;
        if (document->page())
            value = document->page()->chrome()->windowRect().height();
    } else if (equalIgnoringCase(valueString, "default")) // This allows us to distinguish the omission of a key from asking for the default value.
        value = -2;
    else if (valueString.length()) // listing a key with no value is shorthand for key=default
        value = valueString.toFloat();

    if (keyString == "initial-scale")
        arguments->initialScale = value;
    else if (keyString == "minimum-scale")
        arguments->minimumScale = value;
    else if (keyString == "maximum-scale") {
        arguments->maximumScale = value;
        if (value > 10.0)
            reportViewportWarning(document, MaximumScaleTooLargeError, keyString);
    } else if (keyString == "user-scalable")
        arguments->userScalable = value;
    else if (keyString == "width") {
        if (document->page() && value == document->page()->chrome()->windowRect().width() && !didUseConstants)
            reportViewportWarning(document, DeviceWidthShouldBeUsedWarning, keyString);
        else if (document->page() && value == document->page()->chrome()->windowRect().height() && !didUseConstants)
            reportViewportWarning(document, DeviceHeightShouldBeUsedWarning, keyString);

        arguments->width = value;
    } else if (keyString == "height") {
        if (document->page() && value == document->page()->chrome()->windowRect().width() && !didUseConstants)
            reportViewportWarning(document, DeviceWidthShouldBeUsedWarning, keyString);
        else if (document->page() && value == document->page()->chrome()->windowRect().height() && !didUseConstants)
            reportViewportWarning(document, DeviceHeightShouldBeUsedWarning, keyString);
        
        arguments->height = value;
    } else
        reportViewportWarning(document, UnrecognizedViewportArgumentError, keyString);
}

static const char* viewportErrorMessageTemplate(ViewportErrorCode errorCode)
{
    static const char* const errors[] = { 
        "Viewport width or height set to physical device width, try using \"device-width\" constant instead for future compatibility.",
        "Viewport height or height set to physical device height, try using \"device-height\" constant instead for future compatibility.",
        "Viewport argument \"%replacement\" not recognized. Content ignored.",
        "Viewport maximum-scale cannot be larger than 10.0.  The maximum-scale will be set to 10.0."
    };

    return errors[errorCode];
}

static MessageLevel viewportErrorMessageLevel(ViewportErrorCode errorCode)
{
    return errorCode == UnrecognizedViewportArgumentError || errorCode == MaximumScaleTooLargeError ? ErrorMessageLevel : TipMessageLevel;
}

void reportViewportWarning(Document* document, ViewportErrorCode errorCode, const String& replacement)
{
    Tokenizer* tokenizer = document->tokenizer();

    Frame* frame = document->frame();
    if (!frame)
        return;

    String message = viewportErrorMessageTemplate(errorCode);
    message.replace("%replacement", replacement);

    frame->domWindow()->console()->addMessage(HTMLMessageSource, LogMessageType, viewportErrorMessageLevel(errorCode), message, tokenizer ? tokenizer->lineNumber() + 1 : 0, document->url().string());
}

} // namespace WebCore
