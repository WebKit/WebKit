/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#import "ColorMac.h"
#import "ClipboardMac.h"
#import "CachedResourceLoader.h"
#import "DocumentFragment.h"
#import "Editor.h"
#import "EditorClient.h"
#import "Frame.h"
#import "FrameView.h"
#import "Pasteboard.h"
#import "RenderBlock.h"
#import "RuntimeApplicationChecks.h"

namespace WebCore {

PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy, Frame* frame)
{
    return ClipboardMac::create(Clipboard::CopyAndPaste, [NSPasteboard generalPasteboard], policy, frame);
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

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, bool allowPlainText)
{
    RefPtr<Range> range = selectedRange();
    bool choosePlainText;
    
    m_frame->editor()->client()->setInsertionPasteboard([NSPasteboard generalPasteboard]);
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    RefPtr<DocumentFragment> fragment = pasteboard->documentFragment(m_frame, range, allowPlainText, choosePlainText);
    if (fragment && shouldInsertFragment(fragment, range, EditorInsertActionPasted))
        pasteAsFragment(fragment, canSmartReplaceWithPasteboard(pasteboard), false);
#else
    // Mail is ignoring the frament passed to the delegate and creates a new one.
    // We want to avoid creating the fragment twice.
    if (applicationIsAppleMail()) {
        if (shouldInsertFragment(NULL, range, EditorInsertActionPasted)) {
            RefPtr<DocumentFragment> fragment = pasteboard->documentFragment(m_frame, range, allowPlainText, choosePlainText);
            if (fragment)
                pasteAsFragment(fragment, canSmartReplaceWithPasteboard(pasteboard), false);
        }        
    } else {
        RefPtr<DocumentFragment>fragment = pasteboard->documentFragment(m_frame, range, allowPlainText, choosePlainText);
        if (fragment && shouldInsertFragment(fragment, range, EditorInsertActionPasted))
            pasteAsFragment(fragment, canSmartReplaceWithPasteboard(pasteboard), false);
    }
#endif
    m_frame->editor()->client()->setInsertionPasteboard(nil);
}

NSDictionary* Editor::fontAttributesForSelectionStart() const
{
    Node* nodeToRemove;
    RenderStyle* style = styleForSelectionStart(nodeToRemove);
    if (!style)
        return nil;

    NSMutableDictionary* result = [NSMutableDictionary dictionary];

    if (style->visitedDependentColor(CSSPropertyBackgroundColor).isValid() && style->visitedDependentColor(CSSPropertyBackgroundColor).alpha() != 0)
        [result setObject:nsColor(style->visitedDependentColor(CSSPropertyBackgroundColor)) forKey:NSBackgroundColorAttributeName];

    if (style->font().primaryFont()->getNSFont())
        [result setObject:style->font().primaryFont()->getNSFont() forKey:NSFontAttributeName];

    if (style->visitedDependentColor(CSSPropertyColor).isValid() && style->visitedDependentColor(CSSPropertyColor) != Color::black)
        [result setObject:nsColor(style->visitedDependentColor(CSSPropertyColor)) forKey:NSForegroundColorAttributeName];

    const ShadowData* shadow = style->textShadow();
    if (shadow) {
        NSShadow* s = [[NSShadow alloc] init];
        [s setShadowOffset:NSMakeSize(shadow->x(), shadow->y())];
        [s setShadowBlurRadius:shadow->blur()];
        [s setShadowColor:nsColor(shadow->color())];
        [result setObject:s forKey:NSShadowAttributeName];
    }

    int decoration = style->textDecorationsInEffect();
    if (decoration & LINE_THROUGH)
        [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];

    int superscriptInt = 0;
    switch (style->verticalAlign()) {
        case BASELINE:
        case BOTTOM:
        case BASELINE_MIDDLE:
        case LENGTH:
        case MIDDLE:
        case TEXT_BOTTOM:
        case TEXT_TOP:
        case TOP:
            break;
        case SUB:
            superscriptInt = -1;
            break;
        case SUPER:
            superscriptInt = 1;
            break;
    }
    if (superscriptInt)
        [result setObject:[NSNumber numberWithInt:superscriptInt] forKey:NSSuperscriptAttributeName];

    if (decoration & UNDERLINE)
        [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];

    if (nodeToRemove) {
        ExceptionCode ec = 0;
        nodeToRemove->remove(ec);
        ASSERT(ec == 0);
    }

    return result;
}

NSWritingDirection Editor::baseWritingDirectionForSelectionStart() const
{
    NSWritingDirection result = NSWritingDirectionLeftToRight;

    Position pos = m_frame->selection()->selection().visibleStart().deepEquivalent();
    Node* node = pos.node();
    if (!node)
        return result;

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return result;

    if (!renderer->isBlockFlow()) {
        renderer = renderer->containingBlock();
        if (!renderer)
            return result;
    }

    RenderStyle* style = renderer->style();
    if (!style)
        return result;
        
    switch (style->direction()) {
        case LTR:
            result = NSWritingDirectionLeftToRight;
            break;
        case RTL:
            result = NSWritingDirectionRightToLeft;
            break;
    }

    return result;
}

} // namespace WebCore
