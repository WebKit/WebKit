/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPETooltip.h"

#include <optional>
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

/**
 * WPETooltip:
 *
 * Boxed type representing a tooltip: its text and, optionally, the area of the
 * view it belongs to.
 *
 * Since: 2.56
 */
struct _WPETooltip {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(_WPETooltip);

    _WPETooltip(const char* text, const WPERectangle* area)
        : text(byteCast<char8_t>(text))
    {
        if (area && area->width > 0 && area->height > 0)
            this->area = *area;
    }

    UTF8CString text;
    std::optional<WPERectangle> area;

    int referenceCount { 1 };
};
G_DEFINE_BOXED_TYPE(WPETooltip, wpe_tooltip, wpe_tooltip_ref, wpe_tooltip_unref)

/**
 * wpe_tooltip_new:
 * @text: the tooltip text
 * @area: (nullable): the area of the view the tooltip belongs to, or %NULL
 *
 * Create a new #WPETooltip showing @text.
 *
 * When given, @area is the bounding box of the element the tooltip belongs
 * to, in view coordinates, which platform implementations can use to keep the
 * tooltip in place while the pointer stays within the same element. An empty
 * @area is ignored.
 *
 * Returns: (transfer full): a new #WPETooltip
 *
 * Since: 2.56
 */
WPETooltip* wpe_tooltip_new(const char* text, const WPERectangle* area)
{
    g_return_val_if_fail(text && *text, nullptr);

    auto* tooltip = static_cast<WPETooltip*>(fastMalloc(sizeof(WPETooltip)));
    new (tooltip) WPETooltip(text, area);
    return tooltip;
}

/**
 * wpe_tooltip_ref:
 * @tooltip: a #WPETooltip
 *
 * Atomically acquires a reference on the given @tooltip.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: (transfer full): The same @tooltip with an additional reference.
 *
 * Since: 2.56
 */
WPETooltip* wpe_tooltip_ref(WPETooltip* tooltip)
{
    g_return_val_if_fail(tooltip, nullptr);

    g_atomic_int_inc(&tooltip->referenceCount);
    return tooltip;
}

/**
 * wpe_tooltip_unref:
 * @tooltip: a #WPETooltip
 *
 * Atomically releases a reference on the given @tooltip.
 *
 * If the reference was the last, the resources associated with the
 * @tooltip are freed. This function is MT-safe and may be called from
 * any thread.
 *
 * Since: 2.56
 */
void wpe_tooltip_unref(WPETooltip* tooltip)
{
    g_return_if_fail(tooltip);

    if (g_atomic_int_dec_and_test(&tooltip->referenceCount)) {
        tooltip->~WPETooltip();
        fastFree(tooltip);
    }
}

/**
 * wpe_tooltip_get_text:
 * @tooltip: a #WPETooltip
 *
 * Get the text of @tooltip.
 *
 * Returns: (transfer none): the tooltip text
 *
 * Since: 2.56
 */
const char* wpe_tooltip_get_text(WPETooltip* tooltip)
{
    g_return_val_if_fail(tooltip, nullptr);

    return tooltip->text.legacyCStringPointer();
}

/**
 * wpe_tooltip_get_area:
 * @tooltip: a #WPETooltip
 *
 * Get the area of the view @tooltip belongs to, in view coordinates.
 *
 * Returns: (transfer none) (nullable): the tooltip area, or %NULL
 *
 * Since: 2.56
 */
const WPERectangle* wpe_tooltip_get_area(WPETooltip* tooltip)
{
    g_return_val_if_fail(tooltip, nullptr);

    return tooltip->area ? &tooltip->area.value() : nullptr;
}
