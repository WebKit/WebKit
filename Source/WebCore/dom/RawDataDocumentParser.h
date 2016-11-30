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

#pragma once

#include "DocumentParser.h"

namespace WebCore {

class RawDataDocumentParser : public DocumentParser {
protected:
    explicit RawDataDocumentParser(Document& document)
        : DocumentParser(document)
    {
    }

    void finish() override
    {
        if (!isStopped())
            document()->finishedParsing();
    }

private:
    void flush(DocumentWriter& writer) override
    {
        // Make sure appendBytes is called at least once.
        appendBytes(writer, 0, 0);
    }

    void insert(SegmentedString&&) override
    {
        // <https://bugs.webkit.org/show_bug.cgi?id=25397>: JS code can always call document.write, we need to handle it.
        ASSERT_NOT_REACHED();
    }

    void append(RefPtr<StringImpl>&&) override
    {
        ASSERT_NOT_REACHED();
    }
};

} // namespace WebCore
