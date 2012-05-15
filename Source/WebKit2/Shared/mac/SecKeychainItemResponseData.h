/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef SecKeychainItemResponseData_h
#define SecKeychainItemResponseData_h

#if USE(SECURITY_FRAMEWORK)

#include "KeychainAttribute.h"
#include <Security/Security.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class ArgumentEncoder;
}

namespace WebKit {
    
class SecKeychainItemResponseData {
public:
    SecKeychainItemResponseData();
    SecKeychainItemResponseData(OSStatus, SecItemClass, SecKeychainAttributeList*, RetainPtr<CFDataRef>);
    SecKeychainItemResponseData(OSStatus, RetainPtr<SecKeychainItemRef>);
    SecKeychainItemResponseData(OSStatus);

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, SecKeychainItemResponseData&);

    SecItemClass itemClass() const { return m_itemClass; }
    CFDataRef data() const { return m_data.get(); }
    OSStatus resultCode() const { return m_resultCode; }
    const Vector<KeychainAttribute>& attributes() const { return m_attributes; }
    SecKeychainItemRef keychainItem() const { return m_keychainItem.get(); }

private:
    SecItemClass m_itemClass;
    RetainPtr<CFDataRef> m_data;
    OSStatus m_resultCode;
    Vector<KeychainAttribute> m_attributes;
    RetainPtr<SecKeychainItemRef> m_keychainItem;
};
    
} // namespace WebKit

#endif // USE(SECURITY_FRAMEWORK)

#endif // SecKeychainItemResponseData_h
