//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// debug.cpp: Debugging utilities.

#include "common/debug.h"

#include <stdio.h>
#include <stdarg.h>

static bool trace_on = true;

namespace gl
{
void trace(const char *format, ...)
{
    if (trace_on)
    {
        if (format)
        {
            FILE *file = fopen("debug.txt", "a");

            if (file)
            {
                va_list vararg;
                va_start(vararg, format);
                vfprintf(file, format, vararg);
                va_end(vararg);

                fclose(file);
            }
        }
    }
}
}
