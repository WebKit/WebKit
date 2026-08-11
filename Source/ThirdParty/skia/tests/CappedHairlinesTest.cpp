/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSurface.h"
#include "tests/Test.h"

#include <cmath>

static sk_sp<SkSurface> create_white_raster_surface() {
    const SkImageInfo info = SkImageInfo::MakeN32Premul(70, 70);
    sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
    if (surface) {
        surface->getCanvas()->clear(SK_ColorWHITE);
    }
    return surface;
}

static SkPaint create_aa_hairline_paint() {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(0);
    paint.setColor(SK_ColorBLACK);
    paint.setAntiAlias(true);
    return paint;
}

static bool color_is_lteq(SkColor a, SkColor b) {
    return SkColorGetR(a) <= SkColorGetR(b) && SkColorGetG(a) <= SkColorGetG(b) &&
           SkColorGetB(a) <= SkColorGetB(b) && SkColorGetA(a) <= SkColorGetA(b);
}

static void assert_color_in_range(skiatest::Reporter* reporter,
                                  const SkPixmap& pixmap,
                                  const std::string& testName,
                                  SkIPoint pt,
                                  SkColor expected_min,
                                  SkColor expected_max) {
    skiatest::ReporterContext subtest(reporter, testName);

    SkColor actual = pixmap.getColor(pt.x(), pt.y());

    REPORTER_ASSERT(reporter,
                    color_is_lteq(expected_min, actual) && color_is_lteq(actual, expected_max),
                    "%s: actual color (%x) is outside the range of expected colors (between (%x) "
                    "and (%x)) for pixel at (%d, %d)",
                    testName.c_str(),
                    actual,
                    expected_min,
                    expected_max,
                    pt.x(),
                    pt.y());
}

static void draw_aa_hairline_path_with_caps_no_offset(skiatest::Reporter* reporter,
                                                      SkPaint::Cap cap) {
    sk_sp<SkSurface> surface = create_white_raster_surface();
    SkASSERT(surface);

    SkCanvas* canvas = surface->getCanvas();

    SkPaint paint = create_aa_hairline_paint();
    paint.setStrokeCap(cap);

    SkPath path = SkPathBuilder()
                          .moveTo(5, 5)
                          .lineTo(5, 10)
                          .lineTo(10, 10)
                          .lineTo(10, 5)
                          .close()
                          .detach();

    canvas->drawPath(path, paint);

    SkPixmap pixmap;
    SkASSERT_RELEASE(surface->peekPixels(&pixmap));

    auto assert_color =
            [&](const std::string& name, SkIPoint pt, SkColor expected_min, SkColor expected_max) {
                assert_color_in_range(reporter, pixmap, name, pt, expected_min, expected_max);
            };

    auto assert_exact_color = [&](const std::string& name, SkIPoint pt, SkColor exact_color) {
        assert_color_in_range(reporter, pixmap, name, pt, exact_color, exact_color);
    };

    // these asserts are to verify that the contour drawn is a square
    // with double pixel wide gray lines. The inside and outside of the square
    // should be white. This should be the expected behavior across all caps

    assert_exact_color("left of west line", {3, 7}, SK_ColorWHITE);
    assert_color("on west line", {4, 7}, SK_ColorBLACK, SK_ColorGRAY);
    assert_color("on west line", {5, 7}, SK_ColorBLACK, SK_ColorGRAY);
    assert_exact_color("right of west line", {6, 7}, SK_ColorWHITE);

    assert_exact_color("above south line", {7, 8}, SK_ColorWHITE);
    assert_color("on south line", {7, 9}, SK_ColorBLACK, SK_ColorGRAY);
    assert_color("on south line", {7, 10}, SK_ColorBLACK, SK_ColorGRAY);
    assert_exact_color("below south line", {7, 11}, SK_ColorWHITE);

    assert_exact_color("left of east line", {8, 7}, SK_ColorWHITE);
    assert_color("on east line", {9, 7}, SK_ColorBLACK, SK_ColorGRAY);
    assert_color("on east line", {10, 7}, SK_ColorBLACK, SK_ColorGRAY);
    assert_exact_color("right of east line", {11, 7}, SK_ColorWHITE);

    assert_exact_color("above north line", {7, 3}, SK_ColorWHITE);
    assert_color("on north line", {7, 4}, SK_ColorBLACK, SK_ColorGRAY);
    assert_color("on north line", {7, 5}, SK_ColorBLACK, SK_ColorGRAY);
    assert_exact_color("below north line", {7, 6}, SK_ColorWHITE);
}

DEF_TEST(DrawAntialiasHairline_SquareCappedNoPixelOffsetClosedContour_DoublePixelWideOutput,
         reporter) {
    draw_aa_hairline_path_with_caps_no_offset(reporter, SkPaint::kSquare_Cap);
}

DEF_TEST(DrawAntialiasHairline_RoundCappedNoPixelOffsetClosedContour_DoublePixelWideOutput,
         reporter) {
    draw_aa_hairline_path_with_caps_no_offset(reporter, SkPaint::kRound_Cap);
}

DEF_TEST(DrawAntialiasHairline_ButtCappedNoPixelOffsetClosedContour_DoublePixelWideOutput,
         reporter) {
    draw_aa_hairline_path_with_caps_no_offset(reporter, SkPaint::kButt_Cap);
}

static void draw_aa_hairline_path_with_caps_and_offset(skiatest::Reporter* reporter,
                                                       SkPaint::Cap cap) {
    sk_sp<SkSurface> surface = create_white_raster_surface();
    SkASSERT(surface);

    SkCanvas* canvas = surface->getCanvas();

    SkPaint paint = create_aa_hairline_paint();
    paint.setStrokeCap(cap);

    SkPath path = SkPathBuilder()
                          .moveTo(5, 5)
                          .lineTo(5, 10)
                          .lineTo(10, 10)
                          .lineTo(10, 5)
                          .close()
                          .detach()
                          .makeOffset(0.5, 0.5);

    canvas->drawPath(path, paint);

    SkPixmap pixmap;
    SkASSERT_RELEASE(surface->peekPixels(&pixmap));

    auto assert_exact_color = [&](const std::string& name, SkIPoint pt, SkColor exact_color) {
        assert_color_in_range(reporter, pixmap, name, pt, exact_color, exact_color);
    };

    // these asserts are to verify that the contour drawn is a square
    // with single pixel wide black lines. The inside and outside of the square
    // should be white. This should be the expected behavior across all caps

    assert_exact_color("left of west line", {4, 7}, SK_ColorWHITE);
    assert_exact_color("on west line", {5, 7}, SK_ColorBLACK);
    assert_exact_color("right of west line", {6, 7}, SK_ColorWHITE);

    assert_exact_color("above south line", {7, 9}, SK_ColorWHITE);
    assert_exact_color("on south line", {7, 10}, SK_ColorBLACK);
    assert_exact_color("below south line", {7, 11}, SK_ColorWHITE);

    assert_exact_color("left of east line", {9, 7}, SK_ColorWHITE);
    assert_exact_color("on east line", {10, 7}, SK_ColorBLACK);
    assert_exact_color("right of east line", {11, 7}, SK_ColorWHITE);

    assert_exact_color("above north line", {7, 4}, SK_ColorWHITE);
    assert_exact_color("on north line", {7, 5}, SK_ColorBLACK);
    assert_exact_color("below north line", {7, 6}, SK_ColorWHITE);
}

DEF_TEST(DrawAntialiasHairline_SquareCappedHalfPixelOffsetClosedContour_SinglePixelWideOutput,
         reporter) {
    draw_aa_hairline_path_with_caps_and_offset(reporter, SkPaint::kSquare_Cap);
}

DEF_TEST(DrawAntialiasHairline_RoundCappedHalfPixelOffsetClosedContour_SinglePixelWideOutput,
         reporter) {
    draw_aa_hairline_path_with_caps_and_offset(reporter, SkPaint::kRound_Cap);
}

DEF_TEST(DrawAntialiasHairline_ButtCappedHalfPixelOffsetClosedContour_SinglePixelWideOutput,
         reporter) {
    draw_aa_hairline_path_with_caps_and_offset(reporter, SkPaint::kButt_Cap);
}
