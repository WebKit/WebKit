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

#ifndef SecKeychainItemRequestData_h
#define SecKeychainItemRequestData_h

#include "DataReference.h"
#include "KeychainAttribute.h"
#include <Security/Security.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class ArgumentEncoder;
}

namespace WebKit {
    
class SecKeychainItemRequestData {
public:
    enum Type {
        Invalid,
        CopyContent,
        CreateFromContent,
        ModifyContent,
    };

    SecKeychainItemRequestData();
    SecKeychainItemRequestData(Type, SecKeychainItemRef, SecKeychainAttributeList*);
    SecKeychainItemRequestData(Type, SecKeychainItemRef, SecKeychainAttributeList*, UInt32 length, const void* data);
    SecKeychainItemRequestData(Type, SecItemClass, SecKeychainAttributeList*, UInt32 length, const void* data);
    ~SecKeychainItemRequestData();

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, SecKeychainItemRequestData&);

    Type type() const { return m_type; }

    SecKeychainItemRef keychainItem() const { return m_keychainItem.get(); }
    SecItemClass itemClass() const { return m_itemClass; }
    UInt32 length() const { return m_dataReference.size(); }
    const void* data() const { return m_dataReference.data(); }
    
    SecKeychainAttributeList* attributeList() const;

private:
    void initializeWithAttributeList(SecKeychainAttributeList*);

    Type m_type;
    RetainPtr<SecKeychainItemRef> m_keychainItem;
    SecItemClass m_itemClass;
    CoreIPC::DataReference m_dataReference;
    
    Vector<KeychainAttribute> m_keychainAttributes;

    struct Attributes : public ThreadSafeRefCounted<Attributes> {
        mutable OwnPtr<SecKeychainAttributeList> m_attributeList;
        mutable OwnArrayPtr<SecKeychainAttribute> m_attributes;
    };
    RefPtr<Attributes> m_attrs;
};
    
} // namespace WebKit

#endif // SecKeychainItemRequestData_h
