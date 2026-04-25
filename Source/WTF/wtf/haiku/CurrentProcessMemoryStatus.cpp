/*
 * Copyright (C) 2018 Haiku, inc
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CurrentProcessMemoryStatus.h"

#include <OS.h>


namespace WTF {

void currentProcessMemoryStatus(ProcessMemoryStatus& memoryStatus)
{
    memoryStatus.size = 0;
    memoryStatus.resident = 0;
    memoryStatus.shared = 0;
    memoryStatus.text = 0;
    memoryStatus.data = 0;

    area_info area;
    ssize_t cookie = 0;

    while (get_next_area_info(0, &cookie, &area) == B_OK) {
        memoryStatus.size += area.size;
        memoryStatus.resident += area.ram_size;

        if (area.copy_count > 0)
            memoryStatus.shared += area.size;

        if (area.protection & B_EXECUTE_AREA)
            memoryStatus.text += area.size;
        else
            memoryStatus.data += area.size;
    }

    memoryStatus.lib = memoryStatus.size;
    memoryStatus.dt = memoryStatus.size;
}

} // namespace WTF

