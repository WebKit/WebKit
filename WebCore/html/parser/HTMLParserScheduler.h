/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#ifndef HTMLParserScheduler_h
#define HTMLParserScheduler_h

#include "Timer.h"
#include <wtf/CurrentTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class HTMLDocumentParser;

class HTMLParserScheduler :  public Noncopyable {
public:
    static PassOwnPtr<HTMLParserScheduler> create(HTMLDocumentParser* parser)
    {
        return adoptPtr(new HTMLParserScheduler(parser));
    }
    ~HTMLParserScheduler();

    struct PumpSession {
        PumpSession()
            : processedTokens(0)
            , startTime(currentTime())
        {
        }

        int processedTokens;
        double startTime;
    };

    // Inline as this is called after every token in the parser.
    bool shouldContinueParsing(PumpSession& session)
    {
        if (session.processedTokens > m_parserChunkSize) {
            session.processedTokens = 0;
            double elapsedTime = currentTime() - session.startTime;
            if (elapsedTime > m_parserTimeLimit) {
                // Schedule the parser to continue and yield from the parser.
                m_continueNextChunkTimer.startOneShot(0);
                return false;
            }
        }

        ++session.processedTokens;
        return true;
    }

    bool isScheduledForResume() const { return m_continueNextChunkTimer.isActive(); }

private:
    HTMLParserScheduler(HTMLDocumentParser*);

    void continueNextChunkTimerFired(Timer<HTMLParserScheduler>*);

    HTMLDocumentParser* m_parser;

    double m_parserTimeLimit;
    int m_parserChunkSize;
    Timer<HTMLParserScheduler> m_continueNextChunkTimer;
};

}

#endif
