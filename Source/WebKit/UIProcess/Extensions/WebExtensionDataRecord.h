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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIObject.h"
#include "WebExtensionDataType.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSMutableArray;
OBJC_CLASS WKWebExtensionDataRecord;

namespace WebKit {

class WebExtensionDataRecord : public API::ObjectImpl<API::Object::Type::WebExtensionDataRecord> {
    WTF_MAKE_NONCOPYABLE(WebExtensionDataRecord);

public:
    template<typename... Args>
    static Ref<WebExtensionDataRecord> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionDataRecord(std::forward<Args>(args)...));
    }

    explicit WebExtensionDataRecord(const String& displayName, const String& uniqueIdentifier);

    using Type = WebExtensionDataType;

    const String& displayName() const { return m_displayName; }
    const String& uniqueIdentifier() const { return m_uniqueIdentifier; }

    OptionSet<Type> types() const;

    size_t totalSize() const;
    size_t sizeOfTypes(OptionSet<Type>) const;

    size_t sizeOfType(Type type) const { return m_typeSizes.get(type); }
    void setSizeOfType(Type type, size_t size) { m_typeSizes.set(type, size); }

    NSArray *errors();
    void addError(NSString *debugDescription, Type);

#ifdef __OBJC__
    WKWebExtensionDataRecord *wrapper() const { return (WKWebExtensionDataRecord *)API::ObjectImpl<API::Object::Type::WebExtensionDataRecord>::wrapper(); }
#endif

    bool operator==(const WebExtensionDataRecord&) const;

private:
    String m_displayName;
    String m_uniqueIdentifier;
    HashMap<Type, size_t> m_typeSizes;
    RetainPtr<NSMutableArray> m_errors;
};

class WebExtensionDataRecordHolder : public RefCounted<WebExtensionDataRecordHolder> {
    WTF_MAKE_NONCOPYABLE(WebExtensionDataRecordHolder);
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionDataRecordHolder);

public:
    template<typename... Args>
    static Ref<WebExtensionDataRecordHolder> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionDataRecordHolder(std::forward<Args>(args)...));
    }

    WebExtensionDataRecordHolder() { };

    HashMap<String, Ref<WebExtensionDataRecord>> recordsMap;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
