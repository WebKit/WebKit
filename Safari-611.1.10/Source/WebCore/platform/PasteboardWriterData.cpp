/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PasteboardWriterData.h"

#include "SharedBuffer.h"

namespace WebCore {

PasteboardWriterData::PasteboardWriterData() = default;
PasteboardWriterData::~PasteboardWriterData() = default;

PasteboardWriterData::WebContent::WebContent() = default;
PasteboardWriterData::WebContent::~WebContent() = default;

bool PasteboardWriterData::isEmpty() const
{
    return !m_plainText && !m_url && !m_webContent;
}

void PasteboardWriterData::setPlainText(PlainText plainText)
{
    ASSERT(!m_plainText);

    m_plainText = WTFMove(plainText);
}

void PasteboardWriterData::setURLData(URLData url)
{
    ASSERT(!m_url);

    m_url = WTFMove(url);
}

void PasteboardWriterData::setWebContent(WebContent webContent)
{
    ASSERT(!m_webContent);

    m_webContent = webContent;
}

}
