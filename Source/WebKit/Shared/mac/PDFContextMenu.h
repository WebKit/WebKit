/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(PDFKIT_PLUGIN)
namespace WebKit {
    
struct PDFContextMenuItem {
    String title;
    bool enabled;
    bool separator;
    int state;
    bool hasAction;
    int tag;

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << title << enabled << separator << state << hasAction << tag;
    }
    
    template<class Decoder> static Optional<PDFContextMenuItem> decode(Decoder& decoder)
    {
        Optional<String> title;
        decoder >> title;
        if (!title)
            return WTF::nullopt;

        Optional<bool> enabled;
        decoder >> enabled;
        if (!enabled)
            return WTF::nullopt;

        Optional<bool> separator;
        decoder >> separator;
        if (!separator)
            return WTF::nullopt;

        Optional<int> state;
        decoder >> state;
        if (!state)
            return WTF::nullopt;

        Optional<bool> hasAction;
        decoder >> hasAction;
        if (!hasAction)
            return WTF::nullopt;

        Optional<int> tag;
        decoder >> tag;
        if (!tag)
            return WTF::nullopt;

        return { { WTFMove(*title), WTFMove(*enabled), WTFMove(*separator), WTFMove(*state), WTFMove(*hasAction), WTFMove(*tag) } };
    }
};

struct PDFContextMenu {
    WebCore::IntPoint m_point;
    Vector<PDFContextMenuItem> m_items;
    
    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << m_point << m_items;
    }
    
    template<class Decoder> static Optional<PDFContextMenu> decode(Decoder& decoder)
    {
        Optional<WebCore::IntPoint> point;
        decoder >> point;
        if (!point)
            return WTF::nullopt;

        Optional<Vector<PDFContextMenuItem>> items;
        decoder >> items;
        if (!items)
            return WTF::nullopt;
        
        return { { WTFMove(*point), WTFMove(*items) } };
    }

};
    
};
#endif

