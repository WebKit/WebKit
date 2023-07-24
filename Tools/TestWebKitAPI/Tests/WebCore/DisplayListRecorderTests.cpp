/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WebCoreTestUtilities.h"
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/DisplayListDrawingContext.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/InMemoryDisplayList.h>
#include <wtf/MathExtras.h>
#if PLATFORM(COCOA)
#include <WebCore/GraphicsContextCG.h>
#endif

#if ENABLE(APPLE_PAY)
#include <WebCore/ApplePayLogoSystemImage.h>
#endif

#if ENABLE(VIDEO)
#include <WebCore/VideoFrame.h>
#endif

namespace TestWebKitAPI {

constexpr unsigned testContextWidth = 77;
constexpr unsigned testContextHeight = 88;

static RefPtr<WebCore::ImageBuffer> createReferenceTarget()
{
    auto colorSpace = WebCore::DestinationColorSpace::SRGB();
    auto pixelFormat = WebCore::PixelFormat::BGRA8;
    WebCore::FloatSize logicalSize { testContextWidth, testContextHeight };
    float scale = 1;
    return WebCore::ImageBuffer::create(logicalSize, WebCore::RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
}

static WebCore::Path createTestPath()
{
    WebCore::Path p;
    p.addLineTo({ 8, 9 });
    p.addLineTo({ 9, 0 });
    p.closeSubpath();
    return p;
}

static Ref<WebCore::ImageBuffer> createTestImageBuffer()
{
    auto colorSpace = WebCore::DestinationColorSpace::SRGB();
    auto pixelFormat = WebCore::PixelFormat::BGRA8;
    WebCore::FloatSize logicalSize { 3, 7 };
    float scale = 1;
    auto result = WebCore::ImageBuffer::create(logicalSize, WebCore::RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RELEASE_ASSERT(result);
    return result.releaseNonNull();
}

namespace {

class TestDrawingContext : public WebCore::DisplayList::DrawingContext {
public:
    TestDrawingContext(WebCore::FloatSize logicalSize)
        : WebCore::DisplayList::DrawingContext { logicalSize }
        , m_writingClient { makeUnique<WebCore::DisplayList::InMemoryDisplayList::WritingClient>() }
        , m_readingClient { makeUnique<WebCore::DisplayList::InMemoryDisplayList::ReadingClient>() }
    {
        displayList().setItemBufferWritingClient(m_writingClient.get());
        displayList().setItemBufferReadingClient(m_readingClient.get());
    }

private:
    std::unique_ptr<WebCore::DisplayList::InMemoryDisplayList::WritingClient> m_writingClient;
    std::unique_ptr<WebCore::DisplayList::InMemoryDisplayList::ReadingClient> m_readingClient;
};

}

static std::unique_ptr<TestDrawingContext> createDisplayListTarget()
{
    return makeUnique<TestDrawingContext>(WebCore::FloatSize { testContextWidth, testContextHeight });
}

// Function that applies same functor to two contexts. BifurbicatedGraphicsContext seems to have missing features.
template<typename F>
void forBoth(WebCore::GraphicsContext& a, WebCore::GraphicsContext& b, F&& func)
{
    func(a);
    func(b);
}

#define FOR_EVERY_GRAPHICS_CONTEXT_STATE(MACRO) \
    MACRO(fillBrush) \
    MACRO(fillRule) \
    MACRO(strokeBrush) \
    MACRO(strokeThickness) \
    MACRO(strokeStyle) \
    MACRO(dropShadow) \
    MACRO(style) \
    MACRO(compositeMode) \
    MACRO(alpha) \
    MACRO(textDrawingMode) \
    MACRO(imageInterpolationQuality) \
    MACRO(shouldAntialias) \
    MACRO(shouldSmoothFonts) \
    MACRO(shouldSubpixelQuantizeFonts) \
    MACRO(shadowsIgnoreTransforms) \
    MACRO(drawLuminanceMask) \
    MACRO(isInTransparencyLayer) \
    MACRO(scaleFactor)

// FIXME: Currently using ImageBuffer, which applies custom CTM over the default.
//        MACRO(getCTM)
// FIXME: Move away from FOR_ macro because it doesn't survive ifdefs
//        MACRO(useDarkAppearance)

static testing::AssertionResult checkEqualState(WebCore::GraphicsContext& a, WebCore::GraphicsContext& b)
{
#define CHECK_STATE(STATE) \
    if (a.STATE() != b.STATE()) \
        return testing::AssertionFailure() << "State " #STATE " does not match.";
    FOR_EVERY_GRAPHICS_CONTEXT_STATE(CHECK_STATE);
#undef CHECK_STATE
    if (a.clipBounds() != b.clipBounds()) {
        return testing::AssertionFailure() << "GraphicsContext::clipBounds() does not match. Expected:" << a.clipBounds() << " got: " << b.clipBounds();
    }
    return testing::AssertionSuccess();
}

namespace {

template<typename Operation>
class DisplayListRecorderResultStateTest : public testing::Test {
protected:
    Operation operation() { return Operation { }; }
    String operationDescription() { return Operation::description(); }
};

struct NoCommands {
    void operator()(WebCore::GraphicsContext& c)
    {
    }

    static String description()
    {
        return ""_s;
    }
};

struct ChangeAntialias {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.setShouldAntialias(false);
    }

    static String description()
    {
        return R"DL(
(set-state
  (change-flags [should-antialias])
  (should-antialias 0)))DL"_s;
    }
};

struct ChangeAntialiasBeforeSave {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.setShouldAntialias(false);
        c.save(); // This is being tested. The previous antialias == false should be restored on matching restore().
        c.fillRect({ 0, 0, 5.5f, 5.7f });
        c.restore();
    }

    static String description()
    {
        return R"DL(
(set-state
  (change-flags [should-antialias])
  (should-antialias 0))
(save)
(fill-rect
  (rect at (0,0) size 5.50x5.70))
(restore))DL"_s;
    }
};

struct ChangeAntialiasBeforeAndAfterSave {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.setShouldAntialias(false);
        c.save(); // This is being tested. The previous antialias == false should be restored on matching restore().
        c.setShouldAntialias(true);
        c.fillRect({ 0, 0, 5.5f, 5.7f });
        c.restore();
    }

    static String description()
    {
        return R"DL(
(set-state
  (change-flags [should-antialias])
  (should-antialias 0))
(save)
(set-state
  (change-flags [should-antialias])
  (should-antialias 1))
(fill-rect
  (rect at (0,0) size 5.50x5.70))
(restore))DL"_s;
    }
};

struct ChangeAntialiasInEmptySaveRestore {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.save();
        c.setShouldAntialias(false);
        c.restore(); // This is being tested. The previous antialias == false should be restored on restore().
    }

    static String description()
    {
        return R"DL(
(save)
(set-state
  (change-flags [should-antialias])
  (should-antialias 0))
(restore))DL"_s;
    }
};

struct DrawSystemImage {
    void operator()(WebCore::GraphicsContext& c)
    {
#if ENABLE(APPLE_PAY)
        auto image = WebCore::ApplePayLogoSystemImage::create(WebCore::ApplePayLogoStyle::White);
        c.setShouldAntialias(false);
        c.drawSystemImage(image.get(), {0, 0, 5, 5}); // This is being tested. The previous antialias == false should be used.
#endif
    }

    static String description()
    {
#if ENABLE(APPLE_PAY)
        return R"DL(
(set-state
  (change-flags [should-antialias])
  (should-antialias 0))
(draw-system-image
  (destination at (0,0) size 5x5)))DL"_s;
#else
        return ""_s;
#endif
    }
};

struct ResetClipRect {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.resetClip();
    }

    static String description()
    {
        return R"DL(
(reset-clip)
(clip
  (rect at (0,0) size 77x88)))DL"_s;
    }
};

struct ChangeAntialiasBeforeClipRect {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.setShouldAntialias(false);
        c.clip({ 0,0, 9.f, 7.7f });
    }

    static String description()
    {
        return R"DL(
(set-state
  (change-flags [should-antialias])
  (should-antialias 0))
(clip
  (rect at (0,0) size 9x7.70)))DL"_s;
    }
};

struct ChangeAntialiasBeforeClipOutRect {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.setShouldAntialias(false);
        c.clipOut({ 0,0, 9.f, 7.7f });
    }

    static String description()
    {
        return R"DL(
(set-state
  (change-flags [should-antialias])
  (should-antialias 0))
(clip-out
  (rect at (0,0) size 9x7.70)))DL"_s;
    }
};

struct ChangeAntialiasBeforeClipOutPath {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.setShouldAntialias(false);
        c.clipOut(createTestPath());
    }

    static String description()
    {
        return R"DL(
(set-state
  (change-flags [should-antialias])
  (should-antialias 0))
(clip-out-to-path
  (path move to (0,0), add line to (8,9), add line to (9,0), close subpath)))DL"_s;
    }
};

struct ChangeAntialiasBeforeClipPath {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.setShouldAntialias(false);
        c.clipPath(createTestPath(), WebCore::WindRule::NonZero);
    }

    static String description()
    {
        return R"DL(
(set-state
  (change-flags [should-antialias])
  (should-antialias 0))
(clip-path
  (path move to (0,0), add line to (8,9), add line to (9,0), close subpath)
  (wind-rule NON-ZERO)))DL"_s;
    }
};

struct ChangeAntialiasBeforeClipToImageBuffer {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.setShouldAntialias(false);
        c.clipToImageBuffer(createTestImageBuffer().get(), { 0, 1, 77, 99 });
    }

    static String description()
    {
        return R"DL(
(set-state
  (change-flags [should-antialias])
  (should-antialias 0))
(clip-to-image-buffer
  (dest-rect at (0,1) size 77x99)))DL"_s;
    }
};

struct TrivialTranslate {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.translate(1, 0);
        c.translate(0, 0);
        c.translate(0, 2);
        c.drawRect({ 0, 0, 1, 1 });
    }

    static String description()
    {
        return R"DL(
(translate
  (x 1.00)
  (y 0.00))
(translate
  (x 0.00)
  (y 2.00))
(draw-rect
  (rect at (0,0) size 1x1)
  (border-thickness 1.00)))DL"_s;
    }
};
struct TrivialScale {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.scale({ 1.1f, 1.2f });
        c.scale({ 1.f, 1.f });
        c.scale({ .1f, .7f });
        c.drawRect({ 0, 0, 1, 1 });
    }

    static String description()
    {
        return R"DL(
(scale
  (size width=1.10 height=1.20))
(scale
  (size width=0.10 height=0.70))
(draw-rect
  (rect at (0,0) size 1x1)
  (border-thickness 1.00)))DL"_s;
    }
};
struct TrivialRotate {
    void operator()(WebCore::GraphicsContext& c)
    {
        c.rotate(1.f);
        c.rotate(0.f);
        c.rotate(2.f);
        c.rotate(piFloat * 2.f);
        c.rotate(7.f * piFloat * 2.f);
        c.rotate(-2.f * piFloat * 2.f);
        c.drawRect({ 0, 0, 1, 1 });
    }

    static String description()
    {
        return R"DL(
(rotate
  (angle 1.00))
(rotate
  (angle 2.00))
(rotate
  (angle 43.98))
(draw-rect
  (rect at (0,0) size 1x1)
  (border-thickness 1.00)))DL"_s;
    }
};

using AllOperations = testing::Types<NoCommands, ChangeAntialias, ChangeAntialiasBeforeSave,
    ChangeAntialiasBeforeAndAfterSave, ChangeAntialiasInEmptySaveRestore, DrawSystemImage, ChangeAntialiasBeforeClipRect,
    ChangeAntialiasBeforeClipOutRect, ChangeAntialiasBeforeClipOutPath, ChangeAntialiasBeforeClipPath,
    ChangeAntialiasBeforeClipToImageBuffer, TrivialTranslate, TrivialScale, TrivialRotate, ResetClipRect>;

}

TYPED_TEST_SUITE_P(DisplayListRecorderResultStateTest);

TYPED_TEST_P(DisplayListRecorderResultStateTest, StateThroughDisplayListIsPreserved)
{
    auto refTarget = createReferenceTarget();
    auto& ref = refTarget->context();
    auto testedTarget = createDisplayListTarget();
    auto& tested = testedTarget->recorder();
    EXPECT_TRUE(checkEqualState(ref, tested));

    forBoth(ref, tested, this->operation());

    EXPECT_TRUE(checkEqualState(ref, tested));

    tested.commitRecording();

    auto resultTarget = createReferenceTarget();
    auto& result = resultTarget->context();

    auto description = testedTarget->displayList().asText({ WebCore::DisplayList::AsTextFlag::IncludePlatformOperations }).trim(deprecatedIsSpaceOrNewline);
    auto expectedDescription = this->operationDescription().trim(deprecatedIsSpaceOrNewline);
    EXPECT_EQ(expectedDescription, description);

    testedTarget->replayDisplayList(result);
    EXPECT_TRUE(checkEqualState(ref, result));

    // Unwind the stack in case the test-case tested state middle of the save/restore pair.
    // FIXME: It should be ok to destroy GraphicsContext in the middle of save / restore.
    while (ref.stackSize()) {
        forBoth(ref, tested, [](WebCore::GraphicsContext& c) {
            c.restore();
        });
    }
    while (result.stackSize())
        result.restore();
}

REGISTER_TYPED_TEST_SUITE_P(DisplayListRecorderResultStateTest, StateThroughDisplayListIsPreserved);

INSTANTIATE_TYPED_TEST_SUITE_P(DisplayListRecorderTest, DisplayListRecorderResultStateTest, AllOperations);

}
