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
#import "EditingText.h"
#import "Editor.h"
#import "EditorClient.h"
#import "Frame.h"
#import "FrameView.h"
#import "HTMLNames.h"
#import "Pasteboard.h"
#import "RenderBlock.h"
#import "RuntimeApplicationChecks.h"
#import "Sound.h"
#import "htmlediting.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy, Frame* frame)
{
    return ClipboardMac::create(Clipboard::CopyAndPaste,
        policy == ClipboardWritable ? [NSPasteboard pasteboardWithUniqueName] : [NSPasteboard generalPasteboard], policy, frame);
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
#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
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

static RenderStyle* styleForSelectionStart(Frame* frame, Node *&nodeToRemove)
{
    nodeToRemove = 0;

    if (frame->selection()->isNone())
        return 0;

    Position position = frame->selection()->selection().visibleStart().deepEquivalent();
    if (!position.isCandidate() || position.isNull())
        return 0;

    RefPtr<EditingStyle> typingStyle = frame->selection()->typingStyle();
    if (!typingStyle || !typingStyle->style())
        return position.deprecatedNode()->renderer()->style();

    RefPtr<Element> styleElement = frame->document()->createElement(spanTag, false);

    String styleText = typingStyle->style()->asText() + " display: inline";
    styleElement->setAttribute(styleAttr, styleText.impl());

    styleElement->appendChild(frame->document()->createEditingTextNode(""), ASSERT_NO_EXCEPTION);

    position.deprecatedNode()->parentNode()->appendChild(styleElement, ASSERT_NO_EXCEPTION);

    nodeToRemove = styleElement.get();
    return styleElement->renderer() ? styleElement->renderer()->style() : 0;
}

const SimpleFontData* Editor::fontForSelection(bool& hasMultipleFonts) const
{
    hasMultipleFonts = false;

    if (!m_frame->selection()->isRange()) {
        Node* nodeToRemove;
        RenderStyle* style = styleForSelectionStart(m_frame, nodeToRemove); // sets nodeToRemove

        const SimpleFontData* result = 0;
        if (style)
            result = style->font().primaryFont();

        if (nodeToRemove) {
            ExceptionCode ec;
            nodeToRemove->remove(ec);
            ASSERT(!ec);
        }

        return result;
    }

    const SimpleFontData* font = 0;
    RefPtr<Range> range = m_frame->selection()->toNormalizedRange();
    if (Node* startNode = adjustedSelectionStartForStyleComputation(m_frame->selection()->selection()).deprecatedNode()) {
        Node* pastEnd = range->pastLastNode();
        // In the loop below, n should eventually match pastEnd and not become nil, but we've seen at least one
        // unreproducible case where this didn't happen, so check for null also.
        for (Node* node = startNode; node && node != pastEnd; node = node->traverseNextNode()) {
            RenderObject* renderer = node->renderer();
            if (!renderer)
                continue;
            // FIXME: Are there any node types that have renderers, but that we should be skipping?
            const SimpleFontData* primaryFont = renderer->style()->font().primaryFont();
            if (!font)
                font = primaryFont;
            else if (font != primaryFont) {
                hasMultipleFonts = true;
                break;
            }
        }
    }

    return font;
}

NSDictionary* Editor::fontAttributesForSelectionStart() const
{
    Node* nodeToRemove;
    RenderStyle* style = styleForSelectionStart(m_frame, nodeToRemove);
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
    Node* node = pos.deprecatedNode();
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

bool Editor::canCopyExcludingStandaloneImages()
{
    FrameSelection* selection = m_frame->selection();
    return selection->isRange() && !selection->isInPasswordField();
}

void Editor::takeFindStringFromSelection()
{
    if (!canCopyExcludingStandaloneImages()) {
        systemBeep();
        return;
    }

    NSString *nsSelectedText = m_frame->displayStringModifiedByEncoding(selectedText());

    NSPasteboard *findPasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
    [findPasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
    [findPasteboard setString:nsSelectedText forType:NSStringPboardType];
}

void Editor::writeSelectionToPasteboard(const String& pasteboardName, const Vector<String>& pasteboardTypes)
{
    RetainPtr<NSMutableArray> types(AdoptNS, [[NSMutableArray alloc] init]);    
    for (size_t i = 0; i < pasteboardTypes.size(); ++i)
        [types.get() addObject:pasteboardTypes[i]];
    Pasteboard::writeSelection([NSPasteboard pasteboardWithName:pasteboardName], types.get(), selectedRange().get(), true, m_frame);
}
    
void Editor::readSelectionFromPasteboard(const String& pasteboardName)
{
    Pasteboard pasteboard([NSPasteboard pasteboardWithName:pasteboardName]);
    if (m_frame->selection()->isContentRichlyEditable())
        pasteWithPasteboard(&pasteboard, true);
    else
        pasteAsPlainTextWithPasteboard(&pasteboard);   
}

} // namespace WebCore
