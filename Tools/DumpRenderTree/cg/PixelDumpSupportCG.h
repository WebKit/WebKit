/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2007 Eric Seidel <eric@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>

typedef struct CGContext* CGContextRef;

using UniqueBitmapBuffer = std::unique_ptr<void, decltype(std::free) *>;

class BitmapContext : public RefCounted<BitmapContext> {
public:
    static Ref<BitmapContext> createByAdoptingBitmapAndContext(UniqueBitmapBuffer&& buffer, RetainPtr<CGContextRef>&& context)
    {
        return adoptRef(*new BitmapContext(WTFMove(buffer), WTFMove(context)));
    }

    ~BitmapContext() = default;

    CGContextRef cgContext() const { return m_context.get(); }
    
    double scaleFactor() const { return m_scaleFactor; }
    void setScaleFactor(double scaleFactor) { m_scaleFactor = scaleFactor; }

private:

    BitmapContext(UniqueBitmapBuffer&& buffer, RetainPtr<CGContextRef>&& context)
        : m_buffer(WTFMove(buffer))
        , m_context(WTFMove(context))
    {
    }

    UniqueBitmapBuffer m_buffer;
    RetainPtr<CGContextRef> m_context;
    double m_scaleFactor { 1.0 };
};

RefPtr<BitmapContext> createBitmapContext(size_t pixelsWide, size_t pixelsHigh, size_t& rowBytes);
RefPtr<BitmapContext> createBitmapContextFromWebView(bool onscreen, bool incrementalRepaint, bool sweepHorizontally, bool drawSelectionRect);
