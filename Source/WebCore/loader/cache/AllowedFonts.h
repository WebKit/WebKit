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

#pragma once

#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>
#include <wtf/RobinHoodHashSet.h>

namespace WebCore {

// This setting is used to define which types of fonts are allowed to be downloaded and loaded into the system.
// Any: any font binary will be downloaded, no checks will be done during load.
// Restricted: any font binary will be donwloaded but just binaries listed by WebKit will be allowed loaded. A not allowed binary will be deleted after the check is done (see CachedFont::shouldAllowCustomFont()).
// None: No font binary will be downloaded or loaded.
enum class DownloadableBinaryFontAllowedTypes: uint8_t {
    Any,
    Restricted,
    None
};

bool isFontBinaryAllowed(Span<const uint8_t>, DownloadableBinaryFontAllowedTypes);
bool isFontBinaryAllowed(const void*, size_t, DownloadableBinaryFontAllowedTypes);

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::DownloadableBinaryFontAllowedTypes> {
    using values = EnumValues<
        WebCore::DownloadableBinaryFontAllowedTypes,
        WebCore::DownloadableBinaryFontAllowedTypes::Any,
        WebCore::DownloadableBinaryFontAllowedTypes::Restricted,
        WebCore::DownloadableBinaryFontAllowedTypes::None
    >;
};

} // namespace WTF


