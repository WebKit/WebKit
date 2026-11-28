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

#include "SetHTMLUnsafeOptions.h"
#include "TrustedHTMLParserOptions.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class ScriptExecutionContext;
class TrustedHTML;
template<typename> class ExceptionOr;

using UnsafeHTMLParserOptions = Variant<Ref<TrustedHTMLParserOptions>, SetHTMLUnsafeOptions>;

// https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-create-a-parser-options-callback-argument
ExceptionOr<SetHTMLUnsafeOptions> createParserOptionsCallbackArgument(SetHTMLUnsafeOptions&&);

ExceptionOr<Ref<TrustedHTMLParserOptions>> trustedHTMLParserOptionsFromPolicyValue(std::optional<SetHTMLUnsafeOptions>&&);

// https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-compliant-parser-options-algorithm
ExceptionOr<UnsafeHTMLParserOptions> trustedTypeCompliantParserOptions(ScriptExecutionContext&, UnsafeHTMLParserOptions&&, bool throwIfMissing, const String& sink, bool optionsFromAuthor = true);

struct TrustedHTMLInput {
    String html;
    UnsafeHTMLParserOptions options;
};

// https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-compliant-input-algorithm
ExceptionOr<TrustedHTMLInput> trustedTypeCompliantInput(ScriptExecutionContext&, Variant<Ref<TrustedHTML>, String>&& input, UnsafeHTMLParserOptions&&, const String& sink, bool isHTMLDocument = true, bool optionsFromAuthor = true);

struct ResolvedUnsafeParserOptions {
    RefPtr<ParserSanitizerConfiguration> sanitizer;
    bool runScripts { false };
};

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#get-a-sanitizer-config-from-options
ExceptionOr<ResolvedUnsafeParserOptions> resolveUnsafeParserOptions(UnsafeHTMLParserOptions&&, bool sanitizerEnabled);

// "Get trusted type compliant input" followed by resolving the resulting parser options.
ExceptionOr<ResolvedUnsafeParserOptions> resolveUnsafeHTMLInput(Document&, Variant<Ref<TrustedHTML>, String>&& input, UnsafeHTMLParserOptions&&, const String& sink, String& markup, bool optionsFromAuthor = true, bool parsesAsHTML = true);

} // namespace WebCore
