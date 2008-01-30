/*
 * Copyright (C) 2007 Apple Computer, Inc.  All rights reserved.
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

#include <windows.h>

#include "QTMovieWin.h"

// Put Movies.h first so build failures here point clearly to QuickTime
#include <Movies.h>
#include <GXMath.h>
#include <QTML.h>

#include "QTMovieWinTimer.h"

#include <wtf/Assertions.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

using namespace std;

static const long minimumQuickTimeVersion = 0x07300000; // 7.3

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

static void updateTaskTimer(int maxInterval = 1000)
{
    if (!gTaskList->size()) {
        stopSharedTimer();
        return;    
    }
    
    long intervalInMS;
    QTGetTimeUntilNextTask(&intervalInMS, 1000);
    if (intervalInMS > maxInterval)
        intervalInMS = maxInterval;
    setSharedTimerFireDelay(static_cast<float>(intervalInMS) / 1000);
}

class QTMovieWinPrivate : Noncopyable {
public:
    QTMovieWinPrivate();
    ~QTMovieWinPrivate();
    void task();
    void startTask();
    void endTask();

    void registerDrawingCallback();
    void drawingComplete();
    void updateGWorld();
    void createGWorld();
    void deleteGWorld();

    void setSize(int, int);

    QTMovieWin* m_movieWin;
    Movie m_movie;
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
};

QTMovieWinPrivate::QTMovieWinPrivate()
    : m_movieWin(0)
    , m_movie(0)
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
{
}

QTMovieWinPrivate::~QTMovieWinPrivate()
{
    endTask();
    if (m_gWorld)
        deleteGWorld();
    if (m_movie)
        DisposeMovie(m_movie);
}

static void taskTimerFired()
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

void QTMovieWinPrivate::task() 
{
    ASSERT(m_tasking);

    if (!m_loadError)
        MoviesTask(m_movie, 0);

    // GetMovieLoadState documentation says that you should not call it more often than every quarter of a second.
    if (systemTime() >= m_lastLoadStateCheckTime + 0.25 || m_loadError) { 
        // If load fails QT's load state is kMovieLoadStateComplete.
        // This is different from QTKit API and seems strange.
        long loadState = m_loadError ? kMovieLoadStateError : GetMovieLoadState(m_movie);
        if (loadState != m_loadState) {
            m_loadState = loadState;
            m_client->movieLoadStateChanged(m_movieWin);
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

void QTMovieWinPrivate::registerDrawingCallback()
{
    UppParam param;
    param.ptr = this;
    SetMovieDrawingCompleteProc(m_movie, movieDrawingCallWhenChanged, gMovieDrawingCompleteUPP, param.longValue);
}

void QTMovieWinPrivate::drawingComplete()
{
    if (!m_gWorld)
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
    if (!m_movie)
        return;

    m_gWorldWidth = max(cGWorldMinWidth, m_width);
    m_gWorldHeight = max(cGWorldMinHeight, m_height);
    Rect bounds; 
    bounds.top = 0;
    bounds.left = 0; 
    bounds.right = m_gWorldWidth;
    bounds.bottom = m_gWorldHeight;
    OSErr err = QTNewGWorld(&m_gWorld, k32BGRAPixelFormat, &bounds, NULL, NULL, NULL); 
    if (err) 
        return;
    GetMovieGWorld(m_movie, &m_savedGWorld, 0);
    SetMovieGWorld(m_movie, m_gWorld, 0);
    bounds.right = m_width;
    bounds.bottom = m_height;
    SetMovieBox(m_movie, &bounds);
}

void QTMovieWinPrivate::setSize(int width, int height)
{
    if (m_width == width && m_height == height)
        return;
    m_width = width;
    m_height = height;
    if (!m_movie)
        return;
    Rect bounds; 
    bounds.top = 0;
    bounds.left = 0; 
    bounds.right = width;
    bounds.bottom = height;
    SetMovieBox(m_movie, &bounds);
    updateGWorld();
}

void QTMovieWinPrivate::deleteGWorld()
{
    ASSERT(m_gWorld);
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
    StartMovie(m_private->m_movie);
    m_private->startTask();
}

void QTMovieWin::pause()
{
    StopMovie(m_private->m_movie);
    updateTaskTimer();
}

float QTMovieWin::rate() const
{
    return FixedToFloat(GetMovieRate(m_private->m_movie));
}

void QTMovieWin::setRate(float rate)
{
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
    m_private->m_seeking = true;
    TimeScale scale = GetMovieTimeScale(m_private->m_movie);
    SetMovieTimeValue(m_private->m_movie, TimeValue(time * scale));
    updateTaskTimer();
}

void QTMovieWin::setVolume(float volume)
{
    SetMovieVolume(m_private->m_movie, static_cast<short>(volume * 256));
}

unsigned QTMovieWin::dataSize() const
{
    // FIXME: How to get this?
    return 1000;
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
    Rect rect;
    GetMovieNaturalBoundsRect(m_private->m_movie, &rect);
    width = rect.right;
    height = rect.bottom;
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

void QTMovieWin::load(const UChar* url, int len)
{
    if (m_private->m_movie) {
        m_private->endTask();
        if (m_private->m_gWorld)
            m_private->deleteGWorld();
        DisposeMovie(m_private->m_movie);
        m_private->m_movie = 0;
    }  

    // Define a property array for NewMovieFromProperties. 8 should be enough for our needs. 
    QTNewMoviePropertyElement movieProps[8]; 
    ItemCount moviePropCount = 0; 

    bool boolTrue = true;

    // Create a URL data reference of type CFURL 
    CFStringRef urlStringRef = CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(url), len);
    
    // Disable streaming support for now. 
    if (CFStringHasPrefix(urlStringRef, CFSTR("rtsp:"))) {
        m_private->m_loadError = noMovieFound;
        goto end;
    }

    CFURLRef urlRef = CFURLCreateWithString(kCFAllocatorDefault, urlStringRef, 0); 

    // Add the movie data location to the property array 
    movieProps[moviePropCount].propClass = kQTPropertyClass_DataLocation; 
    movieProps[moviePropCount].propID = kQTDataLocationPropertyID_CFURL; 
    movieProps[moviePropCount].propValueSize = sizeof(urlRef); 
    movieProps[moviePropCount].propValueAddress = &urlRef; 
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

    m_private->m_loadError = NewMovieFromProperties(moviePropCount, movieProps, 0, NULL, &m_private->m_movie);

    CFRelease(urlRef);
end:
    m_private->startTask();
    // get the load fail callback quickly 
    if (m_private->m_loadError)
        updateTaskTimer(0);
    else
        m_private->registerDrawingCallback();

    CFRelease(urlStringRef);
}

void QTMovieWin::disableUnsupportedTracks(unsigned& enabledTrackCount)
{
    if (!m_private->m_movie) {
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
        allowedTrackTypes->add('clcp'); // Closed caption
        allowedTrackTypes->add('sbtl'); // Subtitle
    }

    long trackCount = GetMovieTrackCount(m_private->m_movie);
    enabledTrackCount = trackCount;
    
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
    // FIXME: This list might not be complete. 
    // There must be some way to get it out from QuickTime.
    gSupportedTypes = new Vector<CFStringRef>;
    gSupportedTypes->append(CFSTR("video/3gpp"));
    gSupportedTypes->append(CFSTR("video/3gpp2"));
    gSupportedTypes->append(CFSTR("video/mp4"));
    gSupportedTypes->append(CFSTR("video/mpeg"));
    gSupportedTypes->append(CFSTR("video/quicktime"));
    gSupportedTypes->append(CFSTR("audio/ac3"));
    gSupportedTypes->append(CFSTR("audio/aiff"));
    gSupportedTypes->append(CFSTR("audio/basic"));
    gSupportedTypes->append(CFSTR("audio/mpeg"));
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

bool QTMovieWin::initializeQuickTime() 
{
    static bool initialized = false;
    static bool initializationSucceeded = false;
    if (!initialized) {
        initialized = true;
        // Initialize and check QuickTime version
        OSErr result = InitializeQTML(0);
        SInt32 version = 0;
        if (result == noErr)
            result = Gestalt(gestaltQuickTime, &version);
        if (result != noErr) {
            LOG_ERROR("No QuickTime available. Disabling <video> and <audio> support.");
            return false;
        }
        if (version < minimumQuickTimeVersion) {
            LOG_ERROR("QuickTime version %x detected, at least %x required. Disabling <video> and <audio> support.", version, minimumQuickTimeVersion);
            return false;
        }
        EnterMovies();
        setSharedTimerFiredFunction(taskTimerFired);
        gMovieDrawingCompleteUPP = NewMovieDrawingCompleteUPP(movieDrawingCompleteProc);
        initializationSucceeded = true;
    }
    return initializationSucceeded;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            setSharedTimerInstanceHandle(hinstDLL);
            return TRUE;
        case DLL_PROCESS_DETACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            return FALSE;
    }
    ASSERT_NOT_REACHED();
    return FALSE;
}
