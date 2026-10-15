/*
 * Copyright (C) 2026 Igalia S.L.
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

#pragma once

#ifdef __OBJC__

#import <Foundation/Foundation.h>

// A class an Objective-C compile unit defines.
@interface HeapWalkPlainObject : NSObject {
@public
    id other;
    long value;
}
@end

#endif // __OBJC__

#ifdef __cplusplus

#include <cstdint>

#if OS(DARWIN) && ENABLE(MYA_HEAP)

namespace JSCToolsTest {

// Only an Objective-C++ compile unit defines the class of this object, which the walk finds from its vtable.
class ObjectiveCHolderInterface {
public:
    virtual ~ObjectiveCHolderInterface();
};

struct ObjectiveCHolderLeaf {
    uint64_t value { 0 };
};

struct ObjectiveCHolderAddresses {
    uint64_t holder;
    // The Objective-C objects and the block the holder points to.
    uint64_t object;
    uint64_t plainObject;
    uint64_t block;
    // Held only by a lambda declared in an Objective-C method.
    uint64_t leaf;
};

ObjectiveCHolderInterface* createObjectiveCHolder(ObjectiveCHolderAddresses&);

} // namespace JSCToolsTest

#endif // OS(DARWIN) && ENABLE(MYA_HEAP)

#endif // __cplusplus
