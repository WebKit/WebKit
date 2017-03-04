/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/windows/video_capture_ds.h"

#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/modules/video_capture/windows/help_functions_ds.h"
#include "webrtc/modules/video_capture/windows/sink_filter_ds.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"

#include <Dvdmedia.h> // VIDEOINFOHEADER2

namespace webrtc
{
namespace videocapturemodule
{
VideoCaptureDS::VideoCaptureDS()
    : _captureFilter(NULL),
      _graphBuilder(NULL), _mediaControl(NULL), _sinkFilter(NULL),
      _inputSendPin(NULL), _outputCapturePin(NULL), _dvFilter(NULL),
      _inputDvPin(NULL), _outputDvPin(NULL)
{
}

VideoCaptureDS::~VideoCaptureDS()
{
    if (_mediaControl)
    {
        _mediaControl->Stop();
    }
    if (_graphBuilder)
    {
        if (_sinkFilter)
            _graphBuilder->RemoveFilter(_sinkFilter);
        if (_captureFilter)
            _graphBuilder->RemoveFilter(_captureFilter);
        if (_dvFilter)
            _graphBuilder->RemoveFilter(_dvFilter);
    }
    RELEASE_AND_CLEAR(_inputSendPin);
    RELEASE_AND_CLEAR(_outputCapturePin);

    RELEASE_AND_CLEAR(_captureFilter); // release the capture device
    RELEASE_AND_CLEAR(_sinkFilter);
    RELEASE_AND_CLEAR(_dvFilter);

    RELEASE_AND_CLEAR(_mediaControl);

    RELEASE_AND_CLEAR(_inputDvPin);
    RELEASE_AND_CLEAR(_outputDvPin);

    RELEASE_AND_CLEAR(_graphBuilder);
}

int32_t VideoCaptureDS::Init(const char* deviceUniqueIdUTF8)
{
    const int32_t nameLength =
        (int32_t) strlen((char*) deviceUniqueIdUTF8);
    if (nameLength > kVideoCaptureUniqueNameLength)
        return -1;

    // Store the device name
    _deviceUniqueId = new (std::nothrow) char[nameLength + 1];
    memcpy(_deviceUniqueId, deviceUniqueIdUTF8, nameLength + 1);

    if (_dsInfo.Init() != 0)
        return -1;

    _captureFilter = _dsInfo.GetDeviceFilter(deviceUniqueIdUTF8);
    if (!_captureFilter)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to create capture filter.");
        return -1;
    }

    // Get the interface for DirectShow's GraphBuilder
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL,
                                  CLSCTX_INPROC_SERVER, IID_IGraphBuilder,
                                  (void **) &_graphBuilder);
    if (FAILED(hr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to create graph builder.");
        return -1;
    }

    hr = _graphBuilder->QueryInterface(IID_IMediaControl,
                                       (void **) &_mediaControl);
    if (FAILED(hr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to create media control builder.");
        return -1;
    }
    hr = _graphBuilder->AddFilter(_captureFilter, CAPTURE_FILTER_NAME);
    if (FAILED(hr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to add the capture device to the graph.");
        return -1;
    }

    _outputCapturePin = GetOutputPin(_captureFilter, PIN_CATEGORY_CAPTURE);

    // Create the sink filte used for receiving Captured frames.
    _sinkFilter = new CaptureSinkFilter(SINK_FILTER_NAME, NULL, &hr,
                                        *this);
    if (hr != S_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to create send filter");
        return -1;
    }
    _sinkFilter->AddRef();

    hr = _graphBuilder->AddFilter(_sinkFilter, SINK_FILTER_NAME);
    if (FAILED(hr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to add the send filter to the graph.");
        return -1;
    }
    _inputSendPin = GetInputPin(_sinkFilter);

    // Temporary connect here.
    // This is done so that no one else can use the capture device.
    if (SetCameraOutput(_requestedCapability) != 0)
    {
        return -1;
    }
    hr = _mediaControl->Pause();
    if (FAILED(hr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to Pause the Capture device. Is it already occupied? %d.",
                     hr);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideoCapture, 0,
                 "Capture device '%s' initialized.", deviceUniqueIdUTF8);
    return 0;
}

int32_t VideoCaptureDS::StartCapture(
                                      const VideoCaptureCapability& capability)
{
    CriticalSectionScoped cs(&_apiCs);

    if (capability != _requestedCapability)
    {
        DisconnectGraph();

        if (SetCameraOutput(capability) != 0)
        {
            return -1;
        }
    }
    HRESULT hr = _mediaControl->Run();
    if (FAILED(hr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to start the Capture device.");
        return -1;
    }
    return 0;
}

int32_t VideoCaptureDS::StopCapture()
{
    CriticalSectionScoped cs(&_apiCs);

    HRESULT hr = _mediaControl->Pause();
    if (FAILED(hr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to stop the capture graph. %d", hr);
        return -1;
    }
    return 0;
}
bool VideoCaptureDS::CaptureStarted()
{
    OAFilterState state = 0;
    HRESULT hr = _mediaControl->GetState(1000, &state);
    if (hr != S_OK && hr != VFW_S_CANT_CUE)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to get the CaptureStarted status");
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                 "CaptureStarted %d", state);
    return state == State_Running;

}
int32_t VideoCaptureDS::CaptureSettings(
                                             VideoCaptureCapability& settings)
{
    settings = _requestedCapability;
    return 0;
}

int32_t VideoCaptureDS::SetCameraOutput(
                             const VideoCaptureCapability& requestedCapability)
{

    // Get the best matching capability
    VideoCaptureCapability capability;
    int32_t capabilityIndex;

    // Store the new requested size
    _requestedCapability = requestedCapability;
    // Match the requested capability with the supported.
    if ((capabilityIndex = _dsInfo.GetBestMatchedCapability(_deviceUniqueId,
                                                            _requestedCapability,
                                                            capability)) < 0)
    {
        return -1;
    }
    //Reduce the frame rate if possible.
    if (capability.maxFPS > requestedCapability.maxFPS)
    {
        capability.maxFPS = requestedCapability.maxFPS;
    } else if (capability.maxFPS <= 0)
    {
        capability.maxFPS = 30;
    }

    // Convert it to the windows capability index since they are not nexessary
    // the same
    VideoCaptureCapabilityWindows windowsCapability;
    if (_dsInfo.GetWindowsCapability(capabilityIndex, windowsCapability) != 0)
    {
        return -1;
    }

    IAMStreamConfig* streamConfig = NULL;
    AM_MEDIA_TYPE *pmt = NULL;
    VIDEO_STREAM_CONFIG_CAPS caps;

    HRESULT hr = _outputCapturePin->QueryInterface(IID_IAMStreamConfig,
                                                   (void**) &streamConfig);
    if (hr)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Can't get the Capture format settings.");
        return -1;
    }

    //Get the windows capability from the capture device
    bool isDVCamera = false;
    hr = streamConfig->GetStreamCaps(
                                    windowsCapability.directShowCapabilityIndex,
                                    &pmt, reinterpret_cast<BYTE*> (&caps));
    if (!FAILED(hr))
    {
        if (pmt->formattype == FORMAT_VideoInfo2)
        {
            VIDEOINFOHEADER2* h =
                reinterpret_cast<VIDEOINFOHEADER2*> (pmt->pbFormat);
            if (capability.maxFPS > 0
                && windowsCapability.supportFrameRateControl)
            {
                h->AvgTimePerFrame = REFERENCE_TIME(10000000.0
                                                    / capability.maxFPS);
            }
        }
        else
        {
            VIDEOINFOHEADER* h = reinterpret_cast<VIDEOINFOHEADER*>
                                (pmt->pbFormat);
            if (capability.maxFPS > 0
                && windowsCapability.supportFrameRateControl)
            {
                h->AvgTimePerFrame = REFERENCE_TIME(10000000.0
                                                    / capability.maxFPS);
            }

        }

        // Set the sink filter to request this capability
        _sinkFilter->SetMatchingMediaType(capability);
        //Order the capture device to use this capability
        hr += streamConfig->SetFormat(pmt);

        //Check if this is a DV camera and we need to add MS DV Filter
        if (pmt->subtype == MEDIASUBTYPE_dvsl
           || pmt->subtype == MEDIASUBTYPE_dvsd
           || pmt->subtype == MEDIASUBTYPE_dvhd)
            isDVCamera = true; // This is a DV camera. Use MS DV filter
    }
    RELEASE_AND_CLEAR(streamConfig);

    if (FAILED(hr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to set capture device output format");
        return -1;
    }

    if (isDVCamera)
    {
        hr = ConnectDVCamera();
    }
    else
    {
        hr = _graphBuilder->ConnectDirect(_outputCapturePin, _inputSendPin,
                                          NULL);
    }
    if (hr != S_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to connect the Capture graph %d", hr);
        return -1;
    }
    return 0;
}

int32_t VideoCaptureDS::DisconnectGraph()
{
    HRESULT hr = _mediaControl->Stop();
    hr += _graphBuilder->Disconnect(_outputCapturePin);
    hr += _graphBuilder->Disconnect(_inputSendPin);

    //if the DV camera filter exist
    if (_dvFilter)
    {
        _graphBuilder->Disconnect(_inputDvPin);
        _graphBuilder->Disconnect(_outputDvPin);
    }
    if (hr != S_OK)
    {
        WEBRTC_TRACE( webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to Stop the Capture device for reconfiguration %d",
                     hr);
        return -1;
    }
    return 0;
}
HRESULT VideoCaptureDS::ConnectDVCamera()
{
    HRESULT hr = S_OK;

    if (!_dvFilter)
    {
        hr = CoCreateInstance(CLSID_DVVideoCodec, NULL, CLSCTX_INPROC,
                              IID_IBaseFilter, (void **) &_dvFilter);
        if (hr != S_OK)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                         "Failed to create the dv decoder: %x", hr);
            return hr;
        }
        hr = _graphBuilder->AddFilter(_dvFilter, L"VideoDecoderDV");
        if (hr != S_OK)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                         "Failed to add the dv decoder to the graph: %x", hr);
            return hr;
        }
        _inputDvPin = GetInputPin(_dvFilter);
        if (_inputDvPin == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                         "Failed to get input pin from DV decoder");
            return -1;
        }
        _outputDvPin = GetOutputPin(_dvFilter, GUID_NULL);
        if (_outputDvPin == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                         "Failed to get output pin from DV decoder");
            return -1;
        }
    }
    hr = _graphBuilder->ConnectDirect(_outputCapturePin, _inputDvPin, NULL);
    if (hr != S_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to connect capture device to the dv devoder: %x",
                     hr);
        return hr;
    }

    hr = _graphBuilder->ConnectDirect(_outputDvPin, _inputSendPin, NULL);
    if (hr != S_OK)
    {
        if (hr == HRESULT_FROM_WIN32(ERROR_TOO_MANY_OPEN_FILES))
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                         "Failed to connect the capture device, busy");
        }
        else
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                         "Failed to connect capture device to the send graph: 0x%x",
                         hr);
        }
        return hr;
    }
    return hr;
}
}  // namespace videocapturemodule
}  // namespace webrtc
