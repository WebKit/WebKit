/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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

#ifndef MediaPlayerPrivateChromium_h
#define MediaPlayerPrivateChromium_h

#if ENABLE(VIDEO)

#include "MediaPlayer.h"

namespace WebCore {

class MediaPlayerPrivate : public Noncopyable {
public:
    MediaPlayerPrivate(MediaPlayer*);
    ~MediaPlayerPrivate();

    IntSize naturalSize() const;
    bool hasVideo() const;

    void load(const String& url);
    void cancelLoad();

    void play();
    void pause();    

    bool paused() const;
    bool seeking() const;

    float duration() const;
    float currentTime() const;
    void seek(float time);
    void setEndTime(float);

    void setRate(float);
    void setVolume(float);

    int dataRate() const;

    MediaPlayer::NetworkState networkState() const;
    MediaPlayer::ReadyState readyState() const;

    float maxTimeBuffered() const;
    float maxTimeSeekable() const;
    unsigned bytesLoaded() const;
    bool totalBytesKnown() const;
    unsigned totalBytes() const;

    void setVisible(bool);
    void setRect(const IntRect&);

    void paint(GraphicsContext*, const IntRect&);

    static void getSupportedTypes(HashSet<String>&);
    static bool isAvailable();

    // Public methods to be called by WebMediaPlayer
    FrameView* frameView();
    void networkStateChanged();
    void readyStateChanged();
    void timeChanged();
    void volumeChanged();
    void repaint();

private:
    MediaPlayer* m_player;
    void* m_data;
};

}  // namespace WebCore

#endif

#endif // MediaPlayerPrivateChromium_h
