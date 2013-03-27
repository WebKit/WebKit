/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebMediaPlayer_h
#define WebMediaPlayer_h

#include "../../../Platform/chromium/public/WebCanvas.h"
#include "../../../Platform/chromium/public/WebString.h"
#include "WebTimeRange.h"

namespace WebKit {

class WebAudioSourceProvider;
class WebAudioSourceProviderClient;
class WebMediaPlayerClient;
class WebMediaSource;
class WebString;
class WebURL;
struct WebRect;
struct WebSize;
class WebGraphicsContext3D;

class WebMediaPlayer {
public:
    enum NetworkState {
        NetworkStateEmpty,
        NetworkStateIdle,
        NetworkStateLoading,
        NetworkStateLoaded,
        NetworkStateFormatError,
        NetworkStateNetworkError,
        NetworkStateDecodeError,
    };

    enum ReadyState {
        ReadyStateHaveNothing,
        ReadyStateHaveMetadata,
        ReadyStateHaveCurrentData,
        ReadyStateHaveFutureData,
        ReadyStateHaveEnoughData,
    };

    enum MovieLoadType {
        MovieLoadTypeUnknown,
        MovieLoadTypeDownload,
        MovieLoadTypeStoredStream,
        MovieLoadTypeLiveStream,
    };

    enum Preload {
        PreloadNone,
        PreloadMetaData,
        PreloadAuto,
    };

    // Represents synchronous exceptions that can be thrown from the Encrypted
    // Media methods. This is different from the asynchronous MediaKeyError.
    enum MediaKeyException {
        MediaKeyExceptionNoError,
        MediaKeyExceptionInvalidPlayerState,
        MediaKeyExceptionKeySystemNotSupported,
    };

    enum CORSMode {
        CORSModeUnspecified,
        CORSModeAnonymous,
        CORSModeUseCredentials,
    };

    virtual ~WebMediaPlayer() {}

    virtual void load(const WebURL&, CORSMode) = 0;
    virtual void load(const WebURL&, WebMediaSource*, CORSMode) = 0;
    virtual void cancelLoad() = 0;

    // Playback controls.
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual bool supportsFullscreen() const = 0;
    virtual bool supportsSave() const = 0;
    virtual void seek(float seconds) = 0;
    virtual void setEndTime(float seconds) = 0;
    virtual void setRate(float) = 0;
    virtual void setVolume(float) = 0;
    virtual void setVisible(bool) = 0;
    virtual void setPreload(Preload) { };
    virtual bool totalBytesKnown() = 0;
    virtual const WebTimeRanges& buffered() = 0;
    virtual float maxTimeSeekable() const = 0;

    virtual void setSize(const WebSize&) = 0;

    virtual void paint(WebCanvas*, const WebRect&, uint8_t alpha) = 0;

    // True if the loaded media has a playable video/audio track.
    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;

    // Dimension of the video.
    virtual WebSize naturalSize() const = 0;

    // Getters of playback state.
    virtual bool paused() const = 0;
    virtual bool seeking() const = 0;
    virtual float duration() const = 0;
    virtual float currentTime() const = 0;

    // Get rate of loading the resource.
    virtual int dataRate() const = 0;

    // Internal states of loading and network.
    virtual NetworkState networkState() const = 0;
    virtual ReadyState readyState() const = 0;

    virtual bool didLoadingProgress() const = 0;
    virtual unsigned long long totalBytes() const = 0;

    virtual bool hasSingleSecurityOrigin() const = 0;
    virtual bool didPassCORSAccessCheck() const = 0;
    virtual MovieLoadType movieLoadType() const = 0;

    virtual float mediaTimeForTimeValue(float timeValue) const = 0;

    virtual unsigned decodedFrameCount() const = 0;
    virtual unsigned droppedFrameCount() const = 0;
    virtual unsigned audioDecodedByteCount() const = 0;
    virtual unsigned videoDecodedByteCount() const = 0;

    // Do a GPU-GPU textures copy if possible.
    virtual bool copyVideoTextureToPlatformTexture(WebGraphicsContext3D*, unsigned texture, unsigned level, unsigned internalFormat, bool premultiplyAlpha, bool flipY) { return false; }

    virtual WebAudioSourceProvider* audioSourceProvider() { return 0; }

    // Returns whether keySystem is supported. If true, the result will be
    // reported by an event.
    virtual MediaKeyException generateKeyRequest(const WebString& keySystem, const unsigned char* initData, unsigned initDataLength) { return MediaKeyExceptionKeySystemNotSupported; }
    virtual MediaKeyException addKey(const WebString& keySystem, const unsigned char* key, unsigned keyLength, const unsigned char* initData, unsigned initDataLength, const WebString& sessionId) { return MediaKeyExceptionKeySystemNotSupported; }
    virtual MediaKeyException cancelKeyRequest(const WebString& keySystem, const WebString& sessionId) { return MediaKeyExceptionKeySystemNotSupported; }

    // Instruct WebMediaPlayer to enter/exit fullscreen.
    virtual void enterFullscreen() { }
    virtual void exitFullscreen() { }
    // Returns true if the player can enter fullscreen.
    virtual bool canEnterFullscreen() const { return false; }
};

} // namespace WebKit

#endif
