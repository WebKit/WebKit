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

#import "config.h"
#import "ClipboardImageReader.h"

#if PLATFORM(MAC)

#import "Document.h"
#import "SharedBuffer.h"

namespace WebCore {

void ClipboardImageReader::readBuffer(const String&, const String&, Ref<SharedBuffer>&& buffer)
{
    if (m_mimeType == "image/png") {
        auto image = adoptNS([[NSImage alloc] initWithData:buffer->createNSData().get()]);
        if (auto cgImage = [image CGImageForProposedRect:nil context:nil hints:nil]) {
            auto representation = adoptNS([[NSBitmapImageRep alloc] initWithCGImage:cgImage]);
            m_result = Blob::create(m_document.get(), SharedBuffer::create([representation representationUsingType:NSBitmapImageFileTypePNG properties:@{ }]), m_mimeType);
        }
    }
}

} // namespace WebCore

#endif // PLATFORM(MAC)
