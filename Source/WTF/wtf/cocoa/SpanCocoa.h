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

#pragma once

#import <dispatch/dispatch.h>
#import <span>
#import <wtf/RetainPtr.h>

namespace WTF {

#ifdef __OBJC__
inline std::span<const uint8_t> span(NSData *data LIFETIME_BOUND)
{
    if (!data)
        return { };

    return unsafeMakeSpan(static_cast<const uint8_t*>(data.bytes), data.length);
}

inline RetainPtr<NSData> toNSData(std::span<const uint8_t> span)
{
    return adoptNS([[NSData alloc] initWithBytes:span.data() length:span.size()]);
}

enum class FreeWhenDone : bool { No, Yes };
inline RetainPtr<NSData> toNSDataNoCopy(std::span<const uint8_t> span, FreeWhenDone freeWhenDone)
{
    return adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<uint8_t*>(span.data()) length:span.size() freeWhenDone:freeWhenDone == FreeWhenDone::Yes]);
}
#endif // #ifdef __OBJC__

template<typename> class Function;

#ifdef __cplusplus
extern "C" {
#endif
WTF_EXPORT_PRIVATE bool dispatch_data_apply_span(dispatch_data_t, NOESCAPE const Function<bool(std::span<const uint8_t>)>& applier);
#ifdef __cplusplus
} // extern "C
#endif

} // namespace WTF

using WTF::dispatch_data_apply_span;

#ifdef __OBJC__
using WTF::FreeWhenDone;
using WTF::span;
using WTF::toNSData;
using WTF::toNSDataNoCopy;
#endif
