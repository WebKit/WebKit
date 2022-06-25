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
                                  angle::EntryPoint entryPoint,
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
                                angle::EntryPoint entryPoint)
{
    angle::LoggingAnnotator::endEvent(context, eventName, entryPoint);
    if (vkCmdBeginDebugUtilsLabelEXT && context)
    {
        ContextVk *contextVk = vk::GetImpl(static_cast<gl::Context *>(context));
        if (isDrawEntryPoint(entryPoint))
        {
            contextVk->endEventLog(entryPoint, PipelineType::Graphics);
        }
        else if (isDispatchEntryPoint(entryPoint))
        {
            contextVk->endEventLog(entryPoint, PipelineType::Compute);
        }
        else if (isClearOrQueryEntryPoint(entryPoint))
        {
            contextVk->endEventLogForClearOrQuery();
        }
    }
}

bool DebugAnnotatorVk::getStatus()
{
    return true;
}

bool DebugAnnotatorVk::isDrawEntryPoint(angle::EntryPoint entryPoint) const
{
    switch (entryPoint)
    {
        case angle::EntryPoint::GLDrawArrays:
        case angle::EntryPoint::GLDrawArraysIndirect:
        case angle::EntryPoint::GLDrawArraysInstanced:
        case angle::EntryPoint::GLDrawArraysInstancedANGLE:
        case angle::EntryPoint::GLDrawArraysInstancedBaseInstance:
        case angle::EntryPoint::GLDrawArraysInstancedBaseInstanceANGLE:
        case angle::EntryPoint::GLDrawArraysInstancedEXT:
        case angle::EntryPoint::GLDrawElements:
        case angle::EntryPoint::GLDrawElementsBaseVertex:
        case angle::EntryPoint::GLDrawElementsBaseVertexEXT:
        case angle::EntryPoint::GLDrawElementsBaseVertexOES:
        case angle::EntryPoint::GLDrawElementsIndirect:
        case angle::EntryPoint::GLDrawElementsInstanced:
        case angle::EntryPoint::GLDrawElementsInstancedANGLE:
        case angle::EntryPoint::GLDrawElementsInstancedBaseInstance:
        case angle::EntryPoint::GLDrawElementsInstancedBaseVertex:
        case angle::EntryPoint::GLDrawElementsInstancedBaseVertexBaseInstance:
        case angle::EntryPoint::GLDrawElementsInstancedBaseVertexBaseInstanceANGLE:
        case angle::EntryPoint::GLDrawElementsInstancedBaseVertexEXT:
        case angle::EntryPoint::GLDrawElementsInstancedBaseVertexOES:
        case angle::EntryPoint::GLDrawElementsInstancedEXT:
        case angle::EntryPoint::GLDrawPixels:
        case angle::EntryPoint::GLDrawRangeElements:
        case angle::EntryPoint::GLDrawRangeElementsBaseVertex:
        case angle::EntryPoint::GLDrawRangeElementsBaseVertexEXT:
        case angle::EntryPoint::GLDrawRangeElementsBaseVertexOES:
        case angle::EntryPoint::GLDrawTexfOES:
        case angle::EntryPoint::GLDrawTexfvOES:
        case angle::EntryPoint::GLDrawTexiOES:
        case angle::EntryPoint::GLDrawTexivOES:
        case angle::EntryPoint::GLDrawTexsOES:
        case angle::EntryPoint::GLDrawTexsvOES:
        case angle::EntryPoint::GLDrawTexxOES:
        case angle::EntryPoint::GLDrawTexxvOES:
        case angle::EntryPoint::GLDrawTransformFeedback:
        case angle::EntryPoint::GLDrawTransformFeedbackInstanced:
        case angle::EntryPoint::GLDrawTransformFeedbackStream:
        case angle::EntryPoint::GLDrawTransformFeedbackStreamInstanced:
            return true;
        default:
            return false;
    }
}

bool DebugAnnotatorVk::isDispatchEntryPoint(angle::EntryPoint entryPoint) const
{
    switch (entryPoint)
    {
        case angle::EntryPoint::GLDispatchCompute:
        case angle::EntryPoint::GLDispatchComputeIndirect:
            return true;
        default:
            return false;
    }
}

bool DebugAnnotatorVk::isClearOrQueryEntryPoint(angle::EntryPoint entryPoint) const
{
    switch (entryPoint)
    {
        case angle::EntryPoint::GLClear:
        case angle::EntryPoint::GLClearBufferfi:
        case angle::EntryPoint::GLClearBufferfv:
        case angle::EntryPoint::GLClearBufferiv:
        case angle::EntryPoint::GLClearBufferuiv:
        case angle::EntryPoint::GLBeginQuery:
        case angle::EntryPoint::GLBeginQueryEXT:
        case angle::EntryPoint::GLBeginQueryIndexed:
        case angle::EntryPoint::GLEndQuery:
        case angle::EntryPoint::GLEndQueryEXT:
        case angle::EntryPoint::GLEndQueryIndexed:
            return true;
        default:
            return false;
    }
}

}  // namespace rx
