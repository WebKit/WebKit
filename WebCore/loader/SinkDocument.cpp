/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "SinkDocument.h"

#include "DocumentParser.h"

namespace WebCore {

class SinkDocumentParser : public DocumentParser {
public:
    SinkDocumentParser(Document* document)
        : DocumentParser(document)
    {
    }

private:
    virtual void write(const SegmentedString&, bool) { ASSERT_NOT_REACHED(); }
    virtual void finish();
    virtual bool finishWasCalled();
    virtual bool isWaitingForScripts() const { return false; }
        
    virtual bool wantsRawData() const { return true; }
    virtual bool writeRawData(const char*, int) { return false; }
};

void SinkDocumentParser::finish()
{
    if (!m_parserStopped) 
        m_document->finishedParsing();    
}

bool SinkDocumentParser::finishWasCalled()
{
    // finish() always calls m_doc->finishedParsing() so we'll be deleted
    // after finish().
    return false;
}

SinkDocument::SinkDocument(Frame* frame, const KURL& url)
    : HTMLDocument(frame, url)
{
    setParseMode(Compat);
}
    
DocumentParser* SinkDocument::createParser()
{
    return new SinkDocumentParser(this);
}

} // namespace WebCore
