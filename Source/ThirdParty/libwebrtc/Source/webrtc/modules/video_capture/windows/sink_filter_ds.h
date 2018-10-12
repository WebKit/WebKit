/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_SINK_FILTER_DS_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_SINK_FILTER_DS_H_

#include <streams.h>  // Include base DS filter header files

#include "modules/video_capture/video_capture_defines.h"

namespace webrtc {
namespace videocapturemodule {
// forward declaration

class CaptureSinkFilter;
/**
 *     input pin for camera input
 *
 */
class CaptureInputPin : public CBaseInputPin {
 public:
  VideoCaptureCapability _requestedCapability;
  VideoCaptureCapability _resultingCapability;
  HANDLE _threadHandle;

  CaptureInputPin(IN TCHAR* szName,
                  IN CaptureSinkFilter* pFilter,
                  IN CCritSec* pLock,
                  OUT HRESULT* pHr,
                  IN LPCWSTR pszName);
  ~CaptureInputPin() override;

  HRESULT GetMediaType(IN int iPos, OUT CMediaType* pmt) override;
  HRESULT CheckMediaType(IN const CMediaType* pmt) override;
  STDMETHODIMP Receive(IN IMediaSample*) override;
  HRESULT SetMatchingMediaType(const VideoCaptureCapability& capability);
};

class CaptureSinkFilter : public CBaseFilter {
 public:
  CaptureSinkFilter(const IN TCHAR* tszName,
                    IN LPUNKNOWN punk,
                    OUT HRESULT* phr,
                    VideoCaptureExternal& captureObserver);
  ~CaptureSinkFilter() override;

  //  --------------------------------------------------------------------
  //  class methods

  void ProcessCapturedFrame(unsigned char* pBuffer,
                            size_t length,
                            const VideoCaptureCapability& frameInfo);
  //  explicit receiver lock aquisition and release
  void LockReceive() { m_crtRecv.Lock(); }
  void UnlockReceive() { m_crtRecv.Unlock(); }
  //  explicit filter lock aquisition and release
  void LockFilter() { m_crtFilter.Lock(); }
  void UnlockFilter() { m_crtFilter.Unlock(); }
  void SetFilterGraph(IGraphBuilder* graph);  // Used if EVR

  //  --------------------------------------------------------------------
  //  COM interfaces
  DECLARE_IUNKNOWN;
  STDMETHODIMP SetMatchingMediaType(const VideoCaptureCapability& capability);

  //  --------------------------------------------------------------------
  //  CBaseFilter methods
  int GetPinCount() override;
  CBasePin* GetPin(IN int Index) override;
  STDMETHODIMP Pause() override;
  STDMETHODIMP Stop() override;
  STDMETHODIMP GetClassID(OUT CLSID* pCLSID) override;
  //  --------------------------------------------------------------------
  //  class factory calls this
  static CUnknown* CreateInstance(IN LPUNKNOWN punk, OUT HRESULT* phr);

 private:
  CCritSec m_crtFilter;  //  filter lock
  CCritSec m_crtRecv;    //  receiver lock; always acquire before filter lock
  CaptureInputPin* m_pInput;
  VideoCaptureExternal& _captureObserver;
};
}  // namespace videocapturemodule
}  // namespace webrtc
#endif  // MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_SINK_FILTER_DS_H_
