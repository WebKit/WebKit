/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGTextChunkBuilder.h"

#include "SVGElement.h"
#include "SVGInlineTextBox.h"
#include "SVGLengthContext.h"
#include "SVGTextFragment.h"

namespace WebCore {

SVGTextChunkBuilder::SVGTextChunkBuilder()
{
}

unsigned SVGTextChunkBuilder::totalCharacters() const
{
    unsigned characters = 0;
    for (const auto& chunk : m_textChunks)
        characters += chunk.totalCharacters();
    return characters;
}

float SVGTextChunkBuilder::totalLength() const
{
    float length = 0;
    for (const auto& chunk : m_textChunks)
        length += chunk.totalLength();
    return length;
}

float SVGTextChunkBuilder::totalAnchorShift() const
{
    float anchorShift = 0;
    for (const auto& chunk : m_textChunks)
        anchorShift += chunk.totalAnchorShift();
    return anchorShift;
}

AffineTransform SVGTextChunkBuilder::transformationForTextBox(InlineIterator::SVGTextBoxIterator textBox) const
{
    auto it = m_textBoxTransformations.find(makeKey(*textBox));
    return it == m_textBoxTransformations.end() ? AffineTransform() : it->value;
}

void SVGTextChunkBuilder::buildTextChunks(const Vector<InlineIterator::SVGTextBoxIterator>& lineLayoutBoxes, const HashSet<InlineIterator::SVGTextBox::Key>& chunkStarts, SVGTextFragmentMap& fragmentMap)
{
    if (lineLayoutBoxes.isEmpty())
        return;

    unsigned limit = lineLayoutBoxes.size();
    unsigned first = limit;

    for (unsigned i = 0; i < limit; ++i) {
        if (!chunkStarts.contains(makeKey(*lineLayoutBoxes[i])))
            continue;

        if (first == limit)
            first = i;
        else {
            ASSERT_WITH_SECURITY_IMPLICATION(first != i);
            m_textChunks.append(SVGTextChunk(lineLayoutBoxes, first, i, fragmentMap));
            first = i;
        }
    }

    if (first != limit)
        m_textChunks.append(SVGTextChunk(lineLayoutBoxes, first, limit, fragmentMap));
}

void SVGTextChunkBuilder::layoutTextChunks(const Vector<InlineIterator::SVGTextBoxIterator>& lineLayoutBoxes, const HashSet<InlineIterator::SVGTextBox::Key>& chunkStarts, SVGTextFragmentMap& fragmentMap)
{
    buildTextChunks(lineLayoutBoxes, chunkStarts, fragmentMap);
    if (m_textChunks.isEmpty())
        return;

    for (const auto& chunk : m_textChunks)
        chunk.layout(m_textBoxTransformations);

    m_textChunks.clear();
}

}
