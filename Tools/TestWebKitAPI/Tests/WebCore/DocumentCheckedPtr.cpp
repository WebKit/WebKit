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

#include <WebCore/DocumentInlines.h>
#include <WebCore/NodeInlines.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/Settings.h>
#include <wtf/CheckedPtr.h>

namespace TestWebKitAPI {

using namespace WebCore;

// Document inherits CheckedPtr support from both Node and ScriptExecutionContext. All CheckedPtrs to a document must
// increment the same counter, or WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR would miss some of them when the document is deleted.
TEST(DocumentCheckedPtr, NodeAndScriptExecutionContextShareCounter)
{
    ProcessWarming::initializeNames();

    auto settings = Settings::create(nullptr);
    Ref document = Document::create(settings.get(), aboutBlankURL());
    Node& node = document.get();
    ScriptExecutionContext& context = document.get();

    auto initialCount = node.checkedPtrCount();
    EXPECT_EQ(context.checkedPtrCount(), initialCount);
    EXPECT_EQ(document->checkedPtrCount(), initialCount);

    {
        CheckedPtr<Node> nodePtr = &node;
        EXPECT_EQ(node.checkedPtrCount(), initialCount + 1);
        EXPECT_EQ(context.checkedPtrCount(), initialCount + 1);

        CheckedPtr<ScriptExecutionContext> contextPtr = &context;
        EXPECT_EQ(node.checkedPtrCount(), initialCount + 2);
        EXPECT_EQ(context.checkedPtrCount(), initialCount + 2);

        CheckedPtr<Document> documentPtr = document.ptr();
        EXPECT_EQ(node.checkedPtrCount(), initialCount + 3);
        EXPECT_EQ(context.checkedPtrCount(), initialCount + 3);
        EXPECT_EQ(document->checkedPtrCount(), initialCount + 3);
    }

    EXPECT_EQ(node.checkedPtrCount(), initialCount);
    EXPECT_EQ(context.checkedPtrCount(), initialCount);
}

} // namespace TestWebKitAPI
