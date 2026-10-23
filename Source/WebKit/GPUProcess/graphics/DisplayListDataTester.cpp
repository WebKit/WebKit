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
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/GraphicsContext.h>
#include <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

namespace DisplayListDataTests {

class Tester {
public:
    Vector<String> takeFailures() { return WTF::move(m_failures); }

    void expect(bool condition, ASCIILiteral description)
    {
        if (!condition)
            m_failures.append(description);
    }

    void expectEqual(const String& actual, const String& expected, ASCIILiteral description)
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

static std::optional<DisplayListData> encodeAndDecode(const DisplayListData& data)
{
    IPC::Encoder encoder(IPC::MessageName::GPUProcess_RunDisplayListDataTestsForTesting, 0);
    encoder << data;
    auto decoder = IPC::Decoder::create(encoder.span(), { });
    if (!decoder)
        return std::nullopt;
    return decoder->decode<DisplayListData>();
}

static RefPtr<const DisplayList::DisplayList> encodeAndDecodeDisplayList(const DisplayList::DisplayList& displayList)
{
    auto data = DisplayListData::create(displayList);
    if (!data)
        return nullptr;
    auto decodedData = encodeAndDecode(*data);
    if (!decodedData)
        return nullptr;
    return decodedData->createDisplayList();
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
    items.append(DisplayList::ConcatenateCTM { transform });
    items.append(DisplayList::DrawDotsForDocumentMarker { rect, { DocumentMarkerLineStyleMode::Grammar, Color::green } });
    items.append(DisplayList::DrawEllipse { rect });
    items.append(DisplayList::DrawFocusRingPath { testPath(), 2, Color::blue, 1.5 });
    items.append(DisplayList::DrawFocusRingRects { Vector<FloatRect> { rect, { 5, 6, 7, 8 } }, 3, Color::red, 2 });
    items.append(DisplayList::DrawLine { { 1, 2 }, { 3, 4 } });
    items.append(DisplayList::DrawLinesForText { { 1, 2 }, lineSegments.span(), 3, true, true, StrokeStyle::DashedStroke });
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
    tester.expect(data->displayLists().size() == 2, "A display list drawn twice is encoded once"_s);

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
    tester.expect(data->displayLists().size() == 2, "A display list drawn by two placeholders is encoded once"_s);

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

static void testItemsWithResources(Tester& tester)
{
    Vector<DisplayList::Item> items;
    items.append(DisplayList::SetState { GraphicsContextState { } });
    Ref displayList = DisplayList::DisplayList::create(WTF::move(items));

    tester.expect(!DisplayListData::create(displayList), "Items that need resources fail to encode"_s);
}

static void testInvalidData(Tester& tester)
{
    tester.expect(!DisplayListData { Vector<Vector<DisplayListDataItem>> { } }.createDisplayList(), "Data without a display list fails to decode"_s);

    Vector<Vector<DisplayListDataItem>> drawingItself;
    drawingItself.append(Vector<DisplayListDataItem> { DrawDisplayListAtIndex { 0 } });
    tester.expect(!DisplayListData { WTF::move(drawingItself) }.createDisplayList(), "A display list that draws itself fails to decode"_s);

    Vector<Vector<DisplayListDataItem>> drawingLater;
    drawingLater.append(Vector<DisplayListDataItem> { DrawDisplayListAtIndex { 1 } });
    drawingLater.append(Vector<DisplayListDataItem> { DisplayList::Save { } });
    tester.expect(!DisplayListData { WTF::move(drawingLater) }.createDisplayList(), "A display list that draws a later one fails to decode"_s);

    Vector<Vector<DisplayListDataItem>> drawingMissing;
    drawingMissing.append(Vector<DisplayListDataItem> { DisplayList::Save { } });
    drawingMissing.append(Vector<DisplayListDataItem> { DrawDisplayListAtIndex { 5 } });
    tester.expect(!DisplayListData { WTF::move(drawingMissing) }.createDisplayList(), "A display list that draws a missing one fails to decode"_s);
}

} // namespace DisplayListDataTests

Vector<String> testDisplayListData()
{
    using namespace DisplayListDataTests;
    Tester tester;
    testValueItems(tester);
    testNestedDisplayLists(tester);
    testPlaceholders(tester);
    testPlaceholderDrawingItself(tester);
    testItemsWithResources(tester);
    testInvalidData(tester);
    return tester.takeFailures();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
