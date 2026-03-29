/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#if USE(CG)

#define NUTJOB_TAP_DISABLE_PROCESS_ACTIVATION 1
#define NUTJOB_TAP_DISABLE_LAYER_METADATA 1
#include "../../../../../Source/WebKit/Shared/RemoteLayerTree/webkit_remote_layer_nutjob_tap.h"
#include <WebCore/BifurcatedGraphicsContext.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListRecorderImpl.h>
#include <WebCore/FontCascade.h>
#include <WebCore/FontSelector.h>
#include <WebCore/GradientImage.h>
#include <WebCore/GraphicsContextCG.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/TextRun.h>
#include <limits>
#include <numbers>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>
#include <wtf/text/MakeString.h>

namespace TestWebKitAPI {
using namespace WebCore;
using DisplayList::DisplayList;
using namespace DisplayList;

constexpr CGFloat contextWidth = 1;
constexpr CGFloat contextHeight = 1;

namespace {

struct OraclePathResource {
    Path path;
    WindRule fillRule { WindRule::NonZero };
};

struct OracleProgram {
    IntSize canvasSize;
    float deviceScale { 1.0f };
    uint32_t backgroundARGB { 0 };
    HashMap<String, OraclePathResource> paths;
    Vector<Ref<JSON::Object>> ops;
};

enum class OracleStatus : uint8_t {
    Ok,
    UnsupportedOp,
    Error,
};

static String envString(const char* name)
{
    if (auto* value = getenv(name))
        return String::fromUTF8(value);
    return { };
}

static bool envFlagEnabled(const char* name)
{
    auto value = envString(name);
    if (value.isEmpty())
        return false;

    switch (value.characterAt(0)) {
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

static bool writeBytesToPath(const String& path, std::span<const uint8_t> bytes)
{
    auto utf8 = path.utf8();
    FILE* file = fopen(utf8.data(), "wb");
    if (!file)
        return false;

    bool success = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    fclose(file);
    return success;
}

static bool writeStringToPath(const String& path, StringView content)
{
    auto utf8Path = path.utf8();
    FILE* file = fopen(utf8Path.data(), "wb");
    if (!file)
        return false;

    auto utf8Content = content.toString().utf8();
    bool success = fwrite(utf8Content.data(), 1, utf8Content.length(), file) == static_cast<size_t>(utf8Content.length());
    fclose(file);
    return success;
}

static Ref<JSON::Object> makeHandlingCountsJSON(const WebKit::NutjobTap::AdapterHandlingCounts& counts)
{
    auto object = JSON::Object::create();
    object->setInteger("direct"_s, counts.direct);
    object->setInteger("lowered"_s, counts.lowered);
    object->setInteger("prerasterized"_s, counts.prerasterized);
    object->setInteger("placeholder"_s, counts.placeholder);
    object->setInteger("lossy"_s, counts.lossy);
    object->setInteger("unsupportedNoOp"_s, counts.unsupportedNoOp);
    object->setInteger("observed"_s, counts.observed());
    object->setInteger("supported"_s, counts.supported());
    object->setDouble("supportRatio"_s, counts.observed() ? static_cast<double>(counts.supported()) / counts.observed() : 1.0);
    return object;
}

static String adapterReportJSONString(const WebKit::NutjobTap::AdapterReportSnapshot& report)
{
    auto root = JSON::Object::create();
    root->setObject("overall"_s, makeHandlingCountsJSON(report.overall));

    auto operations = JSON::Array::create();
    for (const auto& summary : report.operations) {
        auto entry = JSON::Object::create();
        entry->setString("operation"_s, summary.operation);
        entry->setObject("counts"_s, makeHandlingCountsJSON(summary.counts));
        operations->pushObject(WTF::move(entry));
    }
    root->setArray("operations"_s, WTF::move(operations));
    return root->toJSONString();
}

static std::optional<double> jsonNumber(const JSON::Object& object, const String& key)
{
    auto value = object.getValue(key);
    if (!value)
        return std::nullopt;
    return value->asDouble();
}

static String jsonString(const JSON::Object& object, const String& key)
{
    auto value = object.getValue(key);
    if (!value || value->type() != JSON::Value::Type::String)
        return { };
    return value->asString();
}

static RefPtr<JSON::Object> jsonObject(const JSON::Object& object, const String& key)
{
    auto value = object.getValue(key);
    if (!value || value->type() != JSON::Value::Type::Object)
        return nullptr;
    return value->asObject();
}

static RefPtr<JSON::Array> jsonArray(const JSON::Object& object, const String& key)
{
    auto value = object.getValue(key);
    if (!value || value->type() != JSON::Value::Type::Array)
        return nullptr;
    return value->asArray();
}

static std::optional<uint32_t> jsonARGB(const JSON::Object& object, const String& key)
{
    auto value = object.getValue(key);
    if (!value)
        return std::nullopt;

    if (auto integerValue = value->asInteger()) {
        if (*integerValue < 0)
            return std::nullopt;
        return static_cast<uint32_t>(*integerValue);
    }

    if (auto doubleValue = value->asDouble()) {
        if (*doubleValue < 0.0 || *doubleValue > static_cast<double>(std::numeric_limits<uint32_t>::max()))
            return std::nullopt;
        return static_cast<uint32_t>(*doubleValue);
    }

    return std::nullopt;
}

static Color colorFromARGB(uint32_t argb)
{
    return Color { SRGBA<uint8_t> {
        static_cast<uint8_t>((argb >> 16) & 0xFF),
        static_cast<uint8_t>((argb >> 8) & 0xFF),
        static_cast<uint8_t>(argb & 0xFF),
        static_cast<uint8_t>((argb >> 24) & 0xFF)
    } };
}

static Ref<JSON::Object> makeRectJSON(const IntRect& rect)
{
    auto object = JSON::Object::create();
    object->setInteger("x"_s, rect.x());
    object->setInteger("y"_s, rect.y());
    object->setInteger("width"_s, rect.width());
    object->setInteger("height"_s, rect.height());
    return object;
}

static Ref<JSON::Object> makeCTMJSON(const AffineTransform& transform)
{
    auto object = JSON::Object::create();
    object->setDouble("a"_s, transform.a());
    object->setDouble("b"_s, transform.b());
    object->setDouble("c"_s, transform.c());
    object->setDouble("d"_s, transform.d());
    object->setDouble("tx"_s, transform.e());
    object->setDouble("ty"_s, transform.f());
    return object;
}

static void updateWitnessDepths(StringView opName, unsigned& saveDepth, unsigned& transparencyDepth)
{
    if (opName == "save"_s) {
        ++saveDepth;
        return;
    }
    if (opName == "restore"_s) {
        if (saveDepth)
            --saveDepth;
        return;
    }
    if (opName == "begin-transparency"_s) {
        ++transparencyDepth;
        return;
    }
    if (opName == "end-transparency"_s && transparencyDepth)
        --transparencyDepth;
}

static Ref<JSON::Object> makeWitnessEntry(const GraphicsContext& context, const JSON::Object& op, size_t index, unsigned saveDepth, unsigned transparencyDepth)
{
    auto entry = JSON::Object::create();
    entry->setInteger("seq"_s, static_cast<int>(index));
    entry->setString("op"_s, jsonString(op, "op"_s));
    entry->setObject("ctm"_s, makeCTMJSON(context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale)));
    entry->setObject("clipBounds"_s, makeRectJSON(context.clipBounds()));
    entry->setDouble("alpha"_s, context.alpha());
    entry->setDouble("strokeThickness"_s, context.strokeThickness());
    entry->setInteger("saveDepth"_s, static_cast<int>(saveDepth));
    entry->setInteger("transparencyDepth"_s, static_cast<int>(transparencyDepth));
    return entry;
}

static String nativeWitnessJSONString(const OracleProgram& program, const Vector<Ref<JSON::Object>>& entries)
{
    auto root = JSON::Object::create();
    auto canvas = JSON::Object::create();
    canvas->setInteger("width"_s, program.canvasSize.width());
    canvas->setInteger("height"_s, program.canvasSize.height());
    canvas->setDouble("deviceScale"_s, program.deviceScale);
    canvas->setInteger("backgroundARGB"_s, static_cast<int64_t>(program.backgroundARGB));
    root->setObject("canvas"_s, WTF::move(canvas));
    root->setInteger("entryCount"_s, entries.size());

    auto items = JSON::Array::create();
    for (const auto& entry : entries)
        items->pushObject(entry.copyRef());
    root->setArray("entries"_s, WTF::move(items));
    return root->toJSONString();
}

static std::optional<OracleProgram> parseOracleProgram(const String& programPath, String& error)
{
    auto fileContents = FileSystem::readEntireFile(programPath);
    if (!fileContents) {
        error = makeString("failed to read program: "_s, programPath);
        return std::nullopt;
    }

    auto rootValue = JSON::Value::parseJSON(String::fromUTF8(fileContents->span()));
    if (!rootValue) {
        error = "failed to parse program JSON"_s;
        return std::nullopt;
    }

    auto root = rootValue->asObject();
    if (!root) {
        error = "program JSON root must be an object"_s;
        return std::nullopt;
    }

    auto version = jsonNumber(*root, "version"_s);
    if (!version || *version != 1.0) {
        error = "program version must be 1"_s;
        return std::nullopt;
    }

    auto mode = jsonString(*root, "mode"_s);
    if (mode != "structural"_s) {
        error = makeString("unsupported mode: "_s, mode);
        return std::nullopt;
    }

    auto canvas = jsonObject(*root, "canvas"_s);
    if (!canvas) {
        error = "program.canvas must be an object"_s;
        return std::nullopt;
    }

    auto width = jsonNumber(*canvas, "width"_s);
    auto height = jsonNumber(*canvas, "height"_s);
    if (!width || !height || *width <= 0 || *height <= 0) {
        error = "canvas width/height must be positive numbers"_s;
        return std::nullopt;
    }

    OracleProgram program;
    program.canvasSize = IntSize { static_cast<int>(*width), static_cast<int>(*height) };
    if (auto deviceScale = jsonNumber(*canvas, "deviceScale"_s))
        program.deviceScale = static_cast<float>(*deviceScale);
    if (auto background = jsonARGB(*canvas, "background"_s))
        program.backgroundARGB = *background;

    if (auto resources = jsonObject(*root, "resources"_s)) {
        if (auto paths = jsonArray(*resources, "paths"_s)) {
            for (auto& pathValue : *paths) {
                auto pathObject = pathValue->asObject();
                if (!pathObject) {
                    error = "path resource must be an object"_s;
                    return std::nullopt;
                }

                auto pathID = jsonString(*pathObject, "id"_s);
                if (pathID.isEmpty()) {
                    error = "path resource requires a non-empty id"_s;
                    return std::nullopt;
                }

                auto fillRuleString = jsonString(*pathObject, "fillRule"_s);
                WindRule fillRule = fillRuleString == "evenodd"_s ? WindRule::EvenOdd : WindRule::NonZero;

                auto segments = jsonArray(*pathObject, "segments"_s);
                if (!segments || !segments->length()) {
                    error = makeString("path "_s, pathID, " requires segments"_s);
                    return std::nullopt;
                }

                Path path;
                for (auto& segmentValue : *segments) {
                    auto segmentObject = segmentValue->asObject();
                    if (!segmentObject) {
                        error = makeString("path "_s, pathID, " segment must be an object"_s);
                        return std::nullopt;
                    }

                    auto op = jsonString(*segmentObject, "op"_s);
                    if (op == "moveTo"_s) {
                        auto x = jsonNumber(*segmentObject, "x"_s);
                        auto y = jsonNumber(*segmentObject, "y"_s);
                        if (!x || !y) {
                            error = makeString("path "_s, pathID, " moveTo requires x/y"_s);
                            return std::nullopt;
                        }
                        path.moveTo({ static_cast<float>(*x), static_cast<float>(*y) });
                        continue;
                    }
                    if (op == "lineTo"_s) {
                        auto x = jsonNumber(*segmentObject, "x"_s);
                        auto y = jsonNumber(*segmentObject, "y"_s);
                        if (!x || !y) {
                            error = makeString("path "_s, pathID, " lineTo requires x/y"_s);
                            return std::nullopt;
                        }
                        path.addLineTo({ static_cast<float>(*x), static_cast<float>(*y) });
                        continue;
                    }
                    if (op == "close"_s) {
                        path.closeSubpath();
                        continue;
                    }

                    error = makeString("unsupported path segment op: "_s, op);
                    return std::nullopt;
                }

                program.paths.add(pathID, OraclePathResource { WTF::move(path), fillRule });
            }
        }
    }

    auto ops = jsonArray(*root, "ops"_s);
    if (!ops) {
        error = "program.ops must be an array"_s;
        return std::nullopt;
    }

    for (auto& opValue : *ops) {
        auto opObject = opValue->asObject();
        if (!opObject) {
            error = "program op must be an object"_s;
            return std::nullopt;
        }
        program.ops.append(opObject.releaseNonNull());
    }

    return program;
}

static OracleStatus applyOracleOperation(GraphicsContext& context, const OracleProgram& program, const JSON::Object& op, bool strictMode, String& message)
{
    auto opName = jsonString(op, "op"_s);

    if (opName == "save"_s) {
        context.save();
        return OracleStatus::Ok;
    }
    if (opName == "restore"_s) {
        context.restore();
        return OracleStatus::Ok;
    }
    if (opName == "set-fill-color"_s) {
        auto argb = jsonARGB(op, "argb"_s);
        if (!argb) {
            message = "set-fill-color requires argb"_s;
            return OracleStatus::Error;
        }
        context.setFillColor(colorFromARGB(*argb));
        return OracleStatus::Ok;
    }
    if (opName == "set-stroke-color"_s) {
        auto argb = jsonARGB(op, "argb"_s);
        if (!argb) {
            message = "set-stroke-color requires argb"_s;
            return OracleStatus::Error;
        }
        context.setStrokeColor(colorFromARGB(*argb));
        return OracleStatus::Ok;
    }
    if (opName == "set-alpha"_s) {
        auto alpha = jsonNumber(op, "alpha"_s);
        if (!alpha) {
            message = "set-alpha requires alpha"_s;
            return OracleStatus::Error;
        }
        context.setAlpha(static_cast<float>(*alpha));
        return OracleStatus::Ok;
    }
    if (opName == "translate"_s) {
        auto tx = jsonNumber(op, "tx"_s);
        auto ty = jsonNumber(op, "ty"_s);
        if (!tx || !ty) {
            message = "translate requires tx/ty"_s;
            return OracleStatus::Error;
        }
        context.translate(static_cast<float>(*tx), static_cast<float>(*ty));
        return OracleStatus::Ok;
    }
    if (opName == "scale"_s) {
        auto sx = jsonNumber(op, "sx"_s);
        auto sy = jsonNumber(op, "sy"_s);
        if (!sx || !sy) {
            message = "scale requires sx/sy"_s;
            return OracleStatus::Error;
        }
        context.scale({ static_cast<float>(*sx), static_cast<float>(*sy) });
        return OracleStatus::Ok;
    }
    if (opName == "rotate"_s) {
        auto radians = jsonNumber(op, "radians"_s);
        if (!radians) {
            message = "rotate requires radians"_s;
            return OracleStatus::Error;
        }
        context.rotate(static_cast<float>(*radians));
        return OracleStatus::Ok;
    }
    if (opName == "set-ctm"_s) {
        auto a = jsonNumber(op, "a"_s);
        auto b = jsonNumber(op, "b"_s);
        auto c = jsonNumber(op, "c"_s);
        auto d = jsonNumber(op, "d"_s);
        auto tx = jsonNumber(op, "tx"_s);
        auto ty = jsonNumber(op, "ty"_s);
        if (!a || !b || !c || !d || !tx || !ty) {
            message = "set-ctm requires a/b/c/d/tx/ty"_s;
            return OracleStatus::Error;
        }
        context.setCTM(AffineTransform(*a, *b, *c, *d, *tx, *ty));
        return OracleStatus::Ok;
    }
    if (opName == "clip-rect"_s) {
        auto x = jsonNumber(op, "x"_s);
        auto y = jsonNumber(op, "y"_s);
        auto width = jsonNumber(op, "width"_s);
        auto height = jsonNumber(op, "height"_s);
        if (!x || !y || !width || !height) {
            message = "clip-rect requires x/y/width/height"_s;
            return OracleStatus::Error;
        }
        context.clip(FloatRect { static_cast<float>(*x), static_cast<float>(*y), static_cast<float>(*width), static_cast<float>(*height) });
        return OracleStatus::Ok;
    }
    if (opName == "clip-path"_s) {
        auto pathID = jsonString(op, "pathId"_s);
        auto iterator = program.paths.find(pathID);
        if (iterator == program.paths.end()) {
            message = makeString("unknown pathId: "_s, pathID);
            return OracleStatus::Error;
        }
        context.clipPath(iterator->value.path, iterator->value.fillRule);
        return OracleStatus::Ok;
    }
    if (opName == "fill-rect"_s) {
        auto x = jsonNumber(op, "x"_s);
        auto y = jsonNumber(op, "y"_s);
        auto width = jsonNumber(op, "width"_s);
        auto height = jsonNumber(op, "height"_s);
        if (!x || !y || !width || !height) {
            message = "fill-rect requires x/y/width/height"_s;
            return OracleStatus::Error;
        }
        context.fillRect(FloatRect { static_cast<float>(*x), static_cast<float>(*y), static_cast<float>(*width), static_cast<float>(*height) });
        return OracleStatus::Ok;
    }
    if (opName == "stroke-rect"_s) {
        auto x = jsonNumber(op, "x"_s);
        auto y = jsonNumber(op, "y"_s);
        auto width = jsonNumber(op, "width"_s);
        auto height = jsonNumber(op, "height"_s);
        if (!x || !y || !width || !height) {
            message = "stroke-rect requires x/y/width/height"_s;
            return OracleStatus::Error;
        }
        auto lineWidth = context.strokeThickness() > 0 ? context.strokeThickness() : 1.0f;
        context.strokeRect(FloatRect { static_cast<float>(*x), static_cast<float>(*y), static_cast<float>(*width), static_cast<float>(*height) }, lineWidth);
        return OracleStatus::Ok;
    }
    if (opName == "begin-transparency"_s) {
        auto opacity = jsonNumber(op, "opacity"_s);
        if (!opacity) {
            message = "begin-transparency requires opacity"_s;
            return OracleStatus::Error;
        }
        context.beginTransparencyLayer(static_cast<float>(*opacity));
        return OracleStatus::Ok;
    }
    if (opName == "end-transparency"_s) {
        context.endTransparencyLayer();
        return OracleStatus::Ok;
    }

    message = makeString("unsupported op: "_s, opName);
    return strictMode ? OracleStatus::UnsupportedOp : OracleStatus::Ok;
}

static OracleStatus runOracleProgram(const OracleProgram& program, const String& artifactsDirectory, StringView adapterLane, bool strictMode, String& message)
{
    auto adapterPath = FileSystem::pathByAppendingComponent(artifactsDirectory, "adapter.nj"_s);
    auto nativeRawPath = FileSystem::pathByAppendingComponent(artifactsDirectory, "native.raw"_s);
    auto adapterReportPath = FileSystem::pathByAppendingComponent(artifactsDirectory, "adapter-report.json"_s);
    auto nativeWitnessPath = FileSystem::pathByAppendingComponent(artifactsDirectory, "native-witness.json"_s);
    auto canvasRect = FloatRect { 0, 0, static_cast<float>(program.canvasSize.width()), static_cast<float>(program.canvasSize.height()) };

    RefPtr imageBuffer = ImageBuffer::create(FloatSize { static_cast<float>(program.canvasSize.width()), static_cast<float>(program.canvasSize.height()) }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, program.deviceScale, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer) {
        message = "failed to allocate native image buffer"_s;
        return OracleStatus::Error;
    }

    auto adapterUTF8 = adapterPath.utf8();
    FILE* adapterFile = fopen(adapterUTF8.data(), "wb");
    if (!adapterFile) {
        message = makeString("failed to open adapter output: "_s, adapterPath);
        return OracleStatus::Error;
    }

    OracleStatus status = OracleStatus::Ok;
    WebKit::NutjobTap::AdapterReportSnapshot adapterReport;
    Vector<Ref<JSON::Object>> nativeWitnessEntries;
    unsigned witnessSaveDepth = 0;
    unsigned witnessTransparencyDepth = 0;
    {
        auto& primaryContext = imageBuffer->context();
        auto initialState = primaryContext.state();
        auto initialCTM = primaryContext.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
        if (adapterLane == "tee"_s) {
            WebKit::NutjobTap::MirroringGraphicsContext recordingContext(adapterFile, initialState, initialCTM, program.canvasSize, canvasRect, nullptr, program.deviceScale);
            BifurcatedGraphicsContext context(primaryContext, recordingContext);

            if (program.backgroundARGB)
                context.fillRect(canvasRect, colorFromARGB(program.backgroundARGB));

            for (auto& op : program.ops) {
                status = applyOracleOperation(context, program, op.get(), strictMode, message);
                if (status != OracleStatus::Ok)
                    break;

                updateWitnessDepths(jsonString(op.get(), "op"_s), witnessSaveDepth, witnessTransparencyDepth);
                nativeWitnessEntries.append(makeWitnessEntry(primaryContext, op.get(), nativeWitnessEntries.size(), witnessSaveDepth, witnessTransparencyDepth));
            }

            adapterReport = recordingContext.adapterReportSnapshot();
        } else if (adapterLane == "display-list"_s) {
            RecorderImpl recordingContext({ }, canvasRect, { });
            BifurcatedGraphicsContext context(primaryContext, recordingContext);

            if (program.backgroundARGB)
                context.fillRect(canvasRect, colorFromARGB(program.backgroundARGB));

            for (auto& op : program.ops) {
                status = applyOracleOperation(context, program, op.get(), strictMode, message);
                if (status != OracleStatus::Ok)
                    break;

                updateWitnessDepths(jsonString(op.get(), "op"_s), witnessSaveDepth, witnessTransparencyDepth);
                nativeWitnessEntries.append(makeWitnessEntry(primaryContext, op.get(), nativeWitnessEntries.size(), witnessSaveDepth, witnessTransparencyDepth));
            }

            if (status == OracleStatus::Ok) {
                Ref displayList = recordingContext.takeDisplayList();
                WebKit::NutjobTap::AdapterReport serializerReport;
                WebKit::NutjobTap::serializeDisplayListToFile(adapterFile, displayList.get(), initialState, initialCTM, program.canvasSize, canvasRect, nullptr, program.deviceScale, &serializerReport);
                adapterReport = serializerReport.snapshot();
            }
        } else {
            fclose(adapterFile);
            message = makeString("unsupported adapter lane: "_s, adapterLane);
            return OracleStatus::UnsupportedOp;
        }
    }

    fclose(adapterFile);

    if (!writeStringToPath(adapterReportPath, adapterReportJSONString(adapterReport))) {
        message = makeString("failed to write adapter-report.json: "_s, adapterReportPath);
        return OracleStatus::Error;
    }

    if (!writeStringToPath(nativeWitnessPath, nativeWitnessJSONString(program, nativeWitnessEntries))) {
        message = makeString("failed to write native-witness.json: "_s, nativeWitnessPath);
        return OracleStatus::Error;
    }

    if (status != OracleStatus::Ok)
        return status;

    PixelBufferFormat pixelFormat { AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = imageBuffer->getPixelBuffer(pixelFormat, IntRect { { }, program.canvasSize });
    if (!pixelBuffer) {
        message = "failed to read native pixel buffer"_s;
        return OracleStatus::Error;
    }

    if (!writeBytesToPath(nativeRawPath, pixelBuffer->bytes())) {
        message = makeString("failed to write native.raw: "_s, nativeRawPath);
        return OracleStatus::Error;
    }

    return OracleStatus::Ok;
}

} // namespace

TEST(BifurcatedGraphicsContextTests, Basic)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContext(primaryCGContext.get());
    RecorderImpl secondaryContext({ }, FloatRect(0, 0, contextWidth, contextHeight), { });

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    ctx.fillRect(FloatRect(0, 0, contextWidth, contextHeight), Color::red);

    // The primary context should have one red pixel.
    CGContextFlush(primaryCGContext.get());
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(primaryCGContext.get()));
    EXPECT_EQ(primaryData[0], 255);
    EXPECT_EQ(primaryData[1], 0);
    EXPECT_EQ(primaryData[2], 0);
    Ref displayList = secondaryContext.takeDisplayList();
    // The secondary context should have a red FillRectWithColor.
    EXPECT_FALSE(displayList->items().empty());
    bool sawFillRect = false;

    for (auto& item : displayList->items()) {
        if (auto* fillRect = std::get_if<FillRectWithColor>(&item)) {
            sawFillRect = true;
            EXPECT_EQ(fillRect->rect(), FloatRect(0, 0, contextWidth, contextHeight));
            EXPECT_EQ(fillRect->color(), Color::red);
        }
    }

    EXPECT_TRUE(sawFillRect);
}

TEST(BifurcatedGraphicsContextTests, Text)
{
    RecorderImpl primaryContext({ }, FloatRect(0, 0, contextWidth, contextHeight), { });
    RecorderImpl secondaryContext({ }, FloatRect(0, 0, contextWidth, contextHeight), { });

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    FontCascadeDescription description;
    description.setOneFamily("Times"_s);
    description.setComputedSize(80);
    FontCascade font(WTF::move(description));
    font.update();

    String string = "Hello!"_s;
    TextRun run(string);
    ctx.drawText(font, run, { });

    auto runTest = [&] (const DisplayList& displayList) {
        EXPECT_FALSE(displayList.items().empty());
        bool sawDrawGlyphs = false;
        for (auto& displayListItem : displayList.items()) {
            if (std::holds_alternative<DrawGlyphs>(displayListItem))
                sawDrawGlyphs = true;
        }

        EXPECT_TRUE(sawDrawGlyphs);
    };

    // Ensure that both contexts have text painting commands.
    runTest(primaryContext.takeDisplayList());
    runTest(secondaryContext.takeDisplayList());
}

TEST(BifurcatedGraphicsContextTests, DrawTiledGradientImage)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    auto secondaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContext(primaryCGContext.get());
    GraphicsContextCG secondaryContext(secondaryCGContext.get());
    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    auto gradient = Gradient::create(Gradient::LinearData { { 0, 0 }, { 1, 1 } }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    gradient->addColorStop({ 0, Color::red });

    auto gradientImage = GradientImage::create(gradient, FloatSize { 1, 1 });

    ctx.drawTiledImage(gradientImage.get(), FloatRect { 0, 0, 100, 100 }, FloatRect { 0, 0, 1, 1 }, FloatSize { 1, 1 }, Image::RepeatTile, Image::RepeatTile);

    // The primary context should be red.
    CGContextFlush(primaryCGContext.get());
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(primaryCGContext.get()));
    EXPECT_EQ(primaryData[0], 255);
    EXPECT_EQ(primaryData[1], 0);
    EXPECT_EQ(primaryData[2], 0);

    // The secondary context should be red.
    CGContextFlush(secondaryCGContext.get());
    uint8_t* secondaryData = static_cast<uint8_t*>(CGBitmapContextGetData(secondaryCGContext.get()));
    EXPECT_EQ(secondaryData[0], 255);
    EXPECT_EQ(secondaryData[1], 0);
    EXPECT_EQ(secondaryData[2], 0);
}

TEST(BifurcatedGraphicsContextTests, DrawGradientImage)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    auto secondaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContext(primaryCGContext.get());
    GraphicsContextCG secondaryContext(secondaryCGContext.get());
    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    auto gradient = Gradient::create(Gradient::LinearData { { 0, 0 }, { 1, 1 } }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    gradient->addColorStop({ 0, Color::red });

    auto gradientImage = GradientImage::create(gradient, FloatSize { 1, 1 });

    ctx.drawImage(gradientImage.get(), FloatRect { 0, 0, 100, 100 }, FloatRect { 0, 0, 1, 1 });

    // The primary context should be red.
    CGContextFlush(primaryCGContext.get());
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(primaryCGContext.get()));
    EXPECT_EQ(primaryData[0], 255);
    EXPECT_EQ(primaryData[1], 0);
    EXPECT_EQ(primaryData[2], 0);

    // The secondary context should be red.
    CGContextFlush(secondaryCGContext.get());
    uint8_t* secondaryData = static_cast<uint8_t*>(CGBitmapContextGetData(secondaryCGContext.get()));
    EXPECT_EQ(secondaryData[0], 255);
    EXPECT_EQ(secondaryData[1], 0);
    EXPECT_EQ(secondaryData[2], 0);
}

TEST(BifurcatedGraphicsContextTests, Borders)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContext(primaryCGContext.get());
    RecorderImpl secondaryContext({ }, FloatRect(0, 0, contextWidth, contextHeight), { });

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    ctx.setStrokeColor(Color::red);
    ctx.setStrokeStyle(StrokeStyle::SolidStroke);
    ctx.setStrokeThickness(10);
    ctx.drawLine({ 0, 0 }, { contextWidth, 0 });

    // The primary context should have red pixels.
    CGContextFlush(primaryCGContext.get());
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(primaryCGContext.get()));
    EXPECT_EQ(primaryData[0], 255);
    EXPECT_EQ(primaryData[1], 0);
    EXPECT_EQ(primaryData[2], 0);
    EXPECT_EQ(primaryData[3], 255);
}

TEST(BifurcatedGraphicsContextTests, TransformedClip)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, 100, 100, 8, 4 * 100, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContextCG(primaryCGContext.get());
    GraphicsContext& primaryContext = primaryContextCG;

    RecorderImpl secondaryContextDL({ }, FloatRect(0, 0, 100, 100), { });
    GraphicsContext& secondaryContext = secondaryContextDL;

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    ctx.clip(FloatRect(25, 25, 50, 50));

    EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
    EXPECT_EQ(primaryContext.clipBounds(), FloatRect(25, 25, 50, 50));

    ctx.scale({ 1, -1 });
    ctx.translate(0, -100);

    EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
    EXPECT_EQ(primaryContext.clipBounds(), FloatRect(25, 25, 50, 50));

    ctx.scale(2);

    EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
    EXPECT_EQ(primaryContext.clipBounds(), FloatRect(12, 12, 26, 26));

    {
        GraphicsContextStateSaver saver(ctx);

        ctx.translate(12, 12);

        EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
        EXPECT_EQ(primaryContext.clipBounds(), FloatRect(0, 0, 26, 26));

        ctx.clip(FloatRect(0, 0, 10, 10));

        EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
        EXPECT_EQ(primaryContext.clipBounds(), FloatRect(0, 0, 10, 10));

        ctx.rotate(std::numbers::pi / 6);

        EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
        EXPECT_EQ(primaryContext.clipBounds(), FloatRect(0, -5, 14, 14));
    }

    EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
    EXPECT_EQ(primaryContext.clipBounds(), FloatRect(12, 12, 26, 26));

    // Make the CTM non-invertible.
    ctx.scale({ 0, 1 });

    EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
    EXPECT_EQ(primaryContext.clipBounds(), FloatRect(25, 25, 50, 50));
}

TEST(BifurcatedGraphicsContextTests, ApplyDeviceScaleFactor)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, 100, 100, 8, 4 * 100, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContextCG(primaryCGContext.get());
    GraphicsContext& primaryContext = primaryContextCG;

    RecorderImpl secondaryContextDL({ }, FloatRect(0, 0, 100, 100), { });
    GraphicsContext& secondaryContext = secondaryContextDL;

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    ctx.applyDeviceScaleFactor(2);

    auto primaryCTM = primaryContext.getCTM(GraphicsContext::IncludeDeviceScale::DefinitelyIncludeDeviceScale);
    auto secondaryCTM = secondaryContext.getCTM(GraphicsContext::IncludeDeviceScale::DefinitelyIncludeDeviceScale);

    EXPECT_EQ(primaryCTM.xScale(), secondaryCTM.xScale());
    EXPECT_EQ(primaryCTM.yScale(), secondaryCTM.yScale());

    EXPECT_EQ(primaryCTM.xScale(), 2);
    EXPECT_EQ(primaryCTM.yScale(), 2);
}

TEST(BifurcatedGraphicsContextTests, ClipToImageBuffer)
{
    RecorderImpl primaryContext({ }, FloatRect(0, 0, contextWidth, contextHeight), { });
    RecorderImpl secondaryContext({ }, FloatRect(0, 0, contextWidth, contextHeight), { });

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    auto imageBuffer = ImageBuffer::create({ 100, 100 }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    ctx.clipToImageBuffer(*imageBuffer, { 0, 0, 100, 100 });

    auto runTest = [&] (const DisplayList& displayList) {
        EXPECT_FALSE(displayList.items().empty());
        bool sawClipToImageBuffer = false;

        for (auto& item : displayList.items()) {
            if (std::holds_alternative<ClipToImageBuffer>(item))
                sawClipToImageBuffer = true;
        }

        EXPECT_TRUE(sawClipToImageBuffer);
    };

    // Ensure that both contexts have clip-to-image-buffer commands.
    runTest(primaryContext.takeDisplayList());
    runTest(secondaryContext.takeDisplayList());
}

TEST(BifurcatedGraphicsContextTests, NutjobAdapterOraclePhaseOne)
{
    auto programPath = envString("NUTJOB_ADAPTER_PROGRAM");
    if (programPath.isEmpty())
        GTEST_SKIP() << "oracle environment not configured";

    auto artifactsDirectory = envString("NUTJOB_ADAPTER_ARTIFACTS");
    ASSERT_FALSE(artifactsDirectory.isEmpty());
    ASSERT_TRUE(FileSystem::makeAllDirectories(artifactsDirectory));

    auto statusPath = FileSystem::pathByAppendingComponent(artifactsDirectory, "oracle-status.txt"_s);
    auto mode = envString("NUTJOB_ADAPTER_MODE");
    auto adapterLane = envString("NUTJOB_ADAPTER_LANE");
    bool strictMode = envFlagEnabled("NUTJOB_ADAPTER_STRICT");

    auto writeStatus = [&](StringView status) {
        EXPECT_TRUE(writeStringToPath(statusPath, status));
    };

    if (mode != "structural"_s) {
        writeStatus("unsupported-op"_s);
        return;
    }

    String error;
    auto program = parseOracleProgram(programPath, error);
    if (!program) {
        writeStatus("error"_s);
        FAIL() << error.utf8().data();
    }

    auto status = runOracleProgram(*program, artifactsDirectory, adapterLane, strictMode, error);
    switch (status) {
    case OracleStatus::Ok:
        writeStatus("ok"_s);
        break;
    case OracleStatus::UnsupportedOp:
        if (strictMode) {
            writeStatus("unsupported-op"_s);
            return;
        }
        writeStatus("ok"_s);
        break;
    case OracleStatus::Error:
        writeStatus("error"_s);
        FAIL() << error.utf8().data();
        break;
    }
}

} // namespace TestWebKitAPI

#endif // USE(CG)
