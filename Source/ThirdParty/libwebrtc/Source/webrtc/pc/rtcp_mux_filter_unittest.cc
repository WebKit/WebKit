/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtcp_mux_filter.h"

#include "pc/session_description.h"
#include "test/gtest.h"

TEST(RtcpMuxFilterTest, IsActiveSender) {
  webrtc::RtcpMuxFilter filter;
  // Init state - not active
  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());
  // After sent offer, demux should not be active.
  filter.SetOffer(true, webrtc::CS_LOCAL);
  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());
  // Remote accepted, filter is now active.
  filter.SetAnswer(true, webrtc::CS_REMOTE);
  EXPECT_TRUE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_TRUE(filter.IsFullyActive());
}

// Test that we can receive provisional answer and final answer.
TEST(RtcpMuxFilterTest, ReceivePrAnswer) {
  webrtc::RtcpMuxFilter filter;
  filter.SetOffer(true, webrtc::CS_LOCAL);
  // Received provisional answer with mux enabled.
  EXPECT_TRUE(filter.SetProvisionalAnswer(true, webrtc::CS_REMOTE));
  // We are now provisionally active since both sender and receiver support mux.
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());
  // Received provisional answer with mux disabled.
  EXPECT_TRUE(filter.SetProvisionalAnswer(false, webrtc::CS_REMOTE));
  // We are now inactive since the receiver doesn't support mux.
  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());
  // Received final answer with mux enabled.
  EXPECT_TRUE(filter.SetAnswer(true, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_TRUE(filter.IsFullyActive());
}

TEST(RtcpMuxFilterTest, IsActiveReceiver) {
  webrtc::RtcpMuxFilter filter;
  // Init state - not active.
  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());
  // After received offer, demux should not be active
  filter.SetOffer(true, webrtc::CS_REMOTE);
  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());
  // We accept, filter is now active
  filter.SetAnswer(true, webrtc::CS_LOCAL);
  EXPECT_TRUE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_TRUE(filter.IsFullyActive());
}

// Test that we can send provisional answer and final answer.
TEST(RtcpMuxFilterTest, SendPrAnswer) {
  webrtc::RtcpMuxFilter filter;
  filter.SetOffer(true, webrtc::CS_REMOTE);
  // Send provisional answer with mux enabled.
  EXPECT_TRUE(filter.SetProvisionalAnswer(true, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());
  // Received provisional answer with mux disabled.
  EXPECT_TRUE(filter.SetProvisionalAnswer(false, webrtc::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());
  // Send final answer with mux enabled.
  EXPECT_TRUE(filter.SetAnswer(true, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_TRUE(filter.IsFullyActive());
}

// Test that we can enable the filter in an update.
// We can not disable the filter later since that would mean we need to
// recreate a rtcp transport channel.
TEST(RtcpMuxFilterTest, EnableFilterDuringUpdate) {
  webrtc::RtcpMuxFilter filter;
  EXPECT_FALSE(filter.IsActive());
  EXPECT_TRUE(filter.SetOffer(false, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(false, webrtc::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());

  EXPECT_TRUE(filter.SetOffer(true, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetOffer(false, webrtc::CS_REMOTE));
  EXPECT_FALSE(filter.SetAnswer(false, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
}

// Test that SetOffer can be called twice.
TEST(RtcpMuxFilterTest, SetOfferTwice) {
  webrtc::RtcpMuxFilter filter;

  EXPECT_TRUE(filter.SetOffer(true, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.SetOffer(true, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  webrtc::RtcpMuxFilter filter2;
  EXPECT_TRUE(filter2.SetOffer(false, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter2.SetOffer(false, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter2.SetAnswer(false, webrtc::CS_REMOTE));
  EXPECT_FALSE(filter2.IsActive());
}

// Test that the filter can be enabled twice.
TEST(RtcpMuxFilterTest, EnableFilterTwiceDuringUpdate) {
  webrtc::RtcpMuxFilter filter;

  EXPECT_TRUE(filter.SetOffer(true, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_TRUE(filter.SetOffer(true, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
}

// Test that the filter can be kept disabled during updates.
TEST(RtcpMuxFilterTest, KeepFilterDisabledDuringUpdate) {
  webrtc::RtcpMuxFilter filter;

  EXPECT_TRUE(filter.SetOffer(false, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(false, webrtc::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());

  EXPECT_TRUE(filter.SetOffer(false, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(false, webrtc::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());
}

// Test that we can SetActive and then can't deactivate.
TEST(RtcpMuxFilterTest, SetActiveCantDeactivate) {
  webrtc::RtcpMuxFilter filter;

  filter.SetActive();
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetOffer(false, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetOffer(true, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetProvisionalAnswer(false, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetProvisionalAnswer(true, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetAnswer(false, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetAnswer(true, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetOffer(false, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetOffer(true, webrtc::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetProvisionalAnswer(false, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetProvisionalAnswer(true, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetAnswer(false, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetAnswer(true, webrtc::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
}
