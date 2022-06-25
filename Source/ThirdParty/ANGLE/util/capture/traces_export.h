//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// restricted_traces_export: Export definitions for restricted traces.

#ifndef ANGLE_TRACES_EXPORT_H_
#define ANGLE_TRACES_EXPORT_H_

// for KHRONOS_APIENTRY
#include <KHR/khrplatform.h>

// See util/util_export.h for details on import/export labels.
#if !defined(ANGLE_TRACE_EXPORT)
#    if defined(_WIN32)
#        if defined(ANGLE_TRACE_IMPLEMENTATION)
#            define ANGLE_TRACE_EXPORT __declspec(dllexport)
#        else
#            define ANGLE_TRACE_EXPORT __declspec(dllimport)
#        endif
#    elif defined(__GNUC__)
#        define ANGLE_TRACE_EXPORT __attribute__((visibility("default")))
#    else
#        define ANGLE_TRACE_EXPORT
#    endif
#endif  // !defined(ANGLE_TRACE_EXPORT)

#if !defined(ANGLE_TRACE_LOADER_EXPORT)
#    if defined(_WIN32)
#        if defined(ANGLE_TRACE_LOADER_IMPLEMENTATION)
#            define ANGLE_TRACE_LOADER_EXPORT __declspec(dllexport)
#        else
#            define ANGLE_TRACE_LOADER_EXPORT __declspec(dllimport)
#        endif
#    elif defined(__GNUC__)
#        define ANGLE_TRACE_LOADER_EXPORT __attribute__((visibility("default")))
#    else
#        define ANGLE_TRACE_LOADER_EXPORT
#    endif
#endif  // !defined(ANGLE_TRACE_LOADER_EXPORT)

namespace trace_angle
{
using GenericProc = void (*)();
using LoadProc    = GenericProc(KHRONOS_APIENTRY *)(const char *);
ANGLE_TRACE_LOADER_EXPORT void LoadEGL(LoadProc loadProc);
ANGLE_TRACE_LOADER_EXPORT void LoadGLES(LoadProc loadProc);
}  // namespace trace_angle

#endif  // ANGLE_TRACES_EXPORT_H_
