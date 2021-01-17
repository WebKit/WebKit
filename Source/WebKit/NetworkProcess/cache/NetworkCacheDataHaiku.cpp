/*
 * Copyright (C) 2019 Haiku, Inc.
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
#include "NetworkCacheData.h"

#include "NotImplemented.h"

namespace WebKit {
namespace NetworkCache {

Data::Data(const uint8_t* data, size_t size)
{
    notImplemented();
}

Data Data::empty()
{
    notImplemented();
    return { };
}

const uint8_t* Data::data() const
{
    notImplemented();
    return nullptr;
}

bool Data::isNull() const
{
    notImplemented();
    return true;
}

bool Data::apply(const Function<bool(const uint8_t*, size_t)>& applier) const
{
    notImplemented();
    return false;
}

Data Data::subrange(size_t offset, size_t size) const
{
    return { };
}

Data concatenate(const Data& a, const Data& b)
{
    notImplemented();
    return { };
}

Data Data::adoptMap(void* map, size_t size, int fd)
{
    notImplemented();
    return { };
}

} // namespace NetworkCache
} // namespace WebKit
