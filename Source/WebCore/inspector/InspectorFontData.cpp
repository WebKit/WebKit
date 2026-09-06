/*
 * Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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
#include "InspectorFontData.h"

#include "BidiResolver.h"
#include "Document.h"
#include "DocumentView.h"
#include "Font.h"
#include "FontCascade.h"
#include "FontCascadeInlines.h"
#include "FontCustomPlatformData.h"
#include "FontPlatformData.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "LocalFrameView.h"
#include "LocalFrameViewLayoutContext.h"
#include "NodeInlines.h"
#include "RenderObject.h"
#include "StyleComputedStyleBase+GettersInlines.h"
#include "TextRun.h"
#include "TextRunIterator.h"
#include <algorithm>
#include <unicode/utf16.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

struct InspectorFontMetadata {
    String displayName;
    String url;
    bool synthesizedBold { false };
    bool synthesizedOblique { false };
    Vector<FontPlatformData::FontVariationAxis> variationAxes;

    friend bool operator==(const InspectorFontMetadata&, const InspectorFontMetadata&) = default;
};

using CharacterSet = HashSet<char32_t, DefaultHash<char32_t>, WTF::UnsignedWithZeroKeyHashTraits<char32_t>>;

class RenderedFont final : public RefCounted<RenderedFont> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RenderedFont);
public:
    static Ref<RenderedFont> create(InspectorFontMetadata&& metadata)
    {
        return adoptRef(*new RenderedFont(WTF::move(metadata)));
    }

    const InspectorFontMetadata& metadata() const { return m_metadata; }

    const CharacterSet& characters() const { return m_characters; }
    void addCharacter(char32_t character) { m_characters.add(character); }

private:
    explicit RenderedFont(InspectorFontMetadata&& metadata)
        : m_metadata(WTF::move(metadata))
    {
    }

    const InspectorFontMetadata m_metadata;
    CharacterSet m_characters;
};

using RenderedFonts = HashMap<Ref<const Font>, Ref<RenderedFont>>;

static InspectorFontMetadata metadataForFont(const Font& font)
{
    const auto& platformData = font.platformData();
    InspectorFontMetadata metadata {
        platformData.familyName(),
        { },
        platformData.syntheticBold(),
        platformData.syntheticOblique(),
        platformData.variationAxes(ShouldLocalizeAxisNames::Yes),
    };

    if (RefPtr customPlatformData = platformData.customPlatformData())
        metadata.url = customPlatformData->sourceURL();

    return metadata;
}

static Ref<Inspector::Protocol::CSS::Font> buildObjectForFont(const InspectorFontMetadata& metadata)
{
    auto result = Inspector::Protocol::CSS::Font::create()
        .setDisplayName(metadata.displayName)
        .release();

    if (!metadata.url.isEmpty())
        result->setUrl(metadata.url);

    auto variationAxes = JSON::ArrayOf<Inspector::Protocol::CSS::FontVariationAxis>::create();
    for (const auto& variationAxis : metadata.variationAxes) {
        auto axis = Inspector::Protocol::CSS::FontVariationAxis::create()
            .setTag(variationAxis.tag())
            .setMinimumValue(variationAxis.minimumValue())
            .setMaximumValue(variationAxis.maximumValue())
            .setDefaultValue(variationAxis.defaultValue())
            .release();

        if (!variationAxis.name().isEmpty() && variationAxis.name() != variationAxis.tag())
            axis->setName(variationAxis.name());

        variationAxes->addItem(WTF::move(axis));
    }
    if (variationAxes->length())
        result->setVariationAxes(WTF::move(variationAxes));

    if (metadata.synthesizedBold)
        result->setSynthesizedBold(metadata.synthesizedBold);

    if (metadata.synthesizedOblique)
        result->setSynthesizedOblique(metadata.synthesizedOblique);

    return result;
}

static Ref<Inspector::Protocol::CSS::Font> buildObjectForFont(const Font& font)
{
    return buildObjectForFont(metadataForFont(font));
}

static RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::CharacterRange>> buildCharacterRanges(const CharacterSet& characters)
{
    Vector<char32_t> sortedCharacters;
    sortedCharacters.reserveInitialCapacity(characters.size());
    for (auto character : characters)
        sortedCharacters.append(character);
    std::sort(sortedCharacters.begin(), sortedCharacters.end());
    if (sortedCharacters.isEmpty())
        return nullptr;

    auto result = JSON::ArrayOf<Inspector::Protocol::CSS::CharacterRange>::create();

    char32_t rangeStart = sortedCharacters[0];
    char32_t rangeEnd = rangeStart;
    for (auto character : sortedCharacters.subspan(1)) {
        if (character == rangeEnd + 1) {
            rangeEnd = character;
            continue;
        }

        result->addItem(Inspector::Protocol::CSS::CharacterRange::create()
            .setStart(rangeStart)
            .setEnd(rangeEnd)
            .release());
        rangeStart = rangeEnd = character;
    }

    result->addItem(Inspector::Protocol::CSS::CharacterRange::create()
        .setStart(rangeStart)
        .setEnd(rangeEnd)
        .release());

    return result;
}

static RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::Font>> buildRenderedFonts(const Vector<Ref<RenderedFont>>& renderedFonts)
{
    if (renderedFonts.isEmpty())
        return nullptr;

    auto result = JSON::ArrayOf<Inspector::Protocol::CSS::Font>::create();
    for (const auto& renderedFont : renderedFonts) {
        auto protocolFont = buildObjectForFont(renderedFont->metadata());

        if (auto characterRanges = buildCharacterRanges(renderedFont->characters()))
            protocolFont->setCharacterRanges(characterRanges.releaseNonNull());

        result->addItem(WTF::move(protocolFont));
    }
    return result;
}

class InspectorFontUsageRecorder final : public GraphicsContext {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(InspectorFontUsageRecorder);
public:
    const RenderedFonts& renderedFonts() const { return m_renderedFonts; }
    const CharacterSet& missingCharacters() const { return m_missingCharacters; }

private:
    Ref<RenderedFont> ensureRenderedFont(const Font& font)
    {
        if (auto iterator = m_renderedFonts.find(&font); iterator != m_renderedFonts.end())
            return iterator->value.copyRef();

        auto metadata = metadataForFont(font);

        RefPtr<RenderedFont> renderedFont;
        for (const auto& candidate : m_renderedFonts.values()) {
            if (candidate->metadata() == metadata) {
                renderedFont = candidate.copyRef();
                break;
            }
        }
        if (!renderedFont)
            renderedFont = RenderedFont::create(WTF::move(metadata));

        return m_renderedFonts.add(font, renderedFont.releaseNonNull()).iterator->value.copyRef();
    }

    FloatSize recordFontUsage(const FontCascade& font, const TextRun& run, const FloatPoint& point, unsigned from, std::optional<unsigned> to, FontCascade::CustomFontNotReadyAction customFontNotReadyAction)
    {
        unsigned destination = to.value_or(run.length());
        auto glyphBuffer = font.layoutText(font.codePath(run, from, to), run, from, destination).glyphBuffer;
        glyphBuffer.flatten();

        recordFontUsage(run, glyphBuffer, from, destination, customFontNotReadyAction);

        if (glyphBuffer.isEmpty())
            return { };

        FloatPoint startPoint = point + WebCore::size(glyphBuffer.initialAdvance());
        font.drawGlyphBuffer(*this, glyphBuffer, startPoint, customFontNotReadyAction);
        return startPoint - point;
    }

    void recordFontUsage(const TextRun& run, const GlyphBuffer& glyphBuffer, unsigned from, unsigned to, FontCascade::CustomFontNotReadyAction customFontNotReadyAction)
    {
        if (from >= to)
            return;

        Vector<unsigned> glyphIndexes;
        glyphIndexes.reserveInitialCapacity(glyphBuffer.size());
        for (unsigned i = 0; i < glyphBuffer.size(); ++i) {
            if (auto offset = glyphBuffer.checkedStringOffsetAt(i, run.length()); offset && unsignedCast(*offset) < to)
                glyphIndexes.append(i);
        }
        std::sort(glyphIndexes.begin(), glyphIndexes.end(), [&](unsigned a, unsigned b) {
            return glyphBuffer.uncheckedStringOffsetAt(a) < glyphBuffer.uncheckedStringOffsetAt(b);
        });

        for (unsigned clusterIndex = 0; clusterIndex < glyphIndexes.size();) {
            unsigned clusterOffset = glyphBuffer.uncheckedStringOffsetAt(glyphIndexes[clusterIndex]);

            unsigned nextClusterIndex = clusterIndex + 1;
            while (nextClusterIndex < glyphIndexes.size()) {
                unsigned nextClusterOffset = glyphBuffer.uncheckedStringOffsetAt(glyphIndexes[nextClusterIndex]);
                if (nextClusterOffset != clusterOffset)
                    break;

                ++nextClusterIndex;
            }

            unsigned clusterStart = std::max(clusterOffset, from);
            unsigned clusterEnd = nextClusterIndex < glyphIndexes.size() ? std::min<unsigned>(glyphBuffer.uncheckedStringOffsetAt(glyphIndexes[nextClusterIndex]), to) : to;
            if (clusterStart >= clusterEnd) {
                clusterIndex = nextClusterIndex;
                continue;
            }

            Vector<Ref<const Font>, 1> renderedFonts;
            bool isMissing = false;
            for (unsigned i = clusterIndex; i < nextClusterIndex; ++i) {
                unsigned glyphIndex = glyphIndexes[i];

                Ref font = glyphBuffer.fontAt(glyphIndex);
                if (font->isInterstitial() && font->visibility() != Font::Visibility::Visible && customFontNotReadyAction != FontCascade::CustomFontNotReadyAction::UseFallbackIfFontNotReady)
                    continue;

                auto glyph = glyphBuffer.glyphAt(glyphIndex);
                if (!glyph)
                    isMissing = true;
                else if (glyph != deletedGlyph && !renderedFonts.contains(font))
                    renderedFonts.append(WTF::move(font));
            }

            auto collectCharacter = [&](char32_t character) {
                if (U_IS_SURROGATE(character))
                    character = replacementCharacter;

                for (const auto& font : renderedFonts)
                    ensureRenderedFont(font)->addCharacter(character);

                if (isMissing)
                    m_missingCharacters.add(character);
            };

            if (run.is8Bit()) {
                for (unsigned offset = clusterStart; offset < clusterEnd; ++offset)
                    collectCharacter(run.span8()[offset]);
            } else {
                auto characters = run.span16();
                for (size_t offset = clusterStart; offset < clusterEnd;) {
                    char32_t character;
                    U16_NEXT(characters, offset, clusterEnd, character);
                    collectCharacter(character);
                }
            }

            clusterIndex = nextClusterIndex;
        }
    }

#if USE(CG)
    bool isCALayerContext() const final { return false; }
#endif

    bool paintingDisabled() const final { return true; }

    void didUpdateState(GraphicsContextState&) final { }

    void drawNativeImage(const NativeImage&, const FloatRect&, const FloatRect&, ImagePaintingOptions) final { }

    void drawSystemImage(SystemImage&, const FloatRect&) final { }

    void drawPattern(const NativeImage&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, const FloatSize&, ImagePaintingOptions) final { }

    IntRect clipBounds() const final { return { }; }

#if USE(CG)
    void applyStrokePattern() final { }
    void applyFillPattern() final { }
    void drawPath(const Path&) final { }
#endif

    void drawRect(const FloatRect&, float = 1) final { }
    void drawLine(const FloatPoint&, const FloatPoint&) final { }
    void drawEllipse(const FloatRect&) final { }
    void fillPath(const Path&) final { }
    void strokePath(const Path&) final { }
    void fillRect(const FloatRect&, RequiresClipToRect) final { }
    void fillRect(const FloatRect&, Gradient&, const AffineTransform&, RequiresClipToRect) final { }
    void fillRect(const FloatRect&, const Color&) final { }
    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final { }
    void strokeRect(const FloatRect&, float) final { }
    void clipPath(const Path&, WindRule = WindRule::EvenOdd) final { }
    void drawLinesForText(const FloatPoint&, float, std::span<const FloatSegment>, bool, bool, StrokeStyle) final { }
    void setLineCap(LineCap) final { }
    void setLineDash(const DashArray&, float) final { }
    void setLineJoin(LineJoin) final { }
    void setMiterLimit(float) final { }
    void clipOut(const Path&) final { }
    void scale(const FloatSize&) final { }
    void rotate(float) final { }
    void translate(float, float) final { }
    void concatCTM(const AffineTransform&) final { }
    void setCTM(const AffineTransform&) final { }
    AffineTransform getCTM(IncludeDeviceScale = PossiblyIncludeDeviceScale) const final { return { }; }
    void clearRect(const FloatRect&) final { }
    void resetClip() final { }
    void clip(const FloatRect&) final { }
    void clipOut(const FloatRect&) final { }
    void save(GraphicsContextState::Purpose = GraphicsContextState::Purpose::SaveRestore) final { }
    void restore(GraphicsContextState::Purpose = GraphicsContextState::Purpose::SaveRestore) final { }

    void drawRaisedEllipse(const FloatRect&, const Color&, const Color&) final { }

    FloatSize drawText(const FontCascade& font, const TextRun& run, const FloatPoint& point, unsigned from = 0, std::optional<unsigned> to = std::nullopt) final
    {
        return recordFontUsage(font, run, point, from, to, FontCascade::CustomFontNotReadyAction::DoNotPaintIfFontNotReady);
    }

    void drawGlyphs(const Font&, std::span<const GlyphBufferGlyph>, std::span<const GlyphBufferAdvance>, const FloatPoint&, FontSmoothingMode) final { }
    void drawDisplayList(const DisplayList::DisplayList&, ControlFactory&) final { }

    void drawEmphasisMarks(const FontCascade&, const TextRun&, const AtomString&, const FloatPoint&, unsigned = 0, std::optional<unsigned> = std::nullopt) final { }
    void drawBidiText(const FontCascade& font, const TextRun& run, const FloatPoint& point, FontCascade::CustomFontNotReadyAction customFontNotReadyAction = FontCascade::CustomFontNotReadyAction::DoNotPaintIfFontNotReady) final
    {
        BidiResolver<TextRunIterator, SimpleBidiCharacterRun> bidiResolver;
        bidiResolver.setStatus(BidiStatus(run.direction(), run.directionalOverride()));
        bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));
        bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()));

        FloatPoint currentPoint = point;
        for (const auto* bidiRun = bidiResolver.runs().firstRun(); bidiRun; bidiRun = bidiRun->next()) {
            TextRun subrun = run.subRun(bidiRun->start(), bidiRun->stop() - bidiRun->start());
            subrun.setDirection(bidiRun->level() % 2 ? TextDirection::RTL : TextDirection::LTR);
            subrun.setDirectionalOverride(bidiRun->dirOverride(false));
            currentPoint.move(recordFontUsage(font, subrun, currentPoint, 0, std::nullopt, customFontNotReadyAction));
        }
    }

    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final { }

    ImageDrawResult drawImage(Image&, const FloatRect&, const FloatRect&, ImagePaintingOptions = { ImageOrientation::Orientation::FromImage }) final { return ImageDrawResult::DidNothing; }

    ImageDrawResult drawTiledImage(Image&, const FloatRect&, const FloatPoint&, const FloatSize&, const FloatSize&, ImagePaintingOptions = { }) final { return ImageDrawResult::DidNothing; }
    ImageDrawResult drawTiledImage(Image&, const FloatRect&, const FloatRect&, const FloatSize&, Image::TileRule, Image::TileRule, ImagePaintingOptions = { }) final { return ImageDrawResult::DidNothing; }

    void drawFocusRing(const Path&, float, const Color&, float) final { }
    void drawFocusRing(const Vector<FloatRect>&, float, const Color&, float) final { }

    void drawImageBuffer(ImageBuffer&, const FloatRect&, const FloatRect&, ImagePaintingOptions = { }) final { }

    void clipRoundedRect(const FloatRoundedRect&) final { }
    void clipOutRoundedRect(const FloatRoundedRect&) final { }
    void clipToImageBuffer(ImageBuffer&, const FloatRect&) final { }

    void fillRect(const FloatRect&, Gradient&) final { }
    void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode = BlendMode::Normal) final { }

    void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode = BlendMode::Normal) final { }
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect&, const Color&) final { }

#if ENABLE(VIDEO)
    void drawVideoFrame(const VideoFrame&, const FloatRect&, ImageOrientation, bool) final { }
#endif

    RenderedFonts m_renderedFonts;
    CharacterSet m_missingCharacters;
};

std::optional<FontData> getFontData(Node& node)
{
    Ref document = node.document();
    RefPtr view = document->view();
    if (!view || !view->renderView() || view->isPainting() || view->layoutContext().isInLayout() || document->inStyleRecalc() || document->isResolvingTreeStyle())
        return std::nullopt;

    view->updateLayoutAndStyleIfNeededRecursive({ LayoutOptions::IgnorePendingStylesheets });
    if (document->view() != view.get() || !view->renderView() || view->isPainting() || view->layoutContext().isInLayout() || document->inStyleRecalc() || document->isResolvingTreeStyle())
        return std::nullopt;

    CheckedPtr computedStyle = node.computedStyle();
    if (!computedStyle)
        return std::nullopt;

    InspectorFontUsageRecorder recorder;

    if (CheckedPtr renderer = node.renderer()) {
        LayoutRect elementRect;
        auto paintingRect = snappedIntRect(renderer->subtreePaintRootRect(elementRect, RenderObject::RespectTransforms::Yes));

        auto oldPaintBehavior = view->paintBehavior();
        auto restorePaintBehavior = makeScopeExit([&] {
            view->setPaintBehavior(oldPaintBehavior);
        });
        view->setPaintBehavior(oldPaintBehavior | PaintBehavior::ExcludeReplacedContentExceptForIFrames);
        view->paintContentsForSnapshot(recorder, paintingRect, &node, LocalFrameView::ExcludeSelection, LocalFrameView::DocumentCoordinates);
    }

    Vector<Ref<RenderedFont>> renderedFonts;
    for (const auto& renderedFont : recorder.renderedFonts().values())
        renderedFonts.appendIfNotContains(renderedFont);

    return { {
        buildObjectForFont(protect(protect(computedStyle->fontCascade())->primaryFont())),
        buildRenderedFonts(renderedFonts),
        buildCharacterRanges(recorder.missingCharacters()),
    } };
}

} // namespace WebCore
