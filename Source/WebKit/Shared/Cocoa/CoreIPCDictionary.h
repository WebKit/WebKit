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

#include "CoreIPCNSCFObject.h"
#include <wtf/ArgumentCoder.h>
#include <wtf/KeyValuePair.h>
#include <wtf/RetainPtr.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace WebKit {

class CoreIPCDictionary {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CoreIPCDictionary(NSDictionary *);

    CoreIPCDictionary(const RetainPtr<NSDictionary>& dictionary)
        : CoreIPCDictionary(dictionary.get())
    {
    }

    RetainPtr<id> toID() const;

    bool keyHasValueOfType(const String&, IPC::NSType) const;
    bool keyIsMissingOrHasValueOfType(const String&, IPC::NSType) const;
    bool collectionValuesAreOfType(const String& key, IPC::NSType) const;
    bool collectionValuesAreOfType(const String& key, IPC::NSType, IPC::NSType) const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCDictionary, void>;

    using ValueType = Vector<KeyValuePair<UniqueRef<CoreIPCNSCFObject>, UniqueRef<CoreIPCNSCFObject>>>;

    CoreIPCDictionary(ValueType&& keyValuePairs)
        : m_keyValuePairs(WTFMove(keyValuePairs))
    {
    }

    void createNSDictionaryIfNeeded() const;

    mutable RetainPtr<NSDictionary> m_nsDictionary;
    ValueType m_keyValuePairs;
};

} // namespace WebKit

#endif // PLATFORM(COCOA)
