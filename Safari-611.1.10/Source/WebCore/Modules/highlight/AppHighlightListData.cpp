/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "AppHighlightListData.h"

#include "Document.h"
#include "DocumentMarkerController.h"
#include "HTMLBodyElement.h"
#include "Node.h"
#include "RenderedDocumentMarker.h"
#include "SharedBuffer.h"
#include "SimpleRange.h"
#include "StaticRange.h"
#include "TextIterator.h"
#include <wtf/persistence/PersistentCoders.h>

namespace WebCore {

#if ENABLE(APP_HIGHLIGHTS)

AppHighlightListData AppHighlightListData::create(const SharedBuffer& buffer)
{
    auto decoder = buffer.decoder();
    Optional<AppHighlightListData> data;
    decoder >> data;
    return data ? *data : AppHighlightListData {{ }};
}

Ref<SharedBuffer> AppHighlightListData::toData() const
{
    WTF::Persistence::Encoder encoder;
    encoder << *this;
    return SharedBuffer::create(encoder.buffer(), encoder.bufferSize());
}

#endif

} // namespace WebCore
