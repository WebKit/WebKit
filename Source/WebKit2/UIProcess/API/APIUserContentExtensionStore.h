/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef APIUserContentExtensionStore_h
#define APIUserContentExtensionStore_h

#if ENABLE(CONTENT_EXTENSIONS)

#include "APIObject.h"
#include <system_error>
#include <wtf/text/WTFString.h>

namespace WTF {
class WorkQueue;
}

namespace API {

class UserContentExtension;

class UserContentExtensionStore final : public ObjectImpl<Object::Type::UserContentExtensionStore> {
public:
    enum class Error {
        LookupFailed = 1,
        VersionMismatch,
        CompileFailed,
        RemoveFailed
    };
    
    // This should be incremented every time a functional change is made to the bytecode, file format, etc.
    // to prevent crashing while loading old data.
    const static uint32_t CurrentContentExtensionFileVersion = 7;

    static UserContentExtensionStore& defaultStore();
    static Ref<UserContentExtensionStore> storeWithPath(const WTF::String& storePath);

    explicit UserContentExtensionStore();
    explicit UserContentExtensionStore(const WTF::String& storePath);
    virtual ~UserContentExtensionStore();

    void compileContentExtension(const WTF::String& identifier, WTF::String&& json, std::function<void(RefPtr<API::UserContentExtension>, std::error_code)>);
    void lookupContentExtension(const WTF::String& identifier, std::function<void(RefPtr<API::UserContentExtension>, std::error_code)>);
    void removeContentExtension(const WTF::String& identifier, std::function<void(std::error_code)>);

    // For testing only.
    void synchronousRemoveAllContentExtensions();
    void invalidateContentExtensionVersion(const WTF::String& identifier);

private:
    WTF::String defaultStorePath();

    const WTF::String m_storePath;
    Ref<WTF::WorkQueue> m_compileQueue;
    Ref<WTF::WorkQueue> m_readQueue;
    Ref<WTF::WorkQueue> m_removeQueue;
};

const std::error_category& userContentExtensionStoreErrorCategory();

inline std::error_code make_error_code(UserContentExtensionStore::Error error)
{
    return { static_cast<int>(error), userContentExtensionStoreErrorCategory() };
}

} // namespace API

namespace std {
    template<> struct is_error_code_enum<API::UserContentExtensionStore::Error> : public true_type { };
}

#endif // ENABLE(CONTENT_EXTENSIONS)
#endif // APIUserContentExtensionStore_h
