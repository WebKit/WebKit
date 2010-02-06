/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#include "config.h"

#include "QTMovieWin.h"

// Put Movies.h first so build failures here point clearly to QuickTime
#include <Movies.h>

#include "QTMovieWinTimer.h"
#include <GXMath.h>
#include <QTML.h>
#include <QuickTimeComponents.h>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

using namespace std;

static const long minimumQuickTimeVersion = 0x07300000; // 7.3

static const long closedCaptionTrackType = 'clcp';
static const long subTitleTrackType = 'sbtl';
static const long mpeg4ObjectDescriptionTrackType = 'odsm';
static const long mpeg4SceneDescriptionTrackType = 'sdsm';
static const long closedCaptionDisplayPropertyID = 'disp';
static LPCTSTR fullscreenQTMovieWinPointerProp = TEXT("fullscreenQTMovieWinPointer");

// Resizing GWorlds is slow, give them a minimum size so size of small 
// videos can be animated smoothly
static const int cGWorldMinWidth = 640;
static const int cGWorldMinHeight = 360;

static const float cNonContinuousTimeChange = 0.2f;

union UppParam {
    long longValue;
    void* ptr;
};

static MovieDrawingCompleteUPP gMovieDrawingCompleteUPP = 0;
static HashSet<QTMovieWinPrivate*>* gTaskList;
static Vector<CFStringRef>* gSupportedTypes = 0;
static SInt32 quickTimeVersion = 0;

static QTMovieWin::SetTaskTimerDelayFunc gSetTaskTimerDelay = 0;
static QTMovieWin::StopTaskTimerFunc gStopTaskTimer = 0;

static void updateTaskTimer(int maxInterval = 1000)
{
    if (!gTaskList->size()) {
        gStopTaskTimer();
        return;    
    }
    
    long intervalInMS;
    QTGetTimeUntilNextTask(&intervalInMS, 1000);
    if (intervalInMS > maxInterval)
        intervalInMS = maxInterval;
    gSetTaskTimerDelay(static_cast<float>(intervalInMS) / 1000);
}

class QTMovieWinPrivate : public Noncopyable {
public:
    QTMovieWinPrivate();
    ~QTMovieWinPrivate();
    void task();
    void startTask();
    void endTask();

    void createMovieController();
    void registerDrawingCallback();
    void drawingComplete();
    void updateGWorld();
    void createGWorld();
    void deleteGWorld();
    void clearGWorld();
    void cacheMovieScale();
    void updateMovieSize();

    void setSize(int, int);

    QTMovieWin* m_movieWin;
    Movie m_movie;
    MovieController m_movieController;
    bool m_tasking;
    QTMovieWinClient* m_client;
    long m_loadState;
    bool m_ended;
    bool m_seeking;
    float m_lastMediaTime;
    double m_lastLoadStateCheckTime;
    int m_width;
    int m_height;
    bool m_visible;
    GWorldPtr m_gWorld;
    int m_gWorldWidth;
    int m_gWorldHeight;
    GWorldPtr m_savedGWorld;
    long m_loadError;
    float m_widthScaleFactor;
    float m_heightScaleFactor;
    CFURLRef m_currentURL;
    float m_timeToRestore;
    float m_rateToRestore;
#if !ASSERT_DISABLED
    bool m_scaleCached;
#endif
    WindowPtr m_fullscreenWindow;
    GWorldPtr m_fullscreenOrigGWorld;
    Rect m_fullscreenRect;
    QTMovieWinFullscreenClient* m_fullscreenClient;
    char* m_fullscreenRestoreState;
};

QTMovieWinPrivate::QTMovieWinPrivate()
    : m_movieWin(0)
    , m_movie(0)
    , m_movieController(0)
    , m_tasking(false)
    , m_client(0)
    , m_loadState(0)
    , m_ended(false)
    , m_seeking(false)
    , m_lastMediaTime(0)
    , m_lastLoadStateCheckTime(0)
    , m_width(0)
    , m_height(0)
    , m_visible(false)
    , m_gWorld(0)
    , m_gWorldWidth(0)
    , m_gWorldHeight(0)
    , m_savedGWorld(0)
    , m_loadError(0)
    , m_widthScaleFactor(1)
    , m_heightScaleFactor(1)
    , m_currentURL(0)
    , m_timeToRestore(-1.0f)
    , m_rateToRestore(-1.0f)
#if !ASSERT_DISABLED
    , m_scaleCached(false)
#endif
    , m_fullscreenWindow(0)
    , m_fullscreenOrigGWorld(0)
    , m_fullscreenClient(0)
    , m_fullscreenRestoreState(0)
{
    Rect rect = { 0, 0, 0, 0 };
    m_fullscreenRect = rect;
}

QTMovieWinPrivate::~QTMovieWinPrivate()
{
    ASSERT(!m_fullscreenWindow);

    endTask();
    if (m_gWorld)
        deleteGWorld();
    if (m_movieController)
        DisposeMovieController(m_movieController);
    if (m_movie)
        DisposeMovie(m_movie);
    if (m_currentURL)
        CFRelease(m_currentURL);
}

void QTMovieWin::taskTimerFired()
{
    // The hash content might change during task()
    Vector<QTMovieWinPrivate*> tasks;
    copyToVector(*gTaskList, tasks);
    size_t count = tasks.size();
    for (unsigned n = 0; n < count; ++n)
        tasks[n]->task();

    updateTaskTimer();
}

void QTMovieWinPrivate::startTask() 
{
    if (m_tasking)
        return;
    if (!gTaskList)
        gTaskList = new HashSet<QTMovieWinPrivate*>;
    gTaskList->add(this);
    m_tasking = true;
    updateTaskTimer();
}

void QTMovieWinPrivate::endTask() 
{
    if (!m_tasking)
        return;
    gTaskList->remove(this);
    m_tasking = false;
    updateTaskTimer();
}

void QTMovieWinPrivate::cacheMovieScale()
{
    Rect naturalRect;
    Rect initialRect;

    GetMovieNaturalBoundsRect(m_movie, &naturalRect);
    GetMovieBox(m_movie, &initialRect);

    float naturalWidth = naturalRect.right - naturalRect.left;
    float naturalHeight = naturalRect.bottom - naturalRect.top;

    if (naturalWidth)
        m_widthScaleFactor = (initialRect.right - initialRect.left) / naturalWidth;
    if (naturalHeight)
        m_heightScaleFactor = (initialRect.bottom - initialRect.top) / naturalHeight;
#if !ASSERT_DISABLED
    m_scaleCached = true;;
#endif
}

void QTMovieWinPrivate::task() 
{
    ASSERT(m_tasking);

    if (!m_loadError) {
        if (m_movieController)
            MCIdle(m_movieController);
        else
            MoviesTask(m_movie, 0);
    }

    // GetMovieLoadState documentation says that you should not call it more often than every quarter of a second.
    if (systemTime() >= m_lastLoadStateCheckTime + 0.25 || m_loadError) { 
        // If load fails QT's load state is QTMovieLoadStateComplete.
        // This is different from QTKit API and seems strange.
        long loadState = m_loadError ? QTMovieLoadStateError : GetMovieLoadState(m_movie);
        if (loadState != m_loadState) {

            // we only need to erase the movie gworld when the load state changes to loaded while it
            //  is visible as the gworld is destroyed/created when visibility changes
            bool shouldRestorePlaybackState = false;
            bool movieNewlyPlayable = loadState >= QTMovieLoadStateLoaded && m_loadState < QTMovieLoadStateLoaded;
            m_loadState = loadState;
            if (movieNewlyPlayable) {
                cacheMovieScale();
                updateMovieSize();
                if (m_visible)
                    clearGWorld();
                shouldRestorePlaybackState = true;
            }

            if (!m_movieController && m_loadState >= QTMovieLoadStateLoaded)
                createMovieController();
            m_client->movieLoadStateChanged(m_movieWin);
            
            if (shouldRestorePlaybackState && m_timeToRestore != -1.0f) {
                m_movieWin->setCurrentTime(m_timeToRestore);
                m_timeToRestore = -1.0f;
                m_movieWin->setRate(m_rateToRestore);
                m_rateToRestore = -1.0f;
            }

            if (m_movieWin->m_disabled) {
                endTask();
                return;
            }
        }
        m_lastLoadStateCheckTime = systemTime();
    }

    bool ended = !!IsMovieDone(m_movie);
    if (ended != m_ended) {
        m_ended = ended;
        if (m_client && ended)
            m_client->movieEnded(m_movieWin);
    }

    float time = m_movieWin->currentTime();
    if (time < m_lastMediaTime || time >= m_lastMediaTime + cNonContinuousTimeChange || m_seeking) {
        m_seeking = false;
        if (m_client)
            m_client->movieTimeChanged(m_movieWin);
    }
    m_lastMediaTime = time;

    if (m_loadError)
        endTask();
}

void QTMovieWinPrivate::createMovieController()
{
    Rect bounds;
    long flags;

    if (!m_movie)
        return;

    if (m_movieController)
        DisposeMovieController(m_movieController);

    GetMovieBox(m_movie, &bounds);
    flags = mcTopLeftMovie | mcNotVisible;
    m_movieController = NewMovieController(m_movie, &bounds, flags);
    if (!m_movieController)
        return;

    MCSetControllerPort(m_movieController, m_gWorld);
    MCSetControllerAttached(m_movieController, false);
}

void QTMovieWinPrivate::registerDrawingCallback()
{
    UppParam param;
    param.ptr = this;
    SetMovieDrawingCompleteProc(m_movie, movieDrawingCallWhenChanged, gMovieDrawingCompleteUPP, param.longValue);
}

void QTMovieWinPrivate::drawingComplete()
{
    if (!m_gWorld || m_movieWin->m_disabled || m_loadState < QTMovieLoadStateLoaded)
        return;
    m_client->movieNewImageAvailable(m_movieWin);
}

void QTMovieWinPrivate::updateGWorld()
{
    bool shouldBeVisible = m_visible;
    if (!m_height || !m_width)
        shouldBeVisible = false;

    if (shouldBeVisible && !m_gWorld)
        createGWorld();
    else if (!shouldBeVisible && m_gWorld)
        deleteGWorld();
    else if (m_gWorld && (m_width > m_gWorldWidth || m_height > m_gWorldHeight)) {
        // need a bigger, better gWorld
        deleteGWorld();
        createGWorld();
    }
}

void QTMovieWinPrivate::createGWorld()
{
    ASSERT(!m_gWorld);
    if (!m_movie || m_loadState < QTMovieLoadStateLoaded)
        return;

    m_gWorldWidth = max(cGWorldMinWidth, m_width);
    m_gWorldHeight = max(cGWorldMinHeight, m_height);
    Rect bounds; 
    bounds.top = 0;
    bounds.left = 0; 
    bounds.right = m_gWorldWidth;
    bounds.bottom = m_gWorldHeight;
    OSErr err = QTNewGWorld(&m_gWorld, k32BGRAPixelFormat, &bounds, 0, 0, 0); 
    if (err) 
        return;
    GetMovieGWorld(m_movie, &m_savedGWorld, 0);
    if (m_movieController)
        MCSetControllerPort(m_movieController, m_gWorld);
    SetMovieGWorld(m_movie, m_gWorld, 0);
    bounds.right = m_width;
    bounds.bottom = m_height;
    if (m_movieController)
        MCSetControllerBoundsRect(m_movieController, &bounds);
    SetMovieBox(m_movie, &bounds);
}

void QTMovieWinPrivate::clearGWorld()
{
    if (!m_movie||!m_gWorld)
        return;

    GrafPtr savePort;
    GetPort(&savePort); 
    MacSetPort((GrafPtr)m_gWorld);

    Rect bounds; 
    bounds.top = 0;
    bounds.left = 0; 
    bounds.right = m_gWorldWidth;
    bounds.bottom = m_gWorldHeight;
    EraseRect(&bounds);

    MacSetPort(savePort);
}

void QTMovieWinPrivate::setSize(int width, int height)
{
    if (m_width == width && m_height == height)
        return;
    m_width = width;
    m_height = height;

    // Do not change movie box before reaching load state loaded as we grab
    // the initial size when task() sees that state for the first time, and
    // we need the initial size to be able to scale movie properly. 
    if (!m_movie || m_loadState < QTMovieLoadStateLoaded)
        return;

#if !ASSERT_DISABLED
    ASSERT(m_scaleCached);
#endif

    updateMovieSize();
}

void QTMovieWinPrivate::updateMovieSize()
{
    if (!m_movie || m_loadState < QTMovieLoadStateLoaded)
        return;

    Rect bounds; 
    bounds.top = 0;
    bounds.left = 0; 
    bounds.right = m_width;
    bounds.bottom = m_height;
    if (m_movieController)
        MCSetControllerBoundsRect(m_movieController, &bounds);
    SetMovieBox(m_movie, &bounds);
    updateGWorld();
}


void QTMovieWinPrivate::deleteGWorld()
{
    ASSERT(m_gWorld);
    if (m_movieController)
        MCSetControllerPort(m_movieController, m_savedGWorld);
    if (m_movie)
        SetMovieGWorld(m_movie, m_savedGWorld, 0);
    m_savedGWorld = 0;
    DisposeGWorld(m_gWorld); 
    m_gWorld = 0;
    m_gWorldWidth = 0;
    m_gWorldHeight = 0;
}


QTMovieWin::QTMovieWin(QTMovieWinClient* client)
    : m_private(new QTMovieWinPrivate())
    , m_disabled(false)
{
    m_private->m_movieWin = this;
    m_private->m_client = client;
    initializeQuickTime();
}

QTMovieWin::~QTMovieWin()
{
    delete m_private;
}

void QTMovieWin::play()
{
    m_private->m_timeToRestore = -1.0f;

    if (m_private->m_movieController)
        MCDoAction(m_private->m_movieController, mcActionPrerollAndPlay, (void *)GetMoviePreferredRate(m_private->m_movie));
    else
        StartMovie(m_private->m_movie);
    m_private->startTask();
}

void QTMovieWin::pause()
{
    m_private->m_timeToRestore = -1.0f;

    if (m_private->m_movieController)
        MCDoAction(m_private->m_movieController, mcActionPlay, 0);
    else
        StopMovie(m_private->m_movie);
    updateTaskTimer();
}

float QTMovieWin::rate() const
{
    if (!m_private->m_movie)
        return 0;
    return FixedToFloat(GetMovieRate(m_private->m_movie));
}

void QTMovieWin::setRate(float rate)
{
    if (!m_private->m_movie)
        return;    
    m_private->m_timeToRestore = -1.0f;
    
    if (m_private->m_movieController)
        MCDoAction(m_private->m_movieController, mcActionPrerollAndPlay, (void *)FloatToFixed(rate));
    else
        SetMovieRate(m_private->m_movie, FloatToFixed(rate));
    updateTaskTimer();
}

float QTMovieWin::duration() const
{
    if (!m_private->m_movie)
        return 0;
    TimeValue val = GetMovieDuration(m_private->m_movie);
    TimeScale scale = GetMovieTimeScale(m_private->m_movie);
    return static_cast<float>(val) / scale;
}

float QTMovieWin::currentTime() const
{
    if (!m_private->m_movie)
        return 0;
    TimeValue val = GetMovieTime(m_private->m_movie, 0);
    TimeScale scale = GetMovieTimeScale(m_private->m_movie);
    return static_cast<float>(val) / scale;
}

void QTMovieWin::setCurrentTime(float time) const
{
    if (!m_private->m_movie)
        return;

    m_private->m_timeToRestore = -1.0f;
    
    m_private->m_seeking = true;
    TimeScale scale = GetMovieTimeScale(m_private->m_movie);
    if (m_private->m_movieController){
        QTRestartAtTimeRecord restart = { time * scale , 0 };
        MCDoAction(m_private->m_movieController, mcActionRestartAtTime, (void *)&restart);
    } else
        SetMovieTimeValue(m_private->m_movie, TimeValue(time * scale));
    updateTaskTimer();
}

void QTMovieWin::setVolume(float volume)
{
    if (!m_private->m_movie)
        return;
    SetMovieVolume(m_private->m_movie, static_cast<short>(volume * 256));
}

void QTMovieWin::setPreservesPitch(bool preservesPitch)
{
    if (!m_private->m_movie || !m_private->m_currentURL)
        return;

    OSErr error;
    bool prop = false;

    error = QTGetMovieProperty(m_private->m_movie, kQTPropertyClass_Audio, kQTAudioPropertyID_RateChangesPreservePitch,
                               sizeof(kQTAudioPropertyID_RateChangesPreservePitch), static_cast<QTPropertyValuePtr>(&prop), 0);

    if (error || prop == preservesPitch)
        return;

    m_private->m_timeToRestore = currentTime();
    m_private->m_rateToRestore = rate();
    load(m_private->m_currentURL, preservesPitch);
}

unsigned QTMovieWin::dataSize() const
{
    if (!m_private->m_movie)
        return 0;
    return GetMovieDataSize(m_private->m_movie, 0, GetMovieDuration(m_private->m_movie));
}

float QTMovieWin::maxTimeLoaded() const
{
    if (!m_private->m_movie)
        return 0;
    TimeValue val;
    GetMaxLoadedTimeInMovie(m_private->m_movie, &val);
    TimeScale scale = GetMovieTimeScale(m_private->m_movie);
    return static_cast<float>(val) / scale;
}

long QTMovieWin::loadState() const
{
    return m_private->m_loadState;
}

void QTMovieWin::getNaturalSize(int& width, int& height)
{
    Rect rect = { 0, };

    if (m_private->m_movie)
        GetMovieNaturalBoundsRect(m_private->m_movie, &rect);
    width = (rect.right - rect.left) * m_private->m_widthScaleFactor;
    height = (rect.bottom - rect.top) * m_private->m_heightScaleFactor;
}

void QTMovieWin::setSize(int width, int height)
{
    m_private->setSize(width, height);
    updateTaskTimer(0);
}

void QTMovieWin::setVisible(bool b)
{
    m_private->m_visible = b;
    m_private->updateGWorld();
}

void QTMovieWin::getCurrentFrameInfo(void*& buffer, unsigned& bitsPerPixel, unsigned& rowBytes, unsigned& width, unsigned& height)
{
    if (!m_private->m_gWorld) {
        buffer = 0;
        bitsPerPixel = 0;
        rowBytes = 0;
        width = 0;
        height = 0;
        return;
    }
    PixMapHandle offscreenPixMap = GetGWorldPixMap(m_private->m_gWorld);
    buffer = (*offscreenPixMap)->baseAddr;
    bitsPerPixel = (*offscreenPixMap)->pixelSize;
    rowBytes = (*offscreenPixMap)->rowBytes & 0x3FFF;
    width = m_private->m_width;
    height = m_private->m_height;
}

void QTMovieWin::paint(HDC hdc, int x, int y)
{
    if (!m_private->m_gWorld)
        return;

    HDC hdcSrc = static_cast<HDC>(GetPortHDC(reinterpret_cast<GrafPtr>(m_private->m_gWorld))); 
    if (!hdcSrc)
        return;

    // FIXME: If we could determine the movie has no alpha, we could use BitBlt for those cases, which might be faster.
    BLENDFUNCTION blendFunction; 
    blendFunction.BlendOp = AC_SRC_OVER;
    blendFunction.BlendFlags = 0;
    blendFunction.SourceConstantAlpha = 255;
    blendFunction.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(hdc, x, y, m_private->m_width, m_private->m_height, hdcSrc, 
         0, 0, m_private->m_width, m_private->m_height, blendFunction);
}

void QTMovieWin::load(const UChar* url, int len, bool preservesPitch)
{
    CFStringRef urlStringRef = CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(url), len);
    CFURLRef cfURL = CFURLCreateWithString(kCFAllocatorDefault, urlStringRef, 0);

    load(cfURL, preservesPitch);

    CFRelease(cfURL);
    CFRelease(urlStringRef);
}

void QTMovieWin::load(CFURLRef url, bool preservesPitch)
{
    if (!url)
        return;

    if (m_private->m_movie) {
        m_private->endTask();
        if (m_private->m_gWorld)
            m_private->deleteGWorld();
        if (m_private->m_movieController)
            DisposeMovieController(m_private->m_movieController);
        m_private->m_movieController = 0;
        DisposeMovie(m_private->m_movie);
        m_private->m_movie = 0;
        m_private->m_loadState = 0;
    }  

    // Define a property array for NewMovieFromProperties. 8 should be enough for our needs. 
    QTNewMoviePropertyElement movieProps[8]; 
    ItemCount moviePropCount = 0; 

    bool boolTrue = true;
    
    // Disable streaming support for now. 
    CFStringRef scheme = CFURLCopyScheme(url);
    bool isRTSP = CFStringHasPrefix(scheme, CFSTR("rtsp:"));
    CFRelease(scheme);

    if (isRTSP) {
        m_private->m_loadError = noMovieFound;
        goto end;
    }

    if (m_private->m_currentURL) {
        if (m_private->m_currentURL != url) {
            CFRelease(m_private->m_currentURL);
            m_private->m_currentURL = url;
            CFRetain(url);
        }
    } else {
        m_private->m_currentURL = url;
        CFRetain(url);
    }

    // Add the movie data location to the property array 
    movieProps[moviePropCount].propClass = kQTPropertyClass_DataLocation; 
    movieProps[moviePropCount].propID = kQTDataLocationPropertyID_CFURL; 
    movieProps[moviePropCount].propValueSize = sizeof(m_private->m_currentURL); 
    movieProps[moviePropCount].propValueAddress = &(m_private->m_currentURL); 
    movieProps[moviePropCount].propStatus = 0; 
    moviePropCount++; 

    movieProps[moviePropCount].propClass = kQTPropertyClass_MovieInstantiation; 
    movieProps[moviePropCount].propID = kQTMovieInstantiationPropertyID_DontAskUnresolvedDataRefs; 
    movieProps[moviePropCount].propValueSize = sizeof(boolTrue); 
    movieProps[moviePropCount].propValueAddress = &boolTrue; 
    movieProps[moviePropCount].propStatus = 0; 
    moviePropCount++; 

    movieProps[moviePropCount].propClass = kQTPropertyClass_MovieInstantiation; 
    movieProps[moviePropCount].propID = kQTMovieInstantiationPropertyID_AsyncOK; 
    movieProps[moviePropCount].propValueSize = sizeof(boolTrue); 
    movieProps[moviePropCount].propValueAddress = &boolTrue; 
    movieProps[moviePropCount].propStatus = 0; 
    moviePropCount++; 

    movieProps[moviePropCount].propClass = kQTPropertyClass_NewMovieProperty; 
    movieProps[moviePropCount].propID = kQTNewMoviePropertyID_Active; 
    movieProps[moviePropCount].propValueSize = sizeof(boolTrue); 
    movieProps[moviePropCount].propValueAddress = &boolTrue; 
    movieProps[moviePropCount].propStatus = 0; 
    moviePropCount++; 

    movieProps[moviePropCount].propClass = kQTPropertyClass_NewMovieProperty; 
    movieProps[moviePropCount].propID = kQTNewMoviePropertyID_DontInteractWithUser; 
    movieProps[moviePropCount].propValueSize = sizeof(boolTrue); 
    movieProps[moviePropCount].propValueAddress = &boolTrue; 
    movieProps[moviePropCount].propStatus = 0; 
    moviePropCount++; 

    movieProps[moviePropCount].propClass = kQTPropertyClass_MovieInstantiation;
    movieProps[moviePropCount].propID = '!url';
    movieProps[moviePropCount].propValueSize = sizeof(boolTrue); 
    movieProps[moviePropCount].propValueAddress = &boolTrue; 
    movieProps[moviePropCount].propStatus = 0; 
    moviePropCount++; 

    movieProps[moviePropCount].propClass = kQTPropertyClass_MovieInstantiation; 
    movieProps[moviePropCount].propID = 'site';
    movieProps[moviePropCount].propValueSize = sizeof(boolTrue); 
    movieProps[moviePropCount].propValueAddress = &boolTrue; 
    movieProps[moviePropCount].propStatus = 0; 
    moviePropCount++;

    movieProps[moviePropCount].propClass = kQTPropertyClass_Audio; 
    movieProps[moviePropCount].propID = kQTAudioPropertyID_RateChangesPreservePitch;
    movieProps[moviePropCount].propValueSize = sizeof(preservesPitch); 
    movieProps[moviePropCount].propValueAddress = &preservesPitch; 
    movieProps[moviePropCount].propStatus = 0; 
    moviePropCount++; 

    ASSERT(moviePropCount <= sizeof(movieProps)/sizeof(movieProps[0]));
    m_private->m_loadError = NewMovieFromProperties(moviePropCount, movieProps, 0, 0, &m_private->m_movie);

end:
    m_private->startTask();
    // get the load fail callback quickly 
    if (m_private->m_loadError)
        updateTaskTimer(0);
    else {
        OSType mode = kQTApertureMode_CleanAperture;

        // Set the aperture mode property on a movie to signal that we want aspect ratio
        // and clean aperture dimensions. Don't worry about errors, we can't do anything if
        // the installed version of QT doesn't support it and it isn't serious enough to 
        // warrant failing.
        QTSetMovieProperty(m_private->m_movie, kQTPropertyClass_Visual, kQTVisualPropertyID_ApertureMode, sizeof(mode), &mode);
        m_private->registerDrawingCallback();
    }
}

void QTMovieWin::disableUnsupportedTracks(unsigned& enabledTrackCount, unsigned& totalTrackCount)
{
    if (!m_private->m_movie) {
        totalTrackCount = 0;
        enabledTrackCount = 0;
        return;
    }

    static HashSet<OSType>* allowedTrackTypes = 0;
    if (!allowedTrackTypes) {
        allowedTrackTypes = new HashSet<OSType>;
        allowedTrackTypes->add(VideoMediaType);
        allowedTrackTypes->add(SoundMediaType);
        allowedTrackTypes->add(TextMediaType);
        allowedTrackTypes->add(BaseMediaType);
        allowedTrackTypes->add(closedCaptionTrackType);
        allowedTrackTypes->add(subTitleTrackType);
        allowedTrackTypes->add(mpeg4ObjectDescriptionTrackType);
        allowedTrackTypes->add(mpeg4SceneDescriptionTrackType);
        allowedTrackTypes->add(TimeCodeMediaType);
        allowedTrackTypes->add(TimeCode64MediaType);
    }

    long trackCount = GetMovieTrackCount(m_private->m_movie);
    enabledTrackCount = trackCount;
    totalTrackCount = trackCount;

    // Track indexes are 1-based. yuck. These things must descend from old-
    // school mac resources or something.
    for (long trackIndex = 1; trackIndex <= trackCount; trackIndex++) {
        // Grab the track at the current index. If there isn't one there, then
        // we can move onto the next one.
        Track currentTrack = GetMovieIndTrack(m_private->m_movie, trackIndex);
        if (!currentTrack)
            continue;
        
        // Check to see if the track is disabled already, we should move along.
        // We don't need to re-disable it.
        if (!GetTrackEnabled(currentTrack))
            continue;

        // Grab the track's media. We're going to check to see if we need to
        // disable the tracks. They could be unsupported.
        Media trackMedia = GetTrackMedia(currentTrack);
        if (!trackMedia)
            continue;
        
        // Grab the media type for this track. Make sure that we don't
        // get an error in doing so. If we do, then something really funky is
        // wrong.
        OSType mediaType;
        GetMediaHandlerDescription(trackMedia, &mediaType, nil, nil);
        OSErr mediaErr = GetMoviesError();    
        if (mediaErr != noErr)
            continue;
        
        if (!allowedTrackTypes->contains(mediaType)) {

            // Different mpeg variants import as different track types so check for the "mpeg 
            // characteristic" instead of hard coding the (current) list of mpeg media types.
            if (GetMovieIndTrackType(m_private->m_movie, 1, 'mpeg', movieTrackCharacteristic | movieTrackEnabledOnly))
                continue;

            SetTrackEnabled(currentTrack, false);
            --enabledTrackCount;
        }
        
        // Grab the track reference count for chapters. This will tell us if it
        // has chapter tracks in it. If there aren't any references, then we
        // can move on the next track.
        long referenceCount = GetTrackReferenceCount(currentTrack, kTrackReferenceChapterList);
        if (referenceCount <= 0)
            continue;
        
        long referenceIndex = 0;        
        while (1) {
            // If we get nothing here, we've overstepped our bounds and can stop
            // looking. Chapter indices here are 1-based as well - hence, the
            // pre-increment.
            referenceIndex++;
            Track chapterTrack = GetTrackReference(currentTrack, kTrackReferenceChapterList, referenceIndex);
            if (!chapterTrack)
                break;
            
            // Try to grab the media for the track.
            Media chapterMedia = GetTrackMedia(chapterTrack);
            if (!chapterMedia)
                continue;
        
            // Grab the media type for this track. Make sure that we don't
            // get an error in doing so. If we do, then something really
            // funky is wrong.
            OSType mediaType;
            GetMediaHandlerDescription(chapterMedia, &mediaType, nil, nil);
            OSErr mediaErr = GetMoviesError();
            if (mediaErr != noErr)
                continue;
            
            // Check to see if the track is a video track. We don't care about
            // other non-video tracks.
            if (mediaType != VideoMediaType)
                continue;
            
            // Check to see if the track is already disabled. If it is, we
            // should move along.
            if (!GetTrackEnabled(chapterTrack))
                continue;
            
            // Disabled the evil, evil track.
            SetTrackEnabled(chapterTrack, false);
            --enabledTrackCount;
        }
    }
}

void QTMovieWin::setDisabled(bool b)
{
    m_disabled = b;
}


bool QTMovieWin::hasVideo() const
{
    if (!m_private->m_movie)
        return false;
    return (GetMovieIndTrackType(m_private->m_movie, 1, VisualMediaCharacteristic, movieTrackCharacteristic | movieTrackEnabledOnly));
}

bool QTMovieWin::hasAudio() const
{
    if (!m_private->m_movie)
        return false;
    return (GetMovieIndTrackType(m_private->m_movie, 1, AudioMediaCharacteristic, movieTrackCharacteristic | movieTrackEnabledOnly));
}


bool QTMovieWin::hasClosedCaptions() const 
{
    if (!m_private->m_movie)
        return false;
    return GetMovieIndTrackType(m_private->m_movie, 1, closedCaptionTrackType, movieTrackMediaType);
}

void QTMovieWin::setClosedCaptionsVisible(bool visible)
{
    if (!m_private->m_movie)
        return;

    Track ccTrack = GetMovieIndTrackType(m_private->m_movie, 1, closedCaptionTrackType, movieTrackMediaType);
    if (!ccTrack)
        return;

    Boolean doDisplay = visible;
    QTSetTrackProperty(ccTrack, closedCaptionTrackType, closedCaptionDisplayPropertyID, sizeof(doDisplay), &doDisplay);
}

pascal OSErr movieDrawingCompleteProc(Movie movie, long data)
{
    UppParam param;
    param.longValue = data;
    QTMovieWinPrivate* mp = static_cast<QTMovieWinPrivate*>(param.ptr);
    if (mp)
        mp->drawingComplete();
    return 0;
}

static void initializeSupportedTypes() 
{
    if (gSupportedTypes)
        return;

    gSupportedTypes = new Vector<CFStringRef>;
    if (quickTimeVersion < minimumQuickTimeVersion) {
        LOG_ERROR("QuickTime version %x detected, at least %x required. Returning empty list of supported media MIME types.", quickTimeVersion, minimumQuickTimeVersion);
        return;
    }

    // QuickTime doesn't have an importer for video/quicktime. Add it manually.
    gSupportedTypes->append(CFSTR("video/quicktime"));

    for (int index = 0; index < 2; index++) {
        ComponentDescription findCD;

        // look at all movie importers that can import in place and are installed. 
        findCD.componentType = MovieImportType;
        findCD.componentSubType = 0;
        findCD.componentManufacturer = 0;
        findCD.componentFlagsMask = cmpIsMissing | movieImportSubTypeIsFileExtension | canMovieImportInPlace | dontAutoFileMovieImport;

        // look at those registered by HFS file types the first time through, by file extension the second time
        findCD.componentFlags = canMovieImportInPlace | (index ? movieImportSubTypeIsFileExtension : 0);
        
        long componentCount = CountComponents(&findCD);
        if (!componentCount)
            continue;

        Component comp = 0;
        while (comp = FindNextComponent(comp, &findCD)) {
            // Does this component have a MIME type container?
            ComponentDescription infoCD;
            OSErr err = GetComponentInfo(comp, &infoCD, nil /*name*/, nil /*info*/, nil /*icon*/);
            if (err)
                continue;
            if (!(infoCD.componentFlags & hasMovieImportMIMEList))
                continue;
            QTAtomContainer mimeList = 0;
            err = MovieImportGetMIMETypeList((ComponentInstance)comp, &mimeList);
            if (err || !mimeList)
                continue;

            // Grab every type from the container.
            QTLockContainer(mimeList);
            int typeCount = QTCountChildrenOfType(mimeList, kParentAtomIsContainer, kMimeInfoMimeTypeTag);
            for (int typeIndex = 1; typeIndex <= typeCount; typeIndex++) {
                QTAtom mimeTag = QTFindChildByIndex(mimeList, 0, kMimeInfoMimeTypeTag, typeIndex, 0);
                if (!mimeTag)
                    continue;
                char* atomData;
                long typeLength;
                if (noErr != QTGetAtomDataPtr(mimeList, mimeTag, &typeLength, &atomData))
                    continue;

                char typeBuffer[256];
                if (typeLength >= sizeof(typeBuffer))
                    continue;
                memcpy(typeBuffer, atomData, typeLength);
                typeBuffer[typeLength] = 0;

                // Only add "audio/..." and "video/..." types.
                if (strncmp(typeBuffer, "audio/", 6) && strncmp(typeBuffer, "video/", 6))
                    continue;

                CFStringRef cfMimeType = CFStringCreateWithCString(0, typeBuffer, kCFStringEncodingUTF8);
                if (!cfMimeType)
                    continue;

                // Only add each type once.
                bool alreadyAdded = false;
                for (int addedIndex = 0; addedIndex < gSupportedTypes->size(); addedIndex++) {
                    CFStringRef type = gSupportedTypes->at(addedIndex);
                    if (kCFCompareEqualTo == CFStringCompare(cfMimeType, type, kCFCompareCaseInsensitive)) {
                        alreadyAdded = true;
                        break;
                    }
                }
                if (!alreadyAdded)
                    gSupportedTypes->append(cfMimeType);
                else
                    CFRelease(cfMimeType);
            }
            DisposeHandle(mimeList);
        }
    }
}

unsigned QTMovieWin::countSupportedTypes()
{
    initializeSupportedTypes();
    return static_cast<unsigned>(gSupportedTypes->size());
}

void QTMovieWin::getSupportedType(unsigned index, const UChar*& str, unsigned& len)
{
    initializeSupportedTypes();
    ASSERT(index < gSupportedTypes->size());

    // Allocate sufficient buffer to hold any MIME type
    static UniChar* staticBuffer = 0;
    if (!staticBuffer)
        staticBuffer = new UniChar[32];

    CFStringRef cfstr = gSupportedTypes->at(index);
    len = CFStringGetLength(cfstr);
    CFRange range = { 0, len };
    CFStringGetCharacters(cfstr, range, staticBuffer);
    str = reinterpret_cast<const UChar*>(staticBuffer);
    
}

void QTMovieWin::setTaskTimerFuncs(SetTaskTimerDelayFunc setTaskTimerDelay, StopTaskTimerFunc stopTaskTimer)
{
    gSetTaskTimerDelay = setTaskTimerDelay;
    gStopTaskTimer = stopTaskTimer;
}

bool QTMovieWin::initializeQuickTime() 
{
    static bool initialized = false;
    static bool initializationSucceeded = false;
    if (!initialized) {
        initialized = true;
        // Initialize and check QuickTime version
        OSErr result = InitializeQTML(kInitializeQTMLEnableDoubleBufferedSurface);
        if (result == noErr)
            result = Gestalt(gestaltQuickTime, &quickTimeVersion);
        if (result != noErr) {
            LOG_ERROR("No QuickTime available. Disabling <video> and <audio> support.");
            return false;
        }
        if (quickTimeVersion < minimumQuickTimeVersion) {
            LOG_ERROR("QuickTime version %x detected, at least %x required. Disabling <video> and <audio> support.", quickTimeVersion, minimumQuickTimeVersion);
            return false;
        }
        EnterMovies();
        gMovieDrawingCompleteUPP = NewMovieDrawingCompleteUPP(movieDrawingCompleteProc);
        initializationSucceeded = true;
    }
    return initializationSucceeded;
}

LRESULT QTMovieWin::fullscreenWndProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    QTMovieWin* movie = static_cast<QTMovieWin*>(GetProp(wnd, fullscreenQTMovieWinPointerProp));

    if (message == WM_DESTROY)
        RemoveProp(wnd, fullscreenQTMovieWinPointerProp);

    if (!movie)
        return DefWindowProc(wnd, message, wParam, lParam);

    return movie->m_private->m_fullscreenClient->fullscreenClientWndProc(wnd, message, wParam, lParam);
}

HWND QTMovieWin::enterFullscreen(QTMovieWinFullscreenClient* client)
{
    m_private->m_fullscreenClient = client;
    
    BeginFullScreen(&m_private->m_fullscreenRestoreState, 0, 0, 0, &m_private->m_fullscreenWindow, 0, fullScreenAllowEvents); 
    QTMLSetWindowWndProc(m_private->m_fullscreenWindow, fullscreenWndProc);
    CreatePortAssociation(GetPortNativeWindow(m_private->m_fullscreenWindow), 0, 0);

    GetMovieBox(m_private->m_movie, &m_private->m_fullscreenRect);
    GetMovieGWorld(m_private->m_movie, &m_private->m_fullscreenOrigGWorld, 0);
    SetMovieGWorld(m_private->m_movie, reinterpret_cast<CGrafPtr>(m_private->m_fullscreenWindow), GetGWorldDevice(reinterpret_cast<CGrafPtr>(m_private->m_fullscreenWindow)));

    // Set the size of the box to preserve aspect ratio
    Rect rect = m_private->m_fullscreenWindow->portRect;

    float movieRatio = static_cast<float>(m_private->m_width) / m_private->m_height;
    int windowWidth =  rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;
    float windowRatio = static_cast<float>(windowWidth) / windowHeight;
    int actualWidth = (windowRatio > movieRatio) ? (windowHeight * movieRatio) : windowWidth;
    int actualHeight = (windowRatio < movieRatio) ? (windowWidth / movieRatio) : windowHeight;
    int offsetX = (windowWidth - actualWidth) / 2;
    int offsetY = (windowHeight - actualHeight) / 2;

    rect.left = offsetX;
    rect.right = offsetX + actualWidth;
    rect.top = offsetY;
    rect.bottom = offsetY + actualHeight;

    SetMovieBox(m_private->m_movie, &rect);
    ShowHideTaskBar(true);

    // Set the 'this' pointer on the HWND
    HWND wnd = static_cast<HWND>(GetPortNativeWindow(m_private->m_fullscreenWindow));
    SetProp(wnd, fullscreenQTMovieWinPointerProp, static_cast<HANDLE>(this));

    return wnd;
}

void QTMovieWin::exitFullscreen()
{
    if (!m_private->m_fullscreenWindow)
        return;

    HWND wnd = static_cast<HWND>(GetPortNativeWindow(m_private->m_fullscreenWindow));
    DestroyPortAssociation(reinterpret_cast<CGrafPtr>(m_private->m_fullscreenWindow));
    SetMovieGWorld(m_private->m_movie, m_private->m_fullscreenOrigGWorld, 0);
    EndFullScreen(m_private->m_fullscreenRestoreState, 0L);
    SetMovieBox(m_private->m_movie, &m_private->m_fullscreenRect);
    m_private->m_fullscreenWindow = 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        return TRUE;
    case DLL_PROCESS_DETACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        return FALSE;
    }
    ASSERT_NOT_REACHED();
    return FALSE;
}
