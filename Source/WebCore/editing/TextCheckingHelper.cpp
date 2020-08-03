/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TextCheckingHelper.h"

#include "Document.h"
#include "DocumentMarkerController.h"
#include "EditorClient.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "Range.h"
#include "Settings.h"
#include "TextCheckerClient.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include <unicode/ubrk.h>
#include <wtf/text/StringView.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

#if !USE(UNIFIED_TEXT_CHECKING)

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
        badGrammar.type = TextCheckingType::Grammar;
        badGrammar.range = CharacterRange(checkLocation + badGrammarLocation, badGrammarLength);
        badGrammar.details = WTFMove(badGrammarDetails);
        results.append(badGrammar);

        checkLocation += badGrammarLocation + badGrammarLength;
    }
}

static void findMisspellings(TextCheckerClient& client, StringView text, Vector<TextCheckingResult>& results)
{
    UBreakIterator* iterator = wordBreakIterator(text);
    if (!iterator)
        return;
    for (int wordStart = ubrk_current(iterator); wordStart >= 0; ) {
        int wordEnd = ubrk_next(iterator);
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
            misspelling.type = TextCheckingType::Spelling;
            misspelling.range = CharacterRange(wordStart + misspellingLocation, misspellingLength);
            misspelling.replacement = client.getAutoCorrectSuggestionForMisspelledWord(text.substring(misspelling.range.location, misspelling.range.length).toStringWithoutCopying());
            results.append(misspelling);
        }

        wordStart = wordEnd;
    }
}

#endif

static SimpleRange expandToParagraphBoundary(const SimpleRange& range)
{
    return {
        *makeBoundaryPoint(startOfParagraph(createLegacyEditingPosition(range.start))),
        *makeBoundaryPoint(endOfParagraph(createLegacyEditingPosition(range.end)))
    };
}

TextCheckingParagraph::TextCheckingParagraph(const SimpleRange& range)
    : m_checkingRange(range)
    , m_automaticReplacementRange(range)
{
}

TextCheckingParagraph::TextCheckingParagraph(const SimpleRange& checkingRange, const SimpleRange& replacementRange, const Optional<SimpleRange>& paragraphRange)
    : m_checkingRange(checkingRange)
    , m_automaticReplacementRange(replacementRange)
    , m_paragraphRange(paragraphRange)
{
}

void TextCheckingParagraph::expandRangeToNextEnd()
{
    paragraphRange();
    if (auto end = makeBoundaryPoint(endOfParagraph(startOfNextParagraph(createLegacyEditingPosition(m_paragraphRange->start)))))
        m_paragraphRange->end = WTFMove(*end);
    invalidateParagraphRangeValues();
}

void TextCheckingParagraph::invalidateParagraphRangeValues()
{
    m_checkingStart.reset();
    m_automaticReplacementStart.reset();
    m_automaticReplacementLength.reset();
    m_offsetAsRange = WTF::nullopt;
    m_text = String();
}

uint64_t TextCheckingParagraph::rangeLength() const
{
    return characterCount(paragraphRange());
}

const SimpleRange& TextCheckingParagraph::paragraphRange() const
{
    if (!m_paragraphRange)
        m_paragraphRange = expandToParagraphBoundary(m_checkingRange);
    return *m_paragraphRange;
}

SimpleRange TextCheckingParagraph::subrange(CharacterRange range) const
{
    return resolveCharacterRange(paragraphRange(), range);
}

ExceptionOr<uint64_t> TextCheckingParagraph::offsetTo(const Position& position) const
{
    auto range = makeSimpleRange(paragraphRange().start, position);
    if (!range)
        return Exception { TypeError };
    return characterCount(*range);
}

bool TextCheckingParagraph::isEmpty() const
{
    // Both predicates should have same result, but we check both just for sure.
    // We need to investigate to remove this redundancy.
    return checkingStart() >= checkingEnd() || text().isEmpty();
}

const SimpleRange& TextCheckingParagraph::offsetAsRange() const
{
    if (!m_offsetAsRange)
        m_offsetAsRange = { { paragraphRange().start, m_checkingRange.start } };
    return *m_offsetAsRange;
}

StringView TextCheckingParagraph::text() const
{
    if (m_text.isNull())
        m_text = plainText(paragraphRange());
    return m_text; 
}

uint64_t TextCheckingParagraph::checkingStart() const
{
    if (!m_checkingStart)
        m_checkingStart = characterCount(offsetAsRange());
    return *m_checkingStart;
}

uint64_t TextCheckingParagraph::checkingEnd() const
{
    return checkingStart() + checkingLength();
}

uint64_t TextCheckingParagraph::checkingLength() const
{
    if (!m_checkingLength)
        m_checkingLength = characterCount(m_checkingRange);
    return *m_checkingLength;
}

uint64_t TextCheckingParagraph::automaticReplacementStart() const
{
    if (!m_automaticReplacementStart)
        m_automaticReplacementStart = characterCount({ paragraphRange().start, m_automaticReplacementRange.start });
    return *m_automaticReplacementStart;
}

uint64_t TextCheckingParagraph::automaticReplacementLength() const
{
    if (!m_automaticReplacementLength)
        m_automaticReplacementLength = characterCount(m_automaticReplacementRange);
    return *m_automaticReplacementLength;
}

TextCheckingHelper::TextCheckingHelper(EditorClient& client, const SimpleRange& range)
    : m_client(client)
    , m_range(range)
{
}

auto TextCheckingHelper::findMisspelledWords(Operation operation) const -> std::pair<MisspelledWord, Optional<SimpleRange>>
{
    std::pair<MisspelledWord, Optional<SimpleRange>> first;

    uint64_t currentChunkOffset = 0;

    for (WordAwareIterator it(m_range); !it.atEnd(); currentChunkOffset += it.text().length(), it.advance()) {
        StringView text = it.text();

        if (text == " "_s)
            continue;

        int misspellingLocation = -1;
        int misspellingLength = 0;
        m_client.textChecker()->checkSpellingOfString(text, &misspellingLocation, &misspellingLength);

        int textLength = text.length();

        // 5490627 shows that there was some code path here where the String constructor below crashes.
        // We don't know exactly what combination of bad input caused this, so we're making this much
        // more robust against bad input on release builds.
        ASSERT(misspellingLength >= 0);
        ASSERT(misspellingLocation >= -1);
        ASSERT(!misspellingLength || misspellingLocation >= 0);
        ASSERT(misspellingLocation < textLength);
        ASSERT(misspellingLength <= textLength);
        ASSERT(misspellingLocation + misspellingLength <= textLength);
        if (!(misspellingLocation >= 0 && misspellingLength > 0 && misspellingLocation < textLength && misspellingLength <= textLength && misspellingLocation + misspellingLength <= textLength))
            continue;

        auto misspellingRange = resolveCharacterRange(m_range, CharacterRange(currentChunkOffset + misspellingLocation, misspellingLength));

        if (operation == Operation::MarkAll)
            addMarker(misspellingRange, DocumentMarker::Spelling);

        if (first.first.word.isNull()) {
            first = {
                {
                    text.substring(misspellingLocation, misspellingLength).toString(),
                    currentChunkOffset + misspellingLocation
                },
                WTFMove(misspellingRange)
            };
        }

        if (operation == Operation::FindFirst)
            break;
    }

    return first;
}

auto TextCheckingHelper::findFirstMisspelledWord() const -> MisspelledWord
{
    return findMisspelledWords(Operation::FindFirst).first;
}

auto TextCheckingHelper::findFirstMisspelledWordOrUngrammaticalPhrase(bool checkGrammar) const -> Variant<MisspelledWord, UngrammaticalPhrase>
{
    if (!unifiedTextCheckerEnabled())
        return { };

    if (platformDrivenTextCheckerEnabled())
        return { };

    Variant<MisspelledWord, UngrammaticalPhrase> firstFoundItem;
    GrammarDetail grammarDetail;

    String misspelledWord;
    Optional<SimpleRange> misspelledWordRange;
    String badGrammarPhrase;
    
    // Expand the search range to encompass entire paragraphs, since text checking needs that much context.
    // Determine the character offset from the start of the paragraph to the start of the original search range,
    // since we will want to ignore results in this area.
    auto paragraphRange = *makeSimpleRange(startOfParagraph(createLegacyEditingPosition(m_range.start)), m_range.end);
    auto totalRangeLength = characterCount(paragraphRange);
    paragraphRange.end = *makeBoundaryPoint(endOfParagraph(createLegacyEditingPosition(m_range.start)));
    
    auto rangeStartOffset = characterCount({ paragraphRange.start, m_range.start });
    uint64_t totalLengthProcessed = 0;

    bool firstIteration = true;
    bool lastIteration = false;
    while (totalLengthProcessed < totalRangeLength) {
        // Iterate through the search range by paragraphs, checking each one for spelling and grammar.
        auto currentLength = characterCount(paragraphRange);
        uint64_t currentStartOffset = firstIteration ? rangeStartOffset : 0;
        uint64_t currentEndOffset = currentLength;
        if (inSameParagraph(createLegacyEditingPosition(paragraphRange.start), createLegacyEditingPosition(m_range.end))) {
            // Determine the character offset from the end of the original search range to the end of the paragraph,
            // since we will want to ignore results in this area.
            currentEndOffset = characterCount({ paragraphRange.start, m_range.end });
            lastIteration = true;
        }
        if (currentStartOffset < currentEndOffset) {
            String paragraphString = plainText(paragraphRange);
            if (paragraphString.length() > 0) {
                bool foundGrammar = false;
                uint64_t spellingLocation = 0;
                uint64_t grammarPhraseLocation = 0;
                uint64_t grammarDetailLocation = 0;
                unsigned grammarDetailIndex = 0;
                
                Vector<TextCheckingResult> results;
                OptionSet<TextCheckingType> checkingTypes { TextCheckingType::Spelling };
                if (checkGrammar)
                    checkingTypes.add(TextCheckingType::Grammar);
                VisibleSelection currentSelection;
                if (Frame* frame = paragraphRange.start.container->document().frame())
                    currentSelection = frame->selection().selection();
                checkTextOfParagraph(*m_client.textChecker(), paragraphString, checkingTypes, results, currentSelection);

                for (auto& result : results) {
                    if (result.type == TextCheckingType::Spelling && result.range.location >= currentStartOffset && result.range.location + result.range.length <= currentEndOffset) {
                        ASSERT(result.range.length > 0);
                        ASSERT(result.range.location >= 0);
                        spellingLocation = result.range.location;
                        misspelledWord = paragraphString.substring(result.range.location, result.range.length);
                        ASSERT(misspelledWord.length());
                        break;
                    }
                    if (checkGrammar && result.type == TextCheckingType::Grammar && result.range.location < currentEndOffset && result.range.location + result.range.length > currentStartOffset) {
                        ASSERT(result.range.length > 0);
                        ASSERT(result.range.location >= 0);
                        // We can't stop after the first grammar result, since there might still be a spelling result after
                        // it begins but before the first detail in it, but we can stop if we find a second grammar result.
                        if (foundGrammar)
                            break;
                        for (unsigned j = 0; j < result.details.size(); j++) {
                            const GrammarDetail* detail = &result.details[j];
                            ASSERT(detail->range.length > 0);
                            ASSERT(detail->range.location >= 0);
                            if (result.range.location + detail->range.location >= currentStartOffset && result.range.location + detail->range.location + detail->range.length <= currentEndOffset && (!foundGrammar || result.range.location + detail->range.location < grammarDetailLocation)) {
                                grammarDetailIndex = j;
                                grammarDetailLocation = result.range.location + detail->range.location;
                                foundGrammar = true;
                            }
                        }
                        if (foundGrammar) {
                            grammarPhraseLocation = result.range.location;
                            grammarDetail = result.details[grammarDetailIndex];
                            badGrammarPhrase = paragraphString.substring(result.range.location, result.range.length);
                            ASSERT(badGrammarPhrase.length());
                        }
                    }
                }

                if (!misspelledWord.isEmpty() && (!checkGrammar || badGrammarPhrase.isEmpty() || spellingLocation <= grammarDetailLocation)) {
                    uint64_t spellingOffset = spellingLocation - currentStartOffset;
                    if (!firstIteration)
                        spellingOffset += characterCount({ m_range.start, paragraphRange.start });
                    firstFoundItem = MisspelledWord {
                        misspelledWord,
                        spellingOffset
                    };
                    break;
                }
                if (checkGrammar && !badGrammarPhrase.isEmpty()) {
                    uint64_t grammarPhraseOffset = grammarPhraseLocation - currentStartOffset;
                    if (!firstIteration)
                        grammarPhraseOffset += characterCount({ m_range.start, paragraphRange.start });
                    firstFoundItem = UngrammaticalPhrase {
                        badGrammarPhrase,
                        grammarPhraseOffset,
                        grammarDetail
                    };
                    break;
                }
            }
        }
        if (lastIteration || totalLengthProcessed + currentLength >= totalRangeLength)
            break;

        auto nextStart = startOfNextParagraph(createLegacyEditingPosition(paragraphRange.end));
        auto nextParagraphRange = makeSimpleRange(nextStart, endOfParagraph(nextStart));
        if (!nextParagraphRange)
            break;
        paragraphRange = WTFMove(*nextParagraphRange);

        firstIteration = false;
        totalLengthProcessed += currentLength;
    }
    return firstFoundItem;
}

int TextCheckingHelper::findUngrammaticalPhrases(Operation operation, const Vector<GrammarDetail>& grammarDetails, uint64_t badGrammarPhraseLocation, uint64_t startOffset, uint64_t endOffset) const
{
    // Found some bad grammar. Find the earliest detail range that starts in our search range (if any).
    // Optionally add a DocumentMarker for each detail in the range.
    uint64_t earliestDetailLocationSoFar = 0;
    int earliestDetailIndex = -1;
    for (unsigned i = 0; i < grammarDetails.size(); i++) {
        const GrammarDetail* detail = &grammarDetails[i];
        ASSERT(detail->range.length > 0);
        ASSERT(detail->range.location >= 0);
        
        uint64_t detailStartOffsetInParagraph = badGrammarPhraseLocation + detail->range.location;
        
        // Skip this detail if it starts before the original search range
        if (detailStartOffsetInParagraph < startOffset)
            continue;
        
        // Skip this detail if it starts after the original search range
        if (detailStartOffsetInParagraph >= endOffset)
            continue;
        
        if (operation == Operation::MarkAll) {
            auto badGrammarRange = resolveCharacterRange(m_range, { badGrammarPhraseLocation - startOffset + detail->range.location, detail->range.length });
            addMarker(badGrammarRange, DocumentMarker::Grammar, detail->userDescription);
        }
        
        // Remember this detail only if it's earlier than our current candidate (the details aren't in a guaranteed order)
        if (earliestDetailIndex < 0 || earliestDetailLocationSoFar > detail->range.location) {
            earliestDetailIndex = i;
            earliestDetailLocationSoFar = detail->range.location;
        }
    }
    return earliestDetailIndex;
}

auto TextCheckingHelper::findUngrammaticalPhrases(Operation operation) const -> UngrammaticalPhrase
{
    UngrammaticalPhrase result;

    // Expand the search range to encompass entire paragraphs, since grammar checking needs that much context.
    // Determine the character offset from the start of the paragraph to the start of the original search range,
    // since we will want to ignore results in this area.
    TextCheckingParagraph paragraph(m_range);
    
    // Start checking from beginning of paragraph, but skip past results that occur before the start of the original search range.
    for (uint64_t startOffset = 0; startOffset < paragraph.checkingEnd(); ) {
        Vector<GrammarDetail> grammarDetails;
        int badGrammarPhraseLocation = -1;
        int badGrammarPhraseLength = 0;
        m_client.textChecker()->checkGrammarOfString(paragraph.text().substring(startOffset), grammarDetails, &badGrammarPhraseLocation, &badGrammarPhraseLength);
        
        if (!badGrammarPhraseLength) {
            ASSERT(badGrammarPhraseLocation == -1);
            return { };
        }

        ASSERT(badGrammarPhraseLocation >= 0);
        badGrammarPhraseLocation += startOffset;

        // Found some bad grammar. Find the earliest detail range that starts in our search range (if any).
        int badGrammarIndex = findUngrammaticalPhrases(operation, grammarDetails, badGrammarPhraseLocation, paragraph.checkingStart(), paragraph.checkingEnd());

        if (badGrammarIndex >= 0 && result.phrase.isEmpty()) {
            result.offset = badGrammarPhraseLocation - paragraph.checkingStart();
            result.phrase = paragraph.text().substring(badGrammarPhraseLocation, badGrammarPhraseLength).toString();
            ASSERT(static_cast<unsigned>(badGrammarIndex) < grammarDetails.size());
            result.detail = grammarDetails[badGrammarIndex];

            // Found one. We're done now, unless we're marking each instance.
            if (operation == Operation::FindFirst)
                break;
        }

        // These results were all between the start of the paragraph and the start of the search range; look beyond this phrase.
        startOffset = badGrammarPhraseLocation + badGrammarPhraseLength;
    }

    return result;
}

auto TextCheckingHelper::findFirstUngrammaticalPhrase() const -> UngrammaticalPhrase
{
    return findUngrammaticalPhrases(Operation::FindFirst);
}

TextCheckingGuesses TextCheckingHelper::guessesForMisspelledWordOrUngrammaticalPhrase(bool checkGrammar) const
{
    if (!unifiedTextCheckerEnabled())
        return { };

    if (platformDrivenTextCheckerEnabled())
        return { };

    if (m_range.collapsed())
        return { };

    // Expand the range to encompass entire paragraphs, since text checking needs that much context.
    TextCheckingParagraph paragraph(m_range);
    if (paragraph.isEmpty())
        return { };

    Vector<TextCheckingResult> results;
    OptionSet<TextCheckingType> checkingTypes { TextCheckingType::Spelling };
    if (checkGrammar)
        checkingTypes.add(TextCheckingType::Grammar);
    VisibleSelection currentSelection;
    if (auto frame = m_range.start.document().frame())
        currentSelection = frame->selection().selection();
    checkTextOfParagraph(*m_client.textChecker(), paragraph.text(), checkingTypes, results, currentSelection);

    for (auto& result : results) {
        if (result.type == TextCheckingType::Spelling && paragraph.checkingRangeMatches(result.range)) {
            String misspelledWord = paragraph.checkingSubstring().toString();
            ASSERT(misspelledWord.length());
            Vector<String> guesses;
            m_client.textChecker()->getGuessesForWord(misspelledWord, String(), currentSelection, guesses);
            m_client.updateSpellingUIWithMisspelledWord(misspelledWord);
            return { WTFMove(guesses), true, false };
        }
    }
    
    if (!checkGrammar)
        return { };

    for (auto& result : results) {
        if (result.type == TextCheckingType::Grammar && paragraph.isCheckingRangeCoveredBy(result.range)) {
            for (auto& detail : result.details) {
                ASSERT(detail.range.length > 0);
                ASSERT(detail.range.location >= 0);
                if (paragraph.checkingRangeMatches({ result.range.location + detail.range.location, detail.range.length })) {
                    String badGrammarPhrase = paragraph.text().substring(result.range.location, result.range.length).toString();
                    ASSERT(badGrammarPhrase.length());
                    m_client.updateSpellingUIWithGrammarString(badGrammarPhrase, detail);
                    return { WTFMove(detail.guesses), false, true };
                }
            }
        }
    }

    return { };
}

Optional<SimpleRange> TextCheckingHelper::markAllMisspelledWords() const
{
    return findMisspelledWords(Operation::MarkAll).second;
}

void TextCheckingHelper::markAllUngrammaticalPhrases() const
{
    findUngrammaticalPhrases(Operation::MarkAll);
}

bool TextCheckingHelper::unifiedTextCheckerEnabled() const
{
    return WebCore::unifiedTextCheckerEnabled(m_range.start.document().frame());
}

void checkTextOfParagraph(TextCheckerClient& client, StringView text, OptionSet<TextCheckingType> checkingTypes, Vector<TextCheckingResult>& results, const VisibleSelection& currentSelection)
{
#if USE(UNIFIED_TEXT_CHECKING)
    results = client.checkTextOfParagraph(text, checkingTypes, currentSelection);
#else
    UNUSED_PARAM(currentSelection);

    Vector<TextCheckingResult> misspellings;
    if (checkingTypes.contains(TextCheckingType::Spelling))
        findMisspellings(client, text, misspellings);

    // Look for grammatical errors that occur before the first misspelling.
    Vector<TextCheckingResult> grammaticalErrors;
    if (checkingTypes.contains(TextCheckingType::Grammar)) {
        unsigned grammarCheckLength = text.length();
        for (auto& misspelling : misspellings)
            grammarCheckLength = std::min<unsigned>(grammarCheckLength, misspelling.range.location);
        findGrammaticalErrors(client, text.substring(0, grammarCheckLength), grammaticalErrors);
    }

    results = WTFMove(grammaticalErrors);

    if (results.isEmpty())
        results = WTFMove(misspellings);
    else
        results.appendVector(misspellings);
#endif // USE(UNIFIED_TEXT_CHECKING)
}

bool unifiedTextCheckerEnabled(const Frame* frame)
{
    if (!frame)
        return false;
    return frame->settings().unifiedTextCheckerEnabled();
}

bool platformDrivenTextCheckerEnabled()
{
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    return true;
#else
    return false;
#endif
}

}
