/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FetchOptions.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

class CachedResourceRequest;
class Document;
struct ResourceLoaderOptions;
struct ThreadableLoaderOptions;

namespace CSS {

struct URLModifiers;
struct URLProvenance;

// https://drafts.csswg.org/css-values-4/#fetch-a-style-resource

// Loader options that do not depend on the url().
ResourceLoaderOptions styleResourceLoaderOptions(bool isInitiatingElementInUserAgentShadowTree);

// Steps 3 through 7, for loads that use a CachedResourceLoader.
CachedResourceRequest createStyleResourceRequest(Document&, const WTF::URL&, const URLModifiers&, const URLProvenance&, FetchOptions::Mode, ResourceLoaderOptions, const AtomString& initiatorType = nullAtom());

// Steps 3 through 7, for loads that use a ThreadableLoader.
void applyStyleResourceRequestSteps(ThreadableLoaderOptions&, const URLModifiers&, const URLProvenance&, FetchOptions::Mode);

} // namespace CSS
} // namespace WebCore
