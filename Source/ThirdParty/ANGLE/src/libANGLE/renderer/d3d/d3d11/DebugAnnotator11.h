//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DebugAnnotator11.h: D3D11 helpers for adding trace annotations.
//

#ifndef LIBANGLE_RENDERER_D3D_D3D11_DEBUGANNOTATOR11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_DEBUGANNOTATOR11_H_

#include "libANGLE/LoggingAnnotator.h"

namespace rx
{

class DebugAnnotator11 : public angle::LoggingAnnotator
{
  public:
    DebugAnnotator11();
    ~DebugAnnotator11() override;
    void initialize(ID3D11DeviceContext *context);
    void release();
    void beginEvent(const char *eventName, const char *eventMessage) override;
    void endEvent(const char *eventName) override;
    void setMarker(const char *markerName) override;
    bool getStatus() override;

  private:
    angle::ComPtr<ID3DUserDefinedAnnotation> mUserDefinedAnnotation;
    static constexpr size_t kMaxMessageLength = 256;
    wchar_t mWCharMessage[kMaxMessageLength];
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D11_DEBUGANNOTATOR11_H_
