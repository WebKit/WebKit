/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
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


#if ENABLE(VIDEO) && !USE(GSTREAMER) && !USE(MEDIA_FOUNDATION)

#include "FullscreenVideoController.h"

#include "WebKitDLL.h"
#include "WebView.h"
#include <WebCore/BitmapInfo.h>
#include <WebCore/Chrome.h>
#include <WebCore/FloatRoundedRect.h>
#include <WebCore/FontCascade.h>
#include <WebCore/FontSelector.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsContextWin.h>
#include <WebCore/HWndDC.h>
#include <WebCore/Page.h>
#include <WebCore/TextRun.h>
#include <windowsx.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringConcatenateNumbers.h>

#if USE(CA)
#include <WebCore/PlatformCALayerClient.h>
#include <WebCore/PlatformCALayerWin.h>
#endif

using namespace WebCore;

static const Seconds timerInterval { 33_ms };

// HUD Size
static const int windowHeight = 59;
static const int windowWidth = 438;

// Margins and button sizes
static const int margin = 9;
static const int marginTop = 9;
static const int buttonSize = 25;
static const int buttonMiniSize = 16;
static const int volumeSliderWidth = 50;
static const int timeSliderWidth = 310;
static const int sliderHeight = 8;
static const int volumeSliderButtonSize = 10;
static const int timeSliderButtonSize = 8;
static const int textSize = 11;
static const float initialHUDPositionY = 0.9; // Initial Y position of HUD in percentage from top of screen

// Background values
static const int borderRadius = 12;
static const int borderThickness = 2;

// Colors
static constexpr auto backgroundColor = SRGBA<uint8_t> { 32, 32, 32, 160 };
static constexpr auto borderColor = SRGBA<uint8_t> { 160, 160, 160 };
static constexpr auto sliderGutterColor = SRGBA<uint8_t> { 20, 20, 20 };
static constexpr auto sliderButtonColor = SRGBA<uint8_t> { 128, 128, 128 };
static constexpr auto textColor = Color::white;

HUDButton::HUDButton(HUDButtonType type, const IntPoint& position)
    : HUDWidget(IntRect(position, IntSize()))
    , m_type(type)
    , m_showAltButton(false)
{
    const char* buttonResource = 0;
    const char* buttonResourceAlt = 0;
    switch (m_type) {
    case PlayPauseButton:
        buttonResource = "fsVideoPlay";
        buttonResourceAlt = "fsVideoPause";
        break;
    case TimeSliderButton:
        break;
    case VolumeUpButton:
        buttonResource = "fsVideoAudioVolumeHigh";
        break;
    case VolumeSliderButton:
        break;
    case VolumeDownButton:
        buttonResource = "fsVideoAudioVolumeLow";
        break;
    case ExitFullscreenButton:
        buttonResource = "fsVideoExitFullscreen";
        break;
    }

    if (buttonResource) {
        m_buttonImage = Image::loadPlatformResource(buttonResource);
        m_rect.setWidth(m_buttonImage->width());
        m_rect.setHeight(m_buttonImage->height());
    }
    if (buttonResourceAlt)
        m_buttonImageAlt = Image::loadPlatformResource(buttonResourceAlt);
}

void HUDButton::draw(GraphicsContext& context)
{
    Image* image = (m_showAltButton && m_buttonImageAlt) ? m_buttonImageAlt.get() : m_buttonImage.get();
    context.drawImage(*image, m_rect.location());
}

HUDSlider::HUDSlider(HUDSliderButtonShape shape, int buttonSize, const IntRect& rect)
    : HUDWidget(rect)
    , m_buttonShape(shape)
    , m_buttonSize(buttonSize)
    , m_buttonPosition(0)
    , m_dragStartOffset(0)
{
}

void HUDSlider::draw(GraphicsContext& context)
{
    // Draw gutter
    IntSize radius(m_rect.height() / 2, m_rect.height() / 2);
    context.fillRoundedRect(FloatRoundedRect(m_rect, radius, radius, radius, radius), Color(sliderGutterColor));

    // Draw button
    context.setStrokeColor(Color(sliderButtonColor));
    context.setFillColor(Color(sliderButtonColor));

    if (m_buttonShape == RoundButton) {
        context.drawEllipse(IntRect(m_rect.location().x() + m_buttonPosition, m_rect.location().y() - (m_buttonSize - m_rect.height()) / 2, m_buttonSize, m_buttonSize));
        return;
    }

    // Draw a diamond
    float half = static_cast<float>(m_buttonSize) / 2;

    Vector<FloatPoint> points = {
        FloatPoint(m_rect.location().x() + m_buttonPosition + half, m_rect.location().y()),
        FloatPoint(m_rect.location().x() + m_buttonPosition + m_buttonSize, m_rect.location().y() + half),
        FloatPoint(m_rect.location().x() + m_buttonPosition + half, m_rect.location().y() + m_buttonSize),
        FloatPoint(m_rect.location().x() + m_buttonPosition, m_rect.location().y() + half)
    };
    context.drawPath(Path::polygonPathFromPoints(points));
}

void HUDSlider::drag(const IntPoint& point, bool start)
{
    if (start) {
        // When we start, we need to snap the slider position to the x position if we clicked the gutter.
        // But if we click the button, we need to drag relative to where we clicked down. We only need
        // to check X because we would not even get here unless Y were already inside.
        int relativeX = point.x() - m_rect.location().x();
        if (relativeX >= m_buttonPosition && relativeX <= m_buttonPosition + m_buttonSize)
            m_dragStartOffset = point.x() - m_buttonPosition;
        else
            m_dragStartOffset = m_rect.location().x() + m_buttonSize / 2;
    }

    m_buttonPosition = std::max(0, std::min(m_rect.width() - m_buttonSize, point.x() - m_dragStartOffset));
}

#if USE(CA)
class FullscreenVideoController::LayerClient : public WebCore::PlatformCALayerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LayerClient(FullscreenVideoController* parent) : m_parent(parent) { }

private:
    virtual void platformCALayerLayoutSublayersOfLayer(PlatformCALayer*);
    virtual bool platformCALayerRespondsToLayoutChanges() const { return true; }

    virtual void platformCALayerAnimationStarted(MonotonicTime beginTime) { }
    virtual GraphicsLayer::CompositingCoordinatesOrientation platformCALayerContentsOrientation() const { return GraphicsLayer::CompositingCoordinatesOrientation::BottomUp; }
    virtual void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect&, GraphicsLayerPaintBehavior) { }
    virtual bool platformCALayerShowDebugBorders() const { return false; }
    virtual bool platformCALayerShowRepaintCounter(PlatformCALayer*) const { return false; }
    virtual int platformCALayerIncrementRepaintCount(PlatformCALayer*) { return 0; }

    virtual bool platformCALayerContentsOpaque() const { return false; }
    virtual bool platformCALayerDrawsContent() const { return false; }
    virtual void platformCALayerLayerDidDisplay(PlatformLayer*) { }
    virtual void platformCALayerDidCreateTiles(const Vector<FloatRect>&) { }
    float platformCALayerDeviceScaleFactor() const override { return 1; }

    FullscreenVideoController* m_parent;
};

void FullscreenVideoController::LayerClient::platformCALayerLayoutSublayersOfLayer(PlatformCALayer* layer) 
{
    ASSERT_ARG(layer, layer == m_parent->m_rootChild);

    HTMLVideoElement* videoElement = m_parent->m_videoElement.get();
    if (!videoElement)
        return;


    auto videoLayer = PlatformCALayer::platformCALayerForLayer(videoElement->platformLayer());
    if (!videoLayer || videoLayer->superlayer() != layer)
        return;

    FloatRect layerBounds = layer->bounds();

    FloatSize videoSize = videoElement->player()->naturalSize();
    float scaleFactor;
    if (videoSize.aspectRatio() > layerBounds.size().aspectRatio())
        scaleFactor = layerBounds.width() / videoSize.width();
    else
        scaleFactor = layerBounds.height() / videoSize.height();
    videoSize.scale(scaleFactor);

    // Calculate the centered position based on the videoBounds and layerBounds:
    FloatPoint videoPosition;
    FloatPoint videoOrigin;
    videoOrigin.setX((layerBounds.width() - videoSize.width()) * 0.5);
    videoOrigin.setY((layerBounds.height() - videoSize.height()) * 0.5);
    videoLayer->setPosition(videoOrigin);
    videoLayer->setBounds(FloatRect(FloatPoint(), videoSize));
}
#endif

FullscreenVideoController::FullscreenVideoController()
    : m_hudWindow(0)
    , m_playPauseButton(HUDButton::PlayPauseButton, IntPoint((windowWidth - buttonSize) / 2, marginTop))
    , m_timeSliderButton(HUDButton::TimeSliderButton, IntPoint(0, 0))
    , m_volumeUpButton(HUDButton::VolumeUpButton, IntPoint(margin + buttonMiniSize + volumeSliderWidth + buttonMiniSize / 2, marginTop + (buttonSize - buttonMiniSize) / 2))
    , m_volumeSliderButton(HUDButton::VolumeSliderButton, IntPoint(0, 0))
    , m_volumeDownButton(HUDButton::VolumeDownButton, IntPoint(margin, marginTop + (buttonSize - buttonMiniSize) / 2))
    , m_exitFullscreenButton(HUDButton::ExitFullscreenButton, IntPoint(windowWidth - 2 * margin - buttonMiniSize, marginTop + (buttonSize - buttonMiniSize) / 2))
    , m_volumeSlider(HUDSlider::RoundButton, volumeSliderButtonSize, IntRect(IntPoint(margin + buttonMiniSize, marginTop + (buttonSize - buttonMiniSize) / 2 + buttonMiniSize / 2 - sliderHeight / 2), IntSize(volumeSliderWidth, sliderHeight)))
    , m_timeSlider(HUDSlider::DiamondButton, timeSliderButtonSize, IntRect(IntPoint(windowWidth / 2 - timeSliderWidth / 2, windowHeight - margin - sliderHeight), IntSize(timeSliderWidth, sliderHeight)))
    , m_hitWidget(0)
    , m_movingWindow(false)
    , m_timer(*this, &FullscreenVideoController::timerFired)
#if USE(CA)
    , m_layerClient(makeUnique<LayerClient>(this))
    , m_rootChild(PlatformCALayerWin::create(PlatformCALayer::LayerTypeLayer, m_layerClient.get()))
#endif
#if ENABLE(FULLSCREEN_API)
    , m_fullscreenWindow(makeUnique<MediaPlayerPrivateFullscreenWindow>(static_cast<MediaPlayerPrivateFullscreenClient*>(this)))
#endif
{
}

FullscreenVideoController::~FullscreenVideoController()
{
#if USE(CA)
    m_rootChild->setOwner(0);
#endif
}

void FullscreenVideoController::setVideoElement(HTMLVideoElement* videoElement)
{
    if (videoElement == m_videoElement)
        return;

    m_videoElement = videoElement;
    if (!m_videoElement) {
        // Can't do full-screen, just get out
        exitFullscreen();
    }
}

void FullscreenVideoController::enterFullscreen()
{
#if ENABLE(FULLSCREEN_API)
    if (!m_videoElement)
        return;

    WebView* webView = kit(m_videoElement->document().page());
    HWND parentHwnd = webView ? webView->viewWindow() : 0;

    m_fullscreenWindow->createWindow(parentHwnd);
    ::ShowWindow(m_fullscreenWindow->hwnd(), SW_SHOW);

#if USE(CA)
    m_fullscreenWindow->setRootChildLayer(*m_rootChild);

    auto videoLayer = PlatformCALayer::platformCALayerForLayer(m_videoElement->platformLayer());
    ASSERT(videoLayer);
    m_rootChild->appendSublayer(*videoLayer);
    m_rootChild->setNeedsLayout();
    m_rootChild->setGeometryFlipped(1);
#endif

    RECT windowRect;
    GetClientRect(m_fullscreenWindow->hwnd(), &windowRect);
    m_fullscreenSize.setWidth(windowRect.right - windowRect.left);
    m_fullscreenSize.setHeight(windowRect.bottom - windowRect.top);

    createHUDWindow();
#endif
}

void FullscreenVideoController::exitFullscreen()
{
    SetWindowLongPtr(m_hudWindow, 0, 0);

#if ENABLE(FULLSCREEN_API)
    m_fullscreenWindow = nullptr;
#endif

    ASSERT(!IsWindow(m_hudWindow));
    m_hudWindow = 0;

    // We previously ripped the videoElement's platform layer out
    // of its orginial layer tree to display it in our fullscreen
    // window.  Now, we need to get the layer back in its original
    // tree.
    // 
    // As a side effect of setting the player to invisible/visible,
    // the player's layer will be recreated, and will be picked up 
    // the next time the layer tree is synched.
    m_videoElement->player()->setPageIsVisible(0);
    m_videoElement->player()->setPageIsVisible(1);
}

bool FullscreenVideoController::canPlay() const
{
    return m_videoElement && m_videoElement->canPlay();
}

void FullscreenVideoController::play()
{
    if (m_videoElement)
        m_videoElement->play();
}

void FullscreenVideoController::pause()
{
    if (m_videoElement)
        m_videoElement->pause();
}

float FullscreenVideoController::volume() const
{
    return m_videoElement ? m_videoElement->volume() : 0;
}

void FullscreenVideoController::setVolume(float volume)
{
    if (m_videoElement)
        m_videoElement->setVolume(volume);
}

float FullscreenVideoController::currentTime() const
{
    return m_videoElement ? m_videoElement->currentTime() : 0;
}

void FullscreenVideoController::setCurrentTime(float value)
{
    if (m_videoElement)
        m_videoElement->setCurrentTime(value);
}

float FullscreenVideoController::duration() const
{
    return m_videoElement ? m_videoElement->duration() : 0;
}

void FullscreenVideoController::beginScrubbing()
{
    if (m_videoElement) 
        m_videoElement->beginScrubbing();
}

void FullscreenVideoController::endScrubbing()
{
    if (m_videoElement) 
        m_videoElement->endScrubbing();
}

LRESULT FullscreenVideoController::fullscreenClientWndProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CHAR:
        onChar(wParam);
        break;
    case WM_KEYDOWN:
        onKeyDown(wParam);
        break;
    case WM_LBUTTONDOWN:
        onMouseDown(IntPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        break;
    case WM_MOUSEMOVE:
        onMouseMove(IntPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        break;
    case WM_LBUTTONUP:
        onMouseUp(IntPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        break;
    }

    return DefWindowProc(wnd, message, wParam, lParam);
}

static const LPCWSTR fullscreenVideeoHUDWindowClassName = L"fullscreenVideeoHUDWindowClass";

void FullscreenVideoController::registerHUDWindowClass()
{
    static bool haveRegisteredHUDWindowClass;
    if (haveRegisteredHUDWindowClass)
        return;

    haveRegisteredHUDWindowClass = true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = hudWndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(FullscreenVideoController*);
    wcex.hInstance = gInstance;
    wcex.hIcon = 0;
    wcex.hCursor = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground = 0;
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = fullscreenVideeoHUDWindowClassName;
    wcex.hIconSm = 0;

    RegisterClassEx(&wcex);
}

void FullscreenVideoController::createHUDWindow()
{
#if ENABLE(FULLSCREEN_API)
    m_hudPosition.setX((m_fullscreenSize.width() - windowWidth) / 2);
    m_hudPosition.setY(m_fullscreenSize.height() * initialHUDPositionY - windowHeight / 2);

    // Local variable that will hold the returned pixels. No need to cleanup this value. It
    // will get cleaned up when m_bitmap is destroyed in the dtor
    void* pixels;
    BitmapInfo bitmapInfo = BitmapInfo::createBottomUp(IntSize(windowWidth, windowHeight));
    m_bitmap = adoptGDIObject(::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0));

    // Dirty the window so the HUD draws
    RECT clearRect = { m_hudPosition.x(), m_hudPosition.y(), m_hudPosition.x() + windowWidth, m_hudPosition.y() + windowHeight };
    InvalidateRect(m_fullscreenWindow->hwnd(), &clearRect, true);

    m_playPauseButton.setShowAltButton(!canPlay());
    m_volumeSlider.setValue(volume());
    m_timeSlider.setValue(currentTime() / duration());

    if (!canPlay())
        m_timer.startRepeating(timerInterval);

    registerHUDWindowClass();

    m_hudWindow = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, 
        fullscreenVideeoHUDWindowClassName, 0, WS_POPUP | WS_VISIBLE,
        m_hudPosition.x(), m_hudPosition.y(), 0, 0, m_fullscreenWindow->hwnd(), 0, gInstance, 0);
    ASSERT(::IsWindow(m_hudWindow));
    SetWindowLongPtr(m_hudWindow, 0, reinterpret_cast<LONG_PTR>(this));

    draw();
#endif
}

static String timeToString(float time)
{
    if (!std::isfinite(time))
        time = 0;
    int seconds = fabsf(time); 
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;
    if (hours)
        return makeString((time < 0 ? "-" : ""), hours, ':', pad('0', 2, minutes), ':', pad('0', 2, seconds));
    return makeString((time < 0 ? "-" : ""), pad('0', 2, minutes), ':', pad('0', 2, seconds));
}

void FullscreenVideoController::draw()
{
    auto bitmapDC = adoptGDIObject(::CreateCompatibleDC(HWndDC(m_hudWindow)));
    HGDIOBJ oldBitmap = SelectObject(bitmapDC.get(), m_bitmap.get());

    GraphicsContextWin context(bitmapDC.get(), true);

    context.save();

    // Draw the background
    IntSize outerRadius(borderRadius, borderRadius);
    IntRect outerRect(0, 0, windowWidth, windowHeight);
    IntSize innerRadius(borderRadius - borderThickness, borderRadius - borderThickness);
    IntRect innerRect(borderThickness, borderThickness, windowWidth - borderThickness * 2, windowHeight - borderThickness * 2);

    context.fillRoundedRect(FloatRoundedRect(outerRect, outerRadius, outerRadius, outerRadius, outerRadius), Color(borderColor));
    context.setCompositeOperation(CompositeOperator::Copy);
    context.fillRoundedRect(FloatRoundedRect(innerRect, innerRadius, innerRadius, innerRadius, innerRadius), Color(backgroundColor));

    // Draw the widgets
    m_playPauseButton.draw(context);
    m_volumeUpButton.draw(context);
    m_volumeSliderButton.draw(context);
    m_volumeDownButton.draw(context);
    m_timeSliderButton.draw(context);
    m_exitFullscreenButton.draw(context);
    m_volumeSlider.draw(context);
    m_timeSlider.draw(context);

    // Draw the text strings
    FontCascadeDescription desc;

    NONCLIENTMETRICS metrics;
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0);
    desc.setOneFamily(metrics.lfSmCaptionFont.lfFaceName);

    desc.setComputedSize(textSize);
    FontCascade font = FontCascade(WTFMove(desc), 0, 0);
    font.update();

    String s;

    // The y positioning of these two text strings is tricky because they are so small. They
    // are currently positioned relative to the center of the slider and then down the font
    // height / 4 (which is actually half of font height /2), which positions the center of
    // the text at the center of the slider.
    // Left string
    s = timeToString(currentTime());
    int fontHeight = font.metricsOfPrimaryFont().height();
    TextRun leftText(s);
    context.setFillColor(Color(textColor));
    context.drawText(font, leftText, IntPoint(windowWidth / 2 - timeSliderWidth / 2 - margin - font.width(leftText), windowHeight - margin - sliderHeight / 2 + fontHeight / 4));

    // Right string
    s = timeToString(currentTime() - duration());
    TextRun rightText(s);
    context.setFillColor(Color(textColor));
    context.drawText(font, rightText, IntPoint(windowWidth / 2 + timeSliderWidth / 2 + margin, windowHeight - margin - sliderHeight / 2 + fontHeight / 4));

    // Copy to the window
    BLENDFUNCTION blendFunction = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    SIZE size = { windowWidth, windowHeight };
    POINT sourcePoint = {0, 0};
    POINT destPoint = { m_hudPosition.x(), m_hudPosition.y() };
    BOOL result = UpdateLayeredWindow(m_hudWindow, 0, &destPoint, &size, bitmapDC.get(), &sourcePoint, 0, &blendFunction, ULW_ALPHA);

    context.restore();

    ::SelectObject(bitmapDC.get(), oldBitmap);
}

LRESULT FullscreenVideoController::hudWndProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR longPtr = GetWindowLongPtr(wnd, 0);
    FullscreenVideoController* controller = reinterpret_cast<FullscreenVideoController*>(longPtr);
    if (!controller)
        return DefWindowProc(wnd, message, wParam, lParam);

    switch (message) {
    case WM_CHAR:
        controller->onChar(wParam);
        break;
    case WM_KEYDOWN:
        controller->onKeyDown(wParam);
        break;
    case WM_LBUTTONDOWN:
        controller->onMouseDown(IntPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        break;
    case WM_MOUSEMOVE:
        controller->onMouseMove(IntPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        break;
    case WM_LBUTTONUP:
        controller->onMouseUp(IntPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        break;
    }

    return DefWindowProc(wnd, message, wParam, lParam);
}

void FullscreenVideoController::onChar(int c)
{
    if (c == VK_ESCAPE) {
        if (m_videoElement)
            m_videoElement->exitFullscreen();
    } else if (c == VK_SPACE)
        togglePlay();
}

void FullscreenVideoController::onKeyDown(int virtualKey)
{
    if (virtualKey == VK_ESCAPE) {
        if (m_videoElement)
            m_videoElement->exitFullscreen();
    }
}

void FullscreenVideoController::timerFired()
{
    // Update the time slider
    m_timeSlider.setValue(currentTime() / duration());
    draw();
}

void FullscreenVideoController::onMouseDown(const IntPoint& point)
{
    IntPoint convertedPoint(fullscreenToHUDCoordinates(point));

    // Don't bother hit testing if we're outside the bounds of the window
    if (convertedPoint.x() < 0 || convertedPoint.x() >= windowWidth || convertedPoint.y() < 0 || convertedPoint.y() >= windowHeight)
        return;

    m_hitWidget = 0;
    m_movingWindow = false;

    if (m_playPauseButton.hitTest(convertedPoint))
        m_hitWidget = &m_playPauseButton;
    else if (m_exitFullscreenButton.hitTest(convertedPoint))
        m_hitWidget = &m_exitFullscreenButton;
    else if (m_volumeUpButton.hitTest(convertedPoint))
        m_hitWidget = &m_volumeUpButton;
    else if (m_volumeDownButton.hitTest(convertedPoint))
        m_hitWidget = &m_volumeDownButton;
    else if (m_volumeSlider.hitTest(convertedPoint)) {
        m_hitWidget = &m_volumeSlider;
        m_volumeSlider.drag(convertedPoint, true);
        setVolume(m_volumeSlider.value());
    } else if (m_timeSlider.hitTest(convertedPoint)) {
        m_hitWidget = &m_timeSlider;
        m_timeSlider.drag(convertedPoint, true);
        beginScrubbing();
        setCurrentTime(m_timeSlider.value() * duration());
    }

    // If we did not pick any of our widgets we are starting a window move
    if (!m_hitWidget) {
        m_moveOffset = convertedPoint;
        m_movingWindow = true;
    }

    draw();
}

void FullscreenVideoController::onMouseMove(const IntPoint& point)
{
    IntPoint convertedPoint(fullscreenToHUDCoordinates(point));

    if (m_hitWidget) {
        m_hitWidget->drag(convertedPoint, false);
        if (m_hitWidget == &m_volumeSlider)
            setVolume(m_volumeSlider.value());
        else if (m_hitWidget == &m_timeSlider)
            setCurrentTime(m_timeSlider.value() * duration());
        draw();
    } else if (m_movingWindow)
        m_hudPosition.move(convertedPoint.x() - m_moveOffset.x(), convertedPoint.y() - m_moveOffset.y());
}

void FullscreenVideoController::onMouseUp(const IntPoint& point)
{
    IntPoint convertedPoint(fullscreenToHUDCoordinates(point));
    m_movingWindow = false;

    if (m_hitWidget) {
        if (m_hitWidget == &m_playPauseButton && m_playPauseButton.hitTest(convertedPoint))
            togglePlay();
        else if (m_hitWidget == &m_volumeUpButton && m_volumeUpButton.hitTest(convertedPoint)) {
            setVolume(1);
            m_volumeSlider.setValue(1);
        } else if (m_hitWidget == &m_volumeDownButton && m_volumeDownButton.hitTest(convertedPoint)) {
            setVolume(0);
            m_volumeSlider.setValue(0);
        } else if (m_hitWidget == &m_timeSlider)
            endScrubbing();
        else if (m_hitWidget == &m_exitFullscreenButton && m_exitFullscreenButton.hitTest(convertedPoint)) {
            m_hitWidget = 0;
            if (m_videoElement)
                m_videoElement->exitFullscreen();
            return;
        }
    }

    m_hitWidget = 0;
    draw();
}

void FullscreenVideoController::togglePlay()
{
    if (canPlay())
        play();
    else
        pause();

    m_playPauseButton.setShowAltButton(!canPlay());

    // Run a timer while the video is playing so we can keep the time
    // slider and time values up to date.
    if (!canPlay())
        m_timer.startRepeating(timerInterval);
    else
        m_timer.stop();

    draw();
}

#endif
