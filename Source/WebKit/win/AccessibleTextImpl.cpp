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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES LOSS OF USE, DATA, OR
 * PROFITS OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "AccessibleTextImpl.h"

#include "WebKitDLL.h"
#include "WebView.h"

#include <WebCore/Document.h>
#include <WebCore/Editor.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/HTMLTextFormControlElement.h>
#include <WebCore/Node.h>
#include <WebCore/Page.h>
#include <WebCore/Position.h>
#include <WebCore/RenderTextControl.h>
#include <WebCore/VisibleSelection.h>
#include <WebCore/VisibleUnits.h>
#include <WebCore/htmlediting.h>

using namespace WebCore;

AccessibleText::AccessibleText(WebCore::AccessibilityObject* obj, HWND window)
    : AccessibleBase(obj, window)
{
    ASSERT_ARG(obj, obj->isStaticText() || obj->isTextControl() || (obj->node() && obj->node()->isTextNode()));
    ASSERT_ARG(obj, obj->isAccessibilityRenderObject());
}

// IAccessibleText
HRESULT AccessibleText::addSelection(long startOffset, long endOffset)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    startOffset = convertSpecialOffset(startOffset);
    endOffset = convertSpecialOffset(endOffset);

    m_object->setSelectedTextRange(PlainTextRange(startOffset, endOffset-startOffset));
    
    return S_OK;
}

HRESULT AccessibleText::get_attributes(long offset, long* startOffset, long* endOffset, BSTR* textAttributes)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    offset = convertSpecialOffset(offset);

    return E_NOTIMPL;
}

HRESULT AccessibleText::get_caretOffset(long* offset)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    VisiblePosition caretPosition = m_object->visiblePositionForPoint(m_object->document()->frame()->selection().absoluteCaretBounds().center());

    int caretOffset = caretPosition.deepEquivalent().offsetInContainerNode();
    if (caretOffset < 0)
        return E_FAIL;
    *offset = caretOffset;
    return S_OK;
}

HRESULT AccessibleText::get_characterExtents(long offset, enum IA2CoordinateType coordType, long* x, long* y, long* width, long* height)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    offset = convertSpecialOffset(offset);

    Node* node = m_object->node();
    if (!node)
        return E_POINTER;

    IntRect boundingRect = m_object->boundsForVisiblePositionRange(VisiblePositionRange(VisiblePosition(Position(node, offset, Position::PositionIsOffsetInAnchor)), VisiblePosition(Position(node, offset+1, Position::PositionIsOffsetInAnchor))));
    *width = boundingRect.width();
    *height = boundingRect.height();
    switch (coordType) {
    case IA2_COORDTYPE_SCREEN_RELATIVE:
        POINT points[1];
        points[0].x = boundingRect.x();
        points[0].y = boundingRect.y();
        MapWindowPoints(m_window, 0, points, 1);
        *x = points[0].x;
        *y = points[0].y;
        break;
    case IA2_COORDTYPE_PARENT_RELATIVE:
        *x = boundingRect.x();
        *y = boundingRect.y();
        break;
    default:
        return E_INVALIDARG;
    }

    return S_OK;
}

HRESULT AccessibleText::get_nSelections(long* nSelections)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    if (m_object->document()->frame()->selection().isNone())
        *nSelections = 0;
    else
        *nSelections = 1;
    return S_OK;
}

HRESULT AccessibleText::get_offsetAtPoint(long x, long y, enum IA2CoordinateType coordType, long* offset)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    Node* node = m_object->node();
    if (!node)
        return E_POINTER;

    VisiblePosition vpos;
    switch (coordType) {
    case IA2_COORDTYPE_SCREEN_RELATIVE:
        POINT points[1];
        points[0].x = x;
        points[0].y = y;
        MapWindowPoints(0, m_window, points, 1);
        vpos = m_object->visiblePositionForPoint(IntPoint(points[0].x, points[0].y));
        break;
    case IA2_COORDTYPE_PARENT_RELATIVE:
        vpos = m_object->visiblePositionForPoint(IntPoint(x, y));
        break;
    default:
        return E_INVALIDARG;
    }

    int caretPosition = vpos.deepEquivalent().offsetInContainerNode();
    if (caretPosition < 0 || caretPosition > m_object->stringValue().length())
        return S_FALSE; 
    return S_OK;
}

HRESULT AccessibleText::get_selection(long selectionIndex, long* startOffset, long* endOffset)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    long selections;
    get_nSelections(&selections);
    if (selectionIndex < 0 || selectionIndex >= selections)
        return E_INVALIDARG;

    PlainTextRange selectionRange = m_object->selectedTextRange();

    *startOffset = selectionRange.start;
    *endOffset = selectionRange.length;
    return S_OK;
}

HRESULT AccessibleText::get_text(long startOffset, long endOffset, BSTR* text)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    startOffset = convertSpecialOffset(startOffset);
    endOffset = convertSpecialOffset(endOffset);
    WTF::String substringText = m_object->stringValue().substring(startOffset, endOffset - startOffset);

    *text = BString(substringText).release();
    if (substringText.length() && !*text)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT AccessibleText::get_textBeforeOffset(long offset, enum IA2TextBoundaryType boundaryType, long* startOffset, long* endOffset, BSTR* text)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    if (!startOffset || !endOffset || !text)
        return E_POINTER;
    
    offset = convertSpecialOffset(offset);

    if (offset < 0 || offset > m_object->stringValue().length())
        return E_INVALIDARG;

    // Obtain the desired text range
    VisiblePosition currentPosition = m_object->visiblePositionForIndex(offset);
    VisiblePositionRange textRange;
    int previousPos = std::max(0, static_cast<int>(offset-1));
    switch (boundaryType) {
    case IA2_TEXT_BOUNDARY_CHAR:
        textRange = m_object->visiblePositionRangeForRange(PlainTextRange(previousPos, 1));
        break;
    case IA2_TEXT_BOUNDARY_WORD:
        textRange = m_object->positionOfLeftWord(currentPosition);
        textRange = m_object->positionOfRightWord(leftWordPosition(textRange.start, true));
        break;
    case IA2_TEXT_BOUNDARY_SENTENCE:
        textRange.start = m_object->previousSentenceStartPosition(currentPosition);
        textRange = m_object->sentenceForPosition(textRange.start);
        if (isInRange(currentPosition, textRange))
            textRange = m_object->sentenceForPosition(m_object->previousSentenceStartPosition(textRange.start.previous()));
        break;
    case IA2_TEXT_BOUNDARY_PARAGRAPH:
        textRange.start = m_object->previousParagraphStartPosition(currentPosition);
        textRange = m_object->paragraphForPosition(textRange.start);
        if (isInRange(currentPosition, textRange))
            textRange = m_object->paragraphForPosition(m_object->previousParagraphStartPosition(textRange.start.previous()));
        break;
    case IA2_TEXT_BOUNDARY_LINE:
        textRange = m_object->visiblePositionRangeForLine(m_object->lineForPosition(currentPosition));
        textRange = m_object->leftLineVisiblePositionRange(textRange.start.previous());
        break;
    case IA2_TEXT_BOUNDARY_ALL:
        textRange = m_object->visiblePositionRangeForRange(PlainTextRange(0, offset));
        break;
    default:
        return E_INVALIDARG;
        break;
    }

    // Obtain string and offsets associated with text range
    *startOffset = textRange.start.deepEquivalent().offsetInContainerNode();
    *endOffset = textRange.end.deepEquivalent().offsetInContainerNode();

    if (*startOffset == *endOffset)
        return S_FALSE;

    WTF::String substringText = m_object->text().substring(*startOffset, *endOffset - *startOffset);
    *text = BString(substringText).release();

    if (substringText.length() && !*text)
        return E_OUTOFMEMORY;

    if (!*text)
        return S_FALSE;

    return S_OK;
}

HRESULT AccessibleText::get_textAfterOffset(long offset, enum IA2TextBoundaryType boundaryType, long* startOffset, long* endOffset, BSTR* text)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    if (!startOffset || !endOffset || !text)
        return E_POINTER;

    int textLength = m_object->stringValue().length();
    offset = convertSpecialOffset(offset);

    if (offset < 0 || offset > textLength)
        return E_INVALIDARG;

    VisiblePosition currentPosition = m_object->visiblePositionForIndex(offset);
    VisiblePositionRange textRange;

    // Obtain the desired text range
    switch (boundaryType) {
    case IA2_TEXT_BOUNDARY_CHAR:
        textRange = m_object->visiblePositionRangeForRange(PlainTextRange(offset + 1, offset + 2));
        break;
    case IA2_TEXT_BOUNDARY_WORD:
        textRange = m_object->positionOfRightWord(rightWordPosition(currentPosition, true));
        break;
    case IA2_TEXT_BOUNDARY_SENTENCE:
        textRange.end = m_object->nextSentenceEndPosition(currentPosition);
        textRange = m_object->sentenceForPosition(textRange.end);
        if (isInRange(currentPosition, textRange))
            textRange = m_object->sentenceForPosition(m_object->nextSentenceEndPosition(textRange.end.next()));
        break;
    case IA2_TEXT_BOUNDARY_PARAGRAPH:
        textRange.end = m_object->nextParagraphEndPosition(currentPosition);
        textRange = m_object->paragraphForPosition(textRange.end);
        if (isInRange(currentPosition, textRange))
            textRange = m_object->paragraphForPosition(m_object->nextParagraphEndPosition(textRange.end.next()));
        break;
    case IA2_TEXT_BOUNDARY_LINE:
        textRange = m_object->visiblePositionRangeForLine(m_object->lineForPosition(currentPosition));
        textRange = m_object->rightLineVisiblePositionRange(textRange.end.next());
        break;
    case IA2_TEXT_BOUNDARY_ALL:
        textRange = m_object->visiblePositionRangeForRange(PlainTextRange(offset, textLength-offset));
        break;
    default:
        return E_INVALIDARG;
        break;
    }

    // Obtain string and offsets associated with text range
    *startOffset = textRange.start.deepEquivalent().offsetInContainerNode();
    *endOffset = textRange.end.deepEquivalent().offsetInContainerNode();

    if (*startOffset == *endOffset)
        return S_FALSE;

    WTF::String substringText = m_object->text().substring(*startOffset, *endOffset - *startOffset);
    *text = BString(substringText).release();
    if (substringText.length() && !*text)
        return E_OUTOFMEMORY;

    if (!*text)
        return S_FALSE;

    return S_OK;
}

HRESULT AccessibleText::get_textAtOffset(long offset, enum IA2TextBoundaryType boundaryType, long* startOffset, long* endOffset, BSTR* text)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    if (!startOffset || !endOffset || !text)
        return E_POINTER;

    int textLength = m_object->stringValue().length();

    offset = convertSpecialOffset(offset);

    if (offset < 0 || offset > textLength)
        return E_INVALIDARG;

    // Obtain the desired text range
    VisiblePosition currentPosition = m_object->visiblePositionForIndex(offset);
    VisiblePositionRange textRange;
    switch (boundaryType) {
    case IA2_TEXT_BOUNDARY_CHAR:
        textRange = m_object->visiblePositionRangeForRange(PlainTextRange(offset, 1));
        break;
    case IA2_TEXT_BOUNDARY_WORD:
        textRange = m_object->positionOfRightWord(leftWordPosition(currentPosition.next(), true));
        break;
    case IA2_TEXT_BOUNDARY_SENTENCE:
        textRange = m_object->sentenceForPosition(currentPosition);
        break;
    case IA2_TEXT_BOUNDARY_PARAGRAPH:
        textRange = m_object->paragraphForPosition(currentPosition);
        break;
    case IA2_TEXT_BOUNDARY_LINE:
        textRange = m_object->leftLineVisiblePositionRange(currentPosition);
        break;
    case IA2_TEXT_BOUNDARY_ALL:
        textRange = m_object->visiblePositionRangeForRange(PlainTextRange(0, m_object->text().length()));
        break;
    default:
        return E_INVALIDARG;
        break;
    }

    // Obtain string and offsets associated with text range
    *startOffset = textRange.start.deepEquivalent().offsetInContainerNode();
    *endOffset = textRange.end.deepEquivalent().offsetInContainerNode();

    if (*startOffset == *endOffset)
        return S_FALSE;

    WTF::String substringText = m_object->text().substring(*startOffset, *endOffset - *startOffset);
    *text = BString(substringText).release();

    if (substringText.length() && !*text)
        return E_OUTOFMEMORY;

    if (!*text)
        return S_FALSE;

    return S_OK;
}

HRESULT AccessibleText::removeSelection(long selectionIndex)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    long selections;
    get_nSelections(&selections);
    if (selectionIndex < 0 || selectionIndex >= selections)
        return E_INVALIDARG;

    m_object->document()->frame()->selection().clear();
    return S_OK;
}

HRESULT AccessibleText::setCaretOffset(long offset)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;
        
    offset = convertSpecialOffset(offset);

    Node* node = m_object->node();
    if (!node)
        return E_POINTER;

    m_object->document()->frame()->selection().setSelection(VisibleSelection(VisiblePosition(Position(node, offset, Position::PositionIsOffsetInAnchor))));
    return S_OK;
}

HRESULT AccessibleText::setSelection(long selectionIndex, long startOffset, long endOffset)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    long selections;
    get_nSelections(&selections);
    if (selectionIndex < 0 || selectionIndex >= selections)
        return E_INVALIDARG;

    m_object->setSelectedTextRange(PlainTextRange(startOffset, endOffset - startOffset));
    return S_OK;
}

HRESULT AccessibleText::get_nCharacters(long* characters)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    int length = m_object->stringValue().length();
    if (length < 0)
        return E_FAIL;

    *characters = length;
    return S_OK;
}

HRESULT AccessibleText::scrollSubstringTo(long startIndex, long endIndex, enum IA2ScrollType scrollType)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    startIndex = convertSpecialOffset(startIndex);
    endIndex = convertSpecialOffset(endIndex);

    VisiblePositionRange textRange = m_object->visiblePositionRangeForRange(PlainTextRange(startIndex, endIndex-startIndex));
    if (textRange.start.isNull() || textRange.end.isNull())
        return S_FALSE;

    IntRect boundingBox = makeRange(textRange.start, textRange.end)->boundingBox();
    switch (scrollType) {
    case IA2_SCROLL_TYPE_TOP_LEFT:
        m_object->scrollToGlobalPoint(boundingBox.minXMinYCorner());
        break;
    case IA2_SCROLL_TYPE_BOTTOM_RIGHT:
        m_object->scrollToGlobalPoint(boundingBox.maxXMaxYCorner());
        break;
    case IA2_SCROLL_TYPE_TOP_EDGE:
        m_object->scrollToGlobalPoint(IntPoint((boundingBox.x() + boundingBox.maxX()) / 2, boundingBox.y()));
        break;
    case IA2_SCROLL_TYPE_BOTTOM_EDGE:
        m_object->scrollToGlobalPoint(IntPoint((boundingBox.x() + boundingBox.maxX()) / 2, boundingBox.maxY()));
        break;
    case IA2_SCROLL_TYPE_LEFT_EDGE:
        m_object->scrollToGlobalPoint(IntPoint(boundingBox.x(), (boundingBox.y() + boundingBox.maxY()) / 2));
        break;
    case IA2_SCROLL_TYPE_RIGHT_EDGE:
        m_object->scrollToGlobalPoint(IntPoint(boundingBox.maxX(), (boundingBox.y() + boundingBox.maxY()) / 2));
        break;
    case IA2_SCROLL_TYPE_ANYWHERE:
        m_object->scrollToGlobalPoint(boundingBox.center());
        break;
    default:
        return E_INVALIDARG;
    }
    return S_OK;
}

HRESULT AccessibleText::scrollSubstringToPoint(long startIndex, long endIndex, enum IA2CoordinateType coordinateType, long x, long y)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    startIndex = convertSpecialOffset(startIndex);
    endIndex = convertSpecialOffset(endIndex);

    switch (coordinateType) {
    case IA2_COORDTYPE_SCREEN_RELATIVE:
        POINT points[1];
        points[0].x = x;
        points[0].y = y;
        MapWindowPoints(0, m_window, points, 1);
        m_object->scrollToGlobalPoint(IntPoint(points[0].x, points[0].y));
        break;
    case IA2_COORDTYPE_PARENT_RELATIVE:
        m_object->scrollToGlobalPoint(IntPoint(x, y));
        break;
    default:
        return E_INVALIDARG;
    }

    return S_OK;
}

HRESULT AccessibleText::get_newText(IA2TextSegment* newText)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;
    
    return E_NOTIMPL;
}

HRESULT AccessibleText::get_oldText(IA2TextSegment* oldText)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    return E_NOTIMPL;
}


// IAccessibleText2
HRESULT AccessibleText::get_attributeRange(long offset, BSTR filter, long* startOffset, long* endOffset, BSTR* attributeValues)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    return E_NOTIMPL;
}

HRESULT AccessibleText::copyText(long startOffset, long endOffset)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    startOffset = convertSpecialOffset(startOffset);
    endOffset = convertSpecialOffset(endOffset);

    Frame* frame = m_object->document()->frame();
    if (!frame)
        return E_POINTER;

    addSelection(startOffset, endOffset);

    frame->editor().copy();
    return S_OK;
}

HRESULT AccessibleText::deleteText(long startOffset, long endOffset)
{
    if (m_object->isReadOnly())
        return S_FALSE;

    if (initialCheck() == E_POINTER)
        return E_POINTER;

    Frame* frame = m_object->document()->frame();
    if (!frame)
        return E_POINTER;

    addSelection(startOffset, endOffset);

    frame->editor().deleteSelectionWithSmartDelete(false);
    return S_OK;
}

HRESULT AccessibleText::insertText(long offset, BSTR* text)
{
    if (m_object->isReadOnly())
        return S_FALSE;

    if (initialCheck() == E_POINTER)
        return E_POINTER;

    offset = convertSpecialOffset(offset);

    Frame* frame = m_object->document()->frame();
    if (!frame)
        return E_POINTER;
    
    addSelection(offset, offset);

    frame->editor().insertText(*text, 0);
    return S_OK;
}

HRESULT AccessibleText::cutText(long startOffset, long endOffset)
{
    if (m_object->isReadOnly())
        return S_FALSE;

    if (initialCheck() == E_POINTER)
        return E_POINTER;

    startOffset = convertSpecialOffset(startOffset);
    endOffset = convertSpecialOffset(endOffset);

    Frame* frame = m_object->document()->frame();
    if (!frame)
        return E_POINTER;

    addSelection(startOffset, endOffset);

    frame->editor().cut();
    return S_OK;
}

HRESULT AccessibleText::pasteText(long offset)
{
    if (m_object->isReadOnly())
        return S_FALSE;

    if (initialCheck() == E_POINTER)
        return E_POINTER;

    offset = convertSpecialOffset(offset);

    Frame* frame = m_object->document()->frame();
    if (!frame)
        return E_POINTER;

    addSelection(offset, offset);

    frame->editor().paste();
    return S_OK;
}

HRESULT AccessibleText::replaceText(long startOffset, long endOffset, BSTR* text)
{
    if (m_object->isReadOnly())
        return S_FALSE;

    if (initialCheck() == E_POINTER)
        return E_POINTER;

    startOffset = convertSpecialOffset(startOffset);
    endOffset = convertSpecialOffset(endOffset);

    Frame* frame = m_object->document()->frame();
    if (!frame)
        return E_POINTER;

    addSelection(startOffset, endOffset);

    frame->editor().replaceSelectionWithText(*text, true, false);
    return S_OK;
}

HRESULT AccessibleText::setAttributes(long startOffset, long endOffset, BSTR* attributes)
{
    if (initialCheck() == E_POINTER)
        return E_POINTER;

    return E_NOTIMPL;
}

// IAccessible2
HRESULT AccessibleText::get_attributes(BSTR* attributes)
{
    WTF::String text("text-model:a1");
    *attributes = BString(text).release();
    return S_OK;
}

// IUnknown
HRESULT STDMETHODCALLTYPE AccessibleText::QueryInterface(REFIID riid, void** ppvObject)
{
    if (IsEqualGUID(riid, __uuidof(IAccessibleText)))
        *ppvObject = static_cast<IAccessibleText*>(this);
    else if (IsEqualGUID(riid, __uuidof(IAccessibleEditableText)))
        *ppvObject = static_cast<IAccessibleEditableText*>(this);
    else if (IsEqualGUID(riid, __uuidof(IAccessible)))
        *ppvObject = static_cast<IAccessible*>(this);
    else if (IsEqualGUID(riid, __uuidof(IDispatch)))
        *ppvObject = static_cast<IAccessible*>(this);
    else if (IsEqualGUID(riid, __uuidof(IUnknown)))
        *ppvObject = static_cast<IAccessible*>(this);
    else if (IsEqualGUID(riid, __uuidof(IAccessible2_2)))
        *ppvObject = static_cast<IAccessible2_2*>(this);
    else if (IsEqualGUID(riid, __uuidof(IAccessible2)))
        *ppvObject = static_cast<IAccessible2*>(this);
    else if (IsEqualGUID(riid, __uuidof(IAccessibleComparable)))
        *ppvObject = static_cast<IAccessibleComparable*>(this);
    else if (IsEqualGUID(riid, __uuidof(IServiceProvider)))
        *ppvObject = static_cast<IServiceProvider*>(this);
    else if (IsEqualGUID(riid, __uuidof(AccessibleBase)))
        *ppvObject = static_cast<AccessibleBase*>(this);
    else {
        *ppvObject = 0;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE AccessibleText::Release(void)
{
    ASSERT(m_refCount > 0);
    if (--m_refCount)
        return m_refCount;
    delete this;
    return 0;
}

int AccessibleText::convertSpecialOffset(int offset)
{
    ASSERT(m_object);
    
    if (offset == IA2_TEXT_OFFSET_LENGTH) 
        return m_object->stringValue().length();
    if (offset == IA2_TEXT_OFFSET_CARET) {
        long caretOffset;
        get_caretOffset(&caretOffset);
        return caretOffset;
    }
    return offset;
}

HRESULT AccessibleText::initialCheck()
{
    if (!m_object)
        return E_FAIL;

    Document* document = m_object->document();
    if (!document)
        return E_FAIL;

    Frame* frame = document->frame();
    if (!frame)
        return E_FAIL;

    return S_OK;
}

bool AccessibleText::isInRange(VisiblePosition& current, VisiblePositionRange& wordRange)
{
    ASSERT(wordRange.start.isNotNull());
    ASSERT(wordRange.end.isNotNull());
    return comparePositions(current.deepEquivalent(), wordRange.start) >= 0 && comparePositions(current.deepEquivalent(), wordRange.end) <= 0;
}
