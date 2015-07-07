/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "WebCrossThreadCopier.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBIdentifier.h"
#include "SecurityOriginData.h"
#include "UniqueIDBDatabaseIdentifier.h"
#include <WebCore/IDBKeyData.h>

using namespace WebKit;

namespace WebCore {

UniqueIDBDatabaseIdentifier CrossThreadCopierBase<false, false, UniqueIDBDatabaseIdentifier>::copy(const UniqueIDBDatabaseIdentifier& identifier)
{
    return identifier.isolatedCopy();
}

IDBIdentifier CrossThreadCopierBase<false, false, IDBIdentifier>::copy(const IDBIdentifier& transactionIdentifier)
{
    return transactionIdentifier.isolatedCopy();
}

Vector<char> CrossThreadCopierBase<false, false, Vector<char>>::copy(const Vector<char>& vector)
{
    Vector<char> result;
    result.appendVector(vector);
    return result;
}

Vector<int64_t> CrossThreadCopierBase<false, false, Vector<int64_t>>::copy(const Vector<int64_t>& vector)
{
    Vector<int64_t> result;
    result.appendVector(vector);
    return result;
}

Vector<uint8_t> CrossThreadCopierBase<false, false, Vector<uint8_t>>::copy(const Vector<uint8_t>& vector)
{
    Vector<uint8_t> result;
    result.appendVector(vector);
    return result;
}

Vector<Vector<IDBKeyData>> CrossThreadCopierBase<false, false, Vector<Vector<IDBKeyData>>>::copy(const Vector<Vector<IDBKeyData>>& vector)
{
    Vector<Vector<IDBKeyData>> result;

    for (const auto& keys : vector) {
        result.append(Vector<IDBKeyData>());
        for (const auto& key : keys)
            result.last().append(WebCore::CrossThreadCopier<IDBKeyData>::copy(key));
    }

    return result;
}

SecurityOriginData CrossThreadCopierBase<false, false, SecurityOriginData>::copy(const SecurityOriginData& securityOriginData)
{
    return securityOriginData.isolatedCopy();
}

ASCIILiteral CrossThreadCopierBase<false, false, ASCIILiteral>::copy(const ASCIILiteral& literal)
{
    return literal;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
