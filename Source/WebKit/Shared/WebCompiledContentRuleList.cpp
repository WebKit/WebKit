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

RefPtr<WebCompiledContentRuleList> WebCompiledContentRuleList::create(WebCompiledContentRuleListData&& data)
{
    if (!data.data)
        return nullptr;
    return adoptRef(*new WebCompiledContentRuleList(WTFMove(data)));
}

WebCompiledContentRuleList::WebCompiledContentRuleList(WebCompiledContentRuleListData&& data)
    : m_data(WTFMove(data))
{
}

WebCompiledContentRuleList::~WebCompiledContentRuleList()
{
}

std::span<const uint8_t> WebCompiledContentRuleList::urlFiltersBytecode() const
{
    return spanWithOffsetAndLength(m_data.urlFiltersBytecodeOffset, m_data.urlFiltersBytecodeSize);
}

std::span<const uint8_t> WebCompiledContentRuleList::topURLFiltersBytecode() const
{
    return spanWithOffsetAndLength(m_data.topURLFiltersBytecodeOffset, m_data.topURLFiltersBytecodeSize);
}

std::span<const uint8_t> WebCompiledContentRuleList::frameURLFiltersBytecode() const
{
    return spanWithOffsetAndLength(m_data.frameURLFiltersBytecodeOffset, m_data.frameURLFiltersBytecodeSize);
}

std::span<const uint8_t> WebCompiledContentRuleList::serializedActions() const
{
    return spanWithOffsetAndLength(m_data.actionsOffset, m_data.actionsSize);
}

std::span<const uint8_t> WebCompiledContentRuleList::spanWithOffsetAndLength(size_t offset, size_t length) const
{
    RELEASE_ASSERT(offset + length <= m_data.data->size());
    return m_data.data->span().subspan(offset, length);
}

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)
