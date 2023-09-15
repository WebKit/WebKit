/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DisplayList.h"

#include "DecomposedGlyphs.h"
#include "DisplayListResourceHeap.h"
#include "Filter.h"
#include "Font.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

void DisplayList::append(Item&& item)
{
    m_items.append(WTFMove(item));
}

void DisplayList::shrinkToFit()
{
    m_items.shrinkToFit();
}

void DisplayList::clear()
{
    m_items.clear();
    m_resourceHeap.clearAllResources();
}

bool DisplayList::isEmpty() const
{
    return m_items.isEmpty();
}

void DisplayList::cacheImageBuffer(ImageBuffer& imageBuffer)
{
    m_resourceHeap.add(Ref { imageBuffer });
}

void DisplayList::cacheNativeImage(NativeImage& image)
{
    m_resourceHeap.add(Ref { image });
}

void DisplayList::cacheFont(Font& font)
{
    m_resourceHeap.add(Ref { font });
}

void DisplayList::cacheDecomposedGlyphs(DecomposedGlyphs& decomposedGlyphs)
{
    m_resourceHeap.add(Ref { decomposedGlyphs });
}

void DisplayList::cacheGradient(Gradient& gradient)
{
    m_resourceHeap.add(Ref { gradient });
}

void DisplayList::cacheFilter(Filter& filter)
{
    m_resourceHeap.add(Ref { filter });
}

String DisplayList::asText(OptionSet<AsTextFlag> flags) const
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    for (const auto& item : m_items) {
        if (!shouldDumpItem(item, flags))
            continue;

        TextStream::GroupScope group(stream);
        dumpItem(stream, item, flags);
    }
    return stream.release();
}

void DisplayList::dump(TextStream& ts) const
{
    TextStream::GroupScope group(ts);
    ts << "display list";

    for (const auto& item : m_items) {
        TextStream::GroupScope group(ts);
        dumpItem(ts, item, { AsTextFlag::IncludePlatformOperations, AsTextFlag::IncludeResourceIdentifiers });
    }
}

TextStream& operator<<(TextStream& ts, const DisplayList& displayList)
{
    displayList.dump(ts);
    return ts;
}

} // namespace DisplayList
} // namespace WebCore
