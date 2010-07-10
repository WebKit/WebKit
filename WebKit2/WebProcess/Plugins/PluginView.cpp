/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PluginView.h"

#include "Plugin.h"
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/ScrollView.h>

using namespace WebCore;

namespace WebKit {

PluginView::PluginView(WebCore::HTMLPlugInElement* pluginElement, PassRefPtr<Plugin> plugin, const Plugin::Parameters& parameters)
    : m_pluginElement(pluginElement)
    , m_plugin(plugin)
    , m_parameters(parameters)
    , m_isInitialized(false)
{
}

PluginView::~PluginView()
{
    if (m_plugin && m_isInitialized)
        m_plugin->destroy();
}

void PluginView::initializePlugin()
{
    if (m_isInitialized)
        return;

    if (!m_plugin) {
        // We've already tried and failed to initialize the plug-in.
        return;
    }

    if (!m_plugin->initialize(m_parameters)) {
        // We failed to initialize the plug-in.
        m_plugin = 0;

        return;
    }
    
    m_isInitialized = true;
}

void PluginView::setFrameRect(const WebCore::IntRect& rect)
{
    Widget::setFrameRect(rect);
    viewGeometryDidChange();
}

void PluginView::paint(GraphicsContext* context, const IntRect& dirtyRect)
{
    if (context->paintingDisabled() || !m_plugin)
        return;
    
    IntRect dirtyRectInWindowCoordinates = parent()->contentsToWindow(dirtyRect);
    
    IntRect paintRectInWindowCoordinates = intersection(dirtyRectInWindowCoordinates, clipRectInWindowCoordinates());
    if (paintRectInWindowCoordinates.isEmpty())
        return;

    context->save();

    // Translate the context so that the origin is at the top left corner of the plug-in view.
    context->translate(frameRect().x(), frameRect().y());

    m_plugin->paint(context, paintRectInWindowCoordinates);
    context->restore();
}

void PluginView::frameRectsChanged()
{
    Widget::frameRectsChanged();
    viewGeometryDidChange();
}

void PluginView::setParent(ScrollView* scrollView)
{
    Widget::setParent(scrollView);
    
    if (scrollView)
        initializePlugin();

    viewGeometryDidChange();
}

void PluginView::viewGeometryDidChange()
{
    if (!parent() || !m_plugin)
        return;

    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = parent()->contentsToWindow(frameRect());

    m_plugin->geometryDidChange(frameRectInWindowCoordinates, clipRectInWindowCoordinates());
}

IntRect PluginView::clipRectInWindowCoordinates() const
{
    ASSERT(parent());

    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = parent()->contentsToWindow(frameRect());

    // Get the window clip rect for the enclosing layer (in window coordinates).
    RenderLayer* layer = m_pluginElement->renderer()->enclosingLayer();
    FrameView* parentView = m_pluginElement->document()->frame()->view();
    IntRect windowClipRect = parentView->windowClipRectForLayer(layer, true);

    // Intersect the two rects to get the view clip rect in window coordinates.
    return intersection(frameRectInWindowCoordinates, windowClipRect);
}

void PluginView::invalidateRect(const IntRect&)
{
}

} // namespace WebKit
