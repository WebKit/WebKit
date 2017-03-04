/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma warning(disable: 4995)  // name was marked as #pragma deprecated

#if (_MSC_VER >= 1310) && (_MSC_VER < 1400)
// Reports the major and minor versions of the compiler.
// For example, 1310 for Microsoft Visual C++ .NET 2003. 1310 represents version 13 and a 1.0 point release.
// The Visual C++ 2005 compiler version is 1400.
// Type cl /? at the command line to see the major and minor versions of your compiler along with the build number.
#pragma message(">> INFO: Windows Core Audio is not supported in VS 2003")
#endif

#include "webrtc/modules/audio_device/audio_device_config.h"

#ifdef WEBRTC_WINDOWS_CORE_AUDIO_BUILD

#include "webrtc/modules/audio_device/win/audio_device_core_win.h"

#include <assert.h>
#include <string.h>

#include <windows.h>
#include <comdef.h>
#include <dmo.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <mmsystem.h>
#include <strsafe.h>
#include <uuids.h>

#include "webrtc/base/logging.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/system_wrappers/include/trace.h"

// Macro that calls a COM method returning HRESULT value.
#define EXIT_ON_ERROR(hres)    do { if (FAILED(hres)) goto Exit; } while(0)

// Macro that continues to a COM error.
#define CONTINUE_ON_ERROR(hres) do { if (FAILED(hres)) goto Next; } while(0)

// Macro that releases a COM object if not NULL.
#define SAFE_RELEASE(p)     do { if ((p)) { (p)->Release(); (p) = NULL; } } while(0)

#define ROUND(x) ((x) >=0 ? (int)((x) + 0.5) : (int)((x) - 0.5))

// REFERENCE_TIME time units per millisecond
#define REFTIMES_PER_MILLISEC  10000

typedef struct tagTHREADNAME_INFO
{
   DWORD dwType;        // must be 0x1000
   LPCSTR szName;       // pointer to name (in user addr space)
   DWORD dwThreadID;    // thread ID (-1=caller thread)
   DWORD dwFlags;       // reserved for future use, must be zero
} THREADNAME_INFO;

namespace webrtc {
namespace {

enum { COM_THREADING_MODEL = COINIT_MULTITHREADED };

enum
{
    kAecCaptureStreamIndex = 0,
    kAecRenderStreamIndex = 1
};

// An implementation of IMediaBuffer, as required for
// IMediaObject::ProcessOutput(). After consuming data provided by
// ProcessOutput(), call SetLength() to update the buffer availability.
//
// Example implementation:
// http://msdn.microsoft.com/en-us/library/dd376684(v=vs.85).aspx
class MediaBufferImpl : public IMediaBuffer
{
public:
    explicit MediaBufferImpl(DWORD maxLength)
        : _data(new BYTE[maxLength]),
          _length(0),
          _maxLength(maxLength),
          _refCount(0)
    {}

    // IMediaBuffer methods.
    STDMETHOD(GetBufferAndLength(BYTE** ppBuffer, DWORD* pcbLength))
    {
        if (!ppBuffer || !pcbLength)
        {
            return E_POINTER;
        }

        *ppBuffer = _data;
        *pcbLength = _length;

        return S_OK;
    }

    STDMETHOD(GetMaxLength(DWORD* pcbMaxLength))
    {
        if (!pcbMaxLength)
        {
            return E_POINTER;
        }

        *pcbMaxLength = _maxLength;
        return S_OK;
    }

    STDMETHOD(SetLength(DWORD cbLength))
    {
        if (cbLength > _maxLength)
        {
            return E_INVALIDARG;
        }

        _length = cbLength;
        return S_OK;
    }

    // IUnknown methods.
    STDMETHOD_(ULONG, AddRef())
    {
        return InterlockedIncrement(&_refCount);
    }

    STDMETHOD(QueryInterface(REFIID riid, void** ppv))
    {
        if (!ppv)
        {
            return E_POINTER;
        }
        else if (riid != IID_IMediaBuffer && riid != IID_IUnknown)
        {
            return E_NOINTERFACE;
        }

        *ppv = static_cast<IMediaBuffer*>(this);
        AddRef();
        return S_OK;
    }

    STDMETHOD_(ULONG, Release())
    {
        LONG refCount = InterlockedDecrement(&_refCount);
        if (refCount == 0)
        {
            delete this;
        }

        return refCount;
    }

private:
    ~MediaBufferImpl()
    {
        delete [] _data;
    }

    BYTE* _data;
    DWORD _length;
    const DWORD _maxLength;
    LONG _refCount;
};
}  // namespace

// ============================================================================
//                              Static Methods
// ============================================================================

// ----------------------------------------------------------------------------
//  CoreAudioIsSupported
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::CoreAudioIsSupported()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1, "%s", __FUNCTION__);

    bool MMDeviceIsAvailable(false);
    bool coreAudioIsSupported(false);

    HRESULT hr(S_OK);
    TCHAR buf[MAXERRORLENGTH];
    TCHAR errorText[MAXERRORLENGTH];

    // 1) Check if Windows version is Vista SP1 or later.
    //
    // CoreAudio is only available on Vista SP1 and later.
    //
    OSVERSIONINFOEX osvi;
    DWORDLONG dwlConditionMask = 0;
    int op = VER_LESS_EQUAL;

    // Initialize the OSVERSIONINFOEX structure.
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    osvi.dwMinorVersion = 0;
    osvi.wServicePackMajor = 0;
    osvi.wServicePackMinor = 0;
    osvi.wProductType = VER_NT_WORKSTATION;

    // Initialize the condition mask.
    VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, op);
    VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, op);
    VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMAJOR, op);
    VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMINOR, op);
    VER_SET_CONDITION(dwlConditionMask, VER_PRODUCT_TYPE, VER_EQUAL);

    DWORD dwTypeMask = VER_MAJORVERSION | VER_MINORVERSION |
                       VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR |
                       VER_PRODUCT_TYPE;

    // Perform the test.
    BOOL isVistaRTMorXP = VerifyVersionInfo(&osvi, dwTypeMask,
                                            dwlConditionMask);
    if (isVistaRTMorXP != 0)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, -1,
            "*** Windows Core Audio is only supported on Vista SP1 or later "
            "=> will revert to the Wave API ***");
        return false;
    }

    // 2) Initializes the COM library for use by the calling thread.

    // The COM init wrapper sets the thread's concurrency model to MTA,
    // and creates a new apartment for the thread if one is required. The
    // wrapper also ensures that each call to CoInitializeEx is balanced
    // by a corresponding call to CoUninitialize.
    //
    ScopedCOMInitializer comInit(ScopedCOMInitializer::kMTA);
    if (!comInit.succeeded()) {
      // Things will work even if an STA thread is calling this method but we
      // want to ensure that MTA is used and therefore return false here.
      return false;
    }

    // 3) Check if the MMDevice API is available.
    //
    // The Windows Multimedia Device (MMDevice) API enables audio clients to
    // discover audio endpoint devices, determine their capabilities, and create
    // driver instances for those devices.
    // Header file Mmdeviceapi.h defines the interfaces in the MMDevice API.
    // The MMDevice API consists of several interfaces. The first of these is the
    // IMMDeviceEnumerator interface. To access the interfaces in the MMDevice API,
    // a client obtains a reference to the IMMDeviceEnumerator interface of a
    // device-enumerator object by calling the CoCreateInstance function.
    //
    // Through the IMMDeviceEnumerator interface, the client can obtain references
    // to the other interfaces in the MMDevice API. The MMDevice API implements
    // the following interfaces:
    //
    // IMMDevice            Represents an audio device.
    // IMMDeviceCollection  Represents a collection of audio devices.
    // IMMDeviceEnumerator  Provides methods for enumerating audio devices.
    // IMMEndpoint          Represents an audio endpoint device.
    //
    IMMDeviceEnumerator* pIMMD(NULL);
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

    hr = CoCreateInstance(
            CLSID_MMDeviceEnumerator,   // GUID value of MMDeviceEnumerator coclass
            NULL,
            CLSCTX_ALL,
            IID_IMMDeviceEnumerator,    // GUID value of the IMMDeviceEnumerator interface
            (void**)&pIMMD );

    if (FAILED(hr))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
            "AudioDeviceWindowsCore::CoreAudioIsSupported() Failed to create the required COM object", hr);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1,
            "AudioDeviceWindowsCore::CoreAudioIsSupported() CoCreateInstance(MMDeviceEnumerator) failed (hr=0x%x)", hr);

        const DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM |
                              FORMAT_MESSAGE_IGNORE_INSERTS;
        const DWORD dwLangID = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

        // Gets the system's human readable message string for this HRESULT.
        // All error message in English by default.
        DWORD messageLength = ::FormatMessageW(dwFlags,
                                               0,
                                               hr,
                                               dwLangID,
                                               errorText,
                                               MAXERRORLENGTH,
                                               NULL);

        assert(messageLength <= MAXERRORLENGTH);

        // Trims tailing white space (FormatMessage() leaves a trailing cr-lf.).
        for (; messageLength && ::isspace(errorText[messageLength - 1]);
             --messageLength)
        {
            errorText[messageLength - 1] = '\0';
        }

        StringCchPrintf(buf, MAXERRORLENGTH, TEXT("Error details: "));
        StringCchCat(buf, MAXERRORLENGTH, errorText);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1, "%S", buf);
    }
    else
    {
        MMDeviceIsAvailable = true;
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1,
            "AudioDeviceWindowsCore::CoreAudioIsSupported() CoCreateInstance(MMDeviceEnumerator) succeeded", hr);
        SAFE_RELEASE(pIMMD);
    }

    // 4) Verify that we can create and initialize our Core Audio class.
    //
    // Also, perform a limited "API test" to ensure that Core Audio is supported for all devices.
    //
    if (MMDeviceIsAvailable)
    {
        coreAudioIsSupported = false;

        AudioDeviceWindowsCore* p = new AudioDeviceWindowsCore(-1);
        if (p == NULL)
        {
            return false;
        }

        int ok(0);
        int temp_ok(0);
        bool available(false);

        if (p->Init() != InitStatus::OK) {
          ok |= -1;
        }

        int16_t numDevsRec = p->RecordingDevices();
        for (uint16_t i = 0; i < numDevsRec; i++)
        {
            ok |= p->SetRecordingDevice(i);
            temp_ok = p->RecordingIsAvailable(available);
            ok |= temp_ok;
            ok |= (available == false);
            if (available)
            {
                ok |= p->InitMicrophone();
            }
            if (ok)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, -1,
                    "AudioDeviceWindowsCore::CoreAudioIsSupported() Failed to use Core Audio Recording for device id=%i", i);
            }
        }

        int16_t numDevsPlay = p->PlayoutDevices();
        for (uint16_t i = 0; i < numDevsPlay; i++)
        {
            ok |= p->SetPlayoutDevice(i);
            temp_ok = p->PlayoutIsAvailable(available);
            ok |= temp_ok;
            ok |= (available == false);
            if (available)
            {
                ok |= p->InitSpeaker();
            }
            if (ok)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, -1 ,
                    "AudioDeviceWindowsCore::CoreAudioIsSupported() Failed to use Core Audio Playout for device id=%i", i);
            }
        }

        ok |= p->Terminate();

        if (ok == 0)
        {
            coreAudioIsSupported = true;
        }

        delete p;
    }

    if (coreAudioIsSupported)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, -1, "*** Windows Core Audio is supported ***");
    }
    else
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, -1, "*** Windows Core Audio is NOT supported => will revert to the Wave API ***");
    }

    return (coreAudioIsSupported);
}

// ============================================================================
//                            Construction & Destruction
// ============================================================================

// ----------------------------------------------------------------------------
//  AudioDeviceWindowsCore() - ctor
// ----------------------------------------------------------------------------

AudioDeviceWindowsCore::AudioDeviceWindowsCore(const int32_t id) :
    _comInit(ScopedCOMInitializer::kMTA),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _volumeMutex(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _ptrAudioBuffer(NULL),
    _ptrEnumerator(NULL),
    _ptrRenderCollection(NULL),
    _ptrCaptureCollection(NULL),
    _ptrDeviceOut(NULL),
    _ptrDeviceIn(NULL),
    _ptrClientOut(NULL),
    _ptrClientIn(NULL),
    _ptrRenderClient(NULL),
    _ptrCaptureClient(NULL),
    _ptrCaptureVolume(NULL),
    _ptrRenderSimpleVolume(NULL),
    _dmo(NULL),
    _mediaBuffer(NULL),
    _builtInAecEnabled(false),
    _playAudioFrameSize(0),
    _playSampleRate(0),
    _playBlockSize(0),
    _playChannels(2),
    _sndCardPlayDelay(0),
    _sndCardRecDelay(0),
    _writtenSamples(0),
    _readSamples(0),
    _playAcc(0),
    _recAudioFrameSize(0),
    _recSampleRate(0),
    _recBlockSize(0),
    _recChannels(2),
    _avrtLibrary(NULL),
    _winSupportAvrt(false),
    _hRenderSamplesReadyEvent(NULL),
    _hPlayThread(NULL),
    _hCaptureSamplesReadyEvent(NULL),
    _hRecThread(NULL),
    _hShutdownRenderEvent(NULL),
    _hShutdownCaptureEvent(NULL),
    _hRenderStartedEvent(NULL),
    _hCaptureStartedEvent(NULL),
    _hGetCaptureVolumeThread(NULL),
    _hSetCaptureVolumeThread(NULL),
    _hSetCaptureVolumeEvent(NULL),
    _hMmTask(NULL),
    _initialized(false),
    _recording(false),
    _playing(false),
    _recIsInitialized(false),
    _playIsInitialized(false),
    _speakerIsInitialized(false),
    _microphoneIsInitialized(false),
    _AGC(false),
    _playWarning(0),
    _playError(0),
    _recWarning(0),
    _recError(0),
    _playBufType(AudioDeviceModule::kAdaptiveBufferSize),
    _playBufDelay(80),
    _playBufDelayFixed(80),
    _usingInputDeviceIndex(false),
    _usingOutputDeviceIndex(false),
    _inputDevice(AudioDeviceModule::kDefaultCommunicationDevice),
    _outputDevice(AudioDeviceModule::kDefaultCommunicationDevice),
    _inputDeviceIndex(0),
    _outputDeviceIndex(0),
    _newMicLevel(0)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id, "%s created", __FUNCTION__);
    assert(_comInit.succeeded());

    // Try to load the Avrt DLL
    if (!_avrtLibrary)
    {
        // Get handle to the Avrt DLL module.
        _avrtLibrary = LoadLibrary(TEXT("Avrt.dll"));
        if (_avrtLibrary)
        {
            // Handle is valid (should only happen if OS larger than vista & win7).
            // Try to get the function addresses.
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::AudioDeviceWindowsCore() The Avrt DLL module is now loaded");

            _PAvRevertMmThreadCharacteristics = (PAvRevertMmThreadCharacteristics)GetProcAddress(_avrtLibrary, "AvRevertMmThreadCharacteristics");
            _PAvSetMmThreadCharacteristicsA = (PAvSetMmThreadCharacteristicsA)GetProcAddress(_avrtLibrary, "AvSetMmThreadCharacteristicsA");
            _PAvSetMmThreadPriority = (PAvSetMmThreadPriority)GetProcAddress(_avrtLibrary, "AvSetMmThreadPriority");

            if ( _PAvRevertMmThreadCharacteristics &&
                 _PAvSetMmThreadCharacteristicsA &&
                 _PAvSetMmThreadPriority)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::AudioDeviceWindowsCore() AvRevertMmThreadCharacteristics() is OK");
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::AudioDeviceWindowsCore() AvSetMmThreadCharacteristicsA() is OK");
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::AudioDeviceWindowsCore() AvSetMmThreadPriority() is OK");
                _winSupportAvrt = true;
            }
        }
    }

    // Create our samples ready events - we want auto reset events that start in the not-signaled state.
    // The state of an auto-reset event object remains signaled until a single waiting thread is released,
    // at which time the system automatically sets the state to nonsignaled. If no threads are waiting,
    // the event object's state remains signaled.
    // (Except for _hShutdownCaptureEvent, which is used to shutdown multiple threads).
    _hRenderSamplesReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hCaptureSamplesReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hShutdownRenderEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hShutdownCaptureEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    _hRenderStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hCaptureStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hSetCaptureVolumeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    _perfCounterFreq.QuadPart = 1;
    _perfCounterFactor = 0.0;
    _avgCPULoad = 0.0;

    // list of number of channels to use on recording side
    _recChannelsPrioList[0] = 2;    // stereo is prio 1
    _recChannelsPrioList[1] = 1;    // mono is prio 2
    _recChannelsPrioList[2] = 4;    // quad is prio 3

    // list of number of channels to use on playout side
    _playChannelsPrioList[0] = 2;    // stereo is prio 1
    _playChannelsPrioList[1] = 1;    // mono is prio 2

    HRESULT hr;

    // We know that this API will work since it has already been verified in
    // CoreAudioIsSupported, hence no need to check for errors here as well.

    // Retrive the IMMDeviceEnumerator API (should load the MMDevAPI.dll)
    // TODO(henrika): we should probably move this allocation to Init() instead
    // and deallocate in Terminate() to make the implementation more symmetric.
    CoCreateInstance(
      __uuidof(MMDeviceEnumerator),
      NULL,
      CLSCTX_ALL,
      __uuidof(IMMDeviceEnumerator),
      reinterpret_cast<void**>(&_ptrEnumerator));
    assert(NULL != _ptrEnumerator);

    // DMO initialization for built-in WASAPI AEC.
    {
        IMediaObject* ptrDMO = NULL;
        hr = CoCreateInstance(CLSID_CWMAudioAEC,
                              NULL,
                              CLSCTX_INPROC_SERVER,
                              IID_IMediaObject,
                              reinterpret_cast<void**>(&ptrDMO));
        if (FAILED(hr) || ptrDMO == NULL)
        {
            // Since we check that _dmo is non-NULL in EnableBuiltInAEC(), the
            // feature is prevented from being enabled.
            _builtInAecEnabled = false;
            _TraceCOMError(hr);
        }
        _dmo = ptrDMO;
        SAFE_RELEASE(ptrDMO);
    }
}

// ----------------------------------------------------------------------------
//  AudioDeviceWindowsCore() - dtor
// ----------------------------------------------------------------------------

AudioDeviceWindowsCore::~AudioDeviceWindowsCore()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s destroyed", __FUNCTION__);

    Terminate();

    // The IMMDeviceEnumerator is created during construction. Must release
    // it here and not in Terminate() since we don't recreate it in Init().
    SAFE_RELEASE(_ptrEnumerator);

    _ptrAudioBuffer = NULL;

    if (NULL != _hRenderSamplesReadyEvent)
    {
        CloseHandle(_hRenderSamplesReadyEvent);
        _hRenderSamplesReadyEvent = NULL;
    }

    if (NULL != _hCaptureSamplesReadyEvent)
    {
        CloseHandle(_hCaptureSamplesReadyEvent);
        _hCaptureSamplesReadyEvent = NULL;
    }

    if (NULL != _hRenderStartedEvent)
    {
        CloseHandle(_hRenderStartedEvent);
        _hRenderStartedEvent = NULL;
    }

    if (NULL != _hCaptureStartedEvent)
    {
        CloseHandle(_hCaptureStartedEvent);
        _hCaptureStartedEvent = NULL;
    }

    if (NULL != _hShutdownRenderEvent)
    {
        CloseHandle(_hShutdownRenderEvent);
        _hShutdownRenderEvent = NULL;
    }

    if (NULL != _hShutdownCaptureEvent)
    {
        CloseHandle(_hShutdownCaptureEvent);
        _hShutdownCaptureEvent = NULL;
    }

    if (NULL != _hSetCaptureVolumeEvent)
    {
        CloseHandle(_hSetCaptureVolumeEvent);
        _hSetCaptureVolumeEvent = NULL;
    }

    if (_avrtLibrary)
    {
        BOOL freeOK = FreeLibrary(_avrtLibrary);
        if (!freeOK)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "AudioDeviceWindowsCore::~AudioDeviceWindowsCore() failed to free the loaded Avrt DLL module correctly");
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "AudioDeviceWindowsCore::~AudioDeviceWindowsCore() the Avrt DLL module is now unloaded");
        }
    }

    delete &_critSect;
    delete &_volumeMutex;
}

// ============================================================================
//                                     API
// ============================================================================

// ----------------------------------------------------------------------------
//  AttachAudioBuffer
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{

    _ptrAudioBuffer = audioBuffer;

    // Inform the AudioBuffer about default settings for this implementation.
    // Set all values to zero here since the actual settings will be done by
    // InitPlayout and InitRecording later.
    _ptrAudioBuffer->SetRecordingSampleRate(0);
    _ptrAudioBuffer->SetPlayoutSampleRate(0);
    _ptrAudioBuffer->SetRecordingChannels(0);
    _ptrAudioBuffer->SetPlayoutChannels(0);
}

// ----------------------------------------------------------------------------
//  ActiveAudioLayer
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const
{
    audioLayer = AudioDeviceModule::kWindowsCoreAudio;
    return 0;
}

// ----------------------------------------------------------------------------
//  Init
// ----------------------------------------------------------------------------

AudioDeviceGeneric::InitStatus AudioDeviceWindowsCore::Init() {
  CriticalSectionScoped lock(&_critSect);

  if (_initialized) {
    return InitStatus::OK;
  }

  _playWarning = 0;
  _playError = 0;
  _recWarning = 0;
  _recError = 0;

  // Enumerate all audio rendering and capturing endpoint devices.
  // Note that, some of these will not be able to select by the user.
  // The complete collection is for internal use only.
  _EnumerateEndpointDevicesAll(eRender);
  _EnumerateEndpointDevicesAll(eCapture);

  _initialized = true;

  return InitStatus::OK;
}

// ----------------------------------------------------------------------------
//  Terminate
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::Terminate()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized) {
        return 0;
    }

    _initialized = false;
    _speakerIsInitialized = false;
    _microphoneIsInitialized = false;
    _playing = false;
    _recording = false;

    SAFE_RELEASE(_ptrRenderCollection);
    SAFE_RELEASE(_ptrCaptureCollection);
    SAFE_RELEASE(_ptrDeviceOut);
    SAFE_RELEASE(_ptrDeviceIn);
    SAFE_RELEASE(_ptrClientOut);
    SAFE_RELEASE(_ptrClientIn);
    SAFE_RELEASE(_ptrRenderClient);
    SAFE_RELEASE(_ptrCaptureClient);
    SAFE_RELEASE(_ptrCaptureVolume);
    SAFE_RELEASE(_ptrRenderSimpleVolume);

    return 0;
}

// ----------------------------------------------------------------------------
//  Initialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::Initialized() const
{
    return (_initialized);
}

// ----------------------------------------------------------------------------
//  InitSpeaker
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::InitSpeaker()
{

    CriticalSectionScoped lock(&_critSect);

    if (_playing)
    {
        return -1;
    }

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    if (_usingOutputDeviceIndex)
    {
        int16_t nDevices = PlayoutDevices();
        if (_outputDeviceIndex > (nDevices - 1))
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "current device selection is invalid => unable to initialize");
            return -1;
        }
    }

    int32_t ret(0);

    SAFE_RELEASE(_ptrDeviceOut);
    if (_usingOutputDeviceIndex)
    {
        // Refresh the selected rendering endpoint device using current index
        ret = _GetListDevice(eRender, _outputDeviceIndex, &_ptrDeviceOut);
    }
    else
    {
        ERole role;
        (_outputDevice == AudioDeviceModule::kDefaultDevice) ? role = eConsole : role = eCommunications;
        // Refresh the selected rendering endpoint device using role
        ret = _GetDefaultDevice(eRender, role, &_ptrDeviceOut);
    }

    if (ret != 0 || (_ptrDeviceOut == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to initialize the rendering enpoint device");
        SAFE_RELEASE(_ptrDeviceOut);
        return -1;
    }

    IAudioSessionManager* pManager = NULL;
    ret = _ptrDeviceOut->Activate(__uuidof(IAudioSessionManager),
                                  CLSCTX_ALL,
                                  NULL,
                                  (void**)&pManager);
    if (ret != 0 || pManager == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "  failed to initialize the render manager");
        SAFE_RELEASE(pManager);
        return -1;
    }

    SAFE_RELEASE(_ptrRenderSimpleVolume);
    ret = pManager->GetSimpleAudioVolume(NULL, FALSE, &_ptrRenderSimpleVolume);
    if (ret != 0 || _ptrRenderSimpleVolume == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "  failed to initialize the render simple volume");
        SAFE_RELEASE(pManager);
        SAFE_RELEASE(_ptrRenderSimpleVolume);
        return -1;
    }
    SAFE_RELEASE(pManager);

    _speakerIsInitialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  InitMicrophone
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::InitMicrophone()
{

    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        return -1;
    }

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    if (_usingInputDeviceIndex)
    {
        int16_t nDevices = RecordingDevices();
        if (_inputDeviceIndex > (nDevices - 1))
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "current device selection is invalid => unable to initialize");
            return -1;
        }
    }

    int32_t ret(0);

    SAFE_RELEASE(_ptrDeviceIn);
    if (_usingInputDeviceIndex)
    {
        // Refresh the selected capture endpoint device using current index
        ret = _GetListDevice(eCapture, _inputDeviceIndex, &_ptrDeviceIn);
    }
    else
    {
        ERole role;
        (_inputDevice == AudioDeviceModule::kDefaultDevice) ? role = eConsole : role = eCommunications;
        // Refresh the selected capture endpoint device using role
        ret = _GetDefaultDevice(eCapture, role, &_ptrDeviceIn);
    }

    if (ret != 0 || (_ptrDeviceIn == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to initialize the capturing enpoint device");
        SAFE_RELEASE(_ptrDeviceIn);
        return -1;
    }

    ret = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume),
                                 CLSCTX_ALL,
                                 NULL,
                                 reinterpret_cast<void **>(&_ptrCaptureVolume));
    if (ret != 0 || _ptrCaptureVolume == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "  failed to initialize the capture volume");
        SAFE_RELEASE(_ptrCaptureVolume);
        return -1;
    }

    _microphoneIsInitialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::SpeakerIsInitialized() const
{

    return (_speakerIsInitialized);
}

// ----------------------------------------------------------------------------
//  MicrophoneIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::MicrophoneIsInitialized() const
{

    return (_microphoneIsInitialized);
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SpeakerVolumeIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioSessionManager* pManager = NULL;
    ISimpleAudioVolume* pVolume = NULL;

    hr = _ptrDeviceOut->Activate(__uuidof(IAudioSessionManager), CLSCTX_ALL, NULL, (void**)&pManager);
    EXIT_ON_ERROR(hr);

    hr = pManager->GetSimpleAudioVolume(NULL, FALSE, &pVolume);
    EXIT_ON_ERROR(hr);

    float volume(0.0f);
    hr = pVolume->GetMasterVolume(&volume);
    if (FAILED(hr))
    {
        available = false;
    }
    available = true;

    SAFE_RELEASE(pManager);
    SAFE_RELEASE(pVolume);

    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pManager);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  SetSpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetSpeakerVolume(uint32_t volume)
{

    {
        CriticalSectionScoped lock(&_critSect);

        if (!_speakerIsInitialized)
        {
        return -1;
        }

        if (_ptrDeviceOut == NULL)
        {
            return -1;
        }
    }

    if (volume < (uint32_t)MIN_CORE_SPEAKER_VOLUME ||
        volume > (uint32_t)MAX_CORE_SPEAKER_VOLUME)
    {
        return -1;
    }

    HRESULT hr = S_OK;

    // scale input volume to valid range (0.0 to 1.0)
    const float fLevel = (float)volume/MAX_CORE_SPEAKER_VOLUME;
    _volumeMutex.Enter();
    hr = _ptrRenderSimpleVolume->SetMasterVolume(fLevel,NULL);
    _volumeMutex.Leave();
    EXIT_ON_ERROR(hr);

    return 0;

Exit:
    _TraceCOMError(hr);
    return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SpeakerVolume(uint32_t& volume) const
{

    {
        CriticalSectionScoped lock(&_critSect);

        if (!_speakerIsInitialized)
        {
            return -1;
        }

        if (_ptrDeviceOut == NULL)
        {
            return -1;
        }
    }

    HRESULT hr = S_OK;
    float fLevel(0.0f);

    _volumeMutex.Enter();
    hr = _ptrRenderSimpleVolume->GetMasterVolume(&fLevel);
    _volumeMutex.Leave();
    EXIT_ON_ERROR(hr);

    // scale input volume range [0.0,1.0] to valid output range
    volume = static_cast<uint32_t> (fLevel*MAX_CORE_SPEAKER_VOLUME);

    return 0;

Exit:
    _TraceCOMError(hr);
    return -1;
}

// ----------------------------------------------------------------------------
//  SetWaveOutVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetWaveOutVolume(uint16_t volumeLeft, uint16_t volumeRight)
{
    return -1;
}

// ----------------------------------------------------------------------------
//  WaveOutVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::WaveOutVolume(uint16_t& volumeLeft, uint16_t& volumeRight) const
{
    return -1;
}

// ----------------------------------------------------------------------------
//  MaxSpeakerVolume
//
//  The internal range for Core Audio is 0.0 to 1.0, where 0.0 indicates
//  silence and 1.0 indicates full volume (no attenuation).
//  We add our (webrtc-internal) own max level to match the Wave API and
//  how it is used today in VoE.
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MaxSpeakerVolume(uint32_t& maxVolume) const
{

    if (!_speakerIsInitialized)
    {
        return -1;
    }

    maxVolume = static_cast<uint32_t> (MAX_CORE_SPEAKER_VOLUME);

    return 0;
}

// ----------------------------------------------------------------------------
//  MinSpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MinSpeakerVolume(uint32_t& minVolume) const
{

    if (!_speakerIsInitialized)
    {
        return -1;
    }

    minVolume = static_cast<uint32_t> (MIN_CORE_SPEAKER_VOLUME);

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeStepSize
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SpeakerVolumeStepSize(uint16_t& stepSize) const
{

    if (!_speakerIsInitialized)
    {
        return -1;
    }

    stepSize = CORE_SPEAKER_VOLUME_STEP_SIZE;

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerMuteIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SpeakerMuteIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    // Query the speaker system mute state.
    hr = _ptrDeviceOut->Activate(__uuidof(IAudioEndpointVolume),
        CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    BOOL mute;
    hr = pVolume->GetMute(&mute);
    if (FAILED(hr))
        available = false;
    else
        available = true;

    SAFE_RELEASE(pVolume);

    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  SetSpeakerMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetSpeakerMute(bool enable)
{

    CriticalSectionScoped lock(&_critSect);

    if (!_speakerIsInitialized)
    {
        return -1;
    }

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    // Set the speaker system mute state.
    hr = _ptrDeviceOut->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    const BOOL mute(enable);
    hr = pVolume->SetMute(mute, NULL);
    EXIT_ON_ERROR(hr);

    SAFE_RELEASE(pVolume);

    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SpeakerMute(bool& enabled) const
{

    if (!_speakerIsInitialized)
    {
        return -1;
    }

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    // Query the speaker system mute state.
    hr = _ptrDeviceOut->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    BOOL mute;
    hr = pVolume->GetMute(&mute);
    EXIT_ON_ERROR(hr);

    enabled = (mute == TRUE) ? true : false;

    SAFE_RELEASE(pVolume);

    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMuteIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MicrophoneMuteIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    // Query the microphone system mute state.
    hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    BOOL mute;
    hr = pVolume->GetMute(&mute);
    if (FAILED(hr))
        available = false;
    else
        available = true;

    SAFE_RELEASE(pVolume);
    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetMicrophoneMute(bool enable)
{

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    // Set the microphone system mute state.
    hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    const BOOL mute(enable);
    hr = pVolume->SetMute(mute, NULL);
    EXIT_ON_ERROR(hr);

    SAFE_RELEASE(pVolume);
    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MicrophoneMute(bool& enabled) const
{

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    // Query the microphone system mute state.
    hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    BOOL mute;
    hr = pVolume->GetMute(&mute);
    EXIT_ON_ERROR(hr);

    enabled = (mute == TRUE) ? true : false;

    SAFE_RELEASE(pVolume);
    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoostIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MicrophoneBoostIsAvailable(bool& available)
{

    available = false;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneBoost
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetMicrophoneBoost(bool enable)
{

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoost
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MicrophoneBoost(bool& enabled) const
{

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    return -1;
}

// ----------------------------------------------------------------------------
//  StereoRecordingIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::StereoRecordingIsAvailable(bool& available)
{

    available = true;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetStereoRecording(bool enable)
{

    CriticalSectionScoped lock(&_critSect);

    if (enable)
    {
        _recChannelsPrioList[0] = 2;    // try stereo first
        _recChannelsPrioList[1] = 1;
        _recChannels = 2;
    }
    else
    {
        _recChannelsPrioList[0] = 1;    // try mono first
        _recChannelsPrioList[1] = 2;
        _recChannels = 1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::StereoRecording(bool& enabled) const
{

    if (_recChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayoutIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::StereoPlayoutIsAvailable(bool& available)
{

    available = true;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetStereoPlayout(bool enable)
{

    CriticalSectionScoped lock(&_critSect);

    if (enable)
    {
        _playChannelsPrioList[0] = 2;    // try stereo first
        _playChannelsPrioList[1] = 1;
        _playChannels = 2;
    }
    else
    {
        _playChannelsPrioList[0] = 1;    // try mono first
        _playChannelsPrioList[1] = 2;
        _playChannels = 1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::StereoPlayout(bool& enabled) const
{

    if (_playChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetAGC
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetAGC(bool enable)
{
    CriticalSectionScoped lock(&_critSect);
    _AGC = enable;
    return 0;
}

// ----------------------------------------------------------------------------
//  AGC
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::AGC() const
{
    CriticalSectionScoped lock(&_critSect);
    return _AGC;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MicrophoneVolumeIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    float volume(0.0f);
    hr = pVolume->GetMasterVolumeLevelScalar(&volume);
    if (FAILED(hr))
    {
        available = false;
    }
    available = true;

    SAFE_RELEASE(pVolume);
    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetMicrophoneVolume(uint32_t volume)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetMicrophoneVolume(volume=%u)", volume);

    {
        CriticalSectionScoped lock(&_critSect);

        if (!_microphoneIsInitialized)
        {
            return -1;
        }

        if (_ptrDeviceIn == NULL)
        {
            return -1;
        }
    }

    if (volume < static_cast<uint32_t>(MIN_CORE_MICROPHONE_VOLUME) ||
        volume > static_cast<uint32_t>(MAX_CORE_MICROPHONE_VOLUME))
    {
        return -1;
    }

    HRESULT hr = S_OK;
    // scale input volume to valid range (0.0 to 1.0)
    const float fLevel = static_cast<float>(volume)/MAX_CORE_MICROPHONE_VOLUME;
    _volumeMutex.Enter();
    _ptrCaptureVolume->SetMasterVolumeLevelScalar(fLevel, NULL);
    _volumeMutex.Leave();
    EXIT_ON_ERROR(hr);

    return 0;

Exit:
    _TraceCOMError(hr);
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MicrophoneVolume(uint32_t& volume) const
{
    {
        CriticalSectionScoped lock(&_critSect);

        if (!_microphoneIsInitialized)
        {
            return -1;
        }

        if (_ptrDeviceIn == NULL)
        {
            return -1;
        }
    }

    HRESULT hr = S_OK;
    float fLevel(0.0f);
    volume = 0;
    _volumeMutex.Enter();
    hr = _ptrCaptureVolume->GetMasterVolumeLevelScalar(&fLevel);
    _volumeMutex.Leave();
    EXIT_ON_ERROR(hr);

    // scale input volume range [0.0,1.0] to valid output range
    volume = static_cast<uint32_t> (fLevel*MAX_CORE_MICROPHONE_VOLUME);

    return 0;

Exit:
    _TraceCOMError(hr);
    return -1;
}

// ----------------------------------------------------------------------------
//  MaxMicrophoneVolume
//
//  The internal range for Core Audio is 0.0 to 1.0, where 0.0 indicates
//  silence and 1.0 indicates full volume (no attenuation).
//  We add our (webrtc-internal) own max level to match the Wave API and
//  how it is used today in VoE.
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MaxMicrophoneVolume(uint32_t& maxVolume) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    maxVolume = static_cast<uint32_t> (MAX_CORE_MICROPHONE_VOLUME);

    return 0;
}

// ----------------------------------------------------------------------------
//  MinMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MinMicrophoneVolume(uint32_t& minVolume) const
{

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    minVolume = static_cast<uint32_t> (MIN_CORE_MICROPHONE_VOLUME);

    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeStepSize
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::MicrophoneVolumeStepSize(uint16_t& stepSize) const
{

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    stepSize = CORE_MICROPHONE_VOLUME_STEP_SIZE;

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDevices
// ----------------------------------------------------------------------------

int16_t AudioDeviceWindowsCore::PlayoutDevices()
{

    CriticalSectionScoped lock(&_critSect);

    if (_RefreshDeviceList(eRender) != -1)
    {
        return (_DeviceListCount(eRender));
    }

    return -1;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice I (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetPlayoutDevice(uint16_t index)
{

    if (_playIsInitialized)
    {
        return -1;
    }

    // Get current number of available rendering endpoint devices and refresh the rendering collection.
    UINT nDevices = PlayoutDevices();

    if (index < 0 || index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    CriticalSectionScoped lock(&_critSect);

    HRESULT hr(S_OK);

    assert(_ptrRenderCollection != NULL);

    //  Select an endpoint rendering device given the specified index
    SAFE_RELEASE(_ptrDeviceOut);
    hr = _ptrRenderCollection->Item(
                                 index,
                                 &_ptrDeviceOut);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(_ptrDeviceOut);
        return -1;
    }

    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (_GetDeviceName(_ptrDeviceOut, szDeviceName, bufferLen) == 0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"", szDeviceName);
    }

    _usingOutputDeviceIndex = true;
    _outputDeviceIndex = index;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice II (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device)
{
    if (_playIsInitialized)
    {
        return -1;
    }

    ERole role(eCommunications);

    if (device == AudioDeviceModule::kDefaultDevice)
    {
        role = eConsole;
    }
    else if (device == AudioDeviceModule::kDefaultCommunicationDevice)
    {
        role = eCommunications;
    }

    CriticalSectionScoped lock(&_critSect);

    // Refresh the list of rendering endpoint devices
    _RefreshDeviceList(eRender);

    HRESULT hr(S_OK);

    assert(_ptrEnumerator != NULL);

    //  Select an endpoint rendering device given the specified role
    SAFE_RELEASE(_ptrDeviceOut);
    hr = _ptrEnumerator->GetDefaultAudioEndpoint(
                           eRender,
                           role,
                           &_ptrDeviceOut);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(_ptrDeviceOut);
        return -1;
    }

    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (_GetDeviceName(_ptrDeviceOut, szDeviceName, bufferLen) == 0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"", szDeviceName);
    }

    _usingOutputDeviceIndex = false;
    _outputDevice = device;

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDeviceName
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::PlayoutDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    bool defaultCommunicationDevice(false);
    const int16_t nDevices(PlayoutDevices());  // also updates the list of devices

    // Special fix for the case when the user selects '-1' as index (<=> Default Communication Device)
    if (index == (uint16_t)(-1))
    {
        defaultCommunicationDevice = true;
        index = 0;
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Default Communication endpoint device will be used");
    }

    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    CriticalSectionScoped lock(&_critSect);

    int32_t ret(-1);
    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (defaultCommunicationDevice)
    {
        ret = _GetDefaultDeviceName(eRender, eCommunications, szDeviceName, bufferLen);
    }
    else
    {
        ret = _GetListDeviceName(eRender, index, szDeviceName, bufferLen);
    }

    if (ret == 0)
    {
        // Convert the endpoint device's friendly-name to UTF-8
        if (WideCharToMultiByte(CP_UTF8, 0, szDeviceName, -1, name, kAdmMaxDeviceNameSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d", GetLastError());
        }
    }

    // Get the endpoint ID string (uniquely identifies the device among all audio endpoint devices)
    if (defaultCommunicationDevice)
    {
        ret = _GetDefaultDeviceID(eRender, eCommunications, szDeviceName, bufferLen);
    }
    else
    {
        ret = _GetListDeviceID(eRender, index, szDeviceName, bufferLen);
    }

    if (guid != NULL && ret == 0)
    {
        // Convert the endpoint device's ID string to UTF-8
        if (WideCharToMultiByte(CP_UTF8, 0, szDeviceName, -1, guid, kAdmMaxGuidSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d", GetLastError());
        }
    }

    return ret;
}

// ----------------------------------------------------------------------------
//  RecordingDeviceName
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::RecordingDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    bool defaultCommunicationDevice(false);
    const int16_t nDevices(RecordingDevices());  // also updates the list of devices

    // Special fix for the case when the user selects '-1' as index (<=> Default Communication Device)
    if (index == (uint16_t)(-1))
    {
        defaultCommunicationDevice = true;
        index = 0;
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Default Communication endpoint device will be used");
    }

    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    CriticalSectionScoped lock(&_critSect);

    int32_t ret(-1);
    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (defaultCommunicationDevice)
    {
        ret = _GetDefaultDeviceName(eCapture, eCommunications, szDeviceName, bufferLen);
    }
    else
    {
        ret = _GetListDeviceName(eCapture, index, szDeviceName, bufferLen);
    }

    if (ret == 0)
    {
        // Convert the endpoint device's friendly-name to UTF-8
        if (WideCharToMultiByte(CP_UTF8, 0, szDeviceName, -1, name, kAdmMaxDeviceNameSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d", GetLastError());
        }
    }

    // Get the endpoint ID string (uniquely identifies the device among all audio endpoint devices)
    if (defaultCommunicationDevice)
    {
        ret = _GetDefaultDeviceID(eCapture, eCommunications, szDeviceName, bufferLen);
    }
    else
    {
        ret = _GetListDeviceID(eCapture, index, szDeviceName, bufferLen);
    }

    if (guid != NULL && ret == 0)
    {
        // Convert the endpoint device's ID string to UTF-8
        if (WideCharToMultiByte(CP_UTF8, 0, szDeviceName, -1, guid, kAdmMaxGuidSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d", GetLastError());
        }
    }

    return ret;
}

// ----------------------------------------------------------------------------
//  RecordingDevices
// ----------------------------------------------------------------------------

int16_t AudioDeviceWindowsCore::RecordingDevices()
{

    CriticalSectionScoped lock(&_critSect);

    if (_RefreshDeviceList(eCapture) != -1)
    {
        return (_DeviceListCount(eCapture));
    }

    return -1;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice I (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetRecordingDevice(uint16_t index)
{

    if (_recIsInitialized)
    {
        return -1;
    }

    // Get current number of available capture endpoint devices and refresh the capture collection.
    UINT nDevices = RecordingDevices();

    if (index < 0 || index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    CriticalSectionScoped lock(&_critSect);

    HRESULT hr(S_OK);

    assert(_ptrCaptureCollection != NULL);

    // Select an endpoint capture device given the specified index
    SAFE_RELEASE(_ptrDeviceIn);
    hr = _ptrCaptureCollection->Item(
                                 index,
                                 &_ptrDeviceIn);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(_ptrDeviceIn);
        return -1;
    }

    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (_GetDeviceName(_ptrDeviceIn, szDeviceName, bufferLen) == 0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"", szDeviceName);
    }

    _usingInputDeviceIndex = true;
    _inputDeviceIndex = index;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetRecordingDevice(AudioDeviceModule::WindowsDeviceType device)
{
    if (_recIsInitialized)
    {
        return -1;
    }

    ERole role(eCommunications);

    if (device == AudioDeviceModule::kDefaultDevice)
    {
        role = eConsole;
    }
    else if (device == AudioDeviceModule::kDefaultCommunicationDevice)
    {
        role = eCommunications;
    }

    CriticalSectionScoped lock(&_critSect);

    // Refresh the list of capture endpoint devices
    _RefreshDeviceList(eCapture);

    HRESULT hr(S_OK);

    assert(_ptrEnumerator != NULL);

    //  Select an endpoint capture device given the specified role
    SAFE_RELEASE(_ptrDeviceIn);
    hr = _ptrEnumerator->GetDefaultAudioEndpoint(
                           eCapture,
                           role,
                           &_ptrDeviceIn);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(_ptrDeviceIn);
        return -1;
    }

    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (_GetDeviceName(_ptrDeviceIn, szDeviceName, bufferLen) == 0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"", szDeviceName);
    }

    _usingInputDeviceIndex = false;
    _inputDevice = device;

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::PlayoutIsAvailable(bool& available)
{

    available = false;

    // Try to initialize the playout side
    int32_t res = InitPlayout();

    // Cancel effect of initialization
    StopPlayout();

    if (res != -1)
    {
        available = true;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::RecordingIsAvailable(bool& available)
{

    available = false;

    // Try to initialize the recording side
    int32_t res = InitRecording();

    // Cancel effect of initialization
    StopRecording();

    if (res != -1)
    {
        available = true;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  InitPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::InitPlayout()
{

    CriticalSectionScoped lock(&_critSect);

    if (_playing)
    {
        return -1;
    }

    if (_playIsInitialized)
    {
        return 0;
    }

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    // Initialize the speaker (devices might have been added or removed)
    if (InitSpeaker() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "InitSpeaker() failed");
    }

    // Ensure that the updated rendering endpoint device is valid
    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    if (_builtInAecEnabled && _recIsInitialized)
    {
        // Ensure the correct render device is configured in case
        // InitRecording() was called before InitPlayout().
        if (SetDMOProperties() == -1)
        {
            return -1;
        }
    }

    HRESULT hr = S_OK;
    WAVEFORMATEX* pWfxOut = NULL;
    WAVEFORMATEX Wfx = WAVEFORMATEX();
    WAVEFORMATEX* pWfxClosestMatch = NULL;

    // Create COM object with IAudioClient interface.
    SAFE_RELEASE(_ptrClientOut);
    hr = _ptrDeviceOut->Activate(
                          __uuidof(IAudioClient),
                          CLSCTX_ALL,
                          NULL,
                          (void**)&_ptrClientOut);
    EXIT_ON_ERROR(hr);

    // Retrieve the stream format that the audio engine uses for its internal
    // processing (mixing) of shared-mode streams.
    hr = _ptrClientOut->GetMixFormat(&pWfxOut);
    if (SUCCEEDED(hr))
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Audio Engine's current rendering mix format:");
        // format type
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wFormatTag     : 0x%X (%u)", pWfxOut->wFormatTag, pWfxOut->wFormatTag);
        // number of channels (i.e. mono, stereo...)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels      : %d", pWfxOut->nChannels);
        // sample rate
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec : %d", pWfxOut->nSamplesPerSec);
        // for buffer estimation
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec: %d", pWfxOut->nAvgBytesPerSec);
        // block size of data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign    : %d", pWfxOut->nBlockAlign);
        // number of bits per sample of mono data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample : %d", pWfxOut->wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize         : %d", pWfxOut->cbSize);
    }

    // Set wave format
    Wfx.wFormatTag = WAVE_FORMAT_PCM;
    Wfx.wBitsPerSample = 16;
    Wfx.cbSize = 0;

    const int freqs[] = {48000, 44100, 16000, 96000, 32000, 8000};
    hr = S_FALSE;

    // Iterate over frequencies and channels, in order of priority
    for (unsigned int freq = 0; freq < sizeof(freqs)/sizeof(freqs[0]); freq++)
    {
        for (unsigned int chan = 0; chan < sizeof(_playChannelsPrioList)/sizeof(_playChannelsPrioList[0]); chan++)
        {
            Wfx.nChannels = _playChannelsPrioList[chan];
            Wfx.nSamplesPerSec = freqs[freq];
            Wfx.nBlockAlign = Wfx.nChannels * Wfx.wBitsPerSample / 8;
            Wfx.nAvgBytesPerSec = Wfx.nSamplesPerSec * Wfx.nBlockAlign;
            // If the method succeeds and the audio endpoint device supports the specified stream format,
            // it returns S_OK. If the method succeeds and provides a closest match to the specified format,
            // it returns S_FALSE.
            hr = _ptrClientOut->IsFormatSupported(
                                  AUDCLNT_SHAREMODE_SHARED,
                                  &Wfx,
                                  &pWfxClosestMatch);
            if (hr == S_OK)
            {
                break;
            }
            else
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels=%d, nSamplesPerSec=%d is not supported",
                    Wfx.nChannels, Wfx.nSamplesPerSec);
            }
        }
        if (hr == S_OK)
            break;
    }

    // TODO(andrew): what happens in the event of failure in the above loop?
    //   Is _ptrClientOut->Initialize expected to fail?
    //   Same in InitRecording().
    if (hr == S_OK)
    {
        _playAudioFrameSize = Wfx.nBlockAlign;
        _playBlockSize = Wfx.nSamplesPerSec/100;
        _playSampleRate = Wfx.nSamplesPerSec;
        _devicePlaySampleRate = Wfx.nSamplesPerSec; // The device itself continues to run at 44.1 kHz.
        _devicePlayBlockSize = Wfx.nSamplesPerSec/100;
        _playChannels = Wfx.nChannels;

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "VoE selected this rendering format:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wFormatTag         : 0x%X (%u)", Wfx.wFormatTag, Wfx.wFormatTag);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels          : %d", Wfx.nChannels);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec     : %d", Wfx.nSamplesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec    : %d", Wfx.nAvgBytesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign        : %d", Wfx.nBlockAlign);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample     : %d", Wfx.wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize             : %d", Wfx.cbSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Additional settings:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_playAudioFrameSize: %d", _playAudioFrameSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_playBlockSize     : %d", _playBlockSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_playChannels      : %d", _playChannels);
    }

    // Create a rendering stream.
    //
    // ****************************************************************************
    // For a shared-mode stream that uses event-driven buffering, the caller must
    // set both hnsPeriodicity and hnsBufferDuration to 0. The Initialize method
    // determines how large a buffer to allocate based on the scheduling period
    // of the audio engine. Although the client's buffer processing thread is
    // event driven, the basic buffer management process, as described previously,
    // is unaltered.
    // Each time the thread awakens, it should call IAudioClient::GetCurrentPadding
    // to determine how much data to write to a rendering buffer or read from a capture
    // buffer. In contrast to the two buffers that the Initialize method allocates
    // for an exclusive-mode stream that uses event-driven buffering, a shared-mode
    // stream requires a single buffer.
    // ****************************************************************************
    //
    REFERENCE_TIME hnsBufferDuration = 0;  // ask for minimum buffer size (default)
    if (_devicePlaySampleRate == 44100)
    {
        // Ask for a larger buffer size (30ms) when using 44.1kHz as render rate.
        // There seems to be a larger risk of underruns for 44.1 compared
        // with the default rate (48kHz). When using default, we set the requested
        // buffer duration to 0, which sets the buffer to the minimum size
        // required by the engine thread. The actual buffer size can then be
        // read by GetBufferSize() and it is 20ms on most machines.
        hnsBufferDuration = 30*10000;
    }
    hr = _ptrClientOut->Initialize(
                          AUDCLNT_SHAREMODE_SHARED,             // share Audio Engine with other applications
                          AUDCLNT_STREAMFLAGS_EVENTCALLBACK,    // processing of the audio buffer by the client will be event driven
                          hnsBufferDuration,                    // requested buffer capacity as a time value (in 100-nanosecond units)
                          0,                                    // periodicity
                          &Wfx,                                 // selected wave format
                          NULL);                                // session GUID

    if (FAILED(hr))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "IAudioClient::Initialize() failed:");
        if (pWfxClosestMatch != NULL)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "closest mix format: #channels=%d, samples/sec=%d, bits/sample=%d",
                pWfxClosestMatch->nChannels, pWfxClosestMatch->nSamplesPerSec, pWfxClosestMatch->wBitsPerSample);
        }
        else
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "no format suggested");
        }
    }
    EXIT_ON_ERROR(hr);

    if (_ptrAudioBuffer)
    {
        // Update the audio buffer with the selected parameters
        _ptrAudioBuffer->SetPlayoutSampleRate(_playSampleRate);
        _ptrAudioBuffer->SetPlayoutChannels((uint8_t)_playChannels);
    }
    else
    {
        // We can enter this state during CoreAudioIsSupported() when no AudioDeviceImplementation
        // has been created, hence the AudioDeviceBuffer does not exist.
        // It is OK to end up here since we don't initiate any media in CoreAudioIsSupported().
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceBuffer must be attached before streaming can start");
    }

    // Get the actual size of the shared (endpoint buffer).
    // Typical value is 960 audio frames <=> 20ms @ 48kHz sample rate.
    UINT bufferFrameCount(0);
    hr = _ptrClientOut->GetBufferSize(
                          &bufferFrameCount);
    if (SUCCEEDED(hr))
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "IAudioClient::GetBufferSize() => %u (<=> %u bytes)",
            bufferFrameCount, bufferFrameCount*_playAudioFrameSize);
    }

    // Set the event handle that the system signals when an audio buffer is ready
    // to be processed by the client.
    hr = _ptrClientOut->SetEventHandle(
                          _hRenderSamplesReadyEvent);
    EXIT_ON_ERROR(hr);

    // Get an IAudioRenderClient interface.
    SAFE_RELEASE(_ptrRenderClient);
    hr = _ptrClientOut->GetService(
                          __uuidof(IAudioRenderClient),
                          (void**)&_ptrRenderClient);
    EXIT_ON_ERROR(hr);

    // Mark playout side as initialized
    _playIsInitialized = true;

    CoTaskMemFree(pWfxOut);
    CoTaskMemFree(pWfxClosestMatch);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "render side is now initialized");
    return 0;

Exit:
    _TraceCOMError(hr);
    CoTaskMemFree(pWfxOut);
    CoTaskMemFree(pWfxClosestMatch);
    SAFE_RELEASE(_ptrClientOut);
    SAFE_RELEASE(_ptrRenderClient);
    return -1;
}

// Capture initialization when the built-in AEC DirectX Media Object (DMO) is
// used. Called from InitRecording(), most of which is skipped over. The DMO
// handles device initialization itself.
// Reference: http://msdn.microsoft.com/en-us/library/ff819492(v=vs.85).aspx
int32_t AudioDeviceWindowsCore::InitRecordingDMO()
{
    assert(_builtInAecEnabled);
    assert(_dmo != NULL);

    if (SetDMOProperties() == -1)
    {
        return -1;
    }

    DMO_MEDIA_TYPE mt = {0};
    HRESULT hr = MoInitMediaType(&mt, sizeof(WAVEFORMATEX));
    if (FAILED(hr))
    {
        MoFreeMediaType(&mt);
        _TraceCOMError(hr);
        return -1;
    }
    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = MEDIASUBTYPE_PCM;
    mt.formattype = FORMAT_WaveFormatEx;

    // Supported formats
    // nChannels: 1 (in AEC-only mode)
    // nSamplesPerSec: 8000, 11025, 16000, 22050
    // wBitsPerSample: 16
    WAVEFORMATEX* ptrWav = reinterpret_cast<WAVEFORMATEX*>(mt.pbFormat);
    ptrWav->wFormatTag = WAVE_FORMAT_PCM;
    ptrWav->nChannels = 1;
    // 16000 is the highest we can support with our resampler.
    ptrWav->nSamplesPerSec = 16000;
    ptrWav->nAvgBytesPerSec = 32000;
    ptrWav->nBlockAlign = 2;
    ptrWav->wBitsPerSample = 16;
    ptrWav->cbSize = 0;

    // Set the VoE format equal to the AEC output format.
    _recAudioFrameSize = ptrWav->nBlockAlign;
    _recSampleRate = ptrWav->nSamplesPerSec;
    _recBlockSize = ptrWav->nSamplesPerSec / 100;
    _recChannels = ptrWav->nChannels;

    // Set the DMO output format parameters.
    hr = _dmo->SetOutputType(kAecCaptureStreamIndex, &mt, 0);
    MoFreeMediaType(&mt);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        return -1;
    }

    if (_ptrAudioBuffer)
    {
        _ptrAudioBuffer->SetRecordingSampleRate(_recSampleRate);
        _ptrAudioBuffer->SetRecordingChannels(_recChannels);
    }
    else
    {
        // Refer to InitRecording() for comments.
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "AudioDeviceBuffer must be attached before streaming can start");
    }

    _mediaBuffer = new MediaBufferImpl(_recBlockSize * _recAudioFrameSize);

    // Optional, but if called, must be after media types are set.
    hr = _dmo->AllocateStreamingResources();
    if (FAILED(hr))
    {
         _TraceCOMError(hr);
        return -1;
    }

    _recIsInitialized = true;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "Capture side is now initialized");

    return 0;
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::InitRecording()
{

    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        return -1;
    }

    if (_recIsInitialized)
    {
        return 0;
    }

    if (QueryPerformanceFrequency(&_perfCounterFreq) == 0)
    {
        return -1;
    }
    _perfCounterFactor = 10000000.0 / (double)_perfCounterFreq.QuadPart;

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    // Initialize the microphone (devices might have been added or removed)
    if (InitMicrophone() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "InitMicrophone() failed");
    }

    // Ensure that the updated capturing endpoint device is valid
    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    if (_builtInAecEnabled)
    {
        // The DMO will configure the capture device.
        return InitRecordingDMO();
    }

    HRESULT hr = S_OK;
    WAVEFORMATEX* pWfxIn = NULL;
    WAVEFORMATEXTENSIBLE Wfx = WAVEFORMATEXTENSIBLE();
    WAVEFORMATEX* pWfxClosestMatch = NULL;

    // Create COM object with IAudioClient interface.
    SAFE_RELEASE(_ptrClientIn);
    hr = _ptrDeviceIn->Activate(
                          __uuidof(IAudioClient),
                          CLSCTX_ALL,
                          NULL,
                          (void**)&_ptrClientIn);
    EXIT_ON_ERROR(hr);

    // Retrieve the stream format that the audio engine uses for its internal
    // processing (mixing) of shared-mode streams.
    hr = _ptrClientIn->GetMixFormat(&pWfxIn);
    if (SUCCEEDED(hr))
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Audio Engine's current capturing mix format:");
        // format type
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wFormatTag     : 0x%X (%u)", pWfxIn->wFormatTag, pWfxIn->wFormatTag);
        // number of channels (i.e. mono, stereo...)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels      : %d", pWfxIn->nChannels);
        // sample rate
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec : %d", pWfxIn->nSamplesPerSec);
        // for buffer estimation
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec: %d", pWfxIn->nAvgBytesPerSec);
        // block size of data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign    : %d", pWfxIn->nBlockAlign);
        // number of bits per sample of mono data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample : %d", pWfxIn->wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize         : %d", pWfxIn->cbSize);
    }

    // Set wave format
    Wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    Wfx.Format.wBitsPerSample = 16;
    Wfx.Format.cbSize = 22;
    Wfx.dwChannelMask = 0;
    Wfx.Samples.wValidBitsPerSample = Wfx.Format.wBitsPerSample;
    Wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    const int freqs[6] = {48000, 44100, 16000, 96000, 32000, 8000};
    hr = S_FALSE;

    // Iterate over frequencies and channels, in order of priority
    for (unsigned int freq = 0; freq < sizeof(freqs)/sizeof(freqs[0]); freq++)
    {
        for (unsigned int chan = 0; chan < sizeof(_recChannelsPrioList)/sizeof(_recChannelsPrioList[0]); chan++)
        {
            Wfx.Format.nChannels = _recChannelsPrioList[chan];
            Wfx.Format.nSamplesPerSec = freqs[freq];
            Wfx.Format.nBlockAlign = Wfx.Format.nChannels *
                                     Wfx.Format.wBitsPerSample / 8;
            Wfx.Format.nAvgBytesPerSec = Wfx.Format.nSamplesPerSec *
                                         Wfx.Format.nBlockAlign;
            // If the method succeeds and the audio endpoint device supports the specified stream format,
            // it returns S_OK. If the method succeeds and provides a closest match to the specified format,
            // it returns S_FALSE.
            hr = _ptrClientIn->IsFormatSupported(
                                  AUDCLNT_SHAREMODE_SHARED,
                                  (WAVEFORMATEX*)&Wfx,
                                  &pWfxClosestMatch);
            if (hr == S_OK)
            {
                break;
            }
            else
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels=%d, nSamplesPerSec=%d is not supported",
                    Wfx.Format.nChannels, Wfx.Format.nSamplesPerSec);
            }
        }
        if (hr == S_OK)
            break;
    }

    if (hr == S_OK)
    {
        _recAudioFrameSize = Wfx.Format.nBlockAlign;
        _recSampleRate = Wfx.Format.nSamplesPerSec;
        _recBlockSize = Wfx.Format.nSamplesPerSec/100;
        _recChannels = Wfx.Format.nChannels;

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "VoE selected this capturing format:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wFormatTag        : 0x%X (%u)", Wfx.Format.wFormatTag,
                                                                                          Wfx.Format.wFormatTag);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels         : %d", Wfx.Format.nChannels);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec    : %d", Wfx.Format.nSamplesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec   : %d", Wfx.Format.nAvgBytesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign       : %d", Wfx.Format.nBlockAlign);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample    : %d", Wfx.Format.wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize            : %d", Wfx.Format.cbSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Additional settings:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_recAudioFrameSize: %d", _recAudioFrameSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_recBlockSize     : %d", _recBlockSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_recChannels      : %d", _recChannels);
    }

    // Create a capturing stream.
    hr = _ptrClientIn->Initialize(
                          AUDCLNT_SHAREMODE_SHARED,             // share Audio Engine with other applications
                          AUDCLNT_STREAMFLAGS_EVENTCALLBACK |   // processing of the audio buffer by the client will be event driven
                          AUDCLNT_STREAMFLAGS_NOPERSIST,        // volume and mute settings for an audio session will not persist across system restarts
                          0,                                    // required for event-driven shared mode
                          0,                                    // periodicity
                          (WAVEFORMATEX*)&Wfx,                  // selected wave format
                          NULL);                                // session GUID


    if (hr != S_OK)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "IAudioClient::Initialize() failed:");
        if (pWfxClosestMatch != NULL)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "closest mix format: #channels=%d, samples/sec=%d, bits/sample=%d",
                pWfxClosestMatch->nChannels, pWfxClosestMatch->nSamplesPerSec, pWfxClosestMatch->wBitsPerSample);
        }
        else
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "no format suggested");
        }
    }
    EXIT_ON_ERROR(hr);

    if (_ptrAudioBuffer)
    {
        // Update the audio buffer with the selected parameters
        _ptrAudioBuffer->SetRecordingSampleRate(_recSampleRate);
        _ptrAudioBuffer->SetRecordingChannels((uint8_t)_recChannels);
    }
    else
    {
        // We can enter this state during CoreAudioIsSupported() when no AudioDeviceImplementation
        // has been created, hence the AudioDeviceBuffer does not exist.
        // It is OK to end up here since we don't initiate any media in CoreAudioIsSupported().
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceBuffer must be attached before streaming can start");
    }

    // Get the actual size of the shared (endpoint buffer).
    // Typical value is 960 audio frames <=> 20ms @ 48kHz sample rate.
    UINT bufferFrameCount(0);
    hr = _ptrClientIn->GetBufferSize(
                          &bufferFrameCount);
    if (SUCCEEDED(hr))
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "IAudioClient::GetBufferSize() => %u (<=> %u bytes)",
            bufferFrameCount, bufferFrameCount*_recAudioFrameSize);
    }

    // Set the event handle that the system signals when an audio buffer is ready
    // to be processed by the client.
    hr = _ptrClientIn->SetEventHandle(
                          _hCaptureSamplesReadyEvent);
    EXIT_ON_ERROR(hr);

    // Get an IAudioCaptureClient interface.
    SAFE_RELEASE(_ptrCaptureClient);
    hr = _ptrClientIn->GetService(
                          __uuidof(IAudioCaptureClient),
                          (void**)&_ptrCaptureClient);
    EXIT_ON_ERROR(hr);

    // Mark capture side as initialized
    _recIsInitialized = true;

    CoTaskMemFree(pWfxIn);
    CoTaskMemFree(pWfxClosestMatch);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "capture side is now initialized");
    return 0;

Exit:
    _TraceCOMError(hr);
    CoTaskMemFree(pWfxIn);
    CoTaskMemFree(pWfxClosestMatch);
    SAFE_RELEASE(_ptrClientIn);
    SAFE_RELEASE(_ptrCaptureClient);
    return -1;
}

// ----------------------------------------------------------------------------
//  StartRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::StartRecording()
{

    if (!_recIsInitialized)
    {
        return -1;
    }

    if (_hRecThread != NULL)
    {
        return 0;
    }

    if (_recording)
    {
        return 0;
    }

    {
        CriticalSectionScoped critScoped(&_critSect);

        // Create thread which will drive the capturing
        LPTHREAD_START_ROUTINE lpStartAddress = WSAPICaptureThread;
        if (_builtInAecEnabled)
        {
            // Redirect to the DMO polling method.
            lpStartAddress = WSAPICaptureThreadPollDMO;

            if (!_playing)
            {
                // The DMO won't provide us captured output data unless we
                // give it render data to process.
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "Playout must be started before recording when using the "
                    "built-in AEC");
                return -1;
            }
        }

        assert(_hRecThread == NULL);
        _hRecThread = CreateThread(NULL,
                                   0,
                                   lpStartAddress,
                                   this,
                                   0,
                                   NULL);
        if (_hRecThread == NULL)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "failed to create the recording thread");
            return -1;
        }

        // Set thread priority to highest possible
        SetThreadPriority(_hRecThread, THREAD_PRIORITY_TIME_CRITICAL);

        assert(_hGetCaptureVolumeThread == NULL);
        _hGetCaptureVolumeThread = CreateThread(NULL,
                                                0,
                                                GetCaptureVolumeThread,
                                                this,
                                                0,
                                                NULL);
        if (_hGetCaptureVolumeThread == NULL)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  failed to create the volume getter thread");
            return -1;
        }

        assert(_hSetCaptureVolumeThread == NULL);
        _hSetCaptureVolumeThread = CreateThread(NULL,
                                                0,
                                                SetCaptureVolumeThread,
                                                this,
                                                0,
                                                NULL);
        if (_hSetCaptureVolumeThread == NULL)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  failed to create the volume setter thread");
            return -1;
        }
    }  // critScoped

    DWORD ret = WaitForSingleObject(_hCaptureStartedEvent, 1000);
    if (ret != WAIT_OBJECT_0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "capturing did not start up properly");
        return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "capture audio stream has now started...");

    _avgCPULoad = 0.0f;
    _playAcc = 0;
    _recording = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  StopRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::StopRecording()
{
    int32_t err = 0;

    if (!_recIsInitialized)
    {
        return 0;
    }

    _Lock();

    if (_hRecThread == NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "no capturing stream is active => close down WASAPI only");
        SAFE_RELEASE(_ptrClientIn);
        SAFE_RELEASE(_ptrCaptureClient);
        _recIsInitialized = false;
        _recording = false;
        _UnLock();
        return 0;
    }

    // Stop the driving thread...
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "closing down the webrtc_core_audio_capture_thread...");
    // Manual-reset event; it will remain signalled to stop all capture threads.
    SetEvent(_hShutdownCaptureEvent);

    _UnLock();
    DWORD ret = WaitForSingleObject(_hRecThread, 2000);
    if (ret != WAIT_OBJECT_0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "failed to close down webrtc_core_audio_capture_thread");
        err = -1;
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "webrtc_core_audio_capture_thread is now closed");
    }

    ret = WaitForSingleObject(_hGetCaptureVolumeThread, 2000);
    if (ret != WAIT_OBJECT_0)
    {
        // the thread did not stop as it should
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to close down volume getter thread");
        err = -1;
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "  volume getter thread is now closed");
    }

    ret = WaitForSingleObject(_hSetCaptureVolumeThread, 2000);
    if (ret != WAIT_OBJECT_0)
    {
        // the thread did not stop as it should
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to close down volume setter thread");
        err = -1;
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "  volume setter thread is now closed");
    }
    _Lock();

    ResetEvent(_hShutdownCaptureEvent); // Must be manually reset.
    // Ensure that the thread has released these interfaces properly.
    assert(err == -1 || _ptrClientIn == NULL);
    assert(err == -1 || _ptrCaptureClient == NULL);

    _recIsInitialized = false;
    _recording = false;

    // These will create thread leaks in the result of an error,
    // but we can at least resume the call.
    CloseHandle(_hRecThread);
    _hRecThread = NULL;

    CloseHandle(_hGetCaptureVolumeThread);
    _hGetCaptureVolumeThread = NULL;

    CloseHandle(_hSetCaptureVolumeThread);
    _hSetCaptureVolumeThread = NULL;

    if (_builtInAecEnabled)
    {
        assert(_dmo != NULL);
        // This is necessary. Otherwise the DMO can generate garbage render
        // audio even after rendering has stopped.
        HRESULT hr = _dmo->FreeStreamingResources();
        if (FAILED(hr))
        {
            _TraceCOMError(hr);
            err = -1;
        }
    }

    // Reset the recording delay value.
    _sndCardRecDelay = 0;

    _UnLock();

    return err;
}

// ----------------------------------------------------------------------------
//  RecordingIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::RecordingIsInitialized() const
{
    return (_recIsInitialized);
}

// ----------------------------------------------------------------------------
//  Recording
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::Recording() const
{
    return (_recording);
}

// ----------------------------------------------------------------------------
//  PlayoutIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::PlayoutIsInitialized() const
{

    return (_playIsInitialized);
}

// ----------------------------------------------------------------------------
//  StartPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::StartPlayout()
{

    if (!_playIsInitialized)
    {
        return -1;
    }

    if (_hPlayThread != NULL)
    {
        return 0;
    }

    if (_playing)
    {
        return 0;
    }

    {
        CriticalSectionScoped critScoped(&_critSect);

        // Create thread which will drive the rendering.
        assert(_hPlayThread == NULL);
        _hPlayThread = CreateThread(
                         NULL,
                         0,
                         WSAPIRenderThread,
                         this,
                         0,
                         NULL);
        if (_hPlayThread == NULL)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                "failed to create the playout thread");
            return -1;
        }

        // Set thread priority to highest possible.
        SetThreadPriority(_hPlayThread, THREAD_PRIORITY_TIME_CRITICAL);
    }  // critScoped

    DWORD ret = WaitForSingleObject(_hRenderStartedEvent, 1000);
    if (ret != WAIT_OBJECT_0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "rendering did not start up properly");
        return -1;
    }

    _playing = true;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "rendering audio stream has now started...");

    return 0;
}

// ----------------------------------------------------------------------------
//  StopPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::StopPlayout()
{

    if (!_playIsInitialized)
    {
        return 0;
    }

    {
        CriticalSectionScoped critScoped(&_critSect) ;

        if (_hPlayThread == NULL)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                "no rendering stream is active => close down WASAPI only");
            SAFE_RELEASE(_ptrClientOut);
            SAFE_RELEASE(_ptrRenderClient);
            _playIsInitialized = false;
            _playing = false;
            return 0;
        }

        // stop the driving thread...
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "closing down the webrtc_core_audio_render_thread...");
        SetEvent(_hShutdownRenderEvent);
    }  // critScoped

    DWORD ret = WaitForSingleObject(_hPlayThread, 2000);
    if (ret != WAIT_OBJECT_0)
    {
        // the thread did not stop as it should
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "failed to close down webrtc_core_audio_render_thread");
        CloseHandle(_hPlayThread);
        _hPlayThread = NULL;
        _playIsInitialized = false;
        _playing = false;
        return -1;
    }

    {
        CriticalSectionScoped critScoped(&_critSect);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "webrtc_core_audio_render_thread is now closed");

        // to reset this event manually at each time we finish with it,
        // in case that the render thread has exited before StopPlayout(),
        // this event might be caught by the new render thread within same VoE instance.
        ResetEvent(_hShutdownRenderEvent);

        SAFE_RELEASE(_ptrClientOut);
        SAFE_RELEASE(_ptrRenderClient);

        _playIsInitialized = false;
        _playing = false;

        CloseHandle(_hPlayThread);
        _hPlayThread = NULL;

        if (_builtInAecEnabled && _recording)
        {
            // The DMO won't provide us captured output data unless we
            // give it render data to process.
            //
            // We still permit the playout to shutdown, and trace a warning.
            // Otherwise, VoE can get into a state which will never permit
            // playout to stop properly.
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "Recording should be stopped before playout when using the "
                "built-in AEC");
        }

        // Reset the playout delay value.
        _sndCardPlayDelay = 0;
    }  // critScoped

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDelay
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::PlayoutDelay(uint16_t& delayMS) const
{
    CriticalSectionScoped critScoped(&_critSect);
    delayMS = static_cast<uint16_t>(_sndCardPlayDelay);
    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDelay
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::RecordingDelay(uint16_t& delayMS) const
{
    CriticalSectionScoped critScoped(&_critSect);
    delayMS = static_cast<uint16_t>(_sndCardRecDelay);
    return 0;
}

// ----------------------------------------------------------------------------
//  Playing
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::Playing() const
{
    return (_playing);
}
// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::SetPlayoutBuffer(const AudioDeviceModule::BufferType type, uint16_t sizeMS)
{

    CriticalSectionScoped lock(&_critSect);

    _playBufType = type;

    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        _playBufDelayFixed = sizeMS;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutBuffer
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::PlayoutBuffer(AudioDeviceModule::BufferType& type, uint16_t& sizeMS) const
{
    CriticalSectionScoped lock(&_critSect);
    type = _playBufType;

    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        sizeMS = _playBufDelayFixed;
    }
    else
    {
        // Use same value as for PlayoutDelay
        sizeMS = static_cast<uint16_t>(_sndCardPlayDelay);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  CPULoad
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::CPULoad(uint16_t& load) const
{

    load = static_cast<uint16_t> (100*_avgCPULoad);

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutWarning
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::PlayoutWarning() const
{
    return ( _playWarning > 0);
}

// ----------------------------------------------------------------------------
//  PlayoutError
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::PlayoutError() const
{
    return ( _playError > 0);
}

// ----------------------------------------------------------------------------
//  RecordingWarning
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::RecordingWarning() const
{
    return ( _recWarning > 0);
}

// ----------------------------------------------------------------------------
//  RecordingError
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::RecordingError() const
{
    return ( _recError > 0);
}

// ----------------------------------------------------------------------------
//  ClearPlayoutWarning
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::ClearPlayoutWarning()
{
    _playWarning = 0;
}

// ----------------------------------------------------------------------------
//  ClearPlayoutError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::ClearPlayoutError()
{
    _playError = 0;
}

// ----------------------------------------------------------------------------
//  ClearRecordingWarning
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::ClearRecordingWarning()
{
    _recWarning = 0;
}

// ----------------------------------------------------------------------------
//  ClearRecordingError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::ClearRecordingError()
{
    _recError = 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

// ----------------------------------------------------------------------------
//  [static] WSAPIRenderThread
// ----------------------------------------------------------------------------

DWORD WINAPI AudioDeviceWindowsCore::WSAPIRenderThread(LPVOID context)
{
    return reinterpret_cast<AudioDeviceWindowsCore*>(context)->
        DoRenderThread();
}

// ----------------------------------------------------------------------------
//  [static] WSAPICaptureThread
// ----------------------------------------------------------------------------

DWORD WINAPI AudioDeviceWindowsCore::WSAPICaptureThread(LPVOID context)
{
    return reinterpret_cast<AudioDeviceWindowsCore*>(context)->
        DoCaptureThread();
}

DWORD WINAPI AudioDeviceWindowsCore::WSAPICaptureThreadPollDMO(LPVOID context)
{
    return reinterpret_cast<AudioDeviceWindowsCore*>(context)->
        DoCaptureThreadPollDMO();
}

DWORD WINAPI AudioDeviceWindowsCore::GetCaptureVolumeThread(LPVOID context)
{
    return reinterpret_cast<AudioDeviceWindowsCore*>(context)->
        DoGetCaptureVolumeThread();
}

DWORD WINAPI AudioDeviceWindowsCore::SetCaptureVolumeThread(LPVOID context)
{
    return reinterpret_cast<AudioDeviceWindowsCore*>(context)->
        DoSetCaptureVolumeThread();
}

DWORD AudioDeviceWindowsCore::DoGetCaptureVolumeThread()
{
    HANDLE waitObject = _hShutdownCaptureEvent;

    while (1)
    {
        if (AGC())
        {
            uint32_t currentMicLevel = 0;
            if (MicrophoneVolume(currentMicLevel) == 0)
            {
                // This doesn't set the system volume, just stores it.
                _Lock();
                if (_ptrAudioBuffer)
                {
                    _ptrAudioBuffer->SetCurrentMicLevel(currentMicLevel);
                }
                _UnLock();
            }
        }

        DWORD waitResult = WaitForSingleObject(waitObject,
                                               GET_MIC_VOLUME_INTERVAL_MS);
        switch (waitResult)
        {
            case WAIT_OBJECT_0: // _hShutdownCaptureEvent
                return 0;
            case WAIT_TIMEOUT:  // timeout notification
                break;
            default:            // unexpected error
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                    "  unknown wait termination on get volume thread");
                return 1;
        }
    }
}

DWORD AudioDeviceWindowsCore::DoSetCaptureVolumeThread()
{
    HANDLE waitArray[2] = {_hShutdownCaptureEvent, _hSetCaptureVolumeEvent};

    while (1)
    {
        DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
        switch (waitResult)
        {
            case WAIT_OBJECT_0:      // _hShutdownCaptureEvent
                return 0;
            case WAIT_OBJECT_0 + 1:  // _hSetCaptureVolumeEvent
                break;
            default:                 // unexpected error
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                    "  unknown wait termination on set volume thread");
                    return 1;
        }

        _Lock();
        uint32_t newMicLevel = _newMicLevel;
        _UnLock();

        if (SetMicrophoneVolume(newMicLevel) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "  the required modification of the microphone volume failed");
        }
    }
}

// ----------------------------------------------------------------------------
//  DoRenderThread
// ----------------------------------------------------------------------------

DWORD AudioDeviceWindowsCore::DoRenderThread()
{

    bool keepPlaying = true;
    HANDLE waitArray[2] = {_hShutdownRenderEvent, _hRenderSamplesReadyEvent};
    HRESULT hr = S_OK;
    HANDLE hMmTask = NULL;

    LARGE_INTEGER t1;
    LARGE_INTEGER t2;
    int32_t time(0);

    // Initialize COM as MTA in this thread.
    ScopedCOMInitializer comInit(ScopedCOMInitializer::kMTA);
    if (!comInit.succeeded()) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
          "failed to initialize COM in render thread");
      return 1;
    }

    rtc::SetCurrentThreadName("webrtc_core_audio_render_thread");

    // Use Multimedia Class Scheduler Service (MMCSS) to boost the thread priority.
    //
    if (_winSupportAvrt)
    {
        DWORD taskIndex(0);
        hMmTask = _PAvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
        if (hMmTask)
        {
            if (FALSE == _PAvSetMmThreadPriority(hMmTask, AVRT_PRIORITY_CRITICAL))
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to boost play-thread using MMCSS");
            }
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "render thread is now registered with MMCSS (taskIndex=%d)", taskIndex);
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to enable MMCSS on render thread (err=%d)", GetLastError());
            _TraceCOMError(GetLastError());
        }
    }

    _Lock();

    IAudioClock* clock = NULL;

    // Get size of rendering buffer (length is expressed as the number of audio frames the buffer can hold).
    // This value is fixed during the rendering session.
    //
    UINT32 bufferLength = 0;
    hr = _ptrClientOut->GetBufferSize(&bufferLength);
    EXIT_ON_ERROR(hr);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[REND] size of buffer       : %u", bufferLength);

    // Get maximum latency for the current stream (will not change for the lifetime  of the IAudioClient object).
    //
    REFERENCE_TIME latency;
    _ptrClientOut->GetStreamLatency(&latency);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[REND] max stream latency   : %u (%3.2f ms)",
        (DWORD)latency, (double)(latency/10000.0));

    // Get the length of the periodic interval separating successive processing passes by
    // the audio engine on the data in the endpoint buffer.
    //
    // The period between processing passes by the audio engine is fixed for a particular
    // audio endpoint device and represents the smallest processing quantum for the audio engine.
    // This period plus the stream latency between the buffer and endpoint device represents
    // the minimum possible latency that an audio application can achieve.
    // Typical value: 100000 <=> 0.01 sec = 10ms.
    //
    REFERENCE_TIME devPeriod = 0;
    REFERENCE_TIME devPeriodMin = 0;
    _ptrClientOut->GetDevicePeriod(&devPeriod, &devPeriodMin);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[REND] device period        : %u (%3.2f ms)",
        (DWORD)devPeriod, (double)(devPeriod/10000.0));

    // Derive initial rendering delay.
    // Example: 10*(960/480) + 15 = 20 + 15 = 35ms
    //
    int playout_delay = 10 * (bufferLength / _playBlockSize) +
        (int)((latency + devPeriod) / 10000);
    _sndCardPlayDelay = playout_delay;
    _writtenSamples = 0;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "[REND] initial delay        : %u", playout_delay);

    double endpointBufferSizeMS = 10.0 * ((double)bufferLength / (double)_devicePlayBlockSize);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[REND] endpointBufferSizeMS : %3.2f", endpointBufferSizeMS);

    // Before starting the stream, fill the rendering buffer with silence.
    //
    BYTE *pData = NULL;
    hr = _ptrRenderClient->GetBuffer(bufferLength, &pData);
    EXIT_ON_ERROR(hr);

    hr = _ptrRenderClient->ReleaseBuffer(bufferLength, AUDCLNT_BUFFERFLAGS_SILENT);
    EXIT_ON_ERROR(hr);

    _writtenSamples += bufferLength;

    hr = _ptrClientOut->GetService(__uuidof(IAudioClock), (void**)&clock);
    if (FAILED(hr)) {
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   "failed to get IAudioClock interface from the IAudioClient");
    }

    // Start up the rendering audio stream.
    hr = _ptrClientOut->Start();
    EXIT_ON_ERROR(hr);

    _UnLock();

    // Set event which will ensure that the calling thread modifies the playing state to true.
    //
    SetEvent(_hRenderStartedEvent);

    // >> ------------------ THREAD LOOP ------------------

    while (keepPlaying)
    {
        // Wait for a render notification event or a shutdown event
        DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, 500);
        switch (waitResult)
        {
        case WAIT_OBJECT_0 + 0:     // _hShutdownRenderEvent
            keepPlaying = false;
            break;
        case WAIT_OBJECT_0 + 1:     // _hRenderSamplesReadyEvent
            break;
        case WAIT_TIMEOUT:          // timeout notification
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "render event timed out after 0.5 seconds");
            goto Exit;
        default:                    // unexpected error
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "unknown wait termination on render side");
            goto Exit;
        }

        while (keepPlaying)
        {
            _Lock();

            // Sanity check to ensure that essential states are not modified
            // during the unlocked period.
            if (_ptrRenderClient == NULL || _ptrClientOut == NULL)
            {
                _UnLock();
                WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                    "output state has been modified during unlocked period");
                goto Exit;
            }

            // Get the number of frames of padding (queued up to play) in the endpoint buffer.
            UINT32 padding = 0;
            hr = _ptrClientOut->GetCurrentPadding(&padding);
            EXIT_ON_ERROR(hr);

            // Derive the amount of available space in the output buffer
            uint32_t framesAvailable = bufferLength - padding;
            // WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "#avaliable audio frames = %u", framesAvailable);

            // Do we have 10 ms available in the render buffer?
            if (framesAvailable < _playBlockSize)
            {
                // Not enough space in render buffer to store next render packet.
                _UnLock();
                break;
            }

            // Write n*10ms buffers to the render buffer
            const uint32_t n10msBuffers = (framesAvailable / _playBlockSize);
            for (uint32_t n = 0; n < n10msBuffers; n++)
            {
                // Get pointer (i.e., grab the buffer) to next space in the shared render buffer.
                hr = _ptrRenderClient->GetBuffer(_playBlockSize, &pData);
                EXIT_ON_ERROR(hr);

                QueryPerformanceCounter(&t1);    // measure time: START

                if (_ptrAudioBuffer)
                {
                    // Request data to be played out (#bytes = _playBlockSize*_audioFrameSize)
                    _UnLock();
                    int32_t nSamples =
                    _ptrAudioBuffer->RequestPlayoutData(_playBlockSize);
                    _Lock();

                    if (nSamples == -1)
                    {
                        _UnLock();
                        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                                     "failed to read data from render client");
                        goto Exit;
                    }

                    // Sanity check to ensure that essential states are not modified during the unlocked period
                    if (_ptrRenderClient == NULL || _ptrClientOut == NULL)
                    {
                        _UnLock();
                        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id, "output state has been modified during unlocked period");
                        goto Exit;
                    }
                    if (nSamples != static_cast<int32_t>(_playBlockSize))
                    {
                        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "nSamples(%d) != _playBlockSize(%d)", nSamples, _playBlockSize);
                    }

                    // Get the actual (stored) data
                    nSamples = _ptrAudioBuffer->GetPlayoutData((int8_t*)pData);
                }

                QueryPerformanceCounter(&t2);    // measure time: STOP
                time = (int)(t2.QuadPart-t1.QuadPart);
                _playAcc += time;

                DWORD dwFlags(0);
                hr = _ptrRenderClient->ReleaseBuffer(_playBlockSize, dwFlags);
                // See http://msdn.microsoft.com/en-us/library/dd316605(VS.85).aspx
                // for more details regarding AUDCLNT_E_DEVICE_INVALIDATED.
                EXIT_ON_ERROR(hr);

                _writtenSamples += _playBlockSize;
            }

            // Check the current delay on the playout side.
            if (clock) {
              UINT64 pos = 0;
              UINT64 freq = 1;
              clock->GetPosition(&pos, NULL);
              clock->GetFrequency(&freq);
              playout_delay = ROUND((double(_writtenSamples) /
                  _devicePlaySampleRate - double(pos) / freq) * 1000.0);
              _sndCardPlayDelay = playout_delay;
            }

            _UnLock();
        }
    }

    // ------------------ THREAD LOOP ------------------ <<

    SleepMs(static_cast<DWORD>(endpointBufferSizeMS+0.5));
    hr = _ptrClientOut->Stop();

Exit:
    SAFE_RELEASE(clock);

    if (FAILED(hr))
    {
        _ptrClientOut->Stop();
        _UnLock();
        _TraceCOMError(hr);
    }

    if (_winSupportAvrt)
    {
        if (NULL != hMmTask)
        {
            _PAvRevertMmThreadCharacteristics(hMmTask);
        }
    }

    _Lock();

    if (keepPlaying)
    {
        if (_ptrClientOut != NULL)
        {
            hr = _ptrClientOut->Stop();
            if (FAILED(hr))
            {
                _TraceCOMError(hr);
            }
            hr = _ptrClientOut->Reset();
            if (FAILED(hr))
            {
                _TraceCOMError(hr);
            }
        }
        // Trigger callback from module process thread
        _playError = 1;
        WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kPlayoutError message posted: rendering thread has ended pre-maturely");
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_Rendering thread is now terminated properly");
    }

    _UnLock();

    return (DWORD)hr;
}

DWORD AudioDeviceWindowsCore::InitCaptureThreadPriority()
{
    _hMmTask = NULL;

    rtc::SetCurrentThreadName("webrtc_core_audio_capture_thread");

    // Use Multimedia Class Scheduler Service (MMCSS) to boost the thread
    // priority.
    if (_winSupportAvrt)
    {
        DWORD taskIndex(0);
        _hMmTask = _PAvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
        if (_hMmTask)
        {
            if (!_PAvSetMmThreadPriority(_hMmTask, AVRT_PRIORITY_CRITICAL))
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                    "failed to boost rec-thread using MMCSS");
            }
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                "capture thread is now registered with MMCSS (taskIndex=%d)",
                taskIndex);
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "failed to enable MMCSS on capture thread (err=%d)",
                GetLastError());
            _TraceCOMError(GetLastError());
        }
    }

    return S_OK;
}

void AudioDeviceWindowsCore::RevertCaptureThreadPriority()
{
    if (_winSupportAvrt)
    {
        if (NULL != _hMmTask)
        {
            _PAvRevertMmThreadCharacteristics(_hMmTask);
        }
    }

    _hMmTask = NULL;
}

DWORD AudioDeviceWindowsCore::DoCaptureThreadPollDMO()
{
    assert(_mediaBuffer != NULL);
    bool keepRecording = true;

    // Initialize COM as MTA in this thread.
    ScopedCOMInitializer comInit(ScopedCOMInitializer::kMTA);
    if (!comInit.succeeded()) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "failed to initialize COM in polling DMO thread");
      return 1;
    }

    HRESULT hr = InitCaptureThreadPriority();
    if (FAILED(hr))
    {
        return hr;
    }

    // Set event which will ensure that the calling thread modifies the
    // recording state to true.
    SetEvent(_hCaptureStartedEvent);

    // >> ---------------------------- THREAD LOOP ----------------------------
    while (keepRecording)
    {
        // Poll the DMO every 5 ms.
        // (The same interval used in the Wave implementation.)
        DWORD waitResult = WaitForSingleObject(_hShutdownCaptureEvent, 5);
        switch (waitResult)
        {
        case WAIT_OBJECT_0:         // _hShutdownCaptureEvent
            keepRecording = false;
            break;
        case WAIT_TIMEOUT:          // timeout notification
            break;
        default:                    // unexpected error
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "Unknown wait termination on capture side");
            hr = -1; // To signal an error callback.
            keepRecording = false;
            break;
        }

        while (keepRecording)
        {
            CriticalSectionScoped critScoped(&_critSect);

            DWORD dwStatus = 0;
            {
                DMO_OUTPUT_DATA_BUFFER dmoBuffer = {0};
                dmoBuffer.pBuffer = _mediaBuffer;
                dmoBuffer.pBuffer->AddRef();

                // Poll the DMO for AEC processed capture data. The DMO will
                // copy available data to |dmoBuffer|, and should only return
                // 10 ms frames. The value of |dwStatus| should be ignored.
                hr = _dmo->ProcessOutput(0, 1, &dmoBuffer, &dwStatus);
                SAFE_RELEASE(dmoBuffer.pBuffer);
                dwStatus = dmoBuffer.dwStatus;
            }
            if (FAILED(hr))
            {
                _TraceCOMError(hr);
                keepRecording = false;
                assert(false);
                break;
            }

            ULONG bytesProduced = 0;
            BYTE* data;
            // Get a pointer to the data buffer. This should be valid until
            // the next call to ProcessOutput.
            hr = _mediaBuffer->GetBufferAndLength(&data, &bytesProduced);
            if (FAILED(hr))
            {
                _TraceCOMError(hr);
                keepRecording = false;
                assert(false);
                break;
            }

            // TODO(andrew): handle AGC.

            if (bytesProduced > 0)
            {
                const int kSamplesProduced = bytesProduced / _recAudioFrameSize;
                // TODO(andrew): verify that this is always satisfied. It might
                // be that ProcessOutput will try to return more than 10 ms if
                // we fail to call it frequently enough.
                assert(kSamplesProduced == static_cast<int>(_recBlockSize));
                assert(sizeof(BYTE) == sizeof(int8_t));
                _ptrAudioBuffer->SetRecordedBuffer(
                    reinterpret_cast<int8_t*>(data),
                    kSamplesProduced);
                _ptrAudioBuffer->SetVQEData(0, 0, 0);

                _UnLock();  // Release lock while making the callback.
                _ptrAudioBuffer->DeliverRecordedData();
                _Lock();
            }

            // Reset length to indicate buffer availability.
            hr = _mediaBuffer->SetLength(0);
            if (FAILED(hr))
            {
                _TraceCOMError(hr);
                keepRecording = false;
                assert(false);
                break;
            }

            if (!(dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE))
            {
                // The DMO cannot currently produce more data. This is the
                // normal case; otherwise it means the DMO had more than 10 ms
                // of data available and ProcessOutput should be called again.
                break;
            }
        }
    }
    // ---------------------------- THREAD LOOP ---------------------------- <<

    RevertCaptureThreadPriority();

    if (FAILED(hr))
    {
        // Trigger callback from module process thread
        _recError = 1;
        WEBRTC_TRACE(kTraceError, kTraceUtility, _id,
            "kRecordingError message posted: capturing thread has ended "
            "prematurely");
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "Capturing thread is now terminated properly");
    }

    return hr;
}


// ----------------------------------------------------------------------------
//  DoCaptureThread
// ----------------------------------------------------------------------------

DWORD AudioDeviceWindowsCore::DoCaptureThread()
{

    bool keepRecording = true;
    HANDLE waitArray[2] = {_hShutdownCaptureEvent, _hCaptureSamplesReadyEvent};
    HRESULT hr = S_OK;

    LARGE_INTEGER t1;
    LARGE_INTEGER t2;
    int32_t time(0);

    BYTE* syncBuffer = NULL;
    UINT32 syncBufIndex = 0;

    _readSamples = 0;

    // Initialize COM as MTA in this thread.
    ScopedCOMInitializer comInit(ScopedCOMInitializer::kMTA);
    if (!comInit.succeeded()) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "failed to initialize COM in capture thread");
      return 1;
    }

    hr = InitCaptureThreadPriority();
    if (FAILED(hr))
    {
        return hr;
    }

    _Lock();

    // Get size of capturing buffer (length is expressed as the number of audio frames the buffer can hold).
    // This value is fixed during the capturing session.
    //
    UINT32 bufferLength = 0;
    if (_ptrClientIn == NULL)
    {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "input state has been modified before capture loop starts.");
      return 1;
    }
    hr = _ptrClientIn->GetBufferSize(&bufferLength);
    EXIT_ON_ERROR(hr);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] size of buffer       : %u", bufferLength);

    // Allocate memory for sync buffer.
    // It is used for compensation between native 44.1 and internal 44.0 and
    // for cases when the capture buffer is larger than 10ms.
    //
    const UINT32 syncBufferSize = 2*(bufferLength * _recAudioFrameSize);
    syncBuffer = new BYTE[syncBufferSize];
    if (syncBuffer == NULL)
    {
        return (DWORD)E_POINTER;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] size of sync buffer  : %u [bytes]", syncBufferSize);

    // Get maximum latency for the current stream (will not change for the lifetime of the IAudioClient object).
    //
    REFERENCE_TIME latency;
    _ptrClientIn->GetStreamLatency(&latency);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] max stream latency   : %u (%3.2f ms)",
        (DWORD)latency, (double)(latency / 10000.0));

    // Get the length of the periodic interval separating successive processing passes by
    // the audio engine on the data in the endpoint buffer.
    //
    REFERENCE_TIME devPeriod = 0;
    REFERENCE_TIME devPeriodMin = 0;
    _ptrClientIn->GetDevicePeriod(&devPeriod, &devPeriodMin);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] device period        : %u (%3.2f ms)",
        (DWORD)devPeriod, (double)(devPeriod / 10000.0));

    double extraDelayMS = (double)((latency + devPeriod) / 10000.0);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] extraDelayMS         : %3.2f", extraDelayMS);

    double endpointBufferSizeMS = 10.0 * ((double)bufferLength / (double)_recBlockSize);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] endpointBufferSizeMS : %3.2f", endpointBufferSizeMS);

    // Start up the capturing stream.
    //
    hr = _ptrClientIn->Start();
    EXIT_ON_ERROR(hr);

    _UnLock();

    // Set event which will ensure that the calling thread modifies the recording state to true.
    //
    SetEvent(_hCaptureStartedEvent);

    // >> ---------------------------- THREAD LOOP ----------------------------

    while (keepRecording)
    {
        // Wait for a capture notification event or a shutdown event
        DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, 500);
        switch (waitResult)
        {
        case WAIT_OBJECT_0 + 0:        // _hShutdownCaptureEvent
            keepRecording = false;
            break;
        case WAIT_OBJECT_0 + 1:        // _hCaptureSamplesReadyEvent
            break;
        case WAIT_TIMEOUT:            // timeout notification
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "capture event timed out after 0.5 seconds");
            goto Exit;
        default:                    // unexpected error
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "unknown wait termination on capture side");
            goto Exit;
        }

        while (keepRecording)
        {
            BYTE *pData = 0;
            UINT32 framesAvailable = 0;
            DWORD flags = 0;
            UINT64 recTime = 0;
            UINT64 recPos = 0;

            _Lock();

            // Sanity check to ensure that essential states are not modified
            // during the unlocked period.
            if (_ptrCaptureClient == NULL || _ptrClientIn == NULL)
            {
                _UnLock();
                WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                    "input state has been modified during unlocked period");
                goto Exit;
            }

            //  Find out how much capture data is available
            //
            hr = _ptrCaptureClient->GetBuffer(&pData,           // packet which is ready to be read by used
                                              &framesAvailable, // #frames in the captured packet (can be zero)
                                              &flags,           // support flags (check)
                                              &recPos,          // device position of first audio frame in data packet
                                              &recTime);        // value of performance counter at the time of recording the first audio frame

            if (SUCCEEDED(hr))
            {
                if (AUDCLNT_S_BUFFER_EMPTY == hr)
                {
                    // Buffer was empty => start waiting for a new capture notification event
                    _UnLock();
                    break;
                }

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                {
                    // Treat all of the data in the packet as silence and ignore the actual data values.
                    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "AUDCLNT_BUFFERFLAGS_SILENT");
                    pData = NULL;
                }

                assert(framesAvailable != 0);

                if (pData)
                {
                    CopyMemory(&syncBuffer[syncBufIndex*_recAudioFrameSize], pData, framesAvailable*_recAudioFrameSize);
                }
                else
                {
                    ZeroMemory(&syncBuffer[syncBufIndex*_recAudioFrameSize], framesAvailable*_recAudioFrameSize);
                }
                assert(syncBufferSize >= (syncBufIndex*_recAudioFrameSize)+framesAvailable*_recAudioFrameSize);

                // Release the capture buffer
                //
                hr = _ptrCaptureClient->ReleaseBuffer(framesAvailable);
                EXIT_ON_ERROR(hr);

                _readSamples += framesAvailable;
                syncBufIndex += framesAvailable;

                QueryPerformanceCounter(&t1);

                // Get the current recording and playout delay.
                uint32_t sndCardRecDelay = (uint32_t)
                    (((((UINT64)t1.QuadPart * _perfCounterFactor) - recTime)
                        / 10000) + (10*syncBufIndex) / _recBlockSize - 10);
                uint32_t sndCardPlayDelay =
                    static_cast<uint32_t>(_sndCardPlayDelay);

                _sndCardRecDelay = sndCardRecDelay;

                while (syncBufIndex >= _recBlockSize)
                {
                    if (_ptrAudioBuffer)
                    {
                        _ptrAudioBuffer->SetRecordedBuffer((const int8_t*)syncBuffer, _recBlockSize);
                        _ptrAudioBuffer->SetVQEData(sndCardPlayDelay,
                                                    sndCardRecDelay,
                                                    0);

                        _ptrAudioBuffer->SetTypingStatus(KeyPressed());

                        QueryPerformanceCounter(&t1);    // measure time: START

                        _UnLock();  // release lock while making the callback
                        _ptrAudioBuffer->DeliverRecordedData();
                        _Lock();    // restore the lock

                        QueryPerformanceCounter(&t2);    // measure time: STOP

                        // Measure "average CPU load".
                        // Basically what we do here is to measure how many percent of our 10ms period
                        // is used for encoding and decoding. This value shuld be used as a warning indicator
                        // only and not seen as an absolute value. Running at ~100% will lead to bad QoS.
                        time = (int)(t2.QuadPart - t1.QuadPart);
                        _avgCPULoad = (float)(_avgCPULoad*.99 + (time + _playAcc) / (double)(_perfCounterFreq.QuadPart));
                        _playAcc = 0;

                        // Sanity check to ensure that essential states are not modified during the unlocked period
                        if (_ptrCaptureClient == NULL || _ptrClientIn == NULL)
                        {
                            _UnLock();
                            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id, "input state has been modified during unlocked period");
                            goto Exit;
                        }
                    }

                    // store remaining data which was not able to deliver as 10ms segment
                    MoveMemory(&syncBuffer[0], &syncBuffer[_recBlockSize*_recAudioFrameSize], (syncBufIndex-_recBlockSize)*_recAudioFrameSize);
                    syncBufIndex -= _recBlockSize;
                    sndCardRecDelay -= 10;
                }

                if (_AGC)
                {
                    uint32_t newMicLevel = _ptrAudioBuffer->NewMicLevel();
                    if (newMicLevel != 0)
                    {
                        // The VQE will only deliver non-zero microphone levels when a change is needed.
                        // Set this new mic level (received from the observer as return value in the callback).
                        WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "AGC change of volume: new=%u",  newMicLevel);
                        // We store this outside of the audio buffer to avoid
                        // having it overwritten by the getter thread.
                        _newMicLevel = newMicLevel;
                        SetEvent(_hSetCaptureVolumeEvent);
                    }
                }
            }
            else
            {
                // If GetBuffer returns AUDCLNT_E_BUFFER_ERROR, the thread consuming the audio samples
                // must wait for the next processing pass. The client might benefit from keeping a count
                // of the failed GetBuffer calls. If GetBuffer returns this error repeatedly, the client
                // can start a new processing loop after shutting down the current client by calling
                // IAudioClient::Stop, IAudioClient::Reset, and releasing the audio client.
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "IAudioCaptureClient::GetBuffer returned AUDCLNT_E_BUFFER_ERROR, hr = 0x%08X",  hr);
                goto Exit;
            }

            _UnLock();
        }
    }

    // ---------------------------- THREAD LOOP ---------------------------- <<

    if (_ptrClientIn)
    {
        hr = _ptrClientIn->Stop();
    }

Exit:
    if (FAILED(hr))
    {
        _ptrClientIn->Stop();
        _UnLock();
        _TraceCOMError(hr);
    }

    RevertCaptureThreadPriority();

    _Lock();

    if (keepRecording)
    {
        if (_ptrClientIn != NULL)
        {
            hr = _ptrClientIn->Stop();
            if (FAILED(hr))
            {
                _TraceCOMError(hr);
            }
            hr = _ptrClientIn->Reset();
            if (FAILED(hr))
            {
                _TraceCOMError(hr);
            }
        }

        // Trigger callback from module process thread
        _recError = 1;
        WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kRecordingError message posted: capturing thread has ended pre-maturely");
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_Capturing thread is now terminated properly");
    }

    SAFE_RELEASE(_ptrClientIn);
    SAFE_RELEASE(_ptrCaptureClient);

    _UnLock();

    if (syncBuffer)
    {
        delete [] syncBuffer;
    }

    return (DWORD)hr;
}

int32_t AudioDeviceWindowsCore::EnableBuiltInAEC(bool enable)
{

    if (_recIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "Attempt to set Windows AEC with recording already initialized");
        return -1;
    }

    if (_dmo == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "Built-in AEC DMO was not initialized properly at create time");
        return -1;
    }

    _builtInAecEnabled = enable;
    return 0;
}

int AudioDeviceWindowsCore::SetDMOProperties()
{
    HRESULT hr = S_OK;
    assert(_dmo != NULL);

    rtc::scoped_refptr<IPropertyStore> ps;
    {
        IPropertyStore* ptrPS = NULL;
        hr = _dmo->QueryInterface(IID_IPropertyStore,
                                  reinterpret_cast<void**>(&ptrPS));
        if (FAILED(hr) || ptrPS == NULL)
        {
            _TraceCOMError(hr);
            return -1;
        }
        ps = ptrPS;
        SAFE_RELEASE(ptrPS);
    }

    // Set the AEC system mode.
    // SINGLE_CHANNEL_AEC - AEC processing only.
    if (SetVtI4Property(ps,
                        MFPKEY_WMAAECMA_SYSTEM_MODE,
                        SINGLE_CHANNEL_AEC))
    {
        return -1;
    }

    // Set the AEC source mode.
    // VARIANT_TRUE - Source mode (we poll the AEC for captured data).
    if (SetBoolProperty(ps,
                        MFPKEY_WMAAECMA_DMO_SOURCE_MODE,
                        VARIANT_TRUE) == -1)
    {
        return -1;
    }

    // Enable the feature mode.
    // This lets us override all the default processing settings below.
    if (SetBoolProperty(ps,
                        MFPKEY_WMAAECMA_FEATURE_MODE,
                        VARIANT_TRUE) == -1)
    {
        return -1;
    }

    // Disable analog AGC (default enabled).
    if (SetBoolProperty(ps,
                        MFPKEY_WMAAECMA_MIC_GAIN_BOUNDER,
                        VARIANT_FALSE) == -1)
    {
        return -1;
    }

    // Disable noise suppression (default enabled).
    // 0 - Disabled, 1 - Enabled
    if (SetVtI4Property(ps,
                        MFPKEY_WMAAECMA_FEATR_NS,
                        0) == -1)
    {
        return -1;
    }

    // Relevant parameters to leave at default settings:
    // MFPKEY_WMAAECMA_FEATR_AGC - Digital AGC (disabled).
    // MFPKEY_WMAAECMA_FEATR_CENTER_CLIP - AEC center clipping (enabled).
    // MFPKEY_WMAAECMA_FEATR_ECHO_LENGTH - Filter length (256 ms).
    //   TODO(andrew): investigate decresing the length to 128 ms.
    // MFPKEY_WMAAECMA_FEATR_FRAME_SIZE - Frame size (0).
    //   0 is automatic; defaults to 160 samples (or 10 ms frames at the
    //   selected 16 kHz) as long as mic array processing is disabled.
    // MFPKEY_WMAAECMA_FEATR_NOISE_FILL - Comfort noise (enabled).
    // MFPKEY_WMAAECMA_FEATR_VAD - VAD (disabled).

    // Set the devices selected by VoE. If using a default device, we need to
    // search for the device index.
    int inDevIndex = _inputDeviceIndex;
    int outDevIndex = _outputDeviceIndex;
    if (!_usingInputDeviceIndex)
    {
        ERole role = eCommunications;
        if (_inputDevice == AudioDeviceModule::kDefaultDevice)
        {
            role = eConsole;
        }

        if (_GetDefaultDeviceIndex(eCapture, role, &inDevIndex) == -1)
        {
            return -1;
        }
    }

    if (!_usingOutputDeviceIndex)
    {
        ERole role = eCommunications;
        if (_outputDevice == AudioDeviceModule::kDefaultDevice)
        {
            role = eConsole;
        }

        if (_GetDefaultDeviceIndex(eRender, role, &outDevIndex) == -1)
        {
            return -1;
        }
    }

    DWORD devIndex = static_cast<uint32_t>(outDevIndex << 16) +
                     static_cast<uint32_t>(0x0000ffff & inDevIndex);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "Capture device index: %d, render device index: %d",
        inDevIndex, outDevIndex);
    if (SetVtI4Property(ps,
                        MFPKEY_WMAAECMA_DEVICE_INDEXES,
                        devIndex) == -1)
    {
        return -1;
    }

    return 0;
}

int AudioDeviceWindowsCore::SetBoolProperty(IPropertyStore* ptrPS,
                                            REFPROPERTYKEY key,
                                            VARIANT_BOOL value)
{
    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_BOOL;
    pv.boolVal = value;
    HRESULT hr = ptrPS->SetValue(key, pv);
    PropVariantClear(&pv);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        return -1;
    }
    return 0;
}

int AudioDeviceWindowsCore::SetVtI4Property(IPropertyStore* ptrPS,
                                            REFPROPERTYKEY key,
                                            LONG value)
{
    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_I4;
    pv.lVal = value;
    HRESULT hr = ptrPS->SetValue(key, pv);
    PropVariantClear(&pv);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
//  _RefreshDeviceList
//
//  Creates a new list of endpoint rendering or capture devices after
//  deleting any previously created (and possibly out-of-date) list of
//  such devices.
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::_RefreshDeviceList(EDataFlow dir)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    IMMDeviceCollection *pCollection = NULL;

    assert(dir == eRender || dir == eCapture);
    assert(_ptrEnumerator != NULL);

    // Create a fresh list of devices using the specified direction
    hr = _ptrEnumerator->EnumAudioEndpoints(
                           dir,
                           DEVICE_STATE_ACTIVE,
                           &pCollection);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pCollection);
        return -1;
    }

    if (dir == eRender)
    {
        SAFE_RELEASE(_ptrRenderCollection);
        _ptrRenderCollection = pCollection;
    }
    else
    {
        SAFE_RELEASE(_ptrCaptureCollection);
        _ptrCaptureCollection = pCollection;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  _DeviceListCount
//
//  Gets a count of the endpoint rendering or capture devices in the
//  current list of such devices.
// ----------------------------------------------------------------------------

int16_t AudioDeviceWindowsCore::_DeviceListCount(EDataFlow dir)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    UINT count = 0;

    assert(eRender == dir || eCapture == dir);

    if (eRender == dir && NULL != _ptrRenderCollection)
    {
        hr = _ptrRenderCollection->GetCount(&count);
    }
    else if (NULL != _ptrCaptureCollection)
    {
        hr = _ptrCaptureCollection->GetCount(&count);
    }

    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        return -1;
    }

    return static_cast<int16_t> (count);
}

// ----------------------------------------------------------------------------
//  _GetListDeviceName
//
//  Gets the friendly name of an endpoint rendering or capture device
//  from the current list of such devices. The caller uses an index
//  into the list to identify the device.
//
//  Uses: _ptrRenderCollection or _ptrCaptureCollection which is updated
//  in _RefreshDeviceList().
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::_GetListDeviceName(EDataFlow dir, int index, LPWSTR szBuffer, int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    IMMDevice *pDevice = NULL;

    assert(dir == eRender || dir == eCapture);

    if (eRender == dir && NULL != _ptrRenderCollection)
    {
        hr = _ptrRenderCollection->Item(index, &pDevice);
    }
    else if (NULL != _ptrCaptureCollection)
    {
        hr = _ptrCaptureCollection->Item(index, &pDevice);
    }

    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pDevice);
        return -1;
    }

    int32_t res = _GetDeviceName(pDevice, szBuffer, bufferLen);
    SAFE_RELEASE(pDevice);
    return res;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDeviceName
//
//  Gets the friendly name of an endpoint rendering or capture device
//  given a specified device role.
//
//  Uses: _ptrEnumerator
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::_GetDefaultDeviceName(EDataFlow dir, ERole role, LPWSTR szBuffer, int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    IMMDevice *pDevice = NULL;

    assert(dir == eRender || dir == eCapture);
    assert(role == eConsole || role == eCommunications);
    assert(_ptrEnumerator != NULL);

    hr = _ptrEnumerator->GetDefaultAudioEndpoint(
                           dir,
                           role,
                           &pDevice);

    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pDevice);
        return -1;
    }

    int32_t res = _GetDeviceName(pDevice, szBuffer, bufferLen);
    SAFE_RELEASE(pDevice);
    return res;
}

// ----------------------------------------------------------------------------
//  _GetListDeviceID
//
//  Gets the unique ID string of an endpoint rendering or capture device
//  from the current list of such devices. The caller uses an index
//  into the list to identify the device.
//
//  Uses: _ptrRenderCollection or _ptrCaptureCollection which is updated
//  in _RefreshDeviceList().
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::_GetListDeviceID(EDataFlow dir, int index, LPWSTR szBuffer, int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    IMMDevice *pDevice = NULL;

    assert(dir == eRender || dir == eCapture);

    if (eRender == dir && NULL != _ptrRenderCollection)
    {
        hr = _ptrRenderCollection->Item(index, &pDevice);
    }
    else if (NULL != _ptrCaptureCollection)
    {
        hr = _ptrCaptureCollection->Item(index, &pDevice);
    }

    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pDevice);
        return -1;
    }

    int32_t res = _GetDeviceID(pDevice, szBuffer, bufferLen);
    SAFE_RELEASE(pDevice);
    return res;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDeviceID
//
//  Gets the uniqe device ID of an endpoint rendering or capture device
//  given a specified device role.
//
//  Uses: _ptrEnumerator
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::_GetDefaultDeviceID(EDataFlow dir, ERole role, LPWSTR szBuffer, int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    IMMDevice *pDevice = NULL;

    assert(dir == eRender || dir == eCapture);
    assert(role == eConsole || role == eCommunications);
    assert(_ptrEnumerator != NULL);

    hr = _ptrEnumerator->GetDefaultAudioEndpoint(
                           dir,
                           role,
                           &pDevice);

    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pDevice);
        return -1;
    }

    int32_t res = _GetDeviceID(pDevice, szBuffer, bufferLen);
    SAFE_RELEASE(pDevice);
    return res;
}

int32_t AudioDeviceWindowsCore::_GetDefaultDeviceIndex(EDataFlow dir,
                                                       ERole role,
                                                       int* index)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    WCHAR szDefaultDeviceID[MAX_PATH] = {0};
    WCHAR szDeviceID[MAX_PATH] = {0};

    const size_t kDeviceIDLength = sizeof(szDeviceID)/sizeof(szDeviceID[0]);
    assert(kDeviceIDLength ==
        sizeof(szDefaultDeviceID) / sizeof(szDefaultDeviceID[0]));

    if (_GetDefaultDeviceID(dir,
                            role,
                            szDefaultDeviceID,
                            kDeviceIDLength) == -1)
    {
        return -1;
    }

    IMMDeviceCollection* collection = _ptrCaptureCollection;
    if (dir == eRender)
    {
        collection = _ptrRenderCollection;
    }

    if (!collection)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "Device collection not valid");
        return -1;
    }

    UINT count = 0;
    hr = collection->GetCount(&count);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        return -1;
    }

    *index = -1;
    for (UINT i = 0; i < count; i++)
    {
        memset(szDeviceID, 0, sizeof(szDeviceID));
        rtc::scoped_refptr<IMMDevice> device;
        {
            IMMDevice* ptrDevice = NULL;
            hr = collection->Item(i, &ptrDevice);
            if (FAILED(hr) || ptrDevice == NULL)
            {
                _TraceCOMError(hr);
                return -1;
            }
            device = ptrDevice;
            SAFE_RELEASE(ptrDevice);
        }

        if (_GetDeviceID(device, szDeviceID, kDeviceIDLength) == -1)
        {
           return -1;
        }

        if (wcsncmp(szDefaultDeviceID, szDeviceID, kDeviceIDLength) == 0)
        {
            // Found a match.
            *index = i;
            break;
        }

    }

    if (*index == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "Unable to find collection index for default device");
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  _GetDeviceName
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::_GetDeviceName(IMMDevice* pDevice,
                                               LPWSTR pszBuffer,
                                               int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    static const WCHAR szDefault[] = L"<Device not available>";

    HRESULT hr = E_FAIL;
    IPropertyStore *pProps = NULL;
    PROPVARIANT varName;

    assert(pszBuffer != NULL);
    assert(bufferLen > 0);

    if (pDevice != NULL)
    {
        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (FAILED(hr))
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                "IMMDevice::OpenPropertyStore failed, hr = 0x%08X", hr);
        }
    }

    // Initialize container for property value.
    PropVariantInit(&varName);

    if (SUCCEEDED(hr))
    {
        // Get the endpoint device's friendly-name property.
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        if (FAILED(hr))
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                "IPropertyStore::GetValue failed, hr = 0x%08X", hr);
        }
    }

    if ((SUCCEEDED(hr)) && (VT_EMPTY == varName.vt))
    {
        hr = E_FAIL;
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "IPropertyStore::GetValue returned no value, hr = 0x%08X", hr);
    }

    if ((SUCCEEDED(hr)) && (VT_LPWSTR != varName.vt))
    {
        // The returned value is not a wide null terminated string.
        hr = E_UNEXPECTED;
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "IPropertyStore::GetValue returned unexpected type, hr = 0x%08X", hr);
    }

    if (SUCCEEDED(hr) && (varName.pwszVal != NULL))
    {
        // Copy the valid device name to the provided ouput buffer.
        wcsncpy_s(pszBuffer, bufferLen, varName.pwszVal, _TRUNCATE);
    }
    else
    {
        // Failed to find the device name.
        wcsncpy_s(pszBuffer, bufferLen, szDefault, _TRUNCATE);
    }

    PropVariantClear(&varName);
    SAFE_RELEASE(pProps);

    return 0;
}

// ----------------------------------------------------------------------------
//  _GetDeviceID
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::_GetDeviceID(IMMDevice* pDevice, LPWSTR pszBuffer, int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    static const WCHAR szDefault[] = L"<Device not available>";

    HRESULT hr = E_FAIL;
    LPWSTR pwszID = NULL;

    assert(pszBuffer != NULL);
    assert(bufferLen > 0);

    if (pDevice != NULL)
    {
        hr = pDevice->GetId(&pwszID);
    }

    if (hr == S_OK)
    {
        // Found the device ID.
        wcsncpy_s(pszBuffer, bufferLen, pwszID, _TRUNCATE);
    }
    else
    {
        // Failed to find the device ID.
        wcsncpy_s(pszBuffer, bufferLen, szDefault, _TRUNCATE);
    }

    CoTaskMemFree(pwszID);
    return 0;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDevice
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::_GetDefaultDevice(EDataFlow dir, ERole role, IMMDevice** ppDevice)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr(S_OK);

    assert(_ptrEnumerator != NULL);

    hr = _ptrEnumerator->GetDefaultAudioEndpoint(
                                   dir,
                                   role,
                                   ppDevice);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  _GetListDevice
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::_GetListDevice(EDataFlow dir, int index, IMMDevice** ppDevice)
{
    HRESULT hr(S_OK);

    assert(_ptrEnumerator != NULL);

    IMMDeviceCollection *pCollection = NULL;

    hr = _ptrEnumerator->EnumAudioEndpoints(
                               dir,
                               DEVICE_STATE_ACTIVE,        // only active endpoints are OK
                               &pCollection);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pCollection);
        return -1;
    }

    hr = pCollection->Item(
                        index,
                        ppDevice);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pCollection);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  _EnumerateEndpointDevicesAll
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsCore::_EnumerateEndpointDevicesAll(EDataFlow dataFlow) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    assert(_ptrEnumerator != NULL);

    HRESULT hr = S_OK;
    IMMDeviceCollection *pCollection = NULL;
    IMMDevice *pEndpoint = NULL;
    IPropertyStore *pProps = NULL;
    IAudioEndpointVolume* pEndpointVolume = NULL;
    LPWSTR pwszID = NULL;

    // Generate a collection of audio endpoint devices in the system.
    // Get states for *all* endpoint devices.
    // Output: IMMDeviceCollection interface.
    hr = _ptrEnumerator->EnumAudioEndpoints(
                                 dataFlow,            // data-flow direction (input parameter)
                                 DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED | DEVICE_STATE_UNPLUGGED,
                                 &pCollection);        // release interface when done

    EXIT_ON_ERROR(hr);

    // use the IMMDeviceCollection interface...

    UINT count = 0;

    // Retrieve a count of the devices in the device collection.
    hr = pCollection->GetCount(&count);
    EXIT_ON_ERROR(hr);
    if (dataFlow == eRender)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#rendering endpoint devices (counting all): %u", count);
    else if (dataFlow == eCapture)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#capturing endpoint devices (counting all): %u", count);

    if (count == 0)
    {
        return 0;
    }

    // Each loop prints the name of an endpoint device.
    for (ULONG i = 0; i < count; i++)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Endpoint %d:", i);

        // Get pointer to endpoint number i.
        // Output: IMMDevice interface.
        hr = pCollection->Item(
                            i,
                            &pEndpoint);
        CONTINUE_ON_ERROR(hr);

        // use the IMMDevice interface of the specified endpoint device...

        // Get the endpoint ID string (uniquely identifies the device among all audio endpoint devices)
        hr = pEndpoint->GetId(&pwszID);
        CONTINUE_ON_ERROR(hr);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "ID string    : %S", pwszID);

        // Retrieve an interface to the device's property store.
        // Output: IPropertyStore interface.
        hr = pEndpoint->OpenPropertyStore(
                          STGM_READ,
                          &pProps);
        CONTINUE_ON_ERROR(hr);

        // use the IPropertyStore interface...

        PROPVARIANT varName;
        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        // Example: "Speakers (Realtek High Definition Audio)"
        hr = pProps->GetValue(
                       PKEY_Device_FriendlyName,
                       &varName);
        CONTINUE_ON_ERROR(hr);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"", varName.pwszVal);

        // Get the endpoint's current device state
        DWORD dwState;
        hr = pEndpoint->GetState(&dwState);
        CONTINUE_ON_ERROR(hr);
        if (dwState & DEVICE_STATE_ACTIVE)
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "state (0x%x)  : *ACTIVE*", dwState);
        if (dwState & DEVICE_STATE_DISABLED)
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "state (0x%x)  : DISABLED", dwState);
        if (dwState & DEVICE_STATE_NOTPRESENT)
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "state (0x%x)  : NOTPRESENT", dwState);
        if (dwState & DEVICE_STATE_UNPLUGGED)
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "state (0x%x)  : UNPLUGGED", dwState);

        // Check the hardware volume capabilities.
        DWORD dwHwSupportMask = 0;
        hr = pEndpoint->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                               NULL, (void**)&pEndpointVolume);
        CONTINUE_ON_ERROR(hr);
        hr = pEndpointVolume->QueryHardwareSupport(&dwHwSupportMask);
        CONTINUE_ON_ERROR(hr);
        if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_VOLUME)
            // The audio endpoint device supports a hardware volume control
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "hwmask (0x%x) : HARDWARE_SUPPORT_VOLUME", dwHwSupportMask);
        if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_MUTE)
            // The audio endpoint device supports a hardware mute control
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "hwmask (0x%x) : HARDWARE_SUPPORT_MUTE", dwHwSupportMask);
        if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_METER)
            // The audio endpoint device supports a hardware peak meter
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "hwmask (0x%x) : HARDWARE_SUPPORT_METER", dwHwSupportMask);

        // Check the channel count (#channels in the audio stream that enters or leaves the audio endpoint device)
        UINT nChannelCount(0);
        hr = pEndpointVolume->GetChannelCount(
                                &nChannelCount);
        CONTINUE_ON_ERROR(hr);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#channels    : %u", nChannelCount);

        if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_VOLUME)
        {
            // Get the volume range.
            float fLevelMinDB(0.0);
            float fLevelMaxDB(0.0);
            float fVolumeIncrementDB(0.0);
            hr = pEndpointVolume->GetVolumeRange(
                                    &fLevelMinDB,
                                    &fLevelMaxDB,
                                    &fVolumeIncrementDB);
            CONTINUE_ON_ERROR(hr);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "volume range : %4.2f (min), %4.2f (max), %4.2f (inc) [dB]",
                fLevelMinDB, fLevelMaxDB, fVolumeIncrementDB);

            // The volume range from vmin = fLevelMinDB to vmax = fLevelMaxDB is divided
            // into n uniform intervals of size vinc = fVolumeIncrementDB, where
            // n = (vmax ?vmin) / vinc.
            // The values vmin, vmax, and vinc are measured in decibels. The client can set
            // the volume level to one of n + 1 discrete values in the range from vmin to vmax.
            int n = (int)((fLevelMaxDB-fLevelMinDB)/fVolumeIncrementDB);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#intervals   : %d", n);

            // Get information about the current step in the volume range.
            // This method represents the volume level of the audio stream that enters or leaves
            // the audio endpoint device as an index or "step" in a range of discrete volume levels.
            // Output value nStepCount is the number of steps in the range. Output value nStep
            // is the step index of the current volume level. If the number of steps is n = nStepCount,
            // then step index nStep can assume values from 0 (minimum volume) to n ?1 (maximum volume).
            UINT nStep(0);
            UINT nStepCount(0);
            hr = pEndpointVolume->GetVolumeStepInfo(
                                    &nStep,
                                    &nStepCount);
            CONTINUE_ON_ERROR(hr);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "volume steps : %d (nStep), %d (nStepCount)", nStep, nStepCount);
        }
Next:
        if (FAILED(hr)) {
          WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                       "Error when logging device information");
        }
        CoTaskMemFree(pwszID);
        pwszID = NULL;
        PropVariantClear(&varName);
        SAFE_RELEASE(pProps);
        SAFE_RELEASE(pEndpoint);
        SAFE_RELEASE(pEndpointVolume);
    }
    SAFE_RELEASE(pCollection);
    return 0;

Exit:
    _TraceCOMError(hr);
    CoTaskMemFree(pwszID);
    pwszID = NULL;
    SAFE_RELEASE(pCollection);
    SAFE_RELEASE(pEndpoint);
    SAFE_RELEASE(pEndpointVolume);
    SAFE_RELEASE(pProps);
    return -1;
}

// ----------------------------------------------------------------------------
//  _TraceCOMError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::_TraceCOMError(HRESULT hr) const
{
    TCHAR buf[MAXERRORLENGTH];
    TCHAR errorText[MAXERRORLENGTH];

    const DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD dwLangID = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

    // Gets the system's human readable message string for this HRESULT.
    // All error message in English by default.
    DWORD messageLength = ::FormatMessageW(dwFlags,
                                           0,
                                           hr,
                                           dwLangID,
                                           errorText,
                                           MAXERRORLENGTH,
                                           NULL);

    assert(messageLength <= MAXERRORLENGTH);

    // Trims tailing white space (FormatMessage() leaves a trailing cr-lf.).
    for (; messageLength && ::isspace(errorText[messageLength - 1]);
         --messageLength)
    {
        errorText[messageLength - 1] = '\0';
    }

    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "Core Audio method failed (hr=0x%x)", hr);
    StringCchPrintf(buf, MAXERRORLENGTH, TEXT("Error details: "));
    StringCchCat(buf, MAXERRORLENGTH, errorText);
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "%s", WideToUTF8(buf));
}

// ----------------------------------------------------------------------------
//  WideToUTF8
// ----------------------------------------------------------------------------

char* AudioDeviceWindowsCore::WideToUTF8(const TCHAR* src) const {
#ifdef UNICODE
    const size_t kStrLen = sizeof(_str);
    memset(_str, 0, kStrLen);
    // Get required size (in bytes) to be able to complete the conversion.
    unsigned int required_size = (unsigned int)WideCharToMultiByte(CP_UTF8, 0, src, -1, _str, 0, 0, 0);
    if (required_size <= kStrLen)
    {
        // Process the entire input string, including the terminating null char.
        if (WideCharToMultiByte(CP_UTF8, 0, src, -1, _str, kStrLen, 0, 0) == 0)
            memset(_str, 0, kStrLen);
    }
    return _str;
#else
    return const_cast<char*>(src);
#endif
}


bool AudioDeviceWindowsCore::KeyPressed() const{

  int key_down = 0;
  for (int key = VK_SPACE; key < VK_NUMLOCK; key++) {
    short res = GetAsyncKeyState(key);
    key_down |= res & 0x1; // Get the LSB
  }
  return (key_down > 0);
}
}  // namespace webrtc

#endif  // WEBRTC_WINDOWS_CORE_AUDIO_BUILD
