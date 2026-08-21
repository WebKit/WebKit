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

#include "config.h"

#include "Helpers/Test.h"
#include <WebCore/SharedStringHash.h>
#include <wtf/URL.h>
#include <wtf/text/AtomString.h>

namespace TestWebKitAPI {

using namespace WebCore;

// computeVisitedLinkHash() must produce exactly the hash of the fully-resolved,
// canonicalized URL — i.e. the same value the store side computes via
// computeSharedStringHash(URL(base, relative).string()). This locks the
// non-allocating parseAndConsume() path to the real URL canonicalizer.
static void expectParity(ASCIILiteral baseString, ASCIILiteral relative)
{
    URL base { String { baseString } };
    AtomString relativeAtom { String { relative } };

    SharedStringHash viaVisitedLinkHash = computeVisitedLinkHash(base, relativeAtom);
    SharedStringHash viaResolvedURL = computeSharedStringHash(URL(base, String { relative }).string());

    EXPECT_EQ(viaVisitedLinkHash, viaResolvedURL) << "base=" << baseString.characters() << " relative=" << relative.characters();
}

TEST(SharedStringHash, VisitedLinkHashParity)
{
    // Already-absolute, already-canonical (no syntax violation, buffer untouched).
    expectParity("https://example.com/"_s, "https://other.example/path?q=1#frag"_s);

    // Relative path resolution (syntax violation -> canonical buffer path).
    expectParity("https://example.com/dir/page.html"_s, "foo.html"_s);
    expectParity("https://example.com/dir/page.html"_s, "../up.html"_s);
    expectParity("https://example.com/a/b/c"_s, "./x/./y/../z"_s);

    // Query-only and fragment-only relatives.
    expectParity("https://example.com/dir/page.html?old=1"_s, "?new=2"_s);
    expectParity("https://example.com/dir/page.html"_s, "#frag"_s);

    // Protocol-relative.
    expectParity("https://example.com/"_s, "//other.example/x"_s);

    // needsTrailingSlash: absolute URL with empty path.
    expectParity("https://example.com/x"_s, "https://example.com"_s);

    // Previously-broken cases where the shadow resolver diverged from URL():
    // "://" inside a relative reference, and mixed schemes.
    expectParity("https://example.com/dir/"_s, "a://b/c"_s);
    expectParity("https://example.com/dir/"_s, "path?x=a://b"_s);
    expectParity("https://example.com/dir/"_s, "notascheme:relative"_s);

    // Non-special scheme with an authority-less "//" path (dot-slash fixup path).
    expectParity("data:text/plain,base"_s, "//foo/bar"_s);

    // Uppercase host / percent-encoding (syntax violations that rewrite the buffer).
    expectParity("https://example.com/"_s, "HTTPS://EXAMPLE.COM/PATH"_s);
    expectParity("https://example.com/dir/"_s, "a b c.html"_s);
}

} // namespace TestWebKitAPI
