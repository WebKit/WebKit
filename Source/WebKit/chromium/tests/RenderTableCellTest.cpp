/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// FIXME: If we get the TestWebKitAPI framework to bring a full Frame + DOM stack
// in a portable way, this test should be shared with all ports!

#include "config.h"

#include "RenderTableCell.h"

#include "Document.h"
#include "Frame.h"
#include "FrameTestHelpers.h"
#include "RenderArena.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "WebView.h"

#include <gtest/gtest.h>

using namespace WebKit;

namespace WebCore {

namespace {

class RenderTableCellDeathTest : public testing::Test {
    // It's unfortunate that we have to get the whole browser stack to test one RenderObject
    // but the code needs it.
    static Frame* frame()
    {
        static WebView* webView;

        if (webView)
            return static_cast<WebFrameImpl*>(webView->mainFrame())->frame();

        webView = FrameTestHelpers::createWebViewAndLoad("about:blank");
        webView->setFocus(true);
        return static_cast<WebFrameImpl*>(webView->mainFrame())->frame();
    }

    static Document* document()
    {
        return frame()->document();
    }

    static RenderArena* arena()
    {
        return document()->renderArena();
    }

    virtual void SetUp()
    {
        m_cell = RenderTableCell::createAnonymous(document());
    }

    virtual void TearDown()
    {
        m_cell->destroy();
    }

protected:
    RenderTableCell* m_cell;
};

TEST_F(RenderTableCellDeathTest, CanSetColumn)
{
    static const unsigned columnIndex = 10;
    m_cell->setCol(columnIndex);
    EXPECT_EQ(columnIndex, m_cell->col());
}

TEST_F(RenderTableCellDeathTest, CanSetColumnToMaxColumnIndex)
{
    m_cell->setCol(maxColumnIndex);
    EXPECT_EQ(maxColumnIndex, m_cell->col());
}

// FIXME: Re-enable these tests once ASSERT_DEATH is supported for Android.
// See: https://bugs.webkit.org/show_bug.cgi?id=74089
#if !OS(ANDROID)

TEST_F(RenderTableCellDeathTest, CrashIfColumnOverflowOnSetting)
{
    ASSERT_DEATH(m_cell->setCol(maxColumnIndex + 1), "");
}

TEST_F(RenderTableCellDeathTest, CrashIfSettingUnsetColumnIndex)
{
    ASSERT_DEATH(m_cell->setCol(unsetColumnIndex), "");
}

#endif

}

} // namespace WebCore
