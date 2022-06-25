/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include <map>
#include <wtf/Deque.h>
#include <wtf/MediaTime.h>
#include <wtf/Observer.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdMap.h>
#include <wtf/WeakHashSet.h>

OBJC_CLASS AVPlayer;
OBJC_CLASS AVPlayerItem;
OBJC_CLASS AVPlayerItemVideoOutput;
OBJC_CLASS WebQueuedVideoOutputDelegate;

typedef struct __CVBuffer *CVPixelBufferRef;

namespace WebCore {

class QueuedVideoOutput
    : public RefCounted<QueuedVideoOutput>
    , public CanMakeWeakPtr<QueuedVideoOutput> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<QueuedVideoOutput> create(AVPlayerItem*, AVPlayer*);
    ~QueuedVideoOutput();

    bool valid();
    void invalidate();
    bool hasImageForTime(const MediaTime&) const;

    struct VideoFrameEntry {
        RetainPtr<CVPixelBufferRef> pixelBuffer;
        MediaTime displayTime;
    };
    VideoFrameEntry takeVideoFrameEntryForTime(const MediaTime&);

    void addVideoFrameEntries(Vector<VideoFrameEntry>&&);
    void purgeVideoFrameEntries();

    using CurrentImageChangedObserver = Observer<void()>;
    void addCurrentImageChangedObserver(const CurrentImageChangedObserver&);

    using ImageMap = StdMap<MediaTime, RetainPtr<CVPixelBufferRef>>;

    void rateChanged(float);

private:
    QueuedVideoOutput(AVPlayerItem*, AVPlayer*);

    void purgeImagesBeforeTime(const MediaTime&);
    void configureNextImageTimeObserver();
    void cancelNextImageTimeObserver();
    void nextImageTimeReached();

    RetainPtr<AVPlayerItem> m_playerItem;
    RetainPtr<AVPlayer> m_player;
    RetainPtr<WebQueuedVideoOutputDelegate> m_delegate;
    RetainPtr<AVPlayerItemVideoOutput> m_videoOutput;
    RetainPtr<id> m_videoTimebaseObserver;
    RetainPtr<id> m_nextImageTimebaseObserver;

    ImageMap m_videoFrames;
    WeakHashSet<CurrentImageChangedObserver> m_currentImageChangedObservers;

    bool m_paused { true };
};

}

#endif
