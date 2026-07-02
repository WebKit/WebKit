// Copyright 2016 The Chromium Authors. All rights reserved.
// Copyright (C) 2019 Sony Interactive Entertainment Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <windows.h>
#include <wtf/HashMap.h>
#include <wtf/OptionSetHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class WindowsKeyNames {
public:
    WEBCORE_EXPORT WindowsKeyNames();

    WEBCORE_EXPORT String domKeyFromParams(WPARAM, LPARAM);
    WEBCORE_EXPORT String domKeyFromChar(char16_t);
    WEBCORE_EXPORT String domCodeFromLParam(LPARAM);

    // Windows has no dedicated AltGr key; it synthesizes AltGr as a left-Control
    // + right-Alt combination. Returns whether that combination should currently
    // be reported as the AltGraph modifier: true only while right-Alt is held on
    // a keyboard layout that actually defines AltGraph (otherwise right-Alt is a
    // plain Alt). Used for events that carry no key code (mouse, wheel).
    WEBCORE_EXPORT bool shouldExposeLeftControlPlusRightAltAsAltGraph();

    // Whether the given keyboard event should be reported with the AltGraph
    // modifier in place of Control+Alt. Beyond the right-Alt case above, this
    // also recognises AltGr simulated via Control+Alt: a character message
    // delivered under Control+Alt, or a key whose printable character requires
    // the Control+Alt combination on the active layout. Matches Blink and Gecko.
    WEBCORE_EXPORT bool shouldExposeAltGraphForKeyEvent(UINT message, WPARAM virtualKey, LPARAM);

    enum class KeyModifier : uint8_t;
    using KeyModifierSet = OptionSet<KeyModifier>;

private:
    void updateLayout();

    KeyModifierSet currentKeyModifiers();

    struct PrintableKey {
        // The DOM key string, or a null String if the key produces no printable
        // character under the requested modifiers.
        String key;
        // Whether the matched modifier combination needed both Control and Alt,
        // i.e. the key is AltGraph-shifted on the active layout.
        bool isAltGraphShifted { false };
    };
    // Looks up the printable DOM key produced by |virtualKey| under the active
    // layout for |modifiers|, following the UIEvents key guidelines.
    PrintableKey printableKeyFromVirtualKey(unsigned virtualKey, KeyModifierSet);


    HKL m_keyboardLayout = 0;
    bool m_hasAltGraph = false;

    using VirtualKeyModifierSetPair = std::pair<unsigned, KeyModifierSet>;
    using VirtualKeyToKeyMap = HashMap<VirtualKeyModifierSetPair, String, DefaultHash<VirtualKeyModifierSetPair>, PairHashTraits<WTF::UnsignedWithZeroKeyHashTraits<unsigned>, HashTraits<KeyModifierSet>>>;
    VirtualKeyToKeyMap m_printableKeyCodeToKey;
};

} // namespace WebCore
