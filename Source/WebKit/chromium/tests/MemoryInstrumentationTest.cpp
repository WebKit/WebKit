/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CachedImage.h"
#include "MemoryInstrumentationImpl.h"
#include <gtest/gtest.h>
#include <wtf/MemoryInstrumentation.h>
#include <wtf/OwnPtr.h>

using namespace WebCore;

using WTF::MemoryObjectInfo;
using WTF::MemoryClassInfo;
using WTF::MemoryObjectType;

namespace {

MemoryObjectType TestType = "TestType";

class ImageObserverTestHelper {
public:
    ImageObserverTestHelper()
        : m_cachedImage(adoptPtr(new CachedImage(0)))
        , m_imageOberver(m_cachedImage.get())
    {
    }
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
        info.addMember(m_cachedImage);
        info.addMember(m_imageOberver);
    }

    OwnPtr<CachedImage> m_cachedImage;
    ImageObserver* m_imageOberver;
};

TEST(MemoryInstrumentationTest, ImageObserver)
{
    ImageObserverTestHelper helper;
    EXPECT_NE(static_cast<void*>(helper.m_cachedImage.get()), static_cast<void*>(helper.m_imageOberver));

    class TestClient : public MemoryInstrumentationClientImpl {
    public:
        explicit TestClient(const void* unexpectedPointer) : m_unexpectedPointed(unexpectedPointer) { }
        virtual void countObjectSize(const void* pointer, MemoryObjectType type, size_t size) OVERRIDE
        {
            EXPECT_NE(m_unexpectedPointed, pointer);
            MemoryInstrumentationClientImpl::countObjectSize(pointer, type, size);
        }

    private:
        const void* m_unexpectedPointed;
    } client(helper.m_imageOberver);
    MemoryInstrumentationImpl instrumentation(&client);
    instrumentation.addRootObject(helper);
    EXPECT_EQ(sizeof(CachedImage), client.reportedSizeForAllTypes());
    EXPECT_EQ(2u, client.visitedObjects());
}


} // namespace

