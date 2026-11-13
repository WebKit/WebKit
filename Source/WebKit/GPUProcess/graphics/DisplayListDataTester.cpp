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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DisplayListDataTester.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "DisplayListData.h"
#include "Encoder.h"
#include "GeneratedSerializers.h"
#include "MessageNames.h"
#include "SharedFont.h"
#include <WebCore/ButtonPart.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/FEFlood.h>
#include <WebCore/FEImage.h>
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/MeshGradient.h>
#include <WebCore/Pattern.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/SVGFilterRenderer.h>
#include <WebCore/SVGUnitTypes.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SourceGraphic.h>
#include <wtf/Borrow.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

#if ENABLE(APPLE_PAY)
#include <WebCore/ApplePayLogoSystemImage.h>
#endif

namespace WebKit {
using namespace WebCore;

namespace DisplayListDataTests {

class Tester {
public:
    Vector<String> takeFailures() { return WTF::move(m_failures); }

    void expect(bool condition, const String& description)
    {
        if (!condition)
            m_failures.append(description);
    }

    void expectEqual(const String& actual, const String& expected, const String& description)
    {
        if (actual != expected)
            m_failures.append(makeString(description, "\nexpected:\n"_s, expected, "\nactual:\n"_s, actual));
    }

private:
    Vector<String> m_failures;
};

static String displayListText(const DisplayList::DisplayList& displayList)
{
    return displayList.asText({ });
}

static std::optional<DisplayListData> encodeAndDecode(DisplayListData&& data)
{
    IPC::Encoder encoder(IPC::MessageName::LegacySessionState, 0);
    encoder << WTF::move(data);
    // A connection stores the attachments it receives in reverse order.
    auto attachments = encoder.releaseAttachments();
    attachments.reverse();
    auto decoder = IPC::Decoder::create(encoder.span(), WTF::move(attachments));
    if (!decoder)
        return std::nullopt;
    return decoder->decode<DisplayListData>();
}

static RefPtr<const DisplayList::DisplayList> encodeAndDecodeDisplayList(const DisplayList::DisplayList& displayList, bool shouldUseLockdownFontParser = false)
{
    auto data = DisplayListData::create(displayList);
    if (!data)
        return nullptr;
    auto decodedData = encodeAndDecode(WTF::move(*data));
    if (!decodedData)
        return nullptr;
    return WTF::move(*decodedData).createDisplayList(shouldUseLockdownFontParser);
}

static constexpr IntSize testImageSize { 100, 100 };

static Vector<uint8_t> drawToPixels(const DisplayList::DisplayList& displayList)
{
    RefPtr imageBuffer = ImageBuffer::create(testImageSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, ColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return { };
    imageBuffer->context().drawDisplayList(displayList);
    RefPtr pixelBuffer = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, ColorSpace::SRGB() }, { { }, testImageSize });
    if (!pixelBuffer)
        return { };
    return Vector<uint8_t>(pixelBuffer->bytes());
}

static bool isBlank(const Vector<uint8_t>& pixels)
{
    return std::ranges::all_of(pixels, [](uint8_t value) {
        return !value;
    });
}

static bool arePixelsClose(const Vector<uint8_t>& pixels, const Vector<uint8_t>& expectedPixels, uint8_t tolerance)
{
    if (pixels.size() != expectedPixels.size())
        return false;
    for (size_t i = 0; i < pixels.size(); ++i) {
        if (std::abs(pixels[i] - expectedPixels[i]) > tolerance)
            return false;
    }
    return true;
}

static RefPtr<const DisplayList::DisplayList> expectSameContents(Tester& tester, const DisplayList::DisplayList& displayList, ASCIILiteral name, bool shouldUseLockdownFontParser = false)
{
    RefPtr decoded = encodeAndDecodeDisplayList(displayList, shouldUseLockdownFontParser);
    tester.expect(!!decoded, makeString(name, " round trip"_s));
    if (decoded)
        tester.expectEqual(displayListText(*decoded), displayListText(displayList), makeString(name, " keep their contents"_s));
    return decoded;
}

static void expectSamePixels(Tester& tester, const DisplayList::DisplayList& displayList, const DisplayList::DisplayList& decoded, ASCIILiteral name, uint8_t tolerance = 0)
{
    auto expectedPixels = drawToPixels(displayList);
    tester.expect(!isBlank(expectedPixels), makeString(name, " draw something"_s));
    tester.expect(arePixelsClose(drawToPixels(decoded), expectedPixels, tolerance), makeString(name, " draw the same pixels"_s));
}

static void expectSimilarDrawing(Tester& tester, const DisplayList::DisplayList& displayList, ASCIILiteral name, uint8_t tolerance, bool shouldUseLockdownFontParser = false)
{
    RefPtr decoded = expectSameContents(tester, displayList, name, shouldUseLockdownFontParser);
    if (!decoded)
        return;
    expectSamePixels(tester, displayList, *decoded, name, tolerance);
}

static void expectSameDrawing(Tester& tester, const DisplayList::DisplayList& displayList, ASCIILiteral name, bool shouldUseLockdownFontParser = false)
{
    expectSimilarDrawing(tester, displayList, name, 0, shouldUseLockdownFontParser);
}

static Path testPath()
{
    Path path;
    path.moveTo({ 1, 2 });
    path.addLineTo({ 30, 4 });
    path.addBezierCurveTo({ 5, 60 }, { 7, 8 }, { 90, 10 });
    path.closeSubpath();
    return path;
}

static void testValueItems(Tester& tester)
{
    FloatRect rect { 1, 2, 30, 40 };
    FloatRoundedRect roundedRect { rect, { 1, 2 }, { 3, 4 }, { 5, 6 }, { 7, 8 } };
    AffineTransform transform { 1, 2, 3, 4, 5, 6 };
    Vector<FloatSegment> lineSegments { { 1, 2 }, { 5, 9 } };

    Vector<DisplayList::Item> items;
    items.append(DisplayList::ApplyDeviceScaleFactor { 2 });
    items.append(DisplayList::BeginTransparencyLayer { 0.5 });
    items.append(DisplayList::BeginTransparencyLayerWithCompositeMode { { CompositeOperator::SourceOut, BlendMode::Multiply } });
    items.append(DisplayList::ClearRect { rect });
    items.append(DisplayList::Clip { rect });
    items.append(DisplayList::ClipRoundedRect { roundedRect });
    items.append(DisplayList::ClipOut { rect });
    items.append(DisplayList::ClipOutRoundedRect { roundedRect });
    items.append(DisplayList::ClipOutToPath { testPath() });
    items.append(DisplayList::ClipPath { testPath(), WindRule::EvenOdd });
    items.append(DisplayList::ClipTransformedPath { transform, testPath(), WindRule::NonZero });
    items.append(DisplayList::ConcatenateCTM { transform });
    items.append(DisplayList::DrawDotsForDocumentMarker { rect, { DocumentMarkerLineStyleMode::Grammar, Color::green } });
    items.append(DisplayList::DrawEllipse { rect });
    items.append(DisplayList::DrawFocusRingPath { testPath(), 2, Color::blue, 1.5 });
    items.append(DisplayList::DrawFocusRingRects { Vector<FloatRect> { rect, { 5, 6, 7, 8 } }, 3, Color::red, 2 });
    items.append(DisplayList::DrawLine { { 1, 2 }, { 3, 4 } });
    items.append(DisplayList::DrawLinesForText { { 1, 2 }, borrow(lineSegments)->span(), 3, true, true, StrokeStyle::DashedStroke });
    items.append(DisplayList::DrawPath { testPath() });
    items.append(DisplayList::DrawRect { rect, 3 });
    items.constructAndAppend(WTF::InPlaceType<DisplayList::EndTransparencyLayer>);
    items.append(DisplayList::FillCompositedRect { rect, Color::cyan, CompositeOperator::Copy, BlendMode::Screen });
    items.append(DisplayList::FillEllipse { rect });
    items.append(DisplayList::FillPath { testPath() });
    items.append(DisplayList::FillRect { rect, GraphicsContext::RequiresClipToRect::No });
    items.append(DisplayList::FillRectWithColor { rect, Color::magenta });
    items.append(DisplayList::FillRectWithRoundedHole { rect, roundedRect, Color::yellow });
    items.append(DisplayList::FillRoundedRect { roundedRect, Color::black, BlendMode::Darken });
    items.constructAndAppend(WTF::InPlaceType<DisplayList::ResetClip>);
    items.constructAndAppend(WTF::InPlaceType<DisplayList::Restore>);
    items.append(DisplayList::Rotate { 0.5 });
    items.constructAndAppend(WTF::InPlaceType<DisplayList::Save>);
    items.append(DisplayList::Scale { { 2, 3 } });
    items.append(DisplayList::SetCTM { transform });
    items.append(DisplayList::SetInlineFillColor { SRGBA<uint8_t> { 1, 2, 3, 4 } });
    items.append(DisplayList::SetInlineStroke { SRGBA<uint8_t> { 5, 6, 7, 8 } });
    items.append(DisplayList::SetInlineStroke { 4.5f });
    items.append(DisplayList::SetLineCap { LineCap::Round });
    items.append(DisplayList::SetLineDash { DashArray { 1, 2, 3 }, 0.5 });
    items.append(DisplayList::SetLineJoin { LineJoin::Bevel });
    items.append(DisplayList::SetMiterLimit { 3 });
    items.append(DisplayList::StrokeEllipse { rect });
    items.append(DisplayList::StrokePath { testPath() });
    items.append(DisplayList::StrokeRect { rect, 2 });
    items.append(DisplayList::Translate { 3, 4 });
#if USE(CG)
    items.constructAndAppend(WTF::InPlaceType<DisplayList::ApplyFillPattern>);
    items.constructAndAppend(WTF::InPlaceType<DisplayList::ApplyStrokePattern>);
#endif
    items.append(DisplayList::BeginPage { rect });
    items.constructAndAppend(WTF::InPlaceType<DisplayList::EndPage>);
    items.append(DisplayList::SetURLForRect { URL { "https://webkit.org/"_s }, rect });
    Ref displayList = DisplayList::DisplayList::create(WTF::move(items));

    RefPtr decoded = encodeAndDecodeDisplayList(displayList);
    tester.expect(!!decoded, "Value items round trip"_s);
    if (!decoded)
        return;
    tester.expect(decoded->items().size() == displayList->items().size(), "Value items keep their count"_s);
    tester.expectEqual(displayListText(*decoded), displayListText(displayList), "Value items keep their contents"_s);
}

static Ref<const DisplayList::DisplayList> childDisplayList()
{
    Vector<DisplayList::Item> items;
    items.append(DisplayList::FillRectWithColor { { 1, 2, 3, 4 }, Color::red });
    return DisplayList::DisplayList::create(WTF::move(items));
}

static void testNestedDisplayLists(Tester& tester)
{
    Ref child = childDisplayList();
    Vector<DisplayList::Item> items;
    items.constructAndAppend(WTF::InPlaceType<DisplayList::Save>);
    items.append(DisplayList::DrawDisplayList { child.copyRef() });
    items.append(DisplayList::DrawDisplayList { child.copyRef() });
    items.constructAndAppend(WTF::InPlaceType<DisplayList::Restore>);
    Ref displayList = DisplayList::DisplayList::create(WTF::move(items));

    auto data = DisplayListData::create(displayList);
    tester.expect(!!data, "Nested display lists encode"_s);
    if (!data)
        return;
    tester.expect(data->bottomUpDisplayLists().size() == 2, "A display list drawn twice is encoded once"_s);

    RefPtr decoded = encodeAndDecodeDisplayList(displayList);
    tester.expect(!!decoded, "Nested display lists round trip"_s);
    if (decoded)
        tester.expectEqual(displayListText(*decoded), displayListText(displayList), "Nested display lists keep their contents"_s);
}

static void testPlaceholders(Tester& tester)
{
    Ref child = childDisplayList();
    auto drawChild = [&] {
        return [child = child.copyRef()](GraphicsContext& context) {
            context.drawDisplayList(child);
        };
    };

    Vector<DisplayList::Item> items;
    items.constructAndAppend(WTF::InPlaceType<DisplayList::Save>);
    items.append(DisplayList::DrawPlaceholder { drawChild() });
    items.append(DisplayList::DrawPlaceholder { drawChild() });
    items.append(DisplayList::DrawPlaceholder { [](GraphicsContext&) { } });
    items.constructAndAppend(WTF::InPlaceType<DisplayList::Restore>);
    Ref displayList = DisplayList::DisplayList::create(WTF::move(items));

    Vector<DisplayList::Item> expectedItems;
    expectedItems.constructAndAppend(WTF::InPlaceType<DisplayList::Save>);
    expectedItems.append(DisplayList::DrawDisplayList { child.copyRef() });
    expectedItems.append(DisplayList::DrawDisplayList { child.copyRef() });
    expectedItems.constructAndAppend(WTF::InPlaceType<DisplayList::Restore>);
    Ref expected = DisplayList::DisplayList::create(WTF::move(expectedItems));

    auto data = DisplayListData::create(displayList);
    tester.expect(!!data, "Placeholders encode"_s);
    if (!data)
        return;
    tester.expect(data->bottomUpDisplayLists().size() == 2, "A display list drawn by two placeholders is encoded once"_s);

    RefPtr decoded = encodeAndDecodeDisplayList(displayList);
    tester.expect(!!decoded, "Placeholders round trip"_s);
    if (decoded)
        tester.expectEqual(displayListText(*decoded), displayListText(expected), "Placeholders are replaced by what they draw"_s);
}

static void testPlaceholderDrawingItself(Tester& tester)
{
    RefPtr<const DisplayList::DisplayList> displayList;
    Vector<DisplayList::Item> items;
    items.append(DisplayList::DrawPlaceholder { [&displayList](GraphicsContext& context) {
        context.drawDisplayList(*displayList);
    } });
    displayList = DisplayList::DisplayList::create(WTF::move(items));

    tester.expect(!DisplayListData::create(*displayList), "A placeholder that draws its own display list fails to encode"_s);
}

#if USE(CORE_TEXT)
static constexpr float testFontSize = 30;

static DisplayList::DrawGlyphs drawGlyphs(const FontBase& font, const String& text, const FloatPoint& localAnchor)
{
    auto upconvertedCharacters = StringView(text).upconvertedCharacters();
    auto characters = upconvertedCharacters.span();
    RetainPtr ctFont = font.ctFont();
    Vector<GlyphBufferGlyph> glyphs(FillWith { }, characters.size(), 0);
    CTFontGetGlyphsForCharacters(ctFont.get(), reinterpret_cast<const UniChar*>(characters.data()), borrow(glyphs)->mutableSpan().data(), glyphs.size());
    glyphs.removeAll(0);
    Vector<GlyphBufferAdvance> advances(FillWith { }, glyphs.size(), makeGlyphBufferAdvance());
    CTFontGetAdvancesForGlyphs(ctFont.get(), kCTFontOrientationHorizontal, borrow(glyphs)->span().data(), borrow(advances)->mutableSpan().data(), glyphs.size());
    return DisplayList::DrawGlyphs { Ref { font }, WTF::move(glyphs), WTF::move(advances), localAnchor, FontSmoothingMode::Antialiased };
}

static Ref<SharedFont> createFont(RetainPtr<CTFontRef>&& ctFont, const FontCustomPlatformData* customPlatformData = nullptr)
{
    float size = CTFontGetSize(ctFont.get());
    FontPlatformData platformData { WTF::move(ctFont), size, false, false, FontOrientation::Horizontal, FontWidthVariant::RegularWidth, TextRenderingMode::Auto, { }, customPlatformData };
    return SharedFont::create(platformData, customPlatformData ? FontOrigin::Remote : FontOrigin::Local, FontIsInterstitial::No, FontVisibility::Visible, FontIsOrientationFallback::No, FontIsSmallCapsFace::No, std::nullopt);
}

static Ref<const DisplayList::DisplayList> createDisplayList(Vector<DisplayList::Item>&& items)
{
    return DisplayList::DisplayList::create(WTF::move(items));
}

static void testSystemFonts(Tester& tester)
{
    Ref font = createFont(adoptCF(CTFontCreateWithName(CFSTR("Helvetica"), testFontSize, nullptr)));
    Vector<DisplayList::Item> items;
    items.append(drawGlyphs(font, "Hello"_s, { 5, 40 }));
    items.append(drawGlyphs(font, "World"_s, { 5, 80 }));
    Ref displayList = createDisplayList(WTF::move(items));

    auto data = DisplayListData::create(displayList);
    tester.expect(!!data, "System fonts encode"_s);
    if (data) {
        tester.expect(data->fonts().size() == 1, "A font drawn twice is encoded once"_s);
        tester.expect(data->customFonts().isEmpty(), "System fonts have no font data"_s);
    }
    expectSameDrawing(tester, displayList, "System fonts"_s);
}

static RetainPtr<CTFontRef> createSystemFontWithWeight(float weight)
{
    RetainPtr font = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, testFontSize, nullptr));
    constexpr int weightAxis = 0x77676874;
    RetainPtr axis = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &weightAxis));
    RetainPtr value = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &weight));
    RetainPtr variations = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(variations.get(), axis.get(), value.get());
    RetainPtr attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(attributes.get(), kCTFontVariationAttribute, variations.get());
    RetainPtr descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    return adoptCF(CTFontCreateCopyWithAttributes(font.get(), testFontSize, nullptr, descriptor.get()));
}

static void testVariableFonts(Tester& tester)
{
    auto displayListWithWeight = [](float weight) {
        Vector<DisplayList::Item> items;
        items.append(drawGlyphs(createFont(createSystemFontWithWeight(weight)), "Weight"_s, { 5, 60 }));
        return createDisplayList(WTF::move(items));
    };
    Ref bold = displayListWithWeight(800);
    expectSameDrawing(tester, bold, "Variable fonts"_s);
    tester.expect(drawToPixels(bold) != drawToPixels(displayListWithWeight(200)), "Variable fonts draw differently with another weight"_s);
}

static RefPtr<FontCustomPlatformData> createCustomFont(SharedBuffer& data)
{
    return FontCustomPlatformData::create(data, nullString());
}

static Ref<SharedFont> createWebFont(FontCustomPlatformData& customFont, float size)
{
    return createFont(adoptCF(CTFontCreateWithFontDescriptor(customFont.fontDescriptor.get(), size, nullptr)), &customFont);
}

static void testWebFonts(Tester& tester, const Vector<Ref<SharedBuffer>>& webFonts)
{
    tester.expect(!webFonts.isEmpty(), "Web fonts are given"_s);
    for (auto& webFont : borrow(webFonts)) {
        RefPtr customFont = createCustomFont(webFont);
        tester.expect(!!customFont, "Web fonts are created"_s);
        if (!customFont)
            continue;

        Vector<DisplayList::Item> items;
        items.append(drawGlyphs(createWebFont(*customFont, 20), "AB"_s, { 5, 30 }));
        items.append(drawGlyphs(createWebFont(*customFont, 30), "AB"_s, { 5, 80 }));
        Ref displayList = createDisplayList(WTF::move(items));

        auto data = DisplayListData::create(displayList);
        tester.expect(!!data, "Web fonts encode"_s);
        if (data) {
            tester.expect(data->fonts().size() == 2, "Each web font is encoded"_s);
            tester.expect(data->customFonts().size() == 1, "Font data shared by two fonts is encoded once"_s);
        }
        expectSameDrawing(tester, displayList, "Web fonts"_s);
        expectSameDrawing(tester, displayList, "Web fonts parsed by the Lockdown Mode parser"_s, true);
    }
}

static void testInvalidFonts(Tester& tester, const Vector<Ref<SharedBuffer>>& webFonts)
{
    Ref font = createFont(adoptCF(CTFontCreateWithName(CFSTR("Helvetica"), testFontSize, nullptr)));
    Vector<DisplayList::Item> items;
    items.append(drawGlyphs(font, "Hello"_s, { 5, 40 }));
    auto data = DisplayListData::create(createDisplayList(WTF::move(items)));
    tester.expect(!!data, "Glyphs for the invalid font tests encode"_s);
    if (!data)
        return;
    auto& fontData = data->fonts()[0];
    auto& glyphs = std::get<DisplayList::DrawGlyphs>(data->bottomUpDisplayLists()[0][0]);

    auto withGlyphs = [&](DisplayList::DrawGlyphs&& item) {
        Vector<Vector<DisplayListDataItem>> displayLists;
        displayLists.append(Vector<DisplayListDataItem> { WTF::move(item) });
        return DisplayListData { { }, { fontData }, { }, { }, WTF::move(displayLists) };
    };

    auto missingFont = glyphs;
    missingFont.setFontIndex(1);
    tester.expect(!withGlyphs(WTF::move(missingFont)).createDisplayList(false), "Glyphs drawn with a missing font fail to decode"_s);

    auto missingAdvances = glyphs.sizes();
    missingAdvances.removeLast();
    DisplayList::DrawGlyphs missingAdvance { 0, Vector(glyphs.glyphs()), WTF::move(missingAdvances), glyphs.localAnchor(), glyphs.fontSmoothingMode() };
    tester.expect(!withGlyphs(WTF::move(missingAdvance)).createDisplayList(false), "Glyphs without an advance each fail to decode"_s);

    auto missingFontData = fontData;
    missingFontData.customFontIndex = 0;
    Vector<Vector<DisplayListDataItem>> displayLists;
    displayLists.append(Vector<DisplayListDataItem> { glyphs });
    tester.expect(!DisplayListData { { }, { WTF::move(missingFontData) }, { }, { }, WTF::move(displayLists) }.createDisplayList(false), "A font with missing font data fails to decode"_s);

    if (webFonts.isEmpty())
        return;
    RefPtr customFont = createCustomFont(webFonts[0]);
    if (!customFont)
        return;
    auto invalidFontData = customFont->serializedData();
    invalidFontData.fontFaceData = SharedBuffer::create(Vector<uint8_t>(FillWith { }, 64, 0xff));
    tester.expect(!DisplayListData { { WTF::move(invalidFontData) }, { }, { }, { }, { Vector<DisplayListDataItem> { DisplayList::Save { } } } }.createDisplayList(false), "Invalid font data fails to decode"_s);
}
#endif // USE(CORE_TEXT)

static Ref<Gradient> testGradient(Gradient::Data&& data)
{
    return Gradient::create(WTF::move(data), { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied }, GradientSpreadMethod::Pad, GradientColorStops { std::array { Color::red, Color::green, Color::blue } });
}

static void testGradients(Tester& tester)
{
    Vector<DisplayList::Item> items;
    items.append(DisplayList::FillRectWithGradient { FloatRect { 0, 0, 100, 50 }, testGradient(Gradient::LinearData { { 0, 0 }, { 100, 0 } }) });
    items.append(DisplayList::FillRectWithGradientAndSpaceTransform { FloatRect { 0, 50, 100, 50 }, testGradient(Gradient::RadialData { { 50, 75 }, { 50, 75 }, 0, 40, 1 }), AffineTransform::makeScale({ 1, 0.5 }), GraphicsContext::RequiresClipToRect::Yes });
    expectSameDrawing(tester, DisplayList::DisplayList::create(WTF::move(items)), "Gradients"_s);
}

static void testState(Tester& tester)
{
    GraphicsContextState state;
    state.setFillGradient(testGradient(Gradient::ConicData { { 50, 50 }, 0.5 }), AffineTransform::makeTranslation({ 2, 3 }));
    state.setFillRule(WindRule::EvenOdd);
    state.setStrokeColor(Color::blue);
    state.setStrokeThickness(4);
    state.setStrokeStyle(StrokeStyle::DottedStroke);
    state.setCompositeMode({ CompositeOperator::SourceOver, BlendMode::Multiply });
    state.setDropShadow(GraphicsDropShadow { { 3, 4 }, 2, Color::black, ShadowRadiusMode::Legacy, 0.5 });
    state.setStyle(GraphicsStyle { GraphicsGaussianBlur { { 2, 2 } } });
    state.setAlpha(0.75);
    state.setImageInterpolationQuality(InterpolationQuality::High);
    state.setTextDrawingMode({ TextDrawingMode::Fill, TextDrawingMode::Stroke });
    state.setShouldAntialias(false);
    state.setShouldSmoothFonts(false);
    state.setShouldSubpixelQuantizeFonts(false);
    state.setShadowsIgnoreTransforms(true);

    Vector<DisplayList::Item> items;
    items.append(DisplayList::SetState { state });
    items.append(DisplayList::FillRect { { 10, 10, 80, 80 }, GraphicsContext::RequiresClipToRect::Yes });
    items.append(DisplayList::StrokeRect { { 20, 20, 60, 60 }, 4 });
    expectSameDrawing(tester, DisplayList::DisplayList::create(WTF::move(items)), "Graphics states"_s);

    GraphicsContextState strokeOnly;
    strokeOnly.setStrokeGradient(testGradient(Gradient::LinearData { { 0, 0 }, { 0, 100 } }), { });
    strokeOnly.setStrokeThickness(10);
    Vector<DisplayList::Item> strokeItems;
    strokeItems.append(DisplayList::SetState { strokeOnly });
    strokeItems.append(DisplayList::StrokeEllipse { { 10, 10, 80, 80 } });
    expectSameDrawing(tester, DisplayList::DisplayList::create(WTF::move(strokeItems)), "Graphics states with only some changes"_s);
}

static void testControlParts(Tester& tester)
{
    Vector<DisplayList::Item> items;
    items.append(DisplayList::DrawControlPart { ButtonPart::create(StyleAppearance::PushButton), FloatRoundedRect { FloatRect { 10, 10, 80, 30 } }, 1, ControlStyle { } });
#if PLATFORM(IOS_FAMILY)
    // ControlFactoryIOS has no platform controls, so control parts draw nothing on iOS.
    expectSameContents(tester, DisplayList::DisplayList::create(WTF::move(items)), "Control parts"_s);
#else
    expectSameDrawing(tester, DisplayList::DisplayList::create(WTF::move(items)), "Control parts"_s);
#endif
}

static void testSystemImages(Tester& tester)
{
#if ENABLE(APPLE_PAY)
    Vector<DisplayList::Item> items;
    items.append(DisplayList::DrawSystemImage { ApplePayLogoSystemImage::create(ApplePayLogoStyle::Black), { 10, 10, 80, 40 } });
    expectSameDrawing(tester, DisplayList::DisplayList::create(WTF::move(items)), "System images"_s);
#else
    UNUSED_PARAM(tester);
#endif
}

static FilterGeometry testFilterGeometry()
{
    return { { 0, 0, 100, 100 }, { 10, 10, 60, 60 }, { 1, 1 } };
}

static Ref<Filter> filterWithEffect(Ref<FilterEffect>&& effect)
{
    return SVGFilterRenderer::create(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE, { { 0, 0, std::nullopt } }, { WTF::move(effect) }, testFilterGeometry(), FilterRenderingMode::Software, { }, std::nullopt);
}

static void testFilters(Tester& tester)
{
    Vector<DisplayList::Item> items;
    items.append(DisplayList::DrawFilteredImageBuffer { std::nullopt, testFilterGeometry().filterRegion, filterWithEffect(FEFlood::create(Color::green, 1)) });
    expectSameDrawing(tester, DisplayList::DisplayList::create(WTF::move(items)), "Filters"_s);
}

static constexpr FloatSize testImageBufferSize { 20, 20 };

static RefPtr<ImageBuffer> createImageBuffer(RenderingMode renderingMode = RenderingMode::Unaccelerated, float resolutionScale = 1)
{
    RefPtr imageBuffer = ImageBuffer::create(testImageBufferSize, renderingMode, RenderingPurpose::Unspecified, resolutionScale, ColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return nullptr;
    auto& context = imageBuffer->context();
    context.fillRect({ { }, testImageBufferSize }, Color::red);
    context.fillRect({ 0, 0, 10, 10 }, Color::blue);
    context.fillRect({ 10, 10, 10, 10 }, Color::green.colorWithAlphaByte(128));
    return imageBuffer;
}

static Ref<ImageBuffer> testImageBuffer()
{
    RefPtr imageBuffer = createImageBuffer();
    RELEASE_ASSERT(imageBuffer);
    return imageBuffer.releaseNonNull();
}

static Ref<NativeImage> testNativeImage()
{
    RefPtr image = testImageBuffer()->copyNativeImage();
    RELEASE_ASSERT(image);
    return image.releaseNonNull();
}

static Ref<const DisplayList::DisplayList> drawingImage(DisplayList::Item&& item)
{
    Vector<DisplayList::Item> items;
    items.append(WTF::move(item));
    return DisplayList::DisplayList::create(WTF::move(items));
}

static void testNativeImages(Tester& tester)
{
    Ref image = testNativeImage();
    FloatRect source { { }, image->size() };
    Vector<DisplayList::Item> items;
    items.append(DisplayList::DrawNativeImage { image, { 10, 10, 40, 40 }, source, { } });
    items.append(DisplayList::DrawNativeImage { image, { 50, 50, 30, 20 }, { 5, 5, 10, 10 }, { } });
    items.append(DisplayList::DrawPatternNativeImage { image, { 0, 60, 100, 40 }, source, AffineTransform::makeScale({ 0.5, 0.5 }), { 2, 3 }, { 1, 1 }, { } });
    Ref displayList = DisplayList::DisplayList::create(WTF::move(items));

    auto data = DisplayListData::create(displayList);
    tester.expect(!!data, "Native images encode"_s);
    if (data)
        tester.expect(data->images().size() == 1, "An image drawn three times is encoded once"_s);
    expectSameDrawing(tester, displayList, "Native images"_s);
}

#if USE(CG)
static Ref<NativeImage> createNativeImage(CFStringRef colorSpaceName, size_t bitsPerComponent, CGBitmapInfo bitmapInfo)
{
    RetainPtr colorSpace = adoptCF(CGColorSpaceCreateWithName(colorSpaceName));
    RetainPtr context = adoptCF(CGBitmapContextCreate(nullptr, 20, 20, bitsPerComponent, 0, colorSpace.get(), bitmapInfo));
    RELEASE_ASSERT(context);
    CGContextSetRGBFillColor(context.get(), 1, 0, 0, 1);
    CGContextFillRect(context.get(), CGRectMake(0, 0, 20, 20));
    CGContextSetRGBFillColor(context.get(), 0, 0.5, 1, 1);
    CGContextFillRect(context.get(), CGRectMake(0, 0, 10, 10));
    RefPtr image = NativeImage::create(adoptCF(CGBitmapContextCreateImage(context.get())));
    RELEASE_ASSERT(image);
    return image.releaseNonNull();
}

static void testNativeImageFormats(Tester& tester)
{
    auto drawingNativeImage = [](Ref<NativeImage>&& image) {
        FloatRect source { { }, image->size() };
        return drawingImage(DisplayList::DrawNativeImage { image, { 10, 10, 80, 80 }, source, { } });
    };
    expectSameDrawing(tester, drawingNativeImage(createNativeImage(kCGColorSpaceDisplayP3, 8, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host)), "Display P3 images"_s);
    expectSameDrawing(tester, drawingNativeImage(createNativeImage(kCGColorSpaceSRGB, 16, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder16Host)), "16-bit images"_s);
    expectSimilarDrawing(tester, drawingNativeImage(createNativeImage(kCGColorSpaceGenericGrayGamma2_2, 8, kCGImageAlphaNone)), "Grayscale images"_s, 2);
    expectSimilarDrawing(tester, drawingNativeImage(createNativeImage(kCGColorSpaceGenericCMYK, 8, kCGImageAlphaNone)), "CMYK images"_s, 2);
}
#endif

static void testImageBuffers(Tester& tester)
{
    Ref imageBuffer = testImageBuffer();
    Ref image = testNativeImage();
    FloatRect source { { }, testImageBufferSize };
    Vector<DisplayList::Item> items;
    items.append(DisplayList::DrawImageBuffer { imageBuffer, { 10, 10, 40, 40 }, source, { } });
    items.append(DisplayList::DrawPatternImageBuffer { imageBuffer, { 0, 60, 100, 40 }, source, AffineTransform::makeScale({ 0.5, 0.5 }), { 2, 3 }, { 1, 1 }, { } });
    items.constructAndAppend(WTF::InPlaceType<DisplayList::Save>);
    items.append(DisplayList::ClipToNativeImage { image, { 50, 10, 40, 40 } });
    items.append(DisplayList::FillRectWithColor { { 50, 10, 40, 40 }, Color::black });
    items.constructAndAppend(WTF::InPlaceType<DisplayList::Restore>);
    items.append(DisplayList::DrawNativeImage { image, { 60, 60, 20, 20 }, source, { } });
    Ref displayList = DisplayList::DisplayList::create(WTF::move(items));

    auto data = DisplayListData::create(displayList);
    tester.expect(!!data, "Image buffers encode"_s);
    if (data)
        tester.expect(data->images().size() == 2, "Images used twice are encoded once"_s);
    expectSameDrawing(tester, displayList, "Image buffers"_s);

    RefPtr scaledImageBuffer = createImageBuffer(RenderingMode::Unaccelerated, 2);
    tester.expect(!!scaledImageBuffer, "Scaled image buffers are created"_s);
    if (scaledImageBuffer)
        expectSameDrawing(tester, drawingImage(DisplayList::DrawImageBuffer { *scaledImageBuffer, { 10, 10, 80, 80 }, source, { } }), "Scaled image buffers"_s);

    RefPtr acceleratedImageBuffer = createImageBuffer(RenderingMode::Accelerated, 2);
    tester.expect(!!acceleratedImageBuffer, "Accelerated image buffers are created"_s);
    if (acceleratedImageBuffer)
        expectSameDrawing(tester, drawingImage(DisplayList::DrawImageBuffer { *acceleratedImageBuffer, { 10, 10, 80, 80 }, source, { } }), "Accelerated image buffers"_s);
}

static void testPatternBrushes(Tester& tester)
{
    GraphicsContextState state;
    state.setFillPattern(WebCore::Pattern::create(SourceImage { testNativeImage() }, { true, true, AffineTransform::makeScale({ 0.5, 0.5 }) }));
    state.setStrokePattern(WebCore::Pattern::create(SourceImage { testImageBuffer() }));
    state.setStrokeThickness(10);

    Vector<DisplayList::Item> items;
    items.append(DisplayList::SetState { state });
    items.append(DisplayList::FillRect { { 10, 10, 40, 80 }, GraphicsContext::RequiresClipToRect::Yes });
    items.append(DisplayList::StrokeRect { { 60, 10, 30, 80 }, 10 });
    Ref displayList = DisplayList::DisplayList::create(WTF::move(items));

    RefPtr decoded = encodeAndDecodeDisplayList(displayList);
    tester.expect(!!decoded, "Pattern brushes round trip"_s);
    if (decoded)
        expectSamePixels(tester, displayList, *decoded, "Pattern brushes"_s);
}

static void testFiltersWithImages(Tester& tester)
{
    FloatRect filterRegion = testFilterGeometry().filterRegion;
    expectSameDrawing(tester, drawingImage(DisplayList::DrawFilteredImageBuffer { testImageBuffer(), filterRegion, filterWithEffect(SourceGraphic::create()) }), "Filters with a source image"_s);
    expectSameDrawing(tester, drawingImage(DisplayList::DrawFilteredImageBuffer { std::nullopt, filterRegion, filterWithEffect(FEImage::create(SourceImage { testNativeImage() }, filterRegion)) }), "Filters with a native image"_s);
    expectSameDrawing(tester, drawingImage(DisplayList::DrawFilteredImageBuffer { std::nullopt, filterRegion, filterWithEffect(FEImage::create(SourceImage { testImageBuffer() }, filterRegion)) }), "Filters with an image buffer"_s);
}

static void testMeshGradients(Tester& tester)
{
    Vector<MeshGradient::Point> points {
        { { 0, 0 }, Color::red, { } },
        { { 100, 0 }, Color::green, { } },
        { { 0, 100 }, Color::blue, { } },
        { { 100, 100 }, Color::yellow, { } },
    };
    Ref gradient = MeshGradient::create({ ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied }, 2, 2, WTF::move(points), Color::cyan);
    expectSameDrawing(tester, drawingImage(DisplayList::DrawMeshGradient { WTF::move(gradient), { 10, 10, 80, 80 } }), "Mesh gradients"_s);
}

class RecordingPDFDocument final : public DisplayList::PDFDocumentData {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RecordingPDFDocument);
public:
    static Ref<RecordingPDFDocument> create(Ref<SharedBuffer>&& data)
    {
        return adoptRef(*new RecordingPDFDocument(WTF::move(data)));
    }

    const Vector<unsigned>& drawnPages() const LIFETIME_BOUND { return m_drawnPages; }

private:
    explicit RecordingPDFDocument(Ref<SharedBuffer>&& data)
        : PDFDocumentData(WTF::move(data))
    {
    }

    void drawPage(GraphicsContext& context, unsigned pageIndex, const FloatRect& pageRect) final
    {
        m_drawnPages.append(pageIndex);
        context.fillRect(pageRect, Color::blue);
    }

    Vector<unsigned> m_drawnPages;
};

static Ref<SharedBuffer> testPDFData()
{
    return SharedBuffer::create(Vector<uint8_t> { '%', 'P', 'D', 'F', '-', '1', '.', '4' });
}

static void testPDFPages(Tester& tester)
{
    Ref document = RecordingPDFDocument::create(testPDFData());
    FloatRect firstPageRect { 10, 10, 40, 40 };
    FloatRect secondPageRect { 50, 50, 40, 40 };
    Vector<DisplayList::Item> items;
    items.append(DisplayList::DrawPDFPage { document.copyRef(), 0, firstPageRect, testNativeImage() });
    items.append(DisplayList::DrawPDFPage { document.copyRef(), 1, secondPageRect, testNativeImage() });
    Ref displayList = DisplayList::DisplayList::create(WTF::move(items));

    tester.expect(!isBlank(drawToPixels(displayList)), "PDF pages draw their fallback image in the GPU process"_s);
    tester.expect(document->drawnPages().isEmpty(), "The GPU process does not draw PDF pages from the PDF data"_s);

    auto data = DisplayListData::create(displayList);
    tester.expect(!!data, "PDF pages encode"_s);
    if (!data)
        return;
    tester.expect(data->pdfDocuments().size() == 1, "A PDF drawn on two pages is encoded once"_s);

    auto decodedData = encodeAndDecode(WTF::move(*data));
    tester.expect(!!decodedData, "PDF pages round trip"_s);
    if (!decodedData)
        return;

    RefPtr<RecordingPDFDocument> decodedDocument;
    RefPtr decoded = WTF::move(*decodedData).createDisplayList(false, [&](Ref<SharedBuffer>&& data) -> Ref<DisplayList::PDFDocumentData> {
        Ref document = RecordingPDFDocument::create(WTF::move(data));
        decodedDocument = document.ptr();
        return document;
    });
    tester.expect(!!decoded, "PDF pages decode"_s);
    if (!decoded || !decodedDocument)
        return;
    tester.expectEqual(displayListText(*decoded), displayListText(displayList), "PDF pages"_s);
    Ref decodedPDFData = decodedDocument->data();
    Ref pdfData = document->data();
    tester.expect(decodedPDFData.get() == pdfData.get(), "PDF data round trips"_s);

    drawToPixels(*decoded);
    tester.expect(decodedDocument->drawnPages() == Vector<unsigned> { 0, 1 }, "The receiver draws PDF pages from the PDF data, without the fallback image"_s);
}

static void testInvalidPDFPages(Tester& tester)
{
    auto decode = [](Vector<DisplayListDataPDFDocument>&& documents, DisplayListDataItem&& item) {
        Vector<Vector<DisplayListDataItem>> displayLists;
        displayLists.append(Vector<DisplayListDataItem> { WTF::move(item) });
        return DisplayListData { { }, { }, { }, WTF::move(documents), WTF::move(displayLists) }.createDisplayList(false);
    };

    auto identifier = RenderingResourceIdentifier::generate();
    FloatRect rect { 0, 0, 10, 10 };
    Vector<DisplayListDataPDFDocument> documents;
    documents.append(DisplayListDataPDFDocument { identifier, testPDFData() });
    tester.expect(!!decode(WTF::move(documents), DisplayList::DrawPDFPage { identifier, 0, rect }), "A PDF document identifier decodes"_s);
    tester.expect(!decode({ }, DisplayList::DrawPDFPage { identifier, 0, rect }), "A missing PDF document fails to decode"_s);

    Vector<DisplayListDataPDFDocument> duplicates;
    duplicates.append(DisplayListDataPDFDocument { identifier, testPDFData() });
    duplicates.append(DisplayListDataPDFDocument { identifier, testPDFData() });
    tester.expect(!decode(WTF::move(duplicates), DisplayList::DrawPDFPage { identifier, 0, rect }), "PDF documents with the same identifier fail to decode"_s);

    tester.expect(!DisplayList::isResolved(DisplayList::DrawPDFPage { identifier, 0, rect }), "A PDF document identifier is unresolved"_s);
}

static DisplayListDataImage testImage(RenderingResourceIdentifier identifier, std::optional<ImageBufferParameters>&& imageBufferParameters = std::nullopt)
{
    RefPtr bitmap = ShareableBitmap::create({ IntSize { 10, 10 }, ColorSpace::SRGB() });
    RELEASE_ASSERT(bitmap);
    auto handle = bitmap->createReadOnlyHandle();
    RELEASE_ASSERT(handle);
    return DisplayListDataImage { identifier, WTF::move(*handle), WTF::move(imageBufferParameters) };
}

static void testInvalidImages(Tester& tester)
{
    auto decodeImages = [](Vector<DisplayListDataImage>&& images, DisplayListDataItem&& item) {
        Vector<Vector<DisplayListDataItem>> displayLists;
        displayLists.append(Vector<DisplayListDataItem> { WTF::move(item) });
        return DisplayListData { { }, { }, WTF::move(images), { }, WTF::move(displayLists) }.createDisplayList(false);
    };
    auto decode = [&](DisplayListDataImage&& image, DisplayListDataItem&& item) {
        Vector<DisplayListDataImage> images;
        images.append(WTF::move(image));
        return decodeImages(WTF::move(images), WTF::move(item));
    };

    auto identifier = RenderingResourceIdentifier::generate();
    auto missingIdentifier = RenderingResourceIdentifier::generate();
    FloatRect rect { 0, 0, 10, 10 };
    ImageBufferParameters parameters { { 10, 10 }, 1, ColorSpace::SRGB(), { PixelFormat::BGRA8 }, RenderingPurpose::Unspecified };
    tester.expect(!!decode(testImage(identifier), DisplayList::DrawNativeImage { identifier, rect, rect, { } }), "A native image identifier decodes"_s);
    tester.expect(!!decode(testImage(identifier, parameters), DisplayList::DrawImageBuffer { identifier, rect, rect, { } }), "An image buffer identifier decodes"_s);
    tester.expect(!decode(testImage(identifier), DisplayList::DrawNativeImage { missingIdentifier, rect, rect, { } }), "A missing native image fails to decode"_s);
    tester.expect(!decode(testImage(identifier, parameters), DisplayList::DrawNativeImage { identifier, rect, rect, { } }), "An image buffer drawn as a native image fails to decode"_s);
    tester.expect(!decode(testImage(identifier), DisplayList::DrawImageBuffer { identifier, rect, rect, { } }), "A native image drawn as an image buffer fails to decode"_s);
    tester.expect(!decode(testImage(identifier, parameters), DisplayList::ClipToNativeImage { identifier, rect }), "An image buffer used as a mask fails to decode"_s);

    Vector<DisplayListDataImage> duplicates;
    duplicates.append(testImage(identifier));
    duplicates.append(testImage(identifier, parameters));
    tester.expect(!decodeImages(WTF::move(duplicates), DisplayList::DrawNativeImage { identifier, rect, rect, { } }), "Images with the same identifier fail to decode"_s);

    auto wrongSize = parameters;
    wrongSize.resolutionScale = 2;
    tester.expect(!decode(testImage(identifier, wrongSize), DisplayList::DrawImageBuffer { identifier, rect, rect, { } }), "An image buffer whose pixels have the wrong size fails to decode"_s);

    tester.expect(!decode(testImage(identifier, parameters), DisplayList::DrawFilteredImageBuffer { missingIdentifier, rect, filterWithEffect(FEFlood::create(Color::green, 1)) }), "A missing filter source image fails to decode"_s);
    tester.expect(!decode(testImage(identifier), DisplayList::DrawFilteredImageBuffer { std::nullopt, rect, filterWithEffect(FEImage::create(SourceImage { missingIdentifier }, rect)) }), "An FEImage with a missing image fails to decode"_s);

    GraphicsContextState state;
    state.setFillPattern(WebCore::Pattern::create(SourceImage { missingIdentifier }));
    tester.expect(!decode(testImage(identifier), DisplayList::SetState { state }), "A pattern with a missing image fails to decode"_s);

    tester.expect(!DisplayList::isResolved(DisplayList::DrawNativeImage { identifier, rect, rect, { } }), "A native image identifier is unresolved"_s);
    tester.expect(!DisplayList::isResolved(DisplayList::DrawFilteredImageBuffer { identifier, rect, filterWithEffect(FEFlood::create(Color::green, 1)) }), "A filter source image identifier is unresolved"_s);
    tester.expect(!DisplayList::isResolved(DisplayList::SetState { state }), "A pattern image identifier is unresolved"_s);
}

static void testInvalidData(Tester& tester)
{
    tester.expect(!DisplayListData { { }, { }, { }, { }, { } }.createDisplayList(false), "Data without a display list fails to decode"_s);

    Vector<Vector<DisplayListDataItem>> drawingItself;
    drawingItself.append(Vector<DisplayListDataItem> { DisplayList::DrawDisplayList { 0 } });
    tester.expect(!DisplayListData { { }, { }, { }, { }, WTF::move(drawingItself) }.createDisplayList(false), "A display list that draws itself fails to decode"_s);

    Vector<Vector<DisplayListDataItem>> drawingLater;
    drawingLater.append(Vector<DisplayListDataItem> { DisplayList::DrawDisplayList { 1 } });
    drawingLater.append(Vector<DisplayListDataItem> { DisplayList::Save { } });
    tester.expect(!DisplayListData { { }, { }, { }, { }, WTF::move(drawingLater) }.createDisplayList(false), "A display list that draws a later one fails to decode"_s);

    Vector<Vector<DisplayListDataItem>> drawingMissing;
    drawingMissing.append(Vector<DisplayListDataItem> { DisplayList::Save { } });
    drawingMissing.append(Vector<DisplayListDataItem> { DisplayList::DrawDisplayList { 5 } });
    tester.expect(!DisplayListData { { }, { }, { }, { }, WTF::move(drawingMissing) }.createDisplayList(false), "A display list that draws a missing one fails to decode"_s);


    tester.expect(!DisplayList::isResolved(DisplayList::DrawDisplayList { 0 }), "A display list index is unresolved"_s);
    tester.expect(!DisplayList::isResolved(DisplayList::DrawGlyphs { 0, { }, { }, { }, FontSmoothingMode::Auto }), "A font index is unresolved"_s);
}

} // namespace DisplayListDataTests

Vector<String> testDisplayListData(const Vector<Ref<SharedBuffer>>& webFonts)
{
    using namespace DisplayListDataTests;
    Tester tester;
    testValueItems(tester);
    testNestedDisplayLists(tester);
    testPlaceholders(tester);
    testPlaceholderDrawingItself(tester);
#if USE(CORE_TEXT)
    testSystemFonts(tester);
    testVariableFonts(tester);
    testWebFonts(tester, webFonts);
    testInvalidFonts(tester, webFonts);
#else
    UNUSED_PARAM(webFonts);
#endif
    testGradients(tester);
    testState(tester);
    testControlParts(tester);
    testSystemImages(tester);
    testFilters(tester);
    testNativeImages(tester);
#if USE(CG)
    testNativeImageFormats(tester);
#endif
    testImageBuffers(tester);
    testPatternBrushes(tester);
    testFiltersWithImages(tester);
    testMeshGradients(tester);
    testInvalidImages(tester);
    testPDFPages(tester);
    testInvalidPDFPages(tester);
    testInvalidData(tester);
    return tester.takeFailures();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
