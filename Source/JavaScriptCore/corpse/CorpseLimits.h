/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#include <JavaScriptCore/CorpsePlatform.h>

#if ENABLE(MYA)

#include <stddef.h>
#include <stdint.h>
#include <wtf/StdLibExtras.h>

namespace JSC {
namespace Corpse {

// Sizes and counts read out of a corpse are used to bound loops and to size
// allocations, so they are checked against these limits first. Each one is a
// sanity check on a single value: it says the struct we read was not what we
// thought it was, in which case the addresses in it are not worth chasing. They
// are not a bound on the work a lookup can do, because the per-image limits
// multiply by the image count. maxTotalBytesRead below is that bound.
//
// The values sit above what was empirically measured: across every Mach-O image
// installed on a sample system the largest load commands were 7.4 KB and the
// largest exports trie 2.1 MB, and a process that dlopens every framework on the
// system reaches about 2,800 images.
constexpr size_t maxLoadCommandsSize = 128 * KB; // About 17× the measured maximum.
constexpr size_t maxExportsTrieSize = 16 * MB; // About 8× the measured maximum.
constexpr uint32_t maxImageCount = 16 * 1024; // About 6× the measured maximum.
constexpr size_t maxPathLength = 4 * KB; // PATH_MAX on Darwin and on Linux.

// A lookup that finds nothing will read every image's load commands and exports
// trie, which measured 101 MB for the ~2,800 image process above and 0.4 MB for
// a small one. This caps the total for one lookup, so a corpse claiming many
// large images cannot turn a single symbol lookup into unbounded copying.
constexpr size_t maxTotalBytesRead = 256 * MB; // About 2.5× the measured maximum.

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA)
