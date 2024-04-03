// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <math.h>

static const double epsilon = 1e-12;

static avifBool avifXyToXYZ(const float xy[2], double XYZ[3])
{
    if (fabsf(xy[1]) < epsilon) {
        return AVIF_FALSE;
    }

    const double factor = 1.0 / xy[1];
    XYZ[0] = xy[0] * factor;
    XYZ[1] = 1;
    XYZ[2] = (1 - xy[0] - xy[1]) * factor;

    return AVIF_TRUE;
}

// Computes I = M^-1. Returns false if M seems to be singular.
static avifBool avifMatInv(double M[3][3], double I[3][3])
{
    double det = M[0][0] * (M[1][1] * M[2][2] - M[2][1] * M[1][2]) - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
                 M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
    if (fabs(det) < epsilon) {
        return AVIF_FALSE;
    }
    det = 1.0 / det;

    I[0][0] = (M[1][1] * M[2][2] - M[2][1] * M[1][2]) * det;
    I[0][1] = (M[0][2] * M[2][1] - M[0][1] * M[2][2]) * det;
    I[0][2] = (M[0][1] * M[1][2] - M[0][2] * M[1][1]) * det;
    I[1][0] = (M[1][2] * M[2][0] - M[1][0] * M[2][2]) * det;
    I[1][1] = (M[0][0] * M[2][2] - M[0][2] * M[2][0]) * det;
    I[1][2] = (M[1][0] * M[0][2] - M[0][0] * M[1][2]) * det;
    I[2][0] = (M[1][0] * M[2][1] - M[2][0] * M[1][1]) * det;
    I[2][1] = (M[2][0] * M[0][1] - M[0][0] * M[2][1]) * det;
    I[2][2] = (M[0][0] * M[1][1] - M[1][0] * M[0][1]) * det;

    return AVIF_TRUE;
}

// Computes C = A*B
static void avifMatMul(double A[3][3], double B[3][3], double C[3][3])
{
    C[0][0] = A[0][0] * B[0][0] + A[0][1] * B[1][0] + A[0][2] * B[2][0];
    C[0][1] = A[0][0] * B[0][1] + A[0][1] * B[1][1] + A[0][2] * B[2][1];
    C[0][2] = A[0][0] * B[0][2] + A[0][1] * B[1][2] + A[0][2] * B[2][2];
    C[1][0] = A[1][0] * B[0][0] + A[1][1] * B[1][0] + A[1][2] * B[2][0];
    C[1][1] = A[1][0] * B[0][1] + A[1][1] * B[1][1] + A[1][2] * B[2][1];
    C[1][2] = A[1][0] * B[0][2] + A[1][1] * B[1][2] + A[1][2] * B[2][2];
    C[2][0] = A[2][0] * B[0][0] + A[2][1] * B[1][0] + A[2][2] * B[2][0];
    C[2][1] = A[2][0] * B[0][1] + A[2][1] * B[1][1] + A[2][2] * B[2][1];
    C[2][2] = A[2][0] * B[0][2] + A[2][1] * B[1][2] + A[2][2] * B[2][2];
}

// Set M to have values of d on the leading diagonal, and zero elsewhere.
static void avifMatDiag(const double d[3], double M[3][3])
{
    M[0][0] = d[0];
    M[0][1] = 0;
    M[0][2] = 0;
    M[1][0] = 0;
    M[1][1] = d[1];
    M[1][2] = 0;
    M[2][0] = 0;
    M[2][1] = 0;
    M[2][2] = d[2];
}

// Computes y = M.x
static void avifVecMul(double M[3][3], const double x[3], double y[3])
{
    y[0] = M[0][0] * x[0] + M[0][1] * x[1] + M[0][2] * x[2];
    y[1] = M[1][0] * x[0] + M[1][1] * x[1] + M[1][2] * x[2];
    y[2] = M[2][0] * x[0] + M[2][1] * x[1] + M[2][2] * x[2];
}

// Bradford chromatic adaptation matrix
// from https://www.researchgate.net/publication/253799640_A_uniform_colour_space_based_upon_CIECAM97s
static double avifBradford[3][3] = {
    { 0.8951, 0.2664, -0.1614 },
    { -0.7502, 1.7135, 0.0367 },
    { 0.0389, -0.0685, 1.0296 },
};

// LMS values for D50 whitepoint
static const double avifLmsD50[3] = { 0.996284, 1.02043, 0.818644 };

avifBool avifColorPrimariesComputeRGBToXYZD50Matrix(avifColorPrimaries colorPrimaries, double coeffs[3][3])
{
    float primaries[8];
    avifColorPrimariesGetValues(colorPrimaries, primaries);

    double whitePointXYZ[3];
    AVIF_CHECK(avifXyToXYZ(&primaries[6], whitePointXYZ));

    double rgbPrimaries[3][3] = {
        { primaries[0], primaries[2], primaries[4] },
        { primaries[1], primaries[3], primaries[5] },
        { 1.0 - primaries[0] - primaries[1], 1.0 - primaries[2] - primaries[3], 1.0 - primaries[4] - primaries[5] }
    };

    double rgbPrimariesInv[3][3];
    AVIF_CHECK(avifMatInv(rgbPrimaries, rgbPrimariesInv));

    double rgbCoefficients[3];
    avifVecMul(rgbPrimariesInv, whitePointXYZ, rgbCoefficients);

    double rgbCoefficientsMat[3][3];
    avifMatDiag(rgbCoefficients, rgbCoefficientsMat);

    double rgbXYZ[3][3];
    avifMatMul(rgbPrimaries, rgbCoefficientsMat, rgbXYZ);

    // ICC stores primaries XYZ under PCS.
    // Adapt using linear bradford transform
    // from https://onlinelibrary.wiley.com/doi/pdf/10.1002/9781119021780.app3
    double lms[3];
    avifVecMul(avifBradford, whitePointXYZ, lms);
    for (int i = 0; i < 3; ++i) {
        if (fabs(lms[i]) < epsilon) {
            return AVIF_FALSE;
        }
        lms[i] = avifLmsD50[i] / lms[i];
    }

    double adaptation[3][3];
    avifMatDiag(lms, adaptation);

    double tmp[3][3];
    avifMatMul(adaptation, avifBradford, tmp);

    double bradfordInv[3][3];
    if (!avifMatInv(avifBradford, bradfordInv)) {
        return AVIF_FALSE;
    }
    avifMatMul(bradfordInv, tmp, adaptation);

    avifMatMul(adaptation, rgbXYZ, coeffs);

    return AVIF_TRUE;
}

avifBool avifColorPrimariesComputeXYZD50ToRGBMatrix(avifColorPrimaries colorPrimaries, double coeffs[3][3])
{
    double rgbToXyz[3][3];
    AVIF_CHECK(avifColorPrimariesComputeRGBToXYZD50Matrix(colorPrimaries, rgbToXyz));
    AVIF_CHECK(avifMatInv(rgbToXyz, coeffs));
    return AVIF_TRUE;
}

avifBool avifColorPrimariesComputeRGBToRGBMatrix(avifColorPrimaries srcColorPrimaries,
                                                 avifColorPrimaries dstColorPrimaries,
                                                 double coeffs[3][3])
{
    // Note: no special casing for srcColorPrimaries == dstColorPrimaries to allow
    // testing that the computation actually produces the identity matrix.
    double srcRGBToXYZ[3][3];
    AVIF_CHECK(avifColorPrimariesComputeRGBToXYZD50Matrix(srcColorPrimaries, srcRGBToXYZ));
    double xyzToDstRGB[3][3];
    AVIF_CHECK(avifColorPrimariesComputeXYZD50ToRGBMatrix(dstColorPrimaries, xyzToDstRGB));
    // coeffs = xyzToDstRGB * srcRGBToXYZ
    // i.e. srcRGB -> XYZ -> dstRGB
    avifMatMul(xyzToDstRGB, srcRGBToXYZ, coeffs);
    return AVIF_TRUE;
}

// Converts a linear RGBA pixel to a different color space. This function actually works for gamma encoded
// RGB as well but linear gives better results. Also, for gamma encoded values, it would be
// better to clamp the output to [0, 1]. Linear values don't need clamping because values
// > 1.0 are valid for HDR transfer curves, and the gamma compression function will do the
// clamping as necessary.
void avifLinearRGBConvertColorSpace(float rgb[4], double coeffs[3][3])
{
    const double rgbDouble[3] = { rgb[0], rgb[1], rgb[2] };
    double converted[3];
    avifVecMul(coeffs, rgbDouble, converted);
    rgb[0] = (float)converted[0];
    rgb[1] = (float)converted[1];
    rgb[2] = (float)converted[2];
}
