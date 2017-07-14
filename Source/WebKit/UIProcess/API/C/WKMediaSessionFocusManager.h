/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef WKMediaSessionFocusManager_h
#define WKMediaSessionFocusManager_h

#include <WebKit/WKBase.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum WKMediaSessionFocusManagerPlaybackAttribute {
    IsPlaying                     = 1 << 0,
    IsPreviousTrackControlEnabled = 1 << 1,
    IsNextTrackControlEnabled     = 1 << 2,
};
typedef uint32_t WKMediaSessionFocusManagerPlaybackAttributes;

// Media Session Focus Manager Client
typedef void (*WKMediaSessionFocusManagerDidChangePlaybackAttribute)(WKMediaSessionFocusManagerRef manager, WKMediaSessionFocusManagerPlaybackAttribute playbackAttribute, bool value, const void *clientInfo);

typedef struct WKMediaSessionFocusManagerClientBase {
    int                                                  version;
    const void *                                         clientInfo;
} WKMediaSessionFocusManagerClientBase;

typedef struct WKMediaSessionFocusManagerClientV0 {
    WKMediaSessionFocusManagerClientBase                 base;

    // Version 0.
    WKMediaSessionFocusManagerDidChangePlaybackAttribute didChangePlaybackAttribute;
} WKMediaSessionFocusManagerClientV0;

WK_EXPORT WKTypeID WKMediaSessionFocusManagerGetTypeID();

WK_EXPORT void WKMediaSessionFocusManagerSetClient(WKMediaSessionFocusManagerRef manager, const WKMediaSessionFocusManagerClientBase* client);

WK_EXPORT bool WKMediaSessionFocusManagerValueForPlaybackAttribute(WKMediaSessionFocusManagerRef, WKMediaSessionFocusManagerPlaybackAttribute);
WK_EXPORT void WKMediaSessionFocusManagerSetVolumeOfFocusedMediaElement(WKMediaSessionFocusManagerRef, double);

#ifdef __cplusplus
}
#endif

#endif /* WKMediaSessionFocusManager_h */
