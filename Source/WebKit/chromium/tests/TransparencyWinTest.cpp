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
#include "TransparencyWin.h"

#include "AffineTransform.h"
#include "ImageBuffer.h"

#include <gtest/gtest.h>
#include <windows.h>

namespace WebCore {

static FloatRect RECTToFloatRect(const RECT* rect)
{
    return FloatRect(static_cast<float>(rect->left),
                     static_cast<float>(rect->top),
                     static_cast<float>(rect->right - rect->left),
                     static_cast<float>(rect->bottom - rect->top));
}

static void drawNativeRect(GraphicsContext* context,
                           int x, int y, int w, int h)
{
    skia::PlatformCanvas* canvas = context->platformContext()->canvas();
    HDC dc = canvas->beginPlatformPaint();

    RECT innerRc;
    innerRc.left = x;
    innerRc.top = y;
    innerRc.right = x + w;
    innerRc.bottom = y + h;
    FillRect(dc, &innerRc,
             reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    canvas->endPlatformPaint();
}

static Color getPixelAt(GraphicsContext* context, int x, int y)
{
    const SkBitmap& bitmap = context->platformContext()->canvas()->
        getTopPlatformDevice().accessBitmap(false);
    return Color(*reinterpret_cast<const RGBA32*>(bitmap.getAddr32(x, y)));
}

// Resets the top layer's alpha channel to 0 for each pixel. This simulates
// Windows messing it up.
static void clearTopLayerAlphaChannel(GraphicsContext* context)
{
    SkBitmap& bitmap = const_cast<SkBitmap&>(context->platformContext()->
        canvas()->getTopPlatformDevice().accessBitmap(false));
    for (int y = 0; y < bitmap.height(); y++) {
        uint32_t* row = bitmap.getAddr32(0, y);
        for (int x = 0; x < bitmap.width(); x++)
            row[x] &= 0x00FFFFFF;
    }
}

// Clears the alpha channel on the specified pixel.
static void clearTopLayerAlphaPixel(GraphicsContext* context, int x, int y)
{
    SkBitmap& bitmap = const_cast<SkBitmap&>(context->platformContext()->
        canvas()->getTopPlatformDevice().accessBitmap(false));
    *bitmap.getAddr32(x, y) &= 0x00FFFFFF;
}

static std::ostream& operator<<(std::ostream& out, const Color& c)
{
    std::ios_base::fmtflags oldFlags = out.flags(std::ios_base::hex |
                                                 std::ios_base::showbase);
    out << c.rgb();
    out.flags(oldFlags);
    return out;
}

TEST(TransparencyWin, NoLayer)
{
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(17, 16), ColorSpaceDeviceRGB));

    // KeepTransform
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::NoLayer,
                    TransparencyWin::KeepTransform,
                    IntRect(1, 1, 14, 12));

        EXPECT_TRUE(src->context() == helper.context());
        EXPECT_TRUE(IntSize(14, 12) == helper.m_layerSize);
        EXPECT_TRUE(IntRect(1, 1, 14, 12) == helper.drawRect());
    }

    // Untransform is not allowed for NoLayer.

    // ScaleTransform
    src->context()->save();
    src->context()->scale(FloatSize(2.0, 0.5));
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::NoLayer,
                    TransparencyWin::ScaleTransform,
                    IntRect(2, 2, 6, 6));
        helper.composite();

        // The coordinate system should be based in the upper left of our box.
        // It should be post-transformed.
        EXPECT_TRUE(src->context() == helper.context());
        EXPECT_TRUE(IntSize(12, 3) == helper.m_layerSize);
        EXPECT_TRUE(IntRect(4, 1, 12, 3) == helper.drawRect());
    }
    src->context()->restore();
}

TEST(TransparencyWin, WhiteLayer)
{
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));

    // KeepTransform
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::WhiteLayer,
                    TransparencyWin::KeepTransform,
                    IntRect(1, 1, 14, 12));
        helper.composite();

        EXPECT_TRUE(src->context() != helper.context());
        EXPECT_TRUE(IntSize(14, 12) == helper.m_layerSize);
        EXPECT_TRUE(IntRect(1, 1, 14, 12) == helper.drawRect());
    }

    // Untransform
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::WhiteLayer,
                    TransparencyWin::Untransform,
                    IntRect(1, 1, 14, 12));
        helper.composite();

        EXPECT_TRUE(src->context() != helper.context());
        EXPECT_TRUE(IntSize(14, 12) == helper.m_layerSize);
        EXPECT_TRUE(IntRect(0, 0, 14, 12) == helper.drawRect());
    }

    // ScaleTransform
    src->context()->save();
    src->context()->scale(FloatSize(2.0, 0.5));
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::WhiteLayer,
                    TransparencyWin::ScaleTransform,
                    IntRect(2, 2, 6, 6));
        helper.composite();

        // The coordinate system should be based in the upper left of our box.
        // It should be post-transformed.
        EXPECT_TRUE(src->context() != helper.context());
        EXPECT_TRUE(IntSize(12, 3) == helper.m_layerSize);
        EXPECT_TRUE(IntRect(0, 0, 12, 3) == helper.drawRect());
    }
    src->context()->restore();
}

TEST(TransparencyWin, TextComposite)
{
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));

    // KeepTransform is the only valid transform mode for TextComposite.
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::TextComposite,
                    TransparencyWin::KeepTransform,
                    IntRect(1, 1, 14, 12));
        helper.composite();

        EXPECT_TRUE(src->context() != helper.context());
        EXPECT_TRUE(IntSize(14, 12) == helper.m_layerSize);
        EXPECT_TRUE(IntRect(1, 1, 14, 12) == helper.drawRect());
    }
}

TEST(TransparencyWin, OpaqueCompositeLayer)
{
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));

    // KeepTransform
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::OpaqueCompositeLayer,
                    TransparencyWin::KeepTransform,
                    IntRect(1, 1, 14, 12));
        helper.composite();

        EXPECT_TRUE(src->context() != helper.context());
        EXPECT_TRUE(IntSize(14, 12) == helper.m_layerSize);
        EXPECT_TRUE(IntRect(1, 1, 14, 12) == helper.drawRect());
    }

    // KeepTransform with scroll applied.
    src->context()->save();
    src->context()->translate(0, -1);
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::OpaqueCompositeLayer,
                    TransparencyWin::KeepTransform,
                    IntRect(1, 1, 14, 14));
        helper.composite();

        EXPECT_TRUE(src->context() != helper.context());
        EXPECT_TRUE(IntSize(14, 14) == helper.m_layerSize);
        EXPECT_TRUE(IntRect(1, 1, 14, 14) == helper.drawRect());
    }
    src->context()->restore();

    // Untransform
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::OpaqueCompositeLayer,
                    TransparencyWin::Untransform,
                    IntRect(1, 1, 14, 12));
        helper.composite();

        EXPECT_TRUE(src->context() != helper.context());
        EXPECT_TRUE(IntSize(14, 12) == helper.m_layerSize);
        EXPECT_TRUE(IntRect(0, 0, 14, 12) == helper.drawRect());
    }

    // ScaleTransform
    src->context()->save();
    src->context()->scale(FloatSize(2.0, 0.5));
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::OpaqueCompositeLayer,
                    TransparencyWin::ScaleTransform,
                    IntRect(2, 2, 6, 6));
        helper.composite();

        // The coordinate system should be based in the upper left of our box.
        // It should be post-transformed.
        EXPECT_TRUE(src->context() != helper.context());
        EXPECT_TRUE(IntSize(12, 3) == helper.m_layerSize);
        EXPECT_TRUE(IntRect(0, 0, 12, 3) == helper.drawRect());
    }
    src->context()->restore();
}

TEST(TransparencyWin, WhiteLayerPixelTest)
{
    // Make a total transparent buffer, and draw the white layer inset by 1 px.
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));

    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::WhiteLayer,
                    TransparencyWin::KeepTransform,
                    IntRect(1, 1, 14, 14));

        // Coordinates should be in the original space, not the layer.
        drawNativeRect(helper.context(), 3, 3, 1, 1);
        clearTopLayerAlphaChannel(helper.context());
        helper.composite();
    }

    // The final image should be transparent around the edges for 1 px, white
    // in the middle, with (3,3) (what we drew above) being opaque black.
    EXPECT_EQ(Color(Color::transparent), getPixelAt(src->context(), 0, 0));
    EXPECT_EQ(Color(Color::white), getPixelAt(src->context(), 2, 2));
    EXPECT_EQ(Color(Color::black), getPixelAt(src->context(), 3, 3));
    EXPECT_EQ(Color(Color::white), getPixelAt(src->context(), 4, 4));
}

TEST(TransparencyWin, OpaqueCompositeLayerPixel)
{
    Color red(0xFFFF0000), darkRed(0xFFBF0000);
    Color green(0xFF00FF00);

    // Make a red bottom layer, followed by a half green next layer @ 50%.
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));

    FloatRect fullRect(0, 0, 16, 16);
    src->context()->fillRect(fullRect, red, ColorSpaceDeviceRGB);
    src->context()->beginTransparencyLayer(0.5);
    FloatRect rightHalf(8, 0, 8, 16);
    src->context()->fillRect(rightHalf, green, ColorSpaceDeviceRGB);

    // Make a transparency layer inset by one pixel, and fill it inset by
    // another pixel with 50% black.
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::OpaqueCompositeLayer,
                    TransparencyWin::KeepTransform,
                    IntRect(1, 1, 14, 14));

        FloatRect inner(2, 2, 12, 12);
        helper.context()->fillRect(inner, Color(0x7f000000), ColorSpaceDeviceRGB);
        // These coordinates are relative to the layer, whish is inset by 1x1
        // pixels from the top left. So we're actually clearing (2, 2) and
        // (13,13), which are the extreme corners of the black area (and which
        // we check below).
        clearTopLayerAlphaPixel(helper.context(), 1, 1);
        clearTopLayerAlphaPixel(helper.context(), 12, 12);
        helper.composite();
    }

    // Finish the compositing.
    src->context()->endTransparencyLayer();

    // Check that we got the right values, it should be like the rectangle was
    // drawn with half opacity even though the alpha channel got messed up.
    EXPECT_EQ(red, getPixelAt(src->context(), 0, 0));
    EXPECT_EQ(red, getPixelAt(src->context(), 1, 1));
    EXPECT_EQ(darkRed, getPixelAt(src->context(), 2, 2));

    // The dark result is:
    //   (black @ 50% atop green) @ 50% atop red = 0xFF804000
    // which is 0xFFA02000 (Skia computes 0xFFA11F00 due to rounding).
    Color darkGreenRed(0xFF803f00);
    EXPECT_EQ(darkGreenRed, getPixelAt(src->context(), 13, 13));

    // 50% green on top of red = FF808000 (rounded to what Skia will produce).
    Color greenRed(0xFF807F00);
    EXPECT_EQ(greenRed, getPixelAt(src->context(), 14, 14));
    EXPECT_EQ(greenRed, getPixelAt(src->context(), 15, 15));
}

// Tests that translations are properly handled when using KeepTransform.
TEST(TransparencyWin, TranslateOpaqueCompositeLayer)
{
    // Fill with white.
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));
    Color white(0xFFFFFFFF);
    FloatRect fullRect(0, 0, 16, 16);
    src->context()->fillRect(fullRect, white, ColorSpaceDeviceRGB);

    // Scroll down by 8 (coordinate system goes up).
    src->context()->save();
    src->context()->translate(0, -8);

    Color red(0xFFFF0000);
    Color green(0xFF00FF00);
    {
        // Make the transparency layer after translation will be @ (0, -8) with
        // size 16x16.
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::OpaqueCompositeLayer,
                    TransparencyWin::KeepTransform,
                    IntRect(0, 0, 16, 16));

        // Draw a red pixel at (15, 15). This should be the at (15, 7) after
        // the transform.
        FloatRect bottomRight(15, 15, 1, 1);
        helper.context()->fillRect(bottomRight, green, ColorSpaceDeviceRGB);
        helper.composite();
    }

    src->context()->restore();

    // Check the pixel we wrote.
    EXPECT_EQ(green, getPixelAt(src->context(), 15, 7));
}

// Same as OpaqueCompositeLayer, but the canvas has a rotation applied. This
// tests that the propert transform is applied to the copied layer.
TEST(TransparencyWin, RotateOpaqueCompositeLayer)
{
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));

    // The background is white.
    Color white(0xFFFFFFFF);
    FloatRect fullRect(0, 0, 16, 16);
    src->context()->fillRect(fullRect, white, ColorSpaceDeviceRGB);

    // Rotate the image by 90 degrees. This matrix is the same as
    // cw90.rotate(90); but avoids rounding errors. Rounding errors can cause
    // Skia to think that !rectStaysRect() and it will fall through to path
    // drawing mode, which in turn gives us antialiasing. We want no
    // antialiasing or other rounding problems since we're testing exact pixel
    // values.
    src->context()->save();
    AffineTransform cw90(0, 1, -1, 0, 0, 0);
    src->context()->concatCTM(cw90);

    // Make a transparency layer consisting of a horizontal line of 50% black.
    // Since the rotation is applied, this will actually be a vertical line
    // down the middle of the image.
    src->context()->beginTransparencyLayer(0.5);
    FloatRect blackRect(0, -9, 16, 2);
    Color black(0xFF000000);
    src->context()->fillRect(blackRect, black, ColorSpaceDeviceRGB);

    // Now draw 50% red square.
    {
        // Create a transparency helper inset one pixel in the buffer. The
        // coordinates are before transforming into this space, and maps to
        // IntRect(1, 1, 14, 14).
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::OpaqueCompositeLayer,
                    TransparencyWin::Untransform,
                    IntRect(1, -15, 14, 14));

        // Fill with red.
        helper.context()->fillRect(helper.drawRect(), Color(0x7f7f0000), ColorSpaceDeviceRGB);
        clearTopLayerAlphaChannel(helper.context());
        helper.composite();
    }

    // Finish the compositing.
    src->context()->endTransparencyLayer();

    // Top corner should be the original background.
    EXPECT_EQ(white, getPixelAt(src->context(), 0, 0));

    // Check the stripe down the middle, first at the top...
    Color gray(0xFF808080);
    EXPECT_EQ(white, getPixelAt(src->context(), 6, 0));
    EXPECT_EQ(gray, getPixelAt(src->context(), 7, 0));
    EXPECT_EQ(gray, getPixelAt(src->context(), 8, 0));
    EXPECT_EQ(white, getPixelAt(src->context(), 9, 0));

    // ...now at the bottom.
    EXPECT_EQ(white, getPixelAt(src->context(), 6, 15));
    EXPECT_EQ(gray, getPixelAt(src->context(), 7, 15));
    EXPECT_EQ(gray, getPixelAt(src->context(), 8, 15));
    EXPECT_EQ(white, getPixelAt(src->context(), 9, 15));

    // Our red square should be 25% red over the top of those two.
    Color redwhite(0xFFdfbfbf);
    Color redgray(0xFF9f8080);
    EXPECT_EQ(white, getPixelAt(src->context(), 0, 1));
    EXPECT_EQ(redwhite, getPixelAt(src->context(), 1, 1));
    EXPECT_EQ(redwhite, getPixelAt(src->context(), 6, 1));
    EXPECT_EQ(redgray, getPixelAt(src->context(), 7, 1));
    EXPECT_EQ(redgray, getPixelAt(src->context(), 8, 1));
    EXPECT_EQ(redwhite, getPixelAt(src->context(), 9, 1));
    EXPECT_EQ(redwhite, getPixelAt(src->context(), 14, 1));
    EXPECT_EQ(white, getPixelAt(src->context(), 15, 1));

    // Complete the 50% transparent layer.
    src->context()->restore();
}

TEST(TransparencyWin, TranslateScaleOpaqueCompositeLayer)
{
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));

    // The background is white on top with red on bottom.
    Color white(0xFFFFFFFF);
    FloatRect topRect(0, 0, 16, 8);
    src->context()->fillRect(topRect, white, ColorSpaceDeviceRGB);
    Color red(0xFFFF0000);
    FloatRect bottomRect(0, 8, 16, 8);
    src->context()->fillRect(bottomRect, red, ColorSpaceDeviceRGB);

    src->context()->save();

    // Translate left by one pixel.
    AffineTransform left;
    left.translate(-1, 0);

    // Scale by 2x.
    AffineTransform scale;
    scale.scale(2.0);
    src->context()->concatCTM(scale);

    // Then translate up by one pixel (which will actually be 2 due to scaling).
    AffineTransform up;
    up.translate(0, -1);
    src->context()->concatCTM(up);

    // Now draw 50% red square.
    {
        // Create a transparency helper inset one pixel in the buffer. The
        // coordinates are before transforming into this space, and maps to
        // IntRect(1, 1, 14, 14).
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::OpaqueCompositeLayer,
                    TransparencyWin::KeepTransform,
                    IntRect(1, -15, 14, 14));

        // Fill with red.
        helper.context()->fillRect(helper.drawRect(), Color(0x7f7f0000), ColorSpaceDeviceRGB);
        clearTopLayerAlphaChannel(helper.context());
        helper.composite();
    }
}

// Tests scale mode with no additional copy.
TEST(TransparencyWin, Scale)
{
    // Create an opaque white buffer.
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));
    FloatRect fullBuffer(0, 0, 16, 16);
    src->context()->fillRect(fullBuffer, Color::white, ColorSpaceDeviceRGB);

    // Scale by 2x.
    src->context()->save();
    AffineTransform scale;
    scale.scale(2.0);
    src->context()->concatCTM(scale);

    // Start drawing a rectangle from 1->4. This should get scaled to 2->8.
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::NoLayer,
                    TransparencyWin::ScaleTransform,
                    IntRect(1, 1, 3, 3));

        // The context should now have the identity transform and the returned
        // rect should be scaled.
        EXPECT_TRUE(helper.context()->getCTM().isIdentity());
        EXPECT_EQ(2, helper.drawRect().x());
        EXPECT_EQ(2, helper.drawRect().y());
        EXPECT_EQ(8, helper.drawRect().right());
        EXPECT_EQ(8, helper.drawRect().bottom());

        // Set the pixel at (2, 2) to be transparent. This should be fixed when
        // the helper goes out of scope. We don't want to call
        // clearTopLayerAlphaChannel because that will actually clear the whole
        // canvas (since we have no extra layer!).
        SkBitmap& bitmap = const_cast<SkBitmap&>(helper.context()->
            platformContext()->canvas()->getTopPlatformDevice().
            accessBitmap(false));
        *bitmap.getAddr32(2, 2) &= 0x00FFFFFF;
        helper.composite();
    }

    src->context()->restore();

    // Check the pixel we previously made transparent, it should have gotten
    // fixed back up to white.

    // The current version doesn't fixup transparency when there is no layer.
    // This seems not to be necessary, so we don't bother, but if it becomes
    // necessary, this line should be uncommented.
    // EXPECT_EQ(Color(Color::white), getPixelAt(src->context(), 2, 2));
}

// Tests scale mode with an additional copy for transparency. This will happen
// if we have a scaled textbox, for example. WebKit will create a new
// transparency layer, draw the text field, then draw the text into it, then
// composite this down with an opacity.
TEST(TransparencyWin, ScaleTransparency)
{
    // Create an opaque white buffer.
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));
    FloatRect fullBuffer(0, 0, 16, 16);
    src->context()->fillRect(fullBuffer, Color::white, ColorSpaceDeviceRGB);

    // Make another layer (which duplicates how WebKit will make this). We fill
    // the top half with red, and have the layer be 50% opaque.
    src->context()->beginTransparencyLayer(0.5);
    FloatRect topHalf(0, 0, 16, 8);
    src->context()->fillRect(topHalf, Color(0xFFFF0000), ColorSpaceDeviceRGB);

    // Scale by 2x.
    src->context()->save();
    AffineTransform scale;
    scale.scale(2.0);
    src->context()->concatCTM(scale);

    // Make a layer inset two pixels (because of scaling, this is 2->14). And
    // will it with 50% black.
    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::OpaqueCompositeLayer,
                    TransparencyWin::ScaleTransform,
                    IntRect(1, 1, 6, 6));

        helper.context()->fillRect(helper.drawRect(), Color(0x7f000000), ColorSpaceDeviceRGB);
        clearTopLayerAlphaChannel(helper.context());
        helper.composite();
    }

    // Finish the layer.
    src->context()->restore();
    src->context()->endTransparencyLayer();

    Color redBackground(0xFFFF8080); // 50% red composited on white.
    EXPECT_EQ(redBackground, getPixelAt(src->context(), 0, 0));
    EXPECT_EQ(redBackground, getPixelAt(src->context(), 1, 1));

    // Top half (minus two pixel border) should be 50% gray atop opaque
    // red = 0xFF804141. Then that's composited with 50% transparency on solid
    // white = 0xFFC0A1A1.
    Color darkRed(0xFFBF8080);
    EXPECT_EQ(darkRed, getPixelAt(src->context(), 2, 2));
    EXPECT_EQ(darkRed, getPixelAt(src->context(), 7, 7));

    // Bottom half (minus a two pixel border) should be a layer with 5% gray
    // with another 50% opacity composited atop white.
    Color darkWhite(0xFFBFBFBF);
    EXPECT_EQ(darkWhite, getPixelAt(src->context(), 8, 8));
    EXPECT_EQ(darkWhite, getPixelAt(src->context(), 13, 13));

    Color white(0xFFFFFFFF); // Background in the lower-right.
    EXPECT_EQ(white, getPixelAt(src->context(), 14, 14));
    EXPECT_EQ(white, getPixelAt(src->context(), 15, 15));
}

TEST(TransparencyWin, Text)
{
    OwnPtr<ImageBuffer> src(ImageBuffer::create(IntSize(16, 16), ColorSpaceDeviceRGB));

    // Our text should end up 50% transparent blue-green.
    Color fullResult(0x80008080);

    {
        TransparencyWin helper;
        helper.init(src->context(),
                    TransparencyWin::TextComposite,
                    TransparencyWin::KeepTransform,
                    IntRect(0, 0, 16, 16));
        helper.setTextCompositeColor(fullResult);

        // Write several different squares to simulate ClearType. These should
        // all reduce to 2/3 coverage.
        FloatRect pixel(0, 0, 1, 1);
        helper.context()->fillRect(pixel, 0xFFFF0000, ColorSpaceDeviceRGB);
        pixel.move(1.0f, 0.0f);
        helper.context()->fillRect(pixel, 0xFF00FF00, ColorSpaceDeviceRGB);
        pixel.move(1.0f, 0.0f);
        helper.context()->fillRect(pixel, 0xFF0000FF, ColorSpaceDeviceRGB);
        pixel.move(1.0f, 0.0f);
        helper.context()->fillRect(pixel, 0xFF008080, ColorSpaceDeviceRGB);
        pixel.move(1.0f, 0.0f);
        helper.context()->fillRect(pixel, 0xFF800080, ColorSpaceDeviceRGB);
        pixel.move(1.0f, 0.0f);
        helper.context()->fillRect(pixel, 0xFF808000, ColorSpaceDeviceRGB);

        // Try one with 100% coverage (opaque black).
        pixel.move(1.0f, 0.0f);
        helper.context()->fillRect(pixel, 0xFF000000, ColorSpaceDeviceRGB);

        // Now mess with the alpha channel.
        clearTopLayerAlphaChannel(helper.context());
        helper.composite();
    }

    Color oneThirdResult(0x55005555); // = fullResult * 2 / 3
    EXPECT_EQ(oneThirdResult, getPixelAt(src->context(), 0, 0));
    EXPECT_EQ(oneThirdResult, getPixelAt(src->context(), 1, 0));
    EXPECT_EQ(oneThirdResult, getPixelAt(src->context(), 2, 0));
    EXPECT_EQ(oneThirdResult, getPixelAt(src->context(), 3, 0));
    EXPECT_EQ(oneThirdResult, getPixelAt(src->context(), 4, 0));
    EXPECT_EQ(oneThirdResult, getPixelAt(src->context(), 5, 0));
    EXPECT_EQ(fullResult, getPixelAt(src->context(), 6, 0));
    EXPECT_EQ(Color::transparent, getPixelAt(src->context(), 7, 0));
}

} // namespace WebCore
