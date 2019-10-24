/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#import "config.h"

#import "SharedBufferTest.h"
#import "Utilities.h"
#import <WebCore/SharedBuffer.h>

using namespace WebCore;

namespace TestWebKitAPI {

static void expectDataArraysEqual(NSArray<NSData *> *expected, NSArray<NSData *> *actual)
{
    EXPECT_EQ(expected.count, actual.count);
    for (NSUInteger i = 0; i < expected.count; ++i) {
        NSData *expectedData = expected[i];
        NSData *actualData = actual[i];
        EXPECT_TRUE([expectedData isKindOfClass:[NSData class]]);
        EXPECT_TRUE([actualData isKindOfClass:[NSData class]]);
        EXPECT_TRUE([expectedData isEqualToData:actualData]);
    }
}

TEST_F(SharedBufferTest, createNSDataArray)
{
    @autoreleasepool {
        auto buffer = SharedBuffer::create();
        expectDataArraysEqual(nil, buffer->createNSDataArray().get());

        NSData *helloData = [NSData dataWithBytes:"hello" length:5];
        buffer->append((const char*)helloData.bytes, helloData.length);
        expectDataArraysEqual(@[ helloData ], buffer->createNSDataArray().get());

        NSData *worldData = [NSData dataWithBytes:"world" length:5];
        buffer->append((__bridge CFDataRef)worldData);
        expectDataArraysEqual(@[ helloData, worldData ], buffer->createNSDataArray().get());

        expectDataArraysEqual(@[ helloData ], SharedBuffer::create(helloData)->createNSDataArray().get());
        expectDataArraysEqual(@[ worldData ], SharedBuffer::create((__bridge CFDataRef)worldData)->createNSDataArray().get());

        expectDataArraysEqual(@[ [NSData dataWithContentsOfFile:tempFilePath()] ], SharedBuffer::createWithContentsOfFile(tempFilePath())->createNSDataArray().get());
    }
}

TEST_F(SharedBufferTest, createNSDataForDataSegment)
{
    @autoreleasepool {
        auto buffer = SharedBuffer::create();

        NSData *helloData = [NSData dataWithBytes:"hello" length:5];
        buffer->append((const char*)helloData.bytes, helloData.length);

        NSData *worldData = [NSData dataWithBytes:"world" length:5];
        buffer->append((__bridge CFDataRef)worldData);

        NSArray *expectedData = @[ helloData, worldData ];
        NSUInteger expectedSize = helloData.length + worldData.length;

        NSUInteger segmentCount = 0;
        for (auto& segment : buffer.get())
            EXPECT_TRUE([segment.segment->createNSData() isEqualToData:expectedData[segmentCount++]]);
        EXPECT_EQ(expectedData.count, segmentCount);

        NSUInteger position = 0;
        segmentCount = 0;
        while (buffer->size() > position) {
            auto incrementalData = buffer->getSomeData(position);
            EXPECT_TRUE([incrementalData.createNSData() isEqualToData:expectedData[segmentCount++]]);
            position += incrementalData.size();
        }
        EXPECT_EQ(expectedData.count, segmentCount);

        auto lastByte = buffer->getSomeData(expectedSize - 1);
        EXPECT_TRUE([lastByte.createNSData() isEqualToData:[@"d" dataUsingEncoding:NSASCIIStringEncoding]]);
        EXPECT_EQ(1LU, lastByte.size());
    }
}

}; // namespace TestWebKitAPI
