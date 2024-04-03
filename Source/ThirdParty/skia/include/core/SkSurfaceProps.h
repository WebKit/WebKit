/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSurfaceProps_DEFINED
#define SkSurfaceProps_DEFINED

#include "include/core/SkScalar.h"
#include "include/core/SkTypes.h"
#include "include/private/base/SkTo.h"

/**
 *  Description of how the LCD strips are arranged for each pixel. If this is unknown, or the
 *  pixels are meant to be "portable" and/or transformed before showing (e.g. rotated, scaled)
 *  then use kUnknown_SkPixelGeometry.
 */
enum SkPixelGeometry {
    kUnknown_SkPixelGeometry,
    kRGB_H_SkPixelGeometry,
    kBGR_H_SkPixelGeometry,
    kRGB_V_SkPixelGeometry,
    kBGR_V_SkPixelGeometry,
};

// Returns true iff geo is a known geometry and is RGB.
static inline bool SkPixelGeometryIsRGB(SkPixelGeometry geo) {
    return kRGB_H_SkPixelGeometry == geo || kRGB_V_SkPixelGeometry == geo;
}

// Returns true iff geo is a known geometry and is BGR.
static inline bool SkPixelGeometryIsBGR(SkPixelGeometry geo) {
    return kBGR_H_SkPixelGeometry == geo || kBGR_V_SkPixelGeometry == geo;
}

// Returns true iff geo is a known geometry and is horizontal.
static inline bool SkPixelGeometryIsH(SkPixelGeometry geo) {
    return kRGB_H_SkPixelGeometry == geo || kBGR_H_SkPixelGeometry == geo;
}

// Returns true iff geo is a known geometry and is vertical.
static inline bool SkPixelGeometryIsV(SkPixelGeometry geo) {
    return kRGB_V_SkPixelGeometry == geo || kBGR_V_SkPixelGeometry == geo;
}

/**
 *  Describes properties and constraints of a given SkSurface. The rendering engine can parse these
 *  during drawing, and can sometimes optimize its performance (e.g. disabling an expensive
 *  feature).
 */
class SK_API SkSurfaceProps {
public:
    enum Flags {
        kDefault_Flag = 0,
        kUseDeviceIndependentFonts_Flag = 1 << 0,
        // Use internal MSAA to render to non-MSAA GPU surfaces.
        kDynamicMSAA_Flag = 1 << 1,
        // If set, all rendering will have dithering enabled
        // Currently this only impacts GPU backends
        kAlwaysDither_Flag = 1 << 2,
    };

    /** No flags, unknown pixel geometry, platform-default contrast/gamma. */
    SkSurfaceProps();
    /** TODO(kschmi): Remove this constructor and replace with the one below. **/
    SkSurfaceProps(uint32_t flags, SkPixelGeometry);
    /** Specified pixel geometry, text contrast, and gamma **/
    SkSurfaceProps(uint32_t flags, SkPixelGeometry, SkScalar textContrast, SkScalar textGamma);

    SkSurfaceProps(const SkSurfaceProps&) = default;
    SkSurfaceProps& operator=(const SkSurfaceProps&) = default;

    SkSurfaceProps cloneWithPixelGeometry(SkPixelGeometry newPixelGeometry) const {
        return SkSurfaceProps(fFlags, newPixelGeometry, fTextContrast, fTextGamma);
    }

    static constexpr SkScalar kMaxContrastInclusive = 1;
    static constexpr SkScalar kMinContrastInclusive = 0;
    static constexpr SkScalar kMaxGammaExclusive = 4;
    static constexpr SkScalar kMinGammaInclusive = 0;

    uint32_t flags() const { return fFlags; }
    SkPixelGeometry pixelGeometry() const { return fPixelGeometry; }
    SkScalar textContrast() const { return fTextContrast; }
    SkScalar textGamma() const { return fTextGamma; }

    bool isUseDeviceIndependentFonts() const {
        return SkToBool(fFlags & kUseDeviceIndependentFonts_Flag);
    }

    bool isAlwaysDither() const {
        return SkToBool(fFlags & kAlwaysDither_Flag);
    }

    bool operator==(const SkSurfaceProps& that) const {
        return fFlags == that.fFlags && fPixelGeometry == that.fPixelGeometry &&
        fTextContrast == that.fTextContrast && fTextGamma == that.fTextGamma;
    }

    bool operator!=(const SkSurfaceProps& that) const {
        return !(*this == that);
    }

private:
    uint32_t        fFlags;
    SkPixelGeometry fPixelGeometry;

    // This gamma value is specifically about blending of mask coverage.
    // The surface also has a color space, but that applies to the colors.
    SkScalar fTextContrast;
    SkScalar fTextGamma;
};

#endif
