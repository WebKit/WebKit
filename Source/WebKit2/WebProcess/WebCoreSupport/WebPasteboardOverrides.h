/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef WebPasteboardOverrides_h
#define WebPasteboardOverrides_h

#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebPasteboardOverrides {
    friend class NeverDestroyed<WebPasteboardOverrides>;
public:
    static WebPasteboardOverrides& sharedPasteboardOverrides();

    void addOverride(const String& pasteboardName, const String& type, const Vector<uint8_t>&);
    void removeOverride(const String& pasteboardName, const String& type);

    Vector<String> overriddenTypes(const String& pasteboardName);

    bool getDataForOverride(const String& pasteboardName, const String& type, Vector<uint8_t>&) const;
    bool getDataForOverride(const String& pasteboardName, const String& type, Vector<char>&) const;

private:
    WebPasteboardOverrides();

    // The m_overridesMap maps string pasteboard names to pasteboard entries.
    // Each pasteboard entry is a map of a string type to a data buffer.
    HashMap<String, std::unique_ptr<HashMap<String, Vector<uint8_t>>>> m_overridesMap;
};

} // namespace WebKit

#endif // WebPasteboardOverrides_h
