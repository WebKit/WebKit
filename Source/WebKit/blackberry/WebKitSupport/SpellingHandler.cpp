/*
 * Copyright (C) Research In Motion Limited 2013. All rights reserved.
 */

#include "config.h"
#include "SpellingHandler.h"

#include "DOMSupport.h"
#include "InputHandler.h"
#include "Range.h"
#include "SpellChecker.h"
#include "visible_units.h"

#include <BlackBerryPlatformIMF.h>
#include <BlackBerryPlatformLog.h>

#define ENABLE_SPELLING_LOG 0

using namespace BlackBerry::Platform;
using namespace WebCore;

#if ENABLE_SPELLING_LOG
#define SpellingLog(severity, format, ...) Platform::logAlways(severity, format, ## __VA_ARGS__)
#else
#define SpellingLog(severity, format, ...)
#endif // ENABLE_SPELLING_LOG

static const double s_timeout = 0.05;

namespace BlackBerry {
namespace WebKit {

SpellingHandler::SpellingHandler(InputHandler* inputHandler)
    : m_inputHandler(inputHandler)
    , m_timer(this, &SpellingHandler::parseBlockForSpellChecking)
    , m_isSpellCheckActive(false)
{
}

SpellingHandler::~SpellingHandler()
{
}

void SpellingHandler::spellCheckTextBlock(WebCore::VisibleSelection& visibleSelection, WebCore::TextCheckingProcessType textCheckingProcessType)
{
    SpellingLog(Platform::LogLevelInfo, "SpellingHandler::spellCheckTextBlock");

    // Check if this request can be sent off in one message, or if it needs to be broken down.
    RefPtr<Range> rangeForSpellChecking = visibleSelection.toNormalizedRange();
    if (!rangeForSpellChecking || !rangeForSpellChecking->text() || !rangeForSpellChecking->text().length())
        return;

    // Spellcheck Batch requests are used when focusing an element. During this time, we might have a lingering request
    // from a previously focused element.
    if (textCheckingProcessType == TextCheckingProcessBatch) {
        // If a previous request is being processed, stop it before continueing.
        if (m_timer.isActive())
            m_timer.stop();
    }

    m_isSpellCheckActive = true;

    // If we have a batch request, try to send off the entire block.
    if (textCheckingProcessType == TextCheckingProcessBatch) {
        // If total block text is under the limited amount, send the entire chunk.
        if (rangeForSpellChecking->text().length() < MaxSpellCheckingStringLength) {
            createSpellCheckRequest(rangeForSpellChecking, TextCheckingProcessBatch);
            return;
        }
    }

    // Since we couldn't check the entire block at once, set up starting and ending markers to fire incrementally.
    m_startOfCurrentLine = startOfLine(visibleSelection.visibleStart());
    m_endOfCurrentLine = endOfLine(m_startOfCurrentLine);
    m_textCheckingProcessType = textCheckingProcessType;

    // Find the end of the region we're intending on checking.
    m_endOfRange = endOfLine(visibleSelection.visibleEnd());
    m_timer.startOneShot(0);
}

void SpellingHandler::createSpellCheckRequest(PassRefPtr<WebCore::Range> rangeForSpellCheckingPtr, WebCore::TextCheckingProcessType textCheckingProcessType)
{
    RefPtr<WebCore::Range> rangeForSpellChecking = rangeForSpellCheckingPtr;
    rangeForSpellChecking = DOMSupport::trimWhitespaceFromRange(rangeForSpellChecking);
    if (!rangeForSpellChecking)
        return;

    SpellingLog(Platform::LogLevelInfo, "SpellingHandler::createSpellCheckRequest Substring text is '%s', of size %d"
        , rangeForSpellChecking->text().latin1().data()
        , rangeForSpellChecking->text().length());

    if (rangeForSpellChecking->text().length() >= MinSpellCheckingStringLength)
        m_inputHandler->callRequestCheckingFor(SpellCheckRequest::create(TextCheckingTypeSpelling, textCheckingProcessType, rangeForSpellChecking, rangeForSpellChecking));
}

void SpellingHandler::parseBlockForSpellChecking(WebCore::Timer<SpellingHandler>*)
{
    // Create a selection with the start and end points of the line, and convert to Range to create a SpellCheckRequest.
    RefPtr<Range> rangeForSpellChecking = makeRange(m_startOfCurrentLine, m_endOfCurrentLine);
    if (!rangeForSpellChecking) {
        SpellingLog(Platform::LogLevelInfo, "SpellingHandler::parseBlockForSpellChecking Failed to set text range for spellchecking.");
        return;
    }

    if (rangeForSpellChecking->text().length() < MaxSpellCheckingStringLength) {
        if (m_endOfCurrentLine == m_endOfRange) {
            createSpellCheckRequest(rangeForSpellChecking, m_textCheckingProcessType);
            m_isSpellCheckActive = false;
            return;
        }
        m_endOfCurrentLine = endOfLine(nextLinePosition(m_endOfCurrentLine, m_endOfCurrentLine.lineDirectionPointForBlockDirectionNavigation()));
        m_timer.startOneShot(s_timeout);
        return;
    }

    // Iterate through words from the start of the line to the end.
    rangeForSpellChecking = getRangeForSpellCheckWithFineGranularity(m_startOfCurrentLine, m_endOfCurrentLine);

    m_startOfCurrentLine = VisiblePosition(rangeForSpellChecking->endPosition());
    m_endOfCurrentLine = endOfLine(m_startOfCurrentLine);

    // Call spellcheck with substring.
    createSpellCheckRequest(rangeForSpellChecking, m_textCheckingProcessType);
    if (isSpellCheckActive())
        m_timer.startOneShot(s_timeout);
}

PassRefPtr<Range> SpellingHandler::getRangeForSpellCheckWithFineGranularity(WebCore::VisiblePosition startPosition, WebCore::VisiblePosition endPosition)
{
    SpellingLog(Platform::LogLevelWarn, "SpellingHandler::getRangeForSpellCheckWithFineGranularity");
    ASSERT(makeRange(startPosition, endOfCurrentWord));
    VisiblePosition endOfCurrentWord = endOfWord(startPosition);

    // Keep iterating until one of our cases is hit, or we've incremented the starting position right to the end.
    while (startPosition != endPosition) {
        // Check the text length within this range.
        if (makeRange(startPosition, endOfCurrentWord)->text().length() >= MaxSpellCheckingStringLength) {
            // If this is not the first word, return a Range with end boundary set to the previous word.
            if (startOfWord(endOfCurrentWord, LeftWordIfOnBoundary) != startPosition && !DOMSupport::isEmptyRangeOrAllSpaces(startPosition, endOfCurrentWord)) {
                // When a series of whitespace follows a word, previousWordPosition will jump passed all of them, and using LeftWordIfOnBoundary brings us to
                // our starting position. Check for this case and use RightWordIfOnBoundary to jump back over the word.
                VisiblePosition endOfPreviousWord = endOfWord(previousWordPosition(endOfCurrentWord), LeftWordIfOnBoundary);
                if (startPosition == endOfPreviousWord)
                    return makeRange(startPosition, endOfWord(previousWordPosition(endOfCurrentWord), RightWordIfOnBoundary));
                return makeRange(startPosition, endOfPreviousWord);
            }
            // Our first word has gone over the character limit. Increment the starting position past an uncheckable word.
            startPosition = endOfCurrentWord;
            endOfCurrentWord = endOfWord(nextWordPosition(endOfCurrentWord));
        } else if (endOfCurrentWord == endPosition)
            return makeRange(startPosition, endPosition); // Return the last segment if the end of our word lies at the end of the range.
        else
            endOfCurrentWord = endOfWord(nextWordPosition(endOfCurrentWord)); // Increment the current word.
    }
    // This will return an range with no string, but allows processing to continue
    return makeRange(startPosition, endPosition);
}

} // WebKit
} // BlackBerry
