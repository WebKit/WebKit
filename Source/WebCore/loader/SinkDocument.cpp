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
#include "SinkDocument.h"

#include "Frame.h"
#include "RawDataDocumentParser.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SinkDocument);

class SinkDocumentParser final : public RawDataDocumentParser {
public:
    static Ref<SinkDocumentParser> create(SinkDocument& document)
    {
        return adoptRef(*new SinkDocumentParser(document));
    }
    
private:
    SinkDocumentParser(SinkDocument& document)
        : RawDataDocumentParser(document)
    {
    }

    // Ignore all data.
    void appendBytes(DocumentWriter&, const uint8_t*, size_t) override
    {
    }
};

SinkDocument::SinkDocument(Frame& frame, const URL& url)
    : HTMLDocument(&frame, frame.settings(), url)
{
    setCompatibilityMode(DocumentCompatibilityMode::QuirksMode);
    lockCompatibilityMode();
}

Ref<DocumentParser> SinkDocument::createParser()
{
    return SinkDocumentParser::create(*this);
}

} // namespace WebCore
