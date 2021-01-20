/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_region.h"

#include <stdlib.h>

#include <algorithm>
#include <cstdint>

#include "test/gtest.h"

namespace webrtc {

namespace {

int RadmonInt(int max) {
  return (rand() / 256) % max;
}

void CompareRegion(const DesktopRegion& region,
                   const DesktopRect rects[],
                   int rects_size) {
  DesktopRegion::Iterator it(region);
  for (int i = 0; i < rects_size; ++i) {
    SCOPED_TRACE(i);
    ASSERT_FALSE(it.IsAtEnd());
    EXPECT_TRUE(it.rect().equals(rects[i]))
        << it.rect().left() << "-" << it.rect().right() << "."
        << it.rect().top() << "-" << it.rect().bottom() << " "
        << rects[i].left() << "-" << rects[i].right() << "." << rects[i].top()
        << "-" << rects[i].bottom();
    it.Advance();
  }
  EXPECT_TRUE(it.IsAtEnd());
}

}  // namespace

// Verify that regions are empty when created.
TEST(DesktopRegionTest, Empty) {
  DesktopRegion r;
  CompareRegion(r, NULL, 0);
}

// Verify that empty rectangles are ignored.
TEST(DesktopRegionTest, AddEmpty) {
  DesktopRegion r;
  DesktopRect rect = DesktopRect::MakeXYWH(1, 2, 0, 0);
  r.AddRect(rect);
  CompareRegion(r, NULL, 0);
}

// Verify that regions with a single rectangles are handled properly.
TEST(DesktopRegionTest, SingleRect) {
  DesktopRegion r;
  DesktopRect rect = DesktopRect::MakeXYWH(1, 2, 3, 4);
  r.AddRect(rect);
  CompareRegion(r, &rect, 1);
}

// Verify that non-overlapping rectangles are not merged.
TEST(DesktopRegionTest, NonOverlappingRects) {
  struct Case {
    int count;
    DesktopRect rects[4];
  } cases[] = {
      {1, {DesktopRect::MakeXYWH(10, 10, 10, 10)}},
      {2,
       {DesktopRect::MakeXYWH(10, 10, 10, 10),
        DesktopRect::MakeXYWH(30, 10, 10, 15)}},
      {2,
       {DesktopRect::MakeXYWH(10, 10, 10, 10),
        DesktopRect::MakeXYWH(10, 30, 10, 5)}},
      {3,
       {DesktopRect::MakeXYWH(10, 10, 10, 9),
        DesktopRect::MakeXYWH(30, 10, 15, 10),
        DesktopRect::MakeXYWH(10, 30, 8, 10)}},
      {4,
       {DesktopRect::MakeXYWH(0, 0, 30, 10),
        DesktopRect::MakeXYWH(40, 0, 10, 30),
        DesktopRect::MakeXYWH(0, 20, 10, 30),
        DesktopRect::MakeXYWH(20, 40, 30, 10)}},
      {4,
       {DesktopRect::MakeXYWH(0, 0, 10, 100),
        DesktopRect::MakeXYWH(20, 10, 30, 10),
        DesktopRect::MakeXYWH(20, 30, 30, 10),
        DesktopRect::MakeXYWH(20, 50, 30, 10)}},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i) {
    SCOPED_TRACE(i);

    DesktopRegion r;

    for (int j = 0; j < cases[i].count; ++j) {
      r.AddRect(cases[i].rects[j]);
    }
    CompareRegion(r, cases[i].rects, cases[i].count);

    SCOPED_TRACE("Reverse");

    // Try inserting rects in reverse order.
    r.Clear();
    for (int j = cases[i].count - 1; j >= 0; --j) {
      r.AddRect(cases[i].rects[j]);
    }
    CompareRegion(r, cases[i].rects, cases[i].count);
  }
}

TEST(DesktopRegionTest, TwoRects) {
  struct Case {
    DesktopRect input_rect1;
    DesktopRect input_rect2;
    int expected_count;
    DesktopRect expected_rects[3];
  } cases[] = {
      // Touching rectangles that merge into one.
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(0, 100, 100, 200),
       1,
       {DesktopRect::MakeLTRB(0, 100, 200, 200)}},
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(100, 0, 200, 100),
       1,
       {DesktopRect::MakeLTRB(100, 0, 200, 200)}},

      // Rectangles touching on the vertical edge.
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(0, 150, 100, 250),
       3,
       {DesktopRect::MakeLTRB(100, 100, 200, 150),
        DesktopRect::MakeLTRB(0, 150, 200, 200),
        DesktopRect::MakeLTRB(0, 200, 100, 250)}},
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(0, 50, 100, 150),
       3,
       {DesktopRect::MakeLTRB(0, 50, 100, 100),
        DesktopRect::MakeLTRB(0, 100, 200, 150),
        DesktopRect::MakeLTRB(100, 150, 200, 200)}},
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(0, 120, 100, 180),
       3,
       {DesktopRect::MakeLTRB(100, 100, 200, 120),
        DesktopRect::MakeLTRB(0, 120, 200, 180),
        DesktopRect::MakeLTRB(100, 180, 200, 200)}},

      // Rectangles touching on the horizontal edge.
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(150, 0, 250, 100),
       2,
       {DesktopRect::MakeLTRB(150, 0, 250, 100),
        DesktopRect::MakeLTRB(100, 100, 200, 200)}},
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(50, 0, 150, 100),
       2,
       {DesktopRect::MakeLTRB(50, 0, 150, 100),
        DesktopRect::MakeLTRB(100, 100, 200, 200)}},
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(120, 0, 180, 100),
       2,
       {DesktopRect::MakeLTRB(120, 0, 180, 100),
        DesktopRect::MakeLTRB(100, 100, 200, 200)}},

      // Overlapping rectangles.
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(50, 50, 150, 150),
       3,
       {DesktopRect::MakeLTRB(50, 50, 150, 100),
        DesktopRect::MakeLTRB(50, 100, 200, 150),
        DesktopRect::MakeLTRB(100, 150, 200, 200)}},
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(150, 50, 250, 150),
       3,
       {DesktopRect::MakeLTRB(150, 50, 250, 100),
        DesktopRect::MakeLTRB(100, 100, 250, 150),
        DesktopRect::MakeLTRB(100, 150, 200, 200)}},
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(0, 120, 150, 180),
       3,
       {DesktopRect::MakeLTRB(100, 100, 200, 120),
        DesktopRect::MakeLTRB(0, 120, 200, 180),
        DesktopRect::MakeLTRB(100, 180, 200, 200)}},
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(120, 0, 180, 150),
       2,
       {DesktopRect::MakeLTRB(120, 0, 180, 100),
        DesktopRect::MakeLTRB(100, 100, 200, 200)}},
      {DesktopRect::MakeLTRB(100, 0, 200, 300),
       DesktopRect::MakeLTRB(0, 100, 300, 200),
       3,
       {DesktopRect::MakeLTRB(100, 0, 200, 100),
        DesktopRect::MakeLTRB(0, 100, 300, 200),
        DesktopRect::MakeLTRB(100, 200, 200, 300)}},

      // One rectangle enclosing another.
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(150, 150, 180, 180),
       1,
       {DesktopRect::MakeLTRB(100, 100, 200, 200)}},
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(100, 100, 180, 180),
       1,
       {DesktopRect::MakeLTRB(100, 100, 200, 200)}},
      {DesktopRect::MakeLTRB(100, 100, 200, 200),
       DesktopRect::MakeLTRB(150, 150, 200, 200),
       1,
       {DesktopRect::MakeLTRB(100, 100, 200, 200)}},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i) {
    SCOPED_TRACE(i);

    DesktopRegion r;

    r.AddRect(cases[i].input_rect1);
    r.AddRect(cases[i].input_rect2);
    CompareRegion(r, cases[i].expected_rects, cases[i].expected_count);

    SCOPED_TRACE("Reverse");

    // Run the same test with rectangles inserted in reverse order.
    r.Clear();
    r.AddRect(cases[i].input_rect2);
    r.AddRect(cases[i].input_rect1);
    CompareRegion(r, cases[i].expected_rects, cases[i].expected_count);
  }
}

// Verify that DesktopRegion::AddRectToRow() works correctly by creating a row
// of not overlapping rectangles and insert an overlapping rectangle into the
// row at different positions. Result is verified by building a map of the
// region in an array and comparing it with the expected values.
TEST(DesktopRegionTest, SameRow) {
  const int kMapWidth = 50;
  const int kLastRectSizes[] = {3, 27};

  DesktopRegion base_region;
  bool base_map[kMapWidth] = {
      false,
  };

  base_region.AddRect(DesktopRect::MakeXYWH(5, 0, 5, 1));
  std::fill_n(base_map + 5, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(15, 0, 5, 1));
  std::fill_n(base_map + 15, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(25, 0, 5, 1));
  std::fill_n(base_map + 25, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(35, 0, 5, 1));
  std::fill_n(base_map + 35, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(45, 0, 5, 1));
  std::fill_n(base_map + 45, 5, true);

  for (size_t i = 0; i < sizeof(kLastRectSizes) / sizeof(kLastRectSizes[0]);
       i++) {
    int last_rect_size = kLastRectSizes[i];
    for (int x = 0; x < kMapWidth - last_rect_size; x++) {
      SCOPED_TRACE(x);

      DesktopRegion r = base_region;
      r.AddRect(DesktopRect::MakeXYWH(x, 0, last_rect_size, 1));

      bool expected_map[kMapWidth];
      std::copy(base_map, base_map + kMapWidth, expected_map);
      std::fill_n(expected_map + x, last_rect_size, true);

      bool map[kMapWidth] = {
          false,
      };

      int pos = -1;
      for (DesktopRegion::Iterator it(r); !it.IsAtEnd(); it.Advance()) {
        EXPECT_GT(it.rect().left(), pos);
        pos = it.rect().right();
        std::fill_n(map + it.rect().left(), it.rect().width(), true);
      }

      EXPECT_TRUE(std::equal(map, map + kMapWidth, expected_map));
    }
  }
}

TEST(DesktopRegionTest, ComplexRegions) {
  struct Case {
    int input_count;
    DesktopRect input_rects[4];
    int expected_count;
    DesktopRect expected_rects[6];
  } cases[] = {
      {3,
       {
           DesktopRect::MakeLTRB(100, 100, 200, 200),
           DesktopRect::MakeLTRB(0, 100, 100, 200),
           DesktopRect::MakeLTRB(310, 110, 320, 120),
       },
       2,
       {DesktopRect::MakeLTRB(0, 100, 200, 200),
        DesktopRect::MakeLTRB(310, 110, 320, 120)}},
      {3,
       {DesktopRect::MakeLTRB(100, 100, 200, 200),
        DesktopRect::MakeLTRB(50, 50, 150, 150),
        DesktopRect::MakeLTRB(300, 125, 350, 175)},
       4,
       {DesktopRect::MakeLTRB(50, 50, 150, 100),
        DesktopRect::MakeLTRB(50, 100, 200, 150),
        DesktopRect::MakeLTRB(300, 125, 350, 175),
        DesktopRect::MakeLTRB(100, 150, 200, 200)}},
      {4,
       {DesktopRect::MakeLTRB(0, 0, 30, 30),
        DesktopRect::MakeLTRB(10, 10, 40, 40),
        DesktopRect::MakeLTRB(20, 20, 50, 50),
        DesktopRect::MakeLTRB(50, 0, 65, 15)},
       6,
       {DesktopRect::MakeLTRB(0, 0, 30, 10),
        DesktopRect::MakeLTRB(50, 0, 65, 15),
        DesktopRect::MakeLTRB(0, 10, 40, 20),
        DesktopRect::MakeLTRB(0, 20, 50, 30),
        DesktopRect::MakeLTRB(10, 30, 50, 40),
        DesktopRect::MakeLTRB(20, 40, 50, 50)}},
      {3,
       {DesktopRect::MakeLTRB(10, 10, 40, 20),
        DesktopRect::MakeLTRB(10, 30, 40, 40),
        DesktopRect::MakeLTRB(10, 20, 40, 30)},
       1,
       {DesktopRect::MakeLTRB(10, 10, 40, 40)}},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i) {
    SCOPED_TRACE(i);

    DesktopRegion r;
    r.AddRects(cases[i].input_rects, cases[i].input_count);
    CompareRegion(r, cases[i].expected_rects, cases[i].expected_count);

    // Try inserting rectangles in reverse order.
    r.Clear();
    for (int j = cases[i].input_count - 1; j >= 0; --j) {
      r.AddRect(cases[i].input_rects[j]);
    }
    CompareRegion(r, cases[i].expected_rects, cases[i].expected_count);
  }
}

TEST(DesktopRegionTest, Equals) {
  struct Region {
    int count;
    DesktopRect rects[4];
    int id;
  } regions[] = {
      // Same region with one of the rectangles 1 pixel wider/taller.
      {2,
       {DesktopRect::MakeLTRB(0, 100, 200, 200),
        DesktopRect::MakeLTRB(310, 110, 320, 120)},
       0},
      {2,
       {DesktopRect::MakeLTRB(0, 100, 201, 200),
        DesktopRect::MakeLTRB(310, 110, 320, 120)},
       1},
      {2,
       {DesktopRect::MakeLTRB(0, 100, 200, 201),
        DesktopRect::MakeLTRB(310, 110, 320, 120)},
       2},

      // Same region with one of the rectangles shifted horizontally and
      // vertically.
      {4,
       {DesktopRect::MakeLTRB(0, 0, 30, 30),
        DesktopRect::MakeLTRB(10, 10, 40, 40),
        DesktopRect::MakeLTRB(20, 20, 50, 50),
        DesktopRect::MakeLTRB(50, 0, 65, 15)},
       3},
      {4,
       {DesktopRect::MakeLTRB(0, 0, 30, 30),
        DesktopRect::MakeLTRB(10, 10, 40, 40),
        DesktopRect::MakeLTRB(20, 20, 50, 50),
        DesktopRect::MakeLTRB(50, 1, 65, 16)},
       4},
      {4,
       {DesktopRect::MakeLTRB(0, 0, 30, 30),
        DesktopRect::MakeLTRB(10, 10, 40, 40),
        DesktopRect::MakeLTRB(20, 20, 50, 50),
        DesktopRect::MakeLTRB(51, 0, 66, 15)},
       5},

      // Same region defined by a different set of rectangles - one of the
      // rectangle is split horizontally into two.
      {3,
       {DesktopRect::MakeLTRB(100, 100, 200, 200),
        DesktopRect::MakeLTRB(50, 50, 150, 150),
        DesktopRect::MakeLTRB(300, 125, 350, 175)},
       6},
      {4,
       {DesktopRect::MakeLTRB(100, 100, 200, 200),
        DesktopRect::MakeLTRB(50, 50, 100, 150),
        DesktopRect::MakeLTRB(100, 50, 150, 150),
        DesktopRect::MakeLTRB(300, 125, 350, 175)},
       6},

      // Rectangle region defined by a set of rectangles that merge into one.
      {3,
       {DesktopRect::MakeLTRB(10, 10, 40, 20),
        DesktopRect::MakeLTRB(10, 30, 40, 40),
        DesktopRect::MakeLTRB(10, 20, 40, 30)},
       7},
      {1, {DesktopRect::MakeLTRB(10, 10, 40, 40)}, 7},
  };
  int kTotalRegions = sizeof(regions) / sizeof(Region);

  for (int i = 0; i < kTotalRegions; ++i) {
    SCOPED_TRACE(i);

    DesktopRegion r1(regions[i].rects, regions[i].count);
    for (int j = 0; j < kTotalRegions; ++j) {
      SCOPED_TRACE(j);

      DesktopRegion r2(regions[j].rects, regions[j].count);
      EXPECT_EQ(regions[i].id == regions[j].id, r1.Equals(r2));
    }
  }
}

TEST(DesktopRegionTest, Translate) {
  struct Case {
    int input_count;
    DesktopRect input_rects[4];
    int dx;
    int dy;
    int expected_count;
    DesktopRect expected_rects[5];
  } cases[] = {
      {3,
       {DesktopRect::MakeLTRB(0, 0, 30, 30),
        DesktopRect::MakeLTRB(10, 10, 40, 40),
        DesktopRect::MakeLTRB(20, 20, 50, 50)},
       3,
       5,
       5,
       {DesktopRect::MakeLTRB(3, 5, 33, 15),
        DesktopRect::MakeLTRB(3, 15, 43, 25),
        DesktopRect::MakeLTRB(3, 25, 53, 35),
        DesktopRect::MakeLTRB(13, 35, 53, 45),
        DesktopRect::MakeLTRB(23, 45, 53, 55)}},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i) {
    SCOPED_TRACE(i);

    DesktopRegion r(cases[i].input_rects, cases[i].input_count);
    r.Translate(cases[i].dx, cases[i].dy);
    CompareRegion(r, cases[i].expected_rects, cases[i].expected_count);
  }
}

TEST(DesktopRegionTest, Intersect) {
  struct Case {
    int input1_count;
    DesktopRect input1_rects[4];
    int input2_count;
    DesktopRect input2_rects[4];
    int expected_count;
    DesktopRect expected_rects[5];
  } cases[] = {
      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(50, 50, 150, 150)},
       1,
       {DesktopRect::MakeLTRB(50, 50, 100, 100)}},

      {1,
       {DesktopRect::MakeLTRB(100, 0, 200, 300)},
       1,
       {DesktopRect::MakeLTRB(0, 100, 300, 200)},
       1,
       {DesktopRect::MakeLTRB(100, 100, 200, 200)}},

      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       2,
       {DesktopRect::MakeLTRB(50, 10, 150, 30),
        DesktopRect::MakeLTRB(50, 30, 160, 50)},
       1,
       {DesktopRect::MakeLTRB(50, 10, 100, 50)}},

      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       2,
       {DesktopRect::MakeLTRB(50, 10, 150, 30),
        DesktopRect::MakeLTRB(50, 30, 90, 50)},
       2,
       {DesktopRect::MakeLTRB(50, 10, 100, 30),
        DesktopRect::MakeLTRB(50, 30, 90, 50)}},
      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(100, 50, 200, 200)},
       0,
       {}},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i) {
    SCOPED_TRACE(i);

    DesktopRegion r1(cases[i].input1_rects, cases[i].input1_count);
    DesktopRegion r2(cases[i].input2_rects, cases[i].input2_count);

    DesktopRegion r;
    r.Intersect(r1, r2);

    CompareRegion(r, cases[i].expected_rects, cases[i].expected_count);
  }
}

TEST(DesktopRegionTest, Subtract) {
  struct Case {
    int input1_count;
    DesktopRect input1_rects[4];
    int input2_count;
    DesktopRect input2_rects[4];
    int expected_count;
    DesktopRect expected_rects[5];
  } cases[] = {
      // Subtract one rect from another.
      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(50, 50, 150, 150)},
       2,
       {DesktopRect::MakeLTRB(0, 0, 100, 50),
        DesktopRect::MakeLTRB(0, 50, 50, 100)}},

      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(-50, -50, 50, 50)},
       2,
       {DesktopRect::MakeLTRB(50, 0, 100, 50),
        DesktopRect::MakeLTRB(0, 50, 100, 100)}},

      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(-50, 50, 50, 150)},
       2,
       {DesktopRect::MakeLTRB(0, 0, 100, 50),
        DesktopRect::MakeLTRB(50, 50, 100, 100)}},

      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(50, 50, 150, 70)},
       3,
       {DesktopRect::MakeLTRB(0, 0, 100, 50),
        DesktopRect::MakeLTRB(0, 50, 50, 70),
        DesktopRect::MakeLTRB(0, 70, 100, 100)}},

      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(50, 50, 70, 70)},
       4,
       {DesktopRect::MakeLTRB(0, 0, 100, 50),
        DesktopRect::MakeLTRB(0, 50, 50, 70),
        DesktopRect::MakeLTRB(70, 50, 100, 70),
        DesktopRect::MakeLTRB(0, 70, 100, 100)}},

      // Empty result.
      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       0,
       {}},

      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(-10, -10, 110, 110)},
       0,
       {}},

      {2,
       {DesktopRect::MakeLTRB(0, 0, 100, 100),
        DesktopRect::MakeLTRB(50, 50, 150, 150)},
       2,
       {DesktopRect::MakeLTRB(0, 0, 100, 100),
        DesktopRect::MakeLTRB(50, 50, 150, 150)},
       0,
       {}},

      // One rect out of disjoint set.
      {3,
       {DesktopRect::MakeLTRB(0, 0, 10, 10),
        DesktopRect::MakeLTRB(20, 20, 30, 30),
        DesktopRect::MakeLTRB(40, 0, 50, 10)},
       1,
       {DesktopRect::MakeLTRB(20, 20, 30, 30)},
       2,
       {DesktopRect::MakeLTRB(0, 0, 10, 10),
        DesktopRect::MakeLTRB(40, 0, 50, 10)}},

      // Row merging.
      {3,
       {DesktopRect::MakeLTRB(0, 0, 100, 50),
        DesktopRect::MakeLTRB(0, 50, 150, 70),
        DesktopRect::MakeLTRB(0, 70, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(100, 50, 150, 70)},
       1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)}},

      // No-op subtraction.
      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(100, 0, 200, 100)},
       1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)}},

      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(-100, 0, 0, 100)},
       1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)}},

      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(0, 100, 0, 200)},
       1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)}},

      {1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)},
       1,
       {DesktopRect::MakeLTRB(0, -100, 100, 0)},
       1,
       {DesktopRect::MakeLTRB(0, 0, 100, 100)}},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i) {
    SCOPED_TRACE(i);

    DesktopRegion r1(cases[i].input1_rects, cases[i].input1_count);
    DesktopRegion r2(cases[i].input2_rects, cases[i].input2_count);

    r1.Subtract(r2);

    CompareRegion(r1, cases[i].expected_rects, cases[i].expected_count);
  }
}

// Verify that DesktopRegion::SubtractRows() works correctly by creating a row
// of not overlapping rectangles and subtracting a set of rectangle. Result
// is verified by building a map of the region in an array and comparing it with
// the expected values.
TEST(DesktopRegionTest, SubtractRectOnSameRow) {
  const int kMapWidth = 50;

  struct SpanSet {
    int count;
    struct Range {
      int start;
      int end;
    } spans[3];
  } span_sets[] = {
      {1, {{0, 3}}},
      {1, {{0, 5}}},
      {1, {{0, 7}}},
      {1, {{0, 12}}},
      {2, {{0, 3}, {4, 5}, {6, 16}}},
  };

  DesktopRegion base_region;
  bool base_map[kMapWidth] = {
      false,
  };

  base_region.AddRect(DesktopRect::MakeXYWH(5, 0, 5, 1));
  std::fill_n(base_map + 5, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(15, 0, 5, 1));
  std::fill_n(base_map + 15, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(25, 0, 5, 1));
  std::fill_n(base_map + 25, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(35, 0, 5, 1));
  std::fill_n(base_map + 35, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(45, 0, 5, 1));
  std::fill_n(base_map + 45, 5, true);

  for (size_t i = 0; i < sizeof(span_sets) / sizeof(span_sets[0]); i++) {
    SCOPED_TRACE(i);
    SpanSet& span_set = span_sets[i];
    int span_set_end = span_set.spans[span_set.count - 1].end;
    for (int x = 0; x < kMapWidth - span_set_end; ++x) {
      SCOPED_TRACE(x);

      DesktopRegion r = base_region;

      bool expected_map[kMapWidth];
      std::copy(base_map, base_map + kMapWidth, expected_map);

      DesktopRegion region2;
      for (int span = 0; span < span_set.count; span++) {
        std::fill_n(x + expected_map + span_set.spans[span].start,
                    span_set.spans[span].end - span_set.spans[span].start,
                    false);
        region2.AddRect(DesktopRect::MakeLTRB(x + span_set.spans[span].start, 0,
                                              x + span_set.spans[span].end, 1));
      }
      r.Subtract(region2);

      bool map[kMapWidth] = {
          false,
      };

      int pos = -1;
      for (DesktopRegion::Iterator it(r); !it.IsAtEnd(); it.Advance()) {
        EXPECT_GT(it.rect().left(), pos);
        pos = it.rect().right();
        std::fill_n(map + it.rect().left(), it.rect().width(), true);
      }

      EXPECT_TRUE(std::equal(map, map + kMapWidth, expected_map));
    }
  }
}

// Verify that DesktopRegion::Subtract() works correctly by creating a column of
// not overlapping rectangles and subtracting a set of rectangle on the same
// column. Result is verified by building a map of the region in an array and
// comparing it with the expected values.
TEST(DesktopRegionTest, SubtractRectOnSameCol) {
  const int kMapHeight = 50;

  struct SpanSet {
    int count;
    struct Range {
      int start;
      int end;
    } spans[3];
  } span_sets[] = {
      {1, {{0, 3}}},
      {1, {{0, 5}}},
      {1, {{0, 7}}},
      {1, {{0, 12}}},
      {2, {{0, 3}, {4, 5}, {6, 16}}},
  };

  DesktopRegion base_region;
  bool base_map[kMapHeight] = {
      false,
  };

  base_region.AddRect(DesktopRect::MakeXYWH(0, 5, 1, 5));
  std::fill_n(base_map + 5, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(0, 15, 1, 5));
  std::fill_n(base_map + 15, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(0, 25, 1, 5));
  std::fill_n(base_map + 25, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(0, 35, 1, 5));
  std::fill_n(base_map + 35, 5, true);
  base_region.AddRect(DesktopRect::MakeXYWH(0, 45, 1, 5));
  std::fill_n(base_map + 45, 5, true);

  for (size_t i = 0; i < sizeof(span_sets) / sizeof(span_sets[0]); i++) {
    SCOPED_TRACE(i);
    SpanSet& span_set = span_sets[i];
    int span_set_end = span_set.spans[span_set.count - 1].end;
    for (int y = 0; y < kMapHeight - span_set_end; ++y) {
      SCOPED_TRACE(y);

      DesktopRegion r = base_region;

      bool expected_map[kMapHeight];
      std::copy(base_map, base_map + kMapHeight, expected_map);

      DesktopRegion region2;
      for (int span = 0; span < span_set.count; span++) {
        std::fill_n(y + expected_map + span_set.spans[span].start,
                    span_set.spans[span].end - span_set.spans[span].start,
                    false);
        region2.AddRect(DesktopRect::MakeLTRB(0, y + span_set.spans[span].start,
                                              1, y + span_set.spans[span].end));
      }
      r.Subtract(region2);

      bool map[kMapHeight] = {
          false,
      };

      int pos = -1;
      for (DesktopRegion::Iterator it(r); !it.IsAtEnd(); it.Advance()) {
        EXPECT_GT(it.rect().top(), pos);
        pos = it.rect().bottom();
        std::fill_n(map + it.rect().top(), it.rect().height(), true);
      }

      for (int j = 0; j < kMapHeight; j++) {
        EXPECT_EQ(expected_map[j], map[j]) << "j = " << j;
      }
    }
  }
}

TEST(DesktopRegionTest, DISABLED_Performance) {
  for (int c = 0; c < 1000; ++c) {
    DesktopRegion r;
    for (int i = 0; i < 10; ++i) {
      r.AddRect(
          DesktopRect::MakeXYWH(RadmonInt(1000), RadmonInt(1000), 200, 200));
    }

    for (int i = 0; i < 1000; ++i) {
      r.AddRect(DesktopRect::MakeXYWH(RadmonInt(1000), RadmonInt(1000),
                                      5 + RadmonInt(10) * 5,
                                      5 + RadmonInt(10) * 5));
    }

    // Iterate over the rectangles.
    for (DesktopRegion::Iterator it(r); !it.IsAtEnd(); it.Advance()) {
    }
  }
}

}  // namespace webrtc
