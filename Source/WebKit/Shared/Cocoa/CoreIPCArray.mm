/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "CoreIPCArray.h"

#if PLATFORM(COCOA)

#import "CoreIPCNSCFObject.h"
#import "CoreIPCTypes.h"

namespace WebKit {

CoreIPCArray::CoreIPCArray(NSArray *array)
{
    for (id value in array) {
        if (!IPC::isSerializableValue(value))
            continue;
        m_array.append(CoreIPCNSCFObject(value));
    }
}

CoreIPCArray::CoreIPCArray(const RetainPtr<NSArray>& array)
    : CoreIPCArray(array.get()) { }

CoreIPCArray::CoreIPCArray(CoreIPCArray&&) = default;

CoreIPCArray::~CoreIPCArray() = default;

CoreIPCArray::CoreIPCArray(Vector<CoreIPCNSCFObject>&& array)
    : m_array(WTFMove(array)) { }

RetainPtr<id> CoreIPCArray::toID() const
{
    auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:m_array.size()]);
    for (auto& object : m_array) {
        if (RetainPtr objectID = object.toID())
            [result addObject:objectID.get()];
    }
    return result;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
