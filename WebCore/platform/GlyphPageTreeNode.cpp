/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "config.h"
#include "GlyphPageTreeNode.h"

#include "CharacterNames.h"
#include "FontData.h"
#include <wtf/unicode/Unicode.h>

namespace WebCore {

GlyphPageTreeNode* GlyphPageTreeNode::getRoot(unsigned pageNumber)
{
    static HashMap<int, GlyphPageTreeNode*> roots;
    static GlyphPageTreeNode* pageZeroRoot;
    GlyphPageTreeNode* node = pageNumber ? roots.get(pageNumber) : pageZeroRoot;
    if (!node) {
        node = new GlyphPageTreeNode;
#ifndef NDEBUG
        node->m_pageNumber = pageNumber;
#endif
        if (pageNumber)
            roots.set(pageNumber, node);
        else
            pageZeroRoot = node;
    }
    return node;
}

void GlyphPageTreeNode::initializePage(const FontData* fontData, unsigned pageNumber)
{
    ASSERT(!m_page);

    GlyphPage* parentPage = m_parent->m_page;

    if (fontData) {
        if (m_level == 1) {
            // Children of the root hold pure pages.
            unsigned start = pageNumber * GlyphPage::size;
            UChar buffer[GlyphPage::size * 2 + 2];
            unsigned bufferLength;
            unsigned i;

            // Fill in a buffer with the entire "page" of characters that we want to look up glyphs for.
            if (start < 0x10000) {
                bufferLength = GlyphPage::size;
                for (i = 0; i < GlyphPage::size; i++)
                    buffer[i] = start + i;

                if (start == 0) {
                    // Control characters must not render at all.
                    for (i = 0; i < 0x20; ++i)
                        buffer[i] = zeroWidthSpace;
                    for (i = 0x7F; i < 0xA0; i++)
                        buffer[i] = zeroWidthSpace;

                    // \n, \t, and nonbreaking space must render as a space.
                    buffer[(int)'\n'] = ' ';
                    buffer[(int)'\t'] = ' ';
                    buffer[noBreakSpace] = ' ';
                }
            } else {
                bufferLength = GlyphPage::size * 2;
                for (i = 0; i < GlyphPage::size; i++) {
                    int c = i + start;
                    buffer[i * 2] = U16_LEAD(c);
                    buffer[i * 2 + 1] = U16_TRAIL(c);
                }
            }
            
            m_page = new GlyphPage(this);

            // Now that we have a buffer full of characters, we want to get back an array
            // of glyph indices.  This part involves calling into the platform-specific 
            // routine of our glyph map for actually filling in the page with the glyphs.
            // Success is not guaranteed. For example, Times fails to fill page 260, giving glyph data
            // for only 128 out of 256 characters.
            bool haveGlyphs = m_page->fill(buffer, bufferLength, fontData);

            if (!haveGlyphs) {
                delete m_page;
                m_page = 0;
            }
        } else if (parentPage && parentPage->owner() != m_parent)
            // Ensures that the overlay page is owned by the child of the owner of the
            // parent page, and therefore will be shared.
            m_page = parentPage->owner()->getChild(fontData, pageNumber)->page();
        else {
            // Get the pure page for the fallback font.
            GlyphPage* fallbackPage = getRootChild(fontData, pageNumber)->page();
            if (!parentPage)
                m_page = fallbackPage;
            else if (!fallbackPage) {
                m_page = parentPage;
            } else {
                m_page = new GlyphPage(this);

                // Overlay the parent page on the fallback page. Check if the fallback font
                // has added anything.
                bool newGlyphs = false;
                for (unsigned i = 0; i < GlyphPage::size; i++) {
                    if (parentPage->m_glyphs[i].glyph)
                        m_page->m_glyphs[i] = parentPage->m_glyphs[i];
                    else  if (fallbackPage->m_glyphs[i].glyph) {
                        m_page->m_glyphs[i] = fallbackPage->m_glyphs[i];
                        newGlyphs = true;
                    } else {
                        const GlyphData data = { 0, 0 };
                        m_page->m_glyphs[i] = data;
                    }
                }

                if (!newGlyphs) {
                    delete m_page;
                    m_page = parentPage;
                }
            }
        }
    } else {
        m_page = new GlyphPage(this);
        // System fallback. Initialized with the parent's page here, as individual
        // entries may use different fonts depending on character.
        if (parentPage)
            memcpy(m_page->m_glyphs, parentPage->m_glyphs, GlyphPage::size * sizeof(m_page->m_glyphs[0]));
        else {
            const GlyphData data = { 0, 0 };
            for (unsigned i = 0; i < GlyphPage::size; i++)
                m_page->m_glyphs[i] = data;
        }
    }
}

GlyphPageTreeNode* GlyphPageTreeNode::getChild(const FontData* fontData, unsigned pageNumber)
{
    ASSERT(fontData || !m_isSystemFallback);
    ASSERT(pageNumber == m_pageNumber);

    GlyphPageTreeNode* child = fontData ? m_children.get(fontData) : m_systemFallbackChild;
    if (!child) {
        child = new GlyphPageTreeNode;
        child->m_parent = this;
        child->m_level = m_level + 1;
#ifndef NDEBUG
        child->m_pageNumber = m_pageNumber;
#endif
        if (fontData)
            m_children.set(fontData, child);
        else {
            m_systemFallbackChild = child;
            child->m_isSystemFallback = true;
        }
        child->initializePage(fontData, pageNumber);
    }
    return child;
}

}
