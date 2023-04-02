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

#include <wtf/EnumTraits.h>
#include <wtf/ObjectIdentifier.h>

namespace WebKit {

enum class DrawingAreaType : uint8_t {
#if PLATFORM(COCOA)
#if !PLATFORM(IOS_FAMILY)
    TiledCoreAnimation,
#endif
    RemoteLayerTree,
#elif USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    CoordinatedGraphics,
#endif
#if USE(GRAPHICS_LAYER_WC)
    WC,
#endif
};
    
enum {
    ActivityStateChangeAsynchronous = 0
};
typedef uint64_t ActivityStateChangeID;

struct DrawingAreaIdentifierType;
using DrawingAreaIdentifier = ObjectIdentifier<DrawingAreaIdentifierType>;

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::DrawingAreaType> {
    using values = EnumValues<
        WebKit::DrawingAreaType
#if PLATFORM(COCOA)
#if !PLATFORM(IOS_FAMILY)
        , WebKit::DrawingAreaType::TiledCoreAnimation
#endif
        , WebKit::DrawingAreaType::RemoteLayerTree
#elif USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
        , WebKit::DrawingAreaType::CoordinatedGraphics
#endif
#if USE(GRAPHICS_LAYER_WC)
        , WebKit::DrawingAreaType::WC
#endif
    >;
};

} // namespace WTF
