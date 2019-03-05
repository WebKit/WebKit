/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY MOTOROLA INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebProcess.h"

#include "WebProcessCreationParameters.h"
#include <WebCore/GStreamerCommon.h>
#include <WebCore/MemoryCache.h>

#if PLATFORM(WAYLAND)
#include "WaylandCompositorDisplay.h"
#endif

#if PLATFORM(WPE)
#include <WebCore/PlatformDisplayLibWPE.h>
#include <wpe/wpe.h>
#endif

namespace WebKit {

void WebProcess::platformSetCacheModel(CacheModel cacheModel)
{
    WebCore::MemoryCache::singleton().setDisabled(cacheModel == CacheModel::DocumentViewer);
}

void WebProcess::platformInitializeWebProcess(WebProcessCreationParameters&& parameters)
{
#if PLATFORM(WPE)
    if (!parameters.isServiceWorkerProcess) {
        auto& implementationLibraryName = parameters.implementationLibraryName;
        if (!implementationLibraryName.isNull() && implementationLibraryName.data()[0] != '\0')
            wpe_loader_init(parameters.implementationLibraryName.data());

        RELEASE_ASSERT(is<PlatformDisplayLibWPE>(PlatformDisplay::sharedDisplay()));
        downcast<PlatformDisplayLibWPE>(PlatformDisplay::sharedDisplay()).initialize(parameters.hostClientFileDescriptor.releaseFileDescriptor());
    }
#endif
#if PLATFORM(WAYLAND)
    m_waylandCompositorDisplay = WaylandCompositorDisplay::create(parameters.waylandCompositorDisplayName);
#endif
#if USE(GSTREAMER)
    WebCore::initializeGStreamer(WTFMove(parameters.gstreamerOptions));
#endif
}

void WebProcess::platformTerminate()
{
}

} // namespace WebKit
