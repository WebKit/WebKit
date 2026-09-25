# This file was generated with the command:
# "./gni-to-cmake.py" "src/libANGLE/renderer/wgpu/wgpu_sources.gni" "Wgpu.cmake" "--prepend" "src/libANGLE/renderer/wgpu/"

# Copyright 2024 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set(wgpu_backend_sources
    "src/libANGLE/renderer/wgpu/BufferWgpu.cpp"
    "src/libANGLE/renderer/wgpu/BufferWgpu.h"
    "src/libANGLE/renderer/wgpu/CompilerWgpu.cpp"
    "src/libANGLE/renderer/wgpu/CompilerWgpu.h"
    "src/libANGLE/renderer/wgpu/ContextWgpu.cpp"
    "src/libANGLE/renderer/wgpu/ContextWgpu.h"
    "src/libANGLE/renderer/wgpu/DeviceWgpu.cpp"
    "src/libANGLE/renderer/wgpu/DeviceWgpu.h"
    "src/libANGLE/renderer/wgpu/DisplayWgpu.cpp"
    "src/libANGLE/renderer/wgpu/DisplayWgpu.h"
    "src/libANGLE/renderer/wgpu/DisplayWgpu_api.h"
    "src/libANGLE/renderer/wgpu/FenceNVWgpu.cpp"
    "src/libANGLE/renderer/wgpu/FenceNVWgpu.h"
    "src/libANGLE/renderer/wgpu/FramebufferWgpu.cpp"
    "src/libANGLE/renderer/wgpu/FramebufferWgpu.h"
    "src/libANGLE/renderer/wgpu/ImageWgpu.cpp"
    "src/libANGLE/renderer/wgpu/ImageWgpu.h"
    "src/libANGLE/renderer/wgpu/ProgramExecutableWgpu.cpp"
    "src/libANGLE/renderer/wgpu/ProgramExecutableWgpu.h"
    "src/libANGLE/renderer/wgpu/ProgramPipelineWgpu.cpp"
    "src/libANGLE/renderer/wgpu/ProgramPipelineWgpu.h"
    "src/libANGLE/renderer/wgpu/ProgramWgpu.cpp"
    "src/libANGLE/renderer/wgpu/ProgramWgpu.h"
    "src/libANGLE/renderer/wgpu/QueryWgpu.cpp"
    "src/libANGLE/renderer/wgpu/QueryWgpu.h"
    "src/libANGLE/renderer/wgpu/RenderTargetWgpu.cpp"
    "src/libANGLE/renderer/wgpu/RenderTargetWgpu.h"
    "src/libANGLE/renderer/wgpu/RenderbufferWgpu.cpp"
    "src/libANGLE/renderer/wgpu/RenderbufferWgpu.h"
    "src/libANGLE/renderer/wgpu/SamplerWgpu.cpp"
    "src/libANGLE/renderer/wgpu/SamplerWgpu.h"
    "src/libANGLE/renderer/wgpu/ShaderWgpu.cpp"
    "src/libANGLE/renderer/wgpu/ShaderWgpu.h"
    "src/libANGLE/renderer/wgpu/SurfaceWgpu.cpp"
    "src/libANGLE/renderer/wgpu/SurfaceWgpu.h"
    "src/libANGLE/renderer/wgpu/SyncWgpu.cpp"
    "src/libANGLE/renderer/wgpu/SyncWgpu.h"
    "src/libANGLE/renderer/wgpu/TextureWgpu.cpp"
    "src/libANGLE/renderer/wgpu/TextureWgpu.h"
    "src/libANGLE/renderer/wgpu/TransformFeedbackWgpu.cpp"
    "src/libANGLE/renderer/wgpu/TransformFeedbackWgpu.h"
    "src/libANGLE/renderer/wgpu/UtilsWgpu.cpp"
    "src/libANGLE/renderer/wgpu/UtilsWgpu.h"
    "src/libANGLE/renderer/wgpu/VertexArrayWgpu.cpp"
    "src/libANGLE/renderer/wgpu/VertexArrayWgpu.h"
    "src/libANGLE/renderer/wgpu/wgpu_command_buffer.cpp"
    "src/libANGLE/renderer/wgpu/wgpu_command_buffer.h"
    "src/libANGLE/renderer/wgpu/wgpu_format_table_autogen.cpp"
    "src/libANGLE/renderer/wgpu/wgpu_format_utils.cpp"
    "src/libANGLE/renderer/wgpu/wgpu_format_utils.h"
    "src/libANGLE/renderer/wgpu/wgpu_helpers.cpp"
    "src/libANGLE/renderer/wgpu/wgpu_helpers.h"
    "src/libANGLE/renderer/wgpu/wgpu_pipeline_state.cpp"
    "src/libANGLE/renderer/wgpu/wgpu_pipeline_state.h"
    "src/libANGLE/renderer/wgpu/wgpu_proc_utils.cpp"
    "src/libANGLE/renderer/wgpu/wgpu_proc_utils.h"
    "src/libANGLE/renderer/wgpu/wgpu_utils.cpp"
    "src/libANGLE/renderer/wgpu/wgpu_utils.h"
    "src/libANGLE/renderer/wgpu/wgpu_wgsl_util.cpp"
    "src/libANGLE/renderer/wgpu/wgpu_wgsl_util.h"
)

if(is_win)
    list(APPEND wgpu_backend_sources
        "src/libANGLE/renderer/wgpu/win32/WindowSurfaceWgpuWin32.cpp"
        "src/libANGLE/renderer/wgpu/win32/WindowSurfaceWgpuWin32.h"
    )
endif()

if(is_mac OR is_ios)
    list(APPEND wgpu_backend_sources
        "src/libANGLE/renderer/wgpu/mac/WindowSurfaceWgpuMetalLayer.h"
        "src/libANGLE/renderer/wgpu/mac/WindowSurfaceWgpuMetalLayer.mm"
    )
endif()

if(angle_use_x11)
    list(APPEND wgpu_backend_sources
        "src/libANGLE/renderer/wgpu/linux/x11/WindowSurfaceWgpuX11.cpp"
        "src/libANGLE/renderer/wgpu/linux/x11/WindowSurfaceWgpuX11.h"
    )
endif()

if(angle_use_wayland)
    list(APPEND wgpu_backend_sources
        "src/libANGLE/renderer/wgpu/linux/wayland/WindowSurfaceWgpuWayland.cpp"
        "src/libANGLE/renderer/wgpu/linux/wayland/WindowSurfaceWgpuWayland.h"
    )
endif()
