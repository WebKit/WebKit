/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(CSS_SHAPES)
#include "ShapeValue.h"

#include "CachedImage.h"

namespace WebCore {

bool ShapeValue::isImageValid() const
{
    if (!image())
        return false;
    if (image()->isCachedImage())
        return image()->cachedImage() && image()->cachedImage()->hasImage();
    return image()->isGeneratedImage();
}

template <typename T>
bool pointersOrValuesEqual(T p1, T p2)
{
    if (p1 == p2)
        return true;
    
    if (!p1 || !p2)
        return false;
    
    return *p1 == *p2;
}

bool ShapeValue::operator==(const ShapeValue& other) const
{
    if (m_type != other.m_type || m_cssBox != other.m_cssBox)
        return false;

    return pointersOrValuesEqual(m_shape.get(), other.m_shape.get())
        && pointersOrValuesEqual(m_image.get(), other.m_image.get());
}


} // namespace WebCore

#endif // ENABLE(CSS_SHAPES)
