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

#include <public/WebImage.h>

#include "FileSystem.h"
#include "SharedBuffer.h"
#include <gtest/gtest.h>
#include <public/WebData.h>
#include <public/WebSize.h>
#include <webkit/support/webkit_support.h>

using namespace WebCore;
using namespace WebKit;

namespace {

static PassRefPtr<SharedBuffer> readFile(const char* fileName)
{
    String filePath = webkit_support::GetWebKitRootDir();
    filePath.append("/Source/WebKit/chromium/tests/data/");
    filePath.append(fileName);

    long long fileSize;
    if (!getFileSize(filePath, fileSize))
        return 0;

    PlatformFileHandle handle = openFile(filePath, OpenForRead);
    int fileLength = static_cast<int>(fileSize);
    Vector<char> buffer(fileLength);
    readFromFile(handle, buffer.data(), fileLength);
    closeFile(handle);
    return SharedBuffer::adoptVector(buffer);
}

#if OS(LINUX)
TEST(WebImageTest, PNGImage)
#else
TEST(WebImageTest, DISABLED_PNGImage)
#endif
{
    RefPtr<SharedBuffer> data = readFile("white-1x1.png");
    ASSERT_TRUE(data.get());

    WebImage image = WebImage::fromData(WebData(data), WebSize());
    EXPECT_TRUE(image.size() == WebSize(1, 1));
    SkAutoLockPixels autoLock(image.getSkBitmap());
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), image.getSkBitmap().getColor(0, 0));
}

#if OS(LINUX)
TEST(WebImageTest, ICOImage)
#else
TEST(WebImageTest, DISABLED_ICOImage)
#endif
{
    RefPtr<SharedBuffer> data = readFile("black-and-white.ico");
    ASSERT_TRUE(data.get());

    WebVector<WebImage> images = WebImage::framesFromData(WebData(data));
    ASSERT_EQ(2u, images.size());
    EXPECT_TRUE(images[0].size() == WebSize(2, 2));
    EXPECT_TRUE(images[1].size() == WebSize(1, 1));
    SkAutoLockPixels autoLock1(images[0].getSkBitmap());
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), images[0].getSkBitmap().getColor(0, 0));
    SkAutoLockPixels autoLock2(images[1].getSkBitmap());
    EXPECT_EQ(SkColorSetARGB(255, 0, 0, 0), images[1].getSkBitmap().getColor(0, 0));
}

TEST(WebImageTest, ICOValidHeaderMissingBitmap)
{
    RefPtr<SharedBuffer> data = readFile("valid_header_missing_bitmap.ico");
    ASSERT_TRUE(data.get());

    WebVector<WebImage> images = WebImage::framesFromData(WebData(data));
    ASSERT_TRUE(images.isEmpty());
}

TEST(WebImageTest, BadImage)
{
    const char badImage[] = "hello world";
    WebVector<WebImage> images = WebImage::framesFromData(WebData(badImage));
    ASSERT_EQ(0u, images.size());

    WebImage image = WebImage::fromData(WebData(badImage), WebSize());
    EXPECT_TRUE(image.getSkBitmap().empty());
    EXPECT_TRUE(image.getSkBitmap().isNull());
}

} // namespace
