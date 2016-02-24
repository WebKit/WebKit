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

#ifndef MediaPlayerPrivateMediaFoundation_h
#define MediaPlayerPrivateMediaFoundation_h

#include "COMPtr.h"
#include "MediaPlayerPrivate.h"
#include "Win32Handle.h"

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

namespace WebCore {

class MediaPlayerPrivateMediaFoundation : public MediaPlayerPrivateInterface {
public:
    explicit MediaPlayerPrivateMediaFoundation(MediaPlayer*);
    ~MediaPlayerPrivateMediaFoundation();
    static void registerMediaEngine(MediaEngineRegistrar);

    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    static bool isAvailable();

    virtual void load(const String& url);
    virtual void cancelLoad();

    virtual void play();
    virtual void pause();

    virtual bool supportsFullscreen() const;

    virtual FloatSize naturalSize() const;

    virtual bool hasVideo() const;
    virtual bool hasAudio() const;

    virtual void setVisible(bool);

    virtual bool seeking() const;
    virtual void seekDouble(double) override;

    virtual void setRateDouble(double) override;

    virtual double durationDouble() const override;

    virtual float currentTime() const override;

    virtual bool paused() const;

    virtual void setVolume(float) override;

    virtual bool supportsMuting() const override;
    virtual void setMuted(bool) override;

    virtual MediaPlayer::NetworkState networkState() const;
    virtual MediaPlayer::ReadyState readyState() const;

    virtual float maxTimeSeekable() const override;

    virtual std::unique_ptr<PlatformTimeRanges> buffered() const;

    virtual bool didLoadingProgress() const;

    virtual void setSize(const IntSize&);

    virtual void paint(GraphicsContext&, const FloatRect&) override;

private:
    MediaPlayer* m_player;
    IntSize m_size;
    bool m_visible;
    bool m_loadingProgress;
    bool m_paused;
    bool m_hasAudio;
    bool m_hasVideo;
    HWND m_hwndVideo;
    MediaPlayer::ReadyState m_readyState;
    FloatRect m_lastPaintRect;

    class MediaPlayerListener;
    HashSet<MediaPlayerListener*> m_listeners;
    Lock m_mutexListeners;

    WeakPtrFactory<MediaPlayerPrivateMediaFoundation> m_weakPtrFactory;
    COMPtr<IMFMediaSession> m_mediaSession;
    COMPtr<IMFSourceResolver> m_sourceResolver;
    COMPtr<IMFMediaSource> m_mediaSource;
    COMPtr<IMFTopology> m_topology;
    COMPtr<IMFPresentationDescriptor> m_sourcePD;
    COMPtr<IMFVideoDisplayControl> m_videoDisplay;

    bool createSession();
    bool endSession();
    bool startCreateMediaSource(const String& url);
    bool endCreatedMediaSource(IMFAsyncResult*);
    bool endGetEvent(IMFAsyncResult*);
    bool createTopologyFromSource();
    bool addBranchToPartialTopology(int stream);
    bool createOutputNode(COMPtr<IMFStreamDescriptor> sourceSD, COMPtr<IMFTopologyNode>&);
    bool createSourceStreamNode(COMPtr<IMFStreamDescriptor> sourceSD, COMPtr<IMFTopologyNode>&);

    void onCreatedMediaSource();
    void onTopologySet();

    LPCWSTR registerVideoWindowClass();
    void createVideoWindow();
    void destroyVideoWindow();

    void invalidateFrameView();

    void addListener(MediaPlayerListener*);
    void removeListener(MediaPlayerListener*);
    void notifyDeleted();

    static LRESULT CALLBACK VideoViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    class MediaPlayerListener {
    public:
        MediaPlayerListener() { }
        virtual ~MediaPlayerListener() { }

        virtual void onMediaPlayerDeleted() { }
    };

    class AsyncCallback : public IMFAsyncCallback, public MediaPlayerListener {
    public:
        AsyncCallback(MediaPlayerPrivateMediaFoundation*, bool event);
        ~AsyncCallback();

        virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) override;
        virtual ULONG STDMETHODCALLTYPE AddRef() override;
        virtual ULONG STDMETHODCALLTYPE Release() override;

        virtual HRESULT STDMETHODCALLTYPE GetParameters(__RPC__out DWORD *pdwFlags, __RPC__out DWORD *pdwQueue) override;
        virtual HRESULT STDMETHODCALLTYPE Invoke(__RPC__in_opt IMFAsyncResult *pAsyncResult) override;

        virtual void onMediaPlayerDeleted() override;

    private:
        ULONG m_refCount;
        MediaPlayerPrivateMediaFoundation* m_mediaPlayer;
        bool m_event;
        Lock m_mutex;
    };

    typedef Deque<COMPtr<IMFSample>> VideoSampleList;

    class VideoSamplePool {
    public:
        VideoSamplePool() { }
        virtual ~VideoSamplePool() { }

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
    public:
        VideoScheduler() { }
        virtual ~VideoScheduler() { }

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
        Win32Handle m_schedulerThread;
        Win32Handle m_threadReadyEvent;
        Win32Handle m_flushEvent;

        float m_playbackRate { 1.0f };
        MFTIME m_frameDuration { 0 };
        MFTIME m_lastSampleTime { 0 };

        std::atomic<bool> m_exitThread { false };

        void stopThread() { m_exitThread = true; }
    };

    class Direct3DPresenter {
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
        HRESULT updateDestRect();

        HRESULT presentSwapChain(IDirect3DSwapChain9*, IDirect3DSurface9*);

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
    public:
        CustomVideoPresenter(MediaPlayerPrivateMediaFoundation*);
        ~CustomVideoPresenter();

        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) override;
        virtual ULONG STDMETHODCALLTYPE AddRef() override;
        virtual ULONG STDMETHODCALLTYPE Release() override;

        // IMFClockStateSink
        virtual HRESULT STDMETHODCALLTYPE OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) override;
        virtual HRESULT STDMETHODCALLTYPE OnClockStop(MFTIME hnsSystemTime) override;
        virtual HRESULT STDMETHODCALLTYPE OnClockPause(MFTIME hnsSystemTime) override;
        virtual HRESULT STDMETHODCALLTYPE OnClockRestart(MFTIME hnsSystemTime) override;
        virtual HRESULT STDMETHODCALLTYPE OnClockSetRate(MFTIME hnsSystemTime, float flRate) override;

        // IMFVideoPresenter
        virtual HRESULT STDMETHODCALLTYPE ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam) override;
        virtual HRESULT STDMETHODCALLTYPE GetCurrentMediaType(_Outptr_  IMFVideoMediaType **ppMediaType) override;

        // IMFVideoDeviceID
        virtual HRESULT STDMETHODCALLTYPE GetDeviceID(IID* pDeviceID) override;

        // IMFTopologyServiceLookupClient
        virtual HRESULT STDMETHODCALLTYPE InitServicePointers(_In_  IMFTopologyServiceLookup *pLookup) override;
        virtual HRESULT STDMETHODCALLTYPE ReleaseServicePointers(void) override;

        // IMFGetService
        virtual HRESULT STDMETHODCALLTYPE GetService(REFGUID guidService, REFIID riid, LPVOID *ppvObject);

        // IMFActivate
        virtual HRESULT STDMETHODCALLTYPE ActivateObject(REFIID riid, void **ppv);
        virtual HRESULT STDMETHODCALLTYPE DetachObject();
        virtual HRESULT STDMETHODCALLTYPE ShutdownObject();

        // IMFAttributes
        virtual HRESULT STDMETHODCALLTYPE GetItem(__RPC__in REFGUID guidKey, __RPC__inout_opt PROPVARIANT *pValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetItemType(__RPC__in REFGUID guidKey, __RPC__out MF_ATTRIBUTE_TYPE *pType) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE CompareItem(__RPC__in REFGUID guidKey, __RPC__in REFPROPVARIANT Value, __RPC__out BOOL *pbResult) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE Compare(__RPC__in_opt IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, __RPC__out BOOL *pbResult) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetUINT32(__RPC__in REFGUID guidKey, __RPC__out UINT32 *punValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetUINT64(__RPC__in REFGUID guidKey, __RPC__out UINT64 *punValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetDouble(__RPC__in REFGUID guidKey, __RPC__out double *pfValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetGUID(__RPC__in REFGUID guidKey, __RPC__out GUID *pguidValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetStringLength(__RPC__in REFGUID guidKey, __RPC__out UINT32 *pcchLength) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetString(__RPC__in REFGUID guidKey, __RPC__out_ecount_full(cchBufSize) LPWSTR pwszValue, UINT32 cchBufSize, __RPC__inout_opt UINT32 *pcchLength) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetAllocatedString(__RPC__in REFGUID guidKey, __RPC__deref_out_ecount_full_opt((*pcchLength + 1)) LPWSTR *ppwszValue, __RPC__out UINT32 *pcchLength) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetBlobSize(__RPC__in REFGUID guidKey, __RPC__out UINT32 *pcbBlobSize) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetBlob(__RPC__in REFGUID guidKey, __RPC__out_ecount_full(cbBufSize) UINT8 *pBuf, UINT32 cbBufSize, __RPC__inout_opt UINT32 *pcbBlobSize) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetAllocatedBlob(__RPC__in REFGUID guidKey, __RPC__deref_out_ecount_full_opt(*pcbSize) UINT8 **ppBuf, __RPC__out UINT32 *pcbSize) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetUnknown(__RPC__in REFGUID guidKey, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID *ppv) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetItem(__RPC__in REFGUID guidKey, __RPC__in REFPROPVARIANT Value) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE DeleteItem(__RPC__in REFGUID guidKey) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE DeleteAllItems(void) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetUINT32(__RPC__in REFGUID guidKey, UINT32 unValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetUINT64(__RPC__in REFGUID guidKey, UINT64 unValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetDouble(__RPC__in REFGUID guidKey, double fValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetGUID(__RPC__in REFGUID guidKey, __RPC__in REFGUID guidValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetString(__RPC__in REFGUID guidKey, __RPC__in_string LPCWSTR wszValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetBlob(__RPC__in REFGUID guidKey, __RPC__in_ecount_full(cbBufSize) const UINT8 *pBuf, UINT32 cbBufSize) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetUnknown(__RPC__in REFGUID guidKey, __RPC__in_opt IUnknown *pUnknown) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE LockStore(void) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE UnlockStore(void) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetCount(__RPC__out UINT32 *pcItems) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetItemByIndex(UINT32 unIndex, __RPC__out GUID *pguidKey, __RPC__inout_opt PROPVARIANT *pValue) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE CopyAllItems(__RPC__in_opt IMFAttributes *pDest) { return E_NOTIMPL; }

        // IMFVideoDisplayControl
        virtual HRESULT STDMETHODCALLTYPE GetNativeVideoSize(SIZE* pszVideo, SIZE* pszARVideo) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetIdealVideoSize(SIZE* pszMin, SIZE* pszMax) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetVideoPosition(const MFVideoNormalizedRect* pnrcSource, const LPRECT prcDest);
        virtual HRESULT STDMETHODCALLTYPE GetVideoPosition(MFVideoNormalizedRect* pnrcSource, LPRECT prcDest);
        virtual HRESULT STDMETHODCALLTYPE SetAspectRatioMode(DWORD dwAspectRatioMode) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetAspectRatioMode(DWORD* pdwAspectRatioMode) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetVideoWindow(HWND hwndVideo);
        virtual HRESULT STDMETHODCALLTYPE GetVideoWindow(HWND* phwndVideo);
        virtual HRESULT STDMETHODCALLTYPE RepaintVideo();
        virtual HRESULT STDMETHODCALLTYPE GetCurrentImage(BITMAPINFOHEADER* pBih, BYTE** pDib, DWORD* pcbDib, LONGLONG* pTimeStamp) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetBorderColor(COLORREF Clr) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetBorderColor(COLORREF* pClr) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetRenderingPrefs(DWORD dwRenderFlags) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetRenderingPrefs(DWORD* pdwRenderFlags) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE SetFullscreen(BOOL bFullscreen) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE GetFullscreen(BOOL* pbFullscreen) { return E_NOTIMPL; }

        // IMFAsyncCallback methods
        virtual HRESULT STDMETHODCALLTYPE GetParameters(DWORD*, DWORD*) { return E_NOTIMPL; }
        virtual HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult* pAsyncResult);

        // MediaPlayerListener
        virtual void onMediaPlayerDeleted() override;

        void paintCurrentFrame(GraphicsContext&, const FloatRect&);

        float currentTime();

        float maxTimeLoaded() const { return m_maxTimeLoaded; }

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
        float m_maxTimeLoaded { 0.0f };

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

}

#endif
