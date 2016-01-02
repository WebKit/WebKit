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
#include "WebCompiledContentExtension.h"

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebKit {

Ref<WebCompiledContentExtension> WebCompiledContentExtension::create(WebCompiledContentExtensionData&& data)
{
    return adoptRef(*new WebCompiledContentExtension(WTFMove(data)));
}

WebCompiledContentExtension::WebCompiledContentExtension(WebCompiledContentExtensionData&& data)
    : m_data(WTFMove(data))
{
}

WebCompiledContentExtension::~WebCompiledContentExtension()
{
}

const WebCore::ContentExtensions::DFABytecode* WebCompiledContentExtension::filtersWithoutDomainsBytecode() const
{
    return static_cast<const WebCore::ContentExtensions::DFABytecode*>(m_data.data->data()) + m_data.filtersWithoutDomainsBytecodeOffset;
}

unsigned WebCompiledContentExtension::filtersWithoutDomainsBytecodeLength() const
{
    return m_data.filtersWithoutDomainsBytecodeSize;
}

const WebCore::ContentExtensions::DFABytecode* WebCompiledContentExtension::filtersWithDomainsBytecode() const
{
    return static_cast<const WebCore::ContentExtensions::DFABytecode*>(m_data.data->data()) + m_data.filtersWithDomainsBytecodeOffset;
}

unsigned WebCompiledContentExtension::filtersWithDomainsBytecodeLength() const
{
    return m_data.filtersWithDomainsBytecodeSize;
}

const WebCore::ContentExtensions::DFABytecode* WebCompiledContentExtension::domainFiltersBytecode() const
{
    return static_cast<const WebCore::ContentExtensions::DFABytecode*>(m_data.data->data()) + m_data.domainFiltersBytecodeOffset;
}

unsigned WebCompiledContentExtension::domainFiltersBytecodeLength() const
{
    return m_data.domainFiltersBytecodeSize;
}

const WebCore::ContentExtensions::SerializedActionByte* WebCompiledContentExtension::actions() const
{
    return static_cast<const WebCore::ContentExtensions::SerializedActionByte*>(m_data.data->data()) + m_data.actionsOffset;
}

unsigned WebCompiledContentExtension::actionsLength() const
{
    return m_data.actionsSize;
}

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)
