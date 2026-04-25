/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/wgc_capturer_win.h"

#include <DispatcherQueue.h>
#include <windows.foundation.metadata.h>
#include <windows.graphics.capture.h>

#include <cstdint>
#include <cwchar>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "modules/desktop_capture/desktop_capture_metrics_helper.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/wgc_capture_session.h"
#include "modules/desktop_capture/win/wgc_capture_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/win/get_activation_factory.h"
#include "rtc_base/win/hstring.h"
#include "rtc_base/win/windows_version.h"
#include "system_wrappers/include/metrics.h"

namespace WGC = ABI::Windows::Graphics::Capture;
using Microsoft::WRL::ComPtr;

namespace webrtc {

namespace {

constexpr wchar_t kCoreMessagingDll[] = L"CoreMessaging.dll";

constexpr wchar_t kWgcSessionType[] =
    L"Windows.Graphics.Capture.GraphicsCaptureSession";
constexpr wchar_t kApiContract[] = L"Windows.Foundation.UniversalApiContract";
constexpr wchar_t kDirtyRegionMode[] = L"DirtyRegionMode";
constexpr UINT16 kRequiredApiContractVersion = 8;

enum class WgcCapturerResult {
  kSuccess = 0,
  kNoDirect3dDevice = 1,
  kNoSourceSelected = 2,
  kItemCreationFailure = 3,
  kSessionStartFailure = 4,
  kGetFrameFailure = 5,
  kFrameDropped = 6,
  kCreateDispatcherQueueFailure = 7,
  kMaxValue = kCreateDispatcherQueueFailure
};

void RecordWgcCapturerResult(WgcCapturerResult error) {
  RTC_HISTOGRAM_ENUMERATION("WebRTC.DesktopCapture.Win.WgcCapturerResult",
                            static_cast<int>(error),
                            static_cast<int>(WgcCapturerResult::kMaxValue));
}

// Checks if the DirtyRegionMode property is present in GraphicsCaptureSession
// and logs a boolean histogram with the result.
// TODO(https://crbug.com/40259177): Detecting support for this property means
// that the WGC API supports dirty regions and it can be utilized to improve
// the capture performance and the existing zero-herz support.
void LogDirtyRegionSupport() {
  ComPtr<ABI::Windows::Foundation::Metadata::IApiInformationStatics>
      api_info_statics;
  HRESULT hr = GetActivationFactory<
      ABI::Windows::Foundation::Metadata::IApiInformationStatics,
      RuntimeClass_Windows_Foundation_Metadata_ApiInformation>(
      &api_info_statics);
  if (FAILED(hr)) {
    return;
  }

  HSTRING dirty_region_mode;
  hr = CreateHstring(kDirtyRegionMode, wcslen(kDirtyRegionMode),
                     &dirty_region_mode);
  if (FAILED(hr)) {
    DeleteHstring(dirty_region_mode);
    return;
  }

  HSTRING wgc_session_type;
  hr = CreateHstring(kWgcSessionType, wcslen(kWgcSessionType),
                     &wgc_session_type);
  if (SUCCEEDED(hr)) {
    boolean is_dirty_region_mode_supported =
        api_info_statics->IsPropertyPresent(wgc_session_type, dirty_region_mode,
                                            &is_dirty_region_mode_supported);
    RTC_HISTOGRAM_BOOLEAN("WebRTC.DesktopCapture.Win.WgcDirtyRegionSupport",
                          !!is_dirty_region_mode_supported);
  }
  DeleteHstring(dirty_region_mode);
  DeleteHstring(wgc_session_type);
}

}  // namespace

bool IsWgcSupported(CaptureType capture_type) {
  if (!HasActiveDisplay()) {
    // There is a bug in `CreateForMonitor` that causes a crash if there are no
    // active displays. The crash was fixed in Win11, but we are still unable
    // to capture screens without an active display.
    if (capture_type == CaptureType::kScreen) {
      return false;
    }
    // There is a bug in the DWM (Desktop Window Manager) that prevents it from
    // providing image data if there are no displays attached. This was fixed in
    // Windows 11.
    if (rtc_win::GetVersion() < rtc_win::Version::VERSION_WIN11) {
      return false;
    }
  }

  // A bug in the WGC API `CreateForMonitor` prevents capturing the entire
  // virtual screen (all monitors simultaneously), this was fixed in 20H1. Since
  // we can't assert that we won't be asked to capture the entire virtual
  // screen, we report unsupported so we can fallback to another capturer.
  if (capture_type == CaptureType::kScreen &&
      rtc_win::GetVersion() < rtc_win::Version::VERSION_WIN10_20H1) {
    return false;
  }

  if (!ResolveCoreWinRTDelayload()) {
    return false;
  }

  // We need to check if the WGC APIs are present on the system. Certain SKUs
  // of Windows ship without these APIs.
  ComPtr<ABI::Windows::Foundation::Metadata::IApiInformationStatics>
      api_info_statics;
  HRESULT hr = GetActivationFactory<
      ABI::Windows::Foundation::Metadata::IApiInformationStatics,
      RuntimeClass_Windows_Foundation_Metadata_ApiInformation>(
      &api_info_statics);
  if (FAILED(hr)) {
    return false;
  }

  HSTRING api_contract;
  hr = CreateHstring(kApiContract, wcslen(kApiContract), &api_contract);
  if (FAILED(hr)) {
    return false;
  }

  boolean is_api_present;
  hr = api_info_statics->IsApiContractPresentByMajor(
      api_contract, kRequiredApiContractVersion, &is_api_present);
  DeleteHstring(api_contract);
  if (FAILED(hr) || !is_api_present) {
    return false;
  }

  HSTRING wgc_session_type;
  hr = CreateHstring(kWgcSessionType, wcslen(kWgcSessionType),
                     &wgc_session_type);
  if (FAILED(hr)) {
    return false;
  }

  boolean is_type_present;
  hr = api_info_statics->IsTypePresent(wgc_session_type, &is_type_present);
  DeleteHstring(wgc_session_type);
  if (FAILED(hr) || !is_type_present) {
    return false;
  }

  // If the APIs are present, we need to check that they are supported.
  ComPtr<WGC::IGraphicsCaptureSessionStatics> capture_session_statics;
  hr = GetActivationFactory<
      WGC::IGraphicsCaptureSessionStatics,
      RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureSession>(
      &capture_session_statics);
  if (FAILED(hr)) {
    return false;
  }

  boolean is_supported;
  hr = capture_session_statics->IsSupported(&is_supported);
  if (FAILED(hr) || !is_supported) {
    return false;
  }

  return true;
}

WgcCapturerWin::WgcCapturerWin(
    const DesktopCaptureOptions& options,
    std::unique_ptr<WgcCaptureSourceFactory> source_factory,
    std::unique_ptr<SourceEnumerator> source_enumerator,
    bool allow_delayed_capturable_check)
    : options_(options),
      source_factory_(std::move(source_factory)),
      source_enumerator_(std::move(source_enumerator)),
      allow_delayed_capturable_check_(allow_delayed_capturable_check),
      full_screen_window_detector_(options.full_screen_window_detector()) {
  if (!core_messaging_library_) {
    core_messaging_library_ = LoadLibraryW(kCoreMessagingDll);
  }

  if (core_messaging_library_) {
    create_dispatcher_queue_controller_func_ =
        reinterpret_cast<CreateDispatcherQueueControllerFunc>(GetProcAddress(
            core_messaging_library_, "CreateDispatcherQueueController"));
  }
  LogDirtyRegionSupport();
}

WgcCapturerWin::~WgcCapturerWin() {
  if (core_messaging_library_) {
    FreeLibrary(core_messaging_library_);
  }
}

// static
std::unique_ptr<DesktopCapturer> WgcCapturerWin::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options,
    bool allow_delayed_capturable_check) {
  return std::make_unique<WgcCapturerWin>(
      options, std::make_unique<WgcWindowSourceFactory>(),
      std::make_unique<WindowEnumerator>(
          options.enumerate_current_process_windows()),
      allow_delayed_capturable_check);
}

// static
std::unique_ptr<DesktopCapturer> WgcCapturerWin::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  return std::make_unique<WgcCapturerWin>(
      options, std::make_unique<WgcScreenSourceFactory>(),
      std::make_unique<ScreenEnumerator>(), false);
}

bool WgcCapturerWin::GetSourceList(SourceList* sources) {
  return source_enumerator_->FindAllSources(sources);
}

bool WgcCapturerWin::SelectSource(DesktopCapturer::SourceId id) {
  selected_source_id_ = id;

  if (full_screen_window_detector_ &&
      full_screen_window_detector_->UseHeuristicForFindingEditor() &&
      editor_id_ == 0) {
    // Use `full_screen_window_detector_` to check if the selected `id` is a
    // full screen window, in which case we would like the `selected_source_id_`
    // to store window id of the editor window.
    // If there's a corresponding editor window for the slide show we pretend
    // that the user selected the editor window and then rely on
    // FullscreenWindowDetector to return the actual slide show window below.
    editor_id_ = full_screen_window_detector_->FindEditorWindow(id);
    if (editor_id_ && editor_id_ != selected_source_id_) {
      // Setting `selected_source_id_` to `editor_id_` allows tying the capture
      // session to the editor window instead of the full screen window.
      selected_source_id_ = editor_id_;
      chosen_slide_show_id_ = id;

      // Updating `selected_source_id_` also creates a new
      // application_handler(for the editor window) inside
      // `full_screen_window_detector_` which then needs to be informed about
      // the fact that the slide show was chosen first.
      full_screen_window_detector_->SetEditorWasFoundForChosenSlideShow();
    }
  }
  // Use `full_screen_window_detector_` to check if there is a corresponding
  // full screen window for the selected `id`.
  const DesktopCapturer::SourceId full_screen_source_id =
      full_screen_window_detector_
          ? full_screen_window_detector_->FindFullScreenWindow(id)
          : 0;

  // `capture_id` represents the SourceId used to create  the `capture_source_`,
  // which is the module responsible for capturing the frames.
  auto capture_id = full_screen_source_id ? full_screen_source_id : id;
  if (capture_id != id && !fullscreen_usage_logged_) {
    // Log the usage of FullScreenDetector only once and only if it's
    // successful.
    fullscreen_usage_logged_ = true;
    LogDesktopCapturerFullscreenDetectorUsage();

    if (chosen_slide_show_id_) {
      RTC_HISTOGRAM_BOOLEAN(
          "WebRTC.Screenshare.DesktopCapturerFullScreenFindEditorUsage",
          capture_id == chosen_slide_show_id_);
    }
  }

  if (!capture_source_ || capture_source_->GetSourceId() != capture_id) {
    capture_source_ = source_factory_->CreateCaptureSource(capture_id);
  }

  if (allow_delayed_capturable_check_) {
    return true;
  }

  return capture_source_->IsCapturable();
}

bool WgcCapturerWin::FocusOnSelectedSource() {
  if (!capture_source_) {
    return false;
  }

  return capture_source_->FocusOnSource();
}

void WgcCapturerWin::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);
  RecordCapturerImpl(DesktopCapturerId::kWgcCapturerWin);

  callback_ = callback;

  // Create a Direct3D11 device to share amongst the WgcCaptureSessions. Many
  // parameters are nullptr as the implemention uses defaults that work well for
  // us.
  HRESULT hr = D3D11CreateDevice(
      /*adapter=*/nullptr, D3D_DRIVER_TYPE_HARDWARE,
      /*software_rasterizer=*/nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      /*feature_levels=*/nullptr, /*feature_levels_size=*/0, D3D11_SDK_VERSION,
      &d3d11_device_, /*feature_level=*/nullptr, /*device_context=*/nullptr);
  if (hr == DXGI_ERROR_UNSUPPORTED) {
    // If a hardware device could not be created, use WARP which is a high speed
    // software device.
    hr = D3D11CreateDevice(
        /*adapter=*/nullptr, D3D_DRIVER_TYPE_WARP,
        /*software_rasterizer=*/nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        /*feature_levels=*/nullptr, /*feature_levels_size=*/0,
        D3D11_SDK_VERSION, &d3d11_device_, /*feature_level=*/nullptr,
        /*device_context=*/nullptr);
  }

  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "Failed to create D3D11Device: " << hr;
  }
}

void WgcCapturerWin::CaptureFrame() {
  RTC_DCHECK(callback_);

  if (!capture_source_) {
    RTC_LOG(LS_ERROR) << "Source hasn't been selected";
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    RecordWgcCapturerResult(WgcCapturerResult::kNoSourceSelected);
    return;
  }

  if (!d3d11_device_) {
    RTC_LOG(LS_ERROR) << "No D3D11D3evice, cannot capture.";
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    RecordWgcCapturerResult(WgcCapturerResult::kNoDirect3dDevice);
    return;
  }

  if (allow_delayed_capturable_check_ && !capture_source_->IsCapturable()) {
    RTC_LOG(LS_ERROR) << "Source is not capturable.";
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    return;
  }

  // Feed the actual list of windows into full screen window detector.
  if (full_screen_window_detector_) {
    full_screen_window_detector_->UpdateWindowListIfNeeded(
        selected_source_id_, [this](DesktopCapturer::SourceList* sources) {
          return GetSourceList(sources);
        });
    SelectSource(selected_source_id_);
  }

  HRESULT hr;
  if (!dispatcher_queue_created_) {
    // Set the apartment type to NONE because this thread should already be COM
    // initialized.
    DispatcherQueueOptions options{
        sizeof(DispatcherQueueOptions),
        DISPATCHERQUEUE_THREAD_TYPE::DQTYPE_THREAD_CURRENT,
        DISPATCHERQUEUE_THREAD_APARTMENTTYPE::DQTAT_COM_NONE};
    ComPtr<ABI::Windows::System::IDispatcherQueueController> queue_controller;
    hr = create_dispatcher_queue_controller_func_(options, &queue_controller);

    // If there is already a DispatcherQueue on this thread, that is fine. Its
    // lifetime is tied to the thread's, and as long as the thread has one, even
    // if we didn't create it, the capture session's events will be delivered on
    // this thread.
    if (FAILED(hr) && hr != RPC_E_WRONG_THREAD) {
      RecordWgcCapturerResult(WgcCapturerResult::kCreateDispatcherQueueFailure);
      callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                                 /*frame=*/nullptr);
    } else {
      dispatcher_queue_created_ = true;
    }
  }

  int64_t capture_start_time_nanos = TimeNanos();

  WgcCaptureSession* capture_session = nullptr;
  std::map<SourceId, WgcCaptureSession>::iterator session_iter =
      ongoing_captures_.find(capture_source_->GetSourceId());
  if (session_iter == ongoing_captures_.end()) {
    ComPtr<WGC::IGraphicsCaptureItem> item;
    hr = capture_source_->GetCaptureItem(&item);
    if (FAILED(hr)) {
      RTC_LOG(LS_ERROR) << "Failed to create a GraphicsCaptureItem: " << hr;
      callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                                 /*frame=*/nullptr);
      RecordWgcCapturerResult(WgcCapturerResult::kItemCreationFailure);
      return;
    }

    std::pair<std::map<SourceId, WgcCaptureSession>::iterator, bool>
        iter_success_pair = ongoing_captures_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(capture_source_->GetSourceId()),
            std::forward_as_tuple(capture_source_->GetSourceId(), d3d11_device_,
                                  item, capture_source_->GetSize()));
    RTC_DCHECK(iter_success_pair.second);
    capture_session = &iter_success_pair.first->second;
  } else {
    capture_session = &session_iter->second;
  }

  if (!capture_session->IsCaptureStarted()) {
    hr = capture_session->StartCapture(options_);
    if (FAILED(hr)) {
      RTC_LOG(LS_ERROR) << "Failed to start capture: " << hr;
      ongoing_captures_.erase(capture_source_->GetSourceId());
      callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                                 /*frame=*/nullptr);
      RecordWgcCapturerResult(WgcCapturerResult::kSessionStartFailure);
      return;
    }
  }

  std::unique_ptr<DesktopFrame> frame;
  if (!capture_session->GetFrame(&frame,
                                 capture_source_->ShouldBeCapturable())) {
    RTC_LOG(LS_ERROR) << "GetFrame failed.";
    ongoing_captures_.erase(capture_source_->GetSourceId());
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    RecordWgcCapturerResult(WgcCapturerResult::kGetFrameFailure);
    return;
  }

  if (!frame) {
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_TEMPORARY,
                               /*frame=*/nullptr);
    RecordWgcCapturerResult(WgcCapturerResult::kFrameDropped);
    return;
  }

  int capture_time_ms =
      (TimeNanos() - capture_start_time_nanos) / kNumNanosecsPerMillisec;
  RTC_HISTOGRAM_COUNTS_1000("WebRTC.DesktopCapture.Win.WgcCapturerFrameTime",
                            capture_time_ms);
  frame->set_capture_time_ms(capture_time_ms);
  frame->set_capturer_id(DesktopCapturerId::kWgcCapturerWin);
  frame->set_may_contain_cursor(options_.prefer_cursor_embedded());
  frame->set_top_left(capture_source_->GetTopLeft());
  RecordWgcCapturerResult(WgcCapturerResult::kSuccess);
  callback_->OnCaptureResult(DesktopCapturer::Result::SUCCESS,
                             std::move(frame));
}

bool WgcCapturerWin::IsSourceBeingCaptured(DesktopCapturer::SourceId id) {
  std::map<DesktopCapturer::SourceId, WgcCaptureSession>::iterator
      session_iter = ongoing_captures_.find(id);
  if (session_iter == ongoing_captures_.end()) {
    return false;
  }

  return session_iter->second.IsCaptureStarted();
}

void WgcCapturerWin::SetUpFullScreenDetectorForTest(
    DesktopCapturer::SourceId source_id,
    bool fullscreen_slide_show_started_after_capture_start,
    bool use_heuristic_for_finding_editor) {
  if (full_screen_window_detector_) {
    full_screen_window_detector_->CreateFullScreenApplicationHandlerForTest(
        source_id, fullscreen_slide_show_started_after_capture_start,
        use_heuristic_for_finding_editor);
  }
}

}  // namespace webrtc
