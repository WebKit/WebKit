/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "DragImage.h"

#include "Image.h"
#include "NativeImageSkia.h"
#include <gtest/gtest.h>
#include <wtf/PassOwnPtr.h>

using namespace WebCore;

namespace {

class TestImage : public Image {
public:

    static PassRefPtr<TestImage> create(const IntSize& size)
    {
        return adoptRef(new TestImage(size));
    }

    explicit TestImage(const IntSize& size)
        : Image(0)
        , m_size(size)
    {
        m_nativeImage = adoptPtr(new NativeImageSkia());
        m_nativeImage->bitmap().setConfig(SkBitmap::kARGB_8888_Config,
                                          size.width(), size.height(), 0);
        m_nativeImage->bitmap().allocPixels();
    }

    virtual IntSize size() const
    {
        return m_size;
    }

    virtual NativeImagePtr nativeImageForCurrentFrame()
    {
        if (m_size.isZero())
            return 0;

        return m_nativeImage.get();
    }

    // Stub implementations of pure virtual Image functions.
    virtual void destroyDecodedData(bool)
    {
    }

    virtual unsigned int decodedSize() const
    {
        return 0u;
    }

    virtual void draw(WebCore::GraphicsContext*, const WebCore::FloatRect&,
                      const WebCore::FloatRect&, WebCore::ColorSpace,
                      WebCore::CompositeOperator)
    {
    }

private:

    IntSize m_size;

    OwnPtr<NativeImagePtr> m_nativeImage;
};

TEST(DragImageTest, NullHandling)
{
    EXPECT_FALSE(createDragImageFromImage(0));

    deleteDragImage(0);
    EXPECT_TRUE(dragImageSize(0).isZero());
    EXPECT_FALSE(scaleDragImage(0, FloatSize(0.5, 0.5)));
    EXPECT_FALSE(dissolveDragImageToFraction(0, 0.5));
    EXPECT_FALSE(createDragImageFromImage(0));
    EXPECT_FALSE(createDragImageIconForCachedImage(0));
}

TEST(DragImageTest, NonNullHandling)
{
    RefPtr<TestImage> testImage(TestImage::create(IntSize(2, 2)));
    DragImageRef dragImage = createDragImageFromImage(testImage.get());
    ASSERT_TRUE(dragImage);

    dragImage = scaleDragImage(dragImage, FloatSize(0.5, 0.5));
    ASSERT_TRUE(dragImage);
    IntSize size = dragImageSize(dragImage);
    EXPECT_EQ(1, size.width());
    EXPECT_EQ(1, size.height());

    dragImage = dissolveDragImageToFraction(dragImage, 0.5);
    ASSERT_TRUE(dragImage);

    deleteDragImage(dragImage);
}

TEST(DragImageTest, CreateDragImage)
{
    {
        // Tests that the DrageImage implementation doesn't choke on null values
        // of nativeImageForCurrentFrame().
        RefPtr<TestImage> testImage(TestImage::create(IntSize()));
        EXPECT_FALSE(createDragImageFromImage(testImage.get()));
    }

    {
        // Tests that the drag image is a deep copy.
        RefPtr<TestImage> testImage(TestImage::create(IntSize(1, 1)));
        DragImageRef dragImage = createDragImageFromImage(testImage.get());
        ASSERT_TRUE(dragImage);
        SkAutoLockPixels lock1(*dragImage), lock2(testImage->nativeImageForCurrentFrame()->bitmap());
        EXPECT_NE(dragImage->getPixels(), testImage->nativeImageForCurrentFrame()->bitmap().getPixels());
    }
}

} // anonymous namespace
