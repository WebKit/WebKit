/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_STREAM_COLLECTION_H_
#define PC_STREAM_COLLECTION_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"

namespace webrtc {

// Implementation of StreamCollection.
class StreamCollection : public StreamCollectionInterface {
 public:
  static scoped_refptr<StreamCollection> Create() {
    return make_ref_counted<StreamCollection>();
  }

  static scoped_refptr<StreamCollection> Create(StreamCollection* streams) {
    return make_ref_counted<StreamCollection>(streams);
  }

  size_t count() override { return media_streams_.size(); }

  MediaStreamInterface* at(size_t index) override {
    return media_streams_.at(index).get();
  }

  MediaStreamInterface* find(const std::string& id) override {
    for (StreamVector::iterator it = media_streams_.begin();
         it != media_streams_.end(); ++it) {
      if ((*it)->id().compare(id) == 0) {
        return (*it).get();
      }
    }
    return NULL;
  }

  MediaStreamTrackInterface* FindAudioTrack(const std::string& id) override {
    for (size_t i = 0; i < media_streams_.size(); ++i) {
      MediaStreamTrackInterface* track =
          media_streams_[i]->FindAudioTrack(id).get();
      if (track) {
        return track;
      }
    }
    return NULL;
  }

  MediaStreamTrackInterface* FindVideoTrack(const std::string& id) override {
    for (size_t i = 0; i < media_streams_.size(); ++i) {
      MediaStreamTrackInterface* track =
          media_streams_[i]->FindVideoTrack(id).get();
      if (track) {
        return track;
      }
    }
    return NULL;
  }

  void AddStream(scoped_refptr<MediaStreamInterface> stream) {
    for (StreamVector::iterator it = media_streams_.begin();
         it != media_streams_.end(); ++it) {
      if ((*it)->id().compare(stream->id()) == 0)
        return;
    }
    media_streams_.push_back(std::move(stream));
  }

  void RemoveStream(MediaStreamInterface* remove_stream) {
    for (StreamVector::iterator it = media_streams_.begin();
         it != media_streams_.end(); ++it) {
      if ((*it)->id().compare(remove_stream->id()) == 0) {
        media_streams_.erase(it);
        break;
      }
    }
  }

 protected:
  StreamCollection() {}
  explicit StreamCollection(StreamCollection* original)
      : media_streams_(original->media_streams_) {}
  typedef std::vector<scoped_refptr<MediaStreamInterface> > StreamVector;
  StreamVector media_streams_;
};

}  // namespace webrtc

#endif  // PC_STREAM_COLLECTION_H_
