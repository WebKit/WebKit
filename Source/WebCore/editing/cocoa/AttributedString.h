/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import <wtf/RetainPtr.h>
#import <wtf/URL.h>
#import <wtf/Vector.h>

OBJC_CLASS NSAttributedString;
OBJC_CLASS NSParagraphStyle;
#if PLATFORM(MAC)
OBJC_CLASS NSColor;
OBJC_CLASS NSFont;
OBJC_CLASS NSTextAttachment;
#else
OBJC_CLASS UIColor;
OBJC_CLASS UIFont;
#endif

namespace WebCore {

struct AttributedString {
    struct Range {
        size_t location { 0 };
        size_t length { 0 };
    };
    struct AttributeValue {
        std::variant<double, String, URL, RetainPtr<NSParagraphStyle>,
#if PLATFORM(MAC)
            RetainPtr<NSFont>, RetainPtr<NSColor>, RetainPtr<NSTextAttachment>
#else
            RetainPtr<UIFont>, RetainPtr<UIColor>
#endif
        > value;
    };

    String string;
    Vector<std::pair<Range, HashMap<String, AttributeValue>>> attributes;
    HashMap<String, AttributeValue> documentAttributes;

    WEBCORE_EXPORT static AttributedString fromNSAttributedStringAndDocumentAttributes(RetainPtr<NSAttributedString>&&, RetainPtr<NSDictionary>&& documentAttributes);
    WEBCORE_EXPORT static bool rangesAreSafe(const String&, const Vector<std::pair<Range, HashMap<String, AttributeValue>>>&);
    WEBCORE_EXPORT RetainPtr<NSDictionary> documentAttributesAsNSDictionary() const;
    WEBCORE_EXPORT RetainPtr<NSAttributedString> nsAttributedString() const;
};

} // namespace WebCore
