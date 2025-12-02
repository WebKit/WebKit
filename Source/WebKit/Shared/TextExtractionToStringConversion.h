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

#include <wtf/CompletionHandler.h>
#include <wtf/NativePromise.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace TextExtraction {

struct Item;

} // namespace TextExtraction

struct NodeIdentifierType;
using NodeIdentifier = ObjectIdentifier<NodeIdentifierType>;

} // namespace WebCore

namespace WebKit {

using TextExtractionVersion = unsigned;

enum class TextExtractionOptionFlag : uint8_t {
    IncludeURLs     = 1 << 0,
    IncludeRects    = 1 << 1,
    OnlyIncludeText = 1 << 2,
};

enum class TextExtractionOutputFormat : uint8_t {
    TextTree,
    HTMLMarkup,
    Markdown
};

using TextExtractionOptionFlags = OptionSet<TextExtractionOptionFlag>;
using TextExtractionFilterPromise = NativePromise<String, void>;
using TextExtractionFilterCallback = Function<Ref<TextExtractionFilterPromise>(const String&, std::optional<WebCore::NodeIdentifier>&&)>;

struct TextExtractionOptions {
    TextExtractionOptions(TextExtractionOptions&& other)
        : filterCallbacks(WTFMove(other.filterCallbacks))
        , nativeMenuItems(WTFMove(other.nativeMenuItems))
        , replacementStrings(WTFMove(other.replacementStrings))
        , version(other.version)
        , flags(other.flags)
        , outputFormat(other.outputFormat)
    {
    }

    TextExtractionOptions(Vector<TextExtractionFilterCallback>&& filters, Vector<String>&& items, HashMap<String, String>&& replacementStrings, std::optional<TextExtractionVersion> version, TextExtractionOptionFlags flags, TextExtractionOutputFormat outputFormat)
        : filterCallbacks(WTFMove(filters))
        , nativeMenuItems(WTFMove(items))
        , replacementStrings(WTFMove(replacementStrings))
        , version(version)
        , flags(flags)
        , outputFormat(outputFormat)
    {
    }

    Vector<TextExtractionFilterCallback> filterCallbacks;
    Vector<String> nativeMenuItems;
    HashMap<String, String> replacementStrings;
    std::optional<TextExtractionVersion> version;
    TextExtractionOptionFlags flags;
    TextExtractionOutputFormat outputFormat { TextExtractionOutputFormat::TextTree };
};

struct TextExtractionResult {
    String textContent;
    bool filteredOutAnyText { false };
};

void convertToText(WebCore::TextExtraction::Item&&, TextExtractionOptions&&, CompletionHandler<void(TextExtractionResult&&)>&&);

} // namespace WebKit
