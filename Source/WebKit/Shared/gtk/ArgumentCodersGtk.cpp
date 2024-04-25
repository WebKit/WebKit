/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "ArgumentCodersGtk.h"

#include "ArgumentCodersGLib.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/Image.h>
#include <WebCore/SelectionData.h>
#include <WebCore/ShareableBitmap.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace IPC {
using namespace WebCore;
using namespace WebKit;

void ArgumentCoder<GRefPtr<GtkPrintSettings>>::encode(Encoder& encoder, const GRefPtr<GtkPrintSettings>& argument)
{
    GRefPtr<GtkPrintSettings> printSettings = argument ? argument : adoptGRef(gtk_print_settings_new());
    GRefPtr<GVariant> variant = adoptGRef(gtk_print_settings_to_gvariant(printSettings.get()));
    encoder << variant;
}

std::optional<GRefPtr<GtkPrintSettings>> ArgumentCoder<GRefPtr<GtkPrintSettings>>::decode(Decoder& decoder)
{
    auto variant = decoder.decode<GRefPtr<GVariant>>();
    if (UNLIKELY(!variant))
        return std::nullopt;

    return gtk_print_settings_new_from_gvariant(variant->get());
}

void ArgumentCoder<GRefPtr<GtkPageSetup>>::encode(Encoder& encoder, const GRefPtr<GtkPageSetup>& argument)
{
    GRefPtr<GtkPageSetup> pageSetup = argument ? argument : adoptGRef(gtk_page_setup_new());
    GRefPtr<GVariant> variant = adoptGRef(gtk_page_setup_to_gvariant(pageSetup.get()));
    encoder << variant;
}

std::optional<GRefPtr<GtkPageSetup>> ArgumentCoder<GRefPtr<GtkPageSetup>>::decode(Decoder& decoder)
{
    auto variant = decoder.decode<GRefPtr<GVariant>>();
    if (UNLIKELY(!variant))
        return std::nullopt;

    return gtk_page_setup_new_from_gvariant(variant->get());
}

}
