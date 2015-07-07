/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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

#ifndef AccessibleTextImpl_h
#define AccessibleTextImpl_h

#include "AccessibleBase.h"
#include <WebCore/VisiblePosition.h>

class AccessibleText : public AccessibleBase, public IAccessibleText2, public IAccessibleEditableText {
public:
    AccessibleText(WebCore::AccessibilityObject*, HWND);
    virtual ~AccessibleText() { }

    // IAccessibleText
    virtual HRESULT STDMETHODCALLTYPE addSelection(long startOffset, long endOffset);
    virtual HRESULT STDMETHODCALLTYPE get_attributes(long offset, long* startOffset, long* endOffset, BSTR* textAttributes);
    virtual HRESULT STDMETHODCALLTYPE get_caretOffset(long* offset);
    virtual HRESULT STDMETHODCALLTYPE get_characterExtents(long offset, enum IA2CoordinateType coordType, long* x, long* y, long* width, long* height);
    virtual HRESULT STDMETHODCALLTYPE get_nSelections(long* nSelections);
    virtual HRESULT STDMETHODCALLTYPE get_offsetAtPoint(long x, long y, enum IA2CoordinateType coordType, long* offset);
    virtual HRESULT STDMETHODCALLTYPE get_selection(long selectionIndex, long* startOffset, long* endOffset);
    virtual HRESULT STDMETHODCALLTYPE get_text(long startOffset, long endOffset, BSTR* text);
    virtual HRESULT STDMETHODCALLTYPE get_textBeforeOffset(long offset, enum IA2TextBoundaryType boundaryType, long* startOffset, long* endOffset, BSTR* text);
    virtual HRESULT STDMETHODCALLTYPE get_textAfterOffset(long offset, enum IA2TextBoundaryType boundaryType, long* startOffset, long* endOffset, BSTR* text);
    virtual HRESULT STDMETHODCALLTYPE get_textAtOffset(long offset, enum IA2TextBoundaryType boundaryType, long* startOffset, long* endOffset, BSTR* text);
    virtual HRESULT STDMETHODCALLTYPE removeSelection(long selectionIndex);
    virtual HRESULT STDMETHODCALLTYPE setCaretOffset(long offset);
    virtual HRESULT STDMETHODCALLTYPE setSelection(long selectionIndex, long startOffset, long endOffset);
    virtual HRESULT STDMETHODCALLTYPE get_nCharacters(long* characters);
    virtual HRESULT STDMETHODCALLTYPE scrollSubstringTo(long startIndex, long endIndex, enum IA2ScrollType scrollType);
    virtual HRESULT STDMETHODCALLTYPE scrollSubstringToPoint(long startIndex, long endIndex, enum IA2CoordinateType coordinateType, long x, long y);
    virtual HRESULT STDMETHODCALLTYPE get_newText(IA2TextSegment* newText);
    virtual HRESULT STDMETHODCALLTYPE get_oldText(IA2TextSegment* oldText);
    
    // IAccessibleText2
    virtual HRESULT STDMETHODCALLTYPE get_attributeRange(long offset, BSTR filter, long* startOffset, long* endOffset, BSTR* attributeValues);

    // IAccessibleEditableText
    virtual HRESULT STDMETHODCALLTYPE copyText(long startOffset, long endOffset);
    virtual HRESULT STDMETHODCALLTYPE deleteText(long startOffset, long endOffset);
    virtual HRESULT STDMETHODCALLTYPE insertText(long offset, BSTR* text);
    virtual HRESULT STDMETHODCALLTYPE cutText(long startOffset, long endOffset);
    virtual HRESULT STDMETHODCALLTYPE pasteText(long offset);
    virtual HRESULT STDMETHODCALLTYPE replaceText(long startOffset, long endOffset, BSTR* text);
    virtual HRESULT STDMETHODCALLTYPE setAttributes(long startOffset, long endOffset, BSTR* attributes);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return ++m_refCount; }
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IAccessibleBase
    virtual HRESULT STDMETHODCALLTYPE get_attributes(BSTR* attributes);

private:
    int convertSpecialOffset(int specialOffset);
    HRESULT initialCheck();
    bool isInRange(WebCore::VisiblePosition& current, WebCore::VisiblePositionRange& wordRange);
};

#endif // AccessibleText_h
