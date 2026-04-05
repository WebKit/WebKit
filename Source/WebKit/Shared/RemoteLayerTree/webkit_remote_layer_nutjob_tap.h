/**
 * Nutjob tap for RemoteLayerBackingStore.
 *
 * This records WebKit display-list items and serializes them into the nutjob
 * bridge protocol over an inherited file descriptor. The active render path is
 * still the native WebKit backing store; this only mirrors the commands.
 */

#ifndef WEBKIT_REMOTE_LAYER_NUTJOB_TAP_H
#define WEBKIT_REMOTE_LAYER_NUTJOB_TAP_H

#ifndef NUTJOB_TAP_DISABLE_PROCESS_ACTIVATION
#define NUTJOB_TAP_DISABLE_PROCESS_ACTIVATION 0
#endif

#ifndef NUTJOB_TAP_DISABLE_LAYER_METADATA
#define NUTJOB_TAP_DISABLE_LAYER_METADATA 0
#endif

#include <WebCore/ColorTypes.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItem.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/Filter.h>
#include <WebCore/FilterImage.h>
#include <WebCore/FilterResults.h>
#include <WebCore/FloatRoundedRect.h>
#include <WebCore/Font.h>
#include <WebCore/GlyphBufferMembers.h>
#include <WebCore/Gradient.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsContextState.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/Image.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NativeImage.h>
#include <WebCore/Path.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/PixelBufferFormat.h>
#include <WebCore/SourceBrush.h>
#include <WebCore/TextFlags.h>
#include <wtf/DataLog.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/Variant.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>
#include <algorithm>
#include <array>
#include <bit>
#include <math.h>
#include <limits.h>
#include <mutex>
#include <span>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>
#include <unistd.h>

#if USE(CORE_TEXT)
#include <CoreText/CoreText.h>
#endif

namespace WebCore {
class PlatformCALayer;
}

namespace WebKit {
class PlatformCALayerRemote;
}

namespace WebKit::NutjobTap {

enum class Command : uint8_t {
    Save = 0x01,
    Restore = 0x02,
    SetFillColor = 0x03,
    SetStrokeColor = 0x04,
    SetLineWidth = 0x05,
    SetAlpha = 0x06,
    SetAntialias = 0x07,

    Translate = 0x10,
    Rotate = 0x11,
    Scale = 0x12,
    SetCTM = 0x13,

    ClipRect = 0x20,
    ClipPath = 0x21,
    ResetClip = 0x22,
    ClipImageMask = 0x23,

    FillRect = 0x30,
    FillRectColor = 0x31,
    FillPath = 0x32,
    FillRoundedRect = 0x33,
    ClearRect = 0x34,

    StrokeRect = 0x40,
    StrokePath = 0x41,
    DrawLine = 0x42,

    DrawImage = 0x50,
    DefineImage = 0x52,
    DefineMask = 0x53,
    FillMask = 0x54,

    DrawGlyphs = 0x60,
    DrawTextRun = 0x61,
    DefineFont = 0x62,
    DefineFontData = 0x63,

    FillLinearGradient = 0x70,
    FillRadialGradient = 0x71,

    BeginTransparency = 0x80,
    EndTransparency = 0x81,

    FrameBegin = 0xF0,
    FrameEnd = 0xF1,
};

enum class PathCommand : uint8_t {
    PathMove = 0,
    PathLine = 1,
    PathQuad = 2,
    PathCubic = 3,
    PathClose = 4,
};

enum class ImagePixelFormat : uint8_t {
    ARGB32PremultipliedLE = 1,
};

enum class MaskPixelFormat : uint8_t {
    A8 = 1,
};

enum class DrawGlyphsEmissionPath : uint8_t {
    None,
    Font,
    Mask,
    Image,
};

struct Writer {
    FILE* out { nullptr };

    void writeU8(uint8_t value) { fputc(static_cast<unsigned char>(value), out); }

    template<typename T> void writeTriviallyCopyable(T value)
    {
        auto bytes = std::bit_cast<std::array<uint8_t, sizeof(T)>>(value);
        for (auto byte : bytes)
            writeU8(byte);
    }

    void writeU16(uint16_t value) { writeTriviallyCopyable(value); }
    void writeU32(uint32_t value) { writeTriviallyCopyable(value); }
    void writeU64(uint64_t value) { writeTriviallyCopyable(value); }
    void writeF32(float value) { writeTriviallyCopyable(value); }
};

struct PathWriter {
    Vector<uint8_t> bytes;
    uint16_t commandCount { 0 };

    void writeU8(uint8_t value) { bytes.append(value); }

    void writeF32(float value)
    {
        auto raw = std::bit_cast<std::array<uint8_t, sizeof(float)>>(value);
        for (auto byte : raw)
            bytes.append(byte);
    }
};

struct SerializationFrame {
    WebCore::GraphicsContextState state;
    WebCore::AffineTransform ctm;
};

struct FrameMetadata {
    WebCore::IntSize size;
    WebCore::FloatPoint origin;
    WebCore::FloatRect dirtyRect;
    WebCore::FloatRect compositeClipRect;
    uint64_t surfaceID { 0 };
    uint64_t maskSurfaceID { 0 };
    uint32_t backgroundColorARGB { 0 };
    float effectiveOpacity { 1.0f };
    uint32_t paintOrder { 0 };
    uint8_t blendMode { static_cast<uint8_t>(WebCore::BlendMode::Normal) };
    uint8_t surfaceFlags { 0 };
};

struct FrameCounters {
    uint32_t stateUpdates { 0 };
    uint32_t saves { 0 };
    uint32_t restores { 0 };
    uint32_t setCTM { 0 };
    uint32_t translates { 0 };
    uint32_t rotates { 0 };
    uint32_t scales { 0 };
    uint32_t clipRects { 0 };
    uint32_t clipPaths { 0 };
    uint32_t resetClips { 0 };
    uint32_t fillRects { 0 };
    uint32_t fillRectColors { 0 };
    uint32_t fillPaths { 0 };
    uint32_t fillRoundedRects { 0 };
    uint32_t clearRects { 0 };
    uint32_t strokeRects { 0 };
    uint32_t strokePaths { 0 };
    uint32_t drawLines { 0 };
    uint32_t drawGlyphRuns { 0 };
    uint32_t gradients { 0 };
    uint32_t prerasterizedFallbacks { 0 };
    uint32_t placeholderFallbacks { 0 };
    uint32_t lossyFallbacks { 0 };
    uint32_t unsupportedNoOps { 0 };
    uint32_t imagePlaceholders { 0 };
    uint32_t controlPlaceholders { 0 };
    uint32_t focusRings { 0 };
    uint32_t textRuns { 0 };
    uint32_t bidiTextRuns { 0 };
    uint32_t displayLists { 0 };
    uint32_t transparencyBegins { 0 };
    uint32_t transparencyEnds { 0 };
};

enum class AdapterHandlingStrategy : uint8_t {
    Direct,
    Lowered,
    Prerasterized,
    Placeholder,
    Lossy,
    UnsupportedNoOp,
};

struct AdapterHandlingCounts {
    uint32_t direct { 0 };
    uint32_t lowered { 0 };
    uint32_t prerasterized { 0 };
    uint32_t placeholder { 0 };
    uint32_t lossy { 0 };
    uint32_t unsupportedNoOp { 0 };

    uint32_t observed() const
    {
        return direct + lowered + prerasterized + placeholder + lossy + unsupportedNoOp;
    }

    uint32_t supported() const
    {
        return direct + lowered + prerasterized;
    }
};

struct AdapterOperationSummary {
    String operation;
    AdapterHandlingCounts counts;
};

struct AdapterReportSnapshot {
    AdapterHandlingCounts overall;
    Vector<AdapterOperationSummary, 64> operations;
};

inline void bumpHandlingCount(AdapterHandlingCounts& counts, AdapterHandlingStrategy strategy)
{
    switch (strategy) {
    case AdapterHandlingStrategy::Direct:
        ++counts.direct;
        break;
    case AdapterHandlingStrategy::Lowered:
        ++counts.lowered;
        break;
    case AdapterHandlingStrategy::Prerasterized:
        ++counts.prerasterized;
        break;
    case AdapterHandlingStrategy::Placeholder:
        ++counts.placeholder;
        break;
    case AdapterHandlingStrategy::Lossy:
        ++counts.lossy;
        break;
    case AdapterHandlingStrategy::UnsupportedNoOp:
        ++counts.unsupportedNoOp;
        break;
    }
}

class AdapterReport {
public:
    void note(const char* operation, AdapterHandlingStrategy strategy)
    {
        if (!operation || !*operation)
            return;

        auto operationString = String::fromLatin1(operation);
        bumpHandlingCount(m_overall, strategy);
        for (auto& summary : m_operations) {
            if (summary.operation == operationString) {
                bumpHandlingCount(summary.counts, strategy);
                return;
            }
        }

        AdapterOperationSummary summary;
        summary.operation = operationString;
        bumpHandlingCount(summary.counts, strategy);
        m_operations.append(summary);
    }

    AdapterReportSnapshot snapshot() const
    {
        return { m_overall, m_operations };
    }

private:
    AdapterHandlingCounts m_overall;
    Vector<AdapterOperationSummary, 64> m_operations;
};

inline const char* adapterReportPath()
{
    const char* path = getenv("NUTJOB_TAP_REPORT_PATH");
    return (path && *path) ? path : nullptr;
}

inline int adapterReportFD()
{
    const char* fdString = getenv("NUTJOB_TAP_REPORT_FD");
    if (!fdString || !*fdString)
        return -1;
    return parseInteger<int>(StringView(unsafeSpan(fdString))).value_or(-1);
}

inline bool hasAdapterReportOutput()
{
    return adapterReportFD() >= 0 || adapterReportPath();
}

inline float envFloatValue(const char* name)
{
    const char* value = getenv(name);
    if (!value || !*value)
        return 0.0f;
    bool isValid = false;
    float parsed = StringView(unsafeSpan(value)).toFloat(isValid);
    return isValid ? parsed : 0.0f;
}

inline WebCore::FloatPoint harnessContentOffset()
{
    static std::once_flag once;
    static WebCore::FloatPoint offset;

    std::call_once(once, [] {
        offset = { envFloatValue("NUTJOB_HARNESS_CONTENT_OFFSET_X"), envFloatValue("NUTJOB_HARNESS_CONTENT_OFFSET_Y") };
    });

    return offset;
}

inline FILE* adapterReportFile()
{
    static std::once_flag once;
    static FILE* file = nullptr;

    std::call_once(once, [] {
        int fdValue = adapterReportFD();
        if (fdValue >= 0) {
            int duplicate = dup(fdValue);
            if (duplicate >= 0)
                file = fdopen(duplicate, "wb");
        }

        if (!file) {
            const char* path = adapterReportPath();
            if (path && *path)
                file = fopen(path, "ab");
        }

        if (file)
            setvbuf(file, nullptr, _IOLBF, 0);
    });

    return file;
}

inline void writeJSONString(FILE* file, StringView value)
{
    fputc('"', file);
    auto utf8 = value.toString().utf8();
    for (auto ch : utf8.span()) {
        switch (ch) {
        case '"':
        case '\\':
            fputc('\\', file);
            fputc(ch, file);
            break;
        case '\n':
            fputc('\\', file);
            fputc('n', file);
            break;
        case '\r':
            fputc('\\', file);
            fputc('r', file);
            break;
        case '\t':
            fputc('\\', file);
            fputc('t', file);
            break;
        default:
            fputc(ch, file);
            break;
        }
    }
    fputc('"', file);
}

inline void writeHandlingCountsJSON(FILE* file, const AdapterHandlingCounts& counts)
{
    fprintf(file,
        "{\"direct\":%u,\"lowered\":%u,\"prerasterized\":%u,\"placeholder\":%u,\"lossy\":%u,\"unsupportedNoOp\":%u,\"observed\":%u,\"supported\":%u,\"supportRatio\":%.6f}",
        counts.direct, counts.lowered, counts.prerasterized, counts.placeholder, counts.lossy, counts.unsupportedNoOp,
        counts.observed(), counts.supported(), counts.observed() ? static_cast<double>(counts.supported()) / counts.observed() : 1.0);
}

inline void appendAdapterReportLine(const FrameMetadata& metadata, const AdapterReportSnapshot& report)
{
    FILE* file = adapterReportFile();
    if (!file)
        return;

    fprintf(file, "{\"surfaceID\":%llu,\"width\":%d,\"height\":%d,\"origin\":{\"x\":%.4f,\"y\":%.4f},\"dirtyRect\":{\"x\":%.4f,\"y\":%.4f,\"width\":%.4f,\"height\":%.4f},\"backgroundARGB\":%u,\"paintOrder\":%u,\"overall\":",
        static_cast<unsigned long long>(metadata.surfaceID),
        metadata.size.width(), metadata.size.height(),
        metadata.origin.x(), metadata.origin.y(),
        metadata.dirtyRect.x(), metadata.dirtyRect.y(), metadata.dirtyRect.width(), metadata.dirtyRect.height(),
        metadata.backgroundColorARGB, metadata.paintOrder);
    writeHandlingCountsJSON(file, report.overall);
    fputc(',', file);
    fputc('"', file);
    fputc('o', file);
    fputc('p', file);
    fputc('e', file);
    fputc('r', file);
    fputc('a', file);
    fputc('t', file);
    fputc('i', file);
    fputc('o', file);
    fputc('n', file);
    fputc('s', file);
    fputc('"', file);
    fputc(':', file);
    fputc('[', file);
    for (size_t index = 0; index < report.operations.size(); ++index) {
        if (index)
            fputc(',', file);
        fputc('{', file);
        fputc('"', file);
        fputc('o', file);
        fputc('p', file);
        fputc('e', file);
        fputc('r', file);
        fputc('a', file);
        fputc('t', file);
        fputc('i', file);
        fputc('o', file);
        fputc('n', file);
        fputc('"', file);
        fputc(':', file);
        writeJSONString(file, report.operations[index].operation);
        fputc(',', file);
        fputc('"', file);
        fputc('c', file);
        fputc('o', file);
        fputc('u', file);
        fputc('n', file);
        fputc('t', file);
        fputc('s', file);
        fputc('"', file);
        fputc(':', file);
        writeHandlingCountsJSON(file, report.operations[index].counts);
        fputc('}', file);
    }
    fputc(']', file);
    fputc('}', file);
    fputc('\n', file);
    fflush(file);
}

struct SerializationState {
#if USE(CORE_TEXT)
    struct FontEntry {
        RetainPtr<CTFontRef> font;
        uint16_t fontId { 0 };
    };
#endif

    Vector<SerializationFrame, 8> stack;
#if USE(CORE_TEXT)
    Vector<FontEntry, 4> definedFonts;
#endif
    uint32_t nextImageID { 1 };
    uint32_t nextMaskID { 1 };
#if USE(CORE_TEXT)
    uint16_t nextFontId { 1 };
#endif

    SerializationState(const WebCore::GraphicsContextState& initialState, const WebCore::AffineTransform& initialCTM)
    {
        stack.append({
            initialState,
            initialCTM
        });
        stack.last().state.didApplyChanges();
    }

    SerializationFrame& current() { return stack.last(); }
    const SerializationFrame& current() const { return stack.last(); }

    void push()
    {
        stack.append({
            current().state,
            current().ctm
        });
        stack.last().state.didApplyChanges();
    }

    void pop()
    {
        if (stack.size() > 1)
            stack.removeLast();
    }

#if USE(CORE_TEXT)
    uint16_t fontIdFor(CTFontRef font) const
    {
        if (!font)
            return 0;

        for (const auto& entry : definedFonts) {
            if (entry.font && CFEqual(entry.font.get(), font))
                return entry.fontId;
        }
        return 0;
    }

    uint16_t defineFontId(CTFontRef font)
    {
        if (!font || !nextFontId)
            return 0;

        uint16_t fontId = nextFontId++;
        definedFonts.append({ font, fontId });
        return fontId;
    }

    // Cached reverse glyph→codepoint map per CTFont.
    // Built by scanning common Unicode ranges (BMP).
    struct GlyphCodepointMap {
        RetainPtr<CTFontRef> font;
        HashMap<uint16_t, uint32_t> map;
    };
    Vector<GlyphCodepointMap, 4> glyphCodepointMaps;

    uint32_t lookupCodepoint(CTFontRef font, uint16_t glyphId)
    {
        // Find or build cached reverse map for this font
        size_t mapIndex = notFound;
        for (size_t idx = 0; idx < glyphCodepointMaps.size(); ++idx) {
            if (glyphCodepointMaps[idx].font && CFEqual(glyphCodepointMaps[idx].font.get(), font)) {
                mapIndex = idx;
                break;
            }
        }

        if (mapIndex == notFound) {
            // Skip color/emoji fonts — they use surrogate pairs and complex
            // cmap tables that don't work with our BMP scanning approach.
            auto traits = CTFontGetSymbolicTraits(font);
            if (traits & kCTFontTraitColorGlyphs) {
                // Insert empty map so we don't re-check
                GlyphCodepointMap emptyMap;
                emptyMap.font = font;
                glyphCodepointMaps.append(std::move(emptyMap));
                return 0;
            }

            if (auto name = adoptCF(CTFontCopyFamilyName(font)))
                WTFLogAlways("nutjob: building codepoint map for font: %s",
                    String(name.get()).utf8().data());

            // Build reverse map by scanning common Unicode ranges
            GlyphCodepointMap newMap;
            newMap.font = font;

            // Scan Latin subset (ASCII + Latin-1 + Latin Extended)
            constexpr size_t chunkSize = 256;
            std::array<UniChar, chunkSize> chars;
            std::array<CGGlyph, chunkSize> cgGlyphs;

            for (uint32_t base = 32; base < 0x0500; base += chunkSize) {
                size_t count = std::min(static_cast<size_t>(0x0500 - base), chunkSize);
                for (size_t i = 0; i < count; ++i)
                    chars[i] = static_cast<UniChar>(base + i);

                cgGlyphs.fill(0);
                CTFontGetGlyphsForCharacters(font, chars.data(), cgGlyphs.data(), count);
                for (size_t i = 0; i < count; ++i) {
                    if (cgGlyphs[i] != 0)
                        newMap.map.add(cgGlyphs[i], base + static_cast<uint32_t>(i));
                }
            }

            WTFLogAlways("nutjob: mapped %u glyphs for font", newMap.map.size());
            glyphCodepointMaps.append(std::move(newMap));
            mapIndex = glyphCodepointMaps.size() - 1;
        }

        auto it = glyphCodepointMaps[mapIndex].map.find(glyphId);
        return it != glyphCodepointMaps[mapIndex].map.end() ? it->value : 0;
    }
#endif
};

inline void noteHandling(AdapterReport* report, const char* operation, AdapterHandlingStrategy strategy)
{
    if (report)
        report->note(operation, strategy);
}

inline std::mutex& tapMutex()
{
    static NeverDestroyed<std::mutex> mutex;
    return mutex.get();
}

inline uint16_t clampDimension(int value)
{
    if (value <= 0)
        return 0;
    if (value >= UINT16_MAX)
        return UINT16_MAX;
    return static_cast<uint16_t>(value);
}

inline uint32_t packARGB(const WebCore::Color& color)
{
    auto srgba = color.toColorTypeLossy<WebCore::SRGBA<uint8_t>>().resolved();
    return WebCore::PackedColor::ARGB(srgba).value;
}

inline uint32_t packARGB(const WebCore::PackedColor::RGBA& color)
{
    return WebCore::PackedColor::ARGB(WebCore::asSRGBA(color)).value;
}

inline bool hasSolidColor(const WebCore::SourceBrush& brush)
{
    return !brush.hasPatternOrGradient();
}

inline bool fillBrushRenderable(const SerializationState& state)
{
    return state.current().state.fillBrush().isVisible();
}

inline bool strokeBrushRenderable(const SerializationState& state)
{
    return state.current().state.strokeBrush().isVisible()
        && state.current().state.strokeThickness() > 0;
}

inline constexpr uint32_t placeholderFillBrushARGB = 0xFF60A5FA;
inline constexpr uint32_t placeholderStrokeBrushARGB = 0xFF8B5CF6;

inline FILE* tapFile()
{
    static std::once_flag once;
    static FILE* file = nullptr;

    std::call_once(once, [] {
#if !NUTJOB_TAP_DISABLE_PROCESS_ACTIVATION
        if (!isInWebProcess())
            return;
#endif

        const char* fdString = getenv("NUTJOB_TAP_FD");
        if (!fdString || !*fdString)
            return;

        int fdValue = parseInteger<int>(StringView(unsafeSpan(fdString))).value_or(-1);
        if (fdValue < 0)
            return;

        int duplicate = dup(fdValue);
        if (duplicate == -1)
            return;

        file = fdopen(duplicate, "wb");
        if (!file) {
            close(duplicate);
            return;
        }

        setvbuf(file, nullptr, _IOFBF, 1 << 16);
    });

    return file;
}

inline bool isActive()
{
    return tapFile() != nullptr;
}

inline bool envFlagEnabled(const char* name)
{
    const char* value = getenv(name);
    if (!value || !*value)
        return false;

    switch (*value) {
    case '0':
    case 'f':
    case 'F':
    case 'n':
    case 'N':
        return false;
    default:
        return true;
    }
}

inline bool strictModeEnabled()
{
    static std::once_flag once;
    static bool enabled = false;

    std::call_once(once, [] {
        enabled = envFlagEnabled("NUTJOB_TAP_STRICT");
    });

    return enabled;
}

[[noreturn]] inline void failStrictMode(const char* feature, const char* strategy)
{
    dataLogLn("nutjob tap strict: ", feature, " hit ", strategy);
    abort();
}

inline void strictUnsupported(const char* feature, const char* strategy)
{
    if (strictModeEnabled())
        failStrictMode(feature, strategy);
}

inline uint32_t encodeBrushColorOrPlaceholder(const WebCore::SourceBrush& brush, uint32_t placeholderARGB, const char* feature)
{
    if (hasSolidColor(brush))
        return packARGB(brush.color());
    strictUnsupported(feature, "placeholder brush encoding");
    return placeholderARGB;
}

inline void emitSetCTM(Writer& writer, const WebCore::AffineTransform& transform)
{
    // Java Transform layout: [ m00 m01 m02 ] = [ a  c  e ]
    //                        [ m10 m11 m12 ]   [ b  d  f ]
    writer.writeU8(static_cast<uint8_t>(Command::SetCTM));
    writer.writeF32(static_cast<float>(transform.a()));  // m00
    writer.writeF32(static_cast<float>(transform.c()));  // m01
    writer.writeF32(static_cast<float>(transform.e()));  // m02
    writer.writeF32(static_cast<float>(transform.b()));  // m10
    writer.writeF32(static_cast<float>(transform.d()));  // m11
    writer.writeF32(static_cast<float>(transform.f()));  // m12
}

inline void emitPath(PathWriter& writer, const WebCore::Path& path)
{
    path.applyElements([&](const WebCore::PathElement& element) {
        switch (element.type) {
        case WebCore::PathElement::Type::MoveToPoint:
            writer.writeU8(static_cast<uint8_t>(PathCommand::PathMove));
            writer.writeF32(element.points[0].x());
            writer.writeF32(element.points[0].y());
            break;
        case WebCore::PathElement::Type::AddLineToPoint:
            writer.writeU8(static_cast<uint8_t>(PathCommand::PathLine));
            writer.writeF32(element.points[0].x());
            writer.writeF32(element.points[0].y());
            break;
        case WebCore::PathElement::Type::AddQuadCurveToPoint:
            writer.writeU8(static_cast<uint8_t>(PathCommand::PathQuad));
            writer.writeF32(element.points[0].x());
            writer.writeF32(element.points[0].y());
            writer.writeF32(element.points[1].x());
            writer.writeF32(element.points[1].y());
            break;
        case WebCore::PathElement::Type::AddCurveToPoint:
            writer.writeU8(static_cast<uint8_t>(PathCommand::PathCubic));
            writer.writeF32(element.points[0].x());
            writer.writeF32(element.points[0].y());
            writer.writeF32(element.points[1].x());
            writer.writeF32(element.points[1].y());
            writer.writeF32(element.points[2].x());
            writer.writeF32(element.points[2].y());
            break;
        case WebCore::PathElement::Type::CloseSubpath:
            writer.writeU8(static_cast<uint8_t>(PathCommand::PathClose));
            break;
        }
        ++writer.commandCount;
    });
}

inline void emitPathCommand(Writer& writer, uint8_t opcode, const WebCore::Path& path)
{
    PathWriter pathWriter;
    emitPath(pathWriter, path);
    writer.writeU8(opcode);
    writer.writeU16(pathWriter.commandCount);
    for (auto byte : pathWriter.bytes)
        writer.writeU8(byte);
}

inline void emitClipPath(Writer& writer, const WebCore::Path& path)
{
    emitPathCommand(writer, static_cast<uint8_t>(Command::ClipPath), path);
}

inline void emitFillPath(Writer& writer, const WebCore::Path& path)
{
    emitPathCommand(writer, static_cast<uint8_t>(Command::FillPath), path);
}

inline void emitStrokePath(Writer& writer, const WebCore::Path& path)
{
    emitPathCommand(writer, static_cast<uint8_t>(Command::StrokePath), path);
}

inline void emitSetFillBrush(Writer& writer, const WebCore::SourceBrush& brush)
{
    writer.writeU8(static_cast<uint8_t>(Command::SetFillColor));
    writer.writeU32(encodeBrushColorOrPlaceholder(brush, placeholderFillBrushARGB, "fill brush"));
}

inline void emitBeginTransparency(Writer& writer, float opacity, WebCore::CompositeOperator compositeOperator, WebCore::BlendMode blendMode)
{
    writer.writeU8(static_cast<uint8_t>(Command::BeginTransparency));
    writer.writeF32(opacity);
    writer.writeU8(static_cast<uint8_t>(compositeOperator));
    writer.writeU8(static_cast<uint8_t>(blendMode));
}

inline void emitSetStrokeBrush(Writer& writer, const WebCore::SourceBrush& brush)
{
    writer.writeU8(static_cast<uint8_t>(Command::SetStrokeColor));
    writer.writeU32(encodeBrushColorOrPlaceholder(brush, placeholderStrokeBrushARGB, "stroke brush"));
}

inline void emitInitialState(Writer& writer, const SerializationState& state)
{
    emitSetCTM(writer, state.current().ctm);
    emitSetFillBrush(writer, state.current().state.fillBrush());
    emitSetStrokeBrush(writer, state.current().state.strokeBrush());

    writer.writeU8(static_cast<uint8_t>(Command::SetLineWidth));
    writer.writeF32(state.current().state.strokeThickness());

    writer.writeU8(static_cast<uint8_t>(Command::SetAlpha));
    writer.writeF32(state.current().state.alpha());

    writer.writeU8(static_cast<uint8_t>(Command::SetAntialias));
    writer.writeU8(state.current().state.shouldAntialias() ? 1 : 0);
}

inline void applyStateChange(SerializationState& state, const WebCore::GraphicsContextState& change)
{
    using Change = WebCore::GraphicsContextState::Change;
    auto changes = change.changes();

    if (changes.contains(Change::FillBrush))
        state.current().state.setFillBrush(change.fillBrush());
    if (changes.contains(Change::StrokeBrush))
        state.current().state.setStrokeBrush(change.strokeBrush());
    if (changes.contains(Change::StrokeThickness))
        state.current().state.setStrokeThickness(change.strokeThickness());
    if (changes.contains(Change::Alpha))
        state.current().state.setAlpha(change.alpha());
    if (changes.contains(Change::TextDrawingMode))
        state.current().state.setTextDrawingMode(change.textDrawingMode());
    if (changes.contains(Change::ShouldAntialias))
        state.current().state.setShouldAntialias(change.shouldAntialias());
    state.current().state.didApplyChanges();
}

inline void emitStateChange(Writer& writer, const WebCore::GraphicsContextState& change)
{
    using Change = WebCore::GraphicsContextState::Change;
    auto changes = change.changes();

    if (changes.contains(Change::FillBrush))
        emitSetFillBrush(writer, change.fillBrush());
    if (changes.contains(Change::StrokeBrush))
        emitSetStrokeBrush(writer, change.strokeBrush());
    if (changes.contains(Change::StrokeThickness)) {
        writer.writeU8(static_cast<uint8_t>(Command::SetLineWidth));
        writer.writeF32(change.strokeThickness());
    }
    if (changes.contains(Change::Alpha)) {
        writer.writeU8(static_cast<uint8_t>(Command::SetAlpha));
        writer.writeF32(change.alpha());
    }
    if (changes.contains(Change::ShouldAntialias)) {
        writer.writeU8(static_cast<uint8_t>(Command::SetAntialias));
        writer.writeU8(change.shouldAntialias() ? 1 : 0);
    }
}

inline void emitFillRect(Writer& writer, const WebCore::FloatRect& rect)
{
    writer.writeU8(static_cast<uint8_t>(Command::FillRect));
    writer.writeF32(rect.x());
    writer.writeF32(rect.y());
    writer.writeF32(rect.width());
    writer.writeF32(rect.height());
}

inline void emitStrokeRect(Writer& writer, const WebCore::FloatRect& rect)
{
    writer.writeU8(static_cast<uint8_t>(Command::StrokeRect));
    writer.writeF32(rect.x());
    writer.writeF32(rect.y());
    writer.writeF32(rect.width());
    writer.writeF32(rect.height());
}

inline void emitClearRect(Writer& writer, const WebCore::FloatRect& rect)
{
    writer.writeU8(static_cast<uint8_t>(Command::ClearRect));
    writer.writeF32(rect.x());
    writer.writeF32(rect.y());
    writer.writeF32(rect.width());
    writer.writeF32(rect.height());
}

inline void emitClipRect(Writer& writer, const WebCore::FloatRect& rect)
{
    writer.writeU8(static_cast<uint8_t>(Command::ClipRect));
    writer.writeF32(rect.x());
    writer.writeF32(rect.y());
    writer.writeF32(rect.width());
    writer.writeF32(rect.height());
}

inline void emitFillRectColor(Writer& writer, const WebCore::FloatRect& rect, const WebCore::Color& color)
{
    writer.writeU8(static_cast<uint8_t>(Command::FillRectColor));
    writer.writeF32(rect.x());
    writer.writeF32(rect.y());
    writer.writeF32(rect.width());
    writer.writeF32(rect.height());
    writer.writeU32(packARGB(color));
}

inline void emitPlaceholderRect(Writer& writer, const WebCore::FloatRect& rect, uint32_t argb)
{
    if (rect.isEmpty())
        return;
    writer.writeU8(static_cast<uint8_t>(Command::FillRectColor));
    writer.writeF32(rect.x());
    writer.writeF32(rect.y());
    writer.writeF32(rect.width());
    writer.writeF32(rect.height());
    writer.writeU32(argb);
}

inline void emitPlaceholderFallback(Writer& writer, const WebCore::FloatRect& rect, uint32_t argb, const char* feature, AdapterReport* report = nullptr, const char* operation = nullptr)
{
    noteHandling(report, operation ? operation : feature, AdapterHandlingStrategy::Placeholder);
    strictUnsupported(feature, "placeholder fallback");
    emitPlaceholderRect(writer, rect, argb);
}

inline uint32_t allocateImageID(SerializationState& state)
{
    if (!state.nextImageID)
        state.nextImageID = 1;
    return state.nextImageID++;
}

inline uint32_t allocateMaskID(SerializationState& state)
{
    if (!state.nextMaskID)
        state.nextMaskID = 1;
    return state.nextMaskID++;
}

inline WebCore::FloatRect normalizedSourceRect(const WebCore::FloatRect& sourceRect, const WebCore::IntSize& imageSize)
{
    if (sourceRect.width() <= 0 || sourceRect.height() <= 0)
        return { 0, 0, static_cast<float>(imageSize.width()), static_cast<float>(imageSize.height()) };
    return sourceRect;
}

inline void emitDefineImage(Writer& writer, uint32_t imageID, const WebCore::PixelBuffer& pixelBuffer)
{
    auto size = pixelBuffer.size();
    auto bytes = pixelBuffer.bytes();
    if (size.isEmpty() || bytes.empty())
        return;

    writer.writeU8(static_cast<uint8_t>(Command::DefineImage));
    writer.writeU32(imageID);
    writer.writeU32(static_cast<uint32_t>(std::max(size.width(), 0)));
    writer.writeU32(static_cast<uint32_t>(std::max(size.height(), 0)));
    writer.writeU8(static_cast<uint8_t>(ImagePixelFormat::ARGB32PremultipliedLE));
    writer.writeU32(static_cast<uint32_t>(bytes.size()));
    for (auto byte : bytes)
        writer.writeU8(byte);
}

inline void emitDefineMask(Writer& writer, uint32_t maskID, const WebCore::PixelBuffer& pixelBuffer, const WebCore::IntRect& sourceRect)
{
    auto size = pixelBuffer.size();
    auto bytes = pixelBuffer.bytes();
    if (size.isEmpty() || bytes.empty() || sourceRect.isEmpty())
        return;

    auto pixelFormat = pixelBuffer.format().pixelFormat;
    auto bytesPerPixel = WebCore::PixelBuffer::bytesPerPixel(pixelFormat);
    if (bytesPerPixel < 4)
        return;

    writer.writeU8(static_cast<uint8_t>(Command::DefineMask));
    writer.writeU32(maskID);
    writer.writeU32(static_cast<uint32_t>(std::max(sourceRect.width(), 0)));
    writer.writeU32(static_cast<uint32_t>(std::max(sourceRect.height(), 0)));
    writer.writeU8(static_cast<uint8_t>(MaskPixelFormat::A8));
    writer.writeU32(static_cast<uint32_t>(std::max(sourceRect.width(), 0) * std::max(sourceRect.height(), 0)));

    for (int y = 0; y < sourceRect.height(); ++y) {
        size_t rowOffset = static_cast<size_t>(y + sourceRect.y()) * size.width() * bytesPerPixel;
        for (int x = 0; x < sourceRect.width(); ++x) {
            size_t alphaOffset = rowOffset + static_cast<size_t>(x + sourceRect.x()) * bytesPerPixel + 3;
            writer.writeU8(alphaOffset < bytes.size() ? bytes[alphaOffset] : 0);
        }
    }
}

inline void emitDrawImage(Writer& writer, uint32_t imageID, const WebCore::FloatRect& destinationRect, const WebCore::FloatRect& sourceRect)
{
    if (destinationRect.isEmpty())
        return;

    writer.writeU8(static_cast<uint8_t>(Command::DrawImage));
    writer.writeU32(imageID);
    writer.writeF32(destinationRect.x());
    writer.writeF32(destinationRect.y());
    writer.writeF32(destinationRect.width());
    writer.writeF32(destinationRect.height());
    writer.writeF32(sourceRect.x());
    writer.writeF32(sourceRect.y());
    writer.writeF32(sourceRect.width());
    writer.writeF32(sourceRect.height());
}

inline void emitFillMask(Writer& writer, uint32_t maskID, const WebCore::FloatRect& destinationRect)
{
    if (destinationRect.isEmpty())
        return;

    writer.writeU8(static_cast<uint8_t>(Command::FillMask));
    writer.writeU32(maskID);
    writer.writeF32(destinationRect.x());
    writer.writeF32(destinationRect.y());
    writer.writeF32(destinationRect.width());
    writer.writeF32(destinationRect.height());
}

inline void emitClipImageMask(Writer& writer, uint32_t imageID, const WebCore::FloatRect& destinationRect)
{
    if (destinationRect.isEmpty())
        return;

    writer.writeU8(static_cast<uint8_t>(Command::ClipImageMask));
    writer.writeU32(imageID);
    writer.writeF32(destinationRect.x());
    writer.writeF32(destinationRect.y());
    writer.writeF32(destinationRect.width());
    writer.writeF32(destinationRect.height());
}

inline RefPtr<WebCore::PixelBuffer> copyPixelBufferFromImageBuffer(const WebCore::ImageBuffer& imageBuffer)
{
    auto logicalSize = imageBuffer.truncatedLogicalSize();
    if (logicalSize.isEmpty())
        return nullptr;

    return imageBuffer.getPixelBuffer(
        { WebCore::AlphaPremultiplication::Premultiplied, WebCore::PixelFormat::BGRA8, WebCore::DestinationColorSpace::SRGB() },
        { { }, logicalSize });
}

inline RefPtr<WebCore::PixelBuffer> copyPixelBufferFromNativeImage(const WebCore::NativeImage& nativeImage)
{
    auto size = nativeImage.size();
    if (size.isEmpty())
        return nullptr;

    auto imageBuffer = WebCore::ImageBuffer::create(size, WebCore::RenderingMode::Unaccelerated, WebCore::RenderingPurpose::Unspecified, 1, WebCore::DestinationColorSpace::SRGB(), WebCore::PixelFormat::BGRA8);
    if (!imageBuffer)
        return nullptr;

    auto rect = WebCore::FloatRect(0, 0, size.width(), size.height());
    imageBuffer->context().drawNativeImage(nativeImage, rect, rect);
    return copyPixelBufferFromImageBuffer(*imageBuffer);
}

inline RefPtr<WebCore::PixelBuffer> copyPixelBufferFromImage(WebCore::Image& image)
{
    RefPtr<WebCore::NativeImage> nativeImage = image.nativeImage(WebCore::DestinationColorSpace::SRGB());
    if (!nativeImage)
        return nullptr;
    return copyPixelBufferFromNativeImage(*nativeImage);
}

inline bool emitImageResource(Writer& writer, SerializationState& state, const WebCore::PixelBuffer& pixelBuffer, const WebCore::FloatRect& destinationRect, const WebCore::FloatRect& sourceRect)
{
    auto imageSize = pixelBuffer.size();
    if (imageSize.isEmpty() || destinationRect.isEmpty())
        return false;

    auto imageID = allocateImageID(state);
    emitDefineImage(writer, imageID, pixelBuffer);
    emitDrawImage(writer, imageID, destinationRect, normalizedSourceRect(sourceRect, imageSize));
    return true;
}

inline std::optional<WebCore::FloatRect> nonTransparentPixelBounds(const WebCore::PixelBuffer& pixelBuffer)
{
    auto imageSize = pixelBuffer.size();
    if (imageSize.isEmpty())
        return std::nullopt;

    auto bytes = pixelBuffer.bytes();
    auto bytesPerPixel = WebCore::PixelBuffer::bytesPerPixel(pixelBuffer.format().pixelFormat);
    if (bytesPerPixel < 4)
        return std::nullopt;

    int width = imageSize.width();
    int height = imageSize.height();
    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < height; ++y) {
        size_t rowOffset = static_cast<size_t>(y) * width * bytesPerPixel;
        for (int x = 0; x < width; ++x) {
            size_t alphaOffset = rowOffset + static_cast<size_t>(x) * bytesPerPixel + 3;
            if (alphaOffset >= bytes.size() || !bytes[alphaOffset])
                continue;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY)
        return std::nullopt;

    return WebCore::FloatRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

inline bool emitTrimmedImageResource(Writer& writer, SerializationState& state, const WebCore::PixelBuffer& pixelBuffer, const WebCore::FloatRect& destinationRect)
{
    auto alphaBounds = nonTransparentPixelBounds(pixelBuffer);
    if (!alphaBounds)
        return false;

    auto imageSize = pixelBuffer.size();
    if (imageSize.isEmpty())
        return false;

    float scaleX = destinationRect.width() / imageSize.width();
    float scaleY = destinationRect.height() / imageSize.height();
    auto trimmedDestinationRect = WebCore::FloatRect(
        destinationRect.x() + alphaBounds->x() * scaleX,
        destinationRect.y() + alphaBounds->y() * scaleY,
        alphaBounds->width() * scaleX,
        alphaBounds->height() * scaleY
    );
    return emitImageResource(writer, state, pixelBuffer, trimmedDestinationRect, *alphaBounds);
}

inline bool emitTrimmedMaskResource(Writer& writer, SerializationState& state, const WebCore::PixelBuffer& pixelBuffer, const WebCore::FloatRect& destinationRect)
{
    auto alphaBounds = nonTransparentPixelBounds(pixelBuffer);
    if (!alphaBounds)
        return false;

    auto imageSize = pixelBuffer.size();
    if (imageSize.isEmpty())
        return false;

    float scaleX = destinationRect.width() / imageSize.width();
    float scaleY = destinationRect.height() / imageSize.height();
    auto trimmedDestinationRect = WebCore::FloatRect(
        destinationRect.x() + alphaBounds->x() * scaleX,
        destinationRect.y() + alphaBounds->y() * scaleY,
        alphaBounds->width() * scaleX,
        alphaBounds->height() * scaleY
    );

    auto maskID = allocateMaskID(state);
    auto sourceRect = WebCore::enclosingIntRect(*alphaBounds);
    if (sourceRect.isEmpty())
        return false;
    emitDefineMask(writer, maskID, pixelBuffer, sourceRect);
    emitFillMask(writer, maskID, trimmedDestinationRect);
    return true;
}

inline bool emitImageBufferResource(Writer& writer, SerializationState& state, const WebCore::ImageBuffer& imageBuffer, const WebCore::FloatRect& destinationRect, const WebCore::FloatRect& sourceRect)
{
    RefPtr<WebCore::PixelBuffer> pixelBuffer = copyPixelBufferFromImageBuffer(imageBuffer);
    if (!pixelBuffer)
        return false;
    return emitImageResource(writer, state, *pixelBuffer, destinationRect, sourceRect);
}

inline bool emitClipImageBufferResource(Writer& writer, SerializationState& state, const WebCore::ImageBuffer& imageBuffer, const WebCore::FloatRect& destinationRect)
{
    RefPtr<WebCore::PixelBuffer> pixelBuffer = copyPixelBufferFromImageBuffer(imageBuffer);
    if (!pixelBuffer)
        return false;

    auto imageSize = pixelBuffer->size();
    if (imageSize.isEmpty() || destinationRect.isEmpty())
        return false;

    auto imageID = allocateImageID(state);
    emitDefineImage(writer, imageID, *pixelBuffer);
    emitClipImageMask(writer, imageID, destinationRect);
    return true;
}

inline bool emitClipOutPathResource(Writer& writer, SerializationState& state, const WebCore::IntSize& surfaceSize, const WebCore::Path& path)
{
    if (surfaceSize.isEmpty() || path.isEmpty())
        return false;

    WebCore::FloatRect surfaceRect(0, 0, surfaceSize.width(), surfaceSize.height());
    auto imageBuffer = WebCore::ImageBuffer::create(surfaceSize, WebCore::RenderingMode::Unaccelerated, WebCore::RenderingPurpose::Unspecified, 1, WebCore::DestinationColorSpace::SRGB(), WebCore::PixelFormat::BGRA8);
    if (!imageBuffer)
        return false;

    auto& context = imageBuffer->context();
    context.clearRect(surfaceRect);
    context.save();
    context.setShouldAntialias(state.current().state.shouldAntialias());
    context.setCTM(state.current().ctm);
    context.clipOut(path);
    context.setCTM(WebCore::AffineTransform());
    context.fillRect(surfaceRect, WebCore::Color::black);
    context.restore();
    return emitClipImageBufferResource(writer, state, *imageBuffer, surfaceRect);
}

inline bool emitNativeImageResource(Writer& writer, SerializationState& state, const WebCore::NativeImage& nativeImage, const WebCore::FloatRect& destinationRect, const WebCore::FloatRect& sourceRect)
{
    RefPtr<WebCore::PixelBuffer> pixelBuffer = copyPixelBufferFromNativeImage(nativeImage);
    if (!pixelBuffer)
        return false;
    return emitImageResource(writer, state, *pixelBuffer, destinationRect, sourceRect);
}

inline bool emitImageResource(Writer& writer, SerializationState& state, WebCore::Image& image, const WebCore::FloatRect& destinationRect, const WebCore::FloatRect& sourceRect)
{
    RefPtr<WebCore::PixelBuffer> pixelBuffer = copyPixelBufferFromImage(image);
    if (!pixelBuffer)
        return false;
    return emitImageResource(writer, state, *pixelBuffer, destinationRect, sourceRect);
}

inline WebCore::FloatSize rasterizedImageBufferSize(const WebCore::FloatRect& destinationRect)
{
    return {
        std::max(1.0f, ceilf(std::max(destinationRect.width(), 0.0f))),
        std::max(1.0f, ceilf(std::max(destinationRect.height(), 0.0f)))
    };
}

inline WebCore::FloatRect rasterizedImageSourceRect(const WebCore::FloatRect& destinationRect)
{
    return {
        0,
        0,
        std::max(destinationRect.width(), 0.0f),
        std::max(destinationRect.height(), 0.0f)
    };
}

template<typename Painter> inline bool emitRasterizedImageResource(Writer& writer, SerializationState& state, const WebCore::FloatRect& destinationRect, Painter&& painter)
{
    if (destinationRect.isEmpty())
        return false;

    auto imageBuffer = WebCore::ImageBuffer::create(rasterizedImageBufferSize(destinationRect), WebCore::RenderingMode::Unaccelerated, WebCore::RenderingPurpose::Unspecified, 1, WebCore::DestinationColorSpace::SRGB(), WebCore::PixelFormat::BGRA8);
    if (!imageBuffer)
        return false;

    auto& context = imageBuffer->context();
    context.clearRect(rasterizedImageSourceRect(destinationRect));
    context.translate(-destinationRect.x(), -destinationRect.y());
    std::forward<Painter>(painter)(context);
    return emitImageBufferResource(writer, state, *imageBuffer, destinationRect, rasterizedImageSourceRect(destinationRect));
}

inline bool emitFilteredImageBufferResource(Writer& writer, SerializationState& state, WebCore::ImageBuffer* sourceImage, const WebCore::FloatRect& sourceImageRect, WebCore::Filter& filter, WebCore::FilterResults& results)
{
    if (!sourceImage || sourceImageRect.isEmpty())
        return false;

    RefPtr<WebCore::FilterImage> result = filter.apply(sourceImage, sourceImageRect, results);
    if (!result)
        return false;

    auto resultRect = WebCore::FloatRect(result->absoluteImageRect());
    if (resultRect.isEmpty())
        return false;

    auto* resultImageBuffer = result->imageBuffer();
    if (!resultImageBuffer)
        return false;

    auto imageBuffer = WebCore::ImageBuffer::create(rasterizedImageBufferSize(resultRect), WebCore::RenderingMode::Unaccelerated, WebCore::RenderingPurpose::Unspecified, 1, WebCore::DestinationColorSpace::SRGB(), WebCore::PixelFormat::BGRA8);
    if (!imageBuffer)
        return false;

    auto& context = imageBuffer->context();
    context.clearRect(rasterizedImageSourceRect(resultRect));
    context.translate(-resultRect.x(), -resultRect.y());

    auto filterScale = filter.filterScale();
    if (fabsf(filterScale.width()) > 0.0001f && fabsf(filterScale.height()) > 0.0001f)
        context.scale({ 1 / filterScale.width(), 1 / filterScale.height() });

    context.drawImageBuffer(*resultImageBuffer, resultRect);
    return emitImageBufferResource(writer, state, *imageBuffer, resultRect, rasterizedImageSourceRect(resultRect));
}

inline void emitTemporaryLineWidthRect(Writer& writer, const SerializationState& state, const WebCore::FloatRect& rect, float lineWidth)
{
    if (fabsf(lineWidth - state.current().state.strokeThickness()) < 0.0001f) {
        emitStrokeRect(writer, rect);
        return;
    }

    writer.writeU8(static_cast<uint8_t>(Command::Save));
    writer.writeU8(static_cast<uint8_t>(Command::SetLineWidth));
    writer.writeF32(lineWidth);
    emitStrokeRect(writer, rect);
    writer.writeU8(static_cast<uint8_t>(Command::Restore));
}

class PhaseOneOperationSerializer {
public:
    PhaseOneOperationSerializer(Writer& writer, SerializationState& state, FrameCounters* counters = nullptr, AdapterReport* report = nullptr)
        : m_writer(writer)
        , m_state(state)
        , m_counters(counters)
        , m_report(report)
    {
    }

    SerializationState& state() { return m_state; }
    const SerializationState& state() const { return m_state; }

    void emitInitialState()
    {
        NutjobTap::emitInitialState(m_writer, m_state);
    }

    void didUpdateGraphicsContextState(WebCore::GraphicsContextState& state)
    {
        bump(&FrameCounters::stateUpdates);
        applyGraphicsContextStateChange(state);
        state.didApplyChanges();
    }

    void applyGraphicsContextStateChange(const WebCore::GraphicsContextState& change)
    {
        noteStateChanges(change, AdapterHandlingStrategy::Lowered);
        NutjobTap::emitStateChange(m_writer, change);
        NutjobTap::applyStateChange(m_state, change);
    }

    void setFillColor(const WebCore::Color& color)
    {
        note("set-fill-color", AdapterHandlingStrategy::Direct);
        m_state.current().state.setFillColor(color);
        m_state.current().state.didApplyChanges();
        m_writer.writeU8(static_cast<uint8_t>(Command::SetFillColor));
        m_writer.writeU32(packARGB(color));
    }

    void setStrokeColor(const WebCore::Color& color)
    {
        note("set-stroke-color", AdapterHandlingStrategy::Direct);
        m_state.current().state.setStrokeColor(color);
        m_state.current().state.didApplyChanges();
        m_writer.writeU8(static_cast<uint8_t>(Command::SetStrokeColor));
        m_writer.writeU32(packARGB(color));
    }

    void setLineWidth(float thickness)
    {
        note("set-line-width", AdapterHandlingStrategy::Direct);
        m_state.current().state.setStrokeThickness(thickness);
        m_state.current().state.didApplyChanges();
        m_writer.writeU8(static_cast<uint8_t>(Command::SetLineWidth));
        m_writer.writeF32(thickness);
    }

    void setAlpha(float alpha)
    {
        note("set-alpha", AdapterHandlingStrategy::Direct);
        m_state.current().state.setAlpha(alpha);
        m_state.current().state.didApplyChanges();
        m_writer.writeU8(static_cast<uint8_t>(Command::SetAlpha));
        m_writer.writeF32(alpha);
    }

    void setShouldAntialias(bool shouldAntialias)
    {
        note("set-antialias", AdapterHandlingStrategy::Direct);
        m_state.current().state.setShouldAntialias(shouldAntialias);
        m_state.current().state.didApplyChanges();
        m_writer.writeU8(static_cast<uint8_t>(Command::SetAntialias));
        m_writer.writeU8(shouldAntialias ? 1 : 0);
    }

    void save()
    {
        bump(&FrameCounters::saves);
        note("save", AdapterHandlingStrategy::Direct);
        m_state.push();
        m_writer.writeU8(static_cast<uint8_t>(Command::Save));
    }

    void restore()
    {
        bump(&FrameCounters::restores);
        note("restore", AdapterHandlingStrategy::Direct);
        m_state.pop();
        m_writer.writeU8(static_cast<uint8_t>(Command::Restore));
    }

    void translate(float x, float y)
    {
        bump(&FrameCounters::translates);
        bump(&FrameCounters::setCTM);
        note("translate", AdapterHandlingStrategy::Direct);
        m_state.current().ctm.translate(x, y);
        m_writer.writeU8(static_cast<uint8_t>(Command::Translate));
        m_writer.writeF32(x);
        m_writer.writeF32(y);
    }

    void rotate(float angleInRadians)
    {
        bump(&FrameCounters::rotates);
        bump(&FrameCounters::setCTM);
        note("rotate", AdapterHandlingStrategy::Direct);
        m_state.current().ctm.rotateRadians(angleInRadians);
        m_writer.writeU8(static_cast<uint8_t>(Command::Rotate));
        m_writer.writeF32(angleInRadians);
    }

    void scale(const WebCore::FloatSize& amount)
    {
        bump(&FrameCounters::scales);
        bump(&FrameCounters::setCTM);
        note("scale", AdapterHandlingStrategy::Direct);
        m_state.current().ctm.scale(amount);
        m_writer.writeU8(static_cast<uint8_t>(Command::Scale));
        m_writer.writeF32(amount.width());
        m_writer.writeF32(amount.height());
    }

    void setCTM(const WebCore::AffineTransform& transform)
    {
        bump(&FrameCounters::setCTM);
        note("set-ctm", AdapterHandlingStrategy::Direct);
        m_state.current().ctm = transform;
        emitSetCTM(m_writer, m_state.current().ctm);
    }

    void concatCTM(const WebCore::AffineTransform& transform)
    {
        bump(&FrameCounters::setCTM);
        note("concat-ctm", AdapterHandlingStrategy::Lowered);
        m_state.current().ctm *= transform;
        emitSetCTM(m_writer, m_state.current().ctm);
    }

    void clipRect(const WebCore::FloatRect& rect)
    {
        bump(&FrameCounters::clipRects);
        note("clip-rect", AdapterHandlingStrategy::Direct);
        emitClipRect(m_writer, rect);
    }

    void clipPath(const WebCore::Path& path)
    {
        bump(&FrameCounters::clipPaths);
        note("clip-path", AdapterHandlingStrategy::Direct);
        emitClipPath(m_writer, path);
    }

    void resetClip()
    {
        bump(&FrameCounters::resetClips);
        note("reset-clip", AdapterHandlingStrategy::Direct);
        m_writer.writeU8(static_cast<uint8_t>(Command::ResetClip));
    }

    void beginTransparency(float opacity, WebCore::CompositeOperator compositeOperator = WebCore::CompositeOperator::SourceOver, WebCore::BlendMode blendMode = WebCore::BlendMode::Normal)
    {
        bump(&FrameCounters::transparencyBegins);
        note("begin-transparency", AdapterHandlingStrategy::Direct);
        m_state.push();
        emitBeginTransparency(m_writer, opacity, compositeOperator, blendMode);
    }

    void endTransparency()
    {
        bump(&FrameCounters::transparencyEnds);
        note("end-transparency", AdapterHandlingStrategy::Direct);
        m_state.pop();
        m_writer.writeU8(static_cast<uint8_t>(Command::EndTransparency));
    }

    void fillRect(const WebCore::FloatRect& rect)
    {
        bump(&FrameCounters::fillRects);
        note("fill-rect", AdapterHandlingStrategy::Direct);
        emitFillRect(m_writer, rect);
    }

    void fillRectColor(const WebCore::FloatRect& rect, const WebCore::Color& color)
    {
        bump(&FrameCounters::fillRectColors);
        note("fill-rect-color", AdapterHandlingStrategy::Direct);
        emitFillRectColor(m_writer, rect, color);
    }

    void strokeRect(const WebCore::FloatRect& rect, float lineWidth)
    {
        bump(&FrameCounters::strokeRects);
        note("stroke-rect", AdapterHandlingStrategy::Direct);
        emitTemporaryLineWidthRect(m_writer, m_state, rect, lineWidth);
    }

    void clearRect(const WebCore::FloatRect& rect)
    {
        bump(&FrameCounters::clearRects);
        note("clear-rect", AdapterHandlingStrategy::Direct);
        emitClearRect(m_writer, rect);
    }

private:
    void note(const char* operation, AdapterHandlingStrategy strategy)
    {
        noteHandling(m_report, operation, strategy);
    }

    void noteStateChanges(const WebCore::GraphicsContextState& change, AdapterHandlingStrategy strategy)
    {
        using Change = WebCore::GraphicsContextState::Change;
        auto changes = change.changes();

        if (changes.contains(Change::FillBrush))
            note("set-fill-color", strategy);
        if (changes.contains(Change::StrokeBrush))
            note("set-stroke-color", strategy);
        if (changes.contains(Change::StrokeThickness))
            note("set-line-width", strategy);
        if (changes.contains(Change::Alpha))
            note("set-alpha", strategy);
        if (changes.contains(Change::ShouldAntialias))
            note("set-antialias", strategy);
    }

    void bump(uint32_t FrameCounters::*member)
    {
        if (!m_counters || !member)
            return;
        ++(m_counters->*member);
    }

    Writer& m_writer;
    SerializationState& m_state;
    FrameCounters* m_counters { nullptr };
    AdapterReport* m_report { nullptr };
};

inline bool serializePhaseOneDisplayListItem(PhaseOneOperationSerializer& serializer, const WebCore::DisplayList::Item& item)
{
    using namespace WebCore::DisplayList;

    return WTF::switchOn(item, [&]<typename ItemType>(const ItemType& command) -> bool {
        if constexpr (std::is_same_v<ItemType, Save>) {
            serializer.save();
            return true;
        } else if constexpr (std::is_same_v<ItemType, Restore>) {
            serializer.restore();
            return true;
        } else if constexpr (std::is_same_v<ItemType, Translate>) {
            serializer.translate(command.x(), command.y());
            return true;
        } else if constexpr (std::is_same_v<ItemType, Rotate>) {
            serializer.rotate(command.angle());
            return true;
        } else if constexpr (std::is_same_v<ItemType, Scale>) {
            serializer.scale(command.amount());
            return true;
        } else if constexpr (std::is_same_v<ItemType, SetCTM>) {
            serializer.setCTM(command.transform());
            return true;
        } else if constexpr (std::is_same_v<ItemType, ConcatenateCTM>) {
            serializer.concatCTM(command.transform());
            return true;
        } else if constexpr (std::is_same_v<ItemType, SetInlineFillColor>) {
            serializer.setFillColor(command.color());
            return true;
        } else if constexpr (std::is_same_v<ItemType, SetInlineStroke>) {
            if (command.colorData())
                serializer.setStrokeColor(command.color().value());
            if (command.thickness())
                serializer.setLineWidth(*command.thickness());
            return true;
        } else if constexpr (std::is_same_v<ItemType, SetState>) {
            serializer.applyGraphicsContextStateChange(command.state());
            return true;
        } else if constexpr (std::is_same_v<ItemType, Clip>) {
            serializer.clipRect(command.rect());
            return true;
        } else if constexpr (std::is_same_v<ItemType, ClipPath>) {
            serializer.clipPath(command.path());
            return true;
        } else if constexpr (std::is_same_v<ItemType, ResetClip>) {
            serializer.resetClip();
            return true;
        } else if constexpr (std::is_same_v<ItemType, BeginTransparencyLayer>) {
            serializer.beginTransparency(command.opacity());
            return true;
        } else if constexpr (std::is_same_v<ItemType, BeginTransparencyLayerWithCompositeMode>) {
            serializer.beginTransparency(1.0f, command.compositeMode().operation, command.compositeMode().blendMode);
            return true;
        } else if constexpr (std::is_same_v<ItemType, EndTransparencyLayer>) {
            serializer.endTransparency();
            return true;
        } else if constexpr (std::is_same_v<ItemType, FillRect>) {
            if (fillBrushRenderable(serializer.state()))
                serializer.fillRect(command.rect());
            return true;
        } else if constexpr (std::is_same_v<ItemType, FillRectWithColor>) {
            serializer.fillRectColor(command.rect(), command.color());
            return true;
        } else if constexpr (std::is_same_v<ItemType, StrokeRect>) {
            if (strokeBrushRenderable(serializer.state()))
                serializer.strokeRect(command.rect(), command.lineWidth());
            return true;
        } else if constexpr (std::is_same_v<ItemType, ClearRect>) {
            serializer.clearRect(command.rect());
            return true;
        } else
            return false;
    });
}

inline void emitTemporaryFillPath(Writer& writer, const WebCore::Path& path, const WebCore::Color& color)
{
    writer.writeU8(static_cast<uint8_t>(Command::Save));
    writer.writeU8(static_cast<uint8_t>(Command::SetFillColor));
    writer.writeU32(packARGB(color));
    emitFillPath(writer, path);
    writer.writeU8(static_cast<uint8_t>(Command::Restore));
}

inline bool emitGradient(Writer& writer, const WebCore::Gradient& gradient, const WebCore::FloatRect& rect)
{
    auto writeStops = [&](const WebCore::GradientColorStops& stops) {
        auto count = std::min<size_t>(stops.size(), 255);
        writer.writeU8(static_cast<uint8_t>(count));
        size_t index = 0;
        for (const auto& stop : stops) {
            if (index++ >= count)
                break;
            writer.writeF32(stop.offset);
            writer.writeU32(packARGB(stop.color));
        }
    };

    return WTF::switchOn(gradient.data(),
        [&](const WebCore::Gradient::LinearData& data) {
            writer.writeU8(static_cast<uint8_t>(Command::FillLinearGradient));
            writer.writeF32(data.point0.x());
            writer.writeF32(data.point0.y());
            writer.writeF32(data.point1.x());
            writer.writeF32(data.point1.y());
            writer.writeF32(rect.x());
            writer.writeF32(rect.y());
            writer.writeF32(rect.width());
            writer.writeF32(rect.height());
            writeStops(gradient.stops().sorted());
            return true;
        },
        [&](const WebCore::Gradient::RadialData& data) {
            writer.writeU8(static_cast<uint8_t>(Command::FillRadialGradient));
            writer.writeF32(data.point1.x());
            writer.writeF32(data.point1.y());
            writer.writeF32(data.endRadius);
            writer.writeF32(rect.x());
            writer.writeF32(rect.y());
            writer.writeF32(rect.width());
            writer.writeF32(rect.height());
            writeStops(gradient.stops().sorted());
            return true;
        },
        [&](const WebCore::Gradient::ConicData&) {
            return false;
        }
    );
}

inline WebCore::Path makeEllipsePath(const WebCore::FloatRect& rect)
{
    WebCore::Path path;
    path.addEllipseInRect(rect);
    return path;
}

inline WebCore::Path makeRoundedRectPath(const WebCore::FloatRoundedRect& roundedRect)
{
    WebCore::Path path;
    path.addRoundedRect(roundedRect);
    return path;
}

inline std::optional<float> uniformRoundedRectRadius(const WebCore::FloatRoundedRect& roundedRect)
{
    auto approximatelyEqual = [](float a, float b) {
        return fabsf(a - b) < 0.01f;
    };

    const auto& radii = roundedRect.radii();
    const auto& topLeft = radii.topLeft();
    const auto& topRight = radii.topRight();
    const auto& bottomLeft = radii.bottomLeft();
    const auto& bottomRight = radii.bottomRight();

    if (!approximatelyEqual(topLeft.width(), topLeft.height()))
        return std::nullopt;

    auto sameCorner = [&](const WebCore::FloatSize& corner) {
        return approximatelyEqual(corner.width(), topLeft.width()) && approximatelyEqual(corner.height(), topLeft.height());
    };

    if (!sameCorner(topRight) || !sameCorner(bottomLeft) || !sameCorner(bottomRight))
        return std::nullopt;

    return topLeft.width();
}

inline void emitFillRoundedRect(Writer& writer, const WebCore::FloatRoundedRect& roundedRect)
{
    if (auto radius = uniformRoundedRectRadius(roundedRect)) {
        const auto& rect = roundedRect.rect();
        writer.writeU8(static_cast<uint8_t>(Command::FillRoundedRect));
        writer.writeF32(rect.x());
        writer.writeF32(rect.y());
        writer.writeF32(rect.width());
        writer.writeF32(rect.height());
        writer.writeF32(*radius);
        return;
    }

    emitFillPath(writer, makeRoundedRectPath(roundedRect));
}

inline float glyphPlaceholderThickness(const WebCore::Font& font)
{
    float thickness = std::max(font.fontMetrics().height(), std::max(font.maxCharWidth(), font.avgCharWidth()));
    if (thickness <= 0)
        thickness = std::max(font.platformData().size(), 1.0f);
    return thickness;
}

inline WebCore::FloatRect paddedGlyphBounds(const WebCore::Font& font, const WebCore::FloatRect& rect)
{
    float horizontalExtent = std::max(font.maxCharWidth(), font.avgCharWidth());
    horizontalExtent = std::max(horizontalExtent, font.platformData().size());
    float padX = std::clamp(horizontalExtent * 0.2f, 2.0f, 8.0f);
    float padY = std::clamp(std::max(font.fontMetrics().height(), font.platformData().size()) * 0.12f, 2.0f, 6.0f);
    return {
        rect.x() - padX,
        rect.y() - padY,
        rect.width() + padX * 2.0f,
        rect.height() + padY * 2.0f
    };
}

inline std::optional<uint32_t> glyphPlaceholderColor(const SerializationState& state)
{
    auto drawingMode = state.current().state.textDrawingMode();

    if (drawingMode.contains(WebCore::TextDrawingMode::Fill) && fillBrushRenderable(state))
        return encodeBrushColorOrPlaceholder(state.current().state.fillBrush(), placeholderFillBrushARGB, "text fill brush");
    if (drawingMode.contains(WebCore::TextDrawingMode::Stroke) && strokeBrushRenderable(state))
        return encodeBrushColorOrPlaceholder(state.current().state.strokeBrush(), placeholderStrokeBrushARGB, "text stroke brush");

    return std::nullopt;
}

inline std::optional<WebCore::FloatRect> glyphRunAdvanceBounds(const WebCore::Font& font, std::span<const WebCore::GlyphBufferAdvance> advances, const WebCore::FloatPoint& anchor)
{
    auto glyphCount = advances.size();
    if (!glyphCount)
        return std::nullopt;

    auto pen = anchor;
    float minX = pen.x();
    float maxX = pen.x();
    float minY = pen.y();
    float maxY = pen.y();
    float totalAdvanceX = 0;
    float totalAdvanceY = 0;

    auto includePoint = [&](const WebCore::FloatPoint& point) {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    };

    includePoint(pen);

    for (size_t i = 0; i < glyphCount; ++i) {
        auto advance = advances[i];
        totalAdvanceX += WebCore::width(advance);
        totalAdvanceY += WebCore::height(advance);
        pen.move(WebCore::size(advance));
        includePoint(pen);
    }

    float ascent = font.fontMetrics().ascent();
    float descent = font.fontMetrics().descent();
    float thickness = glyphPlaceholderThickness(font);
    bool isVerticalRun = font.platformData().orientation() == WebCore::FontOrientation::Vertical || fabsf(totalAdvanceY) > fabsf(totalAdvanceX);

    float rectX = minX;
    float rectY = minY - ascent;
    float rectWidth = maxX - minX;
    float rectHeight = (maxY - minY) + ascent + descent;

    if (isVerticalRun) {
        rectX -= thickness * 0.5f;
        rectWidth += thickness;
        if (rectHeight <= 0)
            rectHeight = thickness;
    } else if (rectWidth <= 0)
        rectWidth = std::max(thickness * 0.5f, 1.0f);

    if (rectHeight <= 0)
        rectHeight = std::max(ascent + descent, thickness);

    return paddedGlyphBounds(font, WebCore::FloatRect(rectX, rectY, rectWidth, rectHeight));
}

inline std::optional<WebCore::FloatRect> glyphRunBounds(const WebCore::Font& font, std::span<const WebCore::GlyphBufferGlyph> glyphs, std::span<const WebCore::GlyphBufferAdvance> advances, const WebCore::FloatPoint& anchor)
{
    auto glyphCount = std::min(glyphs.size(), advances.size());
    if (!glyphCount)
        return std::nullopt;

#if USE(CORE_TEXT)
    if (font.platformData().orientation() == WebCore::FontOrientation::Horizontal) {
        auto ctFont = font.platformData().ctFont();
        if (ctFont) {
            Vector<CGGlyph, 256> coreTextGlyphs;
            coreTextGlyphs.reserveInitialCapacity(glyphCount);
            for (size_t i = 0; i < glyphCount; ++i)
                coreTextGlyphs.append(static_cast<CGGlyph>(glyphs[i]));

            Vector<CGRect, 256> coreTextBounds;
            coreTextBounds.grow(glyphCount);
            CTFontGetBoundingRectsForGlyphs(ctFont, kCTFontOrientationHorizontal, coreTextGlyphs.span().data(), coreTextBounds.mutableSpan().data(), glyphCount);

            auto pen = anchor;
            std::optional<WebCore::FloatRect> runBounds;
            for (size_t i = 0; i < glyphCount; ++i) {
                auto bounds = coreTextBounds[i];
                if (!CGRectIsNull(bounds) && !CGRectIsEmpty(bounds)) {
                    WebCore::FloatRect floatBounds(bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height);
                    floatBounds.moveBy(pen);
                    if (runBounds)
                        runBounds->unite(floatBounds);
                    else
                        runBounds = floatBounds;
                }
                pen.move(WebCore::size(advances[i]));
            }

            if (runBounds && !runBounds->isEmpty())
                return paddedGlyphBounds(font, *runBounds);
        }
    }
#else
    if (font.platformData().orientation() == WebCore::FontOrientation::Horizontal) {
        auto pen = anchor;
        float ascent = font.fontMetrics().ascent();
        float descent = font.fontMetrics().descent();
        float rightPad = std::clamp(std::max(font.maxCharWidth(), font.platformData().size()) * 0.45f, 4.0f, 16.0f);
        float leftPad = std::clamp(font.platformData().size() * 0.12f, 1.0f, 4.0f);
        float topPad = std::clamp(ascent * 0.12f, 1.0f, 4.0f);
        float bottomPad = std::clamp(descent * 0.2f, 1.0f, 5.0f);
        float minX = anchor.x() - leftPad;
        float maxX = anchor.x();
        for (size_t i = 0; i < glyphCount; ++i) {
            minX = std::min(minX, pen.x() - leftPad);
            pen.move(WebCore::size(advances[i]));
            maxX = std::max(maxX, pen.x() + rightPad);
        }
        return WebCore::FloatRect(minX, anchor.y() - ascent - topPad, std::max(maxX - minX, 1.0f), std::max(ascent + descent + topPad + bottomPad, 1.0f));
    }
#endif

    return glyphRunAdvanceBounds(font, advances.first(glyphCount), anchor);
}

inline WebCore::FloatRect glyphRasterizationBounds(const WebCore::Font& font, const WebCore::FloatRect& rect)
{
    float sidePad = std::clamp(std::max(font.maxCharWidth(), font.platformData().size()) * 0.08f, 2.0f, 6.0f);
    float topPad = std::clamp(std::max(font.fontMetrics().ascent() * 0.22f, font.platformData().size() * 0.18f), 4.0f, 10.0f);
    float bottomPad = std::clamp(std::max(font.fontMetrics().descent() * 0.2f, font.platformData().size() * 0.08f), 2.0f, 6.0f);

    return {
        rect.x() - sidePad,
        rect.y() - topPad,
        rect.width() + sidePad * 2.0f,
        rect.height() + topPad + bottomPad
    };
}

inline std::optional<WebCore::FloatRect> glyphRunBounds(const WebCore::DisplayList::DrawGlyphs& item)
{
    auto glyphCount = std::min(item.glyphs().size(), item.advances().size());
    if (!glyphCount)
        return std::nullopt;

    auto fontRef = item.font();
    return glyphRunBounds(fontRef.get(), item.glyphs().span().first(glyphCount), item.advances().span().first(glyphCount), item.localAnchor());
}

inline void emitDrawGlyphsPlaceholder(Writer& writer, const SerializationState& state, const WebCore::DisplayList::DrawGlyphs& item)
{
    auto color = glyphPlaceholderColor(state);
    if (!color)
        return;

    auto bounds = glyphRunBounds(item);
    if (!bounds || bounds->isEmpty())
        return;

    writer.writeU8(static_cast<uint8_t>(Command::DrawGlyphs));
    writer.writeU32(*color);
    writer.writeF32(bounds->x());
    writer.writeF32(bounds->y());
    writer.writeF32(bounds->width());
    writer.writeF32(bounds->height());
}

inline void emitDrawGlyphsPlaceholder(Writer& writer, const SerializationState& state, const WebCore::Font& font, std::span<const WebCore::GlyphBufferGlyph> glyphs, std::span<const WebCore::GlyphBufferAdvance> advances, const WebCore::FloatPoint& anchor)
{
    auto color = glyphPlaceholderColor(state);
    if (!color)
        return;

    auto bounds = glyphRunBounds(font, glyphs, advances, anchor);
    if (!bounds || bounds->isEmpty())
        return;

    writer.writeU8(static_cast<uint8_t>(Command::DrawGlyphs));
    writer.writeU32(*color);
    writer.writeF32(bounds->x());
    writer.writeF32(bounds->y());
    writer.writeF32(bounds->width());
    writer.writeF32(bounds->height());
}

inline bool configureTextRasterContext(WebCore::GraphicsContext& context, const SerializationState& state)
{
    const auto& graphicsState = state.current().state;
    auto drawingMode = graphicsState.textDrawingMode();
    if (!drawingMode)
        return false;

    if (drawingMode.contains(WebCore::TextDrawingMode::Fill)) {
        if (!fillBrushRenderable(state) || !hasSolidColor(graphicsState.fillBrush()))
            return false;
        context.setFillColor(graphicsState.fillBrush().color());
    }

    if (drawingMode.contains(WebCore::TextDrawingMode::Stroke)) {
        if (!strokeBrushRenderable(state) || !hasSolidColor(graphicsState.strokeBrush()))
            return false;
        context.setStrokeColor(graphicsState.strokeBrush().color());
        context.setStrokeThickness(graphicsState.strokeThickness());
    }

    context.setTextDrawingMode(drawingMode);
    context.setShouldAntialias(graphicsState.shouldAntialias());
    context.setShouldSmoothFonts(graphicsState.shouldSmoothFonts());
    context.setShouldSubpixelQuantizeFonts(graphicsState.shouldSubpixelQuantizeFonts());
    context.setShadowsIgnoreTransforms(graphicsState.shadowsIgnoreTransforms());
    context.setDrawLuminanceMask(graphicsState.drawLuminanceMask());
    return true;
}

inline bool configureTextMaskRasterContext(WebCore::GraphicsContext& context, const SerializationState& state)
{
    const auto& graphicsState = state.current().state;
    auto drawingMode = graphicsState.textDrawingMode();
    if (!drawingMode || !drawingMode.contains(WebCore::TextDrawingMode::Fill) || drawingMode.contains(WebCore::TextDrawingMode::Stroke))
        return false;
    if (!fillBrushRenderable(state) || !hasSolidColor(graphicsState.fillBrush()))
        return false;

    context.setFillColor(WebCore::Color::black);
    context.setTextDrawingMode(WebCore::TextDrawingMode::Fill);
    context.setShouldAntialias(graphicsState.shouldAntialias());
    context.setShouldSmoothFonts(graphicsState.shouldSmoothFonts());
    context.setShouldSubpixelQuantizeFonts(graphicsState.shouldSubpixelQuantizeFonts());
    context.setShadowsIgnoreTransforms(graphicsState.shadowsIgnoreTransforms());
    context.setDrawLuminanceMask(false);
    return true;
}

inline bool emitDrawGlyphsImageResource(Writer& writer, SerializationState& state, const WebCore::Font& font, std::span<const WebCore::GlyphBufferGlyph> glyphs, std::span<const WebCore::GlyphBufferAdvance> advances, const WebCore::FloatPoint& anchor, WebCore::FontSmoothingMode smoothingMode)
{
    auto glyphCount = std::min(glyphs.size(), advances.size());
    if (!glyphCount)
        return false;

    auto usedGlyphs = glyphs.first(glyphCount);
    auto usedAdvances = advances.first(glyphCount);
    auto bounds = glyphRunBounds(font, usedGlyphs, usedAdvances, anchor);
    if (!bounds || bounds->isEmpty())
        return false;

    auto rasterBounds = glyphRasterizationBounds(font, *bounds);

    auto imageBuffer = WebCore::ImageBuffer::create(rasterizedImageBufferSize(rasterBounds), WebCore::RenderingMode::Unaccelerated, WebCore::RenderingPurpose::Unspecified, 1, WebCore::DestinationColorSpace::SRGB(), WebCore::PixelFormat::BGRA8);
    if (!imageBuffer)
        return false;

    auto& context = imageBuffer->context();
    if (!configureTextRasterContext(context, state))
        return false;
    context.clearRect(rasterizedImageSourceRect(rasterBounds));
    // The offscreen buffer is sized to rasterBounds, so map that padded rect into local buffer
    // space before rasterizing. This keeps the emitted destination rect and the rasterized ink in
    // the same coordinate system.
    context.translate(-rasterBounds.x(), -rasterBounds.y());
    context.drawGlyphs(font, usedGlyphs, usedAdvances, anchor, smoothingMode);
    RefPtr<WebCore::PixelBuffer> pixelBuffer = copyPixelBufferFromImageBuffer(*imageBuffer);
    if (!pixelBuffer)
        return false;
    return emitTrimmedImageResource(writer, state, *pixelBuffer, rasterBounds);
}

inline bool emitDrawGlyphsMaskResource(Writer& writer, SerializationState& state, const WebCore::Font& font, std::span<const WebCore::GlyphBufferGlyph> glyphs, std::span<const WebCore::GlyphBufferAdvance> advances, const WebCore::FloatPoint& anchor, WebCore::FontSmoothingMode smoothingMode)
{
    auto glyphCount = std::min(glyphs.size(), advances.size());
    if (!glyphCount)
        return false;

    auto usedGlyphs = glyphs.first(glyphCount);
    auto usedAdvances = advances.first(glyphCount);
    auto bounds = glyphRunBounds(font, usedGlyphs, usedAdvances, anchor);
    if (!bounds || bounds->isEmpty())
        return false;

    auto rasterBounds = glyphRasterizationBounds(font, *bounds);
    auto imageBuffer = WebCore::ImageBuffer::create(rasterizedImageBufferSize(rasterBounds), WebCore::RenderingMode::Unaccelerated, WebCore::RenderingPurpose::Unspecified, 1, WebCore::DestinationColorSpace::SRGB(), WebCore::PixelFormat::BGRA8);
    if (!imageBuffer)
        return false;

    auto& context = imageBuffer->context();
    if (!configureTextMaskRasterContext(context, state))
        return false;
    context.clearRect(rasterizedImageSourceRect(rasterBounds));
    context.translate(-rasterBounds.x(), -rasterBounds.y());
    context.drawGlyphs(font, usedGlyphs, usedAdvances, anchor, smoothingMode);
    RefPtr<WebCore::PixelBuffer> pixelBuffer = copyPixelBufferFromImageBuffer(*imageBuffer);
    if (!pixelBuffer)
        return false;
    return emitTrimmedMaskResource(writer, state, *pixelBuffer, rasterBounds);
}

inline bool emitDrawGlyphsFontResource(Writer& writer, SerializationState& state, const WebCore::Font& font, std::span<const WebCore::GlyphBufferGlyph> glyphs, std::span<const WebCore::GlyphBufferAdvance> advances, const WebCore::FloatPoint& anchor, WebCore::FontSmoothingMode)
{
#if !USE(CORE_TEXT)
    UNUSED_PARAM(writer);
    UNUSED_PARAM(state);
    UNUSED_PARAM(font);
    UNUSED_PARAM(glyphs);
    UNUSED_PARAM(advances);
    UNUSED_PARAM(anchor);
    return false;
#else
    auto glyphCount = std::min(glyphs.size(), advances.size());
    if (!glyphCount || glyphCount > UINT16_MAX)
        return false;

    const auto& graphicsState = state.current().state;
    auto drawingMode = graphicsState.textDrawingMode();
    if (!drawingMode || !drawingMode.contains(WebCore::TextDrawingMode::Fill) || drawingMode.contains(WebCore::TextDrawingMode::Stroke))
        return false;
    if (!fillBrushRenderable(state) || !hasSolidColor(graphicsState.fillBrush()))
        return false;

    auto ctFont = font.platformData().ctFont();
    if (!ctFont)
        return false;

    uint16_t fontId = state.fontIdFor(ctFont);
    if (!fontId) {
        auto familyName = adoptCF(CTFontCopyFamilyName(ctFont));
        if (!familyName)
            return false;

        auto utf8Name = String(familyName.get()).utf8();
        if (utf8Name.length() > UINT16_MAX)
            return false;

        fontId = state.defineFontId(ctFont);
        if (!fontId)
            return false;

        writer.writeU8(static_cast<uint8_t>(Command::DefineFont));
        writer.writeU16(fontId);
        writer.writeU16(static_cast<uint16_t>(utf8Name.length()));
        for (auto byte : utf8Name.span())
            writer.writeU8(static_cast<uint8_t>(byte));
        writer.writeU16(0);
    }

    Vector<uint16_t, 256> glyphIds;
    Vector<float, 256> offsetsX;
    Vector<float, 256> offsetsY;
    Vector<uint32_t, 256> codepoints;
    glyphIds.reserveInitialCapacity(glyphCount);
    offsetsX.reserveInitialCapacity(glyphCount);
    offsetsY.reserveInitialCapacity(glyphCount);
    codepoints.reserveInitialCapacity(glyphCount);

    // Reverse-map glyph IDs to Unicode codepoints using CoreText.
    // This allows nutjob to look up the correct glyphs in its own
    // substitute font (e.g., Noto Serif instead of Georgia).
    for (size_t i = 0; i < glyphCount; ++i)
        codepoints.append(state.lookupCodepoint(ctFont, static_cast<uint16_t>(glyphs[i])));

    float penX = 0;
    float penY = 0;
    for (size_t i = 0; i < glyphCount; ++i) {
        glyphIds.append(static_cast<uint16_t>(glyphs[i]));
        offsetsX.append(penX);
        offsetsY.append(penY);
        penX += WebCore::width(advances[i]);
        penY += WebCore::height(advances[i]);
    }

    writer.writeU8(static_cast<uint8_t>(Command::DrawTextRun));
    writer.writeU32(packARGB(graphicsState.fillBrush().color()));
    writer.writeU16(fontId);
    writer.writeF32(font.platformData().size());
    writer.writeF32(anchor.x());
    writer.writeF32(anchor.y());
    writer.writeU16(static_cast<uint16_t>(glyphCount));
    for (size_t i = 0; i < glyphCount; ++i) {
        writer.writeU16(glyphIds[i]);
        writer.writeF32(offsetsX[i]);
        writer.writeF32(offsetsY[i]);
        writer.writeU32(codepoints[i]);  // Unicode codepoint for nutjob remapping
    }
    return true;
#endif
}

inline DrawGlyphsEmissionPath emitDrawGlyphsResource(Writer& writer, SerializationState& state, const WebCore::Font& font, std::span<const WebCore::GlyphBufferGlyph> glyphs, std::span<const WebCore::GlyphBufferAdvance> advances, const WebCore::FloatPoint& anchor, WebCore::FontSmoothingMode smoothingMode)
{
    if (emitDrawGlyphsFontResource(writer, state, font, glyphs, advances, anchor, smoothingMode))
        return DrawGlyphsEmissionPath::Font;
    if (emitDrawGlyphsMaskResource(writer, state, font, glyphs, advances, anchor, smoothingMode))
        return DrawGlyphsEmissionPath::Mask;
    if (emitDrawGlyphsImageResource(writer, state, font, glyphs, advances, anchor, smoothingMode))
        return DrawGlyphsEmissionPath::Image;
    return DrawGlyphsEmissionPath::None;
}

inline void emitDrawPath(Writer& writer, const SerializationState& state, const WebCore::Path& path)
{
    if (fillBrushRenderable(state))
        emitFillPath(writer, path);
    if (strokeBrushRenderable(state))
        emitStrokePath(writer, path);
}

inline void serializeItem(Writer& writer, SerializationState& state, const WebCore::IntSize& surfaceSize, const WebCore::DisplayList::Item& item, FrameCounters* counters = nullptr, AdapterReport* report = nullptr)
{
    PhaseOneOperationSerializer phaseOne(writer, state, counters, report);
    if (serializePhaseOneDisplayListItem(phaseOne, item))
        return;

    using namespace WebCore;
    using namespace WebCore::DisplayList;

    auto note = [&](const char* operation, AdapterHandlingStrategy strategy) {
        noteHandling(report, operation, strategy);
    };

    WTF::switchOn(item, [&]<typename ItemType>(const ItemType& command) {
        if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::ClipRoundedRect>) {
            note("clip-rounded-rect", AdapterHandlingStrategy::Lowered);
            emitClipPath(writer, makeRoundedRectPath(command.rect()));
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::ClipOutRoundedRect>) {
            if (emitClipOutPathResource(writer, state, surfaceSize, makeRoundedRectPath(command.rect())))
                note("clip-out-rounded-rect", AdapterHandlingStrategy::Prerasterized);
            else {
                note("clip-out-rounded-rect", AdapterHandlingStrategy::UnsupportedNoOp);
                strictUnsupported("DisplayList::ClipOutRoundedRect", "inverse clip mask fallback unavailable");
            }
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::ClipPath>) {
            note("clip-path", AdapterHandlingStrategy::Direct);
            emitClipPath(writer, command.path());
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::ClipToImageBuffer>) {
            if (!emitClipImageBufferResource(writer, state, command.imageBuffer(), command.destinationRect())) {
                note("clip-to-image-buffer", AdapterHandlingStrategy::Lossy);
                strictUnsupported("DisplayList::ClipToImageBuffer", "clip-rect fallback");
                emitClipRect(writer, command.destinationRect());
            } else
                note("clip-to-image-buffer", AdapterHandlingStrategy::Prerasterized);
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::DrawRect>) {
            if (strokeBrushRenderable(state)) {
                note("draw-rect", AdapterHandlingStrategy::Direct);
                emitTemporaryLineWidthRect(writer, state, command.rect(), command.borderThickness());
            }
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::DrawLine>) {
            note("draw-line", AdapterHandlingStrategy::Direct);
            writer.writeU8(static_cast<uint8_t>(Command::DrawLine));
            writer.writeF32(command.point1().x());
            writer.writeF32(command.point1().y());
            writer.writeF32(command.point2().x());
            writer.writeF32(command.point2().y());
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::DrawPath>) {
            note("draw-path", AdapterHandlingStrategy::Direct);
            emitDrawPath(writer, state, command.path());
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::DrawGlyphs>) {
            auto fontRef = command.font();
            auto emissionPath = emitDrawGlyphsResource(writer, state, fontRef.get(), command.glyphs().span(), command.advances().span(), command.localAnchor(), command.fontSmoothingMode());
            if (emissionPath == DrawGlyphsEmissionPath::None) {
                note("draw-glyphs", AdapterHandlingStrategy::Placeholder);
                strictUnsupported("DisplayList::DrawGlyphs", "placeholder fallback");
                emitDrawGlyphsPlaceholder(writer, state, command);
            } else if (emissionPath == DrawGlyphsEmissionPath::Font)
                note("draw-glyphs", AdapterHandlingStrategy::Direct);
            else
                note("draw-glyphs", AdapterHandlingStrategy::Prerasterized);
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::DrawImageBuffer>) {
            if (!emitImageBufferResource(writer, state, command.imageBuffer(), command.destinationRect(), command.source()))
                emitPlaceholderFallback(writer, command.destinationRect(), 0xFF9CA3AF, "DisplayList::DrawImageBuffer", report, "draw-image-buffer");
            else
                note("draw-image-buffer", AdapterHandlingStrategy::Lowered);
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::DrawFilteredImageBuffer>) {
            WebCore::FilterResults results;
            if (!emitFilteredImageBufferResource(writer, state, command.sourceImage(), command.sourceImageRect(), command.filter(), results))
                emitPlaceholderFallback(writer, command.sourceImageRect(), 0xFF6B7280, "DisplayList::DrawFilteredImageBuffer", report, "draw-filtered-image-buffer");
            else
                note("draw-filtered-image-buffer", AdapterHandlingStrategy::Prerasterized);
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::DrawNativeImage>) {
            if (!emitNativeImageResource(writer, state, command.nativeImage(), command.destinationRect(), command.source()))
                emitPlaceholderFallback(writer, command.destinationRect(), 0xFF9CA3AF, "DisplayList::DrawNativeImage", report, "draw-native-image");
            else
                note("draw-native-image", AdapterHandlingStrategy::Lowered);
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::DrawSystemImage>) {
            if (!emitRasterizedImageResource(writer, state, command.destinationRect(), [&](WebCore::GraphicsContext& context) {
                context.drawSystemImage(const_cast<WebCore::SystemImage&>(command.systemImage()), command.destinationRect());
            }))
                emitPlaceholderFallback(writer, command.destinationRect(), 0xFFE5E7EB, "DisplayList::DrawSystemImage", report, "draw-system-image");
            else
                note("draw-system-image", AdapterHandlingStrategy::Prerasterized);
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::DrawPatternNativeImage>) {
            if (!emitRasterizedImageResource(writer, state, command.destRect(), [&](WebCore::GraphicsContext& context) {
                context.drawPattern(command.nativeImage(), command.destRect(), command.tileRect(), command.patternTransform(), command.phase(), command.spacing(), command.options());
            }))
                emitPlaceholderFallback(writer, command.destRect(), 0xFF60A5FA, "DisplayList::DrawPatternNativeImage", report, "draw-pattern-native-image");
            else
                note("draw-pattern-native-image", AdapterHandlingStrategy::Prerasterized);
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::DrawPatternImageBuffer>) {
            if (!emitRasterizedImageResource(writer, state, command.destRect(), [&](WebCore::GraphicsContext& context) {
                context.drawPattern(command.imageBuffer(), command.destRect(), command.tileRect(), command.patternTransform(), command.phase(), command.spacing(), command.options());
            }))
                emitPlaceholderFallback(writer, command.destRect(), 0xFF60A5FA, "DisplayList::DrawPatternImageBuffer", report, "draw-pattern-image-buffer");
            else
                note("draw-pattern-image-buffer", AdapterHandlingStrategy::Prerasterized);
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::FillRectWithGradient>) {
            if (!emitGradient(writer, command.gradient(), command.rect()))
                emitPlaceholderFallback(writer, command.rect(), placeholderFillBrushARGB, "DisplayList::FillRectWithGradient", report, "fill-rect-with-gradient");
            else
                note("fill-rect-with-gradient", AdapterHandlingStrategy::Direct);
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::FillRectWithGradientAndSpaceTransform>) {
            if (!command.gradientSpaceTransform().isIdentity()) {
                note("fill-rect-with-gradient-and-space-transform", AdapterHandlingStrategy::Lossy);
                strictUnsupported("DisplayList::FillRectWithGradientAndSpaceTransform", "ignored gradient space transform");
            }
            if (!emitGradient(writer, command.gradient(), command.rect()))
                emitPlaceholderFallback(writer, command.rect(), placeholderFillBrushARGB, "DisplayList::FillRectWithGradientAndSpaceTransform", report, "fill-rect-with-gradient-and-space-transform");
            else if (command.gradientSpaceTransform().isIdentity())
                note("fill-rect-with-gradient-and-space-transform", AdapterHandlingStrategy::Direct);
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::FillCompositedRect>) {
            note("fill-composited-rect", AdapterHandlingStrategy::Lowered);
            emitFillRectColor(writer, command.rect(), command.color());
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::FillRectWithRoundedHole>) {
            note("fill-rect-with-rounded-hole", AdapterHandlingStrategy::Lossy);
            strictUnsupported("DisplayList::FillRectWithRoundedHole", "lossy fill-rect fallback");
            emitFillRectColor(writer, command.rect(), command.color());
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::FillRoundedRect>) {
            note("fill-rounded-rect", AdapterHandlingStrategy::Lowered);
            writer.writeU8(static_cast<uint8_t>(Command::Save));
            writer.writeU8(static_cast<uint8_t>(Command::SetFillColor));
            writer.writeU32(packARGB(command.color()));
            emitFillRoundedRect(writer, command.roundedRect());
            writer.writeU8(static_cast<uint8_t>(Command::Restore));
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::FillPath>) {
            if (fillBrushRenderable(state)) {
                note("fill-path", AdapterHandlingStrategy::Direct);
                emitFillPath(writer, command.path());
            }
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::FillEllipse>) {
            if (fillBrushRenderable(state)) {
                note("fill-ellipse", AdapterHandlingStrategy::Lowered);
                emitFillPath(writer, makeEllipsePath(command.rect()));
            }
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::StrokePath>) {
            if (strokeBrushRenderable(state)) {
                note("stroke-path", AdapterHandlingStrategy::Direct);
                emitStrokePath(writer, command.path());
            }
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::StrokeEllipse>) {
            if (strokeBrushRenderable(state)) {
                note("stroke-ellipse", AdapterHandlingStrategy::Lowered);
                emitStrokePath(writer, makeEllipsePath(command.rect()));
            }
        } else if constexpr (std::is_same_v<ItemType, WebCore::DisplayList::ApplyDeviceScaleFactor>) {
            note("apply-device-scale-factor", AdapterHandlingStrategy::Lowered);
            // Device scale already stripped from the serializer's initial CTM.
            // Do not emit or update — nutjob always works in CSS pixels.
            return;
        }
    });
}

inline WebCore::FloatPoint computeLayerPageOrigin(const WebCore::PlatformCALayer* layer)
{
#if NUTJOB_TAP_DISABLE_LAYER_METADATA
    UNUSED_PARAM(layer);
    return { };
#else
    float x = 0, y = 0;
    for (auto* current = layer; current; current = current->superlayer()) {
        auto bounds = current->bounds();
        auto anchor = current->anchorPoint();
        auto position = current->position();
        x += position.x() - anchor.x() * bounds.width() - bounds.x();
        y += position.y() - anchor.y() * bounds.height() - bounds.y();
    }
    return { x, y };
#endif
}

inline WebCore::FloatRect pageRectForLayerBounds(const WebCore::PlatformCALayer* layer)
{
#if NUTJOB_TAP_DISABLE_LAYER_METADATA
    UNUSED_PARAM(layer);
    return { };
#else
    auto origin = computeLayerPageOrigin(layer);
    auto bounds = layer->bounds();
    return { origin.x() + bounds.x(), origin.y() + bounds.y(), bounds.width(), bounds.height() };
#endif
}

inline float computeEffectiveOpacity(const WebCore::PlatformCALayer* layer)
{
#if NUTJOB_TAP_DISABLE_LAYER_METADATA
    UNUSED_PARAM(layer);
    return 1.0f;
#else
    float opacity = 1.0f;
    for (auto* current = layer; current; current = current->superlayer()) {
        if (current->isHidden() || current->contentsHidden())
            return 0.0f;
        opacity *= current->opacity();
        if (opacity <= 0.0f)
            return 0.0f;
    }
    return opacity;
#endif
}

inline WebCore::BlendMode computeSurfaceBlendMode(const WebCore::PlatformCALayer* layer)
{
#if NUTJOB_TAP_DISABLE_LAYER_METADATA
    UNUSED_PARAM(layer);
    return WebCore::BlendMode::Normal;
#else
    if (!layer)
        return WebCore::BlendMode::Normal;

    switch (layer->type()) {
    case WebCore::PlatformCALayer::Type::Remote:
    case WebCore::PlatformCALayer::Type::RemoteCustom:
    case WebCore::PlatformCALayer::Type::RemoteHost:
    case WebCore::PlatformCALayer::Type::RemoteModel:
        return static_cast<const WebKit::PlatformCALayerRemote*>(layer)->properties().blendMode;
    case WebCore::PlatformCALayer::Type::Cocoa:
        return WebCore::BlendMode::Normal;
    }

    return WebCore::BlendMode::Normal;
#endif
}

inline uint8_t computeSurfaceFlags(const WebCore::PlatformCALayer* layer)
{
#if NUTJOB_TAP_DISABLE_LAYER_METADATA
    UNUSED_PARAM(layer);
    return 0;
#else
    uint8_t flags = 0;
    if (layer && layer->isOpaque())
        flags |= 1 << 0;
    return flags;
#endif
}

inline WebCore::FloatRect computeCompositeClipRect(const WebCore::PlatformCALayer* layer, const WebCore::FloatPoint& origin, const WebCore::IntSize& size)
{
#if NUTJOB_TAP_DISABLE_LAYER_METADATA
    UNUSED_PARAM(layer);
    return { origin.x(), origin.y(), static_cast<float>(size.width()), static_cast<float>(size.height()) };
#else
    WebCore::FloatRect clip(origin.x(), origin.y(), size.width(), size.height());
    for (auto* current = layer ? layer->superlayer() : nullptr; current; current = current->superlayer()) {
        if (!current->masksToBounds())
            continue;
        clip.intersect(pageRectForLayerBounds(current));
        if (clip.isEmpty())
            break;
    }
    return clip;
#endif
}

inline bool findPaintOrder(const WebCore::PlatformCALayer* current, const WebCore::PlatformCALayer* target, uint32_t& next, uint32_t& result)
{
#if NUTJOB_TAP_DISABLE_LAYER_METADATA
    UNUSED_PARAM(current);
    UNUSED_PARAM(target);
    UNUSED_PARAM(next);
    UNUSED_PARAM(result);
    return false;
#else
    auto currentIndex = next++;
    if (current == target) {
        result = currentIndex;
        return true;
    }

    auto children = current->sublayersForLogging();
    for (const auto& child : children) {
        auto* childLayer = child.get();
        if (!childLayer)
            continue;
        if (findPaintOrder(childLayer, target, next, result))
            return true;
    }
    return false;
#endif
}

inline uint32_t computePaintOrder(const WebCore::PlatformCALayer* layer)
{
#if NUTJOB_TAP_DISABLE_LAYER_METADATA
    UNUSED_PARAM(layer);
    return 0;
#else
    if (!layer)
        return 0;

    auto* root = layer;
    while (auto* parent = root->superlayer())
        root = parent;

    uint32_t next = 0;
    uint32_t result = 0;
    findPaintOrder(root, layer, next, result);
    return result;
#endif
}

inline uint64_t packSurfaceID(const WebCore::PlatformCALayer* layer)
{
#if NUTJOB_TAP_DISABLE_LAYER_METADATA
    UNUSED_PARAM(layer);
    return 0;
#else
    if (!layer)
        return 0;
    return layer->layerID().object().toUInt64();
#endif
}

inline FrameMetadata computeFrameMetadata(const WebCore::IntSize& size, const WebCore::FloatRect& dirtyRect, const WebCore::PlatformCALayer* layer)
{
    FrameMetadata metadata;
    metadata.size = size;
#if NUTJOB_TAP_DISABLE_LAYER_METADATA
    UNUSED_PARAM(layer);
    metadata.compositeClipRect = WebCore::FloatRect(0, 0, size.width(), size.height());
    metadata.effectiveOpacity = 1.0f;
    metadata.paintOrder = 0;
    metadata.blendMode = static_cast<uint8_t>(WebCore::BlendMode::Normal);
    metadata.surfaceFlags = 0;
#else
    if (layer) {
        metadata.origin = computeLayerPageOrigin(layer);
        auto backgroundColor = layer->backgroundColor();
        if (backgroundColor.isValid())
            metadata.backgroundColorARGB = packARGB(backgroundColor);
    }
    metadata.surfaceID = packSurfaceID(layer);
    metadata.maskSurfaceID = packSurfaceID(layer ? layer->maskLayer() : nullptr);
    metadata.compositeClipRect = computeCompositeClipRect(layer, metadata.origin, size);
    metadata.effectiveOpacity = computeEffectiveOpacity(layer);
    metadata.paintOrder = computePaintOrder(layer);
    metadata.blendMode = static_cast<uint8_t>(computeSurfaceBlendMode(layer));
    metadata.surfaceFlags = computeSurfaceFlags(layer);
#endif
    auto contentOffset = harnessContentOffset();
    metadata.origin.move(-contentOffset.x(), -contentOffset.y());
    metadata.compositeClipRect.move(-contentOffset.x(), -contentOffset.y());
    metadata.dirtyRect = dirtyRect;
    if (metadata.dirtyRect.isEmpty())
        metadata.dirtyRect = WebCore::FloatRect(0, 0, size.width(), size.height());
    return metadata;
}

inline WebCore::AffineTransform normalizedInitialCTM(const WebCore::AffineTransform& initialCTM, float backingScale, const WebCore::FloatPoint& origin, const WebCore::IntSize& size)
{
    UNUSED_PARAM(initialCTM);
    UNUSED_PARAM(backingScale);
    UNUSED_PARAM(origin);
    UNUSED_PARAM(size);
    // The mirrored nutjob stream is surface-local CSS space. FRAME_BEGIN carries the
    // surface placement in the composed page, and subsequent live translate calls
    // carry tile/page offsets. The backing-store base CTM only adds device-scale and
    // flipped-CG setup that should not leak into the protocol.
    return { };
}

inline void emitFrameBegin(Writer& writer, const FrameMetadata& metadata)
{
    writer.writeU8(static_cast<uint8_t>(Command::FrameBegin));
    writer.writeU16(clampDimension(metadata.size.width()));
    writer.writeU16(clampDimension(metadata.size.height()));
    writer.writeU64(metadata.surfaceID);
    writer.writeU64(metadata.maskSurfaceID);
    writer.writeF32(metadata.origin.x());
    writer.writeF32(metadata.origin.y());
    writer.writeF32(metadata.dirtyRect.x());
    writer.writeF32(metadata.dirtyRect.y());
    writer.writeF32(metadata.dirtyRect.width());
    writer.writeF32(metadata.dirtyRect.height());
    writer.writeF32(metadata.compositeClipRect.x());
    writer.writeF32(metadata.compositeClipRect.y());
    writer.writeF32(metadata.compositeClipRect.width());
    writer.writeF32(metadata.compositeClipRect.height());
    writer.writeF32(metadata.effectiveOpacity);
    writer.writeU32(metadata.paintOrder);
    writer.writeU32(metadata.backgroundColorARGB);
    writer.writeU8(metadata.blendMode);
    writer.writeU8(metadata.surfaceFlags);
}

class MirroringGraphicsContext final : public WebCore::GraphicsContext {
public:
    MirroringGraphicsContext(const WebCore::GraphicsContextState& initialState, const WebCore::AffineTransform& initialCTM, const WebCore::IntSize& size, const WebCore::FloatRect& dirtyRect, const WebCore::PlatformCALayer* layer = nullptr, float backingScale = 1.0f)
        : MirroringGraphicsContext(tapFile(), initialState, initialCTM, size, dirtyRect, layer, backingScale)
    {
    }

    MirroringGraphicsContext(FILE* output, const WebCore::GraphicsContextState& initialState, const WebCore::AffineTransform& initialCTM, const WebCore::IntSize& size, const WebCore::FloatRect& dirtyRect, const WebCore::PlatformCALayer* layer = nullptr, float backingScale = 1.0f)
        : WebCore::GraphicsContext(WebCore::GraphicsContext::IsDeferred::No, initialState)
        , m_size(size)
        , m_writer { output }
        , m_lock(tapMutex())
        , m_serializationState(initialState, normalizedInitialCTM(initialCTM, backingScale, layer ? computeLayerPageOrigin(layer) : WebCore::FloatPoint(), size))
        , m_metadata(computeFrameMetadata(size, dirtyRect, layer))
    {
        emitFrameBegin(m_writer, m_metadata);
        phaseOneSerializer().emitInitialState();
    }

    ~MirroringGraphicsContext() final
    {
        closeFrame();
    }

    AdapterReportSnapshot adapterReportSnapshot() const
    {
        return m_adapterReport.snapshot();
    }

    bool hasPlatformContext() const final { return false; }
    PlatformGraphicsContext* platformContext() const final { return nullptr; }
    const WebCore::DestinationColorSpace& colorSpace() const final { return WebCore::DestinationColorSpace::SRGB(); }

#if USE(CG)
    bool isCALayerContext() const final { return false; }
    void applyStrokePattern() final { }
    void applyFillPattern() final { }
#endif

    WebCore::RenderingMode renderingMode() const final { return WebCore::RenderingMode::Unaccelerated; }

    void didUpdateState(WebCore::GraphicsContextState& state) final
    {
        phaseOneSerializer().didUpdateGraphicsContextState(state);
    }

    void save(WebCore::GraphicsContextState::Purpose purpose = WebCore::GraphicsContextState::Purpose::SaveRestore) final
    {
        WebCore::GraphicsContext::save(purpose);
        phaseOneSerializer().save();
    }

    void restore(WebCore::GraphicsContextState::Purpose purpose = WebCore::GraphicsContextState::Purpose::SaveRestore) final
    {
        WebCore::GraphicsContext::restore(purpose);
        phaseOneSerializer().restore();
    }

    void drawRect(const WebCore::FloatRect& rect, float borderThickness = 1) final
    {
        noteContentCommand();
        noteHandling("draw-rect", AdapterHandlingStrategy::Direct);
        if (fillBrushRenderable(m_serializationState))
            ++m_counters.fillRects;
        if (fillBrushRenderable(m_serializationState)) {
            emitFillRect(m_writer, rect);
        }
        if (strokeBrushRenderable(m_serializationState)) {
            ++m_counters.strokeRects;
            emitTemporaryLineWidthRect(m_writer, m_serializationState, rect, borderThickness);
        }
    }

    void drawLine(const WebCore::FloatPoint& from, const WebCore::FloatPoint& to) final
    {
        noteContentCommand();
        if (!strokeBrushRenderable(m_serializationState))
            return;
        ++m_counters.drawLines;
        noteHandling("draw-line", AdapterHandlingStrategy::Direct);
        m_writer.writeU8(static_cast<uint8_t>(Command::DrawLine));
        m_writer.writeF32(from.x());
        m_writer.writeF32(from.y());
        m_writer.writeF32(to.x());
        m_writer.writeF32(to.y());
    }

    void drawEllipse(const WebCore::FloatRect& rect) final
    {
        noteContentCommand();
        ++m_counters.fillPaths;
        ++m_counters.strokePaths;
        noteHandling("draw-ellipse", AdapterHandlingStrategy::Lowered);
        emitDrawPath(m_writer, m_serializationState, makeEllipsePath(rect));
    }

    void drawPath(const WebCore::Path& path) final
    {
        noteContentCommand();
        ++m_counters.fillPaths;
        ++m_counters.strokePaths;
        noteHandling("draw-path", AdapterHandlingStrategy::Direct);
        emitDrawPath(m_writer, m_serializationState, path);
    }

    void fillPath(const WebCore::Path& path) final
    {
        noteContentCommand();
        if (fillBrushRenderable(m_serializationState)) {
            ++m_counters.fillPaths;
            noteHandling("fill-path", AdapterHandlingStrategy::Direct);
            emitFillPath(m_writer, path);
        }
    }

    void strokePath(const WebCore::Path& path) final
    {
        noteContentCommand();
        if (strokeBrushRenderable(m_serializationState)) {
            ++m_counters.strokePaths;
            noteHandling("stroke-path", AdapterHandlingStrategy::Direct);
            emitStrokePath(m_writer, path);
        }
    }

    void beginTransparencyLayer(float opacity) final
    {
        noteContentCommand();
        WebCore::GraphicsContext::beginTransparencyLayer(opacity);
        WebCore::GraphicsContext::save(WebCore::GraphicsContextState::Purpose::TransparencyLayer);
        phaseOneSerializer().beginTransparency(opacity);
    }

    void beginTransparencyLayer(WebCore::CompositeOperator compositeOperator, WebCore::BlendMode blendMode) final
    {
        noteContentCommand();
        WebCore::GraphicsContext::beginTransparencyLayer(compositeOperator, blendMode);
        WebCore::GraphicsContext::save(WebCore::GraphicsContextState::Purpose::TransparencyLayer);
        phaseOneSerializer().beginTransparency(1.0f, compositeOperator, blendMode);
    }

    void endTransparencyLayer() final
    {
        WebCore::GraphicsContext::endTransparencyLayer();
        WebCore::GraphicsContext::restore(WebCore::GraphicsContextState::Purpose::TransparencyLayer);
        phaseOneSerializer().endTransparency();
    }

    void applyDeviceScaleFactor(float) final
    {
        noteHandling("apply-device-scale-factor", AdapterHandlingStrategy::Lowered);
        // Nutjob normalizes the mirrored stream to CSS pixels.
    }

    using WebCore::GraphicsContext::fillRect;
    void fillRect(const WebCore::FloatRect& rect, WebCore::RequiresClipToRect) final
    {
        noteContentCommand();
        if (fillBrushRenderable(m_serializationState))
            phaseOneSerializer().fillRect(rect);
    }

    void fillRect(const WebCore::FloatRect& rect, const WebCore::Color& color) final
    {
        noteContentCommand();
        phaseOneSerializer().fillRectColor(rect, color);
    }

    void fillRect(const WebCore::FloatRect& rect, WebCore::Gradient& gradient) final
    {
        noteContentCommand();
        ++m_counters.gradients;
        if (!emitGradient(m_writer, gradient, rect)) {
            ++m_counters.fillRectColors;
            emitPlaceholderFallback("fill-rect-with-gradient", "GraphicsContext::fillRect(gradient)", rect, placeholderFillBrushARGB);
        } else
            noteHandling("fill-rect-with-gradient", AdapterHandlingStrategy::Direct);
    }

    void fillRect(const WebCore::FloatRect& rect, WebCore::Gradient& gradient, const WebCore::AffineTransform& gradientSpaceTransform, WebCore::RequiresClipToRect) final
    {
        noteContentCommand();
        ++m_counters.gradients;
        if (!gradientSpaceTransform.isIdentity())
            noteLossyFallback("fill-rect-with-gradient-and-space-transform", "GraphicsContext::fillRect(gradient,transform)", "ignored gradient space transform");
        if (!emitGradient(m_writer, gradient, rect)) {
            ++m_counters.fillRectColors;
            emitPlaceholderFallback("fill-rect-with-gradient-and-space-transform", "GraphicsContext::fillRect(gradient,transform)", rect, placeholderFillBrushARGB);
        } else if (gradientSpaceTransform.isIdentity())
            noteHandling("fill-rect-with-gradient-and-space-transform", AdapterHandlingStrategy::Direct);
    }

    void fillRoundedRectImpl(const WebCore::FloatRoundedRect& rect, const WebCore::Color& color) final
    {
        noteContentCommand();
        ++m_counters.fillRoundedRects;
        noteHandling("fill-rounded-rect", AdapterHandlingStrategy::Lowered);
        m_writer.writeU8(static_cast<uint8_t>(Command::Save));
        m_writer.writeU8(static_cast<uint8_t>(Command::SetFillColor));
        m_writer.writeU32(packARGB(color));
        emitFillRoundedRect(m_writer, rect);
        m_writer.writeU8(static_cast<uint8_t>(Command::Restore));
    }

    void fillRectWithRoundedHole(const WebCore::FloatRect& rect, const WebCore::FloatRoundedRect&, const WebCore::Color& color) final
    {
        noteContentCommand();
        ++m_counters.fillRectColors;
        noteLossyFallback("fill-rect-with-rounded-hole", "GraphicsContext::fillRectWithRoundedHole", "fill-rect fallback");
        emitFillRectColor(m_writer, rect, color);
    }

    void clearRect(const WebCore::FloatRect& rect) final
    {
        noteContentCommand();
        phaseOneSerializer().clearRect(rect);
    }

    void strokeRect(const WebCore::FloatRect& rect, float lineWidth) final
    {
        noteContentCommand();
        if (strokeBrushRenderable(m_serializationState))
            phaseOneSerializer().strokeRect(rect, lineWidth);
    }

    void fillEllipse(const WebCore::FloatRect& ellipse) final
    {
        noteContentCommand();
        if (fillBrushRenderable(m_serializationState)) {
            ++m_counters.fillPaths;
            noteHandling("fill-ellipse", AdapterHandlingStrategy::Lowered);
            emitFillPath(m_writer, makeEllipsePath(ellipse));
        }
    }

    void strokeEllipse(const WebCore::FloatRect& ellipse) final
    {
        noteContentCommand();
        if (strokeBrushRenderable(m_serializationState)) {
            ++m_counters.strokePaths;
            noteHandling("stroke-ellipse", AdapterHandlingStrategy::Lowered);
            emitStrokePath(m_writer, makeEllipsePath(ellipse));
        }
    }

    void resetClip() final
    {
        noteContentCommand();
        phaseOneSerializer().resetClip();
    }

    void clip(const WebCore::FloatRect& rect) final
    {
        noteContentCommand();
        phaseOneSerializer().clipRect(rect);
    }

    void clipRoundedRect(const WebCore::FloatRoundedRect& rect) final
    {
        noteContentCommand();
        ++m_counters.clipPaths;
        noteHandling("clip-rounded-rect", AdapterHandlingStrategy::Lowered);
        emitClipPath(m_writer, makeRoundedRectPath(rect));
    }

    void clipPath(const WebCore::Path& path, WebCore::WindRule = WebCore::WindRule::EvenOdd) final
    {
        noteContentCommand();
        phaseOneSerializer().clipPath(path);
    }

    void clipOut(const WebCore::FloatRect&) final { noteUnsupportedNoOp("clip-out-rect", "GraphicsContext::clipOut(rect)"); }
    void clipOutRoundedRect(const WebCore::FloatRoundedRect& rect) final
    {
        noteContentCommand();
        ++m_counters.clipPaths;
        if (emitClipOutPathResource(m_writer, m_serializationState, m_size, makeRoundedRectPath(rect)))
            noteHandling("clip-out-rounded-rect", AdapterHandlingStrategy::Prerasterized);
        else
            noteUnsupportedNoOp("clip-out-rounded-rect", "GraphicsContext::clipOutRoundedRect");
    }
    void clipOut(const WebCore::Path&) final { noteUnsupportedNoOp("clip-out-path", "GraphicsContext::clipOut(path)"); }

    void clipToImageBuffer(WebCore::ImageBuffer& imageBuffer, const WebCore::FloatRect& rect) final
    {
        noteContentCommand();
        ++m_counters.clipRects;
        // The current tap only receives the live mask buffer in this override.
        // Mirror the alpha mask when possible; fall back to a rect clip otherwise.
        if (!emitClipImageBufferResource(m_writer, m_serializationState, imageBuffer, rect)) {
            noteLossyFallback("clip-to-image-buffer", "GraphicsContext::clipToImageBuffer", "clip-rect fallback");
            emitClipRect(m_writer, rect);
        } else
            noteHandling("clip-to-image-buffer", AdapterHandlingStrategy::Prerasterized);
    }

    WebCore::IntRect clipBounds() const final
    {
        return { 0, 0, std::max(m_size.width(), 0), std::max(m_size.height(), 0) };
    }

    void setLineCap(WebCore::LineCap) final { noteUnsupportedNoOp("set-line-cap", "GraphicsContext::setLineCap"); }
    void setLineDash(const WebCore::DashArray&, float) final { noteUnsupportedNoOp("set-line-dash", "GraphicsContext::setLineDash"); }
    void setLineJoin(WebCore::LineJoin) final { noteUnsupportedNoOp("set-line-join", "GraphicsContext::setLineJoin"); }
    void setMiterLimit(float) final { noteUnsupportedNoOp("set-miter-limit", "GraphicsContext::setMiterLimit"); }

    void drawNativeImage(const WebCore::NativeImage& image, const WebCore::FloatRect& destRect, const WebCore::FloatRect& sourceRect, WebCore::ImagePaintingOptions = { }) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        if (!emitNativeImageResource(m_writer, m_serializationState, image, destRect, sourceRect))
            emitPlaceholderFallback("draw-native-image", "GraphicsContext::drawNativeImage", destRect, 0xFF9CA3AF);
        else
            noteHandling("draw-native-image", AdapterHandlingStrategy::Lowered);
    }

    void drawSystemImage(WebCore::SystemImage& systemImage, const WebCore::FloatRect& destinationRect) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        ++m_counters.prerasterizedFallbacks;
        if (!emitRasterizedImageResource(m_writer, m_serializationState, destinationRect, [&](WebCore::GraphicsContext& context) {
            context.drawSystemImage(systemImage, destinationRect);
        }))
            emitPlaceholderFallback("draw-system-image", "GraphicsContext::drawSystemImage", destinationRect, 0xFFE5E7EB);
        else
            noteHandling("draw-system-image", AdapterHandlingStrategy::Prerasterized);
    }

    void drawPattern(const WebCore::NativeImage& image, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform& patternTransform, const WebCore::FloatPoint& phase, const WebCore::FloatSize& spacing, WebCore::ImagePaintingOptions options = { }) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        ++m_counters.prerasterizedFallbacks;
        if (!emitRasterizedImageResource(m_writer, m_serializationState, destRect, [&](WebCore::GraphicsContext& context) {
            context.drawPattern(image, destRect, tileRect, patternTransform, phase, spacing, options);
        }))
            emitPlaceholderFallback("draw-pattern-native-image", "GraphicsContext::drawPattern(nativeImage)", destRect, 0xFF60A5FA);
        else
            noteHandling("draw-pattern-native-image", AdapterHandlingStrategy::Prerasterized);
    }

    WebCore::ImageDrawResult drawImage(WebCore::Image& image, const WebCore::FloatRect& destination, const WebCore::FloatRect& sourceRect, WebCore::ImagePaintingOptions = { WebCore::ImageOrientation::Orientation::FromImage }) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        if (!emitImageResource(m_writer, m_serializationState, image, destination, sourceRect))
            emitPlaceholderFallback("draw-image", "GraphicsContext::drawImage", destination, 0xFF9CA3AF);
        else
            noteHandling("draw-image", AdapterHandlingStrategy::Lowered);
        return WebCore::ImageDrawResult::DidDraw;
    }

    WebCore::ImageDrawResult drawTiledImage(WebCore::Image& image, const WebCore::FloatRect& destination, const WebCore::FloatPoint& source, const WebCore::FloatSize& tileSize, const WebCore::FloatSize& spacing, WebCore::ImagePaintingOptions options = { }) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        ++m_counters.prerasterizedFallbacks;
        if (!emitRasterizedImageResource(m_writer, m_serializationState, destination, [&](WebCore::GraphicsContext& context) {
            context.drawTiledImage(image, destination, source, tileSize, spacing, options);
        }))
            emitPlaceholderFallback("draw-tiled-image-point", "GraphicsContext::drawTiledImage(point)", destination, 0xFF60A5FA);
        else
            noteHandling("draw-tiled-image-point", AdapterHandlingStrategy::Prerasterized);
        return WebCore::ImageDrawResult::DidDraw;
    }

    WebCore::ImageDrawResult drawTiledImage(WebCore::Image& image, const WebCore::FloatRect& destination, const WebCore::FloatRect& sourceRect, const WebCore::FloatSize& tileScaleFactor, WebCore::Image::TileRule horizontalRule, WebCore::Image::TileRule verticalRule, WebCore::ImagePaintingOptions options = { }) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        ++m_counters.prerasterizedFallbacks;
        if (!emitRasterizedImageResource(m_writer, m_serializationState, destination, [&](WebCore::GraphicsContext& context) {
            context.drawTiledImage(image, destination, sourceRect, tileScaleFactor, horizontalRule, verticalRule, options);
        }))
            emitPlaceholderFallback("draw-tiled-image-rect", "GraphicsContext::drawTiledImage(rect)", destination, 0xFF60A5FA);
        else
            noteHandling("draw-tiled-image-rect", AdapterHandlingStrategy::Prerasterized);
        return WebCore::ImageDrawResult::DidDraw;
    }

    void drawImageBuffer(WebCore::ImageBuffer& imageBuffer, const WebCore::FloatRect& destination, const WebCore::FloatRect& sourceRect, WebCore::ImagePaintingOptions = { }) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        if (!emitImageBufferResource(m_writer, m_serializationState, imageBuffer, destination, sourceRect))
            emitPlaceholderFallback("draw-image-buffer", "GraphicsContext::drawImageBuffer", destination, 0xFF9CA3AF);
        else
            noteHandling("draw-image-buffer", AdapterHandlingStrategy::Lowered);
    }

    void drawConsumingImageBuffer(RefPtr<WebCore::ImageBuffer> imageBuffer, const WebCore::FloatRect& destination, const WebCore::FloatRect& sourceRect, WebCore::ImagePaintingOptions = { }) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        if (!imageBuffer || !emitImageBufferResource(m_writer, m_serializationState, *imageBuffer, destination, sourceRect))
            emitPlaceholderFallback("draw-consuming-image-buffer", "GraphicsContext::drawConsumingImageBuffer", destination, 0xFF9CA3AF);
        else
            noteHandling("draw-consuming-image-buffer", AdapterHandlingStrategy::Lowered);
    }

    void drawFilteredImageBuffer(WebCore::ImageBuffer* sourceImage, const WebCore::FloatRect& sourceImageRect, WebCore::Filter& filter, WebCore::FilterResults& results) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        if (!emitFilteredImageBufferResource(m_writer, m_serializationState, sourceImage, sourceImageRect, filter, results))
            emitPlaceholderFallback("draw-filtered-image-buffer", "GraphicsContext::drawFilteredImageBuffer", sourceImageRect, 0xFF6B7280);
        else
            noteHandling("draw-filtered-image-buffer", AdapterHandlingStrategy::Prerasterized);
    }

    void drawPattern(WebCore::ImageBuffer& imageBuffer, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform& patternTransform, const WebCore::FloatPoint& phase, const WebCore::FloatSize& spacing, WebCore::ImagePaintingOptions options = { }) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        ++m_counters.prerasterizedFallbacks;
        if (!emitRasterizedImageResource(m_writer, m_serializationState, destRect, [&](WebCore::GraphicsContext& context) {
            context.drawPattern(imageBuffer, destRect, tileRect, patternTransform, phase, spacing, options);
        }))
            emitPlaceholderFallback("draw-pattern-image-buffer", "GraphicsContext::drawPattern(imageBuffer)", destRect, 0xFF60A5FA);
        else
            noteHandling("draw-pattern-image-buffer", AdapterHandlingStrategy::Prerasterized);
    }

    void drawControlPart(WebCore::ControlPart&, const WebCore::FloatRoundedRect& borderRect, float, const WebCore::ControlStyle&) final
    {
        noteContentCommand();
        ++m_counters.controlPlaceholders;
        emitPlaceholderFallback("draw-control-part", "GraphicsContext::drawControlPart", borderRect.rect(), 0xFFD1D5DB);
    }

#if ENABLE(VIDEO)
    void drawVideoFrame(const WebCore::VideoFrame&, const WebCore::FloatRect& destination, WebCore::ImageOrientation, bool) final
    {
        noteContentCommand();
        ++m_counters.imagePlaceholders;
        emitPlaceholderFallback("draw-video-frame", "GraphicsContext::drawVideoFrame", destination, 0xFF4B5563);
    }
#endif

    using WebCore::GraphicsContext::scale;
    void scale(const WebCore::FloatSize& amount) final
    {
        if (!m_hasSeenContentCommand && m_suppressedInitialFlipTranslate
            && fabsf(amount.width() - 1.0f) < 0.01f
            && fabsf(amount.height() + 1.0f) < 0.01f) {
            noteHandling("scale", AdapterHandlingStrategy::Lowered);
            return;
        }
        phaseOneSerializer().scale(amount);
    }

    void rotate(float angleInRadians) final
    {
        phaseOneSerializer().rotate(angleInRadians);
    }

    void translate(float x, float y) final
    {
        if (!m_hasSeenContentCommand
            && !m_suppressedInitialFlipTranslate
            && fabsf(x) < 0.01f
            && fabsf(y - m_size.height()) < 1.0f) {
            m_suppressedInitialFlipTranslate = true;
            noteHandling("translate", AdapterHandlingStrategy::Lowered);
            return;
        }
        phaseOneSerializer().translate(x, y);
    }

    void concatCTM(const WebCore::AffineTransform& transform) final
    {
        phaseOneSerializer().concatCTM(transform);
    }

    void setCTM(const WebCore::AffineTransform& transform) final
    {
        phaseOneSerializer().setCTM(transform);
    }

    WebCore::AffineTransform getCTM(IncludeDeviceScale = PossiblyIncludeDeviceScale) const final
    {
        return m_serializationState.current().ctm;
    }

    void drawFocusRing(const WebCore::Path& path, float outlineWidth, const WebCore::Color& color, float) final
    {
        noteContentCommand();
        ++m_counters.focusRings;
        noteHandling("draw-focus-ring-path", AdapterHandlingStrategy::Lowered);
        m_writer.writeU8(static_cast<uint8_t>(Command::Save));
        m_writer.writeU8(static_cast<uint8_t>(Command::SetStrokeColor));
        m_writer.writeU32(packARGB(color));
        m_writer.writeU8(static_cast<uint8_t>(Command::SetLineWidth));
        m_writer.writeF32(outlineWidth);
        emitStrokePath(m_writer, path);
        m_writer.writeU8(static_cast<uint8_t>(Command::Restore));
    }

    void drawFocusRing(const Vector<WebCore::FloatRect>& rects, float outlineWidth, const WebCore::Color& color, float) final
    {
        noteContentCommand();
        ++m_counters.focusRings;
        noteHandling("draw-focus-ring-rects", AdapterHandlingStrategy::Lowered);
        m_writer.writeU8(static_cast<uint8_t>(Command::Save));
        m_writer.writeU8(static_cast<uint8_t>(Command::SetStrokeColor));
        m_writer.writeU32(packARGB(color));
        m_writer.writeU8(static_cast<uint8_t>(Command::SetLineWidth));
        m_writer.writeF32(outlineWidth);
        for (const auto& rect : rects)
            emitStrokeRect(m_writer, rect);
        m_writer.writeU8(static_cast<uint8_t>(Command::Restore));
    }

    WebCore::FloatSize drawText(const WebCore::FontCascade& cascade, const WebCore::TextRun& run, const WebCore::FloatPoint& point, unsigned from = 0, std::optional<unsigned> to = std::nullopt) final
    {
        noteContentCommand();
        ++m_counters.textRuns;
        return WebCore::GraphicsContext::drawText(cascade, run, point, from, to);
    }

    void drawGlyphs(const WebCore::Font& font, std::span<const WebCore::GlyphBufferGlyph> glyphs, std::span<const WebCore::GlyphBufferAdvance> advances, const WebCore::FloatPoint& point, WebCore::FontSmoothingMode smoothingMode) final
    {
        noteContentCommand();
        ++m_counters.drawGlyphRuns;
        auto emissionPath = emitDrawGlyphsResource(m_writer, m_serializationState, font, glyphs, advances, point, smoothingMode);
        if (emissionPath == DrawGlyphsEmissionPath::None) {
            notePlaceholderFallback("draw-glyphs", "GraphicsContext::drawGlyphs");
            emitDrawGlyphsPlaceholder(m_writer, m_serializationState, font, glyphs, advances, point);
        } else if (emissionPath == DrawGlyphsEmissionPath::Font)
            noteHandling("draw-glyphs", AdapterHandlingStrategy::Direct);
        else
            noteHandling("draw-glyphs", AdapterHandlingStrategy::Prerasterized);
    }

    void drawDisplayList(const WebCore::DisplayList::DisplayList& displayList, WebCore::ControlFactory& controlFactory) final
    {
        noteContentCommand();
        ++m_counters.displayLists;
        WebCore::GraphicsContext::drawDisplayList(displayList, controlFactory);
    }

    void drawEmphasisMarks(const WebCore::FontCascade& cascade, const WebCore::TextRun& run, const AtomString& mark, const WebCore::FloatPoint& point, unsigned from = 0, std::optional<unsigned> to = std::nullopt) final
    {
        noteContentCommand();
        ++m_counters.textRuns;
        WebCore::GraphicsContext::drawEmphasisMarks(cascade, run, mark, point, from, to);
    }

    void drawBidiText(const WebCore::FontCascade& cascade, const WebCore::TextRun& run, const WebCore::FloatPoint& point, WebCore::FontCascade::CustomFontNotReadyAction customFontNotReadyAction = WebCore::FontCascade::CustomFontNotReadyAction::DoNotPaintIfFontNotReady) final
    {
        noteContentCommand();
        ++m_counters.bidiTextRuns;
        WebCore::GraphicsContext::drawBidiText(cascade, run, point, customFontNotReadyAction);
    }

    void drawLinesForText(const WebCore::FloatPoint& origin, float thickness, std::span<const WebCore::FloatSegment> lineSegments, bool isPrinting, bool doubleLines, WebCore::StrokeStyle strokeStyle) final
    {
        noteContentCommand();
        if (lineSegments.empty())
            return;

        auto color = strokeColor();
        auto computeLineBounds = [&](const WebCore::FloatRect& rect) {
            auto adjustedOrigin = rect.location();
            auto adjustedThickness = std::max(rect.height(), 0.5f);
            if (isPrinting)
                return WebCore::FloatRect(adjustedOrigin, WebCore::FloatSize(rect.width(), adjustedThickness));

            auto transform = getCTM(WebCore::GraphicsContext::DefinitelyIncludeDeviceScale);
            float scale = transform.b() ? std::hypot(transform.a(), transform.b()) : transform.a();
            if (scale < 1.0f) {
                static constexpr float minimumUnderlineAlpha = 0.4f;
                float shade = scale > minimumUnderlineAlpha ? scale : minimumUnderlineAlpha;
                color = color.colorWithAlphaMultipliedBy(shade);
            }

            auto devicePoint = transform.mapPoint(rect.location());
            auto deviceOrigin = WebCore::FloatPoint(roundf(devicePoint.x()), ceilf(devicePoint.y()));
            if (auto inverse = transform.inverse())
                adjustedOrigin = inverse.value().mapPoint(deviceOrigin);
            return WebCore::FloatRect(adjustedOrigin, WebCore::FloatSize(rect.width(), adjustedThickness));
        };

        WTF::Vector<WebCore::FloatRect, 4> rects;
        auto bounds = computeLineBounds(WebCore::FloatRect { origin, WebCore::FloatSize { lineSegments.back().end, thickness } });
        if (bounds.isEmpty() || !color.isValid())
            return;

        rects.reserveInitialCapacity((doubleLines ? 2 : 1) * lineSegments.size());

        float dashWidth = 0.0f;
        switch (strokeStyle) {
        case WebCore::StrokeStyle::DottedStroke:
            dashWidth = bounds.height();
            break;
        case WebCore::StrokeStyle::DashedStroke:
            dashWidth = 2.0f * bounds.height();
            break;
        case WebCore::StrokeStyle::SolidStroke:
        default:
            break;
        }

        if (dashWidth) {
            for (const auto& lineSegment : lineSegments) {
                auto left = lineSegment.begin;
                auto width = lineSegment.length();
                auto doubleWidth = 2.0f * dashWidth;
                auto quotient = WTF::truncateDoubleToInt32(left / doubleWidth);
                auto startOffset = left - quotient * doubleWidth;
                auto effectiveLeft = left + startOffset;
                auto startParticle = WTF::truncateDoubleToInt32(std::floor(effectiveLeft / doubleWidth));
                auto endParticle = WTF::truncateDoubleToInt32(std::ceil((left + width) / doubleWidth));

                for (auto j = startParticle; j < endParticle; ++j) {
                    auto actualDashWidth = dashWidth;
                    auto dashStart = bounds.x() + j * doubleWidth;

                    if (j == startParticle && startOffset > 0 && startOffset < dashWidth) {
                        actualDashWidth -= startOffset;
                        dashStart += startOffset;
                    }

                    if (j == endParticle - 1) {
                        auto remainingWidth = left + width - (j * doubleWidth);
                        if (remainingWidth < dashWidth)
                            actualDashWidth = remainingWidth;
                    }

                    rects.append(WebCore::FloatRect(dashStart, bounds.y(), actualDashWidth, bounds.height()));
                }
            }
        } else {
            for (const auto& lineSegment : lineSegments)
                rects.append(WebCore::FloatRect(bounds.x() + lineSegment.begin, bounds.y(), lineSegment.length(), bounds.height()));
        }

        if (doubleLines) {
            auto secondLineY = bounds.y() + 2.0f * bounds.height();
            for (const auto& lineSegment : lineSegments)
                rects.append(WebCore::FloatRect(bounds.x() + lineSegment.begin, secondLineY, lineSegment.length(), bounds.height()));
        }

        noteHandling("draw-lines-for-text", AdapterHandlingStrategy::Lowered);
        for (const auto& rect : rects) {
            ++m_counters.drawGlyphRuns;
            emitFillRectColor(m_writer, rect, color);
        }
    }

    void drawDotsForDocumentMarker(const WebCore::FloatRect& rect, WebCore::DocumentMarkerLineStyle) final
    {
        noteContentCommand();
        ++m_counters.controlPlaceholders;
        emitPlaceholderFallback("draw-dots-for-document-marker", "GraphicsContext::drawDotsForDocumentMarker", rect, 0xFFF59E0B);
    }

    void beginPage(const WebCore::FloatRect&) final { }
    void endPage() final { }
    void setURLForRect(const URL&, const WebCore::FloatRect&) final { }
    void setDestinationForRect(const String&, const WebCore::FloatRect&) final { }
    void addDestinationAtPoint(const String&, const WebCore::FloatPoint&) final { }
    bool supportsInternalLinks() const final { return false; }

private:
    PhaseOneOperationSerializer phaseOneSerializer()
    {
        return { m_writer, m_serializationState, &m_counters, &m_adapterReport };
    }

    void noteContentCommand()
    {
        m_hasSeenContentCommand = true;
    }

    void noteHandling(const char* operation, AdapterHandlingStrategy strategy)
    {
        m_adapterReport.note(operation, strategy);
    }

    void notePlaceholderFallback(const char* operation, const char* feature)
    {
        ++m_counters.placeholderFallbacks;
        noteHandling(operation, AdapterHandlingStrategy::Placeholder);
        strictUnsupported(feature, "placeholder fallback");
    }

    void emitPlaceholderFallback(const char* operation, const char* feature, const WebCore::FloatRect& rect, uint32_t argb)
    {
        notePlaceholderFallback(operation, feature);
        emitPlaceholderRect(m_writer, rect, argb);
    }

    void noteLossyFallback(const char* operation, const char* feature, const char* strategy)
    {
        ++m_counters.lossyFallbacks;
        noteHandling(operation, AdapterHandlingStrategy::Lossy);
        strictUnsupported(feature, strategy);
    }

    void noteUnsupportedNoOp(const char* operation, const char* feature)
    {
        ++m_counters.unsupportedNoOps;
        noteHandling(operation, AdapterHandlingStrategy::UnsupportedNoOp);
        strictUnsupported(feature, "unsupported no-op");
    }

    void closeFrame()
    {
        if (m_frameClosed)
            return;
        fprintf(stderr, "nutjob tap: surface %llu size %dx%d origin(%.1f,%.1f) dirty(%.1f,%.1f,%.1f,%.1f) bg=0x%08x opacity %.3f order %u blend=%u flags=0x%02x ops state=%u save=%u restore=%u ctm=%u translate=%u rotate=%u scale=%u clipRect=%u clipPath=%u resetClip=%u fillRect=%u fillColorRect=%u fillPath=%u fillRounded=%u clear=%u strokeRect=%u strokePath=%u line=%u glyph=%u gradient=%u image=%u preraster=%u placeholder=%u lossy=%u noop=%u control=%u focus=%u text=%u bidi=%u displayList=%u beginT=%u endT=%u\n",
            static_cast<unsigned long long>(m_metadata.surfaceID), m_metadata.size.width(), m_metadata.size.height(),
            m_metadata.origin.x(), m_metadata.origin.y(),
            m_metadata.dirtyRect.x(), m_metadata.dirtyRect.y(), m_metadata.dirtyRect.width(), m_metadata.dirtyRect.height(),
            m_metadata.backgroundColorARGB, m_metadata.effectiveOpacity, m_metadata.paintOrder, m_metadata.blendMode, m_metadata.surfaceFlags,
            m_counters.stateUpdates, m_counters.saves, m_counters.restores, m_counters.setCTM, m_counters.translates, m_counters.rotates, m_counters.scales,
            m_counters.clipRects, m_counters.clipPaths, m_counters.resetClips,
            m_counters.fillRects, m_counters.fillRectColors, m_counters.fillPaths, m_counters.fillRoundedRects, m_counters.clearRects,
            m_counters.strokeRects, m_counters.strokePaths, m_counters.drawLines, m_counters.drawGlyphRuns, m_counters.gradients,
            m_counters.imagePlaceholders, m_counters.prerasterizedFallbacks, m_counters.placeholderFallbacks, m_counters.lossyFallbacks, m_counters.unsupportedNoOps,
            m_counters.controlPlaceholders, m_counters.focusRings, m_counters.textRuns, m_counters.bidiTextRuns,
            m_counters.displayLists, m_counters.transparencyBegins, m_counters.transparencyEnds);
        appendAdapterReportLine(m_metadata, m_adapterReport.snapshot());
        m_writer.writeU8(static_cast<uint8_t>(Command::FrameEnd));
        fflush(m_writer.out);
        m_frameClosed = true;
    }

    WebCore::IntSize m_size;
    Writer m_writer;
    std::unique_lock<std::mutex> m_lock;
    SerializationState m_serializationState;
    FrameMetadata m_metadata;
    FrameCounters m_counters;
    AdapterReport m_adapterReport;
    bool m_hasSeenContentCommand { false };
    bool m_suppressedInitialFlipTranslate { false };
    bool m_frameClosed { false };
};

inline void serializeDisplayListToFile(FILE* file, const WebCore::DisplayList::DisplayList& displayList, const WebCore::GraphicsContextState& initialState, const WebCore::AffineTransform& initialCTM, const WebCore::IntSize& size, const WebCore::FloatRect& dirtyRect, const WebCore::PlatformCALayer* layer = nullptr, float backingScale = 1.0f, AdapterReport* report = nullptr)
{
    if (!file)
        return;

    std::lock_guard<std::mutex> lock(tapMutex());

    Writer writer { file };
    auto metadata = computeFrameMetadata(size, dirtyRect, layer);
    emitFrameBegin(writer, metadata);

    SerializationState serializationState(initialState, normalizedInitialCTM(initialCTM, backingScale, metadata.origin, size));
    AdapterReport localReport;
    AdapterReport* activeReport = report;
    if (!activeReport && hasAdapterReportOutput())
        activeReport = &localReport;

    PhaseOneOperationSerializer(writer, serializationState, nullptr, activeReport).emitInitialState();

    for (const auto& item : displayList.items())
        serializeItem(writer, serializationState, size, item, nullptr, activeReport);

    if (activeReport)
        appendAdapterReportLine(metadata, activeReport->snapshot());
    writer.writeU8(static_cast<uint8_t>(Command::FrameEnd));
    fflush(writer.out);
}

inline void serializeDisplayList(const WebCore::DisplayList::DisplayList& displayList, const WebCore::GraphicsContextState& initialState, const WebCore::AffineTransform& initialCTM, const WebCore::IntSize& size, const WebCore::FloatRect& dirtyRect, const WebCore::PlatformCALayer* layer = nullptr, float backingScale = 1.0f, AdapterReport* report = nullptr)
{
    serializeDisplayListToFile(tapFile(), displayList, initialState, initialCTM, size, dirtyRect, layer, backingScale, report);
}

} // namespace WebKit::NutjobTap

#endif /* WEBKIT_REMOTE_LAYER_NUTJOB_TAP_H */
