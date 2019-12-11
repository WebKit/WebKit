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

#pragma once

#if ENABLE(CONTENT_EXTENSIONS)

#include "APIObject.h"
#include <system_error>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SharedBuffer;
}

namespace WTF {
class WorkQueue;
}

namespace API {

class ContentRuleList;

class ContentRuleListStore final : public ObjectImpl<Object::Type::ContentRuleListStore> {
public:
    enum class Error {
        LookupFailed = 1,
        VersionMismatch,
        CompileFailed,
        RemoveFailed
    };
    
    // This should be incremented every time a functional change is made to the bytecode, file format, etc.
    // to prevent crashing while loading old data.
    // Also update ContentRuleListStore::getContentRuleListSource to be able to find the original JSON
    // source from old versions.
    // Update ContentRuleListStore::getContentRuleListSource with this.
    static constexpr uint32_t CurrentContentRuleListFileVersion = 10;

    static ContentRuleListStore& defaultStore(bool legacyFilename);
    static Ref<ContentRuleListStore> storeWithPath(const WTF::String& storePath, bool legacyFilename);

    explicit ContentRuleListStore(bool legacyFilename);
    explicit ContentRuleListStore(const WTF::String& storePath, bool legacyFilename);
    virtual ~ContentRuleListStore();

    void compileContentRuleList(const WTF::String& identifier, WTF::String&& json, CompletionHandler<void(RefPtr<API::ContentRuleList>, std::error_code)>);
    void lookupContentRuleList(const WTF::String& identifier, CompletionHandler<void(RefPtr<API::ContentRuleList>, std::error_code)>);
    void removeContentRuleList(const WTF::String& identifier, CompletionHandler<void(std::error_code)>);
    void getAvailableContentRuleListIdentifiers(CompletionHandler<void(WTF::Vector<WTF::String>)>);

    // For testing only.
    void synchronousRemoveAllContentRuleLists();
    void invalidateContentRuleListVersion(const WTF::String& identifier);
    void getContentRuleListSource(const WTF::String& identifier, CompletionHandler<void(WTF::String)>);

private:
    WTF::String defaultStorePath(bool legacyFilename);
    static ContentRuleListStore& legacyDefaultStore();
    static ContentRuleListStore& nonLegacyDefaultStore();
    
    const WTF::String m_storePath;
    Ref<WTF::WorkQueue> m_compileQueue;
    Ref<WTF::WorkQueue> m_readQueue;
    Ref<WTF::WorkQueue> m_removeQueue;
    bool m_legacyFilename { false };
};

const std::error_category& contentRuleListStoreErrorCategory();

inline std::error_code make_error_code(ContentRuleListStore::Error error)
{
    return { static_cast<int>(error), contentRuleListStoreErrorCategory() };
}

} // namespace API

namespace std {
template<> struct is_error_code_enum<API::ContentRuleListStore::Error> : public true_type { };
}

#endif // ENABLE(CONTENT_EXTENSIONS)
