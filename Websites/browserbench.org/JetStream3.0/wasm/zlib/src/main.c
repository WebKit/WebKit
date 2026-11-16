// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include "zlib.h"

// To be compiled together with `zpipe.c` from zlib. Forward declarations:
int def(FILE *source, FILE *dest, int level);
int inf(FILE *source, FILE *dest);
void zerr(int ret);

// Exported to JavaScript:
int compressFile(const char* infilename, const char* outfilename)
{
    FILE* infile = fopen(infilename, "rb");
    if (infile == NULL) {
        fputs("error opening input file", stderr);
        return 1;
    }
    FILE* outfile = fopen(outfilename, "wb");
    if (outfile == NULL) {
        fputs("error opening output file", stderr);
        return 1;
    }

    int ret = def(infile, outfile, Z_BEST_COMPRESSION);
    if (ret != Z_OK)
        zerr(ret);

    if (fclose(infile)) {
        fputs("error closing input file", stderr);
        return 1;
    }
    if (fclose(outfile)) {
        fputs("error closing output file", stderr);
        return 1;
    }
    return ret;
}
int decompressFile(const char* infilename, const char* outfilename)
{
    FILE* infile = fopen(infilename, "rb");
    if (infile == NULL) {
        fputs("error opening input file", stderr);
        return 1;
    }
    FILE* outfile = fopen(outfilename, "wb");
    if (outfile == NULL) {
        fputs("error opening output file", stderr);
        return 1;
    }

    int ret = inf(infile, outfile);
    if (ret != Z_OK)
        zerr(ret);

    if (fclose(infile)) {
        fputs("error closing input file", stderr);
        return 1;
    }
    if (fclose(outfile)) {
        fputs("error closing output file", stderr);
        return 1;
    }
    return ret;
}
