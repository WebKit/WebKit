/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <optional>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebKit {

struct CacheStorageRecord;
struct CacheStorageRecordInformation;

class CacheStorageStore : public RefCounted<CacheStorageStore> {
public:
    virtual ~CacheStorageStore() = default;
    using ReadAllRecordsCallback = CompletionHandler<void(Vector<CacheStorageRecord>&&)>;
    using ReadRecordsCallback = CompletionHandler<void(Vector<std::optional<CacheStorageRecord>>&&)>;
    using WriteRecordsCallback = CompletionHandler<void(bool)>;
    virtual void readAllRecords(ReadAllRecordsCallback&&) = 0;
    virtual void readRecords(const Vector<CacheStorageRecordInformation>&, ReadRecordsCallback&&) = 0;
    virtual void deleteRecords(const Vector<CacheStorageRecordInformation>&, WriteRecordsCallback&&) = 0;
    virtual void writeRecords(Vector<CacheStorageRecord>&&, WriteRecordsCallback&&) = 0;

protected:
    CacheStorageStore() = default;
};

} // namespace WebKit

