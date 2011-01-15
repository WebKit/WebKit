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

#ifndef UpdateChunk_h
#define UpdateChunk_h

#include <WebCore/IntRect.h>
#include <wtf/RetainPtr.h>

namespace CoreIPC {
    class ArgumentEncoder;
    class ArgumentDecoder;
}

namespace WebKit {

class UpdateChunk {
public:
    UpdateChunk();
    UpdateChunk(const WebCore::IntRect&);
    ~UpdateChunk();

    uint8_t* data() { return m_data; }
    const WebCore::IntRect& rect() const { return m_rect; }
    bool isEmpty() const { return m_rect.isEmpty(); }

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, UpdateChunk&);

    RetainPtr<CGImageRef> createImage();

private:
    size_t size() const { return m_rect.width() * 4 * m_rect.height(); }

    WebCore::IntRect m_rect;

    // This needs to be mutable, because encoding an update chunk will hand over its memory to the target process.
    mutable uint8_t* m_data;
    size_t m_size;
};

} // namespace WebKit

#endif // UpdateChunk_h
