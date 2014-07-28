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
    typedef HashMap<RenderBlock*, std::unique_ptr<RenderBlockSelectionInfo>> SelectedBlockMap;
    typedef HashMap<SelectionSubtreeRoot*, SelectionSubtreeRoot> RenderSubtreesMap;

    struct OldSelectionData {
        OldSelectionData()
            : selectionStartPos(-1)
            , selectionEndPos(-1)
        {
        }

        int selectionStartPos;
        int selectionEndPos;
        SelectedObjectMap selectedObjects;
        SelectedBlockMap selectedBlocks;
    };
    
    typedef HashMap<SelectionSubtreeRoot*, std::unique_ptr<OldSelectionData>> SubtreeOldSelectionDataMap;

    SelectionSubtreeRoot();
    SelectionSubtreeRoot(RenderObject* selectionStart, int selectionStartPos, RenderObject* selectionEnd, int selectionEndPos);

    RenderObject* selectionStart() const { return m_selectionStart; }
    int selectionStartPos() const { return m_selectionStartPos; }
    RenderObject* selectionEnd() const { return m_selectionEnd; }
    int selectionEndPos() const { return m_selectionEndPos; }
    void selectionStartEndPositions(int& startPos, int& endPos) const
    {
        startPos = m_selectionStartPos;
        endPos = m_selectionEndPos;
    }

    bool selectionClear() const
    {
        return !m_selectionStart
        && (m_selectionStartPos == -1)
        && !m_selectionEnd
        && (m_selectionEndPos == -1);
    }

    void setSelectionStart(RenderObject* selectionStart) { m_selectionStart = selectionStart; }
    void setSelectionStartPos(int selectionStartPos) { m_selectionStartPos = selectionStartPos; }
    void setSelectionEnd(RenderObject* selectionEnd) { m_selectionEnd = selectionEnd; }
    void setSelectionEndPos(int selectionEndPos) { m_selectionEndPos = selectionEndPos; }

    void adjustForVisibleSelection(Document&);

private:

    RenderObject* m_selectionStart;
    int m_selectionStartPos;
    RenderObject* m_selectionEnd;
    int m_selectionEndPos;

};

} // namespace WebCore

#endif // SelectionSubtreeRoot_h
