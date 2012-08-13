/*
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(VIDEO)
#include "MediaPlayerPrivateBlackBerry.h"

#include "CookieManager.h"
#include "Credential.h"
#include "CredentialStorage.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HostWindow.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "ProtectionSpace.h"
#include "RenderBox.h"
#include "TimeRanges.h"
#include "WebPageClient.h"

#include <BlackBerryPlatformClient.h>
#include <set>
#include <string>
#include <wtf/text/CString.h>

#if USE(ACCELERATED_COMPOSITING)
#include "NativeImageSkia.h"
#include "VideoLayerWebKitThread.h"
#include <GLES2/gl2.h>
#endif

using namespace std;
using namespace BlackBerry::Platform;

namespace WebCore {

// Static callback functions for factory
PassOwnPtr<MediaPlayerPrivateInterface> MediaPlayerPrivate::create(MediaPlayer* player)
{
    return adoptPtr(new MediaPlayerPrivate(player));
}

void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(create, getSupportedTypes, supportsType, 0, 0, 0);
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    set<string> supported = MMRPlayer::allSupportedMimeTypes();
    set<string>::iterator i = supported.begin();
    for (; i != supported.end(); i++)
        types.add(i->c_str());
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs, const KURL&)
{
    if (type.isNull() || type.isEmpty()) {
        LOG(Media, "MediaPlayer does not support type; type is null or empty.");
        return MediaPlayer::IsNotSupported;
    }

    // spec says we should not return "probably" if the codecs string is empty
    if (MMRPlayer::mimeTypeSupported(type.ascii().data())) {
        LOG(Media, "MediaPlayer supports type; cache contains type '%s'.", type.ascii().data());
        return codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported;
    }
    LOG(Media, "MediaPlayer does not support type; cache doesn't contain type '%s'.", type.ascii().data());
    return MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivate::notifyAppActivatedEvent(bool activated)
{
    MMRPlayer::notifyAppActivatedEvent(activated);
}

void MediaPlayerPrivate::setCertificatePath(const String& caPath)
{
    MMRPlayer::setCertificatePath(string(caPath.utf8().data()));
}

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_webCorePlayer(player)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_fullscreenWebPageClient(0)
#if USE(ACCELERATED_COMPOSITING)
    , m_bufferingTimer(this, &MediaPlayerPrivate::bufferingTimerFired)
    , m_showBufferingImage(false)
    , m_mediaIsBuffering(false)
#endif
    , m_userDrivenSeekTimer(this, &MediaPlayerPrivate::userDrivenSeekTimerFired)
    , m_lastSeekTime(0)
    , m_lastSeekTimePending(false)
    , m_waitMetadataTimer(this, &MediaPlayerPrivate::waitMetadataTimerFired)
    , m_waitMetadataPopDialogCounter(0)
{
    void* tabId = 0;
    if (frameView() && frameView()->hostWindow())
        tabId = frameView()->hostWindow()->platformPageClient();
#if USE(ACCELERATED_COMPOSITING)
    m_platformPlayer = new MMRPlayer(this, tabId, true);
#else
    m_platformPlayer = new MMRPlayer(this, tabId, false);
#endif
}

MediaPlayerPrivate::~MediaPlayerPrivate()
{
    if (isFullscreen()) {
        HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_webCorePlayer->mediaPlayerClient());
        element->exitFullscreen();
    }
#if USE(ACCELERATED_COMPOSITING)
    // Remove media player from platform layer.
    if (m_platformLayer)
        static_cast<VideoLayerWebKitThread*>(m_platformLayer.get())->setMediaPlayer(0);
#endif

    deleteGuardedObject(m_platformPlayer);
}

void MediaPlayerPrivate::load(const String& url)
{
    String modifiedUrl(url);

    if (modifiedUrl.startsWith("local://")) {
        KURL kurl = KURL(KURL(), modifiedUrl);
        kurl.setProtocol("file");
        String tempPath(BlackBerry::Platform::Client::get()->getApplicationLocalDirectory().c_str());
        tempPath.append(kurl.path());
        kurl.setPath(tempPath);
        modifiedUrl = kurl.string();
    }
    if (modifiedUrl.startsWith("file://")) {
        // The QNX Multimedia Framework cannot handle filenames containing URL escape sequences.
        modifiedUrl = decodeURLEscapeSequences(modifiedUrl);
    }

    String cookiePairs;
    if (!url.isEmpty())
        cookiePairs = cookieManager().getCookie(KURL(ParsedURLString, url.utf8().data()), WithHttpOnlyCookies);
    if (!cookiePairs.isEmpty() && cookiePairs.utf8().data())
        m_platformPlayer->load(modifiedUrl.utf8().data(), m_webCorePlayer->userAgent().utf8().data(), cookiePairs.utf8().data());
    else
        m_platformPlayer->load(modifiedUrl.utf8().data(), m_webCorePlayer->userAgent().utf8().data(), 0);
}

void MediaPlayerPrivate::cancelLoad()
{
    m_platformPlayer->cancelLoad();
}

void MediaPlayerPrivate::prepareToPlay()
{
    m_platformPlayer->prepareToPlay();
}

void MediaPlayerPrivate::play()
{
    m_platformPlayer->play();
}

void MediaPlayerPrivate::pause()
{
    m_platformPlayer->pause();
}

bool MediaPlayerPrivate::supportsFullscreen() const
{
    return true;
}

IntSize MediaPlayerPrivate::naturalSize() const
{
    // Cannot return empty size, otherwise paint() will never get called.
    // Also, the values here will affect the aspect ratio of the output rectangle that will
    // be used for renderering the video, so we must take PAR into account.
    // Now, hope that metadata has been provided before this gets called if this is a video.
    double pixelWidth = static_cast<double>(m_platformPlayer->pixelWidth());
    double pixelHeight = static_cast<double>(m_platformPlayer->pixelHeight());
    if (!m_platformPlayer->pixelWidth() || !m_platformPlayer->pixelHeight())
        pixelWidth = pixelHeight = 1.0;

    // Use floating point arithmetic to eliminate the chance of integer
    // overflow. PAR numbers can be 5 digits or more.
    double adjustedSourceWidth = static_cast<double>(m_platformPlayer->sourceWidth()) * pixelWidth / pixelHeight;
    return IntSize(static_cast<int>(adjustedSourceWidth + 0.5), m_platformPlayer->sourceHeight());
}

bool MediaPlayerPrivate::hasVideo() const
{
    return m_platformPlayer->hasVideo();
}

bool MediaPlayerPrivate::hasAudio() const
{
    return m_platformPlayer->hasAudio();
}

void MediaPlayerPrivate::setVisible(bool)
{
    notImplemented();
}

float MediaPlayerPrivate::duration() const
{
    return m_platformPlayer->duration();
}

static const double SeekSubmissionDelay = 0.1; // Reasonable throttling value.
static const double ShortMediaThreshold = SeekSubmissionDelay * 2.0;

float MediaPlayerPrivate::currentTime() const
{
    // For very short media on the order of SeekSubmissionDelay we get
    // unwanted repeats if we don't return the most up-to-date currentTime().
    bool shortMedia = m_platformPlayer->duration() < ShortMediaThreshold;
    return m_userDrivenSeekTimer.isActive() && !shortMedia ? m_lastSeekTime: m_platformPlayer->currentTime();
}

void MediaPlayerPrivate::seek(float time)
{
    m_lastSeekTime = time;
    m_lastSeekTimePending = true;
    if (!m_userDrivenSeekTimer.isActive())
        userDrivenSeekTimerFired(0);
}

void MediaPlayerPrivate::userDrivenSeekTimerFired(Timer<MediaPlayerPrivate>*)
{
    if (m_lastSeekTimePending) {
        m_platformPlayer->seek(m_lastSeekTime);
        m_lastSeekTimePending = false;
        m_userDrivenSeekTimer.startOneShot(SeekSubmissionDelay);
    }
}

bool MediaPlayerPrivate::seeking() const
{
    return false;
}

void MediaPlayerPrivate::setRate(float rate)
{
    m_platformPlayer->setRate(rate);
}

bool MediaPlayerPrivate::paused() const
{
    return m_platformPlayer->paused();
}

void MediaPlayerPrivate::setVolume(float volume)
{
    m_platformPlayer->setVolume(volume);
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    return m_readyState;
}

float MediaPlayerPrivate::maxTimeSeekable() const
{
    return m_platformPlayer->maxTimeSeekable();
}

PassRefPtr<TimeRanges> MediaPlayerPrivate::buffered() const
{
    RefPtr<TimeRanges> timeRanges = TimeRanges::create();
    if (float bufferLoaded = m_platformPlayer->bufferLoaded())
        timeRanges->add(0, bufferLoaded);
    return timeRanges.release();
}

bool MediaPlayerPrivate::didLoadingProgress() const
{
    notImplemented();
    return false;
}

void MediaPlayerPrivate::setSize(const IntSize&)
{
    notImplemented();
}

void MediaPlayerPrivate::paint(GraphicsContext* context, const IntRect& rect)
{
#if USE(ACCELERATED_COMPOSITING)
    // Only process paint calls coming via the accelerated compositing code
    // path, where we get called with a null graphics context. See
    // LayerCompositingThread::drawTextures(). Ignore calls from the regular
    // rendering path.
    if (!context)
        m_platformPlayer->notifyOutputUpdate(BlackBerry::Platform::IntRect(rect.x(), rect.y(), rect.width(), rect.height()));
    return;
#endif

    if (!hasVideo() || context->paintingDisabled() || !m_webCorePlayer->visible())
        return;

    PlatformGraphicsContext* graphics = context->platformContext();
    ASSERT(graphics);

    BlackBerry::Platform::IntRect platformRect(rect.x(), rect.y(), rect.width(), rect.height());
    IntRect clippedRect = frameView()->windowClipRect();
    BlackBerry::Platform::IntRect platformWindowClipRect(clippedRect.x(), clippedRect.y(), clippedRect.width(), clippedRect.height());
    m_platformPlayer->paint(graphics->canvas(), platformRect, platformWindowClipRect);
}

bool MediaPlayerPrivate::hasAvailableVideoFrame() const
{
    return m_platformPlayer->hasAvailableVideoFrame();
}

bool MediaPlayerPrivate::hasSingleSecurityOrigin() const
{
    return false;
}

MediaPlayer::MovieLoadType MediaPlayerPrivate::movieLoadType() const
{
    return static_cast<MediaPlayer::MovieLoadType>(m_platformPlayer->movieLoadType());
}

void MediaPlayerPrivate::resizeSourceDimensions()
{
    if (!m_webCorePlayer)
        return;

    HTMLMediaElement* client = static_cast<HTMLMediaElement*>(m_webCorePlayer->mediaPlayerClient());

    if (!client || !client->isVideo())
        return;

    RenderObject* o = client->renderer();
    if (!o)
        return;

    // If we have an HTMLVideoElement but the source has no video, then we need to resize the media element.
    if (!hasVideo()) {
        IntRect rect = o->enclosingBox()->contentBoxRect();

        static const int playbookMinAudioElementWidth = 300;
        static const int playbookMinAudioElementHeight = 32;
        // If the rect dimensions are less than the allowed minimum, use the minimum instead.
        int newWidth = max(rect.width(), playbookMinAudioElementWidth);
        int newHeight = max(rect.height(), playbookMinAudioElementHeight);

        char attrString[12];

        sprintf(attrString, "%d", newWidth);
        client->setAttribute(HTMLNames::widthAttr, attrString);

        sprintf(attrString, "%d", newHeight);
        client->setAttribute(HTMLNames::heightAttr, attrString);
    }

    // If we don't know what the width and height of the video source is, then we need to set it to something sane.
    if (m_platformPlayer->sourceWidth() && m_platformPlayer->sourceHeight())
        return;
    IntRect rect = o->enclosingBox()->contentBoxRect();
    m_platformPlayer->setSourceDimension(rect.width(), rect.height());
}

void MediaPlayerPrivate::setFullscreenWebPageClient(BlackBerry::WebKit::WebPageClient* client)
{
    if (m_fullscreenWebPageClient == client)
        return;

    m_fullscreenWebPageClient = client;
    m_platformPlayer->toggleFullscreen(client);

    // The following repaint is needed especially if video is paused and
    // fullscreen is exiting, so that a MediaPlayerPrivate::paint() is
    // triggered and the code in outputUpdate() sets the correct window
    // rectangle.
    if (!client)
        m_webCorePlayer->repaint();
}

BlackBerry::Platform::Graphics::Window* MediaPlayerPrivate::getWindow()
{
    return m_platformPlayer->getWindow();
}

BlackBerry::Platform::Graphics::Window* MediaPlayerPrivate::getPeerWindow(const char* uniqueID) const
{
    return m_platformPlayer->getPeerWindow(uniqueID);
}

int MediaPlayerPrivate::getWindowPosition(unsigned& x, unsigned& y, unsigned& width, unsigned& height) const
{
    return m_platformPlayer->getWindowPosition(x, y, width, height);
}

const char* MediaPlayerPrivate::mmrContextName()
{
    return m_platformPlayer->mmrContextName();
}

float MediaPlayerPrivate::percentLoaded()
{
    if (!m_platformPlayer->duration())
        return 0;

    float buffered = 0;
    RefPtr<TimeRanges> timeRanges = this->buffered();
    for (unsigned i = 0; i < timeRanges->length(); ++i) {
        ExceptionCode ignoredException;
        float start = timeRanges->start(i, ignoredException);
        float end = timeRanges->end(i, ignoredException);
        buffered += end - start;
    }

    float loaded = buffered / m_platformPlayer->duration();
    return loaded;
}

unsigned MediaPlayerPrivate::sourceWidth()
{
    return m_platformPlayer->sourceWidth();
}

unsigned MediaPlayerPrivate::sourceHeight()
{
    return m_platformPlayer->sourceHeight();
}

void MediaPlayerPrivate::setAllowPPSVolumeUpdates(bool allow)
{
    return m_platformPlayer->setAllowPPSVolumeUpdates(allow);
}

void MediaPlayerPrivate::updateStates()
{
    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;

    MMRPlayer::Error currentError = m_platformPlayer->error();

    HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_webCorePlayer->mediaPlayerClient());

    if (currentError != MMRPlayer::MediaOK) {
        m_readyState = MediaPlayer::HaveNothing;
        if (currentError == MMRPlayer::MediaDecodeError)
            m_networkState = MediaPlayer::DecodeError;
        else if (currentError == MMRPlayer::MediaMetaDataError
            || currentError == MMRPlayer::MediaAudioReceiveError
            || currentError == MMRPlayer::MediaVideoReceiveError)
            m_networkState = MediaPlayer::NetworkError;
    } else {
        switch (m_platformPlayer->mediaState()) {
        case MMRPlayer::MMRPlayStateIdle:
            m_networkState = MediaPlayer::Idle;
            break;
        case MMRPlayer::MMRPlayStatePlaying:
            m_networkState = MediaPlayer::Loading;
            break;
        case MMRPlayer::MMRPlayStateStopped:
            m_networkState = MediaPlayer::Idle;
            break;
        case MMRPlayer::MMRPlayStateUnknown:
        default:
            break;
        }

        switch (m_platformPlayer->state()) {
        case MMRPlayer::MP_STATE_IDLE:
#if USE(ACCELERATED_COMPOSITING)
            setBuffering(false);
            m_mediaIsBuffering = false;
            // Remove media player from platform layer (remove hole punch rect).
            if (m_platformLayer) {
                static_cast<VideoLayerWebKitThread*>(m_platformLayer.get())->setMediaPlayer(0);
                m_platformLayer.clear();
            }
#endif
            if (isFullscreen())
                element->exitFullscreen();
            break;
        case MMRPlayer::MP_STATE_ACTIVE:
#if USE(ACCELERATED_COMPOSITING)
            m_showBufferingImage = false;
            m_mediaIsBuffering = false;
            // Create platform layer for video (create hole punch rect).
            if (!m_platformLayer)
                m_platformLayer = VideoLayerWebKitThread::create(m_webCorePlayer);
#endif
            break;
        case MMRPlayer::MP_STATE_UNSUPPORTED:
            break;
        default:
            break;
        }
        if ((duration() || movieLoadType() == MediaPlayer::LiveStream)
            && m_readyState != MediaPlayer::HaveEnoughData)
            m_readyState = MediaPlayer::HaveEnoughData;
    }

    if (m_readyState != oldReadyState)
        m_webCorePlayer->readyStateChanged();
    if (m_networkState != oldNetworkState)
        m_webCorePlayer->networkStateChanged();
}

// IMMRPlayerListener callbacks implementation
void MediaPlayerPrivate::onStateChanged(MMRPlayer::MpState)
{
    updateStates();
}

void MediaPlayerPrivate::onMediaStatusChanged(MMRPlayer::MMRPlayState)
{
    updateStates();
}

void MediaPlayerPrivate::onError(MMRPlayer::Error type)
{
    updateStates();
}

void MediaPlayerPrivate::onDurationChanged(float duration)
{
    updateStates();
    m_webCorePlayer->durationChanged();
}

void MediaPlayerPrivate::onTimeChanged(float)
{
    m_webCorePlayer->timeChanged();
}

void MediaPlayerPrivate::onPauseStateChanged()
{
    if (!isFullscreen())
        return;

    HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_webCorePlayer->mediaPlayerClient());
    // Paused state change not due to local controller.
    if (m_platformPlayer->isPaused())
        element->pause();
    else {
        // The HMI fullscreen widget has resumed play. Check if the
        // pause timeout occurred.
        m_platformPlayer->processPauseTimeoutIfNecessary();
        element->play();
    }
}

void MediaPlayerPrivate::onRateChanged(float)
{
    m_webCorePlayer->rateChanged();
}

void MediaPlayerPrivate::onVolumeChanged(float volume)
{
    m_webCorePlayer->volumeChanged(volume);
}

void MediaPlayerPrivate::onRepaint()
{
    m_webCorePlayer->repaint();
}

void MediaPlayerPrivate::onSizeChanged()
{
    resizeSourceDimensions();
    if (hasVideo())
        m_webCorePlayer->sizeChanged();
}

void MediaPlayerPrivate::onPlayNotified()
{
    if (HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_webCorePlayer->mediaPlayerClient()))
        element->play();
}

void MediaPlayerPrivate::onPauseNotified()
{
    if (HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_webCorePlayer->mediaPlayerClient()))
        element->pause();
}

static const int popupDialogInterval = 10;
static const double checkMetadataReadyInterval = 0.5;
void MediaPlayerPrivate::onWaitMetadataNotified(bool hasFinished, int timeWaited)
{
    if (!hasFinished) {
        if (!m_waitMetadataTimer.isActive()) {
            // Make sure to popup dialog every 10 seconds after metadata start to load.
            // This should be set only once at the first time when user press the play button.
            m_waitMetadataPopDialogCounter = static_cast<int>(timeWaited / checkMetadataReadyInterval);
            m_waitMetadataTimer.startOneShot(checkMetadataReadyInterval);
        }
    } else if (m_waitMetadataTimer.isActive()) {
        m_waitMetadataTimer.stop();
        m_waitMetadataPopDialogCounter = 0;
    }
}

void MediaPlayerPrivate::waitMetadataTimerFired(Timer<MediaPlayerPrivate>*)
{
    if (m_platformPlayer->isMetadataReady()) {
        m_platformPlayer->playWithMetadataReady();
        m_waitMetadataPopDialogCounter = 0;
        return;
    }

    static const int hitTimeToPopupDialog = static_cast<int>(popupDialogInterval / checkMetadataReadyInterval);
    m_waitMetadataPopDialogCounter++;
    if (m_waitMetadataPopDialogCounter < hitTimeToPopupDialog) {
        m_waitMetadataTimer.startOneShot(checkMetadataReadyInterval);
        return;
    }
    m_waitMetadataPopDialogCounter = 0;

    int wait = showErrorDialog(MMRPlayer::MediaMetaDataTimeoutError);
    if (!wait)
        onPauseNotified();
    else {
        if (m_platformPlayer->isMetadataReady())
            m_platformPlayer->playWithMetadataReady();
        else
            m_waitMetadataTimer.startOneShot(checkMetadataReadyInterval);
    }
}

#if USE(ACCELERATED_COMPOSITING)
void MediaPlayerPrivate::onBuffering(bool flag)
{
    setBuffering(flag);
}
#endif

static ProtectionSpace generateProtectionSpaceFromMMRAuthChallenge(const MMRAuthChallenge& authChallenge)
{
    KURL url(ParsedURLString, String(authChallenge.url().c_str()));
    ASSERT(url.isValid());

    return ProtectionSpace(url.host(), url.port(),
                           static_cast<ProtectionSpaceServerType>(authChallenge.serverType()),
                           authChallenge.realm().c_str(),
                           static_cast<ProtectionSpaceAuthenticationScheme>(authChallenge.authScheme()));
}

bool MediaPlayerPrivate::onAuthenticationNeeded(MMRAuthChallenge& authChallenge)
{
    KURL url(ParsedURLString, String(authChallenge.url().c_str()));
    if (!url.isValid())
        return false;

    ProtectionSpace protectionSpace = generateProtectionSpaceFromMMRAuthChallenge(authChallenge);
    Credential credential = CredentialStorage::get(protectionSpace);
    bool isConfirmed = false;
    if (credential.isEmpty()) {
        if (frameView() && frameView()->hostWindow())
            isConfirmed = frameView()->hostWindow()->platformPageClient()->authenticationChallenge(url, protectionSpace, credential);
    } else
        isConfirmed = true;

    if (isConfirmed)
        authChallenge.setCredential(credential.user().utf8().data(), credential.password().utf8().data(), static_cast<MMRAuthChallenge::CredentialPersistence>(credential.persistence()));

    return isConfirmed;
}

void MediaPlayerPrivate::onAuthenticationAccepted(const MMRAuthChallenge& authChallenge) const
{
    KURL url(ParsedURLString, String(authChallenge.url().c_str()));
    if (!url.isValid())
        return;

    ProtectionSpace protectionSpace = generateProtectionSpaceFromMMRAuthChallenge(authChallenge);
    Credential savedCredential = CredentialStorage::get(protectionSpace);
    if (savedCredential.isEmpty())
        CredentialStorage::set(Credential(authChallenge.username().c_str(), authChallenge.password().c_str(), static_cast<CredentialPersistence>(authChallenge.persistence())), protectionSpace, url);
}

int MediaPlayerPrivate::showErrorDialog(MMRPlayer::Error type)
{
    using namespace BlackBerry::WebKit;

    WebPageClient::AlertType atype;
    switch (type) {
    case MMRPlayer::MediaOK:
        atype = WebPageClient::MediaOK;
        break;
    case MMRPlayer::MediaDecodeError:
        atype = WebPageClient::MediaDecodeError;
        break;
    case MMRPlayer::MediaMetaDataError:
        atype = WebPageClient::MediaMetaDataError;
        break;
    case MMRPlayer::MediaMetaDataTimeoutError:
        atype = WebPageClient::MediaMetaDataTimeoutError;
        break;
    case MMRPlayer::MediaNoMetaDataError:
        atype = WebPageClient::MediaNoMetaDataError;
        break;
    case MMRPlayer::MediaVideoReceiveError:
        atype = WebPageClient::MediaVideoReceiveError;
        break;
    case MMRPlayer::MediaAudioReceiveError:
        atype = WebPageClient::MediaAudioReceiveError;
        break;
    case MMRPlayer::MediaInvalidError:
        atype = WebPageClient::MediaInvalidError;
        break;
    default:
        LOG(Media, "Alert type does not exist.");
        return -1;
    }

    int rc = 0;
    if (frameView() && frameView()->hostWindow())
        rc = frameView()->hostWindow()->platformPageClient()->showAlertDialog(atype);
    return rc;
}

FrameView* MediaPlayerPrivate::frameView() const
{
    // We previously used m_webCorePlayer->frameView(), but this method returns
    // a null frameView until quite late in the media player initialization,
    // and starting quite early in the media player destruction (because
    // it may be set to zero by the destructor in RenderVideo.cpp before
    // our destructor is called, leaving us unable to clean up child windows
    // in mmrDisconnect).
    HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_webCorePlayer->mediaPlayerClient());
    return element->document()->view();
}

BlackBerry::Platform::Graphics::Window* MediaPlayerPrivate::platformWindow()
{
    if (frameView() && frameView()->hostWindow())
        return frameView()->hostWindow()->platformPageClient()->platformWindow();
    return 0;
}

bool MediaPlayerPrivate::isFullscreen() const
{
    return m_fullscreenWebPageClient;
}

bool MediaPlayerPrivate::isElementPaused() const
{
    HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_webCorePlayer->mediaPlayerClient());
    if (!element || element->paused())
        return true;
    return false;
}

bool MediaPlayerPrivate::isTabVisible() const
{
    if (frameView() && frameView()->hostWindow())
        return frameView()->hostWindow()->platformPageClient()->isVisible();
    return true;
}

#if USE(ACCELERATED_COMPOSITING)
static const double BufferingAnimationDelay = 1.0 / 24;
static char* s_bufferingImageData = 0;
static int s_bufferingImageWidth = 0;
static int s_bufferingImageHeight = 0;

PlatformMedia MediaPlayerPrivate::platformMedia() const
{
    PlatformMedia pm;
    pm.type = PlatformMedia::QNXMediaPlayerType;
    pm.media.qnxMediaPlayer = const_cast<MediaPlayerPrivate*>(this);
    return pm;
}

PlatformLayer* MediaPlayerPrivate::platformLayer() const
{
    if (m_platformLayer)
        return m_platformLayer.get();
    return 0;
}

static void loadBufferingImageData()
{
    static bool loaded = false;
    if (!loaded) {
        static Image* bufferingIcon = Image::loadPlatformResource("vidbuffer").leakRef();
        NativeImageSkia* nativeImage = bufferingIcon->nativeImageForCurrentFrame();
        if (!nativeImage)
            return;

        if (!nativeImage->isDataComplete())
            return;

        loaded = true;
        nativeImage->bitmap().lockPixels();

        int bufSize = nativeImage->bitmap().width() * nativeImage->bitmap().height() * 4;
        s_bufferingImageWidth = nativeImage->bitmap().width();
        s_bufferingImageHeight = nativeImage->bitmap().height();
        s_bufferingImageData = static_cast<char*>(malloc(bufSize));
        memcpy(s_bufferingImageData, nativeImage->bitmap().getPixels(), bufSize);

        nativeImage->bitmap().unlockPixels();
        bufferingIcon->deref();
    }
}

void MediaPlayerPrivate::bufferingTimerFired(Timer<MediaPlayerPrivate>*)
{
    if (m_showBufferingImage) {
        if (!isFullscreen() && m_platformLayer)
            m_platformLayer->setNeedsDisplay();
        m_bufferingTimer.startOneShot(BufferingAnimationDelay);
    }
}

void MediaPlayerPrivate::setBuffering(bool buffering)
{
    if (!hasVideo())
        buffering = false; // Buffering animation not visible for audio.
    if (buffering != m_showBufferingImage) {
        m_showBufferingImage = buffering;
        if (buffering) {
            loadBufferingImageData();
            m_bufferingTimer.startOneShot(BufferingAnimationDelay);
        } else
            m_bufferingTimer.stop();

        if (m_platformLayer)
            m_platformLayer->setNeedsDisplay();
    }
}

static unsigned int allocateTextureId()
{
    unsigned int texid;
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    // Do basic linear filtering on resize.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // NPOT textures in GL ES only work when the wrap mode is set to GL_CLAMP_TO_EDGE.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texid;
}

void MediaPlayerPrivate::drawBufferingAnimation(const TransformationMatrix& matrix, int positionLocation, int texCoordLocation)
{
    if (m_showBufferingImage && s_bufferingImageData && !isFullscreen()) {
        TransformationMatrix renderMatrix = matrix;

        // Rotate the buffering indicator so that it takes 1 second to do 1 revolution.
        timespec time;
        clock_gettime(CLOCK_REALTIME, &time);
        renderMatrix.rotate(time.tv_nsec / 1000000000.0 * 360.0);

        static bool initialized = false;
        static unsigned int texId = allocateTextureId();
        glBindTexture(GL_TEXTURE_2D, texId);
        if (!initialized) {
            initialized = true;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_bufferingImageWidth, s_bufferingImageHeight,
                0, GL_RGBA, GL_UNSIGNED_BYTE, s_bufferingImageData);
            free(s_bufferingImageData);
        }

        float texcoords[] = { 0, 0,  0, 1,  1, 1,  1, 0 };
        FloatPoint vertices[4];
        float bx = s_bufferingImageWidth / 2.0;
        float by = s_bufferingImageHeight / 2.0;
        vertices[0] = renderMatrix.mapPoint(FloatPoint(-bx, -by));
        vertices[1] = renderMatrix.mapPoint(FloatPoint(-bx, by));
        vertices[2] = renderMatrix.mapPoint(FloatPoint(bx, by));
        vertices[3] = renderMatrix.mapPoint(FloatPoint(bx, -by));

        glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
}
#endif

} // namespace WebCore

#endif // ENABLE(VIDEO)
