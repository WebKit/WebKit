/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef Nix_Platform_h
#define Nix_Platform_h

#include "AudioDevice.h"
#include "Gamepads.h"
#include <stdint.h>

namespace Nix {

class MultiChannelPCMData;
class FFTFrame;
class ThemeEngine;
class MediaPlayer;
class MediaPlayerClient;

class NIX_EXPORT Platform {
public:

    static void initialize(Platform*);
    static Platform* current();

    // Audio
    virtual float audioHardwareSampleRate() { return 0; }
    virtual size_t audioHardwareBufferSize() { return 0; }
    virtual unsigned audioHardwareOutputChannels() { return 0; }

    // Creates a device for audio I/O.
    // Pass in (numberOfInputChannels > 0) if live/local audio input is desired.
    virtual AudioDevice* createAudioDevice(const char* /*inputDeviceId*/, size_t /*bufferSize*/, unsigned /*numberOfInputChannels*/, unsigned /*numberOfOutputChannels*/, double /*sampleRate*/, AudioDevice::RenderCallback*) { return nullptr; }

    // Gamepad
    virtual void sampleGamepads(Gamepads& into) { into.length = 0; }

    // FFTFrame
    virtual FFTFrame* createFFTFrame(unsigned /*fftsize*/) { return nullptr; }

    // Decodes the in-memory audio file data and returns the linear PCM audio data.
    virtual MultiChannelPCMData* decodeAudioResource(const void* /*audioData*/, size_t /*dataSize*/, double /*sampleRate*/) { return nullptr; }

    // Theme engine
    virtual ThemeEngine* themeEngine();

    // Create a MediaPlayer, used to... play media :-)
    virtual MediaPlayer* createMediaPlayer(MediaPlayerClient*) { return nullptr; }
protected:
    virtual ~Platform() { }
};

} // namespace Nix

#endif // Nix_Platform_h
