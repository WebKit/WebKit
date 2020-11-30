//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DebugAnnotator11.cpp: D3D11 helpers for adding trace annotations.
//

#include "libANGLE/renderer/d3d/d3d11/DebugAnnotator11.h"

#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

#include <versionhelpers.h>

namespace rx
{

DebugAnnotator11::DebugAnnotator11() {}

DebugAnnotator11::~DebugAnnotator11() {}

void DebugAnnotator11::beginEvent(const char *eventName, const char *eventMessage)
{
    angle::LoggingAnnotator::beginEvent(eventName, eventMessage);
    if (loggingEnabledForThisThread())
    {
        std::mbstate_t state = std::mbstate_t();
        std::mbsrtowcs(mWCharMessage, &eventMessage, kMaxMessageLength, &state);
        mUserDefinedAnnotation->BeginEvent(mWCharMessage);
    }
}

void DebugAnnotator11::endEvent(const char *eventName)
{
    angle::LoggingAnnotator::endEvent(eventName);
    if (loggingEnabledForThisThread())
    {
        mUserDefinedAnnotation->EndEvent();
    }
}

void DebugAnnotator11::setMarker(const char *markerName)
{
    angle::LoggingAnnotator::setMarker(markerName);
    if (loggingEnabledForThisThread())
    {
        std::mbstate_t state = std::mbstate_t();
        std::mbsrtowcs(mWCharMessage, &markerName, kMaxMessageLength, &state);
        mUserDefinedAnnotation->SetMarker(mWCharMessage);
    }
}

bool DebugAnnotator11::getStatus()
{
    if (loggingEnabledForThisThread())
    {
        return !!(mUserDefinedAnnotation->GetStatus());
    }

    return false;
}

bool DebugAnnotator11::loggingEnabledForThisThread() const
{
    return mUserDefinedAnnotation != nullptr && std::this_thread::get_id() == mAnnotationThread;
}

void DebugAnnotator11::initialize(ID3D11DeviceContext *context)
{
#if !defined(ANGLE_ENABLE_WINDOWS_UWP)
    // ID3DUserDefinedAnnotation.GetStatus only works on Windows10 or greater.
    // Returning true unconditionally from DebugAnnotator11::getStatus() means
    // writing out all compiled shaders to temporary files even if debugging
    // tools are not attached. See rx::ShaderD3D::prepareSourceAndReturnOptions.
    // If you want debug annotations, you must use Windows 10.
    if (IsWindows10OrGreater())
#endif
    {
        mAnnotationThread = std::this_thread::get_id();
        mUserDefinedAnnotation.Attach(
            d3d11::DynamicCastComObject<ID3DUserDefinedAnnotation>(context));
    }
}

void DebugAnnotator11::release()
{
    mUserDefinedAnnotation.Reset();
}

}  // namespace rx
