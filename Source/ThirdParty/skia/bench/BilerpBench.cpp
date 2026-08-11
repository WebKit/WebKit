/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bench/Benchmark.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "tools/ToolUtils.h"

class BilerpBench : public Benchmark {
public:
    BilerpBench(float scale) : fScale(scale) {
        fName.printf("bilerp_4k_scale_%0.2f", scale);
    }

protected:
    const char* onGetName() override {
        return fName.c_str();
    }

    void onDelayedSetup() override {
        // Create a 4K UHD source image with a checkerboard pattern
        fImage = ToolUtils::create_checkerboard_image(3840, 2160, SK_ColorGRAY, SK_ColorDKGRAY, 16);

        // Target rect for drawing
        fDst = SkRect::MakeWH(3840 * fScale, 2160 * fScale);
    }

    void onDraw(int loops, SkCanvas* canvas) override {
        SkPaint paint;
        SkSamplingOptions sampling(SkFilterMode::kLinear, SkMipmapMode::kNone);

        for (int i = 0; i < loops; i++) {
            canvas->drawImageRect(fImage, fDst, sampling, &paint);
        }
    }

private:
    SkString fName;
    sk_sp<SkImage> fImage;
    SkRect fDst;
    float fScale;

    using INHERITED = Benchmark;
};

DEF_BENCH( return new BilerpBench(1.00f); )
DEF_BENCH( return new BilerpBench(0.99f); )
DEF_BENCH( return new BilerpBench(0.75f); )
DEF_BENCH( return new BilerpBench(0.50f); )
DEF_BENCH( return new BilerpBench(0.33f); )
