/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/desktop_geometry.h"

#include "webrtc/test/gtest.h"

namespace webrtc {

TEST(DesktopRectTest, UnionBetweenTwoNonEmptyRects) {
  DesktopRect rect = DesktopRect::MakeLTRB(1, 1, 2, 2);
  rect.UnionWith(DesktopRect::MakeLTRB(-2, -2, -1, -1));
  ASSERT_TRUE(rect.equals(DesktopRect::MakeLTRB(-2, -2, 2, 2)));
}

TEST(DesktopRectTest, UnionWithEmptyRect) {
  DesktopRect rect = DesktopRect::MakeWH(1, 1);
  rect.UnionWith(DesktopRect());
  ASSERT_TRUE(rect.equals(DesktopRect::MakeWH(1, 1)));

  rect = DesktopRect::MakeXYWH(1, 1, 2, 2);
  rect.UnionWith(DesktopRect());
  ASSERT_TRUE(rect.equals(DesktopRect::MakeXYWH(1, 1, 2, 2)));

  rect = DesktopRect::MakeXYWH(1, 1, 2, 2);
  rect.UnionWith(DesktopRect::MakeXYWH(3, 3, 0, 0));
  ASSERT_TRUE(rect.equals(DesktopRect::MakeXYWH(1, 1, 2, 2)));
}

TEST(DesktopRectTest, EmptyRectUnionWithNonEmptyOne) {
  DesktopRect rect;
  rect.UnionWith(DesktopRect::MakeWH(1, 1));
  ASSERT_TRUE(rect.equals(DesktopRect::MakeWH(1, 1)));

  rect = DesktopRect();
  rect.UnionWith(DesktopRect::MakeXYWH(1, 1, 2, 2));
  ASSERT_TRUE(rect.equals(DesktopRect::MakeXYWH(1, 1, 2, 2)));

  rect = DesktopRect::MakeXYWH(3, 3, 0, 0);
  rect.UnionWith(DesktopRect::MakeXYWH(1, 1, 2, 2));
  ASSERT_TRUE(rect.equals(DesktopRect::MakeXYWH(1, 1, 2, 2)));
}

TEST(DesktopRectTest, EmptyRectUnionWithEmptyOne) {
  DesktopRect rect;
  rect.UnionWith(DesktopRect());
  ASSERT_TRUE(rect.is_empty());

  rect = DesktopRect::MakeXYWH(1, 1, 0, 0);
  rect.UnionWith(DesktopRect());
  ASSERT_TRUE(rect.is_empty());

  rect = DesktopRect();
  rect.UnionWith(DesktopRect::MakeXYWH(1, 1, 0, 0));
  ASSERT_TRUE(rect.is_empty());

  rect = DesktopRect::MakeXYWH(1, 1, 0, 0);
  rect.UnionWith(DesktopRect::MakeXYWH(-1, -1, 0, 0));
  ASSERT_TRUE(rect.is_empty());
}

}  // namespace webrtc
