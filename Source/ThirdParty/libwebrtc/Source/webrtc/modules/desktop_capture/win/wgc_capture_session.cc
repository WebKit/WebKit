/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/wgc_capture_session.h"

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directX.direct3d11.interop.h>
#include <wrl.h>

#include <memory>
#include <utility>
#include <vector>

#include "modules/desktop_capture/win/wgc_desktop_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/win/create_direct3d_device.h"
#include "rtc_base/win/get_activation_factory.h"
#include "system_wrappers/include/metrics.h"

using Microsoft::WRL::ComPtr;
namespace WGC = ABI::Windows::Graphics::Capture;

namespace webrtc {
namespace {

// We must use a BGRA pixel format that has 4 bytes per pixel, as required by
// the DesktopFrame interface.
const auto kPixelFormat = ABI::Windows::Graphics::DirectX::DirectXPixelFormat::
    DirectXPixelFormat_B8G8R8A8UIntNormalized;

// We only want 1 buffer in our frame pool to reduce latency. If we had more,
// they would sit in the pool for longer and be stale by the time we are asked
// for a new frame.
const int kNumBuffers = 1;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class StartCaptureResult {
  kSuccess = 0,
  kSourceClosed = 1,
  kAddClosedFailed = 2,
  kDxgiDeviceCastFailed = 3,
  kD3dDelayLoadFailed = 4,
  kD3dDeviceCreationFailed = 5,
  kFramePoolActivationFailed = 6,
  kFramePoolCastFailed = 7,
  kGetItemSizeFailed = 8,
  kCreateFreeThreadedFailed = 9,
  kCreateCaptureSessionFailed = 10,
  kStartCaptureFailed = 11,
  kMaxValue = kStartCaptureFailed
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetFrameResult {
  kSuccess = 0,
  kItemClosed = 1,
  kTryGetNextFrameFailed = 2,
  kFrameDropped = 3,
  kGetSurfaceFailed = 4,
  kDxgiInterfaceAccessFailed = 5,
  kTexture2dCastFailed = 6,
  kCreateMappedTextureFailed = 7,
  kMapFrameFailed = 8,
  kGetContentSizeFailed = 9,
  kResizeMappedTextureFailed = 10,
  kRecreateFramePoolFailed = 11,
  kMaxValue = kRecreateFramePoolFailed
};

void RecordStartCaptureResult(StartCaptureResult error) {
  RTC_HISTOGRAM_ENUMERATION(
      "WebRTC.DesktopCapture.Win.WgcCaptureSessionStartResult",
      static_cast<int>(error), static_cast<int>(StartCaptureResult::kMaxValue));
}

void RecordGetFrameResult(GetFrameResult error) {
  RTC_HISTOGRAM_ENUMERATION(
      "WebRTC.DesktopCapture.Win.WgcCaptureSessionGetFrameResult",
      static_cast<int>(error), static_cast<int>(GetFrameResult::kMaxValue));
}

}  // namespace

WgcCaptureSession::WgcCaptureSession(ComPtr<ID3D11Device> d3d11_device,
                                     ComPtr<WGC::IGraphicsCaptureItem> item)
    : d3d11_device_(std::move(d3d11_device)), item_(std::move(item)) {}
WgcCaptureSession::~WgcCaptureSession() = default;

HRESULT WgcCaptureSession::StartCapture() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(!is_capture_started_);

  if (item_closed_) {
    RTC_LOG(LS_ERROR) << "The target source has been closed.";
    RecordStartCaptureResult(StartCaptureResult::kSourceClosed);
    return E_ABORT;
  }

  RTC_DCHECK(d3d11_device_);
  RTC_DCHECK(item_);

  // Listen for the Closed event, to detect if the source we are capturing is
  // closed (e.g. application window is closed or monitor is disconnected). If
  // it is, we should abort the capture.
  auto closed_handler =
      Microsoft::WRL::Callback<ABI::Windows::Foundation::ITypedEventHandler<
          WGC::GraphicsCaptureItem*, IInspectable*>>(
          this, &WgcCaptureSession::OnItemClosed);
  EventRegistrationToken item_closed_token;
  HRESULT hr = item_->add_Closed(closed_handler.Get(), &item_closed_token);
  if (FAILED(hr)) {
    RecordStartCaptureResult(StartCaptureResult::kAddClosedFailed);
    return hr;
  }

  ComPtr<IDXGIDevice> dxgi_device;
  hr = d3d11_device_->QueryInterface(IID_PPV_ARGS(&dxgi_device));
  if (FAILED(hr)) {
    RecordStartCaptureResult(StartCaptureResult::kDxgiDeviceCastFailed);
    return hr;
  }

  if (!ResolveCoreWinRTDirect3DDelayload()) {
    RecordStartCaptureResult(StartCaptureResult::kD3dDelayLoadFailed);
    return E_FAIL;
  }

  hr = CreateDirect3DDeviceFromDXGIDevice(dxgi_device.Get(), &direct3d_device_);
  if (FAILED(hr)) {
    RecordStartCaptureResult(StartCaptureResult::kD3dDeviceCreationFailed);
    return hr;
  }

  ComPtr<WGC::IDirect3D11CaptureFramePoolStatics> frame_pool_statics;
  hr = GetActivationFactory<
      ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePoolStatics,
      RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool>(
      &frame_pool_statics);
  if (FAILED(hr)) {
    RecordStartCaptureResult(StartCaptureResult::kFramePoolActivationFailed);
    return hr;
  }

  // Cast to FramePoolStatics2 so we can use CreateFreeThreaded and avoid the
  // need to have a DispatcherQueue. We don't listen for the FrameArrived event,
  // so there's no difference.
  ComPtr<WGC::IDirect3D11CaptureFramePoolStatics2> frame_pool_statics2;
  hr = frame_pool_statics->QueryInterface(IID_PPV_ARGS(&frame_pool_statics2));
  if (FAILED(hr)) {
    RecordStartCaptureResult(StartCaptureResult::kFramePoolCastFailed);
    return hr;
  }

  ABI::Windows::Graphics::SizeInt32 item_size;
  hr = item_.Get()->get_Size(&item_size);
  if (FAILED(hr)) {
    RecordStartCaptureResult(StartCaptureResult::kGetItemSizeFailed);
    return hr;
  }

  previous_size_ = item_size;

  hr = frame_pool_statics2->CreateFreeThreaded(direct3d_device_.Get(),
                                               kPixelFormat, kNumBuffers,
                                               item_size, &frame_pool_);
  if (FAILED(hr)) {
    RecordStartCaptureResult(StartCaptureResult::kCreateFreeThreadedFailed);
    return hr;
  }

  hr = frame_pool_->CreateCaptureSession(item_.Get(), &session_);
  if (FAILED(hr)) {
    RecordStartCaptureResult(StartCaptureResult::kCreateCaptureSessionFailed);
    return hr;
  }

  hr = session_->StartCapture();
  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "Failed to start CaptureSession: " << hr;
    RecordStartCaptureResult(StartCaptureResult::kStartCaptureFailed);
    return hr;
  }

  RecordStartCaptureResult(StartCaptureResult::kSuccess);

  is_capture_started_ = true;
  return hr;
}

HRESULT WgcCaptureSession::GetFrame(
    std::unique_ptr<DesktopFrame>* output_frame) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (item_closed_) {
    RTC_LOG(LS_ERROR) << "The target source has been closed.";
    RecordGetFrameResult(GetFrameResult::kItemClosed);
    return E_ABORT;
  }

  RTC_DCHECK(is_capture_started_);

  ComPtr<WGC::IDirect3D11CaptureFrame> capture_frame;
  HRESULT hr = frame_pool_->TryGetNextFrame(&capture_frame);
  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "TryGetNextFrame failed: " << hr;
    RecordGetFrameResult(GetFrameResult::kTryGetNextFrameFailed);
    return hr;
  }

  if (!capture_frame) {
    RecordGetFrameResult(GetFrameResult::kFrameDropped);
    return hr;
  }

  // We need to get this CaptureFrame as an ID3D11Texture2D so that we can get
  // the raw image data in the format required by the DesktopFrame interface.
  ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>
      d3d_surface;
  hr = capture_frame->get_Surface(&d3d_surface);
  if (FAILED(hr)) {
    RecordGetFrameResult(GetFrameResult::kGetSurfaceFailed);
    return hr;
  }

  ComPtr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>
      direct3DDxgiInterfaceAccess;
  hr = d3d_surface->QueryInterface(IID_PPV_ARGS(&direct3DDxgiInterfaceAccess));
  if (FAILED(hr)) {
    RecordGetFrameResult(GetFrameResult::kDxgiInterfaceAccessFailed);
    return hr;
  }

  ComPtr<ID3D11Texture2D> texture_2D;
  hr = direct3DDxgiInterfaceAccess->GetInterface(IID_PPV_ARGS(&texture_2D));
  if (FAILED(hr)) {
    RecordGetFrameResult(GetFrameResult::kTexture2dCastFailed);
    return hr;
  }

  if (!mapped_texture_) {
    hr = CreateMappedTexture(texture_2D);
    if (FAILED(hr)) {
      RecordGetFrameResult(GetFrameResult::kCreateMappedTextureFailed);
      return hr;
    }
  }

  // We need to copy `texture_2D` into `mapped_texture_` as the latter has the
  // D3D11_CPU_ACCESS_READ flag set, which lets us access the image data.
  // Otherwise it would only be readable by the GPU.
  ComPtr<ID3D11DeviceContext> d3d_context;
  d3d11_device_->GetImmediateContext(&d3d_context);
  d3d_context->CopyResource(mapped_texture_.Get(), texture_2D.Get());

  D3D11_MAPPED_SUBRESOURCE map_info;
  hr = d3d_context->Map(mapped_texture_.Get(), /*subresource_index=*/0,
                        D3D11_MAP_READ, /*D3D11_MAP_FLAG_DO_NOT_WAIT=*/0,
                        &map_info);
  if (FAILED(hr)) {
    RecordGetFrameResult(GetFrameResult::kMapFrameFailed);
    return hr;
  }

  ABI::Windows::Graphics::SizeInt32 new_size;
  hr = capture_frame->get_ContentSize(&new_size);
  if (FAILED(hr)) {
    RecordGetFrameResult(GetFrameResult::kGetContentSizeFailed);
    return hr;
  }

  // If the size has changed since the last capture, we must be sure to use
  // the smaller dimensions. Otherwise we might overrun our buffer, or
  // read stale data from the last frame.
  int image_height = std::min(previous_size_.Height, new_size.Height);
  int image_width = std::min(previous_size_.Width, new_size.Width);
  int row_data_length = image_width * DesktopFrame::kBytesPerPixel;

  // Make a copy of the data pointed to by `map_info.pData` so we are free to
  // unmap our texture.
  uint8_t* src_data = static_cast<uint8_t*>(map_info.pData);
  std::vector<uint8_t> image_data;
  image_data.reserve(image_height * row_data_length);
  uint8_t* image_data_ptr = image_data.data();
  for (int i = 0; i < image_height; i++) {
    memcpy(image_data_ptr, src_data, row_data_length);
    image_data_ptr += row_data_length;
    src_data += map_info.RowPitch;
  }

  // Transfer ownership of `image_data` to the output_frame.
  DesktopSize size(image_width, image_height);
  *output_frame = std::make_unique<WgcDesktopFrame>(size, row_data_length,
                                                    std::move(image_data));

  d3d_context->Unmap(mapped_texture_.Get(), 0);

  // If the size changed, we must resize the texture and frame pool to fit the
  // new size.
  if (previous_size_.Height != new_size.Height ||
      previous_size_.Width != new_size.Width) {
    hr = CreateMappedTexture(texture_2D, new_size.Width, new_size.Height);
    if (FAILED(hr)) {
      RecordGetFrameResult(GetFrameResult::kResizeMappedTextureFailed);
      return hr;
    }

    hr = frame_pool_->Recreate(direct3d_device_.Get(), kPixelFormat,
                               kNumBuffers, new_size);
    if (FAILED(hr)) {
      RecordGetFrameResult(GetFrameResult::kRecreateFramePoolFailed);
      return hr;
    }
  }

  RecordGetFrameResult(GetFrameResult::kSuccess);

  previous_size_ = new_size;
  return hr;
}

HRESULT WgcCaptureSession::CreateMappedTexture(
    ComPtr<ID3D11Texture2D> src_texture,
    UINT width,
    UINT height) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  D3D11_TEXTURE2D_DESC src_desc;
  src_texture->GetDesc(&src_desc);
  D3D11_TEXTURE2D_DESC map_desc;
  map_desc.Width = width == 0 ? src_desc.Width : width;
  map_desc.Height = height == 0 ? src_desc.Height : height;
  map_desc.MipLevels = src_desc.MipLevels;
  map_desc.ArraySize = src_desc.ArraySize;
  map_desc.Format = src_desc.Format;
  map_desc.SampleDesc = src_desc.SampleDesc;
  map_desc.Usage = D3D11_USAGE_STAGING;
  map_desc.BindFlags = 0;
  map_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  map_desc.MiscFlags = 0;
  return d3d11_device_->CreateTexture2D(&map_desc, nullptr, &mapped_texture_);
}

HRESULT WgcCaptureSession::OnItemClosed(WGC::IGraphicsCaptureItem* sender,
                                        IInspectable* event_args) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  RTC_LOG(LS_INFO) << "Capture target has been closed.";
  item_closed_ = true;
  is_capture_started_ = false;

  mapped_texture_ = nullptr;
  session_ = nullptr;
  frame_pool_ = nullptr;
  direct3d_device_ = nullptr;
  item_ = nullptr;
  d3d11_device_ = nullptr;

  return S_OK;
}

}  // namespace webrtc
