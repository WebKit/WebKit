/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/encoded_frame.h"
#include "modules/video_coding/generic_encoder.h"
#include "modules/video_coding/jitter_buffer_common.h"

namespace webrtc {

VCMEncodedFrame::VCMEncodedFrame()
    : webrtc::EncodedImage(),
      _renderTimeMs(-1),
      _payloadType(0),
      _missingFrame(false),
      _codec(kVideoCodecUnknown),
      _rotation_set(false) {
  _codecSpecificInfo.codecType = kVideoCodecUnknown;
}

VCMEncodedFrame::VCMEncodedFrame(const webrtc::EncodedImage& rhs)
    : webrtc::EncodedImage(rhs),
      _renderTimeMs(-1),
      _payloadType(0),
      _missingFrame(false),
      _codec(kVideoCodecUnknown),
      _rotation_set(false) {
  _codecSpecificInfo.codecType = kVideoCodecUnknown;
  _buffer = NULL;
  _size = 0;
  _length = 0;
  if (rhs._buffer != NULL) {
    VerifyAndAllocate(rhs._length +
                      EncodedImage::GetBufferPaddingBytes(_codec));
    memcpy(_buffer, rhs._buffer, rhs._length);
  }
}

VCMEncodedFrame::VCMEncodedFrame(const VCMEncodedFrame& rhs)
    : webrtc::EncodedImage(rhs),
      _renderTimeMs(rhs._renderTimeMs),
      _payloadType(rhs._payloadType),
      _missingFrame(rhs._missingFrame),
      _codecSpecificInfo(rhs._codecSpecificInfo),
      _codec(rhs._codec),
      _rotation_set(rhs._rotation_set) {
  _buffer = NULL;
  _size = 0;
  _length = 0;
  if (rhs._buffer != NULL) {
    VerifyAndAllocate(rhs._length +
                      EncodedImage::GetBufferPaddingBytes(_codec));
    memcpy(_buffer, rhs._buffer, rhs._length);
    _length = rhs._length;
  }
}

VCMEncodedFrame::~VCMEncodedFrame() {
  Free();
}

void VCMEncodedFrame::Free() {
  Reset();
  if (_buffer != NULL) {
    delete[] _buffer;
    _buffer = NULL;
  }
}

void VCMEncodedFrame::Reset() {
  _renderTimeMs = -1;
  _timeStamp = 0;
  _payloadType = 0;
  _frameType = kVideoFrameDelta;
  _encodedWidth = 0;
  _encodedHeight = 0;
  _completeFrame = false;
  _missingFrame = false;
  _length = 0;
  _codecSpecificInfo.codecType = kVideoCodecUnknown;
  _codec = kVideoCodecUnknown;
  rotation_ = kVideoRotation_0;
  content_type_ = VideoContentType::UNSPECIFIED;
  timing_.flags = TimingFrameFlags::kInvalid;
  _rotation_set = false;
}

void VCMEncodedFrame::CopyCodecSpecific(const RTPVideoHeader* header) {
  if (header) {
    switch (header->codec) {
      case kRtpVideoVp8: {
        if (_codecSpecificInfo.codecType != kVideoCodecVP8) {
          // This is the first packet for this frame.
          _codecSpecificInfo.codecSpecific.VP8.pictureId = -1;
          _codecSpecificInfo.codecSpecific.VP8.temporalIdx = 0;
          _codecSpecificInfo.codecSpecific.VP8.layerSync = false;
          _codecSpecificInfo.codecSpecific.VP8.keyIdx = -1;
          _codecSpecificInfo.codecType = kVideoCodecVP8;
        }
        _codecSpecificInfo.codecSpecific.VP8.nonReference =
            header->codecHeader.VP8.nonReference;
        if (header->codecHeader.VP8.pictureId != kNoPictureId) {
          _codecSpecificInfo.codecSpecific.VP8.pictureId =
              header->codecHeader.VP8.pictureId;
        }
        if (header->codecHeader.VP8.temporalIdx != kNoTemporalIdx) {
          _codecSpecificInfo.codecSpecific.VP8.temporalIdx =
              header->codecHeader.VP8.temporalIdx;
          _codecSpecificInfo.codecSpecific.VP8.layerSync =
              header->codecHeader.VP8.layerSync;
        }
        if (header->codecHeader.VP8.keyIdx != kNoKeyIdx) {
          _codecSpecificInfo.codecSpecific.VP8.keyIdx =
              header->codecHeader.VP8.keyIdx;
        }
        break;
      }
      case kRtpVideoVp9: {
        if (_codecSpecificInfo.codecType != kVideoCodecVP9) {
          // This is the first packet for this frame.
          _codecSpecificInfo.codecSpecific.VP9.picture_id = -1;
          _codecSpecificInfo.codecSpecific.VP9.temporal_idx = 0;
          _codecSpecificInfo.codecSpecific.VP9.spatial_idx = 0;
          _codecSpecificInfo.codecSpecific.VP9.gof_idx = 0;
          _codecSpecificInfo.codecSpecific.VP9.inter_layer_predicted = false;
          _codecSpecificInfo.codecSpecific.VP9.tl0_pic_idx = -1;
          _codecSpecificInfo.codecType = kVideoCodecVP9;
        }
        _codecSpecificInfo.codecSpecific.VP9.inter_pic_predicted =
            header->codecHeader.VP9.inter_pic_predicted;
        _codecSpecificInfo.codecSpecific.VP9.flexible_mode =
            header->codecHeader.VP9.flexible_mode;
        _codecSpecificInfo.codecSpecific.VP9.num_ref_pics =
            header->codecHeader.VP9.num_ref_pics;
        for (uint8_t r = 0; r < header->codecHeader.VP9.num_ref_pics; ++r) {
          _codecSpecificInfo.codecSpecific.VP9.p_diff[r] =
              header->codecHeader.VP9.pid_diff[r];
        }
        _codecSpecificInfo.codecSpecific.VP9.ss_data_available =
            header->codecHeader.VP9.ss_data_available;
        if (header->codecHeader.VP9.picture_id != kNoPictureId) {
          _codecSpecificInfo.codecSpecific.VP9.picture_id =
              header->codecHeader.VP9.picture_id;
        }
        if (header->codecHeader.VP9.tl0_pic_idx != kNoTl0PicIdx) {
          _codecSpecificInfo.codecSpecific.VP9.tl0_pic_idx =
              header->codecHeader.VP9.tl0_pic_idx;
        }
        if (header->codecHeader.VP9.temporal_idx != kNoTemporalIdx) {
          _codecSpecificInfo.codecSpecific.VP9.temporal_idx =
              header->codecHeader.VP9.temporal_idx;
          _codecSpecificInfo.codecSpecific.VP9.temporal_up_switch =
              header->codecHeader.VP9.temporal_up_switch;
        }
        if (header->codecHeader.VP9.spatial_idx != kNoSpatialIdx) {
          _codecSpecificInfo.codecSpecific.VP9.spatial_idx =
              header->codecHeader.VP9.spatial_idx;
          _codecSpecificInfo.codecSpecific.VP9.inter_layer_predicted =
              header->codecHeader.VP9.inter_layer_predicted;
        }
        if (header->codecHeader.VP9.gof_idx != kNoGofIdx) {
          _codecSpecificInfo.codecSpecific.VP9.gof_idx =
              header->codecHeader.VP9.gof_idx;
        }
        if (header->codecHeader.VP9.ss_data_available) {
          _codecSpecificInfo.codecSpecific.VP9.num_spatial_layers =
              header->codecHeader.VP9.num_spatial_layers;
          _codecSpecificInfo.codecSpecific.VP9
              .spatial_layer_resolution_present =
              header->codecHeader.VP9.spatial_layer_resolution_present;
          if (header->codecHeader.VP9.spatial_layer_resolution_present) {
            for (size_t i = 0; i < header->codecHeader.VP9.num_spatial_layers;
                 ++i) {
              _codecSpecificInfo.codecSpecific.VP9.width[i] =
                  header->codecHeader.VP9.width[i];
              _codecSpecificInfo.codecSpecific.VP9.height[i] =
                  header->codecHeader.VP9.height[i];
            }
          }
          _codecSpecificInfo.codecSpecific.VP9.gof.CopyGofInfoVP9(
              header->codecHeader.VP9.gof);
        }
        break;
      }
      case kRtpVideoH264: {
        _codecSpecificInfo.codecType = kVideoCodecH264;
        break;
      }
      case kRtpVideoStereo: {
        _codecSpecificInfo.codecType = kVideoCodecStereo;
        VideoCodecType associated_codec_type = kVideoCodecUnknown;
        switch (header->codecHeader.stereo.associated_codec_type) {
          case kRtpVideoVp8:
            associated_codec_type = kVideoCodecVP8;
            break;
          case kRtpVideoVp9:
            associated_codec_type = kVideoCodecVP9;
            break;
          case kRtpVideoH264:
            associated_codec_type = kVideoCodecH264;
            break;
          default:
            RTC_NOTREACHED();
        }
        _codecSpecificInfo.codecSpecific.stereo.associated_codec_type =
            associated_codec_type;
        _codecSpecificInfo.codecSpecific.stereo.indices =
            header->codecHeader.stereo.indices;
        break;
      }
      default: {
        _codecSpecificInfo.codecType = kVideoCodecUnknown;
        break;
      }
    }
  }
}

void VCMEncodedFrame::VerifyAndAllocate(size_t minimumSize) {
  if (minimumSize > _size) {
    // create buffer of sufficient size
    uint8_t* newBuffer = new uint8_t[minimumSize];
    if (_buffer) {
      // copy old data
      memcpy(newBuffer, _buffer, _size);
      delete[] _buffer;
    }
    _buffer = newBuffer;
    _size = minimumSize;
  }
}

}  // namespace webrtc
