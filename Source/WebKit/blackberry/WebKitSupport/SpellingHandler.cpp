/*
 * Copyright (C) Research In Motion Limited 2013. All rights reserved.
 */

#include "config.h"
#include "SpellingHandler.h"

#include "DOMSupport.h"
#include "InputHandler.h"
#include "Range.h"
#include "SpellChecker.h"
#include "TextIterator.h"
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

    m_timer.startOneShot(0);
}

void SpellingHandler::createSpellCheckRequest(PassRefPtr<WebCore::Range> rangeForSpellCheckingPtr, WebCore::TextCheckingProcessType textCheckingProcessType)
{
    RefPtr<WebCore::Range> rangeForSpellChecking = rangeForSpellCheckingPtr;
    if (isSpellCheckActive()) {
        m_inputHandler->callRequestCheckingFor(SpellCheckRequest::create(TextCheckingTypeSpelling, textCheckingProcessType, rangeForSpellChecking, rangeForSpellChecking));
        m_timer.startOneShot(s_timeout);
    }
}

void SpellingHandler::parseBlockForSpellChecking(WebCore::Timer<SpellingHandler>*)
{
    if (isEndOfDocument(m_startOfCurrentLine)) {
        m_isSpellCheckActive = false;
        return;
    }

    // Create a selection with the start and end points of the line, and convert to Range to create a SpellCheckRequest.
    RefPtr<Range> rangeForSpellChecking = VisibleSelection(m_startOfCurrentLine, m_endOfCurrentLine).toNormalizedRange();
    if (!rangeForSpellChecking) {
        SpellingLog(Platform::LogLevelInfo, "SpellingHandler::spellCheckBlock Failed to set text range for spellchecking.");
        return;
    }

    if (rangeForSpellChecking->text().length() < MaxSpellCheckingStringLength) {
        m_startOfCurrentLine = nextLinePosition(m_startOfCurrentLine, m_startOfCurrentLine.lineDirectionPointForBlockDirectionNavigation());
        m_endOfCurrentLine = endOfLine(m_startOfCurrentLine);
    } else {
        // Iterate through words from the start of the line to the end.
        rangeForSpellChecking = getRangeForSpellCheckWithFineGranularity(m_startOfCurrentLine, m_endOfCurrentLine);
        if (!rangeForSpellChecking) {
            SpellingLog(Platform::LogLevelWarn, "SpellingHandler::spellCheckBlock Failed to set text range with fine granularity.");
            return;
        }
        m_startOfCurrentLine = VisiblePosition(rangeForSpellChecking->endPosition());
        m_endOfCurrentLine = endOfLine(m_startOfCurrentLine);
        rangeForSpellChecking = DOMSupport::trimWhitespaceFromRange(VisiblePosition(rangeForSpellChecking->startPosition()), VisiblePosition(rangeForSpellChecking->endPosition()));
    }

    SpellingLog(Platform::LogLevelInfo,
        "SpellingHandler::spellCheckBlock Substring text is '%s', of size %d",
        rangeForSpellChecking->text().latin1().data(),
        rangeForSpellChecking->text().length());

    // Call spellcheck with substring.
    createSpellCheckRequest(rangeForSpellChecking, m_textCheckingProcessType);
}

PassRefPtr<Range> SpellingHandler::getRangeForSpellCheckWithFineGranularity(WebCore::VisiblePosition startPosition, WebCore::VisiblePosition endPosition)
{
    VisiblePosition endOfCurrentWord = endOfWord(startPosition);

    // Keep iterating until one of our cases is hit, or we've incremented the starting position right to the end.
    while (startPosition != endPosition) {
        // Check the text length within this range.
        if (VisibleSelection(startPosition, endOfCurrentWord).toNormalizedRange()->text().length() >= MaxSpellCheckingStringLength) {
            // If this is not the first word, return a Range with end boundary set to the previous word.
            if (startOfWord(endOfCurrentWord, LeftWordIfOnBoundary) != startPosition && !DOMSupport::isEmptyRangeOrAllSpaces(startPosition, endOfCurrentWord))
                return VisibleSelection(startPosition, endOfWord(previousWordPosition(endOfCurrentWord), LeftWordIfOnBoundary)).toNormalizedRange();

            // Our first word has gone over the character limit. Increment the starting position past an uncheckable word.
            startPosition = endOfCurrentWord;
            endOfCurrentWord = endOfWord(nextWordPosition(endOfCurrentWord));
        } else if (endOfCurrentWord == endPosition)
            return VisibleSelection(startPosition, endPosition).toNormalizedRange(); // Return the last segment if the end of our word lies at the end of the range.
        else
            endOfCurrentWord = endOfWord(nextWordPosition(endOfCurrentWord)); // Increment the current word.
    }
    return 0;
}

} // WebKit
} // BlackBerry
