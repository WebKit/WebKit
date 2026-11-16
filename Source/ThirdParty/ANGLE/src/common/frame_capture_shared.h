//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_shared.h:
//   Common code between the ANGLE frame-capture runtime and the replay trace fixture.
//

#ifndef COMMON_FRAME_CAPTURE_SHARED_H_
#define COMMON_FRAME_CAPTURE_SHARED_H_

#include <cstdint>

#include "common/system_utils.h"

namespace angle
{
// TODO: Consolidate to C output and remove option. http://anglebug.com/42266223

// Capture configuration environment variables.
constexpr char kEnabledVarName[]        = "ANGLE_CAPTURE_ENABLED";
constexpr char kOutDirectoryVarName[]   = "ANGLE_CAPTURE_OUT_DIR";
constexpr char kFrameStartVarName[]     = "ANGLE_CAPTURE_FRAME_START";
constexpr char kFrameEndVarName[]       = "ANGLE_CAPTURE_FRAME_END";
constexpr char kBinaryDataSizeVarName[] = "ANGLE_CAPTURE_MAX_RESIDENT_BINARY_SIZE";
constexpr char kBlockSizeVarName[]      = "ANGLE_CAPTURE_BLOCK_SIZE";
constexpr char kTriggerVarName[]        = "ANGLE_CAPTURE_TRIGGER";
constexpr char kEndCaptureVarName[]     = "ANGLE_CAPTURE_END_CAPTURE";
constexpr char kCaptureLabelVarName[]   = "ANGLE_CAPTURE_LABEL";
constexpr char kCompressionVarName[]    = "ANGLE_CAPTURE_COMPRESSION";
constexpr char kSerializeStateVarName[] = "ANGLE_CAPTURE_SERIALIZE_STATE";
constexpr char kValidationVarName[]     = "ANGLE_CAPTURE_VALIDATION";
constexpr char kValidationExprVarName[] = "ANGLE_CAPTURE_VALIDATION_EXPR";
constexpr char kSourceExtVarName[]      = "ANGLE_CAPTURE_SOURCE_EXT";
constexpr char kSourceSizeVarName[]     = "ANGLE_CAPTURE_SOURCE_SIZE";
constexpr char kForceShadowVarName[]    = "ANGLE_CAPTURE_FORCE_SHADOW";

// Android debug properties that correspond to the above environment variables.
constexpr char kAndroidEnabled[]        = "debug.angle.capture.enabled";
constexpr char kAndroidOutDir[]         = "debug.angle.capture.out_dir";
constexpr char kAndroidFrameStart[]     = "debug.angle.capture.frame_start";
constexpr char kAndroidFrameEnd[]       = "debug.angle.capture.frame_end";
constexpr char kAndroidBinaryDataSize[] = "debug.angle.capture.max_resident_binary_size";
constexpr char kAndroidBlockSize[]      = "debug.angle.capture.block_size";
constexpr char kAndroidTrigger[]        = "debug.angle.capture.trigger";
constexpr char kAndroidEndCapture[]     = "debug.angle.capture.end_capture";
constexpr char kAndroidCaptureLabel[]   = "debug.angle.capture.label";
constexpr char kAndroidCompression[]    = "debug.angle.capture.compression";
constexpr char kAndroidValidation[]     = "debug.angle.capture.validation";
constexpr char kAndroidValidationExpr[] = "debug.angle.capture.validation_expr";
constexpr char kAndroidSourceExt[]      = "debug.angle.capture.source_ext";
constexpr char kAndroidSourceSize[]     = "debug.angle.capture.source_size";
constexpr char kAndroidForceShadow[]    = "debug.angle.capture.force_shadow";

// Markers used with glDebugMessageInsert to tag fixture-injected API calls
constexpr uint32_t kFixtureInjectedCommandsBeginId = 0x41494342;  // 'AICB'
constexpr uint32_t kFixtureInjectedCommandsEndId   = 0x41494345;  // 'AICE'

bool IsCaptureEnabledFromEnv();
bool IsCaptureConfiguredFromEnv();
}  // namespace angle

#endif  // COMMON_FRAME_CAPTURE_SHARED_H_
