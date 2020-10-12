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
#include "LayoutIntegrationLineIterator.h"

#include "LayoutIntegrationLineLayout.h"
#include "LayoutIntegrationRunIterator.h"

namespace WebCore {
namespace LayoutIntegration {

LineIterator::LineIterator(PathLine::PathVariant&& pathVariant)
    : m_line(WTFMove(pathVariant))
{
}

bool LineIterator::atEnd() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

LineIterator LineIterator::next() const
{
    return LineIterator(*this).traverseNext();
}

LineIterator LineIterator::previous() const
{
    return LineIterator(*this).traversePrevious();
}

LineIterator& LineIterator::traverseNext()
{
    WTF::switchOn(m_line.m_pathVariant, [](auto& path) {
        return path.traverseNext();
    });
    return *this;
}

LineIterator& LineIterator::traversePrevious()
{
    WTF::switchOn(m_line.m_pathVariant, [](auto& path) {
        return path.traversePrevious();
    });
    return *this;
}

bool LineIterator::operator==(const LineIterator& other) const
{
    if (m_line.m_pathVariant.index() != other.m_line.m_pathVariant.index())
        return false;

    return WTF::switchOn(m_line.m_pathVariant, [&](const auto& path) {
        return path == WTF::get<std::decay_t<decltype(path)>>(other.m_line.m_pathVariant);
    });
}

LineRunIterator LineIterator::firstRun() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) -> RunIterator {
        return { path.firstRun() };
    });
}

LineRunIterator LineIterator::lastRun() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) -> RunIterator {
        return { path.lastRun() };
    });
}

LineRunIterator LineIterator::logicalStartRunWithNode() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) -> RunIterator {
        return { path.logicalStartRunWithNode() };
    });
}

LineRunIterator LineIterator::logicalEndRunWithNode() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) -> RunIterator {
        return { path.logicalEndRunWithNode() };
    });
}


}
}

