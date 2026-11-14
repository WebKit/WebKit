/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameTimelineAgent.h"

#include "Document.h"
#include "Element.h"
#include "FloatQuad.h"
#include "InspectorWebAgentBase.h"
#include "InstrumentingAgents.h"
#include "LayoutRect.h"
#include "LocalFrameView.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "TimelineRecordFactory.h"
#include <wtf/JSONValues.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameTimelineAgent);

FrameTimelineAgent::FrameTimelineAgent(FrameAgentContext& context)
    : InspectorTimelineAgent(context)
{
}

FrameTimelineAgent::~FrameTimelineAgent() = default;

Inspector::Protocol::ErrorStringOr<void> FrameTimelineAgent::setAutoCaptureEnabled(bool)
{
    return makeUnexpected("Not supported"_s);
}

bool FrameTimelineAgent::tracking() const
{
    return protect(m_instrumentingAgents)->trackingFrameTimelineAgent() == this && InspectorTimelineAgent::tracking();
}

void FrameTimelineAgent::internalStart(std::optional<int>&& maxCallStackDepth)
{
    // Set before the base Timeline slot and cleared after it, so InspectorInstrumentation never finds the base slot
    // set without this one, which would send this frame's records to the page's agent.
    protect(m_instrumentingAgents)->setTrackingFrameTimelineAgent(this);

    InspectorTimelineAgent::internalStart(WTF::move(maxCallStackDepth));
}

void FrameTimelineAgent::internalStop()
{
    InspectorTimelineAgent::internalStop();

    protect(m_instrumentingAgents)->setTrackingFrameTimelineAgent(nullptr);
}

bool FrameTimelineAgent::shouldToggleInstrument(Inspector::Protocol::Timeline::Instrument instrument) const
{
    // CPU and Memory stay off: their data covers the whole process, so the frontend picks which target tracks it.
    // The other instruments belong to domains that frame targets do not have yet, so their getters would reach
    // the page's agents.
    // FIXME: <https://webkit.org/b/329360> Also toggle ScriptProfiler once frame targets have that domain.
    return instrument == Inspector::Protocol::Timeline::Instrument::Timeline;
}

void FrameTimelineAgent::willInvalidateLayout(const RenderObject& renderer)
{
    if (renderer.needsLayout())
        return;

    if (!is<RenderElement>(renderer))
        return;

    auto data = JSON::Object::create();

    if (auto nodeId = nodeIdForRenderer(renderer))
        TimelineRecordFactory::appendNodeId(data.get(), nodeId);

    appendRecord(WTF::move(data), TimelineRecordType::InvalidateLayout, true);
}

void FrameTimelineAgent::didScheduleLayout(const RenderElement& layoutRoot)
{
    auto data = JSON::Object::create();

    if (auto nodeId = nodeIdForRenderer(layoutRoot))
        TimelineRecordFactory::appendNodeId(data.get(), nodeId);

    appendRecord(WTF::move(data), TimelineRecordType::ScheduleLayout, true);
}

void FrameTimelineAgent::willLayout()
{
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::Layout, true);
}

void FrameTimelineAgent::didLayout(const RenderElement& layoutRoot, const Vector<FloatQuad>& layoutAreas)
{
    if (!lastRecordEntry())
        return;

    // Get the node ID first: it calls into the DOM agent, and a record pushed meanwhile would invalidate a
    // pointer into the record stack.
    auto nodeId = nodeIdForRenderer(layoutRoot);

    auto* entry = lastRecordEntry();
    ASSERT(entry->type == TimelineRecordType::Layout);

    if (!layoutAreas.isEmpty())
        TimelineRecordFactory::appendLayoutRoot(entry->data.get(), layoutAreas[0]);

    if (nodeId)
        TimelineRecordFactory::appendNodeId(entry->data.get(), nodeId);

    didCompleteCurrentRecord(TimelineRecordType::Layout);
}

void FrameTimelineAgent::didScheduleStyleRecalculation(Document& document)
{
    auto data = JSON::Object::create();

    if (auto nodeId = nodeIdForDocument(document))
        TimelineRecordFactory::appendNodeId(data.get(), nodeId);

    appendRecord(WTF::move(data), TimelineRecordType::ScheduleStyleRecalculation, true);
}

void FrameTimelineAgent::willRecalculateStyle()
{
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::RecalculateStyles, true);
}

void FrameTimelineAgent::didRecalculateStyle(Document& document)
{
    if (!lastRecordEntry())
        return;

    // Get the node ID before taking a pointer into the record stack (see didLayout).
    auto nodeId = nodeIdForDocument(document);

    auto* entry = lastRecordEntry();
    ASSERT(entry->type == TimelineRecordType::RecalculateStyles);

    if (nodeId)
        TimelineRecordFactory::appendNodeId(entry->data.get(), nodeId);

    didCompleteCurrentRecord(TimelineRecordType::RecalculateStyles);
}

void FrameTimelineAgent::willPaint()
{
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::Paint, true);
}

void FrameTimelineAgent::didPaint(RenderObject& renderer, const LayoutRect& clipRect)
{
    if (!lastRecordEntry())
        return;

    // Get the node ID before taking a pointer into the record stack (see didLayout).
    auto nodeId = nodeIdForRenderer(renderer);

    auto* entry = lastRecordEntry();
    ASSERT(entry->type == TimelineRecordType::Paint);

    auto clipQuadInMainFrameView = protect(renderer.view().frameView())->contentsToMainFrameView(renderer.localToAbsoluteQuad({ clipRect }));
    entry->data = TimelineRecordFactory::createPaintData(clipQuadInMainFrameView);

    if (nodeId)
        TimelineRecordFactory::appendNodeId(entry->data.get(), nodeId);

    didCompleteCurrentRecord(TimelineRecordType::Paint);
}

void FrameTimelineAgent::didEnqueueFirstContentfulPaint()
{
    appendRecord(JSON::Object::create(), TimelineRecordType::FirstContentfulPaint, false);
}

void FrameTimelineAgent::didEnqueueLargestContentfulPaint(Element* element, unsigned area)
{
    appendRecord(TimelineRecordFactory::createLargestContentfulPaintData(nodeIdForNode(element), area), TimelineRecordType::LargestContentfulPaint, false);
}

// FIXME: <https://webkit.org/b/329361> Push nodes through FrameDOMAgent once the frontend resolves node IDs in
// Timeline records through the record's target.
Inspector::Protocol::DOM::NodeId FrameTimelineAgent::nodeIdForNode(Node*) const
{
    return 0;
}

Inspector::Protocol::DOM::NodeId FrameTimelineAgent::nodeIdForDocument(Document&) const
{
    return 0;
}

Inspector::Protocol::DOM::NodeId FrameTimelineAgent::nodeIdForRenderer(const RenderObject& renderer) const
{
    if (renderer.isRenderView())
        return nodeIdForDocument(protect(renderer.document()).get());

    if (renderer.isAnonymous()) {
        for (CheckedPtr ancestor = renderer.parent(); ancestor; ancestor = ancestor->parent()) {
            if (RefPtr element = ancestor->element())
                return nodeIdForNode(element.get());
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    if (CheckedPtr renderElement = dynamicDowncast<RenderElement>(renderer)) {
        if (renderElement->isPseudoElement()) {
            RefPtr generatingElement = renderElement->generatingElement();
            return nodeIdForNode(generatingElement.get());
        }

        RefPtr element = renderElement->element();
        return nodeIdForNode(element.get());
    }

    RefPtr node = renderer.node();
    return nodeIdForNode(node.get());
}

} // namespace WebCore
