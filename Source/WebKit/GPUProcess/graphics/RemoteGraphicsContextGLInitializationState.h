/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL)

#include <WebCore/GraphicsTypesGL.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class RemoteGraphicsContextGLInitializationState {
public:
    String availableExtensions;
    String requestableExtensions;
    GCGLenum externalImageTarget { 0 };
    GCGLenum externalImageBindingQuery { 0 };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<RemoteGraphicsContextGLInitializationState> decode(Decoder&);
};

template<class Encoder>
void RemoteGraphicsContextGLInitializationState::encode(Encoder& encoder) const
{
    encoder << availableExtensions;
    encoder << requestableExtensions;
    encoder << externalImageTarget;
    encoder << externalImageBindingQuery;
}

template<class Decoder>
std::optional<RemoteGraphicsContextGLInitializationState> RemoteGraphicsContextGLInitializationState::decode(Decoder& decoder)
{
    RemoteGraphicsContextGLInitializationState initializationState;
    if (!decoder.decode(initializationState.availableExtensions))
        return std::nullopt;
    if (!decoder.decode(initializationState.requestableExtensions))
        return std::nullopt;
    if (!decoder.decode(initializationState.externalImageTarget))
        return std::nullopt;
    if (!decoder.decode(initializationState.externalImageBindingQuery))
        return std::nullopt;
    return initializationState;
}

} // namespace WebKit

#endif
