/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SESSION_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SESSION_H_

#include <d3d11.h>
#include <windows.graphics.capture.h>
#include <wrl/client.h>

#include <memory>

#include "api/sequence_checker.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/win/wgc_capture_source.h"

namespace webrtc {

class WgcCaptureSession final {
 public:
  WgcCaptureSession(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Capture::IGraphicsCaptureItem> item);

  // Disallow copy and assign.
  WgcCaptureSession(const WgcCaptureSession&) = delete;
  WgcCaptureSession& operator=(const WgcCaptureSession&) = delete;

  ~WgcCaptureSession();

  HRESULT StartCapture();

  // Returns a frame from the frame pool, if any are present.
  HRESULT GetFrame(std::unique_ptr<DesktopFrame>* output_frame);

  bool IsCaptureStarted() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return is_capture_started_;
  }

 private:
  // Initializes `mapped_texture_` with the properties of the `src_texture`,
  // overrides the values of some necessary properties like the
  // D3D11_CPU_ACCESS_READ flag. Also has optional parameters for what size
  // `mapped_texture_` should be, if they aren't provided we will use the size
  // of `src_texture`.
  HRESULT CreateMappedTexture(
      Microsoft::WRL::ComPtr<ID3D11Texture2D> src_texture,
      UINT width = 0,
      UINT height = 0);

  // Event handler for `item_`'s Closed event.
  HRESULT OnItemClosed(
      ABI::Windows::Graphics::Capture::IGraphicsCaptureItem* sender,
      IInspectable* event_args);

  // A Direct3D11 Device provided by the caller. We use this to create an
  // IDirect3DDevice, and also to create textures that will hold the image data.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;

  // This item represents what we are capturing, we use it to create the
  // capture session, and also to listen for the Closed event.
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>
      item_;

  // The IDirect3DDevice is necessary to instantiate the frame pool.
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>
      direct3d_device_;

  // The frame pool is where frames are deposited during capture, we retrieve
  // them from here with TryGetNextFrame().
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool>
      frame_pool_;

  // This texture holds the final image data. We made it a member so we can
  // reuse it, instead of having to create a new texture every time we grab a
  // frame.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> mapped_texture_;

  // This lets us know when the source has been resized, which is important
  // because we must resize the framepool and our texture to be able to hold
  // enough data for the frame.
  ABI::Windows::Graphics::SizeInt32 previous_size_;

  // The capture session lets us set properties about the capture before it
  // starts such as whether to capture the mouse cursor, and it lets us tell WGC
  // to start capturing frames.
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Capture::IGraphicsCaptureSession>
      session_;

  bool item_closed_ = false;
  bool is_capture_started_ = false;

  SequenceChecker sequence_checker_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SESSION_H_
