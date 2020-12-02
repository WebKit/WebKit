//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DebugAnnotatorVk.cpp: Vulkan helpers for adding trace annotations.
//

#include "libANGLE/renderer/vulkan/DebugAnnotatorVk.h"

#include "common/entry_points_enum_autogen.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"

namespace rx
{

DebugAnnotatorVk::DebugAnnotatorVk() {}

DebugAnnotatorVk::~DebugAnnotatorVk() {}

void DebugAnnotatorVk::beginEvent(gl::Context *context,
                                  gl::EntryPoint entryPoint,
                                  const char *eventName,
                                  const char *eventMessage)
{
    angle::LoggingAnnotator::beginEvent(context, entryPoint, eventName, eventMessage);
    if (vkCmdBeginDebugUtilsLabelEXT && context)
    {
        ContextVk *contextVk = vk::GetImpl(static_cast<gl::Context *>(context));
        contextVk->logEvent(eventMessage);
    }
}

void DebugAnnotatorVk::endEvent(gl::Context *context,
                                const char *eventName,
                                gl::EntryPoint entryPoint)
{
    angle::LoggingAnnotator::endEvent(context, eventName, entryPoint);
    if (vkCmdBeginDebugUtilsLabelEXT && context && isDrawOrDispatchEntryPoint(entryPoint))
    {
        ContextVk *contextVk = vk::GetImpl(static_cast<gl::Context *>(context));
        contextVk->endEventLog(entryPoint);
    }
}

bool DebugAnnotatorVk::getStatus()
{
    return true;
}

bool DebugAnnotatorVk::isDrawOrDispatchEntryPoint(gl::EntryPoint entryPoint) const
{
    switch (entryPoint)
    {
        case gl::EntryPoint::DispatchCompute:
        case gl::EntryPoint::DispatchComputeIndirect:
        case gl::EntryPoint::DrawArrays:
        case gl::EntryPoint::DrawArraysIndirect:
        case gl::EntryPoint::DrawArraysInstanced:
        case gl::EntryPoint::DrawArraysInstancedANGLE:
        case gl::EntryPoint::DrawArraysInstancedBaseInstance:
        case gl::EntryPoint::DrawArraysInstancedBaseInstanceANGLE:
        case gl::EntryPoint::DrawArraysInstancedEXT:
        case gl::EntryPoint::DrawBuffer:
        case gl::EntryPoint::DrawBuffers:
        case gl::EntryPoint::DrawBuffersEXT:
        case gl::EntryPoint::DrawElements:
        case gl::EntryPoint::DrawElementsBaseVertex:
        case gl::EntryPoint::DrawElementsBaseVertexEXT:
        case gl::EntryPoint::DrawElementsBaseVertexOES:
        case gl::EntryPoint::DrawElementsIndirect:
        case gl::EntryPoint::DrawElementsInstanced:
        case gl::EntryPoint::DrawElementsInstancedANGLE:
        case gl::EntryPoint::DrawElementsInstancedBaseInstance:
        case gl::EntryPoint::DrawElementsInstancedBaseVertex:
        case gl::EntryPoint::DrawElementsInstancedBaseVertexBaseInstance:
        case gl::EntryPoint::DrawElementsInstancedBaseVertexBaseInstanceANGLE:
        case gl::EntryPoint::DrawElementsInstancedBaseVertexEXT:
        case gl::EntryPoint::DrawElementsInstancedBaseVertexOES:
        case gl::EntryPoint::DrawElementsInstancedEXT:
        case gl::EntryPoint::DrawPixels:
        case gl::EntryPoint::DrawRangeElements:
        case gl::EntryPoint::DrawRangeElementsBaseVertex:
        case gl::EntryPoint::DrawRangeElementsBaseVertexEXT:
        case gl::EntryPoint::DrawRangeElementsBaseVertexOES:
        case gl::EntryPoint::DrawTexfOES:
        case gl::EntryPoint::DrawTexfvOES:
        case gl::EntryPoint::DrawTexiOES:
        case gl::EntryPoint::DrawTexivOES:
        case gl::EntryPoint::DrawTexsOES:
        case gl::EntryPoint::DrawTexsvOES:
        case gl::EntryPoint::DrawTexxOES:
        case gl::EntryPoint::DrawTexxvOES:
        case gl::EntryPoint::DrawTransformFeedback:
        case gl::EntryPoint::DrawTransformFeedbackInstanced:
        case gl::EntryPoint::DrawTransformFeedbackStream:
        case gl::EntryPoint::DrawTransformFeedbackStreamInstanced:
            return true;
        default:
            return false;
    }
}

}  // namespace rx
