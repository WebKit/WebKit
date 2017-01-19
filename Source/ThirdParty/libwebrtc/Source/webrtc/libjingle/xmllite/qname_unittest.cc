/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/base/gunit.h"

using buzz::StaticQName;
using buzz::QName;

TEST(QNameTest, TestTrivial) {
  QName name("test");
  EXPECT_EQ(name.LocalPart(), "test");
  EXPECT_EQ(name.Namespace(), "");
}

TEST(QNameTest, TestSplit) {
  QName name("a:test");
  EXPECT_EQ(name.LocalPart(), "test");
  EXPECT_EQ(name.Namespace(), "a");
  QName name2("a-very:long:namespace:test-this");
  EXPECT_EQ(name2.LocalPart(), "test-this");
  EXPECT_EQ(name2.Namespace(), "a-very:long:namespace");
}

TEST(QNameTest, TestMerge) {
  QName name("a", "test");
  EXPECT_EQ(name.LocalPart(), "test");
  EXPECT_EQ(name.Namespace(), "a");
  EXPECT_EQ(name.Merged(), "a:test");
  QName name2("a-very:long:namespace", "test-this");
  EXPECT_EQ(name2.LocalPart(), "test-this");
  EXPECT_EQ(name2.Namespace(), "a-very:long:namespace");
  EXPECT_EQ(name2.Merged(), "a-very:long:namespace:test-this");
}

TEST(QNameTest, TestAssignment) {
  QName name("a", "test");
  // copy constructor
  QName namecopy(name);
  EXPECT_EQ(namecopy.LocalPart(), "test");
  EXPECT_EQ(namecopy.Namespace(), "a");
  QName nameassigned("");
  nameassigned = name;
  EXPECT_EQ(nameassigned.LocalPart(), "test");
  EXPECT_EQ(nameassigned.Namespace(), "a");
}

TEST(QNameTest, TestConstAssignment) {
  StaticQName name = { "a", "test" };
  QName namecopy(name);
  EXPECT_EQ(namecopy.LocalPart(), "test");
  EXPECT_EQ(namecopy.Namespace(), "a");
  QName nameassigned("");
  nameassigned = name;
  EXPECT_EQ(nameassigned.LocalPart(), "test");
  EXPECT_EQ(nameassigned.Namespace(), "a");
}

TEST(QNameTest, TestEquality) {
  QName name("a-very:long:namespace:test-this");
  QName name2("a-very:long:namespace", "test-this");
  QName name3("a-very:long:namespaxe", "test-this");
  EXPECT_TRUE(name == name2);
  EXPECT_FALSE(name == name3);
}

TEST(QNameTest, TestCompare) {
  QName name("a");
  QName name2("nsa", "a");
  QName name3("nsa", "b");
  QName name4("nsb", "b");

  EXPECT_TRUE(name < name2);
  EXPECT_FALSE(name2 < name);

  EXPECT_FALSE(name2 < name2);

  EXPECT_TRUE(name2 < name3);
  EXPECT_FALSE(name3 < name2);

  EXPECT_TRUE(name3 < name4);
  EXPECT_FALSE(name4 < name3);
}

TEST(QNameTest, TestStaticQName) {
  const StaticQName const_name1 = { "namespace", "local-name1" };
  const StaticQName const_name2 = { "namespace", "local-name2" };
  const QName name("namespace", "local-name1");
  const QName name1 = const_name1;
  const QName name2 = const_name2;

  EXPECT_TRUE(name == const_name1);
  EXPECT_TRUE(const_name1 == name);
  EXPECT_FALSE(name != const_name1);
  EXPECT_FALSE(const_name1 != name);

  EXPECT_TRUE(name == name1);
  EXPECT_TRUE(name1 == name);
  EXPECT_FALSE(name != name1);
  EXPECT_FALSE(name1 != name);

  EXPECT_FALSE(name == name2);
  EXPECT_FALSE(name2 == name);
  EXPECT_TRUE(name != name2);
  EXPECT_TRUE(name2 != name);
}
