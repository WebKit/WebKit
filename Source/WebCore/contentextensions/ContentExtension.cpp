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

#include "config.h"
#include "ContentExtension.h"

#include "CompiledContentExtension.h"
#include "ContentExtensionsBackend.h"
#include "StyleSheetContents.h"
#include <wtf/text/StringBuilder.h>

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {
namespace ContentExtensions {

RefPtr<ContentExtension> ContentExtension::create(const String& identifier, Ref<CompiledContentExtension>&& compiledExtension)
{
    return adoptRef(*new ContentExtension(identifier, WTF::move(compiledExtension)));
}

ContentExtension::ContentExtension(const String& identifier, Ref<CompiledContentExtension>&& compiledExtension)
    : m_identifier(identifier)
    , m_compiledExtension(WTF::move(compiledExtension))
    , m_parsedGlobalDisplayNoneStyleSheet(false)
{
}

StyleSheetContents* ContentExtension::globalDisplayNoneStyleSheet()
{
    if (m_parsedGlobalDisplayNoneStyleSheet)
        return m_globalDisplayNoneStyleSheet.get();

    m_parsedGlobalDisplayNoneStyleSheet = true;

    Vector<String> selectors = m_compiledExtension->globalDisplayNoneSelectors();
    if (selectors.isEmpty())
        return nullptr;

    StringBuilder css;
    for (auto& selector : selectors) {
        css.append(selector);
        css.append("{");
        css.append(ContentExtensionsBackend::displayNoneCSSRule());
        css.append("}");
    }

    m_globalDisplayNoneStyleSheet = StyleSheetContents::create();
    m_globalDisplayNoneStyleSheet->setIsUserStyleSheet(true);
    if (!m_globalDisplayNoneStyleSheet->parseString(css.toString()))
        m_globalDisplayNoneStyleSheet = nullptr;

    return m_globalDisplayNoneStyleSheet.get();
}

const DFABytecodeInterpreter::Actions& ContentExtension::cachedDomainActions(const String& domain)
{
    if (m_cachedDomain != domain) {
        DFABytecodeInterpreter interpreter(m_compiledExtension->domainFiltersBytecode(), m_compiledExtension->domainFiltersBytecodeLength());
        const uint16_t allLoadTypesAndResourceTypes = LoadTypeMask | ResourceTypeMask;
        auto domainActions = interpreter.interpret(domain.utf8(), allLoadTypesAndResourceTypes);
        
        m_cachedDomainActions.clear();
        for (uint64_t action : domainActions)
            m_cachedDomainActions.add(action);
        m_cachedDomain = domain;
    }
    return m_cachedDomainActions;
}
    
} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
