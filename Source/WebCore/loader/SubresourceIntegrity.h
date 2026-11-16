/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "ResourceCryptographicDigest.h"
#include <optional>
#include <wtf/Forward.h>

namespace WebCore {

class CachedResource;
class FragmentedSharedBuffer;
class ResourceError;
class ResourceResponse;

enum class FetchOptionsDestination : uint8_t;
enum class ResourceResponseBaseTainting : uint8_t;

// The algorithms below do not depend on CachedResource, so that any loader can use them.

// https://w3c.github.io/webappsec-subresource-integrity/#parse-metadata-section
Vector<EncodedResourceCryptographicDigest> parseIntegrityMetadata(const String& metadata);

// https://w3c.github.io/webappsec-subresource-integrity/#does-response-match-metadatalist
// `digestForAlgorithm` lets callers that already hold digests of the bytes (e.g. CachedResource) avoid hashing them again.
bool doBytesMatchMetadataList(ResourceResponseBaseTainting, NOESCAPE const ScopedLambda<ResourceCryptographicDigest(ResourceCryptographicDigest::Algorithm)>& digestForAlgorithm, const String& metadataList);
bool doBytesMatchMetadataList(ResourceResponseBaseTainting, const FragmentedSharedBuffer* bytes, const String& metadataList);

// Step 23 of https://fetch.spec.whatwg.org/#main-fetch, run once response's body has been fully read.
// Returns the network error the response must be replaced with, if any.
std::optional<ResourceError> checkResponseBodyIntegrity(const ResourceResponse&, ResourceResponseBaseTainting, FetchOptionsDestination, const FragmentedSharedBuffer* body, const String& integrityMetadata);

String integrityMismatchDescription(const URL&, const ResourceResponse&, const FragmentedSharedBuffer* body, const String& integrityMetadata);

// The network error returned by checkResponseBodyIntegrity(), distinguishable from other access control errors.
bool isIntegrityMismatchError(const ResourceError&);

// CachedResource conveniences, for consumers that check a resource that may have come from the memory cache.
bool matchIntegrityMetadata(const CachedResource&, const String& integrityMetadata);
bool matchIntegrityMetadataWithoutReportingHashes(const CachedResource&, const String& integrityMetadata);
void reportHashesIfNeeded(const CachedResource&);
String integrityMismatchDescription(const CachedResource&, const String& integrityMetadata);

}
