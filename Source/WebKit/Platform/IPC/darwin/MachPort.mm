/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MachPort.h"

#import "Logging.h"

namespace IPC {

kern_return_t allocateImmovableConnectionPort(mach_port_name_t* port)
{
#if HAVE(IMMOVABLE_MACH_PORT)
    mach_port_options_t options = {
        .flags = MPO_CONNECTION_PORT | MPO_IMMOVABLE_RECEIVE,
        .service_port_name = MPO_ANONYMOUS_SERVICE
    };
    auto kr = mach_port_construct(mach_task_self(), &options, 0x0, port);
    if (kr == KERN_SUCCESS)
        return kr;
    RELEASE_LOG_ERROR(IPC, "allocateImmovableConnectionPort: Call to mach_port_construct() failed with %{private}s (%x), falling back to calling mach_port_allocate()", mach_error_string(kr), kr);
#endif
    return mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, port);
}

} // namespace IPC
