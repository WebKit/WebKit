/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
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

#include "Memory.h"
#include <iostream>
#include <stdlib.h>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/task_info.h>
#else
#include <stdio.h>
#include <unistd.h>
#endif

Memory currentMemoryBytes()
{
    Memory memory;

#ifdef __APPLE__
    task_vm_info_data_t vm_info;
    mach_msg_type_number_t vm_size = TASK_VM_INFO_COUNT;
    if (KERN_SUCCESS != task_info(mach_task_self(), TASK_VM_INFO_PURGEABLE, (task_info_t)(&vm_info), &vm_size)) {
        std::cout << "Failed to get mach task info" << std::endl;
        exit(1);
    }

    memory.resident = vm_info.internal + vm_info.compressed - vm_info.purgeable_volatile_pmap;
    memory.residentMax = vm_info.resident_size_peak;
#else
    FILE* file = fopen("/proc/self/status", "r");

    auto forEachLine = [] (FILE* file, auto functor) {
        char* buffer = nullptr;
        size_t size = 0;
        while (getline(&buffer, &size, file) != -1) {
            functor(buffer, size);
            ::free(buffer); // Be careful. getline's memory allocation is done by system malloc.
            buffer = nullptr;
            size = 0;
        }
    };

    unsigned long vmHWM = 0;
    unsigned long vmRSS = 0;
    unsigned long rssFile = 0;
    unsigned long rssShmem = 0;
    forEachLine(file, [&] (char* buffer, size_t) {
        unsigned long sizeInKB = 0;
        if (sscanf(buffer, "VmHWM: %lu kB", &sizeInKB) == 1)
            vmHWM = sizeInKB * 1024;
        else if (sscanf(buffer, "VmRSS: %lu kB", &sizeInKB) == 1)
            vmRSS = sizeInKB * 1024;
        else if (sscanf(buffer, "RssFile: %lu kB", &sizeInKB) == 1)
            rssFile = sizeInKB * 1024;
        else if (sscanf(buffer, "RssShmem: %lu kB", &sizeInKB) == 1)
            rssShmem = sizeInKB * 1024;
    });
    fclose(file);
    memory.resident = vmRSS - (rssFile + rssShmem);
    memory.residentMax = vmHWM - (rssFile + rssShmem); // We do not have any way to get the peak of RSS of anonymous pages. Here, we subtract RSS of files and shmem to estimate the peak of RSS of anonymous pages.
#endif
    return memory;
}
