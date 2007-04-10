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
#import "TypingCommand.h"
#import "TextIterator.h"
#import "htmlediting.h"
#import "visible_units.h"

#ifdef BUILDING_ON_TIGER
@interface NSSpellChecker (NotYetPublicMethods)
- (void)learnWord:(NSString *)word;
@end
#endif

namespace WebCore {

extern "C" {

// Kill ring calls. Would be better to use NSKillRing.h, but that's not available in SPI.

void _NSAppendToKillRing(NSString *);
void _NSPrependToKillRing(NSString *);
void _NSNewKillRingSequence(void);
}

PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy)
{
    return new ClipboardMac(false, [NSPasteboard generalPasteboard], policy);
}

NSString* Editor::userVisibleString(NSURL* nsURL)
{
    if (client())
        return client()->userVisibleString(nsURL);
    return nil;
}

void Editor::addToKillRing(Range* range, bool prepend)
{
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

void Editor::ignoreSpelling()
{
    String text = frame()->selectedText();
    ASSERT(text.length() != 0);
    [[NSSpellChecker sharedSpellChecker] ignoreWord:text 
                             inSpellDocumentWithTag:spellCheckerDocumentTag()];
}

void Editor::learnSpelling()
{
    String text = frame()->selectedText();
    ASSERT(text.length() != 0);
    [[NSSpellChecker sharedSpellChecker] learnWord:text];
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

void Editor::advanceToNextMisspelling(bool startBeforeSelection)
{
    ExceptionCode ec = 0;

    // The basic approach is to search in two phases - from the selection end to the end of the doc, and
    // then we wrap and search from the doc start to (approximately) where we started.
    
    // Start at the end of the selection, search to edge of document.  Starting at the selection end makes
    // repeated "check spelling" commands work.
    Selection selection(frame()->selectionController()->selection());
    RefPtr<Range> spellingSearchRange(rangeOfContents(frame()->document()));
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

    Position position = spellingSearchRange->startPosition();
    if (!isEditablePosition(position)) {
        // This shouldn't happen in practice because the Spelling menu items aren't enabled unless the
        // selection is editable.
        position = firstEditablePositionAfterPositionInRoot(position, frame()->document()->documentElement()).deepEquivalent();
        if (position.isNull())
            return;
        
        spellingSearchRange->setStart(position.node(), position.offset(), ec);
        startedWithSelection = false;   // won't need to wrap
    }
    
    // topNode defines the whole range we want to operate on 
    Node* topNode = highestEditableRoot(position);
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
    NSString *misspelledWord = findFirstMisspellingInRange(checker, spellCheckerDocumentTag(), spellingSearchRange.get(), misspellingOffset, false);
    
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
    
    if (isGrammarCheckingEnabled())
        badGrammarPhrase = findFirstBadGrammarInRange(checker, spellCheckerDocumentTag(), grammarSearchRange.get(), grammarDetail, grammarPhraseOffset, false);
#endif
    
    // If we found neither bad grammar nor a misspelled word, wrap and try again (but don't bother if we started at the beginning of the
    // block rather than at a selection).
    if (startedWithSelection && !misspelledWord && !badGrammarPhrase) {
        spellingSearchRange->setStart(topNode, 0, ec);
        // going until the end of the very first chunk we tested is far enough
        spellingSearchRange->setEnd(searchEndNodeAfterWrap, searchEndOffsetAfterWrap, ec);
        
        misspelledWord = findFirstMisspellingInRange(checker, spellCheckerDocumentTag(), spellingSearchRange.get(), misspellingOffset, false);

#ifndef BUILDING_ON_TIGER
        grammarSearchRange = spellingSearchRange->cloneRange(ec);
        if (misspelledWord) {
            // Stop looking at start of next misspelled word
            CharacterIterator chars(grammarSearchRange.get());
            chars.advance(misspellingOffset);
            grammarSearchRange->setEnd(chars.range()->startContainer(ec), chars.range()->startOffset(ec), ec);
        }
        if (isGrammarCheckingEnabled())
            badGrammarPhrase = findFirstBadGrammarInRange(checker, spellCheckerDocumentTag(), grammarSearchRange.get(), grammarDetail, grammarPhraseOffset, false);
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
        frame()->selectionController()->setSelection(Selection(badGrammarRange.get(), SEL_DEFAULT_AFFINITY));
        frame()->revealSelection();
        
        [checker updateSpellingPanelWithGrammarString:badGrammarPhrase detail:grammarDetail];
        frame()->document()->addMarker(badGrammarRange.get(), DocumentMarker::Grammar, [grammarDetail objectForKey:NSGrammarUserDescription]);
#endif        
    } else if (misspelledWord) {
        // We found a misspelling, but not any earlier bad grammar. Select the misspelling, update the spelling panel, and store
        // a marker so we draw the red squiggle later.
        
        RefPtr<Range> misspellingRange = TextIterator::subrange(spellingSearchRange.get(), misspellingOffset, [misspelledWord length]);
        frame()->selectionController()->setSelection(Selection(misspellingRange.get(), DOWNSTREAM));
        frame()->revealSelection();
        
        [checker updateSpellingPanelWithMisspelledWord:misspelledWord];
        frame()->document()->addMarker(misspellingRange.get(), DocumentMarker::Spelling);
    }
}

bool Editor::isSelectionMisspelled()
{
    String selectedString = frame()->selectedText();
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
                            inSpellDocumentWithTag:spellCheckerDocumentTag() 
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
    ExceptionCode ec;
    if (!range || range->collapsed(ec))
        return NO;
    
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

bool Editor::isSelectionUngrammatical()
{
#ifdef BUILDING_ON_TIGER
    return false;
#else
    Vector<String> ignoredGuesses;
    return isRangeUngrammatical(spellCheckerDocumentTag(), frame()->selectionController()->toRange().get(), ignoredGuesses);
#endif
}

Vector<String> Editor::guessesForUngrammaticalSelection()
{
#ifdef BUILDING_ON_TIGER
    return Vector<String>();
#else
    Vector<String> guesses;
    // Ignore the result of isRangeUngrammatical; we just want the guesses, whether or not there are any
    isRangeUngrammatical(spellCheckerDocumentTag(), frame()->selectionController()->toRange().get(), guesses);
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

Vector<String> Editor::guessesForMisspelledSelection()
{
    String selectedString = frame()->selectedText();
    ASSERT(selectedString.length() != 0);
    return core([[NSSpellChecker sharedSpellChecker] guessesForWord:selectedString]);
}

void Editor::showSpellingGuessPanel()
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }
    
    NSPanel *spellingPanel = [checker spellingPanel];
#ifndef BUILDING_ON_TIGER
    // Post-Tiger, this menu item is a show/hide toggle, to match AppKit. Leave Tiger behavior alone
    // to match rest of OS X.
    if ([spellingPanel isVisible]) {
        [spellingPanel orderOut:nil];
        return;
    }
#endif
    
    advanceToNextMisspelling(true);
    [spellingPanel orderFront:nil];
}

bool Editor::spellingPanelIsShowing()
{
    return [[[NSSpellChecker sharedSpellChecker] spellingPanel] isVisible];
}

void Editor::markMisspellingsAfterTypingToPosition(const VisiblePosition &p)
{
    if (!isContinuousSpellCheckingEnabled())
        return;
    
    // Check spelling of one word
    markMisspellings(Selection(startOfWord(p, LeftWordIfOnBoundary), endOfWord(p, RightWordIfOnBoundary)));
    
    if (!isGrammarCheckingEnabled())
        return;
    
    // Check grammar of entire sentence
    markBadGrammar(Selection(startOfSentence(p), endOfSentence(p)));
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
    
static void markMisspellingsOrBadGrammar(Editor* editor, const Selection& selection, bool checkSpelling)
{
    // This function is called with a selection already expanded to word boundaries.
    // Might be nice to assert that here.
    
    // This function is used only for as-you-type checking, so if that's off we do nothing. Note that
    // grammar checking can only be on if spell checking is also on.
    if (!editor->isContinuousSpellCheckingEnabled())
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
    
    if (checkSpelling)
        markAllMisspellingsInRange(checker, editor->spellCheckerDocumentTag(), searchRange.get());
    else {
#ifdef BUILDING_ON_TIGER
        ASSERT_NOT_REACHED();
#else
        if (editor->isGrammarCheckingEnabled())
            markAllBadGrammarInRange(checker, editor->spellCheckerDocumentTag(), searchRange.get());
#endif
    }    
}

void Editor::markMisspellings(const Selection& selection)
{
    markMisspellingsOrBadGrammar(this, selection, true);
}
    
void Editor::markBadGrammar(const Selection& selection)
{
#ifndef BUILDING_ON_TIGER
    markMisspellingsOrBadGrammar(this, selection, false);
#endif
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

void Editor::unmarkText()
{
    m_frame->setMarkedTextRange(0, nil, nil);
}

} // namespace WebCore
