/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc. All Rights reserved.
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

#if PLATFORM(COCOA)
OBJC_CLASS CALayer;
using PlatformLayer = CALayer;
#elif PLATFORM(WIN) && USE(CA)
typedef struct _CACFLayer PlatformLayer;
#elif USE(NICOSIA)
namespace Nicosia {
class PlatformLayer;
}
using PlatformLayer = Nicosia::PlatformLayer;
#elif USE(COORDINATED_GRAPHICS)
namespace WebCore {
class TextureMapperPlatformLayerProxyProvider;
};
using PlatformLayer = WebCore::TextureMapperPlatformLayerProxyProvider;
#elif USE(TEXTURE_MAPPER)
namespace WebCore {
class TextureMapperPlatformLayer;
};
using PlatformLayer = WebCore::TextureMapperPlatformLayer;
#else
using PlatformLayer = void*;
#endif

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
using PlatformLayerContainer = WTF::RetainPtr<PlatformLayer>;
#elif USE(TEXTURE_MAPPER)
using PlatformLayerContainer = std::unique_ptr<PlatformLayer>;
#else
#include <wtf/RefPtr.h>
using PlatformLayerContainer = WTF::RefPtr<PlatformLayer>;
#endif
