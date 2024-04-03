// Copyright 2021 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void avifDiagnosticsClearError(avifDiagnostics * diag)
{
    *diag->error = '\0';
}

#ifdef __clang__
__attribute__((__format__(__printf__, 2, 3)))
#endif
void avifDiagnosticsPrintf(avifDiagnostics * diag, const char * format, ...)
{
    if (!diag) {
        // It is possible this is NULL (e.g. calls to avifPeekCompatibleFileType())
        return;
    }
    if (*diag->error) {
        // There is already a detailed error set.
        return;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(diag->error, AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE, format, args);
    diag->error[AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE - 1] = '\0';
    va_end(args);
}
