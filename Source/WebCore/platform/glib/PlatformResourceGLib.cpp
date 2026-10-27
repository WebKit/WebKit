/*
 * Copyright (C) 2025 Igalia S.L
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformResource.h"

#include "SharedBuffer.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GSpanExtras.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(GTK)
#define IMAGES_GRESOURCE_PATH "/org/webkitgtk/resources/images"
#else
#define IMAGES_GRESOURCE_PATH "/org/webkitwpe/resources/images"
#endif

namespace WebCore {

RefPtr<SharedBuffer> loadPlatformResource(StringView name, StringView)
{
    auto path = makeString(IMAGES_GRESOURCE_PATH "/"_s, name).utf8();
    GRefPtr<GBytes> data = adoptGRef(g_resources_lookup_data(path.legacyCStringPointer(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr));
    ASSERT(data);
    if (!data)
        return nullptr;
    return SharedBuffer::create(span(data));
}

} // namespace WebCore
