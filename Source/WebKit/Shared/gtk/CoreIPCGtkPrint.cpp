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
#include "CoreIPCGtkPrint.h"

#if PLATFORM(GTK)

#include <gtk/gtk.h>

namespace WebKit {

CoreIPCGtkPrintSettings::CoreIPCGtkPrintSettings(const GRefPtr<GtkPrintSettings>& settings)
    : m_settings(gtk_print_settings_to_gvariant(settings.get()))
{
}

CoreIPCGtkPrintSettings::operator GRefPtr<GtkPrintSettings>() const
{
    if (!m_settings)
        return nullptr;
    return adoptGRef(gtk_print_settings_new_from_gvariant(m_settings.get()));
}

CoreIPCGtkPageSetup::CoreIPCGtkPageSetup(const GRefPtr<GtkPageSetup>& pageSetup)
    : m_pageSetup(gtk_page_setup_to_gvariant(pageSetup.get()))
{
}

CoreIPCGtkPageSetup::operator GRefPtr<GtkPageSetup>() const
{
    if (!m_pageSetup)
        return nullptr;
    return adoptGRef(gtk_page_setup_new_from_gvariant(m_pageSetup.get()));
}

} // namespace WebKit

#endif // PLATFORM(GTK)
