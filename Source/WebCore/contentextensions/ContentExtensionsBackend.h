/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#include "ContentExtension.h"
#include "ContentExtensionRule.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DocumentLoader;
class ResourceRequest;

namespace ContentExtensions {

class CompiledContentExtension;
struct ResourceLoadInfo;

// The ContentExtensionsBackend is the internal model of all the content extensions.
//
// It provides two services:
// 1) It stores the rules for each content extension.
// 2) It provides APIs for the WebCore interfaces to use those rules efficiently.
class ContentExtensionsBackend {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // - Rule management interface. This can be used by upper layer.

    // Set a list of rules for a given name. If there were existing rules for the name, they are overridden.
    // The identifier cannot be empty.
    WEBCORE_EXPORT void addContentExtension(const String& identifier, Ref<CompiledContentExtension>, ContentExtension::ShouldCompileCSS = ContentExtension::ShouldCompileCSS::Yes);
    WEBCORE_EXPORT void removeContentExtension(const String& identifier);
    WEBCORE_EXPORT void removeAllContentExtensions();

    // - Internal WebCore Interface.
    WEBCORE_EXPORT Vector<ActionsFromContentRuleList> actionsForResourceLoad(const ResourceLoadInfo&) const;
    WEBCORE_EXPORT StyleSheetContents* globalDisplayNoneStyleSheet(const String& identifier) const;

    ContentRuleListResults processContentRuleListsForLoad(Page&, const URL&, OptionSet<ResourceType>, DocumentLoader& initiatingDocumentLoader, const URL& redirectFrom);
    WEBCORE_EXPORT ContentRuleListResults processContentRuleListsForPingLoad(const URL&, const URL& mainDocumentURL);

    static const String& displayNoneCSSRule();

    void forEach(const Function<void(const String&, ContentExtension&)>&);

    WEBCORE_EXPORT static bool shouldBeMadeSecure(const URL&);

private:
    HashMap<String, Ref<ContentExtension>> m_contentExtensions;
};

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
