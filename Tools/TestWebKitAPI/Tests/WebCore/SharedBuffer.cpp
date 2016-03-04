/*
 * Copyright (C) 2015 Canon Inc. All rights reserved.
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

#include "Test.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/MainThread.h>
#include <wtf/StringExtras.h>

using namespace WebCore;

namespace TestWebKitAPI {

const char* SharedBufferTestData = "This is a test";

class SharedBufferTest : public testing::Test {
public:
    void SetUp() override
    {
        WTF::initializeMainThread();
        
        // create temp file
        PlatformFileHandle handle;
        m_tempFilePath = openTemporaryFile("tempTestFile", handle);
        writeToFile(handle, SharedBufferTestData, strlen(SharedBufferTestData));
        closeFile(handle); 

        m_tempEmptyFilePath = openTemporaryFile("tempEmptyTestFile", handle);
        closeFile(handle); 
    }

    void TearDown() override
    {
        deleteFile(m_tempFilePath);
        deleteFile(m_tempEmptyFilePath);
    }

    const String& tempFilePath() { return m_tempFilePath; }
    const String& tempEmptyFilePath() { return m_tempEmptyFilePath; }

private:
    String m_tempFilePath;
    String m_tempEmptyFilePath;
};

TEST_F(SharedBufferTest, createWithContentsOfMissingFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(String("not_existing_file"));
    ASSERT_NULL(buffer);
}

TEST_F(SharedBufferTest, createWithContentsOfExistingFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    EXPECT_TRUE(buffer->size() == strlen(SharedBufferTestData));
    EXPECT_TRUE(String(SharedBufferTestData) == String(buffer->data(), buffer->size()));
}

TEST_F(SharedBufferTest, createWithContentsOfExistingEmptyFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempEmptyFilePath());
    ASSERT_NOT_NULL(buffer);
    EXPECT_TRUE(buffer->isEmpty());
}

TEST_F(SharedBufferTest, copyBufferCreatedWithContentsOfExistingFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    RefPtr<SharedBuffer> copy = buffer->copy();
    EXPECT_GT(buffer->size(), 0U);
    EXPECT_TRUE(buffer->size() == copy->size());
    EXPECT_TRUE(!memcmp(buffer->data(), copy->data(), buffer->size()));
}

TEST_F(SharedBufferTest, clearBufferCreatedWithContentsOfExistingFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    buffer->clear();
    EXPECT_TRUE(!buffer->size());
    EXPECT_TRUE(!buffer->data());
}

TEST_F(SharedBufferTest, appendBufferCreatedWithContentsOfExistingFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    buffer->append("a", 1);
    EXPECT_TRUE(buffer->size() == (strlen(SharedBufferTestData) + 1));
    EXPECT_TRUE(!memcmp(buffer->data(), SharedBufferTestData, strlen(SharedBufferTestData)));
    EXPECT_EQ('a', buffer->data()[strlen(SharedBufferTestData)]);
}

}
