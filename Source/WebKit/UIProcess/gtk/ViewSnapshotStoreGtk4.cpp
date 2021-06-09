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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,dd
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

#if USE(GTK4)

namespace WebKit {
using namespace WebCore;

Ref<ViewSnapshot> ViewSnapshot::create(GRefPtr<GdkTexture>&& texture)
{
    return adoptRef(*new ViewSnapshot(WTFMove(texture)));
}

ViewSnapshot::ViewSnapshot(GRefPtr<GdkTexture>&& texture)
    : m_texture(WTFMove(texture))
{
    if (hasImage())
        ViewSnapshotStore::singleton().didAddImageToSnapshot(*this);
}

bool ViewSnapshot::hasImage() const
{
    return m_texture;
}

void ViewSnapshot::clearImage()
{
    if (!hasImage())
        return;

    ViewSnapshotStore::singleton().willRemoveImageFromSnapshot(*this);

    m_texture = nullptr;
}

size_t ViewSnapshot::estimatedImageSizeInBytes() const
{
    if (!m_texture)
        return 0;

    int width = gdk_texture_get_width(m_texture.get());
    int height = gdk_texture_get_height(m_texture.get());

    // Unfortunately we don't have a way to get size of a texture in
    // bytes, so we'll have to make something up. Let's assume that
    // pixel == 4 bytes.
    return width * height * 4;
}

WebCore::IntSize ViewSnapshot::size() const
{
    if (!m_texture)
        return { };

    int width = gdk_texture_get_width(m_texture.get());
    int height = gdk_texture_get_height(m_texture.get());

    return { width, height };
}

} // namespace WebKit

#endif
