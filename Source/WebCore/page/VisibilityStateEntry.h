/*
 * Copyright (C) 2026 Helmut Januschka <helmut@januschka.com>
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

#include "DOMHighResTimeStamp.h"
#include "PerformanceEntry.h"

namespace WebCore {

// https://html.spec.whatwg.org/multipage/interaction.html#the-visibilitystateentry-interface
class VisibilityStateEntry final : public PerformanceEntry {
public:
    static Ref<VisibilityStateEntry> create(const String& name, DOMHighResTimeStamp startTime)
    {
        return adoptRef(*new VisibilityStateEntry(name, startTime));
    }

private:
    VisibilityStateEntry(const String& name, DOMHighResTimeStamp startTime)
        : PerformanceEntry(name, startTime, startTime)
    {
    }

    ASCIILiteral entryType() const final { return "visibility-state"_s; }
    Type performanceEntryType() const final { return Type::VisibilityState; }

    ~VisibilityStateEntry() = default;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_PERFORMANCE_ENTRY(VisibilityStateEntry, VisibilityState);
