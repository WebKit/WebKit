/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/windows/device_info_ds.h"

#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/modules/video_capture/windows/help_functions_ds.h"
#include "webrtc/system_wrappers/include/trace.h"

#include <Dvdmedia.h>
#include <Streams.h>

namespace webrtc
{
namespace videocapturemodule
{

// static
DeviceInfoDS* DeviceInfoDS::Create()
{
    DeviceInfoDS* dsInfo = new DeviceInfoDS();
    if (!dsInfo || dsInfo->Init() != 0)
    {
        delete dsInfo;
        dsInfo = NULL;
    }
    return dsInfo;
}

DeviceInfoDS::DeviceInfoDS()
    : _dsDevEnum(NULL), _dsMonikerDevEnum(NULL),
      _CoUninitializeIsRequired(true)
{
    // 1) Initialize the COM library (make Windows load the DLLs).
    //
    // CoInitializeEx must be called at least once, and is usually called only once,
    // for each thread that uses the COM library. Multiple calls to CoInitializeEx
    // by the same thread are allowed as long as they pass the same concurrency flag,
    // but subsequent valid calls return S_FALSE.
    // To close the COM library gracefully on a thread, each successful call to
    // CoInitializeEx, including any call that returns S_FALSE, must be balanced
    // by a corresponding call to CoUninitialize.
    //

    /*Apartment-threading, while allowing for multiple threads of execution,
     serializes all incoming calls by requiring that calls to methods of objects created by this thread always run on the same thread
     the apartment/thread that created them. In addition, calls can arrive only at message-queue boundaries (i.e., only during a
     PeekMessage, SendMessage, DispatchMessage, etc.). Because of this serialization, it is not typically necessary to write concurrency control into
     the code for the object, other than to avoid calls to PeekMessage and SendMessage during processing that must not be interrupted by other method
     invocations or calls to other objects in the same apartment/thread.*/

    ///CoInitializeEx(NULL, COINIT_APARTMENTTHREADED ); //| COINIT_SPEED_OVER_MEMORY
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED); // Use COINIT_MULTITHREADED since Voice Engine uses COINIT_MULTITHREADED
    if (FAILED(hr))
    {
        // Avoid calling CoUninitialize() since CoInitializeEx() failed.
        _CoUninitializeIsRequired = FALSE;

        if (hr == RPC_E_CHANGED_MODE)
        {
            // Calling thread has already initialized COM to be used in a single-threaded
            // apartment (STA). We are then prevented from using STA.
            // Details: hr = 0x80010106 <=> "Cannot change thread mode after it is set".
            //
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, 0,
                         "VideoCaptureWindowsDSInfo::VideoCaptureWindowsDSInfo "
                         "CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) => "
                         "RPC_E_CHANGED_MODE, error 0x%x",
                         hr);
        }
    }
}

DeviceInfoDS::~DeviceInfoDS()
{
    RELEASE_AND_CLEAR(_dsMonikerDevEnum);
    RELEASE_AND_CLEAR(_dsDevEnum);
    if (_CoUninitializeIsRequired)
    {
        CoUninitialize();
    }
}

int32_t DeviceInfoDS::Init()
{
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
                                  IID_ICreateDevEnum, (void **) &_dsDevEnum);
    if (hr != NOERROR)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to create CLSID_SystemDeviceEnum, error 0x%x", hr);
        return -1;
    }
    return 0;
}
uint32_t DeviceInfoDS::NumberOfDevices()
{
    ReadLockScoped cs(_apiLock);
    return GetDeviceInfo(0, 0, 0, 0, 0, 0, 0);
}

int32_t DeviceInfoDS::GetDeviceName(
                                       uint32_t deviceNumber,
                                       char* deviceNameUTF8,
                                       uint32_t deviceNameLength,
                                       char* deviceUniqueIdUTF8,
                                       uint32_t deviceUniqueIdUTF8Length,
                                       char* productUniqueIdUTF8,
                                       uint32_t productUniqueIdUTF8Length)
{
    ReadLockScoped cs(_apiLock);
    const int32_t result = GetDeviceInfo(deviceNumber, deviceNameUTF8,
                                         deviceNameLength,
                                         deviceUniqueIdUTF8,
                                         deviceUniqueIdUTF8Length,
                                         productUniqueIdUTF8,
                                         productUniqueIdUTF8Length);
    return result > (int32_t) deviceNumber ? 0 : -1;
}

int32_t DeviceInfoDS::GetDeviceInfo(
                                       uint32_t deviceNumber,
                                       char* deviceNameUTF8,
                                       uint32_t deviceNameLength,
                                       char* deviceUniqueIdUTF8,
                                       uint32_t deviceUniqueIdUTF8Length,
                                       char* productUniqueIdUTF8,
                                       uint32_t productUniqueIdUTF8Length)

{

    // enumerate all video capture devices
    RELEASE_AND_CLEAR(_dsMonikerDevEnum);
    HRESULT hr =
        _dsDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                          &_dsMonikerDevEnum, 0);
    if (hr != NOERROR)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to enumerate CLSID_SystemDeviceEnum, error 0x%x."
                     " No webcam exist?", hr);
        return 0;
    }

    _dsMonikerDevEnum->Reset();
    ULONG cFetched;
    IMoniker *pM;
    int index = 0;
    while (S_OK == _dsMonikerDevEnum->Next(1, &pM, &cFetched))
    {
        IPropertyBag *pBag;
        hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void **) &pBag);
        if (S_OK == hr)
        {
            // Find the description or friendly name.
            VARIANT varName;
            VariantInit(&varName);
            hr = pBag->Read(L"Description", &varName, 0);
            if (FAILED(hr))
            {
                hr = pBag->Read(L"FriendlyName", &varName, 0);
            }
            if (SUCCEEDED(hr))
            {
                // ignore all VFW drivers
                if ((wcsstr(varName.bstrVal, (L"(VFW)")) == NULL) &&
                    (_wcsnicmp(varName.bstrVal, (L"Google Camera Adapter"),21)
                        != 0))
                {
                    // Found a valid device.
                    if (index == static_cast<int>(deviceNumber))
                    {
                        int convResult = 0;
                        if (deviceNameLength > 0)
                        {
                            convResult = WideCharToMultiByte(CP_UTF8, 0,
                                                             varName.bstrVal, -1,
                                                             (char*) deviceNameUTF8,
                                                             deviceNameLength, NULL,
                                                             NULL);
                            if (convResult == 0)
                            {
                                WEBRTC_TRACE(webrtc::kTraceError,
                                             webrtc::kTraceVideoCapture, 0,
                                             "Failed to convert device name to UTF8. %d",
                                             GetLastError());
                                return -1;
                            }
                        }
                        if (deviceUniqueIdUTF8Length > 0)
                        {
                            hr = pBag->Read(L"DevicePath", &varName, 0);
                            if (FAILED(hr))
                            {
                                strncpy_s((char *) deviceUniqueIdUTF8,
                                          deviceUniqueIdUTF8Length,
                                          (char *) deviceNameUTF8, convResult);
                                WEBRTC_TRACE(webrtc::kTraceError,
                                             webrtc::kTraceVideoCapture, 0,
                                             "Failed to get deviceUniqueIdUTF8 using deviceNameUTF8");
                            }
                            else
                            {
                                convResult = WideCharToMultiByte(
                                                          CP_UTF8,
                                                          0,
                                                          varName.bstrVal,
                                                          -1,
                                                          (char*) deviceUniqueIdUTF8,
                                                          deviceUniqueIdUTF8Length,
                                                          NULL, NULL);
                                if (convResult == 0)
                                {
                                    WEBRTC_TRACE(webrtc::kTraceError,
                                                 webrtc::kTraceVideoCapture, 0,
                                                 "Failed to convert device name to UTF8. %d",
                                                 GetLastError());
                                    return -1;
                                }
                                if (productUniqueIdUTF8
                                    && productUniqueIdUTF8Length > 0)
                                {
                                    GetProductId(deviceUniqueIdUTF8,
                                                 productUniqueIdUTF8,
                                                 productUniqueIdUTF8Length);
                                }
                            }
                        }

                    }
                    ++index; // increase the number of valid devices
                }
            }
            VariantClear(&varName);
            pBag->Release();
            pM->Release();
        }

    }
    if (deviceNameLength)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug,
                     webrtc::kTraceVideoCapture, 0, "%s %s",
                     __FUNCTION__, deviceNameUTF8);
    }
    return index;
}

IBaseFilter * DeviceInfoDS::GetDeviceFilter(
                                     const char* deviceUniqueIdUTF8,
                                     char* productUniqueIdUTF8,
                                     uint32_t productUniqueIdUTF8Length)
{

    const int32_t deviceUniqueIdUTF8Length =
        (int32_t) strlen((char*) deviceUniqueIdUTF8); // UTF8 is also NULL terminated
    if (deviceUniqueIdUTF8Length > kVideoCaptureUniqueNameLength)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Device name too long");
        return NULL;
    }

    // enumerate all video capture devices
    RELEASE_AND_CLEAR(_dsMonikerDevEnum);
    HRESULT hr = _dsDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                                   &_dsMonikerDevEnum, 0);
    if (hr != NOERROR)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to enumerate CLSID_SystemDeviceEnum, error 0x%x."
                     " No webcam exist?", hr);
        return 0;
    }
    _dsMonikerDevEnum->Reset();
    ULONG cFetched;
    IMoniker *pM;

    IBaseFilter *captureFilter = NULL;
    bool deviceFound = false;
    while (S_OK == _dsMonikerDevEnum->Next(1, &pM, &cFetched) && !deviceFound)
    {
        IPropertyBag *pBag;
        hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void **) &pBag);
        if (S_OK == hr)
        {
            // Find the description or friendly name.
            VARIANT varName;
            VariantInit(&varName);
            if (deviceUniqueIdUTF8Length > 0)
            {
                hr = pBag->Read(L"DevicePath", &varName, 0);
                if (FAILED(hr))
                {
                    hr = pBag->Read(L"Description", &varName, 0);
                    if (FAILED(hr))
                    {
                        hr = pBag->Read(L"FriendlyName", &varName, 0);
                    }
                }
                if (SUCCEEDED(hr))
                {
                    char tempDevicePathUTF8[256];
                    tempDevicePathUTF8[0] = 0;
                    WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1,
                                        tempDevicePathUTF8,
                                        sizeof(tempDevicePathUTF8), NULL,
                                        NULL);
                    if (strncmp(tempDevicePathUTF8,
                                (const char*) deviceUniqueIdUTF8,
                                deviceUniqueIdUTF8Length) == 0)
                    {
                        // We have found the requested device
                        deviceFound = true;
                        hr = pM->BindToObject(0, 0, IID_IBaseFilter,
                                              (void**) &captureFilter);
                        if FAILED(hr)
                        {
                            WEBRTC_TRACE(
                                webrtc::kTraceError, webrtc::kTraceVideoCapture,
                                0, "Failed to bind to the selected capture "
                                "device %d",hr);
                        }

                        if (productUniqueIdUTF8
                            && productUniqueIdUTF8Length > 0) // Get the device name
                        {

                            GetProductId(deviceUniqueIdUTF8,
                                         productUniqueIdUTF8,
                                         productUniqueIdUTF8Length);
                        }

                    }
                }
            }
            VariantClear(&varName);
            pBag->Release();
            pM->Release();
        }
    }
    return captureFilter;
}

int32_t DeviceInfoDS::GetWindowsCapability(
    const int32_t capabilityIndex,
    VideoCaptureCapabilityWindows& windowsCapability) {
  ReadLockScoped cs(_apiLock);

  if (capabilityIndex < 0 || static_cast<size_t>(capabilityIndex) >=
                                 _captureCapabilitiesWindows.size()) {
    return -1;
  }

  windowsCapability = _captureCapabilitiesWindows[capabilityIndex];
  return 0;
}

int32_t DeviceInfoDS::CreateCapabilityMap(
                                         const char* deviceUniqueIdUTF8)

{
    // Reset old capability list
    _captureCapabilities.clear();

    const int32_t deviceUniqueIdUTF8Length =
        (int32_t) strlen((char*) deviceUniqueIdUTF8);
    if (deviceUniqueIdUTF8Length > kVideoCaptureUniqueNameLength)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Device name too long");
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                 "CreateCapabilityMap called for device %s", deviceUniqueIdUTF8);


    char productId[kVideoCaptureProductIdLength];
    IBaseFilter* captureDevice = DeviceInfoDS::GetDeviceFilter(
                                               deviceUniqueIdUTF8,
                                               productId,
                                               kVideoCaptureProductIdLength);
    if (!captureDevice)
        return -1;
    IPin* outputCapturePin = GetOutputPin(captureDevice, GUID_NULL);
    if (!outputCapturePin)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to get capture device output pin");
        RELEASE_AND_CLEAR(captureDevice);
        return -1;
    }
    IAMExtDevice* extDevice = NULL;
    HRESULT hr = captureDevice->QueryInterface(IID_IAMExtDevice,
                                               (void **) &extDevice);
    if (SUCCEEDED(hr) && extDevice)
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                     "This is an external device");
        extDevice->Release();
    }

    IAMStreamConfig* streamConfig = NULL;
    hr = outputCapturePin->QueryInterface(IID_IAMStreamConfig,
                                          (void**) &streamConfig);
    if (FAILED(hr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to get IID_IAMStreamConfig interface from capture device");
        return -1;
    }

    // this  gets the FPS
    IAMVideoControl* videoControlConfig = NULL;
    HRESULT hrVC = captureDevice->QueryInterface(IID_IAMVideoControl,
                                      (void**) &videoControlConfig);
    if (FAILED(hrVC))
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, 0,
                     "IID_IAMVideoControl Interface NOT SUPPORTED");
    }

    AM_MEDIA_TYPE *pmt = NULL;
    VIDEO_STREAM_CONFIG_CAPS caps;
    int count, size;

    hr = streamConfig->GetNumberOfCapabilities(&count, &size);
    if (FAILED(hr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to GetNumberOfCapabilities");
        RELEASE_AND_CLEAR(videoControlConfig);
        RELEASE_AND_CLEAR(streamConfig);
        RELEASE_AND_CLEAR(outputCapturePin);
        RELEASE_AND_CLEAR(captureDevice);
        return -1;
    }

    // Check if the device support formattype == FORMAT_VideoInfo2 and FORMAT_VideoInfo.
    // Prefer FORMAT_VideoInfo since some cameras (ZureCam) has been seen having problem with MJPEG and FORMAT_VideoInfo2
    // Interlace flag is only supported in FORMAT_VideoInfo2
    bool supportFORMAT_VideoInfo2 = false;
    bool supportFORMAT_VideoInfo = false;
    bool foundInterlacedFormat = false;
    GUID preferedVideoFormat = FORMAT_VideoInfo;
    for (int32_t tmp = 0; tmp < count; ++tmp)
    {
        hr = streamConfig->GetStreamCaps(tmp, &pmt,
                                         reinterpret_cast<BYTE*> (&caps));
        if (!FAILED(hr))
        {
            if (pmt->majortype == MEDIATYPE_Video
                && pmt->formattype == FORMAT_VideoInfo2)
            {
                WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, 0,
                             " Device support FORMAT_VideoInfo2");
                supportFORMAT_VideoInfo2 = true;
                VIDEOINFOHEADER2* h =
                    reinterpret_cast<VIDEOINFOHEADER2*> (pmt->pbFormat);
                assert(h);
                foundInterlacedFormat |= h->dwInterlaceFlags
                                        & (AMINTERLACE_IsInterlaced
                                           | AMINTERLACE_DisplayModeBobOnly);
            }
            if (pmt->majortype == MEDIATYPE_Video
                && pmt->formattype == FORMAT_VideoInfo)
            {
                WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, 0,
                             " Device support FORMAT_VideoInfo2");
                supportFORMAT_VideoInfo = true;
            }
        }
    }
    if (supportFORMAT_VideoInfo2)
    {
        if (supportFORMAT_VideoInfo && !foundInterlacedFormat)
        {
            preferedVideoFormat = FORMAT_VideoInfo;
        }
        else
        {
            preferedVideoFormat = FORMAT_VideoInfo2;
        }
    }

    for (int32_t tmp = 0; tmp < count; ++tmp)
    {
        hr = streamConfig->GetStreamCaps(tmp, &pmt,
                                         reinterpret_cast<BYTE*> (&caps));
        if (FAILED(hr))
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                         "Failed to GetStreamCaps");
            RELEASE_AND_CLEAR(videoControlConfig);
            RELEASE_AND_CLEAR(streamConfig);
            RELEASE_AND_CLEAR(outputCapturePin);
            RELEASE_AND_CLEAR(captureDevice);
            return -1;
        }

        if (pmt->majortype == MEDIATYPE_Video
            && pmt->formattype == preferedVideoFormat)
        {

            VideoCaptureCapabilityWindows capability;
            int64_t avgTimePerFrame = 0;

            if (pmt->formattype == FORMAT_VideoInfo)
            {
                VIDEOINFOHEADER* h =
                    reinterpret_cast<VIDEOINFOHEADER*> (pmt->pbFormat);
                assert(h);
                capability.directShowCapabilityIndex = tmp;
                capability.width = h->bmiHeader.biWidth;
                capability.height = h->bmiHeader.biHeight;
                avgTimePerFrame = h->AvgTimePerFrame;
            }
            if (pmt->formattype == FORMAT_VideoInfo2)
            {
                VIDEOINFOHEADER2* h =
                    reinterpret_cast<VIDEOINFOHEADER2*> (pmt->pbFormat);
                assert(h);
                capability.directShowCapabilityIndex = tmp;
                capability.width = h->bmiHeader.biWidth;
                capability.height = h->bmiHeader.biHeight;
                capability.interlaced = h->dwInterlaceFlags
                                        & (AMINTERLACE_IsInterlaced
                                           | AMINTERLACE_DisplayModeBobOnly);
                avgTimePerFrame = h->AvgTimePerFrame;
            }

            if (hrVC == S_OK)
            {
                LONGLONG *frameDurationList;
                LONGLONG maxFPS;
                long listSize;
                SIZE size;
                size.cx = capability.width;
                size.cy = capability.height;

                // GetMaxAvailableFrameRate doesn't return max frame rate always
                // eg: Logitech Notebook. This may be due to a bug in that API
                // because GetFrameRateList array is reversed in the above camera. So
                // a util method written. Can't assume the first value will return
                // the max fps.
                hrVC = videoControlConfig->GetFrameRateList(outputCapturePin,
                                                            tmp, size,
                                                            &listSize,
                                                            &frameDurationList);

                // On some odd cameras, you may get a 0 for duration.
                // GetMaxOfFrameArray returns the lowest duration (highest FPS)
                if (hrVC == S_OK && listSize > 0 &&
                    0 != (maxFPS = GetMaxOfFrameArray(frameDurationList,
                                                      listSize)))
                {
                    capability.maxFPS = static_cast<int> (10000000
                                                           / maxFPS);
                    capability.supportFrameRateControl = true;
                }
                else // use existing method
                {
                    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture,
                                 0,
                                 "GetMaxAvailableFrameRate NOT SUPPORTED");
                    if (avgTimePerFrame > 0)
                        capability.maxFPS = static_cast<int> (10000000
                                                               / avgTimePerFrame);
                    else
                        capability.maxFPS = 0;
                }
            }
            else // use existing method in case IAMVideoControl is not supported
            {
                if (avgTimePerFrame > 0)
                    capability.maxFPS = static_cast<int> (10000000
                                                           / avgTimePerFrame);
                else
                    capability.maxFPS = 0;
            }

            // can't switch MEDIATYPE :~(
            if (pmt->subtype == MEDIASUBTYPE_I420)
            {
                capability.rawType = kVideoI420;
            }
            else if (pmt->subtype == MEDIASUBTYPE_IYUV)
            {
                capability.rawType = kVideoIYUV;
            }
            else if (pmt->subtype == MEDIASUBTYPE_RGB24)
            {
                capability.rawType = kVideoRGB24;
            }
            else if (pmt->subtype == MEDIASUBTYPE_YUY2)
            {
                capability.rawType = kVideoYUY2;
            }
            else if (pmt->subtype == MEDIASUBTYPE_RGB565)
            {
                capability.rawType = kVideoRGB565;
            }
            else if (pmt->subtype == MEDIASUBTYPE_MJPG)
            {
                capability.rawType = kVideoMJPEG;
            }
            else if (pmt->subtype == MEDIASUBTYPE_dvsl
                    || pmt->subtype == MEDIASUBTYPE_dvsd
                    || pmt->subtype == MEDIASUBTYPE_dvhd) // If this is an external DV camera
            {
                capability.rawType = kVideoYUY2;// MS DV filter seems to create this type
            }
            else if (pmt->subtype == MEDIASUBTYPE_UYVY) // Seen used by Declink capture cards
            {
                capability.rawType = kVideoUYVY;
            }
            else if (pmt->subtype == MEDIASUBTYPE_HDYC) // Seen used by Declink capture cards. Uses BT. 709 color. Not entiry correct to use UYVY. http://en.wikipedia.org/wiki/YCbCr
            {
                WEBRTC_TRACE(webrtc::kTraceWarning,
                             webrtc::kTraceVideoCapture, 0,
                             "Device support HDYC.");
                capability.rawType = kVideoUYVY;
            }
            else
            {
                WCHAR strGuid[39];
                StringFromGUID2(pmt->subtype, strGuid, 39);
                WEBRTC_TRACE( webrtc::kTraceWarning,
                             webrtc::kTraceVideoCapture, 0,
                             "Device support unknown media type %ls, width %d, height %d",
                             strGuid);
                continue;
            }

            _captureCapabilities.push_back(capability);
            _captureCapabilitiesWindows.push_back(capability);
            WEBRTC_TRACE( webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                         "Camera capability, width:%d height:%d type:%d fps:%d",
                         capability.width, capability.height,
                         capability.rawType, capability.maxFPS);
        }
        DeleteMediaType(pmt);
        pmt = NULL;
    }
    RELEASE_AND_CLEAR(streamConfig);
    RELEASE_AND_CLEAR(videoControlConfig);
    RELEASE_AND_CLEAR(outputCapturePin);
    RELEASE_AND_CLEAR(captureDevice); // Release the capture device

    // Store the new used device name
    _lastUsedDeviceNameLength = deviceUniqueIdUTF8Length;
    _lastUsedDeviceName = (char*) realloc(_lastUsedDeviceName,
                                                   _lastUsedDeviceNameLength
                                                       + 1);
    memcpy(_lastUsedDeviceName, deviceUniqueIdUTF8, _lastUsedDeviceNameLength+ 1);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                 "CreateCapabilityMap %d", _captureCapabilities.size());

    return static_cast<int32_t>(_captureCapabilities.size());
}

/* Constructs a product ID from the Windows DevicePath. on a USB device the devicePath contains product id and vendor id.
 This seems to work for firewire as well
 /* Example of device path
 "\\?\usb#vid_0408&pid_2010&mi_00#7&258e7aaf&0&0000#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\global"
 "\\?\avc#sony&dv-vcr&camcorder&dv#65b2d50301460008#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\global"
 */
void DeviceInfoDS::GetProductId(const char* devicePath,
                                      char* productUniqueIdUTF8,
                                      uint32_t productUniqueIdUTF8Length)
{
    *productUniqueIdUTF8 = '\0';
    char* startPos = strstr((char*) devicePath, "\\\\?\\");
    if (!startPos)
    {
        strncpy_s((char*) productUniqueIdUTF8, productUniqueIdUTF8Length, "", 1);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, -1,
                     "Failed to get the product Id");
        return;
    }
    startPos += 4;

    char* pos = strchr(startPos, '&');
    if (!pos || pos >= (char*) devicePath + strlen((char*) devicePath))
    {
        strncpy_s((char*) productUniqueIdUTF8, productUniqueIdUTF8Length, "", 1);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, -1,
                     "Failed to get the product Id");
        return;
    }
    // Find the second occurrence.
    pos = strchr(pos + 1, '&');
    uint32_t bytesToCopy = (uint32_t)(pos - startPos);
    if (pos && (bytesToCopy <= productUniqueIdUTF8Length) && bytesToCopy
        <= kVideoCaptureProductIdLength)
    {
        strncpy_s((char*) productUniqueIdUTF8, productUniqueIdUTF8Length,
                  (char*) startPos, bytesToCopy);
    }
    else
    {
        strncpy_s((char*) productUniqueIdUTF8, productUniqueIdUTF8Length, "", 1);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, -1,
                     "Failed to get the product Id");
    }
}

int32_t DeviceInfoDS::DisplayCaptureSettingsDialogBox(
                                         const char* deviceUniqueIdUTF8,
                                         const char* dialogTitleUTF8,
                                         void* parentWindow,
                                         uint32_t positionX,
                                         uint32_t positionY)
{
    ReadLockScoped cs(_apiLock);
    HWND window = (HWND) parentWindow;

    IBaseFilter* filter = GetDeviceFilter(deviceUniqueIdUTF8, NULL, 0);
    if (!filter)
        return -1;

    ISpecifyPropertyPages* pPages = NULL;
    CAUUID uuid;
    HRESULT hr = S_OK;

    hr = filter->QueryInterface(IID_ISpecifyPropertyPages, (LPVOID*) &pPages);
    if (!SUCCEEDED(hr))
    {
        filter->Release();
        return -1;
    }
    hr = pPages->GetPages(&uuid);
    if (!SUCCEEDED(hr))
    {
        filter->Release();
        return -1;
    }

    WCHAR tempDialogTitleWide[256];
    tempDialogTitleWide[0] = 0;
    int size = 255;

    // UTF-8 to wide char
    MultiByteToWideChar(CP_UTF8, 0, (char*) dialogTitleUTF8, -1,
                        tempDialogTitleWide, size);

    // Invoke a dialog box to display.

    hr = OleCreatePropertyFrame(window, // You must create the parent window.
                                positionX, // Horizontal position for the dialog box.
                                positionY, // Vertical position for the dialog box.
                                tempDialogTitleWide,// String used for the dialog box caption.
                                1, // Number of pointers passed in pPlugin.
                                (LPUNKNOWN*) &filter, // Pointer to the filter.
                                uuid.cElems, // Number of property pages.
                                uuid.pElems, // Array of property page CLSIDs.
                                LOCALE_USER_DEFAULT, // Locale ID for the dialog box.
                                0, NULL); // Reserved
    // Release memory.
    if (uuid.pElems)
    {
        CoTaskMemFree(uuid.pElems);
    }
    filter->Release();
    return 0;
}
}  // namespace videocapturemodule
}  // namespace webrtc
