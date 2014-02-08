/*
 * Copyright (C) 2006, 2007, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "TextCheckingHelper.h"

#include "Document.h"
#include "DocumentMarkerController.h"
#include "Frame.h"
#include "Settings.h"
#include "TextBreakIterator.h"
#include "TextCheckerClient.h"
#include "TextIterator.h"
#include "VisiblePosition.h"
#include "VisibleUnits.h"
#include <wtf/text/StringView.h>

namespace WebCore {

#if !USE(UNIFIED_TEXT_CHECKING)

#if USE(GRAMMAR_CHECKING)

static void findGrammaticalErrors(TextCheckerClient& client, StringView text, Vector<TextCheckingResult>& results)
{
    for (unsigned checkLocation = 0; checkLocation < text.length(); ) {
        int badGrammarLocation = -1;
        int badGrammarLength = 0;
        Vector<GrammarDetail> badGrammarDetails;
        client.checkGrammarOfString(text.substring(checkLocation), badGrammarDetails, &badGrammarLocation, &badGrammarLength);
        if (!badGrammarLength)
            break;

        ASSERT(badGrammarLocation >= 0);
        ASSERT(static_cast<unsigned>(badGrammarLocation) <= text.length() - checkLocation);
        ASSERT(badGrammarLength > 0);
        ASSERT(static_cast<unsigned>(badGrammarLength) <= text.length() - checkLocation - badGrammarLocation);

        TextCheckingResult badGrammar;
        badGrammar.type = TextCheckingTypeGrammar;
        badGrammar.location = checkLocation + badGrammarLocation;
        badGrammar.length = badGrammarLength;
        badGrammar.details = std::move(badGrammarDetails);
        results.append(badGrammar);

        checkLocation += badGrammarLocation + badGrammarLength;
    }
}

#endif

static void findMisspellings(TextCheckerClient& client, StringView text, Vector<TextCheckingResult>& results)
{
    TextBreakIterator* iterator = wordBreakIterator(text);
    if (!iterator)
        return;
    for (int wordStart = textBreakCurrent(iterator); wordStart > 0; ) {
        int wordEnd = textBreakNext(iterator);
        if (wordEnd < 0)
            break;

        int wordLength = wordEnd - wordStart;
        int misspellingLocation = -1;
        int misspellingLength = 0;
        client.checkSpellingOfString(text.substring(wordStart, wordLength), &misspellingLocation, &misspellingLength);

        if (misspellingLength > 0) {
            ASSERT(misspellingLocation >= 0);
            ASSERT(misspellingLocation <= wordLength);
            ASSERT(misspellingLength > 0);
            ASSERT(misspellingLocation + misspellingLength <= wordLength);

            TextCheckingResult misspelling;
            misspelling.type = TextCheckingTypeSpelling;
            misspelling.location = wordStart + misspellingLocation;
            misspelling.length = misspellingLength;
            misspelling.replacement = client.getAutoCorrectSuggestionForMisspelledWord(text.substring(misspelling.location, misspelling.length).toStringWithoutCopying());
            results.append(misspelling);
        }

        wordStart = wordEnd;
    }
}

#endif

static PassRefPtr<Range> expandToParagraphBoundary(PassRefPtr<Range> range)
{
    RefPtr<Range> paragraphRange = range->cloneRange(IGNORE_EXCEPTION);
    setStart(paragraphRange.get(), startOfParagraph(range->startPosition()));
    setEnd(paragraphRange.get(), endOfParagraph(range->endPosition()));
    return paragraphRange;
}

TextCheckingParagraph::TextCheckingParagraph(PassRefPtr<Range> checkingRange)
    : m_checkingRange(checkingRange)
    , m_checkingStart(-1)
    , m_checkingEnd(-1)
    , m_checkingLength(-1)
{
}

TextCheckingParagraph::TextCheckingParagraph(PassRefPtr<Range> checkingRange, PassRefPtr<Range> paragraphRange)
    : m_checkingRange(checkingRange)
    , m_paragraphRange(paragraphRange)
    , m_checkingStart(-1)
    , m_checkingEnd(-1)
    , m_checkingLength(-1)
{
}

TextCheckingParagraph::~TextCheckingParagraph()
{
}

void TextCheckingParagraph::expandRangeToNextEnd()
{
    ASSERT(m_checkingRange);
    setEnd(paragraphRange().get(), endOfParagraph(startOfNextParagraph(paragraphRange()->startPosition())));
    invalidateParagraphRangeValues();
}

void TextCheckingParagraph::invalidateParagraphRangeValues()
{
    m_checkingStart = m_checkingEnd = -1;
    m_offsetAsRange = 0;
    m_text = String();
}

int TextCheckingParagraph::rangeLength() const
{
    ASSERT(m_checkingRange);
    return TextIterator::rangeLength(paragraphRange().get());
}

PassRefPtr<Range> TextCheckingParagraph::paragraphRange() const
{
    ASSERT(m_checkingRange);
    if (!m_paragraphRange)
        m_paragraphRange = expandToParagraphBoundary(checkingRange());
    return m_paragraphRange;
}

PassRefPtr<Range> TextCheckingParagraph::subrange(int characterOffset, int characterCount) const
{
    ASSERT(m_checkingRange);
    return TextIterator::subrange(paragraphRange().get(), characterOffset, characterCount);
}

int TextCheckingParagraph::offsetTo(const Position& position, ExceptionCode& ec) const
{
    ASSERT(m_checkingRange);
    RefPtr<Range> range = offsetAsRange()->cloneRange(ASSERT_NO_EXCEPTION);
    range->setEnd(position.containerNode(), position.computeOffsetInContainerNode(), ec);
    if (ec)
        return 0;
    return TextIterator::rangeLength(range.get());
}

bool TextCheckingParagraph::isEmpty() const
{
    // Both predicates should have same result, but we check both just for sure.
    // We need to investigate to remove this redundancy.
    return isRangeEmpty() || isTextEmpty();
}

PassRefPtr<Range> TextCheckingParagraph::offsetAsRange() const
{
    ASSERT(m_checkingRange);
    if (!m_offsetAsRange)
        m_offsetAsRange = Range::create(paragraphRange()->startContainer()->document(), paragraphRange()->startPosition(), checkingRange()->startPosition());

    return m_offsetAsRange;
}

const String& TextCheckingParagraph::text() const
{
    ASSERT(m_checkingRange);
    if (m_text.isEmpty())
        m_text = plainText(paragraphRange().get());
    return m_text; 
}

int TextCheckingParagraph::checkingStart() const
{
    ASSERT(m_checkingRange);
    if (m_checkingStart == -1)
        m_checkingStart = TextIterator::rangeLength(offsetAsRange().get());
    return m_checkingStart;
}

int TextCheckingParagraph::checkingEnd() const
{
    ASSERT(m_checkingRange);
    if (m_checkingEnd == -1)
        m_checkingEnd = checkingStart() + TextIterator::rangeLength(checkingRange().get());
    return m_checkingEnd;
}

int TextCheckingParagraph::checkingLength() const
{
    ASSERT(m_checkingRange);
    if (-1 == m_checkingLength)
        m_checkingLength = TextIterator::rangeLength(checkingRange().get());
    return m_checkingLength;
}

TextCheckingHelper::TextCheckingHelper(EditorClient* client, PassRefPtr<Range> range)
    : m_client(client)
    , m_range(range)
{
    ASSERT_ARG(m_client, m_client);
    ASSERT_ARG(m_range, m_range);
}

TextCheckingHelper::~TextCheckingHelper()
{
}

#if !PLATFORM(IOS)

String TextCheckingHelper::findFirstMisspelling(int& firstMisspellingOffset, bool markAll, RefPtr<Range>& firstMisspellingRange)
{
    WordAwareIterator it(m_range.get());
    firstMisspellingOffset = 0;
    
    String firstMisspelling;
    int currentChunkOffset = 0;

    while (!it.atEnd()) {
        StringView text = it.text();
        int textLength = text.length();

        // Skip some work for one-space-char hunks
        if (textLength == 1 && text[0] == ' ') {
            int misspellingLocation = -1;
            int misspellingLength = 0;
            m_client->textChecker()->checkSpellingOfString(text, &misspellingLocation, &misspellingLength);

            // 5490627 shows that there was some code path here where the String constructor below crashes.
            // We don't know exactly what combination of bad input caused this, so we're making this much
            // more robust against bad input on release builds.
            ASSERT(misspellingLength >= 0);
            ASSERT(misspellingLocation >= -1);
            ASSERT(!misspellingLength || misspellingLocation >= 0);
            ASSERT(misspellingLocation < textLength);
            ASSERT(misspellingLength <= textLength);
            ASSERT(misspellingLocation + misspellingLength <= textLength);

            if (misspellingLocation >= 0 && misspellingLength > 0 && misspellingLocation < textLength && misspellingLength <= textLength && misspellingLocation + misspellingLength <= textLength) {
                // Compute range of misspelled word
                RefPtr<Range> misspellingRange = TextIterator::subrange(m_range.get(), currentChunkOffset + misspellingLocation, misspellingLength);

                // Remember first-encountered misspelling and its offset.
                if (!firstMisspelling) {
                    firstMisspellingOffset = currentChunkOffset + misspellingLocation;
                    firstMisspelling = text.substring(misspellingLocation, misspellingLength).toString();
                    firstMisspellingRange = misspellingRange;
                }

                // Store marker for misspelled word.
                misspellingRange->startContainer()->document().markers().addMarker(misspellingRange.get(), DocumentMarker::Spelling);

                // Bail out if we're marking only the first misspelling, and not all instances.
                if (!markAll)
                    break;
            }
        }
        
        currentChunkOffset += textLength;
        it.advance();
    }
    
    return firstMisspelling;
}

String TextCheckingHelper::findFirstMisspellingOrBadGrammar(bool checkGrammar, bool& outIsSpelling, int& outFirstFoundOffset, GrammarDetail& outGrammarDetail)
{
    if (!unifiedTextCheckerEnabled())
        return "";

    String firstFoundItem;
    String misspelledWord;
    String badGrammarPhrase;
    
    // Initialize out parameters; these will be updated if we find something to return.
    outIsSpelling = true;
    outFirstFoundOffset = 0;
    outGrammarDetail.location = -1;
    outGrammarDetail.length = 0;
    outGrammarDetail.guesses.clear();
    outGrammarDetail.userDescription = "";
    
    // Expand the search range to encompass entire paragraphs, since text checking needs that much context.
    // Determine the character offset from the start of the paragraph to the start of the original search range,
    // since we will want to ignore results in this area.
    RefPtr<Range> paragraphRange = m_range->cloneRange(IGNORE_EXCEPTION);
    setStart(paragraphRange.get(), startOfParagraph(m_range->startPosition()));
    int totalRangeLength = TextIterator::rangeLength(paragraphRange.get());
    setEnd(paragraphRange.get(), endOfParagraph(m_range->startPosition()));
    
    RefPtr<Range> offsetAsRange = Range::create(paragraphRange->startContainer()->document(), paragraphRange->startPosition(), m_range->startPosition());
    int rangeStartOffset = TextIterator::rangeLength(offsetAsRange.get());
    int totalLengthProcessed = 0;
    
    bool firstIteration = true;
    bool lastIteration = false;
    while (totalLengthProcessed < totalRangeLength) {
        // Iterate through the search range by paragraphs, checking each one for spelling and grammar.
        int currentLength = TextIterator::rangeLength(paragraphRange.get());
        int currentStartOffset = firstIteration ? rangeStartOffset : 0;
        int currentEndOffset = currentLength;
        if (inSameParagraph(paragraphRange->startPosition(), m_range->endPosition())) {
            // Determine the character offset from the end of the original search range to the end of the paragraph,
            // since we will want to ignore results in this area.
            RefPtr<Range> endOffsetAsRange = Range::create(paragraphRange->startContainer()->document(), paragraphRange->startPosition(), m_range->endPosition());
            currentEndOffset = TextIterator::rangeLength(endOffsetAsRange.get());
            lastIteration = true;
        }
        if (currentStartOffset < currentEndOffset) {
            String paragraphString = plainText(paragraphRange.get());
            if (paragraphString.length() > 0) {
                bool foundGrammar = false;
                int spellingLocation = 0;
                int grammarPhraseLocation = 0;
                int grammarDetailLocation = 0;
                unsigned grammarDetailIndex = 0;
                
                Vector<TextCheckingResult> results;
                TextCheckingTypeMask checkingTypes = checkGrammar ? (TextCheckingTypeSpelling | TextCheckingTypeGrammar) : TextCheckingTypeSpelling;
                checkTextOfParagraph(*m_client->textChecker(), paragraphString, checkingTypes, results);
                
                for (unsigned i = 0; i < results.size(); i++) {
                    const TextCheckingResult* result = &results[i];
                    if (result->type == TextCheckingTypeSpelling && result->location >= currentStartOffset && result->location + result->length <= currentEndOffset) {
                        ASSERT(result->length > 0);
                        ASSERT(result->location >= 0);
                        spellingLocation = result->location;
                        misspelledWord = paragraphString.substring(result->location, result->length);
                        ASSERT(misspelledWord.length());
                        break;
                    }
                    if (checkGrammar && result->type == TextCheckingTypeGrammar && result->location < currentEndOffset && result->location + result->length > currentStartOffset) {
                        ASSERT(result->length > 0);
                        ASSERT(result->location >= 0);
                        // We can't stop after the first grammar result, since there might still be a spelling result after
                        // it begins but before the first detail in it, but we can stop if we find a second grammar result.
                        if (foundGrammar)
                            break;
                        for (unsigned j = 0; j < result->details.size(); j++) {
                            const GrammarDetail* detail = &result->details[j];
                            ASSERT(detail->length > 0);
                            ASSERT(detail->location >= 0);
                            if (result->location + detail->location >= currentStartOffset && result->location + detail->location + detail->length <= currentEndOffset && (!foundGrammar || result->location + detail->location < grammarDetailLocation)) {
                                grammarDetailIndex = j;
                                grammarDetailLocation = result->location + detail->location;
                                foundGrammar = true;
                            }
                        }
                        if (foundGrammar) {
                            grammarPhraseLocation = result->location;
                            outGrammarDetail = result->details[grammarDetailIndex];
                            badGrammarPhrase = paragraphString.substring(result->location, result->length);
                            ASSERT(badGrammarPhrase.length());
                        }
                    }
                }

                if (!misspelledWord.isEmpty() && (!checkGrammar || badGrammarPhrase.isEmpty() || spellingLocation <= grammarDetailLocation)) {
                    int spellingOffset = spellingLocation - currentStartOffset;
                    if (!firstIteration) {
                        RefPtr<Range> paragraphOffsetAsRange = Range::create(paragraphRange->startContainer()->document(), m_range->startPosition(), paragraphRange->startPosition());
                        spellingOffset += TextIterator::rangeLength(paragraphOffsetAsRange.get());
                    }
                    outIsSpelling = true;
                    outFirstFoundOffset = spellingOffset;
                    firstFoundItem = misspelledWord;
                    break;
                }
                if (checkGrammar && !badGrammarPhrase.isEmpty()) {
                    int grammarPhraseOffset = grammarPhraseLocation - currentStartOffset;
                    if (!firstIteration) {
                        RefPtr<Range> paragraphOffsetAsRange = Range::create(paragraphRange->startContainer()->document(), m_range->startPosition(), paragraphRange->startPosition());
                        grammarPhraseOffset += TextIterator::rangeLength(paragraphOffsetAsRange.get());
                    }
                    outIsSpelling = false;
                    outFirstFoundOffset = grammarPhraseOffset;
                    firstFoundItem = badGrammarPhrase;
                    break;
                }
            }
        }
        if (lastIteration || totalLengthProcessed + currentLength >= totalRangeLength)
            break;
        VisiblePosition newParagraphStart = startOfNextParagraph(paragraphRange->endPosition());
        setStart(paragraphRange.get(), newParagraphStart);
        setEnd(paragraphRange.get(), endOfParagraph(newParagraphStart));
        firstIteration = false;
        totalLengthProcessed += currentLength;
    }
    return firstFoundItem;
}

#endif // !PLATFORM(IOS)

#if USE(GRAMMAR_CHECKING)

int TextCheckingHelper::findFirstGrammarDetail(const Vector<GrammarDetail>& grammarDetails, int badGrammarPhraseLocation, int startOffset, int endOffset, bool markAll) const
{
    // Found some bad grammar. Find the earliest detail range that starts in our search range (if any).
    // Optionally add a DocumentMarker for each detail in the range.
    int earliestDetailLocationSoFar = -1;
    int earliestDetailIndex = -1;
    for (unsigned i = 0; i < grammarDetails.size(); i++) {
        const GrammarDetail* detail = &grammarDetails[i];
        ASSERT(detail->length > 0);
        ASSERT(detail->location >= 0);
        
        int detailStartOffsetInParagraph = badGrammarPhraseLocation + detail->location;
        
        // Skip this detail if it starts before the original search range
        if (detailStartOffsetInParagraph < startOffset)
            continue;
        
        // Skip this detail if it starts after the original search range
        if (detailStartOffsetInParagraph >= endOffset)
            continue;
        
        if (markAll) {
            RefPtr<Range> badGrammarRange = TextIterator::subrange(m_range.get(), badGrammarPhraseLocation - startOffset + detail->location, detail->length);
            badGrammarRange->startContainer()->document().markers().addMarker(badGrammarRange.get(), DocumentMarker::Grammar, detail->userDescription);
        }
        
        // Remember this detail only if it's earlier than our current candidate (the details aren't in a guaranteed order)
        if (earliestDetailIndex < 0 || earliestDetailLocationSoFar > detail->location) {
            earliestDetailIndex = i;
            earliestDetailLocationSoFar = detail->location;
        }
    }
    
    return earliestDetailIndex;
}

String TextCheckingHelper::findFirstBadGrammar(GrammarDetail& outGrammarDetail, int& outGrammarPhraseOffset, bool markAll) const
{
    // Initialize out parameters; these will be updated if we find something to return.
    outGrammarDetail.location = -1;
    outGrammarDetail.length = 0;
    outGrammarDetail.guesses.clear();
    outGrammarDetail.userDescription = "";
    outGrammarPhraseOffset = 0;
    
    String firstBadGrammarPhrase;

    // Expand the search range to encompass entire paragraphs, since grammar checking needs that much context.
    // Determine the character offset from the start of the paragraph to the start of the original search range,
    // since we will want to ignore results in this area.
    TextCheckingParagraph paragraph(m_range);
    
    // Start checking from beginning of paragraph, but skip past results that occur before the start of the original search range.
    for (int startOffset = 0; startOffset < paragraph.checkingEnd(); ) {
        Vector<GrammarDetail> grammarDetails;
        int badGrammarPhraseLocation = -1;
        int badGrammarPhraseLength = 0;
        m_client->textChecker()->checkGrammarOfString(StringView(paragraph.text()).substring(startOffset), grammarDetails, &badGrammarPhraseLocation, &badGrammarPhraseLength);
        
        if (!badGrammarPhraseLength) {
            ASSERT(badGrammarPhraseLocation == -1);
            return String();
        }

        ASSERT(badGrammarPhraseLocation >= 0);
        badGrammarPhraseLocation += startOffset;

        // Found some bad grammar. Find the earliest detail range that starts in our search range (if any).
        int badGrammarIndex = findFirstGrammarDetail(grammarDetails, badGrammarPhraseLocation, paragraph.checkingStart(), paragraph.checkingEnd(), markAll);
        if (badGrammarIndex >= 0) {
            ASSERT(static_cast<unsigned>(badGrammarIndex) < grammarDetails.size());
            outGrammarDetail = grammarDetails[badGrammarIndex];
        }

        // If we found a detail in range, then we have found the first bad phrase (unless we found one earlier but
        // kept going so we could mark all instances).
        if (badGrammarIndex >= 0 && firstBadGrammarPhrase.isEmpty()) {
            outGrammarPhraseOffset = badGrammarPhraseLocation - paragraph.checkingStart();
            firstBadGrammarPhrase = paragraph.textSubstring(badGrammarPhraseLocation, badGrammarPhraseLength);
            
            // Found one. We're done now, unless we're marking each instance.
            if (!markAll)
                break;
        }

        // These results were all between the start of the paragraph and the start of the search range; look
        // beyond this phrase.
        startOffset = badGrammarPhraseLocation + badGrammarPhraseLength;
    }
    
    return firstBadGrammarPhrase;
}

bool TextCheckingHelper::isUngrammatical() const
{
    if (!m_client)
        return false;

    if (!m_range || m_range->collapsed(IGNORE_EXCEPTION))
        return false;
    
    // Returns true only if the passed range exactly corresponds to a bad grammar detail range. This is analogous
    // to isSelectionMisspelled. It's not good enough for there to be some bad grammar somewhere in the range,
    // or overlapping the range; the ranges must exactly match.
    int grammarPhraseOffset;
    
    GrammarDetail grammarDetail;
    String badGrammarPhrase = findFirstBadGrammar(grammarDetail, grammarPhraseOffset, false);
    
    // No bad grammar in these parts at all.
    if (badGrammarPhrase.isEmpty())
        return false;
    
    // Bad grammar, but phrase (e.g. sentence) starts beyond start of range.
    if (grammarPhraseOffset > 0)
        return false;

    ASSERT(grammarDetail.location >= 0);
    ASSERT(grammarDetail.length > 0);

    // Bad grammar, but start of detail (e.g. ungrammatical word) doesn't match start of range
    if (grammarDetail.location + grammarPhraseOffset)
        return false;
    
    // Bad grammar at start of range, but end of bad grammar is before or after end of range
    if (grammarDetail.length != TextIterator::rangeLength(m_range.get()))
        return false;
    
    // Update the spelling panel to be displaying this error (whether or not the spelling panel is on screen).
    // This is necessary to make a subsequent call to [NSSpellChecker ignoreWord:inSpellDocumentWithTag:] work
    // correctly; that call behaves differently based on whether the spelling panel is displaying a misspelling
    // or a grammar error.
    m_client->updateSpellingUIWithGrammarString(badGrammarPhrase, grammarDetail);
    
    return true;
}

#endif // USE(GRAMMAR_CHECKING)

Vector<String> TextCheckingHelper::guessesForMisspelledOrUngrammaticalRange(bool checkGrammar, bool& misspelled, bool& ungrammatical) const
{
    if (!unifiedTextCheckerEnabled())
        return Vector<String>();

    Vector<String> guesses;
    misspelled = false;
    ungrammatical = false;
    
    if (!m_client || !m_range || m_range->collapsed(IGNORE_EXCEPTION))
        return guesses;

    // Expand the range to encompass entire paragraphs, since text checking needs that much context.
    TextCheckingParagraph paragraph(m_range);
    if (paragraph.isEmpty())
        return guesses;

    Vector<TextCheckingResult> results;
    TextCheckingTypeMask checkingTypes = checkGrammar ? (TextCheckingTypeSpelling | TextCheckingTypeGrammar) : TextCheckingTypeSpelling;
    checkTextOfParagraph(*m_client->textChecker(), paragraph.text(), checkingTypes, results);
    
    for (unsigned i = 0; i < results.size(); i++) {
        const TextCheckingResult* result = &results[i];
        if (result->type == TextCheckingTypeSpelling && paragraph.checkingRangeMatches(result->location, result->length)) {
            String misspelledWord = paragraph.checkingSubstring();
            ASSERT(misspelledWord.length());
            m_client->textChecker()->getGuessesForWord(misspelledWord, String(), guesses);
            m_client->updateSpellingUIWithMisspelledWord(misspelledWord);
            misspelled = true;
            return guesses;
        }
    }
    
    if (!checkGrammar)
        return guesses;
        
    for (unsigned i = 0; i < results.size(); i++) {
        const TextCheckingResult* result = &results[i];
        if (result->type == TextCheckingTypeGrammar && paragraph.isCheckingRangeCoveredBy(result->location, result->length)) {
            for (unsigned j = 0; j < result->details.size(); j++) {
                const GrammarDetail* detail = &result->details[j];
                ASSERT(detail->length > 0);
                ASSERT(detail->location >= 0);
                if (paragraph.checkingRangeMatches(result->location + detail->location, detail->length)) {
                    String badGrammarPhrase = paragraph.textSubstring(result->location, result->length);
                    ASSERT(badGrammarPhrase.length());
                    for (unsigned k = 0; k < detail->guesses.size(); k++)
                        guesses.append(detail->guesses[k]);
                    m_client->updateSpellingUIWithGrammarString(badGrammarPhrase, *detail);
                    ungrammatical = true;
                    return guesses;
                }
            }
        }
    }
    return guesses;
}

#if !PLATFORM(IOS)

void TextCheckingHelper::markAllMisspellings(RefPtr<Range>& firstMisspellingRange)
{
    // Use the "markAll" feature of findFirstMisspelling. Ignore the return value and the "out parameter";
    // all we need to do is mark every instance.
    int ignoredOffset;
    findFirstMisspelling(ignoredOffset, true, firstMisspellingRange);
}

#if USE(GRAMMAR_CHECKING)
void TextCheckingHelper::markAllBadGrammar()
{
    // Use the "markAll" feature of ofindFirstBadGrammar. Ignore the return value and "out parameters"; all we need to
    // do is mark every instance.
    GrammarDetail ignoredGrammarDetail;
    int ignoredOffset;
    findFirstBadGrammar(ignoredGrammarDetail, ignoredOffset, true);
}
#endif

#endif // !PLATFORM(IOS)

bool TextCheckingHelper::unifiedTextCheckerEnabled() const
{
    return m_range && WebCore::unifiedTextCheckerEnabled(m_range->ownerDocument().frame());
}

void checkTextOfParagraph(TextCheckerClient& client, StringView text, TextCheckingTypeMask checkingTypes, Vector<TextCheckingResult>& results)
{
#if USE(UNIFIED_TEXT_CHECKING)
    results = client.checkTextOfParagraph(text, checkingTypes);
#else
    Vector<TextCheckingResult> mispellings;
    if (checkingTypes & TextCheckingTypeSpelling)
        findMisspellings(client, text, mispellings);

#if USE(GRAMMAR_CHECKING)
    // Look for grammatical errors that occur before the first misspelling.
    Vector<TextCheckingResult> grammaticalErrors;
    if (checkingTypes & TextCheckingTypeGrammar) {
        unsigned grammarCheckLength = text.length();
        for (auto& mispelling : mispellings)
            grammarCheckLength = std::min<unsigned>(grammarCheckLength, mispelling.location);
        findGrammaticalErrors(client, text.substring(0, grammarCheckLength), grammaticalErrors);
    }

    results = std::move(grammaticalErrors);
#endif

    if (results.isEmpty())
        results = std::move(mispellings);
    else
        results.appendVector(mispellings);
#endif // USE(UNIFIED_TEXT_CHECKING)
}

bool unifiedTextCheckerEnabled(const Frame* frame)
{
    if (!frame)
        return false;
    return frame->settings().unifiedTextCheckerEnabled();
}

}
