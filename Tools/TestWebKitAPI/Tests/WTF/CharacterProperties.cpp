/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include <wtf/text/CharacterProperties.h>

namespace TestWebKitAPI {

TEST(WTF_CharacterProperties, IsEmojiWithPresentationByDefaultForLatin1)
{
    // Latin-1 characters do not have UCHAR_EMOJI_PRESENTATION.
    for (char32_t i = 0; i < 256; ++i)
        EXPECT_FALSE(u_hasBinaryProperty(i, UCHAR_EMOJI_PRESENTATION)) << static_cast<unsigned>(i);
}

TEST(WTF_CharacterProperties, isEmojiModifierBaseForLatin1)
{
    // Latin-1 characters do not have UCHAR_EMOJI_MODIFIER_BASE.
    for (char32_t i = 0; i < 256; ++i)
        EXPECT_FALSE(u_hasBinaryProperty(i, UCHAR_EMOJI_MODIFIER_BASE)) << static_cast<unsigned>(i);
}

} // namespace TestWebKitAPI
