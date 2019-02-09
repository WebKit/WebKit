/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "ViewSnapshotStore.h"

#include <WebCore/CairoUtilities.h>

namespace WebKit {
using namespace WebCore;

Ref<ViewSnapshot> ViewSnapshot::create(RefPtr<cairo_surface_t>&& surface)
{
    return adoptRef(*new ViewSnapshot(WTFMove(surface)));
}

ViewSnapshot::ViewSnapshot(RefPtr<cairo_surface_t>&& surface)
    : m_surface(WTFMove(surface))
{
    if (hasImage())
        ViewSnapshotStore::singleton().didAddImageToSnapshot(*this);
}

bool ViewSnapshot::hasImage() const
{
    return !!m_surface;
}

void ViewSnapshot::clearImage()
{
    if (!hasImage())
        return;

    ViewSnapshotStore::singleton().willRemoveImageFromSnapshot(*this);

    m_surface = nullptr;
}

size_t ViewSnapshot::imageSizeInBytes() const
{
    if (!m_surface)
        return 0;

    cairo_surface_t* surface = m_surface.get();
    int stride = cairo_image_surface_get_stride(surface);
    int height = cairo_image_surface_get_width(surface);

    return stride * height;
}

WebCore::IntSize ViewSnapshot::size() const
{
    if (!m_surface)
        return { };

    cairo_surface_t* surface = m_surface.get();
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);

    return { width, height };
}

} // namespace WebKit
