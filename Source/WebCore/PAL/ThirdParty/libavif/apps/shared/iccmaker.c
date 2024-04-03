// Copyright 2023 Yuan Tong. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "iccmaker.h"

#include <math.h>
#include <string.h>

// ICCv2 profile specification: https://www.color.org/icc32.pdf

/**
 * Color Profile Structure
 *
 * Header:
 *  size         = 376 bytes (*1)
 *  CMM          = 'lcms' (*2)
 *  Version      = 2.2.0
 *  Device Class = Display
 *  Color Space  = RGB
 *  Conn. Space  = XYZ
 *  Date, Time   = 1 Jan 2000, 0:00:00
 *  Platform     = Microsoft
 *  Flags        = Not Embedded Profile, Use anywhere
 *  Dev. Mnfctr. = 0x0
 *  Dev. Model   = 0x0
 *  Dev. Attrbts = Reflective, Glossy, Positive, Color
 *  Rndrng Intnt = Perceptual
 *  Illuminant   = 0.96420288, 1.00000000, 0.82490540    [Lab 100.000000, 0.000000, 0.000000]
 *  Creator      = 'avif'
 *
 * Profile Tags:
 *                    Tag    ID      Offset         Size                 Value
 *                   ----  ------    ------         ----                 -----
 *  profileDescriptionTag  'desc'       240           95                  avif
 *     mediaWhitePointTag  'wtpt'       268 (*3)      20        (to be filled)
 *         redColorantTag  'rXYZ'       288           20        (to be filled)
 *       greenColorantTag  'gXYZ'       308           20        (to be filled)
 *        blueColorantTag  'bXYZ'       328           20        (to be filled)
 *              redTRCTag  'rTRC'       348 (*4)      16        (to be filled)
 *            greenTRCTag  'gTRC'       348           16        (to be filled)
 *             blueTRCTag  'bTRC'       348           16        (to be filled)
 *           copyrightTag  'cprt'       364           12                   CC0
 *
 * (*1): The template data is padded to 448 bytes according to MD5 specification, so that computation can be applied
 *       directly on it. The actual ICC profile data is the first 376 bytes.
 * (*2): 6.1.2 CMM Type: The signatures must be registered in order to avoid conflicts.
 *       The registry can be found at https://www.color.org/signatures2.xalter (Private and ICC tag and CMM registry)
 *       Therefore we are using the signature of Little CMS.
 * (*3): The profileDescriptionTag requires 95 bytes of data, but with some trick, the content of the last 67 bytes
 *       can be anything. Therefore we are placing the following tags in this region to reduce profile size.
 * (*4): The transfer characteristic (gamma) of the 3 channels are the same, so the data can be shared.
 */

static const uint8_t iccColorTemplate[448] = {
    0x00, 0x00, 0x01, 0x78, 0x6c, 0x63, 0x6d, 0x73, 0x02, 0x20, 0x00, 0x00, 0x6d, 0x6e, 0x74, 0x72, 0x52, 0x47, 0x42, 0x20, 0x58,
    0x59, 0x5a, 0x20, 0x07, 0xd0, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0x63, 0x73, 0x70, 0x4d, 0x53,
    0x46, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6, 0xd6, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xd3, 0x2d, 0x61, 0x76, 0x69, 0x66,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x5f, 0x77, 0x74, 0x70,
    0x74, 0x00, 0x00, 0x01, 0x0c, 0x00, 0x00, 0x00, 0x14, 0x72, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0x20, 0x00, 0x00, 0x00, 0x14,
    0x67, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0x34, 0x00, 0x00, 0x00, 0x14, 0x62, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0x48, 0x00,
    0x00, 0x00, 0x14, 0x72, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0x5c, 0x00, 0x00, 0x00, 0x10, 0x67, 0x54, 0x52, 0x43, 0x00, 0x00,
    0x01, 0x5c, 0x00, 0x00, 0x00, 0x10, 0x62, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0x5c, 0x00, 0x00, 0x00, 0x10, 0x63, 0x70, 0x72,
    0x74, 0x00, 0x00, 0x01, 0x6c, 0x00, 0x00, 0x00, 0x0c, 0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x61, 0x76, 0x69, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x59, 0x5a, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xf3, 0x54, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0xc9, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x6f, 0xa0, 0x00, 0x00, 0x38, 0xf2, 0x00, 0x00, 0x03, 0x8f, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x62, 0x96, 0x00, 0x00, 0xb7, 0x89, 0x00, 0x00, 0x18, 0xda, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x24, 0xa0, 0x00, 0x00, 0x0f, 0x85, 0x00, 0x00, 0xb6, 0xc4, 0x63, 0x75, 0x72, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x74, 0x65, 0x78, 0x74, 0x00, 0x00, 0x00, 0x00, 0x43, 0x43, 0x30, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,
    0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const size_t iccColorLength = 376;

static const ptrdiff_t colorWhiteOffset = 0x114;
static const ptrdiff_t colorRedOffset = 0x128;
static const ptrdiff_t colorGreenOffset = 0x13c;
static const ptrdiff_t colorBlueOffset = 0x150;
static const ptrdiff_t colorGammaOffset = 0x168;

/**
 * Gray Profile Structure
 *
 * Header:
 *  size         = 275 bytes
 *  CMM          = 'lcms'
 *  Version      = 2.2.0
 *  Device Class = Display
 *  Color Space  = Gray
 *  Conn. Space  = XYZ
 *  Date, Time   = 1 Jan 2000, 0:00:00
 *  Platform     = Microsoft
 *  Flags        = Not Embedded Profile, Use anywhere
 *  Dev. Mnfctr. = 0x0
 *  Dev. Model   = 0x0
 *  Dev. Attrbts = Reflective, Glossy, Positive, Color
 *  Rndrng Intnt = Perceptual
 *  Illuminant   = 0.96420288, 1.00000000, 0.82490540    [Lab 100.000000, 0.000000, 0.000000]
 *  Creator      = 'avif'
 *
 * Profile Tags:
 *                    Tag    ID      Offset         Size                 Value
 *                   ----  ------    ------         ----                 -----
 *  profileDescriptionTag  'desc'       180           95                  avif
 *     mediaWhitePointTag  'wtpt'       208           20        (to be filled)
 *             grayTRCTag  'kTRC'       228           16        (to be filled)
 *           copyrightTag  'cprt'       244           12                   CC0
 */

static const uint8_t iccGrayTemplate[320] = {
    0x00, 0x00, 0x01, 0x13, 0x6c, 0x63, 0x6d, 0x73, 0x02, 0x20, 0x00, 0x00, 0x6d, 0x6e, 0x74, 0x72, 0x47, 0x52, 0x41, 0x59,
    0x58, 0x59, 0x5a, 0x20, 0x07, 0xd0, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0x63, 0x73, 0x70,
    0x4d, 0x53, 0x46, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6, 0xd6, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xd3, 0x2d,
    0x61, 0x76, 0x69, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0xb4,
    0x00, 0x00, 0x00, 0x5f, 0x77, 0x74, 0x70, 0x74, 0x00, 0x00, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x14, 0x6b, 0x54, 0x52, 0x43,
    0x00, 0x00, 0x00, 0xe4, 0x00, 0x00, 0x00, 0x10, 0x63, 0x70, 0x72, 0x74, 0x00, 0x00, 0x00, 0xf4, 0x00, 0x00, 0x00, 0x0c,
    0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x61, 0x76, 0x69, 0x66, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf3, 0x54,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0xc9, 0x63, 0x75, 0x72, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x74, 0x65, 0x78, 0x74, 0x00, 0x00, 0x00, 0x00, 0x43, 0x43, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const size_t iccGrayLength = 275;

static const ptrdiff_t grayWhiteOffset = 0xd8;
static const ptrdiff_t grayGammaOffset = 0xf0;

static const ptrdiff_t checksumOffset = 0x54;

static const double small = 1e-12;

static uint32_t readLittleEndianU32(const uint8_t * data)
{
    return ((uint32_t)data[0] << 0) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void writeLittleEndianU32(uint8_t * data, uint32_t value)
{
    data[0] = (value >> 0) & 0xff;
    data[1] = (value >> 8) & 0xff;
    data[2] = (value >> 16) & 0xff;
    data[3] = (value >> 24) & 0xff;
}

static void writeBigEndianU16(uint8_t * data, uint16_t value)
{
    data[0] = (value >> 8) & 0xff;
    data[1] = (value >> 0) & 0xff;
}

static void writeBigEndianU32(uint8_t * data, uint32_t value)
{
    data[0] = (value >> 24) & 0xff;
    data[1] = (value >> 16) & 0xff;
    data[2] = (value >> 8) & 0xff;
    data[3] = (value >> 0) & 0xff;
}

static avifBool putS15Fixed16(uint8_t * data, double value)
{
    value = round(value * 65536);
    if (value > INT32_MAX || value < INT32_MIN) {
        return AVIF_FALSE;
    }

    int32_t fixed = (int32_t)value;
    // reinterpret into uint32_t to ensure the exact bits are written.
    writeBigEndianU32(data, *(uint32_t *)&fixed);

    return AVIF_TRUE;
}

static avifBool putU8Fixed8(uint8_t * data, float value)
{
    value = roundf(value * 256);
    if (value > UINT16_MAX || value < 1) {
        return AVIF_FALSE;
    }

    uint16_t fixed = (uint16_t)value;
    writeBigEndianU16(data, fixed);
    return AVIF_TRUE;
}

static avifBool putColorant(uint8_t * data, const double XYZ[3])
{
    if (!putS15Fixed16(data, XYZ[0])) {
        return AVIF_FALSE;
    }

    if (!putS15Fixed16(data + 4, XYZ[1])) {
        return AVIF_FALSE;
    }

    if (!putS15Fixed16(data + 8, XYZ[2])) {
        return AVIF_FALSE;
    }

    return AVIF_TRUE;
}

static avifBool xyToXYZ(const float xy[2], double XYZ[3])
{
    if (fabsf(xy[1]) < small) {
        return AVIF_FALSE;
    }

    const double factor = 1.0 / xy[1];
    XYZ[0] = xy[0] * factor;
    XYZ[1] = 1;
    XYZ[2] = (1 - xy[0] - xy[1]) * factor;

    return AVIF_TRUE;
}

// Computes I = M^-1. Returns false if M seems to be singular.
static avifBool matInv(const double M[3][3], double I[3][3])
{
    double det = M[0][0] * (M[1][1] * M[2][2] - M[2][1] * M[1][2]) - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
                 M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
    if (fabs(det) < small) {
        return AVIF_FALSE;
    }
    det = 1 / det;

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
static void matMul(const double A[3][3], const double B[3][3], double C[3][3])
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
static void matDiag(const double d[3], double M[3][3])
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

static void swap(double * a, double * b)
{
    double tmp = *a;
    *a = *b;
    *b = tmp;
}

// Transpose M
static void matTrans(double M[3][3])
{
    swap(&M[0][1], &M[1][0]);
    swap(&M[0][2], &M[2][0]);
    swap(&M[1][2], &M[2][1]);
}

// Computes y = M.x
static void vecMul(const double M[3][3], const double x[3], double y[3])
{
    y[0] = M[0][0] * x[0] + M[0][1] * x[1] + M[0][2] * x[2];
    y[1] = M[1][0] * x[0] + M[1][1] * x[1] + M[1][2] * x[2];
    y[2] = M[2][0] * x[0] + M[2][1] * x[1] + M[2][2] * x[2];
}

// MD5 algorithm. See https://www.ietf.org/rfc/rfc1321.html#appendix-A.3
// This function writes the MD5 checksum in place at offset `checksumOffset` of `data`.
// This function shall only be called with a copy of iccColorTemplate or iccGrayTemplate, and sizeof(icc*Template).
static void computeMD5(uint8_t * data, size_t length)
{
    static const uint32_t sineparts[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af,
        0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97,
        0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
    };
    static const uint8_t shift[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
    };

    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;

    for (uint32_t i = 0; i < length; i += 64) {
        uint32_t a = a0, b = b0, c = c0, d = d0, f, g;
        for (uint32_t j = 0; j < 64; j++) {
            if (j < 16) {
                f = (b & c) | ((~b) & d);
                g = j;
            } else if (j < 32) {
                f = (d & b) | ((~d) & c);
                g = (5 * j + 1) & 0xf;
            } else if (j < 48) {
                f = b ^ c ^ d;
                g = (3 * j + 5) & 0xf;
            } else {
                f = c ^ (b | (~d));
                g = (7 * j) & 0xf;
            }
            uint32_t u = readLittleEndianU32(data + i + g * 4);
            f += a + sineparts[j] + u;
            a = d;
            d = c;
            c = b;
            b += (f << shift[j]) | (f >> (32u - shift[j]));
        }
        a0 += a;
        b0 += b;
        c0 += c;
        d0 += d;
    }

    uint8_t * output = data + checksumOffset;
    writeLittleEndianU32(output, a0);
    writeLittleEndianU32(output + 4, b0);
    writeLittleEndianU32(output + 8, c0);
    writeLittleEndianU32(output + 12, d0);
}

// Bradford chromatic adaptation matrix
// from https://www.researchgate.net/publication/253799640_A_uniform_colour_space_based_upon_CIECAM97s
static const double bradford[3][3] = {
    { 0.8951, 0.2664, -0.1614 },
    { -0.7502, 1.7135, 0.0367 },
    { 0.0389, -0.0685, 1.0296 },
};

// LMS values for D50 whitepoint
static const double lmsD50[3] = { 0.996284, 1.02043, 0.818644 };

avifBool avifGenerateRGBICC(avifRWData * icc, float gamma, const float primaries[8])
{
    uint8_t buffer[sizeof(iccColorTemplate)];
    memcpy(buffer, iccColorTemplate, sizeof(iccColorTemplate));

    double whitePointXYZ[3];
    if (!xyToXYZ(&primaries[6], whitePointXYZ)) {
        return AVIF_FALSE;
    }

    if (!putColorant(buffer + colorWhiteOffset, whitePointXYZ)) {
        return AVIF_FALSE;
    }

    const double rgbPrimaries[3][3] = {
        { primaries[0], primaries[2], primaries[4] },
        { primaries[1], primaries[3], primaries[5] },
        { 1.0 - primaries[0] - primaries[1], 1.0 - primaries[2] - primaries[3], 1.0 - primaries[4] - primaries[5] }
    };

    double rgbPrimariesInv[3][3];
    if (!matInv(rgbPrimaries, rgbPrimariesInv)) {
        return AVIF_FALSE;
    }

    double rgbCoefficients[3];
    vecMul(rgbPrimariesInv, whitePointXYZ, rgbCoefficients);

    double rgbCoefficientsMat[3][3];
    matDiag(rgbCoefficients, rgbCoefficientsMat);

    double rgbXYZ[3][3];
    matMul(rgbPrimaries, rgbCoefficientsMat, rgbXYZ);

    // ICC stores primaries XYZ under PCS.
    // Adapt using linear bradford transform
    // from https://onlinelibrary.wiley.com/doi/pdf/10.1002/9781119021780.app3
    double lms[3];
    vecMul(bradford, whitePointXYZ, lms);
    for (int i = 0; i < 3; ++i) {
        if (fabs(lms[i]) < small) {
            return AVIF_FALSE;
        }
        lms[i] = lmsD50[i] / lms[i];
    }

    double adaptation[3][3];
    matDiag(lms, adaptation);

    double tmp[3][3];
    matMul(adaptation, bradford, tmp);

    double bradfordInv[3][3];
    if (!matInv(bradford, bradfordInv)) {
        return AVIF_FALSE;
    }
    matMul(bradfordInv, tmp, adaptation);

    double rgbXYZD50[3][3];
    matMul(adaptation, rgbXYZ, rgbXYZD50);
    matTrans(rgbXYZD50);

    if (!putColorant(buffer + colorRedOffset, rgbXYZD50[0])) {
        return AVIF_FALSE;
    }

    if (!putColorant(buffer + colorGreenOffset, rgbXYZD50[1])) {
        return AVIF_FALSE;
    }

    if (!putColorant(buffer + colorBlueOffset, rgbXYZD50[2])) {
        return AVIF_FALSE;
    }

    if (!putU8Fixed8(buffer + colorGammaOffset, gamma)) {
        return AVIF_FALSE;
    }

    computeMD5(buffer, sizeof(iccColorTemplate));
    if (avifRWDataSet(icc, buffer, iccColorLength) != AVIF_RESULT_OK) {
        return AVIF_FALSE;
    }

    return AVIF_TRUE;
}

avifBool avifGenerateGrayICC(avifRWData * icc, float gamma, const float white[2])
{
    uint8_t buffer[sizeof(iccGrayTemplate)];
    memcpy(buffer, iccGrayTemplate, sizeof(iccGrayTemplate));

    double whitePointXYZ[3];
    if (!xyToXYZ(white, whitePointXYZ)) {
        return AVIF_FALSE;
    }

    if (!putColorant(buffer + grayWhiteOffset, whitePointXYZ)) {
        return AVIF_FALSE;
    }

    if (!putU8Fixed8(buffer + grayGammaOffset, gamma)) {
        return AVIF_FALSE;
    }

    computeMD5(buffer, sizeof(iccGrayTemplate));
    if (avifRWDataSet(icc, buffer, iccGrayLength) != AVIF_RESULT_OK) {
        return AVIF_FALSE;
    }

    return AVIF_TRUE;
}
