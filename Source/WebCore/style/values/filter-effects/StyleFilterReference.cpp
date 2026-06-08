/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "StyleFilterReference.h"

#include "CSSFilterReference.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "SVGURIReference.h"
#include "StyleBuilderState.h"

namespace WebCore {
namespace Style {

void FilterReference::loadExternalDocumentIfNeeded(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    if (cachedSVGDocumentReference)
        return;
    if (!SVGURIReference::isExternalURIReference(url.resolved.string(), *protect(cachedResourceLoader.document())))
        return;
    lazyInitialize(cachedSVGDocumentReference, CachedSVGDocumentReference::create(url));
    cachedSVGDocumentReference->load(cachedResourceLoader, options);
}

// MARK: - Conversion

auto ToCSS<FilterReference>::operator()(const FilterReference& value, const RenderStyle& style) -> CSS::FilterReference
{
    return { .url = toCSS(value.url, style) };
}

auto ToStyle<CSS::FilterReference>::operator()(const CSS::FilterReference& value, const BuilderState& state) -> FilterReference
{
    auto url = toStyle(value.url, state);

    // FIXME: Unify all the fragment accessing/construction.
    auto fragment = url.resolved.string().startsWith('#')
        ? StringView(url.resolved.string()).substring(1).toAtomString()
        : url.resolved.fragmentIdentifier().toAtomString();

    return {
        .url = WTF::move(url),
        .cachedFragment = WTF::move(fragment),
        .cachedSVGDocumentReference = nullptr
    };
}

} // namespace Style
} // namespace WebCore
