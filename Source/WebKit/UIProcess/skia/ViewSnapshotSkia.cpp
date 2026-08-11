/*
 * Copyright (C) 2025 Igalia S.L.
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

#if USE(SKIA)

namespace WebKit {
using namespace WebCore;

Ref<ViewSnapshot> ViewSnapshot::create(sk_sp<SkImage>&& image)
{
    return adoptRef(*new ViewSnapshot(WTF::move(image)));
}

ViewSnapshot::ViewSnapshot(sk_sp<SkImage>&& image)
    : m_image(WTF::move(image))
{
    if (hasImage())
        ViewSnapshotStore::singleton().didAddImageToSnapshot(*this);
}

bool ViewSnapshot::hasImage() const
{
    return !!m_image;
}

void ViewSnapshot::clearImage()
{
    if (!hasImage())
        return;

    ViewSnapshotStore::singleton().willRemoveImageFromSnapshot(*this);

    m_image = nullptr;
}

size_t ViewSnapshot::estimatedImageSizeInBytes() const
{
    if (!m_image)
        return 0;

    return m_image->imageInfo().computeMinByteSize();
}

WebCore::IntSize ViewSnapshot::size() const
{
    if (!m_image)
        return { };

    return { m_image->width(), m_image->height() };
}

} // namespace WebKit

#endif // USE(SKIA)

