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
    
    template<class Decoder> static std::optional<PDFContextMenuItem> decode(Decoder& decoder)
    {
        std::optional<String> title;
        decoder >> title;
        if (!title)
            return std::nullopt;

        std::optional<bool> enabled;
        decoder >> enabled;
        if (!enabled)
            return std::nullopt;

        std::optional<bool> separator;
        decoder >> separator;
        if (!separator)
            return std::nullopt;

        std::optional<int> state;
        decoder >> state;
        if (!state)
            return std::nullopt;

        std::optional<bool> hasAction;
        decoder >> hasAction;
        if (!hasAction)
            return std::nullopt;

        std::optional<int> tag;
        decoder >> tag;
        if (!tag)
            return std::nullopt;

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
    
    template<class Decoder> static std::optional<PDFContextMenu> decode(Decoder& decoder)
    {
        std::optional<WebCore::IntPoint> point;
        decoder >> point;
        if (!point)
            return std::nullopt;

        std::optional<Vector<PDFContextMenuItem>> items;
        decoder >> items;
        if (!items)
            return std::nullopt;
        
        return { { WTFMove(*point), WTFMove(*items) } };
    }

};
    
};
#endif

