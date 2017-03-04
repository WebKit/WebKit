/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DXGI_DUPLICATOR_CONTROLLER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DXGI_DUPLICATOR_CONTROLLER_H_

#include <D3DCommon.h>

#include <memory>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/resolution_change_detector.h"
#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"
#include "webrtc/modules/desktop_capture/win/d3d_device.h"
#include "webrtc/modules/desktop_capture/win/dxgi_adapter_duplicator.h"

namespace webrtc {

// A controller for all the objects we need to call Windows DirectX capture APIs
// It's a singleton because only one IDXGIOutputDuplication instance per monitor
// is allowed per application.
//
// Consumers should create a DxgiDuplicatorController::Context and keep it
// throughout their lifetime, and pass it when calling Duplicate(). Consumers
// can also call IsSupported() to determine whether the system supports DXGI
// duplicator or not. If a previous IsSupported() function call returns true,
// but a later Duplicate() returns false, this usually means the display mode is
// changing. Consumers should retry after a while. (Typically 50 milliseconds,
// but according to hardware performance, this time may vary.)
class DxgiDuplicatorController {
 public:
  // A context to store the status of a single consumer of
  // DxgiDuplicatorController.
  class Context {
   public:
    Context();
    // Unregister this Context instance from all Dxgi duplicators during
    // destructing.
    ~Context();

    // Reset current Context, so it will be reinitialized next time.
    void Reset();

   private:
    friend class DxgiDuplicatorController;

    // A Context will have an exactly same |identity_| as
    // DxgiDuplicatorController, to ensure it has been correctly setted up after
    // each DxgiDuplicatorController::Initialize().
    int identity_ = 0;

    // Child DxgiAdapterDuplicator::Context belongs to this Context.
    std::vector<DxgiAdapterDuplicator::Context> contexts_;
  };

  // A collection of D3d information we are interested on, which may impact
  // capturer performance or reliability.
  struct D3dInfo {
    // Each video adapter has its own D3D_FEATURE_LEVEL, so this structure
    // contains the minimum and maximium D3D_FEATURE_LEVELs current system
    // supports.
    // Both fields can be 0, which is the default value to indicate no valid
    // D3D_FEATURE_LEVEL has been retrieved from underlying OS APIs.
    D3D_FEATURE_LEVEL min_feature_level;
    D3D_FEATURE_LEVEL max_feature_level;

    // TODO(zijiehe): Add more fields, such as manufacturer name, mode, driver
    // version.
  };

  // Returns the singleton instance of DxgiDuplicatorController.
  static DxgiDuplicatorController* Instance();

  // Destructs current instance. We need to make sure COM components and their
  // containers are destructed in correct order.
  ~DxgiDuplicatorController();

  // All the following functions implicitly call Initialize() function if
  // current instance has not been initialized.

  // Detects whether the system supports DXGI based capturer.
  bool IsSupported();

  // Calls Deinitialize() function with lock. Consumers can call this function
  // to force the DxgiDuplicatorController to be reinitialized to avoid an
  // expected failure in next Duplicate() call.
  void Reset();

  // Returns a copy of D3dInfo composed by last Initialize() function call.
  bool RetrieveD3dInfo(D3dInfo* info);

  // Captures current screen and writes into target. Since we are using double
  // buffering, |last_frame|.updated_region() is used to represent the not
  // updated regions in current |target| frame, which should also be copied this
  // time.
  // TODO(zijiehe): Windows cannot guarantee the frames returned by each
  // IDXGIOutputDuplication are synchronized. But we are using a totally
  // different threading model than the way Windows suggested, it's hard to
  // synchronize them manually. We should find a way to do it.
  bool Duplicate(Context* context, SharedDesktopFrame* target);

  // Captures one monitor and writes into target. |monitor_id| should >= 0. If
  // |monitor_id| is greater than the total screen count of all the Duplicators,
  // this function returns false.
  bool DuplicateMonitor(Context* context,
                        int monitor_id,
                        SharedDesktopFrame* target);

  // Returns dpi of current system. Returns an empty DesktopVector if system
  // does not support DXGI based capturer.
  DesktopVector dpi();

  // Returns entire desktop size. Returns an empty DesktopRect if system does
  // not support DXGI based capturer.
  DesktopRect desktop_rect();

  // Returns a DesktopSize to cover entire desktop_rect. This may be different
  // than desktop_rect().size(), since top-left screen does not need to start
  // from (0, 0).
  DesktopSize desktop_size();

  // Returns the size of one screen. |monitor_id| should be >= 0. If system does
  // not support DXGI based capturer, or |monitor_id| is greater than the total
  // screen count of all the Duplicators, this function returns an empty
  // DesktopRect.
  DesktopRect ScreenRect(int id);

  // Returns the count of screens on the system. These screens can be retrieved
  // by an integer in the range of [0, ScreenCount()). If system does not
  // support DXGI based capturer, this function returns 0.
  int ScreenCount();

 private:
  // Context calls private Unregister(Context*) function during
  // destructing.
  friend class Context;

  // A private constructor to ensure consumers to use
  // DxgiDuplicatorController::Instance().
  DxgiDuplicatorController();

  // Unregisters Context from this instance and all DxgiAdapterDuplicator(s)
  // it owns.
  void Unregister(const Context* const context);

  // All functions below should be called in |lock_| locked scope.

  // If current instance has not been initialized, executes DoInitialize
  // function, and returns initialize result. Otherwise directly returns true.
  bool Initialize();

  bool DoInitialize();

  // Clears all COM components referred by this instance. So next Duplicate()
  // call will eventually initialize this instance again.
  void Deinitialize();

  // A helper function to check whether a Context has been expired.
  bool ContextExpired(const Context* const context) const;

  // Updates Context if needed.
  void Setup(Context* context);

  // Does the real duplication work. |monitor_id < 0| to capture entire screen.
  bool DoDuplicate(Context* context,
                   int monitor_id,
                   SharedDesktopFrame* target);

  bool DoDuplicateUnlocked(Context* context,
                           int monitor_id,
                           SharedDesktopFrame* target);

  // Captures all monitors.
  bool DoDuplicateAll(Context* context, SharedDesktopFrame* target);

  // Captures one monitor.
  bool DoDuplicateOne(Context* context,
                      int monitor_id,
                      SharedDesktopFrame* target);

  // The minimum GetNumFramesCaptured() returned by |duplicators_|.
  int64_t GetNumFramesCaptured() const;

  int ScreenCountUnlocked();

  // Retries DoDuplicateAll() for several times until GetNumFramesCaptured() is
  // large enough. Returns false if DoDuplicateAll() returns false, or
  // GetNumFramesCaptured() has never reached the requirement.
  // According to http://crbug.com/682112, dxgi capturer returns a black frame
  // during first several capture attempts.
  bool EnsureFrameCaptured(Context* context, SharedDesktopFrame* target);

  // This lock must be locked whenever accessing any of the following objects.
  rtc::CriticalSection lock_;

  // A self-incremented integer to compare with the one in Context, to
  // ensure a Context has been initialized after DxgiDuplicatorController.
  int identity_ = 0;
  DesktopRect desktop_rect_;
  DesktopVector dpi_;
  std::vector<DxgiAdapterDuplicator> duplicators_;
  D3dInfo d3d_info_;
  ResolutionChangeDetector resolution_change_detector_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DXGI_DUPLICATOR_CONTROLLER_H_
