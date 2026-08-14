/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "ObjectIdentifierReferenceTracker.h"
#include <WebCore/RenderingResourceIdentifier.h>

namespace WebKit {

// NativeImages held in the RemoteSharedResourceCache are identified by the same
// WebCore::RenderingResourceIdentifier that keys the rendering backend's cache, so an image can be
// adopted from the shared cache into a rendering backend without re-identifying it.
using RemoteNativeImageIdentifier = WebCore::RenderingResourceIdentifier;

// Reference-tracker versioning on top of the identifier, so a NativeImage produced by one context can
// be safely read by another and released when the WebContent-side handle is destroyed.
using RemoteNativeImageReadReference = IPC::ObjectIdentifierReadReference<RemoteNativeImageIdentifier>;
using RemoteNativeImageWriteReference = IPC::ObjectIdentifierWriteReference<RemoteNativeImageIdentifier>;
using RemoteNativeImageReference = IPC::ObjectIdentifierReference<RemoteNativeImageIdentifier>;
using RemoteNativeImageReferenceTracker = IPC::ObjectIdentifierReferenceTracker<RemoteNativeImageIdentifier>;

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
