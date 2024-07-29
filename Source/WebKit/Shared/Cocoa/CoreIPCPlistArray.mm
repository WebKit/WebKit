/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "CoreIPCPlistArray.h"

#if PLATFORM(COCOA)

#import "CoreIPCData.h"
#import "CoreIPCDate.h"
#import "CoreIPCNumber.h"
#import "CoreIPCPlistDictionary.h"
#import "CoreIPCPlistObject.h"
#import "CoreIPCString.h"
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

CoreIPCPlistArray::CoreIPCPlistArray(NSArray *array)
{
    for (id value in array) {
        if (!CoreIPCPlistObject::isPlistType(value))
            continue;
        m_array.append(CoreIPCPlistObject(value));
    }
}

CoreIPCPlistArray::CoreIPCPlistArray(const RetainPtr<NSArray>& array)
    : CoreIPCPlistArray(array.get()) { }

CoreIPCPlistArray::CoreIPCPlistArray(CoreIPCPlistArray&&) = default;

CoreIPCPlistArray::~CoreIPCPlistArray() = default;

CoreIPCPlistArray::CoreIPCPlistArray(Vector<CoreIPCPlistObject>&& array)
    : m_array(WTFMove(array)) { }

RetainPtr<id> CoreIPCPlistArray::toID() const
{
    return createNSArray(m_array, [] (auto& object) -> id {
        return object.toID().get();
    });
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
