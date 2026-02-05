/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)

#include <wtf/Box.h>
#include <wtf/HashMap.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

namespace WebCore {

using ISO18013IssuerIdentifiers = Vector<String>;

using ISO18013ElementReference = HashMap<String, String>;

using ISO18013AlternativeDataElementSet = Vector<ISO18013ElementReference>;

using ISO18013AlternativeDataElementSets = Vector<ISO18013AlternativeDataElementSet>;

struct ISO18013AlternativeDataElementsSet {
    ISO18013ElementReference requestedElement;
    ISO18013AlternativeDataElementSets alternativeElementSets;
};

struct ISO18013ZkSystemSpec {
    String zkSystemId;
    String system;
};

struct ISO18013ZkRequest {
    Vector<ISO18013ZkSystemSpec> systemSpecs;
    bool zkRequired;
};

struct ISO18013Any {
    Variant<
        std::monostate,
        int,
        bool,
        String,
        Vector<Box<ISO18013Any>>,
        HashMap<String, Box<ISO18013Any>>
    > data;
};

using ISO18013DocumentRequestInfoExtension = HashMap<String, ISO18013Any>;

struct ISO18013DocumentRequestInfo {
    std::optional<ISO18013AlternativeDataElementsSet> alternativeDataElements;
    std::optional<ISO18013IssuerIdentifiers> issuerIdentifiers;
    std::optional<bool> uniqueDocSetRequired;
    std::optional<uint32_t> maximumResponseSize;
    std::optional<ISO18013ZkRequest> zkRequest;
    std::optional<String> encryptionParameterBytes;
    ISO18013DocumentRequestInfoExtension extension;
};

} // namespace WebCore

#endif // ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)
