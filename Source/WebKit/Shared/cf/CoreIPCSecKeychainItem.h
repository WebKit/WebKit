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

#if HAVE(SEC_KEYCHAIN)

#import <Security/SecKeychainItem.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebKit {

// For now, the only way to serialize/deserialize SecKeychainItem objects is via
// SecKeychainItemCreatePersistentReference()/SecKeychainItemCopyFromPersistentReference(). rdar://122050787

class CoreIPCSecKeychainItem {
public:
    CoreIPCSecKeychainItem(SecKeychainItemRef keychainItem)
        : m_persistentRef(persistentRefForKeychainItem(keychainItem))
    {
    }

    CoreIPCSecKeychainItem(RetainPtr<CFDataRef> data)
        : m_persistentRef(data)
    {
    }

    CoreIPCSecKeychainItem(std::span<const uint8_t> data)
        : m_persistentRef(data.empty() ? nullptr : adoptCF(CFDataCreate(kCFAllocatorDefault, data.data(), data.size())))
    {
    }

    RetainPtr<SecKeychainItemRef> createSecKeychainItem() const
    {
        RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));

        if (!m_persistentRef)
            return nullptr;

        CFDataRef data = m_persistentRef.get();
        // SecKeychainItemCopyFromPersistentReference() cannot handle 0-length CFDataRefs.
        if (!CFDataGetLength(data))
            return nullptr;

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        SecKeychainItemRef keychainItem = NULL;
        SecKeychainItemCopyFromPersistentReference(data, &keychainItem);
        ALLOW_DEPRECATED_DECLARATIONS_END
        return adoptCF(keychainItem);
    }

    std::span<const uint8_t> dataReference() const
    {
        if (!m_persistentRef)
            return { };

        CFDataRef data = m_persistentRef.get();
        return { CFDataGetBytePtr(data), static_cast<size_t>(CFDataGetLength(data)) };
    }

private:
    RetainPtr<CFDataRef> persistentRefForKeychainItem(SecKeychainItemRef keychainItem) const
    {
        RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));
        if (!keychainItem)
            return nullptr;

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CFDataRef data = NULL;
        SecKeychainItemCreatePersistentReference(keychainItem, &data);
        ALLOW_DEPRECATED_DECLARATIONS_END

        return adoptCF(data);
    }

    RetainPtr<CFDataRef> m_persistentRef;
};

} // namespace WebKit

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // HAVE(SEC_KEYCHAIN)
