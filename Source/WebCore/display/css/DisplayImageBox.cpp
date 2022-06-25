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

#include "config.h"
#include "DisplayImageBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "CachedImage.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Display {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ImageBox);

ImageBox::ImageBox(Tree& tree, UnadjustedAbsoluteFloatRect borderBox, Style&& displayStyle, CachedResourceHandle<CachedImage>&& image)
    : ReplacedBox(tree, borderBox, WTFMove(displayStyle), { TypeFlags::ImageBox })
    , m_cachedImage(WTFMove(image))
{
    if (m_cachedImage)
        m_cachedImage->addClient(*this);
}

ImageBox::~ImageBox()
{
    if (m_cachedImage)
        m_cachedImage->removeClient(*this);
}

Image* ImageBox::image() const
{
    return m_cachedImage ? m_cachedImage->image() : nullptr;
}

void ImageBox::imageChanged(CachedImage*, const IntRect*)
{
    setNeedsDisplay(); // FIXME: Compute correct rect.
}

const char* ImageBox::boxName() const
{
    return "image box";
}

String ImageBox::debugDescription() const
{
    TextStream stream;
    stream << boxName() << " " << absoluteBorderBoxRect() << " (" << this << ") replaced content rect: " << replacedContentRect() << " image: " << m_cachedImage.get();
    return stream.release();
}

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
