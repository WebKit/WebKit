/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_HYBRIDDATAENGINE_H_
#define WEBRTC_MEDIA_BASE_HYBRIDDATAENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/media/base/codec.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/mediaengine.h"

namespace cricket {

class HybridDataEngine : public DataEngineInterface {
 public:
  // Takes ownership.
  HybridDataEngine(DataEngineInterface* first,
                   DataEngineInterface* second)
      : first_(first),
        second_(second) {
    codecs_ = first_->data_codecs();
    codecs_.insert(
        codecs_.end(),
        second_->data_codecs().begin(),
        second_->data_codecs().end());
  }

  virtual DataMediaChannel* CreateChannel(DataChannelType data_channel_type) {
    DataMediaChannel* channel = NULL;
    if (first_) {
      channel = first_->CreateChannel(data_channel_type);
    }
    if (!channel && second_) {
      channel = second_->CreateChannel(data_channel_type);
    }
    return channel;
  }

  virtual const std::vector<DataCodec>& data_codecs() { return codecs_; }

 private:
  std::unique_ptr<DataEngineInterface> first_;
  std::unique_ptr<DataEngineInterface> second_;
  std::vector<DataCodec> codecs_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_HYBRIDDATAENGINE_H_
