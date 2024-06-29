/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include "ObjectIdentifier.h"

#include "MainThread.h"
#include <atomic>

namespace WTF {

uint64_t ObjectIdentifierMainThreadAccessTraits<uint64_t>::generateIdentifierInternal()
{
    ASSERT(isMainThread()); // You should use AtomicObjectIdentifier if you're hitting this assertion.
    static uint64_t current = 0;
    return ++current;
}

uint64_t ObjectIdentifierThreadSafeAccessTraits<uint64_t>::generateIdentifierInternal()
{
    static std::atomic<uint64_t> current;
    return ++current;
}

TextStream& operator<<(TextStream& ts, const ObjectIdentifierGenericBase<uint64_t>& identifier)
{
    ts << identifier.toRawValue();
    return ts;
}

UUID ObjectIdentifierMainThreadAccessTraits<UUID>::generateIdentifierInternal()
{
    ASSERT(isMainThread()); // You should use AtomicObjectIdentifier if you're hitting this assertion.
    return UUID::createVersion4();
}

UUID ObjectIdentifierThreadSafeAccessTraits<UUID>::generateIdentifierInternal()
{
    return UUID::createVersion4();
}

TextStream& operator<<(TextStream& ts, const ObjectIdentifierGenericBase<UUID>& identifier)
{
    ts << identifier.toRawValue();
    return ts;
}

} // namespace WTF
