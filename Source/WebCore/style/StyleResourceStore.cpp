/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "StyleResourceStore.h"

#include "StyleCachedImage.h"

#define CACHE_STYLE_IMAGES 1

namespace WebCore {
namespace Style {

Ref<ResourceStore> ResourceStore::create()
{
    return adoptRef(*new ResourceStore);
}

ResourceStore::ResourceStore() = default;
ResourceStore::~ResourceStore() = default;

RefPtr<StyleCachedImage> ResourceStore::image(const URL& location)
{
#if CACHE_STYLE_IMAGES
    return m_images.get(location).get();
#else
    return StyleCachedImage::create(location);
#endif
}

Ref<StyleCachedImage> ResourceStore::ensureImage(const URL& location)
{
#if CACHE_STYLE_IMAGES
    // NOTE: Can't use m_images.ensure() because the value is WeakPtr and a
    // will be destroyed by the time the function returns.

    RefPtr existingImage = m_images.get(location).get();
    if (existingImage)
        return existingImage.releaseNonNull();

    auto newImage = StyleCachedImage::create(location);
    m_images.add(location, newImage.ptr());
    return newImage;
#else
    return StyleCachedImage::create(location);
#endif
}

} // namespace Style
} // namespace WebCore

#undef CACHE_STYLE_IMAGES
