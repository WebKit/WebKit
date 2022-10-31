/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#include "MockRealtimeVideoSource.h"

#if ENABLE(MEDIA_STREAM)
#include "CaptureDevice.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "MockRealtimeMediaSourceCenter.h"
#include "NotImplemented.h"
#include "PlatformLayer.h"
#include "RealtimeMediaSourceSettings.h"
#include "RealtimeVideoSource.h"
#include <math.h>
#include <wtf/UUID.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

#if !PLATFORM(MAC) && !PLATFORM(IOS_FAMILY) && !USE(GSTREAMER)
CaptureSourceOrError MockRealtimeVideoSource::create(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
#ifndef NDEBUG
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return { "No mock camera device"_s };
#endif

    auto source = adoptRef(*new MockRealtimeVideoSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalts), pageIdentifier));
    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(RealtimeVideoSource::create(WTFMove(source)));
}
#endif

static HashSet<MockRealtimeVideoSource*>& allMockRealtimeVideoSource()
{
    static MainThreadNeverDestroyed<HashSet<MockRealtimeVideoSource*>> videoSources;
    return videoSources;
}

MockRealtimeVideoSource::MockRealtimeVideoSource(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts, PageIdentifier pageIdentifier)
    : RealtimeVideoCaptureSource(CaptureDevice { WTFMove(deviceID), CaptureDevice::DeviceType::Camera, WTFMove(name) }, WTFMove(hashSalts), pageIdentifier)
    , m_emitFrameTimer(RunLoop::current(), this, &MockRealtimeVideoSource::generateFrame)
{
    allMockRealtimeVideoSource().add(this);

    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(persistentID());
    ASSERT(device);
    m_device = *device;

    m_dashWidths.reserveInitialCapacity(2);
    m_dashWidths.uncheckedAppend(6);
    m_dashWidths.uncheckedAppend(6);

    if (mockDisplay()) {
        auto& properties = std::get<MockDisplayProperties>(m_device.properties);
        setIntrinsicSize(properties.defaultSize);
        setSize(properties.defaultSize);
        m_fillColor = properties.fillColor;
        return;
    }

    auto& properties = std::get<MockCameraProperties>(m_device.properties);
    setFrameRate(properties.defaultFrameRate);
    setFacingMode(properties.facingMode);
    m_fillColor = properties.fillColor;
}

MockRealtimeVideoSource::~MockRealtimeVideoSource()
{
    allMockRealtimeVideoSource().remove(this);
}

bool MockRealtimeVideoSource::supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> rate)
{
    // FIXME: consider splitting mock display into another class so we don't don't have to do this silly dance
    // because of the RealtimeVideoSource inheritance.
    if (mockCamera())
        return RealtimeVideoCaptureSource::supportsSizeAndFrameRate(width, height, rate);

    return RealtimeMediaSource::supportsSizeAndFrameRate(width, height, rate);
}

void MockRealtimeVideoSource::setSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> rate)
{
    // FIXME: consider splitting mock display into another class so we don't don't have to do this silly dance
    // because of the RealtimeVideoSource inheritance.
    if (mockCamera()) {
        RealtimeVideoCaptureSource::setSizeAndFrameRate(width, height, rate);
        return;
    }

    RealtimeMediaSource::setSizeAndFrameRate(width, height, rate);
}

void MockRealtimeVideoSource::generatePresets()
{
    ASSERT(mockCamera());
    setSupportedPresets(WTFMove(std::get<MockCameraProperties>(m_device.properties).presets));
}

const RealtimeMediaSourceCapabilities& MockRealtimeVideoSource::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

        if (mockCamera()) {
            capabilities.addFacingMode(std::get<MockCameraProperties>(m_device.properties).facingMode);
            capabilities.setDeviceId(hashedId());
            updateCapabilities(capabilities);
        } else if (mockDisplay()) {
            capabilities.setWidth(CapabilityValueOrRange(72, std::get<MockDisplayProperties>(m_device.properties).defaultSize.width()));
            capabilities.setHeight(CapabilityValueOrRange(45, std::get<MockDisplayProperties>(m_device.properties).defaultSize.height()));
            capabilities.setFrameRate(CapabilityValueOrRange(.01, 60.0));
        } else {
            capabilities.setWidth(CapabilityValueOrRange(72, 2880));
            capabilities.setHeight(CapabilityValueOrRange(45, 1800));
            capabilities.setFrameRate(CapabilityValueOrRange(.01, 60.0));
        }

        m_capabilities = WTFMove(capabilities);
    }

    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& MockRealtimeVideoSource::settings()
{
    if (m_currentSettings)
        return m_currentSettings.value();

    RealtimeMediaSourceSettings settings;
    settings.setLabel(name());
    if (mockCamera()) {
        settings.setFacingMode(facingMode());
        settings.setDeviceId(hashedId());
    } else {
        settings.setDisplaySurface(mockScreen() ? RealtimeMediaSourceSettings::DisplaySurfaceType::Monitor : RealtimeMediaSourceSettings::DisplaySurfaceType::Window);
        settings.setLogicalSurface(false);
    }
    settings.setDeviceId(hashedId());
    settings.setFrameRate(frameRate());
    auto size = this->size();
    if (mockCamera()) {
        if (m_deviceOrientation == VideoFrame::Rotation::Left || m_deviceOrientation == VideoFrame::Rotation::Right)
            size = size.transposedSize();
    }
    settings.setWidth(size.width());
    settings.setHeight(size.height());
    if (aspectRatio())
        settings.setAspectRatio(aspectRatio());

    RealtimeMediaSourceSupportedConstraints supportedConstraints;
    supportedConstraints.setSupportsFrameRate(true);
    supportedConstraints.setSupportsWidth(true);
    supportedConstraints.setSupportsHeight(true);
    supportedConstraints.setSupportsAspectRatio(true);
    supportedConstraints.setSupportsDeviceId(true);
    if (mockCamera())
        supportedConstraints.setSupportsFacingMode(true);
    else {
        supportedConstraints.setSupportsDisplaySurface(true);
        supportedConstraints.setSupportsLogicalSurface(true);
    }
    settings.setSupportedConstraints(supportedConstraints);

    m_currentSettings = WTFMove(settings);

    return m_currentSettings.value();
}

void MockRealtimeVideoSource::setFrameRateWithPreset(double frameRate, RefPtr<VideoPreset> preset)
{
    m_preset = WTFMove(preset);
    if (m_preset)
        setIntrinsicSize(m_preset->size);
    if (isProducingData())
        m_emitFrameTimer.startRepeating(1_s / frameRate);
}

IntSize MockRealtimeVideoSource::captureSize() const
{
    return m_preset ? m_preset->size : this->size();
}

void MockRealtimeVideoSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    m_currentSettings = std::nullopt;
    if (settings.containsAny({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height })) {
        m_baseFontSize = captureSize().height() * .08;
        m_bipBopFontSize = m_baseFontSize * 2.5;
        m_statsFontSize = m_baseFontSize * .5;
        m_imageBuffer = nullptr;
    }
}

void MockRealtimeVideoSource::startCaptureTimer()
{
    m_emitFrameTimer.startRepeating(1_s / frameRate());
}

void MockRealtimeVideoSource::startProducingData()
{
    startCaptureTimer();
    m_startTime = MonotonicTime::now();
}

void MockRealtimeVideoSource::stopProducingData()
{
    m_emitFrameTimer.stop();
    m_elapsedTime += MonotonicTime::now() - m_startTime;
    m_startTime = MonotonicTime::nan();
}

Seconds MockRealtimeVideoSource::elapsedTime()
{
    if (std::isnan(m_startTime))
        return m_elapsedTime;

    return m_elapsedTime + (MonotonicTime::now() - m_startTime);
}

void MockRealtimeVideoSource::drawAnimation(GraphicsContext& context)
{
    auto size = captureSize();
    float radius = size.width() * .09;
    FloatPoint location(size.width() * .8, size.height() * .3);

    m_path.clear();
    m_path.moveTo(location);
    m_path.addArc(location, radius, 0, 2 * piFloat, false);
    m_path.closeSubpath();
    context.setFillColor(Color::white);
    context.setFillRule(WindRule::NonZero);
    context.fillPath(m_path);

    float endAngle = piFloat * (((fmod(m_frameNumber, frameRate()) + 0.5) * (2.0 / frameRate())) + 1);
    m_path.clear();
    m_path.moveTo(location);
    m_path.addArc(location, radius, 1.5 * piFloat, endAngle, false);
    m_path.closeSubpath();
    context.setFillColor(Color::gray);
    context.setFillRule(WindRule::NonZero);
    context.fillPath(m_path);
}

void MockRealtimeVideoSource::drawBoxes(GraphicsContext& context)
{
    IntSize size = captureSize();
    float boxSize = size.width() * .035;
    float boxTop = size.height() * .6;

    m_path.clear();
    FloatRect frameRect(2, 2, size.width() - 3, size.height() - 3);
    context.setStrokeColor(Color::white);
    context.setStrokeThickness(3);
    context.setLineDash(m_dashWidths, 0);
    m_path.addRect(frameRect);
    m_path.closeSubpath();
    context.strokePath(m_path);

    context.setLineDash(DashArray(), 0);
    m_path.clear();
    m_path.moveTo(FloatPoint(0, boxTop + boxSize));
    m_path.addLineTo(FloatPoint(size.width(), boxTop + boxSize));
    m_path.closeSubpath();
    context.setStrokeColor(Color::white);
    context.setStrokeThickness(2);
    context.strokePath(m_path);

    context.setStrokeThickness(1);
    float boxLeft = boxSize;
    m_path.clear();
    for (unsigned i = 0; i < boxSize / 4; i++) {
        m_path.moveTo(FloatPoint(boxLeft + 4 * i, boxTop));
        m_path.addLineTo(FloatPoint(boxLeft + 4 * i, boxTop + boxSize));
    }
    boxLeft += boxSize + 2;
    for (unsigned i = 0; i < boxSize / 4; i++) {
        m_path.moveTo(FloatPoint(boxLeft, boxTop + 4 * i));
        m_path.addLineTo(FloatPoint(boxLeft + boxSize - 1, boxTop + 4 * i));
    }
    context.setStrokeThickness(3);
    boxLeft += boxSize + 2;
    for (unsigned i = 0; i < boxSize / 8; i++) {
        m_path.moveTo(FloatPoint(boxLeft + 8 * i, boxTop));
        m_path.addLineTo(FloatPoint(boxLeft + 8 * i, boxTop + boxSize - 1));
    }
    boxLeft += boxSize + 2;
    for (unsigned i = 0; i < boxSize / 8; i++) {
        m_path.moveTo(FloatPoint(boxLeft, boxTop + 8 * i));
        m_path.addLineTo(FloatPoint(boxLeft + boxSize - 1, boxTop + 8 * i));
    }

    boxTop += boxSize + 2;
    boxLeft = boxSize;
    constexpr SRGBA<uint8_t> boxColors[] = { Color::white, Color::yellow, Color::cyan, Color::darkGreen, Color::magenta, Color::red, Color::blue };
    for (auto& boxColor : boxColors) {
        context.fillRect(FloatRect(boxLeft, boxTop, boxSize + 1, boxSize + 1), boxColor);
        boxLeft += boxSize + 1;
    }
    context.strokePath(m_path);
}

void MockRealtimeVideoSource::drawText(GraphicsContext& context)
{
    unsigned milliseconds = lround(elapsedTime().milliseconds());
    unsigned seconds = milliseconds / 1000 % 60;
    unsigned minutes = seconds / 60 % 60;
    unsigned hours = minutes / 60 % 60;

    FontCascadeDescription fontDescription;
    fontDescription.setOneFamily("Courier"_s);
    fontDescription.setWeight(FontSelectionValue(500));

    fontDescription.setSpecifiedSize(m_baseFontSize);
    fontDescription.setComputedSize(m_baseFontSize);
    FontCascade timeFont { FontCascadeDescription { fontDescription }, 0, 0 };
    timeFont.update(nullptr);

    fontDescription.setSpecifiedSize(m_bipBopFontSize);
    fontDescription.setComputedSize(m_bipBopFontSize);
    FontCascade bipBopFont { FontCascadeDescription { fontDescription }, 0, 0 };
    bipBopFont.update(nullptr);

    fontDescription.setSpecifiedSize(m_statsFontSize);
    fontDescription.setComputedSize(m_statsFontSize);
    FontCascade statsFont { WTFMove(fontDescription), 0, 0 };
    statsFont.update(nullptr);

    IntSize captureSize = this->captureSize();
    FloatPoint timeLocation(captureSize.width() * .05, captureSize.height() * .15);
    context.setFillColor(Color::white);
    context.setTextDrawingMode(TextDrawingMode::Fill);
    auto string = makeString(pad('0', 2, hours), ':', pad('0', 2, minutes), ':', pad('0', 2, seconds), '.', pad('0', 3, milliseconds % 1000));
    context.drawText(timeFont, TextRun((StringView(string))), timeLocation);

    string = makeString(pad('0', 6, m_frameNumber++));
    timeLocation.move(0, m_baseFontSize);
    context.drawText(timeFont, TextRun((StringView(string))), timeLocation);

    FloatPoint statsLocation(captureSize.width() * .45, captureSize.height() * .75);
    string = makeString("Requested frame rate: ", FormattedNumber::fixedWidth(frameRate(), 1), " fps");
    context.drawText(statsFont, TextRun((StringView(string))), statsLocation);

    statsLocation.move(0, m_statsFontSize);
    string = makeString("Observed frame rate: ", FormattedNumber::fixedWidth(observedFrameRate(), 1), " fps");
    context.drawText(statsFont, TextRun((StringView(string))), statsLocation);

    auto size = this->size();
    statsLocation.move(0, m_statsFontSize);
    string = makeString("Size: ", size.width(), " x ", size.height());
    context.drawText(statsFont, TextRun((StringView(string))), statsLocation);

    if (mockCamera()) {
        statsLocation.move(0, m_statsFontSize);
        string = makeString("Preset size: ", captureSize.width(), " x ", captureSize.height());
        context.drawText(statsFont, TextRun((StringView(string))), statsLocation);

        const char* camera;
        switch (facingMode()) {
        case RealtimeMediaSourceSettings::User:
            camera = "User facing";
            break;
        case RealtimeMediaSourceSettings::Environment:
            camera = "Environment facing";
            break;
        case RealtimeMediaSourceSettings::Left:
            camera = "Left facing";
            break;
        case RealtimeMediaSourceSettings::Right:
            camera = "Right facing";
            break;
        case RealtimeMediaSourceSettings::Unknown:
            camera = "Unknown";
            break;
        }
        string = makeString("Camera: ", camera);
        statsLocation.move(0, m_statsFontSize);
        context.drawText(statsFont, TextRun(string), statsLocation);
    } else if (!name().isNull()) {
        statsLocation.move(0, m_statsFontSize);
        context.drawText(statsFont, TextRun { name().string() }, statsLocation);
    }

    FloatPoint bipBopLocation(captureSize.width() * .6, captureSize.height() * .6);
    unsigned frameMod = m_frameNumber % 60;
    if (frameMod <= 15) {
        context.setFillColor(Color::cyan);
        String bip("Bip"_s);
        context.drawText(bipBopFont, TextRun(StringView(bip)), bipBopLocation);
    } else if (frameMod > 30 && frameMod <= 45) {
        context.setFillColor(Color::yellow);
        String bop("Bop"_s);
        context.drawText(bipBopFont, TextRun(StringView(bop)), bipBopLocation);
    }
}

void MockRealtimeVideoSource::delaySamples(Seconds delta)
{
    m_delayUntil = MonotonicTime::now() + delta;
}

void MockRealtimeVideoSource::generateFrame()
{
    if (m_delayUntil) {
        if (m_delayUntil < MonotonicTime::now())
            return;
        m_delayUntil = MonotonicTime();
    }

    ImageBuffer* buffer = imageBuffer();
    if (!buffer)
        return;

    GraphicsContext& context = buffer->context();
    GraphicsContextStateSaver stateSaver(context);

    auto size = this->captureSize();
    FloatRect frameRect(FloatPoint(), size);

    context.fillRect(FloatRect(FloatPoint(), size), m_fillColor);

    if (!muted()) {
        drawText(context);
        drawAnimation(context);
        drawBoxes(context);
    }

    updateSampleBuffer();
}

ImageBuffer* MockRealtimeVideoSource::imageBuffer() const
{
    if (m_imageBuffer)
        return m_imageBuffer.get();

    m_imageBuffer = ImageBuffer::create(captureSize(), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!m_imageBuffer)
        return nullptr;

    m_imageBuffer->context().setImageInterpolationQuality(InterpolationQuality::Default);
    m_imageBuffer->context().setStrokeThickness(1);

    return m_imageBuffer.get();
}

bool MockRealtimeVideoSource::mockDisplayType(CaptureDevice::DeviceType type) const
{
    if (!std::holds_alternative<MockDisplayProperties>(m_device.properties))
        return false;

    return std::get<MockDisplayProperties>(m_device.properties).type == type;
}

void MockRealtimeVideoSource::orientationChanged(int orientation)
{
    auto deviceOrientation = m_deviceOrientation;
    switch (orientation) {
    case 0:
        m_deviceOrientation = VideoFrame::Rotation::None;
        break;
    case 90:
        m_deviceOrientation = VideoFrame::Rotation::Right;
        break;
    case -90:
        m_deviceOrientation = VideoFrame::Rotation::Left;
        break;
    case 180:
        m_deviceOrientation = VideoFrame::Rotation::UpsideDown;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    if (deviceOrientation == m_deviceOrientation)
        return;

    notifySettingsDidChangeObservers({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height });
}

void MockRealtimeVideoSource::monitorOrientation(OrientationNotifier& notifier)
{
    if (!mockCamera())
        return;

    notifier.addObserver(*this);
    orientationChanged(notifier.orientation());
}

void MockRealtimeVideoSource::setIsInterrupted(bool isInterrupted)
{
    for (auto* source : allMockRealtimeVideoSource()) {
        if (!source->isProducingData())
            continue;
        if (isInterrupted)
            source->m_emitFrameTimer.stop();
        else
            source->startCaptureTimer();
        source->notifyMutedChange(isInterrupted);
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
