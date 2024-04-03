/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm/gm.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkImageFilters.h"
#include "tools/ToolUtils.h"

#include <utility>

static sk_sp<SkImage> make_image(SkCanvas* rootCanvas) {
    SkImageInfo info = SkImageInfo::MakeN32Premul(100, 100);
    auto        surface(ToolUtils::makeSurface(rootCanvas, info));

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorRED);
    surface->getCanvas()->drawCircle(50, 50, 50, paint);
    return surface->makeImageSnapshot();
}

static void show_image(SkCanvas* canvas, SkImage* image, sk_sp<SkImageFilter> filter) {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    SkRect r = SkRect::MakeIWH(image->width(), image->height()).makeOutset(SK_ScalarHalf,
                                                                           SK_ScalarHalf);
    canvas->drawRect(r, paint);

    paint.setStyle(SkPaint::kFill_Style);
    paint.setImageFilter(filter);
    canvas->drawImage(image, 0, 0, SkSamplingOptions(), &paint);
}

typedef sk_sp<SkImageFilter> (*ImageFilterFactory)();

// Show the effect of localmatriximagefilter with various matrices, on various filters
DEF_SIMPLE_GM(localmatriximagefilter, canvas, 640, 640) {
    sk_sp<SkImage> image0(make_image(canvas));

    const ImageFilterFactory factories[] = {
        []{ return SkImageFilters::Blur(8, 8, nullptr); },
        []{ return SkImageFilters::Dilate(8, 8, nullptr); },
        []{ return SkImageFilters::Erode(8, 8, nullptr); },
        []{ return SkImageFilters::Offset(8, 8, nullptr); },
    };

    const SkMatrix matrices[] = {
        SkMatrix::Scale(SK_ScalarHalf, SK_ScalarHalf),
        SkMatrix::Scale(2, 2),
        SkMatrix::Translate(10, 10)
    };

    const SkScalar spacer = image0->width() * 3.0f / 2;

    canvas->translate(40, 40);
    for (auto&& factory : factories) {
        sk_sp<SkImageFilter> filter(factory());

        canvas->save();
        show_image(canvas, image0.get(), filter);
        for (const auto& matrix : matrices) {
            sk_sp<SkImageFilter> localFilter(filter->makeWithLocalMatrix(matrix));
            canvas->translate(spacer, 0);
            show_image(canvas, image0.get(), std::move(localFilter));
        }
        canvas->restore();
        canvas->translate(0, spacer);
    }
}
