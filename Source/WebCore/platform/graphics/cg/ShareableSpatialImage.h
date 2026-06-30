/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(SPATIAL_IMAGE_DETECTION)

#include <WebCore/BitmapImage.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SpatialImageTypes.h>

#include <wtf/RetainPtr.h>

namespace WebCore {

class ShareableSpatialImage {
public:
    ShareableSpatialImage(ShareableSpatialImage&&) = default;
    ShareableSpatialImage(const ShareableSpatialImage&) = default;
    WEBCORE_EXPORT ShareableSpatialImage(ShareableBitmap::Handle&&, ShareableBitmap::Handle&&, const SpatialImageEyeProperties&, const SpatialImageEyeProperties&);

    ShareableSpatialImage& operator=(ShareableSpatialImage&&) = default;

    WEBCORE_EXPORT static std::optional<ShareableSpatialImage> create(BitmapImage&);

    WEBCORE_EXPORT RetainPtr<CFDataRef> createHEICData() const;

private:
    friend struct IPC::ArgumentCoder<ShareableSpatialImage>;

    ShareableBitmap::Handle m_leftHandle;
    ShareableBitmap::Handle m_rightHandle;
    SpatialImageEyeProperties m_leftMetadata;
    SpatialImageEyeProperties m_rightMetadata;
};

} // namespace WebCore

#endif // USE(CG)
