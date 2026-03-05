/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "HTMLVideoElementCaptionDisplaySettings.h"

#if ENABLE(VIDEO)

#include "CSSParserContext.h"
#include "CSSParserMode.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Anchor.h"
#include "CSSPropertyParserState.h"
#include "CSSTokenizer.h"
#include "CSSValuePair.h"
#include "CaptionDisplaySettingsOptions.h"
#include "Element.h"
#include "EventHandler.h"
#include "HTMLVideoElement.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDOMWindow.h"
#include "MouseEvent.h"
#include "ResolvedCaptionDisplaySettingsOptions.h"
#include "TouchEvent.h"

namespace WebCore {

static void parsePositionAreaString(const String& positionArea, ResolvedCaptionDisplaySettingsOptions& options)
{
    CSSParserContext context { HTMLStandardMode };
    CSS::PropertyParserState state { context };
    CSSTokenizer tokenizer { positionArea };

    auto tokenRange = tokenizer.tokenRange();

    options.xPositionArea = std::nullopt;
    options.yPositionArea = std::nullopt;

    RefPtr value = CSSPropertyParserHelpers::consumePositionArea(tokenRange, state);
    if (!value)
        return;

    RefPtr valuePair = dynamicDowncast<CSSValuePair>(value.get());
    if (!valuePair)
        return;

    RefPtr firstValue = valuePair->first();
    RefPtr secondValue = valuePair->second();

    if (!firstValue->isValueID() || !secondValue->isValueID())
        return;

    using XPositionArea = ResolvedCaptionDisplaySettingsOptions::XPositionArea;
    switch (firstValue->valueID()) {
    case CSSValueLeft:
    case CSSValueSpanLeft:
    case CSSValueXStart:
    case CSSValueSpanXStart:
    case CSSValueSelfXStart:
    case CSSValueSpanSelfXStart:
        options.xPositionArea = XPositionArea::Left;
        break;

    case CSSValueCenter:
        options.xPositionArea = XPositionArea::Center;
        break;

    case CSSValueRight:
    case CSSValueSpanRight:
    case CSSValueXEnd:
    case CSSValueSpanXEnd:
    case CSSValueSelfXEnd:
    case CSSValueSpanSelfXEnd:
        options.xPositionArea = XPositionArea::Right;
        break;

    default:
        return;
    }

    using YPositionArea = ResolvedCaptionDisplaySettingsOptions::YPositionArea;
    switch (secondValue->valueID()) {
    case CSSValueTop:
    case CSSValueSpanTop:
    case CSSValueYStart:
    case CSSValueSpanYStart:
    case CSSValueSelfYStart:
    case CSSValueSpanSelfYStart:
        options.yPositionArea = YPositionArea::Top;
        break;

    case CSSValueCenter:
        options.yPositionArea = YPositionArea::Center;
        break;

    case CSSValueBottom:
    case CSSValueSpanBottom:
    case CSSValueYEnd:
    case CSSValueSpanYEnd:
    case CSSValueSelfYEnd:
    case CSSValueSpanSelfYEnd:
        options.yPositionArea = YPositionArea::Bottom;
        break;

    default:
        return;
    }
}

void HTMLVideoElementCaptionDisplaySettings::showCaptionDisplaySettings(HTMLVideoElement& element, std::optional<CaptionDisplaySettingsOptions>&& options, Ref<DeferredPromise>&& promise)
{
    RefPtr page = element.document().page();
    if (!page) {
        promise->reject();
        return;
    }

    ResolvedCaptionDisplaySettingsOptions resolvedOptions;
    if (options) {
        if (RefPtr anchorElement = dynamicDowncast<Element>(options->anchorNode.get()))
            resolvedOptions.anchorBounds = anchorElement->boundingBoxInRootViewCoordinates();
        if (!options->positionArea.isEmpty())
            parsePositionAreaString(options->positionArea, resolvedOptions);
    }

    if (!resolvedOptions.anchorBounds) {
        resolvedOptions.anchorBounds = [&] -> std::optional<FloatRect> {
            // In the absense of an explicit anchor element, provide a
            // default anchor using the current window event, if present,
            // or the last known mouse position, if not.
            RefPtr frame = element.document().frame();
            if (!frame)
                return std::nullopt;

            auto* JSDOMWindowBase = toJSDOMWindow(frame, mainThreadNormalWorldSingleton());
            if (!JSDOMWindowBase)
                return std::nullopt;

            constexpr auto locationToRect = [](const DoublePoint& point) {
                return FloatRect::narrowPrecision(point.x(), point.y(), 0, 0);
            };

            if (RefPtr currentEvent = JSDOMWindowBase->currentEvent()) {
                if (RefPtr mouseEvent = dynamicDowncast<MouseEvent>(currentEvent))
                    return locationToRect(mouseEvent->locationInRootViewCoordinates());

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(TOUCH_EVENTS)
                if (RefPtr touchEvent = dynamicDowncast<TouchEvent>(currentEvent))
                    return locationToRect(touchEvent->locationInRootViewCoordinates());
#endif

                if (RefPtr currentElement = downcast<Element>(currentEvent->currentTarget()))
                    return currentElement->boundingBoxInRootViewCoordinates();
            }

            RefPtr frameView = frame->view();
            if (!frameView)
                return std::nullopt;

            auto position = frame->eventHandler().lastKnownMousePosition();
            if (!position.isZero())
                return locationToRect(position);

            return std::nullopt;
        }();
    }

    element.showCaptionDisplaySettingsPreview();

    page->showCaptionDisplaySettings(element, resolvedOptions, [weakElement = WeakPtr { element }, promise = WTF::move(promise)] (ExceptionOr<void>&& result) {

        if (RefPtr element = weakElement.get())
            element->hideCaptionDisplaySettingsPreview();

        if (result.hasException())
            promise->reject(result.releaseException());
        else
            promise->resolve();

    });
}

}

#endif
