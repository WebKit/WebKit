/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "SharedBuffer.h"

namespace WebCore {

enum class PromisedBlobType { DataBacked, FileBacked };

struct PromisedBlobInfo {
    String blobURL;
    String contentType;
    String filename;
    PromisedBlobType blobType;

    Vector<String> additionalTypes;
    Vector<RefPtr<SharedBuffer>> additionalData;

    operator bool() const { return !blobURL.isEmpty(); }
};

struct PromisedBlobData {
    String blobURL;
    String filePath;
    RefPtr<SharedBuffer> data;

    bool hasData() const { return data; }
    bool hasFile() const { return !filePath.isEmpty(); }
    operator bool() const { return !blobURL.isEmpty(); }
    bool fulfills(const PromisedBlobInfo& info) const { return *this && blobURL == info.blobURL; }
};

} // namespace WebCore

namespace WTF {

template<typename> struct EnumTraits;
template<typename E, E...> struct EnumValues;

template<> struct EnumTraits<WebCore::PromisedBlobType> {
    using values = EnumValues<WebCore::PromisedBlobType,
    WebCore::PromisedBlobType::DataBacked,
    WebCore::PromisedBlobType::FileBacked
    >;
};

} // namespace WTF
