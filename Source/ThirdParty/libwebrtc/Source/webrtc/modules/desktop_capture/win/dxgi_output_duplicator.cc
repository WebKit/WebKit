/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/dxgi_output_duplicator.h"

#include <string.h>

#include <unknwn.h>
#include <DXGIFormat.h>
#include <Windows.h>

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/desktop_capture/win/dxgi_texture_mapping.h"
#include "webrtc/modules/desktop_capture/win/dxgi_texture_staging.h"

namespace webrtc {

using Microsoft::WRL::ComPtr;

namespace {

// Timeout for AcquireNextFrame() call.
const int kAcquireTimeoutMs = 10;

DesktopRect RECTToDesktopRect(const RECT& rect) {
  return DesktopRect::MakeLTRB(rect.left, rect.top, rect.right, rect.bottom);
}

}  // namespace

DxgiOutputDuplicator::DxgiOutputDuplicator(const D3dDevice& device,
                                           const ComPtr<IDXGIOutput1>& output,
                                           const DXGI_OUTPUT_DESC& desc)
    : device_(device),
      output_(output),
      desktop_rect_(RECTToDesktopRect(desc.DesktopCoordinates)) {
  RTC_DCHECK(output_);
  RTC_DCHECK(!desktop_rect_.is_empty());
  RTC_DCHECK(desktop_rect_.left() >= 0 && desktop_rect_.top() >= 0);
}

DxgiOutputDuplicator::DxgiOutputDuplicator(DxgiOutputDuplicator&& other) =
    default;

DxgiOutputDuplicator::~DxgiOutputDuplicator() {
  if (duplication_) {
    duplication_->ReleaseFrame();
  }
  texture_.reset();
}

bool DxgiOutputDuplicator::Initialize() {
  if (DuplicateOutput()) {
    if (desc_.DesktopImageInSystemMemory) {
      texture_.reset(new DxgiTextureMapping(desktop_rect_, duplication_.Get()));
    } else {
      texture_.reset(new DxgiTextureStaging(desktop_rect_, device_));
    }
    return true;
  } else {
    duplication_.Reset();
    return false;
  }
}

bool DxgiOutputDuplicator::DuplicateOutput() {
  RTC_DCHECK(!duplication_);
  _com_error error =
      output_->DuplicateOutput(static_cast<IUnknown*>(device_.d3d_device()),
                               duplication_.GetAddressOf());
  if (error.Error() != S_OK || !duplication_) {
    LOG(LS_WARNING) << "Failed to duplicate output from IDXGIOutput1, error "
                    << error.ErrorMessage() << ", with code " << error.Error();
    return false;
  }

  memset(&desc_, 0, sizeof(desc_));
  duplication_->GetDesc(&desc_);
  if (desc_.ModeDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
    LOG(LS_ERROR) << "IDXGIDuplicateOutput does not use RGBA (8 bit) "
                     "format, which is required by downstream components, "
                     "format is "
                  << desc_.ModeDesc.Format;
    return false;
  }

  if (static_cast<int>(desc_.ModeDesc.Width) != desktop_rect_.width() ||
      static_cast<int>(desc_.ModeDesc.Height) != desktop_rect_.height()) {
    LOG(LS_ERROR) << "IDXGIDuplicateOutput does not return a same size as its "
                     "IDXGIOutput1, size returned by IDXGIDuplicateOutput is "
                  << desc_.ModeDesc.Width << " x " << desc_.ModeDesc.Height
                  << ", size returned by IDXGIOutput1 is "
                  << desktop_rect_.width() << " x " << desktop_rect_.height();
    return false;
  }

  return true;
}

bool DxgiOutputDuplicator::ReleaseFrame() {
  RTC_DCHECK(duplication_);
  _com_error error = duplication_->ReleaseFrame();
  if (error.Error() != S_OK) {
    LOG(LS_ERROR) << "Failed to release frame from IDXGIOutputDuplication, "
                     "error"
                  << error.ErrorMessage() << ", code " << error.Error();
    return false;
  }
  return true;
}

bool DxgiOutputDuplicator::Duplicate(Context* context,
                                     DesktopVector offset,
                                     SharedDesktopFrame* target) {
  RTC_DCHECK(duplication_);
  RTC_DCHECK(texture_);
  RTC_DCHECK(target);
  if (!DesktopRect::MakeSize(target->size())
           .ContainsRect(TranslatedDesktopRect(offset))) {
    // target size is not large enough to cover current output region.
    return false;
  }

  DXGI_OUTDUPL_FRAME_INFO frame_info;
  memset(&frame_info, 0, sizeof(frame_info));
  ComPtr<IDXGIResource> resource;
  _com_error error = duplication_->AcquireNextFrame(
      kAcquireTimeoutMs, &frame_info, resource.GetAddressOf());
  if (error.Error() != S_OK && error.Error() != DXGI_ERROR_WAIT_TIMEOUT) {
    LOG(LS_ERROR) << "Failed to capture frame, error " << error.ErrorMessage()
                  << ", code " << error.Error();
    return false;
  }

  // We need to merge updated region with the one from last frame, since current
  // frame contains the content one frame before. Note, this is for double
  // buffering implementation, as what we have in ScreenCapturerWinDirectx. If
  // a consumer uses single buffering, we should clear context->updated_region
  // after it has been merged to updated_region.
  DesktopRegion updated_region;
  updated_region.Swap(&context->updated_region);
  if (error.Error() == S_OK &&
      frame_info.AccumulatedFrames > 0 &&
      resource) {
    DetectUpdatedRegion(frame_info, offset, &context->updated_region);
    if (!texture_->CopyFrom(frame_info, resource.Get(),
                            context->updated_region)) {
      return false;
    }
    SpreadContextChange(context);
    updated_region.AddRegion(context->updated_region);

    const DesktopFrame& source = texture_->AsDesktopFrame();
    for (DesktopRegion::Iterator it(updated_region); !it.IsAtEnd();
         it.Advance()) {
      target->CopyPixelsFrom(
          source, SourceRect(it.rect()).top_left(), it.rect());
    }
    last_frame_ = target->Share();
    last_frame_offset_ = offset;
    target->mutable_updated_region()->AddRegion(updated_region);
    return texture_->Release() && ReleaseFrame();
  }

  if (last_frame_) {
    // No change since last frame or AcquireNextFrame() timed out, we will
    // export last frame to the target.
    for (DesktopRegion::Iterator it(updated_region); !it.IsAtEnd();
         it.Advance()) {
      target->CopyPixelsFrom(*last_frame_, it.rect().top_left(), it.rect());
    }
    target->mutable_updated_region()->AddRegion(updated_region);
  }
  // If AcquireNextFrame() failed with timeout error, we do not need to release
  // the frame.
  return error.Error() == DXGI_ERROR_WAIT_TIMEOUT || ReleaseFrame();
}

DesktopRect DxgiOutputDuplicator::TranslatedDesktopRect(DesktopVector offset) {
  DesktopRect result(DesktopRect::MakeSize(desktop_rect_.size()));
  result.Translate(offset);
  return result;
}

void DxgiOutputDuplicator::DetectUpdatedRegion(
    const DXGI_OUTDUPL_FRAME_INFO& frame_info,
    DesktopVector offset,
    DesktopRegion* updated_region) {
  if (DoDetectUpdatedRegion(frame_info, updated_region)) {
    updated_region->Translate(offset.x(), offset.y());
    // Make sure even a region returned by Windows API is out of the scope of
    // desktop_rect_, we still won't export it to the target DesktopFrame.
    updated_region->IntersectWith(TranslatedDesktopRect(offset));
  } else {
    updated_region->SetRect(TranslatedDesktopRect(offset));
  }
}

bool DxgiOutputDuplicator::DoDetectUpdatedRegion(
    const DXGI_OUTDUPL_FRAME_INFO& frame_info,
    DesktopRegion* updated_region) {
  RTC_DCHECK(updated_region);
  updated_region->Clear();
  if (frame_info.TotalMetadataBufferSize == 0) {
    // This should not happen, since frame_info.AccumulatedFrames > 0.
    LOG(LS_ERROR) << "frame_info.AccumulatedFrames > 0, "
                     "but TotalMetadataBufferSize == 0";
    return false;
  }

  if (metadata.capacity() < frame_info.TotalMetadataBufferSize) {
    metadata.clear();  // Avoid data copy
    metadata.reserve(frame_info.TotalMetadataBufferSize);
  }

  UINT buff_size = 0;
  DXGI_OUTDUPL_MOVE_RECT* move_rects =
      reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(metadata.data());
  size_t move_rects_count = 0;
  _com_error error = duplication_->GetFrameMoveRects(
      static_cast<UINT>(metadata.capacity()), move_rects, &buff_size);
  if (error.Error() != S_OK) {
    LOG(LS_ERROR) << "Failed to get move rectangles, error "
                  << error.ErrorMessage() << ", code " << error.Error();
    return false;
  }
  move_rects_count = buff_size / sizeof(DXGI_OUTDUPL_MOVE_RECT);

  RECT* dirty_rects = reinterpret_cast<RECT*>(metadata.data() + buff_size);
  size_t dirty_rects_count = 0;
  error = duplication_->GetFrameDirtyRects(
      static_cast<UINT>(metadata.capacity()) - buff_size, dirty_rects,
      &buff_size);
  if (error.Error() != S_OK) {
    LOG(LS_ERROR) << "Failed to get dirty rectangles, error "
                  << error.ErrorMessage() << ", code " << error.Error();
    return false;
  }
  dirty_rects_count = buff_size / sizeof(RECT);

  while (move_rects_count > 0) {
    updated_region->AddRect(DesktopRect::MakeXYWH(
        move_rects->SourcePoint.x, move_rects->SourcePoint.y,
        move_rects->DestinationRect.right - move_rects->DestinationRect.left,
        move_rects->DestinationRect.bottom - move_rects->DestinationRect.top));
    updated_region->AddRect(DesktopRect::MakeLTRB(
        move_rects->DestinationRect.left, move_rects->DestinationRect.top,
        move_rects->DestinationRect.right, move_rects->DestinationRect.bottom));
    move_rects++;
    move_rects_count--;
  }

  while (dirty_rects_count > 0) {
    updated_region->AddRect(
        DesktopRect::MakeLTRB(dirty_rects->left, dirty_rects->top,
                              dirty_rects->right, dirty_rects->bottom));
    dirty_rects++;
    dirty_rects_count--;
  }

  return true;
}

void DxgiOutputDuplicator::Setup(Context* context) {
  RTC_DCHECK(context->updated_region.is_empty());
  // Always copy entire monitor during the first Duplicate() function call.
  context->updated_region.AddRect(desktop_rect_);
  RTC_DCHECK(std::find(contexts_.begin(), contexts_.end(), context) ==
             contexts_.end());
  contexts_.push_back(context);
}

void DxgiOutputDuplicator::Unregister(const Context* const context) {
  auto it = std::find(contexts_.begin(), contexts_.end(), context);
  RTC_DCHECK(it != contexts_.end());
  contexts_.erase(it);
}

void DxgiOutputDuplicator::SpreadContextChange(const Context* const source) {
  for (Context* dest : contexts_) {
    RTC_DCHECK(dest);
    if (dest != source) {
      dest->updated_region.AddRegion(source->updated_region);
    }
  }
}

DesktopRect DxgiOutputDuplicator::SourceRect(DesktopRect rect) {
  // |texture_|->AsDesktopFrame() starts from (0, 0).
  rect.Translate(-desktop_rect_.left(), -desktop_rect_.top());
  return rect;
}

}  // namespace webrtc
