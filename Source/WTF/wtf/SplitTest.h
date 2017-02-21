/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <limits>
#include <wtf/ExportMacros.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>

namespace WTF {

class SplitTest {
    WTF_MAKE_FAST_ALLOCATED;
    friend class WTF::NeverDestroyed<SplitTest>;
public:
    WTF_EXPORT static SplitTest& singleton();

    WTF_EXPORT void initializeSingleton(uint64_t lo, uint64_t hi);

    // FIXME: Query with a per-experiment unique ID to determine which experiment
    // bucket the user should fall into. It's important to "salt" with unique
    // experiment IDs so that users don't all fall in the same set of experiments
    // when there are multiple experiments.
    // Also add a version which returns a number in a provided [min, max] range,
    // which is more useful for bucketed experiments.
    // https://bugs.webkit.org/show_bug.cgi?id=168467
    WTF_EXPORT std::optional<bool> enableBooleanExperiment();

private:
    uint64_t m_lo;
    uint64_t m_hi;
    bool m_initialized { false };
};

} // namespace WTF

using WTF::SplitTest;
