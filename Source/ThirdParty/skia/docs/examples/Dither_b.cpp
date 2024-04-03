// Copyright 2019 Google LLC.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
#include "tools/fiddle/examples.h"
// HASH=76d4d4a7931a48495e4d5f54e073be53
REG_FIDDLE(Dither_b, 256, 256, false, 0) {
void draw(SkCanvas* canvas) {
    canvas->clear(0);
    SkBitmap bm32;
    bm32.allocPixels(SkImageInfo::Make(20, 10, kN32_SkColorType, kPremul_SkAlphaType));
    SkCanvas c32(bm32);
    SkPoint points[] = {{0, 0}, {20, 0}};
    SkColor colors[] = {0xFF334455, 0xFF662211 };
    SkPaint paint;
    paint.setShader(SkGradientShader::MakeLinear(
                     points, colors, nullptr, std::size(colors),
                     SkTileMode::kClamp));
    paint.setDither(true);
    auto img = bm32.asImage();
    c32.drawPaint(paint);
    canvas->scale(12, 12);
    canvas->drawImage(img, 0, 0);
    paint.setBlendMode(SkBlendMode::kPlus);
    SkSamplingOptions sampling;
    canvas->drawImage(img, 0, 11, sampling, &paint);
    canvas->drawImage(img, 0, 11, sampling, &paint);
    canvas->drawImage(img, 0, 11, sampling, &paint);
}
}  // END FIDDLE
