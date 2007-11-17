/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "Editor.h"

#import "ClipboardAccessPolicy.h"
#import "Clipboard.h"
#import "ClipboardMac.h"
#import "Document.h"
#import "EditorClient.h"
#import "Element.h"
#import "ExceptionHandlers.h"
#import "Frame.h"
#import "PlatformString.h"
#import "Selection.h"
#import "SelectionController.h"
#import "Sound.h"
#import "TypingCommand.h"
#import "TextIterator.h"
#import "htmlediting.h"
#import "visible_units.h"

namespace WebCore {

extern "C" {

// Kill ring calls. Would be better to use NSKillRing.h, but that's not available as API or SPI.

void _NSInitializeKillRing();
void _NSAppendToKillRing(NSString *);
void _NSPrependToKillRing(NSString *);
NSString *_NSYankFromKillRing();
NSString *_NSYankPreviousFromKillRing();
void _NSNewKillRingSequence();
void _NSSetKillRingToYankedState();
void _NSResetKillRingOperationFlag();

}

PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy)
{
    return new ClipboardMac(false, [NSPasteboard generalPasteboard], policy);
}

static void initializeKillRingIfNeeded()
{
    static bool initializedKillRing = false;
    if (!initializedKillRing) {
        initializedKillRing = true;
        _NSInitializeKillRing();
    }
}

NSString* Editor::userVisibleString(NSURL* nsURL)
{
    if (client())
        return client()->userVisibleString(nsURL);
    return nil;
}

void Editor::addToKillRing(Range* range, bool prepend)
{
    initializeKillRingIfNeeded();

    if (m_startNewKillRingSequence)
        _NSNewKillRingSequence();

    String text = plainText(range);
    text.replace('\\', m_frame->backslashAsCurrencySymbol());
    if (prepend)
        _NSPrependToKillRing((NSString*)text);
    else
        _NSAppendToKillRing((NSString*)text);
    m_startNewKillRingSequence = false;
}

void Editor::yank()
{
    initializeKillRingIfNeeded();

    if (!canEdit())
        return;

    NSString* yankee = _NSYankFromKillRing();
    insertTextWithoutSendingTextEvent(yankee, false);
    _NSSetKillRingToYankedState();
}

void Editor::yankAndSelect()
{
    initializeKillRingIfNeeded();

    if (!canEdit())
        return;

    NSString* yankee = _NSYankFromKillRing();
    insertTextWithoutSendingTextEvent(yankee, true);
    _NSSetKillRingToYankedState();
}

void Editor::setMark()
{
    m_frame->setMark(m_frame->selectionController()->selection());
}

static RefPtr<Range> unionDOMRanges(Range* a, Range* b)
{
    ExceptionCode ec = 0;
    Range* start = a->compareBoundaryPoints(Range::START_TO_START, b, ec) <= 0 ? a : b;
    ASSERT(!ec);
    Range* end = a->compareBoundaryPoints(Range::END_TO_END, b, ec) <= 0 ? b : a;
    ASSERT(!ec);

    return new Range(a->startContainer(ec)->ownerDocument(), start->startContainer(ec), start->startOffset(ec), end->endContainer(ec), end->endOffset(ec));
}

void Editor::deleteToMark()
{
    if (!canEdit())
        return;

    RefPtr<Range> mark = m_frame->mark().toRange();
    if (mark) {
        SelectionController* selectionController = m_frame->selectionController();
        bool selected = selectionController->setSelectedRange(unionDOMRanges(mark.get(), selectedRange().get()).get(), DOWNSTREAM, true);
        ASSERT(selected);
        if (!selected)
            return;
    }

    performDelete();
    m_frame->setMark(m_frame->selectionController()->selection());
}

void Editor::selectToMark()
{
    RefPtr<Range> mark = m_frame->mark().toRange();
    RefPtr<Range> selection = selectedRange();
    if (!mark || !selection) {
        systemBeep();
        return;
    }
    m_frame->selectionController()->setSelectedRange(unionDOMRanges(mark.get(), selection.get()).get(), DOWNSTREAM, true);
}

void Editor::swapWithMark()
{
    const Selection& mark = m_frame->mark();
    Selection selection = m_frame->selectionController()->selection();
    if (mark.isNone() || selection.isNone()) {
        systemBeep();
        return;
    }

    m_frame->selectionController()->setSelection(mark);
    m_frame->setMark(selection);
}

void Editor::showFontPanel()
{
    [[NSFontManager sharedFontManager] orderFrontFontPanel:nil];
}

void Editor::showStylesPanel()
{
    [[NSFontManager sharedFontManager] orderFrontStylesPanel:nil];
}

void Editor::showColorPanel()
{
    [[NSApplication sharedApplication] orderFrontColorPanel:nil];
}

} // namespace WebCore
