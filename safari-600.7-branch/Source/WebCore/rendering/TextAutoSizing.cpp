/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "TextAutoSizing.h"

#if ENABLE(IOS_TEXT_AUTOSIZING)

#include "CSSFontSelector.h"
#include "Document.h"
#include "RenderListMarker.h"
#include "RenderText.h"
#include "StyleResolver.h"

namespace WebCore {

static PassRefPtr<RenderStyle> cloneRenderStyleWithState(const RenderStyle& currentStyle)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::clone(&currentStyle);
    if (currentStyle.lastChildState())
        newStyle->setLastChildState();
    if (currentStyle.firstChildState())
        newStyle->setFirstChildState();
    return newStyle.release();
}

TextAutoSizingKey::TextAutoSizingKey()
    : m_style(0)
    , m_doc(0)
{
}

TextAutoSizingKey::TextAutoSizingKey(RenderStyle* style, Document* doc)
    : m_style(style)
    , m_doc(doc)
{
    ref();
}

TextAutoSizingKey::TextAutoSizingKey(const TextAutoSizingKey& other)
    : m_style(other.m_style)
    , m_doc(other.m_doc)
{
    ref();
}

TextAutoSizingKey::~TextAutoSizingKey()
{
    deref();
}

TextAutoSizingKey& TextAutoSizingKey::operator=(const TextAutoSizingKey& other)
{
    other.ref();
    deref();
    m_style = other.m_style;
    m_doc = other.m_doc;
    return *this;
}

void TextAutoSizingKey::ref() const
{
    if (isValidStyle())
        m_style->ref();
}

void TextAutoSizingKey::deref() const
{
    if (isValidStyle() && isValidDoc())
        m_style->deref();
}

int TextAutoSizingValue::numNodes() const
{
    return m_autoSizedNodes.size();
}

void TextAutoSizingValue::addNode(Node* node, float size)
{
    ASSERT(node);
    RenderText* renderText = toRenderText(node->renderer());
    renderText->setCandidateComputedTextSize(size);
    m_autoSizedNodes.add(node);
}

#define MAX_SCALE_INCREASE 1.7f

bool TextAutoSizingValue::adjustNodeSizes()
{
    bool objectsRemoved = false;
    
    // Remove stale nodes.  Nodes may have had their renderers detached.  We'll
    // also need to remove the style from the documents m_textAutoSizedNodes
    // collection.  Return true indicates we need to do that removal.
    Vector<RefPtr<Node> > nodesForRemoval;
    HashSet<RefPtr<Node> >::iterator end = m_autoSizedNodes.end();
    for (HashSet<RefPtr<Node> >::iterator i = m_autoSizedNodes.begin(); i != end; ++i) {
        RefPtr<Node> autoSizingNode = *i;
        RenderText* text = static_cast<RenderText*>(autoSizingNode->renderer());
        if (!text || !text->style().textSizeAdjust().isAuto() || !text->candidateComputedTextSize()) {
            // remove node.
            nodesForRemoval.append(autoSizingNode);
            objectsRemoved = true;
        }
    }
    
    unsigned count = nodesForRemoval.size();
    for (unsigned i = 0; i < count; i++)
        m_autoSizedNodes.remove(nodesForRemoval[i]);
    
    // If we only have one piece of text with the style on the page don't
    // adjust it's size.
    if (m_autoSizedNodes.size() <= 1)
        return objectsRemoved;
    
    // Compute average size
    float cumulativeSize = 0;
    end = m_autoSizedNodes.end();
    for (HashSet<RefPtr<Node> >::iterator i = m_autoSizedNodes.begin(); i != end; ++i) {
        RefPtr<Node> autoSizingNode = *i;
        RenderText* renderText = static_cast<RenderText*>(autoSizingNode->renderer());
        cumulativeSize += renderText->candidateComputedTextSize();
    }
    
    float averageSize = roundf(cumulativeSize / m_autoSizedNodes.size());
    
    // Adjust sizes
    bool firstPass = true;
    end = m_autoSizedNodes.end();
    for (HashSet<RefPtr<Node> >::iterator i = m_autoSizedNodes.begin(); i != end; ++i) {
        const RefPtr<Node>& autoSizingNode = *i;
        RenderText* text = static_cast<RenderText*>(autoSizingNode->renderer());
        if (text && text->style().fontDescription().computedSize() != averageSize) {
            float specifiedSize = text->style().fontDescription().specifiedSize();
            float scaleChange = averageSize / specifiedSize;
            if (scaleChange > MAX_SCALE_INCREASE && firstPass) {
                firstPass = false;
                averageSize = roundf(specifiedSize * MAX_SCALE_INCREASE);
                scaleChange = averageSize / specifiedSize;
            }
            
            RefPtr<RenderStyle> style = cloneRenderStyleWithState(text->style());
            FontDescription fontDescription = style->fontDescription();
            fontDescription.setComputedSize(averageSize);
            style->setFontDescription(fontDescription);
            style->font().update(autoSizingNode->document().ensureStyleResolver().fontSelector());
            text->parent()->setStyle(style.releaseNonNull());
            
            RenderElement* parentRenderer = text->parent();
            if (parentRenderer->isAnonymousBlock())
                parentRenderer = parentRenderer->parent();
            
            // If we have a list we should resize ListMarkers separately.
            RenderObject* listMarkerRenderer = parentRenderer->firstChild();
            if (listMarkerRenderer->isListMarker()) {
                RefPtr<RenderStyle> style = cloneRenderStyleWithState(listMarkerRenderer->style());
                style->setFontDescription(fontDescription);
                style->font().update(autoSizingNode->document().ensureStyleResolver().fontSelector());
                toRenderListMarker(*listMarkerRenderer).setStyle(style.releaseNonNull());
            }
            
            // Resize the line height of the parent.
            const RenderStyle& parentStyle = parentRenderer->style();
            Length lineHeightLength = parentStyle.specifiedLineHeight();
            
            int specifiedLineHeight = 0;
            if (lineHeightLength.isPercent())
                specifiedLineHeight = minimumValueForLength(lineHeightLength, fontDescription.specifiedSize());
            else
                specifiedLineHeight = lineHeightLength.value();
            
            int lineHeight = specifiedLineHeight * scaleChange;
            if (!lineHeightLength.isFixed() || lineHeightLength.value() != lineHeight) {
                RefPtr<RenderStyle> newParentStyle = cloneRenderStyleWithState(parentStyle);
                newParentStyle->setLineHeight(Length(lineHeight, Fixed));
                newParentStyle->setSpecifiedLineHeight(lineHeightLength);
                newParentStyle->setFontDescription(fontDescription);
                newParentStyle->font().update(autoSizingNode->document().ensureStyleResolver().fontSelector());
                parentRenderer->setStyle(newParentStyle.releaseNonNull());
            }
        }
    }
    
    return objectsRemoved;
}

void TextAutoSizingValue::reset()
{
    HashSet<RefPtr<Node> >::iterator end = m_autoSizedNodes.end();
    for (HashSet<RefPtr<Node> >::iterator i = m_autoSizedNodes.begin(); i != end; ++i) {
        const RefPtr<Node>& autoSizingNode = *i;
        RenderText* text = static_cast<RenderText*>(autoSizingNode->renderer());
        if (!text)
            continue;
        // Reset the font size back to the original specified size
        FontDescription fontDescription = text->style().fontDescription();
        float originalSize = fontDescription.specifiedSize();
        if (fontDescription.computedSize() != originalSize) {
            fontDescription.setComputedSize(originalSize);
            RefPtr<RenderStyle> style = cloneRenderStyleWithState(text->style());
            style->setFontDescription(fontDescription);
            style->font().update(autoSizingNode->document().ensureStyleResolver().fontSelector());
            text->parent()->setStyle(style.releaseNonNull());
        }
        // Reset the line height of the parent.
        RenderElement* parentRenderer = text->parent();
        if (!parentRenderer)
            continue;
        
        if (parentRenderer->isAnonymousBlock())
            parentRenderer = parentRenderer->parent();
        
        const RenderStyle& parentStyle = parentRenderer->style();
        Length originalLineHeight = parentStyle.specifiedLineHeight();
        if (originalLineHeight != parentStyle.lineHeight()) {
            RefPtr<RenderStyle> newParentStyle = cloneRenderStyleWithState(parentStyle);
            newParentStyle->setLineHeight(originalLineHeight);
            newParentStyle->setFontDescription(fontDescription);
            newParentStyle->font().update(autoSizingNode->document().ensureStyleResolver().fontSelector());
            parentRenderer->setStyle(newParentStyle.releaseNonNull());
        }
    }
}

} // namespace WebCore

#endif // ENABLE(IOS_TEXT_AUTOSIZING)
