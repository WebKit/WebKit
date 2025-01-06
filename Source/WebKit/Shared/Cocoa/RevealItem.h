/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(REVEAL)

#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

OBJC_CLASS RVItem;

typedef struct _NSRange NSRange;
typedef unsigned long NSUInteger;

namespace WebKit {

struct RevealItemRange {
    RevealItemRange() = default;
    RevealItemRange(NSRange);
    RevealItemRange(NSUInteger loc, NSUInteger len)
        : location(loc)
        , length(len)
    {
    }

    NSUInteger location { 0 };
    NSUInteger length { 0 };
};

class RevealItem {
public:
    RevealItem() = default;
    RevealItem(const String& text, RevealItemRange selectedRange);

    const String& text() const { return m_text; }
    const RevealItemRange& selectedRange() const { return m_selectedRange; }
    NSRange highlightRange() const;

    RVItem *item() const;

private:
    String m_text;
    RevealItemRange m_selectedRange;

    mutable RetainPtr<RVItem> m_item;
};

}

#endif // ENABLE(REVEAL)
