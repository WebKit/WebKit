 /*
 * Copyright (C) 2014 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef SelectionSubtreeRoot_h
#define SelectionSubtreeRoot_h

#include "RenderObject.h"
#include "RenderSelectionInfo.h"

namespace WebCore {

class Document;

class SelectionSubtreeRoot {
public:
    
    typedef HashMap<RenderObject*, std::unique_ptr<RenderSelectionInfo>> SelectedObjectMap;
    typedef HashMap<const RenderBlock*, std::unique_ptr<RenderBlockSelectionInfo>> SelectedBlockMap;

    struct OldSelectionData {
        OldSelectionData()
        {
        }

        Optional<unsigned> selectionStartPos;
        Optional<unsigned> selectionEndPos;
        SelectedObjectMap selectedObjects;
        SelectedBlockMap selectedBlocks;
    };

    class SelectionSubtreeData {
    public:
        SelectionSubtreeData() = default;

        SelectionSubtreeData(RenderObject* selectionStart, Optional<unsigned> selectionStartPos, RenderObject* selectionEnd, Optional<unsigned> selectionEndPos)
            : m_selectionStart(selectionStart)
            , m_selectionStartPos(selectionStartPos)
            , m_selectionEnd(selectionEnd)
            , m_selectionEndPos(selectionEndPos)
        {
        }

        RenderObject* selectionStart() const { return m_selectionStart; }
        Optional<unsigned> selectionStartPos() const { return m_selectionStartPos; }
        RenderObject* selectionEnd() const { return m_selectionEnd; }
        Optional<unsigned> selectionEndPos() const { return m_selectionEndPos; }
        bool selectionClear() const
        {
            return !m_selectionStart
            && (!m_selectionStartPos)
            && !m_selectionEnd
            && (!m_selectionEndPos);
        }

        void selectionStartEndPositions(unsigned& startPos, unsigned& endPos) const
        {
            startPos = m_selectionStartPos.value();
            endPos = m_selectionEndPos.value();
        }
        void setSelectionStart(RenderObject* selectionStart) { m_selectionStart = selectionStart; }
        void setSelectionStartPos(Optional<unsigned> selectionStartPos) { m_selectionStartPos = selectionStartPos;}
        void setSelectionEnd(RenderObject* selectionEnd) { m_selectionEnd = selectionEnd; }
        void setSelectionEndPos(Optional<unsigned> selectionEndPos) { m_selectionEndPos = selectionEndPos;}
        void clearSelection()
        {
            m_selectionStart = nullptr;
            m_selectionStartPos = Nullopt;
            m_selectionEnd = nullptr;
            m_selectionEndPos = Nullopt;
        }

    private:
        RenderObject* m_selectionStart { nullptr };
        Optional<unsigned> m_selectionStartPos;
        RenderObject* m_selectionEnd { nullptr };
        Optional<unsigned> m_selectionEndPos;
    };

    typedef HashMap<SelectionSubtreeRoot*, SelectionSubtreeData> RenderSubtreesMap;
    typedef HashMap<const SelectionSubtreeRoot*, std::unique_ptr<OldSelectionData>> SubtreeOldSelectionDataMap;

    SelectionSubtreeRoot();

    SelectionSubtreeData& selectionData() { return m_selectionSubtreeData; }
    const SelectionSubtreeData& selectionData() const { return m_selectionSubtreeData; }

    void setSelectionData(const SelectionSubtreeData& selectionSubtreeData) { m_selectionSubtreeData = selectionSubtreeData; }
    void adjustForVisibleSelection(Document&);

private:
    SelectionSubtreeData m_selectionSubtreeData;
};

} // namespace WebCore

#endif // SelectionSubtreeRoot_h
