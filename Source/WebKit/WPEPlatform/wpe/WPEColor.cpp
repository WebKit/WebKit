/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "WPEColor.h"

/**
 * WPEColor:
 * @red: Red channel, between 0.0 and 1.0 inclusive
 * @green: Green channel, between 0.0 and 1.0 inclusive
 * @blue: Blue channel, between 0.0 and 1.0 inclusive
 * @alpha: Alpha channel, between 0.0 and 1.0 inclusive
 *
 * Boxed type representing a RGBA color.
 */

/**
 * wpe_color_copy:
 * @color: a #WPEColor
 *
 * Make a copy of @color.
 *
 * Returns: (transfer full): A copy of passed in #WPEColor.
 */
WPEColor* wpe_color_copy(WPEColor* color)
{
    g_return_val_if_fail(color, nullptr);

    WPEColor* copy = static_cast<WPEColor*>(fastZeroedMalloc(sizeof(WPEColor)));
    copy->red = color->red;
    copy->green = color->green;
    copy->blue = color->blue;
    copy->alpha = color->alpha;
    return copy;
}

/**
 * wpe_color_free:
 * @color: a #WPEColor
 *
 * Free the #WPEColor.
 */
void wpe_color_free(WPEColor* color)
{
    g_return_if_fail(color);

    fastFree(color);
}

G_DEFINE_BOXED_TYPE(WPEColor, wpe_color, wpe_color_copy, wpe_color_free)
