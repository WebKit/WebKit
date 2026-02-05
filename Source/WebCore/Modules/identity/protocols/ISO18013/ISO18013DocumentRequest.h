/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include <WebCore/ISO18013DocumentRequestInfo.h>
#endif // ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)
#include <WebCore/ISO18013ElementInfo.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using ISO18013ElementNamespaceVector = Vector<std::pair<String, ISO18013ElementInfo>>;

using ISO18013ElementNamespacesVector = Vector<std::pair<String, ISO18013ElementNamespaceVector>>;

struct ISO18013DocumentRequest {
    String documentType;
    ISO18013ElementNamespacesVector namespaces;
#if ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)
    std::optional<ISO18013DocumentRequestInfo> requestInfo;
#endif // ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)
};

using ISO18013DocumentRequests = Vector<ISO18013DocumentRequest>;

} // namespace WebCore
