/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsChecksMac.h"

#if PLATFORM(MAC)

#if HAVE(APPLE_GRAPHICS_CONTROL)
#include <sys/sysctl.h>
#endif

namespace WebCore {

#if HAVE(APPLE_GRAPHICS_CONTROL)

enum {
    kAGCOpen,
    kAGCClose
};

static io_connect_t attachToAppleGraphicsControl()
{
    mach_port_t masterPort = MACH_PORT_NULL;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (IOMasterPort(MACH_PORT_NULL, &masterPort) != KERN_SUCCESS)
        return IO_OBJECT_NULL;
    ALLOW_DEPRECATED_DECLARATIONS_END

    CFDictionaryRef classToMatch = IOServiceMatching("AppleGraphicsControl");
    if (!classToMatch)
        return IO_OBJECT_NULL;

    kern_return_t kernResult;
    io_iterator_t iterator;
    if ((kernResult = IOServiceGetMatchingServices(masterPort, classToMatch, &iterator)) != KERN_SUCCESS)
        return IO_OBJECT_NULL;

    io_service_t serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
        return IO_OBJECT_NULL;

    io_connect_t dataPort;
    IOObjectRetain(serviceObject);
    kernResult = IOServiceOpen(serviceObject, mach_task_self(), 0, &dataPort);
    IOObjectRelease(serviceObject);

    return (kernResult == KERN_SUCCESS) ? dataPort : IO_OBJECT_NULL;
}

static bool hasMuxCapability()
{
    io_connect_t dataPort = attachToAppleGraphicsControl();

    if (dataPort == IO_OBJECT_NULL)
        return false;

    bool result;
    if (IOConnectCallScalarMethod(dataPort, kAGCOpen, nullptr, 0, nullptr, nullptr) == KERN_SUCCESS) {
        IOConnectCallScalarMethod(dataPort, kAGCClose, nullptr, 0, nullptr, nullptr);
        result = true;
    } else
        result = false;

    IOServiceClose(dataPort);

    if (result) {
        // This is detecting Mac hardware with an Intel g575 GPU, which
        // we don't want to make available to muxing.
        // Based on information from Apple's OpenGL team, such devices
        // have four or fewer processors.
        // <rdar://problem/30060378>
        int names[2] = { CTL_HW, HW_NCPU };
        int cpuCount;
        size_t cpuCountLength = sizeof(cpuCount);
        sysctl(names, 2, &cpuCount, &cpuCountLength, nullptr, 0);
        result = cpuCount > 4;
    }

    return result;
}

#endif // HAVE(APPLE_GRAPHICS_CONTROL)

bool hasLowAndHighPowerGPUs()
{
#if HAVE(APPLE_GRAPHICS_CONTROL)
    static bool canMux = hasMuxCapability();
    return canMux;
#else
    return false;
#endif
}

}

#endif

