/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008 Apple Inc.  All right reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef BidiResolver_h
#define BidiResolver_h

#include "BidiContext.h"
#include "BidiRunList.h"
#include "TextDirection.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderObject;

template <class Iterator> class MidpointState {
public:
    MidpointState()
    {
        reset();
    }
    
    void reset()
    {
        m_numMidpoints = 0;
        m_currentMidpoint = 0;
        m_betweenMidpoints = false;
    }
    
    void startIgnoringSpaces(const Iterator& midpoint)
    {
        ASSERT(!(m_numMidpoints % 2));
        addMidpoint(midpoint);
    }

    void stopIgnoringSpaces(const Iterator& midpoint)
    {
        ASSERT(m_numMidpoints % 2);
        addMidpoint(midpoint);
    }

    // When ignoring spaces, this needs to be called for objects that need line boxes such as RenderInlines or
    // hard line breaks to ensure that they're not ignored.
    void ensureLineBoxInsideIgnoredSpaces(RenderObject* renderer)
    {
        Iterator midpoint(0, renderer, 0);
        stopIgnoringSpaces(midpoint);
        startIgnoringSpaces(midpoint);
    }

    Vector<Iterator>& midpoints() { return m_midpoints; }
    const unsigned& numMidpoints() const { return m_numMidpoints; }
    const unsigned& currentMidpoint() const { return m_currentMidpoint; }
    void incrementCurrentMidpoint() { ++m_currentMidpoint; }
    void decreaseNumMidpoints() { --m_numMidpoints; }
    const bool& betweenMidpoints() const { return m_betweenMidpoints; }
    void setBetweenMidpoints(bool betweenMidpoint) { m_betweenMidpoints = betweenMidpoint; }
private:
    // The goal is to reuse the line state across multiple
    // lines so we just keep an array around for midpoints and never clear it across multiple
    // lines. We track the number of items and position using the two other variables.
    Vector<Iterator> m_midpoints;
    unsigned m_numMidpoints;
    unsigned m_currentMidpoint;
    bool m_betweenMidpoints;

    void addMidpoint(const Iterator& midpoint)
    {
        if (m_midpoints.size() <= m_numMidpoints)
            m_midpoints.grow(m_numMidpoints + 10);

        Iterator* midpointsIterator = m_midpoints.data();
        midpointsIterator[m_numMidpoints++] = midpoint;
    }
};

// The BidiStatus at a given position (typically the end of a line) can
// be cached and then used to restart bidi resolution at that position.
struct BidiStatus {
    BidiStatus()
        : eor(U_OTHER_NEUTRAL)
        , lastStrong(U_OTHER_NEUTRAL)
        , last(U_OTHER_NEUTRAL)
    {
    }

    // Creates a BidiStatus representing a new paragraph root with a default direction.
    // Uses TextDirection as it only has two possibilities instead of UCharDirection which has at least 19.
    BidiStatus(TextDirection textDirection, bool isOverride)
    {
        UCharDirection direction = textDirection == LTR ? U_LEFT_TO_RIGHT : U_RIGHT_TO_LEFT;
        eor = lastStrong = last = direction;
        context = BidiContext::create(textDirection == LTR ? 0 : 1, direction, isOverride);
    }

    BidiStatus(UCharDirection eorDir, UCharDirection lastStrongDir, UCharDirection lastDir, PassRefPtr<BidiContext> bidiContext)
        : eor(eorDir)
        , lastStrong(lastStrongDir)
        , last(lastDir)
        , context(bidiContext)
    {
    }

    UCharDirection eor;
    UCharDirection lastStrong;
    UCharDirection last;
    RefPtr<BidiContext> context;
};

class BidiEmbedding {
public:
    BidiEmbedding(UCharDirection direction, BidiEmbeddingSource source)
    : m_direction(direction)
    , m_source(source)
    {
    }

    UCharDirection direction() const { return m_direction; }
    BidiEmbeddingSource source() const { return m_source; }
private:
    UCharDirection m_direction;
    BidiEmbeddingSource m_source;
};

inline bool operator==(const BidiStatus& status1, const BidiStatus& status2)
{
    return status1.eor == status2.eor && status1.last == status2.last && status1.lastStrong == status2.lastStrong && *(status1.context) == *(status2.context);
}

inline bool operator!=(const BidiStatus& status1, const BidiStatus& status2)
{
    return !(status1 == status2);
}

struct BidiCharacterRun {
    BidiCharacterRun(int start, int stop, BidiContext* context, UCharDirection direction)
        : m_override(context->override())
        , m_next(0)
        , m_start(start)
        , m_stop(stop)
    {
        if (direction == U_OTHER_NEUTRAL)
            direction = context->dir();

        m_level = context->level();

        // add level of run (cases I1 & I2)
        if (m_level % 2) {
            if (direction == U_LEFT_TO_RIGHT || direction == U_ARABIC_NUMBER || direction == U_EUROPEAN_NUMBER)
                m_level++;
        } else {
            if (direction == U_RIGHT_TO_LEFT)
                m_level++;
            else if (direction == U_ARABIC_NUMBER || direction == U_EUROPEAN_NUMBER)
                m_level += 2;
        }
    }

    int start() const { return m_start; }
    int stop() const { return m_stop; }
    unsigned char level() const { return m_level; }
    bool reversed(bool visuallyOrdered) { return m_level % 2 && !visuallyOrdered; }
    bool dirOverride(bool visuallyOrdered) { return m_override || visuallyOrdered; }

    BidiCharacterRun* next() const { return m_next; }
    void setNext(BidiCharacterRun* next) { m_next = next; }

    // Do not add anything apart from bitfields until after m_next. See https://bugs.webkit.org/show_bug.cgi?id=100173
    bool m_override : 1;
    bool m_hasHyphen : 1; // Used by BidiRun subclass which is a layering violation but enables us to save 8 bytes per object on 64-bit.
#if ENABLE(CSS_SHAPES)
    bool m_startsSegment : 1; // Same comment as m_hasHyphen.
#endif
    unsigned char m_level;
    BidiCharacterRun* m_next;
    int m_start;
    int m_stop;
};

enum VisualDirectionOverride {
    NoVisualOverride,
    VisualLeftToRightOverride,
    VisualRightToLeftOverride
};

// BidiResolver is WebKit's implementation of the Unicode Bidi Algorithm
// http://unicode.org/reports/tr9
template <class Iterator, class Run> class BidiResolver {
    WTF_MAKE_NONCOPYABLE(BidiResolver);
public:
    BidiResolver()
        : m_direction(U_OTHER_NEUTRAL)
        , m_reachedEndOfLine(false)
        , m_emptyRun(true)
        , m_nestedIsolateCount(0)
    {
    }

#ifndef NDEBUG
    ~BidiResolver();
#endif

    const Iterator& position() const { return m_current; }
    void setPositionIgnoringNestedIsolates(const Iterator& position) { m_current = position; }
    void setPosition(const Iterator& position, unsigned nestedIsolatedCount)
    {
        m_current = position;
        m_nestedIsolateCount = nestedIsolatedCount;
    }

    void increment() { m_current.increment(); }

    BidiContext* context() const { return m_status.context.get(); }
    void setContext(PassRefPtr<BidiContext> c) { m_status.context = c; }

    void setLastDir(UCharDirection lastDir) { m_status.last = lastDir; }
    void setLastStrongDir(UCharDirection lastStrongDir) { m_status.lastStrong = lastStrongDir; }
    void setEorDir(UCharDirection eorDir) { m_status.eor = eorDir; }

    UCharDirection dir() const { return m_direction; }
    void setDir(UCharDirection d) { m_direction = d; }

    const BidiStatus& status() const { return m_status; }
    void setStatus(const BidiStatus s) { m_status = s; }

    MidpointState<Iterator>& midpointState() { return m_midpointState; }

    // The current algorithm handles nested isolates one layer of nesting at a time.
    // But when we layout each isolated span, we will walk into (and ignore) all
    // child isolated spans.
    void enterIsolate() { m_nestedIsolateCount++; }
    void exitIsolate() { ASSERT(m_nestedIsolateCount >= 1); m_nestedIsolateCount--; }
    bool inIsolate() const { return m_nestedIsolateCount; }

    void embed(UCharDirection, BidiEmbeddingSource);
    bool commitExplicitEmbedding();

    void createBidiRunsForLine(const Iterator& end, VisualDirectionOverride = NoVisualOverride, bool hardLineBreak = false);

    BidiRunList<Run>& runs() { return m_runs; }

    // FIXME: This used to be part of deleteRuns() but was a layering violation.
    // It's unclear if this is still needed.
    void markCurrentRunEmpty() { m_emptyRun = true; }

    Vector<Run*>& isolatedRuns() { return m_isolatedRuns; }

protected:
    // FIXME: Instead of InlineBidiResolvers subclassing this method, we should
    // pass in some sort of Traits object which knows how to create runs for appending.
    void appendRun();

    Iterator m_current;
    // sor and eor are "start of run" and "end of run" respectively and correpond
    // to abreviations used in UBA spec: http://unicode.org/reports/tr9/#BD7
    Iterator m_sor; // Points to the first character in the current run.
    Iterator m_eor; // Points to the last character in the current run.
    Iterator m_last;
    BidiStatus m_status;
    UCharDirection m_direction;
    Iterator endOfLine;
    bool m_reachedEndOfLine;
    Iterator m_lastBeforeET; // Before a U_EUROPEAN_NUMBER_TERMINATOR
    bool m_emptyRun;

    // FIXME: This should not belong to the resolver, but rather be passed
    // into createBidiRunsForLine by the caller.
    BidiRunList<Run> m_runs;

    MidpointState<Iterator> m_midpointState;

    unsigned m_nestedIsolateCount;
    Vector<Run*> m_isolatedRuns;

private:
    void raiseExplicitEmbeddingLevel(UCharDirection from, UCharDirection to);
    void lowerExplicitEmbeddingLevel(UCharDirection from);
    void checkDirectionInLowerRaiseEmbeddingLevel();

    void updateStatusLastFromCurrentDirection(UCharDirection);
    void reorderRunsFromLevels();

    Vector<BidiEmbedding, 8> m_currentExplicitEmbeddingSequence;
};

#ifndef NDEBUG
template <class Iterator, class Run>
BidiResolver<Iterator, Run>::~BidiResolver()
{
    // The owner of this resolver should have handled the isolated runs
    // or should never have called enterIsolate().
    ASSERT(m_isolatedRuns.isEmpty());
    ASSERT(!m_nestedIsolateCount);
}
#endif

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::appendRun()
{
    if (!m_emptyRun && !m_eor.atEnd()) {
        unsigned startOffset = m_sor.offset();
        unsigned endOffset = m_eor.offset();

        if (!endOfLine.atEnd() && endOffset >= endOfLine.offset()) {
            m_reachedEndOfLine = true;
            endOffset = endOfLine.offset();
        }

        if (endOffset >= startOffset)
            m_runs.addRun(new Run(startOffset, endOffset + 1, context(), m_direction));

        m_eor.increment();
        m_sor = m_eor;
    }

    m_direction = U_OTHER_NEUTRAL;
    m_status.eor = U_OTHER_NEUTRAL;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::embed(UCharDirection dir, BidiEmbeddingSource source)
{
    // Isolated spans compute base directionality during their own UBA run.
    // Do not insert fake embed characters once we enter an isolated span.
    ASSERT(!inIsolate());

    ASSERT(dir == U_POP_DIRECTIONAL_FORMAT || dir == U_LEFT_TO_RIGHT_EMBEDDING || dir == U_LEFT_TO_RIGHT_OVERRIDE || dir == U_RIGHT_TO_LEFT_EMBEDDING || dir == U_RIGHT_TO_LEFT_OVERRIDE);
    m_currentExplicitEmbeddingSequence.append(BidiEmbedding(dir, source));
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::checkDirectionInLowerRaiseEmbeddingLevel()
{
    ASSERT(m_status.eor != U_OTHER_NEUTRAL || m_eor.atEnd());
    ASSERT(m_status.last != U_DIR_NON_SPACING_MARK
        && m_status.last != U_BOUNDARY_NEUTRAL
        && m_status.last != U_RIGHT_TO_LEFT_EMBEDDING
        && m_status.last != U_LEFT_TO_RIGHT_EMBEDDING
        && m_status.last != U_RIGHT_TO_LEFT_OVERRIDE 
        && m_status.last != U_LEFT_TO_RIGHT_OVERRIDE
        && m_status.last != U_POP_DIRECTIONAL_FORMAT);
    if (m_direction == U_OTHER_NEUTRAL)
        m_direction = m_status.lastStrong == U_LEFT_TO_RIGHT ? U_LEFT_TO_RIGHT : U_RIGHT_TO_LEFT;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::lowerExplicitEmbeddingLevel(UCharDirection from)
{
    if (!m_emptyRun && m_eor != m_last) {
        checkDirectionInLowerRaiseEmbeddingLevel();
        // bidi.sor ... bidi.eor ... bidi.last eor; need to append the bidi.sor-bidi.eor run or extend it through bidi.last
        if (from == U_LEFT_TO_RIGHT) {
            // bidi.sor ... bidi.eor ... bidi.last L
            if (m_status.eor == U_EUROPEAN_NUMBER) {
                if (m_status.lastStrong != U_LEFT_TO_RIGHT) {
                    m_direction = U_EUROPEAN_NUMBER;
                    appendRun();
                }
            } else if (m_status.eor == U_ARABIC_NUMBER) {
                m_direction = U_ARABIC_NUMBER;
                appendRun();
            } else if (m_status.lastStrong != U_LEFT_TO_RIGHT) {
                appendRun();
                m_direction = U_LEFT_TO_RIGHT;
            }
        } else if (m_status.eor == U_EUROPEAN_NUMBER || m_status.eor == U_ARABIC_NUMBER || m_status.lastStrong == U_LEFT_TO_RIGHT) {
            appendRun();
            m_direction = U_RIGHT_TO_LEFT;
        }
        m_eor = m_last;
    }

    appendRun();
    m_emptyRun = true;

    // sor for the new run is determined by the higher level (rule X10)
    setLastDir(from);
    setLastStrongDir(from);
    m_eor = Iterator();
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::raiseExplicitEmbeddingLevel(UCharDirection from, UCharDirection to)
{
    if (!m_emptyRun && m_eor != m_last) {
        checkDirectionInLowerRaiseEmbeddingLevel();
        // bidi.sor ... bidi.eor ... bidi.last eor; need to append the bidi.sor-bidi.eor run or extend it through bidi.last
        if (to == U_LEFT_TO_RIGHT) {
            // bidi.sor ... bidi.eor ... bidi.last L
            if (m_status.eor == U_EUROPEAN_NUMBER) {
                if (m_status.lastStrong != U_LEFT_TO_RIGHT) {
                    m_direction = U_EUROPEAN_NUMBER;
                    appendRun();
                }
            } else if (m_status.eor == U_ARABIC_NUMBER) {
                m_direction = U_ARABIC_NUMBER;
                appendRun();
            } else if (m_status.lastStrong != U_LEFT_TO_RIGHT && from == U_LEFT_TO_RIGHT) {
                appendRun();
                m_direction = U_LEFT_TO_RIGHT;
            }
        } else if (m_status.eor == U_ARABIC_NUMBER
            || (m_status.eor == U_EUROPEAN_NUMBER && (m_status.lastStrong != U_LEFT_TO_RIGHT || from == U_RIGHT_TO_LEFT))
            || (m_status.eor != U_EUROPEAN_NUMBER && m_status.lastStrong == U_LEFT_TO_RIGHT && from == U_RIGHT_TO_LEFT)) {
            appendRun();
            m_direction = U_RIGHT_TO_LEFT;
        }
        m_eor = m_last;
    }

    appendRun();
    m_emptyRun = true;

    setLastDir(to);
    setLastStrongDir(to);
    m_eor = Iterator();
}

template <class Iterator, class Run>
bool BidiResolver<Iterator, Run>::commitExplicitEmbedding()
{
    // When we're "inIsolate()" we're resolving the parent context which
    // ignores (skips over) the isolated content, including embedding levels.
    // We should never accrue embedding levels while skipping over isolated content.
    ASSERT(!inIsolate() || m_currentExplicitEmbeddingSequence.isEmpty());

    unsigned char fromLevel = context()->level();
    RefPtr<BidiContext> toContext = context();

    for (size_t i = 0; i < m_currentExplicitEmbeddingSequence.size(); ++i) {
        BidiEmbedding embedding = m_currentExplicitEmbeddingSequence[i];
        if (embedding.direction() == U_POP_DIRECTIONAL_FORMAT) {
            if (BidiContext* parentContext = toContext->parent())
                toContext = parentContext;
        } else {
            UCharDirection direction = (embedding.direction() == U_RIGHT_TO_LEFT_EMBEDDING || embedding.direction() == U_RIGHT_TO_LEFT_OVERRIDE) ? U_RIGHT_TO_LEFT : U_LEFT_TO_RIGHT;
            bool override = embedding.direction() == U_LEFT_TO_RIGHT_OVERRIDE || embedding.direction() == U_RIGHT_TO_LEFT_OVERRIDE;
            unsigned char level = toContext->level();
            if (direction == U_RIGHT_TO_LEFT)
                level = nextGreaterOddLevel(level);
            else
                level = nextGreaterEvenLevel(level);
            if (level < 61)
                toContext = BidiContext::create(level, direction, override, embedding.source(), toContext.get());
        }
    }

    unsigned char toLevel = toContext->level();

    if (toLevel > fromLevel)
        raiseExplicitEmbeddingLevel(fromLevel % 2 ? U_RIGHT_TO_LEFT : U_LEFT_TO_RIGHT, toLevel % 2 ? U_RIGHT_TO_LEFT : U_LEFT_TO_RIGHT);
    else if (toLevel < fromLevel)
        lowerExplicitEmbeddingLevel(fromLevel % 2 ? U_RIGHT_TO_LEFT : U_LEFT_TO_RIGHT);

    setContext(toContext);

    m_currentExplicitEmbeddingSequence.clear();

    return fromLevel != toLevel;
}

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::updateStatusLastFromCurrentDirection(UCharDirection dirCurrent)
{
    switch (dirCurrent) {
    case U_EUROPEAN_NUMBER_TERMINATOR:
        if (m_status.last != U_EUROPEAN_NUMBER)
            m_status.last = U_EUROPEAN_NUMBER_TERMINATOR;
        break;
    case U_EUROPEAN_NUMBER_SEPARATOR:
    case U_COMMON_NUMBER_SEPARATOR:
    case U_SEGMENT_SEPARATOR:
    case U_WHITE_SPACE_NEUTRAL:
    case U_OTHER_NEUTRAL:
        switch (m_status.last) {
        case U_LEFT_TO_RIGHT:
        case U_RIGHT_TO_LEFT:
        case U_RIGHT_TO_LEFT_ARABIC:
        case U_EUROPEAN_NUMBER:
        case U_ARABIC_NUMBER:
            m_status.last = dirCurrent;
            break;
        default:
            m_status.last = U_OTHER_NEUTRAL;
        }
        break;
    case U_DIR_NON_SPACING_MARK:
    case U_BOUNDARY_NEUTRAL:
    case U_RIGHT_TO_LEFT_EMBEDDING:
    case U_LEFT_TO_RIGHT_EMBEDDING:
    case U_RIGHT_TO_LEFT_OVERRIDE:
    case U_LEFT_TO_RIGHT_OVERRIDE:
    case U_POP_DIRECTIONAL_FORMAT:
        // ignore these
        break;
    case U_EUROPEAN_NUMBER:
        FALLTHROUGH;
    default:
        m_status.last = dirCurrent;
    }
}

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::reorderRunsFromLevels()
{
    unsigned char levelLow = 128;
    unsigned char levelHigh = 0;
    for (Run* run = m_runs.firstRun(); run; run = run->next()) {
        levelHigh = std::max(run->level(), levelHigh);
        levelLow = std::min(run->level(), levelLow);
    }

    // This implements reordering of the line (L2 according to Bidi spec):
    // http://unicode.org/reports/tr9/#L2
    // L2. From the highest level found in the text to the lowest odd level on each line,
    // reverse any contiguous sequence of characters that are at that level or higher.

    // Reversing is only done up to the lowest odd level.
    if (!(levelLow % 2))
        levelLow++;

    unsigned count = m_runs.runCount() - 1;

    while (levelHigh >= levelLow) {
        unsigned i = 0;
        Run* run = m_runs.firstRun();
        while (i < count) {
            for (;i < count && run && run->level() < levelHigh; i++)
                run = run->next();
            unsigned start = i;
            for (;i <= count && run && run->level() >= levelHigh; i++)
                run = run->next();
            unsigned end = i - 1;
            m_runs.reverseRuns(start, end);
        }
        levelHigh--;
    }
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::createBidiRunsForLine(const Iterator& end, VisualDirectionOverride override, bool hardLineBreak)
{
    ASSERT(m_direction == U_OTHER_NEUTRAL);

    if (override != NoVisualOverride) {
        m_emptyRun = false;
        m_sor = m_current;
        m_eor = Iterator();
        while (m_current != end && !m_current.atEnd()) {
            m_eor = m_current;
            increment();
        }
        m_direction = override == VisualLeftToRightOverride ? U_LEFT_TO_RIGHT : U_RIGHT_TO_LEFT;
        appendRun();
        m_runs.setLogicallyLastRun(m_runs.lastRun());
        if (override == VisualRightToLeftOverride && m_runs.runCount())
            m_runs.reverseRuns(0, m_runs.runCount() - 1);
        return;
    }

    m_emptyRun = true;

    m_eor = Iterator();

    m_last = m_current;
    bool pastEnd = false;
    BidiResolver<Iterator, Run> stateAtEnd;

    while (true) {
        UCharDirection dirCurrent;
        if (pastEnd && (hardLineBreak || m_current.atEnd())) {
            BidiContext* c = context();
            if (hardLineBreak) {
                // A deviation from the Unicode Bidi Algorithm in order to match
                // WinIE and user expectations: hard line breaks reset bidi state
                // coming from unicode bidi control characters, but not those from
                // DOM nodes with specified directionality
                stateAtEnd.setContext(c->copyStackRemovingUnicodeEmbeddingContexts());

                dirCurrent = stateAtEnd.context()->dir();
                stateAtEnd.setEorDir(dirCurrent);
                stateAtEnd.setLastDir(dirCurrent);
                stateAtEnd.setLastStrongDir(dirCurrent);
            } else {
                while (c->parent())
                    c = c->parent();
                dirCurrent = c->dir();
            }
        } else {
            dirCurrent = m_current.direction();
            if (context()->override()
                    && dirCurrent != U_RIGHT_TO_LEFT_EMBEDDING
                    && dirCurrent != U_LEFT_TO_RIGHT_EMBEDDING
                    && dirCurrent != U_RIGHT_TO_LEFT_OVERRIDE
                    && dirCurrent != U_LEFT_TO_RIGHT_OVERRIDE
                    && dirCurrent != U_POP_DIRECTIONAL_FORMAT)
                dirCurrent = context()->dir();
            else if (dirCurrent == U_DIR_NON_SPACING_MARK)
                dirCurrent = m_status.last;
        }

        // We ignore all character directionality while in unicode-bidi: isolate spans.
        // We'll handle ordering the isolated characters in a second pass.
        if (inIsolate())
            dirCurrent = U_OTHER_NEUTRAL;

        ASSERT(m_status.eor != U_OTHER_NEUTRAL || m_eor.atEnd());
        switch (dirCurrent) {

        // embedding and overrides (X1-X9 in the Bidi specs)
        case U_RIGHT_TO_LEFT_EMBEDDING:
        case U_LEFT_TO_RIGHT_EMBEDDING:
        case U_RIGHT_TO_LEFT_OVERRIDE:
        case U_LEFT_TO_RIGHT_OVERRIDE:
        case U_POP_DIRECTIONAL_FORMAT:
            embed(dirCurrent, FromUnicode);
            commitExplicitEmbedding();
            break;

        // strong types
        case U_LEFT_TO_RIGHT:
            switch(m_status.last) {
                case U_RIGHT_TO_LEFT:
                case U_RIGHT_TO_LEFT_ARABIC:
                case U_EUROPEAN_NUMBER:
                case U_ARABIC_NUMBER:
                    if (m_status.last != U_EUROPEAN_NUMBER || m_status.lastStrong != U_LEFT_TO_RIGHT)
                        appendRun();
                    break;
                case U_LEFT_TO_RIGHT:
                    break;
                case U_EUROPEAN_NUMBER_SEPARATOR:
                case U_EUROPEAN_NUMBER_TERMINATOR:
                case U_COMMON_NUMBER_SEPARATOR:
                case U_BOUNDARY_NEUTRAL:
                case U_BLOCK_SEPARATOR:
                case U_SEGMENT_SEPARATOR:
                case U_WHITE_SPACE_NEUTRAL:
                case U_OTHER_NEUTRAL:
                    if (m_status.eor == U_EUROPEAN_NUMBER) {
                        if (m_status.lastStrong != U_LEFT_TO_RIGHT) {
                            // the numbers need to be on a higher embedding level, so let's close that run
                            m_direction = U_EUROPEAN_NUMBER;
                            appendRun();
                            if (context()->dir() != U_LEFT_TO_RIGHT) {
                                // the neutrals take the embedding direction, which is R
                                m_eor = m_last;
                                m_direction = U_RIGHT_TO_LEFT;
                                appendRun();
                            }
                        }
                    } else if (m_status.eor == U_ARABIC_NUMBER) {
                        // Arabic numbers are always on a higher embedding level, so let's close that run
                        m_direction = U_ARABIC_NUMBER;
                        appendRun();
                        if (context()->dir() != U_LEFT_TO_RIGHT) {
                            // the neutrals take the embedding direction, which is R
                            m_eor = m_last;
                            m_direction = U_RIGHT_TO_LEFT;
                            appendRun();
                        }
                    } else if (m_status.lastStrong != U_LEFT_TO_RIGHT) {
                        //last stuff takes embedding dir
                        if (context()->dir() == U_RIGHT_TO_LEFT) {
                            m_eor = m_last; 
                            m_direction = U_RIGHT_TO_LEFT;
                        }
                        appendRun();
                    }
                    break;
                default:
                    break;
            }
            m_eor = m_current;
            m_status.eor = U_LEFT_TO_RIGHT;
            m_status.lastStrong = U_LEFT_TO_RIGHT;
            m_direction = U_LEFT_TO_RIGHT;
            break;
        case U_RIGHT_TO_LEFT_ARABIC:
        case U_RIGHT_TO_LEFT:
            switch (m_status.last) {
                case U_LEFT_TO_RIGHT:
                case U_EUROPEAN_NUMBER:
                case U_ARABIC_NUMBER:
                    appendRun();
                    FALLTHROUGH;
                case U_RIGHT_TO_LEFT:
                case U_RIGHT_TO_LEFT_ARABIC:
                    break;
                case U_EUROPEAN_NUMBER_SEPARATOR:
                case U_EUROPEAN_NUMBER_TERMINATOR:
                case U_COMMON_NUMBER_SEPARATOR:
                case U_BOUNDARY_NEUTRAL:
                case U_BLOCK_SEPARATOR:
                case U_SEGMENT_SEPARATOR:
                case U_WHITE_SPACE_NEUTRAL:
                case U_OTHER_NEUTRAL:
                    if (m_status.eor == U_EUROPEAN_NUMBER) {
                        if (m_status.lastStrong == U_LEFT_TO_RIGHT && context()->dir() == U_LEFT_TO_RIGHT)
                            m_eor = m_last;
                        appendRun();
                    } else if (m_status.eor == U_ARABIC_NUMBER)
                        appendRun();
                    else if (m_status.lastStrong == U_LEFT_TO_RIGHT) {
                        if (context()->dir() == U_LEFT_TO_RIGHT)
                            m_eor = m_last;
                        appendRun();
                    }
                    break;
                default:
                    break;
            }
            m_eor = m_current;
            m_status.eor = U_RIGHT_TO_LEFT;
            m_status.lastStrong = dirCurrent;
            m_direction = U_RIGHT_TO_LEFT;
            break;

            // weak types:

        case U_EUROPEAN_NUMBER:
            if (m_status.lastStrong != U_RIGHT_TO_LEFT_ARABIC) {
                // if last strong was AL change EN to AN
                switch (m_status.last) {
                    case U_EUROPEAN_NUMBER:
                    case U_LEFT_TO_RIGHT:
                        break;
                    case U_RIGHT_TO_LEFT:
                    case U_RIGHT_TO_LEFT_ARABIC:
                    case U_ARABIC_NUMBER:
                        m_eor = m_last;
                        appendRun();
                        m_direction = U_EUROPEAN_NUMBER;
                        break;
                    case U_EUROPEAN_NUMBER_SEPARATOR:
                    case U_COMMON_NUMBER_SEPARATOR:
                        if (m_status.eor == U_EUROPEAN_NUMBER)
                            break;
                        FALLTHROUGH;
                    case U_EUROPEAN_NUMBER_TERMINATOR:
                    case U_BOUNDARY_NEUTRAL:
                    case U_BLOCK_SEPARATOR:
                    case U_SEGMENT_SEPARATOR:
                    case U_WHITE_SPACE_NEUTRAL:
                    case U_OTHER_NEUTRAL:
                        if (m_status.eor == U_EUROPEAN_NUMBER) {
                            if (m_status.lastStrong == U_RIGHT_TO_LEFT) {
                                // ENs on both sides behave like Rs, so the neutrals should be R.
                                // Terminate the EN run.
                                appendRun();
                                // Make an R run.
                                m_eor = m_status.last == U_EUROPEAN_NUMBER_TERMINATOR ? m_lastBeforeET : m_last;
                                m_direction = U_RIGHT_TO_LEFT;
                                appendRun();
                                // Begin a new EN run.
                                m_direction = U_EUROPEAN_NUMBER;
                            }
                        } else if (m_status.eor == U_ARABIC_NUMBER) {
                            // Terminate the AN run.
                            appendRun();
                            if (m_status.lastStrong == U_RIGHT_TO_LEFT || context()->dir() == U_RIGHT_TO_LEFT) {
                                // Make an R run.
                                m_eor = m_status.last == U_EUROPEAN_NUMBER_TERMINATOR ? m_lastBeforeET : m_last;
                                m_direction = U_RIGHT_TO_LEFT;
                                appendRun();
                                // Begin a new EN run.
                                m_direction = U_EUROPEAN_NUMBER;
                            }
                        } else if (m_status.lastStrong == U_RIGHT_TO_LEFT) {
                            // Extend the R run to include the neutrals.
                            m_eor = m_status.last == U_EUROPEAN_NUMBER_TERMINATOR ? m_lastBeforeET : m_last;
                            m_direction = U_RIGHT_TO_LEFT;
                            appendRun();
                            // Begin a new EN run.
                            m_direction = U_EUROPEAN_NUMBER;
                        }
                        break;
                    default:
                        break;
                }
                m_eor = m_current;
                m_status.eor = U_EUROPEAN_NUMBER;
                if (m_direction == U_OTHER_NEUTRAL)
                    m_direction = U_LEFT_TO_RIGHT;
                break;
            }
            FALLTHROUGH;
        case U_ARABIC_NUMBER:
            dirCurrent = U_ARABIC_NUMBER;
            switch (m_status.last) {
                case U_LEFT_TO_RIGHT:
                    if (context()->dir() == U_LEFT_TO_RIGHT)
                        appendRun();
                    break;
                case U_ARABIC_NUMBER:
                    break;
                case U_RIGHT_TO_LEFT:
                case U_RIGHT_TO_LEFT_ARABIC:
                case U_EUROPEAN_NUMBER:
                    m_eor = m_last;
                    appendRun();
                    break;
                case U_COMMON_NUMBER_SEPARATOR:
                    if (m_status.eor == U_ARABIC_NUMBER)
                        break;
                    FALLTHROUGH;
                case U_EUROPEAN_NUMBER_SEPARATOR:
                case U_EUROPEAN_NUMBER_TERMINATOR:
                case U_BOUNDARY_NEUTRAL:
                case U_BLOCK_SEPARATOR:
                case U_SEGMENT_SEPARATOR:
                case U_WHITE_SPACE_NEUTRAL:
                case U_OTHER_NEUTRAL:
                    if (m_status.eor == U_ARABIC_NUMBER
                        || (m_status.eor == U_EUROPEAN_NUMBER && (m_status.lastStrong == U_RIGHT_TO_LEFT || context()->dir() == U_RIGHT_TO_LEFT))
                        || (m_status.eor != U_EUROPEAN_NUMBER && m_status.lastStrong == U_LEFT_TO_RIGHT && context()->dir() == U_RIGHT_TO_LEFT)) {
                        // Terminate the run before the neutrals.
                        appendRun();
                        // Begin an R run for the neutrals.
                        m_direction = U_RIGHT_TO_LEFT;
                    } else if (m_direction == U_OTHER_NEUTRAL)
                        m_direction = m_status.lastStrong == U_LEFT_TO_RIGHT ? U_LEFT_TO_RIGHT : U_RIGHT_TO_LEFT;
                    m_eor = m_last;
                    appendRun();
                    break;
                default:
                    break;
            }
            m_eor = m_current;
            m_status.eor = U_ARABIC_NUMBER;
            if (m_direction == U_OTHER_NEUTRAL)
                m_direction = U_ARABIC_NUMBER;
            break;
        case U_EUROPEAN_NUMBER_SEPARATOR:
        case U_COMMON_NUMBER_SEPARATOR:
            break;
        case U_EUROPEAN_NUMBER_TERMINATOR:
            if (m_status.last == U_EUROPEAN_NUMBER) {
                dirCurrent = U_EUROPEAN_NUMBER;
                m_eor = m_current;
                m_status.eor = dirCurrent;
            } else if (m_status.last != U_EUROPEAN_NUMBER_TERMINATOR)
                m_lastBeforeET = m_emptyRun ? m_eor : m_last;
            break;

        // boundary neutrals should be ignored
        case U_BOUNDARY_NEUTRAL:
            if (m_eor == m_last)
                m_eor = m_current;
            break;
            // neutrals
        case U_BLOCK_SEPARATOR:
            // FIXME: What do we do with newline and paragraph separators that come to here?
            break;
        case U_SEGMENT_SEPARATOR:
            // FIXME: Implement rule L1.
            break;
        case U_WHITE_SPACE_NEUTRAL:
            break;
        case U_OTHER_NEUTRAL:
            break;
        default:
            break;
        }

        if (pastEnd && m_eor == m_current) {
            if (!m_reachedEndOfLine) {
                m_eor = endOfLine;
                switch (m_status.eor) {
                    case U_LEFT_TO_RIGHT:
                    case U_RIGHT_TO_LEFT:
                    case U_ARABIC_NUMBER:
                        m_direction = m_status.eor;
                        break;
                    case U_EUROPEAN_NUMBER:
                        m_direction = m_status.lastStrong == U_LEFT_TO_RIGHT ? U_LEFT_TO_RIGHT : U_EUROPEAN_NUMBER;
                        break;
                    default:
                        ASSERT_NOT_REACHED();
                }
                appendRun();
            }
            m_current = end;
            m_status = stateAtEnd.m_status;
            m_sor = stateAtEnd.m_sor; 
            m_eor = stateAtEnd.m_eor;
            m_last = stateAtEnd.m_last;
            m_reachedEndOfLine = stateAtEnd.m_reachedEndOfLine;
            m_lastBeforeET = stateAtEnd.m_lastBeforeET;
            m_emptyRun = stateAtEnd.m_emptyRun;
            m_direction = U_OTHER_NEUTRAL;
            break;
        }

        updateStatusLastFromCurrentDirection(dirCurrent);
        m_last = m_current;

        if (m_emptyRun) {
            m_sor = m_current;
            m_emptyRun = false;
        }

        increment();
        if (!m_currentExplicitEmbeddingSequence.isEmpty()) {
            bool committed = commitExplicitEmbedding();
            if (committed && pastEnd) {
                m_current = end;
                m_status = stateAtEnd.m_status;
                m_sor = stateAtEnd.m_sor; 
                m_eor = stateAtEnd.m_eor;
                m_last = stateAtEnd.m_last;
                m_reachedEndOfLine = stateAtEnd.m_reachedEndOfLine;
                m_lastBeforeET = stateAtEnd.m_lastBeforeET;
                m_emptyRun = stateAtEnd.m_emptyRun;
                m_direction = U_OTHER_NEUTRAL;
                break;
            }
        }

        if (!pastEnd && (m_current == end || m_current.atEnd())) {
            if (m_emptyRun)
                break;
            stateAtEnd.m_status = m_status;
            stateAtEnd.m_sor = m_sor;
            stateAtEnd.m_eor = m_eor;
            stateAtEnd.m_last = m_last;
            stateAtEnd.m_reachedEndOfLine = m_reachedEndOfLine;
            stateAtEnd.m_lastBeforeET = m_lastBeforeET;
            stateAtEnd.m_emptyRun = m_emptyRun;
            endOfLine = m_last;
            pastEnd = true;
        }
    }

    m_runs.setLogicallyLastRun(m_runs.lastRun());
    reorderRunsFromLevels();
    endOfLine = Iterator();
}

} // namespace WebCore

#endif // BidiResolver_h
