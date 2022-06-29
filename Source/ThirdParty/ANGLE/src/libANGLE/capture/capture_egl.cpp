//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// capture_egl.h:
//   EGL capture functions
//

#include "libANGLE/capture/capture_egl.h"
#include "libANGLE/capture/FrameCapture.h"
#include "libANGLE/capture/frame_capture_utils.h"
#include "libANGLE/capture/gl_enum_utils_autogen.h"

#define USE_SYSTEM_ZLIB
#include "compression_utils_portable.h"

#if !ANGLE_CAPTURE_ENABLED
#    error Frame capture must be enabled to include this file.
#endif  // !ANGLE_CAPTURE_ENABLED

namespace angle
{
template <>
void WriteParamValueReplay<ParamType::TEGLContext>(std::ostream &os,
                                                   const CallCapture &call,
                                                   EGLContext value)
{
    // We actually capture the context ID
    uint64_t contextID = reinterpret_cast<uint64_t>(value);

    // The context map uses uint32_t as key type
    ASSERT(contextID <= 0xffffffffull);
    os << "static_cast<EGLContext>(gContextMap[" << contextID << "])";
}

template <>
void WriteParamValueReplay<ParamType::TEGLDisplay>(std::ostream &os,
                                                   const CallCapture &call,
                                                   EGLDisplay value)
{
    ASSERT(value == EGL_NO_DISPLAY);
    os << "EGL_NO_DISPLAY";
}

template <>
void WriteParamValueReplay<ParamType::TEGLSurface>(std::ostream &os,
                                                   const CallCapture &call,
                                                   EGLSurface value)
{
    if (value == EGL_NO_SURFACE)
    {
        os << "EGL_NO_SURFACE";
        return;
    }

    // We don't support capturing this EGL call
    UNREACHABLE();
}

template <>
void WriteParamValueReplay<ParamType::TEGLClientBuffer>(std::ostream &os,
                                                        const CallCapture &call,
                                                        EGLClientBuffer value)
{
    const auto &targetParam = call.params.getParam("target", ParamType::TEGLenum, 2);
    os << "GetClientBuffer(" << targetParam.value.EGLenumVal << ", " << value << ")";
}

}  // namespace angle

namespace egl
{

static angle::ParamCapture CaptureAttributeMap(const egl::AttributeMap &attribMap)
{
    std::vector<EGLAttrib> attribs;
    for (const auto &[key, value] : attribMap)
    {
        attribs.push_back(key);
        attribs.push_back(value);
    }
    attribs.push_back(EGL_NONE);

    angle::ParamCapture paramCapture("attrib_list", angle::ParamType::TGLint64Pointer);
    angle::CaptureMemory(attribs.data(), attribs.size() * sizeof(EGLAttrib), &paramCapture);
    return paramCapture;
}

angle::CallCapture CaptureCreateNativeClientBufferANDROID(gl::Context *context,
                                                          const egl::AttributeMap &attribMap,
                                                          EGLClientBuffer eglClientBuffer)
{
    angle::ParamBuffer paramBuffer;
    paramBuffer.addParam(CaptureAttributeMap(attribMap));

    angle::ParamCapture retval;
    angle::SetParamVal<angle::ParamType::TEGLClientBuffer, EGLClientBuffer>(eglClientBuffer,
                                                                            &retval.value);
    paramBuffer.addReturnValue(std::move(retval));

    return angle::CallCapture(angle::EntryPoint::EGLCreateNativeClientBufferANDROID,
                              std::move(paramBuffer));
}

angle::CallCapture CaptureEGLCreateImage(gl::Context *context,
                                         EGLenum target,
                                         EGLClientBuffer buffer,
                                         const egl::AttributeMap &attributes,
                                         egl::Image *image)
{
    angle::ParamBuffer paramBuffer;

    // The EGL display will be queried directly in the emitted code
    // so this is actually just a place holder
    paramBuffer.addValueParam("display", angle::ParamType::TEGLContext, EGL_NO_DISPLAY);

    // In CaptureMidExecutionSetup and FrameCaptureShared::captureCall
    // we capture the actual context ID (via CaptureMakeCurrent),
    // so we have to do the same here.
    uint64_t contextID    = context->id().value;
    EGLContext eglContext = reinterpret_cast<EGLContext>(contextID);
    paramBuffer.addValueParam("context", angle::ParamType::TEGLContext, eglContext);

    paramBuffer.addEnumParam("target", gl::GLenumGroup::DefaultGroup, angle::ParamType::TEGLenum,
                             target);

    angle::ParamCapture paramsClientBuffer("buffer", angle::ParamType::TEGLClientBuffer);
    uint64_t bufferID = reinterpret_cast<uintptr_t>(buffer);
    angle::SetParamVal<angle::ParamType::TGLuint64>(bufferID, &paramsClientBuffer.value);
    paramBuffer.addParam(std::move(paramsClientBuffer));

    angle::ParamCapture paramsAttr = CaptureAttributeMap(attributes);
    paramBuffer.addParam(std::move(paramsAttr));

    angle::ParamCapture retval;
    angle::SetParamVal<angle::ParamType::TGLeglImageOES, GLeglImageOES>(image, &retval.value);
    paramBuffer.addReturnValue(std::move(retval));

    return angle::CallCapture(angle::EntryPoint::EGLCreateImage, std::move(paramBuffer));
}

angle::CallCapture CaptureEGLDestroyImage(gl::Context *context,
                                          egl::Display *display,
                                          egl::Image *image)
{
    angle::ParamBuffer paramBuffer;
    paramBuffer.addValueParam("display", angle::ParamType::TEGLContext, EGL_NO_DISPLAY);

    angle::ParamCapture paramImage("image", angle::ParamType::TGLeglImageOES);
    angle::SetParamVal<angle::ParamType::TGLeglImageOES, GLeglImageOES>(image, &paramImage.value);
    paramBuffer.addParam(std::move(paramImage));

    return angle::CallCapture(angle::EntryPoint::EGLDestroyImage, std::move(paramBuffer));
}

}  // namespace egl
