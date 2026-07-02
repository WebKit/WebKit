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

#include "config.h"
#include "WebExtensionDataRecord.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include <wtf/OptionSet.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionDataRecordHolder);

WebExtensionDataRecord::WebExtensionDataRecord(const String& displayName, const String& uniqueIdentifier)
    : m_displayName(displayName)
    , m_uniqueIdentifier(uniqueIdentifier)
{
}

bool WebExtensionDataRecord::operator==(const WebExtensionDataRecord& other) const
{
    return this == &other || (m_displayName == other.m_displayName && m_uniqueIdentifier == other.m_uniqueIdentifier);
}

size_t WebExtensionDataRecord::totalSize() const
{
    size_t total = 0;
    for (auto& entry : m_typeSizes)
        total += entry.value;
    return total;
}

size_t WebExtensionDataRecord::sizeOfTypes(OptionSet<Type> types) const
{
    size_t total = 0;
    for (auto type : types)
        total += m_typeSizes.get(type);
    return total;
}

OptionSet<WebExtensionDataRecord::Type> WebExtensionDataRecord::types() const
{
    OptionSet<WebExtensionDataRecord::Type> result;
    for (auto& entry : m_typeSizes)
        result.add(entry.key);
    return result;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
