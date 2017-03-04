/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_STREAMCOLLECTION_H_
#define WEBRTC_PC_STREAMCOLLECTION_H_

#include <string>
#include <vector>

#include "webrtc/api/peerconnectioninterface.h"

namespace webrtc {

// Implementation of StreamCollection.
class StreamCollection : public StreamCollectionInterface {
 public:
  static rtc::scoped_refptr<StreamCollection> Create() {
    rtc::RefCountedObject<StreamCollection>* implementation =
         new rtc::RefCountedObject<StreamCollection>();
    return implementation;
  }

  static rtc::scoped_refptr<StreamCollection> Create(
      StreamCollection* streams) {
    rtc::RefCountedObject<StreamCollection>* implementation =
         new rtc::RefCountedObject<StreamCollection>(streams);
    return implementation;
  }

  virtual size_t count() {
    return media_streams_.size();
  }

  virtual MediaStreamInterface* at(size_t index) {
    return media_streams_.at(index);
  }

  virtual MediaStreamInterface* find(const std::string& label) {
    for (StreamVector::iterator it = media_streams_.begin();
         it != media_streams_.end(); ++it) {
      if ((*it)->label().compare(label) == 0) {
        return (*it);
      }
    }
    return NULL;
  }

  virtual MediaStreamTrackInterface* FindAudioTrack(
      const std::string& id) {
    for (size_t i = 0; i < media_streams_.size(); ++i) {
      MediaStreamTrackInterface* track = media_streams_[i]->FindAudioTrack(id);
      if (track) {
        return track;
      }
    }
    return NULL;
  }

  virtual MediaStreamTrackInterface* FindVideoTrack(
      const std::string& id) {
    for (size_t i = 0; i < media_streams_.size(); ++i) {
      MediaStreamTrackInterface* track = media_streams_[i]->FindVideoTrack(id);
      if (track) {
        return track;
      }
    }
    return NULL;
  }

  void AddStream(MediaStreamInterface* stream) {
    for (StreamVector::iterator it = media_streams_.begin();
         it != media_streams_.end(); ++it) {
      if ((*it)->label().compare(stream->label()) == 0)
        return;
    }
    media_streams_.push_back(stream);
  }

  void RemoveStream(MediaStreamInterface* remove_stream) {
    for (StreamVector::iterator it = media_streams_.begin();
         it != media_streams_.end(); ++it) {
      if ((*it)->label().compare(remove_stream->label()) == 0) {
        media_streams_.erase(it);
        break;
      }
    }
  }

 protected:
  StreamCollection() {}
  explicit StreamCollection(StreamCollection* original)
      : media_streams_(original->media_streams_) {
  }
  typedef std::vector<rtc::scoped_refptr<MediaStreamInterface> >
      StreamVector;
  StreamVector media_streams_;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_STREAMCOLLECTION_H_
