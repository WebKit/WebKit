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
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

template <class Iterator> struct MidpointState {
    MidpointState()
    {
        reset();
    }
    
    void reset()
    {
        numMidpoints = 0;
        currentMidpoint = 0;
        betweenMidpoints = false;
    }
    
    // The goal is to reuse the line state across multiple
    // lines so we just keep an array around for midpoints and never clear it across multiple
    // lines.  We track the number of items and position using the two other variables.
    Vector<Iterator> midpoints;
    unsigned numMidpoints;
    unsigned currentMidpoint;
    bool betweenMidpoints;
};

// The BidiStatus at a given position (typically the end of a line) can
// be cached and then used to restart bidi resolution at that position.
struct BidiStatus {
    BidiStatus()
        : eor(WTF::Unicode::OtherNeutral)
        , lastStrong(WTF::Unicode::OtherNeutral)
        , last(WTF::Unicode::OtherNeutral)
    {
    }

    BidiStatus(WTF::Unicode::Direction eorDir, WTF::Unicode::Direction lastStrongDir, WTF::Unicode::Direction lastDir, PassRefPtr<BidiContext> bidiContext)
        : eor(eorDir)
        , lastStrong(lastStrongDir)
        , last(lastDir)
        , context(bidiContext)
    {
    }

    WTF::Unicode::Direction eor;
    WTF::Unicode::Direction lastStrong;
    WTF::Unicode::Direction last;
    RefPtr<BidiContext> context;
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
    BidiCharacterRun(int start, int stop, BidiContext* context, WTF::Unicode::Direction dir)
        : m_start(start)
        , m_stop(stop)
        , m_override(context->override())
        , m_next(0)
    {
        if (dir == WTF::Unicode::OtherNeutral)
            dir = context->dir();

        m_level = context->level();

        // add level of run (cases I1 & I2)
        if (m_level % 2) {
            if (dir == WTF::Unicode::LeftToRight || dir == WTF::Unicode::ArabicNumber || dir == WTF::Unicode::EuropeanNumber)
                m_level++;
        } else {
            if (dir == WTF::Unicode::RightToLeft)
                m_level++;
            else if (dir == WTF::Unicode::ArabicNumber || dir == WTF::Unicode::EuropeanNumber)
                m_level += 2;
        }
    }

    void destroy() { delete this; }

    int start() const { return m_start; }
    int stop() const { return m_stop; }
    unsigned char level() const { return m_level; }
    bool reversed(bool visuallyOrdered) { return m_level % 2 && !visuallyOrdered; }
    bool dirOverride(bool visuallyOrdered) { return m_override || visuallyOrdered; }

    BidiCharacterRun* next() const { return m_next; }

    unsigned char m_level;
    int m_start;
    int m_stop;
    bool m_override;
    BidiCharacterRun* m_next;
};

template <class Iterator, class Run> class BidiResolver : public Noncopyable {
public :
    BidiResolver()
        : m_direction(WTF::Unicode::OtherNeutral)
        , reachedEndOfLine(false)
        , emptyRun(true)
        , m_firstRun(0)
        , m_lastRun(0)
        , m_logicallyLastRun(0)
        , m_runCount(0)
    {
    }

    const Iterator& position() const { return current; }
    void setPosition(const Iterator& position) { current = position; }

    void increment() { current.increment(); }

    BidiContext* context() const { return m_status.context.get(); }
    void setContext(PassRefPtr<BidiContext> c) { m_status.context = c; }

    void setLastDir(WTF::Unicode::Direction lastDir) { m_status.last = lastDir; }
    void setLastStrongDir(WTF::Unicode::Direction lastStrongDir) { m_status.lastStrong = lastStrongDir; }
    void setEorDir(WTF::Unicode::Direction eorDir) { m_status.eor = eorDir; }

    WTF::Unicode::Direction dir() const { return m_direction; }
    void setDir(WTF::Unicode::Direction d) { m_direction = d; }

    const BidiStatus& status() const { return m_status; }
    void setStatus(const BidiStatus s) { m_status = s; }

    MidpointState<Iterator>& midpointState() { return m_midpointState; }

    void embed(WTF::Unicode::Direction);
    void commitExplicitEmbedding();

    void createBidiRunsForLine(const Iterator& end, bool visualOrder = false, bool hardLineBreak = false);

    Run* firstRun() const { return m_firstRun; }
    Run* lastRun() const { return m_lastRun; }
    Run* logicallyLastRun() const { return m_logicallyLastRun; }
    unsigned runCount() const { return m_runCount; }

    void addRun(Run*);
    void prependRun(Run*);

    void moveRunToEnd(Run*);
    void moveRunToBeginning(Run*);

    void deleteRuns();

protected:
    void appendRun();
    void reverseRuns(unsigned start, unsigned end);

    Iterator current;
    Iterator sor;
    Iterator eor;
    Iterator last;
    BidiStatus m_status;
    WTF::Unicode::Direction m_direction;
    Iterator endOfLine;
    bool reachedEndOfLine;
    Iterator lastBeforeET;
    bool emptyRun;

    Run* m_firstRun;
    Run* m_lastRun;
    Run* m_logicallyLastRun;
    unsigned m_runCount;
    MidpointState<Iterator> m_midpointState;

private:
    void raiseExplicitEmbeddingLevel(WTF::Unicode::Direction from, WTF::Unicode::Direction to);
    void lowerExplicitEmbeddingLevel(WTF::Unicode::Direction from);

    Vector<WTF::Unicode::Direction, 8> m_currentExplicitEmbeddingSequence;
};

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::addRun(Run* run)
{
    if (!m_firstRun)
        m_firstRun = run;
    else
        m_lastRun->m_next = run;
    m_lastRun = run;
    m_runCount++;
}

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::prependRun(Run* run)
{
    ASSERT(!run->m_next);

    if (!m_lastRun)
        m_lastRun = run;
    else
        run->m_next = m_firstRun;
    m_firstRun = run;
    m_runCount++;
}

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::moveRunToEnd(Run* run)
{
    ASSERT(m_firstRun);
    ASSERT(m_lastRun);
    ASSERT(run->m_next);

    Run* current = 0;
    Run* next = m_firstRun;
    while (next != run) {
        current = next;
        next = current->next();
    }

    if (!current)
        m_firstRun = run->next();
    else
        current->m_next = run->m_next;

    run->m_next = 0;
    m_lastRun->m_next = run;
    m_lastRun = run;
}

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::moveRunToBeginning(Run* run)
{
    ASSERT(m_firstRun);
    ASSERT(m_lastRun);
    ASSERT(run != m_firstRun);

    Run* current = m_firstRun;
    Run* next = current->next();
    while (next != run) {
        current = next;
        next = current->next();
    }

    current->m_next = run->m_next;
    if (run == m_lastRun)
        m_lastRun = current;

    run->m_next = m_firstRun;
    m_firstRun = run;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::appendRun()
{
    if (!emptyRun && !eor.atEnd()) {
        unsigned startOffset = sor.offset();
        unsigned endOffset = eor.offset();

        if (!endOfLine.atEnd() && endOffset >= endOfLine.offset()) {
            reachedEndOfLine = true;
            endOffset = endOfLine.offset();
        }

        if (endOffset >= startOffset)
            addRun(new Run(startOffset, endOffset + 1, context(), m_direction));

        eor.increment();
        sor = eor;
    }

    m_direction = WTF::Unicode::OtherNeutral;
    m_status.eor = WTF::Unicode::OtherNeutral;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::embed(WTF::Unicode::Direction d)
{
    using namespace WTF::Unicode;

    ASSERT(d == PopDirectionalFormat || d == LeftToRightEmbedding || d == LeftToRightOverride || d == RightToLeftEmbedding || d == RightToLeftOverride);
    m_currentExplicitEmbeddingSequence.append(d);
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::lowerExplicitEmbeddingLevel(WTF::Unicode::Direction from)
{
    using namespace WTF::Unicode;

    if (!emptyRun && eor != last) {
        ASSERT(m_status.eor != OtherNeutral || eor.atEnd());
        // bidi.sor ... bidi.eor ... bidi.last eor; need to append the bidi.sor-bidi.eor run or extend it through bidi.last
        ASSERT(m_status.last == EuropeanNumberSeparator
            || m_status.last == EuropeanNumberTerminator
            || m_status.last == CommonNumberSeparator
            || m_status.last == BoundaryNeutral
            || m_status.last == BlockSeparator
            || m_status.last == SegmentSeparator
            || m_status.last == WhiteSpaceNeutral
            || m_status.last == OtherNeutral);
        if (m_direction == OtherNeutral)
            m_direction = m_status.lastStrong == LeftToRight ? LeftToRight : RightToLeft;
        if (from == LeftToRight) {
            // bidi.sor ... bidi.eor ... bidi.last L
            if (m_status.eor == EuropeanNumber) {
                if (m_status.lastStrong != LeftToRight) {
                    m_direction = EuropeanNumber;
                    appendRun();
                }
            } else if (m_status.eor == ArabicNumber) {
                m_direction = ArabicNumber;
                appendRun();
            } else if (m_status.lastStrong != LeftToRight) {
                appendRun();
                m_direction = LeftToRight;
            }
        } else if (m_status.eor == EuropeanNumber || m_status.eor == ArabicNumber || m_status.lastStrong == LeftToRight) {
            appendRun();
            m_direction = RightToLeft;
        }
        eor = last;
    }
    appendRun();
    emptyRun = true;
    // sor for the new run is determined by the higher level (rule X10)
    setLastDir(from);
    setLastStrongDir(from);
    eor = Iterator();
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::raiseExplicitEmbeddingLevel(WTF::Unicode::Direction from, WTF::Unicode::Direction to)
{
    using namespace WTF::Unicode;

    if (!emptyRun && eor != last) {
        ASSERT(m_status.eor != OtherNeutral || eor.atEnd());
        // bidi.sor ... bidi.eor ... bidi.last eor; need to append the bidi.sor-bidi.eor run or extend it through bidi.last
        ASSERT(m_status.last == EuropeanNumberSeparator
            || m_status.last == EuropeanNumberTerminator
            || m_status.last == CommonNumberSeparator
            || m_status.last == BoundaryNeutral
            || m_status.last == BlockSeparator
            || m_status.last == SegmentSeparator
            || m_status.last == WhiteSpaceNeutral
            || m_status.last == OtherNeutral);
        if (m_direction == OtherNeutral)
            m_direction = m_status.lastStrong == LeftToRight ? LeftToRight : RightToLeft;
        if (to == LeftToRight) {
            // bidi.sor ... bidi.eor ... bidi.last L
            if (m_status.eor == EuropeanNumber) {
                if (m_status.lastStrong != LeftToRight) {
                    m_direction = EuropeanNumber;
                    appendRun();
                }
            } else if (m_status.eor == ArabicNumber) {
                m_direction = ArabicNumber;
                appendRun();
            } else if (m_status.lastStrong != LeftToRight && from == LeftToRight) {
                appendRun();
                m_direction = LeftToRight;
            }
        } else if (m_status.eor == ArabicNumber
            || (m_status.eor == EuropeanNumber && (m_status.lastStrong != LeftToRight || from == RightToLeft))
            || (m_status.eor != EuropeanNumber && m_status.lastStrong == LeftToRight && from == RightToLeft)) {
            appendRun();
            m_direction = RightToLeft;
        }
        eor = last;
    }
    appendRun();
    emptyRun = true;
    setLastDir(to);
    setLastStrongDir(to);
    eor = Iterator();
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::commitExplicitEmbedding()
{
    using namespace WTF::Unicode;

    unsigned char fromLevel = context()->level();
    RefPtr<BidiContext> toContext = context();

    for (size_t i = 0; i < m_currentExplicitEmbeddingSequence.size(); ++i) {
        Direction embedding = m_currentExplicitEmbeddingSequence[i];
        if (embedding == PopDirectionalFormat) {
            if (BidiContext* parentContext = toContext->parent())
                toContext = parentContext;
        } else {
            Direction direction = (embedding == RightToLeftEmbedding || embedding == RightToLeftOverride) ? RightToLeft : LeftToRight;
            bool override = embedding == LeftToRightOverride || embedding == RightToLeftOverride;
            unsigned char level = toContext->level();
            if (direction == RightToLeft) {
                // Go to the least greater odd integer
                level += 1;
                level |= 1;
            } else {
                // Go to the least greater even integer
                level += 2;
                level &= ~1;
            }
            if (level < 61)
                toContext = BidiContext::create(level, direction, override, toContext.get());
        }
    }

    unsigned char toLevel = toContext->level();

    if (toLevel > fromLevel)
        raiseExplicitEmbeddingLevel(fromLevel % 2 ? RightToLeft : LeftToRight, toLevel % 2 ? RightToLeft : LeftToRight);
    else if (toLevel < fromLevel)
        lowerExplicitEmbeddingLevel(fromLevel % 2 ? RightToLeft : LeftToRight);

    setContext(toContext);

    m_currentExplicitEmbeddingSequence.clear();
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::deleteRuns()
{
    emptyRun = true;
    if (!m_firstRun)
        return;

    Run* curr = m_firstRun;
    while (curr) {
        Run* s = curr->next();
        curr->destroy();
        curr = s;
    }

    m_firstRun = 0;
    m_lastRun = 0;
    m_runCount = 0;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::reverseRuns(unsigned start, unsigned end)
{
    if (start >= end)
        return;

    ASSERT(end < m_runCount);
    
    // Get the item before the start of the runs to reverse and put it in
    // |beforeStart|.  |curr| should point to the first run to reverse.
    Run* curr = m_firstRun;
    Run* beforeStart = 0;
    unsigned i = 0;
    while (i < start) {
        i++;
        beforeStart = curr;
        curr = curr->next();
    }

    Run* startRun = curr;
    while (i < end) {
        i++;
        curr = curr->next();
    }
    Run* endRun = curr;
    Run* afterEnd = curr->next();

    i = start;
    curr = startRun;
    Run* newNext = afterEnd;
    while (i <= end) {
        // Do the reversal.
        Run* next = curr->next();
        curr->m_next = newNext;
        newNext = curr;
        curr = next;
        i++;
    }

    // Now hook up beforeStart and afterEnd to the startRun and endRun.
    if (beforeStart)
        beforeStart->m_next = endRun;
    else
        m_firstRun = endRun;

    startRun->m_next = afterEnd;
    if (!afterEnd)
        m_lastRun = startRun;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::createBidiRunsForLine(const Iterator& end, bool visualOrder, bool hardLineBreak)
{
    using namespace WTF::Unicode;

    ASSERT(m_direction == OtherNeutral);

    emptyRun = true;

    eor = Iterator();

    last = current;
    bool pastEnd = false;
    BidiResolver<Iterator, Run> stateAtEnd;

    while (true) {
        Direction dirCurrent;
        if (pastEnd && (hardLineBreak || current.atEnd())) {
            BidiContext* c = context();
            while (c->parent())
                c = c->parent();
            dirCurrent = c->dir();
            if (hardLineBreak) {
                // A deviation from the Unicode Bidi Algorithm in order to match
                // Mac OS X text and WinIE: a hard line break resets bidi state.
                stateAtEnd.setContext(c);
                stateAtEnd.setEorDir(dirCurrent);
                stateAtEnd.setLastDir(dirCurrent);
                stateAtEnd.setLastStrongDir(dirCurrent);
            }
        } else {
            dirCurrent = current.direction();
            if (context()->override()
                    && dirCurrent != RightToLeftEmbedding
                    && dirCurrent != LeftToRightEmbedding
                    && dirCurrent != RightToLeftOverride
                    && dirCurrent != LeftToRightOverride
                    && dirCurrent != PopDirectionalFormat)
                dirCurrent = context()->dir();
            else if (dirCurrent == NonSpacingMark)
                dirCurrent = m_status.last;
        }

        ASSERT(m_status.eor != OtherNeutral || eor.atEnd());
        switch (dirCurrent) {

        // embedding and overrides (X1-X9 in the Bidi specs)
        case RightToLeftEmbedding:
        case LeftToRightEmbedding:
        case RightToLeftOverride:
        case LeftToRightOverride:
        case PopDirectionalFormat:
            embed(dirCurrent);
            commitExplicitEmbedding();
            break;

            // strong types
        case LeftToRight:
            switch(m_status.last) {
                case RightToLeft:
                case RightToLeftArabic:
                case EuropeanNumber:
                case ArabicNumber:
                    if (m_status.last != EuropeanNumber || m_status.lastStrong != LeftToRight)
                        appendRun();
                    break;
                case LeftToRight:
                    break;
                case EuropeanNumberSeparator:
                case EuropeanNumberTerminator:
                case CommonNumberSeparator:
                case BoundaryNeutral:
                case BlockSeparator:
                case SegmentSeparator:
                case WhiteSpaceNeutral:
                case OtherNeutral:
                    if (m_status.eor == EuropeanNumber) {
                        if (m_status.lastStrong != LeftToRight) {
                            // the numbers need to be on a higher embedding level, so let's close that run
                            m_direction = EuropeanNumber;
                            appendRun();
                            if (context()->dir() != LeftToRight) {
                                // the neutrals take the embedding direction, which is R
                                eor = last;
                                m_direction = RightToLeft;
                                appendRun();
                            }
                        }
                    } else if (m_status.eor == ArabicNumber) {
                        // Arabic numbers are always on a higher embedding level, so let's close that run
                        m_direction = ArabicNumber;
                        appendRun();
                        if (context()->dir() != LeftToRight) {
                            // the neutrals take the embedding direction, which is R
                            eor = last;
                            m_direction = RightToLeft;
                            appendRun();
                        }
                    } else if (m_status.lastStrong != LeftToRight) {
                        //last stuff takes embedding dir
                        if (context()->dir() == RightToLeft) {
                            eor = last; 
                            m_direction = RightToLeft;
                        }
                        appendRun();
                    }
                default:
                    break;
            }
            eor = current;
            m_status.eor = LeftToRight;
            m_status.lastStrong = LeftToRight;
            m_direction = LeftToRight;
            break;
        case RightToLeftArabic:
        case RightToLeft:
            switch (m_status.last) {
                case LeftToRight:
                case EuropeanNumber:
                case ArabicNumber:
                    appendRun();
                case RightToLeft:
                case RightToLeftArabic:
                    break;
                case EuropeanNumberSeparator:
                case EuropeanNumberTerminator:
                case CommonNumberSeparator:
                case BoundaryNeutral:
                case BlockSeparator:
                case SegmentSeparator:
                case WhiteSpaceNeutral:
                case OtherNeutral:
                    if (m_status.eor == EuropeanNumber) {
                        if (m_status.lastStrong == LeftToRight && context()->dir() == LeftToRight)
                            eor = last;
                        appendRun();
                    } else if (m_status.eor == ArabicNumber)
                        appendRun();
                    else if (m_status.lastStrong == LeftToRight) {
                        if (context()->dir() == LeftToRight)
                            eor = last;
                        appendRun();
                    }
                default:
                    break;
            }
            eor = current;
            m_status.eor = RightToLeft;
            m_status.lastStrong = dirCurrent;
            m_direction = RightToLeft;
            break;

            // weak types:

        case EuropeanNumber:
            if (m_status.lastStrong != RightToLeftArabic) {
                // if last strong was AL change EN to AN
                switch (m_status.last) {
                    case EuropeanNumber:
                    case LeftToRight:
                        break;
                    case RightToLeft:
                    case RightToLeftArabic:
                    case ArabicNumber:
                        eor = last;
                        appendRun();
                        m_direction = EuropeanNumber;
                        break;
                    case EuropeanNumberSeparator:
                    case CommonNumberSeparator:
                        if (m_status.eor == EuropeanNumber)
                            break;
                    case EuropeanNumberTerminator:
                    case BoundaryNeutral:
                    case BlockSeparator:
                    case SegmentSeparator:
                    case WhiteSpaceNeutral:
                    case OtherNeutral:
                        if (m_status.eor == EuropeanNumber) {
                            if (m_status.lastStrong == RightToLeft) {
                                // ENs on both sides behave like Rs, so the neutrals should be R.
                                // Terminate the EN run.
                                appendRun();
                                // Make an R run.
                                eor = m_status.last == EuropeanNumberTerminator ? lastBeforeET : last;
                                m_direction = RightToLeft;
                                appendRun();
                                // Begin a new EN run.
                                m_direction = EuropeanNumber;
                            }
                        } else if (m_status.eor == ArabicNumber) {
                            // Terminate the AN run.
                            appendRun();
                            if (m_status.lastStrong == RightToLeft || context()->dir() == RightToLeft) {
                                // Make an R run.
                                eor = m_status.last == EuropeanNumberTerminator ? lastBeforeET : last;
                                m_direction = RightToLeft;
                                appendRun();
                                // Begin a new EN run.
                                m_direction = EuropeanNumber;
                            }
                        } else if (m_status.lastStrong == RightToLeft) {
                            // Extend the R run to include the neutrals.
                            eor = m_status.last == EuropeanNumberTerminator ? lastBeforeET : last;
                            m_direction = RightToLeft;
                            appendRun();
                            // Begin a new EN run.
                            m_direction = EuropeanNumber;
                        }
                    default:
                        break;
                }
                eor = current;
                m_status.eor = EuropeanNumber;
                if (m_direction == OtherNeutral)
                    m_direction = LeftToRight;
                break;
            }
        case ArabicNumber:
            dirCurrent = ArabicNumber;
            switch (m_status.last) {
                case LeftToRight:
                    if (context()->dir() == LeftToRight)
                        appendRun();
                    break;
                case ArabicNumber:
                    break;
                case RightToLeft:
                case RightToLeftArabic:
                case EuropeanNumber:
                    eor = last;
                    appendRun();
                    break;
                case CommonNumberSeparator:
                    if (m_status.eor == ArabicNumber)
                        break;
                case EuropeanNumberSeparator:
                case EuropeanNumberTerminator:
                case BoundaryNeutral:
                case BlockSeparator:
                case SegmentSeparator:
                case WhiteSpaceNeutral:
                case OtherNeutral:
                    if (m_status.eor == ArabicNumber
                        || (m_status.eor == EuropeanNumber && (m_status.lastStrong == RightToLeft || context()->dir() == RightToLeft))
                        || (m_status.eor != EuropeanNumber && m_status.lastStrong == LeftToRight && context()->dir() == RightToLeft)) {
                        // Terminate the run before the neutrals.
                        appendRun();
                        // Begin an R run for the neutrals.
                        m_direction = RightToLeft;
                    } else if (m_direction == OtherNeutral)
                        m_direction = m_status.lastStrong == LeftToRight ? LeftToRight : RightToLeft;
                    eor = last;
                    appendRun();
                default:
                    break;
            }
            eor = current;
            m_status.eor = ArabicNumber;
            if (m_direction == OtherNeutral)
                m_direction = ArabicNumber;
            break;
        case EuropeanNumberSeparator:
        case CommonNumberSeparator:
            break;
        case EuropeanNumberTerminator:
            if (m_status.last == EuropeanNumber) {
                dirCurrent = EuropeanNumber;
                eor = current;
                m_status.eor = dirCurrent;
            } else if (m_status.last != EuropeanNumberTerminator)
                lastBeforeET = emptyRun ? eor : last;
            break;

        // boundary neutrals should be ignored
        case BoundaryNeutral:
            if (eor == last)
                eor = current;
            break;
            // neutrals
        case BlockSeparator:
            // ### what do we do with newline and paragraph seperators that come to here?
            break;
        case SegmentSeparator:
            // ### implement rule L1
            break;
        case WhiteSpaceNeutral:
            break;
        case OtherNeutral:
            break;
        default:
            break;
        }

        if (pastEnd) {
            if (eor == current) {
                if (!reachedEndOfLine) {
                    eor = endOfLine;
                    switch (m_status.eor) {
                        case LeftToRight:
                        case RightToLeft:
                        case ArabicNumber:
                            m_direction = m_status.eor;
                            break;
                        case EuropeanNumber:
                            m_direction = m_status.lastStrong == LeftToRight ? LeftToRight : EuropeanNumber;
                            break;
                        default:
                            ASSERT(false);
                    }
                    appendRun();
                }
                current = end;
                m_status = stateAtEnd.m_status;
                sor = stateAtEnd.sor; 
                eor = stateAtEnd.eor;
                last = stateAtEnd.last;
                reachedEndOfLine = stateAtEnd.reachedEndOfLine;
                lastBeforeET = stateAtEnd.lastBeforeET;
                emptyRun = stateAtEnd.emptyRun;
                m_direction = OtherNeutral;
                break;
            }
        }

        // set m_status.last as needed.
        switch (dirCurrent) {
            case EuropeanNumberTerminator:
                if (m_status.last != EuropeanNumber)
                    m_status.last = EuropeanNumberTerminator;
                break;
            case EuropeanNumberSeparator:
            case CommonNumberSeparator:
            case SegmentSeparator:
            case WhiteSpaceNeutral:
            case OtherNeutral:
                switch(m_status.last) {
                    case LeftToRight:
                    case RightToLeft:
                    case RightToLeftArabic:
                    case EuropeanNumber:
                    case ArabicNumber:
                        m_status.last = dirCurrent;
                        break;
                    default:
                        m_status.last = OtherNeutral;
                    }
                break;
            case NonSpacingMark:
            case BoundaryNeutral:
            case RightToLeftEmbedding:
            case LeftToRightEmbedding:
            case RightToLeftOverride:
            case LeftToRightOverride:
            case PopDirectionalFormat:
                // ignore these
                break;
            case EuropeanNumber:
                // fall through
            default:
                m_status.last = dirCurrent;
        }

        last = current;

        if (emptyRun && !(dirCurrent == RightToLeftEmbedding
                || dirCurrent == LeftToRightEmbedding
                || dirCurrent == RightToLeftOverride
                || dirCurrent == LeftToRightOverride
                || dirCurrent == PopDirectionalFormat)) {
            sor = current;
            emptyRun = false;
        }

        increment();
        if (!m_currentExplicitEmbeddingSequence.isEmpty())
            commitExplicitEmbedding();

        if (emptyRun && (dirCurrent == RightToLeftEmbedding
                || dirCurrent == LeftToRightEmbedding
                || dirCurrent == RightToLeftOverride
                || dirCurrent == LeftToRightOverride
                || dirCurrent == PopDirectionalFormat)) {
            // exclude the embedding char itself from the new run so that ATSUI will never see it
            eor = Iterator();
            last = current;
            sor = current;
        }

        if (!pastEnd && (current == end || current.atEnd())) {
            if (emptyRun)
                break;
            stateAtEnd.m_status = m_status;
            stateAtEnd.sor = sor; 
            stateAtEnd.eor = eor;
            stateAtEnd.last = last;
            stateAtEnd.reachedEndOfLine = reachedEndOfLine;
            stateAtEnd.lastBeforeET = lastBeforeET;
            stateAtEnd.emptyRun = emptyRun;
            endOfLine = last;
            pastEnd = true;
        }
    }

    m_logicallyLastRun = m_lastRun;

    // reorder line according to run structure...
    // do not reverse for visually ordered web sites
    if (!visualOrder) {

        // first find highest and lowest levels
        unsigned char levelLow = 128;
        unsigned char levelHigh = 0;
        Run* r = firstRun();
        while (r) {
            if (r->m_level > levelHigh)
                levelHigh = r->m_level;
            if (r->m_level < levelLow)
                levelLow = r->m_level;
            r = r->next();
        }

        // implements reordering of the line (L2 according to Bidi spec):
        // L2. From the highest level found in the text to the lowest odd level on each line,
        // reverse any contiguous sequence of characters that are at that level or higher.

        // reversing is only done up to the lowest odd level
        if (!(levelLow % 2))
            levelLow++;

        unsigned count = runCount() - 1;

        while (levelHigh >= levelLow) {
            unsigned i = 0;
            Run* currRun = firstRun();
            while (i < count) {
                while (i < count && currRun && currRun->m_level < levelHigh) {
                    i++;
                    currRun = currRun->next();
                }
                unsigned start = i;
                while (i <= count && currRun && currRun->m_level >= levelHigh) {
                    i++;
                    currRun = currRun->next();
                }
                unsigned end = i - 1;
                reverseRuns(start, end);
            }
            levelHigh--;
        }
    }
    endOfLine = Iterator();
}

} // namespace WebCore

#endif // BidiResolver_h
