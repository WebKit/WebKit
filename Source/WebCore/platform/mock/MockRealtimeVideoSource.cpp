/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include <math.h>
#include <wtf/UUID.h>
#include <wtf/text/StringView.h>

namespace WebCore {

class MockRealtimeVideoSourceFactory : public RealtimeMediaSource::VideoCaptureFactory
{
public:
    CaptureSourceOrError createVideoCaptureSource(const CaptureDevice& device, const MediaConstraints* constraints) final
    {
        ASSERT(MockRealtimeMediaSource::captureDeviceWithPersistentID(device.type(), device.persistentId()));

        switch (device.type()) {
        case CaptureDevice::DeviceType::Camera:
        case CaptureDevice::DeviceType::Screen:
            return MockRealtimeVideoSource::create(device.persistentId(), device.label(), constraints);
            break;
        case CaptureDevice::DeviceType::Application:
        case CaptureDevice::DeviceType::Window:
        case CaptureDevice::DeviceType::Browser:
        case CaptureDevice::DeviceType::Microphone:
        case CaptureDevice::DeviceType::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }

        return { };
    }

#if PLATFORM(IOS)
private:
    void setVideoCapturePageState(bool interrupted, bool pageMuted)
    {
        if (activeSource())
            activeSource()->setInterrupted(interrupted, pageMuted);
    }
#endif
};

#if !PLATFORM(MAC) && !PLATFORM(IOS)
CaptureSourceOrError MockRealtimeVideoSource::create(const String& deviceID, const String& name, const MediaConstraints* constraints)
{
    auto source = adoptRef(*new MockRealtimeVideoSource(deviceID, name));
    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}
#endif

static MockRealtimeVideoSourceFactory& mockVideoCaptureSourceFactory()
{
    static NeverDestroyed<MockRealtimeVideoSourceFactory> factory;
    return factory.get();
}

RealtimeMediaSource::VideoCaptureFactory& MockRealtimeVideoSource::factory()
{
    return mockVideoCaptureSourceFactory();
}

MockRealtimeVideoSource::MockRealtimeVideoSource(const String& deviceID, const String& name)
    : MockRealtimeMediaSource(deviceID, RealtimeMediaSource::Type::Video, name)
    , m_timer(RunLoop::current(), this, &MockRealtimeVideoSource::generateFrame)
{
    switch (device()) {
    case MockDevice::Camera1:
        setFrameRate(30);
        setFacingMode(RealtimeMediaSourceSettings::User);
        break;
    case MockDevice::Camera2:
        setFrameRate(15);
        setFacingMode(RealtimeMediaSourceSettings::Environment);
        break;
    case MockDevice::Screen1:
        setFrameRate(30);
        break;
    case MockDevice::Screen2:
        setFrameRate(10);
        break;
    case MockDevice::Microphone1:
    case MockDevice::Microphone2:
    case MockDevice::Invalid:
        ASSERT_NOT_REACHED();
        break;
    }

    m_dashWidths.reserveInitialCapacity(2);
    m_dashWidths.uncheckedAppend(6);
    m_dashWidths.uncheckedAppend(6);
}

MockRealtimeVideoSource::~MockRealtimeVideoSource()
{
#if PLATFORM(IOS)
    MockRealtimeMediaSourceCenter::videoCaptureSourceFactory().unsetActiveSource(*this);
#endif
}

void MockRealtimeVideoSource::startProducingData()
{
#if PLATFORM(IOS)
    MockRealtimeMediaSourceCenter::videoCaptureSourceFactory().setActiveSource(*this);
#endif

    if (size().isEmpty()) {
        setWidth(640);
        setHeight(480);
    }

    m_startTime = MonotonicTime::now();
    m_timer.startRepeating(1_ms * lround(1000 / frameRate()));
}

void MockRealtimeVideoSource::stopProducingData()
{
    m_timer.stop();
    m_elapsedTime += MonotonicTime::now() - m_startTime;
    m_startTime = MonotonicTime::nan();
}

Seconds MockRealtimeVideoSource::elapsedTime()
{
    if (std::isnan(m_startTime))
        return m_elapsedTime;

    return m_elapsedTime + (MonotonicTime::now() - m_startTime);
}

void MockRealtimeVideoSource::updateSettings(RealtimeMediaSourceSettings& settings)
{
    if (mockCamera())
        settings.setFacingMode(facingMode());
    else {
        settings.setDisplaySurface(RealtimeMediaSourceSettings::DisplaySurfaceType::Monitor);
        settings.setLogicalSurface(true);
    }
    settings.setFrameRate(frameRate());
    IntSize size = this->size();
    settings.setWidth(size.width());
    settings.setHeight(size.height());
    if (aspectRatio())
        settings.setAspectRatio(aspectRatio());
}

void MockRealtimeVideoSource::initializeCapabilities(RealtimeMediaSourceCapabilities& capabilities)
{
    if (mockCamera()) {
        if (device() == MockDevice::Camera1)
            capabilities.addFacingMode(RealtimeMediaSourceSettings::User);
        else
            capabilities.addFacingMode(RealtimeMediaSourceSettings::Environment);

        capabilities.setWidth(CapabilityValueOrRange(320, 1920));
        capabilities.setHeight(CapabilityValueOrRange(240, 1080));
        capabilities.setFrameRate(CapabilityValueOrRange(15.0, 60.0));
        capabilities.setAspectRatio(CapabilityValueOrRange(4 / 3.0, 16 / 9.0));
    } else {
        capabilities.setWidth(CapabilityValueOrRange(72, 2880));
        capabilities.setHeight(CapabilityValueOrRange(45, 1800));
        capabilities.setFrameRate(CapabilityValueOrRange(.01, 60.0));
    }
}

void MockRealtimeVideoSource::initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints& supportedConstraints)
{
    supportedConstraints.setSupportsWidth(true);
    supportedConstraints.setSupportsHeight(true);
    supportedConstraints.setSupportsAspectRatio(true);
    supportedConstraints.setSupportsFrameRate(true);
    if (mockCamera())
        supportedConstraints.setSupportsFacingMode(true);
}

bool MockRealtimeVideoSource::applyFrameRate(double rate)
{
    if (m_timer.isActive())
        m_timer.startRepeating(1_ms * lround(1000 / rate));

    updateSampleBuffer();
    return true;
}

bool MockRealtimeVideoSource::applySize(const IntSize& size)
{
    m_baseFontSize = size.height() * .08;
    m_bipBopFontSize = m_baseFontSize * 2.5;
    m_statsFontSize = m_baseFontSize * .5;
    m_imageBuffer = nullptr;
    return true;
}

void MockRealtimeVideoSource::drawAnimation(GraphicsContext& context)
{
    float radius = size().width() * .09;
    FloatPoint location(size().width() * .8, size().height() * .3);

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
    static const RGBA32 magenta = 0xffff00ff;
    static const RGBA32 yellow = 0xffffff00;
    static const RGBA32 blue = 0xff0000ff;
    static const RGBA32 red = 0xffff0000;
    static const RGBA32 green = 0xff008000;
    static const RGBA32 cyan = 0xFF00FFFF;

    IntSize size = this->size();
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
    Color boxColors[] = { Color::white, yellow, cyan, green, magenta, red, blue };
    for (unsigned i = 0; i < sizeof(boxColors) / sizeof(boxColors[0]); i++) {
        context.fillRect(FloatRect(boxLeft, boxTop, boxSize + 1, boxSize + 1), boxColors[i]);
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
    fontDescription.setOneFamily("Courier");
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

    IntSize size = this->size();
    FloatPoint timeLocation(size.width() * .05, size.height() * .15);
    context.setFillColor(Color::white);
    context.setTextDrawingMode(TextModeFill);
    String string = String::format("%02u:%02u:%02u.%03u", hours, minutes, seconds, milliseconds % 1000);
    context.drawText(timeFont, TextRun((StringView(string))), timeLocation);

    string = String::format("%06u", m_frameNumber++);
    timeLocation.move(0, m_baseFontSize);
    context.drawText(timeFont, TextRun((StringView(string))), timeLocation);

    FloatPoint statsLocation(size.width() * .65, size.height() * .75);
    string = String::format("Frame rate: %ffps", frameRate());
    context.drawText(statsFont, TextRun((StringView(string))), statsLocation);

    string = String::format("Size: %u x %u", size.width(), size.height());
    statsLocation.move(0, m_statsFontSize);
    context.drawText(statsFont, TextRun((StringView(string))), statsLocation);

    if (mockCamera()) {
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
        string = String::format("Camera: %s", camera);
        statsLocation.move(0, m_statsFontSize);
        context.drawText(statsFont, TextRun((StringView(string))), statsLocation);
    } else {
        statsLocation.move(0, m_statsFontSize);
        context.drawText(statsFont, TextRun((StringView(device() == MockDevice::Screen1 ? "Screen 1" : "Screen 2"))), statsLocation);
    }

    FloatPoint bipBopLocation(size.width() * .6, size.height() * .6);
    unsigned frameMod = m_frameNumber % 60;
    if (frameMod <= 15) {
        context.setFillColor(Color::cyan);
        String bip(ASCIILiteral("Bip"));
        context.drawText(bipBopFont, TextRun(StringView(bip)), bipBopLocation);
    } else if (frameMod > 30 && frameMod <= 45) {
        context.setFillColor(Color::yellow);
        String bop(ASCIILiteral("Bop"));
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

    auto& size = this->size();
    FloatRect frameRect(FloatPoint(), size);

    Color fillColor;
    switch (device()) {
    case MockDevice::Camera1:
        fillColor = Color::black;
        break;
    case MockDevice::Camera2:
        fillColor = Color::darkGray;
        break;
    case MockDevice::Screen1:
        fillColor = Color::lightGray;
        break;
    case MockDevice::Screen2:
        fillColor = Color::yellow;
        break;
    case MockDevice::Microphone1:
    case MockDevice::Microphone2:
    case MockDevice::Invalid:
        ASSERT_NOT_REACHED();
        break;
    }

    context.fillRect(FloatRect(FloatPoint(), size), fillColor);

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

    m_imageBuffer = ImageBuffer::create(size(), Unaccelerated);
    if (!m_imageBuffer)
        return nullptr;

    m_imageBuffer->context().setImageInterpolationQuality(InterpolationDefault);
    m_imageBuffer->context().setStrokeThickness(1);

    return m_imageBuffer.get();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
