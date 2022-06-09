/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#pragma once

#include "APIObject.h"
#include "ImageOptions.h"
#include "ShareableBitmap.h"
#include <wtf/Ref.h>

namespace WebCore {
class IntSize;
}

namespace WebKit {

// WebImage - An image type suitable for vending to an API.

class WebImage : public API::ObjectImpl<API::Object::Type::Image> {
public:
    static RefPtr<WebImage> create(const WebCore::IntSize&, ImageOptions);
    static RefPtr<WebImage> create(const WebCore::IntSize&, ImageOptions, const ShareableBitmap::Configuration&);
    static Ref<WebImage> create(Ref<ShareableBitmap>&&);
    ~WebImage();
    
    const WebCore::IntSize& size() const;

    ShareableBitmap& bitmap() { return m_bitmap.get(); }
    const ShareableBitmap& bitmap() const { return m_bitmap.get(); }

private:
    WebImage(Ref<ShareableBitmap>&&);

    Ref<ShareableBitmap> m_bitmap;
};

} // namespace WebKit
