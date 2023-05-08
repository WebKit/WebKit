/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "RegionContext.h"

#include "Path.h"

namespace WebCore {

void RegionContext::pushTransform(const AffineTransform& transform)
{
    if (m_transformStack.isEmpty())
        m_transformStack.append(transform);
    else
        m_transformStack.append(m_transformStack.last() * transform);
}

void RegionContext::popTransform()
{
    if (m_transformStack.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_transformStack.removeLast();
}

void RegionContext::pushClip(const IntRect& clipRect)
{
    auto transformedClip = m_transformStack.isEmpty() ? clipRect : m_transformStack.last().mapRect(clipRect);

    if (m_clipStack.isEmpty())
        m_clipStack.append(transformedClip);
    else
        m_clipStack.append(intersection(m_clipStack.last(), transformedClip));
}

void RegionContext::pushClip(const Path& path)
{
    // FIXME: Approximate paths better.
    auto pathBounds = enclosingIntRect(path.boundingRect());
    pushClip(pathBounds);
}

void RegionContext::popClip()
{
    if (m_clipStack.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_clipStack.removeLast();
}

} // namespace WebCore
