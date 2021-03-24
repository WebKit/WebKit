/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PlatformMediaSessionCoordinator.h"

#if ENABLE(MEDIA_SESSION_COORDINATOR)

#include <wtf/NeverDestroyed.h>

namespace WebCore {

String convertEnumerationToString(PlatformMediaSessionCoordinator::MediaSessionReadyState state)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("HaveNothing"),
        MAKE_STATIC_STRING_IMPL("HaveMetadata"),
        MAKE_STATIC_STRING_IMPL("HaveCurrentData"),
        MAKE_STATIC_STRING_IMPL("HaveFutureData"),
        MAKE_STATIC_STRING_IMPL("HaveEnoughData"),
    };
    static_assert(!static_cast<size_t>(PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveNothing), "MediaSessionReadyState::HaveNothing is not 0 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveMetadata) == 1, "MediaSessionReadyState::HaveMetadata is not 1 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveCurrentData) == 2, "MediaSessionReadyState::HaveCurrentData is not 2 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveFutureData) == 3, "MediaSessionReadyState::HaveFutureData is not 3 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveEnoughData) == 4, "MediaSessionReadyState::HaveEnoughData is not 4 as expected");
    ASSERT(static_cast<size_t>(state) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(state)];
}

String convertEnumerationToString(PlatformMediaSessionCoordinator::MediaSessionPlaybackState state)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("None"),
        MAKE_STATIC_STRING_IMPL("Paused"),
        MAKE_STATIC_STRING_IMPL("Playing"),
    };
    static_assert(!static_cast<size_t>(PlatformMediaSessionCoordinator::MediaSessionPlaybackState::None), "MediaSessionPlaybackState::None is not 0 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSessionCoordinator::MediaSessionPlaybackState::Paused) == 1, "MediaSessionPlaybackState::Paused is not 1 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSessionCoordinator::MediaSessionPlaybackState::Playing) == 2, "MediaSessionPlaybackState::Playing is not 2 as expected");
    ASSERT(static_cast<size_t>(state) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(state)];
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
