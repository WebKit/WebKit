/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "SkColor.h"
#include "SkImageFilter.h"
#include "SkScalar.h"

namespace WebCore {

class DropShadowImageFilter : public SkImageFilter {
public:
    DropShadowImageFilter(SkScalar dx, SkScalar dy, SkScalar sigma, SkColor, SkImageFilter* input = 0);
    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(DropShadowImageFilter)

protected:
    explicit DropShadowImageFilter(SkFlattenableReadBuffer&);
    virtual void flatten(SkFlattenableWriteBuffer&) const OVERRIDE;
    virtual bool onFilterImage(Proxy*, const SkBitmap& source, const SkMatrix&, SkBitmap* result, SkIPoint* loc) OVERRIDE;

private:
    SkScalar m_dx, m_dy, m_sigma;
    SkColor m_color;
};

};
