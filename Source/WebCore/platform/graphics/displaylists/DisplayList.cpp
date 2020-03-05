/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "DisplayListItems.h"
#include "Logging.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

#if !defined(NDEBUG) || !LOG_DISABLED
WTF::CString DisplayList::description() const
{
    TextStream ts;
    ts << this;
    return ts.release().utf8();
}

void DisplayList::dump() const
{
    fprintf(stderr, "%s", description().data());
}
#endif

void DisplayList::clear()
{
    m_list.clear();
}

void DisplayList::removeItemsFromIndex(size_t index)
{
    m_list.resize(index);
}

bool DisplayList::shouldDumpForFlags(AsTextFlags flags, const Item& item)
{
    switch (item.type()) {
    case ItemType::SetState:
        if (!(flags & AsTextFlag::IncludesPlatformOperations)) {
            const auto& stateItem = downcast<SetState>(item);
            // FIXME: for now, only drop the item if the only state-change flags are platform-specific.
            if (stateItem.state().m_changeFlags == GraphicsContextState::ShouldSubpixelQuantizeFontsChange)
                return false;

            if (stateItem.state().m_changeFlags == GraphicsContextState::ShouldSubpixelQuantizeFontsChange)
                return false;
        }
        break;
#if USE(CG)
    case ItemType::ApplyFillPattern:
    case ItemType::ApplyStrokePattern:
        if (!(flags & AsTextFlag::IncludesPlatformOperations))
            return false;
        break;
#endif
    default:
        break;
    }
    return true;
}

String DisplayList::asText(AsTextFlags flags) const
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    for (auto& item : m_list) {
        if (!shouldDumpForFlags(flags, item))
            continue;
        stream << item;
    }
    return stream.release();
}

void DisplayList::dump(TextStream& ts) const
{
    TextStream::GroupScope group(ts);
    ts << "display list";

    size_t numItems = m_list.size();
    for (size_t i = 0; i < numItems; ++i) {
        TextStream::GroupScope scope(ts);
        ts << i << " " << m_list[i].get();
    }
    ts.startGroup();
    ts << "size in bytes: " << sizeInBytes();
    ts.endGroup();
}

size_t DisplayList::sizeInBytes() const
{
    size_t result = 0;
    for (auto& ref : m_list)
        result += Item::sizeInBytes(ref);

    return result;
}

} // namespace DisplayList

TextStream& operator<<(TextStream& ts, const DisplayList::DisplayList& displayList)
{
    displayList.dump(ts);
    return ts;
}

} // namespace WebCore
