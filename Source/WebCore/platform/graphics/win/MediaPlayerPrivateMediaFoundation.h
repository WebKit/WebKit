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

#include "COMPtr.h"
#include "MediaPlayerPrivate.h"

#include <Mfapi.h>
#include <Mfidl.h>
#include <evr.h>

#include <wtf/ThreadingPrimitives.h>

namespace WebCore {

class MediaPlayerPrivateMediaFoundation : public MediaPlayerPrivateInterface {
public:
    explicit MediaPlayerPrivateMediaFoundation(MediaPlayer*);
    ~MediaPlayerPrivateMediaFoundation();
    static void registerMediaEngine(MediaEngineRegistrar);

    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    static bool isAvailable();

    virtual void load(const String& url);
    virtual void cancelLoad();

    virtual void play();
    virtual void pause();

    virtual FloatSize naturalSize() const;

    virtual bool hasVideo() const;
    virtual bool hasAudio() const;

    virtual void setVisible(bool);

    virtual bool seeking() const;
    virtual void seekDouble(double) override;
    virtual double durationDouble() const override;

    virtual bool paused() const;

    virtual MediaPlayer::NetworkState networkState() const;
    virtual MediaPlayer::ReadyState readyState() const;

    virtual std::unique_ptr<PlatformTimeRanges> buffered() const;

    virtual bool didLoadingProgress() const;

    virtual void setSize(const IntSize&);

    virtual void paint(GraphicsContext*, const FloatRect&);

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
    Mutex m_mutexListeners;

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
    static void onCreatedMediaSourceCallback(void* context);
    static void onTopologySetCallback(void* context);

    LPCWSTR registerVideoWindowClass();
    void createVideoWindow();
    void destroyVideoWindow();

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

        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) override;
        virtual ULONG STDMETHODCALLTYPE AddRef() override;
        virtual ULONG STDMETHODCALLTYPE Release() override;

        virtual HRESULT STDMETHODCALLTYPE GetParameters(__RPC__out DWORD *pdwFlags, __RPC__out DWORD *pdwQueue) override;
        virtual HRESULT STDMETHODCALLTYPE Invoke(__RPC__in_opt IMFAsyncResult *pAsyncResult) override;

        virtual void onMediaPlayerDeleted() override;

    private:
        ULONG m_refCount;
        MediaPlayerPrivateMediaFoundation* m_mediaPlayer;
        bool m_event;
        Mutex m_mutex;
    };

};

}
