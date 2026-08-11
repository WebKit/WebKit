/*
 * Copyright 2018 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "src/core/SkRegionPriv.h"

bool FuzzRegionDeserialize(const uint8_t* data, size_t size) {
    SkRegion region;
    if (!region.readFromMemory(data, size)) {
        return false;
    }
    SkDEBUGCODE(SkRegionPriv::Validate(region));
    region.computeRegionComplexity();
    region.isComplex();
    region.contains(1,1);

    SkRegion::Iterator iter(region);
    while (!iter.done()) {
        iter.next();
    }

    SkRegion::Cliperator clip(region, {10, 20, 30, 40});
    while (!clip.done()) {
        clip.next();
    }

    SkRegion::Spanerator spn(region, 3, 10, 40);
    int left, right;
    while(spn.next(&left, &right)) {}
    return true;
}

#if defined(SK_BUILD_FOR_LIBFUZZER)
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 512) {
        return 0;
    }
    FuzzRegionDeserialize(data, size);
    return 0;
}
#endif
