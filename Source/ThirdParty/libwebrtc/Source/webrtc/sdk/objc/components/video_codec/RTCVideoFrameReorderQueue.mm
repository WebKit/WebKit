/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCVideoFrameReorderQueue.h"

namespace webrtc {

bool RTCVideoFrameReorderQueue::isEmpty()
{
    return _reorderQueue.empty();
}

uint8_t RTCVideoFrameReorderQueue::reorderSize() const
{
    webrtc::MutexLock lock(&_reorderQueueLock);
    return _reorderSize;
}

void RTCVideoFrameReorderQueue::setReorderSize(uint8_t size)
{
    webrtc::MutexLock lock(&_reorderQueueLock);
    _reorderSize = size;
}

void RTCVideoFrameReorderQueue::append(RTCVideoFrame* frame, uint8_t reorderSize)
{
    webrtc::MutexLock lock(&_reorderQueueLock);
    _reorderQueue.push_back(std::make_unique<RTCVideoFrameWithOrder>(frame, reorderSize));
    std::sort(_reorderQueue.begin(), _reorderQueue.end(), [](auto& a, auto& b) {
        return a->timeStamp < b->timeStamp;
    });
}

RTCVideoFrame* RTCVideoFrameReorderQueue::takeIfAvailable()
{
    webrtc::MutexLock lock(&_reorderQueueLock);
    if (_reorderQueue.size() && _reorderQueue.size() > _reorderQueue.front()->reorderSize) {
        auto *frame = _reorderQueue.front()->take();
        _reorderQueue.pop_front();
        return frame;
    }
    return nil;
}

RTCVideoFrame* RTCVideoFrameReorderQueue::takeIfAny()
{
    webrtc::MutexLock lock(&_reorderQueueLock);
    if (_reorderQueue.size()) {
        auto *frame = _reorderQueue.front()->take();
        _reorderQueue.pop_front();
        return frame;
    }
    return nil;
}

}
