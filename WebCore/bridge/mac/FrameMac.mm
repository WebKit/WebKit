/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#import "FrameMac.h"

#import "AXObjectCache.h"
#import "BeforeUnloadEvent.h"
#import "BlockExceptions.h"
#import "Chrome.h"
#import "Cache.h"
#import "ClipboardEvent.h"
#import "ClipboardMac.h"
#import "Cursor.h"
#import "DOMInternal.h"
#import "DocumentLoader.h"
#import "EditCommand.h"
#import "EditorClient.h"
#import "Event.h"
#import "EventNames.h"
#import "FloatRect.h"
#import "FontData.h"
#import "FoundationExtras.h"
#import "FrameLoadRequest.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "FrameLoaderTypes.h"
#import "FramePrivate.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "HTMLDocument.h"
#import "HTMLFormElement.h"
#import "HTMLGenericFormElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "HTMLTableCellElement.h"
#import "HitTestRequest.h"
#import "HitTestResult.h"
#import "KeyboardEvent.h"
#import "Logging.h"
#import "MouseEventWithHitTestResults.h"
#import "Page.h"
#import "PlatformKeyboardEvent.h"
#import "PlatformScrollBar.h"
#import "PlatformWheelEvent.h"
#import "Plugin.h"
#import "RegularExpression.h"
#import "RenderImage.h"
#import "RenderListItem.h"
#import "RenderPart.h"
#import "RenderTableCell.h"
#import "RenderTheme.h"
#import "RenderView.h"
#import "ResourceHandle.h"
#import "SystemTime.h"
#import "TextIterator.h"
#import "TextResourceDecoder.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreSystemInterface.h"
#import "WebCoreViewFactory.h"
#import "WebDashboardRegion.h"
#import "WebScriptObjectPrivate.h"
#import "csshelper.h"
#import "htmlediting.h"
#import "kjs_proxy.h"
#import "kjs_window.h"
#import "visible_units.h"
#import <Carbon/Carbon.h>
#import <JavaScriptCore/NP_jsobject.h>
#import <JavaScriptCore/npruntime_impl.h>

#undef _webcore_TIMING

@interface NSObject (WebPlugIn)
- (id)objectForWebScript;
- (NPObject *)createPluginScriptableObject;
@end

using namespace std;
using namespace KJS::Bindings;

using KJS::JSLock;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

static SEL selectorForKeyEvent(KeyboardEvent* event)
{
    // FIXME: This helper function is for the auto-fill code so the bridge can pass a selector to the form delegate.  
    // Eventually, we should move all of the auto-fill code down to WebKit and remove the need for this function by
    // not relying on the selector in the new implementation.
    String key = event->keyIdentifier();
    if (key == "Up")
        return @selector(moveUp:);
    if (key == "Down")
        return @selector(moveDown:);
    if (key == "U+00001B")
        return @selector(cancel:);
    if (key == "U+000009") {
        if (event->shiftKey())
            return @selector(insertBacktab:);
        return @selector(insertTab:);
    }
    if (key == "Enter")
        return @selector(insertNewline:);
    return 0;
}

FrameMac::FrameMac(Page* page, Element* ownerElement, FrameLoaderClient* frameLoaderClient)
    : Frame(page, ownerElement, frameLoaderClient)
    , _bridge(nil)
    , _bindingRoot(0)
    , _windowScriptObject(0)
    , _windowScriptNPObject(0)
{
}

FrameMac::~FrameMac()
{
    setView(0);
    loader()->clearRecordedFormValues();    
    
    [_bridge clearFrame];
    HardRelease(_bridge);
    _bridge = nil;

    loader()->cancelAndClear();
}

// Either get cached regexp or build one that matches any of the labels.
// The regexp we build is of the form:  (STR1|STR2|STRN)
RegularExpression *regExpForLabels(NSArray *labels)
{
    // All the ObjC calls in this method are simple array and string
    // calls which we can assume do not raise exceptions


    // Parallel arrays that we use to cache regExps.  In practice the number of expressions
    // that the app will use is equal to the number of locales is used in searching.
    static const unsigned int regExpCacheSize = 4;
    static NSMutableArray *regExpLabels = nil;
    static Vector<RegularExpression*> regExps;
    static RegularExpression wordRegExp = RegularExpression("\\w");

    RegularExpression *result;
    if (!regExpLabels)
        regExpLabels = [[NSMutableArray alloc] initWithCapacity:regExpCacheSize];
    CFIndex cacheHit = [regExpLabels indexOfObject:labels];
    if (cacheHit != NSNotFound)
        result = regExps.at(cacheHit);
    else {
        DeprecatedString pattern("(");
        unsigned int numLabels = [labels count];
        unsigned int i;
        for (i = 0; i < numLabels; i++) {
            DeprecatedString label = DeprecatedString::fromNSString((NSString *)[labels objectAtIndex:i]);

            bool startsWithWordChar = false;
            bool endsWithWordChar = false;
            if (label.length() != 0) {
                startsWithWordChar = wordRegExp.search(label.at(0)) >= 0;
                endsWithWordChar = wordRegExp.search(label.at(label.length() - 1)) >= 0;
            }
            
            if (i != 0)
                pattern.append("|");
            // Search for word boundaries only if label starts/ends with "word characters".
            // If we always searched for word boundaries, this wouldn't work for languages
            // such as Japanese.
            if (startsWithWordChar) {
                pattern.append("\\b");
            }
            pattern.append(label);
            if (endsWithWordChar) {
                pattern.append("\\b");
            }
        }
        pattern.append(")");
        result = new RegularExpression(pattern, false);
    }

    // add regexp to the cache, making sure it is at the front for LRU ordering
    if (cacheHit != 0) {
        if (cacheHit != NSNotFound) {
            // remove from old spot
            [regExpLabels removeObjectAtIndex:cacheHit];
            regExps.remove(cacheHit);
        }
        // add to start
        [regExpLabels insertObject:labels atIndex:0];
        regExps.insert(0, result);
        // trim if too big
        if ([regExpLabels count] > regExpCacheSize) {
            [regExpLabels removeObjectAtIndex:regExpCacheSize];
            RegularExpression *last = regExps.last();
            regExps.removeLast();
            delete last;
        }
    }
    return result;
}

NSString* FrameMac::searchForLabelsAboveCell(RegularExpression* regExp, HTMLTableCellElement* cell)
{
    RenderTableCell* cellRenderer = static_cast<RenderTableCell*>(cell->renderer());

    if (cellRenderer && cellRenderer->isTableCell()) {
        RenderTableCell* cellAboveRenderer = cellRenderer->table()->cellAbove(cellRenderer);

        if (cellAboveRenderer) {
            HTMLTableCellElement *aboveCell =
                static_cast<HTMLTableCellElement*>(cellAboveRenderer->element());

            if (aboveCell) {
                // search within the above cell we found for a match
                for (Node *n = aboveCell->firstChild(); n; n = n->traverseNextNode(aboveCell)) {
                    if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
                        // For each text chunk, run the regexp
                        DeprecatedString nodeString = n->nodeValue().deprecatedString();
                        int pos = regExp->searchRev(nodeString);
                        if (pos >= 0)
                            return nodeString.mid(pos, regExp->matchedLength()).getNSString();
                    }
                }
            }
        }
    }
    // Any reason in practice to search all cells in that are above cell?
    return nil;
}

NSString *FrameMac::searchForLabelsBeforeElement(NSArray *labels, Element *element)
{
    RegularExpression *regExp = regExpForLabels(labels);
    // We stop searching after we've seen this many chars
    const unsigned int charsSearchedThreshold = 500;
    // This is the absolute max we search.  We allow a little more slop than
    // charsSearchedThreshold, to make it more likely that we'll search whole nodes.
    const unsigned int maxCharsSearched = 600;
    // If the starting element is within a table, the cell that contains it
    HTMLTableCellElement *startingTableCell = 0;
    bool searchedCellAbove = false;

    // walk backwards in the node tree, until another element, or form, or end of tree
    int unsigned lengthSearched = 0;
    Node *n;
    for (n = element->traversePreviousNode();
         n && lengthSearched < charsSearchedThreshold;
         n = n->traversePreviousNode())
    {
        if (n->hasTagName(formTag)
            || (n->isHTMLElement()
                && static_cast<HTMLElement*>(n)->isGenericFormElement()))
        {
            // We hit another form element or the start of the form - bail out
            break;
        } else if (n->hasTagName(tdTag) && !startingTableCell) {
            startingTableCell = static_cast<HTMLTableCellElement*>(n);
        } else if (n->hasTagName(trTag) && startingTableCell) {
            NSString *result = searchForLabelsAboveCell(regExp, startingTableCell);
            if (result) {
                return result;
            }
            searchedCellAbove = true;
        } else if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
            // For each text chunk, run the regexp
            DeprecatedString nodeString = n->nodeValue().deprecatedString();
            // add 100 for slop, to make it more likely that we'll search whole nodes
            if (lengthSearched + nodeString.length() > maxCharsSearched)
                nodeString = nodeString.right(charsSearchedThreshold - lengthSearched);
            int pos = regExp->searchRev(nodeString);
            if (pos >= 0)
                return nodeString.mid(pos, regExp->matchedLength()).getNSString();
            else
                lengthSearched += nodeString.length();
        }
    }

    // If we started in a cell, but bailed because we found the start of the form or the
    // previous element, we still might need to search the row above us for a label.
    if (startingTableCell && !searchedCellAbove) {
         return searchForLabelsAboveCell(regExp, startingTableCell);
    } else {
        return nil;
    }
}

NSString *FrameMac::matchLabelsAgainstElement(NSArray *labels, Element *element)
{
    DeprecatedString name = element->getAttribute(nameAttr).deprecatedString();
    // Make numbers and _'s in field names behave like word boundaries, e.g., "address2"
    name.replace(RegularExpression("[[:digit:]]"), " ");
    name.replace('_', ' ');
    
    RegularExpression *regExp = regExpForLabels(labels);
    // Use the largest match we can find in the whole name string
    int pos;
    int length;
    int bestPos = -1;
    int bestLength = -1;
    int start = 0;
    do {
        pos = regExp->search(name, start);
        if (pos != -1) {
            length = regExp->matchedLength();
            if (length >= bestLength) {
                bestPos = pos;
                bestLength = length;
            }
            start = pos+1;
        }
    } while (pos != -1);

    if (bestPos != -1)
        return name.mid(bestPos, bestLength).getNSString();
    return nil;
}

void FrameMac::setView(FrameView *view)
{
    Frame::setView(view);
    
#ifdef MULTIPLE_FORM_SUBMISSION_PROTECTION
    // Only one form submission is allowed per view of a part.
    // Since this part may be getting reused as a result of being
    // pulled from the back/forward cache, reset this flag.
    loader()->resetMultipleFormSubmissionProtection();
#endif
}

void FrameMac::setStatusBarText(const String& status)
{
    String text = status;
    text.replace('\\', backslashAsCurrencySymbol());
    
    // We want the temporaries allocated here to be released even before returning to the 
    // event loop; see <http://bugs.webkit.org/show_bug.cgi?id=9880>.
    NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge setStatusText:text];
    END_BLOCK_OBJC_EXCEPTIONS;

    [localPool release];
}

void FrameMac::scheduleClose()
{
    if (!shouldClose())
        return;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge closeWindowSoon];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::focusWindow()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // If we're a top level window, bring the window to the front.
    if (!tree()->parent())
        page()->chrome()->focus();

    // Might not have a view yet: this could be a child frame that has not yet received its first byte of data.
    // FIXME: Should remember that the frame needs focus.  See <rdar://problem/4645685>.
    if (d->m_view) {
        NSView *view = d->m_view->getDocumentView();
        if ([_bridge firstResponder] != view)
            [_bridge makeFirstResponder:view];
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::unfocusWindow()
{
    // Might not have a view yet: this could be a child frame that has not yet received its first byte of data.
    // FIXME: Should remember that the frame needs to unfocus.  See <rdar://problem/4645685>.
    if (!d->m_view)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *view = d->m_view->getDocumentView();
    if ([_bridge firstResponder] == view) {
        // If we're a top level window, deactivate the window.
        if (!tree()->parent())
            page()->chrome()->unfocus();
        else {
            // We want to shift focus to our parent.
            FrameMac* parentFrame = static_cast<FrameMac*>(tree()->parent());
            NSView* parentView = parentFrame->d->m_view->getDocumentView();
            [parentFrame->_bridge makeFirstResponder:parentView];
        }
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}
    
static NSString *findFirstMisspellingInRange(NSSpellChecker *checker, int tag, Range* searchRange, int& firstMisspellingOffset, bool markAll)
{
    ASSERT_ARG(checker, checker);
    ASSERT_ARG(searchRange, searchRange);
    
    WordAwareIterator it(searchRange);
    firstMisspellingOffset = 0;
    
    NSString *firstMisspelling = nil;
    int currentChunkOffset = 0;

    while (!it.atEnd()) {
        const UChar* chars = it.characters();
        int len = it.length();
        
        // Skip some work for one-space-char hunks
        if (!(len == 1 && chars[0] == ' ')) {
            
            NSString *chunk = [[NSString alloc] initWithCharactersNoCopy:const_cast<UChar*>(chars) length:len freeWhenDone:NO];
            NSRange misspellingNSRange = [checker checkSpellingOfString:chunk startingAt:0 language:nil wrap:NO inSpellDocumentWithTag:tag wordCount:NULL];
            NSString *misspelledWord = (misspellingNSRange.length > 0) ? [chunk substringWithRange:misspellingNSRange] : nil;
            [chunk release];
            
            if (misspelledWord) {
                
                // Remember first-encountered misspelling and its offset
                if (!firstMisspelling) {
                    firstMisspellingOffset = currentChunkOffset + misspellingNSRange.location;
                    firstMisspelling = misspelledWord;
                }
                
                // Mark this instance if we're marking all instances. Otherwise bail out because we found the first one.
                if (!markAll)
                    break;
                
                // Compute range of misspelled word
                RefPtr<Range> misspellingRange = TextIterator::subrange(searchRange, currentChunkOffset + misspellingNSRange.location, [misspelledWord length]);
                
                // Store marker for misspelled word
                ExceptionCode ec = 0;
                misspellingRange->startContainer(ec)->document()->addMarker(misspellingRange.get(), DocumentMarker::Spelling);
                ASSERT(ec == 0);
            }
        }
        
        currentChunkOffset += len;
        it.advance();
    }
    
    return firstMisspelling;
}
    
#ifndef BUILDING_ON_TIGER

static PassRefPtr<Range> paragraphAlignedRangeForRange(Range* arbitraryRange, int& offsetIntoParagraphAlignedRange, NSString*& paragraphNSString)
{
    ASSERT_ARG(arbitraryRange, arbitraryRange);
    
    ExceptionCode ec = 0;
    
    // Expand range to paragraph boundaries
    RefPtr<Range> paragraphRange = arbitraryRange->cloneRange(ec);
    setStart(paragraphRange.get(), startOfParagraph(arbitraryRange->startPosition()));
    setEnd(paragraphRange.get(), endOfParagraph(arbitraryRange->endPosition()));
    
    // Compute offset from start of expanded range to start of original range
    RefPtr<Range> offsetAsRange = new Range(paragraphRange->startContainer(ec)->document(), paragraphRange->startPosition(), arbitraryRange->startPosition());
    offsetIntoParagraphAlignedRange = TextIterator::rangeLength(offsetAsRange.get());
    
    // Fill in out parameter with autoreleased string representing entire paragraph range.
    // Someday we might have a caller that doesn't use this, but for now all callers do.
    paragraphNSString = plainText(paragraphRange.get()).getNSString();

    return paragraphRange;
}

static NSDictionary *findFirstGrammarDetailInRange(NSArray *grammarDetails, NSRange badGrammarPhraseNSRange, Range *searchRange, int startOffset, int endOffset, bool markAll)
{
    // Found some bad grammar. Find the earliest detail range that starts in our search range (if any).
    // Optionally add a DocumentMarker for each detail in the range.
    NSRange earliestDetailNSRangeSoFar = NSMakeRange(NSNotFound, 0);
    NSDictionary *earliestDetail = nil;
    for (NSDictionary *detail in grammarDetails) {
        NSValue *detailRangeAsNSValue = [detail objectForKey:NSGrammarRange];
        NSRange detailNSRange = [detailRangeAsNSValue rangeValue];
        ASSERT(detailNSRange.length > 0 && detailNSRange.location != NSNotFound);
        
        int detailStartOffsetInParagraph = badGrammarPhraseNSRange.location + detailNSRange.location;
        
        // Skip this detail if it starts before the original search range
        if (detailStartOffsetInParagraph < startOffset)
            continue;
        
        // Skip this detail if it starts after the original search range
        if (detailStartOffsetInParagraph >= endOffset)
            continue;
        
        if (markAll) {
            RefPtr<Range> badGrammarRange = TextIterator::subrange(searchRange, badGrammarPhraseNSRange.location - startOffset + detailNSRange.location, detailNSRange.length);
            ExceptionCode ec = 0;
            badGrammarRange->startContainer(ec)->document()->addMarker(badGrammarRange.get(), DocumentMarker::Grammar, [detail objectForKey:NSGrammarUserDescription]);
            ASSERT(ec == 0);
        }
        
        // Remember this detail only if it's earlier than our current candidate (the details aren't in a guaranteed order)
        if (!earliestDetail || earliestDetailNSRangeSoFar.location > detailNSRange.location) {
            earliestDetail = detail;
            earliestDetailNSRangeSoFar = detailNSRange;
        }
    }
    
    return earliestDetail;
}
    
static NSString *findFirstBadGrammarInRange(NSSpellChecker *checker, int tag, Range* searchRange, NSDictionary*& outGrammarDetail, int& outGrammarPhraseOffset, bool markAll)
{
    ASSERT_ARG(checker, checker);
    ASSERT_ARG(searchRange, searchRange);
    
    // Initialize out parameters; these will be updated if we find something to return.
    outGrammarDetail = nil;
    outGrammarPhraseOffset = 0;
    
    NSString *firstBadGrammarPhrase = nil;

    // Expand the search range to encompass entire paragraphs, since grammar checking needs that much context.
    // Determine the character offset from the start of the paragraph to the start of the original search range,
    // since we will want to ignore results in this area.
    int searchRangeStartOffset;
    NSString *paragraphNSString;
    RefPtr<Range> paragraphRange = paragraphAlignedRangeForRange(searchRange, searchRangeStartOffset, paragraphNSString);
        
    // Determine the character offset from the start of the paragraph to the end of the original search range, 
    // since we will want to ignore results in this area also.
    int searchRangeEndOffset = searchRangeStartOffset + TextIterator::rangeLength(searchRange);
        
    // Start checking from beginning of paragraph, but skip past results that occur before the start of the original search range.
    NSInteger startOffset = 0;
    while (startOffset < searchRangeEndOffset) {
        NSArray *grammarDetails;
        NSRange badGrammarPhraseNSRange = [checker checkGrammarOfString:paragraphNSString startingAt:startOffset language:nil wrap:NO inSpellDocumentWithTag:tag details:&grammarDetails];
        
        if (badGrammarPhraseNSRange.location == NSNotFound) {
            ASSERT(badGrammarPhraseNSRange.length == 0);
            return nil;
        }
        
        // Found some bad grammar. Find the earliest detail range that starts in our search range (if any).
        outGrammarDetail = findFirstGrammarDetailInRange(grammarDetails, badGrammarPhraseNSRange, searchRange, searchRangeStartOffset, searchRangeEndOffset, markAll);
        
        // If we found a detail in range, then we have found the first bad phrase (unless we found one earlier but
        // kept going so we could mark all instances).
        if (outGrammarDetail && !firstBadGrammarPhrase) {
            outGrammarPhraseOffset = badGrammarPhraseNSRange.location - searchRangeStartOffset;
            firstBadGrammarPhrase = [paragraphNSString substringWithRange:badGrammarPhraseNSRange];
            
            // Found one. We're done now, unless we're marking each instance.
            if (!markAll)
                break;
        }

        // These results were all between the start of the paragraph and the start of the search range; look
        // beyond this phrase.
        startOffset = NSMaxRange(badGrammarPhraseNSRange);
    }
    
    return firstBadGrammarPhrase;
}
    
#endif /* not BUILDING_ON_TIGER */

void FrameMac::advanceToNextMisspelling(bool startBeforeSelection)
{
    ExceptionCode ec = 0;

    // The basic approach is to search in two phases - from the selection end to the end of the doc, and
    // then we wrap and search from the doc start to (approximately) where we started.
    
    // Start at the end of the selection, search to edge of document.  Starting at the selection end makes
    // repeated "check spelling" commands work.
    Selection selection(selectionController()->selection());
    RefPtr<Range> spellingSearchRange(rangeOfContents(document()));
    bool startedWithSelection = false;
    if (selection.start().node()) {
        startedWithSelection = true;
        if (startBeforeSelection) {
            VisiblePosition start(selection.visibleStart());
            // We match AppKit's rule: Start 1 character before the selection.
            VisiblePosition oneBeforeStart = start.previous();
            setStart(spellingSearchRange.get(), oneBeforeStart.isNotNull() ? oneBeforeStart : start);
        } else
            setStart(spellingSearchRange.get(), selection.visibleEnd());
    }

    // If we're not in an editable node, try to find one, make that our range to work in
    Node *editableNode = spellingSearchRange->startContainer(ec);
    if (!editableNode->isContentEditable()) {
        editableNode = editableNode->nextEditable();
        if (!editableNode)
            return;

        spellingSearchRange->setStartBefore(editableNode, ec);
        startedWithSelection = false;   // won't need to wrap
    }
    
    // topNode defines the whole range we want to operate on 
    Node *topNode = editableNode->rootEditableElement();
    spellingSearchRange->setEnd(topNode, maxDeepOffset(topNode), ec);

    // If spellingSearchRange starts in the middle of a word, advance to the next word so we start checking
    // at a word boundary. Going back by one char and then forward by a word does the trick.
    if (startedWithSelection) {
        VisiblePosition oneBeforeStart = startVisiblePosition(spellingSearchRange.get(), DOWNSTREAM).previous();
        if (oneBeforeStart.isNotNull()) {
            setStart(spellingSearchRange.get(), endOfWord(oneBeforeStart));
        } // else we were already at the start of the editable node
    }
    
    if (spellingSearchRange->collapsed(ec))
        return;       // nothing to search in
    
    // Get the spell checker if it is available
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker)
        return;
        
    // We go to the end of our first range instead of the start of it, just to be sure
    // we don't get foiled by any word boundary problems at the start.  It means we might
    // do a tiny bit more searching.
    Node *searchEndNodeAfterWrap = spellingSearchRange->endContainer(ec);
    int searchEndOffsetAfterWrap = spellingSearchRange->endOffset(ec);
    
    int misspellingOffset;
    NSString *misspelledWord = findFirstMisspellingInRange(checker, editor()->spellCheckerDocumentTag(), spellingSearchRange.get(), misspellingOffset, false);
    
    NSString *badGrammarPhrase = nil;

#ifndef BUILDING_ON_TIGER
    int grammarPhraseOffset;
    NSDictionary *grammarDetail = nil;

    // Search for bad grammar that occurs prior to the next misspelled word (if any)
    RefPtr<Range> grammarSearchRange = spellingSearchRange->cloneRange(ec);
    if (misspelledWord) {
        // Stop looking at start of next misspelled word
        CharacterIterator chars(grammarSearchRange.get());
        chars.advance(misspellingOffset);
        grammarSearchRange->setEnd(chars.range()->startContainer(ec), chars.range()->startOffset(ec), ec);
    }
    
    if (editor()->isGrammarCheckingEnabled())
        badGrammarPhrase = findFirstBadGrammarInRange(checker, editor()->spellCheckerDocumentTag(), grammarSearchRange.get(), grammarDetail, grammarPhraseOffset, false);
#endif
    
    // If we found neither bad grammar nor a misspelled word, wrap and try again (but don't bother if we started at the beginning of the
    // block rather than at a selection).
    if (startedWithSelection && !misspelledWord && !badGrammarPhrase) {
        spellingSearchRange->setStart(topNode, 0, ec);
        // going until the end of the very first chunk we tested is far enough
        spellingSearchRange->setEnd(searchEndNodeAfterWrap, searchEndOffsetAfterWrap, ec);
        
        misspelledWord = findFirstMisspellingInRange(checker, editor()->spellCheckerDocumentTag(), spellingSearchRange.get(), misspellingOffset, false);

#ifndef BUILDING_ON_TIGER
        grammarSearchRange = spellingSearchRange->cloneRange(ec);
        if (misspelledWord) {
            // Stop looking at start of next misspelled word
            CharacterIterator chars(grammarSearchRange.get());
            chars.advance(misspellingOffset);
            grammarSearchRange->setEnd(chars.range()->startContainer(ec), chars.range()->startOffset(ec), ec);
        }
        if (editor()->isGrammarCheckingEnabled())
            badGrammarPhrase = findFirstBadGrammarInRange(checker, editor()->spellCheckerDocumentTag(), grammarSearchRange.get(), grammarDetail, grammarPhraseOffset, false);
#endif
    }
    
    if (badGrammarPhrase) {
#ifdef BUILDING_ON_TIGER
        ASSERT_NOT_REACHED();
#else
        // We found bad grammar. Since we only searched for bad grammar up to the first misspelled word, the bad grammar
        // takes precedence and we ignore any potential misspelled word. Select the grammar detail, update the spelling
        // panel, and store a marker so we draw the green squiggle later.
        
        ASSERT([badGrammarPhrase length] > 0);
        ASSERT(grammarDetail);
        NSValue *detailRangeAsNSValue = [grammarDetail objectForKey:NSGrammarRange];
        ASSERT(detailRangeAsNSValue);
        NSRange detailNSRange = [detailRangeAsNSValue rangeValue];
        ASSERT(detailNSRange.location != NSNotFound && detailNSRange.length > 0);
        
        // FIXME 4859190: This gets confused with doubled punctuation at the end of a paragraph
        RefPtr<Range> badGrammarRange = TextIterator::subrange(grammarSearchRange.get(), grammarPhraseOffset + detailNSRange.location, detailNSRange.length);
        selectionController()->setSelection(Selection(badGrammarRange.get(), SEL_DEFAULT_AFFINITY));
        revealSelection();
        
        [checker updateSpellingPanelWithGrammarString:badGrammarPhrase detail:grammarDetail];
        document()->addMarker(badGrammarRange.get(), DocumentMarker::Grammar, [grammarDetail objectForKey:NSGrammarUserDescription]);
#endif        
    } else if (misspelledWord) {
        // We found a misspelling, but not any earlier bad grammar. Select the misspelling, update the spelling panel, and store
        // a marker so we draw the red squiggle later.
        
        RefPtr<Range> misspellingRange = TextIterator::subrange(spellingSearchRange.get(), misspellingOffset, [misspelledWord length]);
        selectionController()->setSelection(Selection(misspellingRange.get(), DOWNSTREAM));
        revealSelection();
        
        [checker updateSpellingPanelWithMisspelledWord:misspelledWord];
        document()->addMarker(misspellingRange.get(), DocumentMarker::Spelling);
    }
}

String FrameMac::mimeTypeForFileName(const String& fileName) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [_bridge MIMETypeForPath:fileName];
    END_BLOCK_OBJC_EXCEPTIONS;

    return String();
}

KJS::Bindings::RootObject *FrameMac::executionContextForDOM()
{
    if (!javaScriptEnabled())
        return 0;

    return bindingRootObject();
}

KJS::Bindings::RootObject *FrameMac::bindingRootObject()
{
    assert(javaScriptEnabled());
    if (!_bindingRoot) {
        JSLock lock;
        _bindingRoot = new KJS::Bindings::RootObject(0);    // The root gets deleted by JavaScriptCore.
        KJS::JSObject *win = KJS::Window::retrieveWindow(this);
        _bindingRoot->setRootObjectImp (win);
        _bindingRoot->setInterpreter(scriptProxy()->interpreter());
        addPluginRootObject (_bindingRoot);
    }
    return _bindingRoot;
}

WebScriptObject *FrameMac::windowScriptObject()
{
    if (!javaScriptEnabled())
        return 0;

    if (!_windowScriptObject) {
        KJS::JSLock lock;
        KJS::JSObject *win = KJS::Window::retrieveWindow(this);
        _windowScriptObject = HardRetainWithNSRelease([[WebScriptObject alloc] _initWithJSObject:win originExecutionContext:bindingRootObject() executionContext:bindingRootObject()]);
    }

    return _windowScriptObject;
}

NPObject *FrameMac::windowScriptNPObject()
{
    if (!_windowScriptNPObject) {
        if (javaScriptEnabled()) {
            // JavaScript is enabled, so there is a JavaScript window object.  Return an NPObject bound to the window
            // object.
            KJS::JSObject *win = KJS::Window::retrieveWindow(this);
            assert(win);
            _windowScriptNPObject = _NPN_CreateScriptObject(0, win, bindingRootObject(), bindingRootObject());
        } else {
            // JavaScript is not enabled, so we cannot bind the NPObject to the JavaScript window object.
            // Instead, we create an NPObject of a different class, one which is not bound to a JavaScript object.
            _windowScriptNPObject = _NPN_CreateNoScriptObject();
        }
    }

    return _windowScriptNPObject;
}

WebCoreFrameBridge *FrameMac::bridgeForWidget(const Widget* widget)
{
    ASSERT_ARG(widget, widget);
    
    FrameMac* frame = Mac(frameForWidget(widget));
    ASSERT(frame);
    return frame->_bridge;
}

void FrameMac::runJavaScriptAlert(const String& message)
{
    String text = message;
    text.replace('\\', backslashAsCurrencySymbol());
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge runJavaScriptAlertPanelWithMessage:text];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool FrameMac::runJavaScriptConfirm(const String& message)
{
    String text = message;
    text.replace('\\', backslashAsCurrencySymbol());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [_bridge runJavaScriptConfirmPanelWithMessage:text];
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

bool FrameMac::runJavaScriptPrompt(const String& prompt, const String& defaultValue, String& result)
{
    String promptText = prompt;
    promptText.replace('\\', backslashAsCurrencySymbol());
    String defaultValueText = defaultValue;
    defaultValueText.replace('\\', backslashAsCurrencySymbol());

    bool ok;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSString *returnedText = nil;

    ok = [_bridge runJavaScriptTextInputPanelWithPrompt:prompt
        defaultText:defaultValue returningText:&returnedText];

    if (ok) {
        result = String(returnedText);
        result.replace(backslashAsCurrencySymbol(), '\\');
    }

    return ok;
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return false;
}

bool FrameMac::shouldInterruptJavaScript()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [_bridge shouldInterruptJavaScript];
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return false;
}

NSImage *FrameMac::imageFromRect(NSRect rect) const
{
    NSView *view = d->m_view->getDocumentView();
    if (!view)
        return nil;
    
    NSImage *resultImage;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    NSRect bounds = [view bounds];
    
    // Round image rect size in window coordinate space to avoid pixel cracks at HiDPI (4622794)
    rect = [view convertRect:rect toView:nil];
    rect.size.height = roundf(rect.size.height);
    rect.size.width = roundf(rect.size.width);
    rect = [view convertRect:rect fromView:nil];
    
    resultImage = [[[NSImage alloc] initWithSize:rect.size] autorelease];

    if (rect.size.width != 0 && rect.size.height != 0) {
        [resultImage setFlipped:YES];
        [resultImage lockFocus];

        CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

        CGContextSaveGState(context);
        CGContextTranslateCTM(context, bounds.origin.x - rect.origin.x, bounds.origin.y - rect.origin.y);
        [view drawRect:rect];
        CGContextRestoreGState(context);
        [resultImage unlockFocus];
        [resultImage setFlipped:NO];
    }

    return resultImage;

    END_BLOCK_OBJC_EXCEPTIONS;
    
    return nil;
}

NSImage* FrameMac::selectionImage(bool forceWhiteText) const
{
    d->m_paintRestriction = forceWhiteText ? PaintRestrictionSelectionOnlyWhiteText : PaintRestrictionSelectionOnly;
    NSImage *result = imageFromRect(visibleSelectionRect());
    d->m_paintRestriction = PaintRestrictionNone;
    return result;
}

NSImage *FrameMac::snapshotDragImage(Node *node, NSRect *imageRect, NSRect *elementRect) const
{
    RenderObject *renderer = node->renderer();
    if (!renderer)
        return nil;
    
    renderer->updateDragState(true);    // mark dragged nodes (so they pick up the right CSS)
    d->m_doc->updateLayout();        // forces style recalc - needed since changing the drag state might
                                        // imply new styles, plus JS could have changed other things
    IntRect topLevelRect;
    NSRect paintingRect = renderer->paintingRootRect(topLevelRect);

    d->m_elementToDraw = node;              // invoke special sub-tree drawing mode
    NSImage *result = imageFromRect(paintingRect);
    renderer->updateDragState(false);
    d->m_doc->updateLayout();
    d->m_elementToDraw = 0;

    if (elementRect)
        *elementRect = topLevelRect;
    if (imageRect)
        *imageRect = paintingRect;
    return result;
}

NSFont *FrameMac::fontForSelection(bool *hasMultipleFonts) const
{
    if (hasMultipleFonts)
        *hasMultipleFonts = false;

    if (!selectionController()->isRange()) {
        Node *nodeToRemove;
        RenderStyle *style = styleForSelectionStart(nodeToRemove); // sets nodeToRemove

        NSFont *result = 0;
        if (style)
            result = style->font().primaryFont()->getNSFont();
        
        if (nodeToRemove) {
            ExceptionCode ec;
            nodeToRemove->remove(ec);
            ASSERT(ec == 0);
        }

        return result;
    }

    NSFont *font = nil;

    RefPtr<Range> range = selectionController()->toRange();
    Node *startNode = range->editingStartPosition().node();
    if (startNode != nil) {
        Node *pastEnd = range->pastEndNode();
        // In the loop below, n should eventually match pastEnd and not become nil, but we've seen at least one
        // unreproducible case where this didn't happen, so check for nil also.
        for (Node *n = startNode; n && n != pastEnd; n = n->traverseNextNode()) {
            RenderObject *renderer = n->renderer();
            if (!renderer)
                continue;
            // FIXME: Are there any node types that have renderers, but that we should be skipping?
            NSFont *f = renderer->style()->font().primaryFont()->getNSFont();
            if (!font) {
                font = f;
                if (!hasMultipleFonts)
                    break;
            } else if (font != f) {
                *hasMultipleFonts = true;
                break;
            }
        }
    }

    return font;
}

NSDictionary *FrameMac::fontAttributesForSelectionStart() const
{
    Node *nodeToRemove;
    RenderStyle *style = styleForSelectionStart(nodeToRemove);
    if (!style)
        return nil;

    NSMutableDictionary *result = [NSMutableDictionary dictionary];

    if (style->backgroundColor().isValid() && style->backgroundColor().alpha() != 0)
        [result setObject:nsColor(style->backgroundColor()) forKey:NSBackgroundColorAttributeName];

    if (style->font().primaryFont()->getNSFont())
        [result setObject:style->font().primaryFont()->getNSFont() forKey:NSFontAttributeName];

    if (style->color().isValid() && style->color() != Color::black)
        [result setObject:nsColor(style->color()) forKey:NSForegroundColorAttributeName];

    ShadowData *shadow = style->textShadow();
    if (shadow) {
        NSShadow *s = [[NSShadow alloc] init];
        [s setShadowOffset:NSMakeSize(shadow->x, shadow->y)];
        [s setShadowBlurRadius:shadow->blur];
        [s setShadowColor:nsColor(shadow->color)];
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

NSWritingDirection FrameMac::baseWritingDirectionForSelectionStart() const
{
    NSWritingDirection result = NSWritingDirectionLeftToRight;

    Position pos = selectionController()->selection().visibleStart().deepEquivalent();
    Node *node = pos.node();
    if (!node || !node->renderer() || !node->renderer()->containingBlock())
        return result;
    RenderStyle *style = node->renderer()->containingBlock()->style();
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

void FrameMac::setBridge(WebCoreFrameBridge *bridge)
{ 
    if (_bridge == bridge)
        return;

    HardRetain(bridge);
    HardRelease(_bridge);
    _bridge = bridge;
}

void FrameMac::print()
{
    [Mac(this)->_bridge print];
}

KJS::Bindings::Instance *FrameMac::getAppletInstanceForWidget(Widget *widget)
{
    NSView *aView = widget->getView();
    if (!aView)
        return 0;
    jobject applet;
    
    // Get a pointer to the actual Java applet instance.
    if ([_bridge respondsToSelector:@selector(getAppletInView:)])
        applet = [_bridge getAppletInView:aView];
    else
        applet = [_bridge pollForAppletInView:aView];
    
    if (applet) {
        // Wrap the Java instance in a language neutral binding and hand
        // off ownership to the APPLET element.
        KJS::Bindings::RootObject *executionContext = KJS::Bindings::RootObject::findRootObjectForNativeHandleFunction ()(aView);
        KJS::Bindings::Instance *instance = KJS::Bindings::Instance::createBindingForLanguageInstance (KJS::Bindings::Instance::JavaLanguage, applet, executionContext);        
        return instance;
    }
    
    return 0;
}

static KJS::Bindings::Instance *getInstanceForView(NSView *aView)
{
    if ([aView respondsToSelector:@selector(objectForWebScript)]){
        id object = [aView objectForWebScript];
        if (object) {
            KJS::Bindings::RootObject *executionContext = KJS::Bindings::RootObject::findRootObjectForNativeHandleFunction ()(aView);
            return KJS::Bindings::Instance::createBindingForLanguageInstance (KJS::Bindings::Instance::ObjectiveCLanguage, object, executionContext);
        }
    }
    else if ([aView respondsToSelector:@selector(createPluginScriptableObject)]) {
        NPObject *object = [aView createPluginScriptableObject];
        if (object) {
            KJS::Bindings::RootObject *executionContext = KJS::Bindings::RootObject::findRootObjectForNativeHandleFunction()(aView);
            KJS::Bindings::Instance *instance = KJS::Bindings::Instance::createBindingForLanguageInstance(KJS::Bindings::Instance::CLanguage, object, executionContext);
            
            // -createPluginScriptableObject returns a retained NPObject.  The caller is expected to release it.
            _NPN_ReleaseObject(object);
            
            return instance;
        }
    }
    return 0;
}

KJS::Bindings::Instance *FrameMac::getEmbedInstanceForWidget(Widget *widget)
{
    return getInstanceForView(widget->getView());
}

KJS::Bindings::Instance *FrameMac::getObjectInstanceForWidget(Widget *widget)
{
    return getInstanceForView(widget->getView());
}

void FrameMac::addPluginRootObject(KJS::Bindings::RootObject *root)
{
    m_rootObjects.append(root);
}

void FrameMac::cleanupPluginObjects()
{
    // Delete old plug-in data structures
    JSLock lock;
    
    unsigned count = m_rootObjects.size();
    for (unsigned i = 0; i < count; i++)
        m_rootObjects[i]->removeAllNativeReferences();
    m_rootObjects.clear();
    
    _bindingRoot = 0;
    HardRelease(_windowScriptObject);
    _windowScriptObject = 0;
    
    if (_windowScriptNPObject) {
        // Call _NPN_DeallocateObject() instead of _NPN_ReleaseObject() so that we don't leak if a plugin fails to release the window
        // script object properly.
        // This shouldn't cause any problems for plugins since they should have already been stopped and destroyed at this point.
        _NPN_DeallocateObject(_windowScriptNPObject);
        _windowScriptNPObject = 0;
    }
}

void FrameMac::issueCutCommand()
{
    [_bridge issueCutCommand];
}

void FrameMac::issueCopyCommand()
{
    [_bridge issueCopyCommand];
}

void FrameMac::issuePasteCommand()
{
    [_bridge issuePasteCommand];
}

void FrameMac::issuePasteAndMatchStyleCommand()
{
    [_bridge issuePasteAndMatchStyleCommand];
}

void FrameMac::issueTransposeCommand()
{
    [_bridge issueTransposeCommand];
}

bool FrameMac::isSelectionMisspelled()
{
    String selectedString = selectedText();
    unsigned length = selectedString.length();
    if (length == 0)
        return false;
    
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker)
        return false;
    
    NSRange range = [checker checkSpellingOfString:selectedString
                                        startingAt:0
                                          language:nil
                                              wrap:NO 
                            inSpellDocumentWithTag:editor()->spellCheckerDocumentTag() 
                                         wordCount:NULL];
    
    // The selection only counts as misspelled if the selected text is exactly one misspelled word
    if (range.length != length)
        return false;
    
    // Update the spelling panel to be displaying this error (whether or not the spelling panel is on screen).
    // This is necessary to make a subsequent call to [NSSpellChecker ignoreWord:inSpellDocumentWithTag:] work
    // correctly; that call behaves differently based on whether the spelling panel is displaying a misspelling
    // or a grammar error.
    [checker updateSpellingPanelWithMisspelledWord:selectedString];
    
    return true;
}
    
#ifndef BUILDING_ON_TIGER
static bool isRangeUngrammatical(int tag, Range *range, Vector<String>& guessesVector)
{
    // Returns true only if the passed range exactly corresponds to a bad grammar detail range. This is analogous
    // to isSelectionMisspelled. It's not good enough for there to be some bad grammar somewhere in the range,
    // or overlapping the range; the ranges must exactly match.
    guessesVector.clear();
    NSDictionary *grammarDetail;
    int grammarPhraseOffset;
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker)
        return false;
    
    NSString *badGrammarPhrase = findFirstBadGrammarInRange(checker, tag, range, grammarDetail, grammarPhraseOffset, false);    
    
    // No bad grammar in these parts at all.
    if (!badGrammarPhrase)
        return false;
    
    // Bad grammar, but phrase (e.g. sentence) starts beyond start of range.
    if (grammarPhraseOffset > 0)
        return false;
    
    ASSERT(grammarDetail);
    NSValue *detailRangeAsNSValue = [grammarDetail objectForKey:NSGrammarRange];
    ASSERT(detailRangeAsNSValue);
    NSRange detailNSRange = [detailRangeAsNSValue rangeValue];
    ASSERT(detailNSRange.location != NSNotFound && detailNSRange.length > 0);
    
    // Bad grammar, but start of detail (e.g. ungrammatical word) doesn't match start of range
    if (detailNSRange.location + grammarPhraseOffset != 0)
        return false;
    
    // Bad grammar at start of range, but end of bad grammar is before or after end of range
    if ((int)detailNSRange.length != TextIterator::rangeLength(range))
        return false;
    
    NSArray *guesses = [grammarDetail objectForKey:NSGrammarCorrections];
    for (NSString *guess in guesses)
        guessesVector.append(guess);

    // Update the spelling panel to be displaying this error (whether or not the spelling panel is on screen).
    // This is necessary to make a subsequent call to [NSSpellChecker ignoreWord:inSpellDocumentWithTag:] work
    // correctly; that call behaves differently based on whether the spelling panel is displaying a misspelling
    // or a grammar error.
    [checker updateSpellingPanelWithGrammarString:badGrammarPhrase detail:grammarDetail];

    return true;
}
#endif
    
bool FrameMac::isSelectionUngrammatical()
{
#ifdef BUILDING_ON_TIGER
    return false;
#else
    Vector<String> ignoredGuesses;
    return isRangeUngrammatical(editor()->spellCheckerDocumentTag(), selectionController()->toRange().get(), ignoredGuesses);
#endif
}

Vector<String> FrameMac::guessesForUngrammaticalSelection()
{
#ifdef BUILDING_ON_TIGER
    return Vector<String>();
#else
    Vector<String> guesses;
    // Ignore the result of isRangeUngrammatical; we just want the guesses, whether or not there are any
    isRangeUngrammatical(editor()->spellCheckerDocumentTag(), selectionController()->toRange().get(), guesses);
    return guesses;
#endif
}

static Vector<String> core(NSArray* stringsArray)
{
    Vector<String> stringsVector = Vector<String>();
    unsigned count = [stringsArray count];
    if (count > 0) {
        NSEnumerator* enumerator = [stringsArray objectEnumerator];
        NSString* string;
        while ((string = [enumerator nextObject]) != nil)
            stringsVector.append(string);
    }
    return stringsVector;
}

Vector<String> FrameMac::guessesForMisspelledSelection()
{
    String selectedString = selectedText();
    ASSERT(selectedString.length() != 0);
    return core([[NSSpellChecker sharedSpellChecker] guessesForWord:selectedString]);
}

void FrameMac::markMisspellingsInAdjacentWords(const VisiblePosition &p)
{
    if (!editor()->isContinuousSpellCheckingEnabled())
        return;
    markMisspellings(Selection(startOfWord(p, LeftWordIfOnBoundary), endOfWord(p, RightWordIfOnBoundary)));
}
    
static void markAllMisspellingsInRange(NSSpellChecker *checker, int tag, Range* searchRange)
{
    // Use the "markAll" feature of findFirstMisspellingInRange. Ignore the return value and the "out parameter";
    // all we need to do is mark every instance.
    int ignoredOffset;
    findFirstMisspellingInRange(checker, tag, searchRange, ignoredOffset, true);
}

#ifndef BUILDING_ON_TIGER
static void markAllBadGrammarInRange(NSSpellChecker *checker, int tag, Range* searchRange)
{
    // Use the "markAll" feature of findFirstBadGrammarInRange. Ignore the return value and "out parameters"; all we need to
    // do is mark every instance.
    NSDictionary *ignoredGrammarDetail;
    int ignoredOffset;
    findFirstBadGrammarInRange(checker, tag, searchRange, ignoredGrammarDetail, ignoredOffset, true);
}
#endif

void FrameMac::markMisspellings(const Selection& selection)
{
    // This function is called with a selection already expanded to word boundaries.
    // Might be nice to assert that here.

    if (!editor()->isContinuousSpellCheckingEnabled())
        return;

    RefPtr<Range> searchRange(selection.toRange());
    if (!searchRange || searchRange->isDetached())
        return;
    
    // If we're not in an editable node, bail.
    int exception = 0;
    Node *editableNode = searchRange->startContainer(exception);
    if (!editableNode->isContentEditable())
        return;
    
    // Get the spell checker if it is available
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (checker == nil)
        return;
    
    markAllMisspellingsInRange(checker, editor()->spellCheckerDocumentTag(), searchRange.get());
    
#ifndef BUILDING_ON_TIGER
    if (editor()->isGrammarCheckingEnabled())
        markAllBadGrammarInRange(checker, editor()->spellCheckerDocumentTag(), searchRange.get());
#endif
}

void FrameMac::respondToChangedSelection(const Selection &oldSelection, bool closeTyping)
{
    if (document()) {
        if (editor()->isContinuousSpellCheckingEnabled()) {
            Selection oldAdjacentWords;
            
            // If this is a change in selection resulting from a delete operation, oldSelection may no longer
            // be in the document.
            if (oldSelection.start().node() && oldSelection.start().node()->inDocument()) {
                VisiblePosition oldStart(oldSelection.visibleStart());
                oldAdjacentWords = Selection(startOfWord(oldStart, LeftWordIfOnBoundary), endOfWord(oldStart, RightWordIfOnBoundary));   
            }

            VisiblePosition newStart(selectionController()->selection().visibleStart());
            Selection newAdjacentWords(startOfWord(newStart, LeftWordIfOnBoundary), endOfWord(newStart, RightWordIfOnBoundary));

            // When typing we check spelling elsewhere, so don't redo it here.
            if (closeTyping && oldAdjacentWords != newAdjacentWords)
                markMisspellings(oldAdjacentWords);

            // This only erases a marker in the first word of the selection.
            // Perhaps peculiar, but it matches AppKit.
            document()->removeMarkers(newAdjacentWords.toRange().get(), DocumentMarker::Spelling);
            document()->removeMarkers(newAdjacentWords.toRange().get(), DocumentMarker::Grammar);
        } else {
            // When continuous spell checking is off, existing markers disappear after the selection changes.
            document()->removeMarkers(DocumentMarker::Spelling);
            document()->removeMarkers(DocumentMarker::Grammar);
        }
    }

    [_bridge respondToChangedSelection];
}

bool FrameMac::shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const
{
    return [_bridge shouldChangeSelectedDOMRange:[DOMRange _rangeWith:oldSelection.toRange().get()]
                                      toDOMRange:[DOMRange _rangeWith:newSelection.toRange().get()]
                                        affinity:affinity
                                  stillSelecting:stillSelecting];
}

bool FrameMac::shouldDeleteSelection(const Selection& selection) const
{
    return [_bridge shouldDeleteSelectedDOMRange:[DOMRange _rangeWith:selection.toRange().get()]];
}

bool FrameMac::isContentEditable() const
{
    return Frame::isContentEditable() || [_bridge isEditable];
}

void FrameMac::textFieldDidBeginEditing(Element* input)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge textFieldDidBeginEditing:(DOMHTMLInputElement *)[DOMElement _elementWith:input]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::textFieldDidEndEditing(Element* input)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge textFieldDidEndEditing:(DOMHTMLInputElement *)[DOMElement _elementWith:input]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::textDidChangeInTextField(Element* input)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge textDidChangeInTextField:(DOMHTMLInputElement *)[DOMElement _elementWith:input]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::textDidChangeInTextArea(Element* textarea)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge textDidChangeInTextArea:(DOMHTMLTextAreaElement *)[DOMElement _elementWith:textarea]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool FrameMac::doTextFieldCommandFromEvent(Element* input, KeyboardEvent* event)
{
    bool result = false;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    SEL selector = selectorForKeyEvent(event);
    if (selector)
        result = [_bridge textField:(DOMHTMLInputElement *)[DOMElement _elementWith:input] doCommandBySelector:selector];
    END_BLOCK_OBJC_EXCEPTIONS;
    return result;
}

void FrameMac::textWillBeDeletedInTextField(Element* input)
{
    // We're using the deleteBackward selector for all deletion operations since the autofill code treats all deletions the same way.
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge textField:(DOMHTMLInputElement *)[DOMElement _elementWith:input] doCommandBySelector:@selector(deleteBackward:)];
    END_BLOCK_OBJC_EXCEPTIONS;
}

const short enableRomanKeyboardsOnly = -23;
void FrameMac::setSecureKeyboardEntry(bool enable)
{
    if (enable) {
        EnableSecureEventInput();
// FIXME: KeyScript is deprecated in Leopard, we need a new solution for this <rdar://problem/4727607>
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
        KeyScript(enableRomanKeyboardsOnly);
#endif
    } else {
        DisableSecureEventInput();
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
        KeyScript(smKeyEnableKybds);
#endif
    }
}

bool FrameMac::isSecureKeyboardEntry()
{
    return IsSecureEventInputEnabled();
}

static void convertAttributesToUnderlines(Vector<MarkedTextUnderline>& result, const Range *markedTextRange, NSArray *attributes, NSArray *ranges)
{
    int exception = 0;
    int baseOffset = markedTextRange->startOffset(exception);

    unsigned length = [attributes count];
    ASSERT([ranges count] == length);

    for (unsigned i = 0; i < length; i++) {
        NSNumber *style = [[attributes objectAtIndex:i] objectForKey:NSUnderlineStyleAttributeName];
        if (!style)
            continue;
        NSRange range = [[ranges objectAtIndex:i] rangeValue];
        NSColor *color = [[attributes objectAtIndex:i] objectForKey:NSUnderlineColorAttributeName];
        Color qColor = Color::black;
        if (color) {
            NSColor* deviceColor = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
            qColor = Color(makeRGBA((int)(255 * [deviceColor redComponent]),
                                    (int)(255 * [deviceColor blueComponent]),
                                    (int)(255 * [deviceColor greenComponent]),
                                    (int)(255 * [deviceColor alphaComponent])));
        }

        result.append(MarkedTextUnderline(range.location + baseOffset, 
                                          range.location + baseOffset + range.length, 
                                          qColor,
                                          [style intValue] > 1));
    }
}

void FrameMac::setMarkedTextRange(const Range *range, NSArray *attributes, NSArray *ranges)
{
    int exception = 0;

    ASSERT(!range || range->startContainer(exception) == range->endContainer(exception));
    ASSERT(!range || range->collapsed(exception) || range->startContainer(exception)->isTextNode());

    d->m_markedTextUnderlines.clear();
    if (attributes == nil)
        d->m_markedTextUsesUnderlines = false;
    else {
        d->m_markedTextUsesUnderlines = true;
        convertAttributesToUnderlines(d->m_markedTextUnderlines, range, attributes, ranges);
    }

    if (m_markedTextRange.get() && document() && m_markedTextRange->startContainer(exception)->renderer())
        m_markedTextRange->startContainer(exception)->renderer()->repaint();

    if (range && range->collapsed(exception))
        m_markedTextRange = 0;
    else
        m_markedTextRange = const_cast<Range*>(range);

    if (m_markedTextRange.get() && document() && m_markedTextRange->startContainer(exception)->renderer())
        m_markedTextRange->startContainer(exception)->renderer()->repaint();
}

NSMutableDictionary *FrameMac::dashboardRegionsDictionary()
{
    Document *doc = document();
    if (!doc)
        return nil;

    const Vector<DashboardRegionValue>& regions = doc->dashboardRegions();
    size_t n = regions.size();

    // Convert the Vector<DashboardRegionValue> into a NSDictionary of WebDashboardRegions
    NSMutableDictionary *webRegions = [NSMutableDictionary dictionaryWithCapacity:n];
    for (size_t i = 0; i < n; i++) {
        const DashboardRegionValue& region = regions[i];

        if (region.type == StyleDashboardRegion::None)
            continue;
        
        NSString *label = region.label;
        WebDashboardRegionType type = WebDashboardRegionTypeNone;
        if (region.type == StyleDashboardRegion::Circle)
            type = WebDashboardRegionTypeCircle;
        else if (region.type == StyleDashboardRegion::Rectangle)
            type = WebDashboardRegionTypeRectangle;
        NSMutableArray *regionValues = [webRegions objectForKey:label];
        if (!regionValues) {
            regionValues = [[NSMutableArray alloc] initWithCapacity:1];
            [webRegions setObject:regionValues forKey:label];
            [regionValues release];
        }
        
        WebDashboardRegion *webRegion = [[WebDashboardRegion alloc] initWithRect:region.bounds clip:region.clip type:type];
        [regionValues addObject:webRegion];
        [webRegion release];
    }
    
    return webRegions;
}

void FrameMac::dashboardRegionsChanged()
{
    NSMutableDictionary *webRegions = dashboardRegionsDictionary();
    [_bridge dashboardRegionsChanged:webRegions];
}

void FrameMac::willPopupMenu(NSMenu * menu)
{
    [_bridge willPopupMenu:menu];
}

bool FrameMac::isCharacterSmartReplaceExempt(UChar c, bool isPreviousChar)
{
    return [_bridge isCharacterSmartReplaceExempt:c isPreviousCharacter:isPreviousChar];
}

bool FrameMac::shouldClose()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (![_bridge canRunBeforeUnloadConfirmPanel])
        return true;

    RefPtr<Document> doc = document();
    if (!doc)
        return true;
    HTMLElement* body = doc->body();
    if (!body)
        return true;

    RefPtr<BeforeUnloadEvent> event = new BeforeUnloadEvent;
    event->setTarget(doc);
    doc->handleWindowEvent(event.get(), false);

    if (!event->defaultPrevented() && doc)
        doc->defaultEventHandler(event.get());
    if (event->result().isNull())
        return true;

    String text = event->result();
    text.replace('\\', backslashAsCurrencySymbol());

    return [_bridge runBeforeUnloadConfirmPanelWithMessage:text];

    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

void Frame::setNeedsReapplyStyles()
{
    [Mac(this)->_bridge setNeedsReapplyStyles];
}

FloatRect FrameMac::customHighlightLineRect(const AtomicString& type, const FloatRect& lineRect)
{
    return [_bridge customHighlightRect:type forLine:lineRect];
}

void FrameMac::paintCustomHighlight(const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect, bool text, bool line)
{
    [_bridge paintCustomHighlight:type forBox:boxRect onLine:lineRect behindText:text entireLine:line];
}

} // namespace WebCore
