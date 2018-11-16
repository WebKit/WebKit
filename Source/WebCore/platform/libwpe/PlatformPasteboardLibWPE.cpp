/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "PlatformPasteboard.h"

#if USE(LIBWPE)

#include "Pasteboard.h"
#include <wpe/wpe.h>
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

PlatformPasteboard::PlatformPasteboard(const String&)
    : m_pasteboard(wpe_pasteboard_get_singleton())
{
    ASSERT(m_pasteboard);
}

PlatformPasteboard::PlatformPasteboard()
    : m_pasteboard(wpe_pasteboard_get_singleton())
{
    ASSERT(m_pasteboard);
}

void PlatformPasteboard::getTypes(Vector<String>& types)
{
    struct wpe_pasteboard_string_vector pasteboardTypes = { nullptr, 0 };
    wpe_pasteboard_get_types(m_pasteboard, &pasteboardTypes);

    for (unsigned i = 0; i < pasteboardTypes.length; ++i) {
        auto& typeString = pasteboardTypes.strings[i];
        types.append(String(typeString.data, typeString.length));
    }

    wpe_pasteboard_string_vector_free(&pasteboardTypes);
}

String PlatformPasteboard::readString(int, const String& type) const
{
    struct wpe_pasteboard_string string = { nullptr, 0 };
    wpe_pasteboard_get_string(m_pasteboard, type.utf8().data(), &string);
    if (!string.length)
        return String();

    String returnValue(string.data, string.length);

    wpe_pasteboard_string_free(&string);
    return returnValue;
}

void PlatformPasteboard::write(const PasteboardWebContent& content)
{
    static const char plainText[] = "text/plain;charset=utf-8";
    static const char htmlText[] = "text/html;charset=utf-8";

    CString textString = content.text.utf8();
    CString markupString = content.markup.utf8();

    struct wpe_pasteboard_string_pair pairs[] = {
        { { nullptr, 0 }, { nullptr, 0 } },
        { { nullptr, 0 }, { nullptr, 0 } },
    };
    wpe_pasteboard_string_initialize(&pairs[0].type, plainText, strlen(plainText));
    wpe_pasteboard_string_initialize(&pairs[0].string, textString.data(), textString.length());
    wpe_pasteboard_string_initialize(&pairs[1].type, htmlText, strlen(htmlText));
    wpe_pasteboard_string_initialize(&pairs[1].string, markupString.data(), markupString.length());
    struct wpe_pasteboard_string_map map = { pairs, 2 };

    wpe_pasteboard_write(m_pasteboard, &map);

    wpe_pasteboard_string_free(&pairs[0].type);
    wpe_pasteboard_string_free(&pairs[0].string);
    wpe_pasteboard_string_free(&pairs[1].type);
    wpe_pasteboard_string_free(&pairs[1].string);
}

void PlatformPasteboard::write(const String& type, const String& string)
{
    struct wpe_pasteboard_string_pair pairs[] = {
        { { nullptr, 0 }, { nullptr, 0 } },
    };

    auto typeUTF8 = type.utf8();
    auto stringUTF8 = string.utf8();
    wpe_pasteboard_string_initialize(&pairs[0].type, typeUTF8.data(), typeUTF8.length());
    wpe_pasteboard_string_initialize(&pairs[0].string, stringUTF8.data(), stringUTF8.length());
    struct wpe_pasteboard_string_map map = { pairs, 1 };

    wpe_pasteboard_write(m_pasteboard, &map);

    wpe_pasteboard_string_free(&pairs[0].type);
    wpe_pasteboard_string_free(&pairs[0].string);
}

Vector<String> PlatformPasteboard::typesSafeForDOMToReadAndWrite(const String&) const
{
    return { };
}

long PlatformPasteboard::write(const PasteboardCustomData&)
{
    return 0;
}

} // namespace WebCore

#endif // USE(LIBWPE)
