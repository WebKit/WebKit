/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include <wtf/EnumTraits.h>

namespace WebCore {

namespace IndexedDB {

enum class TransactionState {
    Active,
    Inactive,
    Committing,
    Aborting,
    Finished,
};

enum class CursorDirection {
    Next,
    Nextunique,
    Prev,
    Prevunique,
};
const unsigned CursorDirectionMaximum = 3;

enum class CursorType : bool {
    KeyAndValue = 0,
    KeyOnly = 1,
};
const unsigned CursorTypeMaximum = 1;

enum class CursorSource : bool {
    Index,
    ObjectStore,
};

enum class VersionNullness {
    Null,
    NonNull,
};

enum class ObjectStoreOverwriteMode {
    Overwrite,
    OverwriteForCursor,
    NoOverwrite,
};

enum class IndexRecordType : bool {
    Key,
    Value,
};

enum class ObjectStoreRecordType {
    ValueOnly,
    KeyOnly,
};

// In order of the least to the highest precedent in terms of sort order.
enum KeyType {
    Max = -1,
    Invalid = 0,
    Array,
    Binary,
    String,
    Date,
    Number,
    Min,
};

enum class RequestType {
    Open,
    Delete,
    Other,
};

enum class GetAllType {
    Keys,
    Values,
};

enum class ConnectionClosedOnBehalfOfServer : bool { No, Yes };

enum class CursorIterateOption : bool {
    DoNotReply,
    Reply,
};

} // namespace IndexedDB

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::IndexedDB::CursorDirection> {
    using values = EnumValues<
        WebCore::IndexedDB::CursorDirection,
        WebCore::IndexedDB::CursorDirection::Next,
        WebCore::IndexedDB::CursorDirection::Nextunique,
        WebCore::IndexedDB::CursorDirection::Prev,
        WebCore::IndexedDB::CursorDirection::Prevunique
    >;
};

template<> struct EnumTraits<WebCore::IndexedDB::KeyType> {
    using values = EnumValues<
        WebCore::IndexedDB::KeyType,
        WebCore::IndexedDB::KeyType::Max,
        WebCore::IndexedDB::KeyType::Invalid,
        WebCore::IndexedDB::KeyType::Array,
        WebCore::IndexedDB::KeyType::Binary,
        WebCore::IndexedDB::KeyType::String,
        WebCore::IndexedDB::KeyType::Date,
        WebCore::IndexedDB::KeyType::Number,
        WebCore::IndexedDB::KeyType::Min
    >;
};

template<> struct EnumTraits<WebCore::IndexedDB::ObjectStoreOverwriteMode> {
    using values = EnumValues<
        WebCore::IndexedDB::ObjectStoreOverwriteMode,
        WebCore::IndexedDB::ObjectStoreOverwriteMode::Overwrite,
        WebCore::IndexedDB::ObjectStoreOverwriteMode::OverwriteForCursor,
        WebCore::IndexedDB::ObjectStoreOverwriteMode::NoOverwrite
    >;
};

template<> struct EnumTraits<WebCore::IndexedDB::RequestType> {
    using values = EnumValues<
        WebCore::IndexedDB::RequestType,
        WebCore::IndexedDB::RequestType::Open,
        WebCore::IndexedDB::RequestType::Delete,
        WebCore::IndexedDB::RequestType::Other
    >;
};

} // namespace WTF

#endif // ENABLED(INDEXED_DATABASE)
