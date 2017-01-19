/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/base/gunit.h"

using buzz::Jid;

TEST(JidTest, TestDomain) {
  Jid jid("dude");
  EXPECT_EQ("", jid.node());
  EXPECT_EQ("dude", jid.domain());
  EXPECT_EQ("", jid.resource());
  EXPECT_EQ("dude", jid.Str());
  EXPECT_EQ("dude", jid.BareJid().Str());
  EXPECT_TRUE(jid.IsValid());
  EXPECT_TRUE(jid.IsBare());
  EXPECT_FALSE(jid.IsFull());
}

TEST(JidTest, TestNodeDomain) {
  Jid jid("walter@dude");
  EXPECT_EQ("walter", jid.node());
  EXPECT_EQ("dude", jid.domain());
  EXPECT_EQ("", jid.resource());
  EXPECT_EQ("walter@dude", jid.Str());
  EXPECT_EQ("walter@dude", jid.BareJid().Str());
  EXPECT_TRUE(jid.IsValid());
  EXPECT_TRUE(jid.IsBare());
  EXPECT_FALSE(jid.IsFull());
}

TEST(JidTest, TestDomainResource) {
  Jid jid("dude/bowlingalley");
  EXPECT_EQ("", jid.node());
  EXPECT_EQ("dude", jid.domain());
  EXPECT_EQ("bowlingalley", jid.resource());
  EXPECT_EQ("dude/bowlingalley", jid.Str());
  EXPECT_EQ("dude", jid.BareJid().Str());
  EXPECT_TRUE(jid.IsValid());
  EXPECT_FALSE(jid.IsBare());
  EXPECT_TRUE(jid.IsFull());
}

TEST(JidTest, TestNodeDomainResource) {
  Jid jid("walter@dude/bowlingalley");
  EXPECT_EQ("walter", jid.node());
  EXPECT_EQ("dude", jid.domain());
  EXPECT_EQ("bowlingalley", jid.resource());
  EXPECT_EQ("walter@dude/bowlingalley", jid.Str());
  EXPECT_EQ("walter@dude", jid.BareJid().Str());
  EXPECT_TRUE(jid.IsValid());
  EXPECT_FALSE(jid.IsBare());
  EXPECT_TRUE(jid.IsFull());
}

TEST(JidTest, TestNode) {
  Jid jid("walter@");
  EXPECT_EQ("", jid.node());
  EXPECT_EQ("", jid.domain());
  EXPECT_EQ("", jid.resource());
  EXPECT_EQ("", jid.Str());
  EXPECT_EQ("", jid.BareJid().Str());
  EXPECT_FALSE(jid.IsValid());
  EXPECT_TRUE(jid.IsBare());
  EXPECT_FALSE(jid.IsFull());
}

TEST(JidTest, TestResource) {
  Jid jid("/bowlingalley");
  EXPECT_EQ("", jid.node());
  EXPECT_EQ("", jid.domain());
  EXPECT_EQ("", jid.resource());
  EXPECT_EQ("", jid.Str());
  EXPECT_EQ("", jid.BareJid().Str());
  EXPECT_FALSE(jid.IsValid());
  EXPECT_TRUE(jid.IsBare());
  EXPECT_FALSE(jid.IsFull());
}

TEST(JidTest, TestNodeResource) {
  Jid jid("walter@/bowlingalley");
  EXPECT_EQ("", jid.node());
  EXPECT_EQ("", jid.domain());
  EXPECT_EQ("", jid.resource());
  EXPECT_EQ("", jid.Str());
  EXPECT_EQ("", jid.BareJid().Str());
  EXPECT_FALSE(jid.IsValid());
  EXPECT_TRUE(jid.IsBare());
  EXPECT_FALSE(jid.IsFull());
}

TEST(JidTest, TestFunky) {
  Jid jid("bowling@muchat/walter@dude");
  EXPECT_EQ("bowling", jid.node());
  EXPECT_EQ("muchat", jid.domain());
  EXPECT_EQ("walter@dude", jid.resource());
  EXPECT_EQ("bowling@muchat/walter@dude", jid.Str());
  EXPECT_EQ("bowling@muchat", jid.BareJid().Str());
  EXPECT_TRUE(jid.IsValid());
  EXPECT_FALSE(jid.IsBare());
  EXPECT_TRUE(jid.IsFull());
}

TEST(JidTest, TestFunky2) {
  Jid jid("muchat/walter@dude");
  EXPECT_EQ("", jid.node());
  EXPECT_EQ("muchat", jid.domain());
  EXPECT_EQ("walter@dude", jid.resource());
  EXPECT_EQ("muchat/walter@dude", jid.Str());
  EXPECT_EQ("muchat", jid.BareJid().Str());
  EXPECT_TRUE(jid.IsValid());
  EXPECT_FALSE(jid.IsBare());
  EXPECT_TRUE(jid.IsFull());
}
