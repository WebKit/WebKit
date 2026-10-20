/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/CommonAtomStrings.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/HTMLBodyElement.h>
#include <WebCore/HTMLDivElement.h>
#include <WebCore/HTMLHtmlElement.h>
#include <WebCore/NodeInlines.h>
#include <WebCore/Position.h>
#include <WebCore/PositionInlines.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/Settings.h>
#include <WebCore/Text.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(Position, OffsetIsBeforeLastNodeOffset)
{
    ProcessWarming::initializeNames();

    auto settings = Settings::create(nullptr);
    auto document = Document::create(settings.get(), aboutBlankURL());
    auto container = HTMLDivElement::create(document);
    auto firstChild = HTMLDivElement::create(document);
    auto secondChild = HTMLDivElement::create(document);
    container->appendChild(firstChild);
    container->appendChild(secondChild);

    EXPECT_TRUE(offsetIsBeforeLastNodeOffset(0, container.ptr()));
    EXPECT_TRUE(offsetIsBeforeLastNodeOffset(1, container.ptr()));
    EXPECT_FALSE(offsetIsBeforeLastNodeOffset(2, container.ptr()));
    EXPECT_FALSE(offsetIsBeforeLastNodeOffset(3, container.ptr()));

    EXPECT_FALSE(offsetIsBeforeLastNodeOffset(0, firstChild.ptr()));

    auto text = document->createTextNode("abc"_s);
    EXPECT_TRUE(offsetIsBeforeLastNodeOffset(0, text.ptr()));
    EXPECT_TRUE(offsetIsBeforeLastNodeOffset(2, text.ptr()));
    EXPECT_FALSE(offsetIsBeforeLastNodeOffset(3, text.ptr()));
}

} // namespace TestWebKitAPI
