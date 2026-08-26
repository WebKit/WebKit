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

#include <wtf/ArgumentCoder.h>
#include <wtf/Forward.h>

namespace WebCore {
class RegistrableDomain;
class SecurityOrigin;
class SecurityOriginData;
class Site;
struct ClientOrigin;
}

namespace IPC {

// Presented with every origin, site, domain and URL reachable from a serialized struct.
// Implemented by the designated validation procedures, which cannot be templates here
// because the traversal is generated into the serializer implementation files.
class UntrustedValueVisitor {
public:
    virtual ~UntrustedValueVisitor() = default;

    virtual void visitUntrusted(const URL&) = 0;
    virtual void visitUntrusted(const WebCore::ClientOrigin&) = 0;
    virtual void visitUntrusted(const WebCore::RegistrableDomain&) = 0;
    virtual void visitUntrusted(const WebCore::SecurityOrigin&) = 0;
    virtual void visitUntrusted(const WebCore::SecurityOriginData&) = 0;
    virtual void visitUntrusted(const WebCore::Site&) = 0;
};

// Specialized by generate-serializers.py: every serialized type that transitively carries
// one of the types above gets ArgumentCoder<T>::visitUntrustedValues. It lives on the
// argument coder because that is already a friend of the type it serializes, so the
// traversal can reach private members. Wrapping such a struct in IPC::Untrusted<T> is
// therefore as strong as wrapping the origin itself: no field can be reached without a visit.
template<typename T> concept CarriesUntrustedValues = requires(const T& value, UntrustedValueVisitor& visitor)
{
    ArgumentCoder<T>::visitUntrustedValues(value, visitor);
};

} // namespace IPC
