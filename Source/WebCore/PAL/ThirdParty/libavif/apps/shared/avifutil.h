// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LIBAVIF_APPS_SHARED_AVIFUTIL_H
#define LIBAVIF_APPS_SHARED_AVIFUTIL_H

#include "avif/avif.h"

#ifdef __cplusplus
extern "C" {
#endif

// The %z format specifier is not available in the old Windows CRT msvcrt,
// hence the %I format specifier must be used instead to print out `size_t`.
// The new Windows CRT UCRT, which is used by Visual Studio 2015 or later,
// supports the %z specifier properly.
//
// Additionally, with c99 set as the standard mingw-w64 toolchains built with
// the commit mentioned can patch format functions to support the %z specifier,
// even if it's using the old msvcrt, and this can be detected by
// the `__USE_MINGW_ANSI_STDIO` macro.
//
// Related mingw-w64 commit: bfd33f6c0ec5e652cc9911857dd1492ece8d8383

#if !defined(_UCRT) && (defined(__USE_MINGW_ANSI_STDIO) && __USE_MINGW_ANSI_STDIO == 0)
#define AVIF_FMT_ZU "%Iu"
#else
#define AVIF_FMT_ZU "%zu"
#endif

void avifImageDump(const avifImage * avif, uint32_t gridCols, uint32_t gridRows, avifProgressiveState progressiveState);
void avifContainerDump(const avifDecoder * decoder);
void avifPrintVersions(void);
void avifDumpDiagnostics(const avifDiagnostics * diag);
int avifQueryCPUCount(void); // Returns 1 if it cannot query or fails to query

typedef enum avifAppFileFormat
{
    AVIF_APP_FILE_FORMAT_UNKNOWN = 0,

    AVIF_APP_FILE_FORMAT_AVIF,
    AVIF_APP_FILE_FORMAT_JPEG,
    AVIF_APP_FILE_FORMAT_PNG,
    AVIF_APP_FILE_FORMAT_Y4M
} avifAppFileFormat;

avifAppFileFormat avifGuessFileFormat(const char * filename);

// This structure holds any timing data coming from source (typically non-AVIF) inputs being fed
// into avifenc. If either or both values are 0, the timing is "invalid" / sentinel and the values
// should be ignored. This structure is used to override the timing defaults in avifenc when the
// enduser doesn't provide timing on the commandline and the source content provides a framerate.
typedef struct avifAppSourceTiming
{
    uint64_t duration;  // duration in time units (based on the timescale below)
    uint64_t timescale; // timescale of the media (Hz)
} avifAppSourceTiming;

struct y4mFrameIterator;
// Reads an image from a file with the requested format and depth.
// In case of a y4m file, sourceTiming and frameIter can be set.
// Returns AVIF_APP_FILE_FORMAT_UNKNOWN in case of error.
avifAppFileFormat avifReadImage(const char * filename,
                                avifPixelFormat requestedFormat,
                                int requestedDepth,
                                avifChromaDownsampling chromaDownsampling,
                                avifBool ignoreICC,
                                avifBool ignoreExif,
                                avifBool ignoreXMP,
                                avifImage * image,
                                uint32_t * outDepth,
                                avifAppSourceTiming * sourceTiming,
                                struct y4mFrameIterator ** frameIter);

// Used by image decoders when the user doesn't explicitly choose a format with --yuv
// This must match the cited fallback for "--yuv auto" in avifenc.c's syntax() function.
#define AVIF_APP_DEFAULT_PIXEL_FORMAT AVIF_PIXEL_FORMAT_YUV444

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ifndef LIBAVIF_APPS_SHARED_AVIFUTIL_H
