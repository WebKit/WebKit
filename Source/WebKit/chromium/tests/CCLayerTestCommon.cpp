/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CCLayerTestCommon.h"
#include "CCDrawQuad.h"

#include <gtest/gtest.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace CCLayerTestCommon {

// Align with expected and actual output
const char* quadString = "    Quad: ";

void verifyQuadsExactlyCoverRect(const CCQuadList& quads, const IntRect& rect)
{
    Region remaining(rect);

    for (size_t i = 0; i < quads.size(); ++i) {
        CCDrawQuad* quad = quads[i].get();
        IntRect quadRect = quad->quadRect();

        EXPECT_TRUE(rect.contains(quadRect)) << quadString << i;
        EXPECT_TRUE(remaining.contains(quadRect)) << quadString << i;
        remaining.subtract(Region(quadRect));
    }

    EXPECT_TRUE(remaining.isEmpty());
}

} // namespace
