/*
 * Copyright (C) 2014 Alex Christensen <achristensen@webkit.org>
 * All rights reserved.
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

#pragma once

#if ENABLE(VIDEO) && USE(MEDIA_FOUNDATION)

#include "COMPtr.h"
#include "MediaPlayerPrivate.h"

#include <D3D9.h>
#include <Dxva2api.h>

#include <Mfapi.h>
#include <Mferror.h>
#include <Mfidl.h>
#include <evcode.h>
#include <evr.h>

#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/WeakPtr.h>
#include <wtf/win/Win32Handle.h>

namespace WebCore {

class MediaPlayerPrivateMediaFoundation final : public MediaPlayerPrivateInterface, public CanMakeWeakPtr<MediaPlayerPrivateMediaFoundation> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit MediaPlayerPrivateMediaFoundation(MediaPlayer*);
    ~MediaPlayerPrivateMediaFoundation();
    static void registerMediaEngine(MediaEngineRegistrar);

    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    static bool isAvailable();

    void load(const String& url) final;
    void cancelLoad() final;

    void play() final;
    void pause() final;

    bool supportsFullscreen() const final;

    FloatSize naturalSize() const final;

    bool hasVideo() const final;
    bool hasAudio() const final;

    void setPageIsVisible(bool) final;

    bool seeking() const final;
    void seek(float) final;

    void setRate(float) final;

    float duration() const final;

    float currentTime() const final;

    bool paused() const final;

    void setVolume(float) final;

    void setMuted(bool) final;

    MediaPlayer::NetworkState networkState() const final;
    MediaPlayer::ReadyState readyState() const final;

    float maxTimeSeekable() const final;

    std::unique_ptr<PlatformTimeRanges> buffered() const final;

    bool didLoadingProgress() const final;

    void setPresentationSize(const IntSize&) final;

    void paint(GraphicsContext&, const FloatRect&) final;

    DestinationColorSpace colorSpace() final;

protected:
    void onCreatedMediaSource(COMPtr<IMFMediaSource>&&, bool loadingProgress);
    void onNetworkStateChanged(MediaPlayer::NetworkState);
    void onTopologySet();
    void onBufferingStarted();
    void onBufferingStopped();
    void onSessionStarted();
    void onSessionEnded();

    friend HRESULT beginGetEvent(WeakPtr<MediaPlayerPrivateMediaFoundation>, COMPtr<IMFMediaSession>);

private:
    float maxTimeLoaded() const { return m_maxTimeLoaded; }

    WeakPtr<MediaPlayerPrivateMediaFoundation> m_weakThis;
    MediaPlayer* m_player;
    IntSize m_size;
    bool m_visible;
    bool m_loadingProgress;
    bool m_paused;
    bool m_seeking { false };
    bool m_sessionEnded { false };
    bool m_hasAudio;
    bool m_hasVideo;
    float m_volume;
    mutable float m_maxTimeLoaded { 0 };
    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;

    class MediaPlayerListener;
    HashSet<MediaPlayerListener*> m_listeners;
    Lock m_mutexListeners;

    FloatSize m_cachedNaturalSize;
    mutable Lock m_cachedNaturalSizeLock;

    COMPtr<IMFMediaSession> m_mediaSession;
    COMPtr<IMFSourceResolver> m_sourceResolver;
    COMPtr<IMFMediaSource> m_mediaSource;
    COMPtr<IMFTopology> m_topology;
    COMPtr<IMFPresentationDescriptor> m_sourcePD;
    COMPtr<IMFVideoDisplayControl> m_videoDisplay;

    bool createSession();
    bool startSession();
    bool endSession();
    bool startCreateMediaSource(const String& url);
    bool createTopologyFromSource();
    bool addBranchToPartialTopology(int stream);
    bool createOutputNode(COMPtr<IMFStreamDescriptor> sourceSD, COMPtr<IMFTopologyNode>&);
    bool createSourceStreamNode(COMPtr<IMFStreamDescriptor> sourceSD, COMPtr<IMFTopologyNode>&);

    void updateReadyState();

    COMPtr<IMFVideoDisplayControl> videoDisplay();

    HWND hostWindow();
    void invalidateVideoArea();

    void addListener(MediaPlayerListener*);
    void removeListener(MediaPlayerListener*);
    void setNaturalSize(const FloatSize&);
    void notifyDeleted();

    bool setAllChannelVolumes(float);

    class MediaPlayerListener {
    public:
        MediaPlayerListener() = default;
        virtual ~MediaPlayerListener() = default;

        virtual void onMediaPlayerDeleted() { }
    };

    class AsyncCallback;

    typedef Deque<COMPtr<IMFSample>> VideoSampleList;

    class VideoSamplePool {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        VideoSamplePool() = default;
        virtual ~VideoSamplePool() = default;

        HRESULT initialize(VideoSampleList& samples);
        void clear();

        HRESULT getSample(COMPtr<IMFSample>&);
        HRESULT returnSample(IMFSample*);
        bool areSamplesPending();

    private:
        Lock m_lock;
        VideoSampleList m_videoSampleQueue;
        bool m_initialized { false };
        unsigned m_pending { 0 };
    };

    class Direct3DPresenter;

    class VideoScheduler {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        VideoScheduler() = default;
        virtual ~VideoScheduler() = default;

        void setPresenter(Direct3DPresenter* presenter) { m_presenter = presenter; }

        void setFrameRate(const MFRatio& fps);
        void setClockRate(float rate) { m_playbackRate = rate; }

        const LONGLONG& lastSampleTime() const { return m_lastSampleTime; }
        const LONGLONG& frameDuration() const { return m_frameDuration; }

        HRESULT startScheduler(IMFClock*);
        HRESULT stopScheduler();

        HRESULT scheduleSample(IMFSample*, bool presentNow);
        HRESULT processSamplesInQueue(LONG& nextSleep);
        HRESULT processSample(IMFSample*, LONG& nextSleep);
        HRESULT flush();

    private:
        static DWORD WINAPI schedulerThreadProc(LPVOID lpParameter);
        DWORD schedulerThreadProcPrivate();

        Deque<COMPtr<IMFSample>> m_scheduledSamples;
        Lock m_lock;

        COMPtr<IMFClock> m_clock;
        Direct3DPresenter* m_presenter { nullptr };

        DWORD m_threadID { 0 };
        WTF::Win32Handle m_schedulerThread;
        WTF::Win32Handle m_threadReadyEvent;
        WTF::Win32Handle m_flushEvent;

        float m_playbackRate { 1.0f };
        MFTIME m_frameDuration { 0 };
        MFTIME m_lastSampleTime { 0 };

        std::atomic<bool> m_exitThread { false };

        void stopThread() { m_exitThread = true; }
    };

    class Direct3DPresenter {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Direct3DPresenter();
        ~Direct3DPresenter();

        enum DeviceState {
            DeviceOK,
            DeviceReset,
            DeviceRemoved,
        };

        // Returns the IDirect3DDeviceManager9 interface.
        HRESULT getService(REFGUID guidService, REFIID riid, void** ppv);

        HRESULT checkFormat(D3DFORMAT);

        HRESULT setVideoWindow(HWND);
        HWND getVideoWindow() const { return m_hwnd; }
        HRESULT setDestinationRect(const RECT& destRect);
        RECT getDestinationRect() const { return m_destRect; };

        HRESULT createVideoSamples(IMFMediaType* format, VideoSampleList& videoSampleQueue);
        void releaseResources();

        HRESULT checkDeviceState(DeviceState&);
        HRESULT presentSample(IMFSample*, LONGLONG target);

        UINT refreshRate() const { return m_displayMode.RefreshRate; }

        void paintCurrentFrame(GraphicsContext&, const FloatRect&);

    private:
        HRESULT initializeD3D();
        HRESULT getSwapChainPresentParameters(IMFMediaType*, D3DPRESENT_PARAMETERS* presentParams);
        HRESULT createD3DDevice();
        HRESULT createD3DSample(IDirect3DSwapChain9*, COMPtr<IMFSample>& videoSample);

        UINT m_deviceResetToken { 0 };
        HWND m_hwnd { nullptr };
        RECT m_destRect;
        D3DDISPLAYMODE m_displayMode;

        Lock m_lock;
        
        COMPtr<IDirect3D9Ex> m_direct3D9;
        COMPtr<IDirect3DDevice9Ex> m_device;
        COMPtr<IDirect3DDeviceManager9> m_deviceManager;
        COMPtr<IDirect3DSurface9> m_surfaceRepaint;

        COMPtr<IDirect3DSurface9> m_memSurface;
        int m_width { 0 };
        int m_height { 0 };
    };

    class CustomVideoPresenter
        : public IMFVideoPresenter
        , public IMFVideoDeviceID
        , public IMFTopologyServiceLookupClient
        , public IMFGetService
        , public IMFActivate
        , public IMFVideoDisplayControl
        , public IMFAsyncCallback
        , public MediaPlayerListener {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        CustomVideoPresenter(MediaPlayerPrivateMediaFoundation*);
        ~CustomVideoPresenter();

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) override;
        ULONG STDMETHODCALLTYPE AddRef() override;
        ULONG STDMETHODCALLTYPE Release() override;

        // IMFClockStateSink
        HRESULT STDMETHODCALLTYPE OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) override;
        HRESULT STDMETHODCALLTYPE OnClockStop(MFTIME hnsSystemTime) override;
        HRESULT STDMETHODCALLTYPE OnClockPause(MFTIME hnsSystemTime) override;
        HRESULT STDMETHODCALLTYPE OnClockRestart(MFTIME hnsSystemTime) override;
        HRESULT STDMETHODCALLTYPE OnClockSetRate(MFTIME hnsSystemTime, float flRate) override;

        // IMFVideoPresenter
        HRESULT STDMETHODCALLTYPE ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam) override;
        HRESULT STDMETHODCALLTYPE GetCurrentMediaType(_Outptr_  IMFVideoMediaType **ppMediaType) override;

        // IMFVideoDeviceID
        HRESULT STDMETHODCALLTYPE GetDeviceID(IID* pDeviceID) override;

        // IMFTopologyServiceLookupClient
        HRESULT STDMETHODCALLTYPE InitServicePointers(_In_  IMFTopologyServiceLookup *pLookup) override;
        HRESULT STDMETHODCALLTYPE ReleaseServicePointers(void) override;

        // IMFGetService
        HRESULT STDMETHODCALLTYPE GetService(REFGUID guidService, REFIID riid, LPVOID *ppvObject) override;

        // IMFActivate
        HRESULT STDMETHODCALLTYPE ActivateObject(REFIID riid, void **ppv) override;
        HRESULT STDMETHODCALLTYPE DetachObject() override;
        HRESULT STDMETHODCALLTYPE ShutdownObject() override;

        // IMFAttributes
        HRESULT STDMETHODCALLTYPE GetItem(__RPC__in REFGUID guidKey, __RPC__inout_opt PROPVARIANT *pValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetItemType(__RPC__in REFGUID guidKey, __RPC__out MF_ATTRIBUTE_TYPE *pType) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE CompareItem(__RPC__in REFGUID guidKey, __RPC__in REFPROPVARIANT Value, __RPC__out BOOL *pbResult) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE Compare(__RPC__in_opt IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, __RPC__out BOOL *pbResult) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetUINT32(__RPC__in REFGUID guidKey, __RPC__out UINT32 *punValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetUINT64(__RPC__in REFGUID guidKey, __RPC__out UINT64 *punValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetDouble(__RPC__in REFGUID guidKey, __RPC__out double *pfValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetGUID(__RPC__in REFGUID guidKey, __RPC__out GUID *pguidValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetStringLength(__RPC__in REFGUID guidKey, __RPC__out UINT32 *pcchLength) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetString(__RPC__in REFGUID guidKey, __RPC__out_ecount_full(cchBufSize) LPWSTR pwszValue, UINT32 cchBufSize, __RPC__inout_opt UINT32 *pcchLength) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetAllocatedString(__RPC__in REFGUID guidKey, __RPC__deref_out_ecount_full_opt((*pcchLength + 1)) LPWSTR *ppwszValue, __RPC__out UINT32 *pcchLength) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetBlobSize(__RPC__in REFGUID guidKey, __RPC__out UINT32 *pcbBlobSize) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetBlob(__RPC__in REFGUID guidKey, __RPC__out_ecount_full(cbBufSize) UINT8 *pBuf, UINT32 cbBufSize, __RPC__inout_opt UINT32 *pcbBlobSize) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetAllocatedBlob(__RPC__in REFGUID guidKey, __RPC__deref_out_ecount_full_opt(*pcbSize) UINT8 **ppBuf, __RPC__out UINT32 *pcbSize) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetUnknown(__RPC__in REFGUID guidKey, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID *ppv) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetItem(__RPC__in REFGUID guidKey, __RPC__in REFPROPVARIANT Value) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE DeleteItem(__RPC__in REFGUID guidKey) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE DeleteAllItems(void) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetUINT32(__RPC__in REFGUID guidKey, UINT32 unValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetUINT64(__RPC__in REFGUID guidKey, UINT64 unValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetDouble(__RPC__in REFGUID guidKey, double fValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetGUID(__RPC__in REFGUID guidKey, __RPC__in REFGUID guidValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetString(__RPC__in REFGUID guidKey, __RPC__in_string LPCWSTR wszValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetBlob(__RPC__in REFGUID guidKey, __RPC__in_ecount_full(cbBufSize) const UINT8 *pBuf, UINT32 cbBufSize) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetUnknown(__RPC__in REFGUID guidKey, __RPC__in_opt IUnknown *pUnknown) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE LockStore(void) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE UnlockStore(void) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetCount(__RPC__out UINT32 *pcItems) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetItemByIndex(UINT32 unIndex, __RPC__out GUID *pguidKey, __RPC__inout_opt PROPVARIANT *pValue) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE CopyAllItems(__RPC__in_opt IMFAttributes *pDest) override { return E_NOTIMPL; }

        // IMFVideoDisplayControl
        HRESULT STDMETHODCALLTYPE GetNativeVideoSize(SIZE* pszVideo, SIZE* pszARVideo) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetIdealVideoSize(SIZE* pszMin, SIZE* pszMax) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetVideoPosition(const MFVideoNormalizedRect* pnrcSource, const LPRECT prcDest) override;
        HRESULT STDMETHODCALLTYPE GetVideoPosition(MFVideoNormalizedRect* pnrcSource, LPRECT prcDest) override;
        HRESULT STDMETHODCALLTYPE SetAspectRatioMode(DWORD dwAspectRatioMode) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetAspectRatioMode(DWORD* pdwAspectRatioMode) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetVideoWindow(HWND hwndVideo) override;
        HRESULT STDMETHODCALLTYPE GetVideoWindow(HWND* phwndVideo) override;
        HRESULT STDMETHODCALLTYPE RepaintVideo() override;
        HRESULT STDMETHODCALLTYPE GetCurrentImage(BITMAPINFOHEADER* pBih, BYTE** pDib, DWORD* pcbDib, LONGLONG* pTimeStamp) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetBorderColor(COLORREF Clr) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetBorderColor(COLORREF* pClr) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetRenderingPrefs(DWORD dwRenderFlags) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetRenderingPrefs(DWORD* pdwRenderFlags) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetFullscreen(BOOL bFullscreen) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetFullscreen(BOOL* pbFullscreen) override { return E_NOTIMPL; }

        // IMFAsyncCallback methods
        HRESULT STDMETHODCALLTYPE GetParameters(DWORD*, DWORD*) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult* pAsyncResult) override;

        // MediaPlayerListener
        void onMediaPlayerDeleted() override;

        void paintCurrentFrame(GraphicsContext&, const FloatRect&);

    private:
        ULONG m_refCount { 0 };
        Lock m_lock;
        MediaPlayerPrivateMediaFoundation* m_mediaPlayer;

        enum RenderState {
            RenderStateStarted = 1,
            RenderStateStopped,
            RenderStatePaused,
            RenderStateShutdown,
        };

        RenderState m_renderState { RenderStateShutdown };
        COMPtr<IMFClock> m_clock;
        COMPtr<IMediaEventSink> m_mediaEventSink;
        COMPtr<IMFTransform> m_mixer;
        COMPtr<IMFMediaType> m_mediaType;
        std::unique_ptr<Direct3DPresenter> m_presenterEngine;
        MFVideoNormalizedRect m_sourceRect;
        bool m_sampleNotify { false };
        bool m_prerolled { false };
        bool m_repaint { false };
        bool m_endStreaming { false };
        VideoScheduler m_scheduler;
        VideoSamplePool m_samplePool;
        unsigned m_tokenCounter { 0 };
        float m_rate { 1.0f };

        bool isActive() const;

        bool isScrubbing() const { return m_rate == 0.0f; }

        HRESULT configureMixer(IMFTransform* mixer);
        HRESULT flush();
        HRESULT setMediaType(IMFMediaType*);
        HRESULT checkShutdown() const;
        HRESULT renegotiateMediaType();
        HRESULT processInputNotify();
        HRESULT beginStreaming();
        HRESULT endStreaming();
        HRESULT checkEndOfStream();
        HRESULT isMediaTypeSupported(IMFMediaType*);
        HRESULT createOptimalVideoType(IMFMediaType* proposedType, IMFMediaType** optimalType);
        HRESULT calculateOutputRectangle(IMFMediaType* proposedType, RECT& outputRect);

        void processOutputLoop();
        HRESULT processOutput();
        HRESULT deliverSample(IMFSample*, bool repaint);
        HRESULT trackSample(IMFSample*);
        void releaseResources();

        HRESULT onSampleFree(IMFAsyncResult*);

        void notifyEvent(long EventCode, LONG_PTR Param1, LONG_PTR Param2);
    };

    COMPtr<CustomVideoPresenter> m_presenter;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(MEDIA_FOUNDATION)
