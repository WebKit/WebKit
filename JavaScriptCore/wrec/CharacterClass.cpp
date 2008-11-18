/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CharacterClass.h"

#if ENABLE(WREC)

using namespace WTF;

namespace JSC { namespace WREC {

const CharacterClass& CharacterClass::newline() {
    static const UChar asciiNewlines[2] = { '\n', '\r' };
    static const UChar unicodeNewlines[2] = { 0x2028, 0x2029 };
    static const CharacterClass charClass = {
        asciiNewlines, 2,
        0, 0,
        unicodeNewlines, 2,
        0, 0,
    };
    
    return charClass;
}

const CharacterClass& CharacterClass::digits() {
    static const CharacterRange asciiDigitsRange[1] = { { '0', '9' } };
    static const CharacterClass charClass = {
        0, 0,
        asciiDigitsRange, 1,
        0, 0,
        0, 0,
    };
    
    return charClass;
}

const CharacterClass& CharacterClass::spaces() {
    static const UChar asciiSpaces[1] = { ' ' };
    static const CharacterRange asciiSpacesRange[1] = { { '\t', '\r' } };
    static const UChar unicodeSpaces[8] = { 0x00a0, 0x1680, 0x180e, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000 };
    static const CharacterRange unicodeSpacesRange[1] = { { 0x2000, 0x200a } };
    static const CharacterClass charClass = {
        asciiSpaces, 1,
        asciiSpacesRange, 1,
        unicodeSpaces, 8,
        unicodeSpacesRange, 1,
    };
    
    return charClass;
}

const CharacterClass& CharacterClass::wordchar() {
    static const UChar asciiWordchar[1] = { '_' };
    static const CharacterRange asciiWordcharRange[3] = { { '0', '9' }, { 'A', 'Z' }, { 'a', 'z' } };
    static const CharacterClass charClass = {
        asciiWordchar, 1,
        asciiWordcharRange, 3,
        0, 0,
        0, 0,
    };
    
    return charClass;
}

const CharacterClass& CharacterClass::nondigits() {
    static const CharacterRange asciiNondigitsRange[2] = { { 0, '0' - 1 }, { '9' + 1, 0x7f } };
    static const CharacterRange unicodeNondigitsRange[1] = { { 0x0080, 0xffff } };
    static const CharacterClass charClass = {
        0, 0,
        asciiNondigitsRange, 2,
        0, 0,
        unicodeNondigitsRange, 1,
    };
    
    return charClass;
}

const CharacterClass& CharacterClass::nonspaces() {
    static const CharacterRange asciiNonspacesRange[3] = { { 0, '\t' - 1 }, { '\r' + 1, ' ' - 1 }, { ' ' + 1, 0x7f } }; 
    static const CharacterRange unicodeNonspacesRange[9] = {
        { 0x0080, 0x009f },
        { 0x00a1, 0x167f },
        { 0x1681, 0x180d },
        { 0x180f, 0x1fff },
        { 0x200b, 0x2027 },
        { 0x202a, 0x202e },
        { 0x2030, 0x205e },
        { 0x2060, 0x2fff },
        { 0x3001, 0xffff }
    }; 
    static const CharacterClass charClass = {
        0, 0,
        asciiNonspacesRange, 3,
        0, 0,
        unicodeNonspacesRange, 9,
    };
    
    return charClass;
}

const CharacterClass& CharacterClass::nonwordchar() {
    static const UChar asciiNonwordchar[1] = { '`' };
    static const CharacterRange asciiNonwordcharRange[4] = { { 0, '0' - 1 }, { '9' + 1, 'A' - 1 }, { 'Z' + 1, '_' - 1 }, { 'z' + 1, 0x7f } };
    static const CharacterRange unicodeNonwordcharRange[1] = { { 0x0080, 0xffff } };
    static const CharacterClass charClass = {
        asciiNonwordchar, 1,
        asciiNonwordcharRange, 4,
        0, 0,
        unicodeNonwordcharRange, 1,
    };
    
    return charClass;
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
