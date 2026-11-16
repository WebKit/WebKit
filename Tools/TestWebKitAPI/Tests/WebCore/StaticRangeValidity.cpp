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
#include <WebCore/ProcessWarming.h>
#include <WebCore/Settings.h>
#include <WebCore/StaticRange.h>

// Tests for https://dom.spec.whatwg.org/#staticrange-valid

namespace TestWebKitAPI {

using namespace WebCore;

static Ref<Document> createDocument()
{
    ProcessWarming::initializeNames();

    auto settings = Settings::create(nullptr);
    auto document = Document::create(settings.get(), aboutBlankURL());
    auto documentElement = HTMLHtmlElement::create(document);
    document->appendChild(documentElement);
    auto body = HTMLBodyElement::create(document);
    documentElement->appendChild(body);
    return document;
}

static Ref<StaticRange> createStaticRange(Node& startContainer, unsigned startOffset, Node& endContainer, unsigned endOffset)
{
    return StaticRange::create(SimpleRange { { startContainer, startOffset }, { endContainer, endOffset } });
}

TEST(StaticRangeValidity, CollapsedSameNode)
{
    auto document = createDocument();
    auto& body = *document->body();

    // Collapsed on element with no children (length 0).
    auto div = HTMLDivElement::create(document);
    body.appendChild(div);
    EXPECT_TRUE(createStaticRange(div, 0, div, 0)->computeValidity());

    // Collapsed on element with children.
    EXPECT_TRUE(createStaticRange(body, 0, body, 0)->computeValidity());
    EXPECT_TRUE(createStaticRange(body, 1, body, 1)->computeValidity());
}

TEST(StaticRangeValidity, NonCollapsedSameNode)
{
    auto document = createDocument();
    auto& body = *document->body();

    auto div1 = HTMLDivElement::create(document);
    body.appendChild(div1);
    auto div2 = HTMLDivElement::create(document);
    body.appendChild(div2);

    // Forward range within element (body has 2 children).
    EXPECT_TRUE(createStaticRange(body, 0, body, 2)->computeValidity());
    EXPECT_TRUE(createStaticRange(body, 0, body, 1)->computeValidity());

    // Backwards range within element (start after end).
    EXPECT_FALSE(createStaticRange(body, 2, body, 0)->computeValidity());
    EXPECT_FALSE(createStaticRange(body, 2, body, 1)->computeValidity());
}

TEST(StaticRangeValidity, OffsetBeyondLength)
{
    auto document = createDocument();
    auto& body = *document->body();

    auto div = HTMLDivElement::create(document);
    body.appendChild(div);

    // Offset beyond length on element with no children.
    EXPECT_FALSE(createStaticRange(div, 1, div, 1)->computeValidity());
    EXPECT_FALSE(createStaticRange(div, 0, div, 1)->computeValidity());

    // body has 1 child, so length is 1.
    EXPECT_TRUE(createStaticRange(body, 0, body, 1)->computeValidity());
    EXPECT_FALSE(createStaticRange(body, 0, body, 2)->computeValidity());
    EXPECT_FALSE(createStaticRange(body, 2, body, 2)->computeValidity());
}

TEST(StaticRangeValidity, DifferentNodesInSameTree)
{
    auto document = createDocument();
    auto& body = *document->body();

    auto div1 = HTMLDivElement::create(document);
    body.appendChild(div1);
    auto div2 = HTMLDivElement::create(document);
    body.appendChild(div2);

    // Start in earlier sibling, end in later sibling.
    EXPECT_TRUE(createStaticRange(div1, 0, div2, 0)->computeValidity());

    // Start in later sibling, end in earlier sibling.
    EXPECT_FALSE(createStaticRange(div2, 0, div1, 0)->computeValidity());

    // Start in ancestor, end in descendant.
    EXPECT_TRUE(createStaticRange(body, 0, div1, 0)->computeValidity());

    // Start in descendant, end in ancestor.
    EXPECT_FALSE(createStaticRange(div1, 0, body, 0)->computeValidity());
}

TEST(StaticRangeValidity, DifferentTrees)
{
    auto document1 = createDocument();
    auto& body1 = *document1->body();

    auto document2 = createDocument();
    auto& body2 = *document2->body();

    // Nodes in different documents.
    EXPECT_FALSE(createStaticRange(body1, 0, body2, 0)->computeValidity());
}

TEST(StaticRangeValidity, DisconnectedNode)
{
    auto document = createDocument();
    auto& body = *document->body();

    auto disconnected = HTMLDivElement::create(document);

    // One node connected, one disconnected.
    EXPECT_FALSE(createStaticRange(body, 0, disconnected, 0)->computeValidity());
    EXPECT_FALSE(createStaticRange(disconnected, 0, body, 0)->computeValidity());

    // Both disconnected but from the same document.
    auto disconnected2 = HTMLDivElement::create(document);
    EXPECT_FALSE(createStaticRange(disconnected, 0, disconnected2, 0)->computeValidity());
}

} // namespace TestWebKitAPI
