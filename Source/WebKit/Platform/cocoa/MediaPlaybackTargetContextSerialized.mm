/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MediaPlaybackTargetContextSerialized.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#import <pal/spi/cocoa/AVFoundationSPI.h>

#if HAVE(WK_SECURE_CODING_AVOUTPUTCONTEXT)
#import <wtf/cocoa/TypeCastsCocoa.h>
#else
#import "WKKeyedCoder.h"
#endif
#import <pal/cocoa/AVFoundationSoftLink.h>

using namespace WebCore;

namespace WebKit {

MediaPlaybackTargetContextSerialized::MediaPlaybackTargetContextSerialized(const MediaPlaybackTargetContext& context)
    : MediaPlaybackTargetContext(MediaPlaybackTargetContextType::Serialized)
    , m_deviceName(context.deviceName())
    , m_hasActiveRoute(context.hasActiveRoute())
    , m_supportsRemoteVideoPlayback(context.supportsRemoteVideoPlayback())
    , m_targetType(is<MediaPlaybackTargetContextSerialized>(context) ? downcast<MediaPlaybackTargetContextSerialized>(context).targetType() : context.type())
{
    if (is<MediaPlaybackTargetContextCocoa>(context)) {
#if HAVE(WK_SECURE_CODING_AVOUTPUTCONTEXT)
        m_context = CoreIPCAVOutputContext { downcast<MediaPlaybackTargetContextCocoa>(context).outputContext().get() };
#else
        auto archiver = adoptNS([WKKeyedCoder new]);
        [downcast<MediaPlaybackTargetContextCocoa>(context).outputContext() encodeWithCoder:archiver.get()];
        auto dictionary = [archiver accumulatedDictionary];
        m_contextID = checked_objc_cast<NSString>([dictionary objectForKey:@"AVOutputContextSerializationKeyContextID"]);
        m_contextType = checked_objc_cast<NSString>([dictionary objectForKey:@"AVOutputContextSerializationKeyContextType"]);
#endif
    } else if (is<MediaPlaybackTargetContextMock>(context))
        m_state = downcast<MediaPlaybackTargetContextMock>(context).state();
    else if (is<MediaPlaybackTargetContextSerialized>(context)) {
        m_state = downcast<MediaPlaybackTargetContextSerialized>(context).m_state;
#if HAVE(WK_SECURE_CODING_AVOUTPUTCONTEXT)
        m_context = downcast<MediaPlaybackTargetContextSerialized>(context).m_context;
#else
        m_contextID = downcast<MediaPlaybackTargetContextSerialized>(context).m_contextID;
        m_contextType = downcast<MediaPlaybackTargetContextSerialized>(context).m_contextType;
#endif
    }
}

#if HAVE(WK_SECURE_CODING_AVOUTPUTCONTEXT)
MediaPlaybackTargetContextSerialized::MediaPlaybackTargetContextSerialized(String&& deviceName, bool hasActiveRoute, bool supportsRemoteVideoPlayback, MediaPlaybackTargetContextType targetType, MediaPlaybackTargetContextMockState state, CoreIPCAVOutputContext&& context)
    : MediaPlaybackTargetContext(MediaPlaybackTargetContextType::Serialized)
    , m_deviceName(WTFMove(deviceName))
    , m_hasActiveRoute(hasActiveRoute)
    , m_supportsRemoteVideoPlayback(supportsRemoteVideoPlayback)
    , m_targetType(targetType)
    , m_state(state)
    , m_context(WTFMove(context))
{
}
#else
MediaPlaybackTargetContextSerialized::MediaPlaybackTargetContextSerialized(String&& deviceName, bool hasActiveRoute, bool supportsRemoteVideoPlayback, MediaPlaybackTargetContextType targetType, MediaPlaybackTargetContextMockState state, String&& contextID, String&& contextType)
    : MediaPlaybackTargetContext(MediaPlaybackTargetContextType::Serialized)
    , m_deviceName(WTFMove(deviceName))
    , m_hasActiveRoute(hasActiveRoute)
    , m_supportsRemoteVideoPlayback(supportsRemoteVideoPlayback)
    , m_targetType(targetType)
    , m_state(state)
    , m_contextID(WTFMove(contextID))
    , m_contextType(WTFMove(contextType))
{
}
#endif

Variant<MediaPlaybackTargetContextCocoa, MediaPlaybackTargetContextMock> MediaPlaybackTargetContextSerialized::platformContext() const
{
    if (m_targetType == MediaPlaybackTargetContextType::Mock)
        return MediaPlaybackTargetContextMock(m_deviceName, m_state);

    ASSERT(m_targetType == MediaPlaybackTargetContextType::AVOutputContext);

#if HAVE(WK_SECURE_CODING_AVOUTPUTCONTEXT)
    return MediaPlaybackTargetContextCocoa(dynamic_objc_cast<AVOutputContext>(m_context.toID()));
#else
    auto propertyList = [NSMutableDictionary dictionaryWithCapacity:2];
    propertyList[@"AVOutputContextSerializationKeyContextID"] = m_contextID.createNSString().get();
    propertyList[@"AVOutputContextSerializationKeyContextType"] = m_contextType.createNSString().get();
    auto unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:propertyList]);
    auto outputContext = adoptNS([[PAL::getAVOutputContextClassSingleton() alloc] initWithCoder:unarchiver.get()]);
    // Variant construction in older clang gives either an error, a vtable linkage error unless we construct it this way.
    Variant<MediaPlaybackTargetContextCocoa, MediaPlaybackTargetContextMock> variant { WTF::InPlaceType<MediaPlaybackTargetContextCocoa>, WTFMove(outputContext) };
    return variant;
#endif
}

} // namespace WebKit

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
