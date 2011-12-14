/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
#include "SharedMemory.h"

#include <WebCore/NotImplemented.h>

namespace WebKit {

SharedMemory::Handle::Handle()
{
    notImplemented();
}

SharedMemory::Handle::~Handle()
{
    notImplemented();
}

void SharedMemory::Handle::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    notImplemented();
}

bool SharedMemory::Handle::decode(CoreIPC::ArgumentDecoder* decoder, Handle& handle)
{
    notImplemented();
    return false;
}

PassRefPtr<SharedMemory> SharedMemory::create(size_t size)
{
    notImplemented();
    return 0;
}

PassRefPtr<SharedMemory> SharedMemory::create(const Handle& handle, Protection protection)
{
    notImplemented();
    return 0;    
}

SharedMemory::~SharedMemory()
{
    notImplemented();
}
    
bool SharedMemory::createHandle(Handle& handle, Protection protection)
{
    notImplemented();
    return false;
}

unsigned SharedMemory::systemPageSize()
{
    unsigned pageSize = 0;

    return pageSize;
}

}
