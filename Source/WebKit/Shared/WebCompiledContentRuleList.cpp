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
#include "WebCompiledContentRuleList.h"

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebKit {

Ref<WebCompiledContentRuleList> WebCompiledContentRuleList::create(WebCompiledContentRuleListData&& data)
{
    return adoptRef(*new WebCompiledContentRuleList(WTFMove(data)));
}

WebCompiledContentRuleList::WebCompiledContentRuleList(WebCompiledContentRuleListData&& data)
    : m_data(WTFMove(data))
{
}

WebCompiledContentRuleList::~WebCompiledContentRuleList()
{
}

bool WebCompiledContentRuleList::conditionsApplyOnlyToDomain() const
{
    return *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(m_data.data->data()) + m_data.conditionsApplyOnlyToDomainOffset);
}

const WebCore::ContentExtensions::DFABytecode* WebCompiledContentRuleList::filtersWithoutConditionsBytecode() const
{
    return static_cast<const WebCore::ContentExtensions::DFABytecode*>(m_data.data->data()) + m_data.filtersWithoutConditionsBytecodeOffset;
}

unsigned WebCompiledContentRuleList::filtersWithoutConditionsBytecodeLength() const
{
    return m_data.filtersWithoutConditionsBytecodeSize;
}

const WebCore::ContentExtensions::DFABytecode* WebCompiledContentRuleList::filtersWithConditionsBytecode() const
{
    return static_cast<const WebCore::ContentExtensions::DFABytecode*>(m_data.data->data()) + m_data.filtersWithConditionsBytecodeOffset;
}

unsigned WebCompiledContentRuleList::filtersWithConditionsBytecodeLength() const
{
    return m_data.filtersWithConditionsBytecodeSize;
}

const WebCore::ContentExtensions::DFABytecode* WebCompiledContentRuleList::topURLFiltersBytecode() const
{
    return static_cast<const WebCore::ContentExtensions::DFABytecode*>(m_data.data->data()) + m_data.topURLFiltersBytecodeOffset;
}

unsigned WebCompiledContentRuleList::topURLFiltersBytecodeLength() const
{
    return m_data.topURLFiltersBytecodeSize;
}

const WebCore::ContentExtensions::SerializedActionByte* WebCompiledContentRuleList::actions() const
{
    return static_cast<const WebCore::ContentExtensions::SerializedActionByte*>(m_data.data->data()) + m_data.actionsOffset;
}

unsigned WebCompiledContentRuleList::actionsLength() const
{
    return m_data.actionsSize;
}

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)
