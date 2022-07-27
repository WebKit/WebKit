/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)

#include <WebCore/SharedBuffer.h>

namespace WebKit {

class CGDisplayList {
    WTF_MAKE_NONCOPYABLE(CGDisplayList);
public:
    CGDisplayList() = default;
    CGDisplayList(WebCore::SharedBuffer& displayList, Vector<MachSendRight>&& surfaces)
        : m_displayList(&displayList)
        , m_surfaces(WTFMove(surfaces))
    {
    }

    CGDisplayList(CGDisplayList&&) = default;
    CGDisplayList& operator=(CGDisplayList&&) = default;

    RefPtr<WebCore::SharedBuffer> buffer() const { return m_displayList; }
    Vector<MachSendRight> takeSurfaces() { return std::exchange(m_surfaces, { }); }

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, CGDisplayList&);

private:
    RefPtr<WebCore::SharedBuffer> m_displayList;
    Vector<MachSendRight> m_surfaces;
};

}

#endif // ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
