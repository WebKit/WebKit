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
#include <shellscalingapi.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <cstdint>
#include <memory>
#include <optional>

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/event.h"

namespace webrtc {

class WgcCaptureSession final {
 public:
  // WgcCaptureSession supports capturing a window as well as a screen.
  // If it is a window, `source_id` is the HWND of the window to be
  // captured, which is never `0`'. If it is a screen, `source_id` is a number
  // in a 0-based monitor index.
  WgcCaptureSession(
      intptr_t source_id,
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Capture::IGraphicsCaptureItem> item,
      ABI::Windows::Graphics::SizeInt32 size);

  // Disallow copy and assign.
  WgcCaptureSession(const WgcCaptureSession&) = delete;
  WgcCaptureSession& operator=(const WgcCaptureSession&) = delete;

  ~WgcCaptureSession();

  HRESULT StartCapture(const DesktopCaptureOptions& options);

  // Returns a frame from the local frame queue, if any are present.
  bool GetFrame(std::unique_ptr<DesktopFrame>* output_frame,
                bool source_should_be_capturable);

  bool IsCaptureStarted() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return is_capture_started_;
  }

  // We keep 2 buffers in the frame pool since it results in a good compromise
  // between latency/capture-rate and the rate at which
  // Direct3D11CaptureFramePool.TryGetNextFrame returns NULL and we have to fall
  // back to providing a copy from our external queue instead.
  // We make this public for tests.
  static constexpr int kNumBuffers = 2;

 private:
  class RefCountedEvent : public RefCountedNonVirtual<RefCountedEvent>,
                          public Event {
   public:
    RefCountedEvent(bool manual_reset, bool initially_signaled);

   private:
    friend class RefCountedNonVirtual<RefCountedEvent>;
    ~RefCountedEvent();
  };

  // Handles the arrival of new frames in the Direct3D11CaptureFramePool.
  // Whenever `Direct3D11CaptureFramePool.FrameArrived` is called,
  // `AgileFrameArrivedHandler::Invoke` will also be called. This class needs to
  // implement the IAgileObject interface so that we can create a WGC frame pool
  // with `Direct3D11CaptureFramePool::CreateFreeThreaded` and be able to call
  // `Invoke` on a thread different from the one that created this class'
  // instance. See more:
  class AgileFrameArrivedHandler
      : public Microsoft::WRL::RuntimeClass<
            Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
            ABI::Windows::Foundation::ITypedEventHandler<
                ABI::Windows::Graphics::Capture::Direct3D11CaptureFramePool*,
                IInspectable*>,
            IAgileObject> {
   public:
    AgileFrameArrivedHandler(scoped_refptr<RefCountedEvent> event);

    IFACEMETHODIMP Invoke(
        ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool* sender,
        IInspectable* args) override;

   private:
    scoped_refptr<RefCountedEvent> frame_arrived_event_;
  };

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

  // Waits for the first frame to arrive in the `frame_pool_`. We should wait
  // for a frame if either this is the first frame ever obtained from the
  // `frame_pool_` or if this is the first frame obtained after a capture
  // interruption - e.g. when a captured window is brought back after being
  // minimized.
  bool WaitForFirstFrame();

  // Wraps calls to ProcessFrame and deals with the uniqe start-up phase
  // ensuring that we always have one captured frame available.
  void EnsureFrame();

  // Process the captured frame and copy it to the `queue_`.
  HRESULT ProcessFrame();

  void RemoveEventHandlers();
  void RemoveItemClosedEventHandler();
  void RemoveFrameArrivedEventHandler();
  HRESULT AddFrameArrivedEventHandler();

  bool FrameContentCanBeCompared();

  bool allow_zero_hertz() const { return allow_zero_hertz_; }

  std::unique_ptr<EventRegistrationToken> item_closed_token_;
  std::unique_ptr<EventRegistrationToken> frame_arrived_token_;

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

  // This is the size of `mapped_texture_` and the buffers in `frame_pool_`. We
  // store this as a member so we can compare it to the size of incoming frames
  // and resize if necessary.
  ABI::Windows::Graphics::SizeInt32 size_;

  // The capture session lets us set properties about the capture before it
  // starts such as whether to capture the mouse cursor, and it lets us tell WGC
  // to start capturing frames.
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Capture::IGraphicsCaptureSession>
      session_;

  // Queue of captured video frames. The queue holds 2 frames and it avoids
  // alloc/dealloc per captured frame. Incoming frames from the internal frame
  // pool are copied to this queue after required processing in ProcessFrame().
  ScreenCaptureFrameQueue<SharedDesktopFrame> queue_;

  bool item_closed_ = false;
  bool is_capture_started_ = false;

  // Caches the value of DesktopCaptureOptions.allow_wgc_zero_hertz() in
  // StartCapture(). Adds 0Hz detection in ProcessFrame() when enabled which
  // adds complexity since memcmp() is performed on two successive frames.
  bool allow_zero_hertz_ = false;

  // Tracks damage region updates that were reported since the last time a frame
  // was captured. Currently only supports either the complete rect being
  // captured or an empty region. Will always be empty if `allow_zero_hertz_` is
  // false.
  DesktopRegion damage_region_;

  // The unique id to represent a Source of current DesktopCapturer.
  intptr_t source_id_;

  // The monitor that is being captured when the target source_id is a
  // screen. For window sources, it can't be used because the window can move
  // around around the different monitors.
  std::optional<HMONITOR> monitor_;

  // The source type of the capture session. It can be either a window or a
  // screen.
  bool is_window_source_;

  // To be shared between `WgcCaptureSession` and `AgileFrameHandler`.
  // AgileFrameHandler will set this event in a WGC working thread and
  // `WgcCaptureSession` will check its state in desktopCaptureThread. This is
  // necessary to avoid race conditions where the desktopCaptureThread preempts
  // the WGC worker thread and destroys the `WgcCaptureSession` while a new
  // frame is being processed In this situation, the `AgileFrameHandler` would
  // end accessing invalid memory, which was previously owned by
  // `WgcCaptureSession`.
  //
  // Will be signaled when the first frame is available in the `frame_pool_` and
  // should not reset for the lifetime of `WgcCaptureSession`.
  scoped_refptr<RefCountedEvent> has_first_frame_arrived_event_;

  // Records if the first frame arrived in a stream arrived. Will be reset if a
  // source becomes momentarilly non-capturable - e.g. a window that gets
  // minimized.
  bool has_first_frame_arrived_ = false;

  SequenceChecker sequence_checker_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SESSION_H_
