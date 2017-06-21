/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/windows/sink_filter_ds.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/modules/video_capture/windows/help_functions_ds.h"
#include "webrtc/system_wrappers/include/trace.h"

#include <Dvdmedia.h> // VIDEOINFOHEADER2
#include <initguid.h>

#define DELETE_RESET(p) { delete (p) ; (p) = NULL ;}

DEFINE_GUID(CLSID_SINKFILTER, 0x88cdbbdc, 0xa73b, 0x4afa, 0xac, 0xbf, 0x15, 0xd5,
            0xe2, 0xce, 0x12, 0xc3);

namespace webrtc
{
namespace videocapturemodule
{

typedef struct tagTHREADNAME_INFO
{
   DWORD dwType;        // must be 0x1000
   LPCSTR szName;       // pointer to name (in user addr space)
   DWORD dwThreadID;    // thread ID (-1=caller thread)
   DWORD dwFlags;       // reserved for future use, must be zero
} THREADNAME_INFO;

CaptureInputPin::CaptureInputPin (IN TCHAR * szName,
                            IN CaptureSinkFilter* pFilter,
                            IN CCritSec * pLock,
                            OUT HRESULT * pHr,
                            IN LPCWSTR pszName)
    : CBaseInputPin (szName, pFilter, pLock, pHr, pszName),
      _requestedCapability(),
      _resultingCapability()
{
    _threadHandle = NULL;
}

CaptureInputPin::~CaptureInputPin()
{
}

HRESULT
CaptureInputPin::GetMediaType (IN int iPosition, OUT CMediaType * pmt)
{
    // reset the thread handle
    _threadHandle = NULL;

    if(iPosition < 0)
    return E_INVALIDARG;

    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*) pmt->AllocFormatBuffer(
                            sizeof(VIDEOINFOHEADER));
    if(NULL == pvi)
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                     "CheckMediaType VIDEOINFOHEADER is NULL. Returning...Line:%d\n", __LINE__);
        return(E_OUTOFMEMORY);
    }

    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biClrImportant = 0;
    pvi->bmiHeader.biClrUsed = 0;
    if (_requestedCapability.maxFPS != 0) {
        pvi->AvgTimePerFrame = 10000000/_requestedCapability.maxFPS;
    }

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    int32_t positionOffset=1;
    switch (iPosition+positionOffset)
    {
        case 0:
        {
            pvi->bmiHeader.biCompression = MAKEFOURCC('I','4','2','0');
            pvi->bmiHeader.biBitCount = 12; //bit per pixel
            pvi->bmiHeader.biWidth = _requestedCapability.width;
            pvi->bmiHeader.biHeight = _requestedCapability.height;
            pvi->bmiHeader.biSizeImage = 3*_requestedCapability.height
                                        *_requestedCapability.width/2;
            pmt->SetSubtype(&MEDIASUBTYPE_I420);
        }
        break;
        case 1:
        {
            pvi->bmiHeader.biCompression = MAKEFOURCC('Y','U','Y','2');;
            pvi->bmiHeader.biBitCount = 16; //bit per pixel
            pvi->bmiHeader.biWidth = _requestedCapability.width;
            pvi->bmiHeader.biHeight = _requestedCapability.height;
            pvi->bmiHeader.biSizeImage = 2*_requestedCapability.width
                                        *_requestedCapability.height;
            pmt->SetSubtype(&MEDIASUBTYPE_YUY2);
        }
        break;
        case 2:
        {
            pvi->bmiHeader.biCompression = BI_RGB;
            pvi->bmiHeader.biBitCount = 24; //bit per pixel
            pvi->bmiHeader.biWidth = _requestedCapability.width;
            pvi->bmiHeader.biHeight = _requestedCapability.height;
            pvi->bmiHeader.biSizeImage = 3*_requestedCapability.height
                                        *_requestedCapability.width;
            pmt->SetSubtype(&MEDIASUBTYPE_RGB24);
        }
        break;
        case 3:
        {
            pvi->bmiHeader.biCompression = MAKEFOURCC('U','Y','V','Y');
            pvi->bmiHeader.biBitCount = 16; //bit per pixel
            pvi->bmiHeader.biWidth = _requestedCapability.width;
            pvi->bmiHeader.biHeight = _requestedCapability.height;
            pvi->bmiHeader.biSizeImage = 2*_requestedCapability.height
                                         *_requestedCapability.width;
            pmt->SetSubtype(&MEDIASUBTYPE_UYVY);
        }
        break;
        case 4:
        {
            pvi->bmiHeader.biCompression = MAKEFOURCC('M','J','P','G');
            pvi->bmiHeader.biBitCount = 12; //bit per pixel
            pvi->bmiHeader.biWidth = _requestedCapability.width;
            pvi->bmiHeader.biHeight = _requestedCapability.height;
            pvi->bmiHeader.biSizeImage = 3*_requestedCapability.height
                                         *_requestedCapability.width/2;
            pmt->SetSubtype(&MEDIASUBTYPE_MJPG);
        }
        break;
        default :
        return VFW_S_NO_MORE_ITEMS;
    }
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
             "GetMediaType position %d, width %d, height %d, biCompression 0x%x",
             iPosition, _requestedCapability.width,
             _requestedCapability.height,pvi->bmiHeader.biCompression);
    return NOERROR;
}

HRESULT
CaptureInputPin::CheckMediaType ( IN const CMediaType * pMediaType)
{
    // reset the thread handle
    _threadHandle = NULL;

    const GUID *type = pMediaType->Type();
    if (*type != MEDIATYPE_Video)
    return E_INVALIDARG;

    const GUID *formatType = pMediaType->FormatType();

    // Check for the subtypes we support
    const GUID *SubType = pMediaType->Subtype();
    if (SubType == NULL)
    {
        return E_INVALIDARG;
    }

    if(*formatType == FORMAT_VideoInfo)
    {
        VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) pMediaType->Format();
        if(pvi == NULL)
        {
            return E_INVALIDARG;
        }

        // Store the incoming width and height
        _resultingCapability.width = pvi->bmiHeader.biWidth;

        // Store the incoming height,
        // for RGB24 we assume the frame to be upside down
        if(*SubType == MEDIASUBTYPE_RGB24
            && pvi->bmiHeader.biHeight > 0)
        {
           _resultingCapability.height = -(pvi->bmiHeader.biHeight);
        }
        else
        {
           _resultingCapability.height = abs(pvi->bmiHeader.biHeight);
        }

        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                     "CheckMediaType width:%d height:%d Compression:0x%x\n",
                     pvi->bmiHeader.biWidth,pvi->bmiHeader.biHeight,
                     pvi->bmiHeader.biCompression);

        if(*SubType == MEDIASUBTYPE_MJPG
            && pvi->bmiHeader.biCompression == MAKEFOURCC('M','J','P','G'))
        {
          _resultingCapability.videoType = VideoType::kMJPEG;
          return S_OK;  // This format is acceptable.
        }
        if(*SubType == MEDIASUBTYPE_I420
            && pvi->bmiHeader.biCompression == MAKEFOURCC('I','4','2','0'))
        {
          _resultingCapability.videoType = VideoType::kI420;
          return S_OK;  // This format is acceptable.
        }
        if(*SubType == MEDIASUBTYPE_YUY2
            && pvi->bmiHeader.biCompression == MAKEFOURCC('Y','U','Y','2'))
        {
          _resultingCapability.videoType = VideoType::kYUY2;
          ::Sleep(60);  // workaround for bad driver
          return S_OK;  // This format is acceptable.
        }
        if(*SubType == MEDIASUBTYPE_UYVY
            && pvi->bmiHeader.biCompression == MAKEFOURCC('U','Y','V','Y'))
        {
          _resultingCapability.videoType = VideoType::kUYVY;
          return S_OK;  // This format is acceptable.
        }

        if(*SubType == MEDIASUBTYPE_HDYC)
        {
          _resultingCapability.videoType = VideoType::kUYVY;
          return S_OK;  // This format is acceptable.
        }
        if(*SubType == MEDIASUBTYPE_RGB24
            && pvi->bmiHeader.biCompression == BI_RGB)
        {
          _resultingCapability.videoType = VideoType::kRGB24;
          return S_OK;  // This format is acceptable.
        }
    }
    if(*formatType == FORMAT_VideoInfo2)
    {
        // VIDEOINFOHEADER2 that has dwInterlaceFlags
        VIDEOINFOHEADER2 *pvi = (VIDEOINFOHEADER2 *) pMediaType->Format();

        if(pvi == NULL)
        {
            return E_INVALIDARG;
        }

        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                     "CheckMediaType width:%d height:%d Compression:0x%x\n",
                     pvi->bmiHeader.biWidth,pvi->bmiHeader.biHeight,
                     pvi->bmiHeader.biCompression);

        _resultingCapability.width = pvi->bmiHeader.biWidth;

        // Store the incoming height,
        // for RGB24 we assume the frame to be upside down
        if(*SubType == MEDIASUBTYPE_RGB24
            && pvi->bmiHeader.biHeight > 0)
        {
           _resultingCapability.height = -(pvi->bmiHeader.biHeight);
        }
        else
        {
           _resultingCapability.height = abs(pvi->bmiHeader.biHeight);
        }

        if(*SubType == MEDIASUBTYPE_MJPG
            && pvi->bmiHeader.biCompression == MAKEFOURCC('M','J','P','G'))
        {
          _resultingCapability.videoType = VideoType::kMJPEG;
          return S_OK;  // This format is acceptable.
        }
        if(*SubType == MEDIASUBTYPE_I420
            && pvi->bmiHeader.biCompression == MAKEFOURCC('I','4','2','0'))
        {
          _resultingCapability.videoType = VideoType::kI420;
          return S_OK;  // This format is acceptable.
        }
        if(*SubType == MEDIASUBTYPE_YUY2
            && pvi->bmiHeader.biCompression == MAKEFOURCC('Y','U','Y','2'))
        {
          _resultingCapability.videoType = VideoType::kYUY2;
          return S_OK;  // This format is acceptable.
        }
        if(*SubType == MEDIASUBTYPE_UYVY
            && pvi->bmiHeader.biCompression == MAKEFOURCC('U','Y','V','Y'))
        {
          _resultingCapability.videoType = VideoType::kUYVY;
          return S_OK;  // This format is acceptable.
        }

        if(*SubType == MEDIASUBTYPE_HDYC)
        {
          _resultingCapability.videoType = VideoType::kUYVY;
          return S_OK;  // This format is acceptable.
        }
        if(*SubType == MEDIASUBTYPE_RGB24
            && pvi->bmiHeader.biCompression == BI_RGB)
        {
          _resultingCapability.videoType = VideoType::kRGB24;
          return S_OK;  // This format is acceptable.
        }
    }
    return E_INVALIDARG;
}

HRESULT
CaptureInputPin::Receive ( IN IMediaSample * pIMediaSample )
{
    HRESULT hr = S_OK;

    RTC_DCHECK(m_pFilter);
    RTC_DCHECK(pIMediaSample);

    // get the thread handle of the delivering thread inc its priority
    if( _threadHandle == NULL)
    {
        HANDLE handle= GetCurrentThread();
        SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
        _threadHandle = handle;

        rtc::SetCurrentThreadName("webrtc_video_capture");
    }

    reinterpret_cast <CaptureSinkFilter *>(m_pFilter)->LockReceive();
    hr = CBaseInputPin::Receive (pIMediaSample);

    if (SUCCEEDED (hr))
    {
        const LONG length = pIMediaSample->GetActualDataLength();
        RTC_DCHECK(length >= 0);

        unsigned char* pBuffer = NULL;
        if(S_OK != pIMediaSample->GetPointer(&pBuffer))
        {
            reinterpret_cast <CaptureSinkFilter *>(m_pFilter)->UnlockReceive();
            return S_FALSE;
        }

        // NOTE: filter unlocked within Send call
        reinterpret_cast <CaptureSinkFilter *> (m_pFilter)->ProcessCapturedFrame(
            pBuffer, static_cast<size_t>(length), _resultingCapability);
    }
    else
    {
        reinterpret_cast <CaptureSinkFilter *>(m_pFilter)->UnlockReceive();
    }

    return hr;
}

// called under LockReceive
HRESULT CaptureInputPin::SetMatchingMediaType(
                                    const VideoCaptureCapability& capability)
{

    _requestedCapability = capability;
    _resultingCapability = VideoCaptureCapability();
    return S_OK;
}
//  ----------------------------------------------------------------------------
CaptureSinkFilter::CaptureSinkFilter (IN TCHAR * tszName,
                              IN LPUNKNOWN punk,
                              OUT HRESULT * phr,
                              VideoCaptureExternal& captureObserver)
    : CBaseFilter(tszName,punk,& m_crtFilter,CLSID_SINKFILTER),
      m_pInput(NULL),
      _captureObserver(captureObserver)
{
    (* phr) = S_OK;
    m_pInput = new CaptureInputPin(NAME ("VideoCaptureInputPin"),
                                   this,
                                   & m_crtFilter,
                                   phr, L"VideoCapture");
    if (m_pInput == NULL || FAILED (* phr))
    {
        (* phr) = FAILED (* phr) ? (* phr) : E_OUTOFMEMORY;
        goto cleanup;
    }
    cleanup :
    return;
}

CaptureSinkFilter::~CaptureSinkFilter()
{
    delete m_pInput;
}

int CaptureSinkFilter::GetPinCount()
{
    return 1;
}

CBasePin *
CaptureSinkFilter::GetPin(IN int Index)
{
    CBasePin * pPin;
    LockFilter ();
    if (Index == 0)
    {
        pPin = m_pInput;
    }
    else
    {
        pPin = NULL;
    }
    UnlockFilter ();
    return pPin;
}

STDMETHODIMP CaptureSinkFilter::Pause()
{
    LockReceive();
    LockFilter();
    if (m_State == State_Stopped)
    {
        //  change the state, THEN activate the input pin
        m_State = State_Paused;
        if (m_pInput && m_pInput->IsConnected())
        {
            m_pInput->Active();
        }
        if (m_pInput && !m_pInput->IsConnected())
        {
            m_State = State_Running;
        }
    }
    else if (m_State == State_Running)
    {
        m_State = State_Paused;
    }
    UnlockFilter();
    UnlockReceive();
    return S_OK;
}

STDMETHODIMP CaptureSinkFilter::Stop()
{
    LockReceive();
    LockFilter();

    //  set the state
    m_State = State_Stopped;

    //  inactivate the pins
    if (m_pInput)
        m_pInput->Inactive();

    UnlockFilter();
    UnlockReceive();
    return S_OK;
}

void CaptureSinkFilter::SetFilterGraph(IGraphBuilder* graph)
{
    LockFilter();
    m_pGraph = graph;
    UnlockFilter();
}

void CaptureSinkFilter::ProcessCapturedFrame(
    unsigned char* pBuffer,
    size_t length,
    const VideoCaptureCapability& frameInfo)
{
    //  we have the receiver lock
    if (m_State == State_Running)
    {
        _captureObserver.IncomingFrame(pBuffer, length, frameInfo);

        // trying to hold it since it's only a memcpy
        // IMPROVEMENT if this work move critsect
        UnlockReceive();
        return;
    }
    UnlockReceive();
    return;
}

STDMETHODIMP CaptureSinkFilter::SetMatchingMediaType(
                                        const VideoCaptureCapability& capability)
{
    LockReceive();
    LockFilter();
    HRESULT hr;
    if (m_pInput)
    {
        hr = m_pInput->SetMatchingMediaType(capability);
    }
    else
    {
        hr = E_UNEXPECTED;
    }
    UnlockFilter();
    UnlockReceive();
    return hr;
}

STDMETHODIMP CaptureSinkFilter::GetClassID( OUT CLSID * pCLSID )
{
    (* pCLSID) = CLSID_SINKFILTER;
    return S_OK;
}

}  // namespace videocapturemodule
}  // namespace webrtc
