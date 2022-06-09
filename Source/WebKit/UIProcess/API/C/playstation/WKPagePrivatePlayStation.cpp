/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include "WKPagePrivatePlayStation.h"

#include "DrawingAreaProxy.h"
#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "WKAPICast.h"
#include "WebEventFactory.h"
#include "WebPageProxy.h"
#include <cairo.h>
#include <wpe/wpe.h>

static void drawPageBackground(cairo_t* ctx, const std::optional<WebCore::Color>& backgroundColor, const WebCore::IntRect& rect)
{
    if (!backgroundColor || backgroundColor.value().isVisible())
        return;

    auto [r, g, b, a] = backgroundColor.value().toColorTypeLossy<WebCore::SRGBA<uint8_t>>().resolved();

    cairo_set_source_rgba(ctx, r, g, b, a);
    cairo_rectangle(ctx, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);
    cairo_fill(ctx);
}

void WKPageHandleKeyboardEvent(WKPageRef pageRef, WKKeyboardEvent event)
{
    using WebKit::NativeWebKeyboardEvent;

    wpe_input_keyboard_event wpeEvent;
    wpeEvent.time = 0;
    wpeEvent.key_code = event.virtualKeyCode;
    wpeEvent.hardware_key_code = event.virtualKeyCode;
    wpeEvent.modifiers = 0; // TODO: Handle modifiers.
    switch (event.type) {
    case kWKEventKeyDown:
        wpeEvent.pressed = true;
        break;
    case kWKEventKeyUp:
        wpeEvent.pressed = false;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    NativeWebKeyboardEvent::HandledByInputMethod handledByInputMethod = NativeWebKeyboardEvent::HandledByInputMethod::No;
    std::optional<Vector<WebCore::CompositionUnderline>> preeditUnderlines;
    std::optional<WebKit::EditingRange> preeditSelectionRange;
    WebKit::toImpl(pageRef)->handleKeyboardEvent(NativeWebKeyboardEvent(&wpeEvent, "", handledByInputMethod, WTFMove(preeditUnderlines), WTFMove(preeditSelectionRange)));
}

void WKPageHandleMouseEvent(WKPageRef pageRef, WKMouseEvent event)
{
    using WebKit::NativeWebMouseEvent;

    wpe_input_pointer_event wpeEvent;

    switch (event.type) {
    case kWKEventMouseDown:
        wpeEvent.type = wpe_input_pointer_event_type_button;
        wpeEvent.state = 1;
        break;
    case kWKEventMouseUp:
        wpeEvent.type = wpe_input_pointer_event_type_button;
        wpeEvent.state = 0;
        break;
    case kWKEventMouseMove:
        wpeEvent.type = wpe_input_pointer_event_type_motion;
        wpeEvent.state = 0;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    switch (event.button) {
    case kWKEventMouseButtonLeftButton:
        wpeEvent.button = 1;
        break;
    case kWKEventMouseButtonMiddleButton:
        wpeEvent.button = 3;
        break;
    case kWKEventMouseButtonRightButton:
        wpeEvent.button = 2;
        break;
    case kWKEventMouseButtonNoButton:
        wpeEvent.button = 0;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }
    wpeEvent.time = 0;
    wpeEvent.x = event.position.x;
    wpeEvent.y = event.position.y;
    wpeEvent.modifiers = 0; // TODO: Handle modifiers.

    const float deviceScaleFactor = 1;

    WebKit::toImpl(pageRef)->handleMouseEvent(NativeWebMouseEvent(&wpeEvent, deviceScaleFactor));
}

void WKPageHandleWheelEvent(WKPageRef pageRef, WKWheelEvent event)
{
    using WebKit::WebWheelEvent;
    using WebKit::NativeWebWheelEvent;

    const float deviceScaleFactor = 1;
    int positionX = event.position.x;
    int positionY = event.position.y;

    struct wpe_input_axis_event xEvent = {
        wpe_input_axis_event_type_motion,
        0, positionX, positionY,
        1, static_cast<int32_t>(event.delta.width), 0
    };

    WebKit::toImpl(pageRef)->handleWheelEvent(NativeWebWheelEvent(&xEvent, deviceScaleFactor, WebWheelEvent::Phase::PhaseNone, WebWheelEvent::Phase::PhaseNone));

    struct wpe_input_axis_event yEvent = {
        wpe_input_axis_event_type_motion,
        0, positionX, positionY,
        0, static_cast<int32_t>(event.delta.height), 0
    };

    WebKit::toImpl(pageRef)->handleWheelEvent(NativeWebWheelEvent(&yEvent, deviceScaleFactor, WebWheelEvent::Phase::PhaseNone, WebWheelEvent::Phase::PhaseNone));
}

void WKPagePaint(WKPageRef pageRef, unsigned char* surfaceData, WKSize wkSurfaceSize, WKRect wkPaintRect)
{
    auto surfaceSize = WebKit::toIntSize(wkSurfaceSize);
    auto paintRect = WebKit::toIntRect(wkPaintRect);
    if (!surfaceData || surfaceSize.isEmpty())
        return;

    const cairo_format_t format = CAIRO_FORMAT_ARGB32;
    cairo_surface_t* surface = cairo_image_surface_create_for_data(surfaceData, format, surfaceSize.width(), surfaceSize.height(), cairo_format_stride_for_width(format, surfaceSize.width()));
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
        return;

    cairo_t* ctx = cairo_create(surface);

    auto page = WebKit::toImpl(pageRef);
    auto& backgroundColor = page->backgroundColor();
    page->endPrinting();
    if (auto* drawingArea = static_cast<WebKit::DrawingAreaProxyCoordinatedGraphics*>(page->drawingArea())) {
        // FIXME: We should port WebKit1's rect coalescing logic here.
        WebCore::Region unpaintedRegion;
        drawingArea->paint(ctx, paintRect, unpaintedRegion);

        for (const auto& rect : unpaintedRegion.rects())
            drawPageBackground(ctx, backgroundColor, rect);
    } else
        drawPageBackground(ctx, backgroundColor, paintRect);

    cairo_destroy(ctx);
    cairo_surface_destroy(surface);
}
