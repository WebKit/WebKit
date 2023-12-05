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

#pragma once

#if PLATFORM(COCOA)

#include "ArgumentCodersCocoa.h"
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
@interface NSObject (WebKitSecureCoding)
- (NSDictionary *)_webKitPropertyListData;
- (id)_initWithWebKitPropertyListData:(NSDictionary *)plist;
@end
#endif

namespace WebKit {

class CoreIPCSecureCoding {
WTF_MAKE_FAST_ALLOCATED;
public:
    CoreIPCSecureCoding(id);
    CoreIPCSecureCoding(const RetainPtr<NSObject<NSSecureCoding>>& object)
        : CoreIPCSecureCoding(object.get())
    {
    }

    RetainPtr<id> toID() const { return m_secureCoding; }

    Class objectClass() { return m_secureCoding.get().class; }

    static bool conformsToWebKitSecureCoding(id);
    static bool conformsToSecureCoding(id);

private:
    friend struct IPC::ArgumentCoder<CoreIPCSecureCoding, void>;

    IPC::CoreIPCRetainPtr<NSObject<NSSecureCoding>> m_secureCoding;
};

} // namespace WebKit

#endif // PLATFORM(COCOA)
