/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBGPU)

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(COCOA)
OBJC_CLASS MTLRenderPassAttachmentDescriptor;
#endif

namespace WebCore {

class GPUTexture;

class GPURenderPassAttachmentDescriptor : public RefCounted<GPURenderPassAttachmentDescriptor> {
public:
    WEBCORE_EXPORT ~GPURenderPassAttachmentDescriptor();

    WEBCORE_EXPORT unsigned long loadAction() const;
    WEBCORE_EXPORT void setLoadAction(unsigned long);

    WEBCORE_EXPORT unsigned long storeAction() const;
    WEBCORE_EXPORT void setStoreAction(unsigned long);

    WEBCORE_EXPORT void setTexture(RefPtr<GPUTexture>);

#if PLATFORM(COCOA)
    WEBCORE_EXPORT MTLRenderPassAttachmentDescriptor *platformRenderPassAttachmentDescriptor();
#endif

protected:
#if PLATFORM(COCOA)
    GPURenderPassAttachmentDescriptor(MTLRenderPassAttachmentDescriptor *);
    RetainPtr<MTLRenderPassAttachmentDescriptor> m_renderPassAttachmentDescriptor;
#else
    GPURenderPassAttachmentDescriptor();
#endif
};
    
} // namespace WebCore
#endif
