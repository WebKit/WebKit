/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

#include "ImageBufferBackendHandle.h"
#include <WebCore/ImageBufferBackend.h>

namespace WebKit {

class ImageBufferBackendHandleSharing : public WebCore::ImageBufferBackendSharing {
public:
    virtual ImageBufferBackendHandle createBackendHandle(SharedMemory::Protection = SharedMemory::Protection::ReadWrite) const = 0;
    virtual ImageBufferBackendHandle takeBackendHandle(SharedMemory::Protection protection = SharedMemory::Protection::ReadWrite) { return createBackendHandle(protection); }
    virtual RefPtr<ShareableBitmap> bitmap() const { return nullptr; }

    virtual void setBackendHandle(ImageBufferBackendHandle&&) { }
    virtual bool hasBackendHandle() const { return false; }
    virtual void clearBackendHandle() { }

private:
    bool isImageBufferBackendHandleSharing() const final { return true; }
};

} // namespace WebKit

#define SPECIALIZE_TYPE_TRAITS_IMAGE_BUFFER_BACKEND_SHARING(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::ImageBufferBackendSharing& backendSharing) { return backendSharing.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_IMAGE_BUFFER_BACKEND_SHARING(WebKit::ImageBufferBackendHandleSharing, isImageBufferBackendHandleSharing())
