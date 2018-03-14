/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/media_file/media_file.h"
#include "test/gtest.h"

class MediaFileTest : public testing::Test {
 protected:
  void SetUp() {
    // Use number 0 as the the identifier and pass to CreateMediaFile.
    media_file_ = webrtc::MediaFile::CreateMediaFile(0);
    ASSERT_TRUE(media_file_ != NULL);
  }
  void TearDown() {
    webrtc::MediaFile::DestroyMediaFile(media_file_);
    media_file_ = NULL;
  }
  webrtc::MediaFile* media_file_;
};
